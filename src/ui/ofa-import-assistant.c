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

#include "api/ofa-hub.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-igetter.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-iimporter.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-bat.h"
#include "api/ofo-account.h"
#include "api/ofo-class.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

#include "core/ofa-stream-format-bin.h"
#include "core/ofa-stream-format-disp.h"

#include "ui/ofa-import-assistant.h"

/* Export Assistant
 *
 * pos.  type     enum     title
 * ---   -------  -------  --------------------------------------------
 *   0   Intro    INTRO    Introduction
 *   1   Content  SELECT   Select a file
 *   2   Content  TYPE     Select a datatype of import
 *   3   Content  IMPORTER Select a importer
 *   4   Content  SETTINGS Set the stream format
 *   5   Confirm  CONFIRM  Summary of the operations to be done
 *   6   Summary  DONE     After import
 */

enum {
	ASSIST_PAGE_INTRO = 0,
	ASSIST_PAGE_SELECT,
	ASSIST_PAGE_TYPE,
	ASSIST_PAGE_IMPORTER,
	ASSIST_PAGE_FORMAT,
	ASSIST_PAGE_CONFIRM,
	ASSIST_PAGE_DONE
};

/* private instance data
 */
typedef struct {
	gboolean             dispose_has_run;

	/* initialization
	 */
	ofaIGetter          *getter;
	ofaIDBMeta          *meta;

	/* p0: introduction
	 */

	/* p1: select file to be imported
	 */
	GtkFileChooser      *p1_chooser;
	gchar               *p1_folder;
	gchar               *p1_furi;			/* the utf-8 imported filename */
	gchar               *p1_content;			/* guessed content */

	/* p2: select a type of data to be imported
	 */
	GtkWidget           *p2_furi;
	GtkWidget           *p2_content;
	GList               *p2_importables;
	GtkWidget           *p2_parent;
	gint                 p2_col;
	gint                 p2_row;
	GtkWidget           *p2_selected_btn;
	gchar               *p2_selected_class;
	gchar               *p2_selected_label;
	GtkWidget           *p2_message;

	/* p3: select an importer
	 */
	GtkWidget           *p3_furi;
	GtkWidget           *p3_content;
	GtkWidget           *p3_datatype;
	GtkWidget           *p3_import_tview;
	GtkListStore        *p3_import_store;
	GList               *p3_importers;
	gchar               *p3_importer_label;
	ofaIImporter        *p3_importer_obj;
	GtkWidget           *p3_message;

	/* p4: stream format
	 */
	GtkWidget           *p4_furi;
	GtkWidget           *p4_content;
	GtkWidget           *p4_datatype;
	GtkWidget           *p4_importer;
	ofaStreamFormat     *p4_import_settings;
	ofaStreamFormatBin  *p4_settings_prefs;
	GtkWidget           *p4_message;

	/* p5: confirm
	 */
	ofaStreamFormatDisp *p5_format;

	/* p6: import the file, display the result
	 */
	myProgressBar       *p6_import;
	myProgressBar       *p6_insert;
	GtkWidget           *p6_page;
	GtkWidget           *p6_text;
	ofaIImportable      *p6_object;
	ofaIImportable      *p6_plugin;
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

/**
 * The columns stored in the page 3 importer store.
 */
enum {
	IMP_COL_LABEL = 0,
	IMP_COL_VERSION,
	IMP_COL_OBJECT,
	IMP_N_COLUMNS
};

/* data set against each of above radio buttons */
#define DATA_TYPE_INDEX                   "ofa-data-type"

static const gchar *st_import_folder    = "ofa-LastImportFolder";
static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-import-assistant.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     iwindow_read_settings( myIWindow *instance, myISettings *settings, const gchar *keyname );
static void     set_user_settings( ofaImportAssistant *self );
static void     set_dossier_settings( ofaImportAssistant *self );
static void     iassistant_iface_init( myIAssistantInterface *iface );
static gboolean iassistant_is_willing_to_quit( myIAssistant*instance, guint keyval );
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
static void     p3_on_import_selection_changed( GtkTreeSelection *select, ofaImportAssistant *self );
static void     p3_on_import_selection_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaImportAssistant *self );
static gboolean p3_check_for_complete( ofaImportAssistant *self );
static void     p3_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p4_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p4_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p4_on_settings_changed( ofaStreamFormatBin *bin, ofaImportAssistant *self );
static void     p4_check_for_complete( ofaImportAssistant *self );
static void     p4_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p5_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p5_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p6_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p6_error_no_interface( const ofaImportAssistant *self );
static gboolean p6_do_import( ofaImportAssistant *self );
static guint    p6_do_import_csv( ofaImportAssistant *self, guint *errors );
static guint    p6_do_import_other( ofaImportAssistant *self, guint *errors );
static void     p6_on_progress( ofaIImporter *importer, ofeImportablePhase phase, gdouble progress, const gchar *text, ofaImportAssistant *self );
static void     p6_on_pulse( ofaIImporter *importer, ofeImportablePhase phase, ofaImportAssistant *self );
static void     p6_on_message( ofaIImporter *importer, guint line_number, ofeImportableMsg status, const gchar *msg, ofaImportAssistant *self );

