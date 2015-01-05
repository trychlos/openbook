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

#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <stdlib.h>

#include "api/my-utils.h"
#include "api/ofa-file-format.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-iimporter.h"
#include "api/ofa-settings.h"
#include "api/ofo-bat.h"
#include "api/ofo-account.h"
#include "api/ofo-class.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

#include "core/my-window-prot.h"
#include "core/ofa-plugin.h"

#include "ui/my-progress-bar.h"
#include "ui/ofa-file-format-piece.h"
#include "ui/ofa-import-assistant.h"
#include "ui/ofa-importer.h"
#include "ui/ofa-main-window.h"

/* Export Assistant
 *
 * pos.  type     enum     title
 * ---   -------  -------  --------------------------------------------
 *   0   Intro    INTRO    Introduction
 *   1   Content  SELECT   Select a file
 *   2   Content  TYPE     Select a type of import
 *   3   Content  SETTINGS Set the locale settings
 *   4   Confirm  CONFIRM  Summary of the operations to be done
 *   5   Summary  DONE     After import
 */

enum {
	ASSIST_PAGE_INTRO = 0,
	ASSIST_PAGE_SELECT,
	ASSIST_PAGE_TYPE,
	ASSIST_PAGE_SETTINGS,
	ASSIST_PAGE_CONFIRM,
	ASSIST_PAGE_DONE
};

/* private instance data
 */
struct _ofaImportAssistantPrivate {

	/* p1: select file to be imported
	 */
	GtkFileChooser     *p1_chooser;
	gchar              *p1_folder;
	gchar              *p1_fname;		/* the utf-8 imported filename */

	/* p2: select a type of data to be imported
	 */
	GSList             *p2_group;
	gint                p2_type;
	gint                p2_idx;
	GtkButton          *p2_type_btn;

	/* p3: locale settings
	 */
	ofaFileFormat      *p3_import_settings;
	ofaFileFormatPiece *p3_settings_prefs;

	/* p5: import the file, display the result
	 */
	myProgressBar      *p5_import;
	myProgressBar      *p5_insert;
	GtkWidget          *p5_page;
	GtkWidget          *p5_text;
	ofaIImportable     *p5_object;
	ofaIImportable     *p5_plugin;
};

/* management of the radio buttons group
 * types are defined in ofa-iimporter.h
 */
typedef GType ( *fn_type )( void );
typedef struct {
	guint        type_id;
	const gchar *w_name;				/* the name of the radio button widget */
	fn_type      get_type;
	gint         headers;				/* count of header lines in CSV formats */
}
	sRadios;

enum {
	IMPORT_BAT = 1,
	IMPORT_CLASS,
	IMPORT_ACCOUNT,
	IMPORT_CURRENCY,
	IMPORT_LEDGER,
	IMPORT_MODEL,
	IMPORT_RATE,
	IMPORT_ENTRY,
};

static const sRadios st_radios[] = {
		{ IMPORT_BAT,      "p2-releve",   NULL,                      0 },
		{ IMPORT_CLASS,    "p2-class",    ofo_class_get_type,        1 },
		{ IMPORT_ACCOUNT,  "p2-account",  ofo_account_get_type,      1 },
		{ IMPORT_CURRENCY, "p2-currency", ofo_currency_get_type,     1 },
		{ IMPORT_LEDGER,   "p2-journals", ofo_ledger_get_type,       1 },
		{ IMPORT_MODEL,    "p2-model",    ofo_ope_template_get_type, 2 },
		{ IMPORT_RATE,     "p2-rate",     ofo_rate_get_type,         2 },
		{ IMPORT_ENTRY,    "p2-entries",  ofo_entry_get_type,        1 },
		{ 0 }
};

/* data set against each of above radio buttons */
#define DATA_BUTTON_INDEX               "ofa-data-button-idx"

/* the user preferences stored as a string list
 * folder
 */
static const gchar *st_prefs_import     = "ImportAssistant";

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-import-assistant.ui";
static const gchar *st_ui_id            = "ImportAssistant";

