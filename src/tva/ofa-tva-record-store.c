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

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-isingle-keeper.h"
#include "api/ofa-preferences.h"

#include "tva/ofa-tva-record-store.h"
#include "tva/ofo-tva-record.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/*
	 */
}
	ofaTVARecordStorePrivate;

static GType st_col_types[TVA_RECORD_N_COLUMNS] = {
		G_TYPE_STRING, 					/* mnemo */
		G_TYPE_STRING,					/* label */
		G_TYPE_STRING, 					/* is_validated */
		G_TYPE_STRING,					/* begin */
		G_TYPE_STRING,					/* end */
		G_TYPE_OBJECT					/* the #ofoTVARecord itself */
};

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaTVARecordStore *store );
static void     load_dataset( ofaTVARecordStore *store, ofaHub *hub );
static void     insert_row( ofaTVARecordStore *store, ofaHub *hub, const ofoTVARecord *record );
static void     set_row( ofaTVARecordStore *store, ofaHub *hub, const ofoTVARecord *record, GtkTreeIter *iter );
static void     on_hub_new_object( ofaHub *hub, ofoBase *object, ofaTVARecordStore *store );
static void     on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaTVARecordStore *store );
static gboolean find_record_by_key( ofaTVARecordStore *store, const gchar *mnemo, const GDate *end, GtkTreeIter *iter );
static gboolean find_record_by_ptr( ofaTVARecordStore *store, const ofoTVARecord *record, GtkTreeIter *iter );
static void     on_hub_deleted_object( ofaHub *hub, ofoBase *object, ofaTVARecordStore *store );
static void     on_hub_reload_dataset( ofaHub *hub, GType type, ofaTVARecordStore *store );

G_DEFINE_TYPE_EXTENDED( ofaTVARecordStore, ofa_tva_record_store, OFA_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaTVARecordStore ))

static void
tva_record_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_record_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_RECORD_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_record_store_parent_class )->finalize( instance );
}

static void
tva_record_store_dispose( GObject *instance )
{
	ofaTVARecordStorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_TVA_RECORD_STORE( instance ));

	priv = ofa_tva_record_store_get_instance_private( OFA_TVA_RECORD_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_record_store_parent_class )->dispose( instance );
}

static void
ofa_tva_record_store_init( ofaTVARecordStore *self )
{
	static const gchar *thisfn = "ofa_tva_record_store_init";
	ofaTVARecordStorePrivate *priv;

	g_return_if_fail( OFA_IS_TVA_RECORD_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_tva_record_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_tva_record_store_class_init( ofaTVARecordStoreClass *klass )
{
	static const gchar *thisfn = "ofa_tva_record_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_record_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_record_store_finalize;
}

/**
 * ofa_tva_record_store_new:
 * @dossier: the currently opened #ofoDossier.
 *
 * Instanciates a new #ofaTVARecordStore and attached it to the @dossier
 * if not already done. Else get the already allocated #ofaTVARecordStore
 * from the @dossier.
 *
 * A weak notify reference is put on this same @dossier, so that the
 * instance will be unreffed when the @dossier will be destroyed.
 */
ofaTVARecordStore *
ofa_tva_record_store_new( ofaHub *hub )
{
	ofaTVARecordStore *store;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	store = ( ofaTVARecordStore * ) ofa_isingle_keeper_get_object( OFA_ISINGLE_KEEPER( hub ), OFA_TYPE_TVA_RECORD_STORE );

	if( store ){
		g_return_val_if_fail( OFA_IS_TVA_RECORD_STORE( store ), NULL );

	} else {
		store = g_object_new(
						OFA_TYPE_TVA_RECORD_STORE,
						OFA_PROP_HUB,              hub,
						NULL );

		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), TVA_RECORD_N_COLUMNS, st_col_types );

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
		g_signal_connect( hub, SIGNAL_HUB_NEW, G_CALLBACK( on_hub_new_object ), store );
		g_signal_connect( hub, SIGNAL_HUB_UPDATED, G_CALLBACK( on_hub_updated_object ), store );
		g_signal_connect( hub, SIGNAL_HUB_DELETED, G_CALLBACK( on_hub_deleted_object ), store );
		g_signal_connect( hub, SIGNAL_HUB_RELOAD, G_CALLBACK( on_hub_reload_dataset ), store );
	}

	return( store );
}

