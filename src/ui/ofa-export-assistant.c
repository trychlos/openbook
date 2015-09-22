/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
 *
 * Open Freelance Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Freelance Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Freelance Accounting; see the file COPYING. If not,
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

#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofa-file-format.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-settings.h"
#include "api/ofo-class.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

#include "core/ofa-file-format-bin.h"

#include "ui/my-progress-bar.h"
#include "ui/ofa-export-assistant.h"
#include "ui/ofa-main-window.h"

/* private instance data
 *
 * There are 3 useful pages
 * + one introduction page (page '0')
 * + one confirmation page (page '4)
 * + one result page (page '5')
 */
struct _ofaExportAssistantPrivate {

	/* p1: introduction
	 */

	/* p2: select data type to be exported
	 */
	GSList           *p2_group;
	GtkWidget        *p2_btn;
	gint              p2_idx;
	gint              p2_last_code;

	/* p3: select format
	 */
	ofaFileFormat    *p3_export_settings;
	ofaFileFormatBin *p3_settings_prefs;

	/* p4: output file
	 */
	GtkFileChooser   *p4_chooser;
	gchar            *p4_uri;			/* the output file URI */
	gchar            *p4_last_folder;

	/* p6: apply
	 */
	myProgressBar    *p6_bar;
	ofaIExportable   *p6_base;
	GtkWidget        *p6_page;

	/* runtime data
	 */
	GtkWidget        *current_page_w;
};

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

/* The informations needed when managing the exported data types:
 * @code: the above type of data
 * @widget_name: the name of the widget in the page (not localized)
 * @base_name: the (localized) basename of the default output file
 *
 * The index to this structure will be set against each radio button.
 */
typedef GType ( *fn_type )( void );
typedef struct {
	gint         code;
	const gchar *widget_name;
	const gchar *base_name;
	fn_type      get_type;
}
	sTypes;

static sTypes st_types[] = {
		{ DATA_ACCOUNT,  "p2-account",  N_( "ofaAccounts.csv" ),     ofo_account_get_type },
		{ DATA_CLASS,    "p2-class",    N_( "ofaClasses.csv" ),      ofo_class_get_type },
		{ DATA_CURRENCY, "p2-currency", N_( "ofaCurrencies.csv" ),   ofo_currency_get_type },
		{ DATA_ENTRY,    "p2-entries",  N_( "ofaEntries.csv" ),      ofo_entry_get_type },
		{ DATA_LEDGER,   "p2-ledger",   N_( "ofaLedgers.csv" ),      ofo_ledger_get_type },
		{ DATA_MODEL,    "p2-model",    N_( "ofaOpeTemplates.csv" ), ofo_ope_template_get_type },
		{ DATA_RATE,     "p2-rate",     N_( "ofaRates.csv" ),        ofo_rate_get_type },
		{ DATA_DOSSIER,  "p2-dossier",  N_( "ofaDossier.csv" ),      ofo_dossier_get_type },
		{ 0 }
};

/* data set against each data type radio button */
#define DATA_TYPE_INDEX                 "ofa-data-type"

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-export-assistant.ui";
static const gchar *st_ui_id            = "ExportAssistant";

static const gchar *st_pref_settings    = "ExportAssistant-settings";

G_DEFINE_TYPE( ofaExportAssistant, ofa_export_assistant, MY_TYPE_ASSISTANT )