static void            iimporter_iface_init( ofaIImporterInterface *iface );
static guint           iimporter_get_interface_version( const ofaIImporter *instance );
static void            on_page_forward( ofaImportAssistant *self, GtkWidget *page, gint page_num, void *empty );
static void            on_prepare( GtkAssistant *assistant, GtkWidget *page, ofaImportAssistant *self );
static void            p1_do_init( ofaImportAssistant *self, GtkAssistant *assistant, GtkWidget *page );
static void            p1_display( ofaImportAssistant *self, GtkAssistant *assistant, GtkWidget *page );
static void            p1_on_selection_changed( GtkFileChooser *chooser, ofaImportAssistant *self );
static void            p1_on_file_activated( GtkFileChooser *chooser, ofaImportAssistant *self );
static void            p1_check_for_complete( ofaImportAssistant *self );
static void            p1_do_forward( ofaImportAssistant *self, GtkWidget *page );
static void            p2_do_init( ofaImportAssistant *self, GtkAssistant *assistant, GtkWidget *page );
static void            p2_on_type_toggled( GtkToggleButton *button, ofaImportAssistant *self );
static void            p2_check_for_complete( ofaImportAssistant *self );
static void            p2_do_forward( ofaImportAssistant *self, GtkWidget *page );
static void            p3_do_init( ofaImportAssistant *self, GtkAssistant *assistant, GtkWidget *page );
static void            p3_display( ofaImportAssistant *self, GtkAssistant *assistant, GtkWidget *page );
static void            p3_on_settings_changed( ofaFileFormatPiece *piece, ofaImportAssistant *self );
static void            p3_check_for_complete( ofaImportAssistant *self );
static void            p3_do_forward( ofaImportAssistant *self, GtkWidget *page );
static void            p4_do_display( ofaImportAssistant *self, GtkAssistant *assistant, GtkWidget *page );
static void            on_apply( GtkAssistant *assistant, ofaImportAssistant *self );
static void            p5_do_display( ofaImportAssistant *self, GtkAssistant *assistant, GtkWidget *page );
static ofaIImportable *p5_search_for_plugin( ofaImportAssistant *self );
static void            p5_error_no_interface( const ofaImportAssistant *self );
static gboolean        p5_do_import( ofaImportAssistant *self );
static guint           p5_do_import_csv( ofaImportAssistant *self, guint *errors );
static guint           p5_do_import_other( ofaImportAssistant *self, guint *errors );
static void            p5_on_progress( ofaIImporter *importer, ofeImportablePhase phase, gdouble progress, const gchar *text, ofaImportAssistant *self );
static void            p5_on_message( ofaIImporter *importer, guint line_number, ofeImportableMsg status, const gchar *msg, ofaImportAssistant *self );
static GSList         *get_lines_from_csv( ofaImportAssistant *self );
static void            error_load_contents( ofaImportAssistant *self, const gchar *fname, GError *error );
static void            error_convert( ofaImportAssistant *self, GError *error );
static void            free_fields( GSList *fields );
static void            free_lines( GSList *lines );
static void            get_settings( ofaImportAssistant *self );
static void            update_settings( ofaImportAssistant *self );

G_DEFINE_TYPE_EXTENDED( ofaImportAssistant, ofa_import_assistant, MY_TYPE_ASSISTANT, 0, \
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTER, iimporter_iface_init ));

static void
import_assistant_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_import_assistant_finalize";
	ofaImportAssistantPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_IMPORT_ASSISTANT( instance ));

	/* free data members here */
	priv = OFA_IMPORT_ASSISTANT( instance )->priv;

	g_free( priv->p1_folder );
	g_free( priv->p1_fname );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_import_assistant_parent_class )->finalize( instance );
}

