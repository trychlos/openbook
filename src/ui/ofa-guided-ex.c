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

#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"

#include "core/ofa-guided-input-bin.h"
#include "core/ofa-main-window.h"

#include "ui/ofa-guided-ex.h"

/* private instance data
 */
struct _ofaGuidedExPrivate {

	/* internals
	 */
	ofaHub               *hub;
	GList                *hub_handlers;
	const ofoOpeTemplate *model;			/* model */
	ofaGuidedInputBin    *input_bin;

	/* UI - the pane
	 */
	GtkWidget            *pane;

	/* UI - left part treeview selection of the entry model
	 */
	GtkTreeView          *left_tview;
	GtkButton            *left_select;

	/* UI - right part guided input
	 *      most if not all elements are taken from ofa-guided-input-bin.ui
	 *      dialog box definition
	 */
	GtkWidget            *ok_btn;
};

/* columns in the left tree view which handles the entry models
 */
enum {
	LEFT_COL_MNEMO = 0,
	LEFT_COL_LABEL,
	LEFT_COL_OBJECT,
	LEFT_N_COLUMNS
};

static GtkWidget *v_setup_view( ofaPage *page );
static void       pane_restore_position( GtkWidget *pane );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static GtkWidget *setup_view_left( ofaGuidedEx *self );
static GtkWidget *setup_view_right( ofaGuidedEx *self );
static GtkWidget *setup_left_treeview( ofaGuidedEx *self );
static gint       on_left_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaGuidedEx *self );
static gchar     *get_sort_key( GtkTreeModel *tmodel, GtkTreeIter *iter );
static void       on_left_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaGuidedEx *self );
static void       init_left_view( ofaGuidedEx *self, GtkWidget *child );
static void       on_left_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaGuidedEx *self );
static void       on_left_row_selected( GtkTreeSelection *selection, ofaGuidedEx *self );
static gboolean   on_left_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaGuidedEx *self );
static void       left_collapse_node( ofaGuidedEx *self, GtkWidget *widget );
static void       left_expand_node( ofaGuidedEx *self, GtkWidget *widget );
static void       enable_left_select( ofaGuidedEx *self );
static gboolean   is_left_select_enableable( ofaGuidedEx *self );
static void       on_left_select_clicked( GtkButton *button, ofaGuidedEx *self );
static void       select_model( ofaGuidedEx *self );
static void       insert_left_ledger_row( ofaGuidedEx *self, ofoLedger *ledger );
static void       update_left_ledger_row( ofaGuidedEx *self, ofoLedger *ledger, const gchar *prev_id );
static void       remove_left_ledger_row( ofaGuidedEx *self, ofoLedger *ledger );
static gboolean   find_left_ledger_by_mnemo( ofaGuidedEx *self, const gchar *mnemo, GtkTreeModel **tmodel, GtkTreeIter *iter );
static void       insert_left_model_row( ofaGuidedEx *self, ofoOpeTemplate *model );
static void       update_left_model_row( ofaGuidedEx *self, ofoOpeTemplate *model, const gchar *prev_id );
static void       remove_left_model_row( ofaGuidedEx *self, ofoOpeTemplate *model );
static gboolean   find_left_model_by_mnemo( ofaGuidedEx *self, const gchar *mnemo, GtkTreeModel **tmodel, GtkTreeIter *iter );
static void       on_right_piece_changed( ofaGuidedInputBin *bin, gboolean ok, ofaGuidedEx *self );
static void       on_right_ok( GtkButton *button, ofaGuidedEx *self );
static void       on_right_cancel( GtkButton *button, ofaGuidedEx *self );
static void       on_hub_new_object( const ofoDossier *dossier, const ofoBase *object, ofaGuidedEx *self );
static void       on_hub_updated_object( const ofoDossier *dossier, const ofoBase *object, const gchar *prev_id, ofaGuidedEx *self );
static void       on_hub_deleted_object( const ofoDossier *dossier, const ofoBase *object, ofaGuidedEx *self );
static void       on_hub_reload_dataset( const ofoDossier *dossier, GType type, ofaGuidedEx *self );
static void       on_page_removed( ofaGuidedEx *page, GtkWidget *page_w, guint page_n, void *empty );
static void       pane_save_position( GtkWidget *pane );

