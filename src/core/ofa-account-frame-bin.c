/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include <glib/gi18n.h>

#include "my/my-utils.h"

#include "api/ofa-buttons-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-iactioner.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itheme-manager.h"
#include "api/ofa-page.h"
#include "api/ofa-preferences.h"
#include "api/ofo-account.h"
#include "api/ofo-class.h"
#include "api/ofo-dossier.h"

#include "core/ofa-account-properties.h"
#include "core/ofa-account-frame-bin.h"
#include "core/ofa-account-store.h"
#include "core/ofa-account-treeview.h"

#include "ui/ofa-entry-page.h"
#include "ui/ofa-reconcil-page.h"
#include "ui/ofa-settlement-page.h"

/* private instance data
 */
typedef struct {
	gboolean             dispose_has_run;

	/* initialization
	 */
	ofaIGetter          *getter;

	/* runtime
	 */
	ofaHub              *hub;
	GList               *hub_handlers;
	gboolean             is_writable;		/* whether the dossier is writable */
	ofaAccountStore     *store;
	GList               *store_handlers;
	GtkTreeCellDataFunc  cell_fn;
	void                *cell_data;
	gint                 prev_class;
	gchar               *settings_key;
	GtkWidget           *current_page;

	/* UI
	 */
	GtkWidget           *grid;
	GtkWidget           *notebook;
	ofaButtonsBox       *buttonsbox;

	/* actions
	 */
	GSimpleAction       *new_action;
	GSimpleAction       *update_action;
	GSimpleAction       *delete_action;
	GSimpleAction       *view_entries_action;
	GSimpleAction       *settlement_action;
	GSimpleAction       *reconciliation_action;
}
	ofaAccountFrameBinPrivate;

/* these are only default labels in the case where we were not able to
 * get the correct #ofoClass objects
 */
static const gchar  *st_class_labels[] = {
		N_( "Class I" ),
		N_( "Class II" ),
		N_( "Class III" ),
		N_( "Class IV" ),
		N_( "Class V" ),
		N_( "Class VI" ),
		N_( "Class VII" ),
		N_( "Class VIII" ),
		N_( "Class IX" ),
		NULL
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	N_SIGNALS
};

static guint        st_signals[ N_SIGNALS ] = { 0 };

static const gchar *st_action_group_name    = "account";

static void       setup_bin( ofaAccountFrameBin *self );
static void       setup_actions( ofaAccountFrameBin *self );
static GtkWidget *book_get_page_by_class( ofaAccountFrameBin *self, gint class_num, gboolean create );
static GtkWidget *book_create_page( ofaAccountFrameBin *self, gint class );
static void       book_expand_all( ofaAccountFrameBin *self );
static void       book_on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaAccountFrameBin *self );
static gboolean   book_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaAccountFrameBin *self );
static void       tview_on_selection_changed( ofaAccountTreeview *treeview, ofoAccount *account, ofaAccountFrameBin *self );
static void       tview_on_selection_activated( ofaAccountTreeview *treeview, ofoAccount *account, ofaAccountFrameBin *self );
static void       tview_on_key_delete( ofaAccountTreeview *treeview, ofoAccount *account, ofaAccountFrameBin *self );
static void       tview_on_key_insert( ofaAccountTreeview *treeview, ofaAccountFrameBin *self );
static void       action_update_enabled( ofaAccountFrameBin *self, ofoAccount *account );
static void       action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaAccountFrameBin *self );
static void       action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaAccountFrameBin *self );
static void       action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaAccountFrameBin *self );
static void       action_on_view_entries_activated( GSimpleAction *action, GVariant *empty, ofaAccountFrameBin *self );
static void       action_on_settlement_activated( GSimpleAction *action, GVariant *empty, ofaAccountFrameBin *self );
static void       action_on_reconciliation_activated( GSimpleAction *action, GVariant *empty, ofaAccountFrameBin *self );
static gboolean   is_new_allowed( ofaAccountFrameBin *self );
static gboolean   is_delete_allowed( ofaAccountFrameBin *self, ofoAccount *account );
static void       do_insert_account( ofaAccountFrameBin *self );
static void       do_update_account( ofaAccountFrameBin *self, ofoAccount *account );
static void       do_delete_account( ofaAccountFrameBin *self, ofoAccount *account );
static gboolean   delete_confirmed( ofaAccountFrameBin *self, ofoAccount *account );
static void       store_on_row_inserted( GtkTreeModel *tmodel, GtkTreePath *path, GtkTreeIter *iter, ofaAccountFrameBin *self );
static void       set_getter_hub( ofaAccountFrameBin *self );
static void       set_getter_store( ofaAccountFrameBin *self );
static void       hub_connect_to_signaling_system( ofaAccountFrameBin *self );
static void       hub_on_new_object( ofaHub *hub, ofoBase *object, ofaAccountFrameBin *self );
static void       hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaAccountFrameBin *self );
static void       hub_on_updated_class_label( ofaAccountFrameBin *self, ofoClass *class );
static void       hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaAccountFrameBin *self );static void       action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaAccountFrameBin *self );
static void       action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaAccountFrameBin *self );
static void       action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaAccountFrameBin *self );

static void       hub_on_deleted_class_label( ofaAccountFrameBin *self, ofoClass *class );
static void       hub_on_reload_dataset( ofaHub *hub, GType type, ofaAccountFrameBin *self );
static void       iactionable_iface_init( ofaIActionableInterface *iface );
static guint      iactionable_get_interface_version( void );
static void       iactioner_iface_init( ofaIActionerInterface *iface );
static guint      iactioner_get_interface_version( void );

