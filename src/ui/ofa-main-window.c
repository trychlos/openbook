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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-dossier-misc.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "api/ofa-preferences.h"

#include "ui/my-tab-label.h"
#include "ui/ofa-account-book-render.h"
#include "ui/ofa-account-page.h"
#include "ui/ofa-application.h"
#include "ui/ofa-backup.h"
#include "ui/ofa-balance-render.h"
#include "ui/ofa-bat-page.h"
#include "ui/ofa-check-balances.h"
#include "ui/ofa-check-integrity.h"
#include "ui/ofa-class-page.h"
#include "ui/ofa-currency-page.h"
#include "ui/ofa-dossier-display-notes.h"
#include "ui/ofa-dossier-login.h"
#include "ui/ofa-dossier-properties.h"
#include "ui/ofa-entry-page.h"
#include "ui/ofa-exercice-close-assistant.h"
#include "ui/ofa-export-assistant.h"
#include "ui/ofa-guided-ex.h"
#include "ui/ofa-guided-input.h"
#include "ui/ofa-import-assistant.h"
#include "ui/ofa-ledger-close.h"
#include "ui/ofa-ledger-book-render.h"
#include "ui/ofa-ledger-page.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-ope-template-page.h"
#include "ui/ofa-page.h"
#include "ui/ofa-rate-page.h"
#include "ui/ofa-reconcil-render.h"
#include "ui/ofa-reconcil-page.h"
#include "ui/ofa-settlement.h"

/* private instance data
 */
struct _ofaMainWindowPrivate {
	gboolean       dispose_has_run;

	/* dossier credentials at opening time
	 */
	gchar         *dos_account;
	gchar         *dos_password;
	gchar         *dos_dbname;

	/* internals
	 */
	gchar         *orig_title;
	GtkGrid       *grid;
	GtkMenuBar    *menubar;
	GtkAccelGroup *accel_group;
	GMenuModel    *menu;				/* the menu model when a dossier is opened */
	GtkPaned      *pane;
	ofoDossier    *dossier;

	/* menu items enabled status
	 */
	gboolean       dossier_begin;		/* whether the exercice beginning date is valid */

	GSimpleAction *action_guided_input;
	GSimpleAction *action_settlement;
	GSimpleAction *action_reconciliation;
	GSimpleAction *action_close_ledger;
	GSimpleAction *action_close_exercice;
	GSimpleAction *action_import;
};

/* signals defined here
 */
enum {
	DOSSIER_OPEN = 0,
	DOSSIER_PROPERTIES,
	OPENED_DOSSIER,
	DIALOG_INIT,
	N_SIGNALS
};

