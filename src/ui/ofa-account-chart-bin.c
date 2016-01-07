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
#include "api/ofa-ihubber.h"
#include "api/ofa-page.h"
#include "api/ofa-preferences.h"
#include "api/ofo-account.h"
#include "api/ofo-class.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-account-properties.h"
#include "ui/ofa-account-chart-bin.h"
#include "ui/ofa-account-frame-bin.h"
#include "ui/ofa-account-store.h"
#include "ui/ofa-entry-page.h"
#include "ui/ofa-settlement.h"
#include "ui/ofa-reconcil-page.h"

/* private instance data
 */
struct _ofaAccountChartBinPrivate {
	gboolean             dispose_has_run;

	const ofaMainWindow *main_window;
	ofaHub              *hub;
	GList               *hub_handlers;

	ofaAccountStore     *store;
	GList               *store_handlers;
	GtkNotebook         *book;
	GtkTreeCellDataFunc  cell_fn;
	void                *cell_data;

	gint                 prev_class;
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

/* the class number is attached to each page of the classes notebook,
 * and also attached to the undelrying treemodelfilter
 */
#define DATA_PAGE_CLASS                 "ofa-data-page-class"

/* the column identifier is attached to each column header
 */
#define DATA_COLUMN_ID                  "ofa-data-column-id"

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void       setup_bin( ofaAccountChartBin *bin );
static void       setup_main_window( ofaAccountChartBin *bin );
static void       on_book_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaAccountChartBin *bin );
static gboolean   on_book_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaAccountChartBin *bin );
static void       on_row_inserted( GtkTreeModel *tmodel, GtkTreePath *path, GtkTreeIter *iter, ofaAccountChartBin *bin );
static GtkWidget *book_get_page_by_class( ofaAccountChartBin *bin, gint class_num, gboolean create );
static GtkWidget *book_create_page( ofaAccountChartBin *bin, gint class );
static GtkWidget *page_add_treeview( ofaAccountChartBin *bin, GtkWidget *page );
static void       page_add_columns( ofaAccountChartBin *bin, GtkTreeView *tview );
static gboolean   is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, GtkWidget *page );
static void       on_tview_row_selected( GtkTreeSelection *selection, ofaAccountChartBin *bin );
static void       on_tview_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaAccountChartBin *bin );
static gboolean   on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaAccountChartBin *bin );
static void       tview_collapse_node( ofaAccountChartBin *bin, GtkWidget *widget );
static void       tview_expand_node( ofaAccountChartBin *bin, GtkWidget *widget );
static void       on_tview_insert( ofaAccountChartBin *bin );
static void       on_tview_delete( ofaAccountChartBin *bin );
static void       on_tview_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRenderer *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaAccountChartBin *bin );
static void       tview_cell_renderer_text( GtkCellRendererText *cell, gboolean is_root, gint level, gboolean is_error );
static void       do_insert_account( ofaAccountChartBin *bin );
static void       do_update_account( ofaAccountChartBin *bin );
static void       do_delete_account( ofaAccountChartBin *bin );
static gboolean   delete_confirmed( ofaAccountChartBin *bin, ofoAccount *account );
static void       do_view_entries( ofaAccountChartBin *bin );
static void       do_settlement( ofaAccountChartBin *bin );
static void       do_reconciliation( ofaAccountChartBin *bin );
static void       connect_to_hub_signaling_system( ofaAccountChartBin *bin );
static void       on_new_object( ofaHub *hub, ofoBase *object, ofaAccountChartBin *bin );
static void       on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaAccountChartBin *bin );
static void       on_updated_class_label( ofaAccountChartBin *bin, ofoClass *class );
static void       on_deleted_object( ofaHub *hub, ofoBase *object, ofaAccountChartBin *bin );
static void       on_deleted_class_label( ofaAccountChartBin *bin, ofoClass *class );
static void       on_reloaded_dataset( ofaHub *hub, GType type, ofaAccountChartBin *bin );
static GtkWidget *get_current_tree_view( const ofaAccountChartBin *bin );
static void       select_row_by_number( ofaAccountChartBin *bin, const gchar *number );
static void       select_row_by_iter( ofaAccountChartBin *bin, GtkTreeView *tview, GtkTreeModel *tfilter, GtkTreeIter *iter );

G_DEFINE_TYPE( ofaAccountChartBin, ofa_account_chart_bin, GTK_TYPE_BIN )

static void
accounts_chart_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_chart_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_CHART_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_chart_bin_parent_class )->finalize( instance );
}

