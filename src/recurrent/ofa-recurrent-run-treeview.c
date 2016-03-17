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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-preferences.h"

#include "core/ofa-main-window.h"

#include "ofa-recurrent-run-treeview.h"
#include "ofo-recurrent-model.h"
#include "ofo-recurrent-run.h"

/* private instance data
 */
struct _ofaRecurrentRunTreeviewPrivate {
	gboolean           dispose_has_run;

	/* runtime
	 */
	ofaHub            *hub;
	GList             *hub_handlers;

	/* UI
	 */
	GtkWidget         *tview;
	GtkListStore      *store;
	GtkTreeModel      *tfilter;
	GtkTreeModel      *tsort;
	GtkListStore      *status_store;

	/* filtering
	 */
	gboolean           cancelled_visible;
	gboolean           waiting_visible;
	gboolean           validated_visible;

	/* sorting the view
	 */
	gint               sort_column_id;
	gint               sort_sens;
	GtkTreeViewColumn *sort_column;
};

/* columns in the operation store
 */
enum {
	COL_MNEMO = 0,
	COL_LABEL,
	COL_DATE,
	COL_STATUS,
	COL_OBJECT,
	COL_N_COLUMNS
};

/* the id of the column is set against sortable columns */
#define DATA_COLUMN_ID                  "ofa-data-column-id"

/* it appears that Gtk+ displays a counter intuitive sort indicator:
 * when asking for ascending sort, Gtk+ displays a 'v' indicator
 * while we would prefer the '^' version -
 * we are defining the inverse indicator, and we are going to sort
 * in reverse order to have our own illusion
 */
#define OFA_SORT_ASCENDING              GTK_SORT_DESCENDING
#define OFA_SORT_DESCENDING             GTK_SORT_ASCENDING

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void     setup_bin( ofaRecurrentRunTreeview *self );
static void     setup_treeview( ofaRecurrentRunTreeview *self );
static gboolean tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaRecurrentRunTreeview *self );
static void     tview_on_header_clicked( GtkTreeViewColumn *column, ofaRecurrentRunTreeview *self );
static gint     tview_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaRecurrentRunTreeview *self );
static void     tview_on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaRecurrentRunTreeview *self );
static void     tview_on_selection_changed( GtkTreeSelection *selection, ofaRecurrentRunTreeview *self );
static GList   *tview_get_selected( ofaRecurrentRunTreeview *self );
static void     store_insert_row( ofaRecurrentRunTreeview *self, ofoRecurrentRun *ope );
static void     store_set_row( ofaRecurrentRunTreeview *self, GtkTreeIter *iter, ofoRecurrentRun *ope );
static void     store_update_mnemo_all( ofaRecurrentRunTreeview *self, const gchar *prev_mnemo, const gchar *new_mnemo );
static void     store_update_object( ofaRecurrentRunTreeview *self, ofoRecurrentRun *object );
static gboolean store_remove_object( ofaRecurrentRunTreeview *self, ofoRecurrentRun *object );
static void     do_insert_dataset( ofaRecurrentRunTreeview *self, GList *dataset );
static void     hub_on_new_object( ofaHub *hub, ofoBase *object, ofaRecurrentRunTreeview *self );
static void     hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaRecurrentRunTreeview *self );
static void     hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaRecurrentRunTreeview *self );
static void     hub_on_reload_dataset( ofaHub *hub, GType type, ofaRecurrentRunTreeview *self );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentRunTreeview, ofa_recurrent_run_treeview, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaRecurrentRunTreeview ))

static void
recurrent_run_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recurrent_run_treeview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECURRENT_RUN_TREEVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_run_treeview_parent_class )->finalize( instance );
}

