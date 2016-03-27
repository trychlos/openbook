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

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofo-ledger.h"

#include "ui/ofa-ledger-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean          dispose_has_run;

	/* UI
	 */
	GtkTreeView      *tview;
	ofaLedgerColumns  columns;
	ofaLedgerStore   *store;
}
	ofaLedgerTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	INSERT,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void     setup_top_widget( ofaLedgerTreeview *self );
static void     create_treeview_columns( ofaLedgerTreeview *view );
static gboolean on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaLedgerTreeview *self );
static void     on_tview_key_insert( ofaLedgerTreeview *page );
static void     on_tview_key_delete( ofaLedgerTreeview *page );
static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaLedgerTreeview *self );
static gint     cmp_by_mnemo( const gchar *a, const gchar *b );
static void     on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaLedgerTreeview *self );
static void     on_row_selected( GtkTreeSelection *selection, ofaLedgerTreeview *self );
static GList   *get_selected( ofaLedgerTreeview *self );
static void     select_row_by_mnemo( ofaLedgerTreeview *tview, const gchar *ledger );
static gboolean find_row_by_mnemo( ofaLedgerTreeview *tview, const gchar *ledger, GtkTreeIter *iter );


G_DEFINE_TYPE_EXTENDED( ofaLedgerTreeview, ofa_ledger_treeview, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaLedgerTreeview ))

static void
ledger_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledger_treeview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGER_TREEVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_treeview_parent_class )->finalize( instance );
}

static void
ledger_treeview_dispose( GObject *instance )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_LEDGER_TREEVIEW( instance ));

	priv = ofa_ledger_treeview_get_instance_private( OFA_LEDGER_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_treeview_parent_class )->dispose( instance );
}

static void
ofa_ledger_treeview_init( ofaLedgerTreeview *self )
{
	static const gchar *thisfn = "ofa_ledger_treeview_init";
	ofaLedgerTreeviewPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_LEDGER_TREEVIEW( self ));

	priv = ofa_ledger_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_ledger_treeview_class_init( ofaLedgerTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledger_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledger_treeview_finalize;

	/**
	 * ofaLedgerTreeview::changed:
	 *
	 * This signal is sent on the #ofaLedgerTreeview when the selection
	 * is changed.
	 *
	 * Arguments is the list of selected mnemos.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaLedgerTreeview *view,
	 * 						GList           *sel_mnemos,
	 * 						gpointer         user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_LEDGER_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );

	/**
	 * ofaLedgerTreeview::activated:
	 *
	 * This signal is sent on the #ofaLedgerTreeview when the selection is
	 * activated.
	 *
	 * Arguments is the list of selected mnemos.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaLedgerTreeview *view,
	 * 						GList           *sel_mnemos,
	 * 						gpointer         user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-activated",
				OFA_TYPE_LEDGER_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );

	/**
	 * ofaLedgerTreeview::insert:
	 *
	 * This signal is sent on the #ofaLedgerTreeview when the insertion
	 * key is hit.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaLedgerTreeview *view,
	 * 						gpointer         user_data );
	 */
	st_signals[ INSERT ] = g_signal_new_class_handler(
				"ofa-insert",
				OFA_TYPE_LEDGER_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0,
				G_TYPE_NONE );

	/**
	 * ofaLedgerTreeview::delete:
	 *
	 * This signal is sent on the #ofaLedgerTreeview when the deletion
	 * key is hit.
	 *
	 * Arguments is the list of selected mnemos.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaLedgerTreeview *view,
	 * 						GList           *sel_mnemos,
	 * 						gpointer         user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-delete",
				OFA_TYPE_LEDGER_TREEVIEW,
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
 * ofa_ledger_treeview_new:
 */
ofaLedgerTreeview *
ofa_ledger_treeview_new( void )
{
	ofaLedgerTreeview *view;

	view = g_object_new( OFA_TYPE_LEDGER_TREEVIEW, NULL );

	setup_top_widget( view );

	return( view );
}

/*
 * if not already done, create a GtkTreeView inside of a GtkScrolledWindow
 */
static void
setup_top_widget( ofaLedgerTreeview *self )
{
	ofaLedgerTreeviewPrivate *priv;
	GtkTreeSelection *select;
	GtkWidget *top_widget, *scrolled;

	priv = ofa_ledger_treeview_get_instance_private( self );

	top_widget = gtk_frame_new( NULL );
	gtk_container_add( GTK_CONTAINER( self ), top_widget );
	gtk_frame_set_shadow_type( GTK_FRAME( top_widget ), GTK_SHADOW_IN );

	scrolled = gtk_scrolled_window_new( NULL, NULL );
	gtk_container_add( GTK_CONTAINER( top_widget ), scrolled );
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW( scrolled ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );

	priv->tview = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_hexpand( GTK_WIDGET( priv->tview ), TRUE );
	gtk_widget_set_vexpand( GTK_WIDGET( priv->tview ), TRUE );
	gtk_container_add( GTK_CONTAINER( scrolled ), GTK_WIDGET( priv->tview ));
	gtk_tree_view_set_headers_visible( priv->tview, TRUE );

	g_signal_connect( priv->tview, "row-activated", G_CALLBACK( on_row_activated ), self );
	g_signal_connect( priv->tview, "key-press-event", G_CALLBACK( on_tview_key_pressed ), self );

	select = gtk_tree_view_get_selection( priv->tview );
	g_signal_connect( select, "changed", G_CALLBACK( on_row_selected ), self );
}

/**
 * ofa_ledger_treeview_set_columns:
 */
void
ofa_ledger_treeview_set_columns( ofaLedgerTreeview *view, ofaLedgerColumns columns )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ));

	priv = ofa_ledger_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	priv->columns = columns;
	create_treeview_columns( view );
}

