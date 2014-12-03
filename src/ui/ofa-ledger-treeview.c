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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofo-ledger.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-ledger-treeview.h"

/* private instance data
 */
struct _ofaLedgerTreeviewPrivate {
	gboolean         dispose_has_run;

	/* runtime datas
	 */
	GtkWidget       *scrolled;
	GtkTreeView     *tview;

	ofoDossier      *dossier;
	GList           *handlers;
};

/* column ordering in the listview
 */
enum {
	COL_MNEMO = 0,
	COL_LABEL,
	COL_LAST_ENTRY,
	COL_LAST_CLOSING,
	COL_OBJECT,
	N_COLUMNS
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

G_DEFINE_TYPE( ofaLedgerTreeview, ofa_ledger_treeview, G_TYPE_OBJECT )

static void        on_parent_finalized( ofaLedgerTreeview *self, gpointer finalized_parent );
static void        setup_treeview( ofaLedgerTreeview *self, ofaLedgerColumns columns, GtkSelectionMode mode );
static gboolean    on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaLedgerTreeview *self );
static void        on_tview_key_insert( ofaLedgerTreeview *page );
static void        on_tview_key_delete( ofaLedgerTreeview *page );
static void        dossier_signal_connect( ofaLedgerTreeview *self );
static void        insert_dataset( ofaLedgerTreeview *self, const gchar *initial_selection );
static void        insert_new_row( ofaLedgerTreeview *self, ofoLedger *ledger, gboolean with_selection );
static void        set_row_by_iter( ofaLedgerTreeview *self, ofoLedger *ledger, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gint        on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaLedgerTreeview *self );
static gint        cmp_by_mnemo( const gchar *a, const gchar *b );
static void        select_row_by_mnemo( ofaLedgerTreeview *self, const gchar *mnemo );
static void        select_row_by_iter( ofaLedgerTreeview *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean    find_row_by_mnemo( ofaLedgerTreeview *self, const gchar *mnemo, GtkTreeModel **tmodel, GtkTreeIter *iter );
static void        on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaLedgerTreeview *self );
static void        on_row_selected( GtkTreeSelection *selection, ofaLedgerTreeview *self );
static GList      *get_selected( ofaLedgerTreeview *self );
static void        on_list_cleanup_handler( ofaLedgerTreeview *self, GList *list );
static void        on_new_object( ofoDossier *dossier, ofoBase *object, ofaLedgerTreeview *self );
static void        on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaLedgerTreeview *self );
static void        on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaLedgerTreeview *self );
static void        on_reloaded_dataset( ofoDossier *dossier, GType type, ofaLedgerTreeview *self );

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
	GList *iha;
	gulong handler_id;

	g_return_if_fail( instance && OFA_IS_LEDGER_TREEVIEW( instance ));

	priv = ( OFA_LEDGER_TREEVIEW( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		/* note when deconnecting the handlers that the dossier may
		 * have been already finalized (e.g. when the application
		 * terminates) */
		if( OFO_IS_DOSSIER( priv->dossier )){
			for( iha=priv->handlers ; iha ; iha=iha->next ){
				handler_id = ( gulong ) iha->data;
				g_signal_handler_disconnect( priv->dossier, handler_id );
			}
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_treeview_parent_class )->dispose( instance );
}

static void
ofa_ledger_treeview_init( ofaLedgerTreeview *self )
{
	static const gchar *thisfn = "ofa_ledger_treeview_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_LEDGER_TREEVIEW( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_LEDGER_TREEVIEW, ofaLedgerTreeviewPrivate );

	self->priv->dispose_has_run = FALSE;
	self->priv->handlers = NULL;
}

static void
ofa_ledger_treeview_class_init( ofaLedgerTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledger_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledger_treeview_finalize;

	g_type_class_add_private( klass, sizeof( ofaLedgerTreeviewPrivate ));

	/**
	 * ofaLedgerTreeview::changed:
	 *
	 * This signal is sent when the selection is changed.
	 *
	 * Arguments is the current selection as a GList of ledger mnemos.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaLedgerTreeview *view,
	 * 						GList           *list,
	 * 						gpointer      user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_LEDGER_TREEVIEW,
				G_SIGNAL_RUN_CLEANUP,
				G_CALLBACK( on_list_cleanup_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );

	/**
	 * ofaLedgerTreeview::activated:
	 *
	 * This signal is sent when the selection is activated.
	 *
	 * Arguments is the current selection as a GList of ledger mnemos.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaLedgerTreeview *view,
	 * 						GList           *list,
	 * 						gpointer      user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"activated",
				OFA_TYPE_LEDGER_TREEVIEW,
				G_SIGNAL_RUN_CLEANUP,
				G_CALLBACK( on_list_cleanup_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );
}

static void
on_parent_finalized( ofaLedgerTreeview *self, gpointer finalized_parent )
{
	static const gchar *thisfn = "ofa_ledger_treeview_on_parent_finalized";

	g_debug( "%s: self=%p, finalized_parent=%p", thisfn, ( void * ) self, ( void * ) finalized_parent );

	g_return_if_fail( self && OFA_IS_LEDGER_TREEVIEW( self ));

	g_object_unref( self );
}

/**
 * ofa_ledger_treeview_new:
 */
ofaLedgerTreeview *
ofa_ledger_treeview_new( void )
{
	ofaLedgerTreeview *view;

	view = g_object_new( OFA_TYPE_LEDGER_TREEVIEW, NULL );

	return( view );
}

/**
 * ofa_ledger_treeview_attach_to:
 */
void
ofa_ledger_treeview_attach_to( ofaLedgerTreeview *view,
									GtkContainer *parent, ofaLedgerColumns columns, GtkSelectionMode mode )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ));
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv = view->priv;

	if( !priv->dispose_has_run ){

		/* setup a weak reference on the container to auto-unref */
		g_object_weak_ref( G_OBJECT( parent ), ( GWeakNotify ) on_parent_finalized, view );

		priv->scrolled = gtk_scrolled_window_new( NULL, NULL );
		gtk_container_set_border_width(
				GTK_CONTAINER( priv->scrolled ), 4 );
		gtk_scrolled_window_set_policy(
				GTK_SCROLLED_WINDOW( priv->scrolled ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
		gtk_container_add( parent, priv->scrolled );

		/* setup the tree (actually a list) view */
		setup_treeview( view, columns, mode );
	}
}

static void
setup_treeview( ofaLedgerTreeview *self, ofaLedgerColumns columns, GtkSelectionMode mode )
{
	ofaLedgerTreeviewPrivate *priv;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = self->priv;

	tview = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_hexpand( GTK_WIDGET( tview ), TRUE );
	gtk_widget_set_vexpand( GTK_WIDGET( tview ), TRUE );
	gtk_tree_view_set_headers_visible( tview, TRUE );
	gtk_container_add( GTK_CONTAINER( priv->scrolled ), GTK_WIDGET( tview ));
	g_signal_connect(
			G_OBJECT( tview ), "row-activated", G_CALLBACK( on_row_activated ), self );
	g_signal_connect(
			G_OBJECT( tview ), "key-press-event", G_CALLBACK( on_tview_key_pressed ), self );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( tview, tmodel );
	g_object_unref( tmodel );

	if( columns & LEDGER_MNEMO ){
		text_cell = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
				_( "Mnemo" ),
				text_cell, "text", COL_MNEMO,
				NULL );
		gtk_tree_view_append_column( tview, column );
	}

	if( columns & LEDGER_LABEL ){
		text_cell = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
				_( "Label" ),
				text_cell, "text", COL_LABEL,
				NULL );
		gtk_tree_view_column_set_expand( column, TRUE );
		gtk_tree_view_append_column( tview, column );
	}

	if( columns & LEDGER_ENTRY ){
		text_cell = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
				_( "Last entry" ),
				text_cell, "text", COL_LAST_ENTRY,
				NULL );
		gtk_tree_view_append_column( tview, column );
	}

	if( columns & LEDGER_CLOSING ){
		text_cell = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
				_( "Last closing" ),
				text_cell, "text", COL_LAST_CLOSING,
				NULL );
		gtk_tree_view_append_column( tview, column );
	}

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, mode );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_row_selected ), self );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ), ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	priv->tview = tview;
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

}