static void
recurrent_run_treeview_dispose( GObject *instance )
{
	ofaRecurrentRunTreeviewPrivate *priv;
	GList *it;

	g_return_if_fail( instance && OFA_IS_RECURRENT_RUN_TREEVIEW( instance ));

	priv = ofa_recurrent_run_treeview_get_instance_private( OFA_RECURRENT_RUN_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		/* disconnect from hub signaling system */
		for( it=priv->hub_handlers ; it ; it=it->next ){
			g_signal_handler_disconnect( priv->hub, ( gulong ) it->data );
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_run_treeview_parent_class )->dispose( instance );
}

static void
ofa_recurrent_run_treeview_init( ofaRecurrentRunTreeview *self )
{
	static const gchar *thisfn = "ofa_recurrent_run_treeview_init";
	ofaRecurrentRunTreeviewPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECURRENT_RUN_TREEVIEW( self ));

	priv = ofa_recurrent_run_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->sort_column_id = -1;
	priv->sort_sens = -1;
}

static void
ofa_recurrent_run_treeview_class_init( ofaRecurrentRunTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_recurrent_run_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_run_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_run_treeview_finalize;

	/**
	 * ofaRecurrentRunTreeview::changed:
	 *
	 * This signal is sent on the #ofaRecurrentRunTreeview when the selection
	 * is changed.
	 *
	 * Arguments is the list of selected mnemos.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaRecurrentRunTreeview *view,
	 * 						GList           *sel_mnemos,
	 * 						gpointer         user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_RECURRENT_RUN_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );

	/**
	 * ofaRecurrentRunTreeview::activated:
	 *
	 * This signal is sent on the #ofaRecurrentRunTreeview when the selection is
	 * activated.
	 *
	 * Arguments is the list of selected mnemos.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaRecurrentRunTreeview *view,
	 * 						GList           *sel_mnemos,
	 * 						gpointer         user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-activated",
				OFA_TYPE_RECURRENT_RUN_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );
}

/**
 * ofa_recurrent_run_treeview_new:
 * @hub: the #ofaHub object hub of the application.
 *
 * Returns: a new empty #ofaRecurrentRunTreeview composite object.
 */
ofaRecurrentRunTreeview *
ofa_recurrent_run_treeview_new( ofaHub *hub )
{
	ofaRecurrentRunTreeview *self;
	ofaRecurrentRunTreeviewPrivate *priv;
	gulong handler;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	self = g_object_new( OFA_TYPE_RECURRENT_RUN_TREEVIEW, NULL );

	priv = ofa_recurrent_run_treeview_get_instance_private( self );

	priv->hub = hub;

	setup_bin( self );
	setup_treeview( self );

	/* connect to the hub signaling system */
	handler = g_signal_connect( hub, SIGNAL_HUB_NEW, G_CALLBACK( hub_on_new_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( hub, SIGNAL_HUB_UPDATED, G_CALLBACK( hub_on_updated_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( hub, SIGNAL_HUB_DELETED, G_CALLBACK( hub_on_deleted_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( hub, SIGNAL_HUB_RELOAD, G_CALLBACK( hub_on_reload_dataset ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	return( self );
}

/*
 * if not already done, create a GtkTreeView inside of a GtkScrolledWindow
 */
static void
setup_bin( ofaRecurrentRunTreeview *self )
{
	ofaRecurrentRunTreeviewPrivate *priv;
	GtkWidget *top_widget, *scrolled;

	priv = ofa_recurrent_run_treeview_get_instance_private( self );

	top_widget = gtk_frame_new( NULL );
	gtk_container_add( GTK_CONTAINER( self ), top_widget );
	gtk_frame_set_shadow_type( GTK_FRAME( top_widget ), GTK_SHADOW_IN );

	scrolled = gtk_scrolled_window_new( NULL, NULL );
	gtk_container_add( GTK_CONTAINER( top_widget ), scrolled );
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW( scrolled ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );

	priv->tview = gtk_tree_view_new();
	gtk_widget_set_hexpand( priv->tview, TRUE );
	gtk_widget_set_vexpand( priv->tview, TRUE );
	gtk_container_add( GTK_CONTAINER( scrolled ), priv->tview );
	gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( priv->tview ), TRUE );
}

/*
 * initialize the previously created treeview
 */
static void
setup_treeview( ofaRecurrentRunTreeview *self )
{
	static const gchar *thisfn = "ofa_recurrent_run_treeview_setup_treeview";
	ofaRecurrentRunTreeviewPrivate *priv;
	GtkListStore *store;
	GtkTreeModel *tfilter, *tsort;
	GtkTreeViewColumn *sort_column;
	gint column_id;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = ofa_recurrent_run_treeview_get_instance_private( self );

	store = gtk_list_store_new(
					COL_N_COLUMNS,
					G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,		/* mnemo, label, date */
					G_TYPE_STRING,										/* status */
					G_TYPE_OBJECT );									/* ofoRecurrentRun object */
	priv->store = store;

	tfilter = gtk_tree_model_filter_new( GTK_TREE_MODEL( store ), NULL );
	g_object_unref( store );
	priv->tfilter = tfilter;

	gtk_tree_model_filter_set_visible_func(
			GTK_TREE_MODEL_FILTER( tfilter ),
			( GtkTreeModelFilterVisibleFunc ) tview_is_visible_row,
			self,
			NULL );

	tsort = gtk_tree_model_sort_new_with_model( tfilter );
	g_object_unref( tfilter );
	priv->tsort = tsort;

	gtk_tree_view_set_model( GTK_TREE_VIEW( priv->tview ), tsort );
	g_object_unref( tsort );

	g_debug( "%s: store=%p, tfilter=%p, tsort=%p",
			thisfn, ( void * ) store, ( void * ) tfilter, ( void * ) tsort );

	/* default is to sort by descending operation date
	 */
	sort_column = NULL;
	if( priv->sort_column_id < 0 ){
		priv->sort_column_id = COL_DATE;
	}
	if( priv->sort_sens < 0 ){
		priv->sort_sens = OFA_SORT_DESCENDING;
	}

	column_id = COL_MNEMO;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Operation" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( priv->tview ), column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	column_id = COL_LABEL;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( priv->tview ), column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_column_set_expand( column, TRUE );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	column_id = COL_DATE;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Date" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( priv->tview ), column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	column_id = COL_STATUS;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Status" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( priv->tview ), column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->tview ));
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( select, "changed", G_CALLBACK( tview_on_selection_changed ), self );

	g_signal_connect( priv->tview, "row-activated", G_CALLBACK( tview_on_row_activated ), self );

	/* default is to sort by ascending operation date
	 */
	g_return_if_fail( sort_column && GTK_IS_TREE_VIEW_COLUMN( sort_column ));
	gtk_tree_view_column_set_sort_indicator( sort_column, TRUE );
	priv->sort_column = sort_column;
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tsort ), priv->sort_column_id, priv->sort_sens );
}

/*
 * a row is visible depending of the status filter
 */
static gboolean
tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaRecurrentRunTreeview *self )
{
	ofaRecurrentRunTreeviewPrivate *priv;
	gboolean visible;
	ofoRecurrentRun *object;
	const gchar *status;

	priv = ofa_recurrent_run_treeview_get_instance_private( self );

	visible = FALSE;

	gtk_tree_model_get( tmodel, iter, COL_OBJECT, &object, -1 );
	if( object ){
		g_return_val_if_fail( OFO_IS_RECURRENT_RUN( object ), FALSE );
		g_object_unref( object );

		status = ofo_recurrent_run_get_status( object );

		if( !my_collate( status, REC_STATUS_CANCELLED )){
			visible = priv->cancelled_visible;

		} else if( !my_collate( status, REC_STATUS_WAITING )){
			visible = priv->waiting_visible;

		} else if( !my_collate( status, REC_STATUS_VALIDATED )){
			visible = priv->validated_visible;
		}
	}

	return( visible );
}

/*
 * Gtk+ changes automatically the sort order
 * we reset yet the sort column id
 *
 * as a side effect of our inversion of indicators, clicking on a new
 * header makes the sort order descending as the default
 */
static void
tview_on_header_clicked( GtkTreeViewColumn *column, ofaRecurrentRunTreeview *self )
{
	ofaRecurrentRunTreeviewPrivate *priv;
	gint sort_column_id, new_column_id;
	GtkSortType sort_order;
	GtkTreeModel *tsort;

	priv = ofa_recurrent_run_treeview_get_instance_private( self );

	gtk_tree_view_column_set_sort_indicator( priv->sort_column, FALSE );
	gtk_tree_view_column_set_sort_indicator( column, TRUE );
	priv->sort_column = column;

	tsort = gtk_tree_view_get_model( GTK_TREE_VIEW( priv->tview ));
	gtk_tree_sortable_get_sort_column_id( GTK_TREE_SORTABLE( tsort ), &sort_column_id, &sort_order );

	new_column_id = gtk_tree_view_column_get_sort_column_id( column );
	gtk_tree_sortable_set_sort_column_id( GTK_TREE_SORTABLE( tsort ), new_column_id, sort_order );

	priv->sort_column_id = new_column_id;
	priv->sort_sens = sort_order;
}

/*
 * sorting the store
 */
static gint
tview_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaRecurrentRunTreeview *self )
{
	static const gchar *thisfn = "ofa_recurrent_run_treeview_tview_on_sort_model";
	ofaRecurrentRunTreeviewPrivate *priv;
	gint cmp;
	gchar *smnemoa, *slabela, *sdatea, *statusa;
	gchar *smnemob, *slabelb, *sdateb, *statusb;

	gtk_tree_model_get( tmodel, a,
			COL_MNEMO,  &smnemoa,
			COL_LABEL,  &slabela,
			COL_DATE,   &sdatea,
			COL_STATUS, &statusa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			COL_MNEMO,  &smnemob,
			COL_LABEL,  &slabelb,
			COL_DATE,   &sdateb,
			COL_STATUS, &statusb,
			-1 );

	cmp = 0;

	priv = ofa_recurrent_run_treeview_get_instance_private( self );

	switch( priv->sort_column_id ){
		case COL_MNEMO:
			cmp = my_collate( smnemoa, smnemob );
			break;
		case COL_LABEL:
			cmp = my_collate( slabela, slabelb );
			break;
		case COL_DATE:
			cmp = my_date_compare_by_str( sdatea, sdateb, ofa_prefs_date_display());
			break;
		case COL_STATUS:
			cmp = my_collate( statusa, statusb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, priv->sort_column_id );
			break;
	}

	g_free( statusa );
	g_free( sdatea );
	g_free( slabela );
	g_free( smnemoa );

	g_free( statusb );
	g_free( sdateb );
	g_free( slabelb );
	g_free( smnemob );

	/* return -1 if a > b, so that the order indicator points to the smallest:
	 * ^: means from smallest to greatest (ascending order)
	 * v: means from greatest to smallest (descending order)
	 */
	return( -cmp );
}

static void
tview_on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaRecurrentRunTreeview *self )
{
	GList *sel_objects;

	sel_objects = tview_get_selected( self );
	g_signal_emit_by_name( self, "ofa-activated", sel_objects );
	ofa_recurrent_run_treeview_free_selected( sel_objects );
}

/*
 * mode BROWSE
 */
static void
tview_on_selection_changed( GtkTreeSelection *selection, ofaRecurrentRunTreeview *self )
{
	GList *sel_objects;

	sel_objects = tview_get_selected( self );
	g_signal_emit_by_name( self, "ofa-changed", sel_objects );
	ofa_recurrent_run_treeview_free_selected( sel_objects );
}

/*
 * Returns a list of selected mnemos.
 * The returned list should be #ofa_recurrent_run_treeview_free_selected().
 */
static GList *
tview_get_selected( ofaRecurrentRunTreeview *self )
{
	ofaRecurrentRunTreeviewPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GList *sel_rows, *irow;
	GList *sel_objects;
	ofoRecurrentRun *run_obj;

	priv = ofa_recurrent_run_treeview_get_instance_private( self );

	sel_objects = NULL;
	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->tview ));
	sel_rows = gtk_tree_selection_get_selected_rows( select, &tmodel );

	for( irow=sel_rows ; irow ; irow=irow->next ){
		if( gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) irow->data )){
			gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &run_obj, -1 );
			sel_objects = g_list_append( sel_objects, run_obj );
		}
	}

	g_list_free_full( sel_rows, ( GDestroyNotify ) gtk_tree_path_free );

	return( sel_objects );
}