static void
create_treeview_columns( ofaLedgerTreeview *view )
{
	ofaLedgerTreeviewPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	priv = ofa_ledger_treeview_get_instance_private( view );

	if( priv->columns & LEDGER_DISP_MNEMO ){
		cell = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
						_( "Mnemo" ), cell, "text", LEDGER_COL_MNEMO, NULL );
		gtk_tree_view_append_column( priv->tview, column );
	}

	if( priv->columns & LEDGER_DISP_LABEL ){
		cell = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
						_( "Label" ), cell, "text", LEDGER_COL_LABEL, NULL );
		gtk_tree_view_column_set_expand( column, TRUE );
		gtk_tree_view_append_column( priv->tview, column );
	}

	if( priv->columns & LEDGER_DISP_LAST_ENTRY ){
		cell = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
						_( "Last entry" ), cell, "text", LEDGER_COL_LAST_ENTRY, NULL );
		gtk_tree_view_append_column( priv->tview, column );
	}

	if( priv->columns & LEDGER_DISP_LAST_CLOSE ){
		cell = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
						_( "Last closing" ), cell, "text", LEDGER_COL_LAST_CLOSE, NULL );
		gtk_tree_view_append_column( priv->tview, column );
	}

	gtk_widget_show_all( GTK_WIDGET( view ));
}

/**
 * ofa_ledger_treeview_set_hub:
 * @view: this #ofaLedgerTreeview instance.
 * @hub: the #ofaHub object of the application.
 *
 * This is required in order to get the dossier which will permit to
 * create the underlying tree store.
 */
void
ofa_ledger_treeview_set_hub( ofaLedgerTreeview *view, ofaHub *hub )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_ledger_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	/* the treeviewbox must have been created first */
	g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));

	priv->store = ofa_ledger_store_new( hub );

	gtk_tree_view_set_model( priv->tview, GTK_TREE_MODEL( priv->store ));

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( priv->store ), ( GtkTreeIterCompareFunc ) on_sort_model, view, NULL );

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( priv->store ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaLedgerTreeview *self )
{
	gboolean stop;

	stop = FALSE;

	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Insert ){
			on_tview_key_insert( self );
		} else if( event->keyval == GDK_KEY_Delete ){
			on_tview_key_delete( self );
		}
	}

	return( stop );
}

static void
on_tview_key_insert( ofaLedgerTreeview *page )
{
	g_signal_emit_by_name( page, "ofa-insert" );
}

static void
on_tview_key_delete( ofaLedgerTreeview *page )
{
	GList *sel_objects;

	sel_objects = get_selected( page );
	g_signal_emit_by_name( page, "ofa-delete", sel_objects );
	ofa_ledger_treeview_free_selected( sel_objects );
}

static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaLedgerTreeview *self )
{
	gchar *amnemo, *bmnemo;
	gint cmp;

	gtk_tree_model_get( tmodel, a, LEDGER_COL_MNEMO, &amnemo, -1 );
	gtk_tree_model_get( tmodel, b, LEDGER_COL_MNEMO, &bmnemo, -1 );

	cmp = cmp_by_mnemo( amnemo, bmnemo );

	g_free( amnemo );
	g_free( bmnemo );

	return( cmp );
}

static gint
cmp_by_mnemo( const gchar *a, const gchar *b )
{
	gchar *afold, *bfold;
	gint cmp;

	afold = g_utf8_casefold( a, -1 );
	bfold = g_utf8_casefold( b, -1 );

	cmp = g_utf8_collate( afold, bfold );

	g_free( afold );
	g_free( bfold );

	return( cmp );
}

static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaLedgerTreeview *self )
{
	GList *sel_objects;

	sel_objects = get_selected( self );
	g_signal_emit_by_name( self, "ofa-activated", sel_objects );
	ofa_ledger_treeview_free_selected( sel_objects );
}

