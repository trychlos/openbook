/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"

#include "core/ofa-guided-input-bin.h"

#include "ui/ofa-guided-ex.h"

/* private instance data
 */
typedef struct {

	/* internals
	 */
	ofaHub               *hub;
	GList                *hub_handlers;
	const ofoOpeTemplate *model;			/* model */
	ofaGuidedInputBin    *input_bin;
	gchar                *settings_prefix;

	/* UI - the pane
	 */
	GtkWidget            *paned;

	/* UI - left part treeview selection of the entry model
	 */
	GtkTreeView          *left_tview;
	GtkButton            *left_select;

	/* UI - right part guided input
	 *      most if not all elements are taken from ofa-guided-input-bin.ui
	 *      dialog box definition
	 */
	GtkWidget            *ok_btn;
}
	ofaGuidedExPrivate;

/* columns in the left tree view which handles the entry models
 */
enum {
	LEFT_COL_MNEMO = 0,
	LEFT_COL_LABEL,
	LEFT_COL_OBJECT,
	LEFT_N_COLUMNS
};

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-guided-ex.ui";
static const gchar *st_ui_name1         = "ofaGuidedExView1";
static const gchar *st_ui_name2         = "ofaGuidedExView2";

static GtkWidget *page_v_get_top_focusable_widget( const ofaPage *page );
static void       paned_page_v_setup_view( ofaPanedPage *page, GtkPaned *paned );
static void       pane_restore_position( ofaGuidedEx *self );
static void       pane_save_position( ofaGuidedEx *self );
static GtkWidget *setup_view1( ofaGuidedEx *self );
static void       left_setup_treeview( ofaGuidedEx *self, GtkContainer *parent );
static gint       left_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaGuidedEx *self );
static gchar     *left_get_sort_key( GtkTreeModel *tmodel, GtkTreeIter *iter );
static void       left_on_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaGuidedEx *self );
static void       left_on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaGuidedEx *self );
static void       left_on_row_selected( GtkTreeSelection *selection, ofaGuidedEx *self );
static gboolean   left_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaGuidedEx *self );
static void       left_collapse_node( ofaGuidedEx *self, GtkWidget *widget );
static void       left_expand_node( ofaGuidedEx *self, GtkWidget *widget );
static void       left_enable_select( ofaGuidedEx *self );
static gboolean   left_is_select_enableable( ofaGuidedEx *self );
static void       left_on_select_clicked( GtkButton *button, ofaGuidedEx *self );
static void       left_init_view( ofaGuidedEx *self );
static void       ledger_insert_row( ofaGuidedEx *self, ofoLedger *ledger );
static void       ledger_update_row( ofaGuidedEx *self, ofoLedger *ledger, const gchar *prev_id );
static void       ledger_remove_row( ofaGuidedEx *self, ofoLedger *ledger );
static gboolean   ledger_find_by_mnemo( ofaGuidedEx *self, const gchar *mnemo, GtkTreeModel **tmodel, GtkTreeIter *iter );
static void       model_insert_row( ofaGuidedEx *self, ofoOpeTemplate *model );
static void       model_update_row( ofaGuidedEx *self, ofoOpeTemplate *model, const gchar *prev_id );
static void       model_remove_row( ofaGuidedEx *self, ofoOpeTemplate *model );
static gboolean   model_find_by_mnemo( ofaGuidedEx *self, const gchar *mnemo, GtkTreeModel **tmodel, GtkTreeIter *iter );
static void       model_select( ofaGuidedEx *self );
static GtkWidget *setup_view2( ofaGuidedEx *self );
static void       right_on_piece_changed( ofaGuidedInputBin *bin, gboolean ok, ofaGuidedEx *self );
static void       right_on_ok( GtkButton *button, ofaGuidedEx *self );
static void       right_on_cancel( GtkButton *button, ofaGuidedEx *self );
static void       connect_to_hub_signaling_system( ofaGuidedEx *self );
static void       hub_on_new_object( ofaHub *hub, const ofoBase *object, ofaGuidedEx *self );
static void       hub_on_updated_object( ofaHub *hub, const ofoBase *object, const gchar *prev_id, ofaGuidedEx *self );
static void       hub_on_deleted_object( ofaHub *hub, const ofoBase *object, ofaGuidedEx *self );
static void       hub_on_reload_dataset( ofaHub *hub, GType type, ofaGuidedEx *self );

