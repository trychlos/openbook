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

#include "my/my-utils.h"

#include "api/ofa-buttons-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-igetter.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"

#include "core/ofa-guided-input.h"
#include "core/ofa-ope-template-frame-bin.h"
#include "core/ofa-ope-template-properties.h"
#include "core/ofa-ope-template-store.h"

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
	gboolean             is_current;
	ofaOpeTemplateStore *store;
	GList               *store_handlers;
	ofaIDBMeta          *meta;

	/* UI
	 */
	GtkWidget           *grid;
	GtkWidget           *notebook;
	ofaButtonsBox       *buttonsbox;
	GList               *buttons;
}
	ofaOpeTemplateFrameBinPrivate;

/* a structure to descriibe each displayed button
 * @sensitive: if FALSE, the button is never sensitive, whatever be the
 *  current conditions.
 */
typedef struct {
	ofeOpeTemplateFrameBtn id;
	GtkWidget             *btn;
	gboolean               sensitive;
}
	sButton;

/* this piece of data is attached to each page of the notebook
 */
typedef struct {
	ofaHub *hub;
	gchar  *ledger;
}
	sPageData;

#define DATA_PAGE_LEDGER                  "ofa-data-page-ledger"

/* the column identifier is attached to each column header
 */
#define DATA_COLUMN_ID                    "ofa-data-column-id"

/* a settings which holds the order of ledger mnemos as a string list
 */
static const gchar *st_ledger_order     = "ofa-OpeTemplateBookOrder";

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	CLOSED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void       setup_bin( ofaOpeTemplateFrameBin *self );
static GtkWidget *book_get_page_by_ledger( ofaOpeTemplateFrameBin *self, const gchar *ledger, gboolean bcreate );
static GtkWidget *book_create_page( ofaOpeTemplateFrameBin *self, const gchar *ledger );
static void       book_on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaOpeTemplateFrameBin *self );
static void       book_on_finalized_page( sPageData *sdata, gpointer finalized_page );
static GtkWidget *page_add_treeview( ofaOpeTemplateFrameBin *self, GtkWidget *page );
static void       page_add_columns( ofaOpeTemplateFrameBin *self, GtkTreeView *tview );
static void       tview_on_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaOpeTemplateFrameBin *self );
static gboolean   tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, GtkWidget *page );
static void       tview_on_row_selected( GtkTreeSelection *selection, ofaOpeTemplateFrameBin *self );
static void       tview_on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaOpeTemplateFrameBin *self );
static gboolean   tview_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaOpeTemplateFrameBin *self );
static void       tview_on_key_insert( ofaOpeTemplateFrameBin *self );
static void       tview_on_key_delete( ofaOpeTemplateFrameBin *self );
static void       tview_select_row_by_mnemo( ofaOpeTemplateFrameBin *self, const gchar *mnemo );
static void       tview_select_row_by_iter( ofaOpeTemplateFrameBin *self, GtkTreeView *tview, GtkTreeModel *tfilter, GtkTreeIter *iter );
static GtkWidget *button_add( ofaOpeTemplateFrameBin *self, ofeOpeTemplateFrameBtn id, const gchar *label, gboolean sensitive, GCallback cb );
static sButton   *button_find_by_id( GList **list, ofeOpeTemplateFrameBtn id, gboolean create );
static void       button_update_sensitivity( ofaOpeTemplateFrameBin *self, const gchar *mnemo );
static void       button_on_new_clicked( GtkButton *button, ofaOpeTemplateFrameBin *self );
static void       button_on_properties_clicked( GtkButton *button, ofaOpeTemplateFrameBin *self );
static void       button_on_delete_clicked( GtkButton *button, ofaOpeTemplateFrameBin *self );
static void       button_on_duplicate_clicked( GtkButton *button, ofaOpeTemplateFrameBin *self );
static void       button_on_guided_input_clicked( GtkButton *button, ofaOpeTemplateFrameBin *self );
static gboolean   is_new_allowed( ofaOpeTemplateFrameBin *self, sButton *sbtn );
static gboolean   is_delete_allowed( ofaOpeTemplateFrameBin *self, sButton *sbtn, ofoOpeTemplate *selected );
static void       do_insert_ope_template( ofaOpeTemplateFrameBin *self );
static void       do_update_ope_template( ofaOpeTemplateFrameBin *self, const gchar *mnemo );
static void       do_delete_ope_template( ofaOpeTemplateFrameBin *self, const gchar *mnemo );
static gboolean   delete_confirmed( ofaOpeTemplateFrameBin *self, ofoOpeTemplate *ope );
static void       do_duplicate_ope_template( ofaOpeTemplateFrameBin *self, const gchar *mnemo );
static void       do_guided_input( ofaOpeTemplateFrameBin *self, const gchar *mnemo );
static void       store_on_row_inserted( GtkTreeModel *tmodel, GtkTreePath *path, GtkTreeIter *iter, ofaOpeTemplateFrameBin *self );
static void       hub_connect_to_signaling_system( ofaOpeTemplateFrameBin *self );
static void       hub_on_new_object( ofaHub *hub, ofoBase *object, ofaOpeTemplateFrameBin *self );
static void       hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaOpeTemplateFrameBin *self );
static void       hub_on_updated_ledger_label( ofaOpeTemplateFrameBin *self, ofoLedger *ledger );
static void       hub_on_updated_ope_template( ofaOpeTemplateFrameBin *self, ofoOpeTemplate *template );
static void       hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaOpeTemplateFrameBin *self );
static void       hub_on_deleted_ledger_object( ofaOpeTemplateFrameBin *self, ofoLedger *ledger );
static void       hub_on_reload_dataset( ofaHub *hub, GType type, ofaOpeTemplateFrameBin *self );
static void       write_settings( ofaOpeTemplateFrameBin *self );
static void       free_button( sButton *sbtn );

