/*
 * Open Freelance OpeTemplateing
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
 *
 * Open Freelance OpeTemplateing is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Freelance OpeTemplateing is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Freelance OpeTemplateing; see the file COPYING. If not,
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
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-ihubber.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-guided-input.h"
#include "ui/ofa-ope-template-properties.h"
#include "ui/ofa-ope-template-store.h"
#include "ui/ofa-ope-template-book-bin.h"
#include "ui/ofa-ope-template-frame-bin.h"

/* private instance data
 */
struct _ofaOpeTemplateBookBinPrivate {
	gboolean             dispose_has_run;

	const ofaMainWindow *main_window;
	ofaHub              *hub;
	GList               *hub_handlers;
	ofaIDBMeta          *meta;

	ofaOpeTemplateStore *ope_store;
	GList               *ope_handlers;
	GtkNotebook         *book;
};

/* this piece of data is attached to each page of the notebook
 */
typedef struct {
	ofaHub     *hub;
	gchar      *ledger;
}
	sPageData;

#define DATA_PAGE_LEDGER                    "ofa-data-page-ledger"

/* the column identifier is attached to each column header
 */
#define DATA_COLUMN_ID                      "ofa-data-column-id"

/* a settings which holds the order of ledger mnemos as a string list
 */
static const gchar *st_ledger_order         = "ofa-OpeTemplateBookOrder";

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	CLOSED,
	N_SIGNALS
};

static guint        st_signals[ N_SIGNALS ] = { 0 };

static void       setup_bin( ofaOpeTemplateBookBin *bin );
static void       setup_main_window( ofaOpeTemplateBookBin *bin );
static void       on_book_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaOpeTemplateBookBin *bin );
static void       on_row_inserted( GtkTreeModel *tmodel, GtkTreePath *path, GtkTreeIter *iter, ofaOpeTemplateBookBin *bin );
static GtkWidget *book_get_page_by_ledger( ofaOpeTemplateBookBin *bin, const gchar *ledger, gboolean create );
static GtkWidget *book_create_page( ofaOpeTemplateBookBin *bin, const gchar *ledger );
static GtkWidget *page_add_treeview( ofaOpeTemplateBookBin *bin, GtkWidget *page );
static void       page_add_columns( ofaOpeTemplateBookBin *bin, GtkTreeView *tview );
static void       on_finalized_page( sPageData *sdata, gpointer finalized_page );
static gboolean   is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, GtkWidget *page );
static void       on_tview_row_selected( GtkTreeSelection *selection, ofaOpeTemplateBookBin *bin );
static void       on_tview_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaOpeTemplateBookBin *bin );
static gboolean   on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaOpeTemplateBookBin *bin );
static void       on_tview_insert( ofaOpeTemplateBookBin *bin );
static void       on_tview_delete( ofaOpeTemplateBookBin *bin );
static void       on_tview_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaOpeTemplateBookBin *bin );
static void       do_insert_ope_template( ofaOpeTemplateBookBin *bin );
static void       do_update_ope_template( ofaOpeTemplateBookBin *bin );
static void       do_duplicate_ope_template( ofaOpeTemplateBookBin *bin );
static void       do_delete_ope_template( ofaOpeTemplateBookBin *bin );
static gboolean   delete_confirmed( ofaOpeTemplateBookBin *bin, ofoOpeTemplate *ope );
static void       do_guided_input( ofaOpeTemplateBookBin *bin );
static void       connect_to_hub_signaling_system( ofaOpeTemplateBookBin *bin );
static void       on_new_object( ofaHub *hub, ofoBase *object, ofaOpeTemplateBookBin *bin );
static void       on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaOpeTemplateBookBin *bin );
static void       on_updated_ledger_label( ofaOpeTemplateBookBin *bin, ofoLedger *ledger );
static void       on_updated_ope_template( ofaOpeTemplateBookBin *bin, ofoOpeTemplate *template );
static void       on_hub_deleted_object( ofaHub *hub, ofoBase *object, ofaOpeTemplateBookBin *bin );
static void       on_deleted_ledger_object( ofaOpeTemplateBookBin *bin, ofoLedger *ledger );
static void       on_hub_reload_dataset( ofaHub *hub, GType type, ofaOpeTemplateBookBin *bin );
static GtkWidget *get_current_tree_view( const ofaOpeTemplateBookBin *bin );
static void       select_row_by_mnemo( ofaOpeTemplateBookBin *bin, const gchar *mnemo );
static void       select_row_by_iter( ofaOpeTemplateBookBin *bin, GtkTreeView *tview, GtkTreeModel *tfilter, GtkTreeIter *iter );
static void       on_action_closed( ofaOpeTemplateBookBin *bin );
static void       write_settings( ofaOpeTemplateBookBin *bin );

G_DEFINE_TYPE( ofaOpeTemplateBookBin, ofa_ope_template_book_bin, GTK_TYPE_BIN )

static void
ope_templates_book_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ope_template_book_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_BOOK_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_book_bin_parent_class )->finalize( instance );
}

