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
#include <string.h>

#include "my/my-utils.h"

#include "api/ofa-buttons-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-settings.h"
#include "api/ofo-currency.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"

#include "core/ofa-currency-store.h"

#include "ui/ofa-currency-properties.h"
#include "ui/ofa-currency-page.h"

/* private instance data
 */
typedef struct {

	/* internals
	 */
	ofaHub            *hub;
	GList             *hub_handlers;
	gboolean           is_writable;

	/* UI
	 */
	ofaCurrencyStore  *store;
	GtkTreeView       *tview;			/* the main treeview of the page */
	GtkWidget         *update_btn;
	GtkWidget         *delete_btn;

	/* sorted model
	 */
	GtkTreeModel      *tsort;			/* a sort model stacked on top of the currency store */
	GtkTreeViewColumn *sort_column;
	gint               sort_column_id;
	gint               sort_sens;
}
	ofaCurrencyPagePrivate;

/* it appears that Gtk+ displays a counter intuitive sort indicator:
 * when asking for ascending sort, Gtk+ displays a 'v' indicator
 * while we would prefer the '^' version -
 * we are defining the inverse indicator, and we are going to sort
 * in reverse order to have our own illusion
 */
#define OFA_SORT_ASCENDING                 GTK_SORT_DESCENDING
#define OFA_SORT_DESCENDING                GTK_SORT_ASCENDING

static const gchar *st_sort_settings    = "ofaCurrencyPage-sort";

static GtkWidget   *v_setup_view( ofaPage *page );
static GtkWidget   *setup_tree_view( ofaCurrencyPage *self );
static void         tview_on_header_clicked( GtkTreeViewColumn *column, ofaCurrencyPage *self );
static gint         tview_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaCurrencyPage *self );
static gint         tview_on_sort_png( const GdkPixbuf *pnga, const GdkPixbuf *pngb );
static gboolean     tview_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaCurrencyPage *self );
static ofoCurrency *tview_get_selected( ofaCurrencyPage *page, GtkTreeModel **tmodel, GtkTreeIter *iter );
static GtkWidget   *v_setup_buttons( ofaPage *page );
static GtkWidget   *v_get_top_focusable_widget( const ofaPage *page );
static void         setup_first_selection( ofaCurrencyPage *self );
static void         on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page );
static void         on_currency_selected( GtkTreeSelection *selection, ofaCurrencyPage *self );
static void         on_new_clicked( GtkButton *button, ofaCurrencyPage *page );
/*
static void         select_row_by_code( ofaCurrencyPage *page, const gchar *code );
static gboolean     find_row_by_code( ofaCurrencyPage *page, const gchar *code, GtkTreeIter *iter );
*/
static void         on_update_clicked( GtkButton *button, ofaCurrencyPage *page );
static void         on_delete_clicked( GtkButton *button, ofaCurrencyPage *page );
static void         try_to_delete_current_row( ofaCurrencyPage *page );
static gboolean     delete_confirmed( ofaCurrencyPage *self, ofoCurrency *currency );
static void         do_delete( ofaCurrencyPage *page, ofoCurrency *currency, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void         on_hub_new_object( ofaHub *hub, ofoBase *object, ofaCurrencyPage *self );
static void         on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaCurrencyPage *self );
static void         do_on_updated_account( ofaCurrencyPage *self, ofoAccount *account );
static void         on_hub_deleted_object( ofaHub *hub, ofoBase *object, ofaCurrencyPage *self );
static void         on_hub_reload_dataset( ofaHub *hub, GType type, ofaCurrencyPage *self );
static void         get_sort_settings( ofaCurrencyPage *self );
static void         set_sort_settings( ofaCurrencyPage *self );

G_DEFINE_TYPE_EXTENDED( ofaCurrencyPage, ofa_currency_page, OFA_TYPE_PAGE, 0,
		G_ADD_PRIVATE( ofaCurrencyPage ))

static void
currencies_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_currency_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CURRENCY_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currency_page_parent_class )->finalize( instance );
}

static void
currencies_page_dispose( GObject *instance )
{
	ofaCurrencyPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_CURRENCY_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = ofa_currency_page_get_instance_private( OFA_CURRENCY_PAGE( instance ));

		/* note when deconnecting the handlers that the dossier may
		 * have been already finalized (e.g. when the application
		 * terminates) */
		ofa_hub_disconnect_handlers( priv->hub, &priv->hub_handlers );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currency_page_parent_class )->dispose( instance );
}

