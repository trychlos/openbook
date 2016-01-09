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
#include "api/ofa-buttons-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofo-currency.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-currency-properties.h"
#include "ui/ofa-currency-page.h"
#include "ui/ofa-currency-store.h"

/* private instance data
 */
struct _ofaCurrencyPagePrivate {

	/* internals
	 */
	ofaHub           *hub;
	GList            *hub_handlers;
	gboolean          is_current;

	/* UI
	 */
	ofaCurrencyStore *store;
	GtkTreeView      *tview;			/* the main treeview of the page */
	GtkWidget        *update_btn;
	GtkWidget        *delete_btn;
};

G_DEFINE_TYPE( ofaCurrencyPage, ofa_currency_page, OFA_TYPE_PAGE )

static GtkWidget   *v_setup_view( ofaPage *page );
static GtkWidget   *setup_tree_view( ofaCurrencyPage *self );
static gboolean     on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaCurrencyPage *self );
static ofoCurrency *tview_get_selected( ofaCurrencyPage *page, GtkTreeModel **tmodel, GtkTreeIter *iter );
static GtkWidget   *v_setup_buttons( ofaPage *page );
static GtkWidget   *v_get_top_focusable_widget( const ofaPage *page );
static void         setup_first_selection( ofaCurrencyPage *self );
static void         on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page );
static void         on_currency_selected( GtkTreeSelection *selection, ofaCurrencyPage *self );
static void         on_new_clicked( GtkButton *button, ofaCurrencyPage *page );
static void         select_row_by_code( ofaCurrencyPage *page, const gchar *code );
static gboolean     find_row_by_code( ofaCurrencyPage *page, const gchar *code, GtkTreeIter *iter );
static void         on_update_clicked( GtkButton *button, ofaCurrencyPage *page );
static void         on_delete_clicked( GtkButton *button, ofaCurrencyPage *page );
static void         try_to_delete_current_row( ofaCurrencyPage *page );
static gboolean     delete_confirmed( ofaCurrencyPage *self, ofoCurrency *currency );
static void         do_delete( ofaCurrencyPage *page, ofoCurrency *currency, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void         on_new_object( ofaHub *hub, ofoBase *object, ofaCurrencyPage *self );
static void         on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaCurrencyPage *self );
static void         do_on_updated_account( ofaCurrencyPage *self, ofoAccount *account );
static void         on_deleted_object( ofaHub *hub, ofoBase *object, ofaCurrencyPage *self );
static void         on_reloaded_dataset( ofaHub *hub, GType type, ofaCurrencyPage *self );

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
		priv = ( OFA_CURRENCY_PAGE( instance ))->priv;

		/* note when deconnecting the handlers that the dossier may
		 * have been already finalized (e.g. when the application
		 * terminates) */
		ofa_hub_disconnect_handlers( priv->hub, priv->hub_handlers );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currency_page_parent_class )->dispose( instance );
}

static void
ofa_currency_page_init( ofaCurrencyPage *self )
{
	static const gchar *thisfn = "ofa_currency_page_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_CURRENCY_PAGE( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_CURRENCY_PAGE, ofaCurrencyPagePrivate );
	self->priv->hub_handlers = NULL;
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

	g_type_class_add_private( klass, sizeof( ofaCurrencyPagePrivate ));
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_currency_page_v_setup_view";
	ofaCurrencyPagePrivate *priv;
	ofoDossier *dossier;
	gulong handler;
	GtkWidget *tview;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = OFA_CURRENCY_PAGE( page )->priv;

	priv->hub = ofa_page_get_hub( page );
	g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );

	dossier = ofa_hub_get_dossier( priv->hub );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	priv->is_current = ofo_dossier_is_current( dossier );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_NEW, G_CALLBACK( on_new_object ), page );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_UPDATED, G_CALLBACK( on_updated_object ), page );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_DELETED, G_CALLBACK( on_deleted_object ), page );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_RELOAD, G_CALLBACK( on_reloaded_dataset ), page );
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
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = self->priv;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	my_utils_widget_set_margin( GTK_WIDGET( frame ), 4, 4, 4, 0 );
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
			G_OBJECT( tview ), "key-press-event", G_CALLBACK( on_tview_key_pressed ), self );
	priv->tview = tview;

	priv->store = ofa_currency_store_new( priv->hub );
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
on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaCurrencyPage *self )
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
	ofaCurrencyPagePrivate *priv;
	ofaButtonsBox *buttons_box;
	GtkWidget *btn;

	priv = OFA_CURRENCY_PAGE( page )->priv;

	buttons_box = ofa_buttons_box_new();

	ofa_buttons_box_add_spacer( buttons_box );

	btn = ofa_buttons_box_add_button_with_mnemonic(
			buttons_box, BUTTON_NEW, G_CALLBACK( on_new_clicked ), page );
	gtk_widget_set_sensitive( btn, priv->is_current );

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
	g_return_val_if_fail( page && OFA_IS_CURRENCY_PAGE( page ), NULL );

	return( GTK_WIDGET( OFA_CURRENCY_PAGE( page )->priv->tview ));
}

