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

#include "api/ofa-amount.h"
#include "api/ofa-counter.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-preferences.h"

#include "recurrent/ofa-recurrent-run-store.h"
#include "recurrent/ofo-recurrent-model.h"
#include "recurrent/ofo-recurrent-run.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;

	/* runtime
	 */
	ofaHub     *hub;
	GList      *hub_handlers;
	gint        mode;						/* from_db or from_list */
}
	ofaRecurrentRunStorePrivate;

static GType st_col_types[REC_RUN_N_COLUMNS] = {
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_ULONG,		/* mnemo, numseq, numseq_int */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* label, date, status */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* amount1, amount2, amount3 */
		G_TYPE_OBJECT, G_TYPE_OBJECT					/* the #ofoRecurrentRun itself, the #ofoRecurrentModel */
};

static ofaRecurrentRunStore *create_new_store( ofaIGetter *getter, gint mode );
static void                  load_dataset( ofaRecurrentRunStore *self );
static gint                  on_sort_run( GtkTreeModel *trun, GtkTreeIter *a, GtkTreeIter *b, ofaRecurrentRunStore *self );
static void                  do_insert_dataset( ofaRecurrentRunStore *self, const GList *dataset );
static void                  insert_row( ofaRecurrentRunStore *self, const ofoRecurrentRun *run );
static void                  set_row_by_iter( ofaRecurrentRunStore *self, const ofoRecurrentRun *run, GtkTreeIter *iter );
static gboolean              find_row_by_object( ofaRecurrentRunStore *self, ofoRecurrentRun *run, GtkTreeIter *iter );
static void                  hub_connect_to_signaling_system( ofaRecurrentRunStore *self );
static void                  hub_on_new_object( ofaHub *hub, ofoBase *object, ofaRecurrentRunStore *self );
static void                  hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaRecurrentRunStore *self );
static void                  hub_on_updated_recurrent_model_mnemo( ofaRecurrentRunStore *self, const gchar *prev_mnemo, const gchar *new_mnemo );
static void                  hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaRecurrentRunStore *self );
static void                  hub_on_reload_dataset( ofaHub *hub, GType type, ofaRecurrentRunStore *self );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentRunStore, ofa_recurrent_run_store, OFA_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaRecurrentRunStore ))

static void
recurrent_run_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recurrent_run_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECURRENT_RUN_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_run_store_parent_class )->finalize( instance );
}

static void
recurrent_run_store_dispose( GObject *instance )
{
	ofaRecurrentRunStorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_RECURRENT_RUN_STORE( instance ));

	priv = ofa_recurrent_run_store_get_instance_private( OFA_RECURRENT_RUN_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		ofa_hub_disconnect_handlers( priv->hub, &priv->hub_handlers );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_run_store_parent_class )->dispose( instance );
}

