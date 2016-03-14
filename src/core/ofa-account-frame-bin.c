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

#include "api/my-utils.h"
#include "api/ofa-buttons-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-page.h"
#include "api/ofa-preferences.h"
#include "api/ofo-account.h"
#include "api/ofo-class.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"

#include "core/ofa-account-properties.h"
#include "core/ofa-account-frame-bin.h"
#include "core/ofa-account-store.h"
#include "core/ofa-main-window.h"

#include "ui/ofa-entry-page.h"
#include "ui/ofa-reconcil-page.h"
#include "ui/ofa-settlement-page.h"

/* private instance data
 */
struct _ofaAccountFrameBinPrivate {
	gboolean             dispose_has_run;

	/* initialization
	 */
	const ofaMainWindow *main_window;

	/* runtime
	 */
	ofaHub              *hub;
	GList               *hub_handlers;
	gboolean             is_current;	/* whether the dossier is current */
	ofaAccountStore     *store;
	GList               *store_handlers;
	GtkTreeCellDataFunc  cell_fn;
	void                *cell_data;
	gint                 prev_class;

	/* UI
	 */
	GtkWidget           *grid;
	GtkWidget           *notebook;
	ofaButtonsBox       *buttonsbox;
	GList               *buttons;		/* list of sButton structs */
};

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

/* a structure to descriibe each displayed button
 * @sensitive: if FALSE, the button is never sensitive, whatever be the
 *  current conditions.
 */
typedef struct {
	ofeAccountFrameBtn id;
	GtkWidget         *btn;
	gboolean           sensitive;
}
	sButton;

/* the class number is attached to each page of the classes notebook,
 * and also attached to the underlying treemodelfilter
 */
#define DATA_PAGE_CLASS                 "ofa-account-frame-bin-data-page-class"

/* the column identifier is attached to each column header
 */
#define DATA_COLUMN_ID                  "ofa-account-frame-bin-data-column-id"

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void       setup_bin( ofaAccountFrameBin *self );
static GtkWidget *book_get_page_by_class( ofaAccountFrameBin *self, gint class_num, gboolean create );
static GtkWidget *book_create_page( ofaAccountFrameBin *self, gint class );
static void       book_expand_all( ofaAccountFrameBin *self );
static void       book_on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaAccountFrameBin *self );
static gboolean   book_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaAccountFrameBin *self );
static GtkWidget *page_add_treeview( ofaAccountFrameBin *self, GtkWidget *page );
static void       page_add_columns( ofaAccountFrameBin *self, GtkTreeView *tview );
static GtkWidget *page_get_treeview( const ofaAccountFrameBin *self );
static void       tview_on_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRenderer *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaAccountFrameBin *self );
static void       tview_cell_renderer_text( GtkCellRendererText *cell, gboolean is_root, gint level, gboolean is_error );
static gboolean   tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, GtkWidget *page );
static void       tview_on_row_selected( GtkTreeSelection *selection, ofaAccountFrameBin *self );
static void       tview_on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaAccountFrameBin *self );
static gboolean   tview_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaAccountFrameBin *self );
static void       tview_collapse_node( ofaAccountFrameBin *self, GtkWidget *widget );
static void       tview_expand_node( ofaAccountFrameBin *self, GtkWidget *widget );
static void       tview_on_key_insert( ofaAccountFrameBin *self );
static void       tview_on_key_delete( ofaAccountFrameBin *self );
static void       tview_select_row_by_number( ofaAccountFrameBin *self, const gchar *number );
static void       tview_select_row_by_iter( ofaAccountFrameBin *self, GtkTreeView *tview, GtkTreeModel *tfilter, GtkTreeIter *iter );
static GtkWidget *button_add( ofaAccountFrameBin *self, ofeAccountFrameBtn id, const gchar *label, gboolean sensitive, GCallback cb );
static sButton   *button_find_by_id( GList **list, ofeAccountFrameBtn id, gboolean create );
static void       button_update_sensitivity( ofaAccountFrameBin *self, const gchar *account_id );
static void       button_on_new_clicked( GtkButton *button, ofaAccountFrameBin *self );
static void       button_on_properties_clicked( GtkButton *button, ofaAccountFrameBin *self );
static void       button_on_delete_clicked( GtkButton *button, ofaAccountFrameBin *self );
static void       button_on_view_entries_clicked( GtkButton *button, ofaAccountFrameBin *self );
static void       button_on_settlement_clicked( GtkButton *button, ofaAccountFrameBin *self );
static void       button_on_reconciliation_clicked( GtkButton *button, ofaAccountFrameBin *self );
static gboolean   is_new_allowed( ofaAccountFrameBin *self, sButton *sbtn );
static gboolean   is_delete_allowed( ofaAccountFrameBin *self, sButton *sbtn, ofoAccount *selected );
static void       do_insert_account( ofaAccountFrameBin *self );
static void       do_update_account( ofaAccountFrameBin *self, const gchar *account_id );
static void       do_delete_account( ofaAccountFrameBin *self, const gchar *account_id );
static gboolean   delete_confirmed( ofaAccountFrameBin *self, ofoAccount *account );
static void       store_on_row_inserted( GtkTreeModel *tmodel, GtkTreePath *path, GtkTreeIter *iter, ofaAccountFrameBin *self );
static void       hub_connect_to_signaling_system( ofaAccountFrameBin *self );
static void       hub_on_new_object( ofaHub *hub, ofoBase *object, ofaAccountFrameBin *self );
static void       hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaAccountFrameBin *self );
static void       hub_on_updated_class_label( ofaAccountFrameBin *self, ofoClass *class );
static void       hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaAccountFrameBin *self );
static void       hub_on_deleted_class_label( ofaAccountFrameBin *self, ofoClass *class );
static void       hub_on_reload_dataset( ofaHub *hub, GType type, ofaAccountFrameBin *self );
static void       free_button( sButton *sbtn );

