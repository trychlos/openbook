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

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-buttons-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofo-dossier.h"

#include "tva/ofa-tva-declare-page.h"
#include "tva/ofa-tva-record-properties.h"
#include "tva/ofa-tva-record-store.h"
#include "tva/ofo-tva-record.h"

/* priv instance data
 */
typedef struct {

	/* internals
	 */
	gboolean           is_writable;

	/* UI
	 */
	GtkWidget         *record_treeview;
	GtkWidget         *update_btn;
	GtkWidget         *delete_btn;

	/* sorting the view
	 */
	gint               sort_column_id;
	gint               sort_sens;
	GtkTreeViewColumn *sort_column;
}
	ofaTVADeclarePagePrivate;

/* it appears that Gtk+ displays a counter intuitive sort indicator:
 * when asking for ascending sort, Gtk+ displays a 'v' indicator
 * while we would prefer the '^' version -
 * we are defining the inverse indicator, and we are going to sort
 * in reverse order to have our own illusion
 */
#define OFA_SORT_ASCENDING              GTK_SORT_DESCENDING
#define OFA_SORT_DESCENDING             GTK_SORT_ASCENDING

static GtkWidget    *v_setup_view( ofaPage *page );
static GtkWidget    *setup_record_treeview( ofaTVADeclarePage *self );
static GtkWidget    *v_setup_buttons( ofaPage *page );
static GtkWidget    *v_get_top_focusable_widget( const ofaPage *page );
static void          tview_on_header_clicked( GtkTreeViewColumn *column, ofaTVADeclarePage *self );
static gint          tview_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaTVADeclarePage *self );
static gboolean      tview_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaTVADeclarePage *self );
static void          tview_on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaTVADeclarePage *page );
static void          tview_on_row_selected( GtkTreeSelection *selection, ofaTVADeclarePage *self );
static ofoTVARecord *tview_get_selected( ofaTVADeclarePage *page, GtkTreeModel **tmodel, GtkTreeIter *iter );
static void          on_update_clicked( GtkButton *button, ofaTVADeclarePage *page );
static void          on_delete_clicked( GtkButton *button, ofaTVADeclarePage *page );
static void          try_to_delete_current_row( ofaTVADeclarePage *page );
static void          do_delete( ofaTVADeclarePage *page, ofoTVARecord *record, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean      delete_confirmed( ofaTVADeclarePage *self, ofoTVARecord *record );
static gboolean      find_row_by_ptr( ofaTVADeclarePage *page, ofoTVARecord *record, GtkTreeModel **tmodel, GtkTreeIter *iter );

G_DEFINE_TYPE_EXTENDED( ofaTVADeclarePage, ofa_tva_declare_page, OFA_TYPE_PAGE, 0,
		G_ADD_PRIVATE( ofaTVADeclarePage ))

static void
tva_declare_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_declare_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_DECLARE_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_declare_page_parent_class )->finalize( instance );
}

static void
tva_declare_page_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_TVA_DECLARE_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_declare_page_parent_class )->dispose( instance );
}

static void
ofa_tva_declare_page_init( ofaTVADeclarePage *self )
{
	static const gchar *thisfn = "ofa_tva_declare_page_init";
	ofaTVADeclarePagePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_TVA_DECLARE_PAGE( self ));

	priv = ofa_tva_declare_page_get_instance_private( self );

	priv->sort_column_id = -1;
	priv->sort_sens = -1;
}

static void
ofa_tva_declare_page_class_init( ofaTVADeclarePageClass *klass )
{
	static const gchar *thisfn = "ofa_tva_declare_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_declare_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_declare_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_tva_declare_page_v_setup_view";
	ofaTVADeclarePagePrivate *priv;
	GtkWidget *grid, *widget;
	ofaHub *hub;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_tva_declare_page_get_instance_private( OFA_TVA_DECLARE_PAGE( page ));

	hub = ofa_igetter_get_hub( OFA_IGETTER( page ));
	priv->is_writable = ofa_hub_dossier_is_writable( hub );

	grid = gtk_grid_new();

	widget = setup_record_treeview( OFA_TVA_DECLARE_PAGE( page ));
	gtk_grid_attach( GTK_GRID( grid ), widget, 0, 0, 1, 1 );

	return( grid );
}

/*
 * returns the container which displays the TVA form
 */
