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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-isingle-keeper.h"
#include "api/ofa-preferences.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"

#include "core/ofa-ledger-store.h"

/* private instance data
 */
struct _ofaLedgerStorePrivate {
	gboolean    dispose_has_run;

	/*
	 */
};

static GType st_col_types[LEDGER_N_COLUMNS] = {
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* mnemo, label, last_entry */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* las_close, notes, upd_user */
		G_TYPE_STRING,									/* upd_stamp */
		G_TYPE_OBJECT									/* the #ofoLedger itself */
};

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaLedgerStore *store );
static void     load_dataset( ofaLedgerStore *store, ofaHub *hub );
static void     insert_row( ofaLedgerStore *store, ofaHub *hub, const ofoLedger *ledger );
static void     set_row( ofaLedgerStore *store, ofaHub *hub, const ofoLedger *ledger, GtkTreeIter *iter );
static void     setup_signaling_connect( ofaLedgerStore *store, ofaHub *hub );
static void     on_hub_new_object( ofaHub *hub, ofoBase *object, ofaLedgerStore *store );
static void     on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaLedgerStore *store );
static gboolean find_ledger_by_mnemo( ofaLedgerStore *store, const gchar *mnemo, GtkTreeIter *iter );
static void     on_hub_deleted_object( ofaHub *hub, ofoBase *object, ofaLedgerStore *store );
static void     on_hub_reload_dataset( ofaHub *hub, GType type, ofaLedgerStore *store );

G_DEFINE_TYPE_EXTENDED( ofaLedgerStore, ofa_ledger_store, OFA_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaLedgerStore ))

static void
ledger_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledger_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGER_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_store_parent_class )->finalize( instance );
}

static void
ledger_store_dispose( GObject *instance )
{
	ofaLedgerStorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_LEDGER_STORE( instance ));

	priv = ofa_ledger_store_get_instance_private( OFA_LEDGER_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_store_parent_class )->dispose( instance );
}

static void
ofa_ledger_store_init( ofaLedgerStore *self )
{
	static const gchar *thisfn = "ofa_ledger_store_init";
	ofaLedgerStorePrivate *priv;

	g_return_if_fail( self && OFA_IS_LEDGER_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_ledger_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_ledger_store_class_init( ofaLedgerStoreClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledger_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledger_store_finalize;
}

/**
 * ofa_ledger_store_new:
 * @hub: the current #ofaHub object.
 *
 * Instanciates a new #ofaLedgerStore and attached it to the @dossier
 * if not already done. Else get the already allocated #ofaLedgerStore
 * from the #ofoDossier attached to the @hub.
 *
 * A weak notify reference is put on this same @hub, so that the
 * instance will be unreffed when the @hub will be destroyed.
 */
ofaLedgerStore *
ofa_ledger_store_new( ofaHub *hub )
{
	ofaLedgerStore *store;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	store = ( ofaLedgerStore * ) ofa_isingle_keeper_get_object( OFA_ISINGLE_KEEPER( hub ), OFA_TYPE_LEDGER_STORE );

	if( store ){
		g_return_val_if_fail( OFA_IS_LEDGER_STORE( store ), NULL );

	} else {
		store = g_object_new(
						OFA_TYPE_LEDGER_STORE,
						OFA_PROP_HUB,          hub,
						NULL );

		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), LEDGER_N_COLUMNS, st_col_types );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		ofa_isingle_keeper_set_object( OFA_ISINGLE_KEEPER( hub ), store );

		load_dataset( store, hub );
		setup_signaling_connect( store, hub );
	}

	return( store );
}