G_DEFINE_TYPE_EXTENDED( ofaAccountFrameBin, ofa_account_frame_bin, GTK_TYPE_BIN, 0, \
		G_ADD_PRIVATE( ofaAccountFrameBin )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IACTIONABLE, iactionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IACTIONER, iactioner_iface_init ));

static void
account_frame_bin_finalize( GObject *instance )
{
	ofaAccountFrameBinPrivate *priv;
	static const gchar *thisfn = "ofa_account_frame_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_FRAME_BIN( instance ));

	priv = ofa_account_frame_bin_get_instance_private( OFA_ACCOUNT_FRAME_BIN( instance ));

	/* free data members here */
	g_free( priv->settings_key );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_frame_bin_parent_class )->finalize( instance );
}

static void
account_frame_bin_dispose( GObject *instance )
{
	ofaAccountFrameBinPrivate *priv;
	GList *it;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_FRAME_BIN( instance ));

	priv = ofa_account_frame_bin_get_instance_private( OFA_ACCOUNT_FRAME_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		/* disconnect from ofaHub signaling system */
		ofa_hub_disconnect_handlers( priv->hub, &priv->hub_handlers );

		/* disconnect from ofaAccountStore */
		if( priv->store && OFA_IS_ACCOUNT_STORE( priv->store )){
			for( it=priv->store_handlers ; it ; it=it->next ){
				g_signal_handler_disconnect( priv->store, ( gulong ) it->data );
			}
		}
		if( priv->store_handlers ){
			g_list_free( priv->store_handlers );
			priv->store_handlers = NULL;
		}
		g_clear_object( &priv->store );

		g_object_unref( priv->new_action );
		g_object_unref( priv->update_action );
		g_object_unref( priv->delete_action );
		g_object_unref( priv->view_entries_action );
		g_object_unref( priv->settlement_action );
		g_object_unref( priv->reconciliation_action );

		/* we expect that the last page seen by the user is those which
		 * has the better sizes and positions for the columns */
		ofa_tvbin_write_columns_settings( OFA_TVBIN( priv->current_page ));
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_frame_bin_parent_class )->dispose( instance );
}

static void
ofa_account_frame_bin_init( ofaAccountFrameBin *self )
{
	static const gchar *thisfn = "ofa_account_frame_bin_init";
	ofaAccountFrameBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ACCOUNT_FRAME_BIN( self ));

	priv = ofa_account_frame_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_key = NULL;
	priv->prev_class = -1;
	priv->current_page = NULL;
}