static GtkWidget *
setup_record_treeview( ofaTVADeclarePage *self )
{
	ofaTVADeclarePagePrivate *priv;
	GtkWidget *frame, *scrolled, *tview;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	ofaTVARecordStore *store;
	ofaHub *hub;
	GtkTreeViewColumn *sort_column;
	gint column_id;
	GtkTreeModel *tsort;

	priv = ofa_tva_declare_page_get_instance_private( self );

	frame = gtk_frame_new( NULL );
	my_utils_widget_set_margins( frame, 4, 4, 4, 0 );
	gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_IN );

	scrolled = gtk_scrolled_window_new( NULL, NULL );
	gtk_container_add( GTK_CONTAINER( frame ), scrolled );

	tview = gtk_tree_view_new();
	gtk_widget_set_hexpand( tview, TRUE );
	gtk_widget_set_vexpand( tview, TRUE );
	gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( tview ), TRUE );
	g_signal_connect( tview, "row-activated", G_CALLBACK( tview_on_row_activated ), self );
	g_signal_connect( tview, "key-press-event", G_CALLBACK( tview_on_key_pressed ), self );
	gtk_container_add( GTK_CONTAINER( scrolled ), tview );

	hub = ofa_igetter_get_hub( OFA_IGETTER( self ));
	store = ofa_tva_record_store_new( hub );

	tsort = gtk_tree_model_sort_new_with_model( GTK_TREE_MODEL( store ));

	gtk_tree_view_set_model( GTK_TREE_VIEW( tview ), tsort );
	g_object_unref( tsort );

	/* default is to sort by descending operation date
	 */
	sort_column = NULL;
	if( priv->sort_column_id < 0 ){
		priv->sort_column_id = TVA_RECORD_COL_END;
	}
	if( priv->sort_sens < 0 ){
		priv->sort_sens = OFA_SORT_DESCENDING;
	}

	column_id = TVA_RECORD_COL_MNEMO;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Mnemo" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	column_id = TVA_RECORD_COL_LABEL;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	column_id = TVA_RECORD_COL_BEGIN;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Begin" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	column_id = TVA_RECORD_COL_END;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "End" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	column_id = TVA_RECORD_COL_IS_VALIDATED;
	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 0.5, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Validated" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	column_id = TVA_RECORD_COL_DOPE;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Operation" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( select, "changed", G_CALLBACK( tview_on_row_selected ), self );

	/* default is to sort by ascending operation date
	 */
	g_return_val_if_fail( sort_column && GTK_IS_TREE_VIEW_COLUMN( sort_column ), NULL );
	gtk_tree_view_column_set_sort_indicator( sort_column, TRUE );
	priv->sort_column = sort_column;
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tsort ), priv->sort_column_id, priv->sort_sens );

	priv->record_treeview = tview;

	return( frame );
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	ofaTVADeclarePagePrivate *priv;
	ofaButtonsBox *buttons_box;

	priv = ofa_tva_declare_page_get_instance_private( OFA_TVA_DECLARE_PAGE( page ));

	buttons_box = ofa_buttons_box_new();
	my_utils_widget_set_margins( GTK_WIDGET( buttons_box ), 4, 4, 0, 0 );

	/* always disabled */
	ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_NEW, NULL, NULL );

	priv->update_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_PROPERTIES, G_CALLBACK( on_update_clicked ), page );

	priv->delete_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_DELETE, G_CALLBACK( on_delete_clicked ), page );

	ofa_buttons_box_add_spacer( buttons_box );

	return( GTK_WIDGET( buttons_box ));
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaTVADeclarePagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_TVA_DECLARE_PAGE( page ), NULL );

	priv = ofa_tva_declare_page_get_instance_private( OFA_TVA_DECLARE_PAGE( page ));

	return( priv->record_treeview );
}

/*
 * Gtk+ changes automatically the sort order
 * we reset yet the sort column id
 *
 * as a side effect of our inversion of indicators, clicking on a new
 * header makes the sort order descending as the default
 */
