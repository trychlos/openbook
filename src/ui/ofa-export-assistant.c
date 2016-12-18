/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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
#include "my/my-isettings.h"
#include "my/my-iwindow.h"
#include "my/my-progress-bar.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-igetter.h"
#include "api/ofa-preferences.h"
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
	GtkWindow           *parent;

	/* runtime
	 */
	ofaHub              *hub;
	ofaIDBDossierMeta   *meta;

	/* p0: introduction
	 */

	/* p1: select data type to be exported
	 */
	GList               *p1_exportables;
	GtkWidget           *p1_parent;
	gint                 p1_row;
	GtkWidget           *p1_selected_btn;
	gchar               *p1_selected_class;
	gchar               *p1_selected_label;

	/* p2: select format
	 */
	GtkWidget           *p2_datatype;
	ofaStreamFormat     *p2_export_settings;
	ofaStreamFormatBin  *p2_settings_prefs;
	GtkWidget           *p2_message;
	gchar               *p2_format;

	/* p3: output file
	 */
	GtkWidget           *p3_datatype;
	GtkWidget           *p3_format;
	GtkFileChooser      *p3_chooser;
	gchar               *p3_furi;			/* the output file URI */
	gchar               *p3_last_folder;

	/* p4: confirm
	 */
	ofaStreamFormatDisp *p4_format;

	/* p5: apply
	 */
	myProgressBar       *p5_bar;
	ofaIExportable      *p5_base;
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

/* data set against each data type radio button */
#define DATA_TYPE_INDEX                   "ofa-data-type"

static const gchar *st_export_folder    = "ofa-LastExportFolder";
static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-export-assistant.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     iwindow_read_settings( myIWindow *instance, myISettings *settings, const gchar *keyname );
static void     write_settings( ofaExportAssistant *self );
static void     iassistant_iface_init( myIAssistantInterface *iface );
static gboolean iassistant_is_willing_to_quit( myIAssistant*instance, guint keyval );
static void     p0_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page_widget );
static void     p1_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void     p1_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void     p1_on_type_toggled( GtkToggleButton *button, ofaExportAssistant *self );
static gboolean p1_is_complete( ofaExportAssistant *self );
static void     p1_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void     p2_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void     p2_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page );
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
export_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_export_assistant_finalize";
	ofaExportAssistantPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EXPORT_ASSISTANT( instance ));

	/* free data members here */
	priv = ofa_export_assistant_get_instance_private( OFA_EXPORT_ASSISTANT( instance ));

	g_free( priv->p1_selected_class );
	g_free( priv->p1_selected_label );
	g_free( priv->p2_format );
	g_free( priv->p3_last_folder );
	g_free( priv->p3_furi );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_export_assistant_parent_class )->finalize( instance );
}

