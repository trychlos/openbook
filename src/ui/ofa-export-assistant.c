/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-iassistant.h"
#include "my/my-ibin.h"
#include "my/my-isettings.h"
#include "my/my-iwindow.h"
#include "my/my-progress-bar.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iexporter.h"
#include "api/ofa-igetter.h"
#include "api/ofa-prefs.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-class.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

#include "core/ofa-stream-format-bin.h"
#include "core/ofa-stream-format-disp.h"

#include "ui/ofa-export-assistant.h"

/* private instance data
 *
 * There are 3 useful pages
 * + one introduction page (page '0')
 * + one confirmation page (page '4)
 * + one result page (page '5')
 */
typedef struct {
	gboolean             dispose_has_run;

	/* initialization
	 */
	ofaIGetter          *getter;

	/* runtime
	 */
	gchar               *settings_prefix;
	ofaIDBDossierMeta   *dossier_meta;

	/* p0: introduction
	 */

	/* p1: select data type to be exported
	 */
	GList               *p1_exportables;
	GList               *p1_exporters;
	GtkWidget           *p1_parent;
	gint                 p1_row;
	GtkWidget           *p1_selected_btn;
	gchar               *p1_selected_class;
	gchar               *p1_selected_label;
	GType                p1_selected_type;

	/* p2: select format
	 */
	GtkWidget           *p2_datatype;
	GtkWidget           *p2_format_combo;
	ofaStreamFormat     *p2_default_stformat;
	ofaStreamFormat     *p2_stformat;
	ofaStreamFormatBin  *p2_settings_prefs;
	GtkWidget           *p2_message;
	gchar               *p2_specific_id;
	gchar               *p2_specific_label;
	ofaIExporter        *p2_specific_exporter;
	gchar               *p2_stream_format_name;

	/* p3: output file
	 */
	GtkWidget           *p3_datatype;
	GtkWidget           *p3_specific;
	GtkWidget           *p3_format;
	GtkWidget           *p3_chooser;
	gchar               *p3_folder_uri;
	gchar               *p3_output_file_uri;

	/* p4: confirm
	 */
	ofaStreamFormatDisp *p4_format;

	/* p5: apply
	 */
	myProgressBar       *p5_bar;
	ofaIExportable      *p5_exportable;
	GtkWidget           *p5_page;
}
	ofaExportAssistantPrivate;

/* ExportAssistant Assistant
 *
 * pos.  type     enum     title
 * ---   -------  -------  --------------------------------------------
 *   0   Intro    INTRO    Introduction
 *   1   Content  SELECT   Select the data
 *   2   Content  FORMAT   Select the export format
 *   3   Content  OUTPUT   Select the output file
 *   4   Confirm  CONFIRM  Summary of the operations to be done
 *   5   Summary  DONE     After export
 */

enum {
	ASSIST_PAGE_INTRO = 0,
	ASSIST_PAGE_SELECT,
	ASSIST_PAGE_FORMAT,
	ASSIST_PAGE_OUTPUT,
	ASSIST_PAGE_CONFIRM,
	ASSIST_PAGE_DONE
};

/* type of exported datas
 */
enum {
	DATA_ACCOUNT = 1,
	DATA_CLASS,
	DATA_CURRENCY,
	DATA_ENTRY,
	DATA_LEDGER,
	DATA_MODEL,
	DATA_RATE,
	DATA_DOSSIER,
};

/* the columns in the specific format combo
 */
enum {
	FORMAT_COL_ID = 0,
	FORMAT_COL_LABEL,
	FORMAT_COL_FORMAT,					/* ofaStreamFormat */
	FORMAT_COL_EXPORTER,				/* ofaIExporter */
	FORMAT_N_COLUMNS
};

/* the default format
 */
static ofsIExporterFormat st_def_format[] = {
		{ OFA_IEXPORTER_DEFAULT_FORMAT_ID, N_( "Default format (all columns are exported in the standard order)" ) },
		{ 0 },
};

/* data set against each data type radio button */
#define DATA_TYPE_INDEX                   "ofa-data-type"		// list of ofaIExportables

