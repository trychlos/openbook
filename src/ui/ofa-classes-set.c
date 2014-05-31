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

#include "ui/ofa-main-page.h"
#include "ui/ofa-class-properties.h"
#include "ui/ofa-classes-set.h"
#include "ui/ofo-class.h"

/* private instance data
 */
struct _ofaClassesSetPrivate {
	gboolean dispose_has_run;
};

/* column ordering in the selection listview
 */
enum {
	COL_NUMBER = 0,
	COL_LABEL,
	COL_OBJECT,
	N_COLUMNS
};

G_DEFINE_TYPE( ofaClassesSet, ofa_classes_set, OFA_TYPE_MAIN_PAGE )

static GtkWidget *v_setup_view( ofaMainPage *page );
static GtkWidget *v_setup_buttons( ofaMainPage *page );
static void       v_init_view( ofaMainPage *page );
static void       insert_new_row( ofaClassesSet *self, ofoClass *class, gboolean with_selection );
static gint       on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaClassesSet *self );
static void       setup_first_selection( ofaClassesSet *self );
static void       on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainPage *page );
static void       on_class_selected( GtkTreeSelection *selection, ofaClassesSet *self );
static void       v_on_update_clicked( GtkButton *button, ofaMainPage *page );

static void
classes_set_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_classes_set_finalize";
	ofaClassesSetPrivate *priv;

	g_return_if_fail( OFA_IS_CLASSES_SET( instance ));

	priv = OFA_CLASSES_SET( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_classes_set_parent_class )->finalize( instance );
}

static void
classes_set_dispose( GObject *instance )
{
	ofaClassesSetPrivate *priv;

	g_return_if_fail( OFA_IS_CLASSES_SET( instance ));

	priv = ( OFA_CLASSES_SET( instance ))->private;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_classes_set_parent_class )->dispose( instance );
}

static void
ofa_classes_set_init( ofaClassesSet *self )
{
	static const gchar *thisfn = "ofa_classes_set_init";

	g_return_if_fail( OFA_IS_CLASSES_SET( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaClassesSetPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
ofa_classes_set_class_init( ofaClassesSetClass *klass )
{
	static const gchar *thisfn = "ofa_classes_set_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = classes_set_dispose;
	G_OBJECT_CLASS( klass )->finalize = classes_set_finalize;

	OFA_MAIN_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_MAIN_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_MAIN_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_MAIN_PAGE_CLASS( klass )->on_update_clicked = v_on_update_clicked;
}

static GtkWidget *
v_setup_view( ofaMainPage *page )
{
	GtkFrame *frame;
	GtkScrolledWindow *scroll;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_margin_left( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_top( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( frame ), 4 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_container_set_border_width( GTK_CONTAINER( scroll ), 4 );
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( scroll ));

	tview = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_vexpand( GTK_WIDGET( tview ), TRUE );
	gtk_tree_view_set_headers_visible( tview, TRUE );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( tview ));
	g_signal_connect(G_OBJECT( tview ), "row-activated", G_CALLBACK( on_row_activated ), page );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( tview, tmodel );
	g_object_unref( tmodel );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Number" ),
			text_cell, "text", COL_NUMBER,
			NULL );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( tview, column );

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_class_selected ), page );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ), ( GtkTreeIterCompareFunc ) on_sort_model, page, NULL );

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	return( GTK_WIDGET( frame ));
}

static GtkWidget *
v_setup_buttons( ofaMainPage *page )
{
	GtkWidget *box;

	box = OFA_MAIN_PAGE_CLASS( ofa_classes_set_parent_class )->setup_buttons( page );

	gtk_widget_set_sensitive( ofa_main_page_get_new_btn( page ), FALSE );
	gtk_widget_set_sensitive( ofa_main_page_get_delete_btn( page ), FALSE );

	return( box );
}

static void
v_init_view( ofaMainPage *page )
{
	ofaClassesSet *self;
	ofoDossier *dossier;
	GList *dataset, *iset;
	ofoClass *class;

	self = OFA_CLASSES_SET( page );
	dossier = ofa_main_page_get_dossier( page );
	dataset = ofo_class_get_dataset( dossier );

	for( iset=dataset ; iset ; iset=iset->next ){

		class = OFO_CLASS( iset->data );
		insert_new_row( self, class, FALSE );
	}

	setup_first_selection( self );
}

static void
insert_new_row( ofaClassesSet *self, ofoClass *class, gboolean with_selection )
{
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;
	gchar *str;

	tview = GTK_TREE_VIEW( ofa_main_page_get_treeview( OFA_MAIN_PAGE( self )));
	tmodel = gtk_tree_view_get_model( tview );

	str = g_strdup_printf( "%d", ofo_class_get_number( class ));

	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( tmodel ),
			&iter,
			-1,
			COL_NUMBER, str,
			COL_LABEL,  ofo_class_get_label( class ),
			COL_OBJECT, class,
			-1 );

	g_free( str );

	/* select the newly added class */
	if( with_selection ){
		path = gtk_tree_model_get_path( tmodel, &iter );
		gtk_tree_view_set_cursor( tview, path, NULL, FALSE );
		gtk_tree_path_free( path );
		gtk_widget_grab_focus( GTK_WIDGET( tview ));
	}
}

static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaClassesSet *self )
{
	gchar *anum, *bnum;
	gint cmp;

	gtk_tree_model_get( tmodel, a, COL_NUMBER, &anum, -1 );
	gtk_tree_model_get( tmodel, b, COL_NUMBER, &bnum, -1 );

	cmp = g_utf8_collate( anum, bnum );

	g_free( anum );
	g_free( bnum );

	return( cmp );
}

static void
setup_first_selection( ofaClassesSet *self )
{
	GtkTreeView *tview;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	tview = GTK_TREE_VIEW( ofa_main_page_get_treeview( OFA_MAIN_PAGE( self )));
	model = gtk_tree_view_get_model( tview );
	if( gtk_tree_model_get_iter_first( model, &iter )){
		select = gtk_tree_view_get_selection( tview );
		gtk_tree_selection_select_iter( select, &iter );
	}

	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}

static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainPage *page )
{
	v_on_update_clicked( NULL, page );
}

static void
on_class_selected( GtkTreeSelection *selection, ofaClassesSet *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoClass *class;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &class, -1 );
		g_object_unref( class );
	}

	gtk_widget_set_sensitive(
			ofa_main_page_get_update_btn( OFA_MAIN_PAGE( self )),
			class && OFO_IS_CLASS( class ));
}

static void
v_on_update_clicked( GtkButton *button, ofaMainPage *page )
{
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoClass *class;

	g_return_if_fail( page && OFA_IS_CLASSES_SET( page ));

	tview = GTK_TREE_VIEW( ofa_main_page_get_treeview( page ));
	select = gtk_tree_view_get_selection( tview );

	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &class, -1 );
		g_object_unref( class );

		if( ofa_class_properties_run(
				ofa_main_page_get_main_window( page ), class )){

			gtk_list_store_set(
					GTK_LIST_STORE( tmodel ),
					&iter,
					COL_LABEL, ofo_class_get_label( class ),
					-1 );
		}
	}

	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}