/*
 * sorting the store per ledger code
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaLedgerStore *store )
{
	gchar *anumber, *bnumber;
	gint cmp;

	gtk_tree_model_get( tmodel, a, LEDGER_COL_MNEMO, &anumber, -1 );
	gtk_tree_model_get( tmodel, b, LEDGER_COL_MNEMO, &bnumber, -1 );

	cmp = g_utf8_collate( anumber, bnumber );

	g_free( anumber );
	g_free( bnumber );

	return( cmp );
}

static void
load_dataset( ofaLedgerStore *store, ofaHub *hub )
{
	const GList *dataset, *it;
	ofoLedger *ledger;

	dataset = ofo_ledger_get_dataset( hub );

	for( it=dataset ; it ; it=it->next ){
		ledger = OFO_LEDGER( it->data );
		insert_row( store, hub, ledger );
	}
}

static void
insert_row( ofaLedgerStore *store, ofaHub *hub, const ofoLedger *ledger )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( store ), &iter, -1 );
	set_row( store, hub, ledger, &iter );
}

static void
set_row( ofaLedgerStore *store, ofaHub *hub, const ofoLedger *ledger, GtkTreeIter *iter )
{
	gchar *sdentry, *sdclose, *stamp;
	const GDate *dclose;
	GDate dentry;

	ofo_ledger_get_last_entry( ledger, &dentry );
	sdentry = my_date_to_str( &dentry, ofa_prefs_date_display());
	dclose = ofo_ledger_get_last_close( ledger );
	sdclose = my_date_to_str( dclose, ofa_prefs_date_display());
	stamp  = my_utils_stamp_to_str( ofo_ledger_get_upd_stamp( ledger ), MY_STAMP_DMYYHM );

	gtk_list_store_set(
			GTK_LIST_STORE( store ),
			iter,
			LEDGER_COL_MNEMO,      ofo_ledger_get_mnemo( ledger ),
			LEDGER_COL_LABEL,      ofo_ledger_get_label( ledger ),
			LEDGER_COL_LAST_ENTRY, sdentry,
			LEDGER_COL_LAST_CLOSE, sdclose,
			LEDGER_COL_NOTES,      ofo_ledger_get_notes( ledger ),
			LEDGER_COL_UPD_USER,   ofo_ledger_get_upd_user( ledger ),
			LEDGER_COL_UPD_STAMP,  stamp,
			LEDGER_COL_OBJECT,     ledger,
			-1 );

	g_free( stamp );
	g_free( sdclose );
	g_free( sdentry );
}

/*
 * connect to the dossier signaling system
 * there is no need to keep trace of the signal handlers, as the lifetime
 * of this store is equal to those of the dossier
 */
static void
setup_signaling_connect( ofaLedgerStore *store, ofaHub *hub )
{
	g_signal_connect( hub, SIGNAL_HUB_NEW, G_CALLBACK( on_hub_new_object ), store );
	g_signal_connect( hub, SIGNAL_HUB_UPDATED, G_CALLBACK( on_hub_updated_object ), store );
	g_signal_connect( hub, SIGNAL_HUB_DELETED, G_CALLBACK( on_hub_deleted_object ), store );
	g_signal_connect( hub, SIGNAL_HUB_RELOAD, G_CALLBACK( on_hub_reload_dataset ), store );
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
on_hub_new_object( ofaHub *hub, ofoBase *object, ofaLedgerStore *store )
{
	static const gchar *thisfn = "ofa_ledger_store_on_hub_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) store );

	if( OFO_IS_LEDGER( object )){
		insert_row( store, hub, OFO_LEDGER( object ));
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaLedgerStore *store )
{
	static const gchar *thisfn = "ofa_ledger_store_on_hub_updated_object";
	GtkTreeIter iter;
	const gchar *mnemo, *new_mnemo;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, store=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) store );

	if( OFO_IS_LEDGER( object )){
		new_mnemo = ofo_ledger_get_mnemo( OFO_LEDGER( object ));
		mnemo = prev_id ? prev_id : new_mnemo;
		if( find_ledger_by_mnemo( store, mnemo, &iter )){
			set_row( store, hub, OFO_LEDGER( object ), &iter);
		}
	}
}

static gboolean
find_ledger_by_mnemo( ofaLedgerStore *store, const gchar *mnemo, GtkTreeIter *iter )
{
	gchar *str;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( store ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( store ), iter, LEDGER_COL_MNEMO, &str, -1 );
			cmp = g_utf8_collate( str, mnemo );
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

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
on_hub_deleted_object( ofaHub *hub, ofoBase *object, ofaLedgerStore *store )
{
	static const gchar *thisfn = "ofa_ledger_store_on_hub_deleted_object";
	GtkTreeIter iter;

	g_debug( "%s: hub=%p, object=%p (%s), store=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) store );

	if( OFO_IS_LEDGER( object )){
		if( find_ledger_by_mnemo( store,
				ofo_ledger_get_mnemo( OFO_LEDGER( object )), &iter )){

			gtk_list_store_remove( GTK_LIST_STORE( store ), &iter );
		}
	}
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
on_hub_reload_dataset( ofaHub *hub, GType type, ofaLedgerStore *store )
{
	static const gchar *thisfn = "ofa_ledger_store_on_hub_reload_dataset";

	g_debug( "%s: hub=%p, type=%lu, store=%p",
			thisfn, ( void * ) hub, type, ( void * ) store );

	if( type == OFO_TYPE_LEDGER ){
		gtk_list_store_clear( GTK_LIST_STORE( store ));
		load_dataset( store, hub );
	}
}