G_DEFINE_TYPE_EXTENDED( ofaGuidedEx, ofa_guided_ex, OFA_TYPE_PANED_PAGE, 0,
		G_ADD_PRIVATE( ofaGuidedEx ))

static void
guided_ex_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_guided_ex_finalize";
	ofaGuidedExPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_GUIDED_EX( instance ));

	/* free data members here */
	priv = ofa_guided_ex_get_instance_private( OFA_GUIDED_EX( instance ));

	g_free( priv->settings_prefix );

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

		if( priv->paned ){
			pane_save_position( OFA_GUIDED_EX( instance ));
		}

		ofa_hub_disconnect_handlers( priv->hub, &priv->hub_handlers );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_guided_ex_parent_class )->dispose( instance );
}

static void
ofa_guided_ex_init( ofaGuidedEx *self )
{
	static const gchar *thisfn = "ofa_guided_ex_init";
	ofaGuidedExPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_GUIDED_EX( self ));

	priv = ofa_guided_ex_get_instance_private( self );

	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_guided_ex_class_init( ofaGuidedExClass *klass )
{
	static const gchar *thisfn = "ofa_guided_ex_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = guided_ex_dispose;
	G_OBJECT_CLASS( klass )->finalize = guided_ex_finalize;

	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = page_v_get_top_focusable_widget;

	OFA_PANED_PAGE_CLASS( klass )->setup_view = paned_page_v_setup_view;
}

static GtkWidget *
page_v_get_top_focusable_widget( const ofaPage *page )
{
	ofaGuidedExPrivate *priv;

	g_return_val_if_fail( page && OFA_IS_GUIDED_EX( page ), NULL );

	priv = ofa_guided_ex_get_instance_private( OFA_GUIDED_EX( page ));

	return( GTK_WIDGET( priv->left_tview ));
}

static void
paned_page_v_setup_view( ofaPanedPage *page, GtkPaned *paned )
{
	static const gchar *thisfn = "ofa_guided_ex_v_setup_view";
	ofaGuidedExPrivate *priv;
	GtkWidget *view;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_guided_ex_get_instance_private( OFA_GUIDED_EX( page ));

	priv->hub = ofa_igetter_get_hub( OFA_IGETTER( page ));
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	priv->paned = GTK_WIDGET( paned );
	pane_restore_position( OFA_GUIDED_EX( page ));

	view = setup_view1( OFA_GUIDED_EX( page ));
	gtk_paned_pack1( paned, view, FALSE, TRUE );

	view = setup_view2( OFA_GUIDED_EX( page ));
	gtk_paned_pack2( paned, view, TRUE, FALSE );

	connect_to_hub_signaling_system( OFA_GUIDED_EX( page ));

	left_init_view( OFA_GUIDED_EX( page ));
}

static void
pane_restore_position( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;
	gchar *settings_key;
	gint pos;

	priv = ofa_guided_ex_get_instance_private( self );

	settings_key = g_strdup_printf( "%s-pane", priv->settings_prefix );
	pos = ofa_settings_user_get_uint( settings_key );
	if( pos <= 100 ){
		pos = 150;
	}
	gtk_paned_set_position( GTK_PANED( priv->paned ), pos );
	g_free( settings_key );
}

static void
pane_save_position( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;
	gchar *settings_key;
	guint pos;

	priv = ofa_guided_ex_get_instance_private( self );

	settings_key = g_strdup_printf( "%s-pane", priv->settings_prefix );
	pos = gtk_paned_get_position( GTK_PANED( priv->paned ));
	ofa_settings_user_set_uint( settings_key, pos );
	g_free( settings_key );
}

/*
 * the left pane is a treeview whose level 0 are the ledgers, and
 * level 1 the entry models defined on the corresponding ledger
 */
