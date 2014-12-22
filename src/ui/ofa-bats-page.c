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
#include "api/my-double.h"
#include "api/ofo-bat.h"

#include "ui/ofa-bat-properties.h"
#include "ui/ofa-bats-page.h"
#include "ui/ofa-buttons-box.h"
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
	GtkWidget   *import_btn;
};

/* column ordering in the selection listview
 */
enum {
	COL_BEGIN = 0,
	COL_END,
	COL_COUNT,
	COL_FORMAT,
	COL_RIB,
	COL_SOLDE,
	COL_CURRENCY,
	COL_OBJECT,
	N_COLUMNS
};

G_DEFINE_TYPE( ofaBatsPage, ofa_bats_page, OFA_TYPE_PAGE )

static GtkWidget *v_setup_view( ofaPage *page );
static gboolean   on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaBatsPage *self );
static GtkWidget *v_setup_buttons( ofaPage *page );
static void       v_init_view( ofaPage *page );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       insert_new_row( ofaBatsPage *self, ofoBat *bat, gboolean with_selection );
static gint       on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaBatsPage *self );
static void       setup_first_selection( ofaBatsPage *self );
static void       on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page );
static void       on_row_selected( GtkTreeSelection *selection, ofaBatsPage *self );
static void       on_update_clicked( GtkButton *button, ofaBatsPage *page );
static void       on_delete_clicked( GtkButton *button, ofaBatsPage *page );
static void       on_import_clicked( GtkButton *button, ofaBatsPage *page );
static void       try_to_delete_current_row( ofaBatsPage *page );
static gboolean   delete_confirmed( ofaBatsPage *self, ofoBat *bat );
static void       do_delete( ofaBatsPage *page, ofoBat *bat, GtkTreeModel *tmodel, GtkTreeIter *iter );

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
	g_signal_connect(
			G_OBJECT( tview ), "row-activated", G_CALLBACK( on_row_activated ), page );
	g_signal_connect(
			G_OBJECT( tview ), "key-press-event", G_CALLBACK( on_tview_key_pressed ), page );
	priv->tview = tview;

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* begin, end, format */
			G_TYPE_STRING, 									/* count */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* rib, solde, currency */
			G_TYPE_OBJECT ));								/* the ofoBat object itself */
	gtk_tree_view_set_model( tview, tmodel );
	g_object_unref( tmodel );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Begin" ), text_cell, "text", COL_BEGIN, NULL );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "End" ), text_cell, "text", COL_END, NULL );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Count" ), text_cell, "text", COL_COUNT, NULL );
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Format" ), text_cell, "text", COL_FORMAT, NULL );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "RIB" ), text_cell, "text", COL_RIB, NULL );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Solde" ), text_cell, "text", COL_SOLDE, NULL );
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Cur." ), text_cell, "text", COL_CURRENCY, NULL );
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

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaBatsPage *self )
{
	gboolean stop;

	stop = FALSE;

	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Delete ){
			try_to_delete_current_row( self );
		}
	}

	return( stop );
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	ofaBatsPagePrivate *priv;
	ofaButtonsBox *buttons_box;

	priv = OFA_BATS_PAGE( page )->priv;

	buttons_box = ofa_buttons_box_new();

	ofa_buttons_box_add_spacer( buttons_box );
	ofa_buttons_box_add_button(
			buttons_box, BUTTON_NEW, FALSE, NULL, NULL );
	priv->update_btn = ofa_buttons_box_add_button(
			buttons_box, BUTTON_PROPERTIES, FALSE, G_CALLBACK( on_update_clicked ), page );
	priv->delete_btn = ofa_buttons_box_add_button(
			buttons_box, BUTTON_DELETE, FALSE, G_CALLBACK( on_delete_clicked ), page );

	ofa_buttons_box_add_spacer( buttons_box );
	priv->import_btn = ofa_buttons_box_add_button(
			buttons_box, BUTTON_IMPORT, TRUE, G_CALLBACK( on_import_clicked ), page );

	return( ofa_buttons_box_get_top_widget( buttons_box ));
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
	gchar *sbegin, *send, *scount, *samount;

	priv = self->priv;
	tmodel = gtk_tree_view_get_model( priv->tview );

	sbegin = my_date_to_str( ofo_bat_get_begin( bat ), MY_DATE_DMYY );
	send = my_date_to_str( ofo_bat_get_end( bat ), MY_DATE_DMYY );
	scount = g_strdup_printf( "%u", ofo_bat_get_count( bat ));
	samount = my_double_to_str( ofo_bat_get_solde( bat ));

	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( tmodel ),
			&iter,
			-1,
			COL_BEGIN,    sbegin,
			COL_END,      send,
			COL_COUNT,    scount,
			COL_FORMAT,   ofo_bat_get_format( bat ),
			COL_RIB,      ofo_bat_get_rib( bat ),
			COL_SOLDE,    samount,
			COL_CURRENCY, ofo_bat_get_currency( bat ),
			COL_OBJECT,   bat,
			-1 );

	g_free( samount );
	g_free( scount );
	g_free( send );
	g_free( sbegin );

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
	on_update_clicked( NULL, OFA_BATS_PAGE( page ));
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

static ofoBat *
get_current_selection( ofaBatsPage *page, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	ofaBatsPagePrivate *priv;
	GtkTreeSelection *select;
	ofoBat *bat;

	priv = page->priv;
	bat = NULL;

	select = gtk_tree_view_get_selection( priv->tview );
	if( gtk_tree_selection_get_selected( select, tmodel, iter )){

		gtk_tree_model_get( *tmodel, iter, COL_OBJECT, &bat, -1 );
		g_object_unref( bat );
	}

	return( bat );
}

/*
 * only notes can be updated
 */
static void
on_update_clicked( GtkButton *button, ofaBatsPage *page )
{
	ofaBatsPagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBat *bat;

	priv = page->priv;
	bat = get_current_selection( page, &tmodel, &iter );
	if( bat ){
		ofa_bat_properties_run( ofa_page_get_main_window( OFA_PAGE( page )), bat );
	}

	gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
}

static void
on_delete_clicked( GtkButton *button, ofaBatsPage *page )
{
	ofaBatsPagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBat *bat;

	priv = page->priv;
	bat = get_current_selection( page, &tmodel, &iter );
	if( bat ){
		g_return_if_fail( ofo_bat_is_deletable( bat ));
		do_delete( page, bat, tmodel, &iter );
		gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
	}
}

static void
try_to_delete_current_row( ofaBatsPage *page )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBat *bat;

	bat = get_current_selection( page, &tmodel, &iter );
	if( bat && ofo_bat_is_deletable( bat )){
		do_delete( page, bat, tmodel, &iter );
	}
}

static void
do_delete( ofaBatsPage *page, ofoBat *bat, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	g_return_if_fail( ofo_bat_is_deletable( bat ));

	if( delete_confirmed( page, bat ) &&
			ofo_bat_delete( bat, ofa_page_get_dossier( OFA_PAGE( page )))){

		/* remove the row from the tmodel
		 * this will cause an automatic new selection */
		gtk_list_store_remove( GTK_LIST_STORE( tmodel ), iter );
	}
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

static void
on_import_clicked( GtkButton *button, ofaBatsPage *page )
{
}