G_DEFINE_TYPE_EXTENDED( ofaGuidedEx, ofa_guided_ex, OFA_TYPE_PAGE, 0,
		G_ADD_PRIVATE( ofaGuidedEx ))

static void
guided_ex_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_guided_ex_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_GUIDED_EX( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_guided_ex_parent_class )->finalize( instance );
}

static void
guided_ex_dispose( GObject *instance )
{
	ofaGuidedExPrivate *priv;

	g_return_if_fail( instance && OFA_IS_GUIDED_EX( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = ofa_guided_ex_get_instance_private( OFA_GUIDED_EX( instance ));

		ofa_hub_disconnect_handlers( priv->hub, priv->hub_handlers );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_guided_ex_parent_class )->dispose( instance );
}

static void
ofa_guided_ex_init( ofaGuidedEx *self )
{
	static const gchar *thisfn = "ofa_guided_ex_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_GUIDED_EX( self ));
}

static void
ofa_guided_ex_class_init( ofaGuidedExClass *klass )
{
	static const gchar *thisfn = "ofa_guided_ex_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = guided_ex_dispose;
	G_OBJECT_CLASS( klass )->finalize = guided_ex_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_guided_ex_v_setup_view";
	ofaGuidedExPrivate *priv;
	GtkWidget *pane, *child;
	gulong handler;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_guided_ex_get_instance_private( OFA_GUIDED_EX( page ));

	priv->hub = ofa_page_get_hub( page );

	pane = gtk_paned_new( GTK_ORIENTATION_HORIZONTAL );
	child = setup_view_left( OFA_GUIDED_EX( page ));
	gtk_paned_pack1( GTK_PANED( pane ), child, FALSE, FALSE );
	gtk_widget_set_size_request( child, 200, -1 );
	child = setup_view_right( OFA_GUIDED_EX( page ));
	gtk_paned_pack2( GTK_PANED( pane ), child, TRUE, FALSE );
	priv->pane = pane;
	pane_restore_position( pane );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_NEW, G_CALLBACK( on_hub_new_object ), page );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_UPDATED, G_CALLBACK( on_hub_updated_object ), page );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_DELETED, G_CALLBACK( on_hub_deleted_object ), page );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_RELOAD, G_CALLBACK( on_hub_reload_dataset ), page );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	g_signal_connect( page, "page-removed", G_CALLBACK( on_page_removed ), NULL );

	init_left_view( OFA_GUIDED_EX( page ), gtk_paned_get_child1( GTK_PANED( priv->pane )));

	return( pane );
}

static void
pane_restore_position( GtkWidget *pane )
{
	gint pos;

	pos = ofa_settings_user_get_uint( "GuidedInputExDlg-pane" );
	if( pos == 0 ){
		pos = 150;
	}
	g_debug( "ofa_guided_ex_pane_restore_position: pos=%d", pos );
	gtk_paned_set_position( GTK_PANED( pane ), pos );
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaGuidedExPrivate *priv;

	g_return_val_if_fail( page && OFA_IS_GUIDED_EX( page ), NULL );

	priv = ofa_guided_ex_get_instance_private( OFA_GUIDED_EX( page ));

	return( GTK_WIDGET( priv->left_tview ));
}

/*
 * the left pane is a treeview whose level 0 are the ledgers, and
 * level 1 the entry models defined on the corresponding ledger
 */