static void
ofa_recurrent_run_store_init( ofaRecurrentRunStore *self )
{
	static const gchar *thisfn = "ofa_recurrent_run_store_init";
	ofaRecurrentRunStorePrivate *priv;

	g_return_if_fail( OFA_IS_RECURRENT_RUN_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_recurrent_run_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->hub_handlers = NULL;
}

static void
ofa_recurrent_run_store_class_init( ofaRecurrentRunStoreClass *klass )
{
	static const gchar *thisfn = "ofa_recurrent_run_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_run_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_run_store_finalize;
}

/**
 * ofa_recurrent_run_store_new:
 * @getter: a #ofaIGetter instance.
 * @mode: whether data come from DBMS or from a provided list.
 *
 * In from DBMS mode, instanciates a new #ofaRecurrentRunStore and
 * attached it to the @hub if not already done. Else get the already
 * allocated #ofaRecurrentRunStore from the @hub.
 *
 * A weak notify reference is put on this same @dossier, so that the
 * instance will be unreffed when the @dossier will be destroyed.
 *
 * Note that the #myICollector associated to the @hub maintains its own
 * reference to the #ofaRecurrentRunStore object, reference which will
 * be freed on @hub finalization.
 *
 * In from list mode, a new store is always allocated.
 *
 * Returns: a new reference to the #ofaRecurrentStore object.
 */
ofaRecurrentRunStore *
ofa_recurrent_run_store_new( ofaIGetter *getter, gint mode )
{
	ofaRecurrentRunStore *store;
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( mode == REC_MODE_FROM_DBMS || mode == REC_MODE_FROM_LIST, NULL );

	if( mode == REC_MODE_FROM_DBMS ){

		collector = ofa_igetter_get_collector( getter );
		store = ( ofaRecurrentRunStore * ) my_icollector_single_get_object( collector, OFA_TYPE_RECURRENT_RUN_STORE );

		if( store ){
			g_return_val_if_fail( OFA_IS_RECURRENT_RUN_STORE( store ), NULL );

		} else {
			store = create_new_store( getter, mode );
			my_icollector_single_set_object( collector, store );
			load_dataset( store );
		}

	} else {
		store = create_new_store( getter, mode );
	}

	return( store );
}

static ofaRecurrentRunStore *
create_new_store( ofaIGetter *getter, gint mode )
{
	ofaRecurrentRunStore *store;
	ofaRecurrentRunStorePrivate *priv;

	store = g_object_new( OFA_TYPE_RECURRENT_RUN_STORE, NULL );

	priv = ofa_recurrent_run_store_get_instance_private( store );

	priv->getter = getter;
	priv->hub = ofa_igetter_get_hub( getter );
	priv->mode = mode;

	gtk_list_store_set_column_types(
			GTK_LIST_STORE( store ), REC_RUN_N_COLUMNS, st_col_types );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_run, store, NULL );
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( store ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	hub_connect_to_signaling_system( store );

	return( store );
}

static void
load_dataset( ofaRecurrentRunStore *self )
{
	ofaRecurrentRunStorePrivate *priv;
	GList *dataset;

	priv = ofa_recurrent_run_store_get_instance_private( self );

	dataset = ofo_recurrent_run_get_dataset( priv->getter );
	do_insert_dataset( self, dataset );
}

/*
 * sorting the store per run code
 */
static gint
on_sort_run( GtkTreeModel *trun, GtkTreeIter *a, GtkTreeIter *b, ofaRecurrentRunStore *self )
{
	gchar *amnemo, *bmnemo;
	gint cmp;

	gtk_tree_model_get( trun, a, REC_RUN_COL_MNEMO, &amnemo, -1 );
	gtk_tree_model_get( trun, b, REC_RUN_COL_MNEMO, &bmnemo, -1 );

	cmp = g_utf8_collate( amnemo, bmnemo );

	g_free( amnemo );
	g_free( bmnemo );

	return( cmp );
}

/**
 * ofa_recurrent_run_store_set_from_list:
 * @store: this #ofaRecurrentRunSgtore instance.
 * @dataset: a list of objects to be added to the store.
 *
 * Stores the provided list of objects.
 */
void
ofa_recurrent_run_store_set_from_list( ofaRecurrentRunStore *store, GList *dataset )
{
	ofaRecurrentRunStorePrivate *priv;

	g_return_if_fail( store && OFA_IS_RECURRENT_RUN_STORE( store ));

	priv = ofa_recurrent_run_store_get_instance_private( store );

	g_return_if_fail( !priv->dispose_has_run );
	g_return_if_fail( priv->mode == REC_MODE_FROM_LIST );

	do_insert_dataset( store, dataset );
}

static void
do_insert_dataset( ofaRecurrentRunStore *self, const GList *dataset )
{
	const GList *it;
	ofoRecurrentRun *run;

	for( it=dataset ; it ; it=it->next ){
		run = OFO_RECURRENT_RUN( it->data );
		insert_row( self, run );
	}
}

static void
insert_row( ofaRecurrentRunStore *self, const ofoRecurrentRun *run )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( self ), &iter, -1 );
	set_row_by_iter( self, run, &iter );
}