static const gchar *st_export_folder    = "ofa-LastExportFolder";
static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-export-assistant.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     iassistant_iface_init( myIAssistantInterface *iface );
static gboolean iassistant_is_willing_to_quit( myIAssistant*instance, guint keyval );
static void     p0_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page_widget );
static void     p1_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void     p1_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void     p1_on_type_toggled( GtkToggleButton *button, ofaExportAssistant *self );
static gboolean p1_is_complete( ofaExportAssistant *self );
static void     p1_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void     p2_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void     p2_init_format_combo( ofaExportAssistant *self, GtkWidget *page );
static void     p2_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void     p2_display_format_combo( ofaExportAssistant *self );
static void     p2_on_format_combo_changed( GtkComboBox *combo, ofaExportAssistant *self );
static void     p2_on_settings_changed( ofaStreamFormatBin *bin, ofaExportAssistant *self );
static void     p2_on_new_profile_clicked( GtkButton *button, ofaExportAssistant *self );
static void     p2_check_for_complete( ofaExportAssistant *self );
static void     p2_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void     p3_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void     p3_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void     p3_on_selection_changed( GtkFileChooser *chooser, ofaExportAssistant *self );
static void     p3_on_file_activated( GtkFileChooser *chooser, ofaExportAssistant *self );
static gboolean p3_check_for_complete( ofaExportAssistant *self );
static void     p3_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void     p4_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void     p4_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static gboolean p3_confirm_overwrite( const ofaExportAssistant *self, const gchar *fname );
static void     p5_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static gboolean p5_export_data( ofaExportAssistant *self );
static void     read_settings( ofaExportAssistant *self );
static void     write_settings( ofaExportAssistant *self );
static void     iprogress_iface_init( myIProgressInterface *iface );
static void     iprogress_start_work( myIProgress *instance, const void *worker, GtkWidget *empty );
static void     iprogress_pulse( myIProgress *instance, const void *worker, gulong count, gulong total );

G_DEFINE_TYPE_EXTENDED( ofaExportAssistant, ofa_export_assistant, GTK_TYPE_ASSISTANT, 0,
		G_ADD_PRIVATE( ofaExportAssistant )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IPROGRESS, iprogress_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IASSISTANT, iassistant_iface_init ))

static const ofsIAssistant st_pages_cb [] = {
		{ ASSIST_PAGE_INTRO,
				NULL,
				NULL,
				( myIAssistantCb ) p0_do_forward },
		{ ASSIST_PAGE_SELECT,
				( myIAssistantCb ) p1_do_init,
				( myIAssistantCb ) p1_do_display,
				( myIAssistantCb ) p1_do_forward },
		{ ASSIST_PAGE_FORMAT,
				( myIAssistantCb ) p2_do_init,
				( myIAssistantCb ) p2_do_display,
				( myIAssistantCb ) p2_do_forward },
		{ ASSIST_PAGE_OUTPUT,
				( myIAssistantCb ) p3_do_init,
				( myIAssistantCb ) p3_do_display,
				( myIAssistantCb ) p3_do_forward },
		{ ASSIST_PAGE_CONFIRM,
				( myIAssistantCb ) p4_do_init,
				( myIAssistantCb ) p4_do_display,
				NULL },
		{ ASSIST_PAGE_DONE,
				NULL,
				( myIAssistantCb ) p5_do_display,
				NULL },
		{ -1 }
};

static void
export_assistant_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_export_assistant_finalize";
	ofaExportAssistantPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EXPORT_ASSISTANT( instance ));

	/* free data members here */
	priv = ofa_export_assistant_get_instance_private( OFA_EXPORT_ASSISTANT( instance ));

	g_list_free( priv->p1_exportables );
	g_list_free( priv->p1_exporters );
	g_free( priv->settings_prefix );
	g_free( priv->p1_selected_class );
	g_free( priv->p1_selected_label );
	g_free( priv->p2_specific_id );
	g_free( priv->p2_specific_label );
	g_free( priv->p2_stream_format_name );
	g_free( priv->p3_folder_uri );
	g_free( priv->p3_output_file_uri );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_export_assistant_parent_class )->finalize( instance );
}

static void
export_assistant_dispose( GObject *instance )
{
	ofaExportAssistantPrivate *priv;

	g_return_if_fail( instance && OFA_IS_EXPORT_ASSISTANT( instance ));

	priv = ofa_export_assistant_get_instance_private( OFA_EXPORT_ASSISTANT( instance ));

	if( !priv->dispose_has_run ){

		write_settings( OFA_EXPORT_ASSISTANT( instance ));

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->p2_default_stformat );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_export_assistant_parent_class )->dispose( instance );
}

static void
ofa_export_assistant_init( ofaExportAssistant *self )
{
	static const gchar *thisfn = "ofa_export_assistant_init";
	ofaExportAssistantPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXPORT_ASSISTANT( self ));

	priv = ofa_export_assistant_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->p1_exportables = NULL;
	priv->p1_exporters = NULL;
	priv->p2_stformat = NULL;
	priv->p3_output_file_uri = NULL;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_export_assistant_class_init( ofaExportAssistantClass *klass )
{
	static const gchar *thisfn = "ofa_export_assistant_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = export_assistant_dispose;
	G_OBJECT_CLASS( klass )->finalize = export_assistant_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_export_assistant_run:
 * @getter: a #ofaIGetter instance.
 *
 * Run the assistant.
 */
void
ofa_export_assistant_run( ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_export_assistant_run";
	ofaExportAssistant *self;
	ofaExportAssistantPrivate *priv;;

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	self = g_object_new( OFA_TYPE_EXPORT_ASSISTANT, NULL );

	priv = ofa_export_assistant_get_instance_private( self );

	priv->getter = getter;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_export_assistant_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_export_assistant_iwindow_init";
	ofaExportAssistantPrivate *priv;
	ofaHub *hub;
	ofaIDBConnect *connect;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_export_assistant_get_instance_private( OFA_EXPORT_ASSISTANT( instance ));

	my_iwindow_set_parent( instance, GTK_WINDOW( ofa_igetter_get_main_window( priv->getter )));
	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));

	my_iassistant_set_callbacks( MY_IASSISTANT( instance ), st_pages_cb );

	hub = ofa_igetter_get_hub( priv->getter );
	connect = ofa_hub_get_connect( hub );
	priv->dossier_meta = ofa_idbconnect_get_dossier_meta( connect );

	read_settings( OFA_EXPORT_ASSISTANT( instance ));
}