static GtkWidget *
setup_view1( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;
	GtkWidget *box, *button;

	priv = ofa_guided_ex_get_instance_private( self );

	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	my_utils_container_attach_from_resource( GTK_CONTAINER( box ), st_resource_ui, st_ui_name1, "top1" );

	left_setup_treeview( self, GTK_CONTAINER( box ));

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "select-btn" );
	g_return_val_if_fail( button && GTK_IS_BUTTON( button ), NULL );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( left_on_select_clicked ), self );
	priv->left_select = GTK_BUTTON( button );

	left_enable_select( self );

	return( box );
}

/*
 * the left pane is a treeview whose level 0 are the ledgers, and
 * level 1 the entry models defined on the corresponding ledger
 */
static void
left_setup_treeview( ofaGuidedEx *self, GtkContainer *parent )
{
	ofaGuidedExPrivate *priv;
	GtkWidget *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = ofa_guided_ex_get_instance_private( self );

	tview = my_utils_container_get_child_by_name( parent, "left-treeview" );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));

	gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( tview ), FALSE );
	g_signal_connect( tview, "row-activated", G_CALLBACK( left_on_row_activated ), self );
	g_signal_connect( tview, "key-press-event", G_CALLBACK( left_on_key_pressed ), self );
	priv->left_tview = GTK_TREE_VIEW( tview );

	tmodel = GTK_TREE_MODEL( gtk_tree_store_new(
			LEFT_N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( GTK_TREE_VIEW( tview ), tmodel );
	g_object_unref( tmodel );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Mnemonic" ),
			text_cell, "text", LEFT_COL_MNEMO,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) left_on_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", LEFT_COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) left_on_cell_data_func, self, NULL );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( left_on_row_selected ), self );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ), ( GtkTreeIterCompareFunc ) left_on_sort_model, self, NULL );

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );
}

static gint
left_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaGuidedEx *self )
{
	gchar *a_key, *b_key;
	gint cmp;

	a_key = left_get_sort_key( tmodel, a );
	b_key = left_get_sort_key( tmodel, b );

	cmp = g_utf8_collate( a_key, b_key );

	g_free( a_key );
	g_free( b_key );

	return( cmp );
}

static gchar *
left_get_sort_key( GtkTreeModel *tmodel, GtkTreeIter *iter )
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
left_on_cell_data_func( GtkTreeViewColumn *tcolumn,
							GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
							ofaGuidedEx *self )
{
	GObject *object;
	GdkRGBA color;

	g_return_if_fail( GTK_IS_CELL_RENDERER_TEXT( cell ));

	gtk_tree_model_get( tmodel, iter,
			LEFT_COL_OBJECT, &object,
			-1 );
	if( object ){
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
}

static void
left_on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaGuidedEx *self )
{
	/* select the operation template if the current selected row is an
	 * operation template */
	if( left_is_select_enableable( self )){
		model_select( self );

	/* else collapse/expand the ledger */
	} else if( gtk_tree_view_row_expanded( view, path )){
		gtk_tree_view_collapse_row( view, path );

	} else {
		gtk_tree_view_expand_row( view, path, TRUE );
	}
}

static void
left_on_row_selected( GtkTreeSelection *selection, ofaGuidedEx *self )
{
	left_enable_select( self );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 *
 * Handles left and right arrows to expand/collapse nodes
 */
static gboolean
left_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaGuidedEx *self )
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
left_enable_select( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;

	priv = ofa_guided_ex_get_instance_private( self );

	gtk_widget_set_sensitive(
			GTK_WIDGET( priv->left_select ), left_is_select_enableable( self ));
}

/*
 * return %TRUE if the current selection is an operation template
 */
static gboolean
left_is_select_enableable( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBase *object;
	gboolean ok;

	priv = ofa_guided_ex_get_instance_private( self );
	ok = FALSE;

	if( priv->left_tview ){
		select = gtk_tree_view_get_selection( priv->left_tview );
		if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
			gtk_tree_model_get( tmodel, &iter, LEFT_COL_OBJECT, &object, -1 );
			if( object ){
				g_object_unref( object );
				ok = OFO_IS_OPE_TEMPLATE( object );
			}
		}
	}

	return( ok );
}

static void
left_on_select_clicked( GtkButton *button, ofaGuidedEx *self )
{
	model_select( self );
}

