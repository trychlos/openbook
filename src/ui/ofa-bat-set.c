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

#include "ui/ofa-page.h"
#include "ui/ofa-bat-properties.h"
#include "ui/ofa-bat-set.h"
#include "api/ofo-bat.h"

/* private instance data
 */
struct _ofaBatSetPrivate {
	gboolean dispose_has_run;
};

/* column ordering in the selection listview
 */
enum {
	COL_URI = 0,
	COL_OBJECT,
	N_COLUMNS
};

G_DEFINE_TYPE( ofaBatSet, ofa_bat_set, OFA_TYPE_PAGE )

static GtkWidget *v_setup_view( ofaPage *page );
static GtkWidget *v_setup_buttons( ofaPage *page );
static void       v_init_view( ofaPage *page );
static void       insert_new_row( ofaBatSet *self, ofoBat *bat, gboolean with_selection );
static gint       on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaBatSet *self );
static void       setup_first_selection( ofaBatSet *self );
static void       on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page );
static void       on_row_selected( GtkTreeSelection *selection, ofaBatSet *self );
/*static void       v_on_new_clicked( GtkButton *button, ofaPage *page );*/
static void       v_on_update_clicked( GtkButton *button, ofaPage *page );
static void       v_on_delete_clicked( GtkButton *button, ofaPage *page );
static gboolean   delete_confirmed( ofaBatSet *self, ofoBat *bat );

static void
bat_set_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_bat_set_finalize";
	ofaBatSetPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BAT_SET( instance ));

	priv = OFA_BAT_SET( instance )->private;

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_set_parent_class )->finalize( instance );
}

static void
bat_set_dispose( GObject *instance )
{
	ofaBatSetPrivate *priv;

	g_return_if_fail( instance && OFA_IS_BAT_SET( instance ));

	priv = ( OFA_BAT_SET( instance ))->private;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_set_parent_class )->dispose( instance );
}

static void
ofa_bat_set_init( ofaBatSet *self )
{
	static const gchar *thisfn = "ofa_bat_set_init";

	g_return_if_fail( OFA_IS_BAT_SET( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaBatSetPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
ofa_bat_set_class_init( ofaBatSetClass *klass )
{
	static const gchar *thisfn = "ofa_bat_set_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_set_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_set_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_PAGE_CLASS( klass )->on_update_clicked = v_on_update_clicked;
	OFA_PAGE_CLASS( klass )->on_delete_clicked = v_on_delete_clicked;
}

static GtkWidget *
v_setup_view( ofaPage *page )
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
			G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( tview, tmodel );
	g_object_unref( tmodel );

	text_cell = gtk_cell_renderer_text_new();
	g_object_set( G_OBJECT( text_cell ), "ellipsize", PANGO_ELLIPSIZE_START, NULL );
	column = gtk_tree_view_column_new_with_attributes(
			_( "URI" ),
			text_cell, "text", COL_URI,
			NULL );
	gtk_tree_view_append_column( tview, column );

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_row_selected ), page );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ), ( GtkTreeIterCompareFunc ) on_sort_model, page, NULL );

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	return( GTK_WIDGET( frame ));
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	GtkWidget *buttons_box;

	buttons_box = OFA_PAGE_CLASS( ofa_bat_set_parent_class )->setup_buttons( page );

	gtk_widget_set_sensitive( ofa_page_get_new_btn( page ), FALSE );

	return( buttons_box );
}

static void
v_init_view( ofaPage *page )
{
	ofaBatSet *self;
	ofoDossier *dossier;
	GList *dataset, *iset;
	ofoBat *bat;

	self = OFA_BAT_SET( page );
	dossier = ofa_page_get_dossier( page );
	dataset = ofo_bat_get_dataset( dossier );

	for( iset=dataset ; iset ; iset=iset->next ){

		bat = OFO_BAT( iset->data );
		insert_new_row( self, bat, FALSE );
	}

	setup_first_selection( self );
}

