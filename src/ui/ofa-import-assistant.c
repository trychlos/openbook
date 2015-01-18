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
#include "ui/ofa-file-format-bin.h"
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

	/* p1: introduction
	 */

	/* p2: select file to be imported
	 */
	GtkFileChooser   *p2_chooser;
	gchar            *p2_folder;
	gchar            *p2_uri;			/* the utf-8 imported filename */

	/* p3: select a type of data to be imported
	 */
	GSList           *p3_group;
	gint              p3_type;
	gint              p3_idx;
	GtkButton        *p3_type_btn;

	/* p4: locale settings
	 */
	ofaFileFormat    *p4_import_settings;
	ofaFileFormatBin *p4_settings_prefs;

	/* p5: confirm
	 */

	/* p6: import the file, display the result
	 */
	myProgressBar    *p6_import;
	myProgressBar    *p6_insert;
	GtkWidget        *p6_page;
	GtkWidget        *p6_text;
	ofaIImportable   *p6_object;
	ofaIImportable   *p6_plugin;

	/* runtime data
	 */
	GtkWidget        *current_page_w;
};

/* management of the radio buttons group
 * types are defined in ofa-iimporter.h
 */
typedef GType ( *fn_type )( void );
typedef struct {
	guint        type_id;
	const gchar *w_name;				/* the name of the radio button widget */
	fn_type      get_type;
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
		{ IMPORT_BAT,      "p3-releve",   NULL },
		{ IMPORT_CLASS,    "p3-class",    ofo_class_get_type },
		{ IMPORT_ACCOUNT,  "p3-account",  ofo_account_get_type },
		{ IMPORT_CURRENCY, "p3-currency", ofo_currency_get_type },
		{ IMPORT_LEDGER,   "p3-journals", ofo_ledger_get_type },
		{ IMPORT_MODEL,    "p3-model",    ofo_ope_template_get_type },
		{ IMPORT_RATE,     "p3-rate",     ofo_rate_get_type },
		{ IMPORT_ENTRY,    "p3-entries",  ofo_entry_get_type },
		{ 0 }
};

/* data set against each of above radio buttons */
#define DATA_BUTTON_INDEX               "ofa-data-button-idx"

/* the user preferences stored as a string list
 * folder
 */
static const gchar *st_prefs_import     = "ImportAssistant-settings";

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-import-assistant.ui";
static const gchar *st_ui_id            = "ImportAssistant";

static void            iimporter_iface_init( ofaIImporterInterface *iface );
static guint           iimporter_get_interface_version( const ofaIImporter *instance );
static void            p2_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void            p2_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void            p2_on_selection_changed( GtkFileChooser *chooser, ofaImportAssistant *self );
static void            p2_on_file_activated( GtkFileChooser *chooser, ofaImportAssistant *self );
static void            p2_check_for_complete( ofaImportAssistant *self );
static void            p2_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void            p3_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void            p3_on_type_toggled( GtkToggleButton *button, ofaImportAssistant *self );
static void            p3_check_for_complete( ofaImportAssistant *self );
static void            p3_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void            p4_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void            p4_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void            p4_on_settings_changed( ofaFileFormatBin *bin, ofaImportAssistant *self );
static void            p4_check_for_complete( ofaImportAssistant *self );
static void            p4_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void            p5_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void            p6_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static ofaIImportable *p6_search_for_plugin( ofaImportAssistant *self );
static void            p6_error_no_interface( const ofaImportAssistant *self );
static gboolean        p6_do_import( ofaImportAssistant *self );
static guint           p6_do_import_csv( ofaImportAssistant *self, guint *errors );
static guint           p6_do_import_other( ofaImportAssistant *self, guint *errors );
static void            p6_on_progress( ofaIImporter *importer, ofeImportablePhase phase, gdouble progress, const gchar *text, ofaImportAssistant *self );
static void            p6_on_message( ofaIImporter *importer, guint line_number, ofeImportableMsg status, const gchar *msg, ofaImportAssistant *self );
static GSList         *get_lines_from_csv( ofaImportAssistant *self );
static void            free_fields( GSList *fields );
static void            free_lines( GSList *lines );
static void            get_settings( ofaImportAssistant *self );
static void            update_settings( ofaImportAssistant *self );