static void      p2_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void      p2_display( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static gboolean  p2_is_complete( ofaExportAssistant *self );
static void      p2_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void      p3_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void      p3_display( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void      p3_on_new_profile_clicked( GtkButton *button, ofaExportAssistant *self );
static void      p3_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void      p4_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void      p4_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void      p4_on_selection_changed( GtkFileChooser *chooser, ofaExportAssistant *self );
static void      p4_on_file_activated( GtkFileChooser *chooser, ofaExportAssistant *self );
static gboolean  p4_check_for_filename( ofaExportAssistant *self );
static void      p4_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void      p5_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static gboolean  confirm_overwrite( const ofaExportAssistant *self, const gchar *fname );
static void      p6_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static gboolean  export_data( ofaExportAssistant *self );
static void      p6_on_progress( ofaIExportable *exportable, gdouble progress, const gchar *text, ofaExportAssistant *self );
static void      get_settings( ofaExportAssistant *self );
static void      set_settings( ofaExportAssistant *self );

static const ofsAssistant st_pages_cb [] = {
		{ ASSIST_PAGE_INTRO,
				NULL,
				NULL,
				NULL },
		{ ASSIST_PAGE_SELECT,
				( myAssistantCb ) p2_do_init,
				( myAssistantCb ) p2_display,
				( myAssistantCb ) p2_do_forward },
		{ ASSIST_PAGE_FORMAT,
				( myAssistantCb ) p3_do_init,
				( myAssistantCb ) p3_display,
				( myAssistantCb ) p3_do_forward },
		{ ASSIST_PAGE_OUTPUT,
				( myAssistantCb ) p4_do_init,
				( myAssistantCb ) p4_do_display,
				( myAssistantCb ) p4_do_forward },
		{ ASSIST_PAGE_CONFIRM,
				NULL,
				( myAssistantCb ) p5_do_display,
				NULL },
		{ ASSIST_PAGE_DONE,
				NULL,
				( myAssistantCb ) p6_do_display,
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
	priv = OFA_EXPORT_ASSISTANT( instance )->priv;
	g_free( priv->p4_last_folder );
	g_free( priv->p4_uri );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_export_assistant_parent_class )->finalize( instance );
}

static void
export_dispose( GObject *instance )
{
	ofaExportAssistantPrivate *priv;

	g_return_if_fail( instance && OFA_IS_EXPORT_ASSISTANT( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		priv = OFA_EXPORT_ASSISTANT( instance )->priv;

		/* unref object members here */
		g_clear_object( &priv->p3_export_settings );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_export_assistant_parent_class )->dispose( instance );
}

static void
ofa_export_assistant_init( ofaExportAssistant *self )
{
	static const gchar *thisfn = "ofa_export_assistant_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXPORT_ASSISTANT( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_EXPORT_ASSISTANT, ofaExportAssistantPrivate );

	self->priv->p2_last_code = -1;
}

static void
ofa_export_assistant_class_init( ofaExportAssistantClass *klass )
{
	static const gchar *thisfn = "ofa_export_assistant_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = export_dispose;
	G_OBJECT_CLASS( klass )->finalize = export_finalize;

	g_type_class_add_private( klass, sizeof( ofaExportAssistantPrivate ));
}

/**
 * Run the assistant.
 *
 * @main: the main window of the application.
 */
void
ofa_export_assistant_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_export_assistant_run";
	ofaExportAssistant *self;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, main_window );

	self = g_object_new( OFA_TYPE_EXPORT_ASSISTANT,
							MY_PROP_MAIN_WINDOW, main_window,
							MY_PROP_WINDOW_XML,  st_ui_xml,
							MY_PROP_WINDOW_NAME, st_ui_id,
							NULL );

	my_assistant_set_callbacks( MY_ASSISTANT( self ), st_pages_cb );
	get_settings( self );

	my_assistant_run( MY_ASSISTANT( self ));
}

/*
 * p1: type of the data to export
 *
 * Selection is radio-button based, so we are sure that at most
 * one exported data type is selected
 * and we make sure that at least one button is active
 *
 * so the page is always completed
 */
static void
p2_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p2_do_init";
	ofaExportAssistantPrivate *priv;
	GtkWidget *btn;
	gint i;
	gboolean found;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;
	found = FALSE;

	for( i=0 ; st_types[i].code ; ++i ){

		btn = my_utils_container_get_child_by_name(
					GTK_CONTAINER( page ), st_types[i].widget_name );
		g_return_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ));

		g_object_set_data( G_OBJECT( btn ), DATA_TYPE_INDEX, GINT_TO_POINTER( i ));

		if( !priv->p2_group ){
			priv->p2_group = gtk_radio_button_get_group( GTK_RADIO_BUTTON( btn ));
		}

		if( st_types[i].code == priv->p2_last_code ){
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( btn ), TRUE );
			found = TRUE;
		}
	}

	if( !found ){
		g_return_if_fail( priv->p2_group && priv->p2_group->data );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->p2_group->data ), TRUE );
	}
}