static void
import_assistant_dispose( GObject *instance )
{
	ofaImportAssistantPrivate *priv;

	g_return_if_fail( instance && OFA_IS_IMPORT_ASSISTANT( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		priv = OFA_IMPORT_ASSISTANT( instance )->priv;

		/* unref object members here */
		g_clear_object( &priv->p3_import_settings );
		g_clear_object( &priv->p5_object );
		g_clear_object( &priv->p5_plugin );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_import_assistant_parent_class )->dispose( instance );
}

static void
ofa_import_assistant_init( ofaImportAssistant *self )
{
	static const gchar *thisfn = "ofa_import_assistant_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_IMPORT_ASSISTANT( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_IMPORT_ASSISTANT, ofaImportAssistantPrivate );

	self->priv->p2_type = -1;
}

static void
ofa_import_assistant_class_init( ofaImportAssistantClass *klass )
{
	static const gchar *thisfn = "ofa_import_assistant_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = import_assistant_dispose;
	G_OBJECT_CLASS( klass )->finalize = import_assistant_finalize;

	g_type_class_add_private( klass, sizeof( ofaImportAssistantPrivate ));
}

/*
 * ofaIImporter interface management
 */
static void
iimporter_iface_init( ofaIImporterInterface *iface )
{
	static const gchar *thisfn = "ofo_account_iexportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iimporter_get_interface_version;
}

static guint
iimporter_get_interface_version( const ofaIImporter *instance )
{
	return( 1 );
}

/**
 * Run the assistant.
 *
 * @main: the main window of the application.
 */
void
ofa_import_assistant_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_import_assistant_run";
	ofaImportAssistant *self;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, main_window );

	self = g_object_new( OFA_TYPE_IMPORT_ASSISTANT,
							MY_PROP_MAIN_WINDOW, main_window,
							MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
							MY_PROP_WINDOW_XML,  st_ui_xml,
							MY_PROP_WINDOW_NAME, st_ui_id,
							NULL );

	get_settings( self );

	/* messages provided by the ofaIImporter interface */
	g_signal_connect(
			G_OBJECT( self ), "progress", G_CALLBACK( p5_on_progress ), self );
	g_signal_connect(
			G_OBJECT( self ), "message", G_CALLBACK( p5_on_message ), self );

	/* message provided by the myAssistant class */
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
on_page_forward( ofaImportAssistant *self, GtkWidget *page, gint page_num, void *empty )
{
	static const gchar *thisfn = "ofa_import_assistant_on_page_forward";

	g_return_if_fail( self && OFA_IS_IMPORT_ASSISTANT( self ));
	g_return_if_fail( page && GTK_IS_WIDGET( page ));

	g_debug( "%s: self=%p, page=%p, page_num=%d, empty=%p",
			thisfn, ( void * ) self, ( void * ) page, page_num, ( void * ) empty );

	switch( page_num ){
		/* 1 [Content] Select the filename to be imported */
		case ASSIST_PAGE_SELECT:
			p1_do_forward( self, page );
			break;

		/* 2 [Content] Select the data type to be imported */
		case ASSIST_PAGE_TYPE:
			p2_do_forward( self, page );
			break;

		/* 3 [Content] file format settings */
		case ASSIST_PAGE_SETTINGS:
			p3_do_forward( self, page );
			break;
	}
}