static void
accounts_chart_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_chart_bin_dispose";
	ofaAccountChartBinPrivate *priv;
	GList *it;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_if_fail( instance && OFA_IS_ACCOUNT_CHART_BIN( instance ));

	priv = ( OFA_ACCOUNT_CHART_BIN( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		ofa_hub_disconnect_handlers( priv->hub, priv->hub_handlers );

		if( priv->store && OFA_IS_ACCOUNT_STORE( priv->store )){
			for( it=priv->store_handlers ; it ; it=it->next ){
				g_signal_handler_disconnect( priv->store, ( gulong ) it->data );
			}
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_chart_bin_parent_class )->dispose( instance );
}

static void
ofa_account_chart_bin_init( ofaAccountChartBin *self )
{
	static const gchar *thisfn = "ofa_account_chart_bin_init";

	g_return_if_fail( self && OFA_IS_ACCOUNT_CHART_BIN( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_ACCOUNT_CHART_BIN, ofaAccountChartBinPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
ofa_account_chart_bin_class_init( ofaAccountChartBinClass *klass )
{
	static const gchar *thisfn = "ofa_account_chart_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = accounts_chart_dispose;
	G_OBJECT_CLASS( klass )->finalize = accounts_chart_finalize;

	g_type_class_add_private( klass, sizeof( ofaAccountChartBinPrivate ));

	/**
	 * ofaAccountChartBin::changed:
	 *
	 * This signal is sent on the ofaAccountChartBin when the selection in
	 * the current treeview is changed.
	 *
	 * Argument is the selected account number.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAccountChartBin *store,
	 * 						const gchar   *number,
	 * 						gpointer       user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_ACCOUNT_CHART_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_STRING );

	/**
	 * ofaAccountChartBin::activated:
	 *
	 * This signal is sent on the ofaAccountChartBin when the selection in
	 * the current treeview is activated.
	 *
	 * Argument is the selected account number.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAccountChartBin *store,
	 * 						const gchar   *number,
	 * 						gpointer       user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"activated",
				OFA_TYPE_ACCOUNT_CHART_BIN,
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
 * ofa_account_chart_bin_new:
 *
 * Creates the structured content, i.e. one notebook with one page per
 * account class.
 *
 * Does NOT insert the data (see: ofa_account_chart_bin_init_view).
 *
 * +-----------------------------------------------------------------------+
 * | parent container:                                                     |
 * |   this is the grid of the main page,                                  |
 * |   or any another container (i.e. a frame)                             |
 * | +-------------------------------------------------------------------+ |
 * | | creates a grid which will contain the book and the buttons        | |
 * | | +---------------------------------------------+-----------------+ + |
 * | | | creates a notebook where each page contains | creates         | | |
 * | | |   the account of the corresponding class    |   a buttons box | | |
 * | | |                                             |                 | | |
 * | | +---------------------------------------------+-----------------+ | |
 * | +-------------------------------------------------------------------+ |
 * +-----------------------------------------------------------------------+
 */
ofaAccountChartBin *
ofa_account_chart_bin_new( const ofaMainWindow *main_window  )
{
	ofaAccountChartBin *bin;

	bin = g_object_new( OFA_TYPE_ACCOUNT_CHART_BIN, NULL );

	bin->priv->main_window = main_window;

	setup_bin( bin );
	setup_main_window( bin );

	return( bin );
}

static void
setup_bin( ofaAccountChartBin *bin )
{
	ofaAccountChartBinPrivate *priv;
	GtkWidget *frame;

	priv = bin->priv;

	/* an invisible frame */
	frame = gtk_frame_new( NULL );
	gtk_container_add( GTK_CONTAINER( bin ), frame );
	gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_NONE );

	priv->book = GTK_NOTEBOOK( gtk_notebook_new());
	gtk_notebook_popup_enable( priv->book );
	gtk_notebook_set_scrollable( priv->book, TRUE );
	gtk_notebook_set_show_tabs( priv->book, TRUE );

	g_signal_connect(
			G_OBJECT( priv->book ),
			"switch-page", G_CALLBACK( on_book_page_switched ), bin );

	g_signal_connect(
			G_OBJECT( priv->book ),
			"key-press-event", G_CALLBACK( on_book_key_pressed ), bin );

	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( priv->book ));
}

/*
 * This is required in order to get the dossier which will permit to
 * create the underlying tree store.
 */
static void
setup_main_window( ofaAccountChartBin *bin )
{
	ofaAccountChartBinPrivate *priv;
	gulong handler;
	GtkApplication *application;

	priv = bin->priv;

	application = gtk_window_get_application( GTK_WINDOW( priv->main_window ));
	g_return_if_fail( application && OFA_IS_IHUBBER( application ));

	priv->hub = ofa_ihubber_get_hub( OFA_IHUBBER( application ));
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	priv->store = ofa_account_store_new( priv->hub );

	handler = g_signal_connect(
			priv->store, "ofa-row-inserted", G_CALLBACK( on_row_inserted ), bin );
	priv->store_handlers = g_list_prepend( priv->store_handlers, ( gpointer ) handler );

	ofa_tree_store_load_dataset( OFA_TREE_STORE( priv->store ));

	connect_to_hub_signaling_system( bin );

	gtk_notebook_set_current_page( priv->book, 0 );
}

/*
 * we have switch to this given page (wpage, npage)
 * just setup the selection
 */
static void
on_book_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaAccountChartBin *self )
{
	GtkWidget *tview;
	GtkTreeSelection *select;

	tview = my_utils_container_get_child_by_type( GTK_CONTAINER( wpage ), GTK_TYPE_TREE_VIEW );
	if( tview ){
		g_return_if_fail( GTK_IS_TREE_VIEW( tview ));
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
		on_tview_row_selected( select, self );
	}
}

/*
 * Returns :
 * TRUE to stop other hub_handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_book_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaAccountChartBin *self )
{
	ofaAccountChartBinPrivate *priv;
	gboolean stop;
	GtkWidget *page_widget;
	gint class_num, page_num;

	stop = FALSE;
	priv = self->priv;

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
				page_num = gtk_notebook_page_num( priv->book, page_widget );
				gtk_notebook_set_current_page( priv->book, page_num );
				stop = TRUE;
			}
		}
	}

	return( stop );
}

/**
 * ofa_account_chart_bin_set_cell_data_func:
 * @book:
 * @fn_cell:
 * @user_data:
 *
 */
void
ofa_account_chart_bin_set_cell_data_func( ofaAccountChartBin *book, GtkTreeCellDataFunc fn_cell, void *user_data )
{
	ofaAccountChartBinPrivate *priv;

	g_return_if_fail( book && OFA_IS_ACCOUNT_CHART_BIN( book ));

	priv = book->priv;

	if( !priv->dispose_has_run ){

		priv->cell_fn = fn_cell;
		priv->cell_data = user_data;
	}
}

/*
 * is triggered by the store when a row is inserted
 * we try to optimize the search by keeping the class of the last
 * inserted row;
 */
static void
on_row_inserted( GtkTreeModel *tmodel, GtkTreePath *path, GtkTreeIter *iter, ofaAccountChartBin *book )
{
	ofaAccountChartBinPrivate *priv;
	gchar *number;
	gint class_num;

	priv = book->priv;

	gtk_tree_model_get( tmodel, iter, ACCOUNT_COL_NUMBER, &number, -1 );
	class_num = ofo_account_get_class_from_number( number );
	g_free( number );

	if( class_num != priv->prev_class ){
		book_get_page_by_class( book, class_num, TRUE );
		priv->prev_class = class_num;
	}
}

/*
 * Returns the notebook's page container which is dedicated to the
 * given class number.
 *
 * If the page doesn't exist, and @bcreate is %TRUE, then it is created.
 */
static GtkWidget *
book_get_page_by_class( ofaAccountChartBin *book, gint class_num, gboolean bcreate )
{
	static const gchar *thisfn = "ofa_account_chart_bin_get_page_by_class";
	ofaAccountChartBinPrivate *priv;
	gint count, i;
	GtkWidget *found, *page_widget;
	gint page_class;

	priv = book->priv;

	if( !ofo_class_is_valid_number( class_num )){
		g_warning( "%s: invalid class number: %d", thisfn, class_num );
		return( NULL );
	}

	found = NULL;
	count = gtk_notebook_get_n_pages( priv->book );

	/* search for an existing page */
	for( i=0 ; i<count ; ++i ){
		page_widget = gtk_notebook_get_nth_page( priv->book, i );
		page_class = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( page_widget ), DATA_PAGE_CLASS ));
		if( page_class == class_num ){
			found = page_widget;
			break;
		}
	}

	/* if not exists, create it (if allowed) */
	if( !found && bcreate ){
		found = book_create_page( book, class_num );
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
book_create_page( ofaAccountChartBin *book, gint class_num )
{
	static const gchar *thisfn = "ofa_account_chart_bin_create_page";
	ofaAccountChartBinPrivate *priv;
	GtkWidget *frame, *scrolled, *tview, *label;
	ofoClass *class_obj;
	const gchar *class_label;
	gchar *str;
	gint page_num;

	g_debug( "%s: book=%p, class_num=%d", thisfn, ( void * ) book, class_num );

	priv = book->priv;

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
	tview = page_add_treeview( book, frame );
	gtk_container_add( GTK_CONTAINER( scrolled ), tview );

	/* then create the columns in the treeview */
	page_add_columns( book, GTK_TREE_VIEW( tview ));

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

	page_num = gtk_notebook_append_page( priv->book, frame, label );
	if( page_num == -1 ){
		g_warning( "%s: unable to add a page to the notebook for class=%d", thisfn, class_num );
		return( NULL );
	}
	gtk_notebook_set_tab_reorderable( priv->book, frame, TRUE );

	return( frame );
}

/*
 * creates the treeview
 * attach it to the container parent (the scrolled window)
 * setup the model filter
 */
static GtkWidget *
page_add_treeview( ofaAccountChartBin *book, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_account_chart_bin_create_treeview";
	ofaAccountChartBinPrivate *priv;
	GtkWidget *tview;
	GtkTreeModel *tfilter;
	GtkTreeSelection *select;

	priv = book->priv;

	tview = gtk_tree_view_new();
	gtk_widget_set_hexpand( tview, TRUE );
	gtk_widget_set_vexpand( tview, TRUE );
	gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( tview ), TRUE );

	tfilter = gtk_tree_model_filter_new( GTK_TREE_MODEL( priv->store ), NULL );
	g_debug( "%s: store=%p, tfilter=%p", thisfn, ( void * ) priv->store, ( void * ) tfilter );
	gtk_tree_model_filter_set_visible_func(
			GTK_TREE_MODEL_FILTER( tfilter ),
			( GtkTreeModelFilterVisibleFunc ) is_visible_row, page, NULL );

	gtk_tree_view_set_model( GTK_TREE_VIEW( tview ), tfilter );
	g_object_unref( tfilter );

	g_signal_connect(
			G_OBJECT( tview ), "row-activated", G_CALLBACK( on_tview_row_activated), book );
	g_signal_connect(
			G_OBJECT( tview ), "key-press-event", G_CALLBACK( on_tview_key_pressed ), book );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect(
			G_OBJECT( select ), "changed", G_CALLBACK( on_tview_row_selected ), book );

	return( tview );
}

/*
 * creates the columns in the GtkTreeView
 */
static void
page_add_columns( ofaAccountChartBin *book, GtkTreeView *tview )
{
	ofaAccountChartBinPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkTreeCellDataFunc fn_cell;
	void *fn_data;

	priv = book->priv;
	fn_cell = priv->cell_fn ? priv->cell_fn : ( GtkTreeCellDataFunc ) on_tview_cell_data_func;
	fn_data = priv->cell_fn ? priv->cell_data : book;

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

/*
 * tmodel here is the ofaTreeStore
 */
static gboolean
is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, GtkWidget *page )
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
on_tview_row_selected( GtkTreeSelection *selection, ofaAccountChartBin *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *account;

	account = NULL;

	/* selection may be null when called from on_delete_clicked() */
	if( selection ){
		if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
			gtk_tree_model_get( tmodel, &iter, ACCOUNT_COL_NUMBER, &account, -1 );
			g_signal_emit_by_name( self, "changed", account );
			g_free( account );
		}
	}

	/*update_buttons_sensitivity( self, account );*/
}