static void
ope_templates_book_dispose( GObject *instance )
{
	ofaOpeTemplateBookBinPrivate *priv;
	GList *it;

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_BOOK_BIN( instance ));

	priv = ( OFA_OPE_TEMPLATE_BOOK_BIN( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->meta );

		/* disconnect from ofaHub signaling system */
		ofa_hub_disconnect_handlers( priv->hub, priv->hub_handlers );

		/* disconnect from ofaOpeTemplateStore */
		if( priv->ope_store && OFA_IS_OPE_TEMPLATE_STORE( priv->ope_store )){
			for( it=priv->ope_handlers ; it ; it=it->next ){
				g_signal_handler_disconnect( priv->ope_store, ( gulong ) it->data );
			}
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_book_bin_parent_class )->dispose( instance );
}

static void
ofa_ope_template_book_bin_init( ofaOpeTemplateBookBin *self )
{
	static const gchar *thisfn = "ofa_ope_template_book_bin_init";

	g_return_if_fail( self && OFA_IS_OPE_TEMPLATE_BOOK_BIN( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_OPE_TEMPLATE_BOOK_BIN, ofaOpeTemplateBookBinPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
ofa_ope_template_book_bin_class_init( ofaOpeTemplateBookBinClass *klass )
{
	static const gchar *thisfn = "ofa_ope_template_book_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ope_templates_book_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_templates_book_finalize;

	g_type_class_add_private( klass, sizeof( ofaOpeTemplateBookBinPrivate ));

	/**
	 * ofaOpeTemplateBookBin::changed:
	 *
	 * This signal is sent on the #ofaOpeTemplateBookBin when the selection
	 * in the current treeview is changed.
	 *
	 * Argument is the selected operation template mnemo.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaOpeTemplateBookBin *store,
	 * 						const gchar      *mnemo,
	 * 						gpointer          user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_OPE_TEMPLATE_BOOK_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_STRING );

	/**
	 * ofaOpeTemplateBookBin::activated:
	 *
	 * This signal is sent on the #ofaOpeTemplateBookBin when the selection
	 * in the current treeview is activated.
	 *
	 * Argument is the selected operation template mnemo.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaOpeTemplateBookBin *store,
	 * 						const gchar      *mnemo,
	 * 						gpointer          user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-activated",
				OFA_TYPE_OPE_TEMPLATE_BOOK_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_STRING );

	/**
	 * ofaOpeTemplateBookBin::closed:
	 *
	 * This signal is sent on the #ofaOpeTemplateBookBin when the book is
	 * about to be closed.
	 *
	 * The #ofaOpeTemplateBookBin takes advantage of this signal to save
	 * its own settings.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaOpeTemplateBookBin *store,
	 * 						gpointer           user_data );
	 */
	st_signals[ CLOSED ] = g_signal_new_class_handler(
				"ofa-closed",
				OFA_TYPE_OPE_TEMPLATE_BOOK_BIN,
				G_SIGNAL_ACTION,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0,
				G_TYPE_NONE );
}

/**
 * ofa_ope_template_book_bin_new:
 * @main_window: the #ofaMainWindow main window of the application.
 *
 * Creates the structured content, i.e. one notebook with one page per
 * ledger.
 *
 * Does NOT insert the data.
 */
ofaOpeTemplateBookBin *
ofa_ope_template_book_bin_new( const ofaMainWindow *main_window  )
{
	ofaOpeTemplateBookBin *bin;

	bin = g_object_new( OFA_TYPE_OPE_TEMPLATE_BOOK_BIN, NULL );

	bin->priv->main_window = main_window;

	setup_bin( bin );
	setup_main_window( bin );

	g_signal_connect( bin, "ofa-closed", G_CALLBACK( on_action_closed ), bin );

	return( bin );
}

static void
setup_bin( ofaOpeTemplateBookBin *bin )
{
	ofaOpeTemplateBookBinPrivate *priv;
	GtkWidget *book;

	priv = bin->priv;

	book = gtk_notebook_new();
	gtk_container_add( GTK_CONTAINER( bin ), book );
	priv->book = GTK_NOTEBOOK( book );

	gtk_notebook_popup_enable( priv->book );
	gtk_notebook_set_scrollable( priv->book, TRUE );
	gtk_notebook_set_show_tabs( priv->book, TRUE );

	g_signal_connect(
			G_OBJECT( priv->book ),
			"switch-page", G_CALLBACK( on_book_page_switched ), bin );
}

/*
 * This is required in order to get the dossier which will permit to
 * create the underlying list store.
 */
static void
setup_main_window( ofaOpeTemplateBookBin *bin )
{
	ofaOpeTemplateBookBinPrivate *priv;
	GtkApplication *application;
	gulong handler;
	GList *strlist, *it;
	const ofaIDBConnect *connect;

	priv = bin->priv;

	application = gtk_window_get_application( GTK_WINDOW( priv->main_window ));
	g_return_if_fail( application && OFA_IS_IHUBBER( application ));

	priv->hub = ofa_ihubber_get_hub( OFA_IHUBBER( priv->hub ));
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	priv->ope_store = ofa_ope_template_store_new( priv->hub );

	connect = ofa_hub_get_connect( priv->hub );
	priv->meta = ofa_idbconnect_get_meta( connect );

	/* create one page per ledger
	 * if strlist is set, then create one page per ledger
	 * other needed pages will be created on fly
	 * nb: if the ledger no more exists, no page is created */
	strlist = ofa_settings_dossier_get_string_list( priv->meta, st_ledger_order );
	for( it=strlist ; it ; it=it->next ){
		book_get_page_by_ledger( bin, ( const gchar * ) it->data, TRUE );
	}
	ofa_settings_free_string_list( strlist );

	handler = g_signal_connect(
			priv->ope_store, "ofa-row-inserted", G_CALLBACK( on_row_inserted ), bin );
	priv->ope_handlers = g_list_prepend( priv->ope_handlers, ( gpointer ) handler );

	ofa_list_store_load_dataset( OFA_LIST_STORE( priv->ope_store ));

	connect_to_hub_signaling_system( bin );

	gtk_notebook_set_current_page( priv->book, 0 );
}

/*
 * we have switch to this given page (wpage, npage)
 * just setup the selection
 */
static void
on_book_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaOpeTemplateBookBin *bin )
{
	GtkWidget *tview;
	GtkTreeSelection *select;

	tview = my_utils_container_get_child_by_type( GTK_CONTAINER( wpage ), GTK_TYPE_TREE_VIEW );
	if( tview ){
		g_return_if_fail( GTK_IS_TREE_VIEW( tview ));
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
		on_tview_row_selected( select, bin );
	}
}

/*
 * is triggered by the store when a row is inserted
 */
static void
on_row_inserted( GtkTreeModel *tmodel, GtkTreePath *path, GtkTreeIter *iter, ofaOpeTemplateBookBin *bin )
{
	ofoOpeTemplate *ope;
	const gchar *ledger;

	gtk_tree_model_get( tmodel, iter, OPE_TEMPLATE_COL_OBJECT, &ope, -1 );
	g_return_if_fail( ope && OFO_IS_OPE_TEMPLATE( ope ));
	g_object_unref( ope );

	ledger = ofo_ope_template_get_ledger( ope );
	if( !book_get_page_by_ledger( bin, ledger, TRUE )){
		book_get_page_by_ledger( bin, UNKNOWN_LEDGER_MNEMO, TRUE );
	}
}

/*
 * Returns the notebook's page container which is dedicated to the
 * given ledger.
 *
 * If the page doesn't exist, and @bcreate is %TRUE, then it is created.
 */
static GtkWidget *
book_get_page_by_ledger( ofaOpeTemplateBookBin *bin, const gchar *ledger, gboolean bcreate )
{
	static const gchar *thisfn = "ofa_ope_template_book_bin_get_page_by_ledger";
	ofaOpeTemplateBookBinPrivate *priv;
	gint count, i;
	GtkWidget *found, *page_widget;
	sPageData *sdata;

	priv = bin->priv;
	found = NULL;
	count = gtk_notebook_get_n_pages( priv->book );

	/* search for an existing page */
	for( i=0 ; i<count ; ++i ){
		page_widget = gtk_notebook_get_nth_page( priv->book, i );
		sdata = ( sPageData * ) g_object_get_data( G_OBJECT( page_widget ), DATA_PAGE_LEDGER );
		if( sdata && !g_utf8_collate( sdata->ledger, ledger )){
			found = page_widget;
			break;
		}
	}

	/* if not exists, create it (if allowed) */
	if( !found && bcreate ){
		found = book_create_page( bin, ledger );
		if( !found ){
			g_warning( "%s: unable to create the page for ledger=%s", thisfn, ledger );
			return( NULL );
		} else {
			gtk_widget_show_all( found );
		}
	}

	return( found );
}

/*
 * @ledger: ledger mnemo
 * @book: this #ofaOpeTemplateBookBin instance.
 * @ledger: ledger mnemonic identifier
 *
 * creates the page widget for the given ledger
 */
static GtkWidget *
book_create_page( ofaOpeTemplateBookBin *bin, const gchar *ledger )
{
	static const gchar *thisfn = "ofa_ope_template_book_bin_create_page";
	ofaOpeTemplateBookBinPrivate *priv;
	GtkWidget *frame, *scrolled, *tview, *label;
	ofoLedger *ledger_obj;
	const gchar *ledger_label;
	gint page_num;
	sPageData *sdata;

	g_debug( "%s: bin=%p, ledger=%s", thisfn, ( void * ) bin, ledger );

	priv = bin->priv;

	/* get ledger label */
	if( !g_utf8_collate( ledger, UNKNOWN_LEDGER_MNEMO )){
		ledger_label = UNKNOWN_LEDGER_LABEL;

	} else {
		ledger_obj = ofo_ledger_get_by_mnemo( priv->hub, ledger );
		if( ledger_obj ){
			g_return_val_if_fail( OFO_IS_LEDGER( ledger_obj ), NULL );
			ledger_label = ofo_ledger_get_label( ledger_obj );
		} else {
			g_warning( "%s: ledger not found: %s", thisfn, ledger );
			return( NULL );
		}
	}

	/* a frame as the top widget of the notebook page */
	frame = gtk_frame_new( NULL );
	gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_IN );

	/* attach data to the notebook page */
	sdata = g_new0( sPageData, 1 );
	sdata->hub = priv->hub;
	sdata->ledger = g_strdup( ledger );
	g_object_set_data( G_OBJECT( frame ), DATA_PAGE_LEDGER, sdata );
	g_object_weak_ref( G_OBJECT( frame ), ( GWeakNotify ) on_finalized_page, sdata );

	/* then a scrolled window inside the frame */
	scrolled = gtk_scrolled_window_new( NULL, NULL );
	gtk_container_add( GTK_CONTAINER( frame ), scrolled );
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW( scrolled ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );

	/* then create the treeview inside the scrolled window */
	tview = page_add_treeview( bin, frame );
	gtk_container_add( GTK_CONTAINER( scrolled ), tview );

	/* then create the columns in the treeview */
	page_add_columns( bin, GTK_TREE_VIEW( tview ));

	/* last add the page to the notebook */
	label = gtk_label_new( ledger_label );

	page_num = gtk_notebook_append_page( priv->book, frame, label );
	if( page_num == -1 ){
		g_warning( "%s: unable to add a page to the notebook for ledger=%s", thisfn, ledger );
		gtk_widget_destroy( frame );
		return( NULL );
	}
	gtk_notebook_set_tab_reorderable( priv->book, frame, TRUE );

	return( frame );
}

/*
 * creates the treeview
 * attach some piece of data to it
 */
static GtkWidget *
page_add_treeview( ofaOpeTemplateBookBin *bin, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_ope_template_book_bin_create_treeview";
	ofaOpeTemplateBookBinPrivate *priv;
	GtkWidget *tview;
	GtkTreeModel *tfilter;
	GtkTreeSelection *select;

	priv = bin->priv;

	tview = gtk_tree_view_new();
	gtk_widget_set_hexpand( tview, TRUE );
	gtk_widget_set_vexpand( tview, TRUE );
	gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( tview ), TRUE );

	tfilter = gtk_tree_model_filter_new( GTK_TREE_MODEL( priv->ope_store ), NULL );
	g_debug( "%s: store=%p, tfilter=%p", thisfn, ( void * ) priv->ope_store, ( void * ) tfilter );
	gtk_tree_model_filter_set_visible_func(
			GTK_TREE_MODEL_FILTER( tfilter ),
			( GtkTreeModelFilterVisibleFunc ) is_visible_row, page, NULL );

	gtk_tree_view_set_model( GTK_TREE_VIEW( tview ), tfilter );
	g_object_unref( tfilter );

	g_signal_connect(
			G_OBJECT( tview ), "row-activated", G_CALLBACK( on_tview_row_activated), bin );
	g_signal_connect(
			G_OBJECT( tview ), "key-press-event", G_CALLBACK( on_tview_key_pressed ), bin );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect(
			G_OBJECT( select ), "changed", G_CALLBACK( on_tview_row_selected ), bin );

	return( tview );
}

