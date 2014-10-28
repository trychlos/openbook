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

#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-currency-properties.h"
#include "ui/ofa-currencies-page.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-page.h"
#include "ui/ofa-page-prot.h"

/* private instance data
 */
struct _ofaCurrenciesPagePrivate {

	/* internals
	 */
	GList       *handlers;

	/* UI
	 */
	GtkTreeView *tview;					/* the main treeview of the page */
};

/* column ordering in the selection listview
 */
enum {
	COL_CODE = 0,
	COL_LABEL,
	COL_SYMBOL,
	COL_OBJECT,
	N_COLUMNS
};

G_DEFINE_TYPE( ofaCurrenciesPage, ofa_currencies_page, OFA_TYPE_PAGE )

static GtkWidget *v_setup_view( ofaPage *page );
static GtkWidget *setup_tree_view( ofaCurrenciesPage *self );
static void       v_init_view( ofaPage *page );
static GtkWidget *v_get_top_focusable_widget( ofaPage *page );
static void       insert_dataset( ofaCurrenciesPage *self );
static void       insert_new_row( ofaCurrenciesPage *self, ofoCurrency *currency, gboolean with_selection );
static gint       on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaCurrenciesPage *self );
static void       setup_first_selection( ofaCurrenciesPage *self );
static void       on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page );
static void       on_currency_selected( GtkTreeSelection *selection, ofaCurrenciesPage *self );
static void       v_on_new_clicked( GtkButton *button, ofaPage *page );
static void       v_on_update_clicked( GtkButton *button, ofaPage *page );
static void       v_on_delete_clicked( GtkButton *button, ofaPage *page );
static gboolean   delete_confirmed( ofaCurrenciesPage *self, ofoCurrency *currency );
static void       on_new_object( ofoDossier *dossier, ofoBase *object, ofaCurrenciesPage *self );
static void       on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaCurrenciesPage *self );
static void       on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaCurrenciesPage *self );
static void       on_reloaded_dataset( ofoDossier *dossier, GType type, ofaCurrenciesPage *self );

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
	OFA_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;
	OFA_PAGE_CLASS( klass )->on_new_clicked = v_on_new_clicked;
	OFA_PAGE_CLASS( klass )->on_update_clicked = v_on_update_clicked;
	OFA_PAGE_CLASS( klass )->on_delete_clicked = v_on_delete_clicked;

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
						OFA_SIGNAL_NEW_OBJECT, G_CALLBACK( on_new_object ), page );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						OFA_SIGNAL_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), page );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						OFA_SIGNAL_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), page );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						OFA_SIGNAL_RELOAD_DATASET, G_CALLBACK( on_reloaded_dataset ), page );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	return( setup_tree_view( OFA_CURRENCIES_PAGE( page )));
}

static GtkWidget *
setup_tree_view( ofaCurrenciesPage *self )
{
	ofaCurrenciesPagePrivate *priv;
	GtkFrame *frame;
	GtkScrolledWindow *scroll;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = self->priv;

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
	g_signal_connect(G_OBJECT( tview ), "row-activated", G_CALLBACK( on_row_activated ), self );
	priv->tview = tview;

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( tview, tmodel );
	g_object_unref( tmodel );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "ISO 3A code" ),
			text_cell, "text", COL_CODE,
			NULL );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Symbol" ),
			text_cell, "text", COL_SYMBOL,
			NULL );
	gtk_tree_view_append_column( tview, column );

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_currency_selected ), self );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ), ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	return( GTK_WIDGET( frame ));
}

static void
v_init_view( ofaPage *page )
{
	insert_dataset( OFA_CURRENCIES_PAGE( page ));
}

static GtkWidget *
v_get_top_focusable_widget( ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_CURRENCIES_PAGE( page ), NULL );

	return( GTK_WIDGET( OFA_CURRENCIES_PAGE( page )->priv->tview ));
}

static void
insert_dataset( ofaCurrenciesPage *self )
{
	ofoDossier *dossier;
	GList *dataset, *iset;
	ofoCurrency *currency;

	dossier = ofa_page_get_dossier( OFA_PAGE( self ));
	dataset = ofo_currency_get_dataset( dossier );

	for( iset=dataset ; iset ; iset=iset->next ){

		currency = OFO_CURRENCY( iset->data );
		insert_new_row( self, currency, FALSE );
	}

	setup_first_selection( self );
}

static void
insert_new_row( ofaCurrenciesPage *self, ofoCurrency *currency, gboolean with_selection )
{
	ofaCurrenciesPagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;

	priv = self->priv;
	tmodel = gtk_tree_view_get_model( priv->tview );
	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( tmodel ),
			&iter,
			-1,
			COL_CODE,   ofo_currency_get_code( currency ),
			COL_LABEL,  ofo_currency_get_label( currency ),
			COL_SYMBOL, ofo_currency_get_symbol( currency ),
			COL_OBJECT, currency,
			-1 );

	/* select the newly added currency */
	if( with_selection ){
		path = gtk_tree_model_get_path( tmodel, &iter );
		gtk_tree_view_set_cursor( priv->tview, path, NULL, FALSE );
		gtk_tree_path_free( path );
		gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
	}
}