G_DEFINE_TYPE_EXTENDED( ofaAccountFrameBin, ofa_account_frame_bin, GTK_TYPE_BIN, 0, \
		G_ADD_PRIVATE( ofaAccountFrameBin ));

static void
account_frame_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_frame_bin_finalize";
	ofaAccountFrameBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_FRAME_BIN( instance ));

	priv = ofa_account_frame_bin_get_instance_private( OFA_ACCOUNT_FRAME_BIN( instance ));

	/* free data members here */
	g_list_free_full( priv->buttons, ( GDestroyNotify ) free_button );

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
		ofa_hub_disconnect_handlers( priv->hub, priv->hub_handlers );

		/* disconnect from ofaAccountStore */
		if( priv->store && OFA_IS_ACCOUNT_STORE( priv->store )){
			for( it=priv->store_handlers ; it ; it=it->next ){
				g_signal_handler_disconnect( priv->store, ( gulong ) it->data );
			}
		}
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
	priv->prev_class = -1;
}

static void
ofa_account_frame_bin_class_init( ofaAccountFrameBinClass *klass )
{
	static const gchar *thisfn = "ofa_account_frame_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_frame_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_frame_bin_finalize;

	/**
	 * ofaAccountFrameBin::changed:
	 *
	 * This signal is sent when the selection is changed.
	 *
	 * Argument is the selected account number.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAccountFrameBin *frame,
	 * 						const gchar      *account_id,
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
				G_TYPE_STRING );

	/**
	 * ofaAccountFrameBin::activated:
	 *
	 * This signal is sent when the selection is activated.
	 *
	 * Argument is the selected account number.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAccountFrameBin *frame,
	 * 						const gchar      *account_id,
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
				G_TYPE_STRING );
}

/**
 * ofa_account_frame_bin_new:
 *
 * Creates the structured content, i.e. The accounts notebook on the
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
ofa_account_frame_bin_new( const ofaMainWindow *main_window  )
{
	ofaAccountFrameBin *self;
	ofaAccountFrameBinPrivate *priv;

	self = g_object_new( OFA_TYPE_ACCOUNT_FRAME_BIN, NULL );

	priv = ofa_account_frame_bin_get_instance_private( self );

	priv->main_window = main_window;

	setup_bin( self );

	return( self );
}

/*
 * create the top grid which contains the accounts notebook and the
 * buttons, and attach it to our #GtkBin
 */
static void
setup_bin( ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;
	gulong handler;
	ofoDossier *dossier;

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

	/* account store */
	priv->hub = ofa_main_window_get_hub( priv->main_window );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	priv->store = ofa_account_store_new( priv->hub );

	handler = g_signal_connect( priv->store, "ofa-row-inserted", G_CALLBACK( store_on_row_inserted ), self );
	priv->store_handlers = g_list_prepend( priv->store_handlers, ( gpointer ) handler );

	ofa_tree_store_load_dataset( OFA_TREE_STORE( priv->store ));

	/* runtime */
	dossier = ofa_hub_get_dossier( priv->hub );
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	priv->is_current = ofo_dossier_is_current( dossier );

	/* hub signaling system */
	hub_connect_to_signaling_system( self );

	book_expand_all( self );
	gtk_notebook_set_current_page( GTK_NOTEBOOK( priv->notebook ), 0 );
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
		page_class = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( page_widget ), DATA_PAGE_CLASS ));
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
	GtkWidget *frame, *scrolled, *tview, *label;
	ofoClass *class_obj;
	const gchar *class_label;
	gchar *str;
	gint page_num;

	g_debug( "%s: self=%p, class_num=%d", thisfn, ( void * ) self, class_num );

	priv = ofa_account_frame_bin_get_instance_private( self );

	/* a frame as the top widget of the notebook page */
	frame = gtk_frame_new( NULL );
	gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_IN );

	/* attach data to the notebook page */
	g_object_set_data( G_OBJECT( frame ), DATA_PAGE_CLASS, GINT_TO_POINTER( class_num ));

	/* then a scrolled window inside the frame */
	scrolled = gtk_scrolled_window_new( NULL, NULL );
	gtk_container_add( GTK_CONTAINER( frame ), scrolled );
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW( scrolled ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );

	/* then create the treeview inside the scrolled window */
	tview = page_add_treeview( self, frame );
	gtk_container_add( GTK_CONTAINER( scrolled ), tview );

	/* then create the columns in the treeview */
	page_add_columns( self, GTK_TREE_VIEW( tview ));

	/* last add the page to the notebook */
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

	page_num = gtk_notebook_append_page( GTK_NOTEBOOK( priv->notebook ), frame, label );
	if( page_num == -1 ){
		g_warning( "%s: unable to add a page to the notebook for class=%d", thisfn, class_num );
		return( NULL );
	}
	gtk_notebook_set_tab_reorderable( GTK_NOTEBOOK( priv->notebook ), frame, TRUE );

	return( frame );
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
 * we have switch to this given page (wpage, npage)
 * just setup the selection
 */