static void
set_row_by_iter( ofaRecurrentRunStore *self, const ofoRecurrentRun *run, GtkTreeIter *iter )
{
	ofaRecurrentRunStorePrivate *priv;
	ofoRecurrentModel *model;
	gchar *sdate, *snum, *status, *samount1, *samount2, *samount3;
	const gchar *cmnemo, *cstatus, *cstr;
	ofxAmount amount;
	ofxCounter numseq;

	priv = ofa_recurrent_run_store_get_instance_private( self );

	cmnemo = ofo_recurrent_run_get_mnemo( run );

	model = ofo_recurrent_model_get_by_mnemo( priv->getter, cmnemo );
	g_return_if_fail( model && OFO_IS_RECURRENT_MODEL( model ));

	sdate = my_date_to_str( ofo_recurrent_run_get_date( run ), ofa_prefs_date_display( priv->getter ));

	cstatus = ofo_recurrent_run_get_status( run );
	status = ofo_recurrent_run_get_status_label( cstatus );

	numseq = ofo_recurrent_run_get_numseq( run );
	snum = ofa_counter_to_str( numseq, priv->getter );

	cstr = ofo_recurrent_model_get_def_amount1( model );
	if( my_strlen( cstr )){
		amount = ofo_recurrent_run_get_amount1( run );
		samount1 = ofa_amount_to_str( amount, NULL, priv->getter );
	} else {
		samount1 = g_strdup( "" );
	}

	cstr = ofo_recurrent_model_get_def_amount2( model );
	if( my_strlen( cstr )){
		amount = ofo_recurrent_run_get_amount2( run );
		samount2 = ofa_amount_to_str( amount, NULL, priv->getter );
	} else {
		samount2 = g_strdup( "" );
	}

	cstr = ofo_recurrent_model_get_def_amount3( model );
	if( my_strlen( cstr )){
		amount = ofo_recurrent_run_get_amount3( run );
		samount3 = ofa_amount_to_str( amount, NULL, priv->getter );
	} else {
		samount3 = g_strdup( "" );
	}

	gtk_list_store_set(
			GTK_LIST_STORE( self ),
			iter,
			REC_RUN_COL_MNEMO,      cmnemo,
			REC_RUN_COL_NUMSEQ,     snum,
			REC_RUN_COL_NUMSEQ_INT, numseq,
			REC_RUN_COL_LABEL,      ofo_recurrent_model_get_label( model ),
			REC_RUN_COL_DATE,       sdate,
			REC_RUN_COL_STATUS,     status,
			REC_RUN_COL_AMOUNT1,    samount1,
			REC_RUN_COL_AMOUNT2,    samount2,
			REC_RUN_COL_AMOUNT3,    samount3,
			REC_RUN_COL_OBJECT,     run,
			REC_RUN_COL_MODEL,      model,
			-1 );

	g_free( sdate );
	g_free( snum );
	g_free( status );
	g_free( samount1 );
	g_free( samount2 );
	g_free( samount3 );
}

static gboolean
find_row_by_object( ofaRecurrentRunStore *self, ofoRecurrentRun *run, GtkTreeIter *iter )
{
	ofoRecurrentRun *row_object;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, REC_RUN_COL_OBJECT, &row_object, -1 );
			cmp = ofo_recurrent_run_compare( row_object, run );
			g_object_unref( row_object );
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

/**
 * ofa_recurrent_run_store_get_iter:
 * @store: this #ofaRecurrentRunSgtore instance.
 * @run: the searched #ofoRecurrentRun object.
 * @iter: [out]: a #GtkTreeIter to be set if found.
 *
 * Returns: %TRUE if the @run object is found in @store, and @iter is
 * set accordingly.
 */
gboolean
ofa_recurrent_run_store_get_iter( ofaRecurrentRunStore *store, ofoRecurrentRun *run, GtkTreeIter *iter )
{
	ofaRecurrentRunStorePrivate *priv;

	g_return_val_if_fail( store && OFA_IS_RECURRENT_RUN_STORE( store ), FALSE );
	g_return_val_if_fail( run && OFO_IS_RECURRENT_RUN( run ), FALSE );

	priv = ofa_recurrent_run_store_get_instance_private( store );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( find_row_by_object( store, run, iter ));
}

