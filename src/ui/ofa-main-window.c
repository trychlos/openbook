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
#include <stdlib.h>

#include "api/my-utils.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "ui/my-tab-label.h"
#include "ui/ofa-accounts-page.h"
#include "ui/ofa-application.h"
#include "ui/ofa-backup.h"
#include "ui/ofa-bats-page.h"
#include "ui/ofa-classes-page.h"
#include "ui/ofa-currencies-page.h"
#include "ui/ofa-dossier-login.h"
#include "ui/ofa-dossier-properties.h"
#include "ui/ofa-export.h"
#include "ui/ofa-guided-ex.h"
#include "ui/ofa-guided-input.h"
#include "ui/ofa-import.h"
#include "ui/ofa-int-closing.h"
#include "ui/ofa-ledgers-page.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-ope-templates-page.h"
#include "ui/ofa-page.h"
#include "ui/ofa-print-reconcil.h"
#include "ui/ofa-rates-page.h"
#include "ui/ofa-reconciliation.h"
#include "ui/ofa-view-entries.h"

static gboolean pref_confirm_on_altf4 = FALSE;

/* private instance data
 */
struct _ofaMainWindowPrivate {
	gboolean       dispose_has_run;

	/* properties
	 */

	/* internals
	 */
	gchar         *orig_title;
	GtkGrid       *grid;
	GtkMenuBar    *menubar;
	GtkAccelGroup *accel_group;
	GMenuModel    *menu;				/* the menu model when a dossier is opened */
	GtkPaned      *pane;
	ofoDossier    *dossier;
};

/* signals defined here
 */
enum {
	OPEN_DOSSIER,
	UPDATE_PROPERTIES,
	N_SIGNALS
};