/*
 * creates the columns in the GtkTreeView
 */
static void
page_add_columns( ofaOpeTemplateBookBin *bin, GtkTreeView *tview )
{
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
				_( "Mnemo" ),
				cell, "text", OPE_TEMPLATE_COL_MNEMO,
				NULL );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( OPE_TEMPLATE_COL_MNEMO ));
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, cell, ( GtkTreeCellDataFunc ) on_tview_cell_data_func, bin, NULL );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			cell, "text", OPE_TEMPLATE_COL_LABEL,
			NULL );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( OPE_TEMPLATE_COL_LABEL ));
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, cell, ( GtkTreeCellDataFunc ) on_tview_cell_data_func, bin, NULL );
}

static void
on_finalized_page( sPageData *sdata, gpointer finalized_page )
{
	static const gchar *thisfn = "ofa_ope_template_book_bin_on_finalized_page";

	g_debug( "%s: sdata=%p, finalized_page=%p", thisfn, ( void * ) sdata, ( void * ) finalized_page );

	g_free( sdata->ledger );
	g_free( sdata );
}

/*
 * tmodel here is the ofaListStore
 *
 * the operation template is visible:
 * - if its ledger is the same than those of the displayed page (from
 *   args)
 * - or its ledger doesn't exist and the ledger of the displayed page
 *   (from args) is 'unclassed)
 */
