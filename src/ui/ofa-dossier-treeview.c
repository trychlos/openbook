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

#include "api/ofa-idbmeta.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-dossier-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean         dispose_has_run;

	/* UI
	 */
	GtkTreeView     *tview;
	ofaDossierStore *store;

	/* runtime data
	 */
	gboolean         show_headers;
	ofaDossierShow   show_mode;
	GtkTreeModel    *tfilter;
}
	ofaDossierTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void     attach_top_widget( ofaDossierTreeview *self );
static void     create_treeview_columns( ofaDossierTreeview *view, ofaDossierDispColumn *columns );
static void     create_treeview_store( ofaDossierTreeview *view );
static gboolean is_visible_row( GtkTreeModel *tfilter, GtkTreeIter *iter, ofaDossierTreeview *tview );
static void     on_row_selected( GtkTreeSelection *selection, ofaDossierTreeview *self );
static void     on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaDossierTreeview *self );
static void     get_and_send( ofaDossierTreeview *self, GtkTreeSelection *selection, const gchar *signal );

G_DEFINE_TYPE_EXTENDED( ofaDossierTreeview, ofa_dossier_treeview, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaDossierTreeview ))

static void
dossier_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_treeview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_TREEVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_treeview_parent_class )->finalize( instance );
}

static void
dossier_treeview_dispose( GObject *instance )
{
	ofaDossierTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_TREEVIEW( instance ));

	priv = ofa_dossier_treeview_get_instance_private( OFA_DOSSIER_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_treeview_parent_class )->dispose( instance );
}

static void
ofa_dossier_treeview_init( ofaDossierTreeview *self )
{
	static const gchar *thisfn = "ofa_dossier_treeview_init";
	ofaDossierTreeviewPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_TREEVIEW( self ));

	priv = ofa_dossier_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_dossier_treeview_class_init( ofaDossierTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_treeview_finalize;

	/**
	 * ofaDossierTreeview::changed:
	 *
	 * This signal is sent on the #ofaDossierTreeview when the selection
	 * is changed.
	 *
	 * Arguments are the selected ofaIDBMeta and ofaIDBPeriod objects.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDossierTreeview *view,
	 * 						ofaIDBMeta       *meta,
	 * 						ofaIDBPeriod     *period,
	 * 						gpointer          user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_DOSSIER_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_OBJECT, G_TYPE_OBJECT );

	/**
	 * ofaDossierTreeview::activated:
	 *
	 * This signal is sent on the #ofaDossierTreeview when the selection is
	 * activated.
	 *
	 * Arguments are the selected ofaIDBMeta and ofaIDBPeriod objects.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDossierTreeview *view,
	 * 						ofaIDBMeta       *meta,
	 * 						ofaIDBPeriod     *period,
	 * 						gpointer          user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"activated",
				OFA_TYPE_DOSSIER_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_OBJECT, G_TYPE_OBJECT );
}

/**
 * ofa_dossier_treeview_new:
 */
ofaDossierTreeview *
ofa_dossier_treeview_new( void )
{
	ofaDossierTreeview *view;

	view = g_object_new( OFA_TYPE_DOSSIER_TREEVIEW, NULL );

	attach_top_widget( view );
	create_treeview_store( view );

	return( view );
}

/*
 * call right after the object instanciation
 * if not already done, create a GtkTreeView inside of a GtkScrolledWindow
 */
static void
attach_top_widget( ofaDossierTreeview *self )
{
	ofaDossierTreeviewPrivate *priv;
	GtkWidget *top_widget;
	GtkTreeSelection *select;
	GtkWidget *scrolled;

	priv = ofa_dossier_treeview_get_instance_private( self );

	top_widget = gtk_frame_new( NULL );
	gtk_frame_set_shadow_type( GTK_FRAME( top_widget ), GTK_SHADOW_IN );

	scrolled = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW( scrolled ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_container_add( GTK_CONTAINER( top_widget ), scrolled );

	priv->tview = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_hexpand( GTK_WIDGET( priv->tview ), TRUE );
	gtk_widget_set_vexpand( GTK_WIDGET( priv->tview ), TRUE );
	gtk_tree_view_set_headers_visible( priv->tview, FALSE );
	gtk_container_add( GTK_CONTAINER( scrolled ), GTK_WIDGET( priv->tview ));

	g_signal_connect(
			G_OBJECT( priv->tview ), "row-activated", G_CALLBACK( on_row_activated ), self );

	select = gtk_tree_view_get_selection( priv->tview );
	g_signal_connect(
			G_OBJECT( select ), "changed", G_CALLBACK( on_row_selected ), self );

	gtk_container_add( GTK_CONTAINER( self ), top_widget );
}

/**
 * ofa_dossier_treeview_set_columns:
 * @view:
 * @columns: a zero-terminated list of columns id.
 */
void
ofa_dossier_treeview_set_columns( ofaDossierTreeview *view, ofaDossierDispColumn *columns )
{
	ofaDossierTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ));

	priv = ofa_dossier_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	create_treeview_columns( view, columns );
}

