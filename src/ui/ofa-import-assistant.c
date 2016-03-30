/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <stdlib.h>

#include "my/my-iassistant.h"
#include "my/my-iwindow.h"
#include "my/my-progress-bar.h"
#include "my/my-utils.h"

#include "api/ofa-file-format.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-igetter.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-iimporter.h"
#include "api/ofa-iregister.h"
#include "api/ofa-preferences.h"
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

#include "core/ofa-file-format-bin.h"

#include "ui/ofa-import-assistant.h"

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
typedef struct {
	gboolean          dispose_has_run;

	/* initialization
	 */
	ofaIGetter       *getter;
	ofaIDBMeta       *meta;

	/* p0: introduction
	 */

	/* p1: select file to be imported
	 */
	GtkFileChooser   *p1_chooser;
	gchar            *p1_folder;
	gchar            *p1_furi;			/* the utf-8 imported filename */
	gchar            *p1_content;

	/* p2: select a type of data to be imported
	 */
	GtkWidget        *p2_furi;
	GtkWidget        *p2_content;
	GList            *p2_importables;
	GtkWidget        *p2_parent;
	gint              p2_row;
	GtkWidget        *p2_selected_btn;
	gchar            *p2_selected_class;
	gchar            *p2_selected_label;

	/* p3: locale settings
	 */
	GtkWidget        *p3_furi;
	GtkWidget        *p3_content;
	GtkWidget        *p3_datatype;
	gchar            *p3_settings_key;
	ofaFileFormat    *p3_import_settings;
	ofaFileFormatBin *p3_settings_prefs;
	GtkWidget        *p3_message;

	/* p4: confirm
	 */

	/* p5: import the file, display the result
	 */
	myProgressBar    *p5_import;
	myProgressBar    *p5_insert;
	GtkWidget        *p5_page;
	GtkWidget        *p5_text;
	ofaIImportable   *p5_object;
	ofaIImportable   *p5_plugin;
}
	ofaImportAssistantPrivate;

/* management of the radio buttons group
 * types are defined in ofa-iimporter.h
 */
typedef GType ( *fn_type )( void );
typedef struct {
	guint        type_id;
	const gchar *w_name;				/* the name of the radio button widget */
	fn_type      get_type;
	const gchar *import_suffix;
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
		{ IMPORT_BAT,      "p2-releve",   NULL,                      "Bat" },
		{ IMPORT_CLASS,    "p2-class",    ofo_class_get_type,        "Class" },
		{ IMPORT_ACCOUNT,  "p2-account",  ofo_account_get_type,      "Account" },
		{ IMPORT_CURRENCY, "p2-currency", ofo_currency_get_type,     "Currency" },
		{ IMPORT_LEDGER,   "p2-journals", ofo_ledger_get_type,       "Ledger" },
		{ IMPORT_MODEL,    "p2-model",    ofo_ope_template_get_type, "Model" },
		{ IMPORT_RATE,     "p2-rate",     ofo_rate_get_type,         "Rate" },
		{ IMPORT_ENTRY,    "p2-entries",  ofo_entry_get_type,        "Entry" },
		{ 0 }
};

/* data set against each of above radio buttons */
#define DATA_TYPE_INDEX                   "ofa-data-type"

static const gchar *st_import_folder    = "ofa-LastImportFolder";
static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-import-assistant.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     iwindow_read_settings( myIWindow *instance, myISettings *settings, const gchar *keyname );
static void     set_settings( ofaImportAssistant *self );
static void     iassistant_iface_init( myIAssistantInterface *iface );
static gboolean iassistant_is_willing_to_quit( myIAssistant*instance, guint keyval );
static void     iimporter_iface_init( ofaIImporterInterface *iface );
static guint    iimporter_get_interface_version( const ofaIImporter *instance );
static void     p0_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p1_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p1_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p1_on_selection_changed( GtkFileChooser *chooser, ofaImportAssistant *self );
static void     p1_on_file_activated( GtkFileChooser *chooser, ofaImportAssistant *self );
static gboolean p1_check_for_complete( ofaImportAssistant *self );
static void     p1_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p2_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p2_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p2_on_type_toggled( GtkToggleButton *button, ofaImportAssistant *self );
static void     p2_check_for_complete( ofaImportAssistant *self );
static void     p2_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p3_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p3_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p3_on_settings_changed( ofaFileFormatBin *bin, ofaImportAssistant *self );
static void     p3_check_for_complete( ofaImportAssistant *self );
static void     p3_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p4_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p5_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p5_error_no_interface( const ofaImportAssistant *self );
static gboolean p5_do_import( ofaImportAssistant *self );
static guint    p5_do_import_csv( ofaImportAssistant *self, guint *errors );
static guint    p5_do_import_other( ofaImportAssistant *self, guint *errors );
static void     p5_on_progress( ofaIImporter *importer, ofeImportablePhase phase, gdouble progress, const gchar *text, ofaImportAssistant *self );
static void     p5_on_pulse( ofaIImporter *importer, ofeImportablePhase phase, ofaImportAssistant *self );
static void     p5_on_message( ofaIImporter *importer, guint line_number, ofeImportableMsg status, const gchar *msg, ofaImportAssistant *self );