G_DEFINE_TYPE_EXTENDED( ofaOpeTemplateFrameBin, ofa_ope_template_frame_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaOpeTemplateFrameBin ))

static void
ope_template_frame_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_finalize";
	ofaOpeTemplateFrameBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_FRAME_BIN( instance ));

	priv = ofa_ope_template_frame_bin_get_instance_private( OFA_OPE_TEMPLATE_FRAME_BIN( instance ));

	/* free data members here */
	g_list_free_full( priv->buttons, ( GDestroyNotify ) free_button );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_frame_bin_parent_class )->finalize( instance );
}

static void
ope_template_frame_bin_dispose( GObject *instance )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	GList *it;

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_FRAME_BIN( instance ));

	priv = ofa_ope_template_frame_bin_get_instance_private( OFA_OPE_TEMPLATE_FRAME_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->meta );

		/* disconnect from ofaHub signaling system */
		ofa_hub_disconnect_handlers( priv->hub, priv->hub_handlers );

		/* disconnect from ofaOpeTemplateStore */
		if( priv->store && OFA_IS_OPE_TEMPLATE_STORE( priv->store )){
			for( it=priv->store_handlers ; it ; it=it->next ){
				g_signal_handler_disconnect( priv->store, ( gulong ) it->data );
			}
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_frame_bin_parent_class )->dispose( instance );
}