static void
on_prepare( GtkAssistant *assistant, GtkWidget *page, ofaImportAssistant *self )
{
	static const gchar *thisfn = "ofa_import_assistant_on_prepare";
	gint page_num;

	g_return_if_fail( assistant && GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( page && GTK_IS_WIDGET( page ));
	g_return_if_fail( self && OFA_IS_IMPORT_ASSISTANT( self ));

	page_num = gtk_assistant_get_current_page( assistant );

	g_debug( "%s: assistant=%p, page=%p, page_num=%d, self=%p",
			thisfn, ( void * ) assistant, ( void * ) page, page_num, ( void * ) self );

	switch( page_num ){
		/* 1 [Content] Select the filename to be imported */
		case ASSIST_PAGE_SELECT:
			if( !my_assistant_is_page_initialized( MY_ASSISTANT( self ), page )){
				p1_do_init( self, assistant, page );
				my_assistant_set_page_initialized( MY_ASSISTANT( self ), page, TRUE );
			}
			p1_display( self, assistant, page );
			break;

		/* 2 [Content] Select the data type to be imported */
		case ASSIST_PAGE_TYPE:
			if( !my_assistant_is_page_initialized( MY_ASSISTANT( self ), page )){
				p2_do_init( self, assistant, page );
				my_assistant_set_page_initialized( MY_ASSISTANT( self ), page, TRUE );
			}
			break;

		/* 3 [Content] Select the file format settings */
		case ASSIST_PAGE_SETTINGS:
			if( !my_assistant_is_page_initialized( MY_ASSISTANT( self ), page )){
				p3_do_init( self, assistant, page );
				my_assistant_set_page_initialized( MY_ASSISTANT( self ), page, TRUE );
			}
			p3_display( self, assistant, page );
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
 * initialize the GtkFileChooser widget with the last used folder
 * we allow only a single selection and no folder creation
 */
static void
p1_do_init( ofaImportAssistant *self, GtkAssistant *assistant, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p1_do_init";
	ofaImportAssistantPrivate *priv;
	GtkWidget *widget;

	g_debug( "%s: self=%p, assistant=%p, page=%p (%s)",
			thisfn,
			( void * ) self, ( void * ) assistant,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p1-filechooser" );
	g_return_if_fail( widget && GTK_IS_FILE_CHOOSER_WIDGET( widget ));
	priv->p1_chooser = GTK_FILE_CHOOSER( widget );

	g_signal_connect(
			G_OBJECT( widget ), "selection-changed", G_CALLBACK( p1_on_selection_changed ), self );
	g_signal_connect(
			G_OBJECT( widget ), "file-activated", G_CALLBACK( p1_on_file_activated ), self );
}

static void
p1_display( ofaImportAssistant *self, GtkAssistant *assistant, GtkWidget *page )
{
	ofaImportAssistantPrivate *priv;

	priv = self->priv;

	if( priv->p1_folder ){
		gtk_file_chooser_set_current_folder( priv->p1_chooser, priv->p1_folder );
	}
}

static void
p1_on_selection_changed( GtkFileChooser *chooser, ofaImportAssistant *self )
{
	p1_check_for_complete( self );
}

static void
p1_on_file_activated( GtkFileChooser *chooser, ofaImportAssistant *self )
{
	p1_check_for_complete( self );
}

static void
p1_check_for_complete( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gboolean ok;

	priv = self->priv;

	g_free( priv->p1_fname );
	priv->p1_fname = gtk_file_chooser_get_filename( priv->p1_chooser );
	g_debug( "p1_check_for_complete: fname=%s", priv->p1_fname );

	ok = priv->p1_fname &&
			g_utf8_strlen( priv->p1_fname, -1 ) > 0 &&
			my_utils_file_is_readable_file( priv->p1_fname );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), ASSIST_PAGE_SELECT, ok );
}

static void
p1_do_forward( ofaImportAssistant *self, GtkWidget *page )
{
	ofaImportAssistantPrivate *priv;

	priv = self->priv;

	g_free( priv->p1_folder );
	priv->p1_folder = gtk_file_chooser_get_current_folder( priv->p1_chooser );

	update_settings( self );
}

/*
 * p2: nature of the data to import
 */
static void
p2_do_init( ofaImportAssistant *self, GtkAssistant *assistant, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p2_do_init";
	ofaImportAssistantPrivate *priv;
	gint i;
	GtkWidget *button;

	g_debug( "%s: self=%p, assistant=%p, page=%p (%s)",
			thisfn,
			( void * ) self, ( void * ) assistant,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	for( i=0 ; st_radios[i].type_id ; ++i ){
		button = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), st_radios[i].w_name );
		g_object_set_data( G_OBJECT( button ), DATA_BUTTON_INDEX, GINT_TO_POINTER( i ));
		g_signal_connect( G_OBJECT( button ), "toggled", G_CALLBACK( p2_on_type_toggled ), self );
		if( st_radios[i].type_id == priv->p2_type ){
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), TRUE );
			p2_on_type_toggled( GTK_TOGGLE_BUTTON( button ), self );
		}
		if( !priv->p2_group ){
			priv->p2_group = gtk_radio_button_get_group( GTK_RADIO_BUTTON( button ));
		}
	}

	p2_check_for_complete( self );
}

static void
p2_on_type_toggled( GtkToggleButton *button, ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;

	priv = self->priv;

	if( gtk_toggle_button_get_active( button )){
		priv->p2_idx = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_BUTTON_INDEX ));
		priv->p2_type = st_radios[priv->p2_idx].type_id;
		priv->p2_type_btn = GTK_BUTTON( button );
	} else {
		priv->p2_idx = -1;
		priv->p2_type = 0;
		priv->p2_type_btn = NULL;
	}

	p2_check_for_complete( self );
}