G_DEFINE_TYPE_EXTENDED( ofaImportAssistant, ofa_import_assistant, GTK_TYPE_ASSISTANT, 0,
		G_ADD_PRIVATE( ofaImportAssistant )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IASSISTANT, iassistant_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTER, iimporter_iface_init ))

static const ofsIAssistant st_pages_cb [] = {
		{ ASSIST_PAGE_INTRO,
				NULL,
				NULL,
				( myIAssistantCb ) p0_do_forward },
		{ ASSIST_PAGE_SELECT,
				( myIAssistantCb ) p1_do_init,
				( myIAssistantCb ) p1_do_display,
				( myIAssistantCb ) p1_do_forward },
		{ ASSIST_PAGE_TYPE,
				( myIAssistantCb ) p2_do_init,
				( myIAssistantCb ) p2_do_display,
				( myIAssistantCb ) p2_do_forward },
		{ ASSIST_PAGE_SETTINGS,
				( myIAssistantCb ) p3_do_init,
				( myIAssistantCb ) p3_do_display,
				( myIAssistantCb ) p3_do_forward },
		{ ASSIST_PAGE_CONFIRM,
				NULL,
				( myIAssistantCb ) p4_do_display,
				NULL },
		{ ASSIST_PAGE_DONE,
				NULL,
				( myIAssistantCb ) p5_do_display,
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
	priv = ofa_import_assistant_get_instance_private( OFA_IMPORT_ASSISTANT( instance ));

	g_free( priv->p1_folder );
	g_free( priv->p1_furi );
	g_free( priv->p1_content );
	g_free( priv->p2_selected_class );
	g_free( priv->p2_selected_label );
	g_free( priv->p3_settings_key );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_import_assistant_parent_class )->finalize( instance );
}

