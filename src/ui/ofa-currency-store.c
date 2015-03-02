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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/my-utils.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-currency-store.h"

/* private instance data
 */
struct _ofaCurrencyStorePrivate {
	gboolean    dispose_has_run;

	/*
	 */
};

static GType st_col_types[CURRENCY_N_COLUMNS] = {
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* code, label, symbol */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* digits, notes, upd_user */
		G_TYPE_STRING,									/* upd_stamp */
		G_TYPE_OBJECT									/* the #ofoCurrency itself */
};

/* the key which is attached to the dossier in order to identify this
 * store
 */
#define STORE_DATA_DOSSIER                   "ofa-currency-store"

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaCurrencyStore *store );
static void     load_dataset( ofaCurrencyStore *store, ofoDossier *dossier );
static void     insert_row( ofaCurrencyStore *store, ofoDossier *dossier, const ofoCurrency *currency );
static void     set_row( ofaCurrencyStore *store, ofoDossier *dossier, const ofoCurrency *currency, GtkTreeIter *iter );
static void     setup_signaling_connect( ofaCurrencyStore *store, ofoDossier *dossier );
static void     on_new_object( ofoDossier *dossier, ofoBase *object, ofaCurrencyStore *store );
static void     on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaCurrencyStore *store );
static gboolean find_currency_by_code( ofaCurrencyStore *store, const gchar *code, GtkTreeIter *iter );
static void     on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaCurrencyStore *store );
static void     on_reload_dataset( ofoDossier *dossier, GType type, ofaCurrencyStore *store );

G_DEFINE_TYPE( ofaCurrencyStore, ofa_currency_store, OFA_TYPE_LIST_STORE )

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

	g_return_if_fail( instance && OFA_IS_CURRENCY_STORE( instance ));

	priv = OFA_CURRENCY_STORE( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currency_store_parent_class )->dispose( instance );
}

static void
ofa_currency_store_init( ofaCurrencyStore *self )
{
	static const gchar *thisfn = "ofa_currency_store_init";

	g_return_if_fail( OFA_IS_CURRENCY_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_CURRENCY_STORE, ofaCurrencyStorePrivate );
}

static void
ofa_currency_store_class_init( ofaCurrencyStoreClass *klass )
{
	static const gchar *thisfn = "ofa_currency_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = currency_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = currency_store_finalize;

	g_type_class_add_private( klass, sizeof( ofaCurrencyStorePrivate ));
}

/**
 * ofa_currency_store_new:
 * @dossier: the currently opened #ofoDossier.
 *
 * Instanciates a new #ofaCurrencyStore and attached it to the @dossier
 * if not already done. Else get the already allocated #ofaCurrencyStore
 * from the @dossier.
 *
 * A weak notify reference is put on this same @dossier, so that the
 * instance will be unreffed when the @dossier will be destroyed.
 */
ofaCurrencyStore *
ofa_currency_store_new( ofoDossier *dossier )
{
	ofaCurrencyStore *store;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	store = ( ofaCurrencyStore * ) g_object_get_data( G_OBJECT( dossier ), STORE_DATA_DOSSIER );

	if( store ){
		g_return_val_if_fail( OFA_IS_CURRENCY_STORE( store ), NULL );

	} else {
		store = g_object_new(
						OFA_TYPE_CURRENCY_STORE,
						OFA_PROP_DOSSIER, dossier,
						NULL );

		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), CURRENCY_N_COLUMNS, st_col_types );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		g_object_set_data( G_OBJECT( dossier ), STORE_DATA_DOSSIER, store );

		load_dataset( store, dossier );
		setup_signaling_connect( store, dossier );
	}

	return( store );
}