/**
 * ofa_recurrent_run_treeview_set_visible:
 * @bin: this #ofaRecurrentRunTreeview instance.
 * @status: the operation status.
 * @visible: whether this status should be visible.
 *
 * Set the visibility status.
 */
void
ofa_recurrent_run_treeview_set_visible( ofaRecurrentRunTreeview *bin, const gchar *status, gboolean visible )
{
	static const gchar *thisfn = "ofa_recurrent_run_treeview_set_visible";
	ofaRecurrentRunTreeviewPrivate *priv;

	g_return_if_fail( bin && OFA_IS_RECURRENT_RUN_TREEVIEW( bin ));

	priv = ofa_recurrent_run_treeview_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	if( !my_collate( status, REC_STATUS_CANCELLED )){
		priv->cancelled_visible = visible;

	} else if( !my_collate( status, REC_STATUS_WAITING )){
		priv->waiting_visible = visible;

	} else if( !my_collate( status, REC_STATUS_VALIDATED )){
		priv->validated_visible = visible;

	} else {
		g_warning( "%s: unknown status: %s", thisfn, status );
		g_return_if_reached();
	}

	gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( priv->tfilter ));
}

/**
 * ofa_recurrent_run_treeview_selection_mode:
 * @bin: this #ofaRecurrentRunTreeview instance.
 * @mode: the desired #GtkSelectionMode mode.
 *
 * Set the selection mode.
 *
 * The @bin defaults to have a single selection mode (GTK_SELECTION_BROWSE).
 */