static void
export_dispose( GObject *instance )
{
	ofaExportAssistantPrivate *priv;

	g_return_if_fail( instance && OFA_IS_EXPORT_ASSISTANT( instance ));

	priv = ofa_export_assistant_get_instance_private( OFA_EXPORT_ASSISTANT( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		g_clear_object( &priv->meta );
		g_list_free_full( priv->p1_exportables, ( GDestroyNotify ) g_object_unref );
		g_clear_object( &priv->p2_export_settings );
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

	priv->p1_exportables = NULL;
	priv->p2_export_settings = NULL;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_export_assistant_class_init( ofaExportAssistantClass *klass )
{
	static const gchar *thisfn = "ofa_export_assistant_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = export_dispose;
	G_OBJECT_CLASS( klass )->finalize = export_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_export_assistant_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 *
 * Run the assistant.
 */
void
ofa_export_assistant_run( ofaIGetter *getter, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_export_assistant_run";
	ofaExportAssistant *self;
	ofaExportAssistantPrivate *priv;;

	g_debug( "%s: getter=%p, parent=%p", thisfn, ( void * ) getter, ( void * ) parent );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_EXPORT_ASSISTANT, NULL );

	priv = ofa_export_assistant_get_instance_private( self );

	priv->getter = ofa_igetter_get_permanent_getter( getter );
	priv->parent = parent;

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
	iface->read_settings = iwindow_read_settings;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_export_assistant_iwindow_init";
	ofaExportAssistantPrivate *priv;
	const ofaIDBConnect *connect;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_export_assistant_get_instance_private( OFA_EXPORT_ASSISTANT( instance ));

	my_iwindow_set_parent( instance, priv->parent );

	priv->hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	my_iwindow_set_geometry_settings( instance, ofa_hub_get_user_settings( priv->hub ));

	my_iassistant_set_callbacks( MY_IASSISTANT( instance ), st_pages_cb );

	connect = ofa_hub_get_connect( priv->hub );
	priv->meta = ofa_idbconnect_get_dossier_meta( connect );
}

/*
 * user settings are: class_name;
 * dossier_settings are: last_export_folder_uri
 */
static void
iwindow_read_settings( myIWindow *instance, myISettings *settings, const gchar *keyname )
{
	ofaExportAssistantPrivate *priv;
	GList *slist, *it;
	const gchar *cstr;
	myISettings *dossier_settings;
	gchar *group;

	priv = ofa_export_assistant_get_instance_private( OFA_EXPORT_ASSISTANT( instance ));

	slist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, keyname );

	it = slist;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->p1_selected_class = g_strdup( cstr );
	}

	my_isettings_free_string_list( settings, slist );

	dossier_settings = ofa_hub_get_dossier_settings( priv->hub );
	group = ofa_idbdossier_meta_get_group_name( priv->meta );

	priv->p3_furi = my_isettings_get_string( dossier_settings, group, st_export_folder );

	g_free( group );
}

static void
write_settings( ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	myISettings *settings;
	gchar *keyname, *str, *group;

	priv = ofa_export_assistant_get_instance_private( self );

	settings = my_iwindow_get_settings( MY_IWINDOW( self ));
	keyname = my_iwindow_get_keyname( MY_IWINDOW( self ));

	str = g_strdup_printf( "%s;",
			priv->p1_selected_class ? priv->p1_selected_class : "" );

	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, keyname, str );

	g_free( str );
	g_free( keyname );

	settings = ofa_hub_get_dossier_settings( priv->hub );
	group = ofa_idbdossier_meta_get_group_name( priv->meta );

	if( my_strlen( priv->p3_last_folder )){
		my_isettings_set_string( settings, group, st_export_folder, priv->p3_last_folder );
	}

	g_free( group );
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

	return( ofa_prefs_assistant_is_willing_to_quit( priv->hub, keyval ));
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

	priv->p1_exportables = ofa_hub_get_for_type( priv->hub, OFA_TYPE_IEXPORTABLE );
	g_debug( "%s: exportables count=%d", thisfn, g_list_length( priv->p1_exportables ));
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
	write_settings( self );
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
	GtkSizeGroup *hgroup;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	priv->p2_datatype = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-datatype" );
	g_return_if_fail( priv->p2_datatype && GTK_IS_LABEL( priv->p2_datatype ));
	my_style_add( priv->p2_datatype, "labelinfo" );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-settings-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->p2_settings_prefs = ofa_stream_format_bin_new( NULL );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p2_settings_prefs ));
	ofa_stream_format_bin_set_mode_sensitive( priv->p2_settings_prefs, FALSE );

	g_signal_connect( priv->p2_settings_prefs, "ofa-changed", G_CALLBACK( p2_on_settings_changed ), self );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-new-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( button, "clicked", G_CALLBACK( p2_on_new_profile_clicked ), self );

	priv->p2_message = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-message" );
	g_return_if_fail( priv->p2_message && GTK_IS_LABEL( priv->p2_message ));
	my_style_add( priv->p2_message, "labelerror" );

	hgroup = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-label221" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	my_utils_size_group_add_size_group(
			hgroup, ofa_stream_format_bin_get_size_group( priv->p2_settings_prefs, 0 ));

	g_object_unref( hgroup );
}

