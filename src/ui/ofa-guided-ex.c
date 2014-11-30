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

#include "api/my-utils.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"

#include "ui/ofa-guided-common.h"
#include "ui/ofa-guided-ex.h"
#include "ui/ofa-page.h"
#include "ui/ofa-page-prot.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaGuidedExPrivate {

	/* internals
	 */
	ofoDossier           *dossier;			/* dossier */
	const ofoOpeTemplate *model;			/* model */
	ofaGuidedCommon      *common;

	/* UI - the pane
	 */
	GtkPaned             *pane;

	/* UI - left part treeview selection of the entry model
	 */
	GtkTreeView          *left_tview;
	GtkButton            *left_select;

	/* UI - right part guided input
	 *      most if not all elements are taken from ofa-guided-input.ui
	 *      dialog box definition
	 */
	GtkContainer         *right_box;		/* the reparented container from dialog */
	GtkButton            *right_ok;
	GtkButton            *right_cancel;
};

/* columns in the left tree view which handles the entry models
 */
enum {
	LEFT_COL_MNEMO = 0,
	LEFT_COL_LABEL,
	LEFT_COL_OBJECT,
	LEFT_N_COLUMNS
};

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-guided-input.ui";
static const gchar *st_ui_id            = "GuidedInputDlg";

G_DEFINE_TYPE( ofaGuidedEx, ofa_guided_ex, OFA_TYPE_PAGE )

static GtkWidget *v_setup_view( ofaPage *page );
static void       pane_restore_position( GtkPaned *pane );
static GtkWidget *v_setup_buttons( ofaPage *page );
static void       v_init_view( ofaPage *page );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static GtkWidget *setup_view_left( ofaGuidedEx *self );
static GtkWidget *setup_view_right( ofaGuidedEx *self );
static GtkWidget *setup_left_treeview( ofaGuidedEx *self );
static gint       on_left_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaGuidedEx *self );
static void       on_left_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaGuidedEx *self );
static void       reparent_from_dialog( ofaGuidedEx *self, GtkContainer *parent );
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
static void       on_right_ok( GtkButton *button, ofaGuidedEx *self );
static void       on_right_cancel( GtkButton *button, ofaGuidedEx *self );
static void       on_new_object( const ofoDossier *dossier, const ofoBase *object, ofaGuidedEx *self );
static void       on_updated_object( const ofoDossier *dossier, const ofoBase *object, const gchar *prev_id, ofaGuidedEx *self );
static void       on_deleted_object( const ofoDossier *dossier, const ofoBase *object, ofaGuidedEx *self );
static void       on_reload_dataset( const ofoDossier *dossier, GType type, ofaGuidedEx *self );
static void       v_pre_remove( ofaPage *page );
static void       pane_save_position( GtkPaned *pane );

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
		priv = OFA_GUIDED_EX( instance )->priv;

		if( priv->common ){
			g_clear_object( &priv->common );
		}
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

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_GUIDED_EX, ofaGuidedExPrivate );
}

static void
ofa_guided_ex_class_init( ofaGuidedExClass *klass )
{
	static const gchar *thisfn = "ofa_guided_ex_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = guided_ex_dispose;
	G_OBJECT_CLASS( klass )->finalize = guided_ex_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->pre_remove = v_pre_remove;

	g_type_class_add_private( klass, sizeof( ofaGuidedExPrivate ));
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	ofaGuidedExPrivate *priv;
	GtkPaned *child;

	priv = OFA_GUIDED_EX( page )->priv;
	priv->dossier = ofa_page_get_dossier( page );

	child = GTK_PANED( gtk_paned_new( GTK_ORIENTATION_HORIZONTAL ));
	gtk_paned_add1( child, setup_view_left( OFA_GUIDED_EX( page )));
	gtk_paned_add2( child, setup_view_right( OFA_GUIDED_EX( page )));
	priv->pane = child;
	pane_restore_position( child );

	g_signal_connect(
			G_OBJECT( priv->dossier ),
			SIGNAL_DOSSIER_NEW_OBJECT, G_CALLBACK( on_new_object ), page );

	g_signal_connect(
			G_OBJECT( priv->dossier ),
			SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), page );

	g_signal_connect(
			G_OBJECT( priv->dossier ),
			SIGNAL_DOSSIER_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), page );

	g_signal_connect(
			G_OBJECT( priv->dossier ),
			SIGNAL_DOSSIER_RELOAD_DATASET, G_CALLBACK( on_reload_dataset ), page );

	return( GTK_WIDGET( child ));
}

static void
pane_restore_position( GtkPaned *pane )
{
	gint pos;

	pos = ofa_settings_get_int( "GuidedInputExDlg-pane" );
	g_debug( "ofa_guided_ex_pane_restore_position: pos=%d", pos );
	gtk_paned_set_position( pane, pos );
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	return( NULL );
}