static void
ofa_ope_template_frame_bin_init( ofaOpeTemplateFrameBin *self )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_init";
	ofaOpeTemplateFrameBinPrivate *priv;

	g_return_if_fail( self && OFA_IS_OPE_TEMPLATE_FRAME_BIN( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_ope_template_frame_bin_class_init( ofaOpeTemplateFrameBinClass *klass )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ope_template_frame_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_template_frame_bin_finalize;

	/**
	 * ofaOpeTemplateFrameBin::changed:
	 *
	 * This signal is sent when the selection is changed.
	 *
	 * Argument is the selected operation template mnemo.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaOpeTemplateFrameBin *frame,
	 * 						const gchar        *mnemo,
	 * 						gpointer            user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_OPE_TEMPLATE_FRAME_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_STRING );

	/**
	 * ofaOpeTemplateFrameBin::activated:
	 *
	 * This signal is sent when the selection is activated.
	 *
	 * Argument is the selected account mnemo.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaOpeTemplateFrameBin *frame,
	 * 						const gchar        *mnemo,
	 * 						gpointer            user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-activated",
				OFA_TYPE_OPE_TEMPLATE_FRAME_BIN,
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
 * ofa_ope_template_frame_bin_new:
 * @getter: a #ofaIGetter instance.
 *
 * Creates the structured content, i.e. The accounts notebook on the
 * left column, the buttons box on the right one.
 *
 * +-----------------------------------------------------------------------+
 * | parent container:                                                     |
 * |   this is the grid of the main page,                                  |
 * |   or any another container (i.e. a frame)                             |
 * | +-------------------------------------------------------------------+ |
 * | | creates a grid which will contain the frame and the buttons       | |
 * | | +---------------------------------------------+-----------------+ + |
 * | | | creates a notebook where each page contains | creates         | | |
 * | | |   the account of the corresponding class    |   a buttons box | | |
 * | | |   (cf. ofaOpeTemplateFrameBin class)        |                 | | |
 * | | |                                             |                 | | |
 * | | +---------------------------------------------+-----------------+ | |
 * | +-------------------------------------------------------------------+ |
 * +-----------------------------------------------------------------------+
 */
ofaOpeTemplateFrameBin *
ofa_ope_template_frame_bin_new( ofaIGetter *getter  )
{
	ofaOpeTemplateFrameBin *self;
	ofaOpeTemplateFrameBinPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	self = g_object_new( OFA_TYPE_OPE_TEMPLATE_FRAME_BIN, NULL );

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	priv->getter = getter;

	setup_bin( self );

	return( self );
}

static void
setup_bin( ofaOpeTemplateFrameBin *self )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	ofoDossier *dossier;
	gulong handler;
	GList *strlist, *it;
	const ofaIDBConnect *connect;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

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

	/* UI buttons box */
	priv->buttonsbox = ofa_buttons_box_new();
	gtk_grid_attach( GTK_GRID( priv->grid ), GTK_WIDGET( priv->buttonsbox ), 1, 0, 1, 1 );

	/* ope template store */
	priv->hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	priv->store = ofa_ope_template_store_new( priv->hub );

	connect = ofa_hub_get_connect( priv->hub );
	priv->meta = ofa_idbconnect_get_meta( connect );

	/* create one page per ledger
	 * if strlist is set, then create one page per ledger
	 * other needed pages will be created on fly
	 * nb: if the ledger no more exists, no page is created */
	strlist = ofa_settings_dossier_get_string_list( priv->meta, st_ledger_order );
	for( it=strlist ; it ; it=it->next ){
		book_get_page_by_ledger( self, ( const gchar * ) it->data, TRUE );
	}
	ofa_settings_free_string_list( strlist );

	handler = g_signal_connect( priv->store, "ofa-row-inserted", G_CALLBACK( store_on_row_inserted ), self );
	priv->store_handlers = g_list_prepend( priv->store_handlers, ( gpointer ) handler );

	ofa_list_store_load_dataset( OFA_LIST_STORE( priv->store ));

	/* runtime */
	dossier = ofa_hub_get_dossier( priv->hub );
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	priv->is_current = ofo_dossier_is_current( dossier );

	/* hub signaling system */
	hub_connect_to_signaling_system( self );

	gtk_notebook_set_current_page( GTK_NOTEBOOK( priv->notebook ), 0 );
}

/*
 * Returns the notebook's page container which is dedicated to the
 * given ledger.
 *
 * If the page doesn't exist, and @bcreate is %TRUE, then it is created.
 */
static GtkWidget *
book_get_page_by_ledger( ofaOpeTemplateFrameBin *self, const gchar *ledger, gboolean bcreate )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_get_page_by_ledger";
	ofaOpeTemplateFrameBinPrivate *priv;
	gint count, i;
	GtkWidget *found, *page_widget;
	sPageData *sdata;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	found = NULL;
	count = gtk_notebook_get_n_pages( GTK_NOTEBOOK( priv->notebook ));

	/* search for an existing page */
	for( i=0 ; i<count ; ++i ){
		page_widget = gtk_notebook_get_nth_page( GTK_NOTEBOOK( priv->notebook ), i );
		sdata = ( sPageData * ) g_object_get_data( G_OBJECT( page_widget ), DATA_PAGE_LEDGER );
		if( sdata && !g_utf8_collate( sdata->ledger, ledger )){
			found = page_widget;
			break;
		}
	}

	/* if not exists, create it (if allowed) */
	if( !found && bcreate ){
		found = book_create_page( self, ledger );
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
 * creates the page widget for the given ledger
 */
static GtkWidget *
book_create_page( ofaOpeTemplateFrameBin *self, const gchar *ledger )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_book_create_page";
	ofaOpeTemplateFrameBinPrivate *priv;
	GtkWidget *frame, *scrolled, *tview, *label;
	ofoLedger *ledger_obj;
	const gchar *ledger_label;
	gint page_num;
	sPageData *sdata;

	g_debug( "%s: self=%p, ledger=%s", thisfn, ( void * ) self, ledger );

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	/* get ledger label */
	if( !my_collate( ledger, UNKNOWN_LEDGER_MNEMO )){
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
	g_object_weak_ref( G_OBJECT( frame ), ( GWeakNotify ) book_on_finalized_page, sdata );

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
	label = gtk_label_new( ledger_label );

	page_num = gtk_notebook_append_page( GTK_NOTEBOOK( priv->notebook ), frame, label );
	if( page_num == -1 ){
		g_warning( "%s: unable to add a page to the notebook for ledger=%s", thisfn, ledger );
		gtk_widget_destroy( frame );
		return( NULL );
	}
	gtk_notebook_set_tab_reorderable( GTK_NOTEBOOK( priv->notebook ), frame, TRUE );

	return( frame );
}

/*
 * we have switch to this given page (wpage, npage)
 * just setup the selection
 */
static void
book_on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaOpeTemplateFrameBin *self )
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

static void
book_on_finalized_page( sPageData *sdata, gpointer finalized_page )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_book_on_finalized_page";

	g_debug( "%s: sdata=%p, finalized_page=%p", thisfn, ( void * ) sdata, ( void * ) finalized_page );

	g_free( sdata->ledger );
	g_free( sdata );
}

/*
 * creates the treeview
 * attach some piece of data to it
 */
