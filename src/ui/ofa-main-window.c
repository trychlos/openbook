/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
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
#include "my/my-iscope-map.h"
#include "my/my-iwindow.h"
#include "my/my-scope-mapper.h"
#include "my/my-style.h"
#include "my/my-tab.h"
#include "my/my-utils.h"

#include "api/ofa-entry-page.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-igetter.h"
#include "api/ofa-ipage-manager.h"
#include "api/ofa-isignaler.h"
#include "api/ofa-page.h"
#include "api/ofa-prefs.h"
#include "api/ofo-dossier.h"

#include "core/ofa-guided-input.h"
#include "core/ofa-open-prefs.h"
#include "core/ofa-reconcil-render.h"
#include "core/ofa-reconcil-page.h"
#include "core/ofa-settlement-page.h"

#include "ui/ofa-account-balance-render.h"
#include "ui/ofa-account-book-render.h"
#include "ui/ofa-account-page.h"
#include "ui/ofa-application.h"
#include "ui/ofa-backup-assistant.h"
#include "ui/ofa-balance-render.h"
#include "ui/ofa-bat-page.h"
#include "ui/ofa-check-balances.h"
#include "ui/ofa-check-integrity.h"
#include "ui/ofa-class-page.h"
#include "ui/ofa-currency-page.h"
#include "ui/ofa-dossier-display-notes.h"
#include "ui/ofa-dossier-properties.h"
#include "ui/ofa-exercice-close-assistant.h"
#include "ui/ofa-export-assistant.h"
#include "ui/ofa-guided-ex.h"
#include "ui/ofa-import-assistant.h"
#include "ui/ofa-ledger-close.h"
#include "ui/ofa-ledger-book-render.h"
#include "ui/ofa-ledger-page.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-misc-audit-ui.h"
#include "ui/ofa-nomodal-page.h"
#include "ui/ofa-ope-template-page.h"
#include "ui/ofa-paimean-page.h"
#include "ui/ofa-period-close.h"
#include "ui/ofa-rate-page.h"
#include "ui/ofa-unreconcil-page.h"
#include "ui/ofa-unsettled-page.h"

/* private instance data
 */
typedef struct {
	gboolean         dispose_has_run;

	/* initialization
	 */
	ofaIGetter      *getter;

	/* runtime
	 */
	gchar           *settings_prefix;
	GMenuModel      *menu_model;
	gchar           *orig_title;
	GtkWidget       *grid;
	GtkMenuBar      *menubar;
	myAccelGroup    *accel_group;
	guint            paned_position;
	gboolean         is_mini;
	ofeMainbookTabs  pages_mode;
	gboolean         have_detach_pin;

	/* when a dossier is opened
	 */
	GtkWidget       *pane;
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

	/* ofaIPageManager interface
	 */
	GList           *themes;			/* registered themes */
}
	ofaMainWindowPrivate;