G_DEFINE_TYPE_EXTENDED( ofaImportAssistant, ofa_import_assistant, MY_TYPE_ASSISTANT, 0, \
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTER, iimporter_iface_init ));

static const ofsAssistant st_pages_cb [] = {
		{ ASSIST_PAGE_INTRO,
				NULL,
				NULL,
				NULL },
		{ ASSIST_PAGE_SELECT,
				( myAssistantCb ) p2_do_init,
				( myAssistantCb ) p2_display,
				( myAssistantCb ) p2_do_forward },
		{ ASSIST_PAGE_TYPE,
				( myAssistantCb ) p3_do_init,
				NULL,
				( myAssistantCb ) p3_do_forward },
		{ ASSIST_PAGE_SETTINGS,
				( myAssistantCb ) p4_do_init,
				( myAssistantCb ) p4_display,
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
import_assistant_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_import_assistant_finalize";
	ofaImportAssistantPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_IMPORT_ASSISTANT( instance ));

	/* free data members here */
	priv = OFA_IMPORT_ASSISTANT( instance )->priv;

	g_free( priv->p2_folder );
	g_free( priv->p2_uri );

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
		g_clear_object( &priv->p4_import_settings );
		g_clear_object( &priv->p6_object );
		g_clear_object( &priv->p6_plugin );
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

	self->priv->p3_type = -1;
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
			G_OBJECT( self ), "progress", G_CALLBACK( p6_on_progress ), self );
	g_signal_connect(
			G_OBJECT( self ), "message", G_CALLBACK( p6_on_message ), self );

	my_assistant_set_callbacks( MY_ASSISTANT( self ), st_pages_cb );

	my_assistant_run( MY_ASSISTANT( self ));
}

/*
 * initialize the GtkFileChooser widget with the last used folder
 * we allow only a single selection and no folder creation
 */
static void
p2_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p2_do_init";
	ofaImportAssistantPrivate *priv;
	GtkWidget *widget;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;
	priv->current_page_w = page;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-filechooser" );
	g_return_if_fail( widget && GTK_IS_FILE_CHOOSER_WIDGET( widget ));
	priv->p2_chooser = GTK_FILE_CHOOSER( widget );

	g_signal_connect(
			G_OBJECT( widget ), "selection-changed", G_CALLBACK( p2_on_selection_changed ), self );
	g_signal_connect(
			G_OBJECT( widget ), "file-activated", G_CALLBACK( p2_on_file_activated ), self );
}

static void
p2_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	ofaImportAssistantPrivate *priv;

	priv = self->priv;

	if( priv->p2_folder ){
		gtk_file_chooser_set_current_folder_uri( priv->p2_chooser, priv->p2_folder );
	}
}

static void
p2_on_selection_changed( GtkFileChooser *chooser, ofaImportAssistant *self )
{
	p2_check_for_complete( self );
}

static void
p2_on_file_activated( GtkFileChooser *chooser, ofaImportAssistant *self )
{
	p2_check_for_complete( self );
}

static void
p2_check_for_complete( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gboolean ok;

	priv = self->priv;

	g_free( priv->p2_uri );
	priv->p2_uri = gtk_file_chooser_get_uri( priv->p2_chooser );

	ok = my_strlen( priv->p2_uri ) &&
			my_utils_uri_is_readable_file( priv->p2_uri );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_w, ok );
}

static void
p2_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	ofaImportAssistantPrivate *priv;

	priv = self->priv;

	g_free( priv->p2_folder );
	priv->p2_folder = gtk_file_chooser_get_current_folder_uri( priv->p2_chooser );

	update_settings( self );
}

/*
 * p2: nature of the data to import
 */
static void
p3_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p3_do_init";
	ofaImportAssistantPrivate *priv;
	gint i;
	GtkWidget *button;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;
	priv->current_page_w = page;

	for( i=0 ; st_radios[i].type_id ; ++i ){
		button = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), st_radios[i].w_name );
		g_object_set_data( G_OBJECT( button ), DATA_BUTTON_INDEX, GINT_TO_POINTER( i ));
		g_signal_connect( G_OBJECT( button ), "toggled", G_CALLBACK( p3_on_type_toggled ), self );
		if( st_radios[i].type_id == priv->p3_type ){
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), TRUE );
			p3_on_type_toggled( GTK_TOGGLE_BUTTON( button ), self );
		}
		if( !priv->p3_group ){
			priv->p3_group = gtk_radio_button_get_group( GTK_RADIO_BUTTON( button ));
		}
	}

	p3_check_for_complete( self );
}