static void
book_on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaAccountFrameBin *self )
{
	GtkWidget *tview;
	GtkTreeSelection *select;

	tview = my_utils_container_get_child_by_type( GTK_CONTAINER( wpage ), GTK_TYPE_TREE_VIEW );
	if( tview ){
		g_return_if_fail( GTK_IS_TREE_VIEW( tview ));
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
		tview_on_row_selected( select, self );
	}
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

/*
 * creates the treeview
 * attach it to the container parent (the scrolled window)
 * setup the model filter
 */
static GtkWidget *
page_add_treeview( ofaAccountFrameBin *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_account_frame_bin_create_treeview";
	ofaAccountFrameBinPrivate *priv;
	GtkWidget *tview;
	GtkTreeModel *tfilter;
	GtkTreeSelection *select;

	priv = ofa_account_frame_bin_get_instance_private( self );

	tview = gtk_tree_view_new();
	gtk_widget_set_hexpand( tview, TRUE );
	gtk_widget_set_vexpand( tview, TRUE );
	gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( tview ), TRUE );

	tfilter = gtk_tree_model_filter_new( GTK_TREE_MODEL( priv->store ), NULL );
	g_debug( "%s: store=%p, tfilter=%p", thisfn, ( void * ) priv->store, ( void * ) tfilter );
	gtk_tree_model_filter_set_visible_func(
			GTK_TREE_MODEL_FILTER( tfilter ),
			( GtkTreeModelFilterVisibleFunc ) tview_is_visible_row, page, NULL );

	gtk_tree_view_set_model( GTK_TREE_VIEW( tview ), tfilter );
	g_object_unref( tfilter );

	g_signal_connect( tview, "row-activated", G_CALLBACK( tview_on_row_activated), self );
	g_signal_connect( tview, "key-press-event", G_CALLBACK( tview_on_key_pressed ), self );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( select, "changed", G_CALLBACK( tview_on_row_selected ), self );

	return( tview );
}

/*
 * creates the columns in the GtkTreeView
 */
static void
page_add_columns( ofaAccountFrameBin *self, GtkTreeView *tview )
{
	ofaAccountFrameBinPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkTreeCellDataFunc fn_cell;
	void *fn_data;

	priv = ofa_account_frame_bin_get_instance_private( self );

	fn_cell = priv->cell_fn ? priv->cell_fn : ( GtkTreeCellDataFunc ) tview_on_cell_data_func;
	fn_data = priv->cell_fn ? priv->cell_data : self;

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Number" ), cell, "text", ACCOUNT_COL_NUMBER, NULL );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ACCOUNT_COL_NUMBER ));
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func( column, cell, fn_cell, fn_data, NULL );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ), cell, "text", ACCOUNT_COL_LABEL, NULL );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ACCOUNT_COL_LABEL ));
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func( column, cell, fn_cell, fn_data, NULL );

	cell = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(
				"", cell, "pixbuf", ACCOUNT_COL_NOTES_PNG, NULL );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ACCOUNT_COL_NOTES_PNG ));
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func( column, cell, fn_cell, fn_data, NULL );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
				_( "C" ), cell, "text", ACCOUNT_COL_CLOSED, NULL );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ACCOUNT_COL_CLOSED ));
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func( column, cell, fn_cell, fn_data, NULL );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
				_( "S" ), cell, "text", ACCOUNT_COL_SETTLEABLE, NULL );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ACCOUNT_COL_SETTLEABLE ));
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(  column, cell, fn_cell, fn_data, NULL );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
				_( "R" ), cell, "text", ACCOUNT_COL_RECONCILIABLE, NULL );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ACCOUNT_COL_RECONCILIABLE ));
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func( column, cell, fn_cell, fn_data, NULL );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
				_( "F" ), cell, "text", ACCOUNT_COL_FORWARD, NULL );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ACCOUNT_COL_FORWARD ));
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func( column, cell, fn_cell, fn_data, NULL );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new();
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ACCOUNT_COL_EXE_DEBIT ));
	gtk_tree_view_column_pack_end( column, cell, TRUE );
	gtk_tree_view_column_set_title( column, _( "Debit" ));
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_add_attribute( column, cell, "text", ACCOUNT_COL_EXE_DEBIT );
	gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func( column, cell, fn_cell, fn_data, NULL );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new();
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ACCOUNT_COL_EXE_CREDIT ));
	gtk_tree_view_column_pack_end( column, cell, TRUE );
	gtk_tree_view_column_set_title( column, _( "Credit" ));
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_add_attribute( column, cell, "text", ACCOUNT_COL_EXE_CREDIT );
	gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func( column, cell, fn_cell, fn_data, NULL );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new();
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ACCOUNT_COL_EXE_SOLDE ));
	gtk_tree_view_column_pack_end( column, cell, TRUE );
	gtk_tree_view_column_set_title( column, _( "Solde" ));
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_add_attribute( column, cell, "text", ACCOUNT_COL_EXE_SOLDE );
	gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func( column, cell, fn_cell, fn_data, NULL );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( cell, 0.0, 0.5 );
	column = gtk_tree_view_column_new();
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ACCOUNT_COL_CURRENCY ));
	gtk_tree_view_column_pack_end( column, cell, FALSE );
	gtk_tree_view_column_set_alignment( column, 0.0 );
	gtk_tree_view_column_add_attribute( column, cell, "text", ACCOUNT_COL_CURRENCY );
	gtk_tree_view_column_set_min_width( column, 40 );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func( column, cell, fn_cell, fn_data, NULL );
}

