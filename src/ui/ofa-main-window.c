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

#include "ui/my-utils.h"
#include "ui/ofa-accounts-chart.h"
#include "ui/ofa-devises-set.h"
#include "ui/ofa-guided-input.h"
#include "ui/ofa-journals-set.h"
#include "ui/ofa-models-set.h"
#include "ui/ofa-taux-set.h"
#include "ui/ofa-main-window.h"
#include "ui/ofo-dossier.h"

static gboolean pref_confirm_on_altf4 = FALSE;

/* private class data
 */
struct _ofaMainWindowClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

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
	LAST_SIGNAL
};

/* the actions handled from the menubar
 */
static void on_close           ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ope_guided      ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_accounts    ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_journals    ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_models      ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_devises     ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_ref_taux        ( GSimpleAction *action, GVariant *parameter, gpointer user_data );

static const GActionEntry st_dos_entries[] = {
		{ "close",        on_close,            NULL, NULL, NULL },
		{ "guided",       on_ope_guided,       NULL, NULL, NULL },
		{ "accounts",     on_ref_accounts,     NULL, NULL, NULL },
		{ "journals",     on_ref_journals,     NULL, NULL, NULL },
		{ "models",       on_ref_models,       NULL, NULL, NULL },
		{ "devises",      on_ref_devises,      NULL, NULL, NULL },
		{ "taux",         on_ref_taux,         NULL, NULL, NULL },
};

/* This structure handles the functions which manage the pages of the
 * main notebook:
 * - the label which is displayed in the tab of the page of the main book
 * - the GObject get_type() function
 * - the external function which is finally to be called  with the newly
 *   created object.
 */
typedef struct {
	gint         theme_id;
	const gchar *label;
	GType      (*fn_get_type)( void );
}
	sThemeDef;

enum {
	THM_ACCOUNTS = 1,
	THM_DEVISES,
	THM_JOURNALS,
	THM_MODELS,
	THM_TAUX
};

static sThemeDef st_theme_defs[] = {

		{ THM_ACCOUNTS,
				N_( "Chart of accounts" ),
				ofa_accounts_chart_get_type },

		{ THM_JOURNALS,
				N_( "Journals" ),
				ofa_journals_set_get_type },

		{ THM_MODELS,
				N_( "Entry models" ),
				ofa_models_set_get_type },

		{ THM_DEVISES,
				N_( "Currencies" ),
				ofa_devises_set_get_type },

		{ THM_TAUX,
				N_( "Rates" ),
				ofa_taux_set_get_type },

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

		{ N_( "Guided input" ),      THM_MODELS },
		{ N_( "Chart of accounts" ), THM_ACCOUNTS },
		{ N_( "Journals" ),          THM_JOURNALS },
		{ N_( "Entry models" ),      THM_MODELS },
		{ N_( "Currencies" ),        THM_DEVISES },
		{ N_( "Rates" ),             THM_TAUX },
		{ 0 }
};

/* A pointer to the handling ofaMainPage object is set against each page
 * ( the GtkGrid) of the main notebook
 */
#define OFA_DATA_HANDLER                "ofa-data-handler"

static const gchar               *st_dosmenu_xml = PKGUIDIR "/ofa-dos-menubar.ui";
static const gchar               *st_dosmenu_id  = "dos-menu";

static GtkApplicationWindowClass *st_parent_class           = NULL;
static gint                       st_signals[ LAST_SIGNAL ] = { 0 };

static GType register_type( void );
static void  class_init( ofaMainWindowClass *klass );
static void  instance_init( GTypeInstance *instance, gpointer klass );
static void  instance_constructed( GObject *window );
static void  instance_dispose( GObject *window );
static void  instance_finalize( GObject *window );

static gboolean      on_delete_event( GtkWidget *toplevel, GdkEvent *event, gpointer user_data );
static void          set_menubar( ofaMainWindow *window, GMenuModel *model );
static void          extract_accels_rec( ofaMainWindow *window, GMenuModel *model, GtkAccelGroup *accel_group );
static void          on_open_dossier( ofaMainWindow *window, ofaOpenDossier* sod, gpointer user_data );
static void          add_treeview_to_pane_left( ofaMainWindow *window );
static void          on_theme_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainWindow *window );
static const sThemeDef *get_theme_def_from_id( gint theme_id );
static void          add_empty_notebook_to_pane_right( ofaMainWindow *window );
static void          on_open_dossier_cleanup_handler( ofaMainWindow *window, ofaOpenDossier* sod, gpointer user_data );
static void          main_activate_theme( ofaMainWindow *main, gint theme );
static GtkNotebook  *main_get_book( const ofaMainWindow *window );
static GtkWidget    *main_book_get_page( const ofaMainWindow *window, GtkNotebook *book, gint theme );
static GtkWidget    *main_book_create_page( ofaMainWindow *main, GtkNotebook *book, const sThemeDef *theme_def );
static void          main_book_activate_page( const ofaMainWindow *window, GtkNotebook *book, GtkWidget *page );

