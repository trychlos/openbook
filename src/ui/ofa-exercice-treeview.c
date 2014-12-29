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
#include "api/ofo-dossier.h"

#include "ui/ofa-exercice-treeview.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaExerciceTreeviewPrivate {
	gboolean           dispose_has_run;

	/* runtime datas
	 */
	GtkWidget         *top_widget;
	GtkTreeView       *tview;
	ofaExerciceColumns columns;
	ofaExerciceStore  *store;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void        on_widget_finalized( ofaExerciceTreeview *view, gpointer finalized_parent );
static GtkWidget  *get_top_widget( ofaExerciceTreeview *self );
static void        create_treeview_columns( ofaExerciceTreeview *view );
static void        on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaExerciceTreeview *self );
static void        on_row_selected( GtkTreeSelection *selection, ofaExerciceTreeview *self );
static void        get_selected( ofaExerciceTreeview *self, gchar **label, gchar **dbname );


G_DEFINE_TYPE( ofaExerciceTreeview, ofa_exercice_treeview, G_TYPE_OBJECT );

static void
exercice_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_exercice_treeview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EXERCICE_TREEVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exercice_treeview_parent_class )->finalize( instance );
}

static void
exercice_treeview_dispose( GObject *instance )
{
	ofaExerciceTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_EXERCICE_TREEVIEW( instance ));

	priv = ( OFA_EXERCICE_TREEVIEW( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exercice_treeview_parent_class )->dispose( instance );
}

static void
ofa_exercice_treeview_init( ofaExerciceTreeview *self )
{
	static const gchar *thisfn = "ofa_exercice_treeview_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXERCICE_TREEVIEW( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_EXERCICE_TREEVIEW, ofaExerciceTreeviewPrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
ofa_exercice_treeview_class_init( ofaExerciceTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_exercice_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = exercice_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = exercice_treeview_finalize;

	g_type_class_add_private( klass, sizeof( ofaExerciceTreeviewPrivate ));

	/**
	 * ofaExerciceTreeview::changed:
	 *
	 * This signal is sent on the #ofaExerciceTreeview when the selection
	 * is changed.
	 *
	 * Arguments are the label and the database name.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaExerciceTreeview *view,
	 * 						const gchar       *label,
	 * 						const gchar       *dbname,
	 * 						gpointer           user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_EXERCICE_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_STRING, G_TYPE_STRING );

	/**
	 * ofaExerciceTreeview::activated:
	 *
	 * This signal is sent on the #ofaExerciceTreeview when the selection is
	 * activated.
	 *
	 * Arguments are the label and the database name.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaExerciceTreeview *view,
	 * 						const gchar       *label,
	 * 						const gchar       *dbname,
	 * 						gpointer           user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"activated",
				OFA_TYPE_EXERCICE_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_STRING, G_TYPE_STRING );
}

/**
 * ofa_exercice_treeview_new:
 */
ofaExerciceTreeview *
ofa_exercice_treeview_new( void )
{
	ofaExerciceTreeview *view;

	view = g_object_new( OFA_TYPE_EXERCICE_TREEVIEW, NULL );

	return( view );
}

/**
 * ofa_exercice_treeview_attach_to:
 */
void
ofa_exercice_treeview_attach_to( ofaExerciceTreeview *view, GtkContainer *parent )
{
	ofaExerciceTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_EXERCICE_TREEVIEW( view ));
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	g_debug( "ofa_exercice_treeview_attach_to: view=%p, parent=%p (%s)",
			( void * ) view,
			( void * ) parent, G_OBJECT_TYPE_NAME( parent ));

	priv = view->priv;

	if( !priv->dispose_has_run ){

		get_top_widget( view );

		gtk_container_add( parent, GTK_WIDGET( priv->top_widget ));
		g_object_weak_ref( G_OBJECT( priv->top_widget ), ( GWeakNotify ) on_widget_finalized, view );

		gtk_widget_show_all( GTK_WIDGET( parent ));
	}
}

static void
on_widget_finalized( ofaExerciceTreeview *view, gpointer finalized_widget )
{
	static const gchar *thisfn = "ofa_exercice_treeview_on_widget_finalized";

	g_debug( "%s: view=%p, finalized_widget=%p (%s)",
			thisfn, ( void * ) view, ( void * ) finalized_widget, G_OBJECT_TYPE_NAME( finalized_widget ));

	g_return_if_fail( view && OFA_IS_EXERCICE_TREEVIEW( view ));

	g_object_unref( view );
}

/*
 * if not already done, create a GtkTreeView inside of a GtkScrolledWindow
 */
static GtkWidget *
get_top_widget( ofaExerciceTreeview *self )
{
	ofaExerciceTreeviewPrivate *priv;
	GtkTreeSelection *select;
	GtkWidget *scrolled;

	priv = self->priv;

	if( !priv->top_widget ){
		priv->top_widget = gtk_frame_new( NULL );
		gtk_frame_set_shadow_type( GTK_FRAME( priv->top_widget ), GTK_SHADOW_IN );

		scrolled = gtk_scrolled_window_new( NULL, NULL );
		gtk_scrolled_window_set_policy(
				GTK_SCROLLED_WINDOW( scrolled ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
		gtk_container_add( GTK_CONTAINER( priv->top_widget ), scrolled );

		priv->tview = GTK_TREE_VIEW( gtk_tree_view_new());
		gtk_widget_set_hexpand( GTK_WIDGET( priv->tview ), TRUE );
		gtk_widget_set_vexpand( GTK_WIDGET( priv->tview ), TRUE );
		gtk_tree_view_set_headers_visible( priv->tview, TRUE );
		gtk_container_add( GTK_CONTAINER( scrolled ), GTK_WIDGET( priv->tview ));

		g_signal_connect(
				G_OBJECT( priv->tview ), "row-activated", G_CALLBACK( on_row_activated ), self );

		select = gtk_tree_view_get_selection( priv->tview );
		g_signal_connect(
				G_OBJECT( select ), "changed", G_CALLBACK( on_row_selected ), self );

		priv->store = ofa_exercice_store_new();
		gtk_tree_view_set_model( priv->tview, GTK_TREE_MODEL( priv->store ));
		g_object_unref( priv->store );
	}

	return( priv->top_widget );
}

/**
 * ofa_exercice_treeview_set_columns:
 */
void
ofa_exercice_treeview_set_columns( ofaExerciceTreeview *view, ofaExerciceColumns columns )
{
	ofaExerciceTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_EXERCICE_TREEVIEW( view ));

	priv = view->priv;

	if( !priv->dispose_has_run ){

		get_top_widget( view );

		priv->columns = columns;
		create_treeview_columns( view );
	}
}

static void
create_treeview_columns( ofaExerciceTreeview *view )
{
	ofaExerciceTreeviewPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	priv = view->priv;

	if( priv->columns & EXERCICE_DISP_STATUS ){
		cell = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
						_( "Status" ), cell, "text", EXERCICE_COL_STATUS, NULL );
		gtk_tree_view_append_column( priv->tview, column );
	}

	if( priv->columns & EXERCICE_DISP_LABEL ){
		cell = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
						_( "Label" ), cell, "text", EXERCICE_COL_LABEL, NULL );
		gtk_tree_view_column_set_expand( column, TRUE );
		gtk_tree_view_append_column( priv->tview, column );
	}

	if( priv->columns & EXERCICE_DISP_BEGIN ){
		cell = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
						_( "Begin" ), cell, "text", EXERCICE_COL_BEGIN, NULL );
		gtk_tree_view_append_column( priv->tview, column );
	}

	if( priv->columns & EXERCICE_DISP_END ){
		cell = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
						_( "End" ), cell, "text", EXERCICE_COL_END, NULL );
		gtk_tree_view_append_column( priv->tview, column );
	}

	if( priv->columns & EXERCICE_DISP_DBNAME ){
		cell = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
						_( "Database" ), cell, "text", EXERCICE_COL_DBNAME, NULL );
		gtk_tree_view_append_column( priv->tview, column );
	}

	gtk_widget_show_all( GTK_WIDGET( priv->top_widget ));
}

