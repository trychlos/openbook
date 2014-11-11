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
 *
 * To display debug messages, run the command:
 *   $ G_MESSAGES_DEBUG=OFA _install/bin/openbook
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*
 * As of version 0.3, a recurent bug appears when deleting an element
 * from a treeview - This program is to try to reproduce this behavior
 */
#include <gtk/gtk.h>

/* header
 */

#define MY_TYPE_FOO            (my_foo_get_type ())
#define MY_FOO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MY_TYPE_FOO, MyFoo))
#define MY_IS_FOO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MY_TYPE_FOO))
#define MY_FOO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MY_TYPE_FOO, MyFooClass))
#define MY_IS_FOO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MY_TYPE_FOO))
#define MY_FOO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MY_TYPE_FOO, MyFooClass))

typedef struct _MyFooClass     MyFooClass;

struct _MyFooClass
{
	GObjectClass parent;
	/* class members */
	/* public virtual methods */
};

typedef struct _MyFoo          MyFoo;
typedef struct _MyFooPrivate   MyFooPrivate;

struct _MyFoo
{
	GObject       parent;
	/* public object members */
	/* private object members */
	MyFooPrivate *priv;
};

GType my_foo_get_type( void ) G_GNUC_CONST;

/* source code
 */

struct _MyFooPrivate
{
	gchar *code;
	gchar *label;
};

/* unable to compile this
G_DEFINE_TYPE_WITH_PRIVATE( MyFoo, my_foo, G_TYPE_OBJECT );
*/
G_DEFINE_TYPE( MyFoo, my_foo, G_TYPE_OBJECT )

#define MY_FOO_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MY_TYPE_FOO, MyFooPrivate))

static void
my_foo_finalize( GObject *instance )
{
	MyFoo *self = MY_FOO( instance );
	g_debug( "my_foo_finalize: self=%p", ( void * ) self );
	/* free data here */
	g_free( self->priv->code );
	g_free( self->priv->label );
	/* chain up to parent class */
	G_OBJECT_CLASS( my_foo_parent_class )->finalize( instance );
}

static void
my_foo_dispose( GObject *instance )
{
	MyFoo *self = MY_FOO( instance );
	g_debug( "my_foo_dispose: self=%p", ( void * ) self );
	/* unref member objects here */
	/* chain up to parent class */
	G_OBJECT_CLASS( my_foo_parent_class )->dispose( instance );
}

static void
my_foo_init( MyFoo *self )
{
	g_debug( "my_foo_init: self=%p", ( void * ) self );
	self->priv = MY_FOO_GET_PRIVATE( self );
	/*self->priv = my_foo_get_instance_private( self );*/
}

static void
my_foo_class_init( MyFooClass *klass )
{
	g_type_class_add_private( klass, sizeof( MyFooPrivate ));

	G_OBJECT_CLASS( klass )->dispose = my_foo_dispose;
	G_OBJECT_CLASS( klass )->finalize = my_foo_finalize;
}

static MyFoo *
my_foo_new( void )
{
	return( g_object_new( MY_TYPE_FOO, NULL ));
}

static GList *
my_foo_delete( GList *set, MyFoo *deleted )
{
	GList *new_set = g_list_remove( set, deleted );
	g_object_unref( deleted );
	return( new_set );
}

static GList *
load_objects( void )
{
	gint i;
	MyFoo *tobj;
	GList *set = NULL;

	for( i=0 ; i<50 ; ++i ){
		tobj = my_foo_new();
		tobj->priv->code = g_strdup_printf( "Code %3.3d", i );
		tobj->priv->label = g_strdup_printf( "Label %3.3d", i );
		set = g_list_prepend( set, tobj );
	}

	return( g_list_reverse( set ));
}

enum {
	COL_CODE = 0,
	COL_LABEL,
	COL_OBJECT,
	N_COLUMNS
};

static GList       *st_list = NULL;
static MyFoo       *st_selected_obj = NULL;
static GtkTreeIter *st_selected_iter = NULL;

