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

#include "api/my-utils.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-currency-istore.h"

/* data associated to each implementor object
 */
typedef struct {

	/* static data
	 * to be set at initialization time
	 */
	ofaCurrencyColumns columns;
	ofoDossier        *dossier;

	/* runtime data
	 */
	GtkListStore      *store;
	GList             *handlers;
}
	sIStore;

/* columns ordering in the store
 */
enum {
	COL_CODE = 0,
	COL_LABEL,
	COL_SYMBOL,
	COL_DIGITS,
	COL_NOTES,
	COL_UPD_USER,
	COL_UPD_STAMP,
	COL_OBJECT,
	N_COLUMNS
};

#define CURRENCY_ISTORE_LAST_VERSION    1
#define CURRENCY_ISTORE_DATA            "ofa-currency-istore-data"

static guint st_initializations = 0;	/* interface initialization count */

static GType    register_type( void );
static void     interface_base_init( ofaCurrencyIStoreInterface *klass );
static void     interface_base_finalize( ofaCurrencyIStoreInterface *klass );
static void     load_dataset( ofaCurrencyIStore *instance, sIStore *sdata );
static void     insert_row( ofaCurrencyIStore *instance, sIStore *sdata, const ofoCurrency *currency );
static void     set_row( ofaCurrencyIStore *instance, sIStore *sdata, GtkTreeIter *iter, const ofoCurrency *currency );
static void     setup_signaling_connect( ofaCurrencyIStore *instance, sIStore *sdata );
static void     on_new_object( ofoDossier *dossier, ofoBase *object, ofaCurrencyIStore *instance );
static void     on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaCurrencyIStore *instance );
static gboolean find_currency_by_code( ofaCurrencyIStore *instance, sIStore *sdata, const gchar *code, GtkTreeIter *iter );
static void     on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaCurrencyIStore *instance );
static void     on_reload_dataset( ofoDossier *dossier, GType type, ofaCurrencyIStore *instance );
static void     on_parent_finalized( ofaCurrencyIStore *instance, gpointer finalized_parent );
static void     on_object_finalized( sIStore *sdata, gpointer finalized_object );
static sIStore *get_istore_data( ofaCurrencyIStore *instance );

/**
 * ofa_currency_istore_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_currency_istore_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_currency_istore_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_currency_istore_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaCurrencyIStoreInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaCurrencyIStore", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaCurrencyIStoreInterface *klass )
{
	static const gchar *thisfn = "ofa_currency_istore_interface_base_init";

	g_debug( "%s: klass=%p (%s), st_initializations=%d",
			thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ), st_initializations );

	st_initializations += 1;

	if( st_initializations == 1 ){
		/* declare interface default methods here */
	}
}

static void
interface_base_finalize( ofaCurrencyIStoreInterface *klass )
{
	static const gchar *thisfn = "ofa_currency_istore_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_currency_istore_get_interface_last_version:
 * @instance: this #ofaCurrencyIStore instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_currency_istore_get_interface_last_version( const ofaCurrencyIStore *instance )
{
	return( CURRENCY_ISTORE_LAST_VERSION );
}

/**
 * ofa_currency_istore_attach_to:
 * @instance: this #ofaCurrencyIStore instance.
 * @parent: the #GtkContainer to which the widget should be attached.
 *
 * Attach the widget to its parent.
 *
 * A weak notify reference is put on the parent, so that the GObject
 * will be unreffed when the parent will be destroyed.
 */
void
ofa_currency_istore_attach_to( ofaCurrencyIStore *instance, GtkContainer *parent )
{
	sIStore *sdata;

	g_return_if_fail( instance && G_IS_OBJECT( instance ) && OFA_IS_CURRENCY_ISTORE( instance ));
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	g_object_weak_ref( G_OBJECT( parent ), ( GWeakNotify ) on_parent_finalized, instance );

	if( OFA_CURRENCY_ISTORE_GET_INTERFACE( instance )->attach_to ){
		OFA_CURRENCY_ISTORE_GET_INTERFACE( instance )->attach_to( instance, parent );
	}

	gtk_widget_show_all( GTK_WIDGET( parent ));
}

/**
 * ofa_currency_istore_set_columns:
 * @instance: this #ofaCurrencyIStore instance.
 * @columns: the columns to be displayed from the #GtkListStore.
 */
void
ofa_currency_istore_set_columns( ofaCurrencyIStore *instance, ofaCurrencyColumns columns )
{
	sIStore *sdata;

	g_return_if_fail( instance && G_IS_OBJECT( instance ) && OFA_IS_CURRENCY_ISTORE( instance ));

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	sdata->columns = columns;

	sdata->store = gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* code, label, symbol */
			G_TYPE_STRING, G_TYPE_STRING, 					/* digits, notes */
			G_TYPE_STRING, G_TYPE_STRING,					/* upd_user, upd_stamp */
			G_TYPE_OBJECT );								/* the #ofoCurrency itself */

	if( OFA_CURRENCY_ISTORE_GET_INTERFACE( instance )->set_columns ){
		OFA_CURRENCY_ISTORE_GET_INTERFACE( instance )->set_columns( instance, sdata->store, columns );
	}

	g_object_unref( sdata->store );
}