/**
 * ofa_dossier_treeview_set_headers:
 */
void
ofa_dossier_treeview_set_headers( ofaDossierTreeview *view, gboolean visible )
{
	ofaDossierTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ));

	priv = ofa_dossier_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	priv->show_headers = visible;
	gtk_tree_view_set_headers_visible( priv->tview, visible );
}

/**
 * ofa_dossier_treeview_set_show:
 */
void
ofa_dossier_treeview_set_show( ofaDossierTreeview *view, ofaDossierShow show )
{
	ofaDossierTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ));

	priv = ofa_dossier_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	priv->show_mode = show;
	if( priv->tfilter ){
		gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( priv->tfilter ));
	}
}

static void
create_treeview_columns( ofaDossierTreeview *view, ofaDossierDispColumn *columns )
{
	ofaDossierTreeviewPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	gint i;

	priv = ofa_dossier_treeview_get_instance_private( view );

	for( i=0 ; columns[i] ; ++i ){
		if( columns[i] == DOSSIER_DISP_DOSNAME ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "Dossier" ), cell, "text", DOSSIER_COL_DOSNAME, NULL );
			gtk_tree_view_append_column( priv->tview, column );
		}

		if( columns[i] == DOSSIER_DISP_PROVNAME ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "Provider" ), cell, "text", DOSSIER_COL_PROVNAME, NULL );
			gtk_tree_view_append_column( priv->tview, column );
		}

		if( columns[i] == DOSSIER_DISP_BEGIN ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "Begin" ), cell, "text", DOSSIER_COL_BEGIN, NULL );
			gtk_tree_view_append_column( priv->tview, column );
		}

		if( columns[i] == DOSSIER_DISP_END ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "End" ), cell, "text", DOSSIER_COL_END, NULL );
			gtk_tree_view_append_column( priv->tview, column );
		}

		if( columns[i] == DOSSIER_DISP_STATUS ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "Status" ), cell, "text", DOSSIER_COL_STATUS, NULL );
			gtk_tree_view_append_column( priv->tview, column );
		}

		if( columns[i] == DOSSIER_DISP_PERNAME ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "Period name" ), cell, "text", DOSSIER_COL_PERNAME, NULL );
			gtk_tree_view_append_column( priv->tview, column );
		}
	}

	gtk_widget_show_all( GTK_WIDGET( view ));
}

/*
 * create the store which automatically loads the dataset
 */
static void
create_treeview_store( ofaDossierTreeview *view )
{
	ofaDossierTreeviewPrivate *priv;

	priv = ofa_dossier_treeview_get_instance_private( view );

	if( !priv->store ){

		g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));

		priv->store = ofa_dossier_store_new( NULL );
		priv->tfilter = gtk_tree_model_filter_new( GTK_TREE_MODEL( priv->store ), NULL );
		/* unref the store so that it will be automatically unreffed
		 * at the same time the treeview will be. */
		g_object_unref( priv->store );

		gtk_tree_model_filter_set_visible_func(
				GTK_TREE_MODEL_FILTER( priv->tfilter ),
				( GtkTreeModelFilterVisibleFunc ) is_visible_row, view, NULL );

		gtk_tree_view_set_model( priv->tview, priv->tfilter );
		g_object_unref( priv->tfilter );
	}

	gtk_widget_show_all( GTK_WIDGET( view ));
}

static gboolean
is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaDossierTreeview *tview )
{
	ofaDossierTreeviewPrivate *priv;
	gboolean visible;
	ofaIDBMeta *meta;
	ofaIDBPeriod *period;
	GList *periods;

	priv = ofa_dossier_treeview_get_instance_private( tview );

	visible = TRUE;
	gtk_tree_model_get( tmodel, iter, DOSSIER_COL_META, &meta, DOSSIER_COL_PERIOD, &period, -1 );

	/* note: a new row is first inserted, before the columns be set
	 * (cf. ofaDossierStore::insert_row())
	 * so just ignore the row if columns are still empty */
	if( !meta || !period ){
		return( FALSE );
	}

	switch( priv->show_mode ){
		case DOSSIER_SHOW_ALL:
			visible = TRUE;
			break;
		case DOSSIER_SHOW_UNIQUE:
			periods = ofa_idbmeta_get_periods( meta );
			if( g_list_length( periods ) == 1 ){
				visible = TRUE;
			} else {
				visible = ofa_idbperiod_get_current( period );
			}
			ofa_idbmeta_free_periods( periods );
			break;
	}

	g_object_unref( period );
	g_object_unref( meta );

	return( visible );
}