static void on_properties       ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_backup           ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_close            ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_guided       ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_view_entries ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_concil       ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_int_closing  ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_import       ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_export       ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_print_reconcil   ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_accounts     ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_ledgers      ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_ope_templates( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_currencies   ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_rates        ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_classes      ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_batfiles     ( GSimpleAction *action, GVariant *parameter, gpointer user_data );

static const GActionEntry st_dos_entries[] = {
		{ "properties",    on_properties,        NULL, NULL, NULL },
		{ "backup",        on_backup,            NULL, NULL, NULL },
		{ "close",         on_close,             NULL, NULL, NULL },
		{ "guided",        on_ope_guided,        NULL, NULL, NULL },
		{ "entries",       on_ope_view_entries,  NULL, NULL, NULL },
		{ "concil",        on_ope_concil,        NULL, NULL, NULL },
		{ "iclosing",      on_ope_int_closing,   NULL, NULL, NULL },
		{ "import",        on_ope_import,        NULL, NULL, NULL },
		{ "export",        on_ope_export,        NULL, NULL, NULL },
		{ "prt-reconcil",  on_print_reconcil,    NULL, NULL, NULL },
		{ "accounts",      on_ref_accounts,      NULL, NULL, NULL },
		{ "ledgers",       on_ref_ledgers,       NULL, NULL, NULL },
		{ "ope-templates", on_ref_ope_templates, NULL, NULL, NULL },
		{ "currencies",    on_ref_currencies,    NULL, NULL, NULL },
		{ "rates",         on_ref_rates,         NULL, NULL, NULL },
		{ "classes",       on_ref_classes,       NULL, NULL, NULL },
		{ "batfiles",      on_ref_batfiles,      NULL, NULL, NULL },
};

/* This structure handles the functions which manage the pages of the
 * main notebook:
 * - the label which is displayed in the tab of the page of the main book
 * - the GObject get_type() function of the ofaPage-derived class
 *   which handles the page
 *
 * There must be here one theme per type of main notebook's page.
 *
 * Each main notebook's page may be reached either from a menubar action,
 * or from an activation of an item in the left treeview (though several
 * menubar actions and/or several items in the left treeview may lead
 * to a same theme, or do not appear at all)
 */
typedef struct {
	gint         theme_id;
	const gchar *label;
	GType      (*fn_get_type)( void );
	gboolean     has_import;
	gboolean     has_export;
}
	sThemeDef;

static sThemeDef st_theme_defs[] = {

		{ THM_ACCOUNTS,
				N_( "Chart of accounts" ),
				ofa_accounts_page_get_type,
				FALSE,
				FALSE },

		{ THM_BATFILES,
				N_( "Imported BAT files" ),
				ofa_bats_page_get_type,
				FALSE,
				FALSE },

		{ THM_CLASSES,
				N_( "Account classes" ),
				ofa_classes_page_get_type,
				FALSE,
				FALSE },

		{ THM_CURRENCIES,
				N_( "Currencies" ),
				ofa_currencies_page_get_type,
				FALSE,
				FALSE },

		{ THM_GUIDED_INPUT,
				N_( "Guided input" ),
				ofa_guided_ex_get_type,
				FALSE,
				FALSE },

		{ THM_LEDGERS,
				N_( "Ledgers" ),
				ofa_ledgers_page_get_type,
				FALSE,
				FALSE },

		{ THM_OPE_TEMPLATES,
				N_( "Operation templates" ),
				ofa_ope_templates_page_get_type,
				FALSE,
				FALSE },

		{ THM_RATES,
				N_( "Rates" ),
				ofa_rates_page_get_type,
				FALSE,
				FALSE },

		{ THM_RECONCIL,
				N_( "Reconciliation" ),
				ofa_reconciliation_get_type,
				FALSE,
				FALSE },

		{ THM_VIEW_ENTRIES,
				N_( "View entries" ),
				ofa_view_entries_get_type,
				FALSE,
				FALSE },

		{ 0 }
};

/* Left treeview definition.
 * For ergonomy reason, we may have here several items which points
 * to the same theme
 */
typedef struct {
	const gchar *label;
	gint         theme_id;
}
	sTreeDef;

enum {
	COL_TREE_IDX = 0,					/* index of the sTreeDef definition in the array */
	COL_LABEL,							/* tree label */
	N_COLUMNS
};

static sTreeDef st_tree_defs[] = {

		{ N_( "Guided input" ),        THM_GUIDED_INPUT },
		{ N_( "Reconciliation" ),      THM_RECONCIL },
		{ N_( "Chart of accounts" ),   THM_ACCOUNTS },
		{ N_( "Ledgers" ),             THM_LEDGERS },
		{ N_( "Operation templates" ), THM_OPE_TEMPLATES },
		{ N_( "Currencies" ),          THM_CURRENCIES },
		{ N_( "Rates" ),               THM_RATES },
		{ N_( "Account classes" ),     THM_CLASSES },
		{ N_( "Imported BAT files" ),  THM_BATFILES },
		{ 0 }
};

/* A pointer to the handling ofaPage object is set against each page
 * ( the GtkGrid) of the main notebook
 */
#define OFA_DATA_HANDLER                  "ofa-data-handler"

static const gchar *st_main_window_name = "MainWindow";
static const gchar *st_dosmenu_xml      = PKGUIDIR "/ofa-dos-menubar.ui";
static const gchar *st_dosmenu_id       = "dos-menu";

static gint         st_signals[ N_SIGNALS ] = { 0 };

G_DEFINE_TYPE( ofaMainWindow, ofa_main_window, GTK_TYPE_APPLICATION_WINDOW )

static void             pane_save_position( GtkPaned *pane );
static gboolean         on_delete_event( GtkWidget *toplevel, GdkEvent *event, gpointer user_data );
static void             set_menubar( ofaMainWindow *window, GMenuModel *model );
static void             extract_accels_rec( ofaMainWindow *window, GMenuModel *model, GtkAccelGroup *accel_group );
static void             on_open_dossier( ofaMainWindow *window, ofsDossierOpen *sdo, gpointer user_data );
static void             on_update_properties( ofaMainWindow *window, gpointer user_data );
static gboolean         check_for_account( ofaMainWindow *main_window, ofsDossierOpen *sdo );
static void             pane_restore_position( GtkPaned *pane );
static void             add_treeview_to_pane_left( ofaMainWindow *window );
static void             on_theme_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainWindow *window );
static const sThemeDef *get_theme_def_from_id( gint theme_id );
static void             add_empty_notebook_to_pane_right( ofaMainWindow *window );
static void             on_open_dossier_cleanup_handler( ofaMainWindow *window, ofsDossierOpen *sdo );
static void             on_update_properties_cleanup_handler( ofaMainWindow *window );
static void             do_close_dossier( ofaMainWindow *self );
static GtkNotebook     *main_get_book( const ofaMainWindow *window );
static GtkWidget       *main_book_get_page( const ofaMainWindow *window, GtkNotebook *book, gint theme );
static GtkWidget       *main_book_create_page( ofaMainWindow *main, GtkNotebook *book, const sThemeDef *theme_def );
static void             main_book_activate_page( const ofaMainWindow *window, GtkNotebook *book, GtkWidget *page );
static void             on_tab_close_clicked( myTabLabel *tab, GtkGrid *grid );

static void
main_window_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_main_instance_finalize";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MAIN_WINDOW( instance ));

	/* free data members here */
	priv = OFA_MAIN_WINDOW( instance )->priv;

	g_free( priv->orig_title );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_main_window_parent_class )->finalize( instance );
}

static void
main_window_dispose( GObject *instance )
{
	ofaMainWindowPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MAIN_WINDOW( instance ));

	priv = OFA_MAIN_WINDOW( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;
		my_utils_window_save_position( GTK_WINDOW( instance ), st_main_window_name );
		if( priv->pane ){
			pane_save_position( priv->pane );
		}

		/* unref object members here */

		if( priv->menu ){
			g_object_unref( priv->menu );
		}

		if( priv->dossier ){
			g_object_unref( priv->dossier );
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_main_window_parent_class )->dispose( instance );
}