/*
 * myIAssistant interface management
 */
static void
iassistant_iface_init( myIAssistantInterface *iface )
{
	static const gchar *thisfn = "ofa_export_assistant_iassistant_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->is_willing_to_quit = iassistant_is_willing_to_quit;
}

static gboolean
iassistant_is_willing_to_quit( myIAssistant *instance, guint keyval )
{
	ofaExportAssistantPrivate *priv;

	priv = ofa_export_assistant_get_instance_private( OFA_EXPORT_ASSISTANT( instance ));

	return( ofa_prefs_assistant_is_willing_to_quit( priv->getter, keyval ));
}

/*
 * get some dossier data
 */
static void
p0_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page_widget )
{
	static const gchar *thisfn = "ofa_export_assistant_p0_do_forward";

	g_debug( "%s: self=%p, page_num=%d, page_widget=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page_widget, G_OBJECT_TYPE_NAME( page_widget ));
}

/*
 * p1: type of the data to export
 *
 * Selection is radio-button based, so we are sure that at most
 * one exported data type is selected
 * and we make sure that at least one button is active.
 *
 * Attach to each button the fake ofaIEXportable object.
 *
 * p1_selected_class: may be set from settings
 * p1_selected_btn: is only set in p1_is_complete().
 * p1_selected_label: is only set in p1_is_complete().
 */
static void
p1_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p1_do_init";
	ofaExportAssistantPrivate *priv;
	GList *it;
	gchar *label;
	GtkWidget *btn, *first;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	priv->p1_exportables = ofa_igetter_get_for_type( priv->getter, OFA_TYPE_IEXPORTABLE );
	g_debug( "%s: exportables count=%d", thisfn, g_list_length( priv->p1_exportables ));

	priv->p1_exporters = ofa_igetter_get_for_type( priv->getter, OFA_TYPE_IEXPORTER );
	g_debug( "%s: exporters count=%d", thisfn, g_list_length( priv->p1_exporters ));

	priv->p1_selected_btn = NULL;
	priv->p1_row = 0;
	first = NULL;

	priv->p1_parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p1-parent" );
	g_return_if_fail( priv->p1_parent && GTK_IS_GRID( priv->p1_parent ));

	for( it=priv->p1_exportables ; it ; it=it->next ){
		label = ofa_iexportable_get_label( OFA_IEXPORTABLE( it->data ));
		if( my_strlen( label )){
			if( !first ){
				btn = gtk_radio_button_new_with_mnemonic( NULL, label );
				first = btn;
			} else {
				btn = gtk_radio_button_new_with_mnemonic_from_widget( GTK_RADIO_BUTTON( first ), label );
			}
			g_object_set_data( G_OBJECT( btn ), DATA_TYPE_INDEX, it->data );
			g_signal_connect( btn, "toggled", G_CALLBACK( p1_on_type_toggled ), self );
			gtk_grid_attach( GTK_GRID( priv->p1_parent ), btn, 0, priv->p1_row++, 1, 1 );
		}
		g_free( label );
	}
}

static void
p1_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p1_do_display";
	ofaExportAssistantPrivate *priv;
	gint i;
	GtkWidget *btn;
	gboolean is_complete;
	ofaIExportable *object;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	if( my_strlen( priv->p1_selected_class )){
		for( i=0 ; i<priv->p1_row ; ++i ){
			btn = gtk_grid_get_child_at( GTK_GRID( priv->p1_parent ), 0, i );
			g_return_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ));
			object = OFA_IEXPORTABLE( g_object_get_data( G_OBJECT( btn ), DATA_TYPE_INDEX ));
			if( !my_collate( G_OBJECT_TYPE_NAME( object ), priv->p1_selected_class )){
				gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( btn ), TRUE );
				break;
			}
		}
	}

	is_complete = p1_is_complete( self );
	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), is_complete );
}

static void
p1_on_type_toggled( GtkToggleButton *button, ofaExportAssistant *self )
{
	gboolean is_complete;

	is_complete = p1_is_complete( self );
	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), is_complete );
}