static void on_properties            ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_backup                ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_close                 ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_guided            ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_guided_ex         ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_entry_page        ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_unsettled_page    ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_unreconcil_page   ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_concil            ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_settlement        ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_ledger_close      ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_period_close      ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_exercice_close    ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_import            ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_export            ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_render_balances       ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_render_account_balance( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_render_accounts_book  ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_render_ledgers_book   ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_render_reconcil       ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_accounts          ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_ledgers           ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_ope_templates     ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_currencies        ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_rates             ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_classes           ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_paimeans          ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_batfiles          ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_check_balances        ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_check_integrity       ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_misc_audit            ( GSimpleAction *action, GVariant *parameter, gpointer user_data );

static const GActionEntry st_dos_entries[] = {
		{ "properties",             on_properties,             NULL, NULL, NULL },
		{ "backup",                 on_backup,                 NULL, NULL, NULL },
		{ "close",                  on_close,                  NULL, NULL, NULL },
		{ "guided",                 on_ope_guided,             NULL, NULL, NULL },
		{ "guidedex",               on_ope_guided_ex,          NULL, NULL, NULL },
		{ "entries",                on_ope_entry_page,         NULL, NULL, NULL },
		{ "unsentries",             on_ope_unsettled_page,     NULL, NULL, NULL },
		{ "unrentries",             on_ope_unreconcil_page,    NULL, NULL, NULL },
		{ "concil",                 on_ope_concil,             NULL, NULL, NULL },
		{ "settlement",             on_ope_settlement,         NULL, NULL, NULL },
		{ "ledclosing",             on_ope_ledger_close,       NULL, NULL, NULL },
		{ "perclosing",             on_ope_period_close,       NULL, NULL, NULL },
		{ "execlosing",             on_ope_exercice_close,     NULL, NULL, NULL },
		{ "import",                 on_ope_import,             NULL, NULL, NULL },
		{ "export",                 on_ope_export,             NULL, NULL, NULL },
		{ "render-balances",        on_render_balances,        NULL, NULL, NULL },
		{ "render-accbal",          on_render_account_balance, NULL, NULL, NULL },
		{ "render-books",           on_render_accounts_book,   NULL, NULL, NULL },
		{ "render-ledgers-book",    on_render_ledgers_book,    NULL, NULL, NULL },
		{ "render-reconcil",        on_render_reconcil,        NULL, NULL, NULL },
		{ "accounts",               on_ref_accounts,           NULL, NULL, NULL },
		{ "ledgers",                on_ref_ledgers,            NULL, NULL, NULL },
		{ "ope-templates",          on_ref_ope_templates,      NULL, NULL, NULL },
		{ "currencies",             on_ref_currencies,         NULL, NULL, NULL },
		{ "rates",                  on_ref_rates,              NULL, NULL, NULL },
		{ "classes",                on_ref_classes,            NULL, NULL, NULL },
		{ "paimeans",               on_ref_paimeans,           NULL, NULL, NULL },
		{ "batfiles",               on_ref_batfiles,           NULL, NULL, NULL },
		{ "chkbal",                 on_check_balances,         NULL, NULL, NULL },
		{ "integrity",              on_check_integrity,        NULL, NULL, NULL },
		{ "misc_audit",             on_misc_audit,             NULL, NULL, NULL },
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
 * @multiple: whether the page is unique or may be displayed several times.
 *
 * @count: the count of page creation fot this @type;
 *  it is used to number the multiple pages.
 *
 * This structure is allocated in #theme_manager_define() method, but
 * cannot be used to initialize our themes (because GType is not a
 * compilation constant).
 *
 * This structure is part of ofaIPageManager implementation.
 */
typedef struct {
	GType    type;
	gchar   *label;
	gboolean multiple;
	guint    count;
}
	sThemeDef;

/* The structure used to initialize our themes
 * This structure is only part of ofaMainWindow initialization.
 * List is ordered by get_type() function name just for reference.
 */
typedef struct {
	gchar     *label;
	GType    (*fn_get_type)( void );
	gboolean   multiple;
}
	sThemeInit;

static sThemeInit st_theme_defs[] = {
		{ N_( "Accounts balance" ),        ofa_account_balance_render_get_type, FALSE },
		{ N_( "Accounts book" ),           ofa_account_book_render_get_type,    FALSE },
		{ N_( "Chart of accounts" ),       ofa_account_page_get_type,           FALSE },
		{ N_( "Entries balance" ),         ofa_balance_render_get_type,         FALSE },
		{ N_( "Imported BAT files" ),      ofa_bat_page_get_type,               FALSE },
		{ N_( "Account classes" ),         ofa_class_page_get_type,             FALSE },
		{ N_( "Currencies" ),              ofa_currency_page_get_type,          FALSE },
		{ N_( "View entries" ),            ofa_entry_page_get_type,             TRUE },
		{ N_( "Guided input" ),            ofa_guided_ex_get_type,              FALSE },
		{ N_( "Ledgers book" ),            ofa_ledger_book_render_get_type,     FALSE },
		{ N_( "Ledgers" ),                 ofa_ledger_page_get_type,            FALSE },
		{ N_( "Means of paiement" ),       ofa_paimean_page_get_type,           FALSE },
		{ N_( "Operation templates" ),     ofa_ope_template_page_get_type,      FALSE },
		{ N_( "Rates" ),                   ofa_rate_page_get_type,              FALSE },
		{ N_( "Reconciliation" ),          ofa_reconcil_page_get_type,          FALSE },
		{ N_( "Reconciliation Sumary" ),   ofa_reconcil_render_get_type,        FALSE },
		{ N_( "Settlement" ),              ofa_settlement_page_get_type,        FALSE },
		{ N_( "Unreconciliated entries" ), ofa_unreconcil_page_get_type,        FALSE },
		{ N_( "Unsettled entries" ),       ofa_unsettled_page_get_type,         FALSE },
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

static const gchar *st_resource_dosmenu = "/org/trychlos/openbook/ui/ofa-dos-menubar.ui";
static const gchar *st_resource_css     = "/org/trychlos/openbook/ui/ofa.css";
static const gchar *st_dosmenu_id       = "dos-menu";
static const gchar *st_icon_fname       = ICONFNAME;

static void         menubar_init( ofaMainWindow *self );
static void         menubar_setup( ofaMainWindow *window, GActionMap *map );
static void         menubar_update_items( ofaMainWindow *self );
static void         init_themes( ofaMainWindow *self );
static void         setup_position_size( ofaMainWindow *self, gboolean position );
static void         signaler_on_dossier_opened( ofaISignaler *signaler, ofaMainWindow *self );
static void         signaler_on_dossier_closed( ofaISignaler *signaler, ofaMainWindow *self );
static void         signaler_on_dossier_changed( ofaISignaler *signaler, ofaMainWindow *main_window );
static void         signaler_on_dossier_preview( ofaISignaler *signaler, const gchar *uri, ofaMainWindow *main_window );
static void         signaler_on_ui_restart( ofaISignaler *signaler, ofaMainWindow *self );
static void         signaler_on_run_export( ofaISignaler *signaler, ofaIExportable *exportable, gboolean force_modal, ofaMainWindow *self );
static void         on_map( ofaMainWindow *self, void *empty );
static void         on_shown( ofaMainWindow *self, void *empty );
static gboolean     on_map_event( ofaMainWindow *self, GdkEvent *event, void *empty );
static gboolean     on_delete_event( GtkWidget *toplevel, GdkEvent *event, gpointer user_data );
static void         set_window_title( ofaMainWindow *window, gboolean with_dossier );
static void         warning_exercice_unset( ofaMainWindow *window );
static void         reset_pages_count( ofaMainWindow *self );
static void         pane_destroy( ofaMainWindow *self );
static void         pane_create( ofaMainWindow *self );
static void         pane_left_add_treeview( ofaMainWindow *window );
static void         pane_left_on_item_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainWindow *window );
static void         pane_right_add_empty_notebook( ofaMainWindow *window );
static void         background_destroy( ofaMainWindow *self );
static void         background_image_update( ofaMainWindow *self );
static void         background_image_set_uri( ofaMainWindow *self, const gchar *uri );
static void         do_backup( ofaMainWindow *self );
static void         do_properties( ofaMainWindow *self );
static GtkNotebook *notebook_get_book( ofaMainWindow *window );
static ofaPage     *notebook_get_page( const ofaMainWindow *window, GtkNotebook *book, const sThemeDef *def );
static ofaPage     *notebook_create_page( ofaMainWindow *self, GtkNotebook *book, sThemeDef *def );
static void         book_attach_page( ofaMainWindow *self, GtkNotebook *book, GtkWidget *page, const gchar *title );
static void         notebook_activate_page( const ofaMainWindow *window, GtkNotebook *book, ofaPage *page );
static gboolean     notebook_on_draw( GtkWidget *widget, cairo_t *cr, ofaMainWindow *self );
static gboolean     book_on_append_page( myDndBook *book, GtkWidget *page, const gchar *title, ofaMainWindow *self );
static void         nomodal_create( ofaMainWindow *self, sThemeDef *theme_def );
static ofaPage     *page_create( ofaMainWindow *self, sThemeDef *theme_def, gchar **title );
static void         on_tab_close_clicked( myTab *tab, ofaPage *page );
static void         do_close( ofaPage *page );
static void         on_page_removed( GtkNotebook *book, GtkWidget *page, guint page_num, ofaMainWindow *self );
static void         close_all_pages( ofaMainWindow *self );
static void         on_tab_pin_clicked( myTab *tab, ofaPage *page );
static void         read_settings( ofaMainWindow *self );
static void         write_settings( ofaMainWindow *self );
static void         ipage_manager_iface_init( ofaIPageManagerInterface *iface );
static void         ipage_manager_define( ofaIPageManager *instance, GType type, const gchar *label, gboolean multiple );
static ofaPage     *ipage_manager_activate( ofaIPageManager *instance, GType type );
static sThemeDef   *theme_get_by_type( GList **list, GType type, gboolean create );
static void         theme_free( sThemeDef *def );

G_DEFINE_TYPE_EXTENDED( ofaMainWindow, ofa_main_window, GTK_TYPE_APPLICATION_WINDOW, 0,
		G_ADD_PRIVATE( ofaMainWindow )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IPAGE_MANAGER, ipage_manager_iface_init ))

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

	g_free( priv->settings_prefix );
	g_free( priv->orig_title );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_main_window_parent_class )->finalize( instance );
}