static void
on_tview_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaAccountChartBin *self )
{
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *account;

	account = NULL;

	select = gtk_tree_view_get_selection( tview );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, ACCOUNT_COL_NUMBER, &account, -1 );
		g_signal_emit_by_name( self, "activated", account );
		g_free( account );
	}
}

/*
 * Returns :
 * TRUE to stop other hub_handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaAccountChartBin *self )
{
	gboolean stop;

	stop = FALSE;

	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Left ){
			tview_collapse_node( self, widget );

		} else if( event->keyval == GDK_KEY_Right ){
			tview_expand_node( self, widget );

		} else if( event->keyval == GDK_KEY_Insert ){
			on_tview_insert( self );

		} else if( event->keyval == GDK_KEY_Delete ){
			on_tview_delete( self );
		}
	}

	return( stop );
}

static void
tview_collapse_node( ofaAccountChartBin *self, GtkWidget *widget )
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
tview_expand_node( ofaAccountChartBin *self, GtkWidget *widget )
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

static void
on_tview_insert( ofaAccountChartBin *self )
{
	do_insert_account( self );
}

static void
on_tview_delete( ofaAccountChartBin *self )
{
	ofaAccountChartBinPrivate *priv;
	gchar *account_number;
	ofoAccount *account_obj;

	priv = self->priv;

	account_number = ofa_account_chart_bin_get_selected( self );
	account_obj = ofo_account_get_by_number( priv->hub, account_number );
	g_free( account_number );

	if( ofo_account_is_deletable( account_obj )){
		do_delete_account( self );
	}
}

/**
 * ofa_account_chart_bin_cell_data_renderer:
 *
 * level 1: not displayed (should not appear)
 * level 2 and root: bold, colored background
 * level 3 and root: colored background
 * other root: italic
 *
 * detail accounts who have no currency are red written.
 */
