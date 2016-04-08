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

#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-isingle-keeper.h"
#include "api/ofa-periodicity.h"

#include "recurrent/ofa-recurrent-model-store.h"
#include "recurrent/ofo-recurrent-model.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/*
	 */
}
	ofaRecurrentModelStorePrivate;

static GType st_col_types[REC_N_COLUMNS] = {
		G_TYPE_STRING, G_TYPE_STRING,					/* mnemo, label */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* ope_template, periodicity, detail */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* notes, upd_user, upd_stamp */
		G_TYPE_OBJECT									/* the #ofoRecurrentModel itself */
};

/* signals defined here
 */
enum {
	INSERTED = 0,
	REMOVED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaRecurrentModelStore *self );
static void     load_dataset( ofaRecurrentModelStore *self, ofaHub *hub );
static void     insert_row( ofaRecurrentModelStore *self, ofaHub *hub, const ofoRecurrentModel *model );
static void     set_row( ofaRecurrentModelStore *self, ofaHub *hub, const ofoRecurrentModel *model, GtkTreeIter *iter );
static gboolean model_find_by_mnemo( ofaRecurrentModelStore *self, const gchar *code, GtkTreeIter *iter );
static void     hub_on_new_object( ofaHub *hub, ofoBase *object, ofaRecurrentModelStore *self );
static void     hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaRecurrentModelStore *self );
static void     hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaRecurrentModelStore *self );
static void     hub_on_reload_dataset( ofaHub *hub, GType type, ofaRecurrentModelStore *self );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentModelStore, ofa_recurrent_model_store, OFA_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaRecurrentModelStore ))

static void
recurrent_model_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recurrent_model_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECURRENT_MODEL_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_model_store_parent_class )->finalize( instance );
}

static void
recurrent_model_store_dispose( GObject *instance )
{
	ofaRecurrentModelStorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_RECURRENT_MODEL_STORE( instance ));

	priv = ofa_recurrent_model_store_get_instance_private( OFA_RECURRENT_MODEL_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_model_store_parent_class )->dispose( instance );
}

static void
ofa_recurrent_model_store_init( ofaRecurrentModelStore *self )
{
	static const gchar *thisfn = "ofa_recurrent_model_store_init";
	ofaRecurrentModelStorePrivate *priv;

	g_return_if_fail( OFA_IS_RECURRENT_MODEL_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_recurrent_model_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_recurrent_model_store_class_init( ofaRecurrentModelStoreClass *klass )
{
	static const gchar *thisfn = "ofa_recurrent_model_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_model_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_model_store_finalize;

	/**
	 * ofaRecurrentModelStore::ofa-inserted:
	 *
	 * This signal is sent on the #ofaRecurrentModelStore when a new
	 * row is inserted.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaRecurrentModelStore *store,
	 * 						gpointer              user_data );
	 */
	st_signals[ INSERTED ] = g_signal_new_class_handler(
				"ofa-inserted",
				OFA_TYPE_RECURRENT_MODEL_STORE,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0,
				G_TYPE_NONE );

	/**
	 * ofaRecurrentModelStore::ofa-removed:
	 *
	 * This signal is sent on the #ofaRecurrentModelStore when a row is
	 * removed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaRecurrentModelStore *store,
	 * 						gpointer              user_data );
	 */
	st_signals[ REMOVED ] = g_signal_new_class_handler(
				"ofa-removed",
				OFA_TYPE_RECURRENT_MODEL_STORE,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0,
				G_TYPE_NONE );
}

/**
 * ofa_recurrent_model_store_new:
 * @hub: the current #ofaHub object.
 *
 * Instanciates a new #ofaRecurrentModelStore and attached it to the @hub
 * if not already done. Else get the already allocated #ofaRecurrentModelStore
 * from the @dossier.
 *
 * A weak notify reference is put on this same @dossier, so that the
 * instance will be unreffed when the @dossier will be destroyed.
 */
ofaRecurrentModelStore *
ofa_recurrent_model_store_new( ofaHub *hub )
{
	ofaRecurrentModelStore *store;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	store = ( ofaRecurrentModelStore * ) ofa_isingle_keeper_get_object( OFA_ISINGLE_KEEPER( hub ), OFA_TYPE_RECURRENT_MODEL_STORE );

	if( store ){
		g_return_val_if_fail( OFA_IS_RECURRENT_MODEL_STORE( store ), NULL );

	} else {
		store = g_object_new(
						OFA_TYPE_RECURRENT_MODEL_STORE,
						OFA_PROP_HUB,            hub,
						NULL );

		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), REC_N_COLUMNS, st_col_types );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		ofa_isingle_keeper_set_object( OFA_ISINGLE_KEEPER( hub ), store );

		load_dataset( store, hub );

		/* connect to the hub signaling system
		 * there is no need to keep trace of the signal handlers, as
		 * this store will only be finalized after the hub finalization */
		g_signal_connect( hub, SIGNAL_HUB_NEW, G_CALLBACK( hub_on_new_object ), store );
		g_signal_connect( hub, SIGNAL_HUB_UPDATED, G_CALLBACK( hub_on_updated_object ), store );
		g_signal_connect( hub, SIGNAL_HUB_DELETED, G_CALLBACK( hub_on_deleted_object ), store );
		g_signal_connect( hub, SIGNAL_HUB_RELOAD, G_CALLBACK( hub_on_reload_dataset ), store );
	}

	return( store );
}

