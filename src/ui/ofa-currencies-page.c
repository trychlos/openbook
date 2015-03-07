/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#include "api/ofo-currency.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-buttons-box.h"
#include "ui/ofa-currency-properties.h"
#include "ui/ofa-currencies-page.h"
#include "ui/ofa-currency-store.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-page.h"
#include "ui/ofa-page-prot.h"

/* private instance data
 */
struct _ofaCurrenciesPagePrivate {

	/* internals
	 */
	GList            *handlers;

	/* UI
	 */
	ofaCurrencyStore *store;
	GtkTreeView      *tview;			/* the main treeview of the page */
	GtkWidget        *update_btn;
	GtkWidget        *delete_btn;
};

G_DEFINE_TYPE( ofaCurrenciesPage, ofa_currencies_page, OFA_TYPE_PAGE )

static GtkWidget   *v_setup_view( ofaPage *page );
static GtkWidget   *setup_tree_view( ofaCurrenciesPage *self );
static gboolean     on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaCurrenciesPage *self );
static ofoCurrency *tview_get_selected( ofaCurrenciesPage *page, GtkTreeModel **tmodel, GtkTreeIter *iter );
static GtkWidget   *v_setup_buttons( ofaPage *page );
static void         v_init_view( ofaPage *page );
static GtkWidget   *v_get_top_focusable_widget( const ofaPage *page );
static void         setup_first_selection( ofaCurrenciesPage *self );
static void         on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page );
static void         on_currency_selected( GtkTreeSelection *selection, ofaCurrenciesPage *self );
static void         on_new_clicked( GtkButton *button, ofaCurrenciesPage *page );
static void         select_row_by_code( ofaCurrenciesPage *page, const gchar *code );
static gboolean     find_row_by_code( ofaCurrenciesPage *page, const gchar *code, GtkTreeIter *iter );
static void         on_update_clicked( GtkButton *button, ofaCurrenciesPage *page );
static void         on_delete_clicked( GtkButton *button, ofaCurrenciesPage *page );
static void         try_to_delete_current_row( ofaCurrenciesPage *page );
static gboolean     delete_confirmed( ofaCurrenciesPage *self, ofoCurrency *currency );
static void         do_delete( ofaCurrenciesPage *page, ofoCurrency *currency, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void         on_new_object( ofoDossier *dossier, ofoBase *object, ofaCurrenciesPage *self );
static void         on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaCurrenciesPage *self );
static void         do_on_updated_account( ofaCurrenciesPage *self, ofoAccount *account );
static void         on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaCurrenciesPage *self );
static void         on_reloaded_dataset( ofoDossier *dossier, GType type, ofaCurrenciesPage *self );

static void
currencies_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_currencies_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CURRENCIES_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currencies_page_parent_class )->finalize( instance );
}

static void
currencies_page_dispose( GObject *instance )
{
	ofaCurrenciesPagePrivate *priv;
	gulong handler_id;
	GList *iha;
	ofoDossier *dossier;

	g_return_if_fail( instance && OFA_IS_CURRENCIES_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = ( OFA_CURRENCIES_PAGE( instance ))->priv;

		/* note when deconnecting the handlers that the dossier may
		 * have been already finalized (e.g. when the application
		 * terminates) */
		dossier = ofa_page_get_dossier( OFA_PAGE( instance ));
		if( OFO_IS_DOSSIER( dossier )){
			for( iha=priv->handlers ; iha ; iha=iha->next ){
				handler_id = ( gulong ) iha->data;
				g_signal_handler_disconnect( dossier, handler_id );
			}
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currencies_page_parent_class )->dispose( instance );
}

static void
ofa_currencies_page_init( ofaCurrenciesPage *self )
{
	static const gchar *thisfn = "ofa_currencies_page_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_CURRENCIES_PAGE( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_CURRENCIES_PAGE, ofaCurrenciesPagePrivate );
	self->priv->handlers = NULL;
}

static void
ofa_currencies_page_class_init( ofaCurrenciesPageClass *klass )
{
	static const gchar *thisfn = "ofa_currencies_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = currencies_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = currencies_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;

	g_type_class_add_private( klass, sizeof( ofaCurrenciesPagePrivate ));
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	ofaCurrenciesPagePrivate *priv;
	ofoDossier *dossier;
	gulong handler;

	priv = OFA_CURRENCIES_PAGE( page )->priv;
	dossier = ofa_page_get_dossier( page );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						SIGNAL_DOSSIER_NEW_OBJECT, G_CALLBACK( on_new_object ), page );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), page );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						SIGNAL_DOSSIER_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), page );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						SIGNAL_DOSSIER_RELOAD_DATASET, G_CALLBACK( on_reloaded_dataset ), page );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	return( setup_tree_view( OFA_CURRENCIES_PAGE( page )));
}