static void
tview_on_header_clicked( GtkTreeViewColumn *column, ofaTVADeclarePage *self )
{
	ofaTVADeclarePagePrivate *priv;
	gint sort_column_id, new_column_id;
	GtkSortType sort_order;
	GtkTreeModel *tsort;

	priv = ofa_tva_declare_page_get_instance_private( self );

	gtk_tree_view_column_set_sort_indicator( priv->sort_column, FALSE );
	gtk_tree_view_column_set_sort_indicator( column, TRUE );
	priv->sort_column = column;

	tsort = gtk_tree_view_get_model( GTK_TREE_VIEW( priv->record_treeview ));
	gtk_tree_sortable_get_sort_column_id( GTK_TREE_SORTABLE( tsort ), &sort_column_id, &sort_order );

	new_column_id = gtk_tree_view_column_get_sort_column_id( column );
	gtk_tree_sortable_set_sort_column_id( GTK_TREE_SORTABLE( tsort ), new_column_id, sort_order );

	priv->sort_column_id = new_column_id;
	priv->sort_sens = sort_order;
}

/*
 * sorting the store
 */
static gint
tview_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaTVADeclarePage *self )
{
	static const gchar *thisfn = "ofa_tva_declare_page_tview_on_sort_model";
	ofaTVADeclarePagePrivate *priv;
	gint cmp;
	gchar *smnemoa, *slabela, *valida, *begina, *enda, *dopea;
	gchar *smnemob, *slabelb, *validb, *beginb, *endb, *dopeb;

	gtk_tree_model_get( tmodel, a,
			TVA_RECORD_COL_MNEMO,        &smnemoa,
			TVA_RECORD_COL_LABEL,        &slabela,
			TVA_RECORD_COL_IS_VALIDATED, &valida,
			TVA_RECORD_COL_BEGIN,        &begina,
			TVA_RECORD_COL_END,          &enda,
			TVA_RECORD_COL_DOPE,         &dopea,
			-1 );

	gtk_tree_model_get( tmodel, b,
			TVA_RECORD_COL_MNEMO,        &smnemob,
			TVA_RECORD_COL_LABEL,        &slabelb,
			TVA_RECORD_COL_IS_VALIDATED, &validb,
			TVA_RECORD_COL_BEGIN,        &beginb,
			TVA_RECORD_COL_END,          &endb,
			TVA_RECORD_COL_DOPE,         &dopeb,
			-1 );

	cmp = 0;

	priv = ofa_tva_declare_page_get_instance_private( self );

	switch( priv->sort_column_id ){
		case TVA_RECORD_COL_MNEMO:
			cmp = my_collate( smnemoa, smnemob );
			break;
		case TVA_RECORD_COL_LABEL:
			cmp = my_collate( slabela, slabelb );
			break;
		case TVA_RECORD_COL_IS_VALIDATED:
			cmp = my_collate( valida, validb );
			break;
		case TVA_RECORD_COL_BEGIN:
			cmp = my_date_compare_by_str( begina, beginb, ofa_prefs_date_display());
			break;
		case TVA_RECORD_COL_END:
			cmp = my_date_compare_by_str( enda, endb, ofa_prefs_date_display());
			break;
		case TVA_RECORD_COL_DOPE:
			cmp = my_date_compare_by_str( dopea, dopeb, ofa_prefs_date_display());
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, priv->sort_column_id );
			break;
	}

	g_free( dopea );
	g_free( enda );
	g_free( begina );
	g_free( valida );
	g_free( slabela );
	g_free( smnemoa );

	g_free( dopeb );
	g_free( endb );
	g_free( beginb );
	g_free( validb );
	g_free( slabelb );
	g_free( smnemob );

	/* return -1 if a > b, so that the order indicator points to the smallest:
	 * ^: means from smallest to greatest (ascending order)
	 * v: means from greatest to smallest (descending order)
	 */
	return( -cmp );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
tview_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaTVADeclarePage *self )
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

static void
tview_on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaTVADeclarePage *page )
{
	on_update_clicked( NULL, page );
}

static void
tview_on_row_selected( GtkTreeSelection *selection, ofaTVADeclarePage *self )
{
	ofaTVADeclarePagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVARecord *record;
	gboolean is_record;

	priv = ofa_tva_declare_page_get_instance_private( self );

	record = tview_get_selected( self, &tmodel, &iter );
	is_record = record && OFO_IS_TVA_RECORD( record );

	if( priv->update_btn ){
		gtk_widget_set_sensitive( priv->update_btn,
				is_record );
	}

	if( priv->delete_btn ){
		gtk_widget_set_sensitive( priv->delete_btn,
				priv->is_writable && is_record && ofo_tva_record_is_deletable( record ));
	}
}