static void
pane_save_position( GtkPaned *pane )
{
	gchar *key;

	key = g_strdup_printf( "%s-pane", st_main_window_name );
	ofa_settings_set_uint( key, gtk_paned_get_position( pane ));
	g_free( key );
}

static void
main_window_constructed( GObject *instance )
{
	static const gchar *thisfn = "ofa_main_instance_constructed";
	ofaMainWindowPrivate *priv;
	GError *error;
	GtkBuilder *builder;
	GMenuModel *menu;

	g_return_if_fail( instance && OFA_IS_MAIN_WINDOW( instance ));

	priv = OFA_MAIN_WINDOW( instance )->priv;

	if( !priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( ofa_main_window_parent_class )->constructed ){
			G_OBJECT_CLASS( ofa_main_window_parent_class )->constructed( instance );
		}

		/* define the main instance actions
		 */
		g_action_map_add_action_entries(
				G_ACTION_MAP( instance ),
		        st_dos_entries, G_N_ELEMENTS( st_dos_entries ),
		        ( gpointer ) instance );

		/* define a traditional menubar
		 * the program will abort if GtkBuilder is not able to be parsed
		 * from the given file
		 */
		error = NULL;
		builder = gtk_builder_new();
		if( gtk_builder_add_from_file( builder, st_dosmenu_xml, &error )){
			menu = G_MENU_MODEL( gtk_builder_get_object( builder, st_dosmenu_id ));
			if( menu ){
				priv->menu = menu;
			} else {
				g_warning( "%s: unable to find '%s' object in '%s' file", thisfn, st_dosmenu_id, st_dosmenu_xml );
			}
		} else {
			g_warning( "%s: %s", thisfn, error->message );
			g_error_free( error );
		}
		g_object_unref( builder );

		/* build the main instance
		 * it consists of a grid of one column:
		 *  +--------------------------------------------------------------------+
		 *  | menubar                                                            |
		 *  +--------------------------------------------------------------------+
		 *  |                                                                    |
		 *  | an empty cell if no dossier is opened                              |
		 *  |                                                                    |
		 *  | or a GtkPane which is created when a dossier is opened             |
		 *  |                                                                    |
		 *  +--------------------------------------------------------------------+
		 */
		priv->grid = GTK_GRID( gtk_grid_new());
		/*gtk_widget_set_hexpand( GTK_WIDGET( priv->grid ), TRUE );*/
		/*gtk_widget_set_vexpand( GTK_WIDGET( priv->grid ), TRUE );*/
		/*gtk_container_set_resize_mode( GTK_CONTAINER( priv->grid ), GTK_RESIZE_QUEUE );*/
		gtk_grid_set_row_homogeneous( priv->grid, FALSE );
		gtk_container_add( GTK_CONTAINER( instance ), GTK_WIDGET( priv->grid ));

		my_utils_window_restore_position( GTK_WINDOW( instance), st_main_window_name );

		/* connect some signals
		 */
		g_signal_connect( instance, "delete-event", G_CALLBACK( on_delete_event ), NULL );

		g_signal_connect( instance, OFA_SIGNAL_OPEN_DOSSIER, G_CALLBACK( on_open_dossier ), NULL );
		g_signal_connect( instance, OFA_SIGNAL_UPDATE_PROPERTIES, G_CALLBACK( on_update_properties ), NULL );
	}
}