/**
 * ofa_currency_istore_set_dossier:
 * @instance: this #ofaCurrencyIStore instance.
 * @dossier: the opened #ofoDossier.
 *
 * Set the dossier and load the corresponding dataset.
 * Connect to the dossier signaling system in order to maintain the
 * dataset up to date.
 */
void
ofa_currency_istore_set_dossier( ofaCurrencyIStore *instance, ofoDossier *dossier )
{
	sIStore *sdata;

	g_return_if_fail( instance && G_IS_OBJECT( instance ) && OFA_IS_CURRENCY_ISTORE( instance ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	sdata->dossier = dossier;

	load_dataset( instance, sdata );
	setup_signaling_connect( instance, sdata );
}

static void
load_dataset( ofaCurrencyIStore *instance, sIStore *sdata )
{
	const GList *dataset, *it;
	ofoCurrency *currency;

	dataset = ofo_currency_get_dataset( sdata->dossier );

	for( it=dataset ; it ; it=it->next ){
		currency = OFO_CURRENCY( it->data );
		insert_row( instance, sdata, currency );
	}
}

static void
insert_row( ofaCurrencyIStore *instance, sIStore *sdata, const ofoCurrency *currency )
{
	GtkTreeIter iter;

	gtk_list_store_insert( sdata->store, &iter, -1 );
	set_row( instance, sdata, &iter, currency );
}

static void
set_row( ofaCurrencyIStore *instance, sIStore *sdata, GtkTreeIter *iter, const ofoCurrency *currency )
{
	gchar *str, *stamp;

	str = g_strdup_printf( "%d", ofo_currency_get_digits( currency ));
	stamp  = my_utils_stamp_to_str( ofo_currency_get_upd_stamp( currency ), MY_STAMP_DMYYHM );

	gtk_list_store_set(
			sdata->store,
			iter,
			COL_CODE,      ofo_currency_get_code( currency ),
			COL_LABEL,     ofo_currency_get_label( currency ),
			COL_SYMBOL,    ofo_currency_get_symbol( currency ),
			COL_DIGITS,    str,
			COL_UPD_USER,  ofo_currency_get_upd_user( currency ),
			COL_UPD_STAMP, stamp,
			COL_OBJECT,    currency,
			-1 );

	g_free( stamp );
	g_free( str );
}

static void
setup_signaling_connect( ofaCurrencyIStore *instance, sIStore *sdata )
{
	gulong handler;

	handler = g_signal_connect( G_OBJECT(
					sdata->dossier ),
					SIGNAL_DOSSIER_NEW_OBJECT, G_CALLBACK( on_new_object ), instance );
	sdata->handlers = g_list_prepend( sdata->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
					G_OBJECT( sdata->dossier ),
					SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), instance );
	sdata->handlers = g_list_prepend( sdata->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
					G_OBJECT( sdata->dossier ),
					SIGNAL_DOSSIER_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), instance );
	sdata->handlers = g_list_prepend( sdata->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
					G_OBJECT( sdata->dossier ),
					SIGNAL_DOSSIER_RELOAD_DATASET, G_CALLBACK( on_reload_dataset ), instance );
	sdata->handlers = g_list_prepend( sdata->handlers, ( gpointer ) handler );
}

static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaCurrencyIStore *instance )
{
	static const gchar *thisfn = "ofa_currency_istore_on_new_object";
	sIStore *sdata;

	g_debug( "%s: dossier=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) instance );

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	if( OFO_IS_CURRENCY( object )){
		insert_row( instance, sdata, OFO_CURRENCY( object ));
	}
}

static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaCurrencyIStore *instance )
{
	static const gchar *thisfn = "ofa_currency_istore_on_updated_object";
	sIStore *sdata;
	GtkTreeIter iter;
	const gchar *code, *new_code;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, instance=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) instance );

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	if( OFO_IS_CURRENCY( object )){
		new_code = ofo_currency_get_code( OFO_CURRENCY( object ));
		code = prev_id ? prev_id : new_code;
		if( find_currency_by_code( instance, sdata, code, &iter )){
			set_row( instance, sdata, &iter, OFO_CURRENCY( object ));
		}
	}
}

