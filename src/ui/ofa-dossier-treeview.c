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

#include "ui/ofa-dossier-treeview.h"

/* private instance data
 */
struct _ofaDossierTreeviewPrivate {
	gboolean      dispose_has_run;

	/* UI
	 */
	GtkWidget    *scrolled;
	GtkTreeView  *tview;
	gint          dname_col;
};

static void       istore_iface_init( ofaDossierIStoreInterface *iface );
static guint      istore_get_interface_version( const ofaDossierIStore *instance );
static void       istore_attach_to( ofaDossierIStore *instance, GtkContainer *parent );
static void       istore_set_columns( ofaDossierIStore *instance, GtkListStore *store, ofaDossierColumns columns );
static gint       on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaDossierTreeview *tview );
static void       on_row_selected( GtkTreeSelection *selection, ofaDossierTreeview *self );
static void       on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaDossierTreeview *self );
static void       get_and_send( ofaDossierTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static GtkWidget *get_scrolled_window( ofaDossierTreeview *self );

G_DEFINE_TYPE_EXTENDED( ofaDossierTreeview, ofa_dossier_treeview, G_TYPE_OBJECT, 0, \
		G_IMPLEMENT_INTERFACE( OFA_TYPE_DOSSIER_ISTORE, istore_iface_init ));

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
}

static void
istore_iface_init( ofaDossierIStoreInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_combo_istore_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = istore_get_interface_version;
	iface->attach_to = istore_attach_to;
	iface->set_columns = istore_set_columns;
}

static guint
istore_get_interface_version( const ofaDossierIStore *instance )
{
	return( 1 );
}

static void
istore_attach_to( ofaDossierIStore *instance, GtkContainer *parent )
{
	ofaDossierTreeview *self;
	GtkWidget *scrolled;

	g_return_if_fail( instance && OFA_IS_DOSSIER_TREEVIEW( instance ));

	g_debug( "ofa_dossier_treeview_istore_attach_to: instance=%p (%s), parent=%p (%s)",
			( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			( void * ) parent, G_OBJECT_TYPE_NAME( parent ));

	self = OFA_DOSSIER_TREEVIEW( instance );

	scrolled = get_scrolled_window( self );
	gtk_container_add( parent, scrolled );

	gtk_widget_show_all( GTK_WIDGET( parent ));
}

static void
istore_set_columns( ofaDossierIStore *instance, GtkListStore *store, ofaDossierColumns columns )
{
	ofaDossierTreeviewPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	gint col_number;

	priv = OFA_DOSSIER_TREEVIEW( instance )->priv;

	get_scrolled_window( OFA_DOSSIER_TREEVIEW( instance ));

	gtk_tree_view_set_model( priv->tview, GTK_TREE_MODEL( store ));

	priv->dname_col = ofa_dossier_istore_get_column_number( instance, DOSSIER_COL_DNAME );

	if( columns & DOSSIER_COL_DNAME ){
		col_number = ofa_dossier_istore_get_column_number( instance, DOSSIER_COL_DNAME );
		cell = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
				"label", cell, "text", col_number, NULL );
		gtk_tree_view_append_column( priv->tview, column );
	}

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( store ),
			( GtkTreeIterCompareFunc ) on_sort_model, instance, NULL );

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( store ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
			GTK_SORT_ASCENDING );

	gtk_widget_show_all( GTK_WIDGET( priv->scrolled ));
}

/**
 * ofa_dossier_treeview_new:
 */
ofaDossierTreeview *
ofa_dossier_treeview_new( void )
{
	ofaDossierTreeview *self;

	self = g_object_new(
				OFA_TYPE_DOSSIER_TREEVIEW,
				NULL );

	return( self );
}

static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaDossierTreeview *tview )
{
	ofaDossierTreeviewPrivate *priv;
	gchar *aname, *bname;
	gint cmp;

	priv = tview->priv;

	gtk_tree_model_get( tmodel, a, priv->dname_col, &aname, -1 );
	gtk_tree_model_get( tmodel, b, priv->dname_col, &bname, -1 );

	cmp = g_utf8_collate( aname, bname );

	g_free( aname );
	g_free( bname );

	return( cmp );
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
	ofaDossierTreeviewPrivate *priv;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *dname;

	priv = self->priv;

	if( gtk_tree_selection_get_selected( selection, &model, &iter )){
		gtk_tree_model_get( model, &iter, priv->dname_col, &dname, -1 );
		g_signal_emit_by_name( self, signal, dname );
		g_free( dname );
	}
}

static GtkWidget *
get_scrolled_window( ofaDossierTreeview *self )
{
	ofaDossierTreeviewPrivate *priv;
	GtkTreeSelection *select;

	priv = self->priv;

	if( !priv->tview ){

		priv->scrolled = gtk_scrolled_window_new( NULL, NULL );
		gtk_scrolled_window_set_policy(
				GTK_SCROLLED_WINDOW( priv->scrolled ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );

		priv->tview = GTK_TREE_VIEW( gtk_tree_view_new());
		gtk_widget_set_hexpand( GTK_WIDGET( priv->tview ), TRUE );
		gtk_widget_set_vexpand( GTK_WIDGET( priv->tview ), TRUE );
		gtk_tree_view_set_headers_visible( priv->tview, FALSE );
		gtk_container_add( GTK_CONTAINER( priv->scrolled ), GTK_WIDGET( priv->tview ));

		g_signal_connect(
				G_OBJECT( priv->tview ), "row-activated", G_CALLBACK( on_row_activated ), self );

		select = gtk_tree_view_get_selection( priv->tview );
		gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );

		g_signal_connect(
				G_OBJECT( select ), "changed", G_CALLBACK( on_row_selected ), self );
	}

	return( GTK_WIDGET( priv->scrolled ));
}

/**
 * ofa_dossier_treeview_get_selected:
 *
 * Return: the name of the currently selected dossier, as a newly
 * allocated string which should be g_free() by the caller, or %NULL.
 */
gchar *
ofa_dossier_treeview_get_selected( ofaDossierTreeview *view )
{
	ofaDossierTreeviewPrivate *priv;
	gchar *dname;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	g_return_val_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ), NULL );

	priv = view->priv;
	dname = NULL;

	if( !priv->dispose_has_run ){

		select = gtk_tree_view_get_selection( priv->tview );
		if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
			gtk_tree_model_get( tmodel, &iter, priv->dname_col, &dname, -1 );
		}
	}

	return( dname );
}