static GtkWidget *
setup_view_left( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;
	GtkWidget *frame, *grid, *tview, *box, *button;

	priv = ofa_guided_ex_get_instance_private( self );

	frame = gtk_frame_new( _( " Per ledger " ));
	my_utils_widget_set_margins( frame, 0, 4, 4, 2 );
	gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_IN );

	grid = gtk_grid_new();
	gtk_grid_set_row_spacing( GTK_GRID( grid ), 3 );
	gtk_container_add( GTK_CONTAINER( frame ), grid );

	tview = setup_left_treeview( self );
	gtk_grid_attach( GTK_GRID( grid ), tview, 0, 0, 1, 1 );

	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 4 );
	gtk_grid_attach( GTK_GRID( grid ), box, 0, 1, 1, 1 );

	button = gtk_button_new_with_mnemonic( _( "_Select" ));
	my_utils_widget_set_margins( button, 0, 4, 4, 4 );
	gtk_box_pack_end( GTK_BOX( box ), button, FALSE, FALSE, 0 );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_left_select_clicked ), self );
	priv->left_select = GTK_BUTTON( button );

	enable_left_select( self );

	return( GTK_WIDGET( frame ));
}

/*
 * note that we do not have any current ofoOpeTemplate at this time
 */
static GtkWidget *
setup_view_right( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;
	GtkWidget *grid, *box, *button, *image;

	priv = ofa_guided_ex_get_instance_private( self );

	grid = gtk_grid_new();

	priv->input_bin = ofa_guided_input_bin_new( ofa_page_get_main_window( OFA_PAGE( self )));
	my_utils_widget_set_margins( GTK_WIDGET( priv->input_bin ), 0, 0, 2, 4 );
	gtk_grid_attach( GTK_GRID( grid ), GTK_WIDGET( priv->input_bin ), 0, 0, 1, 1 );

	g_signal_connect( priv->input_bin, "ofa-changed", G_CALLBACK( on_right_piece_changed ), self );

	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 4 );
	gtk_grid_attach( GTK_GRID( grid ), box, 0, 1, 1, 1 );

	button = gtk_button_new_with_mnemonic( _( "_Validate" ));
	my_utils_widget_set_margins( button, 0, 4, 0, 6 );
	gtk_box_pack_end( GTK_BOX( box ), button, FALSE, FALSE, 0 );
	image = gtk_image_new_from_icon_name( "gtk-ok", GTK_ICON_SIZE_BUTTON );
	gtk_button_set_image( GTK_BUTTON( button ), image );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_right_ok ), self );
	priv->ok_btn = button;

	button = gtk_button_new_with_mnemonic( _( "  _Reset  " ));
	my_utils_widget_set_margins( button, 0, 4, 0, 4 );
	gtk_box_pack_end( GTK_BOX( box ), button, FALSE, FALSE, 0 );
	image = gtk_image_new_from_icon_name( "gtk-cancel", GTK_ICON_SIZE_BUTTON );
	gtk_button_set_image( GTK_BUTTON( button ), image );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_right_cancel ), self );

	return( grid );
}

/*
 * the left pane is a treeview whose level 0 are the ledgers, and
 * level 1 the entry models defined on the corresponding ledger
 */
static GtkWidget *
setup_left_treeview( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;
	GtkScrolledWindow *scroll;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = ofa_guided_ex_get_instance_private( self );

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_container_set_border_width( GTK_CONTAINER( scroll ), 4 );
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );

	tview = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_hexpand( GTK_WIDGET( tview ), TRUE );
	gtk_widget_set_vexpand( GTK_WIDGET( tview ), TRUE );
	gtk_tree_view_set_headers_visible( tview, FALSE );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( tview ));
	g_signal_connect(G_OBJECT( tview ), "row-activated", G_CALLBACK( on_left_row_activated ), self );
	g_signal_connect( G_OBJECT( tview ), "key-press-event", G_CALLBACK( on_left_key_pressed ), self );
	priv->left_tview = tview;

	tmodel = GTK_TREE_MODEL( gtk_tree_store_new(
			LEFT_N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( tview, tmodel );
	g_object_unref( tmodel );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Mnemonic" ),
			text_cell, "text", LEFT_COL_MNEMO,
			NULL );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_left_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", LEFT_COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_left_cell_data_func, self, NULL );

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_left_row_selected ), self );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ), ( GtkTreeIterCompareFunc ) on_left_sort_model, self, NULL );

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	return( GTK_WIDGET( scroll ));
}