void
ofa_recurrent_run_treeview_set_selection_mode( ofaRecurrentRunTreeview *bin, GtkSelectionMode mode )
{
	ofaRecurrentRunTreeviewPrivate *priv;
	GtkTreeSelection *select;

	g_return_if_fail( bin && OFA_IS_RECURRENT_RUN_TREEVIEW( bin ));

	priv = ofa_recurrent_run_treeview_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->tview ));
	gtk_tree_selection_set_mode( select, mode );
}

/**
 * ofa_recurrent_run_treeview_get_sort_settings:
 * @bin: this #ofaRecurrentRunTreeview widget.
 * @sort_column_id: [out]: the identifier of the sorted column.
 * @sort_sens: [out]: the sens of the sort.
 *
 * Returns: the current sort settings.
 */
void
ofa_recurrent_run_treeview_get_sort_settings( ofaRecurrentRunTreeview *bin, gint *sort_column_id, gint *sort_sens )
{
	ofaRecurrentRunTreeviewPrivate *priv;

	g_return_if_fail( bin && OFA_IS_RECURRENT_RUN_TREEVIEW( bin ));

	priv = ofa_recurrent_run_treeview_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	if( sort_column_id ){
		*sort_column_id = priv->sort_column_id;
	}
	if( sort_sens ){
		*sort_sens = priv->sort_sens;
	}
}