static void on_properties          ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_backup              ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_close               ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_guided          ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_entry_page      ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_concil          ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_settlement      ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_ledger_close    ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_exercice_close  ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_import          ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_export          ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_render_balances     ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_render_accounts_book( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_render_ledgers_book ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_render_reconcil     ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_accounts        ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_ledgers         ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_ope_templates   ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_currencies      ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_rates           ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_classes         ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_batfiles        ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_check_balances      ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_check_integrity     ( GSimpleAction *action, GVariant *parameter, gpointer user_data );

static const GActionEntry st_dos_entries[] = {
		{ "properties",       on_properties,           NULL, NULL, NULL },
		{ "backup",           on_backup,               NULL, NULL, NULL },
		{ "close",            on_close,                NULL, NULL, NULL },
		{ "guided",           on_ope_guided,           NULL, NULL, NULL },
		{ "entries",          on_ope_entry_page,       NULL, NULL, NULL },
		{ "concil",           on_ope_concil,           NULL, NULL, NULL },
		{ "settlement",       on_ope_settlement,       NULL, NULL, NULL },
		{ "ledclosing",       on_ope_ledger_close,     NULL, NULL, NULL },
		{ "execlosing",       on_ope_exercice_close,   NULL, NULL, NULL },
		{ "import",           on_ope_import,           NULL, NULL, NULL },
		{ "export",           on_ope_export,           NULL, NULL, NULL },
		{ "render-balances",  on_render_balances,      NULL, NULL, NULL },
		{ "render-books",     on_render_accounts_book, NULL, NULL, NULL },
		{ "render-ledgers",   on_render_ledgers_book,  NULL, NULL, NULL },
		{ "render-reconcil",  on_render_reconcil,      NULL, NULL, NULL },
		{ "accounts",         on_ref_accounts,         NULL, NULL, NULL },
		{ "ledgers",          on_ref_ledgers,          NULL, NULL, NULL },
		{ "ope-templates",    on_ref_ope_templates,    NULL, NULL, NULL },
		{ "currencies",       on_ref_currencies,       NULL, NULL, NULL },
		{ "rates",            on_ref_rates,            NULL, NULL, NULL },
		{ "classes",          on_ref_classes,          NULL, NULL, NULL },
		{ "batfiles",         on_ref_batfiles,         NULL, NULL, NULL },
		{ "chkbal",           on_check_balances,       NULL, NULL, NULL },
		{ "integrity",        on_check_integrity,      NULL, NULL, NULL },
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
	gboolean     if_entries_allowed;
}
	sThemeDef;

static sThemeDef st_theme_defs[] = {

		{ THM_ACCOUNTS,
				N_( "Chart of accounts" ),
				ofa_account_page_get_type,
				FALSE
		},
		{ THM_BATFILES,
				N_( "Imported BAT files" ),
				ofa_bat_page_get_type,
				FALSE
		},

		{ THM_CLASSES,
				N_( "Account classes" ),
				ofa_class_page_get_type,
				FALSE
		},
		{ THM_CURRENCIES,
				N_( "Currencies" ),
				ofa_currency_page_get_type,
				FALSE
		},
		{ THM_GUIDED_INPUT,
				N_( "Guided input" ),
				ofa_guided_ex_get_type,
				TRUE
		},
		{ THM_LEDGERS,
				N_( "Ledgers" ),
				ofa_ledger_page_get_type,
				FALSE
		},
		{ THM_OPE_TEMPLATES,
				N_( "Operation templates" ),
				ofa_ope_template_page_get_type,
				FALSE
		},
		{ THM_RATES,
				N_( "Rates" ),
				ofa_rate_page_get_type,
				FALSE
		},
		{ THM_RECONCIL,
				N_( "Reconciliation" ),
				ofa_reconcil_page_get_type,
				FALSE
		},
		{ THM_RENDER_BALANCES,
				N_( "Entries balance" ),
				ofa_balance_render_get_type,
				FALSE
		},
		{ THM_RENDER_ACCOUNTS_BOOK,
				N_( "Accounts book" ),
				ofa_account_book_render_get_type,
				FALSE
		},
		{ THM_RENDER_LEDGERS_BOOK,
				N_( "Ledgers book" ),
				ofa_ledger_book_render_get_type,
				FALSE
		},
		{ THM_RENDER_RECONCIL,
				N_( "Reconciliation Sumary" ),
				ofa_reconcil_render_get_type,
				FALSE
		},
		{ THM_SETTLEMENT,
				N_( "Settlement" ),
				ofa_settlement_get_type,
				FALSE
		},
		{ THM_ENTRIES,
				N_( "View entries" ),
				ofa_entry_page_get_type,
				FALSE
		},
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

static const gchar *st_main_window_name = "MainWindow";
static const gchar *st_dosmenu_xml      = PKGUIDIR "/ofa-dos-menubar.ui";
static const gchar *st_dosmenu_id       = "dos-menu";
static const gchar *st_icon_fname       = ICONFNAME;

static guint        st_signals[ N_SIGNALS ] = { 0 };

G_DEFINE_TYPE( ofaMainWindow, ofa_main_window, GTK_TYPE_APPLICATION_WINDOW )

static void             pane_save_position( GtkPaned *pane );
static gboolean         on_delete_event( GtkWidget *toplevel, GdkEvent *event, gpointer user_data );
static void             do_open_dossier( ofaMainWindow *main_window, ofaIDBConnect *connect, gboolean remediation );
static void             set_menubar( ofaMainWindow *window, GMenuModel *model );
static void             extract_accels_rec( ofaMainWindow *window, GMenuModel *model, GtkAccelGroup *accel_group );
static void             set_window_title( const ofaMainWindow *window );
static void             on_dossier_open( ofaMainWindow *window, ofaIDBConnect *connect, gpointer user_data );
static void             warning_exercice_unset( const ofaMainWindow *window );
static void             warning_archived_dossier( const ofaMainWindow *window );
static void             on_dossier_properties( ofaMainWindow *window, gpointer user_data );
static void             pane_restore_position( GtkPaned *pane );
static void             add_treeview_to_pane_left( ofaMainWindow *window );
static void             on_theme_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainWindow *window );
static const sThemeDef *get_theme_def_from_id( gint theme_id );
static void             add_empty_notebook_to_pane_right( ofaMainWindow *window );
static void             on_opened_dossier( ofaMainWindow *window, ofoDossier *dossier, void *empty );
static void             enable_action_guided_input( ofaMainWindow *window, gboolean enable );
static void             enable_action_settlement( ofaMainWindow *window, gboolean enable );
static void             enable_action_reconciliation( ofaMainWindow *window, gboolean enable );
static void             enable_action_close_ledger( ofaMainWindow *window, gboolean enable );
static void             enable_action_close_exercice( ofaMainWindow *window, gboolean enable );
static void             enable_action_import( ofaMainWindow *window, gboolean enable );
static void             on_dossier_properties_cleanup_handler( ofaMainWindow *window );
static void             do_close_dossier( ofaMainWindow *self );
static GtkNotebook     *main_get_book( const ofaMainWindow *window );
static ofaPage         *main_book_get_page( const ofaMainWindow *window, GtkNotebook *book, gint theme );
static ofaPage         *main_book_create_page( const ofaMainWindow *main, GtkNotebook *book, const sThemeDef *theme_def );
static void             main_book_activate_page( const ofaMainWindow *window, GtkNotebook *book, ofaPage *page );
static void             on_tab_close_clicked( myTabLabel *tab, ofaPage *page );
static void             on_page_removed( GtkNotebook *book, GtkWidget *page, guint page_num, ofaMainWindow *main_window );
static void             close_all_pages( ofaMainWindow *main_window );

static void
main_window_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_main_window_finalize";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MAIN_WINDOW( instance ));

	/* free data members here */
	priv = OFA_MAIN_WINDOW( instance )->priv;

	g_free( priv->dos_account );
	g_free( priv->dos_password );
	g_free( priv->dos_dbname );
	g_free( priv->orig_title );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_main_window_parent_class )->finalize( instance );
}

