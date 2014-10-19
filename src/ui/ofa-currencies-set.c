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

#include "ui/ofa-page.h"
#include "ui/ofa-currency-properties.h"
#include "ui/ofa-currencies-set.h"

/* private instance data
 */
struct _ofaCurrenciesSetPrivate {
	gboolean  dispose_has_run;

	/* internals
	 */
	GList    *handlers;
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

G_DEFINE_TYPE( ofaCurrenciesSet, ofa_currencies_set, OFA_TYPE_PAGE )

static GtkWidget *v_setup_view( ofaPage *page );
static GtkWidget *setup_tree_view( ofaCurrenciesSet *self );
static void       v_init_view( ofaPage *page );
static void       insert_dataset( ofaCurrenciesSet *self );
static void       insert_new_row( ofaCurrenciesSet *self, ofoCurrency *currency, gboolean with_selection );
static gint       on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaCurrenciesSet *self );
static void       setup_first_selection( ofaCurrenciesSet *self );
static void       on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page );
static void       on_currency_selected( GtkTreeSelection *selection, ofaCurrenciesSet *self );
static void       v_on_new_clicked( GtkButton *button, ofaPage *page );
static void       v_on_update_clicked( GtkButton *button, ofaPage *page );
static void       v_on_delete_clicked( GtkButton *button, ofaPage *page );
static gboolean   delete_confirmed( ofaCurrenciesSet *self, ofoCurrency *currency );
static void       on_new_object( ofoDossier *dossier, ofoBase *object, ofaCurrenciesSet *self );
static void       on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaCurrenciesSet *self );
static void       on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaCurrenciesSet *self );
static void       on_reloaded_dataset( ofoDossier *dossier, GType type, ofaCurrenciesSet *self );

static void
currencies_set_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_currencies_set_finalize";
	ofaCurrenciesSetPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CURRENCIES_SET( instance ));

	priv = OFA_CURRENCIES_SET( instance )->private;

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currencies_set_parent_class )->finalize( instance );
}

static void
currencies_set_dispose( GObject *instance )
{
	ofaCurrenciesSetPrivate *priv;
	gulong handler_id;
	GList *iha;
	ofoDossier *dossier;

	g_return_if_fail( instance && OFA_IS_CURRENCIES_SET( instance ));

	priv = ( OFA_CURRENCIES_SET( instance ))->private;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */

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
	G_OBJECT_CLASS( ofa_currencies_set_parent_class )->dispose( instance );
}

static void
ofa_currencies_set_init( ofaCurrenciesSet *self )
{
	static const gchar *thisfn = "ofa_currencies_set_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_CURRENCIES_SET( self ));

	self->private = g_new0( ofaCurrenciesSetPrivate, 1 );

	self->private->dispose_has_run = FALSE;
	self->private->handlers = NULL;
}

static void
ofa_currencies_set_class_init( ofaCurrenciesSetClass *klass )
{
	static const gchar *thisfn = "ofa_currencies_set_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = currencies_set_dispose;
	G_OBJECT_CLASS( klass )->finalize = currencies_set_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_PAGE_CLASS( klass )->on_new_clicked = v_on_new_clicked;
	OFA_PAGE_CLASS( klass )->on_update_clicked = v_on_update_clicked;
	OFA_PAGE_CLASS( klass )->on_delete_clicked = v_on_delete_clicked;
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	ofaCurrenciesSetPrivate *priv;
	ofoDossier *dossier;
	gulong handler;

	priv = OFA_CURRENCIES_SET( page )->private;
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

	return( setup_tree_view( OFA_CURRENCIES_SET( page )));
}

static GtkWidget *
setup_tree_view( ofaCurrenciesSet *self )
{
	GtkFrame *frame;
	GtkScrolledWindow *scroll;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

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
	insert_dataset( OFA_CURRENCIES_SET( page ));
}

