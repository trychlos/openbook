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

#include "api/my-utils.h"
#include "api/ofa-settings.h"

#include "ui/ofa-dossier-misc.h"
#include "ui/ofa-dossier-treeview.h"

/* private instance data
 */
struct _ofaDossierTreeviewPrivate {
	gboolean      dispose_has_run;

	/* UI
	 */
	GtkContainer *container;
	GtkTreeView  *tview;
};

static const gchar  *st_window_xml      = PKGUIDIR "/ofa-dossier-treeview.piece.ui";
static const gchar  *st_window_id       = "DossierTreeviewWindow";

/* column ordering in the dossier selection listview
 */
enum {
	COL_NAME = 0,
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

G_DEFINE_TYPE( ofaDossierTreeview, ofa_dossier_treeview, G_TYPE_OBJECT )

static void on_parent_finalized( ofaDossierTreeview *self, gpointer finalized_parent );
static void setup_treeview( ofaDossierTreeview *tview );
static gint on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data );
static void on_dossier_changed( GtkTreeSelection *selection, ofaDossierTreeview *self );
static void on_dossier_changed_cleanup_handler( ofaDossierTreeview *self, gchar *name );
static void on_dossier_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaDossierTreeview *self );
static void on_dossier_activated_cleanup_handler( ofaDossierTreeview *self, gchar *name );
static void get_and_send( ofaDossierTreeview *self, GtkTreeSelection *selection, const gchar *signal );

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
	 * This signal is sent when the selection is changed.
	 *
	 * Arguments is the name of the selected dossier.
	 *
	 * The cleanup handler will take care of freeing this name.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDossierTreeview *treeview,
	 * 						const gchar      *name,
	 * 						gpointer          user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_DOSSIER_TREEVIEW,
				G_SIGNAL_RUN_CLEANUP,
				G_CALLBACK( on_dossier_changed_cleanup_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );

	/**
	 * ofaDossierTreeview::activated:
	 *
	 * This signal is sent when the selection is activated.
	 *
	 * Arguments is the name of the selected dossier.
	 *
	 * The cleanup handler will take care of freeing this name.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDossierTreeview *treeview,
	 * 						const gchar      *name,
	 * 						gpointer          user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"activated",
				OFA_TYPE_DOSSIER_TREEVIEW,
				G_SIGNAL_RUN_CLEANUP,
				G_CALLBACK( on_dossier_activated_cleanup_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );
}

static void
on_parent_finalized( ofaDossierTreeview *self, gpointer finalized_parent )
{
	static const gchar *thisfn = "ofa_dossier_treeview_on_parent_finalized";

	g_debug( "%s: self=%p, finalized_parent=%p", thisfn, ( void * ) self, ( void * ) finalized_parent );

	g_return_if_fail( self && OFA_IS_DOSSIER_TREEVIEW( self ));

	g_object_unref( self );
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

/**
 * ofa_dossier_treeview_attach_to:
 * @tview: this #ofaDossierTreeview instance.
 * @new_parent: the new parent.
 *
 * Attach the piece of window to the @new_parent.
 */
void
ofa_dossier_treeview_attach_to( ofaDossierTreeview *tview, GtkContainer *new_parent )
{
	ofaDossierTreeviewPrivate *priv;
	GtkWidget *window, *widget;

	g_return_if_fail( tview && OFA_IS_DOSSIER_TREEVIEW( tview ));
	g_return_if_fail( new_parent && GTK_IS_CONTAINER( new_parent ));

	priv = tview->priv;

	if( !priv->dispose_has_run ){

		g_object_weak_ref( G_OBJECT( new_parent ), ( GWeakNotify ) on_parent_finalized, tview );

		window = my_utils_builder_load_from_path( st_window_xml, st_window_id );
		g_return_if_fail( window && GTK_IS_CONTAINER( window ));

		widget = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "scrolled" );
		g_return_if_fail( widget && GTK_IS_WIDGET( widget ));

		gtk_widget_reparent( widget, GTK_WIDGET( new_parent ));
		priv->container = GTK_CONTAINER( widget );

		setup_treeview( tview );
	}
}

