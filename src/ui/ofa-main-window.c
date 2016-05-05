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

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-accel-group.h"
#include "my/my-date.h"
#include "my/my-dnd-book.h"
#include "my/my-dnd-window.h"
#include "my/my-iaction-map.h"
#include "my/my-iwindow.h"
#include "my/my-tab.h"
#include "my/my-utils.h"

#include "api/ofa-dossier-prefs.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itheme-manager.h"
#include "api/ofa-page.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "core/ofa-guided-input.h"

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
#include "ui/ofa-dossier-properties.h"
#include "ui/ofa-entry-page.h"
#include "ui/ofa-exercice-close-assistant.h"
#include "ui/ofa-export-assistant.h"
#include "ui/ofa-guided-ex.h"
#include "ui/ofa-import-assistant.h"
#include "ui/ofa-ledger-close.h"
#include "ui/ofa-ledger-book-render.h"
#include "ui/ofa-ledger-page.h"
#include "ui/ofa-ledger-summary-render.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-ope-template-page.h"
#include "ui/ofa-period-close.h"
#include "ui/ofa-rate-page.h"
#include "ui/ofa-reconcil-render.h"
#include "ui/ofa-reconcil-page.h"
#include "ui/ofa-settlement-page.h"

/* private instance data
 */
typedef struct {
	gboolean         dispose_has_run;

	/* internals
	 */
	gchar           *orig_title;
	GtkGrid         *grid;
	GtkMenuBar      *menubar;
	myAccelGroup    *accel_group;

	/* when a dossier is opened
	 */
	GMenuModel      *menu;
	GtkPaned        *pane;
	guint            last_theme;
	cairo_surface_t *background_image;
	gint             background_image_width;
	gint             background_image_height;

	/* menu items enabled status
	 */
	gboolean         dossier_begin;		/* whether the exercice beginning date is valid */

	GSimpleAction   *action_guided_input;
	GSimpleAction   *action_settlement;
	GSimpleAction   *action_reconciliation;
	GSimpleAction   *action_close_ledger;
	GSimpleAction   *action_close_period;
	GSimpleAction   *action_close_exercice;
	GSimpleAction   *action_import;

	/* ofaIThemeManager interface
	 */
	GList           *themes;			/* registered themes */
}
	ofaMainWindowPrivate;