void
ofa_account_chart_bin_cell_data_renderer( ofaAccountChartBin *book,
							GtkTreeViewColumn *tcolumn,
							GtkCellRenderer *cell, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	ofaAccountChartBinPrivate *priv;

	g_return_if_fail( book && OFA_IS_ACCOUNT_CHART_BIN( book ));
	g_return_if_fail( tcolumn && GTK_IS_TREE_VIEW_COLUMN( tcolumn ));
	g_return_if_fail( cell && GTK_IS_CELL_RENDERER( cell ));
	g_return_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ));

	priv = book->priv;

	if( !priv->dispose_has_run ){

		on_tview_cell_data_func( tcolumn, cell, tmodel, iter, book );
	}
}

static void
on_tview_cell_data_func( GtkTreeViewColumn *tcolumn,
							GtkCellRenderer *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
							ofaAccountChartBin *self )
{
	ofaAccountChartBinPrivate *priv;
	gchar *account_num;
	ofoAccount *account_obj;
	GString *number;
	gint level;
	gint column;
	ofoCurrency *currency;
	gboolean is_root, is_error;

	g_return_if_fail( GTK_IS_CELL_RENDERER( cell ));

	priv = self->priv;

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

static void
do_insert_account( ofaAccountChartBin *self )
{
	ofaAccountChartBinPrivate *priv;
	ofoAccount *account;

	priv = self->priv;
	account = ofo_account_new();

	if( !ofa_account_properties_run( priv->main_window, account )){
		g_object_unref( account );

	} else {
		select_row_by_number( self, ofo_account_get_number( account ));
	}
}

static void
do_update_account( ofaAccountChartBin *self )
{
	ofaAccountChartBinPrivate *priv;
	ofoAccount *account;
	gchar *number;
	GtkWidget *tview;

	priv = self->priv;

	number = ofa_account_chart_bin_get_selected( self );
	if( number ){
		account = ofo_account_get_by_number( priv->hub, number );
		g_return_if_fail( account && OFO_IS_ACCOUNT( account ));

		ofa_account_properties_run( priv->main_window, account );
	}
	g_free( number );

	tview = get_current_tree_view( self );
	if( tview ){
		gtk_widget_grab_focus( tview );
	}
}

static void
do_delete_account( ofaAccountChartBin *self )
{
	ofaAccountChartBinPrivate *priv;
	ofoAccount *account;
	gchar *number;
	GtkWidget *tview;

	priv = self->priv;

	number = ofa_account_chart_bin_get_selected( self );
	if( number ){
		account = ofo_account_get_by_number( priv->hub, number );
		g_return_if_fail( account &&
				OFO_IS_ACCOUNT( account ) &&
				ofo_account_is_deletable( account ));

		if( delete_confirmed( self, account ) &&
				ofo_account_delete( account )){

			/* nothing to do here, all being managed by signal hub_handlers
			 * just reset the selection as this is not managed by the
			 * account notebook (and doesn't have to)
			 * asking for selection of the just deleted account makes
			 * almost sure that we are going to select the most close
			 * row */
			on_tview_row_selected( NULL, self );
			ofa_account_chart_bin_set_selected( self, number );
		}
	}
	g_free( number );

	tview = get_current_tree_view( self );
	if( tview ){
		gtk_widget_grab_focus( tview );
	}
}

/*
 * - this is a root account with children and the preference is set so
 *   that all accounts will be deleted
 * - this is a root account and the preference is not set
 * - this is a detail account
 */
static gboolean
delete_confirmed( ofaAccountChartBin *self, ofoAccount *account )
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

static void
do_view_entries( ofaAccountChartBin *self )
{
	ofaAccountChartBinPrivate *priv;
	gchar *number;
	GtkWidget *tview;
	ofaPage *page;

	priv = self->priv;

	number = ofa_account_chart_bin_get_selected( self );
	g_debug( "ofa_account_chart_bin_do_view_entries: number=%s", number );
	page = ofa_main_window_activate_theme( priv->main_window, THM_ENTRIES );
	ofa_entry_page_display_entries( OFA_ENTRY_PAGE( page ), OFO_TYPE_ACCOUNT, number, NULL, NULL );
	g_free( number );

	tview = get_current_tree_view( self );
	if( tview ){
		gtk_widget_grab_focus( tview );
	}
}

static void
do_settlement( ofaAccountChartBin *self )
{
	ofaAccountChartBinPrivate *priv;
	gchar *number;
	ofaPage *page;

	priv = self->priv;

	number = ofa_account_chart_bin_get_selected( self );
	g_debug( "ofa_account_chart_bin_do_settlement: number=%s", number );
	page = ofa_main_window_activate_theme( priv->main_window, THM_SETTLEMENT );
	ofa_settlement_set_account( OFA_SETTLEMENT( page ), number );
	g_free( number );
}

static void
do_reconciliation( ofaAccountChartBin *self )
{
	ofaAccountChartBinPrivate *priv;
	gchar *number;
	ofaPage *page;

	priv = self->priv;

	number = ofa_account_chart_bin_get_selected( self );
	g_debug( "ofa_account_chart_bin_do_reconciliation: number=%s", number );
	page = ofa_main_window_activate_theme( priv->main_window, THM_RECONCIL );
	ofa_reconcil_page_set_account( OFA_RECONCIL_PAGE( page ), number );
	g_free( number );
}

static void
connect_to_hub_signaling_system( ofaAccountChartBin *book )
{
	ofaAccountChartBinPrivate *priv;
	gulong handler;

	priv = book->priv;

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_NEW, G_CALLBACK( on_new_object ), book );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_UPDATED, G_CALLBACK( on_updated_object ), book );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_DELETED, G_CALLBACK( on_deleted_object ), book );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_RELOAD, G_CALLBACK( on_reloaded_dataset ), book );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
on_new_object( ofaHub *hub, ofoBase *object, ofaAccountChartBin *book )
{
	static const gchar *thisfn = "ofa_account_chart_bin_on_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), book=%p",
			thisfn, ( void * ) hub,
					( void * ) object, G_OBJECT_TYPE_NAME( object ),
					( void * ) book );

	if( OFO_IS_CLASS( object )){
		on_updated_class_label( book, OFO_CLASS( object ));
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaAccountChartBin *book )
{
	static const gchar *thisfn = "ofa_account_chart_bin_on_updated_object";

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, book=%p",
			thisfn, ( void * ) hub,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), prev_id,
					( void * ) book );

	if( OFO_IS_CLASS( object )){
		on_updated_class_label( book, OFO_CLASS( object ));
	}
}

