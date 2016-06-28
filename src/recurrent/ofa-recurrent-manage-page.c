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
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-buttons-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itheme-manager.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "ofa-recurrent-main.h"
#include "ofa-recurrent-manage-page.h"
#include "ofa-recurrent-model-properties.h"
#include "ofa-recurrent-model-store.h"
#include "ofa-recurrent-new.h"
#include "ofa-recurrent-run-page.h"
#include "ofo-recurrent-model.h"

/* priv instance data
 */
typedef struct {

	/* internals
	 */
	gboolean           is_writable;

	/* UI
	 */
	GtkWidget         *tview;
	GtkWidget         *update_btn;
	GtkWidget         *delete_btn;
	GtkWidget         *generate_btn;

	/* sorting the view
	 */
	gint               sort_column_id;
	gint               sort_sens;
	GtkTreeViewColumn *sort_column;
}
	ofaRecurrentManagePagePrivate;

/* it appears that Gtk+ displays a counter intuitive sort indicator:
 * when asking for ascending sort, Gtk+ displays a 'v' indicator
 * while we would prefer the '^' version -
 * we are defining the inverse indicator, and we are going to sort
 * in reverse order to have our own illusion
 */
#define OFA_SORT_ASCENDING              GTK_SORT_DESCENDING
#define OFA_SORT_DESCENDING             GTK_SORT_ASCENDING

static const gchar *st_page_settings    = "ofaRecurrentManagePage-settings";

static GtkWidget *v_setup_view( ofaPage *page );
static GtkWidget *setup_treeview( ofaRecurrentManagePage *self );
static GtkWidget *v_setup_buttons( ofaPage *page );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       tview_on_header_clicked( GtkTreeViewColumn *column, ofaRecurrentManagePage *self );
static gint       tview_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaRecurrentManagePage *self );
static gint       tview_on_sort_detail( const gchar *detaila, const gchar *detailb );
static gboolean   tview_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaRecurrentManagePage *self );
static void       tview_on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaRecurrentManagePage *self );
static void       tview_on_selection_changed( GtkTreeSelection *selection, ofaRecurrentManagePage *self );
static GList     *tview_get_selected( ofaRecurrentManagePage *self );
static void       store_on_row_inserted_or_removed( ofaRecurrentModelStore *store, ofaRecurrentManagePage *self );
static void       action_on_new_clicked( GtkButton *button, ofaRecurrentManagePage *self );
static void       action_on_update_clicked( GtkButton *button, ofaRecurrentManagePage *self );
static void       action_on_delete_clicked( GtkButton *button, ofaRecurrentManagePage *self );
static void       action_try_to_delete_current_row( ofaRecurrentManagePage *self );
static void       action_do_delete( ofaRecurrentManagePage *self, ofoRecurrentModel *model );
static gboolean   action_delete_confirmed( ofaRecurrentManagePage *self, ofoRecurrentModel *model );
static void       action_on_generate_clicked( GtkButton *button, ofaRecurrentManagePage *self );
static void       action_on_view_clicked( GtkButton *button, ofaRecurrentManagePage *self );
static void       get_settings( ofaRecurrentManagePage *self );
static void       set_settings( ofaRecurrentManagePage *self );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentManagePage, ofa_recurrent_manage_page, OFA_TYPE_PAGE, 0,
		G_ADD_PRIVATE( ofaRecurrentManagePage ))

static void
recurrent_manage_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recurrent_manage_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECURRENT_MANAGE_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_manage_page_parent_class )->finalize( instance );
}

static void
recurrent_manage_page_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_RECURRENT_MANAGE_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		set_settings( OFA_RECURRENT_MANAGE_PAGE( instance ));

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_manage_page_parent_class )->dispose( instance );
}

static void
ofa_recurrent_manage_page_init( ofaRecurrentManagePage *self )
{
	static const gchar *thisfn = "ofa_recurrent_manage_page_init";
	ofaRecurrentManagePagePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECURRENT_MANAGE_PAGE( self ));

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	priv->sort_column_id = -1;
	priv->sort_sens = -1;
}