static void
ofa_account_frame_bin_class_init( ofaAccountFrameBinClass *klass )
{
	static const gchar *thisfn = "ofa_account_frame_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_frame_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_frame_bin_finalize;

	/**
	 * ofaAccountFrameBin::ofa-changed:
	 *
	 * This signal is sent when the selection is changed.
	 *
	 * Argument is the selected account. It may be %NULL.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAccountFrameBin *frame,
	 * 						ofoAccount       *account,
	 * 						gpointer          user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_ACCOUNT_FRAME_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaAccountFrameBin::ofa-activated:
	 *
	 * This signal is sent when the selection is activated.
	 *
	 * Argument is the selected account.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAccountFrameBin *frame,
	 * 						ofoAccount       *account,
	 * 						gpointer          user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-activated",
				OFA_TYPE_ACCOUNT_FRAME_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );
}

/**
 * ofa_account_frame_bin_new:
 * @getter: a #ofaIGetter instance.
 *
 * Creates the structured content, i.e. the accounts notebook on the
 * left column, the buttons box on the right one.
 *
 *   +-------------------------------------------------------------------+
 *   | creates a grid which will contain the frame and the buttons       |
 *   | +---------------------------------------------+-----------------+ +
 *   | | creates a notebook where each page contains | creates         | |
 *   | |   the account of the corresponding class    |   a buttons box | |
 *   | |   (cf. ofaAccountFrameBin class)            |                 | |
 *   | |                                             |                 | |
 *   | +---------------------------------------------+-----------------+ |
 *   +-------------------------------------------------------------------+
 * +-----------------------------------------------------------------------+
 */
ofaAccountFrameBin *
ofa_account_frame_bin_new( void  )
{
	ofaAccountFrameBin *self;

	self = g_object_new( OFA_TYPE_ACCOUNT_FRAME_BIN, NULL );

	setup_bin( self );
	setup_actions( self );

	return( self );
}

/*
 * Create the top grid which contains the accounts notebook and the
 * buttons box, and attach it to our #GtkBin
 *
 * Note that each page of the notebook is created on the fly, when an
 * account for this page is inserted in the store.
 *
 * Each page of the notebook presents the accounts of a given class.
 */
static void
setup_bin( ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;

	priv = ofa_account_frame_bin_get_instance_private( self );

	/* UI grid */
	priv->grid = gtk_grid_new();
	gtk_container_add( GTK_CONTAINER( self ), priv->grid );

	/* UI notebook */
	priv->notebook = gtk_notebook_new();
	gtk_notebook_popup_enable( GTK_NOTEBOOK( priv->notebook ));
	gtk_notebook_set_scrollable( GTK_NOTEBOOK( priv->notebook ), TRUE );
	gtk_notebook_set_show_tabs( GTK_NOTEBOOK( priv->notebook ), TRUE );
	gtk_grid_attach( GTK_GRID( priv->grid ), priv->notebook, 0, 0, 1, 1 );

	g_signal_connect( priv->notebook, "switch-page", G_CALLBACK( book_on_page_switched ), self );
	g_signal_connect( priv->notebook, "key-press-event", G_CALLBACK( book_on_key_pressed ), self );

	/* UI buttons box */
	priv->buttonsbox = ofa_buttons_box_new();
	gtk_grid_attach( GTK_GRID( priv->grid ), GTK_WIDGET( priv->buttonsbox ), 1, 0, 1, 1 );
}

/*
 * Defined the actions managed here.
 * All possible actions are defined, and default to be be disabled.
 * Only actions which are explicitely setup by the caller will be
 * activatable and have a button and a menu item.
 */
static void
setup_actions( ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;

	priv = ofa_account_frame_bin_get_instance_private( self );

	/* new */
	priv->new_action = g_simple_action_new( "new", NULL );
	g_simple_action_set_enabled( priv->new_action, FALSE );

	/* update */
	priv->update_action = g_simple_action_new( "update", NULL );
	g_simple_action_set_enabled( priv->update_action, FALSE );

	/* delete */
	priv->delete_action = g_simple_action_new( "delete", NULL );
	g_simple_action_set_enabled( priv->delete_action, FALSE );

	/* view_entries */
	priv->view_entries_action = g_simple_action_new( "view-entries", NULL );
	g_simple_action_set_enabled( priv->view_entries_action, FALSE );

	/* settlement */
	priv->settlement_action = g_simple_action_new( "settlement", NULL );
	g_simple_action_set_enabled( priv->settlement_action, FALSE );

	/* reconciliation */
	priv->reconciliation_action = g_simple_action_new( "reconciliation", NULL );
	g_simple_action_set_enabled( priv->reconciliation_action, FALSE );
}

/*
 * Returns the notebook's page container which is dedicated to the
 * given class number.
 *
 * If the page doesn't exist, and @bcreate is %TRUE, then it is created.
 */
static GtkWidget *
book_get_page_by_class( ofaAccountFrameBin *self, gint class_num, gboolean bcreate )
{
	static const gchar *thisfn = "ofa_account_frame_bin_get_page_by_class";
	ofaAccountFrameBinPrivate *priv;
	gint count, i;
	GtkWidget *found, *page_widget;
	gint page_class;

	priv = ofa_account_frame_bin_get_instance_private( self );

	if( !ofo_class_is_valid_number( class_num )){
		/* this is not really an error as %X macros do not begin with
		 * a valid digit class number */
		g_debug( "%s: invalid class number: %d", thisfn, class_num );
		return( NULL );
	}

	found = NULL;
	count = gtk_notebook_get_n_pages( GTK_NOTEBOOK( priv->notebook ));

	/* search for an existing page */
	for( i=0 ; i<count ; ++i ){
		page_widget = gtk_notebook_get_nth_page( GTK_NOTEBOOK( priv->notebook ), i );
		g_return_val_if_fail( page_widget && OFA_IS_ACCOUNT_TREEVIEW( page_widget ), NULL );
		page_class = ofa_account_treeview_get_filter_class( OFA_ACCOUNT_TREEVIEW( page_widget ));
		if( page_class == class_num ){
			found = page_widget;
			break;
		}
	}

	/* if not exists, create it (if allowed) */
	if( !found && bcreate ){
		found = book_create_page( self, class_num );
		if( !found ){
			g_warning( "%s: unable to create the page for class %d", thisfn, class_num );
			return( NULL );
		}
	}

	return( found );
}

/*
 * creates the page widget for the given class number
 */
static GtkWidget *
book_create_page( ofaAccountFrameBin *self, gint class_num )
{
	static const gchar *thisfn = "ofa_account_frame_bin_create_page";
	ofaAccountFrameBinPrivate *priv;
	ofaAccountTreeview *view;
	GtkWidget *label;
	ofoClass *class_obj;
	const gchar *class_label;
	gchar *str;
	gint page_num;
	GMenu *menu;

	g_debug( "%s: self=%p, class_num=%d", thisfn, ( void * ) self, class_num );

	priv = ofa_account_frame_bin_get_instance_private( self );

	view = ofa_account_treeview_new( class_num );

	ofa_account_treeview_set_settings_key( view, priv->settings_key );
	ofa_account_treeview_set_cell_data_func( view, priv->cell_fn, priv->cell_data );
	ofa_tvbin_set_store( OFA_TVBIN( view ), GTK_TREE_MODEL( priv->store ));

	g_signal_connect( view, "ofa-accchanged", G_CALLBACK( tview_on_selection_changed ), self );
	g_signal_connect( view, "ofa-accactivated", G_CALLBACK( tview_on_selection_activated ), self );
	g_signal_connect( view, "ofa-accdelete", G_CALLBACK( tview_on_key_delete ), self );
	g_signal_connect( view, "ofa-insert", G_CALLBACK( tview_on_key_insert ), self );

	/* add the page to the notebook */
	class_obj = ofo_class_get_by_number( priv->hub, class_num );
	if( class_obj && OFO_IS_CLASS( class_obj )){
		class_label = ofo_class_get_label( class_obj );
	} else {
		class_label = st_class_labels[class_num-1];
	}

	label = gtk_label_new( class_label );
	str = g_strdup_printf( "Alt-%d", class_num );
	gtk_widget_set_tooltip_text( label, str );
	g_free( str );

	page_num = gtk_notebook_append_page( GTK_NOTEBOOK( priv->notebook ), GTK_WIDGET( view ), label );
	if( page_num == -1 ){
		g_warning( "%s: unable to add a page to the notebook for class=%d", thisfn, class_num );
		g_object_unref( view );
		return( NULL );
	}
	gtk_notebook_set_tab_reorderable( GTK_NOTEBOOK( priv->notebook ), GTK_WIDGET( view ), TRUE );

	/* create a new context menu for each page of the notebook */
	menu = g_menu_new();
	g_menu_append_section( menu, NULL,
			G_MENU_MODEL( ofa_iactionable_get_menu( OFA_IACTIONABLE( self ), st_action_group_name )));
	ofa_icontext_set_menu(
			OFA_ICONTEXT( view ), OFA_IACTIONABLE( self ),
			menu );
	g_object_unref( menu );

	menu = ofa_tvbin_get_menu( OFA_TVBIN( view ));
	ofa_icontext_append_submenu(
			OFA_ICONTEXT( view ), OFA_IACTIONABLE( view ),
			OFA_IACTIONABLE_VISIBLE_COLUMNS_ITEM, menu );

	/* proxy and sync of action messages */
	ofa_iactioner_register_actionable( OFA_IACTIONER( self ), OFA_IACTIONABLE( view ));

	return( GTK_WIDGET( view ));
}

static void
book_expand_all( ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;
	gint pages_count, i;
	GtkWidget *page_w;
	GtkTreeView *tview;

	priv = ofa_account_frame_bin_get_instance_private( self );

	pages_count = gtk_notebook_get_n_pages( GTK_NOTEBOOK( priv->notebook ));
	for( i=0 ; i<pages_count ; ++i ){
		page_w = gtk_notebook_get_nth_page( GTK_NOTEBOOK( priv->notebook ), i );
		g_return_if_fail( page_w && GTK_IS_WIDGET( page_w ));
		tview = ( GtkTreeView * ) my_utils_container_get_child_by_type( GTK_CONTAINER( page_w ), GTK_TYPE_TREE_VIEW );
		g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));
		gtk_tree_view_expand_all( tview );
	}
}