static void
main_window_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofa_main_window_dispose";
	ofaMainWindowPrivate *priv;
	ofaHub *hub;
	myISettings *settings;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_if_fail( instance && OFA_IS_MAIN_WINDOW( instance ));

	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( instance ));

	if( !priv->dispose_has_run ){

		hub = ofa_igetter_get_hub( priv->getter );
		ofa_hub_close_dossier( hub );

		/* save the window position (always)
		 * only save the windows size if display mode is normal */
		settings = ofa_igetter_get_user_settings( priv->getter );
		if( priv->is_mini ){
			my_utils_window_position_save_pos_only( GTK_WINDOW( instance ), settings, priv->settings_prefix );
		} else {
			my_utils_window_position_save( GTK_WINDOW( instance ), settings, priv->settings_prefix );
		}

		write_settings( OFA_MAIN_WINDOW( instance ));

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->menu_model );
		g_list_free_full( priv->themes, ( GDestroyNotify ) theme_free );

		my_style_free();
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_main_window_parent_class )->dispose( instance );
}

static void
main_window_constructed( GObject *instance )
{
	static const gchar *thisfn = "ofa_main_window_constructed";
	ofaMainWindowPrivate *priv;
	GError *error;

	g_return_if_fail( instance && OFA_IS_MAIN_WINDOW( instance ));

	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( instance ));

	if( !priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( ofa_main_window_parent_class )->constructed ){
			G_OBJECT_CLASS( ofa_main_window_parent_class )->constructed( instance );
		}

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
		priv->grid = gtk_grid_new();
		g_debug( "%s: grid=%p (%s)", thisfn, priv->grid, G_OBJECT_TYPE_NAME( priv->grid ));
		/*gtk_widget_set_hexpand( GTK_WIDGET( priv->grid ), TRUE );*/
		/*gtk_widget_set_vexpand( GTK_WIDGET( priv->grid ), TRUE );*/
		/*gtk_container_set_resize_mode( GTK_CONTAINER( priv->grid ), GTK_RESIZE_QUEUE );*/
		gtk_grid_set_row_homogeneous( GTK_GRID( priv->grid ), FALSE );
		gtk_container_add( GTK_CONTAINER( instance ), priv->grid );

		/* connect some signals
		 */
		g_signal_connect( instance, "show", G_CALLBACK( on_shown ), NULL );
		g_signal_connect( instance, "map", G_CALLBACK( on_map ), NULL );
		g_signal_connect( instance, "map-event", G_CALLBACK( on_map_event ), NULL );
		g_signal_connect( instance, "delete-event", G_CALLBACK( on_delete_event ), NULL );

		/* set the default icon for all windows of the application */
		error = NULL;
		gtk_window_set_default_icon_from_file( st_icon_fname, &error );
		if( error ){
			g_warning( "%s: %s", thisfn, error->message );
			g_error_free( error );
		}

		/* style class initialisation */
		my_style_set_css_resource( st_resource_css );
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
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->last_theme = THM_LAST_THEME;
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
 * ofa_main_window_new:
 * @getter: a #ofaIGetter instance.
 *
 * Returns a newly allocated ofaMainWindow object.
 */
ofaMainWindow *
ofa_main_window_new( ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_main_window_new";
	ofaMainWindow *window;
	ofaMainWindowPrivate *priv;
	GApplication *application;
	ofaHub *hub;
	ofaISignaler *signaler;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	g_debug( "%s: getter=%p", thisfn, getter );

	/* 'application' is a GtkWindow property
	 *  because it is not defined as a construction property, it is only
	 *  available after g_object_new() has returned
	 */
	application = ofa_igetter_get_application( getter );
	window = g_object_new( OFA_TYPE_MAIN_WINDOW, "application", application, NULL );

	/* advertise the main window and the page manager
	 */
	hub = ofa_igetter_get_hub( getter );
	ofa_hub_set_main_window( hub, GTK_APPLICATION_WINDOW( window ));
	ofa_hub_set_page_manager( hub, OFA_IPAGE_MANAGER( window ));

	/* connect to the ofaISignaler signals
	 */
	signaler = ofa_igetter_get_signaler( getter );
	g_signal_connect( signaler, SIGNALER_DOSSIER_OPENED, G_CALLBACK( signaler_on_dossier_opened ), window );
	g_signal_connect( signaler, SIGNALER_DOSSIER_CLOSED, G_CALLBACK( signaler_on_dossier_closed ), window );
	g_signal_connect( signaler, SIGNALER_DOSSIER_CHANGED, G_CALLBACK( signaler_on_dossier_changed ), window );
	g_signal_connect( signaler, SIGNALER_DOSSIER_PREVIEW, G_CALLBACK( signaler_on_dossier_preview ), window );
	g_signal_connect( signaler, SIGNALER_UI_RESTART, G_CALLBACK( signaler_on_ui_restart ), window );
	g_signal_connect( signaler, SIGNALER_EXPORT_ASSISTANT_RUN, G_CALLBACK( signaler_on_run_export ), window );

	/* set the IGetter and continue the initialization
	 */
	priv = ofa_main_window_get_instance_private( window );
	priv->getter = getter;

	read_settings( window );
	g_object_get( G_OBJECT( application ), OFA_PROP_APPLICATION_NAME, &priv->orig_title, NULL );

	/* initialize the theme manager, then let the plugins advertise theirs */
	init_themes( window );

	/* load the main window menubar
	 * installing the application one
	 */
	menubar_init( window );
	menubar_setup( window, G_ACTION_MAP( application ));

	priv->is_mini = ( ofa_prefs_mainbook_get_startup_mode( getter ) == MAINBOOK_STARTMINI );
	setup_position_size( window, TRUE );

	return( window );
}

static void
menubar_init( ofaMainWindow *self )
{
	static const gchar *thisfn = "ofa_main_window_menubar_init";
	ofaMainWindowPrivate *priv;
	GtkBuilder *builder;
	GMenuModel *menu;
	myScopeMapper *mapper;
	ofaISignaler *signaler;

	priv = ofa_main_window_get_instance_private( self );

	/* define the main instance actions
	 */
	g_action_map_add_action_entries(
			G_ACTION_MAP( self ),
			st_dos_entries, G_N_ELEMENTS( st_dos_entries ),
			( gpointer ) self );

	/* define a traditional menubar
	 * the program will abort if GtkBuilder is not able to be parsed
	 * from the given file
	 */
	builder = gtk_builder_new_from_resource( st_resource_dosmenu );
	menu = G_MENU_MODEL( gtk_builder_get_object( builder, st_dosmenu_id ));
	if( menu ){
		g_debug( "%s: menu successfully loaded from %s at %p: items=%d",
				thisfn, st_resource_dosmenu, ( void * ) menu, g_menu_model_get_n_items( menu ));

		priv->menu_model = g_object_ref( menu );

		/* register the menu model with the action map
		 */
		mapper = ofa_igetter_get_scope_mapper( priv->getter );
		my_scope_mapper_register( mapper, "win", G_ACTION_MAP( self ), menu );

		signaler = ofa_igetter_get_signaler( priv->getter );
		g_signal_emit_by_name( signaler, SIGNALER_MENU_AVAILABLE, "win", self );

	} else {
		g_warning( "%s: unable to find '%s' object in '%s' resource", thisfn, st_dosmenu_id, st_resource_dosmenu );
	}

	g_object_unref( builder );
}