static gboolean
p1_is_complete( ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	gint i;
	GtkWidget *btn;
	ofaIExportable *object;
	const gchar *label;

	priv = ofa_export_assistant_get_instance_private( self );

	/* which is the currently active button ? */
	for( i=0 ; i<priv->p1_row ; ++i ){
		btn = gtk_grid_get_child_at( GTK_GRID( priv->p1_parent ), 0, i );
		if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( btn ))){
			object = OFA_IEXPORTABLE( g_object_get_data( G_OBJECT( btn ), DATA_TYPE_INDEX ));
			priv->p1_selected_btn = btn;
			g_free( priv->p1_selected_class );
			priv->p1_selected_class = g_strdup( G_OBJECT_TYPE_NAME( object ));
			g_free( priv->p1_selected_label );
			label = gtk_button_get_label( GTK_BUTTON( priv->p1_selected_btn ));
			priv->p1_selected_label = my_utils_str_remove_underlines( label );
			priv->p1_selected_type = G_OBJECT_TYPE( object );
			break;
		}
	}

	return( GTK_IS_WIDGET( priv->p1_selected_btn ));
}

static void
p1_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p1_do_forward";
	ofaExportAssistantPrivate *priv;

	g_debug( "%s: self=%p, page=%p", thisfn, ( void * ) self, ( void * ) page );

	priv = ofa_export_assistant_get_instance_private( self );

	g_debug( "%s: selected_btn=%p, selected_label='%s', selected_class=%s",
			thisfn, ( void * ) priv->p1_selected_btn, priv->p1_selected_label, priv->p1_selected_class );
}

/*
 * p2: export format
 *
 * These are initialized with the export settings for this name, or
 * with default settings
 */
static void
p2_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p2_do_init";
	ofaExportAssistantPrivate *priv;
	GtkWidget *label, *parent, *button;
	GtkSizeGroup *hgroup, *group_bin;
	const gchar *found_key;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	/* already set */
	priv->p2_datatype = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-datatype" );
	g_return_if_fail( priv->p2_datatype && GTK_IS_LABEL( priv->p2_datatype ));
	my_style_add( priv->p2_datatype, "labelinfo" );

	/* specific format combo */
	p2_init_format_combo( self, page );

	/* stream format */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-settings-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->p2_settings_prefs = ofa_stream_format_bin_new( NULL );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p2_settings_prefs ));

	g_signal_connect( priv->p2_settings_prefs, "ofa-changed", G_CALLBACK( p2_on_settings_changed ), self );

	/* get a suitable default stream format */
	found_key = NULL;
	if( ofa_stream_format_exists( priv->getter, priv->p1_selected_class, OFA_SFMODE_EXPORT )){
		found_key = priv->p1_selected_class;
	}
	g_clear_object( &priv->p2_default_stformat );
	priv->p2_default_stformat = ofa_stream_format_new( priv->getter, found_key, OFA_SFMODE_EXPORT );
	ofa_stream_format_set_field_updatable( priv->p2_default_stformat, OFA_SFHAS_MODE, FALSE );

	/* new profile */
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-new-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( button, "clicked", G_CALLBACK( p2_on_new_profile_clicked ), self );

	/* error message */
	priv->p2_message = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-message" );
	g_return_if_fail( priv->p2_message && GTK_IS_LABEL( priv->p2_message ));
	my_style_add( priv->p2_message, "labelerror" );

	/* horizontal alignment between the frames */
	hgroup = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-label221" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-label222" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	if(( group_bin = my_ibin_get_size_group( MY_IBIN( priv->p2_settings_prefs ), 0 ))){
		my_utils_size_group_add_size_group( hgroup, group_bin );
	}

	g_object_unref( hgroup );
}

/*
 * Initialize the specific format combo box
 * Have a line "Default" + lines provided by the target class
 */
static void
p2_init_format_combo( ofaExportAssistant *self, GtkWidget *page )
{
	ofaExportAssistantPrivate *priv;
	GtkListStore *store;
	GtkCellRenderer *cell;

	priv = ofa_export_assistant_get_instance_private( self );

	priv->p2_format_combo = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-format-combo" );
	g_return_if_fail( priv->p2_format_combo && GTK_IS_COMBO_BOX( priv->p2_format_combo ));

	store = gtk_list_store_new( FORMAT_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT, G_TYPE_OBJECT );
	gtk_combo_box_set_model( GTK_COMBO_BOX( priv->p2_format_combo ), GTK_TREE_MODEL( store ));
	g_object_unref( store );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->p2_format_combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->p2_format_combo ), cell, "text", FORMAT_COL_LABEL );

	gtk_combo_box_set_id_column( GTK_COMBO_BOX( priv->p2_format_combo ), FORMAT_COL_ID );

	g_signal_connect( priv->p2_format_combo, "changed", G_CALLBACK( p2_on_format_combo_changed ), self );
}

static void
p2_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p2_do_display";
	ofaExportAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	/* previously set */
	gtk_label_set_text( GTK_LABEL( priv->p2_datatype ), priv->p1_selected_label );

	/* display specific format combo */
	priv->p2_stformat = NULL;

	p2_display_format_combo( self );

	p2_check_for_complete( self );
}