static void
ofa_currency_page_init( ofaCurrencyPage *self )
{
	static const gchar *thisfn = "ofa_currency_page_init";
	ofaCurrencyPagePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_CURRENCY_PAGE( self ));

	priv = ofa_currency_page_get_instance_private( self );

	priv->hub_handlers = NULL;
}

static void
ofa_currency_page_class_init( ofaCurrencyPageClass *klass )
{
	static const gchar *thisfn = "ofa_currency_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = currencies_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = currencies_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_currency_page_v_setup_view";
	ofaCurrencyPagePrivate *priv;
	gulong handler;
	GtkWidget *tview;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_currency_page_get_instance_private( OFA_CURRENCY_PAGE( page ));

	priv->hub = ofa_igetter_get_hub( OFA_IGETTER( page ));
	g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );
	priv->is_writable = ofa_hub_dossier_is_writable( priv->hub );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_NEW, G_CALLBACK( on_hub_new_object ), page );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_UPDATED, G_CALLBACK( on_hub_updated_object ), page );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_DELETED, G_CALLBACK( on_hub_deleted_object ), page );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_RELOAD, G_CALLBACK( on_hub_reload_dataset ), page );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	tview = setup_tree_view( OFA_CURRENCY_PAGE( page ));
	setup_first_selection( OFA_CURRENCY_PAGE( page ));

	return( tview );
}

static GtkWidget *
setup_tree_view( ofaCurrencyPage *self )
{
	ofaCurrencyPagePrivate *priv;
	GtkFrame *frame;
	GtkScrolledWindow *scroll;
	GtkTreeView *tview;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	gint column_id;

	priv = ofa_currency_page_get_instance_private( self );

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	my_utils_widget_set_margins( GTK_WIDGET( frame ), 4, 4, 4, 0 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_container_set_border_width( GTK_CONTAINER( scroll ), 4 );
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( scroll ));

	tview = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_hexpand( GTK_WIDGET( tview ), TRUE );
	gtk_widget_set_vexpand( GTK_WIDGET( tview ), TRUE );
	gtk_tree_view_set_headers_visible( tview, TRUE );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( tview ));
	g_signal_connect(G_OBJECT( tview ),
			"row-activated", G_CALLBACK( on_row_activated ), self );
	g_signal_connect(
			G_OBJECT( tview ), "key-press-event", G_CALLBACK( tview_on_key_pressed ), self );
	priv->tview = tview;

	priv->store = ofa_currency_store_new( priv->hub );

	priv->tsort = gtk_tree_model_sort_new_with_model( GTK_TREE_MODEL( priv->store ));
	gtk_tree_view_set_model( priv->tview, priv->tsort );
	g_object_unref( priv->tsort );

	/* default is to sort by ascending iso 3a identifier
	 */
	priv->sort_column = NULL;
	get_sort_settings( self );
	if( priv->sort_column_id < 0 ){
		priv->sort_column_id = CURRENCY_COL_CODE;
	}
	if( priv->sort_sens < 0 ){
		priv->sort_sens = OFA_SORT_ASCENDING;
	}

	/* iso 3a identifier */
	column_id = CURRENCY_COL_CODE;
	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "ISO 3A code" ),
			cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		priv->sort_column = column;
	}

	/* label */
	column_id = CURRENCY_COL_LABEL;
	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		priv->sort_column = column;
	}

	/* notes indicator */
	column_id = CURRENCY_COL_NOTES_PNG;
	cell = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(
				"", cell, "pixbuf", column_id, NULL );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		priv->sort_column = column;
	}

	/* symbol */
	column_id = CURRENCY_COL_SYMBOL;
	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Symbol" ),
			cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		priv->sort_column = column;
	}

	/* decimal digits count */
	column_id = CURRENCY_COL_DIGITS;
	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Digits" ),
			cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		priv->sort_column = column;
	}

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_currency_selected ), self );

	return( GTK_WIDGET( frame ));
}

/*
 * Gtk+ changes automatically the sort order
 * we reset yet the sort column id
 *
 * as a side effect of our inversion of indicators, clicking on a new
 * header makes the sort order descending as the default
 */