/*
 * @map is:
 * - the GtkApplication at main window creation and on dossier close
 * - the GtkApplicationWindow on dossier open
 */
static void
menubar_setup( ofaMainWindow *window, GActionMap *map )
{
	static const gchar *thisfn = "ofa_main_window_menubar_setup";
	ofaMainWindowPrivate *priv;
	GtkWidget *menubar;
	GMenuModel *model;
	myScopeMapper *mapper;

	g_debug( "%s: window=%p, map=%p", thisfn, ( void * ) window, ( void * ) map );

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

	mapper = ofa_igetter_get_scope_mapper( priv->getter );
	model = my_iscope_map_get_menu_model( MY_ISCOPE_MAP( mapper ), map );
	if( model ){
		priv->accel_group = my_accel_group_new();
		my_accel_group_setup_accels_from_menu( priv->accel_group, model, MY_ISCOPE_MAP( mapper ));
		gtk_window_add_accel_group( GTK_WINDOW( window ), GTK_ACCEL_GROUP( priv->accel_group ));

		menubar = gtk_menu_bar_new_from_model( model );

		g_debug( "%s: model=%p (%s), menubar=%p, grid=%p (%s)",
				thisfn,
				( void * ) model, G_OBJECT_TYPE_NAME( model ), ( void * ) menubar,
				( void * ) priv->grid, G_OBJECT_TYPE_NAME( priv->grid ));

		gtk_grid_attach( GTK_GRID( priv->grid ), menubar, 0, 0, 1, 1 );
		gtk_widget_show_all( priv->grid );

		priv->menubar = GTK_MENU_BAR( menubar );
	}
}

/*
 * Enable menu items depending of the writability status of the current
 * dossier
 */
static void
menubar_update_items( ofaMainWindow *self )
{
	ofaMainWindowPrivate *priv;
	ofaHub *hub;
	gboolean is_writable;

	priv = ofa_main_window_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );
	is_writable = ofa_hub_is_writable_dossier( hub );

	my_utils_action_enable( G_ACTION_MAP( self ), &priv->action_guided_input,   "guided",     is_writable );
	my_utils_action_enable( G_ACTION_MAP( self ), &priv->action_settlement,     "settlement", is_writable );
	my_utils_action_enable( G_ACTION_MAP( self ), &priv->action_reconciliation, "concil",     is_writable );
	my_utils_action_enable( G_ACTION_MAP( self ), &priv->action_close_ledger,   "ledclosing", is_writable );
	my_utils_action_enable( G_ACTION_MAP( self ), &priv->action_close_period,   "perclosing", is_writable );
	my_utils_action_enable( G_ACTION_MAP( self ), &priv->action_close_exercice, "execlosing", is_writable );
	my_utils_action_enable( G_ACTION_MAP( self ), &priv->action_import,         "import",     is_writable );
}

/*
 * the main window initialization of the theme manager:
 * - define the themes for the main window
 * - then declare the theme manager general availability
 */
static void
init_themes( ofaMainWindow *self )
{
	ofaMainWindowPrivate *priv;
	gint i;
	ofaISignaler *signaler;

	priv = ofa_main_window_get_instance_private( self );

	/* define the themes for the main window */
	for( i=0 ; st_theme_defs[i].label ; ++i ){
		ofa_ipage_manager_define( OFA_IPAGE_MANAGER( self ),
				( *st_theme_defs[i].fn_get_type )(), gettext( st_theme_defs[i].label ), st_theme_defs[i].multiple );
	}

	/* declare then the theme manager general availability */
	signaler = ofa_igetter_get_signaler( priv->getter );
	g_signal_emit_by_name( signaler, SIGNALER_PAGE_MANAGER_AVAILABLE, self );
}

/*
 * Set the main window position (if asked for) and size.
 *
 * The priv->is_mini must have been set prior to call this function.
 */
static void
setup_position_size( ofaMainWindow *self, gboolean position )
{
	static const gchar *thisfn = "ofa_main_window_setup_position_size";
	ofaMainWindowPrivate *priv;
	myISettings *settings;
	GtkRequisition min_size, nat_size;
	gint x, y, width, height;
	gboolean set;

	priv = ofa_main_window_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	set = my_utils_window_position_get( settings, priv->settings_prefix, &x, &y, &width, &height );

	if( set && position && x >= 0 && y >= 0 ){
		gtk_window_move( GTK_WINDOW( self ), x, y );
	}

	/* the minimal/natural sizes here are only relevant when the child
	 * widgets have been gtk_widget_show() and even if they are not
	 * actually visible at this time of the construction
	 */
	gtk_widget_get_preferred_size( GTK_WIDGET( self ), &min_size, &nat_size );
	g_debug( "%s: min_size.width=%d, min_size.height=%d, nat_size.width=%d, nat_size.height=%d",
			thisfn, min_size.width, min_size.height, nat_size.width, nat_size.height );

	if( priv->is_mini && min_size.width > 0 && min_size.height > 0 ){
		gtk_window_resize( GTK_WINDOW( self ), min_size.width, min_size.height );

	} else if( width > 0 && height > 0 ){
		gtk_window_resize( GTK_WINDOW( self ), width, height );
	}
}

static void
signaler_on_dossier_opened( ofaISignaler *signaler, ofaMainWindow *self )
{
	static const gchar *thisfn = "ofa_main_window_signaler_on_dossier_opened";

	g_debug( "%s: signaler=%p, self=%p", thisfn, ( void * ) signaler, ( void * ) self );

	pane_create( self );

	/* install the application menubar
	 */
	menubar_setup( self, G_ACTION_MAP( self ));

	/* the window title defaults to the application title
	 * the application menubar does not have any dynamic item *
	 * no background image
	 */
	signaler_on_dossier_changed( signaler, self );
}

/*
 * At this time, the main window may have been already destroyed
 */
static void
signaler_on_dossier_closed( ofaISignaler *signaler, ofaMainWindow *self )
{
	static const gchar *thisfn = "ofa_main_window_signaler_on_dossier_closed";
	ofaMainWindowPrivate *priv;
	GtkApplication *application;
	gboolean mini;

	g_debug( "%s: signaler=%p, self=%p", thisfn, ( void * ) signaler, ( void * ) self );

	//if( GTK_IS_WINDOW( self ) && ofa_hub_get_dossier( signaler )){
	if( GTK_IS_WINDOW( self )){

		priv = ofa_main_window_get_instance_private( self );

		ofa_main_window_dossier_close_windows( self );

		pane_destroy( self );

		application = gtk_window_get_application( GTK_WINDOW( self ));
		menubar_setup( self, G_ACTION_MAP( application ));

		set_window_title( self, FALSE );

		/* must we come back from a 'normal' display mode to a 'mini' one ? */
		if( ofa_prefs_mainbook_get_close_mode( priv->getter ) == MAINBOOK_CLOSERESET ){
			mini = ( ofa_prefs_mainbook_get_startup_mode( priv->getter ) == MAINBOOK_STARTMINI );
			if( mini != priv->is_mini ){
				priv->is_mini = mini;
				setup_position_size( self, FALSE );
			}
		}

		reset_pages_count( self );
	}
}