static gint
on_left_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaGuidedEx *self )
{
	gchar *a_key, *b_key;
	gint cmp;

	a_key = get_sort_key( tmodel, a );
	b_key = get_sort_key( tmodel, b );

	cmp = g_utf8_collate( a_key, b_key );

	g_free( a_key );
	g_free( b_key );

	return( cmp );
}

static gchar *
get_sort_key( GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gchar *key;
	ofoBase *obj;

	key = NULL;
	gtk_tree_model_get( tmodel, iter, LEFT_COL_OBJECT, &obj, -1 );
	g_object_unref( obj );

	if( OFO_IS_LEDGER( obj )){
		key = g_strdup( ofo_ledger_get_mnemo( OFO_LEDGER( obj )));
	} else {
		g_return_val_if_fail( OFO_IS_OPE_TEMPLATE( obj ), NULL );
		key = g_strdup_printf( "%s%s",
				ofo_ope_template_get_ledger( OFO_OPE_TEMPLATE( obj )),
				ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( obj )));
	}

	return( key );
}

/*
 * display yellow background the ledger rows
 */
static void
on_left_cell_data_func( GtkTreeViewColumn *tcolumn,
							GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
							ofaGuidedEx *self )
{
	GObject *object;
	GdkRGBA color;

	g_return_if_fail( GTK_IS_CELL_RENDERER_TEXT( cell ));

	gtk_tree_model_get( tmodel, iter,
			LEFT_COL_OBJECT, &object,
			-1 );
	g_object_unref( object );

	g_object_set( G_OBJECT( cell ),
						"style-set", FALSE,
						"background-set", FALSE,
						NULL );

	g_return_if_fail( OFO_IS_LEDGER( object ) || OFO_IS_OPE_TEMPLATE( object ));

	if( OFO_IS_LEDGER( object )){
		gdk_rgba_parse( &color, "#ffffb0" );
		g_object_set( G_OBJECT( cell ), "background-rgba", &color, NULL );
		g_object_set( G_OBJECT( cell ), "style", PANGO_STYLE_ITALIC, NULL );
	}
}

static void
init_left_view( ofaGuidedEx *self, GtkWidget *child )
{
	ofaGuidedExPrivate *priv;
	GList *dataset, *ise;

	priv = ofa_guided_ex_get_instance_private( self );

	dataset = ofo_ledger_get_dataset( priv->hub );
	for( ise=dataset ; ise ; ise=ise->next ){
		insert_left_ledger_row( self, OFO_LEDGER( ise->data ));
	}

	dataset = ofo_ope_template_get_dataset( priv->hub );
	for( ise=dataset ; ise ; ise=ise->next ){
		insert_left_model_row( self, OFO_OPE_TEMPLATE( ise->data ));
	}
}

static void
on_left_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaGuidedEx *self )
{
	/* select the operation template if the current selected row is an
	 * operation template */
	if( is_left_select_enableable( self )){
		select_model( self );

	/* else collapse/expand the ledger */
	} else if( gtk_tree_view_row_expanded( view, path )){
		gtk_tree_view_collapse_row( view, path );

	} else {
		gtk_tree_view_expand_row( view, path, TRUE );
	}
}

static void
on_left_row_selected( GtkTreeSelection *selection, ofaGuidedEx *self )
{
	enable_left_select( self );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 *
 * Handles left and right arrows to expand/collapse nodes
 */
static gboolean
on_left_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaGuidedEx *self )
{
	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Left ){
			left_collapse_node( self, widget );
		} else if( event->keyval == GDK_KEY_Right ){
			left_expand_node( self, widget );
		}
	}

	return( FALSE );
}