static void
p2_display_format_combo( ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	guint i, count;
	GList *it;
	ofsIExporterFormat *fmt_array;

	priv = ofa_export_assistant_get_instance_private( self );

	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( priv->p2_format_combo ));
	g_return_if_fail( tmodel && GTK_IS_LIST_STORE( tmodel ));

	gtk_list_store_clear( GTK_LIST_STORE( tmodel ));

	for( i=0 ; st_def_format[i].format_id ; ++i ){
		gtk_list_store_insert_with_values( GTK_LIST_STORE( tmodel ), &iter, -1,
				FORMAT_COL_ID,       st_def_format[i].format_id,
				FORMAT_COL_LABEL,    gettext( st_def_format[i].format_label ),
				FORMAT_COL_FORMAT,   priv->p2_default_stformat,
				FORMAT_COL_EXPORTER, NULL,
				-1 );
	}

	count = 0;

	for( it=priv->p1_exporters ; it ; it=it->next ){
		fmt_array = ofa_iexporter_get_formats( OFA_IEXPORTER( it->data ), priv->p1_selected_type, priv->getter );
		if( fmt_array ){
			for( i=0 ; fmt_array[i].format_id ; ++i ){
				count += 1;
				gtk_list_store_insert_with_values( GTK_LIST_STORE( tmodel ), &iter, -1,
						FORMAT_COL_ID,       fmt_array[i].format_id,
						FORMAT_COL_LABEL,    fmt_array[i].format_label,
						FORMAT_COL_FORMAT,   fmt_array[i].stream_format,
						FORMAT_COL_EXPORTER, it->data,
						-1 );
			}
		}
	}

	gtk_combo_box_set_active_id( GTK_COMBO_BOX( priv->p2_format_combo ), OFA_IEXPORTER_DEFAULT_FORMAT_ID );
	gtk_widget_set_sensitive( priv->p2_format_combo, count > 0 );
}

/*
 * change the stream format when the user change the specific export format
 */
static void
p2_on_format_combo_changed( GtkComboBox *combo, ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	GtkTreeIter iter;
	GtkTreeModel *tmodel;
	ofaStreamFormat *stformat;

	priv = ofa_export_assistant_get_instance_private( self );

	if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX( priv->p2_format_combo ), &iter )){
		tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( priv->p2_format_combo ));
		gtk_tree_model_get( tmodel, &iter, FORMAT_COL_FORMAT, &stformat, -1 );
		g_return_if_fail( stformat && OFA_IS_STREAM_FORMAT( stformat ));
		g_object_unref( stformat );
		ofa_stream_format_bin_set_format( priv->p2_settings_prefs, stformat );
		priv->p2_stformat = stformat;
	}

	p2_check_for_complete( self );
}

static void
p2_on_settings_changed( ofaStreamFormatBin *bin, ofaExportAssistant *self )
{
	p2_check_for_complete( self );
}

static void
p2_on_new_profile_clicked( GtkButton *button, ofaExportAssistant *self )
{
	g_warning( "ofa_export_assistant_p2_on_new_profile_clicked: TO BE MANAGED" );
}

static void
p2_check_for_complete( ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	gboolean ok;
	gchar *message;
	GtkTreeIter iter;

	priv = ofa_export_assistant_get_instance_private( self );

	ok = TRUE;

	if( ok ){
		if( !gtk_combo_box_get_active_iter( GTK_COMBO_BOX( priv->p2_format_combo ), &iter )){
			ok = FALSE;
			message = g_strdup( _( "You have to select a specific export format" ));
		}
	}
	if( ok ){
		ok = my_ibin_is_valid( MY_IBIN( priv->p2_settings_prefs ), &message );
	}

	gtk_label_set_text( GTK_LABEL( priv->p2_message ), my_strlen( message ) ? message : "" );
	g_free( message );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );
}

static void
p2_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p2_do_forward";
	ofaExportAssistantPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gboolean ok;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	/* specific format */
	g_free( priv->p2_specific_id );
	g_free( priv->p2_specific_label );
	g_clear_object( &priv->p2_specific_exporter );

	ok = gtk_combo_box_get_active_iter( GTK_COMBO_BOX( priv->p2_format_combo ), &iter );
	g_return_if_fail( ok );

	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( priv->p2_format_combo ));
	gtk_tree_model_get( tmodel, &iter,
			FORMAT_COL_ID,       &priv->p2_specific_id,
			FORMAT_COL_LABEL,    &priv->p2_specific_label,
			FORMAT_COL_EXPORTER, &priv->p2_specific_exporter,		/* may be NULL */
			-1 );

	/* stream format */
	my_ibin_apply( MY_IBIN( priv->p2_settings_prefs ));
	g_free( priv->p2_stream_format_name );
	priv->p2_stream_format_name = g_strdup( ofa_stream_format_get_name( priv->p2_stformat ));
}