/*
 * the dossier has advertized the signaler that its properties has been
 * modified (or may have been modified) by the user
 */
static void
signaler_on_dossier_changed( ofaISignaler *signaler, ofaMainWindow *self )
{
	static const gchar *thisfn = "ofa_main_window_signaler_on_dossier_changed";

	g_debug( "%s: signaler=%p, self=%p", thisfn, ( void * ) signaler, ( void * ) self );

	set_window_title( self, TRUE );
	menubar_update_items( self );
	background_image_update( self );
}

/*
 * set a background image (if the UI permits this)
 */
static void
signaler_on_dossier_preview( ofaISignaler *signaler, const gchar *uri, ofaMainWindow *self )
{
	background_image_set_uri( self, uri );
}

/*
 * Restart the UI let us take into account the new user preferences.
 * This is mainly recreating the main notebook.
 */
static void
signaler_on_ui_restart( ofaISignaler *signaler, ofaMainWindow *self )
{
	static const gchar *thisfn = "ofa_main_window_signaler_on_ui_restart";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: signaler=%p, self=%p",
			thisfn, ( void * ) signaler, ( void * ) self );

	priv = ofa_main_window_get_instance_private( self );

	/* close all */
	ofa_main_window_dossier_close_windows( self );

	/* recreate the main ui */
	pane_destroy( self );
	pane_create( self );

	if( priv->pane ){
		gtk_widget_show_all( priv->pane );
	}
}

static void
signaler_on_run_export( ofaISignaler *signaler, ofaIExportable *exportable, gboolean force_modal, ofaMainWindow *self )
{
	static const gchar *thisfn = "ofa_main_window_signaler_on_run_export";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: handling '%s' signal", thisfn, SIGNALER_EXPORT_ASSISTANT_RUN );

	priv = ofa_main_window_get_instance_private( self );

	ofa_export_assistant_run( priv->getter, exportable, force_modal );
}

static void
on_map( ofaMainWindow *self, void *empty )
{
	g_debug( "ofaMainWindow::map" );
}

static void
on_shown( ofaMainWindow *self, void *empty )
{
	g_debug( "ofaMainWindow::show" );
}

static gboolean
on_map_event( ofaMainWindow *self, GdkEvent *event, void *empty )
{
	g_debug( "ofaMainWindow::map-event" );
	return( FALSE );
}

/*
 * Triggered when the user clicks on the top right [X] button.
 *
 * Returns %TRUE to stop the signal to be propagated (which would cause
 * the window to be destroyed); instead we gracefully quit the application.
 *
 * Or, in other terms:
 * If you return FALSE in the "delete_event" signal handler, GTK will
 * emit the "destroy" signal. Returning TRUE means you don't want the
 * window to be destroyed.
 */
static gboolean
on_delete_event( GtkWidget *toplevel, GdkEvent *event, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_delete_event";
	ofaMainWindowPrivate *priv;
	gboolean ok_to_quit;

	g_debug( "%s: toplevel=%p (%s), event=%p, user_data=%p",
			thisfn,
			( void * ) toplevel, G_OBJECT_TYPE_NAME( toplevel ),
			( void * ) event,
			( void * ) user_data );


	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( toplevel ), FALSE );

	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( toplevel ));

	ok_to_quit = !ofa_prefs_appli_confirm_on_altf4( priv->getter ) ||
					ofa_main_window_is_willing_to_quit( OFA_MAIN_WINDOW( toplevel ));

	return( !ok_to_quit );
}

/**
 * ofa_main_window_dossier_close_windows:
 * @main_window: this #ofaMainWindow instance.
 *
 * Not only as part of closing a dossier, but also when closing an exercice...
 */
void
ofa_main_window_dossier_close_windows( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_main_window_dossier_close_windows";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv = ofa_main_window_get_instance_private( main_window );

	g_return_if_fail( !priv->dispose_has_run );

	close_all_pages( main_window );
	my_iwindow_close_all();
}

/**
 * ofa_main_window_dossier_apply_actions:
 * @main_window: this #ofaMainWindow instance.
 *
 * Run the the standard actions after having opened the dossier.
 *
 * This is in particularly used by the functions which open a dossier
 * in order to have the actions run *after* the dialog (resp. assistant)
 * is closed.
 */
void
ofa_main_window_dossier_apply_actions( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_main_window_dossier_apply_actions";
	ofaMainWindowPrivate *priv;
	myISettings *settings;
	const ofaIDBConnect *connect;
	ofaIDBDossierMeta *dossier_meta;
	ofaOpenPrefs *prefs;
	const gchar *group, *main_notes, *exe_notes;
	ofoDossier *dossier;
	gboolean empty, only_non_empty;
	const GDate *exe_begin, *exe_end;
	ofaHub *hub;

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv = ofa_main_window_get_instance_private( main_window );

	g_return_if_fail( !priv->dispose_has_run );

	hub = ofa_igetter_get_hub( priv->getter );
	connect = ofa_hub_get_connect( hub );
	dossier_meta = ofa_idbconnect_get_dossier_meta( connect );
	settings = ofa_idbdossier_meta_get_settings_iface( dossier_meta );
	group = ofa_idbdossier_meta_get_settings_group( dossier_meta );
	prefs = ofa_open_prefs_new( settings, group, OPEN_PREFS_DOSSIER_KEY );

	/* display dossier notes ? */
	if( ofa_open_prefs_get_display_notes( prefs )){

		dossier = ofa_hub_get_dossier( hub );
		main_notes = ofo_dossier_get_notes( dossier );
		exe_notes = ofo_dossier_get_exe_notes( dossier );
		empty = my_strlen( main_notes ) == 0 && my_strlen( exe_notes ) == 0;
		only_non_empty = ofa_open_prefs_get_non_empty_notes( prefs );

		g_debug( "%s: empty=%s, only_non_empty=%s",
				thisfn, empty ? "True":"False", only_non_empty ? "True":"False" );

		if( !empty || !only_non_empty ){
			ofa_dossier_display_notes_run( priv->getter, GTK_WINDOW( main_window ), main_notes, exe_notes );
		}
	}

	/* check balances and DBMS integrity ? */
	if( ofa_open_prefs_get_check_balances( prefs )){
		ofa_check_balances_run( priv->getter, GTK_WINDOW( main_window ));
	}
	if( ofa_open_prefs_get_check_integrity( prefs )){
		ofa_check_integrity_run( priv->getter, GTK_WINDOW( main_window ));
	}

	/* display dossier properties ? */
	if( ofa_open_prefs_get_display_properties( prefs )){
		do_properties( main_window );

	/* at least warns if begin or end of exercice is not set */
	} else {
		dossier = ofa_hub_get_dossier( hub );
		exe_begin = ofo_dossier_get_exe_begin( dossier );
		exe_end = ofo_dossier_get_exe_end( dossier );
		if( !my_date_is_valid( exe_begin ) || !my_date_is_valid( exe_end )){
			warning_exercice_unset( main_window );
		}
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
				GTK_WINDOW( main_window ),
				_( "Are you sure you want to quit the application ?" ),
				_( "_Quit" )));
}