static void on_properties            ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_backup                ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_close                 ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_guided            ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_entry_page        ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_concil            ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_settlement        ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_ledger_close      ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_period_close      ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_exercice_close    ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_import            ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_export            ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_render_balances       ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_render_accounts_book  ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_render_ledgers_book   ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_render_ledgers_summary( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_render_reconcil       ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_accounts          ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_ledgers           ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_ope_templates     ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_currencies        ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_rates             ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_classes           ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_batfiles          ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_check_balances        ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_check_integrity       ( GSimpleAction *action, GVariant *parameter, gpointer user_data );

static const GActionEntry st_dos_entries[] = {
		{ "properties",             on_properties,             NULL, NULL, NULL },
		{ "backup",                 on_backup,                 NULL, NULL, NULL },
		{ "close",                  on_close,                  NULL, NULL, NULL },
		{ "guided",                 on_ope_guided,             NULL, NULL, NULL },
		{ "entries",                on_ope_entry_page,         NULL, NULL, NULL },
		{ "concil",                 on_ope_concil,             NULL, NULL, NULL },
		{ "settlement",             on_ope_settlement,         NULL, NULL, NULL },
		{ "ledclosing",             on_ope_ledger_close,       NULL, NULL, NULL },
		{ "perclosing",             on_ope_period_close,       NULL, NULL, NULL },
		{ "execlosing",             on_ope_exercice_close,     NULL, NULL, NULL },
		{ "import",                 on_ope_import,             NULL, NULL, NULL },
		{ "export",                 on_ope_export,             NULL, NULL, NULL },
		{ "render-balances",        on_render_balances,        NULL, NULL, NULL },
		{ "render-books",           on_render_accounts_book,   NULL, NULL, NULL },
		{ "render-ledgers-book",    on_render_ledgers_book,    NULL, NULL, NULL },
		{ "render-ledgers-summary", on_render_ledgers_summary, NULL, NULL, NULL },
		{ "render-reconcil",        on_render_reconcil,        NULL, NULL, NULL },
		{ "accounts",               on_ref_accounts,           NULL, NULL, NULL },
		{ "ledgers",                on_ref_ledgers,            NULL, NULL, NULL },
		{ "ope-templates",          on_ref_ope_templates,      NULL, NULL, NULL },
		{ "currencies",             on_ref_currencies,         NULL, NULL, NULL },
		{ "rates",                  on_ref_rates,              NULL, NULL, NULL },
		{ "classes",                on_ref_classes,            NULL, NULL, NULL },
		{ "batfiles",               on_ref_batfiles,           NULL, NULL, NULL },
		{ "chkbal",                 on_check_balances,         NULL, NULL, NULL },
		{ "integrity",              on_check_integrity,        NULL, NULL, NULL },
};

/* This structure handles the data needed to manage the themes.
 *
 * @type: the GType of the corresponding ofaPage,
 *  aka the theme identifier.
 *
 * @label: the theme label,
 *  aka the notebook tab title,
 *  aka the non modal window title.
 *
 * This structure is allocated in #theme_manager_define() method, but
 * cannot be used to initialize our themes (because GType is not a
 * compilation constant).
 *
 * This structure is part of ofaIThemeManager implementation.
 */
typedef struct {
	GType   type;
	gchar  *label;
}
	sThemeDef;

/* The structure used to initialize our themes
 * This structure is only part of ofaMainWindow initialization.
 * List is ordered by get_type() function name just for reference.
 */
typedef struct {
	gchar     *label;
	GType    (*fn_get_type)( void );
}
	sThemeInit;

static sThemeInit st_theme_defs[] = {
		{ N_( "Accounts book" ),         ofa_account_book_render_get_type },
		{ N_( "Chart of accounts" ),     ofa_account_page_get_type },
		{ N_( "Entries balance" ),       ofa_balance_render_get_type },
		{ N_( "Imported BAT files" ),    ofa_bat_page_get_type },
		{ N_( "Account classes" ),       ofa_class_page_get_type },
		{ N_( "Currencies" ),            ofa_currency_page_get_type },
		{ N_( "View entries" ),          ofa_entry_page_get_type },
		{ N_( "Guided input" ),          ofa_guided_ex_get_type },
		{ N_( "Ledgers book" ),          ofa_ledger_book_render_get_type },
		{ N_( "Ledgers" ),               ofa_ledger_page_get_type },
		{ N_( "Ledgers summary" ),       ofa_ledger_summary_render_get_type },
		{ N_( "Operation templates" ),   ofa_ope_template_page_get_type },
		{ N_( "Rates" ),                 ofa_rate_page_get_type },
		{ N_( "Reconciliation" ),        ofa_reconcil_page_get_type },
		{ N_( "Reconciliation Sumary" ), ofa_reconcil_render_get_type },
		{ N_( "Settlement" ),            ofa_settlement_page_get_type },
		{ 0 }
};

/* Left treeview definition.
 * For ergonomy reason, we may have here several items which points
 * to the same theme.
 * In display order.
 */
typedef struct {
	const gchar *label;
	GType     ( *fntype )( void );		/* must be a theme-registered GType */
}
	sTreeDef;

enum {
	COL_TREE_IDX = 0,					/* index of the sTreeDef definition in the array */
	COL_LABEL,							/* tree label */
	N_COLUMNS
};

static sTreeDef st_tree_defs[] = {

		{ N_( "Guided input" ),        ofa_guided_ex_get_type },
		{ N_( "Reconciliation" ),      ofa_reconcil_page_get_type },
		{ N_( "Chart of accounts" ),   ofa_account_page_get_type },
		{ N_( "Ledgers" ),             ofa_ledger_page_get_type },
		{ N_( "Operation templates" ), ofa_ope_template_page_get_type },
		{ N_( "Currencies" ),          ofa_currency_page_get_type },
		{ N_( "Rates" ),               ofa_rate_page_get_type },
		{ N_( "Account classes" ),     ofa_class_page_get_type },
		{ N_( "Imported BAT files" ),  ofa_bat_page_get_type },
		{ 0 }
};

static const gchar *st_main_window_name = "MainWindow";
static const gchar *st_resource_dosmenu = "/org/trychlos/openbook/ui/ofa-dos-menubar.ui";
static const gchar *st_dosmenu_id       = "dos-menu";
static const gchar *st_icon_fname       = ICONFNAME;

static void                  pane_save_position( GtkPaned *pane );
static void                  window_store_ref( ofaMainWindow *self, GtkBuilder *builder, const gchar *placeholder );
static void                  init_themes( ofaMainWindow *self );
static void                  hub_on_dossier_opened( ofaHub *hub, gboolean run_prefs, gboolean read_only, ofaMainWindow *self );
static void                  hub_on_dossier_closed( ofaHub *hub, ofaMainWindow *self );
static void                  hub_on_dossier_changed( ofaHub *hub, ofaMainWindow *main_window );
static void                  hub_on_dossier_preview( ofaHub *hub, const gchar *uri, ofaMainWindow *main_window );
static gboolean              on_delete_event( GtkWidget *toplevel, GdkEvent *event, gpointer user_data );
static void                  do_open_dossier( ofaMainWindow *self, ofaHub *hub, gboolean run_prefs, gboolean read_only );
static void                  do_open_run_prefs( ofaMainWindow *self, ofaHub *hub, gboolean read_only );
static void                  do_close_dossier( ofaMainWindow *self, ofaHub *hub );
static void                  menubar_setup( ofaMainWindow *window, myIActionMap *map );
static void                  set_window_title( const ofaMainWindow *window, gboolean with_dossier );
static void                  warning_exercice_unset( const ofaMainWindow *window );
static void                  pane_restore_position( GtkPaned *pane );
static void                  pane_left_add_treeview( ofaMainWindow *window );
static void                  pane_left_on_item_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainWindow *window );
static void                  pane_right_add_empty_notebook( ofaMainWindow *window, ofaHub *hub );
static void                  background_image_update( ofaMainWindow *self, ofaHub *hub );
static void                  background_image_set_uri( ofaMainWindow *self, const gchar *uri );
static void                  do_update_menubar_items( ofaMainWindow *self );
static void                  enable_action_guided_input( ofaMainWindow *window, gboolean enable );
static void                  enable_action_settlement( ofaMainWindow *window, gboolean enable );
static void                  enable_action_reconciliation( ofaMainWindow *window, gboolean enable );
static void                  enable_action_close_ledger( ofaMainWindow *window, gboolean enable );
static void                  enable_action_close_period( ofaMainWindow *window, gboolean enable );
static void                  enable_action_close_exercice( ofaMainWindow *window, gboolean enable );
static void                  enable_action_import( ofaMainWindow *window, gboolean enable );
static void                  do_backup( ofaMainWindow *self );
static void                  do_properties( const ofaMainWindow *self );
static GtkNotebook          *notebook_get_book( const ofaMainWindow *window );
static ofaPage              *notebook_get_page( const ofaMainWindow *window, GtkNotebook *book, const sThemeDef *def );
static ofaPage              *notebook_create_page( const ofaMainWindow *main, GtkNotebook *book, const sThemeDef *def );
static void                  book_attach_page( const ofaMainWindow *main, GtkNotebook *book, GtkWidget *page, const gchar *title );
static void                  notebook_activate_page( const ofaMainWindow *window, GtkNotebook *book, ofaPage *page );
static gboolean              notebook_on_draw( GtkWidget *widget, cairo_t *cr, ofaMainWindow *self );
static gboolean              book_on_append_page( myDndBook *book, GtkWidget *page, const gchar *title, ofaMainWindow *self );
static void                  on_tab_close_clicked( myTab *tab, ofaPage *page );
static void                  do_close( ofaPage *page );
static void                  on_page_removed( GtkNotebook *book, GtkWidget *page, guint page_num, ofaMainWindow *self );
static void                  close_all_pages( ofaMainWindow *self );
static void                  igetter_iface_init( ofaIGetterInterface *iface );
static ofaIGetter           *igetter_get_permanent_getter( const ofaIGetter *instance );
static GApplication         *igetter_get_application( const ofaIGetter *instance );
static ofaHub               *igetter_get_hub( const ofaIGetter *instance );
static GtkApplicationWindow *igetter_get_main_window( const ofaIGetter *instance );
static ofaIThemeManager     *igetter_get_theme_manager( const ofaIGetter *instance );
static void                  itheme_manager_iface_init( ofaIThemeManagerInterface *iface );
static void                  itheme_manager_define( ofaIThemeManager *instance, GType type, const gchar *label );
static ofaPage              *itheme_manager_activate( ofaIThemeManager *instance, GType type );
static sThemeDef            *theme_get_by_type( GList **list, GType type );
static void                  theme_free( sThemeDef *def );
static void                  iaction_map_iface_init( myIActionMapInterface *iface );
static GMenuModel           *iaction_map_get_menu_model( const myIActionMap *instance );
static const gchar          *iaction_map_get_action_target( const myIActionMap *instance );