static gboolean
is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, GtkWidget *page )
{
	gboolean is_visible;
	ofoOpeTemplate *ope;
	const gchar *ope_ledger;
	sPageData *sdata;
	ofoLedger *ledger_obj;

	is_visible = FALSE;

	gtk_tree_model_get( tmodel, iter, OPE_TEMPLATE_COL_OBJECT, &ope, -1 );
	g_return_val_if_fail( ope && OFO_IS_OPE_TEMPLATE( ope ), FALSE );
	g_object_unref( ope );

	ope_ledger = ofo_ope_template_get_ledger( ope );
	sdata = ( sPageData * ) g_object_get_data( G_OBJECT( page ), DATA_PAGE_LEDGER );

	if( !g_utf8_collate( sdata->ledger, ope_ledger )){
		is_visible = TRUE;

	} else if( !g_utf8_collate( sdata->ledger, UNKNOWN_LEDGER_MNEMO )){
		ledger_obj = ofo_ledger_get_by_mnemo( sdata->hub, ope_ledger );
		if( !ledger_obj ){
			is_visible = TRUE;
		}
	}

	return( is_visible );
}

static void
on_tview_row_selected( GtkTreeSelection *selection, ofaOpeTemplateBookBin *bin )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *mnemo;

	mnemo = NULL;

	/* selection may be null when called from on_delete_clicked() */
	if( selection ){
		if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
			gtk_tree_model_get( tmodel, &iter, OPE_TEMPLATE_COL_MNEMO, &mnemo, -1 );
			g_signal_emit_by_name( bin, "ofa-changed", mnemo );
			g_free( mnemo );
		}
	}

	/*update_buttons_sensitivity( bin, account );*/
}