static GtkWidget *
page_add_treeview( ofaOpeTemplateFrameBin *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_create_treeview";
	ofaOpeTemplateFrameBinPrivate *priv;
	GtkWidget *tview;
	GtkTreeModel *tfilter;
	GtkTreeSelection *select;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

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
page_add_columns( ofaOpeTemplateFrameBin *self, GtkTreeView *tview )
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
			column, cell, ( GtkTreeCellDataFunc ) tview_on_cell_data_func, self, NULL );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			cell, "text", OPE_TEMPLATE_COL_LABEL,
			NULL );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( OPE_TEMPLATE_COL_LABEL ));
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, cell, ( GtkTreeCellDataFunc ) tview_on_cell_data_func, self, NULL );
}

static GtkWidget *
page_get_treeview( const ofaOpeTemplateFrameBin *self )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	gint page_n;
	GtkWidget *page_w;
	GtkWidget *tview;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );
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
 * ofa_ope_template_frame_bin_get_current_treeview:
 * @bin:
 *
 * Returns the treeview associated to the current page.
 */
GtkWidget *
ofa_ope_template_frame_bin_get_current_treeview( const ofaOpeTemplateFrameBin *bin )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_get_current_treeview";
	ofaOpeTemplateFrameBinPrivate *priv;
	GtkWidget *tview;

	g_debug( "%s: bin=%p", thisfn, ( void * ) bin );

	g_return_val_if_fail( bin && OFA_IS_OPE_TEMPLATE_FRAME_BIN( bin ), NULL );

	priv = ofa_ope_template_frame_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	tview = page_get_treeview( bin );

	return( tview );
}

/**
 * ofa_ope_template_frame_bin_get_selected:
 * @bin:
 *
 * Returns: the currently selected operation template, as a newly
 * allocated string which should be g_free() by the caller.
 */
gchar *
ofa_ope_template_frame_bin_get_selected( ofaOpeTemplateFrameBin *bin )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_get_selected";
	ofaOpeTemplateFrameBinPrivate *priv;
	gchar *mnemo;
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	g_debug( "%s: bin=%p", thisfn, ( void * ) bin );

	g_return_val_if_fail( bin && OFA_IS_OPE_TEMPLATE_FRAME_BIN( bin ), NULL );

	priv = ofa_ope_template_frame_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	mnemo = NULL;
	tview = ( GtkTreeView * ) page_get_treeview( bin );

	if( tview ){
		select = gtk_tree_view_get_selection( tview );
		if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
			gtk_tree_model_get( tmodel, &iter, OPE_TEMPLATE_COL_MNEMO, &mnemo, -1 );
		}
	}

	return( mnemo );
}

/**
 * ofa_ope_template_frame_bin_set_selected:
 * @bin: this #ofaOpeTemplateFrameBin instance
 * @mnemo: the operation template mnemonic to be selected.
 *
 * Let the user reset the selection after the end of setup and
 * initialization phases
 */
void
ofa_ope_template_frame_bin_set_selected( ofaOpeTemplateFrameBin *bin, const gchar *mnemo )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_set_selected";
	ofaOpeTemplateFrameBinPrivate *priv;

	g_debug( "%s: bin=%p", thisfn, ( void * ) bin );

	g_return_if_fail( bin && OFA_IS_OPE_TEMPLATE_FRAME_BIN( bin ));

	priv = ofa_ope_template_frame_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	tview_select_row_by_mnemo( bin, mnemo );
}

/*
 * no particular style here
 */
static void
tview_on_cell_data_func( GtkTreeViewColumn *tcolumn,
						GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
						ofaOpeTemplateFrameBin *self )
{
	g_return_if_fail( GTK_IS_CELL_RENDERER_TEXT( cell ));
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
tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, GtkWidget *page )
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
tview_on_row_selected( GtkTreeSelection *selection, ofaOpeTemplateFrameBin *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *mnemo;

	mnemo = NULL;

	/* selection may be null when called from on_delete_clicked() */
	if( selection ){
		if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
			gtk_tree_model_get( tmodel, &iter, OPE_TEMPLATE_COL_MNEMO, &mnemo, -1 );
			button_update_sensitivity( self, mnemo );
			g_signal_emit_by_name( self, "ofa-changed", mnemo );
			g_free( mnemo );
		}
	}

	/*update_buttons_sensitivity( bin, account );*/
}

static void
tview_on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaOpeTemplateFrameBin *self )
{
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *mnemo;

	mnemo = NULL;

	select = gtk_tree_view_get_selection( tview );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, OPE_TEMPLATE_COL_MNEMO, &mnemo, -1 );
		g_signal_emit_by_name( self, "ofa-activated", mnemo );
		g_free( mnemo );
	}
}