static GtkWidget *
page_get_treeview( const ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;
	gint page_n;
	GtkWidget *page_w;
	GtkWidget *tview;

	priv = ofa_account_frame_bin_get_instance_private( self );
	tview = NULL;
	page_n = gtk_notebook_get_current_page( GTK_NOTEBOOK( priv->notebook ));

	if( page_n >= 0 ){
		page_w = gtk_notebook_get_nth_page( GTK_NOTEBOOK( priv->notebook ), page_n );
		g_return_val_if_fail( page_w && GTK_IS_CONTAINER( page_w ), NULL );

		tview = my_utils_container_get_child_by_type(
								GTK_CONTAINER( page_w ), GTK_TYPE_TREE_VIEW );
		g_return_val_if_fail( tview && GTK_IS_TREE_VIEW( tview ), NULL );
	}

	return( tview );
}

/**
 * ofa_account_frame_bin_get_current_treeview:
 * @bin:
 *
 * Returns the treeview associated to the current page.
 */
GtkWidget *
ofa_account_frame_bin_get_current_treeview( const ofaAccountFrameBin *bin )
{
	static const gchar *thisfn = "ofa_account_frame_bin_get_current_treeview";
	ofaAccountFrameBinPrivate *priv;
	GtkWidget *tview;

	g_debug( "%s: bin=%p", thisfn, ( void * ) bin );

	g_return_val_if_fail( bin && OFA_IS_ACCOUNT_FRAME_BIN( bin ), NULL );

	priv = ofa_account_frame_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	tview = page_get_treeview( bin );

	return( tview );
}

/**
 * ofa_account_frame_bin_get_selected:
 * @bin:
 *
 * Returns: the currently selected account number, as a newly allocated
 * string which should be g_free() by the caller.
 */
gchar *
ofa_account_frame_bin_get_selected( ofaAccountFrameBin *bin )
{
	static const gchar *thisfn = "ofa_account_frame_bin_get_selected";
	ofaAccountFrameBinPrivate *priv;
	gchar *account;
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	g_debug( "%s: bin=%p", thisfn, ( void * ) bin );

	g_return_val_if_fail( bin && OFA_IS_ACCOUNT_FRAME_BIN( bin ), NULL );

	priv = ofa_account_frame_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	account = NULL;
	tview = ( GtkTreeView * ) page_get_treeview( bin );

	if( tview ){
		select = gtk_tree_view_get_selection( tview );
		if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
			gtk_tree_model_get( tmodel, &iter, ACCOUNT_COL_NUMBER, &account, -1 );
		}
	}

	return( account );
}

/**
 * ofa_account_frame_bin_set_selected:
 * @bin: this #ofaAccountFrameBin instance
 * @number: the account identifier to be selected.
 *
 * Let the user reset the selection after the end of setup and
 * initialization phases
 */
void
ofa_account_frame_bin_set_selected( ofaAccountFrameBin *bin, const gchar *number )
{
	static const gchar *thisfn = "ofa_account_frame_bin_set_selected";
	ofaAccountFrameBinPrivate *priv;

	g_debug( "%s: bin=%p, number=%s", thisfn, ( void * ) bin, number );

	g_return_if_fail( bin && OFA_IS_ACCOUNT_FRAME_BIN( bin ));

	priv = ofa_account_frame_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	tview_select_row_by_number( bin, number );
}

/**
 * ofa_account_frame_bin_set_cell_data_func:
 * @bin:
 * @fn_cell:
 * @user_data:
 *
 */
void
ofa_account_frame_bin_set_cell_data_func( ofaAccountFrameBin *bin, GtkTreeCellDataFunc fn_cell, void *user_data )
{
	ofaAccountFrameBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_ACCOUNT_FRAME_BIN( bin ));

	priv = ofa_account_frame_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	priv->cell_fn = fn_cell;
	priv->cell_data = user_data;
}

/**
 * ofa_account_frame_bin_cell_data_render:
 *
 * level 1: not displayed (should not appear)
 * level 2 and root: bold, colored background
 * level 3 and root: colored background
 * other root: italic
 *
 * detail accounts who have no currency are red written.
 */
void
ofa_account_frame_bin_cell_data_render( ofaAccountFrameBin *bin,
							GtkTreeViewColumn *tcolumn, GtkCellRenderer *cell, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	ofaAccountFrameBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_ACCOUNT_FRAME_BIN( bin ));
	g_return_if_fail( tcolumn && GTK_IS_TREE_VIEW_COLUMN( tcolumn ));
	g_return_if_fail( cell && GTK_IS_CELL_RENDERER( cell ));
	g_return_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ));

	priv = ofa_account_frame_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	tview_on_cell_data_func( tcolumn, cell, tmodel, iter, bin );
}