GType
ofa_main_window_get_type( void )
{
	static GType window_type = 0;

	if( !window_type ){
		window_type = register_type();
	}

	return( window_type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_main_window_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaMainWindowClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaMainWindow ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( GTK_TYPE_APPLICATION_WINDOW, "ofaMainWindow", &info, 0 );

	return( type );
}

static void
class_init( ofaMainWindowClass *klass )
{
	static const gchar *thisfn = "ofa_main_window_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->constructed = instance_constructed;
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofaMainWindowClassPrivate, 1 );

	/*
	 * ofaMainWindow::main-signal-open-dossier:
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
	 * 						ofaOpenDossier *struct,
	 * 						gpointer user_data );
	 */
	st_signals[ OPEN_DOSSIER ] = g_signal_new_class_handler(
				MAIN_SIGNAL_OPEN_DOSSIER,
				OFA_TYPE_MAIN_WINDOW,
				G_SIGNAL_RUN_CLEANUP | G_SIGNAL_ACTION,
				G_CALLBACK( on_open_dossier_cleanup_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_main_window_instance_init";
	ofaMainWindow *self;
	ofaMainWindowPrivate *priv;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_MAIN_WINDOW( instance );
	self->private = g_new0( ofaMainWindowPrivate, 1 );
	priv = self->private;
	priv->dispose_has_run = FALSE;
}

static void
instance_constructed( GObject *window )
{
	static const gchar *thisfn = "ofa_main_window_instance_constructed";
	ofaMainWindowPrivate *priv;
	GError *error;
	GtkBuilder *builder;
	GMenuModel *menu;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( window ));

	priv = OFA_MAIN_WINDOW( window )->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->constructed ){
			G_OBJECT_CLASS( st_parent_class )->constructed( window );
		}

		/* define the main window actions
		 */
		g_action_map_add_action_entries(
				G_ACTION_MAP( window ),
		        st_dos_entries, G_N_ELEMENTS( st_dos_entries ),
		        ( gpointer ) window );

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

		/* build the main window
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
		gtk_container_add( GTK_CONTAINER( window ), GTK_WIDGET( priv->grid ));

		gtk_window_set_default_size( GTK_WINDOW( window ), 600, 400 );

		/* connect some signals
		 */
		g_signal_connect( window, "delete-event", G_CALLBACK( on_delete_event ), NULL );

		g_signal_connect( window, MAIN_SIGNAL_OPEN_DOSSIER, G_CALLBACK( on_open_dossier ), NULL );
	}
}

static void
instance_dispose( GObject *window )
{
	static const gchar *thisfn = "ofa_main_window_instance_dispose";
	ofaMainWindowPrivate *priv;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( window ));

	priv = OFA_MAIN_WINDOW( window )->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

		priv->dispose_has_run = TRUE;

		g_free( priv->orig_title );

		if( priv->menu ){
			g_object_unref( priv->menu );
		}

		if( priv->dossier ){
			g_object_unref( priv->dossier );
		}

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( window );
		}
	}
}

static void
instance_finalize( GObject *window )
{
	static const gchar *thisfn = "ofa_main_window_instance_finalize";
	ofaMainWindow *self;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( window ));

	g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

	self = OFA_MAIN_WINDOW( window );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( window );
	}
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

	window = g_object_new( OFA_TYPE_MAIN_WINDOW,
					"application", application,
					NULL );

	g_object_get( G_OBJECT( application ),
			OFA_PROP_APPLICATION_NAME, &window->private->orig_title,
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
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_QUIT, GTK_RESPONSE_OK,
			NULL );

	response = gtk_dialog_run( GTK_DIALOG( dialog ));

	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
}