G_DEFINE_TYPE_EXTENDED( ofaImportAssistant, ofa_import_assistant, GTK_TYPE_ASSISTANT, 0,
		G_ADD_PRIVATE( ofaImportAssistant )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
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
		{ ASSIST_PAGE_TYPE,
				( myIAssistantCb ) p2_do_init,
				( myIAssistantCb ) p2_do_display,
				( myIAssistantCb ) p2_do_forward },
		{ ASSIST_PAGE_IMPORTER,
				( myIAssistantCb ) p3_do_init,
				( myIAssistantCb ) p3_do_display,
				( myIAssistantCb ) p3_do_forward },
		{ ASSIST_PAGE_FORMAT,
				( myIAssistantCb ) p4_do_init,
				( myIAssistantCb ) p4_do_display,
				( myIAssistantCb ) p4_do_forward },
		{ ASSIST_PAGE_CONFIRM,
				( myIAssistantCb ) p5_do_init,
				( myIAssistantCb ) p5_do_display,
				NULL },
		{ ASSIST_PAGE_DONE,
				NULL,
				( myIAssistantCb ) p6_do_display,
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
	g_free( priv->p3_importer_label );

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
		g_list_free_full( priv->p3_importers, ( GDestroyNotify ) g_object_unref );
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
	ofaImportAssistantPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_IMPORT_ASSISTANT( self ));

	priv = ofa_import_assistant_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->p3_importers = NULL;

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
	if( 0 ){
	g_signal_connect( instance, "pulse", G_CALLBACK( p6_on_pulse ), instance );
	g_signal_connect( instance, "progress", G_CALLBACK( p6_on_progress ), instance );
	g_signal_connect( instance, "message", G_CALLBACK( p6_on_message ), instance );
	}
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
set_user_settings( ofaImportAssistant *self )
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
}

static void
set_dossier_settings( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;

	priv = ofa_import_assistant_get_instance_private( self );

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

	set_dossier_settings( self );
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

	/* previously set */
	priv->p2_furi = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-furi" );
	g_return_if_fail( priv->p2_furi && GTK_IS_LABEL( priv->p2_furi ));
	my_utils_widget_set_style( priv->p2_furi, "labelinfo" );

	priv->p2_content = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-content" );
	g_return_if_fail( priv->p2_content && GTK_IS_LABEL( priv->p2_content ));
	my_utils_widget_set_style( priv->p2_content, "labelinfo" );

	/* expected data */
	hub = ofa_igetter_get_hub( priv->getter );
	priv->p2_importables = ofa_hub_get_for_type( hub, OFA_TYPE_IIMPORTABLE );
	g_debug( "%s: importables count=%d", thisfn, g_list_length( priv->p2_importables ));
	priv->p2_selected_btn = NULL;
	priv->p2_col = 1;
	priv->p2_row = 0;
	first = NULL;

	priv->p2_parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-datatype-parent" );
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
			gtk_grid_attach( GTK_GRID( priv->p2_parent ), btn, priv->p2_col, priv->p2_row++, 1, 1 );
		}
		g_free( label );
	}

	/* error message */
	priv->p2_message = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-message" );
	g_return_if_fail( priv->p2_message && GTK_IS_LABEL( priv->p2_message ));
	my_utils_widget_set_style( priv->p2_message, "labelerror" );
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

	/* previously set */
	gtk_label_set_text( GTK_LABEL( priv->p2_furi ), priv->p1_furi );
	gtk_label_set_text( GTK_LABEL( priv->p2_content ), priv->p1_content );

	/* importables */
	if( my_strlen( priv->p2_selected_class )){
		for( i=0 ; i<priv->p2_row ; ++i ){
			btn = gtk_grid_get_child_at( GTK_GRID( priv->p2_parent ), priv->p2_col, i );
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
	gboolean complete;
	gint i;
	GtkWidget *btn;
	ofaIImportable *object;
	const gchar *label;

	priv = ofa_import_assistant_get_instance_private( self );

	complete = TRUE;
	gtk_label_set_text( GTK_LABEL( priv->p2_message ), "" );

	/* do we have a currently active button ? */
	priv->p2_selected_btn = NULL;
	g_free( priv->p2_selected_class );
	priv->p2_selected_class = NULL;
	g_free( priv->p2_selected_label );
	priv->p2_selected_label = NULL;

	if( complete ){
		for( i=0 ; i<priv->p2_row ; ++i ){
			btn = gtk_grid_get_child_at( GTK_GRID( priv->p2_parent ), priv->p2_col, i );
			if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( btn ))){
				object = OFA_IIMPORTABLE( g_object_get_data( G_OBJECT( btn ), DATA_TYPE_INDEX ));
				priv->p2_selected_btn = btn;
				priv->p2_selected_class = g_strdup( G_OBJECT_TYPE_NAME( object ));
				label = gtk_button_get_label( GTK_BUTTON( priv->p2_selected_btn ));
				priv->p2_selected_label = my_utils_str_remove_underlines( label );
				break;
			}
		}
		if( !priv->p2_selected_btn ){
			complete = FALSE;
			gtk_label_set_text( GTK_LABEL( priv->p2_message ), _( "No selected data type" ));
		}
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), complete );
}

