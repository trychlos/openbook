/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "api/my-utils.h"
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

#include "core/my-window-prot.h"
#include "core/ofa-file-format.h"

#include "ui/ofa-export-assistant.h"
#include "ui/ofa-file-format-piece.h"
#include "ui/ofa-main-window.h"

/* private instance data
 *
 * There are 3 useful pages
 * + one introduction page (page '0')
 * + one confirmation page (page '4)
 * + one result page (page '5')
 */
struct _ofaExportAssistantPrivate {

	/* p1: select data type to be exported
	 */
	GSList             *p1_group;
	GtkWidget          *p1_btn;
	gint                p1_idx;

	/* p2: select format
	 */
	ofaFileFormatPiece *p2_settings_prefs;
	ofaFileFormat      *p2_export_settings;

	/* p3: output file
	 */
	GtkFileChooser     *p3_chooser;
	gchar              *p3_fname;		/* the output file */
	gchar              *p3_last_folder;

	/* p5: apply
	 */
	GtkWidget          *progress_bar;
	ofaIExportable     *base;
	GtkWidget          *p5_page;
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
		{ DATA_ACCOUNT,  "p1-account",  N_( "ofaAccounts.csv" ),   ofo_account_get_type },
		{ DATA_CLASS,    "p1-class",    N_( "ofaClasses.csv" ),    ofo_class_get_type },
		{ DATA_CURRENCY, "p1-currency", N_( "ofaCurrencies.csv" ), ofo_currency_get_type },
		{ DATA_ENTRY,    "p1-entries",  N_( "ofaEntries.csv" ),    ofo_entry_get_type },
		{ DATA_LEDGER,   "p1-ledger",   N_( "ofaLedgers.csv" ),    ofo_ledger_get_type },
		{ DATA_MODEL,    "p1-model",    N_( "ofaModels.csv" ),     ofo_ope_template_get_type },
		{ DATA_RATE,     "p1-rate",     N_( "ofaRates.csv" ),      ofo_rate_get_type },
		{ DATA_DOSSIER,  "p1-dossier",  N_( "ofaDossier.csv" ),    ofo_dossier_get_type },
		{ 0 }
};

/* data set against each data type radio button */
#define DATA_TYPE_INDEX                 "ofa-data-type"

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-export-assistant.ui";
static const gchar *st_ui_id            = "ExportAssistant";

static const gchar *st_pref_data        = "ExportAssistantContent";
static const gchar *st_pref_fname       = "ExportAssistantFilename";

G_DEFINE_TYPE( ofaExportAssistant, ofa_export_assistant, MY_TYPE_ASSISTANT )

static void      on_page_forward( ofaExportAssistant *self, GtkWidget *page, gint page_num, void *empty );
static void      on_prepare( GtkAssistant *assistant, GtkWidget *page, ofaExportAssistant *self );
static void      p1_do_init( ofaExportAssistant *self, GtkAssistant *assistant, GtkWidget *page );
static gboolean  p1_is_complete( ofaExportAssistant *self );
static void      p1_do_forward( ofaExportAssistant *self, GtkWidget *page );
static void      p2_do_init( ofaExportAssistant *self, GtkAssistant *assistant, GtkWidget *page );
static void      p2_on_new_profile_clicked( GtkButton *button, ofaExportAssistant *self );
static void      p2_do_forward( ofaExportAssistant *self, GtkWidget *page );
static void      p3_do_init( ofaExportAssistant *self, GtkAssistant *assistant, GtkWidget *page );
static void      p3_do_display( ofaExportAssistant *self, GtkAssistant *assistant, GtkWidget *page );
static void      p3_on_file_activated( GtkFileChooser *chooser, ofaExportAssistant *self );
static void      p3_do_forward( ofaExportAssistant *self, GtkWidget *page );
static void      p4_do_display( ofaExportAssistant *self, GtkAssistant *assistant, GtkWidget *page );
static gboolean  confirm_overwrite( const ofaExportAssistant *self, const gchar *fname );
static void      on_apply( GtkAssistant *assistant, ofaExportAssistant *self );
static void      p5_do_display( ofaExportAssistant *self, GtkAssistant *assistant, GtkWidget *page );
static gboolean  export_data( ofaExportAssistant *self );
static void      error_no_interface( const ofaExportAssistant *self );
static void      set_progress_double( const ofaExportAssistant *self, gdouble progress );
static void      set_progress_text( const ofaExportAssistant *self, const gchar *text );

typedef void ( *cb )( ofaExportAssistant *, GtkWidget * );