static void
main_window_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofa_main_window_dispose";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_if_fail( instance && OFA_IS_MAIN_WINDOW( instance ));

	priv = OFA_MAIN_WINDOW( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;
		my_utils_window_save_position( GTK_WINDOW( instance ), st_main_window_name );
		if( priv->pane ){
			pane_save_position( priv->pane );
		}

		/* unref object members here */
		g_clear_object( &priv->menu );
		g_clear_object( &priv->dossier );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_main_window_parent_class )->dispose( instance );
}

static void
pane_save_position( GtkPaned *pane )
{
	gchar *key;

	key = g_strdup_printf( "%s-pane", st_main_window_name );
	ofa_settings_set_int( key, gtk_paned_get_position( pane ));
	g_free( key );
}

static void
main_window_constructed( GObject *instance )
{
	static const gchar *thisfn = "ofa_main_window_constructed";
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
				priv->menu = g_object_ref( menu );
				g_debug( "%s: menu successfully loaded from %s at %p: items=%d",
						thisfn, st_dosmenu_xml, ( void * ) menu, g_menu_model_get_n_items( menu ));
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

		/* connect the action signals
		 */
		g_signal_connect( instance,
				OFA_SIGNAL_DOSSIER_OPEN, G_CALLBACK( on_dossier_open ), NULL );
		g_signal_connect( instance,
				OFA_SIGNAL_DOSSIER_PROPERTIES, G_CALLBACK( on_dossier_properties ), NULL );

		/* set the default icon for all windows of the application */
		error = NULL;
		gtk_window_set_default_icon_from_file( st_icon_fname, &error );
		if( error ){
			g_warning( "%s: %s", thisfn, error->message );
			g_error_free( error );
		}
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

	/**
	 * ofaMainWindow::ofa-signal-dossier-open:
	 *
	 * This signal is to be sent to the main window in order a dossier
	 * be opened in the user interface.
	 *
	 * Argument is the #ofaIDBConnect object which handles the
	 * (already opened et validated) connection.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaMainWindow   *window,
	 * 						ofaIDBConnect *connect,
	 * 						gpointer        user_data );
	 */
	st_signals[ DOSSIER_OPEN ] = g_signal_new_class_handler(
				OFA_SIGNAL_DOSSIER_OPEN,
				OFA_TYPE_MAIN_WINDOW,
				G_SIGNAL_ACTION,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );

	/**
	 * ofaMainWindow::ofa-signal-dossier-properties:
	 *
	 * This signal is to be sent to the main window for updating the
	 * dossier properties (as an alternative to the menu item).
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaMainWindow *window,
	 * 						gpointer user_data );
	 */
	st_signals[ DOSSIER_PROPERTIES ] = g_signal_new_class_handler(
				OFA_SIGNAL_DOSSIER_PROPERTIES,
				OFA_TYPE_MAIN_WINDOW,
				G_SIGNAL_RUN_CLEANUP | G_SIGNAL_ACTION,
				G_CALLBACK( on_dossier_properties_cleanup_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0 );

	/**
	 * ofaMainWindow::ofa-opened-dossier:
	 *
	 * This signal is sent on the main window when a dossier has been
	 * opened.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaMainWindow *window,
	 *                      ofoDossier  *dossier,
	 * 						gpointer     user_data );
	 */
	st_signals[ OPENED_DOSSIER ] = g_signal_new_class_handler(
				"ofa-opened-dossier",
				OFA_TYPE_MAIN_WINDOW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaMainWindow::my-dialog-init:
	 *
	 * This signal is sent on the main window at the end of the
	 * initialization of a MyDialog
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaMainWindow *window,
	 *                      const gchar *dialog_name,
	 *                      GtkWindow   *toplevel,
	 * 						gpointer     user_data );
	 */
	st_signals[ DIALOG_INIT ] = g_signal_new_class_handler(
				"my-dialog-init",
				OFA_TYPE_MAIN_WINDOW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_STRING, G_TYPE_POINTER );
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
	g_signal_emit_by_name(( gpointer ) application, "main-window-created", window );

	g_object_get( G_OBJECT( application ),
			OFA_PROP_APPLICATION_NAME, &window->priv->orig_title,
			NULL );

	set_menubar( window, ofa_application_get_menu_model( application ));

	g_signal_connect( window, "ofa-opened-dossier", G_CALLBACK( on_opened_dossier ), NULL );

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

	ok_to_quit = !ofa_prefs_appli_confirm_on_altf4() ||
					ofa_main_window_is_willing_to_quit( OFA_MAIN_WINDOW( toplevel ));

	return( !ok_to_quit );
}

/**
 * ofa_main_window_is_willing_to_quit:
 * @main_window: this #ofaMainWindow instance.
 *
 * Ask a user for a confirmation when quitting.
 *
 * Returns: %TRUE is the user confirms he wants quit the application,
 * %FALSE else.
 */
gboolean
ofa_main_window_is_willing_to_quit( ofaMainWindow *main_window )
{
	return( my_utils_dialog_question(
			_( "Are you sure you want to quit the application ?" ), _( "_Quit" )));
}

/**
 * ofa_main_window_open_dossier:
 * @main_window: this #ofaMainWindow instance.
 * @connect: the #ofaIDBConnect object which handles the opened
 *  connection.
 * @remediation: whether remediate the dossier settings regarding
 *  the actual content of the dossier database.
 *
 * Open the dossier in the user interface.
 */
void
ofa_main_window_open_dossier( ofaMainWindow *main_window, ofaIDBConnect *connect, gboolean remediation )
{
	static const gchar *thisfn = "ofa_main_window_open_dossier";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: main_window=%p, connect=%p", thisfn, ( void * ) main_window, ( void * ) connect );

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	g_return_if_fail( connect && OFA_IS_IDBCONNECT( connect ));

	priv = main_window->priv;

	if( !priv->dispose_has_run ){

		if( priv->dossier ){
			g_return_if_fail( OFO_IS_DOSSIER( priv->dossier ));
			do_close_dossier( main_window );
		}

		do_open_dossier( main_window, connect, remediation );
	}
}

static void
do_open_dossier( ofaMainWindow *main_window, ofaIDBConnect *connect, gboolean remediation )
{
	ofaMainWindowPrivate *priv;
	const GDate *exe_begin, *exe_end;
	const gchar *main_notes, *exe_notes;

	priv = main_window->priv;
	priv->dossier = ofo_dossier_new( connect, remediation );

	if( priv->dossier ){
		priv->pane = GTK_PANED( gtk_paned_new( GTK_ORIENTATION_HORIZONTAL ));
		gtk_grid_attach( priv->grid, GTK_WIDGET( priv->pane ), 0, 1, 1, 1 );
		pane_restore_position( priv->pane );
		add_treeview_to_pane_left( main_window );
		add_empty_notebook_to_pane_right( main_window );

		set_menubar( main_window, priv->menu );
		set_window_title( main_window );

		/* warns if begin or end of exercice is not set */
		exe_begin = ofo_dossier_get_exe_begin( priv->dossier );
		exe_end = ofo_dossier_get_exe_end( priv->dossier );
		if( !my_date_is_valid( exe_begin ) || !my_date_is_valid( exe_end )){
			warning_exercice_unset( main_window );
		}

		/* display dossier notes */
		if( ofa_prefs_dossier_open_notes()){
			main_notes = ofo_dossier_get_notes( priv->dossier );
			exe_notes = ofo_dossier_get_exe_notes( priv->dossier );
			if( my_strlen( main_notes ) ||
					my_strlen( exe_notes ) ||
					ofa_prefs_dossier_open_notes_if_empty()){
				ofa_dossier_display_notes_run( main_window, main_notes, exe_notes );
			}
		}

		/* display dossier properties */
		if( ofa_prefs_dossier_open_properties()){
			g_signal_emit_by_name(( gpointer ) main_window, OFA_SIGNAL_DOSSIER_PROPERTIES );
		}

		g_signal_emit_by_name( main_window, "ofa-opened-dossier", priv->dossier );
	}
}

static void
set_menubar( ofaMainWindow *window, GMenuModel *model )
{
	ofaMainWindowPrivate *priv;
	GtkWidget *menubar;

	priv = window->priv;

	if( priv->menubar ){
		gtk_widget_destroy( GTK_WIDGET( priv->menubar ));
		priv->menubar = NULL;
	}

	if( priv->accel_group ){
		gtk_window_remove_accel_group( GTK_WINDOW( window ), priv->accel_group );
		g_object_unref( priv->accel_group );
		priv->accel_group = NULL;
	}

	/*accels = gtk_accel_groups_from_object( G_OBJECT( model ));
	 *accels = gtk_accel_groups_from_object( G_OBJECT( menubar ));
	 * return nil */

	priv->accel_group = gtk_accel_group_new();
	extract_accels_rec( window, model, priv->accel_group );
	gtk_window_add_accel_group( GTK_WINDOW( window ), priv->accel_group );

	menubar = gtk_menu_bar_new_from_model( model );
	g_debug( "set_menubar: model=%p (%s), menubar=%p, grid=%p (%s)",
			( void * ) model, G_OBJECT_TYPE_NAME( model ), ( void * ) menubar,
			( void * ) priv->grid, G_OBJECT_TYPE_NAME( priv->grid ));
	gtk_grid_attach( priv->grid, menubar, 0, 0, 1, 1 );
	priv->menubar = GTK_MENU_BAR( menubar );
	gtk_widget_show_all( GTK_WIDGET( window ));
}

static void
extract_accels_rec( ofaMainWindow *window, GMenuModel *model, GtkAccelGroup *accel_group )
{
	static const gchar *thisfn = "ofa_main_window_extract_accels_rec";
	GMenuLinkIter *lter;
	GMenuAttributeIter *ater;
	gint i;
	const gchar *aname, *lname;
	GMenuModel *lv;
	GVariant *av;
	guint accel_key;
	GdkModifierType accel_mods;

	g_debug( "%s: window=%p, model=%p, accel_group=%p",
			thisfn, ( void * ) window, ( void * ) model, ( void * ) accel_group );

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

/**
 * ofa_main_window_update_title:
 */
void
ofa_main_window_update_title( const ofaMainWindow *window )
{
	ofaMainWindowPrivate *priv;

	g_return_if_fail( window && OFA_IS_MAIN_WINDOW( window ));

	priv = window->priv;

	if( !priv->dispose_has_run ){

		set_window_title( window );
	}
}

static void
set_window_title( const ofaMainWindow *window )
{
	ofaMainWindowPrivate *priv;
	const ofaIDBConnect *cnx;
	ofaIFileMeta *meta;
	ofaIFilePeriod *period;
	gchar *title, *dos_name, *label;

	priv = window->priv;

	if( priv->dossier ){
		cnx = ofo_dossier_get_connect( priv->dossier );
		meta = ofa_idbconnect_get_meta( cnx );
		period = ofa_idbconnect_get_period( cnx );
		dos_name = ofa_ifile_meta_get_dossier_name( meta );
		label = ofa_ifile_period_get_label( period );

		title = g_strdup_printf( "%s (%s) %s - %s",
				dos_name,
				priv->dos_dbname,
				label,
				priv->orig_title );

		g_free( dos_name );
		g_free( label );
		g_object_unref( period );
		g_object_unref( meta );

	} else {
		title = g_strdup( priv->orig_title );
	}

	gtk_window_set_title( GTK_WINDOW( window ), title );
	g_free( title );
}

static void
on_dossier_open( ofaMainWindow *window, ofaIDBConnect *connect, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_dossier_open";

	g_debug( "%s: window=%p, connect=%p, user_data=%p",
			thisfn, ( void * ) window, ( void * ) connect, ( void * ) user_data );
}

/*
 * warning_exercice_unset:
 */
static void
warning_exercice_unset( const ofaMainWindow *window )
{
	GtkWidget *dialog;
	gchar *str;
	gint resp;

	str = g_strdup_printf(
				_( "Warning: the exercice beginning or ending dates of "
					"the dossier are not set.\n\n"
					"This may be very problematic and error prone if you "
					"ever want import past entries, or enter future operations.\n\n"
					"You are strongly advised to set both beginning and "
					"ending dates of the current exercice." ));

	dialog = gtk_message_dialog_new(
			NULL,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_CLOSE,
			"%s", str );

	gtk_dialog_add_button( GTK_DIALOG( dialog ), "Dossier properties...", 1 );

	g_free( str );
	resp = gtk_dialog_run( GTK_DIALOG( dialog ));
	gtk_widget_destroy( dialog );

	if( resp == 1 ){
		g_signal_emit_by_name(( gpointer ) window, OFA_SIGNAL_DOSSIER_PROPERTIES );
	}
}

/*
 * warning_archived_dossier:
 */
static void
warning_archived_dossier( const ofaMainWindow *window )
{
	gchar *str;

	str = g_strdup_printf(
				_( "Warning: this exercice has been archived.\n\n"
					"No new entry is allowed on an archived exercice." ));

	my_utils_dialog_warning( str );

	g_free( str );
}

static void
pane_restore_position( GtkPaned *pane )
{
	gchar *key;

	key = g_strdup_printf( "%s-pane", st_main_window_name );
	gtk_paned_set_position( pane, ofa_settings_get_int( key ));
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
	my_utils_widget_set_margin( GTK_WIDGET( frame ), 4, 4, 4, 2 );
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
	my_utils_widget_set_margin( GTK_WIDGET( book ), 4, 4, 2, 4 );
	gtk_notebook_set_scrollable( book, TRUE );
	gtk_notebook_popup_enable( book );

	g_signal_connect(
			G_OBJECT( book ), "page-removed", G_CALLBACK( on_page_removed ), window );

	gtk_paned_pack2( window->priv->pane, GTK_WIDGET( book ), TRUE, FALSE );
}

/*
 * signal sent after the dossier has been successfully opened
 */
static void
on_opened_dossier( ofaMainWindow *window, ofoDossier *dossier, void *empty )
{
	gboolean is_current;

	is_current = ofo_dossier_is_current( dossier );

	enable_action_guided_input( window, is_current );
	enable_action_settlement( window, is_current );
	enable_action_reconciliation( window, is_current );
	enable_action_close_ledger( window, is_current );
	enable_action_close_exercice( window, is_current );
	enable_action_import( window, is_current );
}

static void
enable_action_guided_input( ofaMainWindow *window, gboolean enable )
{
	ofaMainWindowPrivate *priv;

	priv = window->priv;

	my_utils_action_enable( G_ACTION_MAP( window ), &priv->action_guided_input, "guided", enable );
}

static void
enable_action_settlement( ofaMainWindow *window, gboolean enable )
{
	ofaMainWindowPrivate *priv;

	priv = window->priv;

	my_utils_action_enable( G_ACTION_MAP( window ), &priv->action_settlement, "settlement", enable );
}

static void
enable_action_reconciliation( ofaMainWindow *window, gboolean enable )
{
	ofaMainWindowPrivate *priv;

	priv = window->priv;

	my_utils_action_enable( G_ACTION_MAP( window ), &priv->action_reconciliation, "concil", enable );
}

static void
enable_action_close_ledger( ofaMainWindow *window, gboolean enable )
{
	ofaMainWindowPrivate *priv;

	priv = window->priv;

	my_utils_action_enable( G_ACTION_MAP( window ), &priv->action_close_ledger, "ledclosing", enable );
}

static void
enable_action_close_exercice( ofaMainWindow *window, gboolean enable )
{
	ofaMainWindowPrivate *priv;

	priv = window->priv;

	my_utils_action_enable( G_ACTION_MAP( window ), &priv->action_close_exercice, "execlosing", enable );
}

static void
enable_action_import( ofaMainWindow *window, gboolean enable )
{
	ofaMainWindowPrivate *priv;

	priv = window->priv;

	my_utils_action_enable( G_ACTION_MAP( window ), &priv->action_import, "import", enable );
}

static void
on_dossier_properties( ofaMainWindow *window, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_dossier_properties";

	g_debug( "%s: window=%p, user_data=%p",
			thisfn, ( void * ) window, ( void * ) user_data );

	if( window->priv->dossier ){
		ofa_dossier_properties_run( window, window->priv->dossier );
	}
}

static void
on_dossier_properties_cleanup_handler( ofaMainWindow *window )
{
	static const gchar *thisfn = "ofa_main_window_on_dossier_properties_cleanup_handler";

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

	close_all_pages( OFA_MAIN_WINDOW( user_data ));
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
	ofaMainWindowPrivate *priv;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv = main_window->priv;

	if( !priv->dispose_has_run ){

		if( priv->dossier ){
			do_close_dossier( main_window );
		}
	}
}

static void
do_close_dossier( ofaMainWindow *self )
{
	ofaApplication *appli;
	ofaMainWindowPrivate *priv;

	priv = self->priv;

	g_clear_object( &priv->dossier );
	g_free( priv->dos_account );
	priv->dos_account = NULL;
	g_free( priv->dos_password );
	priv->dos_password = NULL;

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
on_ope_entry_page( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_guided";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_main_window_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_ENTRIES );
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
on_ope_settlement( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_settlement";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_main_window_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_SETTLEMENT );
}

static void
on_ope_ledger_close( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_ledger_close";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ledger_close_run( OFA_MAIN_WINDOW( user_data ));
}

static void
on_ope_exercice_close( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_exercice_close";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_exercice_close_assistant_run( OFA_MAIN_WINDOW( user_data ));
}

static void
on_ope_import( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_import";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_import_assistant_run( OFA_MAIN_WINDOW( user_data ));
}

static void
on_ope_export( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_export";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_export_assistant_run( OFA_MAIN_WINDOW( user_data ));
}

static void
on_render_balances( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_render_balances";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_main_window_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_RENDER_BALANCES );
}

static void
on_render_accounts_book( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_render_accounts_book";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_main_window_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_RENDER_ACCOUNTS_BOOK );
}

static void
on_render_ledgers_book( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_render_ledgers_book";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_main_window_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_RENDER_LEDGERS_BOOK );
}

static void
on_render_reconcil( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_render_reconcil";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_main_window_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_RENDER_RECONCIL );
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

static void
on_check_balances( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_misc_check_balances";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_check_balances_run( OFA_MAIN_WINDOW( user_data ));
}

static void
on_check_integrity( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_misc_check_integrity";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_check_integrity_run( OFA_MAIN_WINDOW( user_data ));
}

/**
 * ofa_main_window_get_dossier:
 */
ofoDossier *
ofa_main_window_get_dossier( const ofaMainWindow *window )
{
	ofaMainWindowPrivate *priv;

	g_return_val_if_fail( window && OFA_IS_MAIN_WINDOW( window ), NULL );

	priv = window->priv;

	if( !priv->dispose_has_run ){

		return( priv->dossier );
	}

	return( NULL );
}

/**
 * ofa_main_window_get_dossier_credentials:
 */
void
ofa_main_window_get_dossier_credentials( const ofaMainWindow *window, const gchar **account, const gchar **password )
{
	ofaMainWindowPrivate *priv;

	g_return_if_fail( window && OFA_IS_MAIN_WINDOW( window ));

	priv = window->priv;

	if( !priv->dispose_has_run ){

		*account = priv->dos_account;
		*password = priv->dos_password;
	}
}

/**
 * ofa_main_window_activate_theme:
 * @main_window: the #ofaMainWindow main window.
 * @theme: the theme identifier as defined in ofa-main-window.h.
 *
 * Activate the specified theme, creating the corresponding page if it
 * didn't exist.
 *
 * Returns: the #ofaPage corresponding to the theme.
 */
ofaPage *
ofa_main_window_activate_theme( const ofaMainWindow *main_window, gint theme )
{
	static const gchar *thisfn = "ofa_main_window_activate_theme";
	ofaMainWindowPrivate *priv;
	GtkNotebook *main_book;
	ofaPage *page;
	const sThemeDef *theme_def;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), NULL );

	page = NULL;
	priv = main_window->priv;

	if( !priv->dispose_has_run ){

		g_debug( "%s: main_window=%p, theme=%d", thisfn, ( void * ) main_window, theme );

		main_book = main_get_book( main_window );
		g_return_val_if_fail( main_book && GTK_IS_NOTEBOOK( main_book ), NULL );

		theme_def = get_theme_def_from_id( theme );
		g_return_val_if_fail( theme_def, NULL );
		g_return_val_if_fail( theme_def->fn_get_type, NULL );

		if( theme_def->if_entries_allowed ){
			if( !ofo_dossier_is_current( priv->dossier )){
				warning_archived_dossier( main_window );
				return( NULL );
			}
		}

		page = main_book_get_page( main_window, main_book, theme );
		if( !page ){
			page = main_book_create_page( main_window, main_book, theme_def );
		}
		g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );
		main_book_activate_page( main_window, main_book, page );
	}

	return( page );
}

static GtkNotebook *
main_get_book( const ofaMainWindow *window )
{
	GtkWidget *book;

	book = gtk_paned_get_child2( window->priv->pane );
	g_return_val_if_fail( GTK_IS_NOTEBOOK( book ), NULL );

	return( GTK_NOTEBOOK( book ));
}

static ofaPage *
main_book_get_page( const ofaMainWindow *window, GtkNotebook *book, gint theme )
{
	GtkWidget *page;
	gint count, i, page_thm;

	count = gtk_notebook_get_n_pages( book );
	for( i=0 ; i<count ; ++i ){
		page = gtk_notebook_get_nth_page( book, i );
		g_return_val_if_fail( OFA_IS_PAGE( page ), NULL );
		page_thm = ofa_page_get_theme( OFA_PAGE( page ));
		if( page_thm == theme ){
			return( OFA_PAGE( page ));
		}
	}

	return( NULL );
}

/*
 * the page for this theme has not been found
 * so, create it here
 */
static ofaPage *
main_book_create_page( const ofaMainWindow *main, GtkNotebook *book, const sThemeDef *theme_def )
{
	ofaPage *page;
	myTabLabel *tab;
	GtkWidget *label;

	/* the top child of the notebook page */
	page = g_object_new(( *theme_def->fn_get_type )(),
					PAGE_PROP_MAIN_WINDOW, main,
					PAGE_PROP_THEME,       theme_def->theme_id,
					NULL );


	/* the tab widget */
	tab = my_tab_label_new( NULL, gettext( theme_def->label ));
	g_signal_connect( tab, MY_SIGNAL_TAB_CLOSE_CLICKED, G_CALLBACK( on_tab_close_clicked ), page );

	/* the menu widget */
	label = gtk_label_new( gettext( theme_def->label ));
	my_utils_widget_set_xalign( label, 0 );

	gtk_notebook_append_page_menu( book, GTK_WIDGET( page ), GTK_WIDGET( tab ), label );
	gtk_notebook_set_tab_reorderable( book, GTK_WIDGET( page ), TRUE );

	return( page );
}

/*
 * ofa_main_book_activate_page:
 *
 * Activating the page mainly consists in giving the focus to the first
 * embedded treeview.
 */
static void
main_book_activate_page( const ofaMainWindow *window, GtkNotebook *book, ofaPage *page )
{
	gint page_num;
	GtkWidget *widget;

	g_return_if_fail( window && OFA_IS_MAIN_WINDOW( window ));
	g_return_if_fail( book && GTK_IS_NOTEBOOK( book ));
	g_return_if_fail( page && OFA_IS_PAGE( page ));

	gtk_widget_show_all( GTK_WIDGET( window ));

	page_num = gtk_notebook_page_num( book, GTK_WIDGET( page ));
	gtk_notebook_set_current_page( book, page_num );

	widget = ofa_page_get_top_focusable_widget( page );
	if( widget ){
		g_return_if_fail( GTK_IS_WIDGET( widget ));
		gtk_widget_grab_focus( widget );
	}
}

static void
on_tab_close_clicked( myTabLabel *tab, ofaPage *page )
{
	static const gchar *thisfn = "ofa_main_window_on_tab_close_clicked";
	const ofaMainWindow *main_window;
	GtkNotebook *book;
	gint page_num;

	g_debug( "%s: tab=%p, page=%p", thisfn, ( void * ) tab, ( void * ) page );

	main_window = ofa_page_get_main_window( page );
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	book = main_get_book( main_window );
	g_return_if_fail( book && GTK_IS_NOTEBOOK( book ));

	page_num = gtk_notebook_page_num( book, GTK_WIDGET( page ));
	g_return_if_fail( page_num >= 0 );

	gtk_notebook_remove_page( book, page_num );
}

/*
 * signal handler triggered when a page is removed from the main notebook
 * the same signal is proxied to the ofaPage
 */
static void
on_page_removed( GtkNotebook *book, GtkWidget *page_w, guint page_num, ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_main_window_on_page_removed";

	g_debug( "%s: book=%p, page_w=%p, page_num=%u, main_window=%p",
			thisfn, ( void * ) book, ( void * ) page_w, page_num, ( void * ) main_window );


	g_signal_emit_by_name( page_w, "page-removed", page_w, page_num );
}

static void
close_all_pages( ofaMainWindow *main_window )
{
	GtkNotebook *book;
	gint count;

	book = main_get_book( main_window );
	while(( count = gtk_notebook_get_n_pages( book )) > 0 ){
		gtk_notebook_remove_page( book, count-1 );
	}
}
