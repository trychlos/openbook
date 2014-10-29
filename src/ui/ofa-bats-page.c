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

#include "api/ofo-bat.h"

#include "ui/my-buttons-box.h"
#include "ui/ofa-bat-properties.h"
#include "ui/ofa-bats-page.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-page.h"
#include "ui/ofa-page-prot.h"

/* private instance data
 */
struct _ofaBatsPagePrivate {

	/* UI
	 */
	GtkTreeView *tview;					/* the main treeview of the page */
	GtkWidget   *update_btn;
	GtkWidget   *delete_btn;
};

/* column ordering in the selection listview
 */
enum {
	COL_URI = 0,
	COL_OBJECT,
	N_COLUMNS
};

G_DEFINE_TYPE( ofaBatsPage, ofa_bats_page, OFA_TYPE_PAGE )

static GtkWidget *v_setup_view( ofaPage *page );
static GtkWidget *v_setup_buttons( ofaPage *page );
static void       v_init_view( ofaPage *page );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       insert_new_row( ofaBatsPage *self, ofoBat *bat, gboolean with_selection );
static gint       on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaBatsPage *self );
static void       setup_first_selection( ofaBatsPage *self );
static void       on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page );
static void       on_row_selected( GtkTreeSelection *selection, ofaBatsPage *self );
static void       v_on_button_clicked( ofaPage *page, guint button_id );
static void       on_update_clicked( ofaBatsPage *page );
static void       on_delete_clicked( ofaBatsPage *page );
static gboolean   delete_confirmed( ofaBatsPage *self, ofoBat *bat );

static void
bats_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_bats_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BATS_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bats_page_parent_class )->finalize( instance );
}

static void
bats_page_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_BATS_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bats_page_parent_class )->dispose( instance );
}

static void
ofa_bats_page_init( ofaBatsPage *self )
{
	static const gchar *thisfn = "ofa_bats_page_init";

	g_return_if_fail( OFA_IS_BATS_PAGE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_BATS_PAGE, ofaBatsPagePrivate );
}

static void
ofa_bats_page_class_init( ofaBatsPageClass *klass )
{
	static const gchar *thisfn = "ofa_bats_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bats_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = bats_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;
	OFA_PAGE_CLASS( klass )->on_button_clicked = v_on_button_clicked;

	g_type_class_add_private( klass, sizeof( ofaBatsPagePrivate ));
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	ofaBatsPagePrivate *priv;
	GtkFrame *frame;
	GtkScrolledWindow *scroll;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = OFA_BATS_PAGE( page )->priv;

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
	priv->tview = tview;

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
	ofaBatsPagePrivate *priv;
	GtkWidget *buttons_box;
	GtkWidget *btn_new;

	buttons_box = OFA_PAGE_CLASS( ofa_bats_page_parent_class )->setup_buttons( page );
	btn_new = ofa_page_get_button_by_id( page, BUTTONS_BOX_NEW );
	gtk_widget_set_sensitive( btn_new, FALSE );

	priv = OFA_BATS_PAGE( page )->priv;
	priv->update_btn = ofa_page_get_button_by_id( page, BUTTONS_BOX_PROPERTIES );
	priv->delete_btn = ofa_page_get_button_by_id( page, BUTTONS_BOX_DELETE );

	return( buttons_box );
}

static void
v_init_view( ofaPage *page )
{
	ofaBatsPage *self;
	ofoDossier *dossier;
	GList *dataset, *iset;
	ofoBat *bat;

	self = OFA_BATS_PAGE( page );
	dossier = ofa_page_get_dossier( page );
	dataset = ofo_bat_get_dataset( dossier );

	for( iset=dataset ; iset ; iset=iset->next ){

		bat = OFO_BAT( iset->data );
		insert_new_row( self, bat, FALSE );
	}

	setup_first_selection( self );
}

static void
insert_new_row( ofaBatsPage *self, ofoBat *bat, gboolean with_selection )
{
	ofaBatsPagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;

	priv = self->priv;
	tmodel = gtk_tree_view_get_model( priv->tview );
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
		gtk_tree_view_set_cursor( priv->tview, path, NULL, FALSE );
		gtk_tree_path_free( path );
		gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
	}
}

/*
 * list of imported BAT is sorted on import timestamp
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaBatsPage *self )
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
setup_first_selection( ofaBatsPage *self )
{
	ofaBatsPagePrivate *priv;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	priv = self->priv;
	model = gtk_tree_view_get_model( priv->tview );
	if( gtk_tree_model_get_iter_first( model, &iter )){
		select = gtk_tree_view_get_selection( priv->tview );
		gtk_tree_selection_select_iter( select, &iter );
	}

	gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
}

static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page )
{
	on_update_clicked( OFA_BATS_PAGE( page ));
}

static void
on_row_selected( GtkTreeSelection *selection, ofaBatsPage *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBat *bat;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &bat, -1 );
		g_object_unref( bat );
	}

	gtk_widget_set_sensitive(
			self->priv->update_btn,
			bat && OFO_IS_BAT( bat ));

	gtk_widget_set_sensitive(
			self->priv->delete_btn,
			bat && OFO_IS_BAT( bat ) && ofo_bat_is_deletable( bat ));
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_BATS_PAGE( page ), NULL );

	return( GTK_WIDGET( OFA_BATS_PAGE( page )->priv->tview ));
}

static void
v_on_button_clicked( ofaPage *page, guint button_id )
{
	g_return_if_fail( page && OFA_IS_BATS_PAGE( page ));

	switch( button_id ){
		case BUTTONS_BOX_PROPERTIES:
			on_update_clicked( OFA_BATS_PAGE( page ));
			break;
		case BUTTONS_BOX_DELETE:
			on_delete_clicked( OFA_BATS_PAGE( page ));
			break;
	}
}

/*
 * only notes can be updated
 */
static void
on_update_clicked( ofaBatsPage *page )
{
	ofaBatsPagePrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBat *bat;

	priv = page->priv;
	select = gtk_tree_view_get_selection( priv->tview );

	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &bat, -1 );
		g_object_unref( bat );
		ofa_bat_properties_run( ofa_page_get_main_window( OFA_PAGE( page )), bat );
	}

	gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
}

static void
on_delete_clicked( ofaBatsPage *page )
{
	ofaBatsPagePrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBat *bat;

	priv = page->priv;
	select = gtk_tree_view_get_selection( priv->tview );

	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &bat, -1 );
		g_object_unref( bat );

		g_return_if_fail( ofo_bat_is_deletable( bat ));

		if( delete_confirmed( page, bat ) &&
				ofo_bat_delete( bat, ofa_page_get_dossier( OFA_PAGE( page )))){

			/* remove the row from the tmodel
			 * this will cause an automatic new selection */
			gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
		}
	}

	gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
}

static gboolean
delete_confirmed( ofaBatsPage *self, ofoBat *bat )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup( _( "Are you sure you want delete this imported BAT file\n"
			"(All the corresponding lines will be deleted too) ?" ));

	delete_ok = ofa_main_window_confirm_deletion(
						ofa_page_get_main_window( OFA_PAGE( self )), msg );

	g_free( msg );

	return( delete_ok );
}