/*
 * Returns :
 * TRUE to stop other hub_handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
tview_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaOpeTemplateFrameBin *self )
{
	gboolean stop;

	stop = FALSE;

	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Insert ){
			tview_on_key_insert( self );

		} else if( event->keyval == GDK_KEY_Delete ){
			tview_on_key_delete( self );
		}
	}

	return( stop );
}

static void
tview_on_key_insert( ofaOpeTemplateFrameBin *self )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	sButton *sbtn;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	sbtn = button_find_by_id( &priv->buttons, TEMPLATE_BTN_NEW, FALSE );
	if( is_new_allowed( self, sbtn )){
		do_insert_ope_template( self );
	}
}

static void
tview_on_key_delete( ofaOpeTemplateFrameBin *self )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	sButton *sbtn;
	gchar *mnemo;
	ofoOpeTemplate *ope;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	mnemo = ofa_ope_template_frame_bin_get_selected( self );
	if( my_strlen( mnemo )){
		ope = ofo_ope_template_get_by_mnemo( priv->hub, mnemo );
		g_return_if_fail( ope && OFO_IS_OPE_TEMPLATE( ope ));

		sbtn = button_find_by_id( &priv->buttons, TEMPLATE_BTN_DELETE, FALSE );
		if( is_delete_allowed( self, sbtn, ope )){
			do_delete_ope_template( self, mnemo );
		}
	}
	g_free( mnemo );
}

/*
 * select the row with the given number, or the most close one
 * doesn't create the page class if it doesn't yet exist
 */
static void
tview_select_row_by_mnemo( ofaOpeTemplateFrameBin *self, const gchar *mnemo )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	ofoOpeTemplate *ope;
	const gchar *ledger;
	GtkWidget *page_w;
	gint page_n;
	GtkWidget *tview;
	GtkTreeModel *tfilter;
	GtkTreeIter store_iter, filter_iter;
	GtkTreePath *path;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	if( my_strlen( mnemo )){
		ope = ofo_ope_template_get_by_mnemo( priv->hub, mnemo );
		if( ope ){
			g_return_if_fail( OFO_IS_OPE_TEMPLATE( ope ));
			ledger = ofo_ope_template_get_ledger( ope );
			g_debug( "tview_select_row_by_mnemo: ledger=%s", ledger );
			if( my_strlen( ledger )){
				page_w = book_get_page_by_ledger( self, ledger, FALSE );
				if( page_w ){
					page_n = gtk_notebook_page_num( GTK_NOTEBOOK( priv->notebook ), page_w );
					gtk_notebook_set_current_page( GTK_NOTEBOOK( priv->notebook ), page_n );

					ofa_ope_template_store_get_by_mnemo( priv->store, mnemo, &store_iter );

					tview = my_utils_container_get_child_by_type(
									GTK_CONTAINER( page_w ), GTK_TYPE_TREE_VIEW );
					tfilter = gtk_tree_view_get_model( GTK_TREE_VIEW( tview ));
					gtk_tree_model_filter_convert_child_iter_to_iter(
							GTK_TREE_MODEL_FILTER( tfilter ), &filter_iter, &store_iter );

					path = gtk_tree_model_get_path( tfilter, &filter_iter );
					gtk_tree_view_expand_to_path( GTK_TREE_VIEW( tview ), path );
					gtk_tree_path_free( path );

					tview_select_row_by_iter( self, GTK_TREE_VIEW( tview ), tfilter, &filter_iter );
				}
			}
		}
	}
}

static void
tview_select_row_by_iter( ofaOpeTemplateFrameBin *self, GtkTreeView *tview, GtkTreeModel *tfilter, GtkTreeIter *iter )
{
	GtkTreePath *path;

	path = gtk_tree_model_get_path( tfilter, iter );
	gtk_tree_view_set_cursor( GTK_TREE_VIEW( tview ), path, NULL, FALSE );
	gtk_tree_path_free( path );
	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}