static void
tview_on_cell_data_func( GtkTreeViewColumn *tcolumn,
							GtkCellRenderer *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
							ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;
	gchar *account_num;
	ofoAccount *account_obj;
	GString *number;
	gint level;
	gint column;
	ofoCurrency *currency;
	gboolean is_root, is_error;

	g_return_if_fail( GTK_IS_CELL_RENDERER( cell ));

	priv = ofa_account_frame_bin_get_instance_private( self );

	gtk_tree_model_get( tmodel, iter,
			ACCOUNT_COL_NUMBER, &account_num, ACCOUNT_COL_OBJECT, &account_obj, -1 );
	g_return_if_fail( account_obj && OFO_IS_ACCOUNT( account_obj ));
	g_object_unref( account_obj );

	level = ofo_account_get_level_from_number( ofo_account_get_number( account_obj ));
	g_return_if_fail( level >= 2 );

	is_root = ofo_account_is_root( account_obj );

	is_error = FALSE;
	if( !is_root ){
		currency = ofo_currency_get_by_code( priv->hub, ofo_account_get_currency( account_obj ));
		is_error |= !currency;
	}

	column = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( tcolumn ), DATA_COLUMN_ID ));
	if( column == ACCOUNT_COL_NUMBER ){
		number = g_string_new( " " );
		g_string_append_printf( number, "%s", account_num );
		g_object_set( G_OBJECT( cell ), "text", number->str, NULL );
		g_string_free( number, TRUE );
	}

	if( GTK_IS_CELL_RENDERER_TEXT( cell )){
		tview_cell_renderer_text( GTK_CELL_RENDERER_TEXT( cell ), is_root, level, is_error );

	} else if( GTK_IS_CELL_RENDERER_PIXBUF( cell )){

		/*if( ofo_account_is_root( account_obj ) && level == 2 ){
			path = gtk_tree_model_get_path( tmodel, iter );
			tview = gtk_tree_view_column_get_tree_view( tcolumn );
			gtk_tree_view_get_cell_area( tview, path, tcolumn, &rc );
			gtk_tree_path_free( path );
		}*/
	}
}

static void
tview_cell_renderer_text( GtkCellRendererText *cell, gboolean is_root, gint level, gboolean is_error )
{
	GdkRGBA color;

	g_return_if_fail( GTK_IS_CELL_RENDERER_TEXT( cell ));

	g_object_set( G_OBJECT( cell ),
						"style-set",      FALSE,
						"weight-set",     FALSE,
						"background-set", FALSE,
						"foreground-set", FALSE,
						NULL );

	if( is_root ){
		if( level == 2 ){
			gdk_rgba_parse( &color, "#c0ffff" );
			g_object_set( G_OBJECT( cell ), "background-rgba", &color, NULL );
			g_object_set( G_OBJECT( cell ), "weight", PANGO_WEIGHT_BOLD, NULL );

		} else if( level == 3 ){
			gdk_rgba_parse( &color, "#0000ff" );
			g_object_set( G_OBJECT( cell ), "foreground-rgba", &color, NULL );
			g_object_set( G_OBJECT( cell ), "weight", PANGO_WEIGHT_BOLD, NULL );

		} else {
			gdk_rgba_parse( &color, "#0000ff" );
			g_object_set( G_OBJECT( cell ), "foreground-rgba", &color, NULL );
			g_object_set( G_OBJECT( cell ), "style", PANGO_STYLE_ITALIC, NULL );
		}

	} else if( is_error ){
		gdk_rgba_parse( &color, "#800000" );
		g_object_set( G_OBJECT( cell ), "foreground-rgba", &color, NULL );
	}
}

/*
 * tmodel here is the ofaTreeStore
 */
static gboolean
tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, GtkWidget *page )
{
	gchar *number;
	gint class_num, filter_class;
	gboolean is_visible;

	is_visible = FALSE;

	gtk_tree_model_get( tmodel, iter, ACCOUNT_COL_NUMBER, &number, -1 );
	class_num = ofo_account_get_class_from_number( number );
	g_free( number );

	filter_class = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( page ), DATA_PAGE_CLASS ));
	is_visible = ( filter_class == class_num );

	return( is_visible );
}

static void
tview_on_row_selected( GtkTreeSelection *selection, ofaAccountFrameBin *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *account;

	account = NULL;

	/* selection may be null when called from button_on_delete_clicked() */
	if( selection ){
		if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
			gtk_tree_model_get( tmodel, &iter, ACCOUNT_COL_NUMBER, &account, -1 );
			button_update_sensitivity( self, account );
			g_signal_emit_by_name( self, "ofa-changed", account );
			g_free( account );
		}
	}
}

static void
tview_on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaAccountFrameBin *self )
{
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *account;

	account = NULL;

	select = gtk_tree_view_get_selection( tview );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, ACCOUNT_COL_NUMBER, &account, -1 );
		g_signal_emit_by_name( self, "ofa-activated", account );
		g_free( account );
	}
}