static void
ofa_recurrent_manage_page_class_init( ofaRecurrentManagePageClass *klass )
{
	static const gchar *thisfn = "ofa_recurrent_manage_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_manage_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_manage_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_recurrent_manage_page_v_setup_view";
	ofaRecurrentManagePagePrivate *priv;
	ofaHub *hub;
	GtkWidget *widget;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_recurrent_manage_page_get_instance_private( OFA_RECURRENT_MANAGE_PAGE( page ));

	hub = ofa_igetter_get_hub( OFA_IGETTER( page ));
	priv->is_writable = ofa_hub_dossier_is_writable( hub );

	get_settings( OFA_RECURRENT_MANAGE_PAGE( page ));

	widget = setup_treeview( OFA_RECURRENT_MANAGE_PAGE( page ));

	return( widget );
}

static GtkWidget *
setup_treeview( ofaRecurrentManagePage *self )
{
	static const gchar *thisfn = "ofa_recurrent_manage_page_setup_treeview";
	ofaRecurrentManagePagePrivate *priv;
	GtkWidget *frame, *scroll, *tview;
	GtkTreeModel *tsort;
	GtkTreeViewColumn *sort_column;
	gint column_id;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	ofaRecurrentModelStore *store;
	ofaHub *hub;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	frame = gtk_frame_new( NULL );
	my_utils_widget_set_margins( frame, 4, 4, 4, 0 );
	gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_IN );

	scroll = gtk_scrolled_window_new( NULL, NULL );
	gtk_container_set_border_width( GTK_CONTAINER( scroll ), 4 );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scroll ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_container_add( GTK_CONTAINER( frame ), scroll );

	tview = gtk_tree_view_new();
	gtk_widget_set_hexpand( tview, TRUE );
	gtk_widget_set_vexpand( tview, TRUE );
	gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( tview ), TRUE );
	gtk_container_add( GTK_CONTAINER( scroll ), tview );
	priv->tview = tview;

	g_signal_connect( tview, "row-activated", G_CALLBACK( tview_on_row_activated ), self );
	g_signal_connect( tview, "key-press-event", G_CALLBACK( tview_on_key_pressed ), self );

	hub = ofa_igetter_get_hub( OFA_IGETTER( self ));
	store = ofa_recurrent_model_store_new( hub );

	g_signal_connect( store, "ofa-inserted", G_CALLBACK( store_on_row_inserted_or_removed ), self );
	g_signal_connect( store, "ofa-inserted", G_CALLBACK( store_on_row_inserted_or_removed ), self );

	tsort = gtk_tree_model_sort_new_with_model( GTK_TREE_MODEL( store ));

	gtk_tree_view_set_model( GTK_TREE_VIEW( tview ), tsort );
	g_object_unref( tsort );

	g_debug( "%s: store=%p, tsort=%p",
			thisfn, ( void * ) store, ( void * ) tsort );

	/* default is to sort by ascending mnemo
	 */
	sort_column = NULL;
	if( priv->sort_column_id < 0 ){
		priv->sort_column_id = REC_MODEL_COL_MNEMO;
	}
	if( priv->sort_sens < 0 ){
		priv->sort_sens = OFA_SORT_ASCENDING;
	}

	column_id = REC_MODEL_COL_MNEMO;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Mnemonic" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	column_id = REC_MODEL_COL_LABEL;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	column_id = REC_MODEL_COL_OPE_TEMPLATE;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Operation" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	column_id = REC_MODEL_COL_PERIODICITY;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Periodicity" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	column_id = REC_MODEL_COL_PERIODICITY_DETAIL;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Detail" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	column_id = REC_MODEL_COL_DEF_AMOUNT1;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Edit. 1" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	column_id = REC_MODEL_COL_DEF_AMOUNT2;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Edit. 2" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	column_id = REC_MODEL_COL_DEF_AMOUNT3;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Edit. 3" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
	gtk_tree_selection_set_mode( select, GTK_SELECTION_MULTIPLE );
	g_signal_connect( select, "changed", G_CALLBACK( tview_on_selection_changed ), self );

	/* default is to sort by ascending operation date
	 */
	g_return_val_if_fail( sort_column && GTK_IS_TREE_VIEW_COLUMN( sort_column ), NULL );
	gtk_tree_view_column_set_sort_indicator( sort_column, TRUE );
	priv->sort_column = sort_column;
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tsort ), priv->sort_column_id, priv->sort_sens );

	return( frame );
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	ofaRecurrentManagePagePrivate *priv;
	ofaButtonsBox *buttons_box;
	GtkWidget *btn;

	priv = ofa_recurrent_manage_page_get_instance_private( OFA_RECURRENT_MANAGE_PAGE( page ));

	buttons_box = ofa_buttons_box_new();
	my_utils_widget_set_margins( GTK_WIDGET( buttons_box ), 4, 4, 0, 0 );

	btn = ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_NEW, G_CALLBACK( action_on_new_clicked ), page );
	gtk_widget_set_sensitive( btn, priv->is_writable );

	priv->update_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_PROPERTIES, G_CALLBACK( action_on_update_clicked ), page );

	priv->delete_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_DELETE, G_CALLBACK( action_on_delete_clicked ), page );

	ofa_buttons_box_add_spacer( buttons_box );

	priv->generate_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, _( "_Generate with selected..." ), G_CALLBACK( action_on_generate_clicked ), page );

	ofa_buttons_box_add_spacer( buttons_box );

	btn = ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, _( "_View waiting operations..." ), G_CALLBACK( action_on_view_clicked ), page );
	gtk_widget_set_sensitive( btn, TRUE );

	return( GTK_WIDGET( buttons_box ));
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaRecurrentManagePagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_RECURRENT_MANAGE_PAGE( page ), NULL );

	priv = ofa_recurrent_manage_page_get_instance_private( OFA_RECURRENT_MANAGE_PAGE( page ));

	return( priv->tview );
}