static void
p3_on_type_toggled( GtkToggleButton *button, ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;

	priv = self->priv;

	if( gtk_toggle_button_get_active( button )){
		priv->p3_idx = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_BUTTON_INDEX ));
		priv->p3_type = st_radios[priv->p3_idx].type_id;
		priv->p3_type_btn = GTK_BUTTON( button );
	} else {
		priv->p3_idx = -1;
		priv->p3_type = 0;
		priv->p3_type_btn = NULL;
	}

	p3_check_for_complete( self );
}

static void
p3_check_for_complete( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;

	priv = self->priv;

	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_w, priv->p3_type > 0 );
}

static void
p3_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page )
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
p4_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p4_do_init";
	ofaImportAssistantPrivate *priv;
	GtkWidget *widget;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;
	priv->current_page_w = page;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-settings-parent" );
	g_return_if_fail( widget && GTK_IS_CONTAINER( widget ));

	priv->p4_import_settings = ofa_file_format_new( SETTINGS_IMPORT_SETTINGS );
	priv->p4_settings_prefs = ofa_file_format_bin_new( priv->p4_import_settings );
	gtk_container_add( GTK_CONTAINER( widget ), GTK_WIDGET( priv->p4_settings_prefs ));

	g_signal_connect(
			G_OBJECT( priv->p4_settings_prefs ), "changed", G_CALLBACK( p4_on_settings_changed ), self );
}

static void
p4_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	p4_check_for_complete( self );
}

static void
p4_on_settings_changed( ofaFileFormatBin *bin, ofaImportAssistant *self )
{
	p4_check_for_complete( self );
}

static void
p4_check_for_complete( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gboolean ok;

	priv = self->priv;
	ok = ofa_file_format_bin_is_validable( priv->p4_settings_prefs );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_w, ok );
}

static void
p4_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p4_do_forward";
	ofaImportAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	ofa_file_format_bin_apply( priv->p4_settings_prefs );
}

/*
 * ask the user to confirm the operation
 */
static void
p5_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p5_do_display";
	ofaImportAssistantPrivate *priv;
	GdkRGBA color;
	GtkWidget *label;
	gchar *str;
	gboolean complete;
	myDateFormat format;
	ofaFFtype ffmt;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	gdk_rgba_parse( &color, "#0000ff" );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-fname" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
	gtk_label_set_text( GTK_LABEL( label ), priv->p2_uri );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-type" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
	str = my_utils_str_remove_underlines( gtk_button_get_label( GTK_BUTTON( priv->p3_type_btn )));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	ffmt = ofa_file_format_get_fftype( priv->p4_import_settings );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-ffmt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
	gtk_label_set_text( GTK_LABEL( label ), ofa_file_format_get_fftype_str( ffmt ));

	if( ffmt != OFA_FFTYPE_OTHER ){

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-charmap" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
		gtk_label_set_text( GTK_LABEL( label ), ofa_file_format_get_charmap( priv->p4_import_settings ));

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-date" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
		format = ofa_file_format_get_date_format( priv->p4_import_settings );
		gtk_label_set_text( GTK_LABEL( label ), my_date_get_format_str( format ));

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-decimal" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
		str = g_strdup_printf( "%c", ofa_file_format_get_decimal_sep( priv->p4_import_settings ));
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-field" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
		str = g_strdup_printf( "%c", ofa_file_format_get_field_sep( priv->p4_import_settings ));
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-headers" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
		str = g_strdup_printf( "%d", ofa_file_format_get_headers_count( priv->p4_import_settings ));
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );
	}

	gtk_widget_show_all( page );

	complete = ( priv->p2_uri && g_utf8_strlen( priv->p2_uri, -1 ) > 0 );
	my_assistant_set_page_complete( MY_ASSISTANT( self ), page, complete );
}