static void
import_assistant_dispose( GObject *instance )
{
	ofaImportAssistantPrivate *priv;

	g_return_if_fail( instance && OFA_IS_IMPORT_ASSISTANT( instance ));

	priv = ofa_import_assistant_get_instance_private( OFA_IMPORT_ASSISTANT( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->meta );
		g_list_free_full( priv->p2_importables, ( GDestroyNotify ) g_object_unref );
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
	ofaImportAssistantPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_IMPORT_ASSISTANT( self ));

	priv = ofa_import_assistant_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_import_assistant_class_init( ofaImportAssistantClass *klass )
{
	static const gchar *thisfn = "ofa_import_assistant_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = import_assistant_dispose;
	G_OBJECT_CLASS( klass )->finalize = import_assistant_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_import_assistant_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 *
 * Run the assistant.
 */
void
ofa_import_assistant_run( ofaIGetter *getter, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_import_assistant_run";
	ofaImportAssistant *self;
	ofaImportAssistantPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p", thisfn, ( void * ) getter, ( void * ) parent );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_IMPORT_ASSISTANT, NULL );
	my_iwindow_set_parent( MY_IWINDOW( self ), parent );
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

	priv = ofa_import_assistant_get_instance_private( self );

	priv->getter = getter;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
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
	ofaImportAssistantPrivate *priv;
	ofaHub *hub;
	const ofaIDBConnect *connect;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_import_assistant_get_instance_private( OFA_IMPORT_ASSISTANT( instance ));

	my_iassistant_set_callbacks( MY_IASSISTANT( instance ), st_pages_cb );

	hub = ofa_igetter_get_hub( priv->getter );
	connect = ofa_hub_get_connect( hub );
	priv->meta = ofa_idbconnect_get_meta( connect );

	/* messages provided by the ofaIImporter interface */
	g_signal_connect( instance, "pulse", G_CALLBACK( p5_on_pulse ), instance );
	g_signal_connect( instance, "progress", G_CALLBACK( p5_on_progress ), instance );
	g_signal_connect( instance, "message", G_CALLBACK( p5_on_message ), instance );
}

/*
 * user settings are: class_name;
 * dossier settings are: last_import_folder_uri
 */
static void
iwindow_read_settings( myIWindow *instance, myISettings *settings, const gchar *keyname )
{
	ofaImportAssistantPrivate *priv;
	GList *slist, *it;
	const gchar *cstr;

	priv = ofa_import_assistant_get_instance_private( OFA_IMPORT_ASSISTANT( instance ));

	slist = my_isettings_get_string_list( settings, SETTINGS_GROUP_GENERAL, keyname );

	if( slist ){
		it = slist;
		cstr = it ? ( const gchar * ) it->data : NULL;
		if( my_strlen( cstr )){
			priv->p2_selected_class = g_strdup( cstr );
		}

		my_isettings_free_string_list( settings, slist );
	}

	priv->p1_folder = ofa_settings_dossier_get_string( priv->meta, st_import_folder );
}

static void
set_settings( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	myISettings *settings;
	gchar *keyname, *str;

	priv = ofa_import_assistant_get_instance_private( self );

	settings = my_iwindow_get_settings( MY_IWINDOW( self ));
	keyname = my_iwindow_get_keyname( MY_IWINDOW( self ));

	str = g_strdup_printf( "%s;",
			priv->p2_selected_class ? priv->p2_selected_class : "" );

	my_isettings_set_string( settings, SETTINGS_GROUP_GENERAL, keyname, str );

	g_free( str );
	g_free( keyname );

	if( my_strlen( priv->p1_folder )){
		ofa_settings_dossier_set_string( priv->meta, st_import_folder, priv->p1_folder );
	}
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
	return( ofa_prefs_assistant_is_willing_to_quit( keyval ));
}

static void
p0_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page_widget )
{
	static const gchar *thisfn = "ofa_import_assistant_p0_do_forward";

	g_debug( "%s: self=%p, page_num=%d, page_widget=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page_widget, G_OBJECT_TYPE_NAME( page_widget ));
}

/*
 * initialize the GtkFileChooser widget with the last used folder
 * we allow only a single selection and no folder creation
 */
static void
p1_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p1_do_init";
	ofaImportAssistantPrivate *priv;
	GtkWidget *widget;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p1-filechooser" );
	g_return_if_fail( widget && GTK_IS_FILE_CHOOSER_WIDGET( widget ));
	priv->p1_chooser = GTK_FILE_CHOOSER( widget );
	gtk_file_chooser_set_action( priv->p1_chooser, GTK_FILE_CHOOSER_ACTION_OPEN );

	g_signal_connect( widget, "selection-changed", G_CALLBACK( p1_on_selection_changed ), self );
	g_signal_connect( widget, "file-activated", G_CALLBACK( p1_on_file_activated ), self );
}

/*
 * gtk_file_chooser_select_uri() is supposed to first set the ad-hoc
 * folder, and then select the appropriate file - but doesn't seem to
 * work alone :(
 *
 * gtk_file_chooser_set_current_folder_uri() rightly set the current
 * folder, but reset the selection (and have a bad location 'ebp_')
 * so, has to select after the right file
 *
 * gtk_file_chooser_set_uri() doesn't work (doesn't select the parent
 * folder, doesn't select the file)
 * gtk_file_chooser_set_filename() idem
 */
static void
p1_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	/*static const gchar *thisfn = "ofa_import_assistant_p1_do_display";*/
	ofaImportAssistantPrivate *priv;

	priv = ofa_import_assistant_get_instance_private( self );

	if( priv->p1_furi ){
		gtk_file_chooser_set_uri( priv->p1_chooser, priv->p1_furi );

	} else if( priv->p1_folder ){
		gtk_file_chooser_set_current_folder_uri( priv->p1_chooser, priv->p1_folder );
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
	if( p1_check_for_complete( self )){
		gtk_assistant_next_page( GTK_ASSISTANT( self ));
	}
}

static gboolean
p1_check_for_complete( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gboolean ok;

	priv = ofa_import_assistant_get_instance_private( self );

	g_free( priv->p1_furi );
	priv->p1_furi = gtk_file_chooser_get_uri( priv->p1_chooser );

	ok = my_strlen( priv->p1_furi ) &&
			my_utils_uri_is_readable_file( priv->p1_furi );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );

	return( ok );
}