static void
left_collapse_node( ofaGuidedEx *self, GtkWidget *widget )
{
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter, parent;
	GtkTreePath *path;

	if( GTK_IS_TREE_VIEW( widget )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ));
		if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

			if( gtk_tree_model_iter_has_child( tmodel, &iter )){
				path = gtk_tree_model_get_path( tmodel, &iter );
				gtk_tree_view_collapse_row( GTK_TREE_VIEW( widget ), path );
				gtk_tree_path_free( path );

			} else if( gtk_tree_model_iter_parent( tmodel, &parent, &iter )){
				path = gtk_tree_model_get_path( tmodel, &parent );
				gtk_tree_view_collapse_row( GTK_TREE_VIEW( widget ), path );
				gtk_tree_path_free( path );
			}
		}
	}
}

static void
left_expand_node( ofaGuidedEx *self, GtkWidget *widget )
{
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;

	if( GTK_IS_TREE_VIEW( widget )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ));
		if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

			if( gtk_tree_model_iter_has_child( tmodel, &iter )){
				path = gtk_tree_model_get_path( tmodel, &iter );
				gtk_tree_view_expand_row( GTK_TREE_VIEW( widget ), path, FALSE );
				gtk_tree_path_free( path );
			}
		}
	}
}

static void
enable_left_select( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;

	priv = ofa_guided_ex_get_instance_private( self );

	gtk_widget_set_sensitive(
			GTK_WIDGET( priv->left_select ), is_left_select_enableable( self ));
}

/*
 * return %TRUE if the current selection is an operation template
 */
static gboolean
is_left_select_enableable( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBase *object;
	gboolean ok;

	priv = ofa_guided_ex_get_instance_private( self );
	ok = FALSE;

	select = gtk_tree_view_get_selection( priv->left_tview );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, LEFT_COL_OBJECT, &object, -1 );
		g_object_unref( object );
		ok = OFO_IS_OPE_TEMPLATE( object );
	}

	return( ok );
}

static void
on_left_select_clicked( GtkButton *button, ofaGuidedEx *self )
{
	select_model( self );
}

static void
select_model( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBase *object;
	gboolean ok;

	priv = ofa_guided_ex_get_instance_private( self );

	object = NULL;
	select = gtk_tree_view_get_selection( priv->left_tview );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, LEFT_COL_OBJECT, &object, -1 );
		g_object_unref( object );
	}
	g_return_if_fail( object && OFO_IS_OPE_TEMPLATE( object ));

	ofa_guided_input_bin_set_ope_template( priv->input_bin, OFO_OPE_TEMPLATE( object ));

	ok = ofa_guided_input_bin_is_valid( priv->input_bin );
	on_right_piece_changed( priv->input_bin, ok, self );

	gtk_widget_show_all( gtk_paned_get_child2( GTK_PANED( priv->pane )));
}

static void
insert_left_ledger_row( ofaGuidedEx *self, ofoLedger *ledger )
{
	ofaGuidedExPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	priv = ofa_guided_ex_get_instance_private( self );

	tmodel = gtk_tree_view_get_model( priv->left_tview );
	gtk_tree_store_insert_with_values(
			GTK_TREE_STORE( tmodel ),
			&iter,
			NULL,
			-1,
			LEFT_COL_MNEMO,  ofo_ledger_get_mnemo( ledger ),
			LEFT_COL_LABEL,  ofo_ledger_get_label( ledger ),
			LEFT_COL_OBJECT, ledger,
			-1 );
}