static void
on_delete_row( GtkButton *button, GtkTreeView *view )
{
	g_return_if_fail( view && GTK_IS_TREE_VIEW( view ));

	GtkTreeSelection *select = gtk_tree_view_get_selection( view );
	GtkTreeModel *model;
	GtkTreeIter iter;
	if( gtk_tree_selection_get_selected( select, &model, &iter )){
		MyFoo *obj;
		gtk_tree_model_get( model, &iter, COL_OBJECT, &obj, -1 );
		g_debug( "on_delete_row: deleting %s - %s", obj->priv->code, obj->priv->label );
		st_list = my_foo_delete( st_list, obj );
		gtk_list_store_remove( GTK_LIST_STORE( model ), &iter );
	}
}

static void
on_row_selected( GtkTreeSelection *selection, gpointer user_data )
{
	static const gchar *thisfn = "on_row_selected";
	GtkTreeModel *model;
	GtkTreeIter iter;
	MyFoo *foo;

	if( st_selected_obj ){
		st_selected_obj = NULL;
		gtk_tree_iter_free( st_selected_iter );
		st_selected_iter = NULL;
	}

	if( gtk_tree_selection_get_selected( selection, &model, &iter )){

		gtk_tree_model_get( model, &iter, COL_OBJECT, &foo, -1 );
		g_return_if_fail( MY_IS_FOO( foo ));

		g_debug( "%s: selecting %s - %s",
				thisfn, foo->priv->code, foo->priv->label );

		st_selected_obj = foo;
		st_selected_iter = gtk_tree_iter_copy( &iter );

		g_object_unref( foo );
	}
}

int
main( int argc, char *argv[] )
{
	gtk_init( &argc, &argv );

	st_list = load_objects();

	GtkWindow *window = GTK_WINDOW( gtk_window_new( GTK_WINDOW_TOPLEVEL ));
	gtk_window_set_title( window, "Openbook [Test] Delete" );
	gtk_window_set_default_size( window, 600, 400 );
	g_signal_connect_swapped( G_OBJECT( window ), "destroy", G_CALLBACK( gtk_main_quit ), NULL );

	GtkGrid *grid = GTK_GRID( gtk_grid_new());
	gtk_container_add( GTK_CONTAINER( window ), GTK_WIDGET( grid ));

	GtkScrolledWindow *scrolled = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_grid_attach( grid, GTK_WIDGET( scrolled ), 0, 0, 1, 1 );

	GtkTreeView *view = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_hexpand( GTK_WIDGET( view ), TRUE );
	gtk_widget_set_vexpand( GTK_WIDGET( view ), TRUE );
	gtk_container_add( GTK_CONTAINER( scrolled ), GTK_WIDGET( view ));

	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
				"Code",
				cell, "text", COL_CODE,
				NULL );
	gtk_tree_view_append_column( view, column );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
				"Label",
				cell, "text", COL_LABEL,
				NULL );
	gtk_tree_view_append_column( view, column );

	GtkTreeModel *model = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( view, model );
	g_object_unref( model ); /* so that view holds the only ref to model */

	GtkButton *button = GTK_BUTTON( gtk_button_new_from_icon_name( "edit-delete", GTK_ICON_SIZE_BUTTON ));
	gtk_widget_set_valign( GTK_WIDGET( button ), GTK_ALIGN_START );
	gtk_grid_attach( grid, GTK_WIDGET( button ), 1, 0, 1, 1 );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_delete_row ), view );

	GtkTreeIter iter;
	GList *io;
	for( io=st_list ; io ; io=io->next ){
		gtk_list_store_append(GTK_LIST_STORE( model ), &iter );
		gtk_list_store_set( GTK_LIST_STORE( model ), &iter,
				COL_CODE, MY_FOO( io->data )->priv->code,
				COL_LABEL, MY_FOO( io->data )->priv->label,
				COL_OBJECT, MY_FOO( io->data ),
				-1 );
	}

	GtkTreeSelection *select = gtk_tree_view_get_selection( view );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_row_selected ), NULL );

	gtk_widget_show_all( GTK_WIDGET( window ));
	gtk_main();

	return 0;
}