/*
 * do not rely on the ofa_hub_get_dossier() here
 * because it has not yet been reset when closing the dossier
 */
static void
set_window_title( ofaMainWindow *self, gboolean with_dossier )
{
	static const gchar *thisfn = "ofa_main_window_set_window_title";
	ofaMainWindowPrivate *priv;
	ofaHub *hub;
	ofoDossier *dossier;
	const ofaIDBConnect *connect;
	ofaIDBDossierMeta *dossier_meta;
	ofaIDBExerciceMeta *period;
	gchar *title, *period_label, *period_name;
	const gchar *dos_name;

	g_debug( "%s: self=%p, with_dossier=%s",
			thisfn, ( void * ) self, with_dossier ? "True":"False" );

	priv = ofa_main_window_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );
	dossier = with_dossier ? ofa_hub_get_dossier( hub ) : NULL;

	if( dossier ){
		connect = ofa_hub_get_connect( hub );
		dossier_meta = ofa_idbconnect_get_dossier_meta( connect );
		period = ofa_idbconnect_get_exercice_meta( connect );
		dos_name = ofa_idbdossier_meta_get_dossier_name( dossier_meta );
		period_label = ofa_idbexercice_meta_get_label( period );
		period_name = ofa_idbexercice_meta_get_name( period );

		title = g_strdup_printf( "%s (%s) %s - %s",
				dos_name,
				period_name,
				period_label,
				priv->orig_title );

		g_free( period_name );
		g_free( period_label );

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
warning_exercice_unset( ofaMainWindow *self )
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
			GTK_WINDOW( self ),
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

/*
 * reinitialize the count of opened multiple pages
 */
static void
reset_pages_count( ofaMainWindow *self )
{
	ofaMainWindowPrivate *priv;
	GList *it;
	sThemeDef *def;

	priv = ofa_main_window_get_instance_private( self );

	for( it=priv->themes ; it ; it=it->next ){
		def = ( sThemeDef * ) it->data;
		def->count = 0;
	}
}

static void
pane_destroy( ofaMainWindow *self )
{
	ofaMainWindowPrivate *priv;

	priv = ofa_main_window_get_instance_private( self );

	background_destroy( self );

	if( priv->pane ){
		priv->paned_position = gtk_paned_get_position( GTK_PANED( priv->pane ));
		gtk_widget_destroy( priv->pane );
		priv->pane = NULL;
	}
}

static void
pane_create( ofaMainWindow *self )
{
	static const gchar *thisfn = "ofa_main_window_pane_create";
	ofaMainWindowPrivate *priv;

	priv = ofa_main_window_get_instance_private( self );

	/* compute the display mode when a dossier is opened:
	 * is normal unless startup_mode=mini and open_mode=keep
	 */
	priv->is_mini =
			ofa_prefs_mainbook_get_startup_mode( priv->getter ) == MAINBOOK_STARTMINI &&
			ofa_prefs_mainbook_get_open_mode( priv->getter ) == MAINBOOK_OPENKEEP;

	setup_position_size( self, FALSE );

	/* pages_mode and pin_detach are evaluated on dossier opening
	 */
	priv->have_detach_pin = ofa_prefs_mainbook_get_with_detach_pin( priv->getter );
	priv->pages_mode = ofa_prefs_mainbook_get_tabs_mode( priv->getter );

	g_debug( "%s: pages_mode=%u, have_detach_pin=%s, is_mini%s",
			thisfn, priv->pages_mode, priv->have_detach_pin ? "True":"False",
			priv->is_mini ? "True":"False" );

	if( !priv->is_mini ){
		priv->pane = gtk_paned_new( GTK_ORIENTATION_HORIZONTAL );
		gtk_grid_attach( GTK_GRID( priv->grid ), priv->pane, 0, 1, 1, 1 );
		gtk_paned_set_position( GTK_PANED( priv->pane ), priv->paned_position );
		pane_left_add_treeview( self );
		pane_right_add_empty_notebook( self );
	}
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
	gtk_paned_pack1( GTK_PANED( priv->pane ), GTK_WIDGET( frame ), FALSE, FALSE );

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

		ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( window ), ( *st_tree_defs[idx].fntype )());
	}
}

static void
pane_right_add_empty_notebook( ofaMainWindow *self )
{
	ofaMainWindowPrivate *priv;
	GtkWidget *book;

	priv = ofa_main_window_get_instance_private( self );

	g_return_if_fail( !priv->is_mini );

	if( priv->pages_mode == MAINBOOK_TABDETACH ){
		book = GTK_WIDGET( my_dnd_book_new());
		g_signal_connect( book, "my-append-page", G_CALLBACK( book_on_append_page ), self );

	} else {
		g_return_if_fail( priv->pages_mode == MAINBOOK_TABREORDER );
		book = gtk_notebook_new();
	}

	my_utils_widget_set_margins( book, 4, 4, 2, 4 );
	gtk_notebook_set_scrollable( GTK_NOTEBOOK( book ), TRUE );
	gtk_notebook_popup_enable( GTK_NOTEBOOK( book ));
	gtk_widget_set_hexpand( book, TRUE );
	gtk_widget_set_vexpand( book, TRUE );

	g_signal_connect( book, "draw", G_CALLBACK( notebook_on_draw ), self );
	g_signal_connect( book, "page-removed", G_CALLBACK( on_page_removed ), self );

	gtk_paned_pack2( GTK_PANED( priv->pane ), book, TRUE, FALSE );
}

static void
background_destroy( ofaMainWindow *self )
{
	ofaMainWindowPrivate *priv;

	priv = ofa_main_window_get_instance_private( self );

	if( priv->background_image ){
		cairo_surface_destroy( priv->background_image );
		priv->background_image = NULL;
	}
}