static void
tview_on_header_clicked( GtkTreeViewColumn *column, ofaCurrencyPage *self )
{
	ofaCurrencyPagePrivate *priv;
	gint sort_column_id, new_column_id;
	GtkSortType sort_order;

	priv = ofa_currency_page_get_instance_private( self );

	gtk_tree_view_column_set_sort_indicator( priv->sort_column, FALSE );
	gtk_tree_view_column_set_sort_indicator( column, TRUE );
	priv->sort_column = column;

	gtk_tree_sortable_get_sort_column_id( GTK_TREE_SORTABLE( priv->tsort ), &sort_column_id, &sort_order );

	new_column_id = gtk_tree_view_column_get_sort_column_id( column );
	gtk_tree_sortable_set_sort_column_id( GTK_TREE_SORTABLE( priv->tsort ), new_column_id, sort_order );

	priv->sort_column_id = new_column_id;
	priv->sort_sens = sort_order;

	set_sort_settings( self );
}

/*
 * sorting the treeview
 */
static gint
tview_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaCurrencyPage *self )
{
	static const gchar *thisfn = "ofa_currency_page_tview_on_sort_model";
	ofaCurrencyPagePrivate *priv;
	gint cmp;
	gchar *codea, *labela, *symbola, *digitsa, *notesa, *usera, *stampa;
	gchar *codeb, *labelb, *symbolb, *digitsb, *notesb, *userb, *stampb;
	GdkPixbuf *pnga, *pngb;

	gtk_tree_model_get( tmodel, a,
			CURRENCY_COL_CODE,      &codea,
			CURRENCY_COL_LABEL,     &labela,
			CURRENCY_COL_SYMBOL,    &symbola,
			CURRENCY_COL_DIGITS,    &digitsa,
			CURRENCY_COL_NOTES,     &notesa,
			CURRENCY_COL_NOTES_PNG, &pnga,
			CURRENCY_COL_UPD_USER,  &usera,
			CURRENCY_COL_UPD_STAMP, &stampa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			CURRENCY_COL_CODE,      &codeb,
			CURRENCY_COL_LABEL,     &labelb,
			CURRENCY_COL_SYMBOL,    &symbolb,
			CURRENCY_COL_DIGITS,    &digitsb,
			CURRENCY_COL_NOTES,     &notesb,
			CURRENCY_COL_NOTES_PNG, &pngb,
			CURRENCY_COL_UPD_USER,  &userb,
			CURRENCY_COL_UPD_STAMP, &stampb,
			-1 );

	cmp = 0;
	priv = ofa_currency_page_get_instance_private( self );

	switch( priv->sort_column_id ){
		case CURRENCY_COL_CODE:
			cmp = my_collate( codea, codeb );
			break;
		case CURRENCY_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case CURRENCY_COL_SYMBOL:
			cmp = my_collate( symbola, symbolb );
			break;
		case CURRENCY_COL_DIGITS:
			cmp = my_collate( digitsa, digitsb );
			break;
		case CURRENCY_COL_NOTES:
			cmp = my_collate( notesa, notesb );
			break;
		case CURRENCY_COL_NOTES_PNG:
			cmp = tview_on_sort_png( pnga, pngb );
			break;
		case CURRENCY_COL_UPD_USER:
			cmp = my_collate( usera, userb );
			break;
		case CURRENCY_COL_UPD_STAMP:
			cmp = my_collate( stampa, stampb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, priv->sort_column_id );
			break;
	}

	g_free( codea );
	g_free( labela );
	g_free( symbola );
	g_free( digitsa );
	g_free( notesa );
	g_free( usera );
	g_free( stampa );
	g_object_unref( pnga );

	g_free( codeb );
	g_free( labelb );
	g_free( symbolb );
	g_free( digitsb );
	g_free( notesb );
	g_free( userb );
	g_free( stampb );
	g_object_unref( pngb );

	/* return -1 if a > b, so that the order indicator points to the smallest:
	 * ^: means from smallest to greatest (ascending order)
	 * v: means from greatest to smallest (descending order)
	 */
	return( -cmp );
}