static void
update_left_ledger_row( ofaGuidedEx *self, ofoLedger *ledger, const gchar *prev_id )
{
	static const gchar *thisfn = "ofa_guided_ex_update_left_ledger_row";
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gboolean found;
	const gchar *cstr;

	cstr = prev_id ? prev_id : ofo_ledger_get_mnemo( ledger );
	found = find_left_ledger_by_mnemo( self, cstr, &tmodel, &iter );
	if( found ){
		gtk_tree_store_set(
				GTK_TREE_STORE( tmodel ),
				&iter,
				LEFT_COL_MNEMO, ofo_ledger_get_mnemo( ledger ),
				LEFT_COL_LABEL, ofo_ledger_get_label( ledger ),
				-1 );
	} else {
		g_warning( "%s: unable to find ledger %s", thisfn, prev_id );
	}
}

/*
 * models which were stored under the removed ledger are to be
 *  reordered under an 'Unclassed' category
 */
static void
remove_left_ledger_row( ofaGuidedEx *self, ofoLedger *ledger )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter, child_iter;
	gboolean found;
	ofoOpeTemplate *model;

	if( !find_left_ledger_by_mnemo( self, ofo_ledger_get_mnemo( ledger ), &tmodel, &iter )){
		return;
	}

	while( gtk_tree_model_iter_n_children( tmodel, &iter )){
		found = gtk_tree_model_iter_children( tmodel, &child_iter, &iter );
		g_return_if_fail( found );

		gtk_tree_model_get(
					tmodel ,
					&child_iter,
					LEFT_COL_OBJECT, &model,
					-1 );
		g_return_if_fail( model && OFO_IS_OPE_TEMPLATE( model ));

		gtk_tree_store_remove( GTK_TREE_STORE( tmodel ), &child_iter );
		insert_left_model_row( self, model );
		g_object_unref( model );
	}

	found = find_left_ledger_by_mnemo( self, ofo_ledger_get_mnemo( ledger ), NULL, &iter );
	g_return_if_fail( found );

	gtk_tree_store_remove( GTK_TREE_STORE( tmodel ), &iter );
}

/*
 * returns TRUE if found
 *
 * we also manage the case of the 'Unclassed' catagory were we are
 * storing models with unreferenced ledgers - the ofoLedger object
 * is null
 */
static gboolean
find_left_ledger_by_mnemo( ofaGuidedEx *self, const gchar *mnemo, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	ofaGuidedExPrivate *priv;
	GtkTreeModel *my_tmodel;
	GtkTreeIter my_iter;
	gchar *ledger;
	ofoBase *object;
	gint cmp;

	priv = ofa_guided_ex_get_instance_private( self );

	my_tmodel = gtk_tree_view_get_model( priv->left_tview );
	if( tmodel ){
		*tmodel = my_tmodel;
	}
	if( gtk_tree_model_get_iter_first( my_tmodel, &my_iter )){
		while( TRUE ){
			gtk_tree_model_get(
						my_tmodel, &my_iter,
						LEFT_COL_MNEMO, &ledger, LEFT_COL_OBJECT, &object, -1 );
			if( object ){
				g_object_unref( object );
			}
			g_return_val_if_fail( !object || OFO_IS_LEDGER( object ), FALSE );

			cmp = g_utf8_collate( mnemo, ledger );
			g_free( ledger );

			if( cmp == 0 ){
				if( iter ){
					*iter = my_iter;
				}
				return( TRUE );
			}

			if( !gtk_tree_model_iter_next( my_tmodel, &my_iter )){
				break;
			}
		}
	}

	return( FALSE );
}