static void
p6_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p6_do_display";
	ofaImportAssistantPrivate *priv;
	GtkWidget *parent;

	g_return_if_fail( OFA_IS_IMPORT_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	my_assistant_set_page_complete( MY_ASSISTANT( self ), page, FALSE );

	priv = self->priv;
	priv->current_page_w = page;

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-bar-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p6_import = my_progress_bar_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p6_import ));

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-insert-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p6_insert = my_progress_bar_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p6_insert ));

	priv->p6_text = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-text-view" );
	g_return_if_fail( priv->p6_text && GTK_IS_TEXT_VIEW( priv->p6_text ));
	gtk_widget_set_can_focus( priv->p6_text, FALSE );

	priv->p6_page = page;
	gtk_widget_show_all( page );

	/* search from something which would be able to import the data */
	if( st_radios[priv->p3_idx].get_type ){
		priv->p6_object = ( ofaIImportable * ) g_object_new( st_radios[priv->p3_idx].get_type(), NULL );
	} else {
		priv->p6_plugin = p6_search_for_plugin( self );
	}
	if( !OFA_IS_IIMPORTABLE( priv->p6_object ) && !priv->p6_plugin ){
		p6_error_no_interface( self );
		return;
	}

	g_idle_add(( GSourceFunc ) p6_do_import, self );
}

/*
 * search for a plugin willing to import an 'other' format
 */
static ofaIImportable *
p6_search_for_plugin( ofaImportAssistant *self )
{
	static const gchar *thisfn = "ofa_import_assistant_p6_search_for_plugin";
	ofaImportAssistantPrivate *priv;
	GList *modules, *it;
	ofaIImportable *found;

	priv = self->priv;
	found = NULL;
	modules = ofa_plugin_get_extensions_for_type( OFA_TYPE_IIMPORTABLE );
	g_debug( "%s: modules=%p, count=%d", thisfn, ( void * ) modules, g_list_length( modules ));

	for( it=modules ; it ; it=it->next ){
		if( ofa_iimportable_is_willing_to(
				OFA_IIMPORTABLE( it->data ), priv->p2_uri, priv->p4_import_settings )){
			found = g_object_ref( OFA_IIMPORTABLE( it->data ));
			break;
		}
	}

	ofa_plugin_free_extensions( modules );

	return( found );
}

static void
p6_error_no_interface( const ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gchar *str;

	priv = self->priv;

	if( priv->p6_object ){
		str = g_strdup_printf(
					_( "The requested type (%s) does not implement the IImportable interface" ),
					G_OBJECT_TYPE_NAME( priv->p6_object ));
	} else {
		str = g_strdup_printf( _( "Unable to find a plugin to import the specified data" ));
	}

	my_utils_dialog_error( str );

	g_free( str );
}

static gboolean
p6_do_import( ofaImportAssistant *self )
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
	ffmt = ofa_file_format_get_fftype( priv->p4_import_settings );
	switch( ffmt ){
		case OFA_FFTYPE_CSV:
			count = p6_do_import_csv( self, &errors );
			break;
		case OFA_FFTYPE_OTHER:
			g_return_val_if_fail( priv->p6_plugin && OFA_IS_IIMPORTABLE( priv->p6_plugin ), FALSE );
			count = p6_do_import_other( self, &errors );
			break;
		default:
			has_worked = FALSE;
			break;
	}

	/* then display the result */
	label = my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )) ), "p6-label" );

	str = my_utils_str_remove_underlines( gtk_button_get_label( GTK_BUTTON( priv->p3_type_btn )));

	if( has_worked ){
		if( !errors ){
			count = ofa_iimportable_get_count( priv->p6_object );
			text = g_strdup_printf( _( "OK: %u lines from '%s' have been successfully "
					"imported into « %s »." ),
					count, priv->p2_uri, str );
		} else {
			text = g_strdup_printf( _( "Unfortunately, '%s' import has encountered errors.\n"
					"The « %s » recordset has been left unchanged.\n"
					"Please fix these errors, and retry then." ), priv->p2_uri, str );
		}
	} else {
		text = g_strdup_printf( _( "Unfortunately, the required file format is not "
				"managed at this time.\n"
				"Please fix this, and retry, then." ));
	}

	g_free( str );

	gtk_label_set_text( GTK_LABEL( label ), text );
	g_free( text );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_w, TRUE );

	/* do not continue and remove from idle callbacks list */
	return( FALSE );
}