/*
 * a class label has changed : update the corresponding tab label
 */
static void
on_updated_class_label( ofaAccountChartBin *book, ofoClass *class )
{
	ofaAccountChartBinPrivate *priv;
	GtkWidget *page_w;
	gint class_num;

	priv = book->priv;

	class_num = ofo_class_get_number( class );
	page_w = book_get_page_by_class( book, class_num, FALSE );
	if( page_w ){
		g_return_if_fail( GTK_IS_WIDGET( page_w ));
		gtk_notebook_set_tab_label_text( priv->book, page_w, ofo_class_get_label( class ));
	}
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
on_deleted_object( ofaHub *hub, ofoBase *object, ofaAccountChartBin *book )
{
	static const gchar *thisfn = "ofa_account_chart_bin_on_deleted_object";

	g_debug( "%s: hub=%p, object=%p (%s), book=%p",
			thisfn, ( void * ) hub,
					( void * ) object, G_OBJECT_TYPE_NAME( object ),
					( void * ) book );

	if( OFO_IS_CLASS( object )){
		on_deleted_class_label( book, OFO_CLASS( object ));
	}
}

static void
on_deleted_class_label( ofaAccountChartBin *book, ofoClass *class )
{
	ofaAccountChartBinPrivate *priv;
	GtkWidget *page_w;
	gint class_num;

	priv = book->priv;

	class_num = ofo_class_get_number( class );
	page_w = book_get_page_by_class( book, class_num, FALSE );
	if( page_w ){
		g_return_if_fail( GTK_IS_WIDGET( page_w ));
		gtk_notebook_set_tab_label_text( priv->book, page_w, st_class_labels[class_num-1] );
	}
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
on_reloaded_dataset( ofaHub *hub, GType type, ofaAccountChartBin *book )
{
	static const gchar *thisfn = "ofa_account_chart_bin_on_reloaded_dataset";

	g_debug( "%s: hub=%p, type=%lu, book=%p",
			thisfn, ( void * ) hub, type, ( void * ) book );

	ofa_account_chart_bin_expand_all( book );
}

static GtkWidget *
get_current_tree_view( const ofaAccountChartBin *self )
{
	ofaAccountChartBinPrivate *priv;
	gint page_n;
	GtkWidget *page_w;
	GtkWidget *tview;

	priv = self->priv;
	tview = NULL;
	page_n = gtk_notebook_get_current_page( priv->book );

	if( page_n >= 0 ){
		page_w = gtk_notebook_get_nth_page( priv->book, page_n );
		g_return_val_if_fail( page_w && GTK_IS_CONTAINER( page_w ), NULL );

		tview = my_utils_container_get_child_by_type(
								GTK_CONTAINER( page_w ), GTK_TYPE_TREE_VIEW );
		g_return_val_if_fail( tview && GTK_IS_TREE_VIEW( tview ), NULL );
	}

	return( tview );
}

/**
 * ofa_account_chart_bin_expand_all:
 */
void
ofa_account_chart_bin_expand_all( ofaAccountChartBin *book )
{
	static const gchar *thisfn = "ofa_account_chart_bin_expand_all";
	ofaAccountChartBinPrivate *priv;
	gint pages_count, i;
	GtkWidget *page_w;
	GtkTreeView *tview;

	g_debug( "%s: book=%p", thisfn, ( void * ) book );

	g_return_if_fail( book && OFA_IS_ACCOUNT_CHART_BIN( book ));

	priv = book->priv;

	if( !priv->dispose_has_run ){

		pages_count = gtk_notebook_get_n_pages( priv->book );
		for( i=0 ; i<pages_count ; ++i ){
			page_w = gtk_notebook_get_nth_page( priv->book, i );
			g_return_if_fail( page_w && GTK_IS_WIDGET( page_w ));
			tview = ( GtkTreeView * ) my_utils_container_get_child_by_type( GTK_CONTAINER( page_w ), GTK_TYPE_TREE_VIEW );
			g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));
			gtk_tree_view_expand_all( tview );
		}
	}
}