static GtkWidget *
setup_tree_view( ofaCurrenciesPage *self )
{
	ofaCurrenciesPagePrivate *priv;
	ofoDossier *dossier;
	GtkFrame *frame;
	GtkScrolledWindow *scroll;
	GtkTreeView *tview;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = self->priv;
	dossier = ofa_page_get_dossier( OFA_PAGE( self ));

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
	g_signal_connect(G_OBJECT( tview ),
			"row-activated", G_CALLBACK( on_row_activated ), self );
	g_signal_connect(
			G_OBJECT( tview ), "key-press-event", G_CALLBACK( on_tview_key_pressed ), self );
	priv->tview = tview;

	priv->store = ofa_currency_store_new( dossier );
	gtk_tree_view_set_model( priv->tview, GTK_TREE_MODEL( priv->store ));

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "ISO 3A code" ),
			text_cell, "text", CURRENCY_COL_CODE,
			NULL );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", CURRENCY_COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Symbol" ),
			text_cell, "text", CURRENCY_COL_SYMBOL,
			NULL );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Digits" ),
			text_cell, "text", CURRENCY_COL_DIGITS,
			NULL );
	gtk_tree_view_append_column( tview, column );

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_currency_selected ), self );

	return( GTK_WIDGET( frame ));
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaCurrenciesPage *self )
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
tview_get_selected( ofaCurrenciesPage *page, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	ofaCurrenciesPagePrivate *priv;
	GtkTreeSelection *select;
	ofoCurrency *currency;

	priv = page->priv;
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
	ofaCurrenciesPagePrivate *priv;
	ofaButtonsBox *buttons_box;

	priv = OFA_CURRENCIES_PAGE( page )->priv;

	buttons_box = ofa_buttons_box_new();

	ofa_buttons_box_add_spacer( buttons_box );
	ofa_buttons_box_add_button(
			buttons_box, BUTTON_NEW, TRUE, G_CALLBACK( on_new_clicked ), page );
	priv->update_btn = ofa_buttons_box_add_button(
			buttons_box, BUTTON_PROPERTIES, FALSE, G_CALLBACK( on_update_clicked ), page );
	priv->delete_btn = ofa_buttons_box_add_button(
			buttons_box, BUTTON_DELETE, FALSE, G_CALLBACK( on_delete_clicked ), page );

	return( GTK_WIDGET( buttons_box ));
}

static void
v_init_view( ofaPage *page )
{
	setup_first_selection( OFA_CURRENCIES_PAGE( page ));
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_CURRENCIES_PAGE( page ), NULL );

	return( GTK_WIDGET( OFA_CURRENCIES_PAGE( page )->priv->tview ));
}

static void
setup_first_selection( ofaCurrenciesPage *self )
{
	ofaCurrenciesPagePrivate *priv;
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
	on_update_clicked( NULL, OFA_CURRENCIES_PAGE( page ));
}

static void
on_currency_selected( GtkTreeSelection *selection, ofaCurrenciesPage *self )
{
	ofaCurrenciesPagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoCurrency *currency;

	currency = NULL;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, CURRENCY_COL_OBJECT, &currency, -1 );
		g_object_unref( currency );
	}

	priv = self->priv;

	if( priv->update_btn ){
		gtk_widget_set_sensitive(
				priv->update_btn,
				currency && OFO_IS_CURRENCY( currency ));
	}

	if( priv->delete_btn ){
		gtk_widget_set_sensitive(
				priv->delete_btn,
				currency && OFO_IS_CURRENCY( currency ) &&
					ofo_currency_is_deletable( currency, ofa_page_get_dossier( OFA_PAGE( self ))));
	}
}

static void
on_new_clicked( GtkButton *button, ofaCurrenciesPage *page )
{
	ofoCurrency *currency;

	currency = ofo_currency_new();

	if( ofa_currency_properties_run(
			ofa_page_get_main_window( OFA_PAGE( page )), currency )){

		select_row_by_code( page, ofo_currency_get_code( currency ));

	} else {
		g_object_unref( currency );
	}
}

