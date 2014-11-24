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

#include "ui/ofa-export-assistant.h"
#include "ui/ofa-export-settings.h"
#include "ui/ofa-export-settings-prefs.h"
#include "ui/ofa-main-window.h"

/* private instance data
 *
 * There is 3 useful pages
 * + one introduction page (page '0')
 * + one confirmation page (page '4)
 * + one result page (page '5')
 */
struct _ofaExportAssistantPrivate {

	/* p1: select data type to be exported
	 */
	GSList                 *p1_group;
	GtkWidget              *p1_btn;
	gint                    p1_idx;

	/* p2: select format
	 */
	ofaExportSettingsPrefs *p2_settings_prefs;
	ofaExportSettings      *p2_export_settings;

	/* p3: output file
	 */
	GtkFileChooser         *p3_chooser;
	gchar                  *p3_fname;		/* the output file */
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
 * @pref_name: the name of the exported settings for the corresponding
 *  data in user preferences (not localized)
 * @widget_name: the name of the widget in the page (not localized)
 * @base_name: the basename of the default output file (localized)
 *
 * The index to this structure will be set against each radio button.
 */
typedef struct {
	gint         code;
	const gchar *pref_name;
	const gchar *widget_name;
	const gchar *base_name;
}
	sTypes;

static const sTypes st_types[] = {
		{ DATA_ACCOUNT,  "Account",  "p1-account",  N_( "Accounts.csv" )},
		{ DATA_CLASS,    "Class",    "p1-class",    N_( "Classes.csv" )},
		{ DATA_CURRENCY, "Currency", "p1-currency", N_( "Currencies.csv" )},
		{ DATA_ENTRY,    "Entry",    "p1-entries",  N_( "Entries.csv" )},
		{ DATA_LEDGER,   "Ledger",   "p1-ledger",   N_( "Ledgers.csv" )},
		{ DATA_MODEL,    "Model",    "p1-model",    N_( "Models.csv" )},
		{ DATA_RATE,     "Rate",     "p1-rate",     N_( "Rates.csv" )},
		{ DATA_DOSSIER,  "Dossier",  "p1-dossier",  N_( "Dossier.csv" )},
		{ 0 }
};

/* data set against each data type radio button */
#define DATA_TYPE_INDEX                 "ofa-data-type"

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-export-assistant.ui";
static const gchar *st_ui_id            = "ExportAssistant";

static const gchar *st_pref_data        = "ExportAssistantContent";
static const gchar *st_pref_fname       = "ExportAssistantFilename";

G_DEFINE_TYPE( ofaExportAssistant, ofa_export_assistant, MY_TYPE_ASSISTANT )

static void      on_page_create( ofaExportAssistant *self, GtkWidget *page, gint page_num, void *empty );
static void      on_page_display( ofaExportAssistant *self, GtkWidget *page, gint page_num, void *empty );
static void      on_page_forward( ofaExportAssistant *self, GtkWidget *page, gint page_num, void *empty );
static void      p1_do_create( ofaExportAssistant *self, GtkWidget *page );
static gboolean  p1_is_complete( ofaExportAssistant *self );
static void      p1_do_forward( ofaExportAssistant *self, GtkWidget *page );
static void      p2_do_create( ofaExportAssistant *self, GtkWidget *page );
static void      p2_on_new_profile_clicked( GtkButton *button, ofaExportAssistant *self );
static void      p2_do_forward( ofaExportAssistant *self, GtkWidget *page );
static void      p3_do_create( ofaExportAssistant *self, GtkWidget *page );
static void      p3_on_file_activated( GtkFileChooser *chooser, ofaExportAssistant *self );
static void      p3_do_forward( ofaExportAssistant *self, GtkWidget *page );
static void      p4_do_display( ofaExportAssistant *self, GtkWidget *page );
static void      on_apply( GtkAssistant *assistant, ofaExportAssistant *self );
static gboolean  do_apply_export_data( ofaExportAssistant *self );

typedef void ( *cb )( ofaExportAssistant *, GtkWidget * );

typedef struct {
	gint  page_num;
	cb    create_cb;
	cb    display_cb;
	cb    forward_cb;
}
	sPagesCB;

static const sPagesCB st_pages_cb [] = {
		{ ASSIST_PAGE_INTRO,   NULL,         NULL,          NULL },
		{ ASSIST_PAGE_SELECT,  p1_do_create, NULL,          p1_do_forward },
		{ ASSIST_PAGE_FORMAT,  p2_do_create, NULL,          p2_do_forward },
		{ ASSIST_PAGE_OUTPUT,  p3_do_create, NULL,          p3_do_forward },
		{ ASSIST_PAGE_CONFIRM, NULL,         p4_do_display, NULL },
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
		g_clear_object( &priv->p2_settings_prefs );
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
			G_OBJECT( self ), MY_SIGNAL_PAGE_CREATE, G_CALLBACK( on_page_create ), NULL );
	g_signal_connect(
			G_OBJECT( self ), MY_SIGNAL_PAGE_DISPLAY, G_CALLBACK( on_page_display ), NULL );
	g_signal_connect(
			G_OBJECT( self ), MY_SIGNAL_PAGE_FORWARD, G_CALLBACK( on_page_forward ), NULL );