static void
insert_new_row( ofaBatSet *self, ofoBat *bat, gboolean with_selection )
{
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;

	tview = GTK_TREE_VIEW( ofa_page_get_treeview( OFA_PAGE( self )));
	tmodel = gtk_tree_view_get_model( tview );
	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( tmodel ),
			&iter,
			-1,
			COL_URI,    ofo_bat_get_uri( bat ),
			COL_OBJECT, bat,
			-1 );

	/* select the newly added bat */
	if( with_selection ){
		path = gtk_tree_model_get_path( tmodel, &iter );
		gtk_tree_view_set_cursor( tview, path, NULL, FALSE );
		gtk_tree_path_free( path );
		gtk_widget_grab_focus( GTK_WIDGET( tview ));
	}
}

/*
 * list of imported BAT is sorted on import timestamp
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaBatSet *self )
{
	ofoBat *aobj, *bobj;
	const GTimeVal *astamp, *bstamp;

	gtk_tree_model_get( tmodel, a, COL_OBJECT, &aobj, -1 );
	g_object_unref( aobj );
	astamp = ofo_bat_get_upd_stamp( aobj );

	gtk_tree_model_get( tmodel, b, COL_OBJECT, &bobj, -1 );
	g_object_unref( bobj );
	bstamp = ofo_bat_get_upd_stamp( bobj );

	return( astamp->tv_sec < bstamp->tv_sec ? -1 : ( astamp->tv_sec > bstamp->tv_sec ? 1 : 0 ));
}

static void
setup_first_selection( ofaBatSet *self )
{
	GtkTreeView *tview;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	tview = GTK_TREE_VIEW( ofa_page_get_treeview( OFA_PAGE( self )));
	model = gtk_tree_view_get_model( tview );
	if( gtk_tree_model_get_iter_first( model, &iter )){
		select = gtk_tree_view_get_selection( tview );
		gtk_tree_selection_select_iter( select, &iter );
	}

	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}

static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page )
{
	v_on_update_clicked( NULL, page );
}

static void
on_row_selected( GtkTreeSelection *selection, ofaBatSet *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBat *bat;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &bat, -1 );
		g_object_unref( bat );
	}

	gtk_widget_set_sensitive(
			ofa_page_get_update_btn( OFA_PAGE( self )),
			bat && OFO_IS_BAT( bat ));

	gtk_widget_set_sensitive(
			ofa_page_get_delete_btn( OFA_PAGE( self )),
			bat && OFO_IS_BAT( bat ) && ofo_bat_is_deletable( bat ));
}

#if 0
static void
v_on_new_clicked( GtkButton *button, ofaPage *page )
{
	ofoBat *bat;

	g_return_if_fail( page && OFA_IS_BAT_SET( page ));

	bat = ofo_bat_new();

	if( ofa_bat_properties_run(
			ofa_page_get_main_window( page ), bat )){

		insert_new_row( OFA_BAT_SET( page ), bat, TRUE );

	} else {
		g_object_unref( bat );
	}
}
#endif

/*
 * only notes can be updated
 */
static void
v_on_update_clicked( GtkButton *button, ofaPage *page )
{
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBat *bat;

	g_return_if_fail( page && OFA_IS_BAT_SET( page ));

	tview = GTK_TREE_VIEW( ofa_page_get_treeview( page ));
	select = gtk_tree_view_get_selection( tview );

	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &bat, -1 );
		g_object_unref( bat );
		ofa_bat_properties_run( ofa_page_get_main_window( page ), bat );
	}

	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}

static void
v_on_delete_clicked( GtkButton *button, ofaPage *page )
{
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBat *bat;

	g_return_if_fail( page && OFA_IS_BAT_SET( page ));

	tview = GTK_TREE_VIEW( ofa_page_get_treeview( page ));
	select = gtk_tree_view_get_selection( tview );

	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &bat, -1 );
		g_object_unref( bat );

		g_return_if_fail( ofo_bat_is_deletable( bat ));

		if( delete_confirmed( OFA_BAT_SET( page ), bat ) &&
				ofo_bat_delete( bat, ofa_page_get_dossier( page ))){

			/* remove the row from the tmodel
			 * this will cause an automatic new selection */
			gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
		}
	}

	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}

static gboolean
delete_confirmed( ofaBatSet *self, ofoBat *bat )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup( _( "Are you sure you want delete this imported BAT file\n"
			"(All the corresponding lines will be deleted too) ?" ));

	delete_ok = ofa_page_delete_confirmed( OFA_PAGE( self ), msg );

	g_free( msg );

	return( delete_ok );
}