static void
p2_check_for_complete( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;

	priv = self->priv;

	my_assistant_set_page_complete( MY_ASSISTANT( self ), ASSIST_PAGE_TYPE, priv->p2_type > 0 );
}

static void
p2_do_forward( ofaImportAssistant *self, GtkWidget *page )
{
	update_settings( self );
}

/*
 * p3: import format
 *
 * These are initialized with the import settings for this name, or
 * with default settings
 *
 * The page is always complete
 */
static void
p3_do_init( ofaImportAssistant *self, GtkAssistant *assistant, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p3_do_init";
	ofaImportAssistantPrivate *priv;
	GtkWidget *widget;

	g_debug( "%s: self=%p, assistant=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) assistant,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-settings-parent" );
	g_return_if_fail( widget && GTK_IS_CONTAINER( widget ));

	priv->p3_import_settings = ofa_file_format_new( SETTINGS_IMPORT_SETTINGS );
	priv->p3_settings_prefs = ofa_file_format_piece_new( priv->p3_import_settings );
	ofa_file_format_piece_attach_to( priv->p3_settings_prefs, GTK_CONTAINER( widget ));

	g_signal_connect(
			G_OBJECT( priv->p3_settings_prefs ), "changed", G_CALLBACK( p3_on_settings_changed ), self );
}

static void
p3_display( ofaImportAssistant *self, GtkAssistant *assistant, GtkWidget *page )
{
	p3_check_for_complete( self );
}

static void
p3_on_settings_changed( ofaFileFormatPiece *piece, ofaImportAssistant *self )
{
	p3_check_for_complete( self );
}

static void
p3_check_for_complete( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gboolean ok;

	priv = self->priv;
	ok = ofa_file_format_piece_is_validable( priv->p3_settings_prefs );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), ASSIST_PAGE_SETTINGS, ok );
}

static void
p3_do_forward( ofaImportAssistant *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p3_do_forward";
	ofaImportAssistantPrivate *priv;

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	ofa_file_format_piece_apply( priv->p3_settings_prefs );
}

/*
 * ask the user to confirm the operation
 */
static void
p4_do_display( ofaImportAssistant *self, GtkAssistant *assistant, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p4_do_display";
	ofaImportAssistantPrivate *priv;
	GdkRGBA color;
	GtkWidget *label;
	gchar *str;
	gboolean complete;
	myDateFormat format;
	ofaFFmt ffmt;

	g_debug( "%s: self=%p, assistant=%p, page=%p (%s)",
			thisfn,
			( void * ) self, ( void * ) assistant,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	gdk_rgba_parse( &color, "#0000ff" );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-fname" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
	gtk_label_set_text( GTK_LABEL( label ), priv->p1_fname );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-type" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
	str = my_utils_str_remove_underlines( gtk_button_get_label( GTK_BUTTON( priv->p2_type_btn )));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	ffmt = ofa_file_format_get_ffmt( priv->p3_import_settings );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-ffmt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
	gtk_label_set_text( GTK_LABEL( label ), ofa_file_format_get_ffmt_str( ffmt ));

	if( ffmt != OFA_FFMT_OTHER ){

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-charmap" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
		gtk_label_set_text( GTK_LABEL( label ), ofa_file_format_get_charmap( priv->p3_import_settings ));

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-date" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
		format = ofa_file_format_get_date_format( priv->p3_import_settings );
		gtk_label_set_text( GTK_LABEL( label ), my_date_get_format_str( format ));

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-decimal" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
		str = g_strdup_printf( "%c", ofa_file_format_get_decimal_sep( priv->p3_import_settings ));
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-field" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
		str = g_strdup_printf( "%c", ofa_file_format_get_field_sep( priv->p3_import_settings ));
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );
	}

	gtk_widget_show_all( page );

	complete = ( priv->p1_fname && g_utf8_strlen( priv->p1_fname, -1 ) > 0 );
	gtk_assistant_set_page_complete( assistant, page, complete );
}