static void
on_tview_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaOpeTemplateBookBin *bin )
{
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *mnemo;

	mnemo = NULL;

	select = gtk_tree_view_get_selection( tview );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, OPE_TEMPLATE_COL_MNEMO, &mnemo, -1 );
		g_signal_emit_by_name( bin, "ofa-activated", mnemo );
		g_free( mnemo );
	}
}

/*
 * Returns :
 * TRUE to stop other hub_handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaOpeTemplateBookBin *bin )
{
	gboolean stop;

	stop = FALSE;

	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Insert ){
			on_tview_insert( bin );

		} else if( event->keyval == GDK_KEY_Delete ){
			on_tview_delete( bin );
		}
	}

	return( stop );
}

static void
on_tview_insert( ofaOpeTemplateBookBin *bin )
{
	do_insert_ope_template( bin );
}

static void
on_tview_delete( ofaOpeTemplateBookBin *bin )
{
	ofaOpeTemplateBookBinPrivate *priv;
	gchar *mnemo;
	ofoOpeTemplate *ope;

	priv = bin->priv;

	mnemo = ofa_ope_template_book_bin_get_selected( bin );
	ope = ofo_ope_template_get_by_mnemo( priv->hub, mnemo );
	g_free( mnemo );

	if( ofo_ope_template_is_deletable( ope )){
		do_delete_ope_template( bin );
	}
}

/*
 * no particular style here
 */
static void
on_tview_cell_data_func( GtkTreeViewColumn *tcolumn,
						GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
						ofaOpeTemplateBookBin *bin )
{
	g_return_if_fail( GTK_IS_CELL_RENDERER_TEXT( cell ));
}

static void
do_insert_ope_template( ofaOpeTemplateBookBin *bin )
{
	ofaOpeTemplateBookBinPrivate *priv;
	ofoOpeTemplate *ope;
	gint page_n;
	GtkWidget *page_w;
	const gchar *ledger;
	sPageData *sdata;

	priv = bin->priv;
	ledger = NULL;

	page_n = gtk_notebook_get_current_page( priv->book );
	if( page_n >= 0 ){
		page_w = gtk_notebook_get_nth_page( priv->book, page_n );
		sdata = ( sPageData * ) g_object_get_data( G_OBJECT( page_w ), DATA_PAGE_LEDGER );
		if( sdata ){
			ledger = sdata->ledger;
		}
	}

	ope = ofo_ope_template_new();

	if( !ofa_ope_template_properties_run( priv->main_window, ope, ledger )){
		g_object_unref( ope );

	} else {
		select_row_by_mnemo( bin, ofo_ope_template_get_mnemo( ope ));
	}
}

static void
do_update_ope_template( ofaOpeTemplateBookBin *bin )
{
	ofaOpeTemplateBookBinPrivate *priv;
	ofoOpeTemplate *ope;
	gchar *mnemo;
	GtkWidget *tview;

	priv = bin->priv;

	mnemo = ofa_ope_template_book_bin_get_selected( bin );
	if( mnemo ){
		ope = ofo_ope_template_get_by_mnemo( priv->hub, mnemo );
		g_return_if_fail( ope && OFO_IS_OPE_TEMPLATE( ope ));

		ofa_ope_template_properties_run( priv->main_window, ope, NULL );
	}
	g_free( mnemo );

	tview = get_current_tree_view( bin );
	if( tview ){
		gtk_widget_grab_focus( tview );
	}
}