/**
 * ofa_account_chart_bin_get_selected:
 * @book:
 *
 * Returns: the currently selected account number, as a newly allocated
 * string which should be g_free() by the caller.
 */
gchar *
ofa_account_chart_bin_get_selected( ofaAccountChartBin *book )
{
	static const gchar *thisfn = "ofa_account_chart_bin_get_selected";
	ofaAccountChartBinPrivate *priv;
	gchar *account;
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	g_debug( "%s: book=%p", thisfn, ( void * ) book );

	g_return_val_if_fail( book && OFA_IS_ACCOUNT_CHART_BIN( book ), NULL );

	priv = book->priv;
	account = NULL;

	if( !priv->dispose_has_run ){

		tview = ( GtkTreeView * ) get_current_tree_view( book );
		if( tview ){
			select = gtk_tree_view_get_selection( tview );
			if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
				gtk_tree_model_get( tmodel, &iter, ACCOUNT_COL_NUMBER, &account, -1 );
			}
		}
	}

	return( account );
}

/**
 * ofa_account_chart_bin_set_selected:
 * @book: this #ofaAccountChartBin instance
 * @number: the account identifier to be selected.
 *
 * Let the user reset the selection after the end of setup and
 * initialization phases
 */
void
ofa_account_chart_bin_set_selected( ofaAccountChartBin *book, const gchar *number )
{
	static const gchar *thisfn = "ofa_account_chart_bin_set_selected";
	ofaAccountChartBinPrivate *priv;

	g_debug( "%s: book=%p, number=%s", thisfn, ( void * ) book, number );

	g_return_if_fail( book && OFA_IS_ACCOUNT_CHART_BIN( book ));

	priv = book->priv;

	if( !priv->dispose_has_run ){

		select_row_by_number( book, number );
	}
}