typedef struct {
	gint  page_num;
	cb    forward_cb;
}
	sPagesCB;

static const sPagesCB st_pages_cb [] = {
		{ ASSIST_PAGE_INTRO,   NULL },
		{ ASSIST_PAGE_SELECT,  p1_do_forward },
		{ ASSIST_PAGE_FORMAT,  p2_do_forward },
		{ ASSIST_PAGE_OUTPUT,  p3_do_forward },
		{ ASSIST_PAGE_CONFIRM, NULL },
		{ ASSIST_PAGE_DONE,    NULL },
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
	g_free( priv->p3_last_folder );
	g_free( priv->p3_fname );

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
		g_clear_object( &priv->p2_export_settings );
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
							MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
							MY_PROP_WINDOW_XML,  st_ui_xml,
							MY_PROP_WINDOW_NAME, st_ui_id,
							NULL );

	g_signal_connect(
			G_OBJECT( self ), MY_SIGNAL_PAGE_FORWARD, G_CALLBACK( on_page_forward ), NULL );

	my_assistant_signal_connect( MY_ASSISTANT( self ), "prepare", G_CALLBACK( on_prepare ));
	my_assistant_signal_connect( MY_ASSISTANT( self ), "apply", G_CALLBACK( on_apply ));

	my_assistant_run( MY_ASSISTANT( self ));
}

/*
 * the provided 'page' is the toplevel widget of the assistant's page
 */
static void
on_page_forward( ofaExportAssistant *self, GtkWidget *page, gint page_num, void *empty )
{
	static const gchar *thisfn = "ofa_export_assistant_on_page_forward";
	gint i;

	g_return_if_fail( self && OFA_IS_EXPORT_ASSISTANT( self ));
	g_return_if_fail( page && GTK_IS_WIDGET( page ));

	g_debug( "%s: self=%p, page=%p, page_num=%d, empty=%p",
			thisfn, ( void * ) self, ( void * ) page, page_num, ( void * ) empty );

	for( i=0 ; st_pages_cb[i].page_num >= 0 ; ++i ){
		if( st_pages_cb[i].page_num == page_num ){
			if( st_pages_cb[i].forward_cb ){
				st_pages_cb[i].forward_cb( self, page );
				break;
			}
		}
	}
}

static void
on_prepare( GtkAssistant *assistant, GtkWidget *page, ofaExportAssistant *self )
{
	static const gchar *thisfn = "ofa_export_assistant_on_prepare";
	gint page_num;

	g_return_if_fail( assistant && GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( page && GTK_IS_WIDGET( page ));
	g_return_if_fail( self && OFA_IS_EXPORT_ASSISTANT( self ));

	page_num = gtk_assistant_get_current_page( assistant );

	g_debug( "%s: assistant=%p, page=%p, page_num=%d, self=%p",
			thisfn, ( void * ) assistant, ( void * ) page, page_num, ( void * ) self );

	switch( page_num ){
		/* 0 [Intro] Introduction */
		case ASSIST_PAGE_INTRO:
			break;

		/* 1 [Content] Select the data types to be exported */
		case ASSIST_PAGE_SELECT:
			if( !my_assistant_is_page_initialized( MY_ASSISTANT( self ), page )){
				p1_do_init( self, assistant, page );
				my_assistant_set_page_initialized( MY_ASSISTANT( self ), page, TRUE );
			}
			break;

		/* 2 [Content] Update the export settings */
		case ASSIST_PAGE_FORMAT:
			if( !my_assistant_is_page_initialized( MY_ASSISTANT( self ), page )){
				p2_do_init( self, assistant, page );
				my_assistant_set_page_initialized( MY_ASSISTANT( self ), page, TRUE );
			}
			break;

		/* 3 [Content] Select the output filename */
		case ASSIST_PAGE_OUTPUT:
			if( !my_assistant_is_page_initialized( MY_ASSISTANT( self ), page )){
				p3_do_init( self, assistant, page );
				my_assistant_set_page_initialized( MY_ASSISTANT( self ), page, TRUE );
			}
			p3_do_display( self, assistant, page );
			break;

		/* 4 [Confirm] Confirm the informations before exporting */
		case ASSIST_PAGE_CONFIRM:
			p4_do_display( self, assistant, page );
			break;

		/* 5 [Summary] Exports the data and print the result */
		case ASSIST_PAGE_DONE:
			p5_do_display( self, assistant, page );
			break;
	}
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
p1_do_init( ofaExportAssistant *self, GtkAssistant *assistant, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p1_do_init";
	ofaExportAssistantPrivate *priv;
	GtkWidget *btn;
	gint i, last;
	gboolean found;

	g_debug( "%s: self=%p, assistant=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) assistant,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;
	last = ofa_settings_get_int( st_pref_data );
	found = FALSE;

	for( i=0 ; st_types[i].code ; ++i ){

		btn = my_utils_container_get_child_by_name(
					GTK_CONTAINER( page ), st_types[i].widget_name );
		g_return_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ));

		g_object_set_data( G_OBJECT( btn ), DATA_TYPE_INDEX, GINT_TO_POINTER( i ));

		if( !priv->p1_group ){
			priv->p1_group = gtk_radio_button_get_group( GTK_RADIO_BUTTON( btn ));
		}

		if( st_types[i].code == last ){
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( btn ), TRUE );
			found = TRUE;
		}
	}

	if( !found ){
		g_return_if_fail( priv->p1_group && priv->p1_group->data );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->p1_group->data ), TRUE );
	}
}

