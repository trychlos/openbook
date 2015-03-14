/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-dossier-treeview.h"

/* private instance data
 */
struct _ofaDossierTreeviewPrivate {
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
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void     attach_top_widget( ofaDossierTreeview *self );
static void     create_treeview_columns( ofaDossierTreeview *view, ofaDossierColumns *columns );
static void     create_treeview_store( ofaDossierTreeview *view );
static gboolean is_visible_row( GtkTreeModel *tfilter, GtkTreeIter *iter, ofaDossierTreeview *tview );
static void     on_row_selected( GtkTreeSelection *selection, ofaDossierTreeview *self );
static void     on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaDossierTreeview *self );
static void     get_and_send( ofaDossierTreeview *self, GtkTreeSelection *selection, const gchar *signal );

G_DEFINE_TYPE( ofaDossierTreeview, ofa_dossier_treeview, GTK_TYPE_BIN );

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

	priv = OFA_DOSSIER_TREEVIEW( instance )->priv;

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

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_TREEVIEW( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_DOSSIER_TREEVIEW, ofaDossierTreeviewPrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
ofa_dossier_treeview_class_init( ofaDossierTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_treeview_finalize;

	g_type_class_add_private( klass, sizeof( ofaDossierTreeviewPrivate ));

	/**
	 * ofaDossierTreeview::changed:
	 *
	 * This signal is sent on the #ofaDossierTreeview when the selection
	 * is changed.
	 *
	 * Arguments is the selected dossier name.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDossierTreeview *view,
	 * 						const gchar      *dname,
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
				1,
				G_TYPE_STRING );

	/**
	 * ofaDossierTreeview::activated:
	 *
	 * This signal is sent on the #ofaDossierTreeview when the selection is
	 * activated.
	 *
	 * Arguments is the selected ledger mnemo.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDossierTreeview *view,
	 * 						const gchar      *dname,
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
				1,
				G_TYPE_STRING );
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

	priv = self->priv;

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
ofa_dossier_treeview_set_columns( ofaDossierTreeview *view, ofaDossierColumns *columns )
{
	ofaDossierTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ));

	priv = view->priv;

	if( !priv->dispose_has_run ){

		create_treeview_columns( view, columns );
	}
}

/**
 * ofa_dossier_treeview_set_headers:
 */
void
ofa_dossier_treeview_set_headers( ofaDossierTreeview *view, gboolean visible )
{
	ofaDossierTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ));

	priv = view->priv;

	if( !priv->dispose_has_run ){

		priv->show_headers = visible;
		gtk_tree_view_set_headers_visible( priv->tview, visible );
	}
}

/**
 * ofa_dossier_treeview_set_show:
 */
void
ofa_dossier_treeview_set_show( ofaDossierTreeview *view, ofaDossierShow show )
{
	ofaDossierTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ));

	priv = view->priv;

	if( !priv->dispose_has_run ){

		priv->show_mode = show;
		if( priv->tfilter ){
			gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( priv->tfilter ));
		}
	}
}

static void
create_treeview_columns( ofaDossierTreeview *view, ofaDossierColumns *columns )
{
	ofaDossierTreeviewPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	gint i;

	priv = view->priv;

	for( i=0 ; columns[i] ; ++i ){
		if( columns[i] == DOSSIER_DISP_DNAME ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "Dossier" ), cell, "text", DOSSIER_COL_DNAME, NULL );
			gtk_tree_view_append_column( priv->tview, column );
		}

		if( columns[i] == DOSSIER_DISP_DBMS ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "Provider" ), cell, "text", DOSSIER_COL_DBMS, NULL );
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

		if( columns[i] == DOSSIER_DISP_DBNAME ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "DB name" ), cell, "text", DOSSIER_COL_DBNAME, NULL );
			gtk_tree_view_append_column( priv->tview, column );
		}

		if( columns[i] == DOSSIER_DISP_CODE ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "St." ), cell, "text", DOSSIER_COL_CODE, NULL );
			gtk_tree_view_append_column( priv->tview, column );
		}
	}

	gtk_widget_show_all( GTK_WIDGET( view ));
}

/*
 * create the store as soon as we add both the treeview and the columns
 * which will automatically load the dataset
 */