/*
 * select the row with the given number, or the most close one
 * doesn't create the page class if it doesn't yet exist
 */
static void
select_row_by_number( ofaAccountChartBin *book, const gchar *number )
{
	ofaAccountChartBinPrivate *priv;
	GtkWidget *page_w;
	gint page_n;
	GtkWidget *tview;
	GtkTreeModel *tfilter;
	GtkTreeIter store_iter, filter_iter;
	GtkTreePath *path;

	priv = book->priv;

	if( my_strlen( number )){
		page_w = book_get_page_by_class( book,
							ofo_account_get_class_from_number( number ), FALSE );
		if( page_w ){
			page_n = gtk_notebook_page_num( priv->book, page_w );
			gtk_notebook_set_current_page( priv->book, page_n );

			if( ofa_account_store_get_by_number( priv->store, number, &store_iter )){
				tview = my_utils_container_get_child_by_type(
								GTK_CONTAINER( page_w ), GTK_TYPE_TREE_VIEW );
				tfilter = gtk_tree_view_get_model( GTK_TREE_VIEW( tview ));
				gtk_tree_model_filter_convert_child_iter_to_iter(
						GTK_TREE_MODEL_FILTER( tfilter ), &filter_iter, &store_iter );

				path = gtk_tree_model_get_path( tfilter, &filter_iter );
				gtk_tree_view_expand_to_path( GTK_TREE_VIEW( tview ), path );
				gtk_tree_path_free( path );

				select_row_by_iter( book, GTK_TREE_VIEW( tview ), tfilter, &filter_iter );
			}
		}
	}
}