static void
p2_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	set_user_settings( self );
}

/*
 * p3: select the importer
 */
static void
p3_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p3_do_init";
	ofaImportAssistantPrivate *priv;
	GtkWidget *label_widget;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	GtkTreeSelection *select;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	/* previously set */
	priv->p3_furi = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-furi" );
	g_return_if_fail( priv->p3_furi && GTK_IS_LABEL( priv->p3_furi ));
	my_utils_widget_set_style( priv->p3_furi, "labelinfo" );

	priv->p3_content = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-content" );
	g_return_if_fail( priv->p3_content && GTK_IS_LABEL( priv->p3_content ));
	my_utils_widget_set_style( priv->p3_content, "labelinfo" );

	priv->p3_datatype = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-datatype" );
	g_return_if_fail( priv->p3_datatype && GTK_IS_LABEL( priv->p3_datatype ));
	my_utils_widget_set_style( priv->p3_datatype, "labelinfo" );

	/* available importers */
	priv->p3_import_tview = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-treeview" );
	g_return_if_fail( priv->p3_import_tview && GTK_IS_TREE_VIEW( priv->p3_import_tview ));

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
					"Label", cell, "text", IMP_COL_LABEL, NULL );
	gtk_tree_view_column_set_alignment( column, 0 );
	gtk_tree_view_append_column( GTK_TREE_VIEW( priv->p3_import_tview ), column );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
					"Version", cell, "text", IMP_COL_VERSION, NULL );
	gtk_tree_view_column_set_alignment( column, 0 );
	gtk_tree_view_append_column( GTK_TREE_VIEW( priv->p3_import_tview ), column );

	priv->p3_import_store = gtk_list_store_new( IMP_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT );
	gtk_tree_view_set_model( GTK_TREE_VIEW( priv->p3_import_tview ), GTK_TREE_MODEL( priv->p3_import_store ));
	g_object_unref( priv->p3_import_store );

	label_widget = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "label32" );
	g_return_if_fail( label_widget && GTK_IS_LABEL( label_widget ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label_widget ), priv->p3_import_tview );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->p3_import_tview ));
	g_signal_connect( select, "changed", G_CALLBACK( p3_on_import_selection_changed ), self );
	g_signal_connect( priv->p3_import_tview, "row-activated", G_CALLBACK( p3_on_import_selection_activated ), self );

	/* error message */
	priv->p3_message = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-message" );
	g_return_if_fail( priv->p3_message && GTK_IS_LABEL( priv->p3_message ));
	my_utils_widget_set_style( priv->p3_message, "labelerror" );
}

