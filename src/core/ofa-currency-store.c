/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-igetter.h"
#include "api/ofa-isignaler.h"
#include "api/ofo-currency.h"

#include "core/ofa-currency-store.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;

	/* runtime
	 */
	GList      *signaler_handlers;
}
	ofaCurrencyStorePrivate;

static GType st_col_types[CURRENCY_N_COLUMNS] = {
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* code, label, symbol */
		G_TYPE_STRING, G_TYPE_STRING, 0,				/* digits, notes, notes_png */
		G_TYPE_STRING,									/* upd_user */
		G_TYPE_STRING,									/* upd_stamp */
		G_TYPE_OBJECT									/* the #ofoCurrency itself */
};

static const gchar *st_resource_filler_png  = "/org/trychlos/openbook/core/filler.png";
static const gchar *st_resource_notes_png   = "/org/trychlos/openbook/core/notes.png";

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaCurrencyStore *self );
static void     load_dataset( ofaCurrencyStore *self );
static void     insert_row( ofaCurrencyStore *self, const ofoCurrency *currency );
static void     set_row_by_iter( ofaCurrencyStore *self, const ofoCurrency *currency, GtkTreeIter *iter );
static gboolean find_currency_by_code( ofaCurrencyStore *self, const gchar *code, GtkTreeIter *iter );
static void     signaler_connect_to_signaling_system( ofaCurrencyStore *self );
static void     signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaCurrencyStore *self );
static void     signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, ofaCurrencyStore *self );
static void     signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, ofaCurrencyStore *self );
static void     signaler_on_reload_collection( ofaISignaler *signaler, GType type, ofaCurrencyStore *self );

G_DEFINE_TYPE_EXTENDED( ofaCurrencyStore, ofa_currency_store, OFA_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaCurrencyStore ))

static void
currency_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_currency_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CURRENCY_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currency_store_parent_class )->finalize( instance );
}

static void
currency_store_dispose( GObject *instance )
{
	ofaCurrencyStorePrivate *priv;
	ofaISignaler *signaler;

	g_return_if_fail( instance && OFA_IS_CURRENCY_STORE( instance ));

	priv = ofa_currency_store_get_instance_private( OFA_CURRENCY_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* disconnect from ofaISignaler signaling system */
		signaler = ofa_igetter_get_signaler( priv->getter );
		ofa_isignaler_disconnect_handlers( signaler, &priv->signaler_handlers );

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currency_store_parent_class )->dispose( instance );
}

static void
ofa_currency_store_init( ofaCurrencyStore *self )
{
	static const gchar *thisfn = "ofa_currency_store_init";
	ofaCurrencyStorePrivate *priv;

	g_return_if_fail( OFA_IS_CURRENCY_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_currency_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->signaler_handlers = NULL;
}

static void
ofa_currency_store_class_init( ofaCurrencyStoreClass *klass )
{
	static const gchar *thisfn = "ofa_currency_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = currency_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = currency_store_finalize;
}

/**
 * ofa_currency_store_new:
 * @getter: a #ofaIGetter instance.
 *
 * Instanciates a new #ofaCurrencyStore and attached it to the @dossier
 * if not already done. Else get the already allocated #ofaCurrencyStore
 * from the @dossier.
 *
 * A weak notify reference is put on this same @dossier, so that the
 * instance will be unreffed when the @dossier will be destroyed.
 *
 * Note that the #myICollector associated to the @getter maintains its
 * own reference to the #ofaCurrencyStore object, reference which will
 * be freed on @getter finalization.
 *
 * Returns: a new reference to the #ofaCurrencyStore object.
 */
ofaCurrencyStore *
ofa_currency_store_new( ofaIGetter *getter )
{
	ofaCurrencyStore *store;
	ofaCurrencyStorePrivate *priv;
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );
	store = ( ofaCurrencyStore * ) my_icollector_single_get_object( collector, OFA_TYPE_CURRENCY_STORE );

	if( store ){
		g_return_val_if_fail( OFA_IS_CURRENCY_STORE( store ), NULL );

	} else {
		store = g_object_new( OFA_TYPE_CURRENCY_STORE, NULL );

		priv = ofa_currency_store_get_instance_private( store );

		priv->getter = getter;

		st_col_types[CURRENCY_COL_NOTES_PNG] = GDK_TYPE_PIXBUF;
		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), CURRENCY_N_COLUMNS, st_col_types );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		my_icollector_single_set_object( collector, store );
		signaler_connect_to_signaling_system( store );
		load_dataset( store );
	}

	return( g_object_ref( store ));
}