	my_assistant_signal_connect(
			MY_ASSISTANT( self ), "apply", G_CALLBACK( on_apply ));

	my_assistant_run( MY_ASSISTANT( self ));
}

/*
 * the provided 'page' is the toplevel widget of the assistant's page
 */
static void
on_page_create( ofaExportAssistant *self, GtkWidget *page, gint page_num, void *empty )
{
	static const gchar *thisfn = "ofa_export_assistant_on_page_create";
	gint i;

	g_return_if_fail( self && OFA_IS_EXPORT_ASSISTANT( self ));
	g_return_if_fail( page && GTK_IS_WIDGET( page ));

	g_debug( "%s: self=%p, page=%p, page_num=%d, empty=%p",
			thisfn, ( void * ) self, ( void * ) page, page_num, ( void * ) empty );

	for( i=0 ; st_pages_cb[i].page_num >= 0 ; ++i ){
		if( st_pages_cb[i].page_num == page_num ){
			if( st_pages_cb[i].create_cb ){
				st_pages_cb[i].create_cb( self, page );
				break;
			}
		}
	}
}

/*
 * the provided 'page' is the toplevel widget of the assistant's page
 */
static void
on_page_display( ofaExportAssistant *self, GtkWidget *page, gint page_num, void *empty )
{
	static const gchar *thisfn = "ofa_export_assistant_on_page_display";
	gint i;

	g_return_if_fail( self && OFA_IS_EXPORT_ASSISTANT( self ));
	g_return_if_fail( page && GTK_IS_WIDGET( page ));

	g_debug( "%s: self=%p, page=%p, page_num=%d, empty=%p",
			thisfn, ( void * ) self, ( void * ) page, page_num, ( void * ) empty );

	for( i=0 ; st_pages_cb[i].page_num >= 0 ; ++i ){
		if( st_pages_cb[i].page_num == page_num ){
			if( st_pages_cb[i].display_cb ){
				st_pages_cb[i].display_cb( self, page );
				break;
			}
		}
	}
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
p1_do_create( ofaExportAssistant *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p1_do_create";
	ofaExportAssistantPrivate *priv;
	GtkWidget *btn;
	gint i, last;
	gboolean found;

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;
	last = ofa_settings_get_uint( st_pref_data );
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
		ofa_settings_set_uint( st_pref_data, st_types[priv->p1_idx].code );
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
p2_do_create( ofaExportAssistant *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p2_do_create";
	ofaExportAssistantPrivate *priv;
	GtkWidget *widget;

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-alignment-parent" );
	g_return_if_fail( widget && GTK_IS_CONTAINER( widget ));

	priv->p2_settings_prefs = ofa_export_settings_prefs_new();
	ofa_export_settings_prefs_attach_to( priv->p2_settings_prefs, GTK_CONTAINER( widget ));
	ofa_export_settings_prefs_init_dialog( priv->p2_settings_prefs );
	ofa_export_settings_prefs_show_folder( priv->p2_settings_prefs, FALSE );

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

	ofa_export_settings_prefs_apply( priv->p2_settings_prefs );

	g_clear_object( &priv->p2_export_settings );
	priv->p2_export_settings = ofa_export_settings_new( NULL );
}

/*
 * p3: choose output file
 */
static void
p3_do_create( ofaExportAssistant *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p3_do_create";
	ofaExportAssistantPrivate *priv;
	gchar *fname;

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	g_return_if_fail( priv->p1_idx >= 0 );

	priv->p3_chooser =
			( GtkFileChooser * ) my_utils_container_get_child_by_name(
					GTK_CONTAINER( page ), "p3-filechooser" );
	g_return_if_fail( priv->p3_chooser && GTK_IS_FILE_CHOOSER( priv->p3_chooser ));

	g_signal_connect( G_OBJECT( priv->p3_chooser ),
			"file-activated", G_CALLBACK( p3_on_file_activated ), self );

	fname = ofa_settings_get_string( st_pref_fname );
	if( fname && g_utf8_strlen( fname, -1 )){
		gtk_file_chooser_set_filename( priv->p3_chooser, fname );
	} else {
		gtk_file_chooser_set_current_folder(
				priv->p3_chooser, ofa_export_settings_get_folder( priv->p2_export_settings ));
		gtk_file_chooser_set_current_name(
				priv->p3_chooser, st_types[priv->p1_idx].base_name );
	}
	g_free( fname );
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

	complete = ( priv->p3_fname && g_utf8_strlen( priv->p3_fname, -1 ) > 0 );

	if( complete ){
		ofa_settings_set_string( st_pref_fname, priv->p3_fname );
	}
}

/*
 * ask the user to confirm the operation
 */
static void
p4_do_display( ofaExportAssistant *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p4_do_prepare_confirm";
	ofaExportAssistantPrivate *priv;
	GtkWidget *label;
	gchar *str;
	gint format;
	GdkRGBA color;
	gboolean complete;

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
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
	gtk_label_set_text( GTK_LABEL( label ), ofa_export_settings_get_charmap( priv->p2_export_settings ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-date" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
	format = ofa_export_settings_get_date_format( priv->p2_export_settings );
	gtk_label_set_text( GTK_LABEL( label ), my_date_get_format_str( format ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-decimal" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
	str = g_strdup_printf( "%c", ofa_export_settings_get_decimal_sep( priv->p2_export_settings ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-field" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
	str = g_strdup_printf( "%c", ofa_export_settings_get_field_sep( priv->p2_export_settings ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-output" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
	gtk_label_set_text( GTK_LABEL( label ), priv->p3_fname );

	complete = ( priv->p3_fname && g_utf8_strlen( priv->p3_fname, -1 ) > 0 );
	my_assistant_set_page_complete( MY_ASSISTANT( self ), ASSIST_PAGE_CONFIRM, complete );
}

static void
on_apply( GtkAssistant *assistant, ofaExportAssistant *self )
{
	static const gchar *thisfn = "ofa_export_assistant_on_apply";

	g_return_if_fail( GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( OFA_IS_EXPORT_ASSISTANT( self ));

	if( !MY_WINDOW( self )->prot->dispose_has_run ){

		g_debug( "%s: assistant=%p, self=%p",
				thisfn, ( void * ) assistant, ( void * ) self );

		do_apply_export_data( self );
	}
}

static gboolean
do_apply_export_data( ofaExportAssistant *self )
{
	static const gchar *thisfn = "ofa_export_assistant_do_apply_export_data";
	GSList *lines, *i;
	GFile *output;
	GOutputStream *stream;
	GError *error;
	gint ret;
	gchar *str;
	gboolean ok;

	if( !my_utils_output_stream_new( self->priv->p3_fname, &output, &stream )){
		return( FALSE );
	}
	g_return_val_if_fail( G_IS_FILE_OUTPUT_STREAM( stream ), FALSE );

	/*lines = ( *st_export_datas[idx].fn_csv )
						( ofa_main_window_get_dossier( self->priv->main_window ));*/
	lines = NULL;

	for( i=lines ; i ; i=i->next ){

		str = g_strdup_printf( "%s\n", ( const gchar * ) i->data );
		ret = g_output_stream_write( stream, str, strlen( str ), NULL, &error );
		g_free( str );

		if( ret == -1 ){
			g_warning( "%s: g_output_stream_write: %s", thisfn, error->message );
			g_error_free( error );
			ok = FALSE;
			goto terminate;
		}
	}
	ok = TRUE;

terminate:
	g_output_stream_close( stream, NULL, NULL );
	g_slist_free_full( lines, ( GDestroyNotify ) g_free );
	g_object_unref( output );
	return( ok );
}