static void
ofa_main_window_init( ofaMainWindow *self )
{
	static const gchar *thisfn = "ofa_main_window_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MAIN_WINDOW( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_MAIN_WINDOW, ofaMainWindowPrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
ofa_main_window_class_init( ofaMainWindowClass *klass )
{
	static const gchar *thisfn = "ofa_main_window_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->constructed = main_window_constructed;
	G_OBJECT_CLASS( klass )->dispose = main_window_dispose;
	G_OBJECT_CLASS( klass )->finalize = main_window_finalize;

	g_type_class_add_private( klass, sizeof( ofaMainWindowPrivate ));

	/*
	 * ofaMainWindow::ofa-signal-open-dossier:
	 *
	 * This signal is to be sent to the main window when someone asks
	 * for opening a dossier.
	 *
	 * Arguments are the name of the dossier, along with the connection
	 * parameters. The connection itself is supposed to have already
	 * been validated.
	 *
	 * They are passed in a ofaOpenDossier structure, that the cleanup handler
	 * takes care of freeing.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaMainWindow *window,
	 * 						ofsDossierOpen *sdo,
	 * 						gpointer user_data );
	 */
	st_signals[ OPEN_DOSSIER ] = g_signal_new_class_handler(
				OFA_SIGNAL_OPEN_DOSSIER,
				OFA_TYPE_MAIN_WINDOW,
				G_SIGNAL_RUN_CLEANUP | G_SIGNAL_ACTION,
				G_CALLBACK( on_open_dossier_cleanup_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );

	/*
	 * ofaMainWindow::ofa-signal-update-properties:
	 *
	 * This signal is to be sent to the main window for updating the
	 * dossier properties (as an alternative to the menu item).
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaMainWindow *window,
	 * 						gpointer user_data );
	 */
	st_signals[ UPDATE_PROPERTIES ] = g_signal_new_class_handler(
				OFA_SIGNAL_UPDATE_PROPERTIES,
				OFA_TYPE_MAIN_WINDOW,
				G_SIGNAL_RUN_CLEANUP | G_SIGNAL_ACTION,
				G_CALLBACK( on_update_properties_cleanup_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0 );
}

/**
 * Returns a newly allocated ofaMainWindow object.
 */
ofaMainWindow *
ofa_main_window_new( const ofaApplication *application )
{
	static const gchar *thisfn = "ofa_main_window_new";
	ofaMainWindow *window;

	g_return_val_if_fail( OFA_IS_APPLICATION( application ), NULL );

	g_debug( "%s: application=%p", thisfn, application );

	/* 'application' is a GtkWindow property */
	window = g_object_new( OFA_TYPE_MAIN_WINDOW,
					"application", application,
					NULL );

	g_object_get( G_OBJECT( application ),
			OFA_PROP_APPLICATION_NAME, &window->priv->orig_title,
			NULL );

	set_menubar( window, ofa_application_get_menu_model( application ));

	return( window );
}

/*
 * triggered when the user clicks on the top right [X] button
 * returns %TRUE to stop the signal to be propagated (which would cause
 * the window to be destroyed); instead we gracefully quit the application.
 * Or, in other terms:
 * If you return FALSE in the "delete_event" signal handler, GTK will
 * emit the "destroy" signal. Returning TRUE means you don't want the
 * window to be destroyed.
 */
static gboolean
on_delete_event( GtkWidget *toplevel, GdkEvent *event, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_delete_event";
	gboolean ok_to_quit;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( toplevel ), FALSE );

	g_debug( "%s: toplevel=%p (%s), event=%p, user_data=%p",
			thisfn,
			( void * ) toplevel, G_OBJECT_TYPE_NAME( toplevel ),
			( void * ) event,
			( void * ) user_data );

	ok_to_quit = !pref_confirm_on_altf4 || ofa_main_window_is_willing_to_quit( OFA_MAIN_WINDOW( toplevel ));

	return( !ok_to_quit );
}

gboolean
ofa_main_window_is_willing_to_quit( ofaMainWindow *window )
{
	GtkWidget *dialog;
	gint response;

	dialog = gtk_message_dialog_new(
			GTK_WINDOW( window ),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			_( "Are you sure you want to quit the application ?" ));

	gtk_dialog_add_buttons( GTK_DIALOG( dialog ),
			_( "_Cancel" ), GTK_RESPONSE_CANCEL,
			_( "_Quit" ), GTK_RESPONSE_OK,
			NULL );

	response = gtk_dialog_run( GTK_DIALOG( dialog ));

	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
}

static void
set_menubar( ofaMainWindow *window, GMenuModel *model )
{
	GtkWidget *menubar;

	if( window->priv->menubar ){
		gtk_widget_destroy( GTK_WIDGET( window->priv->menubar ));
		window->priv->menubar = NULL;
	}

	if( window->priv->accel_group ){
		gtk_window_remove_accel_group( GTK_WINDOW( window ), window->priv->accel_group );
		g_object_unref( window->priv->accel_group );
		window->priv->accel_group = NULL;
	}

	/*accels = gtk_accel_groups_from_object( G_OBJECT( model ));
	 *accels = gtk_accel_groups_from_object( G_OBJECT( menubar ));
	 * return nil */

	window->priv->accel_group = gtk_accel_group_new();
	extract_accels_rec( window, model, window->priv->accel_group );
	gtk_window_add_accel_group( GTK_WINDOW( window ), window->priv->accel_group );

	menubar = gtk_menu_bar_new_from_model( model );
	gtk_grid_attach( window->priv->grid, menubar, 0, 0, 1, 1 );
	window->priv->menubar = GTK_MENU_BAR( menubar );
	gtk_widget_show_all( GTK_WIDGET( window ));
}

static void
extract_accels_rec( ofaMainWindow *window, GMenuModel *model, GtkAccelGroup *accel_group )
{
	GMenuLinkIter *lter;
	GMenuAttributeIter *ater;
	gint i;
	const gchar *aname, *lname;
	GMenuModel *lv;
	GVariant *av;
	guint accel_key;
	GdkModifierType accel_mods;

	/* only attribute of the two first items of the GMenuModel is 'label'
	 * only link of the two first items of the GMenuModel are named 'submenu'
	 * the GMenuModel pointed to by 'submenu' link has no attribute
	 * but a link to a 'section'
	 * GMenuModel[attribute] = (label)
	 * GMenuModel[link] = (submenu)
	 * GMenuModel->submenu[attribute] = NULL
	 * GMenuModel->submenu[link] = (section)
	 * GMenuModel->submenu->section[attribute] = (action,label,accel)
	 * GMenuModel->submenu->section[link] = NULL
	 *
(openbook:20242): OFA-DEBUG: ofa_main_window_extract_accels: model=0x2069b30: found link at i=0 to 0x206a690
(openbook:20242): OFA-DEBUG: ofa_main_window_extract_accels: model=0x206a690: found link at i=0 to 0x206a750
(openbook:20242): OFA-DEBUG: ofa_main_window_extract_accels: model=0x206a750: found accel at i=0: <Control>n
(openbook:20242): OFA-DEBUG: ofa_main_window_extract_accels: model=0x206a750: found accel at i=1: <Control>o
(openbook:20242): OFA-DEBUG: ofa_main_window_extract_accels: model=0x206a690: found link at i=1 to 0x206b410
(openbook:20242): OFA-DEBUG: ofa_main_window_extract_accels: model=0x206b410: found accel at i=0: <Control>q
(openbook:20242): OFA-DEBUG: ofa_main_window_extract_accels: model=0x2069b30: found link at i=1 to 0x7f36cc00a000
(openbook:20242): OFA-DEBUG: ofa_main_window_extract_accels: model=0x7f36cc00a000: found link at i=0 to 0x7f36cc009f00
	 */
	for( i=0 ; i<g_menu_model_get_n_items( model ) ; ++i ){
		ater = g_menu_model_iterate_item_attributes( model, i );
		while( g_menu_attribute_iter_get_next( ater, &aname, &av )){
			if( !strcmp( aname, "accel" )){
				g_debug(
						"ofa_main_window_extract_accels: model=%p: found accel at i=%d: %s",
						( void * ) model, i, g_variant_get_string( av, NULL ));
				gtk_accelerator_parse( g_variant_get_string( av, NULL ), &accel_key, &accel_mods );
			}
			g_variant_unref( av );
		}
		g_object_unref( ater );
		lter = g_menu_model_iterate_item_links( model, i );
		while( g_menu_link_iter_get_next( lter, &lname, &lv )){
			/*g_debug( "ofa_main_window_extract_accels: model=%p: found link at i=%d to %p", ( void * ) model, i, ( void * ) lv );*/
			extract_accels_rec( window, lv, accel_group );
			g_object_unref( lv );
		}
		g_object_unref( lter );
	}
}

static void
set_window_title( ofaMainWindow *window )
{
	gchar *title;

	if( window->priv->dossier ){
		title = g_strdup_printf( "%s - %s",
				ofo_dossier_get_name( window->priv->dossier ),
				window->priv->orig_title );
	} else {
		title = g_strdup( window->priv->orig_title );
	}

	gtk_window_set_title( GTK_WINDOW( window ), title );
	g_free( title );
}

static void
on_open_dossier( ofaMainWindow *window, ofsDossierOpen *sdo, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_open_dossier";

	g_debug( "%s: window=%p, sdo=%p, label=%s, account=%s, password=%s, user_data=%p",
			thisfn, ( void * ) window,
			( void * ) sdo, sdo->label, sdo->account, sdo->password,
			( void * ) user_data );

	if( !check_for_account( window, sdo )){
		return;
	}

	if( window->priv->dossier ){
		g_return_if_fail( OFO_IS_DOSSIER( window->priv->dossier ));
		do_close_dossier( window );
	}

	window->priv->dossier = ofo_dossier_new( sdo->label );

	if( !ofo_dossier_open( window->priv->dossier, sdo->account, sdo->password )){
		g_clear_object( &window->priv->dossier );
		return;
	}

	window->priv->pane = GTK_PANED( gtk_paned_new( GTK_ORIENTATION_HORIZONTAL ));
	gtk_grid_attach( window->priv->grid, GTK_WIDGET( window->priv->pane ), 0, 1, 1, 1 );
	pane_restore_position( window->priv->pane );
	add_treeview_to_pane_left( window );
	add_empty_notebook_to_pane_right( window );

	set_menubar( window, window->priv->menu );
	set_window_title( window );
}

/*
 * If account and/or user password are empty, then let the user enter
 * a new pair of account/password.
 */
static gboolean
check_for_account( ofaMainWindow *main_window, ofsDossierOpen *sdo )
{
	if( !sdo->account || !g_utf8_strlen( sdo->account, -1 ) ||
		! sdo->password || !g_utf8_strlen( sdo->password, -1 )){

		g_free( sdo->account );
		g_free( sdo->password );

		ofa_dossier_login_run( main_window, sdo->label, &sdo->account, &sdo->password );
	}

	return( sdo->account && g_utf8_strlen( sdo->account, -1 ) &&
			sdo->password && g_utf8_strlen( sdo->password, -1 ));
}

static void
pane_restore_position( GtkPaned *pane )
{
	gchar *key;

	key = g_strdup_printf( "%s-pane", st_main_window_name );
	gtk_paned_set_position( pane, ofa_settings_get_uint( key ));
	g_free( key );
}

static void
add_treeview_to_pane_left( ofaMainWindow *window )
{
	GtkFrame *frame;
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	gint i;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_margin_left( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( frame ), 4 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );
	gtk_paned_pack1( window->priv->pane, GTK_WIDGET( frame ), FALSE, FALSE );

	view = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_hexpand( GTK_WIDGET( view ), FALSE );
	gtk_widget_set_vexpand( GTK_WIDGET( view ), TRUE );
	gtk_tree_view_set_headers_visible( view, FALSE );
	gtk_tree_view_set_activate_on_single_click( view, FALSE );
	g_signal_connect(G_OBJECT( view ), "row-activated", G_CALLBACK( on_theme_activated ), window );

	model = GTK_TREE_MODEL( gtk_list_store_new( N_COLUMNS, G_TYPE_INT, G_TYPE_STRING ));
	gtk_tree_view_set_model( view, model );
	g_object_unref( model );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			"label",
			text_cell,
			"text", COL_LABEL,
			NULL );
	gtk_tree_view_append_column( view, column );

	select = gtk_tree_view_get_selection( view );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );

	for( i=0; st_tree_defs[i].label ; ++i ){
		gtk_list_store_append( GTK_LIST_STORE( model ), &iter );
		gtk_list_store_set(
				GTK_LIST_STORE( model ),
				&iter,
				COL_TREE_IDX, i,
				COL_LABEL,    st_tree_defs[i].label,
				-1 );
	}

	gtk_tree_model_get_iter_first( model, &iter );
	gtk_tree_selection_select_iter( select, &iter );

	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( view ));
}

