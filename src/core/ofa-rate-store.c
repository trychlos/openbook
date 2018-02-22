/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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
#include "api/ofo-rate.h"

#include "core/ofa-rate-store.h"

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
	ofaRateStorePrivate;

static GType st_col_types[RATE_N_COLUMNS] = {
		G_TYPE_STRING,  G_TYPE_STRING,  G_TYPE_STRING,		/* mnemo, cre_user, cre_stamp */
		G_TYPE_STRING,  G_TYPE_STRING,  0,					/* label, notes, notes_png */
		G_TYPE_STRING,  G_TYPE_STRING,						/* upd_user, upd_stamp */
		G_TYPE_OBJECT										/* the #ofoRate itself */
};

static const gchar *st_resource_filler_png  = "/org/trychlos/openbook/core/filler.png";
static const gchar *st_resource_notes_png   = "/org/trychlos/openbook/core/notes.png";

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaRateStore *self );
static void     load_dataset( ofaRateStore *self );
static void     insert_row( ofaRateStore *self, const ofoRate *rate );
static void     set_row_by_iter( ofaRateStore *self, const ofoRate *rate, GtkTreeIter *iter );
static gboolean find_rate_by_mnemo( ofaRateStore *self, const gchar *code, GtkTreeIter *iter );
static void     signaler_connect_to_signaling_system( ofaRateStore *self );
static void     signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaRateStore *self );
static void     signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, ofaRateStore *self );
static void     signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, ofaRateStore *self );
static void     signaler_on_reload_collection( ofaISignaler *signaler, GType type, ofaRateStore *self );

G_DEFINE_TYPE_EXTENDED( ofaRateStore, ofa_rate_store, OFA_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaRateStore ))

static void
rate_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_rate_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RATE_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rate_store_parent_class )->finalize( instance );
}

static void
rate_store_dispose( GObject *instance )
{
	ofaRateStorePrivate *priv;
	ofaISignaler *signaler;

	g_return_if_fail( instance && OFA_IS_RATE_STORE( instance ));

	priv = ofa_rate_store_get_instance_private( OFA_RATE_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* disconnect from ofaISignaler signaling system */
		signaler = ofa_igetter_get_signaler( priv->getter );
		ofa_isignaler_disconnect_handlers( signaler, &priv->signaler_handlers );

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rate_store_parent_class )->dispose( instance );
}

static void
ofa_rate_store_init( ofaRateStore *self )
{
	static const gchar *thisfn = "ofa_rate_store_init";
	ofaRateStorePrivate *priv;

	g_return_if_fail( OFA_IS_RATE_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_rate_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->signaler_handlers = NULL;
}

static void
ofa_rate_store_class_init( ofaRateStoreClass *klass )
{
	static const gchar *thisfn = "ofa_rate_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = rate_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = rate_store_finalize;
}

/**
 * ofa_rate_store_new:
 * @getter: a #ofaIGetter instance.
 *
 * Instanciates a new #ofaRateStore and attached it to the
 * #myICollector if not already done. Else get the already allocated
 * #ofaRateStore from this same #myICollector.
 *
 * Returns: a new reference to the #ofaRateStore object.
 */
ofaRateStore *
ofa_rate_store_new( ofaIGetter *getter )
{
	ofaRateStore *store;
	ofaRateStorePrivate *priv;
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );
	store = ( ofaRateStore * ) my_icollector_single_get_object( collector, OFA_TYPE_RATE_STORE );

	if( store ){
		g_return_val_if_fail( OFA_IS_RATE_STORE( store ), NULL );

	} else {
		store = g_object_new( OFA_TYPE_RATE_STORE, NULL );

		priv = ofa_rate_store_get_instance_private( store );

		priv->getter = getter;

		st_col_types[RATE_COL_NOTES_PNG] = GDK_TYPE_PIXBUF;
		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), RATE_N_COLUMNS, st_col_types );

		load_dataset( store );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		my_icollector_single_set_object( collector, store );
		signaler_connect_to_signaling_system( store );
	}

	return( g_object_ref( store ));
}