static void
p1_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p1_do_forward";
	ofaImportAssistantPrivate *priv;

	priv = ofa_import_assistant_get_instance_private( self );

	g_free( priv->p1_folder );
	priv->p1_folder = gtk_file_chooser_get_current_folder_uri( priv->p1_chooser );

	g_free( priv->p1_content );
	priv->p1_content = g_content_type_guess( priv->p1_furi, NULL, 0, NULL );

	g_debug( "%s: uri=%s, folder=%s", thisfn, priv->p1_furi, priv->p1_folder );

	set_settings( self );
}

/*
 * p2: nature of the data to import
 */
static void
p2_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p2_do_init";
	ofaImportAssistantPrivate *priv;
	ofaHub *hub;
	GList *it;
	gchar *label;
	GtkWidget *btn, *first;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	priv->p2_furi = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-furi" );
	g_return_if_fail( priv->p2_furi && GTK_IS_LABEL( priv->p2_furi ));
	my_utils_widget_set_style( priv->p2_furi, "labelinfo" );

	priv->p2_content = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-content" );
	g_return_if_fail( priv->p2_content && GTK_IS_LABEL( priv->p2_content ));
	my_utils_widget_set_style( priv->p2_content, "labelinfo" );

	hub = ofa_igetter_get_hub( priv->getter );
	priv->p2_importables = ofa_hub_get_for_type( hub, OFA_TYPE_IIMPORTABLE );
	g_debug( "%s: importables count=%d", thisfn, g_list_length( priv->p2_importables ));
	priv->p2_selected_btn = NULL;
	priv->p2_row = 0;
	first = NULL;

	priv->p2_parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "grid22-parent" );
	g_return_if_fail( priv->p2_parent && GTK_IS_GRID( priv->p2_parent ));

	for( it=priv->p2_importables ; it ; it=it->next ){
		label = ofa_iimportable_get_label( OFA_IIMPORTABLE( it->data ));
		if( my_strlen( label )){
			if( !first ){
				btn = gtk_radio_button_new_with_mnemonic( NULL, label );
				first = btn;
			} else {
				btn = gtk_radio_button_new_with_mnemonic_from_widget( GTK_RADIO_BUTTON( first ), label );
			}
			g_object_set_data( G_OBJECT( btn ), DATA_TYPE_INDEX, it->data );
			g_signal_connect( btn, "toggled", G_CALLBACK( p2_on_type_toggled ), self );
			gtk_grid_attach( GTK_GRID( priv->p2_parent ), btn, 0, priv->p2_row++, 1, 1 );
		}
		g_free( label );
	}
}

static void
p2_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p2_do_display";
	ofaImportAssistantPrivate *priv;
	gint i;
	GtkWidget *btn;
	ofaIImportable *object;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->p2_furi ), priv->p1_furi );
	gtk_label_set_text( GTK_LABEL( priv->p2_content ), priv->p1_content );

	if( my_strlen( priv->p2_selected_class )){
		for( i=0 ; i<priv->p2_row ; ++i ){
			btn = gtk_grid_get_child_at( GTK_GRID( priv->p2_parent ), 0, i );
			g_return_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ));
			object = OFA_IIMPORTABLE( g_object_get_data( G_OBJECT( btn ), DATA_TYPE_INDEX ));
			if( !my_collate( G_OBJECT_TYPE_NAME( object ), priv->p2_selected_class )){
				gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( btn ), TRUE );
				break;
			}
		}
	}

	p2_check_for_complete( self );
}

static void
p2_on_type_toggled( GtkToggleButton *button, ofaImportAssistant *self )
{
	p2_check_for_complete( self );
}