static void
setup_treeview( ofaDossierTreeview *self )
{
	ofaDossierTreeviewPrivate *priv;
	GtkWidget *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = self->priv;

	tview = my_utils_container_get_child_by_name( priv->container, "treeview" );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));
	priv->tview = GTK_TREE_VIEW( tview );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new( N_COLUMNS, G_TYPE_STRING ));
	gtk_tree_view_set_model( priv->tview, tmodel );
	g_object_unref( tmodel );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ),
			( GtkTreeIterCompareFunc ) on_sort_model, NULL, NULL );
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
			GTK_SORT_ASCENDING );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			"label",
			cell,
			"text", COL_NAME,
			NULL );
	gtk_tree_view_append_column( priv->tview, column );

	g_signal_connect(
			G_OBJECT( priv->tview ), "row-activated", G_CALLBACK( on_dossier_activated ), self );

	select = gtk_tree_view_get_selection( priv->tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect(
			G_OBJECT( select ), "changed", G_CALLBACK( on_dossier_changed ), self );
}

static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data )
{
	gchar *aname, *bname;
	gint cmp;

	gtk_tree_model_get( tmodel, a, COL_NAME, &aname, -1 );
	gtk_tree_model_get( tmodel, b, COL_NAME, &bname, -1 );

	cmp = g_utf8_collate( aname, bname );

	g_free( aname );
	g_free( bname );

	return( cmp );
}

static void
on_dossier_changed( GtkTreeSelection *selection, ofaDossierTreeview *self )
{
	get_and_send( self, selection, "changed" );
}

static void
on_dossier_changed_cleanup_handler( ofaDossierTreeview *self, gchar *name )
{
	static const gchar *thisfn = "ofa_dossier_treeview_on_dossier_changed_cleanup_handler";

	g_debug( "%s: self=%p, name=%s", thisfn, ( void * ) self, name );

	g_free( name );
}

static void
on_dossier_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaDossierTreeview *self )
{
	GtkTreeSelection *select;

	select = gtk_tree_view_get_selection( tview );
	get_and_send( self, select, "activated" );
}

static void
on_dossier_activated_cleanup_handler( ofaDossierTreeview *self, gchar *name )
{
	static const gchar *thisfn = "ofa_dossier_treeview_on_dossier_activated_cleanup_handler";

	g_debug( "%s: self=%p, name=%s", thisfn, ( void * ) self, name );

	g_free( name );
}

static void
get_and_send( ofaDossierTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *name;

	if( gtk_tree_selection_get_selected( selection, &model, &iter )){
		gtk_tree_model_get( model, &iter, COL_NAME, &name, -1 );
		g_signal_emit_by_name( self, signal, name );
	}
}

/**
 * ofa_dossier_treeview_init_view:
 * @tview: this #ofaDossierTreeview instance.
 * @name: [allow-none]: the first name to be selected.
 *
 * Populates the view.
 */
void
ofa_dossier_treeview_init_view( ofaDossierTreeview *self, const gchar *name )
{
	ofaDossierTreeviewPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter, select_iter;
	GtkTreeSelection *select;
	GSList *list, *it;
	gboolean found;

	g_return_if_fail( self && OFA_IS_DOSSIER_TREEVIEW( self ));

	priv = self->priv;

	if( !priv->dispose_has_run ){

		list = ofa_dossier_misc_get_dossiers();
		tmodel = gtk_tree_view_get_model( priv->tview );
		found = FALSE;

		for( it=list ; it ; it=it->next ){
			gtk_list_store_insert_with_values(
					GTK_LIST_STORE( tmodel ), &iter, -1,
					COL_NAME, ( const gchar * ) it->data,
					-1 );
			if( name && !found && !g_utf8_collate( name, ( const gchar * ) it->data )){
				found = TRUE;
				select_iter = iter;
			}
		}

		ofa_dossier_misc_free_dossiers( list );

		select = gtk_tree_view_get_selection( priv->tview );

		if( found ){
			gtk_tree_selection_select_iter( select, &select_iter );

		} else if( gtk_tree_model_get_iter_first( tmodel, &iter )){
			gtk_tree_selection_select_iter( select, &iter );
		}
	}
}