static void
set_menubar( ofaMainWindow *window, GMenuModel *model )
{
	GtkWidget *menubar;

	if( window->private->menubar ){
		gtk_widget_destroy( GTK_WIDGET( window->private->menubar ));
		window->private->menubar = NULL;
	}

	if( window->private->accel_group ){
		gtk_window_remove_accel_group( GTK_WINDOW( window ), window->private->accel_group );
		g_object_unref( window->private->accel_group );
		window->private->accel_group = NULL;
	}

	/*accels = gtk_accel_groups_from_object( G_OBJECT( model ));
	 *accels = gtk_accel_groups_from_object( G_OBJECT( menubar ));
	 * return nil */

	window->private->accel_group = gtk_accel_group_new();
	extract_accels_rec( window, model, window->private->accel_group );
	gtk_window_add_accel_group( GTK_WINDOW( window ), window->private->accel_group );

	menubar = gtk_menu_bar_new_from_model( model );
	gtk_grid_attach( window->private->grid, menubar, 0, 0, 1, 1 );
	window->private->menubar = GTK_MENU_BAR( menubar );
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

	if( window->private->dossier ){
		title = g_strdup_printf( "%s - %s",
				ofo_dossier_get_name( window->private->dossier ),
				window->private->orig_title );
	} else {
		title = g_strdup( window->private->orig_title );
	}

	gtk_window_set_title( GTK_WINDOW( window ), title );
	g_free( title );
}

static void
on_open_dossier( ofaMainWindow *window, ofaOpenDossier* sod, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_open_dossier";

	g_debug( "%s: window=%p, sod=%p, user_data=%p",
			thisfn, ( void * ) window, ( void * ) sod, ( void * ) user_data );
	g_debug( "%s: name=%s", thisfn, sod->dossier );
	g_debug( "%s: host=%s", thisfn, sod->host );
	g_debug( "%s: port=%d", thisfn, sod->port );
	g_debug( "%s: socket=%s", thisfn, sod->socket );
	g_debug( "%s: dbname=%s", thisfn, sod->dbname );
	g_debug( "%s: account=%s", thisfn, sod->account );
	g_debug( "%s: password=%s", thisfn, sod->password );

	window->private->dossier = ofo_dossier_new( sod->dossier );

	if( !ofo_dossier_open(
			window->private->dossier,
			sod->host, sod->port, sod->socket, sod->dbname, sod->account, sod->password )){

		g_object_unref( window->private->dossier );
		window->private->dossier = NULL;
		return;
	}

	window->private->pane = GTK_PANED( gtk_paned_new( GTK_ORIENTATION_HORIZONTAL ));
	gtk_grid_attach( window->private->grid, GTK_WIDGET( window->private->pane ), 0, 1, 1, 1 );
	add_treeview_to_pane_left( window );
	add_empty_notebook_to_pane_right( window );

	set_menubar( window, window->private->menu );
	set_window_title( window );
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
	gtk_paned_pack1( window->private->pane, GTK_WIDGET( frame ), FALSE, FALSE );

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

		main_activate_theme( window, st_tree_defs[idx].theme_id );
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
	gtk_paned_pack2( window->private->pane, GTK_WIDGET( book ), TRUE, FALSE );
}

static void
on_open_dossier_cleanup_handler( ofaMainWindow *window, ofaOpenDossier* sod, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_open_dossier_final_handler";

	g_debug( "%s: window=%p, sod=%p, user_data=%p",
			thisfn, ( void * ) window, ( void * ) sod, ( void * ) user_data );

	g_free( sod->dossier );
	g_free( sod->host );
	g_free( sod->socket );
	g_free( sod->dbname );
	g_free( sod->account );
	g_free( sod->password );
	g_free( sod );
}

static void
on_close( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_close";
	ofaApplication *appli;
	ofaMainWindowPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));
	priv = OFA_MAIN_WINDOW( user_data )->private;

	g_return_if_fail( priv->dossier && OFO_IS_DOSSIER( priv->dossier ));

	g_object_unref( priv->dossier );
	priv->dossier = NULL;

	gtk_widget_destroy( GTK_WIDGET( priv->pane ));
	priv->pane = NULL;
	appli = OFA_APPLICATION( gtk_window_get_application( GTK_WINDOW( user_data )));

	set_menubar( OFA_MAIN_WINDOW( user_data ), ofa_application_get_menu_model( appli ));
	set_window_title( OFA_MAIN_WINDOW( user_data ));
}

static void
on_ope_guided( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ope_guided";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	main_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_MODELS );
}

static void
on_ref_accounts( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_accounts";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	main_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_ACCOUNTS );
}

static void
on_ref_journals( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_journals";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	main_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_JOURNALS );
}

static void
on_ref_models( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_models";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	main_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_MODELS );
}

static void
on_ref_devises( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_devises";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	main_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_DEVISES );
}

static void
on_ref_taux( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_ref_taux";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));

	main_activate_theme( OFA_MAIN_WINDOW( user_data ), THM_TAUX );
}