/*
 * Returns :
 * TRUE to stop other hub_handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
tview_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaAccountFrameBin *self )
{
	gboolean stop;

	stop = FALSE;

	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Left ){
			tview_collapse_node( self, widget );

		} else if( event->keyval == GDK_KEY_Right ){
			tview_expand_node( self, widget );

		} else if( event->keyval == GDK_KEY_Insert ){
			tview_on_key_insert( self );

		} else if( event->keyval == GDK_KEY_Delete ){
			tview_on_key_delete( self );
		}
	}

	return( stop );
}

static void
tview_collapse_node( ofaAccountFrameBin *self, GtkWidget *widget )
{
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter, parent;
	GtkTreePath *path;

	if( GTK_IS_TREE_VIEW( widget )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ));
		if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

			if( gtk_tree_model_iter_has_child( tmodel, &iter )){
				path = gtk_tree_model_get_path( tmodel, &iter );
				gtk_tree_view_collapse_row( GTK_TREE_VIEW( widget ), path );
				gtk_tree_path_free( path );

			} else if( gtk_tree_model_iter_parent( tmodel, &parent, &iter )){
				path = gtk_tree_model_get_path( tmodel, &parent );
				gtk_tree_view_collapse_row( GTK_TREE_VIEW( widget ), path );
				gtk_tree_path_free( path );
			}
		}
	}
}

static void
tview_expand_node( ofaAccountFrameBin *self, GtkWidget *widget )
{
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;

	if( GTK_IS_TREE_VIEW( widget )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ));
		if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
			if( gtk_tree_model_iter_has_child( tmodel, &iter )){
				path = gtk_tree_model_get_path( tmodel, &iter );
				gtk_tree_view_expand_row( GTK_TREE_VIEW( widget ), path, FALSE );
				gtk_tree_path_free( path );
			}
		}
	}
}

/*
 * Inserting has same conditions than 'New' button
 */
static void
tview_on_key_insert( ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;
	sButton *sbtn;

	priv = ofa_account_frame_bin_get_instance_private( self );

	sbtn = button_find_by_id( &priv->buttons, ACCOUNT_BTN_NEW, FALSE );
	if( is_new_allowed( self, sbtn )){
		do_insert_account( self );
	}
}

static void
tview_on_key_delete( ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;
	sButton *sbtn;
	gchar *account_id;
	ofoAccount *account_obj;

	priv = ofa_account_frame_bin_get_instance_private( self );

	account_id = ofa_account_frame_bin_get_selected( self );
	if( my_strlen( account_id )){
		account_obj = ofo_account_get_by_number( priv->hub, account_id );
		g_return_if_fail( account_obj && OFO_IS_ACCOUNT( account_obj ));

		sbtn = button_find_by_id( &priv->buttons, ACCOUNT_BTN_DELETE, FALSE );
		if( is_delete_allowed( self, sbtn, account_obj )){
			do_delete_account( self, account_id );
		}
	}
	g_free( account_id );
}

/*
 * select the row with the given number, or the most close one
 * doesn't create the page class if it doesn't yet exist
 */
static void
tview_select_row_by_number( ofaAccountFrameBin *self, const gchar *number )
{
	ofaAccountFrameBinPrivate *priv;
	GtkWidget *page_w;
	gint page_n;
	GtkWidget *tview;
	GtkTreeModel *tfilter;
	GtkTreeIter store_iter, filter_iter;
	GtkTreePath *path;
	gint acc_class;
	gboolean valid;

	priv = ofa_account_frame_bin_get_instance_private( self );

	if( my_strlen( number )){
		acc_class = ofo_account_get_class_from_number( number );
		page_w = book_get_page_by_class( self, acc_class, FALSE );
		/* asked page is empty */
		if( !page_w ){
			return;
		}
		page_n = gtk_notebook_page_num( GTK_NOTEBOOK( priv->notebook ), page_w );
		gtk_notebook_set_current_page( GTK_NOTEBOOK( priv->notebook ), page_n );

		valid = ofa_account_store_get_by_number( priv->store, number, &store_iter );
		g_return_if_fail( valid );

		tview = my_utils_container_get_child_by_type(
						GTK_CONTAINER( page_w ), GTK_TYPE_TREE_VIEW );
		tfilter = gtk_tree_view_get_model( GTK_TREE_VIEW( tview ));

		if( gtk_tree_model_filter_convert_child_iter_to_iter(
					GTK_TREE_MODEL_FILTER( tfilter ), &filter_iter, &store_iter )){

			path = gtk_tree_model_get_path( tfilter, &filter_iter );
			gtk_tree_view_expand_to_path( GTK_TREE_VIEW( tview ), path );
			gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW( tview ), path, NULL, FALSE, 0, 0 );
			gtk_tree_path_free( path );

			tview_select_row_by_iter( self, GTK_TREE_VIEW( tview ), tfilter, &filter_iter );
		}
	}
}

static void
tview_select_row_by_iter( ofaAccountFrameBin *self, GtkTreeView *tview, GtkTreeModel *tfilter, GtkTreeIter *iter )
{
	GtkTreePath *path;

	path = gtk_tree_model_get_path( tfilter, iter );
	gtk_tree_view_set_cursor( GTK_TREE_VIEW( tview ), path, NULL, FALSE );
	gtk_tree_path_free( path );
	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}

/**
 * ofa_account_frame_bin_add_button:
 * @bin: this #ofaAccountFrameBin instance.
 * @id: the button identifier.
 * @sensitive: whether the button is sensitive relative to current
 *  conditions; when %FALSE, the button is never sensitive, whatever
 *  be the current runtime conditions.
 *
 * Create a new button in the #ofaButtonsBox.
 *
 * Returns: the newly created button.
 */