/*
 * We have switch to this given page (wpage, npage):
 * just setup the selection
 *
 * At this time, GtkNotebook current page is not yet set. We so cannot
 * rely on it.
 */
static void
book_on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;
	ofoAccount *account;

	g_return_if_fail( wpage && OFA_IS_ACCOUNT_TREEVIEW( wpage ));

	priv = ofa_account_frame_bin_get_instance_private( self );

	priv->current_page = wpage;
	account = ofa_account_treeview_get_selected( OFA_ACCOUNT_TREEVIEW( wpage ));
	tview_on_selection_changed( OFA_ACCOUNT_TREEVIEW( wpage ), account, self );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
book_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;
	gboolean stop;
	GtkWidget *page_widget;
	gint class_num, page_num;

	stop = FALSE;
	priv = ofa_account_frame_bin_get_instance_private( self );

	if( event->state == GDK_MOD1_MASK ||
		event->state == ( GDK_MOD1_MASK | GDK_SHIFT_MASK )){

		class_num = -1;

		switch( event->keyval ){
			case GDK_KEY_1:
			case GDK_KEY_ampersand:
				class_num = 1;
				break;
			case GDK_KEY_2:
			case GDK_KEY_eacute:
				class_num = 2;
				break;
			case GDK_KEY_3:
			case GDK_KEY_quotedbl:
				class_num = 3;
				break;
			case GDK_KEY_4:
			case GDK_KEY_apostrophe:
				class_num = 4;
				break;
			case GDK_KEY_5:
			case GDK_KEY_parenleft:
				class_num = 5;
				break;
			case GDK_KEY_6:
			case GDK_KEY_minus:
				class_num = 6;
				break;
			case GDK_KEY_7:
			case GDK_KEY_egrave:
				class_num = 7;
				break;
			case GDK_KEY_8:
			case GDK_KEY_underscore:
				class_num = 8;
				break;
			case GDK_KEY_9:
			case GDK_KEY_ccedilla:
				class_num = 9;
				break;
		}

		if( class_num > 0 ){
			page_widget = book_get_page_by_class( self, class_num, FALSE );
			if( page_widget ){
				page_num = gtk_notebook_page_num( GTK_NOTEBOOK( priv->notebook ), page_widget );
				gtk_notebook_set_current_page( GTK_NOTEBOOK( priv->notebook ), page_num );
				stop = TRUE;
			}
		}
	}

	return( stop );
}

/**
 * ofa_account_frame_bin_get_current_page:
 * @bin: this #ofaAccountFrameBin instance.
 *
 * Returns the #ofaAccountTreeview associated to the current page.
 */
GtkWidget *
ofa_account_frame_bin_get_current_page( ofaAccountFrameBin *bin )
{
	ofaAccountFrameBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_ACCOUNT_FRAME_BIN( bin ), NULL );

	priv = ofa_account_frame_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->current_page );
}

/**
 * ofa_account_frame_bin_get_selected:
 * @bin:
 *
 * Returns: the currently selected account.
 *
 * The returned reference is owned by the underlying #ofaAccountStore,
 * and should not be unreffed by the caller.
 */
ofoAccount *
ofa_account_frame_bin_get_selected( ofaAccountFrameBin *bin )
{
	static const gchar *thisfn = "ofa_account_frame_bin_get_selected";
	ofaAccountFrameBinPrivate *priv;
	ofoAccount *account;

	g_debug( "%s: bin=%p", thisfn, ( void * ) bin );

	g_return_val_if_fail( bin && OFA_IS_ACCOUNT_FRAME_BIN( bin ), NULL );

	priv = ofa_account_frame_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );
	g_return_val_if_fail( priv->current_page, NULL );

	account = ofa_account_treeview_get_selected( OFA_ACCOUNT_TREEVIEW( priv->current_page ));

	return( account );
}