/*
 * Gtk+ changes automatically the sort order
 * we reset yet the sort column id
 *
 * as a side effect of our inversion of indicators, clicking on a new
 * header makes the sort order descending as the default
 */
static void
tview_on_header_clicked( GtkTreeViewColumn *column, ofaRecurrentManagePage *self )
{
	ofaRecurrentManagePagePrivate *priv;
	gint sort_column_id, new_column_id;
	GtkSortType sort_order;
	GtkTreeModel *tsort;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	gtk_tree_view_column_set_sort_indicator( priv->sort_column, FALSE );
	gtk_tree_view_column_set_sort_indicator( column, TRUE );
	priv->sort_column = column;

	tsort = gtk_tree_view_get_model( GTK_TREE_VIEW( priv->tview ));
	gtk_tree_sortable_get_sort_column_id( GTK_TREE_SORTABLE( tsort ), &sort_column_id, &sort_order );

	new_column_id = gtk_tree_view_column_get_sort_column_id( column );
	gtk_tree_sortable_set_sort_column_id( GTK_TREE_SORTABLE( tsort ), new_column_id, sort_order );

	priv->sort_column_id = new_column_id;
	priv->sort_sens = sort_order;

	set_settings( self );
}

/*
 * sorting the store
 */
static gint
tview_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaRecurrentManagePage *self )
{
	static const gchar *thisfn = "ofa_recurrent_manage_page_tview_on_sort_model";
	ofaRecurrentManagePagePrivate *priv;
	gint cmp;
	gchar *smnemoa, *slabela, *stemplatea, *sperioda, *sdetaila, *sdef1a, *sdef2a, *sdef3a;
	gchar *smnemob, *slabelb, *stemplateb, *speriodb, *sdetailb, *sdef1b, *sdef2b, *sdef3b;

	gtk_tree_model_get( tmodel, a,
			REC_MODEL_COL_MNEMO,              &smnemoa,
			REC_MODEL_COL_LABEL,              &slabela,
			REC_MODEL_COL_OPE_TEMPLATE,       &stemplatea,
			REC_MODEL_COL_PERIODICITY,        &sperioda,
			REC_MODEL_COL_PERIODICITY_DETAIL, &sdetaila,
			REC_MODEL_COL_DEF_AMOUNT1,        &sdef1a,
			REC_MODEL_COL_DEF_AMOUNT2,        &sdef2a,
			REC_MODEL_COL_DEF_AMOUNT3,        &sdef3a,
			-1 );

	gtk_tree_model_get( tmodel, b,
			REC_MODEL_COL_MNEMO,              &smnemob,
			REC_MODEL_COL_LABEL,              &slabelb,
			REC_MODEL_COL_OPE_TEMPLATE,       &stemplateb,
			REC_MODEL_COL_PERIODICITY,        &speriodb,
			REC_MODEL_COL_PERIODICITY_DETAIL, &sdetailb,
			REC_MODEL_COL_DEF_AMOUNT1,        &sdef1b,
			REC_MODEL_COL_DEF_AMOUNT2,        &sdef2b,
			REC_MODEL_COL_DEF_AMOUNT3,        &sdef3b,
			-1 );

	cmp = 0;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	switch( priv->sort_column_id ){
		case REC_MODEL_COL_MNEMO:
			cmp = my_collate( smnemoa, smnemob );
			break;
		case REC_MODEL_COL_LABEL:
			cmp = my_collate( slabela, slabelb );
			break;
		case REC_MODEL_COL_OPE_TEMPLATE:
			cmp = my_collate( stemplatea, stemplateb );
			break;
		case REC_MODEL_COL_PERIODICITY:
			cmp = my_collate( sperioda, speriodb );
			break;
		case REC_MODEL_COL_PERIODICITY_DETAIL:
			cmp = tview_on_sort_detail( sdetaila, sdetailb );
			break;
		case REC_MODEL_COL_DEF_AMOUNT1:
			cmp = my_collate( sdef1a, sdef1b );
			break;
		case REC_MODEL_COL_DEF_AMOUNT2:
			cmp = my_collate( sdef2a, sdef2b );
			break;
		case REC_MODEL_COL_DEF_AMOUNT3:
			cmp = my_collate( sdef3a, sdef3b );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, priv->sort_column_id );
			break;
	}

	g_free( sdef1a );
	g_free( sdef2a );
	g_free( sdef3a );
	g_free( sdetaila );
	g_free( sperioda );
	g_free( stemplatea );
	g_free( slabela );
	g_free( smnemoa );

	g_free( sdef1b );
	g_free( sdef2b );
	g_free( sdef3b );
	g_free( sdetailb );
	g_free( speriodb );
	g_free( stemplateb );
	g_free( slabelb );
	g_free( smnemob );

	/* return -1 if a > b, so that the order indicator points to the smallest:
	 * ^: means from smallest to greatest (ascending order)
	 * v: means from greatest to smallest (descending order)
	 */
	return( -cmp );
}