static void
p2_check_for_complete( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gint i;
	GtkWidget *btn;
	ofaIImportable *object;
	const gchar *label;

	priv = ofa_import_assistant_get_instance_private( self );

	/* which is the currently active button ? */
	for( i=0 ; i<priv->p2_row ; ++i ){
		btn = gtk_grid_get_child_at( GTK_GRID( priv->p2_parent ), 0, i );
		if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( btn ))){
			object = OFA_IIMPORTABLE( g_object_get_data( G_OBJECT( btn ), DATA_TYPE_INDEX ));
			priv->p2_selected_btn = btn;
			g_free( priv->p2_selected_class );
			priv->p2_selected_class = g_strdup( G_OBJECT_TYPE_NAME( object ));
			g_free( priv->p2_selected_label );
			label = gtk_button_get_label( GTK_BUTTON( priv->p2_selected_btn ));
			priv->p2_selected_label = my_utils_str_remove_underlines( label );
			break;
		}
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), priv->p2_selected_btn != NULL );
}

static void
p2_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	set_settings( self );
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
p3_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p3_do_init";
	ofaImportAssistantPrivate *priv;
	GtkWidget *parent, *label;
	GtkSizeGroup *hgroup;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	priv->p3_furi = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-furi" );
	g_return_if_fail( priv->p3_furi && GTK_IS_LABEL( priv->p3_furi ));
	my_utils_widget_set_style( priv->p3_furi, "labelinfo" );

	priv->p3_content = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-content" );
	g_return_if_fail( priv->p3_content && GTK_IS_LABEL( priv->p3_content ));
	my_utils_widget_set_style( priv->p3_content, "labelinfo" );

	priv->p3_datatype = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-datatype" );
	g_return_if_fail( priv->p3_datatype && GTK_IS_LABEL( priv->p3_datatype ));
	my_utils_widget_set_style( priv->p3_datatype, "labelinfo" );

	hgroup = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-label311" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-label312" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-settings-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->p3_settings_prefs = ofa_file_format_bin_new( NULL );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p3_settings_prefs ));
	my_utils_size_group_add_size_group(
			hgroup, ofa_file_format_bin_get_size_group( priv->p3_settings_prefs, 0 ));

	g_signal_connect(
			priv->p3_settings_prefs, "ofa-changed", G_CALLBACK( p3_on_settings_changed ), self );

	priv->p3_message = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-message" );
	g_return_if_fail( priv->p3_message && GTK_IS_LABEL( priv->p3_message ));
	my_utils_widget_set_style( priv->p3_message, "labelerror" );

	g_object_unref( hgroup );
}

static void
p3_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p3_do_init";
	ofaImportAssistantPrivate *priv;
	gchar *candidate_key;
	const gchar *found_key;
	myISettings *instance;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->p3_furi ), priv->p1_furi );
	gtk_label_set_text( GTK_LABEL( priv->p3_content ), priv->p1_content );
	gtk_label_set_text( GTK_LABEL( priv->p3_datatype ), priv->p2_selected_label );

	candidate_key =  g_strdup_printf( "%s-%s", SETTINGS_IMPORT_SETTINGS, priv->p2_selected_class );
	instance = ofa_settings_get_settings( SETTINGS_TARGET_USER );
	g_return_if_fail( instance && MY_IS_ISETTINGS( instance ));

	if( my_isettings_has_key( instance, SETTINGS_GROUP_GENERAL, candidate_key )){
		found_key = candidate_key;
	} else {
		g_debug( "%s: candidate_key=%s not found", thisfn, candidate_key );
		found_key = SETTINGS_IMPORT_SETTINGS;
	}
	priv->p3_settings_key = g_strdup( candidate_key );

	g_clear_object( &priv->p3_import_settings );
	priv->p3_import_settings = ofa_file_format_new( found_key );
	ofa_file_format_set_prefs_name( priv->p3_import_settings, priv->p3_settings_key );
	ofa_file_format_set_mode( priv->p3_import_settings, OFA_FFMODE_IMPORT );
	ofa_file_format_bin_set_format( priv->p3_settings_prefs, priv->p3_import_settings );

	g_free( candidate_key );

	p3_check_for_complete( self );
}

static void
p3_on_settings_changed( ofaFileFormatBin *bin, ofaImportAssistant *self )
{
	p3_check_for_complete( self );
}

static void
p3_check_for_complete( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gboolean ok;
	gchar *message;

	priv = ofa_import_assistant_get_instance_private( self );

	ok = ofa_file_format_bin_is_valid( priv->p3_settings_prefs, &message );

	gtk_label_set_text( GTK_LABEL( priv->p3_message ), my_strlen( message ) ? message : "" );
	g_free( message );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );
}