GtkWidget *
ofa_account_frame_bin_add_button( ofaAccountFrameBin *bin, ofeAccountFrameBtn id, gboolean sensitive )
{
	ofaAccountFrameBinPrivate *priv;
	GtkWidget *button;

	g_return_val_if_fail( bin && OFA_IS_ACCOUNT_FRAME_BIN( bin ), NULL );

	priv = ofa_account_frame_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	button = NULL;

	switch( id ){
		case ACCOUNT_BTN_SPACER:
			ofa_buttons_box_add_spacer( priv->buttonsbox );
			break;
		case ACCOUNT_BTN_NEW:
			button = button_add( bin, ACCOUNT_BTN_NEW, BUTTON_NEW, sensitive, G_CALLBACK( button_on_new_clicked ));
			gtk_widget_set_sensitive( button, sensitive && priv->is_current );
			break;
		case ACCOUNT_BTN_PROPERTIES:
			button = button_add( bin, ACCOUNT_BTN_PROPERTIES, BUTTON_PROPERTIES, sensitive, G_CALLBACK( button_on_properties_clicked ));
			gtk_widget_set_sensitive( button, sensitive );
			break;
		case ACCOUNT_BTN_DELETE:
			button = button_add( bin, ACCOUNT_BTN_DELETE, BUTTON_DELETE, sensitive, G_CALLBACK( button_on_delete_clicked ));
			gtk_widget_set_sensitive( button, sensitive && priv->is_current );
			break;
		case ACCOUNT_BTN_VIEW_ENTRIES:
			button = button_add( bin, ACCOUNT_BTN_VIEW_ENTRIES, _( "View _entries..." ), sensitive, G_CALLBACK( button_on_view_entries_clicked ));
			gtk_widget_set_sensitive( button, sensitive );
			break;
		case ACCOUNT_BTN_SETTLEMENT:
			button = button_add( bin, ACCOUNT_BTN_SETTLEMENT, _( "_Settlement..." ), sensitive, G_CALLBACK( button_on_settlement_clicked ));
			gtk_widget_set_sensitive( button, sensitive && priv->is_current );
			break;
		case ACCOUNT_BTN_RECONCILIATION:
			button = button_add( bin, ACCOUNT_BTN_RECONCILIATION, _( "_Reconciliation..." ), sensitive, G_CALLBACK( button_on_reconciliation_clicked ));
			gtk_widget_set_sensitive( button, sensitive && priv->is_current );
			break;
		default:
			break;
	}

	return( button );
}

static GtkWidget *
button_add( ofaAccountFrameBin *self, ofeAccountFrameBtn id, const gchar *label, gboolean sensitive, GCallback cb )
{
	ofaAccountFrameBinPrivate *priv;
	sButton *sbtn;

	priv = ofa_account_frame_bin_get_instance_private( self );

	sbtn = button_find_by_id( &priv->buttons, id, TRUE );
	sbtn->sensitive = sensitive;
	sbtn->btn = ofa_buttons_box_add_button_with_mnemonic( priv->buttonsbox, label, cb, self );

	return( sbtn->btn );
}

static sButton *
button_find_by_id( GList **list, ofeAccountFrameBtn id, gboolean create )
{
	GList *it;
	sButton *sbtn;

	for( it=*list ; it ; it=it->next ){
		sbtn = ( sButton * ) it->data;
		if( sbtn->id == id ){
			return( sbtn );
		}
	}

	sbtn = NULL;

	if( create ){
		sbtn = g_new0( sButton, 1 );
		*list = g_list_prepend( *list, sbtn );
		sbtn->id = id;
	}

	return( sbtn );
}

static void
button_update_sensitivity( ofaAccountFrameBin *self, const gchar *account_id )
{
	ofaAccountFrameBinPrivate *priv;
	ofoAccount *account_obj;
	gboolean has_account;
	sButton *sbtn;

	priv = ofa_account_frame_bin_get_instance_private( self );

	has_account = FALSE;
	account_obj = NULL;

	if( my_strlen( account_id )){
		account_obj = ofo_account_get_by_number( priv->hub, account_id );
		has_account = ( account_obj && OFO_IS_ACCOUNT( account_obj ));
	}

	sbtn = button_find_by_id( &priv->buttons, ACCOUNT_BTN_NEW, FALSE );
	if( sbtn ){
		g_return_if_fail( sbtn->btn && GTK_IS_WIDGET( sbtn->btn ));
		gtk_widget_set_sensitive( sbtn->btn, is_new_allowed( self, sbtn ));
	}
	sbtn = button_find_by_id( &priv->buttons, ACCOUNT_BTN_PROPERTIES, FALSE );
	if( sbtn ){
		g_return_if_fail( sbtn->btn && GTK_IS_WIDGET( sbtn->btn ));
		gtk_widget_set_sensitive( sbtn->btn, sbtn->sensitive && has_account );
	}
	sbtn = button_find_by_id( &priv->buttons, ACCOUNT_BTN_DELETE, FALSE );
	if( sbtn ){
		g_return_if_fail( sbtn->btn && GTK_IS_WIDGET( sbtn->btn ));
		gtk_widget_set_sensitive( sbtn->btn, is_delete_allowed( self, sbtn, account_obj ));
	}
	sbtn = button_find_by_id( &priv->buttons, ACCOUNT_BTN_VIEW_ENTRIES, FALSE );
	if( sbtn ){
		g_return_if_fail( sbtn->btn && GTK_IS_WIDGET( sbtn->btn ));
		gtk_widget_set_sensitive( sbtn->btn, sbtn->sensitive && has_account && !ofo_account_is_root( account_obj ));
	}
	sbtn = button_find_by_id( &priv->buttons, ACCOUNT_BTN_SETTLEMENT, FALSE );
	if( sbtn ){
		g_return_if_fail( sbtn->btn && GTK_IS_WIDGET( sbtn->btn ));
		gtk_widget_set_sensitive( sbtn->btn, sbtn->sensitive && has_account && priv->is_current && ofo_account_is_settleable( account_obj ));
	}
	sbtn = button_find_by_id( &priv->buttons, ACCOUNT_BTN_RECONCILIATION, FALSE );
	if( sbtn ){
		g_return_if_fail( sbtn->btn && GTK_IS_WIDGET( sbtn->btn ));
		gtk_widget_set_sensitive( sbtn->btn, sbtn->sensitive && has_account && priv->is_current && ofo_account_is_reconciliable( account_obj ));
	}
}