static gint
tview_on_sort_detail( const gchar *detaila, const gchar *detailb )
{
	int deta, detb;

	if( my_strlen( detaila ) && my_strlen( detailb )){
		deta = atoi( detaila );
		detb = atoi( detailb );
		return( deta < detb ? -1 : ( deta > detb ? 1 : 0 ));
	}

	return( my_collate( detaila, detailb ));
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
tview_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaRecurrentManagePage *self )
{
	gboolean stop;

	stop = FALSE;

	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Insert ){
			action_on_new_clicked( NULL, self );
		} else if( event->keyval == GDK_KEY_Delete ){
			action_try_to_delete_current_row( self );
		}
	}

	return( stop );
}

static void
tview_on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaRecurrentManagePage *self )
{
	action_on_update_clicked( NULL, self );
}

/*
 * selection is multiple mode (0 to n selected rows)
 */
static void
tview_on_selection_changed( GtkTreeSelection *selection, ofaRecurrentManagePage *self )
{
	ofaRecurrentManagePagePrivate *priv;
	GList *selected;
	guint count;
	ofoRecurrentModel *model;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	selected = tview_get_selected( self );
	count = g_list_length( selected );

	model = ( count == 1 ) ? ( ofoRecurrentModel * ) selected->data : NULL;
	g_return_if_fail( count != 1 || ( model && OFO_IS_RECURRENT_MODEL( model )));

	if( priv->update_btn ){
		gtk_widget_set_sensitive( priv->update_btn,
				count == 1 );
	}
	if( priv->delete_btn ){
		gtk_widget_set_sensitive( priv->delete_btn,
				priv->is_writable && count == 1 && ofo_recurrent_model_is_deletable( model ));
	}
	if( priv->generate_btn ){
		gtk_widget_set_sensitive( priv->generate_btn,
				priv->is_writable && count > 0 );
	}

	g_list_free( selected );
}