/*
 * the theme is activated from the left treeview pane
 */
static void
on_theme_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainWindow *window )
{
	static const gchar *thisfn = "ofa_main_window_on_theme_activated";
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint idx;

	g_debug( "%s: view=%p, path=%p, column=%p, window=%p",
			thisfn, ( void * ) view, ( void * ) path, ( void * ) column, ( void * ) window );

	model = gtk_tree_view_get_model( view );

	if( gtk_tree_model_get_iter( model, &iter, path )){
		gtk_tree_model_get( model, &iter, COL_TREE_IDX, &idx, -1 );

		ofa_main_window_activate_theme( window, st_tree_defs[idx].theme_id );
	}
}

/*
 * return NULL if not found
 */
static const sThemeDef *
get_theme_def_from_id( gint theme_id )
{
	static const gchar *thisfn = "ofa_main_window_get_theme_def_from_id";
	gint i;

	for( i=0 ; st_theme_defs[i].label ; ++i ){
		if( st_theme_defs[i].theme_id == theme_id ){
			return(( sThemeDef * ) &st_theme_defs[i] );
		}
	}

	g_warning( "%s: unable to find theme definition for id=%d", thisfn, theme_id );

	return( NULL );
}

static void
add_empty_notebook_to_pane_right( ofaMainWindow *window )
{
	GtkNotebook *book;

	book = GTK_NOTEBOOK( gtk_notebook_new());
	gtk_widget_set_margin_right( GTK_WIDGET( book ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( book ), 4 );
	gtk_notebook_set_scrollable( book, TRUE );
	gtk_notebook_popup_enable( book );

	gtk_paned_pack2( window->priv->pane, GTK_WIDGET( book ), TRUE, FALSE );
}

static void
on_open_dossier_cleanup_handler( ofaMainWindow *window, ofsDossierOpen *sdo )
{
	static const gchar *thisfn = "ofa_main_window_on_open_dossier_cleanup_handler";

	g_debug( "%s: window=%p, sdo=%p, label=%s, account=%s, password=%s",
			thisfn, ( void * ) window,
			( void * ) sdo, sdo->label, sdo->account, sdo->password );

	g_free( sdo->label );
	g_free( sdo->account );
	g_free( sdo->password );
	g_free( sdo );
}

static void
on_update_properties( ofaMainWindow *window, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_update_properties";

	g_debug( "%s: window=%p, user_data=%p",
			thisfn, ( void * ) window, ( void * ) user_data );

	if( window->priv->dossier ){
		ofa_dossier_properties_run( window, window->priv->dossier );
	}
}

static void
on_update_properties_cleanup_handler( ofaMainWindow *window )
{
	static const gchar *thisfn = "ofa_main_window_on_update_properties_cleanup_handler";

	g_debug( "%s: window=%p", thisfn, ( void * ) window );
}

static void
on_backup( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_backup";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));
	priv = OFA_MAIN_WINDOW( user_data )->priv;

	g_return_if_fail( priv->dossier && OFO_IS_DOSSIER( priv->dossier ));

	ofa_backup_run( OFA_MAIN_WINDOW( user_data ));
}

static void
on_close( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_close";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));
	priv = OFA_MAIN_WINDOW( user_data )->priv;

	g_return_if_fail( priv->dossier && OFO_IS_DOSSIER( priv->dossier ));

	do_close_dossier( OFA_MAIN_WINDOW( user_data ));
}

/**
 * ofa_main_window_close_dossier:
 */
void
ofa_main_window_close_dossier( ofaMainWindow *main_window )
{
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	if( !main_window->priv->dispose_has_run ){

		do_close_dossier( main_window );
	}
}

static void
do_close_dossier( ofaMainWindow *self )
{
	ofaApplication *appli;
	ofaMainWindowPrivate *priv;

	priv = self->priv;

	g_clear_object( &priv->dossier );

	gtk_widget_destroy( GTK_WIDGET( priv->pane ));
	priv->pane = NULL;

	appli = OFA_APPLICATION( gtk_window_get_application( GTK_WINDOW( self )));
	set_menubar( self, ofa_application_get_menu_model( appli ));

	set_window_title( self );
}

static void
on_properties( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_properties";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));
	priv = OFA_MAIN_WINDOW( user_data )->priv;

	ofa_dossier_properties_run( OFA_MAIN_WINDOW( user_data ), priv->dossier );
}

static void
on_ope_guided( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_guided";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_main_window_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_OPE_TEMPLATES );
}

static void
on_ope_view_entries( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_guided";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_main_window_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_VIEW_ENTRIES );
}

static void
on_ope_concil( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_concil";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_main_window_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_RECONCIL );
}

static void
on_ope_int_closing( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_int_closing";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_int_closing_run( OFA_MAIN_WINDOW( user_data ));
}

static void
on_ope_import( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_import";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_import_run( OFA_MAIN_WINDOW( user_data ));
}

static void
on_ope_export( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_export";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_export_run( OFA_MAIN_WINDOW( user_data ));
}

static void
on_print_reconcil( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_export";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_print_reconcil_run( OFA_MAIN_WINDOW( user_data ));
}

static void
on_ref_accounts( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_accounts";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_main_window_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_ACCOUNTS );
}

static void
on_ref_ledgers( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_ledgers";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_main_window_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_LEDGERS );
}

static void
on_ref_ope_templates( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_models";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_main_window_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_OPE_TEMPLATES );
}

static void
on_ref_currencies( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_devises";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_main_window_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_CURRENCIES );
}

static void
on_ref_rates( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_rates";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_main_window_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_RATES );
}

static void
on_ref_classes( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_classes";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_main_window_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_CLASSES );
}

static void
on_ref_batfiles( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_classes";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_main_window_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_BATFILES );
}

/**
 * ofa_main_window_get_dossier:
 */
ofoDossier *
ofa_main_window_get_dossier( const ofaMainWindow *window )
{
	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( window ), NULL );

	if( !window->priv->dispose_has_run ){

		return( window->priv->dossier );
	}

	return( NULL );
}