/**
 * ofa_recurrent_run_treeview_set_sort_settings:
 * @bin: this #ofaRecurrentRunTreeview widget.
 * @sort_column_id: the identifier of the sorted column.
 * @sort_sens: the sens of the sort.
 *
 * Set the requested sort settings.
 */
void
ofa_recurrent_run_treeview_set_sort_settings( ofaRecurrentRunTreeview *bin, gint sort_column_id, gint sort_sens )
{
	ofaRecurrentRunTreeviewPrivate *priv;

	g_return_if_fail( bin && OFA_IS_RECURRENT_RUN_TREEVIEW( bin ));

	priv = ofa_recurrent_run_treeview_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	priv->sort_column_id = sort_column_id;
	priv->sort_sens = sort_sens;

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( priv->tsort ), priv->sort_column_id, priv->sort_sens );
}

static void
store_insert_row( ofaRecurrentRunTreeview *self, ofoRecurrentRun *ope )
{
	ofaRecurrentRunTreeviewPrivate *priv;
	GtkTreeIter iter;

	priv = ofa_recurrent_run_treeview_get_instance_private( self );

	gtk_list_store_insert( priv->store, &iter, -1 );
	store_set_row( self, &iter, ope );
}

/*
 * set data through a store iter
 */
static void
store_set_row( ofaRecurrentRunTreeview *self, GtkTreeIter *iter, ofoRecurrentRun *ope )
{
	ofaRecurrentRunTreeviewPrivate *priv;
	ofoRecurrentModel *model;
	const gchar *mnemo, *stt;
	gchar *sdate, *status;

	priv = ofa_recurrent_run_treeview_get_instance_private( self );

	mnemo = ofo_recurrent_run_get_mnemo( ope );
	model = ofo_recurrent_model_get_by_mnemo( priv->hub, mnemo );
	if( model ){
		sdate = my_date_to_str( ofo_recurrent_run_get_date( ope ), ofa_prefs_date_display());
		stt = ofo_recurrent_run_get_status( ope );
		status = ofo_recurrent_run_get_status_label( stt );

		gtk_list_store_set( priv->store, iter,
				COL_MNEMO,  mnemo,
				COL_LABEL,  ofo_recurrent_model_get_label( model ),
				COL_DATE,   sdate ? sdate : "",
				COL_STATUS, status,
				COL_OBJECT, ope,
				-1 );

		g_free( status );
		g_free( sdate );
	}
}