/**
 * ofa_account_frame_bin_set_selected:
 * @bin: this #ofaAccountFrameBin instance
 * @number: [allow-none]: the account identifier to be selected.
 *
 * Let the user reset the selection after the end of setup and
 * initialization phases.
 */
void
ofa_account_frame_bin_set_selected( ofaAccountFrameBin *bin, const gchar *number )
{
	static const gchar *thisfn = "ofa_account_frame_bin_set_selected";
	ofaAccountFrameBinPrivate *priv;
	gint acc_class, page_n;
	GtkWidget *page_w;

	g_debug( "%s: bin=%p, number=%s", thisfn, ( void * ) bin, number );

	g_return_if_fail( bin && OFA_IS_ACCOUNT_FRAME_BIN( bin ));

	priv = ofa_account_frame_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	if( my_strlen( number )){
		acc_class = ofo_account_get_class_from_number( number );
		page_w = book_get_page_by_class( bin, acc_class, FALSE );
		/* asked page is empty */
		if( !page_w ){
			return;
		}
		g_return_if_fail( OFA_IS_ACCOUNT_TREEVIEW( page_w ));

		page_n = gtk_notebook_page_num( GTK_NOTEBOOK( priv->notebook ), page_w );
		gtk_notebook_set_current_page( GTK_NOTEBOOK( priv->notebook ), page_n );

		ofa_account_treeview_set_selected( OFA_ACCOUNT_TREEVIEW( page_w ), number );
	}
}

/**
 * ofa_account_frame_bin_set_cell_data_func:
 * @bin:
 * @fn_cell:
 * @user_data:
 *
 */
void
ofa_account_frame_bin_set_cell_data_func( ofaAccountFrameBin *bin, GtkTreeCellDataFunc fn_cell, void *fn_data )
{
	ofaAccountFrameBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_ACCOUNT_FRAME_BIN( bin ));

	priv = ofa_account_frame_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	priv->cell_fn = fn_cell;
	priv->cell_data = fn_data;
}

/*
 * @account: may be %NULL.
 */
static void
tview_on_selection_changed( ofaAccountTreeview *treeview, ofoAccount *account, ofaAccountFrameBin *self )
{
	g_return_if_fail( !account || OFO_IS_ACCOUNT( account ));

	action_update_enabled( self, account );

	g_signal_emit_by_name( self, "ofa-changed", account );
}

/*
 * @account
 */
static void
tview_on_selection_activated( ofaAccountTreeview *treeview, ofoAccount *account, ofaAccountFrameBin *self )
{
	g_return_if_fail( account && OFO_IS_ACCOUNT( account ));

	g_signal_emit_by_name( self, "ofa-activated", account );
}

/*
 * @account
 */
static void
tview_on_key_delete( ofaAccountTreeview *treeview, ofoAccount *account, ofaAccountFrameBin *self )
{
	g_return_if_fail( !account || OFO_IS_ACCOUNT( account ));

	if( account && is_delete_allowed( self, account )){
		do_delete_account( self, account );
	}
}

static void
tview_on_key_insert( ofaAccountTreeview *treeview, ofaAccountFrameBin *self )
{
	if( is_new_allowed( self )){
		do_insert_account( self );
	}
}

/**
 * ofa_account_frame_bin_add_action:
 * @bin: this #ofaAccountFrameBin instance.
 * @id: the action identifier.
 *
 * Create a new button in the #ofaButtonsBox, and define a menu item
 * for the contextual menu.
 */