static void
p3_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p3_do_display";
	ofaImportAssistantPrivate *priv;
	ofaHub *hub;
	GList *importers, *it;
	gchar *label, *version;
	GtkTreeIter iter;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	/* previously set */
	gtk_label_set_text( GTK_LABEL( priv->p3_furi ), priv->p1_furi );
	gtk_label_set_text( GTK_LABEL( priv->p3_content ), priv->p1_content );
	gtk_label_set_text( GTK_LABEL( priv->p3_datatype ), priv->p2_selected_label );

	/* importers */
	if( priv->p3_importers ){
		g_list_free_full( priv->p3_importers, ( GDestroyNotify ) g_object_unref );
		priv->p3_importers = NULL;
		gtk_list_store_clear( priv->p3_import_store );
	}

	hub = ofa_igetter_get_hub( priv->getter );
	importers = ofa_hub_get_for_type( hub, OFA_TYPE_IIMPORTER );
	for( it=importers ; it ; it=it->next ){
		if( ofa_iimporter_get_accept_content( OFA_IIMPORTER( it->data ), priv->p1_content )){
			priv->p3_importers = g_list_prepend( priv->p3_importers, g_object_ref( it->data ));
			label = ofa_iimporter_get_display_name( OFA_IIMPORTER( it->data ));
			version = ofa_iimporter_get_version( OFA_IIMPORTER( it->data ));
			if( my_strlen( label )){
				gtk_list_store_insert_with_values( priv->p3_import_store, &iter, -1,
						IMP_COL_LABEL,   label,
						IMP_COL_VERSION, version ? version : "",
						IMP_COL_OBJECT,  it->data,
						-1 );
			}
			g_free( version );
			g_free( label );
		}
	}
	g_list_free_full( importers, ( GDestroyNotify ) g_object_unref );

	p3_check_for_complete( self );
}

static void
p3_on_import_selection_changed( GtkTreeSelection *select, ofaImportAssistant *self )
{
	p3_check_for_complete( self );
}

static void
p3_on_import_selection_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaImportAssistant *self )
{
	if( p3_check_for_complete( self )){
		gtk_assistant_next_page( GTK_ASSISTANT( self ));
	}
}

static gboolean
p3_check_for_complete( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gboolean complete;
	GtkTreeSelection *select;

	priv = ofa_import_assistant_get_instance_private( self );

	complete = TRUE;
	gtk_label_set_text( GTK_LABEL( priv->p3_message ), "" );

	/* do we have a selected importer */
	if( complete ){
		if( !g_list_length( priv->p3_importers )){
			complete = FALSE;
			gtk_label_set_text( GTK_LABEL( priv->p3_message ), _( "No available importer" ));
		}
	}
	if( complete ){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->p3_import_tview ));
		if( !gtk_tree_selection_get_selected( select, NULL, NULL )){
			complete = FALSE;
			gtk_label_set_text( GTK_LABEL( priv->p3_message ), _( "No selected importer" ));
		}
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), complete );

	return( complete );
}

static void
p3_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	ofaImportAssistantPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	priv = ofa_import_assistant_get_instance_private( self );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->p3_import_tview ));
	gtk_tree_selection_get_selected( select, &tmodel, &iter );
	gtk_tree_model_get( tmodel, &iter, IMP_COL_LABEL, &priv->p3_importer_label, IMP_COL_OBJECT, &priv->p3_importer_obj, -1 );
	g_return_if_fail( priv->p3_importer_obj && OFA_IS_IIMPORTER( priv->p3_importer_obj ));
	g_object_unref( priv->p3_importer_obj );
}

/*
 * p4: stream format
 *
 * These are initialized with the import settings for this name, or
 * with default settings
 */