/*
 * sorting the store per model code
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaRecurrentModelStore *self )
{
	gchar *amnemo, *bmnemo;
	gint cmp;

	gtk_tree_model_get( tmodel, a, REC_MODEL_COL_MNEMO, &amnemo, -1 );
	gtk_tree_model_get( tmodel, b, REC_MODEL_COL_MNEMO, &bmnemo, -1 );

	cmp = g_utf8_collate( amnemo, bmnemo );

	g_free( amnemo );
	g_free( bmnemo );

	return( cmp );
}

static void
load_dataset( ofaRecurrentModelStore *self, ofaHub *hub )
{
	const GList *dataset, *it;
	ofoRecurrentModel *model;

	dataset = ofo_recurrent_model_get_dataset( hub );

	for( it=dataset ; it ; it=it->next ){
		model = OFO_RECURRENT_MODEL( it->data );
		insert_row( self, hub, model );
	}
}

static void
insert_row( ofaRecurrentModelStore *self, ofaHub *hub, const ofoRecurrentModel *model )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( self ), &iter, -1 );
	set_row( self, hub, model, &iter );

	g_signal_emit_by_name( self, "ofa-inserted" );
}

static void
set_row( ofaRecurrentModelStore *self, ofaHub *hub, const ofoRecurrentModel *model, GtkTreeIter *iter )
{
	gchar *stamp;
	const gchar *csper, *csdet;
	const gchar *periodicity;

	stamp  = my_utils_stamp_to_str( ofo_recurrent_model_get_upd_stamp( model ), MY_STAMP_DMYYHM );
	periodicity = ofo_recurrent_model_get_periodicity( model );
	csper = ofa_periodicity_get_label( periodicity );
	csdet = ofa_periodicity_get_detail_label( periodicity, ofo_recurrent_model_get_periodicity_detail( model ));

	gtk_list_store_set(
			GTK_LIST_STORE( self ),
			iter,
			REC_MODEL_COL_MNEMO,              ofo_recurrent_model_get_mnemo( model ),
			REC_MODEL_COL_LABEL,              ofo_recurrent_model_get_label( model ),
			REC_MODEL_COL_OPE_TEMPLATE,       ofo_recurrent_model_get_ope_template( model ),
			REC_MODEL_COL_PERIODICITY,        csper ? csper : "",
			REC_MODEL_COL_PERIODICITY_DETAIL, csdet ? csdet : "",
			REC_MODEL_COL_UPD_USER,           ofo_recurrent_model_get_upd_user( model ),
			REC_MODEL_COL_UPD_STAMP,          stamp,
			REC_MODEL_COL_OBJECT,             model,
			-1 );

	g_free( stamp );
}

static gboolean
model_find_by_mnemo( ofaRecurrentModelStore *self, const gchar *code, GtkTreeIter *iter )
{
	gchar *str;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, REC_MODEL_COL_MNEMO, &str, -1 );
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
 * SIGNAL_HUB_NEW signal handler
 */
static void
hub_on_new_object( ofaHub *hub, ofoBase *object, ofaRecurrentModelStore *self )
{
	static const gchar *thisfn = "ofa_recurrent_model_store_hub_on_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_RECURRENT_MODEL( object )){
		insert_row( self, hub, OFO_RECURRENT_MODEL( object ));
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaRecurrentModelStore *self )
{
	static const gchar *thisfn = "ofa_recurrent_model_store_hub_on_updated_object";
	GtkTreeIter iter;
	const gchar *code, *new_code;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_RECURRENT_MODEL( object )){
		new_code = ofo_recurrent_model_get_mnemo( OFO_RECURRENT_MODEL( object ));
		code = prev_id ? prev_id : new_code;
		if( model_find_by_mnemo( self, code, &iter )){
			set_row( self, hub, OFO_RECURRENT_MODEL( object ), &iter);
		}
	}
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaRecurrentModelStore *self )
{
	static const gchar *thisfn = "ofa_recurrent_model_store_hub_on_deleted_object";
	GtkTreeIter iter;

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_RECURRENT_MODEL( object )){
		if( model_find_by_mnemo( self,
				ofo_recurrent_model_get_mnemo( OFO_RECURRENT_MODEL( object )), &iter )){

			gtk_list_store_remove( GTK_LIST_STORE( self ), &iter );
			g_signal_emit_by_name( self, "ofa-removed" );
		}
	}
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
hub_on_reload_dataset( ofaHub *hub, GType type, ofaRecurrentModelStore *self )
{
	static const gchar *thisfn = "ofa_recurrent_model_store_hub_on_reload_dataset";

	g_debug( "%s: hub=%p, type=%lu, self=%p",
			thisfn, ( void * ) hub, type, ( void * ) self );

	if( type == OFO_TYPE_RECURRENT_MODEL ){
		gtk_list_store_clear( GTK_LIST_STORE( self ));
		load_dataset( self, hub );
	}
}