static gboolean
find_currency_by_code( ofaCurrencyIStore *instance, sIStore *sdata, const gchar *code, GtkTreeIter *iter )
{
	GtkTreeModel *tmodel;
	gchar *str;
	gint cmp;

	tmodel = GTK_TREE_MODEL( sdata->store );

	if( gtk_tree_model_get_iter_first( tmodel, iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, iter, COL_CODE, &str, -1 );
			cmp = g_utf8_collate( str, code );
			g_free( str );
			if( cmp == 0 ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( tmodel, iter )){
				break;
			}
		}
	}

	return( FALSE );
}

static void
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaCurrencyIStore *instance )
{
	static const gchar *thisfn = "ofa_currency_istore_on_deleted_object";
	sIStore *sdata;
	GtkTreeIter iter;

	g_debug( "%s: dossier=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) instance );

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	if( OFO_IS_CURRENCY( object )){
		if( find_currency_by_code( instance, sdata,
				ofo_currency_get_code( OFO_CURRENCY( object )), &iter )){

			gtk_list_store_remove( sdata->store, &iter );
		}
	}
}

static void
on_reload_dataset( ofoDossier *dossier, GType type, ofaCurrencyIStore *instance )
{
	static const gchar *thisfn = "ofa_currency_istore_on_reload_dataset";
	sIStore *sdata;

	g_debug( "%s: dossier=%p, type=%lu, instance=%p",
			thisfn, ( void * ) dossier, type, ( void * ) instance );

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	if( type == OFO_TYPE_CURRENCY ){
		gtk_list_store_clear( sdata->store );
		load_dataset( instance, sdata );
	}
}

/**
 * ofa_currency_istore_get_column_number:
 * @instance: this #ofaCurrencyIStore instance.
 * @column: the #ofaCurrencyColumns identifier.
 *
 * Returns: the number of the column in the store, counted from zero,
 * or -1 if the column is unknown.
 */
gint
ofa_currency_istore_get_column_number( const ofaCurrencyIStore *instance, ofaCurrencyColumns column )
{
	static const gchar *thisfn = "ofa_currency_istore_get_column_number";

	switch( column ){
		case CURRENCY_COL_CODE:
			return( COL_CODE );
			break;

		case CURRENCY_COL_LABEL:
			return( COL_LABEL );
			break;

		case CURRENCY_COL_SYMBOL:
			return( COL_SYMBOL );
			break;

		case CURRENCY_COL_DIGITS:
			return( COL_DIGITS );
			break;

		case CURRENCY_COL_NOTES:
			return( COL_NOTES );
			break;

		case CURRENCY_COL_UPD_USER:
			return( COL_UPD_USER );
			break;

		case CURRENCY_COL_UPD_STAMP:
			return( COL_UPD_STAMP );
			break;
	}

	g_warning( "%s: unknown column:%d", thisfn, column );
	return( -1 );
}

static sIStore *
get_istore_data( ofaCurrencyIStore *instance )
{
	sIStore *sdata;

	sdata = ( sIStore * ) g_object_get_data( G_OBJECT( instance ), CURRENCY_ISTORE_DATA );

	if( !sdata ){
		sdata = g_new0( sIStore, 1 );
		g_object_set_data( G_OBJECT( instance ), CURRENCY_ISTORE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_object_finalized, sdata );
	}

	return( sdata );
}

static void
on_parent_finalized( ofaCurrencyIStore *instance, gpointer finalized_parent )
{
	static const gchar *thisfn = "ofa_currency_istore_on_parent_finalized";

	g_debug( "%s: instance=%p, finalized_parent=%p",
			thisfn, ( void * ) instance, ( void * ) finalized_parent );

	g_return_if_fail( instance );

	g_object_unref( G_OBJECT( instance ));
}

static void
on_object_finalized( sIStore *sdata, gpointer finalized_object )
{
	static const gchar *thisfn = "ofa_currency_istore_on_object_finalized";
	GList *it;

	g_debug( "%s: sdata=%p, finalized_object=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_object );

	g_return_if_fail( sdata );

	if( sdata->dossier && OFO_IS_DOSSIER( sdata->dossier )){
		for( it=sdata->handlers ; it ; it=it->next ){
			g_signal_handler_disconnect( sdata->dossier, ( gulong ) it->data );
		}
	}

	g_free( sdata );
}