static void
p4_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p4_do_init";
	ofaImportAssistantPrivate *priv;
	GtkWidget *parent, *label, *combo;
	GtkSizeGroup *hgroup;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	/* previously set */
	priv->p4_furi = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-furi" );
	g_return_if_fail( priv->p4_furi && GTK_IS_LABEL( priv->p4_furi ));
	my_utils_widget_set_style( priv->p4_furi, "labelinfo" );

	priv->p4_content = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-content" );
	g_return_if_fail( priv->p4_content && GTK_IS_LABEL( priv->p4_content ));
	my_utils_widget_set_style( priv->p4_content, "labelinfo" );

	priv->p4_datatype = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-datatype" );
	g_return_if_fail( priv->p4_datatype && GTK_IS_LABEL( priv->p4_datatype ));
	my_utils_widget_set_style( priv->p4_datatype, "labelinfo" );

	priv->p4_importer = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-importer" );
	g_return_if_fail( priv->p4_importer && GTK_IS_LABEL( priv->p4_importer ));
	my_utils_widget_set_style( priv->p4_importer, "labelinfo" );

	hgroup = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-furi-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-content-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-data-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-importer-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	/* stream format */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-settings-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->p4_settings_prefs = ofa_stream_format_bin_new( NULL );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p4_settings_prefs ));
	my_utils_size_group_add_size_group(
			hgroup, ofa_stream_format_bin_get_size_group( priv->p4_settings_prefs, 0 ));

	combo = ofa_stream_format_bin_get_mode_combo( priv->p4_settings_prefs );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));
	gtk_widget_set_sensitive( combo, FALSE );

	g_signal_connect(
			priv->p4_settings_prefs, "ofa-changed", G_CALLBACK( p4_on_settings_changed ), self );

	priv->p4_message = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-message" );
	g_return_if_fail( priv->p4_message && GTK_IS_LABEL( priv->p4_message ));
	my_utils_widget_set_style( priv->p4_message, "labelerror" );

	g_object_unref( hgroup );
}

static void
p4_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p4_do_init";
	ofaImportAssistantPrivate *priv;
	const gchar *found_key;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	/* previously set */
	gtk_label_set_text( GTK_LABEL( priv->p4_furi ), priv->p1_furi );
	gtk_label_set_text( GTK_LABEL( priv->p4_content ), priv->p1_content );
	gtk_label_set_text( GTK_LABEL( priv->p4_datatype ), priv->p2_selected_label );
	gtk_label_set_text( GTK_LABEL( priv->p4_importer ), priv->p3_importer_label );

	/* stream format */
	found_key = NULL;
	if( ofa_stream_format_exists( priv->p2_selected_class, OFA_SFMODE_IMPORT )){
		found_key = priv->p2_selected_class;
	}

	g_clear_object( &priv->p4_import_settings );
	priv->p4_import_settings = ofa_stream_format_new( found_key, OFA_SFMODE_IMPORT );
	ofa_stream_format_bin_set_format( priv->p4_settings_prefs, priv->p4_import_settings );

	p4_check_for_complete( self );
}

static void
p4_on_settings_changed( ofaStreamFormatBin *bin, ofaImportAssistant *self )
{
	p4_check_for_complete( self );
}

static void
p4_check_for_complete( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gboolean ok;
	gchar *message;

	priv = ofa_import_assistant_get_instance_private( self );

	ok = ofa_stream_format_bin_is_valid( priv->p4_settings_prefs, &message );

	gtk_label_set_text( GTK_LABEL( priv->p4_message ), my_strlen( message ) ? message : "" );
	g_free( message );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );
}

static void
p4_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p4_do_forward";
	ofaImportAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	ofa_stream_format_bin_apply( priv->p4_settings_prefs );
}

/*
 * ask the user to confirm the operation
 */
static void
p5_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p5_do_init";
	ofaImportAssistantPrivate *priv;
	GtkWidget *label, *parent;
	GtkSizeGroup *group;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-source-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( group, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-target-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( group, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-importer-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( group, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-stream-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( group, label );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-stream-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->p5_format = ofa_stream_format_disp_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p5_format ));
	my_utils_size_group_add_size_group(
			group, ofa_stream_format_disp_get_size_group( priv->p5_format, 0 ));

	g_object_unref( group );
}

static void
p5_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p5_do_display";
	ofaImportAssistantPrivate *priv;
	GtkWidget *label;
	gboolean complete;
	gchar *str, *str2;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-furi" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), priv->p1_furi );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-content" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), priv->p1_content );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-type" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), priv->p2_selected_label );

	str = ofa_iimporter_get_version( priv->p3_importer_obj );
	str2 = g_strdup_printf( "%s %s", priv->p3_importer_label, str );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-importer" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ),  str2 );
	g_free( str2 );
	g_free( str );

	ofa_stream_format_disp_set_format( priv->p5_format, priv->p4_import_settings );

	complete = my_strlen( priv->p1_furi ) > 0;
	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), complete );
}

/*
 * import
 * execution summary
 */