static void
v_init_view( ofaPage *page )
{
	init_left_view( OFA_GUIDED_EX( page ),
						gtk_paned_get_child1( OFA_GUIDED_EX( page )->priv->pane ));
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_GUIDED_EX( page ), NULL );

	return( GTK_WIDGET( OFA_GUIDED_EX( page )->priv->left_tview ));
}

/*
 * the left pane is a treeview whose level 0 are the ledgers, and
 * level 1 the entry models defined on the corresponding ledger
 */
static GtkWidget *
setup_view_left( ofaGuidedEx *self )
{
	GtkFrame *frame;
	GtkWidget *tview;
	GtkBox *box, *box2;
	GtkButton *button;

	frame = GTK_FRAME( gtk_frame_new( _( " Per ledger " )));
	gtk_widget_set_margin_left( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( frame ), 4 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 ));
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( box ));

	tview = setup_left_treeview( self );
	gtk_box_pack_start( box, tview, TRUE, TRUE, 0 );

	box2 = GTK_BOX( gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 4 ));
	gtk_box_pack_end( box, GTK_WIDGET( box2 ), FALSE, FALSE, 0 );

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_Select" )));
	gtk_widget_set_margin_bottom( GTK_WIDGET( button ), 4 );
	gtk_widget_set_margin_right( GTK_WIDGET( button ), 4 );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_left_select_clicked ), self );
	gtk_box_pack_end( box2, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	self->priv->left_select = button;

	enable_left_select( self );

	return( GTK_WIDGET( frame ));
}

/*
 * note that we may have no current ofoOpeTemplate at this time
 */
static GtkWidget *
setup_view_right( ofaGuidedEx *self )
{
	GtkFrame *frame;
	ofaGuidedExPrivate *priv;
	GtkScrolledWindow *scroll;
	GtkWidget *widget;

	priv = self->priv;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_margin_right( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( frame ), 4 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_NONE );

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_container_set_border_width( GTK_CONTAINER( scroll ), 0 );
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( scroll ));

	/* setup the box from the ofaGuidedInput dialog */
	reparent_from_dialog( self, GTK_CONTAINER( scroll ));

	widget = my_utils_container_get_child_by_name( priv->right_box, "box-ok" );
	g_return_val_if_fail( widget && GTK_IS_BUTTON( widget ), NULL );
	priv->right_ok = GTK_BUTTON( widget );
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_right_ok ), self );

	widget = my_utils_container_get_child_by_name( priv->right_box, "box-cancel" );
	g_return_val_if_fail( widget && GTK_IS_BUTTON( widget ), NULL );
	priv->right_cancel = GTK_BUTTON( widget );
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_right_cancel ), self );

	priv->common = ofa_guided_common_new(
							ofa_page_get_main_window( OFA_PAGE( self )),
							priv->right_box );

	return( GTK_WIDGET( frame ));
}

/*
 * the left pane is a treeview whose level 0 are the ledgers, and
 * level 1 the entry models defined on the corresponding ledger
 */
static GtkWidget *
setup_left_treeview( ofaGuidedEx *self )
{
	GtkScrolledWindow *scroll;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_container_set_border_width( GTK_CONTAINER( scroll ), 4 );
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );

	tview = GTK_TREE_VIEW( gtk_tree_view_new());
	/*gtk_widget_set_vexpand( GTK_WIDGET( tview ), TRUE );*/
	gtk_tree_view_set_headers_visible( tview, FALSE );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( tview ));
	g_signal_connect(G_OBJECT( tview ), "row-activated", G_CALLBACK( on_left_row_activated ), self );
	g_signal_connect( G_OBJECT( tview ), "key-press-event", G_CALLBACK( on_left_key_pressed ), self );
	self->priv->left_tview = tview;

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
	return( 0 );
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
reparent_from_dialog( ofaGuidedEx *self, GtkContainer *parent )
{
	GtkWidget *dialog;
	GtkWidget *box;

	/* load our dialog */
	dialog = my_utils_builder_load_from_path( st_ui_xml, st_ui_id );
	g_return_if_fail( dialog && GTK_IS_WINDOW( dialog ));

	box = my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "px-box" );
	g_return_if_fail( box && GTK_IS_BOX( box ));
	self->priv->right_box = GTK_CONTAINER( box );

	/* attach our box to the parent's frame */
	gtk_widget_reparent( box, GTK_WIDGET( parent ));
}