static void
insert_left_model_row( ofaGuidedEx *self, ofoOpeTemplate *model )
{
	static const gchar *thisfn = "ofa_guided_ex_insert_left_model_row";
	GtkTreeModel *tmodel;
	GtkTreeIter parent_iter, iter;
	gboolean found;

	found = find_left_ledger_by_mnemo( self, ofo_ope_template_get_ledger( model ), &tmodel, &parent_iter );
	if( !found ){
		g_debug( "%s: ledger not found: %s", thisfn, ofo_ope_template_get_ledger( model ));

		found = find_left_ledger_by_mnemo( self, UNKNOWN_LEDGER_MNEMO, &tmodel, &parent_iter );
		if( !found ){
			g_debug( "%s: defining ledger for unclassed models: %s", thisfn, UNKNOWN_LEDGER_MNEMO );

			gtk_tree_store_insert_with_values(
					GTK_TREE_STORE( tmodel ),
					&parent_iter,
					NULL,
					-1,
					LEFT_COL_MNEMO,  UNKNOWN_LEDGER_MNEMO,
					LEFT_COL_LABEL,  UNKNOWN_LEDGER_LABEL,
					LEFT_COL_OBJECT, NULL,
					-1 );
		}
	}

	gtk_tree_store_insert_with_values(
			GTK_TREE_STORE( tmodel ),
			&iter,
			&parent_iter,
			-1,
			LEFT_COL_MNEMO,  ofo_ope_template_get_mnemo( model ),
			LEFT_COL_LABEL,  ofo_ope_template_get_label( model ),
			LEFT_COL_OBJECT, model,
			-1 );
}

static void
update_left_model_row( ofaGuidedEx *self, ofoOpeTemplate *model, const gchar *prev_id )
{
	static const gchar *thisfn = "ofa_guided_ex_udpate_left_model_row";
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gboolean found;

	found = find_left_model_by_mnemo( self, prev_id, &tmodel, &iter );

	if( found ){
		gtk_tree_store_remove( GTK_TREE_STORE( tmodel ), &iter );
		insert_left_model_row( self, model );

	} else {
		g_warning( "%s: unable to find model %s", thisfn, prev_id );
	}
}

static void
remove_left_model_row( ofaGuidedEx *self, ofoOpeTemplate *model )
{
	static const gchar *thisfn = "ofa_guided_ex_remove_left_model_row";
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gboolean found;
	const gchar *mnemo;

	mnemo = ofo_ope_template_get_mnemo( model );
	found = find_left_model_by_mnemo( self, mnemo, &tmodel, &iter );

	if( found ){
		gtk_tree_store_remove( GTK_TREE_STORE( tmodel ), &iter );

	} else {
		g_warning( "%s: unable to find model %s", thisfn, mnemo );
	}
}

/*
 * returns TRUE if found
 */
static gboolean
find_left_model_by_mnemo( ofaGuidedEx *self, const gchar *mnemo, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	ofaGuidedExPrivate *priv;
	GtkTreeModel *my_tmodel;
	GtkTreeIter my_iter, child_iter;
	ofoBase *my_object, *child_object;
	gchar *child_mnemo;
	gint count, i, cmp;

	priv = ofa_guided_ex_get_instance_private( self );

	my_tmodel = gtk_tree_view_get_model( priv->left_tview );
	if( tmodel ){
		*tmodel = my_tmodel;
	}
	if( gtk_tree_model_get_iter_first( my_tmodel, &my_iter )){
		while( TRUE ){
			gtk_tree_model_get(
					my_tmodel, &my_iter, LEFT_COL_OBJECT, &my_object, -1 );
			if( my_object ){
				g_object_unref( my_object );
			}
			g_return_val_if_fail( !my_object || OFO_IS_LEDGER( my_object ), FALSE );

			count = gtk_tree_model_iter_n_children( my_tmodel, &my_iter );
			for( i=0 ; i < count ; ++i ){
				if( gtk_tree_model_iter_nth_child( my_tmodel, &child_iter, &my_iter, i )){
					gtk_tree_model_get(
							my_tmodel, &child_iter,
							LEFT_COL_MNEMO, &child_mnemo, LEFT_COL_OBJECT, &child_object, -1 );
					g_object_unref( child_object );
					g_return_val_if_fail( child_object && OFO_IS_OPE_TEMPLATE( child_object ), FALSE );

					cmp = g_utf8_collate( mnemo, child_mnemo );
					g_free( child_mnemo );
					if( cmp == 0 ){
						if( iter ){
							*iter = child_iter;
						}
						return( TRUE );
					}
				}
			}
			if( !gtk_tree_model_iter_next( my_tmodel, &my_iter )){
				break;
			}
		}
	}

	return( FALSE );
}