static void
p2_display( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	gboolean is_complete;

	is_complete = p2_is_complete( self );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), page, is_complete );
}

static gboolean
p2_is_complete( ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	GSList *it;

	priv = self->priv;

	g_return_val_if_fail( priv->p2_group, FALSE );

	priv->p2_btn = NULL;
	priv->p2_idx = -1;

	/* which is the currently active button ? */
	for( it=priv->p2_group ; it ; it=it->next ){
		if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( it->data ))){
			priv->p2_idx = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( it->data ), DATA_TYPE_INDEX ));
			priv->p2_btn = GTK_WIDGET( it->data );
			break;;
		}
	}

	return( priv->p2_idx >= 0 && priv->p2_btn );
}

static void
p2_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p2_do_forward";
	ofaExportAssistantPrivate *priv;

	g_debug( "%s: self=%p, page=%p", thisfn, ( void * ) self, ( void * ) page );

	if( p2_is_complete( self )){

		priv = self->priv;
		g_debug( "%s: idx=%d", thisfn, priv->p2_idx );
		priv->p2_last_code = st_types[priv->p2_idx].code;
		set_settings( self );
	}
}

/*
 * p2: export format
 *
 * These are initialized with the export settings for this name, or
 * with default settings
 */
static void
p3_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p3_do_init";
	ofaExportAssistantPrivate *priv;
	GtkWidget *widget;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-alignment-parent" );
	g_return_if_fail( widget && GTK_IS_CONTAINER( widget ));

	priv->p3_export_settings = ofa_file_format_new( SETTINGS_EXPORT_SETTINGS );
	priv->p3_settings_prefs = ofa_file_format_bin_new( priv->p3_export_settings );
	gtk_container_add( GTK_CONTAINER( widget ), GTK_WIDGET( priv->p3_settings_prefs ));

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-new-btn" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( p3_on_new_profile_clicked ), self );
}

static void
p3_display( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	my_assistant_set_page_complete( MY_ASSISTANT( self ), page, TRUE );
}

static void
p3_on_new_profile_clicked( GtkButton *button, ofaExportAssistant *self )
{
	g_warning( "ofa_export_assistant_p3_on_new_profile_clicked: TO BE MANAGED" );
}

static void
p3_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p3_do_forward";
	ofaExportAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	ofa_file_format_bin_apply( priv->p3_settings_prefs );
}

/*
 * p3: choose output file
 */
static void
p4_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p4_do_init";
	ofaExportAssistantPrivate *priv;
	gchar *dirname;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;
	priv->current_page_w = page;

	g_return_if_fail( priv->p2_idx >= 0 );

	priv->p4_chooser =
			( GtkFileChooser * ) my_utils_container_get_child_by_name(
					GTK_CONTAINER( page ), "p4-filechooser" );
	g_return_if_fail( priv->p4_chooser && GTK_IS_FILE_CHOOSER( priv->p4_chooser ));

	g_signal_connect( G_OBJECT( priv->p4_chooser ),
			"selection-changed", G_CALLBACK( p4_on_selection_changed ), self );

	g_signal_connect( G_OBJECT( priv->p4_chooser ),
			"file-activated", G_CALLBACK( p4_on_file_activated ), self );

	/* build a default output filename from the last used folder
	 * plus the default basename for this data type class */
	if( my_strlen( priv->p4_uri )){
		dirname = g_path_get_dirname( priv->p4_uri );
	} else {
		dirname = g_strdup( ofa_settings_get_string( SETTINGS_EXPORT_FOLDER ));
	}
	g_free( priv->p4_uri );
	priv->p4_uri = g_build_filename( dirname, st_types[priv->p2_idx].base_name, NULL );

	g_free( dirname );
}