static void
select_row_by_iter( ofaAccountChartBin *book, GtkTreeView *tview, GtkTreeModel *tfilter, GtkTreeIter *iter )
{
	GtkTreePath *path;

	path = gtk_tree_model_get_path( tfilter, iter );
	gtk_tree_view_set_cursor( GTK_TREE_VIEW( tview ), path, NULL, FALSE );
	gtk_tree_path_free( path );
	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}

/**
 * ofa_account_chart_bin_toggle_collapse:
 *
 * Expand/Collapse the tree if a current selection has children
 */
void
ofa_account_chart_bin_toggle_collapse( ofaAccountChartBin *book )
{
	static const gchar *thisfn = "ofa_account_chart_bin_toggle_collapse";
	ofaAccountChartBinPrivate *priv;
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;

	g_debug( "%s: book=%p", thisfn, ( void * ) book );

	g_return_if_fail( book && OFA_IS_ACCOUNT_CHART_BIN( book ));

	priv = book->priv;

	if( !priv->dispose_has_run ){

		tview = ( GtkTreeView * ) get_current_tree_view( book );
		if( tview ){
			select = gtk_tree_view_get_selection( tview );
			if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
				path = gtk_tree_model_get_path( tmodel, &iter );
				if( gtk_tree_view_row_expanded( tview, path )){
					gtk_tree_view_collapse_row( tview, path );
				} else {
					gtk_tree_view_expand_row( tview, path, TRUE );
				}
				gtk_tree_path_free( path );
			}
		}
	}
}

/**
 * ofa_account_chart_bin_get_current_treeview:
 *
 * Returns the treeview associated to the current page
 */
GtkWidget *
ofa_account_chart_bin_get_current_treeview( const ofaAccountChartBin *book )
{
	static const gchar *thisfn = "ofa_account_chart_bin_get_current_treeview";
	ofaAccountChartBinPrivate *priv;
	GtkWidget *tview;

	g_debug( "%s: book=%p", thisfn, ( void * ) book );

	g_return_val_if_fail( book && OFA_IS_ACCOUNT_CHART_BIN( book ), NULL );

	priv = book->priv;
	tview = NULL;

	if( !priv->dispose_has_run ){

		tview = get_current_tree_view( book );
	}

	return( tview );
}

#if 0
/*
 * the iso code 3a of a currency has changed - update the display
 * this should be very rare
 */
static void
on_updated_currency_code( ofaAccountChartBin *self, ofoCurrency *currency )
{
	gint pages_count, i;
	GtkWidget *page_w;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoAccount *account;
	gint dev_id;

	dev_id = ofo_currency_get_id( currency );
	pages_count = gtk_notebook_get_n_pages( self->priv->book );

	for( i=0 ; i<pages_count ; ++i ){

		page_w = gtk_notebook_get_nth_page( self->priv->book, i );
		g_return_if_fail( page_w && GTK_IS_CONTAINER( page_w ));

		tview = ( GtkTreeView * )
						my_utils_container_get_child_by_type(
								GTK_CONTAINER( page_w ), GTK_TYPE_TREE_VIEW );
		g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));

		tmodel = gtk_tree_view_get_model( tview );
		if( gtk_tree_model_get_iter_first( tmodel, &iter )){
			while( TRUE ){
				gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &account, -1 );
				g_object_unref( account );

				if( ofo_account_get_currency( account ) == dev_id ){
					tstore_set_row_by_iter( self, account, tmodel, &iter );
				}
				if( !gtk_tree_model_iter_next( tmodel, &iter )){
					break;
				}
			}
		}
	}
}
#endif

/**
 * ofa_account_chart_bin_button_clicked:
 * @book: this #ofaAccountChartBin instance.
 * @button_id: the button identifier which has been clicked on.
 *
 * The button identifier is declared in ofa-buttons-box.h.
 */
void
ofa_account_chart_bin_button_clicked( ofaAccountChartBin *book, gint button_id )
{
	ofaAccountChartBinPrivate *priv;

	g_return_if_fail( book && OFA_IS_ACCOUNT_CHART_BIN( book ));

	priv = book->priv;

	if( !priv->dispose_has_run ){

		switch( button_id ){
			case ACCOUNT_BUTTON_NEW:
				do_insert_account( book );
				break;
			case ACCOUNT_BUTTON_PROPERTIES:
				do_update_account( book );
				break;
			case ACCOUNT_BUTTON_DELETE:
				do_delete_account( book );
				break;
			case ACCOUNT_BUTTON_VIEW_ENTRIES:
				do_view_entries( book );
				break;
			case ACCOUNT_BUTTON_SETTLEMENT:
				do_settlement( book );
				break;
			case ACCOUNT_BUTTON_RECONCILIATION:
				do_reconciliation( book );
				break;
		}
	}
}