/**
 * ofa_ope_template_frame_bin_add_button:
 * @bin: this #ofaOpeTemplateFrameBin instance.
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
ofa_ope_template_frame_bin_add_button( ofaOpeTemplateFrameBin *bin, ofeOpeTemplateFrameBtn id, gboolean sensitive )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	GtkWidget *button;

	g_return_val_if_fail( bin && OFA_IS_OPE_TEMPLATE_FRAME_BIN( bin ), NULL );

	priv = ofa_ope_template_frame_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	button = NULL;

	switch( id ){
		case TEMPLATE_BTN_SPACER:
			ofa_buttons_box_add_spacer( priv->buttonsbox );
			break;
		case TEMPLATE_BTN_NEW:
			button = button_add( bin, TEMPLATE_BTN_NEW, BUTTON_NEW, sensitive, G_CALLBACK( button_on_new_clicked ));
			gtk_widget_set_sensitive( button, sensitive && priv->is_current );
			break;
		case TEMPLATE_BTN_PROPERTIES:
			button = button_add( bin, TEMPLATE_BTN_PROPERTIES, BUTTON_PROPERTIES, sensitive, G_CALLBACK( button_on_properties_clicked ));
			gtk_widget_set_sensitive( button, sensitive );
			break;
		case TEMPLATE_BTN_DELETE:
			button = button_add( bin, TEMPLATE_BTN_DELETE, BUTTON_DELETE, sensitive, G_CALLBACK( button_on_delete_clicked ));
			gtk_widget_set_sensitive( button, sensitive && priv->is_current );
			break;
		case TEMPLATE_BTN_DUPLICATE:
			button = button_add( bin, TEMPLATE_BTN_DUPLICATE, _( "Duplicate..." ), sensitive, G_CALLBACK( button_on_duplicate_clicked ));
			gtk_widget_set_sensitive( button, sensitive );
			break;
		case TEMPLATE_BTN_GUIDED_INPUT:
			button = button_add( bin, TEMPLATE_BTN_GUIDED_INPUT, _( "Guided input..." ), sensitive, G_CALLBACK( button_on_guided_input_clicked ));
			gtk_widget_set_sensitive( button, sensitive && priv->is_current );
			break;
		default:
			break;
	}

	return( button );
}

static GtkWidget *
button_add( ofaOpeTemplateFrameBin *self, ofeOpeTemplateFrameBtn id, const gchar *label, gboolean sensitive, GCallback cb )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	sButton *sbtn;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	sbtn = button_find_by_id( &priv->buttons, id, TRUE );
	sbtn->sensitive = sensitive;
	sbtn->btn = ofa_buttons_box_add_button_with_mnemonic( priv->buttonsbox, label, cb, self );

	return( sbtn->btn );
}

static sButton *
button_find_by_id( GList **list, ofeOpeTemplateFrameBtn id, gboolean create )
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
button_update_sensitivity( ofaOpeTemplateFrameBin *self, const gchar *mnemo )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	ofoOpeTemplate *template;
	gboolean has_template;
	sButton *sbtn;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	has_template = FALSE;
	template = NULL;

	if( my_strlen( mnemo )){
		template = ofo_ope_template_get_by_mnemo( priv->hub, mnemo );
		has_template = ( template && OFO_IS_OPE_TEMPLATE( template ));
	}

	sbtn = button_find_by_id( &priv->buttons, TEMPLATE_BTN_NEW, FALSE );
	if( sbtn ){
		g_return_if_fail( sbtn->btn && GTK_IS_WIDGET( sbtn->btn ));
		gtk_widget_set_sensitive( sbtn->btn, is_new_allowed( self, sbtn ));
	}
	sbtn = button_find_by_id( &priv->buttons, TEMPLATE_BTN_PROPERTIES, FALSE );
	if( sbtn ){
		g_return_if_fail( sbtn->btn && GTK_IS_WIDGET( sbtn->btn ));
		gtk_widget_set_sensitive( sbtn->btn, sbtn->sensitive && has_template );
	}
	sbtn = button_find_by_id( &priv->buttons, TEMPLATE_BTN_DELETE, FALSE );
	if( sbtn ){
		g_return_if_fail( sbtn->btn && GTK_IS_WIDGET( sbtn->btn ));
		gtk_widget_set_sensitive( sbtn->btn, is_delete_allowed( self, sbtn, template ));
	}
	sbtn = button_find_by_id( &priv->buttons, TEMPLATE_BTN_DUPLICATE, FALSE );
	if( sbtn ){
		g_return_if_fail( sbtn->btn && GTK_IS_WIDGET( sbtn->btn ));
		gtk_widget_set_sensitive( sbtn->btn, sbtn->sensitive && has_template && priv->is_current );
	}
	sbtn = button_find_by_id( &priv->buttons, TEMPLATE_BTN_GUIDED_INPUT, FALSE );
	if( sbtn ){
		g_return_if_fail( sbtn->btn && GTK_IS_WIDGET( sbtn->btn ));
		gtk_widget_set_sensitive( sbtn->btn, sbtn->sensitive && has_template && priv->is_current );
	}
}

static void
button_on_new_clicked( GtkButton *button, ofaOpeTemplateFrameBin *self )
{
	do_insert_ope_template( self );
}

static void
button_on_properties_clicked( GtkButton *button, ofaOpeTemplateFrameBin *self )
{
	gchar *mnemo;

	mnemo = ofa_ope_template_frame_bin_get_selected( self );
	if( my_strlen( mnemo )){
		do_update_ope_template( self, mnemo );
	}
	g_free( mnemo );
}

static void
button_on_delete_clicked( GtkButton *button, ofaOpeTemplateFrameBin *self )
{
	gchar *mnemo;

	mnemo = ofa_ope_template_frame_bin_get_selected( self );
	if( my_strlen( mnemo )){
		do_delete_ope_template( self, mnemo );
	}
	g_free( mnemo );
}

static void
button_on_duplicate_clicked( GtkButton *button, ofaOpeTemplateFrameBin *self )
{
	gchar *mnemo;

	mnemo = ofa_ope_template_frame_bin_get_selected( self );
	if( my_strlen( mnemo )){
		do_duplicate_ope_template( self, mnemo );
	}
	g_free( mnemo );
}

static void
button_on_guided_input_clicked( GtkButton *button, ofaOpeTemplateFrameBin *self )
{
	gchar *mnemo;

	mnemo = ofa_ope_template_frame_bin_get_selected( self );
	if( my_strlen( mnemo )){
		do_guided_input( self, mnemo );
	}
	g_free( mnemo );
}

static gboolean
is_new_allowed( ofaOpeTemplateFrameBin *self, sButton *sbtn )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	gboolean ok;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	ok = sbtn && sbtn->btn && sbtn->sensitive && priv->is_current;

	return( ok );
}

static gboolean
is_delete_allowed( ofaOpeTemplateFrameBin *self, sButton *sbtn, ofoOpeTemplate *selected )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	gboolean ok;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	ok = sbtn && sbtn->btn && sbtn->sensitive && priv->is_current && selected && ofo_ope_template_is_deletable( selected );

	return( ok );
}

static void
do_insert_ope_template( ofaOpeTemplateFrameBin *self )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	ofoOpeTemplate *ope;
	gint page_n;
	GtkWidget *page_w;
	GtkWindow *toplevel;
	const gchar *ledger;
	sPageData *sdata;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	ledger = NULL;

	page_n = gtk_notebook_get_current_page( GTK_NOTEBOOK( priv->notebook ));
	if( page_n >= 0 ){
		page_w = gtk_notebook_get_nth_page( GTK_NOTEBOOK( priv->notebook ), page_n );
		sdata = ( sPageData * ) g_object_get_data( G_OBJECT( page_w ), DATA_PAGE_LEDGER );
		if( sdata ){
			ledger = sdata->ledger;
		}
	}

	ope = ofo_ope_template_new();
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_ope_template_properties_run( priv->getter, toplevel, ope, ledger );
}

static void
do_update_ope_template( ofaOpeTemplateFrameBin *self, const gchar *mnemo )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	ofoOpeTemplate *ope;
	GtkWindow *toplevel;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	ope = ofo_ope_template_get_by_mnemo( priv->hub, mnemo );
	g_return_if_fail( ope && OFO_IS_OPE_TEMPLATE( ope ));

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_ope_template_properties_run( priv->getter, toplevel, ope, NULL );
}

static void
do_delete_ope_template( ofaOpeTemplateFrameBin *self, const gchar *mnemo )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	ofoOpeTemplate *ope;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	ope = ofo_ope_template_get_by_mnemo( priv->hub, mnemo );
	g_return_if_fail( ope && OFO_IS_OPE_TEMPLATE( ope ) && ofo_ope_template_is_deletable( ope ));

	if( delete_confirmed( self, ope ) &&
			ofo_ope_template_delete( ope )){

		/* nothing to do here, all being managed by signal hub_handlers
		 * just reset the selection as this is not managed by the
		 * ope notebook (and doesn't have to)
		 * asking for selection of the just deleted ope makes
		 * almost sure that we are going to select the most close
		 * row */
		tview_on_row_selected( NULL, self );
		ofa_ope_template_frame_bin_set_selected( self, mnemo );
	}
}