static void
insert_dataset( ofaCurrenciesSet *self )
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
insert_new_row( ofaCurrenciesSet *self, ofoCurrency *currency, gboolean with_selection )
{
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;

	tview = GTK_TREE_VIEW( ofa_page_get_treeview( OFA_PAGE( self )));
	tmodel = gtk_tree_view_get_model( tview );
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
		gtk_tree_view_set_cursor( tview, path, NULL, FALSE );
		gtk_tree_path_free( path );
		gtk_widget_grab_focus( GTK_WIDGET( tview ));
	}
}

static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaCurrenciesSet *self )
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
setup_first_selection( ofaCurrenciesSet *self )
{
	GtkTreeView *tview;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	tview = GTK_TREE_VIEW( ofa_page_get_treeview( OFA_PAGE( self )));
	model = gtk_tree_view_get_model( tview );
	if( gtk_tree_model_get_iter_first( model, &iter )){
		select = gtk_tree_view_get_selection( tview );
		gtk_tree_selection_select_iter( select, &iter );
	}

	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}

static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page )
{
	v_on_update_clicked( NULL, page );
}

static void
on_currency_selected( GtkTreeSelection *selection, ofaCurrenciesSet *self )
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

	g_return_if_fail( page && OFA_IS_CURRENCIES_SET( page ));

	currency = ofo_currency_new();

	if( ofa_currency_properties_run(
			ofa_page_get_main_window( page ), currency )){

		insert_new_row( OFA_CURRENCIES_SET( page ), currency, TRUE );

	} else {
		g_object_unref( currency );
	}
}

static void
v_on_update_clicked( GtkButton *button, ofaPage *page )
{
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoCurrency *currency;

	g_return_if_fail( page && OFA_IS_CURRENCIES_SET( page ));

	tview = GTK_TREE_VIEW( ofa_page_get_treeview( page ));
	select = gtk_tree_view_get_selection( tview );

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

	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}

static void
v_on_delete_clicked( GtkButton *button, ofaPage *page )
{
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoCurrency *currency;

	g_return_if_fail( page && OFA_IS_CURRENCIES_SET( page ));

	tview = GTK_TREE_VIEW( ofa_page_get_treeview( page ));
	select = gtk_tree_view_get_selection( tview );

	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &currency, -1 );
		g_object_unref( currency );

		g_return_if_fail( ofo_currency_is_deletable( currency ));

		if( delete_confirmed( OFA_CURRENCIES_SET( page ), currency ) &&
				ofo_currency_delete( currency )){

			/* remove the row from the tmodel
			 * this will cause an automatic new selection */
			gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
		}
	}

	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}

static gboolean
delete_confirmed( ofaCurrenciesSet *self, ofoCurrency *currency )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s - %s' currency ?" ),
			ofo_currency_get_code( currency ),
			ofo_currency_get_label( currency ));

	delete_ok = ofa_page_delete_confirmed( OFA_PAGE( self ), msg );

	g_free( msg );

	return( delete_ok );
}

/*
 * OFA_SIGNAL_NEW_OBJECT signal handler
 */
static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaCurrenciesSet *self )
{
	static const gchar *thisfn = "ofa_currencies_set_on_new_object";

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
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaCurrenciesSet *self )
{
	static const gchar *thisfn = "ofa_currencies_set_on_updated_object";

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
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaCurrenciesSet *self )
{
	static const gchar *thisfn = "ofa_currencies_set_on_deleted_object";

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
on_reloaded_dataset( ofoDossier *dossier, GType type, ofaCurrenciesSet *self )
{
	static const gchar *thisfn = "ofa_currencies_set_on_reloaded_dataset";
	GtkTreeView *tview;
	GtkTreeModel *tmodel;

	g_debug( "%s: dossier=%p, type=%lu, self=%p",
			thisfn, ( void * ) dossier, type, ( void * ) self );

	if( type == OFO_TYPE_CURRENCY ){
		tview = GTK_TREE_VIEW( ofa_page_get_treeview( OFA_PAGE( self )));
		tmodel = gtk_tree_view_get_model( tview );
		gtk_list_store_clear( GTK_LIST_STORE( tmodel ));
		insert_dataset( self );
	}
}