static gint
tview_on_sort_png( const GdkPixbuf *pnga, const GdkPixbuf *pngb )
{
	gsize lena, lenb;

	if( !pnga ){
		return( -1 );
	}
	lena = gdk_pixbuf_get_byte_length( pnga );

	if( !pngb ){
		return( 1 );
	}
	lenb = gdk_pixbuf_get_byte_length( pngb );

	if( lena < lenb ){
		return( -1 );
	}
	if( lena > lenb ){
		return( 1 );
	}

	return( memcmp( pnga, pngb, lena ));
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
tview_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaCurrencyPage *self )
{
	gboolean stop;

	stop = FALSE;

	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Insert ){
			on_new_clicked( NULL, self );
		} else if( event->keyval == GDK_KEY_Delete ){
			try_to_delete_current_row( self );
		}
	}

	return( stop );
}

static ofoCurrency *
tview_get_selected( ofaCurrencyPage *page, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	ofaCurrencyPagePrivate *priv;
	GtkTreeSelection *select;
	ofoCurrency *currency;

	priv = ofa_currency_page_get_instance_private( page );

	currency = NULL;

	select = gtk_tree_view_get_selection( priv->tview );
	if( gtk_tree_selection_get_selected( select, tmodel, iter )){

		gtk_tree_model_get( *tmodel, iter, CURRENCY_COL_OBJECT, &currency, -1 );
		g_object_unref( currency );
	}

	return( currency );
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	ofaCurrencyPagePrivate *priv;
	ofaButtonsBox *buttons_box;
	GtkWidget *btn;

	priv = ofa_currency_page_get_instance_private( OFA_CURRENCY_PAGE( page ));

	buttons_box = ofa_buttons_box_new();
	my_utils_widget_set_margins( GTK_WIDGET( buttons_box ), 4, 4, 0, 0 );

	btn = ofa_buttons_box_add_button_with_mnemonic(
			buttons_box, BUTTON_NEW, G_CALLBACK( on_new_clicked ), page );
	gtk_widget_set_sensitive( btn, priv->is_writable );

	priv->update_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_PROPERTIES, G_CALLBACK( on_update_clicked ), page );

	priv->delete_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_DELETE, G_CALLBACK( on_delete_clicked ), page );

	return( GTK_WIDGET( buttons_box ));
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaCurrencyPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_CURRENCY_PAGE( page ), NULL );

	priv = ofa_currency_page_get_instance_private( OFA_CURRENCY_PAGE( page ));

	return( GTK_WIDGET( priv->tview ));
}

static void
setup_first_selection( ofaCurrencyPage *self )
{
	ofaCurrencyPagePrivate *priv;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	priv = ofa_currency_page_get_instance_private( self );

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
	on_update_clicked( NULL, OFA_CURRENCY_PAGE( page ));
}

static void
on_currency_selected( GtkTreeSelection *selection, ofaCurrencyPage *self )
{
	ofaCurrencyPagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoCurrency *currency;
	gboolean is_currency;

	currency = NULL;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, CURRENCY_COL_OBJECT, &currency, -1 );
		g_object_unref( currency );
	}

	priv = ofa_currency_page_get_instance_private( self );

	is_currency = currency && OFO_IS_CURRENCY( currency );

	if( priv->update_btn ){
		gtk_widget_set_sensitive( priv->update_btn,
				is_currency );
	}

	if( priv->delete_btn ){
		gtk_widget_set_sensitive( priv->delete_btn,
				priv->is_writable && is_currency && ofo_currency_is_deletable( currency ));
	}
}

static void
on_new_clicked( GtkButton *button, ofaCurrencyPage *self )
{
	ofoCurrency *currency;
	GtkWindow *toplevel;

	currency = ofo_currency_new();
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_currency_properties_run( OFA_IGETTER( self ), toplevel, currency );
}

#if 0
static void
select_row_by_code( ofaCurrencyPage *page, const gchar *code )
{
	ofaCurrencyPagePrivate *priv;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	priv = ofa_currency_page_get_instance_private( page );

	if( find_row_by_code( page, code, &iter )){
		select = gtk_tree_view_get_selection( priv->tview );
		gtk_tree_selection_select_iter( select, &iter );
	}
}

static gboolean
find_row_by_code( ofaCurrencyPage *page, const gchar *code, GtkTreeIter *iter )
{
	ofaCurrencyPagePrivate *priv;
	gchar *row_code;
	gint cmp;

	priv = ofa_currency_page_get_instance_private( page );

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( priv->store ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), iter, CURRENCY_COL_CODE, &row_code, -1 );
			cmp = g_utf8_collate( row_code, code );
			g_free( row_code );
			if( cmp == 0 ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( priv->store ), iter )){
				break;
			}
		}
	}

	return( FALSE );
}
#endif