static void
p2_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p2_do_display";
	ofaExportAssistantPrivate *priv;
	const gchar *found_key;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	/* previously set */
	gtk_label_set_text( GTK_LABEL( priv->p2_datatype ), priv->p1_selected_label );

	/* get a suitable format */
	found_key = NULL;
	if( ofa_stream_format_exists( priv->hub, priv->p1_selected_class, OFA_SFMODE_EXPORT )){
		found_key = priv->p1_selected_class;
	}

	g_clear_object( &priv->p2_export_settings );
	priv->p2_export_settings = ofa_stream_format_new( priv->hub, found_key, OFA_SFMODE_EXPORT );
	ofa_stream_format_bin_set_format( priv->p2_settings_prefs, priv->p2_export_settings );

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

	priv = ofa_export_assistant_get_instance_private( self );

	ok = ofa_stream_format_bin_is_valid( priv->p2_settings_prefs, &message );

	gtk_label_set_text( GTK_LABEL( priv->p2_message ), my_strlen( message ) ? message : "" );
	g_free( message );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );
}

static void
p2_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p2_do_forward";
	ofaExportAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	ofa_stream_format_bin_apply( priv->p2_settings_prefs );

	g_free( priv->p2_format );
	priv->p2_format = g_strdup( ofa_stream_format_get_name( priv->p2_export_settings ));
}

/*
 * p3: choose output file
 */