/*
 * p3: choose output file
 *
 * p3_folder_uri has been set from user setttings
 *   . either as the last export folder for this dossier
 *   . or from the default export folder from user preferences
 */
static void
p3_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p3_do_init";
	ofaExportAssistantPrivate *priv;
	gchar *basename;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	g_return_if_fail( my_strlen( priv->p1_selected_class ));

	/* target class */
	priv->p3_datatype = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-datatype" );
	g_return_if_fail( priv->p3_datatype && GTK_IS_LABEL( priv->p3_datatype ));
	my_style_add( priv->p3_datatype, "labelinfo" );

	/* specific format */
	priv->p3_specific = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-specific" );
	g_return_if_fail( priv->p3_specific && GTK_IS_LABEL( priv->p3_specific ));
	my_style_add( priv->p3_specific, "labelinfo" );

	/* stream format */
	priv->p3_format = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-format" );
	g_return_if_fail( priv->p3_format && GTK_IS_LABEL( priv->p3_format ));
	my_style_add( priv->p3_format, "labelinfo" );

	/* file chooser */
	priv->p3_chooser = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-filechooser" );
	g_return_if_fail( priv->p3_chooser && GTK_IS_FILE_CHOOSER( priv->p3_chooser ));

	g_signal_connect( priv->p3_chooser, "selection-changed", G_CALLBACK( p3_on_selection_changed ), self );
	g_signal_connect( priv->p3_chooser, "file-activated", G_CALLBACK( p3_on_file_activated ), self );

	/* build a default output filename from the last used folder
	 * plus the default basename for this data type class */
	basename = g_strdup_printf( "%s.csv", priv->p1_selected_class );
	priv->p3_output_file_uri = g_build_filename( priv->p3_folder_uri, basename, NULL );
	g_debug( "%s: p3_output_file_uri=%s", thisfn, priv->p3_output_file_uri );
	g_free( basename );
}

/*
 * output_file_uri comes:
 * - either from the default (built from folder_uri + default_basename)
 * - or has been previously selected
 */
static void
p3_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p3_do_display";
	ofaExportAssistantPrivate *priv;
	gchar *dirname, *basename;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->p3_datatype ), priv->p1_selected_label );
	gtk_label_set_text( GTK_LABEL( priv->p3_specific ), priv->p2_specific_label );
	gtk_label_set_text( GTK_LABEL( priv->p3_format ), priv->p2_stream_format_name );

	if( my_strlen( priv->p3_output_file_uri )){
		dirname = g_path_get_dirname( priv->p3_output_file_uri );
		gtk_file_chooser_set_current_folder_uri( GTK_FILE_CHOOSER( priv->p3_chooser ), dirname );
		basename = g_path_get_basename( priv->p3_output_file_uri );
		gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER( priv->p3_chooser ), basename );
		g_debug( "%s: p3_output_file_uri=%s, dirname=%s, basename=%s",
				thisfn, priv->p3_output_file_uri, dirname, basename );
		g_free( dirname );
		g_free( basename );

	} else if( my_strlen( priv->p3_folder_uri )){
		gtk_file_chooser_set_current_folder_uri( GTK_FILE_CHOOSER( priv->p3_chooser ), priv->p3_folder_uri );
		basename = g_strdup_printf( "%s.csv", priv->p1_selected_class );
		gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER( priv->p3_chooser ), basename );
		g_debug( "%s: p3_folder_uri=%s, basename=%s",
				thisfn, priv->p3_folder_uri, basename );
		g_free( basename );
	}

	p3_check_for_complete( self );
}

static void
p3_on_selection_changed( GtkFileChooser *chooser, ofaExportAssistant *self )
{
	p3_check_for_complete( self );
}

static void
p3_on_file_activated( GtkFileChooser *chooser, ofaExportAssistant *self )
{
	gboolean ok;

	ok = p3_check_for_complete( self );

	if( ok ){
		gtk_assistant_next_page( GTK_ASSISTANT( self ));
	}
}

/*
 * Building the output file from the selection in the chooser, or by
 * taking the name the user may have entered in the entry.
 *
 * As the UI is dynamic, this function is called many times
 * We so cannot ask here for user confirmation in case of an existing
 * file: this confirmation is postponed to page_forward.
 */