static void
p6_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p6_do_display";
	ofaImportAssistantPrivate *priv;
	GtkWidget *parent;
	ofaHub *hub;

	g_return_if_fail( OFA_IS_IMPORT_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), FALSE );

	priv = ofa_import_assistant_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );
	priv->p6_page = page;

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

	/* search from something which would be able to import the data */
	if( st_radios[0].get_type ){
		priv->p6_object = ( ofaIImportable * ) g_object_new( st_radios[0].get_type(), NULL );
	} else {
		priv->p6_plugin = ofa_iimportable_find_willing_to( hub, priv->p1_furi, priv->p4_import_settings );
	}
	if( !priv->p6_object && !priv->p6_plugin ){
		p6_error_no_interface( self );
		return;
	}

	gtk_widget_show_all( page );

	g_idle_add(( GSourceFunc ) p6_do_import, self );
}

static void
p6_error_no_interface( const ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gchar *str;
	GtkWidget *label;
	const gchar *cstr;

	priv = ofa_import_assistant_get_instance_private( self );

	/* display an error dialog */
	if( priv->p6_object ){
		str = g_strdup_printf(
					_( "The requested type (%s) does not implement the IImportable interface" ),
					G_OBJECT_TYPE_NAME( priv->p6_object ));
	} else {
		str = g_strdup_printf( _( "Unable to find a plugin to import the specified data" ));
	}

	my_iwindow_msg_dialog( MY_IWINDOW( self ), GTK_MESSAGE_WARNING, str );

	g_free( str );

	/* prepare the assistant to terminate */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p6-label" );

	cstr = _( "Unfortunately, we have not been able to do anything for "
			"import the specified file.\n"
			"You should most probably open a feature request against the "
			"Openbook maintainer." );

	gtk_label_set_text( GTK_LABEL( label ), cstr );
	my_utils_widget_set_style( label, "labelerror" );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), TRUE );
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
	const gchar *style;

	priv = ofa_import_assistant_get_instance_private( self );

	count = 0;
	errors = 0;
	has_worked = TRUE;

	/* first, import */
	if( 0 ){
	//ffmt = ofa_stream_format_get_fftype( priv->p4_import_settings );
	switch( ffmt ){
		case 1:
			count = p6_do_import_csv( self, &errors );
			break;
		case 2:
			g_return_val_if_fail( priv->p6_plugin && OFA_IS_IIMPORTABLE( priv->p6_plugin ), FALSE );
			count = p6_do_import_other( self, &errors );
			break;
		default:
			has_worked = FALSE;
			break;
	}
	}

	/* then display the result */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p6-label" );

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
p6_do_import_csv( ofaImportAssistant *self, guint *errors )
{
	ofaImportAssistantPrivate *priv;
	guint count;
	ofaHub *hub;

	priv = ofa_import_assistant_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );

	count = ofa_hub_import_csv(
					hub, priv->p6_object, priv->p1_furi, priv->p4_import_settings, self, errors );

	return( count );
}

static guint
p6_do_import_other( ofaImportAssistant *self, guint *errors )
{
	ofaImportAssistantPrivate *priv;
	guint count;
	ofaHub *hub;

	priv = ofa_import_assistant_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );

	*errors = ofa_iimportable_import_uri( priv->p6_plugin, hub, self, NULL );
	count = ofa_iimportable_get_count( priv->p6_plugin );

	return( count );
}

static void
p6_on_progress( ofaIImporter *importer, ofeImportablePhase phase, gdouble progress, const gchar *text, ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;

	g_debug( "ofa_import_assistant_p6_on_progress: progress=%lf", progress );

	priv = ofa_import_assistant_get_instance_private( self );

	if( phase == IMPORTABLE_PHASE_IMPORT ){
		g_signal_emit_by_name( priv->p6_import, "my-double", progress );
		g_signal_emit_by_name( priv->p6_import, "my-text", text );

	} else {
		g_return_if_fail( phase == IMPORTABLE_PHASE_INSERT );
		g_signal_emit_by_name( priv->p6_insert, "my-double", progress );
		g_signal_emit_by_name( priv->p6_insert, "my-text", text );
	}
}

static void
p6_on_pulse( ofaIImporter *importer, ofeImportablePhase phase, ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;

	priv = ofa_import_assistant_get_instance_private( self );

	if( phase == IMPORTABLE_PHASE_IMPORT ){
		g_signal_emit_by_name( priv->p6_import, "my-pulse" );

	} else {
		g_return_if_fail( phase == IMPORTABLE_PHASE_INSERT );
		g_signal_emit_by_name( priv->p6_insert, "my-pulse" );
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

	priv = ofa_import_assistant_get_instance_private( self );

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