static void
on_update_clicked( GtkButton *button, ofaCurrencyPage *self )
{
	ofaCurrencyPagePrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoCurrency *currency;
	GtkWindow *toplevel;

	priv = ofa_currency_page_get_instance_private( self );

	select = gtk_tree_view_get_selection( priv->tview );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, CURRENCY_COL_OBJECT, &currency, -1 );
		g_object_unref( currency );
		toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
		ofa_currency_properties_run( OFA_IGETTER( self ), toplevel, currency );
	}
}

static void
on_delete_clicked( GtkButton *button, ofaCurrencyPage *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoCurrency *currency;

	currency = tview_get_selected( self, &tmodel, &iter );
	if( currency ){
		do_delete( self, currency, tmodel, &iter );
	}

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( self )));
}

static void
try_to_delete_current_row( ofaCurrencyPage *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoCurrency *currency;

	currency = tview_get_selected( self, &tmodel, &iter );
	if( currency && ofo_currency_is_deletable( currency )){
		do_delete( self, currency, tmodel, &iter );
	}
}

static gboolean
delete_confirmed( ofaCurrencyPage *self, ofoCurrency *currency )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s - %s' currency ?" ),
			ofo_currency_get_code( currency ),
			ofo_currency_get_label( currency ));

	delete_ok = my_utils_dialog_question( msg, _( "_Delete" ));

	g_free( msg );

	return( delete_ok );
}

static void
do_delete( ofaCurrencyPage *self, ofoCurrency *currency, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	ofaCurrencyPagePrivate *priv;
	gboolean deletable;

	priv = ofa_currency_page_get_instance_private( self );

	deletable = ofo_currency_is_deletable( currency );
	g_return_if_fail( deletable );
	g_return_if_fail( !priv->is_writable );

	if( delete_confirmed( self, currency ) &&
			ofo_currency_delete( currency )){

		/* take into account by dossier signaling system */
	}
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
on_hub_new_object( ofaHub *hub, ofoBase *object, ofaCurrencyPage *self )
{
	static const gchar *thisfn = "ofa_currency_page_on_hub_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_CURRENCY( object )){
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaCurrencyPage *self )
{
	static const gchar *thisfn = "ofa_currency_page_on_hub_updated_object";

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn, ( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id, ( void * ) self );

	/* make sure the buttons reflect the new currency of the account */
	if( OFO_IS_ACCOUNT( object )){
		do_on_updated_account( self, OFO_ACCOUNT( object ));
	}
}

/*
 * make sure the buttons reflect the new currency of the account
 */
static void
do_on_updated_account( ofaCurrencyPage *self, ofoAccount *account )
{
	ofaCurrencyPagePrivate *priv;
	GtkTreeSelection *select;

	priv = ofa_currency_page_get_instance_private( self );

	select = gtk_tree_view_get_selection( priv->tview );
	on_currency_selected( select, self );
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
on_hub_deleted_object( ofaHub *hub, ofoBase *object, ofaCurrencyPage *self )
{
	static const gchar *thisfn = "ofa_currency_page_on_hub_deleted_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn, ( void * ) hub,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), ( void * ) self );

	if( OFO_IS_CURRENCY( object )){
	}
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
on_hub_reload_dataset( ofaHub *hub, GType type, ofaCurrencyPage *self )
{
	static const gchar *thisfn = "ofa_currency_page_on_hub_reload_dataset";

	g_debug( "%s: hub=%p, type=%lu, self=%p",
			thisfn, ( void * ) hub, type, ( void * ) self );

	if( type == OFO_TYPE_CURRENCY ){
	}
}

/*
 * sort_settings: sort_column_id;sort_sens;
 */
static void
get_sort_settings( ofaCurrencyPage *self )
{
	ofaCurrencyPagePrivate *priv;
	GList *slist, *it;
	const gchar *cstr;

	priv = ofa_currency_page_get_instance_private( self );

	slist = ofa_settings_user_get_string_list( st_sort_settings );

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

static void set_sort_settings( ofaCurrencyPage *self )
{
	ofaCurrencyPagePrivate *priv;
	gchar *str;

	priv = ofa_currency_page_get_instance_private( self );

	str = g_strdup_printf( "%d;%d;", priv->sort_column_id, priv->sort_sens );

	ofa_settings_user_set_string( st_sort_settings, str );

	g_free( str );
}