static gboolean
p1_is_complete( ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	GSList *it;

	priv = self->priv;

	g_return_val_if_fail( priv->p1_group, FALSE );

	priv->p1_btn = NULL;
	priv->p1_idx = -1;

	/* which is the currently active button ? */
	for( it=priv->p1_group ; it ; it=it->next ){
		if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( it->data ))){
			priv->p1_idx = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( it->data ), DATA_TYPE_INDEX ));
			priv->p1_btn = GTK_WIDGET( it->data );
			break;;
		}
	}

	return( priv->p1_idx >= 0 && priv->p1_btn );
}

static void
p1_do_forward( ofaExportAssistant *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p1_do_forward";
	ofaExportAssistantPrivate *priv;

	g_debug( "%s: self=%p, page=%p", thisfn, ( void * ) self, ( void * ) page );

	if( p1_is_complete( self )){

		priv = self->priv;
		g_debug( "%s: idx=%d", thisfn, priv->p1_idx );
		ofa_settings_set_int( st_pref_data, st_types[priv->p1_idx].code );
	}
}

/*
 * p2: export format
 *
 * These are initialized with the export settings for this name, or
 * with default settings
 *
 * The page is always complete
 */
static void
p2_do_init( ofaExportAssistant *self, GtkAssistant *assistant, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p2_do_init";
	ofaExportAssistantPrivate *priv;
	GtkWidget *widget;

	g_debug( "%s: self=%p, assistant=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) assistant,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-alignment-parent" );
	g_return_if_fail( widget && GTK_IS_CONTAINER( widget ));

	priv->p2_settings_prefs = ofa_file_format_piece_new( SETTINGS_EXPORT_SETTINGS );
	ofa_file_format_piece_attach_to( priv->p2_settings_prefs, GTK_CONTAINER( widget ));
	ofa_file_format_piece_display( priv->p2_settings_prefs );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-new-btn" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( p2_on_new_profile_clicked ), self );
}

static void
p2_on_new_profile_clicked( GtkButton *button, ofaExportAssistant *self )
{
	g_warning( "ofa_export_assistant_p2_on_new_profile_clicked: TO BE MANAGED" );
}

static void
p2_do_forward( ofaExportAssistant *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p2_do_forward";
	ofaExportAssistantPrivate *priv;

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	ofa_file_format_piece_apply( priv->p2_settings_prefs );

	g_clear_object( &priv->p2_export_settings );
	priv->p2_export_settings =
			g_object_ref( ofa_file_format_piece_get_file_format( priv->p2_settings_prefs ));
}

/*
 * p3: choose output file
 */