void
ofa_account_frame_bin_add_action( ofaAccountFrameBin *bin, ofeAccountAction id )
{
	ofaAccountFrameBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_ACCOUNT_FRAME_BIN( bin ));

	priv = ofa_account_frame_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	switch( id ){
		case ACCOUNT_ACTION_SPACER:
			ofa_buttons_box_add_spacer( priv->buttonsbox );
			break;

		case ACCOUNT_ACTION_NEW:
			ofa_iactionable_set_menu_item(
					OFA_IACTIONABLE( bin ), st_action_group_name, G_ACTION( priv->new_action ),
					OFA_IACTIONABLE_NEW_ITEM );
			ofa_buttons_box_append_button(
					priv->buttonsbox,
					ofa_iactionable_set_button(
							OFA_IACTIONABLE( bin ), st_action_group_name, G_ACTION( priv->new_action ),
							OFA_IACTIONABLE_NEW_BTN ));
			g_signal_connect( priv->new_action, "activate", G_CALLBACK( action_on_new_activated ), bin );
			g_simple_action_set_enabled( priv->new_action, priv->is_writable );
			break;

		case ACCOUNT_ACTION_UPDATE:
			ofa_iactionable_set_menu_item(
					OFA_IACTIONABLE( bin ), st_action_group_name, G_ACTION( priv->update_action ),
					priv->is_writable ? OFA_IACTIONABLE_PROPERTIES_ITEM_EDIT : OFA_IACTIONABLE_PROPERTIES_ITEM_DISPLAY );
			ofa_buttons_box_append_button(
					priv->buttonsbox,
					ofa_iactionable_set_button(
							OFA_IACTIONABLE( bin ), st_action_group_name, G_ACTION( priv->update_action ),
							OFA_IACTIONABLE_PROPERTIES_BTN ));
			g_signal_connect( priv->update_action, "activate", G_CALLBACK( action_on_update_activated ), bin );
			g_simple_action_set_enabled( priv->update_action, TRUE );
			break;

		case ACCOUNT_ACTION_DELETE:
			ofa_iactionable_set_menu_item(
					OFA_IACTIONABLE( bin ), st_action_group_name, G_ACTION( priv->delete_action ),
					OFA_IACTIONABLE_DELETE_ITEM );
			ofa_buttons_box_append_button(
					priv->buttonsbox,
					ofa_iactionable_set_button(
							OFA_IACTIONABLE( bin ), st_action_group_name, G_ACTION( priv->delete_action ),
							OFA_IACTIONABLE_DELETE_BTN ));
			g_signal_connect( priv->delete_action, "activate", G_CALLBACK( action_on_delete_activated ), bin );
			g_simple_action_set_enabled( priv->delete_action, TRUE );
			break;

		case ACCOUNT_ACTION_VIEW_ENTRIES:
			ofa_iactionable_set_menu_item(
					OFA_IACTIONABLE( bin ), st_action_group_name, G_ACTION( priv->view_entries_action ),
					_( "View entries" ));
			ofa_buttons_box_append_button(
					priv->buttonsbox,
					ofa_iactionable_set_button(
							OFA_IACTIONABLE( bin ), st_action_group_name, G_ACTION( priv->view_entries_action ),
							_( "_View entries..." )));
			g_signal_connect( priv->view_entries_action, "activate", G_CALLBACK( action_on_view_entries_activated ), bin );
			g_simple_action_set_enabled( priv->view_entries_action, TRUE );
			break;

		case ACCOUNT_ACTION_SETTLEMENT:
			ofa_iactionable_set_menu_item(
					OFA_IACTIONABLE( bin ), st_action_group_name, G_ACTION( priv->settlement_action ),
					_( "Settlement page" ));
			ofa_buttons_box_append_button(
					priv->buttonsbox,
					ofa_iactionable_set_button(
							OFA_IACTIONABLE( bin ), st_action_group_name, G_ACTION( priv->settlement_action ),
							_( "Settlement..." )));
			g_signal_connect( priv->settlement_action, "activate", G_CALLBACK( action_on_settlement_activated ), bin );
			g_simple_action_set_enabled( priv->settlement_action, TRUE );
			break;

		case ACCOUNT_ACTION_RECONCILIATION:
			ofa_iactionable_set_menu_item(
					OFA_IACTIONABLE( bin ), st_action_group_name, G_ACTION( priv->reconciliation_action ),
					_( "Reconciliation page" ));
			ofa_buttons_box_append_button(
					priv->buttonsbox,
					ofa_iactionable_set_button(
							OFA_IACTIONABLE( bin ), st_action_group_name, G_ACTION( priv->reconciliation_action ),
							_( "_Reconciliation..." )));
			g_signal_connect( priv->reconciliation_action, "activate", G_CALLBACK( action_on_reconciliation_activated ), bin );
			g_simple_action_set_enabled( priv->reconciliation_action, TRUE );
			break;

		default:
			break;
	}
}

static void
action_update_enabled( ofaAccountFrameBin *self, ofoAccount *account )
{
	ofaAccountFrameBinPrivate *priv;
	gboolean has_account;

	priv = ofa_account_frame_bin_get_instance_private( self );

	has_account = account && OFO_IS_ACCOUNT( account );

	g_simple_action_set_enabled( priv->new_action, is_new_allowed( self ));

	g_simple_action_set_enabled( priv->update_action, has_account );

	g_simple_action_set_enabled( priv->delete_action, is_delete_allowed( self, account ));

	g_simple_action_set_enabled( priv->view_entries_action, has_account && !ofo_account_is_root( account ));

	g_simple_action_set_enabled( priv->settlement_action, has_account && ofo_account_is_settleable( account ));

	g_simple_action_set_enabled( priv->reconciliation_action, has_account && ofo_account_is_reconciliable( account ));
}

static void
action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaAccountFrameBin *self )
{
	do_insert_account( self );
}

static void
action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaAccountFrameBin *self )
{
	ofoAccount *account;

	account = ofa_account_frame_bin_get_selected( self );
	if( account ){
		do_update_account( self, account );
	}
}

static void
action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaAccountFrameBin *self )
{
	ofoAccount *account;

	account = ofa_account_frame_bin_get_selected( self );
	if( account ){
		g_return_if_fail( is_delete_allowed( self, account ));
		do_delete_account( self, account );
	}
}

static void
action_on_view_entries_activated( GSimpleAction *action, GVariant *empty, ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;
	ofaIThemeManager *manager;
	ofoAccount *account;
	ofaPage *page;

	priv = ofa_account_frame_bin_get_instance_private( self );

	account = ofa_account_frame_bin_get_selected( self );
	manager = ofa_igetter_get_theme_manager( priv->getter );
	page = ofa_itheme_manager_activate( manager, OFA_TYPE_ENTRY_PAGE );
	ofa_entry_page_display_entries(
			OFA_ENTRY_PAGE( page ), OFO_TYPE_ACCOUNT, ofo_account_get_number( account ), NULL, NULL );
}

static void
action_on_settlement_activated( GSimpleAction *action, GVariant *empty, ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;
	ofaIThemeManager *manager;
	ofoAccount *account;
	ofaPage *page;

	priv = ofa_account_frame_bin_get_instance_private( self );

	account = ofa_account_frame_bin_get_selected( self );
	manager = ofa_igetter_get_theme_manager( priv->getter );
	page = ofa_itheme_manager_activate( manager, OFA_TYPE_SETTLEMENT_PAGE );
	ofa_settlement_page_set_account( OFA_SETTLEMENT_PAGE( page ), ofo_account_get_number( account ));
}