static gboolean
p3_check_for_complete( ofaExportAssistant *self )
{
	static const gchar *thisfn = "ofa_export_assistant_p3_check_for_complete";
	ofaExportAssistantPrivate *priv;
	gboolean ok;
	gchar *name, *final, *folder;

	priv = ofa_export_assistant_get_instance_private( self );

	g_free( priv->p3_output_file_uri );

	name = gtk_file_chooser_get_current_name( GTK_FILE_CHOOSER( priv->p3_chooser ));
	if( my_strlen( name )){
		g_debug( "%s: name=%s", thisfn, name );
		final = NULL;
		if( g_path_is_absolute( name )){
			final = g_strdup( name );
		} else {
			folder = gtk_file_chooser_get_current_folder( GTK_FILE_CHOOSER( priv->p3_chooser ));
			final = g_build_filename( folder, name, NULL );
			g_free( folder );
		}
		priv->p3_output_file_uri = g_filename_to_uri( final, NULL, NULL );
		g_free( final );

	} else {
		priv->p3_output_file_uri = gtk_file_chooser_get_uri( GTK_FILE_CHOOSER( priv->p3_chooser ));
	}
	ok = my_strlen( priv->p3_output_file_uri ) > 0 && !my_utils_uri_is_dir( priv->p3_output_file_uri );

	g_debug( "%s: p3_output_file_uri=%s, ok=%s",
			thisfn, priv->p3_output_file_uri, ok ? "True":"False" );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );

	return( ok );
}

/*
 * should be directly managed by the GtkFileChooser class, but doesn't
 * seem to work :(
 *
 * Returns: %TRUE in order to confirm overwrite.
 */
static gboolean
p3_confirm_overwrite( const ofaExportAssistant *self, const gchar *fname )
{
	gboolean ok;
	gchar *str;
	GtkWindow *toplevel;

	str = g_strdup_printf(
				_( "The file '%s' already exists.\n"
					"Are you sure you want to overwrite it ?" ), fname );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ok = my_utils_dialog_question( toplevel, str, _( "_Overwrite" ));

	g_free( str );

	return( ok );
}

static void
p3_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p3_do_forward";
	ofaExportAssistantPrivate *priv;
	gboolean ok;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	/* keep the last used folder in case we go back to this page
	 * we choose to keep the same folder, letting the user choose
	 * another basename */
	g_free( priv->p3_folder_uri );
	priv->p3_folder_uri = g_path_get_dirname( priv->p3_output_file_uri );
	g_debug( "%s: folder_uri=%s", thisfn, priv->p3_folder_uri );

	/* We cannot prevent this test to be made only here.
	 * If the user cancel, then the assistant will anyway go to the
	 * Confirmation page, without any dest uri
	 * This is because GtkAssistant does not let us stay on the same page
	 * when the user has clicked on the Next button */
	if( my_utils_uri_exists( priv->p3_output_file_uri )){
		ok = p3_confirm_overwrite( self, priv->p3_output_file_uri );
		if( !ok ){
			g_free( priv->p3_output_file_uri );
			priv->p3_output_file_uri = NULL;
		}
	}
}

/*
 * ask the user to confirm the operation
 */
static void
p4_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p4_do_init";
	ofaExportAssistantPrivate *priv;
	GtkWidget *label, *parent;
	GtkSizeGroup *group, *group_bin;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-content-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( group, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-format-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( group, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-target-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( group, label );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-stream-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->p4_format = ofa_stream_format_disp_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p4_format ));
	if(( group_bin = my_ibin_get_size_group( MY_IBIN( priv->p4_format ), 0 ))){
		my_utils_size_group_add_size_group( group, group_bin );
	}

	g_object_unref( group );
}

static void
p4_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p4_do_display";
	ofaExportAssistantPrivate *priv;
	GtkWidget *label;
	gboolean complete;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-data" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_style_add( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), priv->p1_selected_label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-specific-id" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_style_add( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), priv->p2_specific_id );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-specific-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_style_add( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), priv->p2_specific_label );

	ofa_stream_format_disp_set_format( priv->p4_format, priv->p2_stformat );

	complete = ( my_strlen( priv->p3_output_file_uri ) > 0 );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-furi" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	if( complete ){
		my_style_add( label, "labelinfo" );
		my_style_remove( label, "labelerror" );
		gtk_label_set_text( GTK_LABEL( label ), priv->p3_output_file_uri );
	} else {
		my_style_add( label, "labelerror" );
		my_style_remove( label, "labelinfo" );
		gtk_label_set_text( GTK_LABEL( label ),
				_( "Target is not set.\nPlease hit 'Back' button to select a target."));
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), complete );
}

/*
 * When executing this function, the display stays on the 'Confirmation'
 * page - The 'Result page is only displayed after this computing
 * returns.
 *
 * from a GType
 * - allocate a new object
 * - check that OFA_IS_IEXPORTABLE
 * - use OFA_IEXPORTABLE_GET_INTERFACE( instance )->method to check for an interface
 *
 * Text: Exporting <Accounts> to <filename>
 *
 * progress bar 50%
 *
 * result: 99 successfully exported records
 * provides a callback to display the progress fraction from 0.0 to 1.0
 */
static void
p5_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p5_do_display";
	ofaExportAssistantPrivate *priv;

	g_return_if_fail( OFA_IS_EXPORT_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), FALSE );

	priv = ofa_export_assistant_get_instance_private( self );

	priv->p5_page = page;
	priv->p5_exportable = OFA_IEXPORTABLE( g_object_get_data( G_OBJECT( priv->p1_selected_btn ), DATA_TYPE_INDEX ));

	g_idle_add(( GSourceFunc ) p5_export_data, self );
}