static void
p3_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p3_do_forward";
	ofaImportAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	ofa_file_format_bin_apply( priv->p3_settings_prefs );
}

/*
 * ask the user to confirm the operation
 */
static void
p4_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p4_do_display";
	ofaImportAssistantPrivate *priv;
	GtkWidget *label;
	gboolean complete;
	myDateFormat format;
	ofaFFtype ffmt;
	gchar *str;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-furi" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), priv->p1_furi );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-type" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), priv->p2_selected_label );

	ffmt = ofa_file_format_get_fftype( priv->p3_import_settings );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-ffmt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), ofa_file_format_get_fftype_str( ffmt ));

	if( ffmt != OFA_FFTYPE_OTHER ){

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-charmap" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_utils_widget_set_style( label, "labelinfo" );
		gtk_label_set_text( GTK_LABEL( label ), ofa_file_format_get_charmap( priv->p3_import_settings ));

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-date" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_utils_widget_set_style( label, "labelinfo" );
		format = ofa_file_format_get_date_format( priv->p3_import_settings );
		gtk_label_set_text( GTK_LABEL( label ), my_date_get_format_str( format ));

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-decimal" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_utils_widget_set_style( label, "labelinfo" );
		str = g_strdup_printf( "%c", ofa_file_format_get_decimal_sep( priv->p3_import_settings ));
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-field" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_utils_widget_set_style( label, "labelinfo" );
		str = g_strdup_printf( "%c", ofa_file_format_get_field_sep( priv->p3_import_settings ));
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-strdelim" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_utils_widget_set_style( label, "labelinfo" );
		str = g_strdup_printf( "%c", ofa_file_format_get_string_delim( priv->p3_import_settings ));
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-headers" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_utils_widget_set_style( label, "labelinfo" );
		str = g_strdup_printf( "%d", ofa_file_format_get_headers_count( priv->p3_import_settings ));
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );
	}

	complete = my_strlen( priv->p1_furi ) > 0;
	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), complete );
}

/*
 * import
 * execution summary
 */
static void
p5_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p5_do_display";
	ofaImportAssistantPrivate *priv;
	GtkWidget *parent;
	ofaHub *hub;

	g_return_if_fail( OFA_IS_IMPORT_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), FALSE );

	priv = ofa_import_assistant_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );
	priv->p5_page = page;

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-bar-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p5_import = my_progress_bar_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p5_import ));

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-insert-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p5_insert = my_progress_bar_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p5_insert ));

	priv->p5_text = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-text-view" );
	g_return_if_fail( priv->p5_text && GTK_IS_TEXT_VIEW( priv->p5_text ));
	gtk_widget_set_can_focus( priv->p5_text, FALSE );

	/* search from something which would be able to import the data */
	if( st_radios[0].get_type ){
		priv->p5_object = ( ofaIImportable * ) g_object_new( st_radios[0].get_type(), NULL );
	} else {
		priv->p5_plugin = ofa_iimportable_find_willing_to( hub, priv->p1_furi, priv->p3_import_settings );
	}
	if( !priv->p5_object && !priv->p5_plugin ){
		p5_error_no_interface( self );
		return;
	}

	gtk_widget_show_all( page );

	g_idle_add(( GSourceFunc ) p5_do_import, self );
}