/*
 * sorting the self per rate code
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaRateStore *self )
{
	gchar *mnemoa, *mnemob;
	gint cmp;

	gtk_tree_model_get( tmodel, a, RATE_COL_MNEMO, &mnemoa, -1 );
	gtk_tree_model_get( tmodel, b, RATE_COL_MNEMO, &mnemob, -1 );

	cmp = my_collate( mnemoa, mnemob );

	g_free( mnemoa );
	g_free( mnemob );

	return( cmp );
}

static void
load_dataset( ofaRateStore *self )
{
	ofaRateStorePrivate *priv;
	const GList *dataset, *it;
	ofoRate *rate;

	priv = ofa_rate_store_get_instance_private( self );

	dataset = ofo_rate_get_dataset( priv->getter );

	for( it=dataset ; it ; it=it->next ){
		rate = OFO_RATE( it->data );
		insert_row( self, rate );
	}
}

static void
insert_row( ofaRateStore *self, const ofoRate *rate )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( self ), &iter, -1 );
	set_row_by_iter( self, rate, &iter );
}

static void
set_row_by_iter( ofaRateStore *self, const ofoRate *rate, GtkTreeIter *iter )
{
	static const gchar *thisfn = "ofa_rate_store_set_row";
	gchar *crestamp, *updstamp;
	const gchar *notes;
	GError *error;
	GdkPixbuf *notes_png;

	crestamp  = my_stamp_to_str( ofo_rate_get_cre_stamp( rate ), MY_STAMP_DMYYHM );
	updstamp  = my_stamp_to_str( ofo_rate_get_upd_stamp( rate ), MY_STAMP_DMYYHM );

	notes = ofo_rate_get_notes( rate );
	error = NULL;
	notes_png = gdk_pixbuf_new_from_resource( my_strlen( notes ) ? st_resource_notes_png : st_resource_filler_png, &error );
	if( error ){
		g_warning( "%s: gdk_pixbuf_new_from_resource: %s", thisfn, error->message );
		g_error_free( error );
	}

	gtk_list_store_set(
			GTK_LIST_STORE( self ),
			iter,
			RATE_COL_MNEMO,     ofo_rate_get_mnemo( rate ),
			RATE_COL_CRE_USER,  ofo_rate_get_cre_user( rate ),
			RATE_COL_CRE_STAMP, crestamp,
			RATE_COL_LABEL,     ofo_rate_get_label( rate ),
			RATE_COL_NOTES,     notes,
			RATE_COL_NOTES_PNG, notes_png,
			RATE_COL_UPD_USER,  ofo_rate_get_upd_user( rate ),
			RATE_COL_UPD_STAMP, updstamp,
			RATE_COL_OBJECT,    rate,
			-1 );

	g_object_unref( notes_png );
	g_free( crestamp );
	g_free( updstamp );
}

static gboolean
find_rate_by_mnemo( ofaRateStore *self, const gchar *code, GtkTreeIter *iter )
{
	gchar *str;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, RATE_COL_MNEMO, &str, -1 );
			cmp = my_collate( str, code );
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
signaler_connect_to_signaling_system( ofaRateStore *self )
{
	ofaRateStorePrivate *priv;
	ofaISignaler *signaler;
	gulong handler;

	priv = ofa_rate_store_get_instance_private( self );

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
signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaRateStore *self )
{
	static const gchar *thisfn = "ofa_rate_store_signaler_on_new_base";

	g_debug( "%s: signaler=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_RATE( object )){
		insert_row( self, OFO_RATE( object ));
	}
}

/*
 * SIGNALER_BASE_UPDATED signal handler
 */
static void
signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, ofaRateStore *self )
{
	static const gchar *thisfn = "ofa_rate_store_signaler_on_updated_base";
	GtkTreeIter iter;
	const gchar *mnemo, *new_mnemo;

	g_debug( "%s: signaler=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_RATE( object )){
		new_mnemo = ofo_rate_get_mnemo( OFO_RATE( object ));
		mnemo = prev_id ? prev_id : new_mnemo;
		if( find_rate_by_mnemo( self, mnemo, &iter )){
			set_row_by_iter( self, OFO_RATE( object ), &iter);
		}
	}
}

/*
 * SIGNALER_BASE_DELETED signal handler
 */
static void
signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, ofaRateStore *self )
{
	static const gchar *thisfn = "ofa_rate_store_signaler_on_deleted_base";
	GtkTreeIter iter;

	g_debug( "%s: signaler=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_RATE( object )){
		if( find_rate_by_mnemo( self,
				ofo_rate_get_mnemo( OFO_RATE( object )), &iter )){

			gtk_list_store_remove( GTK_LIST_STORE( self ), &iter );
		}
	}
}

/*
 * SIGNALER_COLLECTION_RELOAD signal handler
 */
static void
signaler_on_reload_collection( ofaISignaler *signaler, GType type, ofaRateStore *self )
{
	static const gchar *thisfn = "ofa_rate_store_signaler_on_reload_collection";

	g_debug( "%s: signaler=%p, type=%lu, self=%p",
			thisfn, ( void * ) signaler, type, ( void * ) self );

	if( type == OFO_TYPE_RATE ){
		gtk_list_store_clear( GTK_LIST_STORE( self ));
		load_dataset( self );
	}
}