static void
on_apply( GtkAssistant *assistant, ofaImportAssistant *self )
{
	static const gchar *thisfn = "ofa_import_assistant_on_apply";

	g_return_if_fail( GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( OFA_IS_IMPORT_ASSISTANT( self ));

	g_debug( "%s: assistant=%p, self=%p", thisfn, ( void * ) assistant, ( void * ) self );
}

static void
p5_do_display( ofaImportAssistant *self, GtkAssistant *assistant, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p5_do_display";
	ofaImportAssistantPrivate *priv;
	GtkWidget *parent;

	g_return_if_fail( OFA_IS_IMPORT_ASSISTANT( self ));

	g_debug( "%s: self=%p, assistant=%p, page=%p",
			thisfn, ( void * ) self, ( void * ) assistant, ( void * ) page );

	gtk_assistant_set_page_complete( assistant, page, FALSE );

	priv = self->priv;

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-bar-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p5_import = my_progress_bar_new();
	my_progress_bar_attach_to( priv->p5_import, GTK_CONTAINER( parent ));

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-insert-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p5_insert = my_progress_bar_new();
	my_progress_bar_attach_to( priv->p5_insert, GTK_CONTAINER( parent ));

	priv->p5_text = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-text-view" );
	g_return_if_fail( priv->p5_text && GTK_IS_TEXT_VIEW( priv->p5_text ));
	gtk_widget_set_can_focus( priv->p5_text, FALSE );

	priv->p5_page = page;

	/* search from something which would be able to import the data */
	if( st_radios[priv->p2_idx].get_type ){
		priv->p5_object = ( ofaIImportable * ) g_object_new( st_radios[priv->p2_idx].get_type(), NULL );
	} else {
		priv->p5_plugin = p5_search_for_plugin( self );
	}
	if( !OFA_IS_IIMPORTABLE( priv->p5_object ) && !priv->p5_plugin ){
		p5_error_no_interface( self );
		return;
	}

	g_idle_add(( GSourceFunc ) p5_do_import, self );
}

/*
 * search for a plugin willing to import an 'other' format
 */
static ofaIImportable *
p5_search_for_plugin( ofaImportAssistant *self )
{
	static const gchar *thisfn = "ofa_import_assistant_p5_search_for_plugin";
	ofaImportAssistantPrivate *priv;
	GList *modules, *it;
	ofaIImportable *found;

	priv = self->priv;
	found = NULL;
	modules = ofa_plugin_get_extensions_for_type( OFA_TYPE_IIMPORTABLE );
	g_debug( "%s: modules=%p, count=%d", thisfn, ( void * ) modules, g_list_length( modules ));

	for( it=modules ; it ; it=it->next ){
		if( ofa_iimportable_is_willing_to(
				OFA_IIMPORTABLE( it->data ), priv->p1_fname, priv->p3_import_settings )){
			found = g_object_ref( OFA_IIMPORTABLE( it->data ));
			break;
		}
	}

	ofa_plugin_free_extensions( modules );

	return( found );
}

static void
p5_error_no_interface( const ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	GtkWidget *dialog;
	gchar *str;

	priv = self->priv;

	if( priv->p5_object ){
		str = g_strdup_printf(
					_( "The requested type (%s) does not implement the IImportable interface" ),
					G_OBJECT_TYPE_NAME( priv->p5_object ));
	} else {
		str = g_strdup_printf( _( "Unable to find a plugin to import the specified data" ));
	}

	dialog = gtk_message_dialog_new(
			my_window_get_toplevel( MY_WINDOW( self )),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_CLOSE,
			"%s", str );

	g_free( str );
	gtk_dialog_run( GTK_DIALOG( dialog ));
	gtk_widget_destroy( dialog );
}

static gboolean
p5_do_import( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	GtkWidget *label;
	gchar *str, *text;
	gint ffmt;
	guint count, errors;
	gboolean has_worked;

	priv = self->priv;
	count = 0;
	errors = 0;
	has_worked = TRUE;

	/* first, import */
	ffmt = ofa_file_format_get_ffmt( priv->p3_import_settings );
	switch( ffmt ){
		case OFA_FFMT_CSV:
			count = p5_do_import_csv( self, &errors );
			break;
		case OFA_FFMT_OTHER:
			g_return_val_if_fail( priv->p5_plugin && OFA_IS_IIMPORTABLE( priv->p5_plugin ), FALSE );
			count = p5_do_import_other( self, &errors );
			break;
		default:
			has_worked = FALSE;
			break;
	}

	/* then display the result */
	label = my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )) ), "p5-label" );

	str = my_utils_str_remove_underlines( gtk_button_get_label( GTK_BUTTON( priv->p2_type_btn )));

	if( has_worked ){
		if( !errors ){
			text = g_strdup_printf( _( "OK: %u lines from '%s' have been successfully "
					"imported into « %s »." ),
					count, priv->p1_fname, str );
		} else {
			text = g_strdup_printf( _( "Unfortunately, '%s' import has encountered errors.\n"
					"The « %s » recordset has been left unchanged.\n"
					"Please fix these errors, and retry then." ), priv->p1_fname, str );
		}
	} else {
		text = g_strdup_printf( _( "Unfortunately, the required file format is not "
				"managed at this time.\n"
				"Please fix this, and retry, then." ));
	}

	g_free( str );

	gtk_label_set_text( GTK_LABEL( label ), text );
	g_free( text );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), ASSIST_PAGE_DONE, TRUE );

	/* do not continue and remove from idle callbacks list */
	return( FALSE );
}