G_DEFINE_TYPE_EXTENDED( ofaMainWindow, ofa_main_window, GTK_TYPE_APPLICATION_WINDOW, 0,
		G_ADD_PRIVATE( ofaMainWindow )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IACTION_MAP, iaction_map_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IGETTER, igetter_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ITHEME_MANAGER, itheme_manager_iface_init ))

static void
main_window_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_main_window_finalize";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MAIN_WINDOW( instance ));

	/* free data members here */
	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( instance ));

	g_free( priv->orig_title );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_main_window_parent_class )->finalize( instance );
}

static void
main_window_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofa_main_window_dispose";
	ofaMainWindowPrivate *priv;
	myISettings *settings;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_if_fail( instance && OFA_IS_MAIN_WINDOW( instance ));

	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		settings = ofa_settings_get_settings( SETTINGS_TARGET_USER );
		my_utils_window_position_save( GTK_WINDOW( instance ), settings, st_main_window_name );
		if( priv->pane ){
			pane_save_position( priv->pane );
		}
		close_all_pages( OFA_MAIN_WINDOW( instance ));
		my_iwindow_close_all();

		/* unref object members here */
		g_clear_object( &priv->menu );
		g_list_free_full( priv->themes, ( GDestroyNotify ) theme_free );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_main_window_parent_class )->dispose( instance );
}

static void
pane_save_position( GtkPaned *pane )
{
	gchar *key;

	key = g_strdup_printf( "%s-pane", st_main_window_name );
	ofa_settings_user_set_uint( key, gtk_paned_get_position( pane ));
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
	myISettings *settings;

	g_return_if_fail( instance && OFA_IS_MAIN_WINDOW( instance ));

	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( instance ));

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
		builder = gtk_builder_new_from_resource( st_resource_dosmenu );
		menu = G_MENU_MODEL( gtk_builder_get_object( builder, st_dosmenu_id ));
		if( menu ){
			priv->menu = g_object_ref( menu );
			g_debug( "%s: menu successfully loaded from %s at %p: items=%d",
					thisfn, st_resource_dosmenu, ( void * ) menu, g_menu_model_get_n_items( menu ));

			/* store the references to the plugins placeholders */
			window_store_ref( OFA_MAIN_WINDOW( instance ), builder, "plugins_app_dossier" );
			window_store_ref( OFA_MAIN_WINDOW( instance ), builder, "plugins_win_ope1" );
			window_store_ref( OFA_MAIN_WINDOW( instance ), builder, "plugins_win_ope2" );
			window_store_ref( OFA_MAIN_WINDOW( instance ), builder, "plugins_win_ope3" );
			window_store_ref( OFA_MAIN_WINDOW( instance ), builder, "plugins_win_ope4" );
			window_store_ref( OFA_MAIN_WINDOW( instance ), builder, "plugins_win_print" );
			window_store_ref( OFA_MAIN_WINDOW( instance ), builder, "plugins_win_ref" );
			window_store_ref( OFA_MAIN_WINDOW( instance ), builder, "plugins_app_misc" );

		} else {
			g_warning( "%s: unable to find '%s' object in '%s' resource", thisfn, st_dosmenu_id, st_resource_dosmenu );
		}
		g_object_unref( builder );

		/* application (GtkWindow's property) is not set here because
		 * this is not a 'construct' property */

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

		settings = ofa_settings_get_settings( SETTINGS_TARGET_USER );
		my_utils_window_position_restore( GTK_WINDOW( instance), settings, st_main_window_name );

		/* connect some signals
		 */
		g_signal_connect( instance, "delete-event", G_CALLBACK( on_delete_event ), NULL );

		/* set the default icon for all windows of the application */
		error = NULL;
		gtk_window_set_default_icon_from_file( st_icon_fname, &error );
		if( error ){
			g_warning( "%s: %s", thisfn, error->message );
			g_error_free( error );
		}
	}
}

/*
 * @main_window:
 * @builder:
 * @placeholder: the name of an object inserted in the menu definition;
 *  this same name will be set against @main_window GObject pointing to
 *  the menu definition.
 *
 * Makes a new association between @placeholder and the object found in
 * the @builder. This association may later be used by plugins and other
 * add-ons to insert their own menu items at @placeholder places.
 */
static void
window_store_ref( ofaMainWindow *main_window, GtkBuilder *builder, const gchar *placeholder )
{
	static const gchar *thisfn = "ofa_main_window_window_store_ref";
	GObject *menu;

	menu = gtk_builder_get_object( builder, placeholder );
	if( !menu ){
		g_warning( "%s: unable to find '%s' placeholder", thisfn, placeholder );
	} else {
		g_object_set_data( G_OBJECT( main_window ), placeholder, menu );
	}
}

static void
ofa_main_window_init( ofaMainWindow *self )
{
	static const gchar *thisfn = "ofa_main_window_init";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MAIN_WINDOW( self ));

	priv = ofa_main_window_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->last_theme = THM_LAST_THEME;

	my_iaction_map_register( MY_IACTION_MAP( self ));
}

static void
ofa_main_window_class_init( ofaMainWindowClass *klass )
{
	static const gchar *thisfn = "ofa_main_window_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->constructed = main_window_constructed;
	G_OBJECT_CLASS( klass )->dispose = main_window_dispose;
	G_OBJECT_CLASS( klass )->finalize = main_window_finalize;
}

/**
 * Returns a newly allocated ofaMainWindow object.
 */
ofaMainWindow *
ofa_main_window_new( ofaApplication *application )
{
	static const gchar *thisfn = "ofa_main_window_new";
	ofaMainWindow *window;
	ofaMainWindowPrivate *priv;
	ofaHub *hub;

	g_return_val_if_fail( application && OFA_IS_APPLICATION( application ), NULL );

	g_debug( "%s: application=%p", thisfn, application );

	/* 'application' is a GtkWindow property
	 *  because it is not defined as a construction property, it is only
	 *  available after g_object_new() has returned */
	window = g_object_new( OFA_TYPE_MAIN_WINDOW, "application", application, NULL );

	priv = ofa_main_window_get_instance_private( window );

	/* connect to the ofaHub signals
	 */
	hub = ofa_igetter_get_hub( OFA_IGETTER( window ));
	g_signal_connect( hub, SIGNAL_HUB_DOSSIER_OPENED, G_CALLBACK( hub_on_dossier_opened ), window );
	g_signal_connect( hub, SIGNAL_HUB_DOSSIER_CLOSED, G_CALLBACK( hub_on_dossier_closed ), window );
	g_signal_connect( hub, SIGNAL_HUB_DOSSIER_CHANGED, G_CALLBACK( hub_on_dossier_changed ), window );
	g_signal_connect( hub, SIGNAL_HUB_DOSSIER_PREVIEW, G_CALLBACK( hub_on_dossier_preview ), window );

	/* let the plugins update these menu map/model
	 * (here because application is not yet set in constructed() */
	g_signal_emit_by_name(( gpointer ) application, "menu-available", window, "win" );

	/* let the plugins update the managed themes */
	init_themes( window );

	g_object_get( G_OBJECT( application ), OFA_PROP_APPLICATION_NAME, &priv->orig_title, NULL );

	menubar_setup( window, MY_IACTION_MAP( application ));

	return( window );
}