static gboolean
p5_export_data( ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	GtkWidget *label;
	gchar *str, *text;
	gboolean ok;

	priv = ofa_export_assistant_get_instance_private( self );

	/* first, export */
	ok = ofa_iexportable_export_to_uri(
			priv->p5_exportable, priv->p3_output_file_uri,
			priv->p2_specific_exporter, priv->p2_specific_id,
			priv->p2_stformat, priv->getter, MY_IPROGRESS( self ));

	/* then display the result */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-label" );

	if( ok ){
		text = g_strdup_printf( _( "OK: « %s » has been successfully exported.\n\n"
				"%ld lines have been written in '%s' output stream." ),
				priv->p1_selected_label, ofa_iexportable_get_count( priv->p5_exportable ), priv->p3_output_file_uri );
	} else {
		text = g_strdup_printf( _( "Unfortunately, « %s » export has encountered errors.\n\n"
				"The '%s' stream may be incomplete or inaccurate.\n\n"
				"Please fix these errors, and retry then." ),
				priv->p1_selected_label, priv->p3_output_file_uri );
	}

	str = g_strdup_printf( "<b>%s</b>", text );
	g_free( text );

	gtk_label_set_markup( GTK_LABEL( label ), str );
	g_free( str );

	gtk_assistant_set_page_complete( GTK_ASSISTANT( self ), priv->p5_page, TRUE );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * user settings are: class_name;
 *
 * dossier_settings are: last_export_folder_uri
 *    which defaults to default_export_folder (from user preferences)
 *    which defaults to '.'
 */
static void
read_settings( ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	GList *strlist, *it;
	const gchar *cstr, *group;
	myISettings *settings;
	gchar *key;

	priv = ofa_export_assistant_get_instance_private( self );

	/* user settings
	 */
	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, key );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->p1_selected_class = g_strdup( cstr );
	}

	my_isettings_free_string_list( settings, strlist );
	g_free( key );

	/* dossier settings
	 */
	settings = ofa_igetter_get_dossier_settings( priv->getter );
	group = ofa_idbdossier_meta_get_settings_group( priv->dossier_meta );

	priv->p3_folder_uri = my_isettings_get_string( settings, group, st_export_folder );
	if( !my_strlen( priv->p3_folder_uri )){
		g_free( priv->p3_folder_uri );
		priv->p3_folder_uri = g_strdup( ofa_prefs_export_get_default_folder( priv->getter ));
	}
	if( !my_strlen( priv->p3_folder_uri )){
		g_free( priv->p3_folder_uri );
		priv->p3_folder_uri = g_strdup( "." );
	}
	g_debug( "read_settings: p3_folder_uri=%s", priv->p3_folder_uri );
}

static void
write_settings( ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	myISettings *settings;
	gchar *key, *str;
	const gchar *group;

	priv = ofa_export_assistant_get_instance_private( self );

	/* user settings
	 */
	str = g_strdup_printf( "%s;",
			priv->p1_selected_class ? priv->p1_selected_class : "" );

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( str );
	g_free( key );

	/* dossier settings
	 */
	settings = ofa_igetter_get_dossier_settings( priv->getter );
	group = ofa_idbdossier_meta_get_settings_group( priv->dossier_meta );

	if( my_strlen( priv->p3_folder_uri )){
		my_isettings_set_string( settings, group, st_export_folder, priv->p3_folder_uri );
	}
}

/*
 * myIProgress interface management
 */
static void
iprogress_iface_init( myIProgressInterface *iface )
{
	static const gchar *thisfn = "ofa_export_assistant_iprogress_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->start_work = iprogress_start_work;
	iface->pulse = iprogress_pulse;
}

static void
iprogress_start_work( myIProgress *instance, const void *worker, GtkWidget *empty )
{
	ofaExportAssistantPrivate *priv;
	GtkWidget *parent;

	priv = ofa_export_assistant_get_instance_private( OFA_EXPORT_ASSISTANT( instance ));

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( priv->p5_page ), "p5-bar-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p5_bar = my_progress_bar_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p5_bar ));

	gtk_widget_show_all( priv->p5_page );
}

static void
iprogress_pulse( myIProgress *instance, const void *worker, gulong count, gulong total )
{
	ofaExportAssistantPrivate *priv;
	gdouble progress;
	gchar *str;

	priv = ofa_export_assistant_get_instance_private( OFA_EXPORT_ASSISTANT( instance ));

	progress = total ? ( gdouble ) count / ( gdouble ) total : 0;
	g_signal_emit_by_name( priv->p5_bar, "my-double", progress );

	str = total ? g_strdup_printf( "%ld/%ld", count, total ) : g_strdup_printf( "%ld", count );
	g_signal_emit_by_name( priv->p5_bar, "my-text", str );
	g_free( str );
}