static void
action_on_reconciliation_activated( GSimpleAction *action, GVariant *empty, ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;
	ofaIThemeManager *manager;
	ofoAccount *account;
	ofaPage *page;

	priv = ofa_account_frame_bin_get_instance_private( self );

	account = ofa_account_frame_bin_get_selected( self );
	manager = ofa_igetter_get_theme_manager( priv->getter );
	page = ofa_itheme_manager_activate( manager, OFA_TYPE_RECONCIL_PAGE );
	ofa_reconcil_page_set_account( OFA_RECONCIL_PAGE( page ), ofo_account_get_number( account ));
}

static gboolean
is_new_allowed( ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;
	gboolean ok;

	priv = ofa_account_frame_bin_get_instance_private( self );

	ok = priv->is_writable;

	return( ok );
}

static gboolean
is_delete_allowed( ofaAccountFrameBin *self, ofoAccount *account )
{
	ofaAccountFrameBinPrivate *priv;
	gboolean ok;

	priv = ofa_account_frame_bin_get_instance_private( self );

	ok = priv->is_writable && account && ofo_account_is_deletable( account );

	return( ok );
}

static void
do_insert_account( ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;
	ofoAccount *account_obj;
	GtkWindow *toplevel;

	priv = ofa_account_frame_bin_get_instance_private( self );

	account_obj = ofo_account_new();

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));

	ofa_account_properties_run( priv->getter, toplevel, account_obj );
}

static void
do_update_account( ofaAccountFrameBin *self, ofoAccount *account )
{
	ofaAccountFrameBinPrivate *priv;
	GtkWindow *toplevel;

	priv = ofa_account_frame_bin_get_instance_private( self );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));

	ofa_account_properties_run(priv->getter, toplevel, account );
}

static void
do_delete_account( ofaAccountFrameBin *self, ofoAccount *account )
{
	ofaAccountFrameBinPrivate *priv;
	gchar *account_id;

	priv = ofa_account_frame_bin_get_instance_private( self );

	g_return_if_fail( priv->is_writable &&
			account && OFO_IS_ACCOUNT( account ) && ofo_account_is_deletable( account ));

	account_id = g_strdup( ofo_account_get_number( account ));

	if( delete_confirmed( self, account ) && ofo_account_delete( account )){

			/* nothing to do here, all being managed by signal hub_handlers
			 * just reset the selection
			 * asking for selection of the just deleted account makes
			 * almost sure that we are going to select the most close
			 * row */
			ofa_account_frame_bin_set_selected( self, account_id );
	}

	g_free( account_id );
}

/*
 * - this is a root account with children and the preference is set so
 *   that all accounts will be deleted
 * - this is a root account and the preference is not set
 * - this is a detail account
 */
static gboolean
delete_confirmed( ofaAccountFrameBin *self, ofoAccount *account )
{
	gchar *msg;
	gboolean delete_ok;

	if( ofo_account_is_root( account )){
		if( ofo_account_has_children( account ) &&
				ofa_prefs_account_delete_root_with_children()){
			msg = g_strdup_printf( _(
					"You are about to delete the %s - %s account.\n"
					"This is a root account which has children.\n"
					"Are you sure ?" ),
					ofo_account_get_number( account ),
					ofo_account_get_label( account ));
		} else {
			msg = g_strdup_printf( _(
					"You are about to delete the %s - %s account.\n"
					"This is a root account. Are you sure ?" ),
					ofo_account_get_number( account ),
					ofo_account_get_label( account ));
		}
	} else {
		msg = g_strdup_printf( _( "Are you sure you want delete the '%s - %s' account ?" ),
					ofo_account_get_number( account ),
					ofo_account_get_label( account ));
	}

	delete_ok = my_utils_dialog_question( msg, _( "_Delete" ));

	g_free( msg );

	return( delete_ok );
}

/*
 * Is triggered by the #ofaTreeStore when a new row is inserted.
 * We try to optimize the search by keeping the class of the last
 * inserted row;
 */
static void
store_on_row_inserted( GtkTreeModel *tmodel, GtkTreePath *path, GtkTreeIter *iter, ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;
	gchar *number;
	gint class_num;

	priv = ofa_account_frame_bin_get_instance_private( self );

	gtk_tree_model_get( tmodel, iter, ACCOUNT_COL_NUMBER, &number, -1 );
	class_num = ofo_account_get_class_from_number( number );
	g_free( number );

	if( class_num != priv->prev_class ){
		book_get_page_by_class( self, class_num, TRUE );
		priv->prev_class = class_num;
	}
}

/**
 * ofa_account_frame_bin_set_settings_key:
 * @bin: this #ofaAccountFrameBin instance.
 * @key: [allow-none]: the prefix of the settings key.
 *
 * Setup the setting key, or reset it to its default if %NULL.
 */
void
ofa_account_frame_bin_set_settings_key( ofaAccountFrameBin *bin, const gchar *key )
{
	static const gchar *thisfn = "ofa_account_frame_bin_set_settings_key";
	ofaAccountFrameBinPrivate *priv;

	g_debug( "%s: bin=%p, key=%s", thisfn, ( void * ) bin, key );

	g_return_if_fail( bin && OFA_IS_ACCOUNT_FRAME_BIN( bin ));

	priv = ofa_account_frame_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->settings_key );
	priv->settings_key = g_strdup( key );
}