static void
setup_first_selection( ofaCurrencyPage *self )
{
	ofaCurrencyPagePrivate *priv;
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

	priv = self->priv;
	is_currency = currency && OFO_IS_CURRENCY( currency );

	if( priv->update_btn ){
		gtk_widget_set_sensitive( priv->update_btn,
				is_currency );
	}

	if( priv->delete_btn ){
		gtk_widget_set_sensitive( priv->delete_btn,
				priv->is_current && is_currency && ofo_currency_is_deletable( currency ));
	}
}

static void
on_new_clicked( GtkButton *button, ofaCurrencyPage *page )
{
	ofoCurrency *currency;

	currency = ofo_currency_new();

	if( ofa_currency_properties_run(
			ofa_page_get_main_window( OFA_PAGE( page )), currency )){

		select_row_by_code( page, ofo_currency_get_code( currency ));

	} else {
		g_object_unref( currency );
	}

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( page )));
}

static void
select_row_by_code( ofaCurrencyPage *page, const gchar *code )
{
	ofaCurrencyPagePrivate *priv;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	priv = page->priv;

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
on_update_clicked( GtkButton *button, ofaCurrencyPage *page )
{
	ofaCurrencyPagePrivate *priv;
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

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( page )));
}

static void
on_delete_clicked( GtkButton *button, ofaCurrencyPage *page )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoCurrency *currency;

	currency = tview_get_selected( page, &tmodel, &iter );
	if( currency ){
		do_delete( page, currency, tmodel, &iter );
	}

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( page )));
}

static void
try_to_delete_current_row( ofaCurrencyPage *page )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoCurrency *currency;

	currency = tview_get_selected( page, &tmodel, &iter );
	if( currency && ofo_currency_is_deletable( currency )){
		do_delete( page, currency, tmodel, &iter );
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
do_delete( ofaCurrencyPage *page, ofoCurrency *currency, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	ofaCurrencyPagePrivate *priv;
	gboolean deletable;

	priv = page->priv;

	deletable = ofo_currency_is_deletable( currency );
	g_return_if_fail( deletable );
	g_return_if_fail( !priv->is_current );

	if( delete_confirmed( page, currency ) &&
			ofo_currency_delete( currency )){

		/* take into account by dossier signaling system */
	}
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
on_new_object( ofaHub *hub, ofoBase *object, ofaCurrencyPage *self )
{
	static const gchar *thisfn = "ofa_currency_page_on_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_CURRENCY( object )){
	}
}

/*
 * SIGNAL_HUB_UPDATE signal handler
 */
static void
on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaCurrencyPage *self )
{
	static const gchar *thisfn = "ofa_currency_page_on_updated_object";

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

	priv = self->priv;
	select = gtk_tree_view_get_selection( priv->tview );
	on_currency_selected( select, self );
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
on_deleted_object( ofaHub *hub, ofoBase *object, ofaCurrencyPage *self )
{
	static const gchar *thisfn = "ofa_currency_page_on_deleted_object";

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
on_reloaded_dataset( ofaHub *hub, GType type, ofaCurrencyPage *self )
{
	static const gchar *thisfn = "ofa_currency_page_on_reloaded_dataset";

	g_debug( "%s: hub=%p, type=%lu, self=%p",
			thisfn, ( void * ) hub, type, ( void * ) self );

	if( type == OFO_TYPE_CURRENCY ){
	}
}