static void
select_row_by_code( ofaCurrenciesPage *page, const gchar *code )
{
	ofaCurrenciesPagePrivate *priv;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	priv = page->priv;

	if( find_row_by_code( page, code, &iter )){
		select = gtk_tree_view_get_selection( priv->tview );
		gtk_tree_selection_select_iter( select, &iter );
	}
}

static gboolean
find_row_by_code( ofaCurrenciesPage *page, const gchar *code, GtkTreeIter *iter )
{
	ofaCurrenciesPagePrivate *priv;
	gchar *row_code;
	gint cmp;

	priv = page->priv;

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

static void
on_update_clicked( GtkButton *button, ofaCurrenciesPage *page )
{
	ofaCurrenciesPagePrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoCurrency *currency;

	priv = page->priv;
	select = gtk_tree_view_get_selection( priv->tview );

	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, CURRENCY_COL_OBJECT, &currency, -1 );
		g_object_unref( currency );

		if( ofa_currency_properties_run(
				ofa_page_get_main_window( OFA_PAGE( page )), currency )){

			/* take into account by dossier signaling system */
		}
	}

	gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
}

static void
on_delete_clicked( GtkButton *button, ofaCurrenciesPage *page )
{
	ofaCurrenciesPagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoCurrency *currency;

	priv = page->priv;
	currency = tview_get_selected( page, &tmodel, &iter );
	if( currency ){
		do_delete( page, currency, tmodel, &iter );
	}

	gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
}

static void
try_to_delete_current_row( ofaCurrenciesPage *page )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoCurrency *currency;

	currency = tview_get_selected( page, &tmodel, &iter );
	if( currency &&
			ofo_currency_is_deletable( currency, ofa_page_get_dossier( OFA_PAGE( page )))){
		do_delete( page, currency, tmodel, &iter );
	}
}

static gboolean
delete_confirmed( ofaCurrenciesPage *self, ofoCurrency *currency )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s - %s' currency ?" ),
			ofo_currency_get_code( currency ),
			ofo_currency_get_label( currency ));

	delete_ok = ofa_main_window_confirm_deletion(
						ofa_page_get_main_window( OFA_PAGE( self )), msg );

	g_free( msg );

	return( delete_ok );
}

static void
do_delete( ofaCurrenciesPage *page, ofoCurrency *currency, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gboolean deletable;
	ofoDossier *dossier;

	dossier = ofa_page_get_dossier( OFA_PAGE( page ));
	deletable = ofo_currency_is_deletable( currency, dossier );
	g_return_if_fail( deletable );

	if( delete_confirmed( page, currency ) &&
			ofo_currency_delete( currency, dossier )){

		/* take into account by dossier signaling system */
	}
}

/*
 * SIGNAL_DOSSIER_NEW_OBJECT signal handler
 */
static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaCurrenciesPage *self )
{
	static const gchar *thisfn = "ofa_currencies_page_on_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_CURRENCY( object )){
	}
}

/*
 * OFA_SIGNAL_UPDATE_OBJECT signal handler
 */
static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaCurrenciesPage *self )
{
	static const gchar *thisfn = "ofa_currencies_page_on_updated_object";

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn, ( void * ) dossier,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), prev_id, ( void * ) self );

	/* make sure the buttons reflect the new currency of the account */
	if( OFO_IS_ACCOUNT( object )){
		do_on_updated_account( self, OFO_ACCOUNT( object ));
	}
}

/*
 * make sure the buttons reflect the new currency of the account
 */
static void
do_on_updated_account( ofaCurrenciesPage *self, ofoAccount *account )
{
	ofaCurrenciesPagePrivate *priv;
	GtkTreeSelection *select;

	priv = self->priv;
	select = gtk_tree_view_get_selection( priv->tview );
	on_currency_selected( select, self );
}

/*
 * SIGNAL_DOSSIER_DELETED_OBJECT signal handler
 */
static void
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaCurrenciesPage *self )
{
	static const gchar *thisfn = "ofa_currencies_page_on_deleted_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn, ( void * ) dossier,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), ( void * ) self );

	if( OFO_IS_CURRENCY( object )){
	}
}

/*
 * SIGNAL_DOSSIER_RELOAD_DATASET signal handler
 */
static void
on_reloaded_dataset( ofoDossier *dossier, GType type, ofaCurrenciesPage *self )
{
	static const gchar *thisfn = "ofa_currencies_page_on_reloaded_dataset";

	g_debug( "%s: dossier=%p, type=%lu, self=%p",
			thisfn, ( void * ) dossier, type, ( void * ) self );

	if( type == OFO_TYPE_CURRENCY ){
	}
}