/*
 * - this is a root account with children and the preference is set so
 *   that all accounts will be deleted
 * - this is a root account and the preference is not set
 * - this is a detail account
 */
static gboolean
delete_confirmed( ofaOpeTemplateFrameBin *self, ofoOpeTemplate *ope )
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
do_duplicate_ope_template( ofaOpeTemplateFrameBin *self, const gchar *mnemo )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_do_duplicate_ope_template";
	ofaOpeTemplateFrameBinPrivate *priv;
	gchar *new_mnemo;
	ofoOpeTemplate *ope;
	ofoOpeTemplate *duplicate;
	gchar *str;

	g_debug( "%s: self=%p, mnemo=%s", thisfn, ( void * ) self, mnemo );

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

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
		tview_select_row_by_mnemo( self, new_mnemo );
	}

	g_free( new_mnemo );
}

static void
do_guided_input( ofaOpeTemplateFrameBin *self, const gchar *mnemo )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	ofoOpeTemplate *ope;
	GtkWindow *toplevel;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	ope = ofo_ope_template_get_by_mnemo( priv->hub, mnemo );
	g_return_if_fail( ope && OFO_IS_OPE_TEMPLATE( ope ));

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_guided_input_run( priv->getter, toplevel, ope );
}

/*
 * is triggered by the store when a row is inserted
 */
static void
store_on_row_inserted( GtkTreeModel *tmodel, GtkTreePath *path, GtkTreeIter *iter, ofaOpeTemplateFrameBin *self )
{
	ofoOpeTemplate *ope;
	const gchar *ledger;

	gtk_tree_model_get( tmodel, iter, OPE_TEMPLATE_COL_OBJECT, &ope, -1 );
	g_return_if_fail( ope && OFO_IS_OPE_TEMPLATE( ope ));
	g_object_unref( ope );

	ledger = ofo_ope_template_get_ledger( ope );
	if( !book_get_page_by_ledger( self, ledger, TRUE )){
		book_get_page_by_ledger( self, UNKNOWN_LEDGER_MNEMO, TRUE );
	}
}