static void
on_row_selected( GtkTreeSelection *selection, ofaLedgerTreeview *self )
{
	GList *sel_objects;

	sel_objects = get_selected( self );
	g_signal_emit_by_name( self, "ofa-changed", sel_objects );
	ofa_ledger_treeview_free_selected( sel_objects );
}

static GList *
get_selected( ofaLedgerTreeview *self )
{
	ofaLedgerTreeviewPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GList *sel_rows, *irow;
	GList *sel_mnemos;
	gchar *ledger;

	priv = ofa_ledger_treeview_get_instance_private( self );

	sel_mnemos = NULL;
	select = gtk_tree_view_get_selection( priv->tview );
	sel_rows = gtk_tree_selection_get_selected_rows( select, &tmodel );

	for( irow=sel_rows ; irow ; irow=irow->next ){
		if( gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) irow->data )){
			gtk_tree_model_get( tmodel, &iter, LEDGER_COL_MNEMO, &ledger, -1 );
			sel_mnemos = g_list_append( sel_mnemos, ledger );
		}
	}

	g_list_free_full( sel_rows, ( GDestroyNotify ) gtk_tree_path_free );

	return( sel_mnemos );
}

/**
 * ofa_ledger_treeview_get_selection:
 * @view:
 *
 * Returns: the #GtkTreeSelection object.
 */
GtkTreeSelection *
ofa_ledger_treeview_get_selection( ofaLedgerTreeview *view )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_val_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ), NULL );

	priv = ofa_ledger_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( gtk_tree_view_get_selection( priv->tview ));
}

/**
 * ofa_ledger_treeview_get_selected:
 *
 * Returns: the list of #ofoLedger ledgers selected mnemonics.
 *
 * The returned list should be ofa_ledger_treeview_free_selected() by
 * the caller.
 */
GList *
ofa_ledger_treeview_get_selected( ofaLedgerTreeview *view )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_val_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ), NULL );

	priv = ofa_ledger_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( get_selected( view ));
}

/**
 * ofa_ledger_treeview_set_selected:
 */
void
ofa_ledger_treeview_set_selected( ofaLedgerTreeview *tview, const gchar *ledger )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_if_fail( tview && OFA_IS_LEDGER_TREEVIEW( tview ));

	priv = ofa_ledger_treeview_get_instance_private( tview );

	g_return_if_fail( !priv->dispose_has_run );

	select_row_by_mnemo( tview, ledger );
}

static void
select_row_by_mnemo( ofaLedgerTreeview *tview, const gchar *ledger )
{
	ofaLedgerTreeviewPrivate *priv;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	priv = ofa_ledger_treeview_get_instance_private( tview );

	if( find_row_by_mnemo( tview, ledger, &iter )){
		select = gtk_tree_view_get_selection( priv->tview );
		gtk_tree_selection_select_iter( select, &iter );
	}
}

static gboolean
find_row_by_mnemo( ofaLedgerTreeview *tview, const gchar *ledger, GtkTreeIter *iter )
{
	ofaLedgerTreeviewPrivate *priv;
	gchar *mnemo;
	gint cmp;

	priv = ofa_ledger_treeview_get_instance_private( tview );

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( priv->store ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), iter, LEDGER_COL_MNEMO, &mnemo, -1 );
			cmp = g_utf8_collate( mnemo, ledger );
			g_free( mnemo );
			if( cmp == 0 ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( priv->store ), iter )){
				break;
			}
		}
	}

	return( FALSE );
}

/**
 * ofa_ledger_treeview_set_selection_mode:
 */
void
ofa_ledger_treeview_set_selection_mode( ofaLedgerTreeview *view, GtkSelectionMode mode )
{
	ofaLedgerTreeviewPrivate *priv;
	GtkTreeSelection *select;

	g_return_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ));

	priv = ofa_ledger_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));
	select = gtk_tree_view_get_selection( priv->tview );
	gtk_tree_selection_set_mode( select, mode );
}

/**
 * ofa_ledger_treeview_set_hexpand:
 * @view:
 * @hexpand:
 */
void
ofa_ledger_treeview_set_hexpand( ofaLedgerTreeview *view, gboolean hexpand )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ));

	priv = ofa_ledger_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));
	gtk_widget_set_hexpand( GTK_WIDGET( priv->tview ), hexpand );
}

/**
 * ofa_ledger_treeview_get_treeview:
 * @view: this #ofaLedgerTreeview instance.
 *
 * Returns: the underlying #GtkTreeView widget.
 */
GtkWidget *
ofa_ledger_treeview_get_treeview( const ofaLedgerTreeview *view )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_val_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ), NULL );

	priv = ofa_ledger_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );
	g_return_val_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ), NULL );

	return( GTK_WIDGET( priv->tview ));
}