/*
 * Returns a GList of selected ofoRecurrentModel objects
 *
 * The returned list should be #g_list_free() by the caller.
 */
static GList *
tview_get_selected( ofaRecurrentManagePage *self )
{
	ofaRecurrentManagePagePrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GList *objects, *paths, *it;
	ofoRecurrentModel *object;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	objects = NULL;

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->tview ));
	paths = gtk_tree_selection_get_selected_rows( select, &tmodel );

	if( paths ){
		for( it=paths ; it ; it=it->next ){
			if( gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) it->data )){
				gtk_tree_model_get( tmodel, &iter, REC_MODEL_COL_OBJECT, &object, -1 );
				g_return_val_if_fail( object && OFO_IS_RECURRENT_MODEL( object ), NULL );
				g_object_unref( object );
				objects = g_list_prepend( objects, object );
			}
		}
		g_list_free_full( paths, ( GDestroyNotify ) gtk_tree_path_free );
	}

	return( objects );
}

/**
 * ofa_recurrent_manage_page_get_selected:
 * @page: this #ofaRecurrentManagePage page.
 *
 * Returns: a GList of selected ofoRecurrentModel objects.
 *
 * The returned list should be #g_list_free() by the caller.
 */
GList *
ofa_recurrent_manage_page_get_selected( ofaRecurrentManagePage *page )
{
	g_return_val_if_fail( page && OFA_IS_RECURRENT_MANAGE_PAGE( page ), NULL );
	g_return_val_if_fail( !OFA_PAGE( page )->prot->dispose_has_run, NULL );

	return( tview_get_selected( page ));
}

static void
store_on_row_inserted_or_removed( ofaRecurrentModelStore *store, ofaRecurrentManagePage *self )
{
	ofaRecurrentManagePagePrivate *priv;
	GList *dataset;
	guint count;
	ofaHub *hub;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	hub = ofa_igetter_get_hub( OFA_IGETTER( self ));
	dataset = ofo_recurrent_model_get_dataset( hub );
	count = g_list_length( dataset );

	gtk_widget_set_sensitive( priv->generate_btn, count > 0 );
}

/*
 * create a new recurrent model
 * creating a new recurrent record is the rule of 'Declare' button
 */
static void
action_on_new_clicked( GtkButton *button, ofaRecurrentManagePage *self )
{
	ofoRecurrentModel *model;
	GtkWindow *toplevel;

	model = ofo_recurrent_model_new();
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_recurrent_model_properties_run( OFA_IGETTER( self ), toplevel, model );
}

/*
 * Update button is expected to be sensitive when the selection count is 1
 * (and dossier is writable, and record is updatable)
 */