/*
 * the main window initialization of the theme manager:
 * - define the themes for the main window
 * - then declare the theme manager general availability
 */
static void
init_themes( ofaMainWindow *self )
{
	gint i;
	GtkApplication *application;

	/* define the themes for the main window */
	for( i=0 ; st_theme_defs[i].label ; ++i ){
		ofa_itheme_manager_define(
				OFA_ITHEME_MANAGER( self ),
				( *st_theme_defs[i].fn_get_type )(), gettext( st_theme_defs[i].label ));
	}

	/* declare then the theme manager general availability */
	application = gtk_window_get_application( GTK_WINDOW( self ));
	g_return_if_fail( application && OFA_IS_APPLICATION( application ));
	g_signal_emit_by_name( application, "theme-available", self );
}

static void
hub_on_dossier_opened( ofaHub *hub, gboolean run_prefs, gboolean read_only, ofaMainWindow *self )
{
	do_open_dossier( self, hub, run_prefs, read_only );
}

static void
hub_on_dossier_closed( ofaHub *hub, ofaMainWindow *self )
{
	do_close_dossier( self, hub );
}

/*
 * the dossier has advertized the hub that its properties has been
 * modified (or may have been modified) by the user
 */
static void
hub_on_dossier_changed( ofaHub *hub, ofaMainWindow *self )
{
	set_window_title( self, TRUE );
	do_update_menubar_items( self );
	background_image_update( self, hub );
}

/*
 * set a background image
 */