static guint
p6_do_import_csv( ofaImportAssistant *self, guint *errors )
{
	ofaImportAssistantPrivate *priv;
	guint count;
	GSList *lines;

	priv = self->priv;

	lines = get_lines_from_csv( self );
	count = g_slist_length( lines );

	*errors = ofa_iimportable_import( priv->p6_object,
			lines, priv->p4_import_settings, MY_WINDOW( self )->prot->dossier, self );

	free_lines( lines );

	return( count );
}

static guint
p6_do_import_other( ofaImportAssistant *self, guint *errors )
{
	ofaImportAssistantPrivate *priv;
	guint count;

	priv = self->priv;

	*errors = ofa_iimportable_import_fname( priv->p6_plugin, MY_WINDOW( self )->prot->dossier, self );
	count = ofa_iimportable_get_count( priv->p6_plugin );

	return( count );
}

static void
p6_on_progress( ofaIImporter *importer, ofeImportablePhase phase, gdouble progress, const gchar *text, ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;

	priv = self->priv;

	if( phase == IMPORTABLE_PHASE_IMPORT ){
		g_signal_emit_by_name( priv->p6_import, "ofa-double", progress );
		g_signal_emit_by_name( priv->p6_import, "ofa-text", text );

	} else {
		g_return_if_fail( phase == IMPORTABLE_PHASE_INSERT );
		g_signal_emit_by_name( priv->p6_insert, "ofa-double", progress );
		g_signal_emit_by_name( priv->p6_insert, "ofa-text", text );
	}
}

static void
p6_on_message( ofaIImporter *importer, guint line_number, ofeImportableMsg status, const gchar *msg, ofaImportAssistant *self )
{
	static const gchar *thisfn = "ofa_import_assistant_p6_on_error";
	ofaImportAssistantPrivate *priv;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	gchar *str;

	g_debug( "%s: importer=%p, line_number=%u, msg=%s, self=%p",
			thisfn, ( void * ) importer, line_number, msg, ( void * ) self );

	priv = self->priv;

	buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( priv->p6_text ));
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
	gchar *field, *field_sep;

	priv = self->priv;

	sysfname = my_utils_filename_from_utf8( priv->p2_uri );
	if( !sysfname ){
		return( NULL );
	}
	gfile = g_file_new_for_path( sysfname );
	g_free( sysfname );

	error = NULL;
	if( !g_file_load_contents( gfile, NULL, &contents, NULL, NULL, &error )){
		str = g_strdup_printf( _( "Unable to load content from '%s' file: %s" ),
				priv->p2_uri, error->message );
		my_utils_dialog_error( str );
		g_free( str );
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
	field_sep = g_strdup_printf( "%c", ofa_file_format_get_field_sep( priv->p4_import_settings ));

	while( *iter_line ){
		error = NULL;
		str = g_convert( *iter_line, -1,
								ofa_file_format_get_charmap( priv->p4_import_settings ),
								"UTF-8", NULL, NULL, &error );
		if( !str ){
			str = g_strdup_printf( _( "Charset conversion error: %s" ), error->message );
			my_utils_dialog_error( str );
			g_free( str );
			g_strfreev( lines );
			return( NULL );
		}
		if( g_utf8_strlen( *iter_line, -1 )){
			fields = g_strsplit(( const gchar * ) *iter_line, field_sep, -1 );
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

	g_free( field_sep );
	g_strfreev( lines );
	return( g_slist_reverse( s_lines ));
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
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->p3_type = atoi( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		g_free( priv->p2_folder );
		priv->p2_folder = g_strdup( cstr );
	}

	ofa_settings_free_string_list( list );
}

static void
update_settings( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gchar *str;

	priv = self->priv;

	str = g_strdup_printf( "%d;%s;",
			priv->p3_type,
			priv->p2_folder ? priv->p2_folder : "" );

	ofa_settings_set_string( st_prefs_import, str );

	g_free( str );
}