static void
p3_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p3_do_init";
	ofaExportAssistantPrivate *priv;
	gchar *dirname, *basename;
	myISettings *settings;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	g_return_if_fail( my_strlen( priv->p1_selected_class ));

	priv->p3_datatype = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-datatype" );
	g_return_if_fail( priv->p3_datatype && GTK_IS_LABEL( priv->p3_datatype ));
	my_style_add( priv->p3_datatype, "labelinfo" );

	priv->p3_format = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-format" );
	g_return_if_fail( priv->p3_format && GTK_IS_LABEL( priv->p3_format ));
	my_style_add( priv->p3_format, "labelinfo" );

	priv->p3_chooser =
			( GtkFileChooser * ) my_utils_container_get_child_by_name(
					GTK_CONTAINER( page ), "p3-filechooser" );
	g_return_if_fail( priv->p3_chooser && GTK_IS_FILE_CHOOSER( priv->p3_chooser ));

	g_signal_connect(
			priv->p3_chooser, "selection-changed", G_CALLBACK( p3_on_selection_changed ), self );
	g_signal_connect(
			priv->p3_chooser, "file-activated", G_CALLBACK( p3_on_file_activated ), self );

	/* build a default output filename from the last used folder
	 * plus the default basename for this data type class */
	settings = ofa_hub_get_user_settings( priv->hub );
	if( my_strlen( priv->p3_furi )){
		dirname = g_strdup( priv->p3_furi );
	} else {
		dirname = my_isettings_get_string( settings, HUB_USER_SETTINGS_GROUP, HUB_USER_SETTINGS_EXPORT_FOLDER );
		if( !my_strlen( dirname )){
			g_free( dirname );
			dirname = g_strdup( "." );
		}
	}
	basename = g_strdup_printf( "%s.csv", priv->p1_selected_class );

	g_free( priv->p3_furi );
	priv->p3_furi = g_build_filename( dirname, basename, NULL );
	g_debug( "%s: p3_furi=%s", thisfn, priv->p3_furi );

	g_free( basename );
	g_free( dirname );
}

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
	gtk_label_set_text( GTK_LABEL( priv->p3_format ), priv->p2_format );

	if( my_strlen( priv->p3_furi )){
		dirname = g_path_get_dirname( priv->p3_furi );
		gtk_file_chooser_set_current_folder_uri( priv->p3_chooser, dirname );
		g_free( dirname );
		basename = g_path_get_basename( priv->p3_furi );
		gtk_file_chooser_set_current_name( priv->p3_chooser, basename );
		g_free( basename );
		g_debug( "%s: p3_furi=%s, dirname=%s, basename=%s",
				thisfn, priv->p3_furi, dirname, basename );

	} else if( my_strlen( priv->p3_last_folder )){
		gtk_file_chooser_set_current_folder( priv->p3_chooser, priv->p3_last_folder );
		basename = g_build_filename( priv->p1_selected_class, ".csv", NULL );
		gtk_file_chooser_set_current_name( priv->p3_chooser, basename );
		g_free( basename );
		g_debug( "%s: p3_last_folder=%s, basename=%s",
				thisfn, priv->p3_last_folder, basename );
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

static gboolean
p3_check_for_complete( ofaExportAssistant *self )
{
	static const gchar *thisfn = "ofa_export_assistant_p3_check_for_complete";
	ofaExportAssistantPrivate *priv;
	gboolean ok;
	gchar *name, *final, *folder;

	priv = ofa_export_assistant_get_instance_private( self );

	g_free( priv->p3_furi );

	name = gtk_file_chooser_get_current_name( priv->p3_chooser );
	if( my_strlen( name )){
		g_debug( "%s: name=%s", thisfn, name );
		final = NULL;
		if( g_path_is_absolute( name )){
			final = g_strdup( name );
		} else {
			folder = gtk_file_chooser_get_current_folder( priv->p3_chooser );
			final = g_build_filename( folder, name, NULL );
			g_free( folder );
		}
		priv->p3_furi = g_filename_to_uri( final, NULL, NULL );
		g_free( final );

	} else {
		priv->p3_furi = gtk_file_chooser_get_uri( priv->p3_chooser );
	}
	g_free( name );
	g_debug( "%s: p3_furi=%s", thisfn, priv->p3_furi );

	ok = my_strlen( priv->p3_furi ) > 0 && !my_utils_uri_is_dir( priv->p3_furi );

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

	str = g_strdup_printf(
				_( "The file '%s' already exists.\n"
					"Are you sure you want to overwrite it ?" ), fname );

	ok = my_utils_dialog_question( str, _( "_Overwrite" ));

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
	g_free( priv->p3_last_folder );
	priv->p3_last_folder = g_path_get_dirname( priv->p3_furi );

	/* we cannot prevent this test to be made only here
	 * if the user cancel, then the assistant will anyway go to the
	 * Confirmation page, without any dest uri
	 * This is because GtkAssistant does not let us stay on the same page
	 * when the user has clicked on the Next button */
	if( my_utils_uri_exists( priv->p3_furi )){
		ok = p3_confirm_overwrite( self, priv->p3_furi );
		if( !ok ){
			g_free( priv->p3_furi );
			priv->p3_furi = NULL;
		}
	}

	write_settings( self );
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
	GtkSizeGroup *group;

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
	my_utils_size_group_add_size_group(
			group, ofa_stream_format_disp_get_size_group( priv->p4_format, 0 ));

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

	ofa_stream_format_disp_set_format( priv->p4_format, priv->p2_export_settings );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-furi" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_style_add( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), priv->p3_furi );

	complete = ( my_strlen( priv->p3_furi ) > 0 );
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
	priv->p5_base = OFA_IEXPORTABLE( g_object_get_data( G_OBJECT( priv->p1_selected_btn ), DATA_TYPE_INDEX ));

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
			priv->p5_base, priv->p3_furi, priv->p2_export_settings, priv->hub, MY_IPROGRESS( self ));

	/* then display the result */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-label" );

	if( ok ){
		text = g_strdup_printf( _( "OK: « %s » has been successfully exported.\n\n"
				"%ld lines have been written in '%s' output stream." ),
				priv->p1_selected_label, ofa_iexportable_get_count( priv->p5_base ), priv->p3_furi );
	} else {
		text = g_strdup_printf( _( "Unfortunately, « %s » export has encountered errors.\n\n"
				"The '%s' stream may be incomplete or inaccurate.\n\n"
				"Please fix these errors, and retry then." ),
				priv->p1_selected_label, priv->p3_furi );
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