static void
create_treeview_store( ofaDossierTreeview *view )
{
	ofaDossierTreeviewPrivate *priv;

	priv = view->priv;

	if( !priv->store ){

		g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));

		priv->store = ofa_dossier_store_new();
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
	gchar *code;

	priv = tview->priv;
	visible = FALSE;
	gtk_tree_model_get( tmodel, iter, DOSSIER_COL_CODE, &code, -1 );

	/* code may not be set when coming from DossierNewMini */
	if( !my_strlen( code )){
		visible = TRUE;

	} else {
		switch( priv->show_mode ){
			case DOSSIER_SHOW_ALL:
				visible = TRUE;
				break;
			case DOSSIER_SHOW_CURRENT:
				visible = ( g_utf8_collate( code, DOS_STATUS_OPENED ) == 0 );
				break;
			case DOSSIER_SHOW_ARCHIVED:
				visible = ( g_utf8_collate( code, DOS_STATUS_CLOSED ) == 0 );
				break;
		}
	}
	/*
	g_debug( "is_visible_row: show_mode=%u, code=%s, visible=%s",
			priv->show_mode, code, visible ? "True":"False" );
	*/
	g_free( code );

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
	gchar *dname;

	if( gtk_tree_selection_get_selected( selection, &model, &iter )){
		gtk_tree_model_get( model, &iter, DOSSIER_COL_DNAME, &dname, -1 );
		g_signal_emit_by_name( self, signal, dname );
		g_free( dname );
	}
}

/**
 * ofa_dossier_treeview_add_row:
 */
void
ofa_dossier_treeview_add_row( ofaDossierTreeview *view, const gchar *dname )
{
	ofaDossierTreeviewPrivate *priv;
	gchar *dbms;

	g_return_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ));

	priv = view->priv;

	if( !priv->dispose_has_run ){

		dbms = ofa_settings_get_dossier_provider( dname );
		ofa_dossier_store_add_row( priv->store, dname, dbms );
		g_free( dbms );
	}
}

/**
 * ofa_dossier_treeview_get_store:
 */
ofaDossierStore *
ofa_dossier_treeview_get_store( const ofaDossierTreeview *view )
{
	ofaDossierTreeviewPrivate *priv;
	ofaDossierStore *store;

	g_return_val_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ), NULL );

	priv = view->priv;
	store = NULL;

	if( !priv->dispose_has_run ){
		store = priv->store;
	}

	return( store );
}

/**
 * ofa_dossier_treeview_get_selected:
 * @view:
 * @column_id:
 *
 * Return: the content of the specified @column_id for the currently
 * selected dossier, as a newly allocated string which should be g_free()
 * by the caller, or %NULL.
 */
gchar *
ofa_dossier_treeview_get_selected( const ofaDossierTreeview *view, gint column_id )
{
	ofaDossierTreeviewPrivate *priv;
	gchar *content;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	g_return_val_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ), NULL );

	priv = view->priv;
	content = NULL;

	if( !priv->dispose_has_run ){

		select = gtk_tree_view_get_selection( priv->tview );
		if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
			gtk_tree_model_get( tmodel, &iter, column_id, &content, -1 );
		}
	}

	return( content );
}

/**
 * ofa_dossier_treeview_set_selected:
 */
void
ofa_dossier_treeview_set_selected( const ofaDossierTreeview *view, const gchar *dname )
{
	ofaDossierTreeviewPrivate *priv;
	GtkTreeIter iter;
	gchar *str;
	gint cmp;
	GtkTreeSelection *select;
	GtkTreePath *path;

	g_return_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ));

	priv = view->priv;

	if( !priv->dispose_has_run ){

		if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( priv->store ), &iter )){
			while( TRUE ){
				gtk_tree_model_get(
						GTK_TREE_MODEL( priv->store ), &iter, DOSSIER_COL_DNAME, &str, -1 );
				cmp = g_utf8_collate( str, dname );
				g_free( str );
				if( !cmp ){
					select = gtk_tree_view_get_selection( priv->tview );
					path = gtk_tree_model_get_path( GTK_TREE_MODEL( priv->store ), &iter );
					gtk_tree_selection_select_path( select, path );
					/* move the cursor so that it is visible */
					gtk_tree_view_scroll_to_cell( priv->tview, path, NULL, FALSE, 0, 0 );
					gtk_tree_path_free( path );
					break;
				}
				if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( priv->store ), &iter )){
					break;
				}
			}
		}
	}
}