static void
button_on_new_clicked( GtkButton *button, ofaAccountFrameBin *self )
{
	do_insert_account( self );
}

static void
button_on_properties_clicked( GtkButton *button, ofaAccountFrameBin *self )
{
	gchar *account_id;

	account_id = ofa_account_frame_bin_get_selected( self );
	if( my_strlen( account_id )){
		do_update_account( self, account_id );
	}
	g_free( account_id );
}

static void
button_on_delete_clicked( GtkButton *button, ofaAccountFrameBin *self )
{
	gchar *account_id;

	account_id = ofa_account_frame_bin_get_selected( self );
	if( my_strlen( account_id )){
		do_delete_account( self, account_id );
	}
	g_free( account_id );
}

static void
button_on_view_entries_clicked( GtkButton *button, ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;
	gchar *number;
	ofaPage *page;

	priv = ofa_account_frame_bin_get_instance_private( self );

	number = ofa_account_frame_bin_get_selected( self );
	page = ofa_main_window_activate_theme( priv->main_window, THM_ENTRIES );
	ofa_entry_page_display_entries( OFA_ENTRY_PAGE( page ), OFO_TYPE_ACCOUNT, number, NULL, NULL );
	g_free( number );
}

static void
button_on_settlement_clicked( GtkButton *button, ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;
	gchar *number;
	ofaPage *page;

	priv = ofa_account_frame_bin_get_instance_private( self );

	number = ofa_account_frame_bin_get_selected( self );
	page = ofa_main_window_activate_theme( priv->main_window, THM_SETTLEMENT );
	ofa_settlement_page_set_account( OFA_SETTLEMENT_PAGE( page ), number );
	g_free( number );
}

static void
button_on_reconciliation_clicked( GtkButton *button, ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;
	gchar *number;
	ofaPage *page;

	priv = ofa_account_frame_bin_get_instance_private( self );

	number = ofa_account_frame_bin_get_selected( self );
	page = ofa_main_window_activate_theme( priv->main_window, THM_RECONCIL );
	ofa_reconcil_page_set_account( OFA_RECONCIL_PAGE( page ), number );
	g_free( number );
}

static gboolean
is_new_allowed( ofaAccountFrameBin *self, sButton *sbtn )
{
	ofaAccountFrameBinPrivate *priv;
	gboolean ok;

	priv = ofa_account_frame_bin_get_instance_private( self );

	ok = sbtn && sbtn->btn && sbtn->sensitive && priv->is_current;

	return( ok );
}

static gboolean
is_delete_allowed( ofaAccountFrameBin *self, sButton *sbtn, ofoAccount *selected )
{
	ofaAccountFrameBinPrivate *priv;
	gboolean ok;

	priv = ofa_account_frame_bin_get_instance_private( self );

	ok = sbtn && sbtn->btn && sbtn->sensitive && priv->is_current && selected && ofo_account_is_deletable( selected );

	return( ok );
}

static void
do_insert_account( ofaAccountFrameBin *self )
{
	ofaAccountFrameBinPrivate *priv;
	ofoAccount *account;

	priv = ofa_account_frame_bin_get_instance_private( self );

	account = ofo_account_new();
	ofa_account_properties_run( priv->main_window, account );
}

static void
do_update_account( ofaAccountFrameBin *self, const gchar *account_id )
{
	ofaAccountFrameBinPrivate *priv;
	ofoAccount *account_obj;

	priv = ofa_account_frame_bin_get_instance_private( self );

	account_obj = ofo_account_get_by_number( priv->hub, account_id );
	g_return_if_fail( account_obj && OFO_IS_ACCOUNT( account_obj ));

	ofa_account_properties_run( priv->main_window, account_obj );
}

static void
do_delete_account( ofaAccountFrameBin *self, const gchar *account_id )
{
	ofaAccountFrameBinPrivate *priv;
	ofoAccount *account_obj;

	priv = ofa_account_frame_bin_get_instance_private( self );

	account_obj = ofo_account_get_by_number( priv->hub, account_id );
	g_return_if_fail( account_obj && OFO_IS_ACCOUNT( account_obj ) && ofo_account_is_deletable( account_obj ));

	if( delete_confirmed( self, account_obj ) && ofo_account_delete( account_obj )){

			/* nothing to do here, all being managed by signal hub_handlers
			 * just reset the selection
			 * asking for selection of the just deleted account makes
			 * almost sure that we are going to select the most close
			 * row */
			tview_on_row_selected( NULL, self );
			ofa_account_frame_bin_set_selected( self, account_id );
	}
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
 * is triggered by the store when a row is inserted
 * we try to optimize the search by keeping the class of the last
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

static void
free_button( sButton *sbtn )
{
	g_free( sbtn );
}