/**
 * ofa_main_window_activate_theme:
 *
 * if the main page doesn't exist yet, then create it
 * then make sure it is displayed, and activate it
 * last run it
 */
ofaPage *
ofa_main_window_activate_theme( ofaMainWindow *main_window, gint theme )
{
	static const gchar *thisfn = "ofa_main_window_activate_theme";
	GtkNotebook *main_book;
	GtkWidget *page;
	ofaPage *handler;
	const sThemeDef *theme_def;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), NULL );

	handler = NULL;

	if( !main_window->priv->dispose_has_run ){

		g_debug( "%s: main_window=%p, theme=%d", thisfn, ( void * ) main_window, theme );

		main_book = main_get_book( main_window );
		g_return_val_if_fail( main_book && GTK_IS_NOTEBOOK( main_book ), NULL );

		theme_def = get_theme_def_from_id( theme );
		g_return_val_if_fail( theme_def, NULL );
		g_return_val_if_fail( theme_def->fn_get_type, NULL );

		page = main_book_get_page( main_window, main_book, theme );
		if( !page ){
			page = main_book_create_page( main_window, main_book, theme_def );
		}
		g_return_val_if_fail( page && GTK_IS_WIDGET( page ), NULL );
		main_book_activate_page( main_window, main_book, page );

		handler = ( ofaPage * ) g_object_get_data( G_OBJECT( page ), OFA_DATA_HANDLER );
		g_return_val_if_fail( handler && OFA_IS_PAGE( handler ), NULL );
	}

	return( handler );
}