static void
on_row_selected( GtkTreeSelection *selection, ofaDossierTreeview *self )
{
	get_and_send( self, selection, "changed" );
}

static void
on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaDossierTreeview *self )
{
	GtkTreeSelection *select;

	select = gtk_tree_view_get_selection( tview );
	get_and_send( self, select, "activated" );
}

static void
get_and_send( ofaDossierTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	ofaIDBMeta *meta;
	ofaIDBPeriod *period;

	if( gtk_tree_selection_get_selected( selection, &model, &iter )){
		gtk_tree_model_get( model, &iter,
				DOSSIER_COL_META,   &meta,
				DOSSIER_COL_PERIOD, &period,
				-1 );
		g_signal_emit_by_name( self, signal, meta, period );
		g_object_unref( meta );
		g_object_unref( period );
	}
}

/**
 * ofa_dossier_treeview_get_treeview:
 * @view: this #ofaDossierTreeview instance.
 *
 * Returns: the underlying #GtkTreeView widget.
 */
GtkWidget *
ofa_dossier_treeview_get_treeview( const ofaDossierTreeview *view )
{
	ofaDossierTreeviewPrivate *priv;
	GtkWidget *tview;

	g_return_val_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ), NULL );

	priv = ofa_dossier_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	tview = GTK_WIDGET( priv->tview );

	return( tview );
}

/**
 * ofa_dossier_treeview_get_store:
 * @view: this #ofaDossierTreeview instance.
 *
 * Returns: the underlying #ofaDossierStore store.
 */
ofaDossierStore *
ofa_dossier_treeview_get_store( const ofaDossierTreeview *view )
{
	ofaDossierTreeviewPrivate *priv;
	ofaDossierStore *store;

	g_return_val_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ), NULL );

	priv = ofa_dossier_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	store = priv->store;

	return( store );
}

/**
 * ofa_dossier_treeview_get_selected:
 * @view: this #ofaDossierTreeview instance.
 * @meta: [allow-none][out]: a new reference on the currently selected
 *  #ofaIDBMeta row, which should be g_object_unref() by the caller
 *  when non %NULL.
 * @period: [allow-none][out]: a new reference on the currently selected
 *  #ofaIDBPeriod row, which should be g_object_unref() by the caller
 *  when non %NULL.
 *
 * Returns: %TRUE if a selection exists, %FALSE else.
 */
gboolean
ofa_dossier_treeview_get_selected( const ofaDossierTreeview *view, ofaIDBMeta **meta, ofaIDBPeriod **period )
{
	ofaDossierTreeviewPrivate *priv;
	gboolean ok;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreeSelection *select;
	ofaIDBMeta *row_meta;
	ofaIDBPeriod *row_period;

	g_return_val_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ), FALSE );

	priv = ofa_dossier_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	ok = FALSE;
	if( meta ){
		*meta = NULL;
	}
	if( period ){
		*period = NULL;
	}
	select = gtk_tree_view_get_selection( priv->tview );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
		ok = TRUE;
		gtk_tree_model_get( tmodel, &iter,
				DOSSIER_COL_META,   &row_meta,
				DOSSIER_COL_PERIOD, &row_period,
				-1 );
		if( meta ){
			*meta = row_meta;
		} else {
			g_object_unref( row_meta );
		}
		if( period ){
			*period = row_period;
		} else {
			g_object_unref( row_period );
		}
	}
	return( ok );
}

/**
 * ofa_dossier_treeview_set_selected:
 *
 * Only a visible row may be selected, so iterate on the filter store.
 */
void
ofa_dossier_treeview_set_selected( const ofaDossierTreeview *view, const gchar *dname )
{
	static const gchar *thisfn = "ofa_dossier_treeview_set_selected";
	ofaDossierTreeviewPrivate *priv;
	GtkTreeIter iter;
	gchar *str;
	gint cmp;
	GtkTreeSelection *select;
	GtkTreePath *path;

	g_debug( "%s: view=%p, dname=%s", thisfn, ( void * ) view, dname );

	g_return_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ));

	priv = ofa_dossier_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( priv->tfilter ), &iter )){
		while( TRUE ){
			gtk_tree_model_get(
					GTK_TREE_MODEL( priv->tfilter ), &iter, DOSSIER_COL_DOSNAME, &str, -1 );
			cmp = g_utf8_collate( str, dname );
			g_free( str );
			if( cmp == 0 ){
				select = gtk_tree_view_get_selection( priv->tview );
				path = gtk_tree_model_get_path( GTK_TREE_MODEL( priv->tfilter ), &iter );
				gtk_tree_selection_select_path( select, path );
				/* move the cursor so that it is visible */
				gtk_tree_view_scroll_to_cell( priv->tview, path, NULL, FALSE, 0, 0 );
				gtk_tree_path_free( path );
				break;
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( priv->tfilter ), &iter )){
				break;
			}
		}
	}
}