static void
p3_do_init( ofaExportAssistant *self, GtkAssistant *assistant, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p3_do_init";
	ofaExportAssistantPrivate *priv;
	gchar *fname, *dirname;

	g_debug( "%s: self=%p, assistant=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) assistant,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	g_return_if_fail( priv->p1_idx >= 0 );
	g_return_if_fail( priv->p3_fname == NULL );

	priv->p3_chooser =
			( GtkFileChooser * ) my_utils_container_get_child_by_name(
					GTK_CONTAINER( page ), "p3-filechooser" );
	g_return_if_fail( priv->p3_chooser && GTK_IS_FILE_CHOOSER( priv->p3_chooser ));

	g_signal_connect( G_OBJECT( priv->p3_chooser ),
			"file-activated", G_CALLBACK( p3_on_file_activated ), self );

	/* build a default output filename from the last used folder
	 * plus the default basename for this data type class */
	fname = ofa_settings_get_string( st_pref_fname );
	if( fname && g_utf8_strlen( fname, -1 )){
		dirname = g_path_get_dirname( fname );
	} else {
		dirname = g_strdup( ofa_settings_get_string( SETTINGS_EXPORT_FOLDER ));
	}
	priv->p3_fname = g_build_filename( dirname, st_types[priv->p1_idx].base_name, NULL );
	g_free( dirname );
	g_free( fname );
}

static void
p3_do_display( ofaExportAssistant *self, GtkAssistant *assistant, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p3_do_display";
	ofaExportAssistantPrivate *priv;
	gchar *dirname, *basename;

	g_debug( "%s: self=%p, assistant=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) assistant,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	if( priv->p3_fname && g_utf8_strlen( priv->p3_fname, -1 )){
		dirname = g_path_get_dirname( priv->p3_fname );
		gtk_file_chooser_set_current_folder( priv->p3_chooser, dirname );
		g_free( dirname );
		basename = g_path_get_basename( priv->p3_fname );
		gtk_file_chooser_set_current_name( priv->p3_chooser, basename );
		g_free( basename );

	} else if( priv->p3_last_folder && g_utf8_strlen( priv->p3_last_folder, -1 )){
		gtk_file_chooser_set_current_folder( priv->p3_chooser, priv->p3_last_folder );
	}
}

static void
p3_on_file_activated( GtkFileChooser *chooser, ofaExportAssistant *self )
{
}

static void
p3_do_forward( ofaExportAssistant *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p3_do_forward";
	ofaExportAssistantPrivate *priv;
	gboolean complete;
	gchar *name, *folder, *final;

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	name = gtk_file_chooser_get_current_name( priv->p3_chooser );
	if( name && g_utf8_strlen( name, -1 )){
		if( g_path_is_absolute( name )){
			final = g_strdup( name );
		} else {
			folder = gtk_file_chooser_get_current_folder( priv->p3_chooser );
			final = g_build_filename( folder, name, NULL );
			g_free( folder );
		}
	} else {
		final = gtk_file_chooser_get_filename( priv->p3_chooser );
	}
	g_free( name );

	g_free( priv->p3_fname );
	priv->p3_fname = final;

	/* keep the last used folder in case we go back to this page
	 * we choose to keep the same folder, letting the user choose
	 * another basename */
	g_free( priv->p3_last_folder );
	priv->p3_last_folder = g_path_get_dirname( priv->p3_fname );

	complete = ( priv->p3_fname && g_utf8_strlen( priv->p3_fname, -1 ) > 0 );

	if( complete ){
		if( my_utils_file_exists( priv->p3_fname )){
			complete &= confirm_overwrite( self, priv->p3_fname );
			if( !complete ){
				g_free( priv->p3_fname );
				priv->p3_fname = NULL;
			}
		}
	}

	if( complete ){
		ofa_settings_set_string( st_pref_fname, priv->p3_fname );
	}
}

/*
 * ask the user to confirm the operation
 */
static void
p4_do_display( ofaExportAssistant *self, GtkAssistant *assistant, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p4_do_prepare_confirm";
	ofaExportAssistantPrivate *priv;
	GtkWidget *label;
	gchar *str;
	gint format;
	GdkRGBA color;
	gboolean complete;

	g_debug( "%s: self=%p, assistant=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) assistant,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	gdk_rgba_parse( &color, "#0000ff" );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-data" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
	str = my_utils_str_remove_underlines( gtk_button_get_label( GTK_BUTTON( priv->p1_btn )));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-charmap" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
	gtk_label_set_text( GTK_LABEL( label ), ofa_file_format_get_charmap( priv->p2_export_settings ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-date" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
	format = ofa_file_format_get_date_format( priv->p2_export_settings );
	gtk_label_set_text( GTK_LABEL( label ), my_date_get_format_str( format ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-decimal" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
	str = g_strdup_printf( "%c", ofa_file_format_get_decimal_sep( priv->p2_export_settings ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-field" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
	str = g_strdup_printf( "%c", ofa_file_format_get_field_sep( priv->p2_export_settings ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-output" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
	gtk_label_set_text( GTK_LABEL( label ), priv->p3_fname );

	complete = ( priv->p3_fname && g_utf8_strlen( priv->p3_fname, -1 ) > 0 );
	gtk_assistant_set_page_complete( assistant, page, complete );
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
	GtkWidget *dialog;
	gchar *str;
	gint response;

	str = g_strdup_printf(
				_( "The file '%s' already exists.\n"
					"Are you sure you want to overwrite it ?" ), fname );

	dialog = gtk_message_dialog_new(
			my_window_get_toplevel( MY_WINDOW( self )),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_NONE,
			"%s", str );

	gtk_dialog_add_buttons( GTK_DIALOG( dialog ),
			_( "Cancel" ), GTK_RESPONSE_CANCEL,
			_( "Overwrite" ), GTK_RESPONSE_OK,
			NULL );

	response = gtk_dialog_run( GTK_DIALOG( dialog ));
	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
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
on_apply( GtkAssistant *assistant, ofaExportAssistant *self )
{
	static const gchar *thisfn = "ofa_export_assistant_on_apply";

	g_debug( "%s: assistant=%p, self=%p", thisfn, ( void * ) assistant, ( void * ) self );
}

static void
p5_do_display( ofaExportAssistant *self, GtkAssistant *assistant, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p5_do_display";
	ofaExportAssistantPrivate *priv;

	g_return_if_fail( OFA_IS_EXPORT_ASSISTANT( self ));

	g_debug( "%s: self=%p, assistant=%p, page=%p",
			thisfn, ( void * ) self, ( void * ) assistant, ( void * ) page );

	gtk_assistant_set_page_complete( assistant, page, FALSE );

	priv = self->priv;

	priv->progress_bar =
			my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-bar" );
	priv->p5_page = page;

	priv->base = ( ofaIExportable * ) g_object_new( st_types[priv->p1_idx].get_type(), NULL );
	if( !OFA_IS_IEXPORTABLE( priv->base )){
		error_no_interface( self );
		return;
	}

	g_idle_add(( GSourceFunc ) export_data, self );
}

static gboolean
export_data( ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	GtkWidget *label;
	gchar *str, *text;
	gboolean ok;

	priv = self->priv;

	/* first, export */
	ok = ofa_iexportable_export_to_path(
			priv->base, priv->p3_fname, priv->p2_export_settings,
			MY_WINDOW( self )->prot->dossier,
			( ofaIExportableFnDouble ) set_progress_double,
			( ofaIExportableFnText ) set_progress_text, self );

	/* then display the result */
	label = my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )) ), "p5-label" );

	str = my_utils_str_remove_underlines( gtk_button_get_label( GTK_BUTTON( priv->p1_btn )));
	if( ok ){
		text = g_strdup_printf( _( "OK: « %s » has been successfully exported.\n\n"
				"%ld lines have been written in '%s' output file." ),
				str, ofa_iexportable_get_count( priv->base ), priv->p3_fname );
	} else {
		text = g_strdup_printf( _( "Unfortunately, « %s » export has encountered errors.\n\n"
				"The '%s' file may be incomplete or inaccurate.\n\n"
				"Please fix these errors, and retry then." ), str, priv->p3_fname );
	}
	g_free( str );

	str = g_strdup_printf( "<b>%s</b>", text );
	g_free( text );

	gtk_label_set_markup( GTK_LABEL( label ), str );
	g_free( str );

	/* unref the reference object _after_ having built the label so
	 * we always have access to its internal datas */
	g_object_unref( priv->base );

	gtk_assistant_set_page_complete(
			GTK_ASSISTANT( my_window_get_toplevel( MY_WINDOW( self ))), priv->p5_page, TRUE );

	/* do not continue and remove from idle callbacks list */
	return( FALSE );
}

static void
error_no_interface( const ofaExportAssistant *self )
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(
			my_window_get_toplevel( MY_WINDOW( self )),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_CLOSE,
			"%s", _( "The requested type does not implement the IExportable interface" ));

	gtk_dialog_run( GTK_DIALOG( dialog ));
	gtk_widget_destroy( dialog );
}

static void
set_progress_double( const ofaExportAssistant *self, gdouble progress )
{
	ofaExportAssistantPrivate *priv;

	priv = self->priv;

	gtk_progress_bar_set_fraction( GTK_PROGRESS_BAR( priv->progress_bar ), progress );
}

static void
set_progress_text( const ofaExportAssistant *self, const gchar *text )
{
	ofaExportAssistantPrivate *priv;

	priv = self->priv;

	gtk_progress_bar_set_show_text( GTK_PROGRESS_BAR( priv->progress_bar ), TRUE );
	gtk_progress_bar_set_text( GTK_PROGRESS_BAR( priv->progress_bar ), text );
}