/**
 * ofa_exercice_treeview_set_dossier:
 */
void
ofa_exercice_treeview_set_dossier( ofaExerciceTreeview *view, const gchar *dname )
{
	ofaExerciceTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_EXERCICE_TREEVIEW( view ));
	g_return_if_fail( dname && g_utf8_strlen( dname, -1 ));

	priv = view->priv;

	if( !priv->dispose_has_run ){

		/* the treeview must have been created first */
		g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));

		ofa_exercice_store_set_dossier( priv->store, dname );
	}
}

static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaExerciceTreeview *self )
{
	gchar *label, *dbname;

	get_selected( self, &label, &dbname );
	g_signal_emit_by_name( self, "activated", label, dbname );
	g_free( label );
	g_free( dbname );
}

static void
on_row_selected( GtkTreeSelection *selection, ofaExerciceTreeview *self )
{
	gchar *label, *dbname;

	get_selected( self, &label, &dbname );
	g_signal_emit_by_name( self, "changed", label, dbname );
	g_free( label );
	g_free( dbname );
}

static void
get_selected( ofaExerciceTreeview *self, gchar **label, gchar **dbname )
{
	ofaExerciceTreeviewPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeIter iter;

	priv = self->priv;
	*label = NULL;
	*dbname = NULL;

	select = gtk_tree_view_get_selection( priv->tview );
	if( gtk_tree_selection_get_selected( select, NULL, &iter )){
		gtk_tree_model_get( GTK_TREE_MODEL( priv->store), &iter,
				EXERCICE_COL_LABEL, label, EXERCICE_COL_DBNAME, dbname, -1 );
	}
}

#if 0
/**
 * ofa_exercice_treeview_get_selected:
 *
 * Returns: the list of #ofoExercice ledgers selected mnemonics.
 *
 * The returned list should be ofa_exercice_treeview_free_selected() by
 * the caller.
 */
GList *
ofa_exercice_treeview_get_selected( ofaExerciceTreeview *self )
{
	g_return_val_if_fail( self && OFA_IS_EXERCICE_TREEVIEW( self ), NULL );

	if( !self->priv->dispose_has_run ){

		return( get_selected( self ));
	}

	return( NULL );
}
#endif