static void
hub_on_dossier_preview( ofaHub *hub, const gchar *uri, ofaMainWindow *self )
{
	background_image_set_uri( self, uri );
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

static void
do_open_dossier( ofaMainWindow *self, ofaHub *hub, gboolean run_prefs, gboolean read_only )
{
	ofaMainWindowPrivate *priv;
	const GDate *exe_begin, *exe_end;
	ofoDossier *dossier;

	priv = ofa_main_window_get_instance_private( self );

	priv->pane = GTK_PANED( gtk_paned_new( GTK_ORIENTATION_HORIZONTAL ));
	gtk_grid_attach( priv->grid, GTK_WIDGET( priv->pane ), 0, 1, 1, 1 );
	pane_restore_position( priv->pane );
	pane_left_add_treeview( self );
	pane_right_add_empty_notebook( self, hub );
	background_image_update( self, hub );

	menubar_setup( self, MY_IACTION_MAP( self ));

	/* warns if begin or end of exercice is not set */
	dossier = ofa_hub_get_dossier( hub );
	exe_begin = ofo_dossier_get_exe_begin( dossier );
	exe_end = ofo_dossier_get_exe_end( dossier );
	if( !my_date_is_valid( exe_begin ) || !my_date_is_valid( exe_end )){
		warning_exercice_unset( self );
	}

	g_signal_emit_by_name( hub, SIGNAL_HUB_DOSSIER_CHANGED );

	if( run_prefs ){
		do_open_run_prefs( self, hub, read_only );
	}
}

/**
 * ofa_main_window_dossier_run_prefs:
 * @main_window: this #ofaMainWindow instance.
 *
 * Run the user preferences.
 */
void
ofa_main_window_dossier_run_prefs( ofaMainWindow *main_window )
{
	ofaHub *hub;
	gboolean read_only;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	hub = igetter_get_hub( OFA_IGETTER( main_window ));
	read_only = !ofa_hub_dossier_is_writable( hub );

	do_open_run_prefs( main_window, hub, read_only );
}

static void
do_open_run_prefs( ofaMainWindow *self, ofaHub *hub, gboolean read_only )
{
	static const gchar *thisfn = "ofa_main_window_do_open_run_prefs";
	const gchar *main_notes, *exe_notes;
	ofoDossier *dossier;
	ofaDossierPrefs *prefs;
	gboolean empty, user_prefs_non_empty, dossier_prefs_non_empty;

	dossier = ofa_hub_get_dossier( hub );
	prefs = ofa_hub_dossier_get_prefs( hub );

	/* display dossier notes ? */
	if( ofa_prefs_dossier_open_notes() || ofa_dossier_prefs_get_open_notes( prefs )){

		main_notes = ofo_dossier_get_notes( dossier );
		exe_notes = ofo_dossier_get_exe_notes( dossier );
		empty = my_strlen( main_notes ) == 0 && my_strlen( exe_notes ) == 0;
		user_prefs_non_empty = ofa_prefs_dossier_open_notes() && ofa_prefs_dossier_open_notes_if_empty();
		dossier_prefs_non_empty = ofa_dossier_prefs_get_open_notes( prefs ) && ofa_dossier_prefs_get_nonempty( prefs );

		g_debug( "%s: empty=%s, user_prefs_non_empty=%s, dossier_prefs_non_empty=%s",
				thisfn, empty ? "True":"False",
				user_prefs_non_empty ? "True":"False",
				dossier_prefs_non_empty ? "True":"False" );

		if( !empty || ( !user_prefs_non_empty && !dossier_prefs_non_empty )){
			ofa_dossier_display_notes_run( OFA_IGETTER( self ), GTK_WINDOW( self ), main_notes, exe_notes );
		}
	}

	/* check balances and DBMS integrity*/
	if( ofa_prefs_dossier_open_balance() || ofa_dossier_prefs_get_balances( prefs )){
		ofa_check_balances_run( OFA_IGETTER( self ), GTK_WINDOW( self ));
	}
	if( ofa_prefs_dossier_open_integrity() || ofa_dossier_prefs_get_integrity( prefs )){
		ofa_check_integrity_run( OFA_IGETTER( self ), GTK_WINDOW( self ));
	}

	/* display dossier properties */
	if( ofa_prefs_dossier_open_properties() || ofa_dossier_prefs_get_properties( prefs )){
		do_properties( self );
	}
}

/*
 * this function may be executed after delete_event handler,
 * so after the main window has actually been destroyed
 */
static void
do_close_dossier( ofaMainWindow *self, ofaHub *hub )
{
	static const gchar *thisfn = "ofa_main_window_do_close_dossier";
	ofaMainWindowPrivate *priv;
	GtkApplication *application;

	g_debug( "%s: self=%p, hub=%p", thisfn, ( void * ) self, ( void * ) hub );

	if( GTK_IS_WINDOW( self ) && ofa_hub_get_dossier( hub )){

		close_all_pages( self );
		my_iwindow_close_all();

		priv = ofa_main_window_get_instance_private( self );

		if( priv->background_image ){
			cairo_surface_destroy( priv->background_image );
			priv->background_image = NULL;
		}

		gtk_widget_destroy( GTK_WIDGET( priv->pane ));
		priv->pane = NULL;

		application = gtk_window_get_application( GTK_WINDOW( self ));
		menubar_setup( self, MY_IACTION_MAP( application ));

		set_window_title( self, FALSE );
	}
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
ofa_main_window_is_willing_to_quit( const ofaMainWindow *main_window )
{
	return( my_utils_dialog_question(
			_( "Are you sure you want to quit the application ?" ), _( "_Quit" )));
}

static void
menubar_setup( ofaMainWindow *window, myIActionMap *map )
{
	static const gchar *thisfn = "ofa_main_window_menubar_setup";
	ofaMainWindowPrivate *priv;
	GtkWidget *menubar;
	GMenuModel *model;

	priv = ofa_main_window_get_instance_private( window );

	if( priv->menubar ){
		gtk_widget_destroy( GTK_WIDGET( priv->menubar ));
		priv->menubar = NULL;
	}

	if( priv->accel_group ){
		gtk_window_remove_accel_group( GTK_WINDOW( window ), GTK_ACCEL_GROUP( priv->accel_group ));
		g_object_unref( priv->accel_group );
		priv->accel_group = NULL;
	}

	priv->accel_group = my_accel_group_new();
	my_accel_group_setup_accels( priv->accel_group, map );
	gtk_window_add_accel_group( GTK_WINDOW( window ), GTK_ACCEL_GROUP( priv->accel_group ));

	model = my_iaction_map_get_menu_model( map );
	menubar = gtk_menu_bar_new_from_model( model );

	g_debug( "%s: model=%p (%s), menubar=%p, grid=%p (%s)",
			thisfn,
			( void * ) model, G_OBJECT_TYPE_NAME( model ), ( void * ) menubar,
			( void * ) priv->grid, G_OBJECT_TYPE_NAME( priv->grid ));

	gtk_grid_attach( priv->grid, menubar, 0, 0, 1, 1 );
	priv->menubar = GTK_MENU_BAR( menubar );
	gtk_widget_show_all( GTK_WIDGET( window ));
}

/*
 * do not rely on the ofa_hub_get_dossier() here
 * because it has not yet been reset when closing the dossier
 */
static void
set_window_title( const ofaMainWindow *self, gboolean with_dossier )
{
	ofaMainWindowPrivate *priv;
	ofaHub *hub;
	ofoDossier *dossier;
	const ofaIDBConnect *connect;
	ofaIDBMeta *meta;
	ofaIDBPeriod *period;
	gchar *title, *dos_name, *period_label, *period_name;

	priv = ofa_main_window_get_instance_private( self );

	hub = ofa_igetter_get_hub( OFA_IGETTER( self ));
	dossier = with_dossier ? ofa_hub_get_dossier( hub ) : NULL;

	if( dossier ){
		connect = ofa_hub_get_connect( hub );
		meta = ofa_idbconnect_get_meta( connect );
		period = ofa_idbconnect_get_period( connect );
		dos_name = ofa_idbmeta_get_dossier_name( meta );
		period_label = ofa_idbperiod_get_label( period );
		period_name = ofa_idbperiod_get_name( period );

		title = g_strdup_printf( "%s (%s) %s - %s",
				dos_name,
				period_name,
				period_label,
				priv->orig_title );

		g_free( period_name );
		g_free( period_label );
		g_free( dos_name );
		g_object_unref( period );
		g_object_unref( meta );

	} else {
		title = g_strdup( priv->orig_title );
	}

	gtk_window_set_title( GTK_WINDOW( self ), title );
	g_free( title );
}

/*
 * warning_exercice_unset:
 */
static void
warning_exercice_unset( const ofaMainWindow *self )
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

	gtk_dialog_add_button( GTK_DIALOG( dialog ), "Dossier _properties...", 1 );

	g_free( str );
	resp = gtk_dialog_run( GTK_DIALOG( dialog ));
	gtk_widget_destroy( dialog );

	if( resp == 1 ){
		do_properties( self );
	}
}

static void
pane_restore_position( GtkPaned *pane )
{
	gchar *key;
	gint pos;

	key = g_strdup_printf( "%s-pane", st_main_window_name );
	pos = ofa_settings_user_get_uint( key );
	if( pos <= 10 ){
		pos = 150;
	}
	gtk_paned_set_position( pane, pos );
	g_free( key );
}

static void
pane_left_add_treeview( ofaMainWindow *window )
{
	ofaMainWindowPrivate *priv;
	GtkFrame *frame;
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	gint i;

	priv = ofa_main_window_get_instance_private( window );

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	my_utils_widget_set_margins( GTK_WIDGET( frame ), 4, 4, 4, 2 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );
	gtk_paned_pack1( priv->pane, GTK_WIDGET( frame ), FALSE, FALSE );

	view = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_hexpand( GTK_WIDGET( view ), FALSE );
	gtk_widget_set_vexpand( GTK_WIDGET( view ), TRUE );
	gtk_tree_view_set_headers_visible( view, FALSE );
	gtk_tree_view_set_activate_on_single_click( view, FALSE );
	g_signal_connect(G_OBJECT( view ), "row-activated", G_CALLBACK( pane_left_on_item_activated ), window );

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

static void
pane_left_on_item_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainWindow *window )
{
	static const gchar *thisfn = "ofa_main_window_pane_left_on_item_activated";
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint idx;

	g_debug( "%s: view=%p, path=%p, column=%p, window=%p",
			thisfn, ( void * ) view, ( void * ) path, ( void * ) column, ( void * ) window );

	model = gtk_tree_view_get_model( view );

	if( gtk_tree_model_get_iter( model, &iter, path )){
		gtk_tree_model_get( model, &iter, COL_TREE_IDX, &idx, -1 );

		ofa_itheme_manager_activate( OFA_ITHEME_MANAGER( window ), ( *st_tree_defs[idx].fntype )());
	}
}

static void
pane_right_add_empty_notebook( ofaMainWindow *self, ofaHub *hub )
{
	ofaMainWindowPrivate *priv;
	myDndBook *book;

	priv = ofa_main_window_get_instance_private( self );

	book = my_dnd_book_new();
	my_utils_widget_set_margins( GTK_WIDGET( book ), 4, 4, 2, 4 );
	gtk_notebook_set_scrollable( GTK_NOTEBOOK( book ), TRUE );
	gtk_notebook_popup_enable( GTK_NOTEBOOK( book ));
	gtk_widget_set_hexpand( GTK_WIDGET( book ), TRUE );
	gtk_widget_set_vexpand( GTK_WIDGET( book ), TRUE );

	g_signal_connect( book, "draw", G_CALLBACK( notebook_on_draw ), self );
	g_signal_connect( book, "page-removed", G_CALLBACK( on_page_removed ), self );
	g_signal_connect( book, "my-append-page", G_CALLBACK( book_on_append_page ), self );

	gtk_paned_pack2( priv->pane, GTK_WIDGET( book ), TRUE, FALSE );
}

static void
background_image_update( ofaMainWindow *self, ofaHub *hub )
{
	ofaDossierPrefs *prefs;
	gchar *background_uri;

	prefs = ofa_hub_dossier_get_prefs( hub );
	background_uri = ofa_dossier_prefs_get_background_img( prefs );
	background_image_set_uri( self, background_uri );
	g_free( background_uri );
}

static void
background_image_set_uri( ofaMainWindow *self, const gchar *uri )
{
	static const gchar *thisfn = "ofa_main_window_background_image_set_uri";
	ofaMainWindowPrivate *priv;
	gchar *filename;
	GtkNotebook *book;
	cairo_surface_t *surface;
	gint width, height;

	priv = ofa_main_window_get_instance_private( self );

	if( priv->background_image ){
		cairo_surface_destroy( priv->background_image );
		priv->background_image = NULL;
	}

	if( my_strlen( uri )){
		filename = g_filename_from_uri( uri, NULL, NULL );
		surface = cairo_image_surface_create_from_png( filename );
		g_free( filename );
		width = cairo_image_surface_get_width( surface );
		height = cairo_image_surface_get_height ( surface );

		if( width > 0 && height > 0 ){
			priv->background_image = surface;
			priv->background_image_width = width;
			priv->background_image_height = height;
			g_debug( "%s: uri=%s, width=%d, height=%d",
					thisfn, uri, priv->background_image_width, priv->background_image_height );

		} else {
			cairo_surface_destroy( surface );
			g_debug( "%s: unable to load %s", thisfn, uri );
		}
	}

	book = notebook_get_book( self );
	gtk_widget_queue_draw( GTK_WIDGET( book ));
}

static void
do_update_menubar_items( ofaMainWindow *self )
{
	ofaHub *hub;
	gboolean is_writable;

	hub = ofa_igetter_get_hub( OFA_IGETTER( self ));
	is_writable = ofa_hub_dossier_is_writable( hub );

	enable_action_guided_input( self, is_writable );
	enable_action_settlement( self, is_writable );
	enable_action_reconciliation( self, is_writable );
	enable_action_close_ledger( self, is_writable );
	enable_action_close_period( self, is_writable );
	enable_action_close_exercice( self, is_writable );
	enable_action_import( self, is_writable );
}

static void
enable_action_guided_input( ofaMainWindow *window, gboolean enable )
{
	ofaMainWindowPrivate *priv;

	priv = ofa_main_window_get_instance_private( window );

	my_utils_action_enable( G_ACTION_MAP( window ), &priv->action_guided_input, "guided", enable );
}

static void
enable_action_settlement( ofaMainWindow *window, gboolean enable )
{
	ofaMainWindowPrivate *priv;

	priv = ofa_main_window_get_instance_private( window );

	my_utils_action_enable( G_ACTION_MAP( window ), &priv->action_settlement, "settlement", enable );
}

static void
enable_action_reconciliation( ofaMainWindow *window, gboolean enable )
{
	ofaMainWindowPrivate *priv;

	priv = ofa_main_window_get_instance_private( window );

	my_utils_action_enable( G_ACTION_MAP( window ), &priv->action_reconciliation, "concil", enable );
}

static void
enable_action_close_ledger( ofaMainWindow *window, gboolean enable )
{
	ofaMainWindowPrivate *priv;

	priv = ofa_main_window_get_instance_private( window );

	my_utils_action_enable( G_ACTION_MAP( window ), &priv->action_close_ledger, "ledclosing", enable );
}

static void
enable_action_close_period( ofaMainWindow *window, gboolean enable )
{
	ofaMainWindowPrivate *priv;

	priv = ofa_main_window_get_instance_private( window );

	my_utils_action_enable( G_ACTION_MAP( window ), &priv->action_close_period, "perclosing", enable );
}

static void
enable_action_close_exercice( ofaMainWindow *window, gboolean enable )
{
	ofaMainWindowPrivate *priv;

	priv = ofa_main_window_get_instance_private( window );

	my_utils_action_enable( G_ACTION_MAP( window ), &priv->action_close_exercice, "execlosing", enable );
}

static void
enable_action_import( ofaMainWindow *window, gboolean enable )
{
	ofaMainWindowPrivate *priv;

	priv = ofa_main_window_get_instance_private( window );

	my_utils_action_enable( G_ACTION_MAP( window ), &priv->action_import, "import", enable );
}

/**
 * ofa_main_window_dossier_backup:
 * @main_window: this #ofaMainWindow instance.
 *
 * Backup the currently opened dossier.
 */
void
ofa_main_window_dossier_backup( ofaMainWindow *main_window )
{
	ofaMainWindowPrivate *priv;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv = ofa_main_window_get_instance_private( main_window );

	g_return_if_fail( !priv->dispose_has_run );

	do_backup( main_window );
}

static void
on_backup( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_backup";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	do_backup( OFA_MAIN_WINDOW( user_data ));
}

static void
do_backup( ofaMainWindow *self )
{
	close_all_pages( self );
	ofa_backup_run( OFA_IGETTER( self ), GTK_WINDOW( self ));
}

/**
 * ofa_main_window_dossier_properties:
 * @main_window: this #ofaMainWindow instance.
 *
 * Display the Properties dialog box.
 */
void
ofa_main_window_dossier_properties( ofaMainWindow *main_window )
{
	ofaMainWindowPrivate *priv;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv = ofa_main_window_get_instance_private( main_window );

	g_return_if_fail( !priv->dispose_has_run );

	do_properties( main_window );
}

static void
on_properties( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_properties";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	do_properties( OFA_MAIN_WINDOW( user_data ));
}

static void
do_properties( const ofaMainWindow *self )
{
	ofa_dossier_properties_run( OFA_IGETTER( self ), GTK_WINDOW( self ));
}

static void
on_close( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_close";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_hub_dossier_close( ofa_igetter_get_hub( OFA_IGETTER( user_data )));
}

static void
on_ope_guided( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_guided";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_itheme_manager_activate( OFA_ITHEME_MANAGER( user_data ), OFA_TYPE_OPE_TEMPLATE_PAGE );
}

static void
on_ope_entry_page( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_guided";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_itheme_manager_activate( OFA_ITHEME_MANAGER( user_data ), OFA_TYPE_ENTRY_PAGE );
}

static void
on_ope_concil( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_concil";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_itheme_manager_activate( OFA_ITHEME_MANAGER( user_data ), OFA_TYPE_RECONCIL_PAGE );
}

static void
on_ope_settlement( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_settlement";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_itheme_manager_activate( OFA_ITHEME_MANAGER( user_data ), OFA_TYPE_SETTLEMENT_PAGE );
}

static void
on_ope_ledger_close( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_ledger_close";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ledger_close_run( OFA_IGETTER( user_data ), GTK_WINDOW( user_data ));
}

static void
on_ope_period_close( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_period_close";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_period_close_run( OFA_IGETTER( user_data ), GTK_WINDOW( user_data ));
}

static void
on_ope_exercice_close( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_exercice_close";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_exercice_close_assistant_run( OFA_IGETTER( user_data ), GTK_WINDOW( user_data ));
}

static void
on_ope_import( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_import";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_import_assistant_run( OFA_IGETTER( user_data ), GTK_WINDOW( user_data ));
}

static void
on_ope_export( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_export";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_export_assistant_run( OFA_IGETTER( user_data ), GTK_WINDOW( user_data ));
}

static void
on_render_balances( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_render_balances";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_itheme_manager_activate( OFA_ITHEME_MANAGER( user_data ), OFA_TYPE_BALANCE_RENDER );
}

static void
on_render_accounts_book( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_render_accounts_book";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_itheme_manager_activate( OFA_ITHEME_MANAGER( user_data ), OFA_TYPE_ACCOUNT_BOOK_RENDER );
}

static void
on_render_ledgers_book( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_render_ledgers_book";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_itheme_manager_activate( OFA_ITHEME_MANAGER( user_data ), OFA_TYPE_LEDGER_BOOK_RENDER );
}

static void
on_render_ledgers_summary( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_render_ledgers_summary";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_itheme_manager_activate( OFA_ITHEME_MANAGER( user_data ), OFA_TYPE_LEDGER_SUMMARY_RENDER );
}

static void
on_render_reconcil( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_render_reconcil";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_itheme_manager_activate( OFA_ITHEME_MANAGER( user_data ), OFA_TYPE_RECONCIL_RENDER );
}

static void
on_ref_accounts( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_accounts";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_itheme_manager_activate( OFA_ITHEME_MANAGER( user_data ), OFA_TYPE_ACCOUNT_PAGE );
}

static void
on_ref_ledgers( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_ledgers";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_itheme_manager_activate( OFA_ITHEME_MANAGER( user_data ), OFA_TYPE_LEDGER_PAGE );
}

static void
on_ref_ope_templates( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_models";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_itheme_manager_activate( OFA_ITHEME_MANAGER( user_data ), OFA_TYPE_OPE_TEMPLATE_PAGE );
}

static void
on_ref_currencies( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_devises";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_itheme_manager_activate( OFA_ITHEME_MANAGER( user_data ), OFA_TYPE_CURRENCY_PAGE );
}

static void
on_ref_rates( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_rates";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_itheme_manager_activate( OFA_ITHEME_MANAGER( user_data ), OFA_TYPE_RATE_PAGE );
}

static void
on_ref_classes( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_classes";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_itheme_manager_activate( OFA_ITHEME_MANAGER( user_data ), OFA_TYPE_CLASS_PAGE );
}

static void
on_ref_batfiles( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_batfiles";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_itheme_manager_activate( OFA_ITHEME_MANAGER( user_data ), OFA_TYPE_BAT_PAGE );
}

static void
on_check_balances( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_misc_check_balances";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_check_balances_run( OFA_IGETTER( user_data ), GTK_WINDOW( user_data ));
}

static void
on_check_integrity( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_misc_check_integrity";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_check_integrity_run( OFA_IGETTER( user_data ), GTK_WINDOW( user_data ));
}

static GtkNotebook *
notebook_get_book( const ofaMainWindow *window )
{
	ofaMainWindowPrivate *priv;
	GtkWidget *book;

	priv = ofa_main_window_get_instance_private( window );

	book = NULL;

	if( priv->pane ){
		book = gtk_paned_get_child2( priv->pane );
		g_return_val_if_fail( book && GTK_IS_NOTEBOOK( book ), NULL );
		return( GTK_NOTEBOOK( book ));
	}

	return( NULL );
}

static ofaPage *
notebook_get_page( const ofaMainWindow *window, GtkNotebook *book, const sThemeDef *def )
{
	GtkWidget *page;
	gint count, i;

	count = gtk_notebook_get_n_pages( book );
	for( i=0 ; i<count ; ++i ){
		page = gtk_notebook_get_nth_page( book, i );
		g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );
		if( G_OBJECT_TYPE( page ) == def->type ){
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
notebook_create_page( const ofaMainWindow *main, GtkNotebook *book, const sThemeDef *def )
{
	ofaPage *page;
	const gchar *title;

	page = g_object_new( def->type, PAGE_PROP_GETTER, main, NULL );
	title = gettext( def->label );

	book_attach_page( main, book, GTK_WIDGET( page ), title );

	return( page );
}

static void
book_attach_page( const ofaMainWindow *main, GtkNotebook *book, GtkWidget *page, const gchar *title )
{
	myTab *tab;
	GtkWidget *label;

	/* the tab widget */
	tab = my_tab_new( NULL, title );

	my_tab_set_show_close( tab, TRUE );
	g_signal_connect( tab, MY_SIGNAL_TAB_CLOSE_CLICKED, G_CALLBACK( on_tab_close_clicked ), page );

	my_tab_set_show_detach( tab, FALSE );

	/* the menu widget */
	label = gtk_label_new( title );
	my_utils_widget_set_xalign( label, 0 );

	gtk_notebook_append_page_menu( book, page, GTK_WIDGET( tab ), label );
	gtk_notebook_set_tab_reorderable( book, page, TRUE );
}

/*
 * ofa_notebook_activate_page:
 *
 * Activating the page mainly consists in giving the focus to the first
 * embedded treeview.
 */
static void
notebook_activate_page( const ofaMainWindow *window, GtkNotebook *book, ofaPage *page )
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

/*
 * notebook 'draw' signal handler
 *
 * Returns: TRUE to stop other handlers from being invoked for the event.
 *  % FALSE to propagate the event further.
 */
static gboolean
notebook_on_draw( GtkWidget *widget, cairo_t *cr, ofaMainWindow *self )
{
	ofaMainWindowPrivate *priv;
	gint width, height;
	gdouble sx, sy;

	priv = ofa_main_window_get_instance_private( self );

	if( priv->background_image ){
		width = gtk_widget_get_allocated_width( widget );
		height = gtk_widget_get_allocated_height( widget );
		sx = ( gdouble ) width / ( gdouble ) priv->background_image_width;
		sy = ( gdouble ) height / ( gdouble ) priv->background_image_height;
		//g_debug( "on_draw: width=%d, height=%d, sx=%lf, sy=%lf", width, height, sx, sy );
		cairo_scale( cr, sx, sy );
		cairo_set_source_surface( cr, priv->background_image, 0, 0 );
		cairo_paint( cr );
	}

	return( FALSE );
}

/*
 * Returns: %TRUE to show that we have handled the signal.
 */
static gboolean
book_on_append_page( myDndBook *book, GtkWidget *page, const gchar *title, ofaMainWindow *self )
{
	static const gchar *thisfn = "ofa_main_window_book_on_append_page";

	g_debug( "%s: book=%p, page=%p, title=%s, self=%p",
			thisfn, ( void * ) book, ( void * ) page, title, ( void * ) self );

	book_attach_page( self, GTK_NOTEBOOK( book ), page, title );

	return( TRUE );
}

static void
on_tab_close_clicked( myTab *tab, ofaPage *page )
{
	static const gchar *thisfn = "ofa_main_window_on_tab_close_clicked";

	g_debug( "%s: tab=%p, page=%p", thisfn, ( void * ) tab, ( void * ) page );

	do_close( page );
}

static void
do_close( ofaPage *page )
{
	GtkApplicationWindow *main_window;
	GtkNotebook *book;
	gint page_num;

	main_window = ofa_igetter_get_main_window( OFA_IGETTER( page ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	book = notebook_get_book( OFA_MAIN_WINDOW( main_window ));
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

	book = notebook_get_book( main_window );
	if( book ){
		while(( count = gtk_notebook_get_n_pages( book )) > 0 ){
			gtk_notebook_remove_page( book, count-1 );
		}
	}
	my_dnd_window_close_all();
}

/*
 * ofaIGetter interface management
 */
static void
igetter_iface_init( ofaIGetterInterface *iface )
{
	static const gchar *thisfn = "ofa_main_window_igetter_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_permanent = igetter_get_permanent_getter;
	iface->get_application = igetter_get_application;
	iface->get_hub = igetter_get_hub;
	iface->get_main_window = igetter_get_main_window;
	iface->get_theme_manager = igetter_get_theme_manager;
}

static ofaIGetter *
igetter_get_permanent_getter( const ofaIGetter *instance )
{
	return( OFA_IGETTER( instance ));
}

static GApplication *
igetter_get_application( const ofaIGetter *instance )
{
	GtkApplication *application;

	application = gtk_window_get_application( GTK_WINDOW( instance ));

	return( G_APPLICATION( application ));
}

static ofaHub *
igetter_get_hub( const ofaIGetter *instance )
{
	GtkApplication *application;

	application = gtk_window_get_application( GTK_WINDOW( instance ));

	return( ofa_application_get_hub( OFA_APPLICATION( application )));
}

static GtkApplicationWindow *
igetter_get_main_window( const ofaIGetter *instance )
{
	return( GTK_APPLICATION_WINDOW( instance ));
}

/*
 * the themes are managed by the main window
 */
static ofaIThemeManager *
igetter_get_theme_manager( const ofaIGetter *instance )
{
	return( OFA_ITHEME_MANAGER( instance ));
}

/*
 * ofaIThemeManager interface management
 */
static void
itheme_manager_iface_init( ofaIThemeManagerInterface *iface )
{
	static const gchar *thisfn = "ofa_main_window_itheme_manager_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->define = itheme_manager_define;
	iface->activate = itheme_manager_activate;
}

static void
itheme_manager_define( ofaIThemeManager *instance, GType type, const gchar *label )
{
	static const gchar *thisfn = "ofa_itheme_manager_define";
	ofaMainWindowPrivate *priv;
	sThemeDef *sdata;

	g_debug( "%s: instance=%p (%s), type=%lu, label=%s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), type, label );

	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( instance ));

	g_return_if_fail( !priv->dispose_has_run );

	sdata = theme_get_by_type( &priv->themes, type );

	g_free( sdata->label );
	sdata->label = g_strdup( label );
}

static ofaPage *
itheme_manager_activate( ofaIThemeManager *instance, GType type )
{
	static const gchar *thisfn = "ofa_itheme_manager_activate";
	ofaMainWindowPrivate *priv;
	GtkNotebook *book;
	ofaPage *page;
	sThemeDef *theme_def;

	g_debug( "%s: instance=%p (%s), type=%lu",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), type );

	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	page = NULL;
	book = notebook_get_book( OFA_MAIN_WINDOW( instance ));
	g_return_val_if_fail( book && GTK_IS_NOTEBOOK( book ), NULL );

	theme_def = theme_get_by_type( &priv->themes, type );

	if( !my_dnd_window_present_by_type( type )){
		page = notebook_get_page( OFA_MAIN_WINDOW( instance ), book, theme_def );
		if( !page ){
			page = notebook_create_page( OFA_MAIN_WINDOW( instance ), book, theme_def );
		}
		g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );
		notebook_activate_page( OFA_MAIN_WINDOW( instance ), book, page );
	}

	return( page );
}

static sThemeDef *
theme_get_by_type( GList **list, GType type )
{
	sThemeDef *sdata;
	GList *it;

	for( it=*list ; it ; it=it->next ){
		sdata = ( sThemeDef * ) it->data;
		if( sdata->type == type ){
			return( sdata );
		}
	}

	sdata = g_new0( sThemeDef, 1 );
	sdata->type = type;

	*list = g_list_prepend( *list, sdata );

	return( sdata );
}

static void
theme_free( sThemeDef *def )
{
	g_free( def->label );
	g_free( def );
}

/*
 * myIActionMap interface management
 */
static void
iaction_map_iface_init( myIActionMapInterface *iface )
{
	static const gchar *thisfn = "ofa_main_window_iaction_map_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_menu_model = iaction_map_get_menu_model;
	iface->get_action_target = iaction_map_get_action_target;
}

static GMenuModel *
iaction_map_get_menu_model( const myIActionMap *instance )
{
	ofaMainWindowPrivate *priv;

	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( instance ));

	return( priv->menu );
}

static const gchar *
iaction_map_get_action_target( const myIActionMap *instance )
{
	return( "win" );
}