static void
p4_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p4_do_display";
	ofaExportAssistantPrivate *priv;
	gchar *dirname, *basename;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	if( my_strlen( priv->p4_uri )){
		dirname = g_path_get_dirname( priv->p4_uri );
		gtk_file_chooser_set_current_folder_uri( priv->p4_chooser, dirname );
		g_free( dirname );
		basename = g_path_get_basename( priv->p4_uri );
		gtk_file_chooser_set_current_name( priv->p4_chooser, basename );
		g_free( basename );

	} else if( my_strlen( priv->p4_last_folder )){
		gtk_file_chooser_set_current_folder( priv->p4_chooser, priv->p4_last_folder );
	}
}

static void
p4_on_selection_changed( GtkFileChooser *chooser, ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	gboolean ok;

	priv =self->priv;

	ok = p4_check_for_filename( self );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_w, ok );
}

static void
p4_on_file_activated( GtkFileChooser *chooser, ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	gboolean ok;

	priv =self->priv;

	ok = p4_check_for_filename( self );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_w, ok );
}

static gboolean
p4_check_for_filename( ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	gboolean ok;
	gchar *name, *final, *folder;

	priv = self->priv;
	g_free( priv->p4_uri );

	name = gtk_file_chooser_get_current_name( priv->p4_chooser );

	if( my_strlen( name )){
		final = NULL;
		if( g_path_is_absolute( name )){
			final = g_strdup( name );
		} else {
			folder = gtk_file_chooser_get_current_folder( priv->p4_chooser );
			final = g_build_filename( folder, name, NULL );
			g_free( folder );
		}
		g_debug( "p4_check_for_filename: final=%s", final );
		priv->p4_uri = g_filename_to_uri( final, NULL, NULL );
		g_free( final );

	} else {
		priv->p4_uri = gtk_file_chooser_get_uri( priv->p4_chooser );
	}

	g_free( name );

	ok = my_strlen( priv->p4_uri ) > 0;

	return( ok );
}

static void
p4_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p4_do_forward";
	ofaExportAssistantPrivate *priv;
	gboolean complete;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	/* keep the last used folder in case we go back to this page
	 * we choose to keep the same folder, letting the user choose
	 * another basename */
	g_free( priv->p4_last_folder );
	priv->p4_last_folder = g_path_get_dirname( priv->p4_uri );

	complete = p4_check_for_filename( self );

	if( my_utils_uri_exists( priv->p4_uri )){
		complete = confirm_overwrite( self, priv->p4_uri );
		if( !complete ){
			g_free( priv->p4_uri );
			priv->p4_uri = NULL;
		}
	}

	if( complete ){
		set_settings( self );
	}
}

/*
 * ask the user to confirm the operation
 */