static void
do_duplicate_ope_template( ofaOpeTemplateBookBin *bin )
{
	static const gchar *thisfn = "ofa_ope_template_book_bin_do_duplicate_ope_template";
	ofaOpeTemplateBookBinPrivate *priv;
	gchar *mnemo, *new_mnemo;
	ofoOpeTemplate *ope;
	ofoOpeTemplate *duplicate;
	gchar *str;

	g_return_if_fail( OFA_IS_OPE_TEMPLATE_BOOK_BIN( bin ));

	g_debug( "%s: bin=%p", thisfn, ( void * ) bin );

	priv = bin->priv;

	mnemo = ofa_ope_template_book_bin_get_selected( bin );
	if( mnemo ){
		ope = ofo_ope_template_get_by_mnemo( priv->hub, mnemo );
		g_return_if_fail( ope && OFO_IS_OPE_TEMPLATE( ope ));

		duplicate = ofo_ope_template_new_from_template( ope );
		new_mnemo = ofo_ope_template_get_mnemo_new_from( ope );
		ofo_ope_template_set_mnemo( duplicate, new_mnemo );

		str = g_strdup_printf( "%s (%s)", ofo_ope_template_get_label( ope ), _( "Duplicate" ));
		ofo_ope_template_set_label( duplicate, str );
		g_free( str );

		if( !ofo_ope_template_insert( duplicate, priv->hub )){
			g_object_unref( duplicate );
		} else {
			select_row_by_mnemo( bin, new_mnemo );
		}
		g_free( new_mnemo );
	}
	g_free( mnemo );
}

static void
do_delete_ope_template( ofaOpeTemplateBookBin *bin )
{
	ofaOpeTemplateBookBinPrivate *priv;
	ofoOpeTemplate *ope;
	gchar *mnemo;
	GtkWidget *tview;

	priv = bin->priv;

	mnemo = ofa_ope_template_book_bin_get_selected( bin );
	if( mnemo ){
		ope = ofo_ope_template_get_by_mnemo( priv->hub, mnemo );
		g_return_if_fail( ope &&
				OFO_IS_OPE_TEMPLATE( ope ) &&
				ofo_ope_template_is_deletable( ope ));

		if( delete_confirmed( bin, ope ) &&
				ofo_ope_template_delete( ope )){

			/* nothing to do here, all being managed by signal hub_handlers
			 * just reset the selection as this is not managed by the
			 * ope notebook (and doesn't have to)
			 * asking for selection of the just deleted ope makes
			 * almost sure that we are going to select the most close
			 * row */
			on_tview_row_selected( NULL, bin );
			ofa_ope_template_book_bin_set_selected( bin, mnemo );
		}
	}
	g_free( mnemo );

	tview = get_current_tree_view( bin );
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
delete_confirmed( ofaOpeTemplateBookBin *bin, ofoOpeTemplate *ope )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want to delete the '%s - %s' entry model ?" ),
			ofo_ope_template_get_mnemo( ope ),
			ofo_ope_template_get_label( ope ));

	delete_ok = my_utils_dialog_question( msg, _( "_Delete" ));

	g_free( msg );

	return( delete_ok );
}

static void
do_guided_input( ofaOpeTemplateBookBin *bin )
{
	ofaOpeTemplateBookBinPrivate *priv;
	ofoOpeTemplate *ope;
	gchar *mnemo;

	priv = bin->priv;

	mnemo = ofa_ope_template_book_bin_get_selected( bin );
	if( mnemo ){
		ope = ofo_ope_template_get_by_mnemo( priv->hub, mnemo );
		g_return_if_fail( ope && OFO_IS_OPE_TEMPLATE( ope ));

		ofa_guided_input_run( priv->main_window, ope );
	}
	g_free( mnemo );
}

static void
connect_to_hub_signaling_system( ofaOpeTemplateBookBin *bin )
{
	ofaOpeTemplateBookBinPrivate *priv;
	gulong handler;

	priv = bin->priv;

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_NEW, G_CALLBACK( on_new_object ), bin );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_UPDATED, G_CALLBACK( on_updated_object ), bin );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_DELETED, G_CALLBACK( on_hub_deleted_object ), bin );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_RELOAD, G_CALLBACK( on_hub_reload_dataset ), bin );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
on_new_object( ofaHub *hub, ofoBase *object, ofaOpeTemplateBookBin *bin )
{
	static const gchar *thisfn = "ofa_ope_template_book_bin_on_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), bin=%p",
			thisfn, ( void * ) hub,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), ( void * ) bin );
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaOpeTemplateBookBin *bin )
{
	static const gchar *thisfn = "ofa_ope_template_book_bin_on_updated_object";

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, bin=%p",
			thisfn, ( void * ) hub,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), prev_id, ( void * ) bin );

	if( OFO_IS_LEDGER( object )){
		on_updated_ledger_label( bin, OFO_LEDGER( object ));

	} else if( OFO_IS_OPE_TEMPLATE( object )){
		on_updated_ope_template( bin, OFO_OPE_TEMPLATE( object ));
	}
}

/*
 * a ledger label has changed : update the corresponding tab label
 */