/*
 * connect to the hub signaling system
 */
static void
hub_connect_to_signaling_system( ofaRecurrentRunStore *self )
{
	ofaRecurrentRunStorePrivate *priv;
	gulong handler;

	priv = ofa_recurrent_run_store_get_instance_private( self );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_NEW, G_CALLBACK( hub_on_new_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_UPDATED, G_CALLBACK( hub_on_updated_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_DELETED, G_CALLBACK( hub_on_deleted_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_RELOAD, G_CALLBACK( hub_on_reload_dataset ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
hub_on_new_object( ofaHub *hub, ofoBase *object, ofaRecurrentRunStore *self )
{
	static const gchar *thisfn = "ofa_recurrent_run_store_hub_on_new_object";
	ofaRecurrentRunStorePrivate *priv;

	g_debug( "%s: hub=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	priv = ofa_recurrent_run_store_get_instance_private( self );

	if( OFO_IS_RECURRENT_RUN( object ) && priv->mode == REC_MODE_FROM_DBMS ){
		insert_row( self, OFO_RECURRENT_RUN( object ));
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaRecurrentRunStore *self )
{
	static const gchar *thisfn = "ofa_recurrent_run_store_hub_on_updated_object";
	const gchar *new_mnemo;
	GtkTreeIter iter;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_RECURRENT_MODEL( object )){
		new_mnemo = ofo_recurrent_model_get_mnemo( OFO_RECURRENT_MODEL( object ));
		if( my_strlen( prev_id ) && my_collate( prev_id, new_mnemo )){
			hub_on_updated_recurrent_model_mnemo( self, prev_id, new_mnemo );
		}
	} else if( OFO_IS_RECURRENT_RUN( object )){
		if( find_row_by_object( self, OFO_RECURRENT_RUN( object ), &iter )){
			set_row_by_iter( self, OFO_RECURRENT_RUN( object ), &iter );
		}
	}
}

/*
 * Update all operations to the new model mnemo
 * Which means updating the store + updating the corresponding object
 * Iter on all rows because several operations may share same model
 */
static void
hub_on_updated_recurrent_model_mnemo( ofaRecurrentRunStore *self, const gchar *prev_mnemo, const gchar *new_mnemo )
{
	GtkTreeIter iter;
	ofoRecurrentRun *run;
	gchar *stored_mnemo;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), &iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), &iter,
					REC_RUN_COL_MNEMO, &stored_mnemo, REC_RUN_COL_OBJECT, &run, -1 );

			g_return_if_fail( run && OFO_IS_RECURRENT_RUN( run ));
			g_object_unref( run );
			cmp = my_collate( stored_mnemo, prev_mnemo );
			g_free( stored_mnemo );

			if( cmp == 0 ){
				ofo_recurrent_run_set_mnemo( run, new_mnemo );
				gtk_list_store_set( GTK_LIST_STORE( self ), &iter, REC_RUN_COL_MNEMO, new_mnemo, -1 );
			}

			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), &iter )){
				break;
			}
		}
	}
}

/*
 * SIGNAL_HUB_DELETED signal handler
 *
 * ofaRecurrentRun is not expected to be deletable after having been
 * recorded in the DBMS.
 */
static void
hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaRecurrentRunStore *self )
{
	static const gchar *thisfn = "ofa_recurrent_run_store_hub_on_deleted_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
hub_on_reload_dataset( ofaHub *hub, GType type, ofaRecurrentRunStore *self )
{
	static const gchar *thisfn = "ofa_recurrent_run_store_hub_on_reload_dataset";
	ofaRecurrentRunStorePrivate *priv;

	g_debug( "%s: hub=%p, type=%lu, self=%p",
			thisfn, ( void * ) hub, type, ( void * ) self );

	priv = ofa_recurrent_run_store_get_instance_private( self );

	if( type == OFO_TYPE_RECURRENT_RUN && priv->mode == REC_MODE_FROM_DBMS ){
		gtk_list_store_clear( GTK_LIST_STORE( self ));
		load_dataset( self );
	}
}