static void
p5_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p5_do_prepare_confirm";
	ofaExportAssistantPrivate *priv;
	GtkWidget *label;
	gchar *str;
	gint format;
	gboolean complete;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-data" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelemphasis1" );
	str = my_utils_str_remove_underlines( gtk_button_get_label( GTK_BUTTON( priv->p2_btn )));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-charmap" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelemphasis1" );
	gtk_label_set_text( GTK_LABEL( label ), ofa_file_format_get_charmap( priv->p3_export_settings ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-date" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelemphasis1" );
	format = ofa_file_format_get_date_format( priv->p3_export_settings );
	gtk_label_set_text( GTK_LABEL( label ), my_date_get_format_str( format ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-decimal" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelemphasis1" );
	str = g_strdup_printf( "%c", ofa_file_format_get_decimal_sep( priv->p3_export_settings ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-field" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelemphasis1" );
	str = g_strdup_printf( "%c", ofa_file_format_get_field_sep( priv->p3_export_settings ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-headers" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelemphasis1" );
	str = g_strdup(
			ofa_file_format_has_headers( priv->p3_export_settings ) ? _( "True" ) : _( "False" ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-output" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelemphasis1" );
	gtk_label_set_text( GTK_LABEL( label ), priv->p4_uri );

	complete = ( my_strlen( priv->p4_uri ) > 0 );
	my_assistant_set_page_complete( MY_ASSISTANT( self ), page, complete );
}

/*
 * should be directly managed by the GtkFileChooser class, but doesn't
 * seem to work :(
 *
 * Returns: %TRUE in order to confirm overwrite.
 */
static gboolean
confirm_overwrite( const ofaExportAssistant *self, const gchar *fname )
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
p6_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p6_do_display";
	ofaExportAssistantPrivate *priv;
	GtkWidget *parent;

	g_return_if_fail( OFA_IS_EXPORT_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	my_assistant_set_page_complete( MY_ASSISTANT( self ), page, FALSE );

	priv = self->priv;

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-bar-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p6_bar = my_progress_bar_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p6_bar ));

	priv->p6_page = page;
	gtk_widget_show_all( page );

	priv->p6_base = ( ofaIExportable * ) g_object_new( st_types[priv->p2_idx].get_type(), NULL );
	if( !OFA_IS_IEXPORTABLE( priv->p6_base )){
		my_utils_dialog_warning( _( "The requested type does not implement the IExportable interface" ));
		return;
	}

	/* message provided by the ofaIExportable interface */
	g_signal_connect(
			G_OBJECT( priv->p6_base ), "ofa-progress", G_CALLBACK( p6_on_progress ), self );

	g_idle_add(( GSourceFunc ) export_data, self );
}

static gboolean
export_data( ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;
	GtkWidget *label;
	gchar *str, *text;
	gboolean ok;

	priv = self->priv;

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	/* first, export */
	ok = ofa_iexportable_export_to_path(
			priv->p6_base, priv->p4_uri, priv->p3_export_settings, dossier, self );

	/* then display the result */
	label = my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )) ), "p6-label" );

	str = my_utils_str_remove_underlines( gtk_button_get_label( GTK_BUTTON( priv->p2_btn )));
	if( ok ){
		text = g_strdup_printf( _( "OK: « %s » has been successfully exported.\n\n"
				"%ld lines have been written in '%s' output file." ),
				str, ofa_iexportable_get_count( priv->p6_base ), priv->p4_uri );
	} else {
		text = g_strdup_printf( _( "Unfortunately, « %s » export has encountered errors.\n\n"
				"The '%s' file may be incomplete or inaccurate.\n\n"
				"Please fix these errors, and retry then." ), str, priv->p4_uri );
	}
	g_free( str );

	str = g_strdup_printf( "<b>%s</b>", text );
	g_free( text );

	gtk_label_set_markup( GTK_LABEL( label ), str );
	g_free( str );

	/* unref the reference object _after_ having built the label so
	 * we always have access to its internal datas */
	g_object_unref( priv->p6_base );

	gtk_assistant_set_page_complete(
			GTK_ASSISTANT( my_window_get_toplevel( MY_WINDOW( self ))), priv->p6_page, TRUE );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

static void
p6_on_progress( ofaIExportable *exportable, gdouble progress, const gchar *text, ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;

	priv = self->priv;

	g_signal_emit_by_name( priv->p6_bar, "ofa-double", progress );
	g_signal_emit_by_name( priv->p6_bar, "ofa-text", text );
}

/*
 * settings are:
 * content;uri;
 */
static void
get_settings( ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	GList *slist, *it;
	const gchar *cstr;

	priv = self->priv;

	slist = ofa_settings_get_string_list( st_pref_settings );

	it = slist;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->p2_last_code = atoi( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->p4_uri = g_strdup( cstr );
	}

	ofa_settings_free_string_list( slist );
}

static void
set_settings( ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	gchar *str;

	priv = self->priv;

	str = g_strdup_printf( "%d;%s;",
			priv->p2_last_code,
			priv->p4_uri ? priv->p4_uri : "" );

	ofa_settings_set_string( st_pref_settings, str );

	g_free( str );
}