static void
left_init_view( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;
	GList *dataset, *it;

	priv = ofa_guided_ex_get_instance_private( self );

	dataset = ofo_ledger_get_dataset( priv->hub );
	for( it=dataset ; it ; it=it->next ){
		ledger_insert_row( self, OFO_LEDGER( it->data ));
	}

	dataset = ofo_ope_template_get_dataset( priv->hub );
	for( it=dataset ; it ; it=it->next ){
		model_insert_row( self, OFO_OPE_TEMPLATE( it->data ));
	}
}

static void
ledger_insert_row( ofaGuidedEx *self, ofoLedger *ledger )
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
ledger_update_row( ofaGuidedEx *self, ofoLedger *ledger, const gchar *prev_id )
{
	static const gchar *thisfn = "ofa_guided_ex_ledger_update_row";
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gboolean found;
	const gchar *cstr;

	cstr = prev_id ? prev_id : ofo_ledger_get_mnemo( ledger );
	found = ledger_find_by_mnemo( self, cstr, &tmodel, &iter );
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
ledger_remove_row( ofaGuidedEx *self, ofoLedger *ledger )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter, child_iter;
	gboolean found;
	ofoOpeTemplate *model;

	if( !ledger_find_by_mnemo( self, ofo_ledger_get_mnemo( ledger ), &tmodel, &iter )){
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
		model_insert_row( self, model );
		g_object_unref( model );
	}

	found = ledger_find_by_mnemo( self, ofo_ledger_get_mnemo( ledger ), NULL, &iter );
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
ledger_find_by_mnemo( ofaGuidedEx *self, const gchar *mnemo, GtkTreeModel **tmodel, GtkTreeIter *iter )
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
model_insert_row( ofaGuidedEx *self, ofoOpeTemplate *model )
{
	static const gchar *thisfn = "ofa_guided_ex_model_insert_row";
	GtkTreeModel *tmodel;
	GtkTreeIter parent_iter, iter;
	gboolean found;

	found = ledger_find_by_mnemo( self, ofo_ope_template_get_ledger( model ), &tmodel, &parent_iter );
	if( !found ){
		g_debug( "%s: ledger not found: %s", thisfn, ofo_ope_template_get_ledger( model ));

		found = ledger_find_by_mnemo( self, UNKNOWN_LEDGER_MNEMO, &tmodel, &parent_iter );
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
model_update_row( ofaGuidedEx *self, ofoOpeTemplate *model, const gchar *prev_id )
{
	static const gchar *thisfn = "ofa_guided_ex_model_udpate_row";
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gboolean found;

	found = model_find_by_mnemo( self, prev_id, &tmodel, &iter );

	if( found ){
		gtk_tree_store_remove( GTK_TREE_STORE( tmodel ), &iter );
		model_insert_row( self, model );

	} else {
		g_warning( "%s: unable to find model %s", thisfn, prev_id );
	}
}

static void
model_remove_row( ofaGuidedEx *self, ofoOpeTemplate *model )
{
	static const gchar *thisfn = "ofa_guided_ex_model_remove_row";
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gboolean found;
	const gchar *mnemo;

	mnemo = ofo_ope_template_get_mnemo( model );
	found = model_find_by_mnemo( self, mnemo, &tmodel, &iter );

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
model_find_by_mnemo( ofaGuidedEx *self, const gchar *mnemo, GtkTreeModel **tmodel, GtkTreeIter *iter )
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
model_select( ofaGuidedEx *self )
{
	static const gchar *thisfn = "ofa_guided_ex_model_select";
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

	g_debug( "%s: ope_template=%s", thisfn, ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object )));

	ok = ofa_guided_input_bin_is_valid( priv->input_bin );
	right_on_piece_changed( priv->input_bin, ok, self );

	gtk_widget_show_all( gtk_paned_get_child2( GTK_PANED( priv->paned )));
}

/*
 * note that we do not have any current ofoOpeTemplate at this time
 */
static GtkWidget *
setup_view2( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;
	GtkWidget *box, *bin_parent, *button, *image;

	priv = ofa_guided_ex_get_instance_private( self );

	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	my_utils_container_attach_from_resource( GTK_CONTAINER( box ), st_resource_ui, st_ui_name2, "top2" );

	bin_parent = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "bin-parent" );
	g_return_val_if_fail( bin_parent && GTK_IS_BOX( bin_parent ), NULL );
	priv->input_bin = ofa_guided_input_bin_new( OFA_IGETTER( self ));
	gtk_container_add( GTK_CONTAINER( bin_parent ), GTK_WIDGET( priv->input_bin ));
	g_signal_connect( priv->input_bin, "ofa-changed", G_CALLBACK( right_on_piece_changed ), self );

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "validate-btn" );
	g_return_val_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ), NULL );
	image = gtk_image_new_from_icon_name( "gtk-ok", GTK_ICON_SIZE_BUTTON );
	gtk_button_set_image( GTK_BUTTON( priv->ok_btn ), image );
	g_signal_connect( priv->ok_btn, "clicked", G_CALLBACK( right_on_ok ), self );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "reset-btn" );
	g_return_val_if_fail( button && GTK_IS_BUTTON( button ), NULL );
	image = gtk_image_new_from_icon_name( "gtk-cancel", GTK_ICON_SIZE_BUTTON );
	gtk_button_set_image( GTK_BUTTON( button ), image );
	g_signal_connect( button, "clicked", G_CALLBACK( right_on_cancel ), self );

	return( box );
}