static void
on_tview_key_delete( ofaLedgerTreeview *page )
{

}

/**
 * ofa_ledger_treeview_init_view:
 */
void
ofa_ledger_treeview_init_view( ofaLedgerTreeview *view, ofoDossier *dossier, const gchar *initial_selection )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	priv = view->priv;

	if( !priv->dispose_has_run ){

		priv->dossier = dossier;

		/* connect to the dossier signaling system */
		dossier_signal_connect( view );

		/* insert the dataset */
		insert_dataset( view, initial_selection );
	}
}

static void
dossier_signal_connect( ofaLedgerTreeview *self )
{
	ofaLedgerTreeviewPrivate *priv;
	gulong handler;

	priv = self->priv;

	handler = g_signal_connect(
						G_OBJECT( priv->dossier ),
						SIGNAL_DOSSIER_NEW_OBJECT,
						G_CALLBACK( on_new_object ),
						self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( priv->dossier ),
						SIGNAL_DOSSIER_UPDATED_OBJECT,
						G_CALLBACK( on_updated_object ),
						self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( priv->dossier ),
						SIGNAL_DOSSIER_DELETED_OBJECT,
						G_CALLBACK( on_deleted_object ),
						self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( priv->dossier ),
						SIGNAL_DOSSIER_RELOAD_DATASET,
						G_CALLBACK( on_reloaded_dataset ),
						self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );
}