/**
 * ofa_account_frame_bin_set_getter:
 * @bin: this #ofaAccountFrameBin instance.
 * @getter: a permanent #ofaIGetter.
 *
 * Setup the getter.
 * This should be done as the last step of the initialization, because
 * this will load the store and initialize the displayed columns.
 */
void
ofa_account_frame_bin_set_getter( ofaAccountFrameBin *bin, ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_account_frame_bin_set_getter";
	ofaAccountFrameBinPrivate *priv;

	g_debug( "%s: bin=%p, getter=%p", thisfn, ( void * ) bin, ( void * ) getter );

	g_return_if_fail( bin && OFA_IS_ACCOUNT_FRAME_BIN( bin ));
	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	priv = ofa_account_frame_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	priv->getter = getter;

	set_getter_hub( bin );
	set_getter_store( bin );

	book_expand_all( bin );
	gtk_notebook_set_current_page( GTK_NOTEBOOK( priv->notebook ), 0 );
}

static void
set_getter_hub( ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;

	priv = ofa_account_frame_bin_get_instance_private( self );

	priv->hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	priv->is_writable = ofa_hub_dossier_is_writable( priv->hub );

	hub_connect_to_signaling_system( self );
}

static void
set_getter_store( ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;
	gulong handler;

	priv = ofa_account_frame_bin_get_instance_private( self );

	priv->store = ofa_account_store_new( priv->hub );

	handler = g_signal_connect( priv->store, "ofa-row-inserted", G_CALLBACK( store_on_row_inserted ), self );
	priv->store_handlers = g_list_prepend( priv->store_handlers, ( gpointer ) handler );

	ofa_tree_store_load_dataset( OFA_TREE_STORE( priv->store ), priv->hub );
}

static void
hub_connect_to_signaling_system( ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;
	gulong handler;

	priv = ofa_account_frame_bin_get_instance_private( self );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_NEW, G_CALLBACK( hub_on_new_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_UPDATED, G_CALLBACK( hub_on_updated_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_DELETED, G_CALLBACK( hub_on_deleted_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_RELOAD, G_CALLBACK( hub_on_reload_dataset ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
hub_on_new_object( ofaHub *hub, ofoBase *object, ofaAccountFrameBin *self )
{
	static const gchar *thisfn = "ofa_account_frame_bin_hub_on_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn, ( void * ) hub,
					( void * ) object, G_OBJECT_TYPE_NAME( object ),
					( void * ) self );

	if( OFO_IS_CLASS( object )){
		hub_on_updated_class_label( self, OFO_CLASS( object ));
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaAccountFrameBin *self )
{
	static const gchar *thisfn = "ofa_account_frame_bin_hub_on_updated_object";

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn, ( void * ) hub,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), prev_id,
					( void * ) self );

	if( OFO_IS_CLASS( object )){
		hub_on_updated_class_label( self, OFO_CLASS( object ));
	}
}

/*
 * a class label has changed : update the corresponding tab label
 */
static void
hub_on_updated_class_label( ofaAccountFrameBin *self, ofoClass *class )
{
	ofaAccountFrameBinPrivate *priv;
	GtkWidget *page_w;
	gint class_num;

	priv = ofa_account_frame_bin_get_instance_private( self );

	class_num = ofo_class_get_number( class );
	page_w = book_get_page_by_class( self, class_num, FALSE );
	if( page_w ){
		g_return_if_fail( GTK_IS_WIDGET( page_w ));
		gtk_notebook_set_tab_label_text( GTK_NOTEBOOK( priv->notebook ), page_w, ofo_class_get_label( class ));
	}
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaAccountFrameBin *self )
{
	static const gchar *thisfn = "ofa_account_frame_bin_hub_on_deleted_object";

	g_debug( "%s: hub=%p, object=%p (%s), book=%p",
			thisfn, ( void * ) hub,
					( void * ) object, G_OBJECT_TYPE_NAME( object ),
					( void * ) self );

	if( OFO_IS_CLASS( object )){
		hub_on_deleted_class_label( self, OFO_CLASS( object ));
	}
}

static void
hub_on_deleted_class_label( ofaAccountFrameBin *self, ofoClass *class )
{
	ofaAccountFrameBinPrivate *priv;
	GtkWidget *page_w;
	gint class_num;

	priv = ofa_account_frame_bin_get_instance_private( self );

	class_num = ofo_class_get_number( class );
	page_w = book_get_page_by_class( self, class_num, FALSE );
	if( page_w ){
		g_return_if_fail( GTK_IS_WIDGET( page_w ));
		gtk_notebook_set_tab_label_text( GTK_NOTEBOOK( priv->notebook ), page_w, st_class_labels[class_num-1] );
	}
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
hub_on_reload_dataset( ofaHub *hub, GType type, ofaAccountFrameBin *self )
{
	static const gchar *thisfn = "ofa_account_frame_bin_hub_on_reload_dataset";

	g_debug( "%s: hub=%p, type=%lu, self=%p",
			thisfn, ( void * ) hub, type, ( void * ) self );

	book_expand_all( self );
}

/*
 * ofaIActionable interface management
 */
static void
iactionable_iface_init( ofaIActionableInterface *iface )
{
	static const gchar *thisfn = "ofa_account_frame_bin_iactionable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iactionable_get_interface_version;
}

static guint
iactionable_get_interface_version( void )
{
	return( 1 );
}

/*
 * ofaIActioner interface management
 */
static void
iactioner_iface_init( ofaIActionerInterface *iface )
{
	static const gchar *thisfn = "ofa_account_frame_bin_iactioner_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iactioner_get_interface_version;
}

static guint
iactioner_get_interface_version( void )
{
	return( 1 );
}