static guint
p5_do_import_csv( ofaImportAssistant *self, guint *errors )
{
	ofaImportAssistantPrivate *priv;
	guint count;
	GSList *lines, *content;

	priv = self->priv;
	lines = get_lines_from_csv( self );
	content = lines;

	if( ofa_file_format_has_headers( priv->p3_import_settings )){
		content = g_slist_nth( lines, st_radios[priv->p2_idx].headers );
	}

	count = g_slist_length( content );
	*errors = ofa_iimportable_import( priv->p5_object,
			content, priv->p3_import_settings, MY_WINDOW( self )->prot->dossier, self );

	free_lines( lines );

	return( count );
}

static guint
p5_do_import_other( ofaImportAssistant *self, guint *errors )
{
	ofaImportAssistantPrivate *priv;
	guint count;

	priv = self->priv;

	*errors = ofa_iimportable_import_fname( priv->p5_plugin, MY_WINDOW( self )->prot->dossier, self );
	count = ofa_iimportable_get_count( priv->p5_plugin );

	return( count );
}

static void
p5_on_progress( ofaIImporter *importer, ofeImportablePhase phase, gdouble progress, const gchar *text, ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;

	priv = self->priv;

	if( phase == IMPORTABLE_PHASE_IMPORT ){
		g_signal_emit_by_name( priv->p5_import, "double", progress );
		g_signal_emit_by_name( priv->p5_import, "text", text );

	} else {
		g_return_if_fail( phase == IMPORTABLE_PHASE_INSERT );
		g_signal_emit_by_name( priv->p5_insert, "double", progress );
		g_signal_emit_by_name( priv->p5_insert, "text", text );
	}
}

static void
p5_on_message( ofaIImporter *importer, guint line_number, ofeImportableMsg status, const gchar *msg, ofaImportAssistant *self )
{
	static const gchar *thisfn = "ofa_import_assistant_p5_on_error";
	ofaImportAssistantPrivate *priv;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	gchar *str;

	g_debug( "%s: importer=%p, line_number=%u, msg=%s, self=%p",
			thisfn, ( void * ) importer, line_number, msg, ( void * ) self );

	priv = self->priv;

	buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( priv->p5_text ));
	gtk_text_buffer_get_end_iter( buffer, &iter );
	str = g_strdup_printf( "[%u] %s\n", line_number, msg );
	gtk_text_buffer_insert( buffer, &iter, str, -1 );
	g_free( str );

	/* let Gtk update the display */
	while( gtk_events_pending()){
		gtk_main_iteration();
	}
}

/*
 * Returns a GSList of lines, where each lines->data is a GSList of
 * fields
 */