static void
insert_dataset( ofaLedgerTreeview *self, const gchar *initial_selection )
{
	GList *dataset, *it;
	ofoLedger *ledger;

	dataset = ofo_ledger_get_dataset( self->priv->dossier );

	for( it=dataset ; it ; it=it->next ){

		ledger = OFO_LEDGER( it->data );
		insert_new_row( self, ledger, FALSE );
	}

	select_row_by_mnemo( self, initial_selection );
}

static void
insert_new_row( ofaLedgerTreeview *self, ofoLedger *ledger, gboolean with_selection )
{
	ofaLedgerTreeviewPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	priv = self->priv;
	tmodel = gtk_tree_view_get_model( priv->tview );

	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( tmodel ),
			&iter,
			-1,
			COL_MNEMO,        ofo_ledger_get_mnemo( ledger ),
			COL_OBJECT,       ledger,
			-1 );

	set_row_by_iter( self, ledger, tmodel, &iter );

	/* select the newly added ledger */
	if( with_selection ){
		select_row_by_iter( self, tmodel, &iter );
		gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
	}
}

static void
set_row_by_iter( ofaLedgerTreeview *self, ofoLedger *ledger, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	ofaLedgerTreeviewPrivate *priv;
	GDate *dlast_entry;
	const GDate *dlast_close;
	gchar *slast_entry, *slast_close;

	priv = self->priv;

	dlast_entry = ofo_ledger_get_last_entry( ledger, priv->dossier );
	slast_entry = my_date_to_str( dlast_entry, MY_DATE_DMYY );

	dlast_close = ofo_ledger_get_last_close( ledger );
	slast_close = my_date_to_str( dlast_close, MY_DATE_DMYY );

	gtk_list_store_set(
			GTK_LIST_STORE( tmodel ),
			iter,
			COL_MNEMO,        ofo_ledger_get_mnemo( ledger ),
			COL_LABEL,        ofo_ledger_get_label( ledger ),
			COL_LAST_ENTRY,   slast_entry,
			COL_LAST_CLOSING, slast_close,
			-1 );

	g_free( slast_close );
	g_free( slast_entry );
	g_free( dlast_entry );
}

static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaLedgerTreeview *self )
{
	gchar *amnemo, *bmnemo;
	gint cmp;

	gtk_tree_model_get( tmodel, a, COL_MNEMO, &amnemo, -1 );
	gtk_tree_model_get( tmodel, b, COL_MNEMO, &bmnemo, -1 );

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
select_row_by_mnemo( ofaLedgerTreeview *self, const gchar *mnemo )
{
	ofaLedgerTreeviewPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	priv = self->priv;
	tmodel = gtk_tree_view_get_model( priv->tview );

	if( gtk_tree_model_get_iter_first( tmodel, &iter )){

		if( mnemo && g_utf8_strlen( mnemo, -1 )){
			find_row_by_mnemo( self, mnemo, NULL, &iter );
		}

		select_row_by_iter( self, tmodel, &iter );
	}

	gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
}

static void
select_row_by_iter( ofaLedgerTreeview *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	ofaLedgerTreeviewPrivate *priv;
	GtkTreeSelection *select;
	GtkTreePath *path;

	priv = self->priv;
	select = gtk_tree_view_get_selection( priv->tview );
	gtk_tree_selection_select_iter( select, iter );

	path = gtk_tree_model_get_path( tmodel, iter );
	gtk_tree_view_set_cursor( priv->tview, path, NULL, FALSE );
	gtk_tree_path_free( path );
}

static gboolean
find_row_by_mnemo( ofaLedgerTreeview *self, const gchar *mnemo, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	static const gchar *thisfn = "ofa_ledger_treeview_find_row_by_mnemo";
	ofaLedgerTreeviewPrivate *priv;
	GtkTreeModel *my_tmodel;
	GtkTreeIter my_iter;
	gchar *row_mnemo;
	gint cmp;
	gboolean found;

	g_debug( "%s: self=%p, mnemo=%s, tmodel=%p, iter=%p",
				thisfn, ( void * ) self, mnemo, ( void * ) tmodel, ( void * ) iter );

	priv = self->priv;
	found = FALSE;

	my_tmodel = gtk_tree_view_get_model( priv->tview );
	if( gtk_tree_model_get_iter_first( my_tmodel, &my_iter )){
		while( TRUE ){
			gtk_tree_model_get( my_tmodel, &my_iter, COL_MNEMO, &row_mnemo, -1 );
			cmp = cmp_by_mnemo( row_mnemo, mnemo );
			g_free( row_mnemo );
			if( cmp == 0 ){
				found = TRUE;
			}
			if( cmp >= 0 ){
				break;
			}
			if( !gtk_tree_model_iter_next( my_tmodel, &my_iter )){
				break;
			}
		}
	}

	if( tmodel ){
		*tmodel = my_tmodel;
	}
	if( iter ){
		*iter = my_iter;
	}

	return( found );
}

static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaLedgerTreeview *self )
{
	GList *sel_objects;

	sel_objects = get_selected( self );

	g_signal_emit_by_name( self, "activated", sel_objects );
}