static void
on_right_piece_changed( ofaGuidedInputBin *bin, gboolean ok, ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;

	priv = ofa_guided_ex_get_instance_private( self );

	if( priv->ok_btn ){
		gtk_widget_set_sensitive( priv->ok_btn, ok );
	}
}

/*
 * the right bottom "OK" has been clicked:
 *  try to validate and generate the entries
 */
static void
on_right_ok( GtkButton *button, ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;

	priv = ofa_guided_ex_get_instance_private( self );

	ofa_guided_input_bin_apply( priv->input_bin );
	gtk_widget_set_sensitive( priv->ok_btn, FALSE );
}

/*
 * the right bottom "Cancel" has been clicked:
 *  reset all the fields, keeping the dates, and the same model
 */
static void
on_right_cancel( GtkButton *button, ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;

	priv = ofa_guided_ex_get_instance_private( self );

	ofa_guided_input_bin_reset( priv->input_bin );
	gtk_widget_set_sensitive( priv->ok_btn, FALSE );
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
on_hub_new_object( const ofoDossier *dossier, const ofoBase *object, ofaGuidedEx *self )
{
	static const gchar *thisfn = "ofa_guided_ex_on_hub_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_OPE_TEMPLATE( object )){
		insert_left_model_row( self, OFO_OPE_TEMPLATE( object ));

	} else if( OFO_IS_LEDGER( object )){
		insert_left_ledger_row( self, OFO_LEDGER( object ));
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
on_hub_updated_object( const ofoDossier *dossier, const ofoBase *object, const gchar *prev_id, ofaGuidedEx *self )
{
	static const gchar *thisfn = "ofa_guided_ex_on_hub_updated_object";

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_OPE_TEMPLATE( object )){
		update_left_model_row( self, OFO_OPE_TEMPLATE( object ), prev_id );

	} else if( OFO_IS_LEDGER( object )){
		update_left_ledger_row( self, OFO_LEDGER( object ), prev_id );
	}
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
on_hub_deleted_object( const ofoDossier *dossier, const ofoBase *object, ofaGuidedEx *self )
{
	static const gchar *thisfn = "ofa_guided_ex_on_hub_deleted_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_OPE_TEMPLATE( object )){
		remove_left_model_row( self, OFO_OPE_TEMPLATE( object ));

	} else if( OFO_IS_LEDGER( object )){
		remove_left_ledger_row( self, OFO_LEDGER( object ));
	}
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
on_hub_reload_dataset( const ofoDossier *dossier, GType type, ofaGuidedEx *self )
{
	static const gchar *thisfn = "ofa_guided_ex_on_hub_reload_dataset";

	g_debug( "%s: dossier=%p, type=%lu, self=%p",
			thisfn, ( void * ) dossier, type, ( void * ) self );

	if( type == OFO_TYPE_OPE_TEMPLATE ){


	} else if( type == OFO_TYPE_LEDGER ){

	}
}

static void
on_page_removed( ofaGuidedEx *page, GtkWidget *page_w, guint page_n, void *empty )
{
	static const gchar *thisfn = "ofa_guided_ex_on_page_removed";
	ofaGuidedExPrivate *priv;

	g_debug( "%s: page=%p, page_w=%p, page_n=%d, empty=%p",
			thisfn, ( void * ) page, ( void * ) page_w, page_n, ( void * ) empty );

	priv = ofa_guided_ex_get_instance_private( page );

	if( priv->pane ){
		pane_save_position( priv->pane );
	}
}

static void
pane_save_position( GtkWidget *pane )
{
	guint pos;

	pos = gtk_paned_get_position( GTK_PANED( pane ));
	g_debug( "ofa_guided_ex_pane_save_position: pos=%d", pos );
	ofa_settings_user_set_uint( "GuidedInputExDlg-pane", pos );
}