static void
p5_error_no_interface( const ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gchar *str;
	GtkWidget *label;
	const gchar *cstr;

	priv = ofa_import_assistant_get_instance_private( self );

	/* display an error dialog */
	if( priv->p5_object ){
		str = g_strdup_printf(
					_( "The requested type (%s) does not implement the IImportable interface" ),
					G_OBJECT_TYPE_NAME( priv->p5_object ));
	} else {
		str = g_strdup_printf( _( "Unable to find a plugin to import the specified data" ));
	}

	my_iwindow_msg_dialog( MY_IWINDOW( self ), GTK_MESSAGE_WARNING, str );

	g_free( str );

	/* prepare the assistant to terminate */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-label" );

	cstr = _( "Unfortunately, we have not been able to do anything for "
			"import the specified file.\n"
			"You should most probably open a feature request against the "
			"Openbook maintainer." );

	gtk_label_set_text( GTK_LABEL( label ), cstr );
	my_utils_widget_set_style( label, "labelerror" );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), TRUE );
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
	const gchar *style;

	priv = ofa_import_assistant_get_instance_private( self );

	count = 0;
	errors = 0;
	has_worked = TRUE;

	/* first, import */
	ffmt = ofa_file_format_get_fftype( priv->p3_import_settings );
	switch( ffmt ){
		case OFA_FFTYPE_CSV:
			count = p5_do_import_csv( self, &errors );
			break;
		case OFA_FFTYPE_OTHER:
			g_return_val_if_fail( priv->p5_plugin && OFA_IS_IIMPORTABLE( priv->p5_plugin ), FALSE );
			count = p5_do_import_other( self, &errors );
			break;
		default:
			has_worked = FALSE;
			break;
	}

	/* then display the result */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-label" );

	str = g_strdup( "" );//my_utils_str_remove_underlines( gtk_button_get_label( GTK_BUTTON( priv->p2_type_btn )));

	if( has_worked ){
		if( !errors ){
			text = g_strdup_printf( _( "OK: %u lines from '%s' have been successfully "
					"imported into « %s »." ),
					count, priv->p1_furi, str );
			style = "labelinfo";

		} else if( errors > 0 ){
			text = g_strdup_printf( _( "Unfortunately, '%s' import has encountered errors "
					"during analyse and import phase.\n"
					"The « %s » recordset has been left unchanged.\n"
					"Please fix these errors, and retry then." ), priv->p1_furi, str );
			style = "labelerror";

		} else if( errors < 0 ){
			text = g_strdup_printf( _( "Unfortunately, '%s' import has encountered errors "
					"during insertion phase.\n"
					"The « %s » recordset only contains the successfully inserted records.\n"
					"Please fix these errors, and retry then." ), priv->p1_furi, str );
			style = "labelerror";
		}
	} else {
		text = g_strdup_printf( _( "Unfortunately, the required file format is not "
				"managed at this time.\n"
				"Please fix this, and retry, then." ));
		style = "labelerror";
	}

	g_free( str );

	gtk_label_set_text( GTK_LABEL( label ), text );
	g_free( text );
	my_utils_widget_set_style( label, style );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), TRUE );

	/* do not continue and remove from idle callbacks list */
	return( FALSE );
}

static guint
p5_do_import_csv( ofaImportAssistant *self, guint *errors )
{
	ofaImportAssistantPrivate *priv;
	guint count;
	ofaHub *hub;

	priv = ofa_import_assistant_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );

	count = ofa_hub_import_csv(
					hub, priv->p5_object, priv->p1_furi, priv->p3_import_settings, self, errors );

	return( count );
}

static guint
p5_do_import_other( ofaImportAssistant *self, guint *errors )
{
	ofaImportAssistantPrivate *priv;
	guint count;
	ofaHub *hub;

	priv = ofa_import_assistant_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );

	*errors = ofa_iimportable_import_uri( priv->p5_plugin, hub, self, NULL );
	count = ofa_iimportable_get_count( priv->p5_plugin );

	return( count );
}

static void
p5_on_progress( ofaIImporter *importer, ofeImportablePhase phase, gdouble progress, const gchar *text, ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;

	g_debug( "ofa_import_assistant_p5_on_progress: progress=%lf", progress );

	priv = ofa_import_assistant_get_instance_private( self );

	if( phase == IMPORTABLE_PHASE_IMPORT ){
		g_signal_emit_by_name( priv->p5_import, "my-double", progress );
		g_signal_emit_by_name( priv->p5_import, "my-text", text );

	} else {
		g_return_if_fail( phase == IMPORTABLE_PHASE_INSERT );
		g_signal_emit_by_name( priv->p5_insert, "my-double", progress );
		g_signal_emit_by_name( priv->p5_insert, "my-text", text );
	}
}

static void
p5_on_pulse( ofaIImporter *importer, ofeImportablePhase phase, ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;

	priv = ofa_import_assistant_get_instance_private( self );

	if( phase == IMPORTABLE_PHASE_IMPORT ){
		g_signal_emit_by_name( priv->p5_import, "my-pulse" );

	} else {
		g_return_if_fail( phase == IMPORTABLE_PHASE_INSERT );
		g_signal_emit_by_name( priv->p5_insert, "my-pulse" );
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

	priv = ofa_import_assistant_get_instance_private( self );

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