/*
 * sorting the self per currency code
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaCurrencyStore *self )
{
	gchar *anumber, *bnumber;
	gint cmp;

	gtk_tree_model_get( tmodel, a, CURRENCY_COL_CODE, &anumber, -1 );
	gtk_tree_model_get( tmodel, b, CURRENCY_COL_CODE, &bnumber, -1 );

	cmp = g_utf8_collate( anumber, bnumber );

	g_free( anumber );
	g_free( bnumber );

	return( cmp );
}

static void
load_dataset( ofaCurrencyStore *self )
{
	ofaCurrencyStorePrivate *priv;
	const GList *dataset, *it;
	ofoCurrency *currency;

	priv = ofa_currency_store_get_instance_private( self );

	dataset = ofo_currency_get_dataset( priv->getter );

	for( it=dataset ; it ; it=it->next ){
		currency = OFO_CURRENCY( it->data );
		insert_row( self, currency );
	}
}

static void
insert_row( ofaCurrencyStore *self, const ofoCurrency *currency )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( self ), &iter, -1 );
	set_row_by_iter( self, currency, &iter );
}

static void
set_row_by_iter( ofaCurrencyStore *self, const ofoCurrency *currency, GtkTreeIter *iter )
{
	static const gchar *thisfn = "ofa_currency_store_set_row_by_iter";
	gchar *str, *stamp;
	const gchar *notes;
	GError *error;
	GdkPixbuf *notes_png;

	str = g_strdup_printf( "%d", ofo_currency_get_digits( currency ));
	stamp  = my_stamp_to_str( ofo_currency_get_upd_stamp( currency ), MY_STAMP_DMYYHM );

	notes = ofo_currency_get_notes( currency );
	error = NULL;
	notes_png = gdk_pixbuf_new_from_resource( my_strlen( notes ) ? st_resource_notes_png : st_resource_filler_png, &error );
	if( error ){
		g_warning( "%s: gdk_pixbuf_new_from_resource: %s", thisfn, error->message );
		g_error_free( error );
	}

	gtk_list_store_set(
			GTK_LIST_STORE( self ),
			iter,
			CURRENCY_COL_CODE,      ofo_currency_get_code( currency ),
			CURRENCY_COL_LABEL,     ofo_currency_get_label( currency ),
			CURRENCY_COL_SYMBOL,    ofo_currency_get_symbol( currency ),
			CURRENCY_COL_DIGITS,    str,
			CURRENCY_COL_NOTES,     notes,
			CURRENCY_COL_NOTES_PNG, notes_png,
			CURRENCY_COL_UPD_USER,  ofo_currency_get_upd_user( currency ),
			CURRENCY_COL_UPD_STAMP, stamp,
			CURRENCY_COL_OBJECT,    currency,
			-1 );

	g_object_unref( notes_png );
	g_free( stamp );
	g_free( str );
}

static gboolean
find_currency_by_code( ofaCurrencyStore *self, const gchar *code, GtkTreeIter *iter )
{
	gchar *str;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, CURRENCY_COL_CODE, &str, -1 );
			cmp = g_utf8_collate( str, code );
			g_free( str );
			if( cmp == 0 ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), iter )){
				break;
			}
		}
	}

	return( FALSE );
}

/*
 * Connect to ofaISignaler signaling system
 */
static void
signaler_connect_to_signaling_system( ofaCurrencyStore *self )
{
	ofaCurrencyStorePrivate *priv;
	ofaISignaler *signaler;
	gulong handler;

	priv = ofa_currency_store_get_instance_private( self );

	signaler = ofa_igetter_get_signaler( priv->getter );

	handler = g_signal_connect( signaler, SIGNALER_BASE_NEW, G_CALLBACK( signaler_on_new_base ), self );
	priv->signaler_handlers = g_list_prepend( priv->signaler_handlers, ( gpointer ) handler );

	handler = g_signal_connect( signaler, SIGNALER_BASE_UPDATED, G_CALLBACK( signaler_on_updated_base ), self );
	priv->signaler_handlers = g_list_prepend( priv->signaler_handlers, ( gpointer ) handler );

	handler = g_signal_connect( signaler, SIGNALER_BASE_DELETED, G_CALLBACK( signaler_on_deleted_base ), self );
	priv->signaler_handlers = g_list_prepend( priv->signaler_handlers, ( gpointer ) handler );

	handler = g_signal_connect( signaler, SIGNALER_COLLECTION_RELOAD, G_CALLBACK( signaler_on_reload_collection ), self );
	priv->signaler_handlers = g_list_prepend( priv->signaler_handlers, ( gpointer ) handler );
}

/*
 * SIGNALER_BASE_NEW signal handler
 */
static void
signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaCurrencyStore *self )
{
	static const gchar *thisfn = "ofa_currency_store_signaler_on_new_base";

	g_debug( "%s: signaler=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_CURRENCY( object )){
		insert_row( self, OFO_CURRENCY( object ));
	}
}

/*
 * SIGNALER_BASE_UPDATED signal handler
 */
static void
signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, ofaCurrencyStore *self )
{
	static const gchar *thisfn = "ofa_currency_store_signaler_on_updated_base";
	GtkTreeIter iter;
	const gchar *code, *new_code;

	g_debug( "%s: signaler=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_CURRENCY( object )){
		new_code = ofo_currency_get_code( OFO_CURRENCY( object ));
		code = prev_id ? prev_id : new_code;
		if( find_currency_by_code( self, code, &iter )){
			set_row_by_iter( self, OFO_CURRENCY( object ), &iter);
		}
	}
}

/*
 * SIGNALER_BASE_DELETED signal handler
 */
static void
signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, ofaCurrencyStore *self )
{
	static const gchar *thisfn = "ofa_currency_store_signaler_on_deleted_base";
	GtkTreeIter iter;

	g_debug( "%s: signaler=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_CURRENCY( object )){
		if( find_currency_by_code( self,
				ofo_currency_get_code( OFO_CURRENCY( object )), &iter )){

			gtk_list_store_remove( GTK_LIST_STORE( self ), &iter );
		}
	}
}

/*
 * SIGNALER_COLLECTION_RELOAD signal handler
 */
static void
signaler_on_reload_collection( ofaISignaler *signaler, GType type, ofaCurrencyStore *self )
{
	static const gchar *thisfn = "ofa_currency_store_signaler_on_reload_collection";

	g_debug( "%s: signaler=%p, type=%lu, self=%p",
			thisfn, ( void * ) signaler, type, ( void * ) self );

	if( type == OFO_TYPE_CURRENCY ){
		gtk_list_store_clear( GTK_LIST_STORE( self ));
		load_dataset( self );
	}
}