/*
 * Change the ope mnemonic of all concerned rows
 */
static void
store_update_mnemo_all( ofaRecurrentRunTreeview *self, const gchar *prev_mnemo, const gchar *new_mnemo )
{
	ofaRecurrentRunTreeviewPrivate *priv;
	GtkTreeIter iter;
	gchar *row_mnemo;
	gint cmp;

	priv = ofa_recurrent_run_treeview_get_instance_private( self );

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( priv->store ), &iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), &iter, COL_MNEMO, &row_mnemo, -1 );
			cmp = my_collate( row_mnemo, prev_mnemo );
			g_free( row_mnemo );
			if( cmp == 0 ){
				gtk_list_store_set( priv->store, &iter, COL_MNEMO, new_mnemo, -1 );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( priv->store ), &iter )){
				break;
			}
		}
	}
}

static void
store_update_object( ofaRecurrentRunTreeview *self, ofoRecurrentRun *object )
{
	ofaRecurrentRunTreeviewPrivate *priv;
	GtkTreeIter iter;
	ofoRecurrentRun *row_obj;
	gint cmp;

	priv = ofa_recurrent_run_treeview_get_instance_private( self );

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( priv->store ), &iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), &iter, COL_OBJECT, &row_obj, -1 );
			cmp = ofo_recurrent_run_compare( object, row_obj );
			g_object_unref( row_obj );
			if( cmp == 0 ){
				store_set_row( self, &iter, object );
				return;
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( priv->store ), &iter )){
				break;
			}
		}
	}
}

static gboolean
store_remove_object( ofaRecurrentRunTreeview *self, ofoRecurrentRun *object )
{
	ofaRecurrentRunTreeviewPrivate *priv;
	GtkTreeIter iter;
	ofoRecurrentRun *row_obj;
	gint cmp;

	priv = ofa_recurrent_run_treeview_get_instance_private( self );

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( priv->store ), &iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), &iter, COL_OBJECT, &row_obj, -1 );
			cmp = ofo_recurrent_run_compare( object, row_obj );
			g_object_unref( row_obj );
			if( cmp == 0 ){
				gtk_list_store_remove( priv->store, &iter );
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( priv->store ), &iter )){
				break;
			}
		}
	}

	return( FALSE );
}

/**
 * ofa_recurrent_run_treeview_set_from_list:
 * @bin: this #ofaRecurrentRunTreeview instance.
 * @dataset: a list of objects to be displayed.
 *
 * Display the provided list of objects.
 */
void
ofa_recurrent_run_treeview_set_from_list( ofaRecurrentRunTreeview *bin, GList *dataset )
{
	ofaRecurrentRunTreeviewPrivate *priv;

	g_return_if_fail( bin && OFA_IS_RECURRENT_RUN_TREEVIEW( bin ));

	priv = ofa_recurrent_run_treeview_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	do_insert_dataset( bin, dataset );
}

/**
 * ofa_recurrent_run_treeview_set_from_db:
 * @bin: this #ofaRecurrentRunTreeview instance.
 *
 * Load the current dataset from database, and displays it.
 */