/*
 * sorting the store per record code
 * we are sorting by mnemo asc, end date desc
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaTVARecordStore *store )
{
	ofoTVARecord *aobj, *bobj;
	gchar *aend, *bend;
	gint cmp;

	gtk_tree_model_get( tmodel, a, TVA_RECORD_COL_OBJECT, &aobj, -1 );
	g_object_unref( aobj );

	gtk_tree_model_get( tmodel, b, TVA_RECORD_COL_OBJECT, &bobj, -1 );
	g_object_unref( bobj );

	cmp = g_utf8_collate( ofo_tva_record_get_mnemo( aobj ), ofo_tva_record_get_mnemo( bobj ));

	if( cmp == 0 ){
		aend = my_date_to_str( ofo_tva_record_get_end( aobj ), MY_DATE_SQL );
		bend = my_date_to_str( ofo_tva_record_get_end( bobj ), MY_DATE_SQL );
		cmp = -1 * g_utf8_collate( aend, bend );
		g_free( aend );
		g_free( bend );
	}

	return( cmp );
}

static void
load_dataset( ofaTVARecordStore *store, ofaHub *hub )
{
	const GList *dataset, *it;
	ofoTVARecord *record;

	dataset = ofo_tva_record_get_dataset( hub );

	for( it=dataset ; it ; it=it->next ){
		record = OFO_TVA_RECORD( it->data );
		insert_row( store, hub, record );
	}
}

static void
insert_row( ofaTVARecordStore *store, ofaHub *hub, const ofoTVARecord *record )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( store ), &iter, -1 );
	set_row( store, hub, record, &iter );
}

static void
set_row( ofaTVARecordStore *store, ofaHub *hub, const ofoTVARecord *record, GtkTreeIter *iter )
{
	const gchar *cvalidated;
	gchar *sbegin, *send;

	cvalidated = ofo_tva_record_get_is_validated( record ) ? _( "Yes" ) : "";
	sbegin = my_date_to_str( ofo_tva_record_get_begin( record ), ofa_prefs_date_display());
	send = my_date_to_str( ofo_tva_record_get_end( record ), ofa_prefs_date_display());

	gtk_list_store_set(
			GTK_LIST_STORE( store ),
			iter,
			TVA_RECORD_COL_MNEMO,        ofo_tva_record_get_mnemo( record ),
			TVA_RECORD_COL_LABEL,        ofo_tva_record_get_label( record ),
			TVA_RECORD_COL_IS_VALIDATED, cvalidated,
			TVA_RECORD_COL_BEGIN,        sbegin,
			TVA_RECORD_COL_END,          send,
			TVA_RECORD_COL_OBJECT,       record,
			-1 );

	g_free( sbegin );
	g_free( send );
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
on_hub_new_object( ofaHub *hub, ofoBase *object, ofaTVARecordStore *store )
{
	static const gchar *thisfn = "ofa_tva_record_store_on_hub_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) store );

	if( OFO_IS_TVA_RECORD( object )){
		insert_row( store, hub, OFO_TVA_RECORD( object ));
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaTVARecordStore *store )
{
	static const gchar *thisfn = "ofa_tva_record_store_on_hub_updated_object";
	GtkTreeIter iter;
	const gchar *mnemo;
	const GDate *dend;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, store=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) store );

	if( OFO_IS_TVA_RECORD( object )){
		mnemo = ofo_tva_record_get_mnemo( OFO_TVA_RECORD( object ));
		dend = ofo_tva_record_get_end( OFO_TVA_RECORD( object ));
		if( find_record_by_key( store, mnemo, dend, &iter )){
			set_row( store, hub, OFO_TVA_RECORD( object ), &iter);
		}
	}
}

static gboolean
find_record_by_key( ofaTVARecordStore *store, const gchar *mnemo, const GDate *end, GtkTreeIter *iter )
{
	ofoTVARecord *iter_obj;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( store ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( store ), iter, TVA_RECORD_COL_OBJECT, &iter_obj, -1 );
			g_object_unref( iter_obj );
			cmp = ofo_tva_record_compare_by_key( iter_obj, mnemo, end );
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

static gboolean
find_record_by_ptr( ofaTVARecordStore *store, const ofoTVARecord *record, GtkTreeIter *iter )
{
	ofoTVARecord *iter_obj;
	const gchar *mnemo;
	const GDate *dend;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( store ), iter )){
		mnemo = ofo_tva_record_get_mnemo( record );
		dend = ofo_tva_record_get_end( record );
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( store ), iter, TVA_RECORD_COL_OBJECT, &iter_obj, -1 );
			g_object_unref( iter_obj );
			cmp = ofo_tva_record_compare_by_key( iter_obj, mnemo, dend );
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

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
on_hub_deleted_object( ofaHub *hub, ofoBase *object, ofaTVARecordStore *store )
{
	static const gchar *thisfn = "ofa_tva_record_store_on_hub_deleted_object";
	GtkTreeIter iter;

	g_debug( "%s: hub=%p, object=%p (%s), store=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) store );

	if( OFO_IS_TVA_RECORD( object )){
		if( find_record_by_ptr( store, OFO_TVA_RECORD( object ), &iter )){
			gtk_list_store_remove( GTK_LIST_STORE( store ), &iter );
		}
	}
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
on_hub_reload_dataset( ofaHub *hub, GType type, ofaTVARecordStore *store )
{
	static const gchar *thisfn = "ofa_tva_record_store_on_hub_reload_dataset";

	g_debug( "%s: hub=%p, type=%lu, store=%p",
			thisfn, ( void * ) hub, type, ( void * ) store );

	if( type == OFO_TYPE_TVA_RECORD ){
		gtk_list_store_clear( GTK_LIST_STORE( store ));
		load_dataset( store, hub );
	}
}