/**
 * ofa_main_window_get_dossier:
 */
ofoDossier *
ofa_main_window_get_dossier( const ofaMainWindow *window )
{
	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( window ), NULL );

	if( !window->private->dispose_has_run ){

		return( window->private->dossier );
	}

	return( NULL );
}

/*
 * if the main page doesn't exist yet, then create it
 * then make sure it is displayed, and activate it
 * last run it
 */
static void
main_activate_theme( ofaMainWindow *main_window, gint theme )
{
	static const gchar *thisfn = "ofa_main_window_main_book_activate_page";
	GtkNotebook *main_book;
	GtkWidget *page;
	const sThemeDef *theme_def;

	g_debug( "%s: main_window=%p, theme=%d", thisfn, ( void * ) main_window, theme );

	main_book = main_get_book( main_window );
	g_return_if_fail( main_book && GTK_IS_NOTEBOOK( main_book ));

	theme_def = get_theme_def_from_id( theme );
	g_return_if_fail( theme_def );
	g_return_if_fail( theme_def->fn_get_type );

	page = main_book_get_page( main_window, main_book, theme );
	if( !page ){
		page = main_book_create_page( main_window, main_book, theme_def );
	}
	g_return_if_fail( page && GTK_IS_WIDGET( page ));
	main_book_activate_page( main_window, main_book, page );
}

static GtkNotebook *
main_get_book( const ofaMainWindow *window )
{
	GtkWidget *book;

	book = gtk_paned_get_child2( window->private->pane );
	g_return_val_if_fail( GTK_IS_NOTEBOOK( book ), NULL );

	return( GTK_NOTEBOOK( book ));
}

static GtkWidget *
main_book_get_page( const ofaMainWindow *window, GtkNotebook *book, gint theme )
{
	GtkWidget *page, *found;
	gint count, i, page_thm;
	ofaMainPage *handler;

	found = NULL;
	count = gtk_notebook_get_n_pages( book );
	for( i=0 ; !found && i<count ; ++i ){
		page = gtk_notebook_get_nth_page( book, i );
		handler = ( ofaMainPage * ) g_object_get_data( G_OBJECT( page ), OFA_DATA_HANDLER );
		g_return_val_if_fail( handler && OFA_IS_MAIN_PAGE( handler ), NULL );
		page_thm = ofa_main_page_get_theme( handler );
		if( page_thm == theme ){
			found = page;
		}
	}

	return( found );
}

/*
 * the page for this theme has not been found
 * so, create it as an empty GtkGrid, simultaneously instanciating the
 * handling object as an ofaMainPage
 */
static GtkWidget *
main_book_create_page( ofaMainWindow *main, GtkNotebook *book, const sThemeDef *theme_def )
{
	GtkGrid *grid;
	GtkLabel *label;
	ofaMainPage *handler;

	/* all pages of the main notebook begin with a GtkGrid
	 */
	grid = GTK_GRID( gtk_grid_new());
	gtk_grid_set_column_spacing( grid, 4 );

	label = GTK_LABEL( gtk_label_new_with_mnemonic( gettext( theme_def->label )));

	gtk_notebook_append_page( book, GTK_WIDGET( grid ), GTK_WIDGET( label ));
	gtk_notebook_set_tab_reorderable( book, GTK_WIDGET( grid ), TRUE );

	/* then instanciates the handling object which happens to be an
	 * ofaMainPage
	 */
	handler = g_object_new(( *theme_def->fn_get_type )(),
					MAIN_PAGE_PROP_WINDOW,  main,
					MAIN_PAGE_PROP_DOSSIER, main->private->dossier,
					MAIN_PAGE_PROP_GRID,    grid,
					MAIN_PAGE_PROP_THEME,   theme_def->theme_id,
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
	GtkWidget *view;
	ofaMainPage *handler;

	g_return_if_fail( window && OFA_IS_MAIN_WINDOW( window ));
	g_return_if_fail( book && GTK_IS_NOTEBOOK( book ));
	g_return_if_fail( page && GTK_IS_WIDGET( page ));

	gtk_widget_show_all( GTK_WIDGET( window ));

	page_num = gtk_notebook_page_num( book, page );
	gtk_notebook_set_current_page( book, page_num );

	handler = ( ofaMainPage * ) g_object_get_data( G_OBJECT( page ), OFA_DATA_HANDLER );
	g_return_if_fail( handler && OFA_IS_MAIN_PAGE( handler ));
	view = ofa_main_page_get_treeview( handler );

	if( view ){
		gtk_widget_grab_focus( view );
	}
}