static void
action_on_update_clicked( GtkButton *button, ofaRecurrentManagePage *self )
{
	GList *selected;
	ofoRecurrentModel *model;
	GtkWindow *toplevel;

	selected = tview_get_selected( self );
	g_return_if_fail( g_list_length( selected ) == 1 );

	model = ( ofoRecurrentModel * ) selected->data;
	g_return_if_fail( model && OFO_IS_RECURRENT_MODEL( model ));

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_recurrent_model_properties_run( OFA_IGETTER( self ), toplevel, model );

	g_list_free( selected );
}

/*
 * Delete button is expected to be sensitive when the selection count is 1
 * (and dossier is writable, and record is deletable)
 */
static void
action_on_delete_clicked( GtkButton *button, ofaRecurrentManagePage *self )
{
	GList *selected;
	ofoRecurrentModel *model;

	selected = tview_get_selected( self );
	g_return_if_fail( g_list_length( selected ) == 1 );

	model = ( ofoRecurrentModel * ) selected->data;
	g_return_if_fail( model && OFO_IS_RECURRENT_MODEL( model ));

	action_do_delete( self, model );

	g_list_free( selected );
	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( self )));
}

/*
 * when pressing the 'Delete' key on the treeview
 * cannot be sure that the current row is deletable
 */
static void
action_try_to_delete_current_row( ofaRecurrentManagePage *self )
{
	GList *selected;
	ofoRecurrentModel *model;

	selected = tview_get_selected( self );
	if( g_list_length( selected ) == 1 ){
		model = ( ofoRecurrentModel * ) selected->data;
		g_return_if_fail( model && OFO_IS_RECURRENT_MODEL( model ));

		if( ofo_recurrent_model_is_deletable( model )){
			action_do_delete( self, model );
		}
	}

	g_list_free( selected );
}

static void
action_do_delete( ofaRecurrentManagePage *self, ofoRecurrentModel *model )
{
	g_return_if_fail( ofo_recurrent_model_is_deletable( model ));

	if( action_delete_confirmed( self, model )){
		ofo_recurrent_model_delete( model );
		/* taken into account by dossier signaling system */
	}
}

static gboolean
action_delete_confirmed( ofaRecurrentManagePage *self, ofoRecurrentModel *model )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s' recurrent model ?" ),
				ofo_recurrent_model_get_mnemo( model ));

	delete_ok = my_utils_dialog_question( msg, _( "_Delete" ));

	g_free( msg );

	return( delete_ok );
}

/*
 * generating new operations on current selection
 */
static void
action_on_generate_clicked( GtkButton *button, ofaRecurrentManagePage *self )
{
	GtkWindow *toplevel;

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));

	ofa_recurrent_new_run( OFA_IGETTER( self ), toplevel, self );
}

/*
 * opening the Run page
 */
static void
action_on_view_clicked( GtkButton *button, ofaRecurrentManagePage *self )
{
	ofaIThemeManager *manager;

	manager = ofa_igetter_get_theme_manager( OFA_IGETTER( self ));
	ofa_itheme_manager_activate( manager, OFA_TYPE_RECURRENT_RUN_PAGE );
}

/*
 * settings: sort_column_id;sort_sens;
 */
static void
get_settings( ofaRecurrentManagePage *self )
{
	ofaRecurrentManagePagePrivate *priv;
	GList *slist, *it;
	const gchar *cstr;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	slist = ofa_settings_user_get_string_list( st_page_settings );

	it = slist ? slist : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->sort_column_id = atoi( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->sort_sens = atoi( cstr );
	}

	ofa_settings_free_string_list( slist );
}

static void
set_settings( ofaRecurrentManagePage *self )
{
	ofaRecurrentManagePagePrivate *priv;
	gchar *str;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	str = g_strdup_printf( "%d;%d;", priv->sort_column_id, priv->sort_sens );

	ofa_settings_user_set_string( st_page_settings, str );

	g_free( str );
}