/*
 * sorting the store per currency code
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaCurrencyStore *store )
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
load_dataset( ofaCurrencyStore *store, ofoDossier *dossier )
{
	const GList *dataset, *it;
	ofoCurrency *currency;

	dataset = ofo_currency_get_dataset( dossier );

	for( it=dataset ; it ; it=it->next ){
		currency = OFO_CURRENCY( it->data );
		insert_row( store, dossier, currency );
	}
}

static void
insert_row( ofaCurrencyStore *store, ofoDossier *dossier, const ofoCurrency *currency )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( store ), &iter, -1 );
	set_row( store, dossier, currency, &iter );
}

static void
set_row( ofaCurrencyStore *store, ofoDossier *dossier, const ofoCurrency *currency, GtkTreeIter *iter )
{
	gchar *str, *stamp;

	str = g_strdup_printf( "%d", ofo_currency_get_digits( currency ));
	stamp  = my_utils_stamp_to_str( ofo_currency_get_upd_stamp( currency ), MY_STAMP_DMYYHM );

	gtk_list_store_set(
			GTK_LIST_STORE( store ),
			iter,
			CURRENCY_COL_CODE,      ofo_currency_get_code( currency ),
			CURRENCY_COL_LABEL,     ofo_currency_get_label( currency ),
			CURRENCY_COL_SYMBOL,    ofo_currency_get_symbol( currency ),
			CURRENCY_COL_DIGITS,    str,
			CURRENCY_COL_UPD_USER,  ofo_currency_get_upd_user( currency ),
			CURRENCY_COL_UPD_STAMP, stamp,
			CURRENCY_COL_OBJECT,    currency,
			-1 );

	g_free( stamp );
	g_free( str );
}

/*
 * connect to the dossier signaling system
 * there is no need to keep trace of the signal handlers, as the lifetime
 * of this store is equal to those of the dossier
 */
static void
setup_signaling_connect( ofaCurrencyStore *store, ofoDossier *dossier )
{
	g_signal_connect(
			G_OBJECT( dossier ),
			SIGNAL_DOSSIER_NEW_OBJECT, G_CALLBACK( on_new_object ), store );

	g_signal_connect(
			G_OBJECT( dossier ),
			SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), store );

	g_signal_connect(
			G_OBJECT( dossier ),
			SIGNAL_DOSSIER_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), store );

	g_signal_connect(
			G_OBJECT( dossier ),
			SIGNAL_DOSSIER_RELOAD_DATASET, G_CALLBACK( on_reload_dataset ), store );
}

static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaCurrencyStore *store )
{
	static const gchar *thisfn = "ofa_currency_store_on_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) store );

	if( OFO_IS_CURRENCY( object )){
		insert_row( store, dossier, OFO_CURRENCY( object ));
	}
}

static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaCurrencyStore *store )
{
	static const gchar *thisfn = "ofa_currency_store_on_updated_object";
	GtkTreeIter iter;
	const gchar *code, *new_code;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, store=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) store );

	if( OFO_IS_CURRENCY( object )){
		new_code = ofo_currency_get_code( OFO_CURRENCY( object ));
		code = prev_id ? prev_id : new_code;
		if( find_currency_by_code( store, code, &iter )){
			set_row( store, dossier, OFO_CURRENCY( object ), &iter);
		}
	}
}

static gboolean
find_currency_by_code( ofaCurrencyStore *store, const gchar *code, GtkTreeIter *iter )
{
	gchar *str;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( store ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( store ), iter, CURRENCY_COL_CODE, &str, -1 );
			cmp = g_utf8_collate( str, code );
			g_free( str );
			if( cmp == 0 ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( store ), iter )){
				break;
			}
		}
	}

	return( FALSE );
}

static void
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaCurrencyStore *store )
{
	static const gchar *thisfn = "ofa_currency_store_on_deleted_object";
	GtkTreeIter iter;

	g_debug( "%s: dossier=%p, object=%p (%s), store=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) store );

	if( OFO_IS_CURRENCY( object )){
		if( find_currency_by_code( store,
				ofo_currency_get_code( OFO_CURRENCY( object )), &iter )){

			gtk_list_store_remove( GTK_LIST_STORE( store ), &iter );
		}
	}
}

static void
on_reload_dataset( ofoDossier *dossier, GType type, ofaCurrencyStore *store )
{
	static const gchar *thisfn = "ofa_currency_store_on_reload_dataset";

	g_debug( "%s: dossier=%p, type=%lu, store=%p",
			thisfn, ( void * ) dossier, type, ( void * ) store );

	if( type == OFO_TYPE_CURRENCY ){
		gtk_list_store_clear( GTK_LIST_STORE( store ));
		load_dataset( store, dossier );
	}
}