static void
init_left_view( ofaGuidedEx *self, GtkWidget *child )
{
	GList *dataset, *ise;

	dataset = ofo_ledger_get_dataset( self->priv->dossier );
	for( ise=dataset ; ise ; ise=ise->next ){
		insert_left_ledger_row( self, OFO_LEDGER( ise->data ));
	}

	dataset = ofo_ope_template_get_dataset( self->priv->dossier );
	for( ise=dataset ; ise ; ise=ise->next ){
		insert_left_model_row( self, OFO_OPE_TEMPLATE( ise->data ));
	}
}

static void
on_left_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaGuidedEx *self )
{
	if( is_left_select_enableable( self )){
		select_model( self );
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
	gtk_widget_set_sensitive(
			GTK_WIDGET( self->priv->left_select ), is_left_select_enableable( self ));
}

static gboolean
is_left_select_enableable( ofaGuidedEx *self )
{
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBase *object;
	gboolean ok;

	ok = FALSE;

	select = gtk_tree_view_get_selection( self->priv->left_tview );
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

	priv = self->priv;

	object = NULL;
	select = gtk_tree_view_get_selection( priv->left_tview );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, LEFT_COL_OBJECT, &object, -1 );
		g_object_unref( object );
	}
	g_return_if_fail( object && OFO_IS_OPE_TEMPLATE( object ));

	ofa_guided_common_set_model( priv->common, OFO_OPE_TEMPLATE( object ));

	gtk_widget_show_all( gtk_paned_get_child2( priv->pane ));
}

static void
insert_left_ledger_row( ofaGuidedEx *self, ofoLedger *ledger )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	tmodel = gtk_tree_view_get_model( self->priv->left_tview );
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

	found = find_left_ledger_by_mnemo( self, prev_id, &tmodel, &iter );
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
	GtkTreeModel *my_tmodel;
	GtkTreeIter my_iter;
	gchar *ledger;
	ofoBase *object;
	gint cmp;

	my_tmodel = gtk_tree_view_get_model( self->priv->left_tview );
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
	GtkTreeModel *my_tmodel;
	GtkTreeIter my_iter, child_iter;
	ofoBase *my_object, *child_object;
	gchar *child_mnemo;
	gint count, i, cmp;

	my_tmodel = gtk_tree_view_get_model( self->priv->left_tview );
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

/*
 * the right bottom "OK" has been clicked:
 *  try to validate and generate the entries
 */
static void
on_right_ok( GtkButton *button, ofaGuidedEx *self )
{
	ofa_guided_common_validate( self->priv->common );
}

/*
 * the right bottom "Cancel" has been clicked:
 *  reset all the fields, keeping the dates, and the same model
 */
static void
on_right_cancel( GtkButton *button, ofaGuidedEx *self )
{
	ofa_guided_common_reset( self->priv->common );
}

/*
 * SIGNAL_DOSSIER_NEW_OBJECT signal handler
 */
static void
on_new_object( const ofoDossier *dossier, const ofoBase *object, ofaGuidedEx *self )
{
	static const gchar *thisfn = "ofa_guided_ex_on_new_object";

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
 * SIGNAL_DOSSIER_UPDATED_OBJECT signal handler
 */
static void
on_updated_object( const ofoDossier *dossier, const ofoBase *object, const gchar *prev_id, ofaGuidedEx *self )
{
	static const gchar *thisfn = "ofa_guided_ex_on_updated_object";

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
 * SIGNAL_DOSSIER_DELETED_OBJECT signal handler
 */
static void
on_deleted_object( const ofoDossier *dossier, const ofoBase *object, ofaGuidedEx *self )
{
	static const gchar *thisfn = "ofa_guided_ex_on_deleted_object";

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
 * SIGNAL_DOSSIER_RELOAD_DATASET signal handler
 */
static void
on_reload_dataset( const ofoDossier *dossier, GType type, ofaGuidedEx *self )
{
	static const gchar *thisfn = "ofa_guided_ex_on_reload_dataset";

	g_debug( "%s: dossier=%p, type=%lu, self=%p",
			thisfn, ( void * ) dossier, type, ( void * ) self );

	if( type == OFO_TYPE_OPE_TEMPLATE ){


	} else if( type == OFO_TYPE_LEDGER ){

	}
}

static void
v_pre_remove( ofaPage *page )
{
	ofaGuidedExPrivate *priv;

	g_return_if_fail( page && OFA_IS_GUIDED_EX( page ));

	if( !page->prot->dispose_has_run ){

		priv = OFA_GUIDED_EX( page )->priv;

		if( priv->pane ){
			pane_save_position( priv->pane );
		}
	}
}

static void
pane_save_position( GtkPaned *pane )
{
	guint pos;

	pos = gtk_paned_get_position( pane );
	g_debug( "ofa_guided_ex_pane_save_position: pos=%d", pos );
	ofa_settings_set_int( "GuidedInputExDlg-pane", pos );
}