static void
background_image_update( ofaMainWindow *self )
{
	ofaMainWindowPrivate *priv;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	ofaIDBDossierMeta *dossier_meta;
	myISettings *settings;
	const gchar *group;
	gchar *background_uri;

	priv = ofa_main_window_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );
	connect = ofa_hub_get_connect( hub );
	dossier_meta = ofa_idbconnect_get_dossier_meta( connect );
	settings = ofa_idbdossier_meta_get_settings_iface( dossier_meta );
	group = ofa_idbdossier_meta_get_settings_group( dossier_meta );
	background_uri = my_isettings_get_string( settings, group, DOSSIER_BACKGROUND_KEY );

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

	background_destroy( self );

	if( priv->pane ){

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
	ofaMainWindowPrivate *priv;

	priv = ofa_main_window_get_instance_private( self );

	close_all_pages( self );

	ofa_backup_assistant_run( priv->getter );
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
do_properties( ofaMainWindow *self )
{
	ofaMainWindowPrivate *priv;

	priv = ofa_main_window_get_instance_private( self );

	ofa_dossier_properties_run( priv->getter, GTK_WINDOW( self ));
}

static void
on_close( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_close";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( user_data ));

	ofa_hub_close_dossier( ofa_igetter_get_hub( priv->getter ));
}

static void
on_ope_guided( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_guided";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( user_data ), OFA_TYPE_OPE_TEMPLATE_PAGE );
}

static void
on_ope_guided_ex( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_guided_ex";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( user_data ), OFA_TYPE_GUIDED_EX );
}

static void
on_ope_entry_page( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_entry_page";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( user_data ), OFA_TYPE_ENTRY_PAGE );
}

static void
on_ope_unreconcil_page( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_unreconcil_page";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( user_data ), OFA_TYPE_UNRECONCIL_PAGE );
}

static void
on_ope_unsettled_page( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_unsettled_page";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( user_data ), OFA_TYPE_UNSETTLED_PAGE );
}

static void
on_ope_concil( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_concil";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( user_data ), OFA_TYPE_RECONCIL_PAGE );
}

static void
on_ope_settlement( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_settlement";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( user_data ), OFA_TYPE_SETTLEMENT_PAGE );
}

static void
on_ope_ledger_close( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_ledger_close";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( user_data ));

	ofa_ledger_close_run( priv->getter, GTK_WINDOW( user_data ));
}

static void
on_ope_period_close( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_period_close";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( user_data ));

	ofa_period_close_run( priv->getter, GTK_WINDOW( user_data ));
}

static void
on_ope_exercice_close( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_exercice_close";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( user_data ));

	ofa_exercice_close_assistant_run( priv->getter );
}

static void
on_ope_import( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_import";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( user_data ));

	ofa_import_assistant_run( priv->getter );
}

static void
on_ope_export( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_export";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( user_data ));

	ofa_export_assistant_run( priv->getter, NULL, FALSE );
}

static void
on_render_balances( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_render_balances";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( user_data ), OFA_TYPE_BALANCE_RENDER );
}

static void
on_render_account_balance( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_render_account_balance";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( user_data ), OFA_TYPE_ACCOUNT_BALANCE_RENDER );
}

static void
on_render_accounts_book( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_render_accounts_book";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( user_data ), OFA_TYPE_ACCOUNT_BOOK_RENDER );
}

static void
on_render_ledgers_book( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_render_ledgers_book";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( user_data ), OFA_TYPE_LEDGER_BOOK_RENDER );
}

static void
on_render_reconcil( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_render_reconcil";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( user_data ), OFA_TYPE_RECONCIL_RENDER );
}

static void
on_ref_accounts( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_accounts";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( user_data ), OFA_TYPE_ACCOUNT_PAGE );
}

static void
on_ref_ledgers( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_ledgers";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( user_data ), OFA_TYPE_LEDGER_PAGE );
}

static void
on_ref_ope_templates( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_models";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( user_data ), OFA_TYPE_OPE_TEMPLATE_PAGE );
}

static void
on_ref_currencies( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_devises";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( user_data ), OFA_TYPE_CURRENCY_PAGE );
}

static void
on_ref_rates( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_rates";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( user_data ), OFA_TYPE_RATE_PAGE );
}

static void
on_ref_classes( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_classes";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( user_data ), OFA_TYPE_CLASS_PAGE );
}

static void
on_ref_paimeans( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_paimeans";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( user_data ), OFA_TYPE_PAIMEAN_PAGE );
}

static void
on_ref_batfiles( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_batfiles";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	ofa_ipage_manager_activate( OFA_IPAGE_MANAGER( user_data ), OFA_TYPE_BAT_PAGE );
}

static void
on_check_balances( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_misc_check_balances";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( user_data ));

	ofa_check_balances_run( priv->getter, GTK_WINDOW( user_data ));
}

static void
on_check_integrity( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_misc_check_integrity";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( user_data ));

	ofa_check_integrity_run( priv->getter, GTK_WINDOW( user_data ));
}

static void
on_misc_audit( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_misc_misc_audit";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( user_data ));

	ofa_misc_audit_ui_run( priv->getter );
}

static GtkNotebook *
notebook_get_book( ofaMainWindow *window )
{
	ofaMainWindowPrivate *priv;
	GtkWidget *book;

	priv = ofa_main_window_get_instance_private( window );

	book = NULL;

	if( priv->pane ){
		book = gtk_paned_get_child2( GTK_PANED( priv->pane ));
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
notebook_create_page( ofaMainWindow *self, GtkNotebook *book, sThemeDef *def )
{
	ofaPage *page;
	gchar *title;

	page = page_create( self, def, &title );
	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );

	book_attach_page( self, book, GTK_WIDGET( page ), title );

	g_free( title );

	return( page );
}

static void
book_attach_page( ofaMainWindow *self, GtkNotebook *book, GtkWidget *page, const gchar *title )
{
	ofaMainWindowPrivate *priv;
	myTab *tab;
	GtkWidget *label;
	GtkRequisition natural_size;

	priv = ofa_main_window_get_instance_private( self );

	/* natural_size is not used, but this makes Gtk happy */
	gtk_widget_get_preferred_size( GTK_WIDGET( page ), NULL, &natural_size );

	/* the tab widget */
	tab = my_tab_new( NULL, title );

	my_tab_set_show_close( tab, TRUE );
	g_signal_connect( tab, MY_SIGNAL_TAB_CLOSE_CLICKED, G_CALLBACK( on_tab_close_clicked ), page );

	/* pin is only displayed if dnd is off */
	my_tab_set_show_detach( tab, priv->pages_mode == MAINBOOK_TABREORDER && priv->have_detach_pin );
	g_signal_connect( tab, MY_SIGNAL_TAB_PIN_CLICKED, G_CALLBACK( on_tab_pin_clicked ), page );

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

/*
 * create a new page as a NomodalPage window
 */
static void
nomodal_create( ofaMainWindow *self, sThemeDef *theme_def )
{
	ofaMainWindowPrivate *priv;
	ofaPage *page;
	gchar *title1, *title2;

	priv = ofa_main_window_get_instance_private( self );

	page = page_create( self, theme_def, &title1 );
	g_return_if_fail( page && OFA_IS_PAGE( page ));

	title2 = my_utils_str_remove_underlines( title1 );

	ofa_nomodal_page_run( priv->getter, NULL, title2, GTK_WIDGET( page ));

	g_free( title2 );
	g_free( title1 );
}

/*
 * Create a new page
 * @self:
 * @theme_def:
 * @title: [out]: a placeholder for the title of the page which should
 *  be g_free() by the caller.
 *
 * Returns: the new #ofaPage.
 */
static ofaPage *
page_create( ofaMainWindow *self, sThemeDef *theme_def, gchar **title )
{
	ofaMainWindowPrivate *priv;
	ofaPage *page;
	const gchar *ctitle;

	priv = ofa_main_window_get_instance_private( self );

	page = g_object_new( theme_def->type, "ofa-page-getter", priv->getter, NULL );
	ctitle = gettext( theme_def->label );
	theme_def->count += 1;

	if( theme_def->multiple ){
		*title = g_strdup_printf( "%s [%u]", ctitle, theme_def->count );
	} else {
		*title = g_strdup( ctitle );
	}

	return( page );
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
	ofaIGetter *getter;
	GtkApplicationWindow *main_window;
	GtkNotebook *book;
	gint page_num;

	getter = ofa_page_get_getter( page );
	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	main_window = ofa_igetter_get_main_window( getter );
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

	g_debug( "%s: book=%p, page_w=%p (%s), page_num=%u, main_window=%p",
			thisfn, ( void * ) book,
			( void * ) page_w, G_OBJECT_TYPE_NAME( page_w ), page_num, ( void * ) main_window );

	g_signal_emit_by_name( page_w, "page-removed", page_w, page_num );
}

static void
close_all_pages( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_main_window_close_all_pages";
	GtkNotebook *book;
	gint count;

	book = notebook_get_book( main_window );
	if( book ){
		while(( count = gtk_notebook_get_n_pages( book )) > 0 ){
			g_debug( "%s: about to remove page index=%d", thisfn, count-1 );
			gtk_notebook_remove_page( book, count-1 );
		}
	}
	my_dnd_window_close_all();
	ofa_nomodal_page_close_all();
}

static void
on_tab_pin_clicked( myTab *tab, ofaPage *page )
{
	static const gchar *thisfn = "ofa_main_window_on_tab_pin_clicked";
	ofaMainWindowPrivate *priv;
	gchar *title1, *title2;
	GtkWindow *toplevel;

	g_debug( "%s: tab=%p, page=%p", thisfn, ( void * ) tab, ( void * ) page );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( page ));
	g_return_if_fail( toplevel && OFA_IS_MAIN_WINDOW( toplevel ));

	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( toplevel ));

	title1 = my_tab_get_label( tab );
	title2 = my_utils_str_remove_underlines( title1 );
	g_free( title1 );

	g_object_ref( G_OBJECT( page ));
	do_close( page );
	ofa_nomodal_page_run( priv->getter, toplevel, title2, GTK_WIDGET( page ));
	g_object_unref( G_OBJECT( page ));

	g_free( title2 );
}

/*
 * settings are: paned_position;
 */
static void
read_settings( ofaMainWindow *self )
{
	ofaMainWindowPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;
	gchar *key;

	priv = ofa_main_window_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, key );

	/* paned position */
	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->paned_position = my_strlen( cstr ) ? atoi( cstr ) : 0;
	if( priv->paned_position < 150 ){
		priv->paned_position = 150;
	}

	my_isettings_free_string_list( settings, strlist );
	g_free( key );
}