static void
on_row_selected( GtkTreeSelection *selection, ofaLedgerTreeview *self )
{
	GList *sel_objects;

	sel_objects = get_selected( self );

	g_signal_emit_by_name( self, "changed", sel_objects );
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

	priv = self->priv;
	sel_mnemos = NULL;
	select = gtk_tree_view_get_selection( priv->tview );
	sel_rows = gtk_tree_selection_get_selected_rows( select, &tmodel );

	for( irow=sel_rows ; irow ; irow=irow->next ){
		if( gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) irow->data )){
			gtk_tree_model_get( tmodel, &iter, COL_MNEMO, &ledger, -1 );
			sel_mnemos = g_list_append( sel_mnemos, ledger );
		}
	}

	g_list_free_full( sel_rows, ( GDestroyNotify ) gtk_tree_path_free );

	return( sel_mnemos );
}

static void
on_list_cleanup_handler( ofaLedgerTreeview *self, GList *list )
{
	static const gchar *thisfn = "ofa_ledger_treeview_on_list_cleanup_handler";

	g_debug( "%s: self=%p, list=%p", thisfn, ( void * ) self, ( void * ) list );

	ofa_ledger_treeview_free_selected( list );
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
ofa_ledger_treeview_get_selected( ofaLedgerTreeview *self )
{
	g_return_val_if_fail( self && OFA_IS_LEDGER_TREEVIEW( self ), NULL );

	if( !self->priv->dispose_has_run ){

		return( get_selected( self ));
	}

	return( NULL );
}

/**
 * ofa_ledger_treeview_get_top_focusable_widget:
 */
GtkWidget *
ofa_ledger_treeview_get_top_focusable_widget( ofaLedgerTreeview *view )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_val_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ), NULL );

	priv = view->priv;

	if( !priv->dispose_has_run ){

		return( GTK_WIDGET( priv->tview ));
	}

	return( NULL );
}

/*
 * SIGNAL_DOSSIER_NEW_OBJECT signal handler
 */
static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaLedgerTreeview *self )
{
	static const gchar *thisfn = "ofa_ledger_treeview_on_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_LEDGER( object )){
		insert_new_row( self, OFO_LEDGER( object ), TRUE );
	}
}

/*
 * OFA_SIGNAL_UPDATE_OBJECT signal handler
 */
static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaLedgerTreeview *self )
{
	static const gchar *thisfn = "ofa_ledger_treeview_on_updated_object";
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	const gchar *mnemo;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn, ( void * ) dossier,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), prev_id, ( void * ) self );

	if( OFO_IS_LEDGER( object )){
		mnemo = prev_id ? prev_id : ofo_ledger_get_mnemo( OFO_LEDGER( object ));
		if( find_row_by_mnemo(
					self,
					mnemo,
					&tmodel,
					&iter )){

			set_row_by_iter( self, OFO_LEDGER( object ), tmodel, &iter );
		}
	}
}

/*
 * SIGNAL_DOSSIER_DELETED_OBJECT signal handler
 */
static void
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaLedgerTreeview *self )
{
	static const gchar *thisfn = "ofa_ledger_treeview_on_deleted_object";
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn, ( void * ) dossier,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), ( void * ) self );

	if( OFO_IS_LEDGER( object )){
		if( find_row_by_mnemo(
					self,
					ofo_ledger_get_mnemo( OFO_LEDGER( object )),
					&tmodel,
					&iter )){

			gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
		}
	}
}

/*
 * SIGNAL_DOSSIER_RELOAD_DATASET signal handler
 */
static void
on_reloaded_dataset( ofoDossier *dossier, GType type, ofaLedgerTreeview *self )
{
	static const gchar *thisfn = "ofa_ledger_treeview_on_reloaded_dataset";
	GtkTreeModel *tmodel;

	g_debug( "%s: dossier=%p, type=%lu, self=%p",
			thisfn, ( void * ) dossier, type, ( void * ) self );

	if( type == OFO_TYPE_LEDGER ){
		tmodel = gtk_tree_view_get_model( self->priv->tview );
		gtk_list_store_clear( GTK_LIST_STORE( tmodel ));
		insert_dataset( self, NULL );
	}
}