static void
hub_connect_to_signaling_system( ofaOpeTemplateFrameBin *self )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	gulong handler;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

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
hub_on_new_object( ofaHub *hub, ofoBase *object, ofaOpeTemplateFrameBin *self )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_hub_on_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn, ( void * ) hub,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), ( void * ) self );
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaOpeTemplateFrameBin *self )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_hub_on_updated_object";

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn, ( void * ) hub,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), prev_id, ( void * ) self );

	if( OFO_IS_LEDGER( object )){
		hub_on_updated_ledger_label( self, OFO_LEDGER( object ));

	} else if( OFO_IS_OPE_TEMPLATE( object )){
		hub_on_updated_ope_template( self, OFO_OPE_TEMPLATE( object ));
	}
}

/*
 * a ledger label has changed : update the corresponding tab label
 */
static void
hub_on_updated_ledger_label( ofaOpeTemplateFrameBin *self, ofoLedger *ledger )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	GtkWidget *page_w;
	const gchar *mnemo;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	mnemo = ofo_ledger_get_mnemo( ledger );
	page_w = book_get_page_by_ledger( self, mnemo, FALSE );
	if( page_w ){
		g_return_if_fail( GTK_IS_WIDGET( page_w ));
		gtk_notebook_set_tab_label_text( GTK_NOTEBOOK( priv->notebook ), page_w, ofo_ledger_get_label( ledger ));
	}
}

/*
 * we do not have any way to know if the ledger attached to the operation
 *  template has changed or not - So just make sure the correct page is
 *  shown
 */
static void
hub_on_updated_ope_template( ofaOpeTemplateFrameBin *self, ofoOpeTemplate *template )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	GtkWidget *page_w;
	const gchar *ledger;
	gint page_n;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	ledger = ofo_ope_template_get_ledger( template );
	page_w = book_get_page_by_ledger( self, ledger, TRUE );
	g_return_if_fail( page_w && GTK_IS_WIDGET( page_w ));
	tview_select_row_by_mnemo( self, ofo_ope_template_get_mnemo( template ));

	page_n = gtk_notebook_page_num( GTK_NOTEBOOK( priv->notebook ), page_w );
	gtk_notebook_set_current_page( GTK_NOTEBOOK( priv->notebook ), page_n );
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaOpeTemplateFrameBin *self )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_hub_on_deleted_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn, ( void * ) hub,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), ( void * ) self );

	if( OFO_IS_LEDGER( object )){
		hub_on_deleted_ledger_object( self, OFO_LEDGER( object ));
	}
}

static void
hub_on_deleted_ledger_object( ofaOpeTemplateFrameBin *self, ofoLedger *ledger )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	GtkWidget *page_w;
	gint page_n;
	const gchar *mnemo;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	mnemo = ofo_ledger_get_mnemo( ledger );
	page_w = book_get_page_by_ledger( self, mnemo, FALSE );
	if( page_w ){
		g_return_if_fail( GTK_IS_WIDGET( page_w ));
		page_n = gtk_notebook_page_num( GTK_NOTEBOOK( priv->notebook ), page_w );
		gtk_notebook_remove_page( GTK_NOTEBOOK( priv->notebook ), page_n );
		book_get_page_by_ledger( self, UNKNOWN_LEDGER_MNEMO, TRUE );
	}
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
hub_on_reload_dataset( ofaHub *hub, GType type, ofaOpeTemplateFrameBin *self )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_hub_on_reload_dataset";

	g_debug( "%s: hub=%p, type=%lu, self=%p",
			thisfn, ( void * ) hub, type, ( void * ) self );
}

/**
 * ofa_ope_template_frame_bin_add_button:
 * @bin: this #ofaOpeTemplateFrameBin instance.
 *
 * Write the settings before the container be destroyed.
 */
void
ofa_ope_template_frame_bin_write_settings( ofaOpeTemplateFrameBin *bin )
{
	ofaOpeTemplateFrameBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_OPE_TEMPLATE_FRAME_BIN( bin ));

	priv = ofa_ope_template_frame_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	write_settings( bin );
}

static void
write_settings( ofaOpeTemplateFrameBin *self )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	GList *strlist;
	gint i, count;
	GtkWidget *page;
	sPageData *sdata;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	strlist = NULL;

	/* record in settings the pages position */
	count = gtk_notebook_get_n_pages( GTK_NOTEBOOK( priv->notebook ) );
	for( i=0 ; i<count ; ++i ){
		page = gtk_notebook_get_nth_page( GTK_NOTEBOOK( priv->notebook ), i );
		sdata = ( sPageData * ) g_object_get_data( G_OBJECT( page ), DATA_PAGE_LEDGER );
		if( g_utf8_collate( sdata->ledger, UNKNOWN_LEDGER_MNEMO )){
			strlist = g_list_append( strlist, ( gpointer ) sdata->ledger );
		}
	}

	ofa_settings_dossier_set_string_list( priv->meta, st_ledger_order, strlist );

	g_list_free( strlist );
}

static void
free_button( sButton *sbtn )
{
	g_free( sbtn );
}