static void
right_on_piece_changed( ofaGuidedInputBin *bin, gboolean ok, ofaGuidedEx *self )
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
right_on_ok( GtkButton *button, ofaGuidedEx *self )
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
right_on_cancel( GtkButton *button, ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;

	priv = ofa_guided_ex_get_instance_private( self );

	ofa_guided_input_bin_reset( priv->input_bin );
	gtk_widget_set_sensitive( priv->ok_btn, FALSE );
}

static void
connect_to_hub_signaling_system( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;
	gulong handler;

	priv = ofa_guided_ex_get_instance_private( self );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_NEW, G_CALLBACK( hub_on_new_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_UPDATED, G_CALLBACK( hub_on_updated_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_DELETED, G_CALLBACK( hub_on_deleted_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_RELOAD, G_CALLBACK( hub_on_reload_dataset ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
hub_on_new_object( ofaHub *hub, const ofoBase *object, ofaGuidedEx *self )
{
	static const gchar *thisfn = "ofa_guided_ex_hub_on_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_OPE_TEMPLATE( object )){
		model_insert_row( self, OFO_OPE_TEMPLATE( object ));

	} else if( OFO_IS_LEDGER( object )){
		ledger_insert_row( self, OFO_LEDGER( object ));
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
hub_on_updated_object( ofaHub *hub, const ofoBase *object, const gchar *prev_id, ofaGuidedEx *self )
{
	static const gchar *thisfn = "ofa_guided_ex_hub_on_updated_object";

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_OPE_TEMPLATE( object )){
		model_update_row( self, OFO_OPE_TEMPLATE( object ), prev_id );

	} else if( OFO_IS_LEDGER( object )){
		ledger_update_row( self, OFO_LEDGER( object ), prev_id );
	}
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
hub_on_deleted_object( ofaHub *hub, const ofoBase *object, ofaGuidedEx *self )
{
	static const gchar *thisfn = "ofa_guided_ex_hub_on_deleted_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_OPE_TEMPLATE( object )){
		model_remove_row( self, OFO_OPE_TEMPLATE( object ));

	} else if( OFO_IS_LEDGER( object )){
		ledger_remove_row( self, OFO_LEDGER( object ));
	}
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
hub_on_reload_dataset( ofaHub *hub, GType type, ofaGuidedEx *self )
{
	static const gchar *thisfn = "ofa_guided_ex_hub_on_reload_dataset";

	g_debug( "%s: hub=%p, type=%lu (%s), self=%p",
			thisfn, ( void * ) hub, type, g_type_name( type ), ( void * ) self );

	if( type == OFO_TYPE_OPE_TEMPLATE ){
		//

	} else if( type == OFO_TYPE_LEDGER ){
		//
	}
}