static ofoTVARecord *
tview_get_selected( ofaTVADeclarePage *page, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	ofaTVADeclarePagePrivate *priv;
	GtkTreeSelection *select;
	ofoTVARecord *object;

	priv = ofa_tva_declare_page_get_instance_private( page );

	object = NULL;

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->record_treeview ));
	if( gtk_tree_selection_get_selected( select, tmodel, iter )){
		gtk_tree_model_get( *tmodel, iter, TVA_RECORD_COL_OBJECT, &object, -1 );
		g_return_val_if_fail( object && OFO_IS_TVA_RECORD( object ), NULL );
		g_object_unref( object );
	}

	return( object );
}

static void
on_update_clicked( GtkButton *button, ofaTVADeclarePage *page )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVARecord *record;
	GtkWindow *toplevel;

	record = tview_get_selected( page, &tmodel, &iter );
	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( page ));
	ofa_tva_record_properties_run( OFA_IGETTER( page ), toplevel, record );
	/* update is taken into account by dossier signaling system */

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( page )));
}

static void
on_delete_clicked( GtkButton *button, ofaTVADeclarePage *page )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVARecord *record;

	record = tview_get_selected( page, &tmodel, &iter );
	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));

	do_delete( page, record, tmodel, &iter );

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( page )));
}

/*
 * when pressing the 'Delete' key on the treeview
 * cannot be sure that the current row is deletable
 */
static void
try_to_delete_current_row( ofaTVADeclarePage *page )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVARecord *record;

	record = tview_get_selected( page, &tmodel, &iter );
	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));

	if( ofo_tva_record_is_deletable( record )){
		do_delete( page, record, tmodel, &iter );
	}
}

static void
do_delete( ofaTVADeclarePage *page, ofoTVARecord *record, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	g_return_if_fail( ofo_tva_record_is_deletable( record ));

	if( delete_confirmed( page, record )){
		ofo_tva_record_delete( record );
		/* taken into account by dossier signaling system */
	}
}

static gboolean
delete_confirmed( ofaTVADeclarePage *self, ofoTVARecord *record )
{
	gchar *msg, *send;
	gboolean delete_ok;

	send = my_date_to_str( ofo_tva_record_get_end( record ), ofa_prefs_date_display());
	msg = g_strdup_printf( _( "Are you sure you want delete the %s at %s TVA declaration ?" ),
				ofo_tva_record_get_mnemo( record ), send );

	delete_ok = my_utils_dialog_question( msg, _( "_Delete" ));

	g_free( msg );
	g_free( send );

	return( delete_ok );
}

/**
 * ofa_tva_declare_page_set_selected:
 * @page: this #ofaTVADeclarePage instance.
 * @record: the #ofoTVARecord to be selected.
 *
 * Select the specified @record.
 */
void
ofa_tva_declare_page_set_selected( ofaTVADeclarePage *page, ofoTVARecord *record )
{
	ofaTVADeclarePagePrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;

	g_return_if_fail( page && OFA_IS_TVA_DECLARE_PAGE( page ));
	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
	g_return_if_fail( !OFA_PAGE( page )->prot->dispose_has_run );

	priv = ofa_tva_declare_page_get_instance_private( page );

	if( find_row_by_ptr( page, record, &tmodel, &iter )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->record_treeview ));
		path = gtk_tree_model_get_path( tmodel, &iter );
		gtk_tree_selection_select_path( select, path );
		gtk_tree_path_free( path );
	}

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( page )));
}

static gboolean
find_row_by_ptr( ofaTVADeclarePage *page, ofoTVARecord *record, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	ofaTVADeclarePagePrivate *priv;
	ofoTVARecord *row_object;
	const gchar *mnemo;
	const GDate *dend;
	gint cmp;

	priv = ofa_tva_declare_page_get_instance_private( page );

	*tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( priv->record_treeview ));
	mnemo = ofo_tva_record_get_mnemo( record );
	dend = ofo_tva_record_get_end( record );
	if( gtk_tree_model_get_iter_first( *tmodel, iter )){
		while( TRUE ){
			gtk_tree_model_get( *tmodel, iter, TVA_RECORD_COL_OBJECT, &row_object, -1 );
			cmp = ofo_tva_record_compare_by_key( row_object, mnemo, dend );
			g_object_unref( row_object );
			if( cmp == 0 ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( *tmodel, iter )){
				break;
			}
		}
	}

	return( FALSE );
}