void
ofa_recurrent_run_treeview_set_from_db( ofaRecurrentRunTreeview *bin )
{
	ofaRecurrentRunTreeviewPrivate *priv;
	GList *dataset;

	g_return_if_fail( bin && OFA_IS_RECURRENT_RUN_TREEVIEW( bin ));

	priv = ofa_recurrent_run_treeview_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	dataset = ofo_recurrent_run_get_dataset( priv->hub );
	do_insert_dataset( bin, dataset );
}

static void
do_insert_dataset( ofaRecurrentRunTreeview *self, GList *dataset )
{
	ofaRecurrentRunTreeviewPrivate *priv;
	GList *it;

	priv = ofa_recurrent_run_treeview_get_instance_private( self );

	gtk_list_store_clear( priv->store );

	for( it=dataset ; it ; it=it->next ){
		g_return_if_fail( OFO_IS_RECURRENT_RUN( it->data ));
		store_insert_row( self, OFO_RECURRENT_RUN( it->data ));
	}
}

/**
 * ofa_recurrent_run_treeview_clear:
 * @bin: this #ofaRecurrentRunTreeview instance.
 *
 * Clears the underlying list store.
 */
void
ofa_recurrent_run_treeview_clear( ofaRecurrentRunTreeview *bin )
{
	ofaRecurrentRunTreeviewPrivate *priv;

	g_return_if_fail( bin && OFA_IS_RECURRENT_RUN_TREEVIEW( bin ));

	priv = ofa_recurrent_run_treeview_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	gtk_list_store_clear( priv->store );
}

/**
 * ofa_recurrent_run_treeview_get_treeview:
 * @bin: this #ofaRecurrentRunTreeview instance.
 *
 * Returns: the underlying #GtkTreeView widget.
 */
GtkWidget *
ofa_recurrent_run_treeview_get_treeview( const ofaRecurrentRunTreeview *bin )
{
	ofaRecurrentRunTreeviewPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_RECURRENT_RUN_TREEVIEW( bin ), NULL );

	priv = ofa_recurrent_run_treeview_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->tview );
}

/**
 * ofa_recurrent_run_treeview_get_selected:
 *
 * Returns: the list of #ofoRecurrentRun selected objects.
 *
 * The returned list should be ofa_recurrent_run_treeview_free_selected() by
 * the caller.
 */
GList *
ofa_recurrent_run_treeview_get_selected( ofaRecurrentRunTreeview *view )
{
	ofaRecurrentRunTreeviewPrivate *priv;

	g_return_val_if_fail( view && OFA_IS_RECURRENT_RUN_TREEVIEW( view ), NULL );

	priv = ofa_recurrent_run_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( tview_get_selected( view ));
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
hub_on_new_object( ofaHub *hub, ofoBase *object, ofaRecurrentRunTreeview *self )
{
	static const gchar *thisfn = "ofa_recurrent_run_treeview_hub_on_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_RECURRENT_RUN( object )){
		store_insert_row( self, OFO_RECURRENT_RUN( object ));
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaRecurrentRunTreeview *self )
{
	static const gchar *thisfn = "ofa_recurrent_run_treeview_hub_on_updated_object";
	const gchar *code, *new_code;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_RECURRENT_MODEL( object )){
		new_code = ofo_recurrent_model_get_mnemo( OFO_RECURRENT_MODEL( object ));
		code = prev_id ? prev_id : new_code;
		if( my_collate( code, new_code )){
			store_update_mnemo_all( self, code, new_code );
		}
	} else if( OFO_IS_RECURRENT_RUN( object )){
			store_update_object( self, OFO_RECURRENT_RUN( object ));
	}
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaRecurrentRunTreeview *self )
{
	static const gchar *thisfn = "ofa_recurrent_run_treeview_hub_on_deleted_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_RECURRENT_RUN( object )){
		store_remove_object( self, OFO_RECURRENT_RUN( object ));
	}
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
hub_on_reload_dataset( ofaHub *hub, GType type, ofaRecurrentRunTreeview *self )
{
	static const gchar *thisfn = "ofa_recurrent_run_treeview_hub_on_reload_dataset";

	g_debug( "%s: hub=%p, type=%lu, self=%p",
			thisfn, ( void * ) hub, type, ( void * ) self );

	if( type == OFO_TYPE_RECURRENT_RUN ){
		ofa_recurrent_run_treeview_set_from_db( self );
	}
}