static void
write_settings( ofaMainWindow *self )
{
	ofaMainWindowPrivate *priv;
	myISettings *settings;
	gchar *str, *key;

	priv = ofa_main_window_get_instance_private( self );

	str = g_strdup_printf( "%d;",
			priv->paned_position );

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( str );
	g_free( key );
}

/*
 * ofaIPageManager interface management
 */
static void
ipage_manager_iface_init( ofaIPageManagerInterface *iface )
{
	static const gchar *thisfn = "ofa_main_window_ipage_manager_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->define = ipage_manager_define;
	iface->activate = ipage_manager_activate;
}

static void
ipage_manager_define( ofaIPageManager *instance, GType type, const gchar *label, gboolean multiple )
{
	static const gchar *thisfn = "ofa_ipage_manager_define";
	ofaMainWindowPrivate *priv;
	sThemeDef *sdata;

	g_debug( "%s: instance=%p (%s), type=%lu, label=%s, multiple=%s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			type, label, multiple ? "True":"False" );

	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( instance ));

	g_return_if_fail( !priv->dispose_has_run );

	sdata = theme_get_by_type( &priv->themes, type, TRUE );

	g_free( sdata->label );
	sdata->label = g_strdup( label );

	sdata->multiple = multiple;
	sdata->count = 0;
}

static ofaPage *
ipage_manager_activate( ofaIPageManager *instance, GType type )
{
	static const gchar *thisfn = "ofa_ipage_manager_activate";
	ofaMainWindowPrivate *priv;
	GtkNotebook *book;
	ofaPage *page;
	sThemeDef *theme_def;
	gboolean found;

	g_debug( "%s: instance=%p (%s), type=%lu",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), type );

	priv = ofa_main_window_get_instance_private( OFA_MAIN_WINDOW( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	page = NULL;
	theme_def = theme_get_by_type( &priv->themes, type, FALSE );

	if( theme_def ){
		found = FALSE;
		book = notebook_get_book( OFA_MAIN_WINDOW( instance ));

		if( !theme_def->multiple ){
			found = my_dnd_window_present_by_type( type ) || ofa_nomodal_page_present_by_type( type );
			if( !found && book ){
				g_return_val_if_fail( GTK_IS_NOTEBOOK( book ), NULL );
				page = notebook_get_page( OFA_MAIN_WINDOW( instance ), book, theme_def );
				found = ( page != NULL );
			}
		}
		if( !found ){
			if( book ){
				page = notebook_create_page( OFA_MAIN_WINDOW( instance ), book, theme_def );
				g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );
				notebook_activate_page( OFA_MAIN_WINDOW( instance ), book, page );

			} else {
				nomodal_create( OFA_MAIN_WINDOW( instance ), theme_def );
			}
		}

	} else {
		g_warning( "%s: theme not found for type=%lu", thisfn, type );
	}

	return( page );
}

static sThemeDef *
theme_get_by_type( GList **list, GType type, gboolean create )
{
	sThemeDef *sdata;
	GList *it;

	for( it=*list ; it ; it=it->next ){
		sdata = ( sThemeDef * ) it->data;
		if( sdata->type == type ){
			return( sdata );
		}
	}

	sdata = NULL;

	if( create ){
		sdata = g_new0( sThemeDef, 1 );
		sdata->type = type;
		*list = g_list_prepend( *list, sdata );
	}

	return( sdata );
}

static void
theme_free( sThemeDef *def )
{
	g_free( def->label );
	g_free( def );
}