static GSList *
get_lines_from_csv( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	GFile *gfile;
	gchar *sysfname, *contents, *str;
	GError *error;
	gchar **lines, **iter_line;
	gchar **fields, **iter_field;
	GSList *s_fields, *s_lines;
	gchar *field;

	priv = self->priv;

	sysfname = my_utils_filename_from_utf8( priv->p1_fname );
	if( !sysfname ){
		return( NULL );
	}
	gfile = g_file_new_for_path( sysfname );
	g_free( sysfname );

	error = NULL;
	if( !g_file_load_contents( gfile, NULL, &contents, NULL, NULL, &error )){
		error_load_contents( self, priv->p1_fname, error );
		g_error_free( error );
		g_free( contents );
		g_object_unref( gfile );
		return( NULL );
	}

	lines = g_strsplit( contents, "\n", -1 );

	g_free( contents );
	g_object_unref( gfile );

	s_lines = NULL;
	iter_line = lines;

	while( *iter_line ){
		error = NULL;
		str = g_convert( *iter_line, -1,
								ofa_file_format_get_charmap( priv->p3_import_settings ),
								"UTF-8", NULL, NULL, &error );
		if( !str ){
			error_convert( self, error );
			g_strfreev( lines );
			return( NULL );
		}
		if( g_utf8_strlen( *iter_line, -1 )){
			fields = g_strsplit(( const gchar * ) *iter_line, ";", -1 );
			s_fields = NULL;
			iter_field = fields;

			while( *iter_field ){
				field = g_strstrip( g_strdup( *iter_field ));
				s_fields = g_slist_prepend( s_fields, field );
				iter_field++;
			}

			g_strfreev( fields );
			s_lines = g_slist_prepend( s_lines, g_slist_reverse( s_fields ));
		}
		g_free( str );
		iter_line++;
	}

	g_strfreev( lines );
	return( g_slist_reverse( s_lines ));
}

static void
error_load_contents( ofaImportAssistant *self, const gchar *fname, GError *error )
{
	GtkWidget *dialog;
	gchar *str;

	str = g_strdup_printf( _( "Unable to load content from '%s' file: %s" ),
				fname, error->message );

	dialog = gtk_message_dialog_new(
			NULL,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_CLOSE,
			"%s", str );

	g_free( str );
	gtk_dialog_run( GTK_DIALOG( dialog ));
	gtk_widget_destroy( dialog );
}

static void
error_convert( ofaImportAssistant *self, GError *error )
{
	GtkWidget *dialog;
	gchar *str;

	str = g_strdup_printf( _( "Charset conversion error: %s" ), error->message );

	dialog = gtk_message_dialog_new(
			NULL,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_CLOSE,
			"%s", str );

	g_free( str );
	gtk_dialog_run( GTK_DIALOG( dialog ));
	gtk_widget_destroy( dialog );
}

static void
free_fields( GSList *fields )
{
	g_slist_free_full( fields, ( GDestroyNotify ) g_free );
}

static void
free_lines( GSList *lines )
{
	g_slist_free_full( lines, ( GDestroyNotify ) free_fields );
}

/*
 * settings is "folder;last_chosen_type;"
 */
static void
get_settings( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	GList *list, *it;
	const gchar *cstr;

	priv = self->priv;

	list = ofa_settings_get_string_list( st_prefs_import );

	it = list;
	cstr = ( it && it->data ? ( const gchar * ) it->data : NULL );
	if( cstr && g_utf8_strlen( cstr, -1 )){
		g_free( priv->p1_folder );
		priv->p1_folder = g_strdup( cstr );
	}

	it = it ? it->next : NULL;
	cstr = ( it && it->data ? ( const gchar * ) it->data : NULL );
	if( cstr && g_utf8_strlen( cstr, -1 )){
		priv->p2_type = atoi( cstr );
	}

	ofa_settings_free_string_list( list );
}

static void
update_settings( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	GList *list;
	gchar *str;

	priv = self->priv;

	list = g_list_append( NULL, g_strdup( priv->p1_folder ));

	str = g_strdup_printf( "%d", priv->p2_type );
	list = g_list_append( list, str );

	ofa_settings_set_string_list( st_prefs_import, list );

	ofa_settings_free_string_list( list );
}