static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaCurrenciesPage *self )
{
	gchar *acod, *bcod, *afold, *bfold;
	gint cmp;

	gtk_tree_model_get( tmodel, a, COL_CODE, &acod, -1 );
	afold = g_utf8_casefold( acod, -1 );

	gtk_tree_model_get( tmodel, b, COL_CODE, &bcod, -1 );
	bfold = g_utf8_casefold( bcod, -1 );

	/*g_debug( "on_sort_model: afold=%p, bfold=%p", afold, bfold );*/
	cmp = g_utf8_collate( afold, bfold );

	g_free( acod );
	g_free( afold );
	g_free( bcod );
	g_free( bfold );

	return( cmp );
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
	v_on_update_clicked( NULL, page );
}

static void
on_currency_selected( GtkTreeSelection *selection, ofaCurrenciesPage *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoCurrency *currency;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &currency, -1 );
		g_object_unref( currency );
	}

	gtk_widget_set_sensitive(
			ofa_page_get_update_btn( OFA_PAGE( self )),
			currency && OFO_IS_CURRENCY( currency ));

	gtk_widget_set_sensitive(
			ofa_page_get_delete_btn( OFA_PAGE( self )),
			currency && OFO_IS_CURRENCY( currency ) && ofo_currency_is_deletable( currency ));
}

static void
v_on_new_clicked( GtkButton *button, ofaPage *page )
{
	ofoCurrency *currency;

	g_return_if_fail( page && OFA_IS_CURRENCIES_PAGE( page ));

	currency = ofo_currency_new();

	if( ofa_currency_properties_run(
			ofa_page_get_main_window( page ), currency )){

		insert_new_row( OFA_CURRENCIES_PAGE( page ), currency, TRUE );

	} else {
		g_object_unref( currency );
	}
}

static void
v_on_update_clicked( GtkButton *button, ofaPage *page )
{
	ofaCurrenciesPagePrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoCurrency *currency;

	g_return_if_fail( page && OFA_IS_CURRENCIES_PAGE( page ));

	priv = OFA_CURRENCIES_PAGE( page )->priv;
	select = gtk_tree_view_get_selection( priv->tview );

	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &currency, -1 );
		g_object_unref( currency );

		if( ofa_currency_properties_run(
				ofa_page_get_main_window( page ), currency )){

			gtk_list_store_set(
					GTK_LIST_STORE( tmodel ),
					&iter,
					COL_CODE,  ofo_currency_get_code( currency ),
					COL_LABEL,  ofo_currency_get_label( currency ),
					COL_SYMBOL, ofo_currency_get_symbol( currency ),
					-1 );
		}
	}

	gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
}

static void
v_on_delete_clicked( GtkButton *button, ofaPage *page )
{
	ofaCurrenciesPagePrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoCurrency *currency;

	g_return_if_fail( page && OFA_IS_CURRENCIES_PAGE( page ));

	priv = OFA_CURRENCIES_PAGE( page )->priv;
	select = gtk_tree_view_get_selection( priv->tview );

	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &currency, -1 );
		g_object_unref( currency );

		g_return_if_fail( ofo_currency_is_deletable( currency ));

		if( delete_confirmed( OFA_CURRENCIES_PAGE( page ), currency ) &&
				ofo_currency_delete( currency )){

			/* remove the row from the tmodel
			 * this will cause an automatic new selection */
			gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
		}
	}

	gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
}

static gboolean
delete_confirmed( ofaCurrenciesPage *self, ofoCurrency *currency )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s - %s' currency ?" ),
			ofo_currency_get_code( currency ),
			ofo_currency_get_label( currency ));

	delete_ok = ofa_main_window_confirm_deletion( OFA_PAGE( self )->prot->main_window, msg );

	g_free( msg );

	return( delete_ok );
}

/*
 * OFA_SIGNAL_NEW_OBJECT signal handler
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

	if( OFO_IS_CURRENCY( object )){
	}
}

/*
 * OFA_SIGNAL_DELETED_OBJECT signal handler
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
 * OFA_SIGNAL_RELOAD_DATASET signal handler
 */
static void
on_reloaded_dataset( ofoDossier *dossier, GType type, ofaCurrenciesPage *self )
{
	static const gchar *thisfn = "ofa_currencies_page_on_reloaded_dataset";
	ofaCurrenciesPagePrivate *priv;
	GtkTreeModel *tmodel;

	g_debug( "%s: dossier=%p, type=%lu, self=%p",
			thisfn, ( void * ) dossier, type, ( void * ) self );

	priv = self->priv;

	if( type == OFO_TYPE_CURRENCY ){
		tmodel = gtk_tree_view_get_model( priv->tview );
		gtk_list_store_clear( GTK_LIST_STORE( tmodel ));
		insert_dataset( self );
	}
}