static GtkNotebook *
main_get_book( const ofaMainWindow *window )
{
	GtkWidget *book;

	book = gtk_paned_get_child2( window->priv->pane );
	g_return_val_if_fail( GTK_IS_NOTEBOOK( book ), NULL );

	return( GTK_NOTEBOOK( book ));
}

static GtkWidget *
main_book_get_page( const ofaMainWindow *window, GtkNotebook *book, gint theme )
{
	GtkWidget *page, *found;
	gint count, i, page_thm;
	ofaPage *handler;

	found = NULL;
	count = gtk_notebook_get_n_pages( book );
	for( i=0 ; !found && i<count ; ++i ){
		page = gtk_notebook_get_nth_page( book, i );
		handler = ( ofaPage * ) g_object_get_data( G_OBJECT( page ), OFA_DATA_HANDLER );
		g_return_val_if_fail( handler && OFA_IS_PAGE( handler ), NULL );
		page_thm = ofa_page_get_theme( handler );
		if( page_thm == theme ){
			found = page;
		}
	}

	return( found );
}

/*
 * the page for this theme has not been found
 * so, create it as an empty GtkGrid, simultaneously instanciating the
 * handling object as an ofaPage
 */
static GtkWidget *
main_book_create_page( ofaMainWindow *main, GtkNotebook *book, const sThemeDef *theme_def )
{
	GtkGrid *grid;
	ofaPage *handler;
	myTabLabel *tab;
	GtkWidget *label;

	/* all pages of the main notebook begin with a GtkGrid
	 */
	grid = GTK_GRID( gtk_grid_new());
	gtk_grid_set_column_spacing( grid, 4 );

	tab = my_tab_label_new( NULL, gettext( theme_def->label ));
	g_signal_connect(
			G_OBJECT( tab),
			MY_SIGNAL_TAB_CLOSE_CLICKED, G_CALLBACK( on_tab_close_clicked ), grid );

	label = gtk_label_new( gettext( theme_def->label ));
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );

	gtk_notebook_append_page_menu( book, GTK_WIDGET( grid ), GTK_WIDGET( tab ), label );
	gtk_notebook_set_tab_reorderable( book, GTK_WIDGET( grid ), TRUE );

	/* then instanciates the handling object which happens to be an
	 * ofaPage
	 */
	handler = g_object_new(( *theme_def->fn_get_type )(),
					PAGE_PROP_MAIN_WINDOW, main,
					PAGE_PROP_TOP_GRID,    grid,
					PAGE_PROP_THEME,       theme_def->theme_id,
					NULL );

	g_object_set_data( G_OBJECT( grid ), OFA_DATA_HANDLER, handler );

	return( GTK_WIDGET( grid ));
}