static void
on_updated_ledger_label( ofaOpeTemplateBookBin *bin, ofoLedger *ledger )
{
	ofaOpeTemplateBookBinPrivate *priv;
	GtkWidget *page_w;
	const gchar *mnemo;

	priv = bin->priv;

	mnemo = ofo_ledger_get_mnemo( ledger );
	page_w = book_get_page_by_ledger( bin, mnemo, FALSE );
	if( page_w ){
		g_return_if_fail( GTK_IS_WIDGET( page_w ));
		gtk_notebook_set_tab_label_text( priv->book, page_w, ofo_ledger_get_label( ledger ));
	}
}

/*
 * we do not have any way to know if the ledger attached to the operation
 *  template has changed or not - So just make sure the correct page is
 *  shown
 */
static void
on_updated_ope_template( ofaOpeTemplateBookBin *bin, ofoOpeTemplate *template )
{
	ofaOpeTemplateBookBinPrivate *priv;
	GtkWidget *page_w;
	const gchar *ledger;
	gint page_n;

	priv = bin->priv;

	ledger = ofo_ope_template_get_ledger( template );
	page_w = book_get_page_by_ledger( bin, ledger, TRUE );
	g_return_if_fail( page_w && GTK_IS_WIDGET( page_w ));
	select_row_by_mnemo( bin, ofo_ope_template_get_mnemo( template ));

	page_n = gtk_notebook_page_num( priv->book, page_w );
	gtk_notebook_set_current_page( priv->book, page_n );
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
on_hub_deleted_object( ofaHub *hub, ofoBase *object, ofaOpeTemplateBookBin *bin )
{
	static const gchar *thisfn = "ofa_ope_template_book_bin_on_hub_deleted_object";

	g_debug( "%s: hub=%p, object=%p (%s), bin=%p",
			thisfn, ( void * ) hub,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), ( void * ) bin );

	if( OFO_IS_LEDGER( object )){
		on_deleted_ledger_object( bin, OFO_LEDGER( object ));
	}
}

static void
on_deleted_ledger_object( ofaOpeTemplateBookBin *bin, ofoLedger *ledger )
{
	ofaOpeTemplateBookBinPrivate *priv;
	GtkWidget *page_w;
	gint page_n;
	const gchar *mnemo;

	priv = bin->priv;

	mnemo = ofo_ledger_get_mnemo( ledger );
	page_w = book_get_page_by_ledger( bin, mnemo, FALSE );
	if( page_w ){
		g_return_if_fail( GTK_IS_WIDGET( page_w ));
		page_n = gtk_notebook_page_num( priv->book, page_w );
		gtk_notebook_remove_page( priv->book, page_n );
		book_get_page_by_ledger( bin, UNKNOWN_LEDGER_MNEMO, TRUE );
	}
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
on_hub_reload_dataset( ofaHub *hub, GType type, ofaOpeTemplateBookBin *bin )
{
	static const gchar *thisfn = "ofa_ope_template_book_bin_on_hub_reload_dataset";

	g_debug( "%s: hub=%p, type=%lu, bin=%p",
			thisfn, ( void * ) hub, type, ( void * ) bin );
}

static GtkWidget *
get_current_tree_view( const ofaOpeTemplateBookBin *bin )
{
	ofaOpeTemplateBookBinPrivate *priv;
	gint page_n;
	GtkWidget *page_w;
	GtkWidget *tview;

	priv = bin->priv;
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
 * ofa_ope_template_book_bin_get_selected:
 * @bin:
 *
 * Returns: the currently selected account number, as a newly allocated
 * string which should be g_free() by the caller.
 */
gchar *
ofa_ope_template_book_bin_get_selected( ofaOpeTemplateBookBin *bin )
{
	ofaOpeTemplateBookBinPrivate *priv;
	gchar *mnemo;
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	g_return_val_if_fail( bin && OFA_IS_OPE_TEMPLATE_BOOK_BIN( bin ), NULL );

	priv = bin->priv;
	mnemo = NULL;

	if( !priv->dispose_has_run ){

		tview = ( GtkTreeView * ) get_current_tree_view( bin );
		if( tview ){
			select = gtk_tree_view_get_selection( tview );
			if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
				gtk_tree_model_get( tmodel, &iter, OPE_TEMPLATE_COL_MNEMO, &mnemo, -1 );
			}
		}
	}

	return( mnemo );
}

/**
 * ofa_ope_template_book_bin_set_selected:
 *
 * Let the user reset the selection after the end of setup and
 * initialization phases
 */
void
ofa_ope_template_book_bin_set_selected( ofaOpeTemplateBookBin *bin, const gchar *mnemo )
{
	ofaOpeTemplateBookBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_OPE_TEMPLATE_BOOK_BIN( bin ));

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		select_row_by_mnemo( bin, mnemo );
	}
}

/*
 * select the row with the given number, or the most close one
 * doesn't create the page class if it doesn't yet exist
 */