/*
 * ofa_main_book_activate_page:
 *
 * Activating the page mainly consists in giving the focus to the first
 * embedded treeview.
 */
static void
main_book_activate_page( const ofaMainWindow *window, GtkNotebook *book, GtkWidget *page )
{
	gint page_num;
	GtkWidget *widget;
	ofaPage *handler;

	g_return_if_fail( window && OFA_IS_MAIN_WINDOW( window ));
	g_return_if_fail( book && GTK_IS_NOTEBOOK( book ));
	g_return_if_fail( page && GTK_IS_WIDGET( page ));

	gtk_widget_show_all( GTK_WIDGET( window ));

	page_num = gtk_notebook_page_num( book, page );
	gtk_notebook_set_current_page( book, page_num );

	handler = ( ofaPage * ) g_object_get_data( G_OBJECT( page ), OFA_DATA_HANDLER );
	g_return_if_fail( handler && OFA_IS_PAGE( handler ));

	widget = ofa_page_get_top_focusable_widget( handler );
	if( widget ){
		g_return_if_fail( GTK_IS_WIDGET( widget ));
		gtk_widget_grab_focus( widget );
	}
}

/*
 * @grid: the grid which contains the #ofaPage content of the main
 *  GtkNotebook
 */
static void
on_tab_close_clicked( myTabLabel *tab, GtkGrid *grid )
{
	ofaPage *handler;
	ofaMainWindow *main_window;
	GtkNotebook *book;
	gint page_num;

	g_debug( "ofa_main_window_on_tab_close_clicked: tab=%p", ( void * ) tab );

	handler = ( ofaPage * ) g_object_get_data( G_OBJECT( grid ), OFA_DATA_HANDLER );
	g_return_if_fail( handler && OFA_IS_PAGE( handler ));

	main_window = ofa_page_get_main_window( handler );
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	book = main_get_book( main_window );
	g_return_if_fail( book && GTK_IS_NOTEBOOK( book ));

	page_num = gtk_notebook_page_num( book, GTK_WIDGET( grid ));
	g_return_if_fail( page_num >= 0 );

	ofa_page_pre_remove( handler );
	gtk_notebook_remove_page( book, page_num );
}

/**
 * ofa_main_window_confirm_deletion:
 * @window: [allow-null]: this main window, or %NULL if the method is
 *  not called from a #ofaPage-derived class.
 * @message: the message to be displayed.
 *
 * Returns: %TRUE if the deletion is confirmed by the user.
 */
gboolean
ofa_main_window_confirm_deletion( const ofaMainWindow *window, const gchar *message )
{
	GtkWidget *dialog;
	gint response;

	dialog = gtk_message_dialog_new(
			window ? GTK_WINDOW( window ) : NULL,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			"%s", message );

	gtk_dialog_add_buttons( GTK_DIALOG( dialog ),
			_( "_Cancel" ), GTK_RESPONSE_CANCEL,
			_( "_Delete" ), GTK_RESPONSE_OK,
			NULL );

	response = gtk_dialog_run( GTK_DIALOG( dialog ));

	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
}