static void
select_row_by_mnemo( ofaOpeTemplateBookBin *bin, const gchar *mnemo )
{
	ofaOpeTemplateBookBinPrivate *priv;
	ofoOpeTemplate *ope;
	const gchar *ledger;
	GtkWidget *page_w;
	gint page_n;
	GtkWidget *tview;
	GtkTreeModel *tfilter;
	GtkTreeIter store_iter, filter_iter;
	GtkTreePath *path;

	priv = bin->priv;

	if( my_strlen( mnemo )){
		ope = ofo_ope_template_get_by_mnemo( priv->hub, mnemo );
		if( ope ){
			g_return_if_fail( OFO_IS_OPE_TEMPLATE( ope ));
			ledger = ofo_ope_template_get_ledger( ope );
			g_debug( "select_row_by_mnemo: ledger=%s", ledger );
			if( my_strlen( ledger )){
				page_w = book_get_page_by_ledger( bin, ledger, FALSE );
				if( page_w ){
					page_n = gtk_notebook_page_num( priv->book, page_w );
					gtk_notebook_set_current_page( priv->book, page_n );

					ofa_ope_template_store_get_by_mnemo( priv->ope_store, mnemo, &store_iter );

					tview = my_utils_container_get_child_by_type(
									GTK_CONTAINER( page_w ), GTK_TYPE_TREE_VIEW );
					tfilter = gtk_tree_view_get_model( GTK_TREE_VIEW( tview ));
					gtk_tree_model_filter_convert_child_iter_to_iter(
							GTK_TREE_MODEL_FILTER( tfilter ), &filter_iter, &store_iter );

					path = gtk_tree_model_get_path( tfilter, &filter_iter );
					gtk_tree_view_expand_to_path( GTK_TREE_VIEW( tview ), path );
					gtk_tree_path_free( path );

					select_row_by_iter( bin, GTK_TREE_VIEW( tview ), tfilter, &filter_iter );
				}
			}
		}
	}
}

static void
select_row_by_iter( ofaOpeTemplateBookBin *bin, GtkTreeView *tview, GtkTreeModel *tfilter, GtkTreeIter *iter )
{
	GtkTreePath *path;

	path = gtk_tree_model_get_path( tfilter, iter );
	gtk_tree_view_set_cursor( GTK_TREE_VIEW( tview ), path, NULL, FALSE );
	gtk_tree_path_free( path );
	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}

/**
 * ofa_ope_template_book_bin_get_current_treeview:
 *
 * Returns the treeview of the current page.
 */
GtkWidget *
ofa_ope_template_book_bin_get_current_treeview( const ofaOpeTemplateBookBin *bin )
{
	ofaOpeTemplateBookBinPrivate *priv;
	GtkWidget *tview;

	g_return_val_if_fail( bin && OFA_IS_OPE_TEMPLATE_BOOK_BIN( bin ), NULL );

	priv = bin->priv;
	tview = NULL;

	if( !priv->dispose_has_run ){

		tview = get_current_tree_view( bin );
	}

	return( tview );
}

/**
 * ofa_ope_template_book_bin_button_clicked:
 */
void
ofa_ope_template_book_bin_button_clicked( ofaOpeTemplateBookBin *bin, gint button_id )
{
	static const gchar *thisfn = "ofa_ope_template_book_bin_button_clicked";
	ofaOpeTemplateBookBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_OPE_TEMPLATE_BOOK_BIN( bin ));

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		switch( button_id ){
			case TEMPLATE_BUTTON_NEW:
				do_insert_ope_template( bin );
				break;
			case TEMPLATE_BUTTON_PROPERTIES:
				do_update_ope_template( bin );
				break;
			case TEMPLATE_BUTTON_DUPLICATE:
				do_duplicate_ope_template( bin );
				break;
			case TEMPLATE_BUTTON_DELETE:
				do_delete_ope_template( bin );
				break;
			case TEMPLATE_BUTTON_GUIDED_INPUT:
				do_guided_input( bin );
				break;
			default:
				g_warning( "%s: unmanaged button_id=%d", thisfn, button_id );
				break;
		}
	}
}

static void
on_action_closed( ofaOpeTemplateBookBin *bin )
{
	static const gchar *thisfn = "ofa_ope_template_book_bin_on_action_closed";

	g_debug( "%s: bin=%p", thisfn, ( void * ) bin );

	write_settings( bin );
}

static void
write_settings( ofaOpeTemplateBookBin *bin )
{
	ofaOpeTemplateBookBinPrivate *priv;
	GList *strlist;
	gint i, count;
	GtkWidget *page;
	sPageData *sdata;

	priv = bin->priv;
	strlist = NULL;

	/* record in settings the pages position */
	count = gtk_notebook_get_n_pages( priv->book );
	for( i=0 ; i<count ; ++i ){
		page = gtk_notebook_get_nth_page( priv->book, i );
		sdata = ( sPageData * ) g_object_get_data( G_OBJECT( page ), DATA_PAGE_LEDGER );
		if( g_utf8_collate( sdata->ledger, UNKNOWN_LEDGER_MNEMO )){
			strlist = g_list_append( strlist, ( gpointer ) sdata->ledger );
		}
	}

	ofa_settings_dossier_set_string_list( priv->meta, st_ledger_order, strlist );

	g_list_free( strlist );
}
