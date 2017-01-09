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

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-preferences.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"

#include "core/ofa-ledger-store.h"

/* private instance data
 */
typedef struct {
	gboolean  dispose_has_run;

	/* runtime
	 */
	ofaHub   *hub;
	GList    *hub_handlers;
}
	ofaLedgerStorePrivate;

static GType st_col_types[LEDGER_N_COLUMNS] = {
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* mnemo, label, last_entry */
		G_TYPE_STRING, G_TYPE_STRING, 0,				/* last_close, notes, notes_png */
		G_TYPE_STRING, G_TYPE_STRING,					/* upd_user, upd_stamp */
		G_TYPE_OBJECT									/* the #ofoLedger itself */
};

static const gchar *st_resource_filler_png  = "/org/trychlos/openbook/core/filler.png";
static const gchar *st_resource_notes_png   = "/org/trychlos/openbook/core/notes.png";

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaLedgerStore *self );
static void     load_dataset( ofaLedgerStore *self );
static void     insert_row( ofaLedgerStore *self, const ofoLedger *ledger );
static void     set_row_by_iter( ofaLedgerStore *self, const ofoLedger *ledger, GtkTreeIter *iter );
static void     hub_connect_to_signaling_system( ofaLedgerStore *self );
static void     hub_on_new_object( ofaHub *hub, ofoBase *object, ofaLedgerStore *self );
static void     hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaLedgerStore *self );
static gboolean find_ledger_by_mnemo( ofaLedgerStore *self, const gchar *mnemo, GtkTreeIter *iter );
static void     hub_on_updated_currency( ofaLedgerStore *self, const gchar *prev_id, const gchar *new_id );
static void     hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaLedgerStore *self );
static void     hub_on_reload_dataset( ofaHub *hub, GType type, ofaLedgerStore *self );

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
		ofa_hub_disconnect_handlers( priv->hub, &priv->hub_handlers );
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
	priv->hub_handlers = NULL;
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
 * Instanciates a new #ofaLedgerStore and attached it to the @hub
 * if not already done. Else get the already allocated #ofaLedgerStore
 * from the #ofoDossier attached to the @hub.
 *
 * A weak notify reference is put on this same @hub, so that the
 * instance will be unreffed when the @hub will be destroyed.
 *
 * Note that the #myICollector associated to the @hub maintains its own
 * reference to the #ofaLedgerStore object, reference which will be
 * freed on @hub finalization.
 *
 * Returns: a new reference to the #ofaLedgerStore object.
 */
ofaLedgerStore *
ofa_ledger_store_new( ofaHub *hub )
{
	ofaLedgerStore *store;
	ofaLedgerStorePrivate *priv;
	myICollector *collector;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	collector = ofa_hub_get_collector( hub );
	store = ( ofaLedgerStore * ) my_icollector_single_get_object( collector, OFA_TYPE_LEDGER_STORE );

	if( store ){
		g_return_val_if_fail( OFA_IS_LEDGER_STORE( store ), NULL );

	} else {
		store = g_object_new( OFA_TYPE_LEDGER_STORE, NULL );

		priv = ofa_ledger_store_get_instance_private( store );
		priv->hub = hub;

		st_col_types[LEDGER_COL_NOTES_PNG] = GDK_TYPE_PIXBUF;
		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), LEDGER_N_COLUMNS, st_col_types );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		my_icollector_single_set_object( collector, store );
		hub_connect_to_signaling_system( store );
		load_dataset( store );
	}

	return( g_object_ref( store ));
}

/*
 * sorting the store per ledger code
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaLedgerStore *self )
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
load_dataset( ofaLedgerStore *self )
{
	ofaLedgerStorePrivate *priv;
	const GList *dataset, *it;
	ofoLedger *ledger;

	priv = ofa_ledger_store_get_instance_private( self );

	dataset = ofo_ledger_get_dataset( priv->hub );

	for( it=dataset ; it ; it=it->next ){
		ledger = OFO_LEDGER( it->data );
		insert_row( self, ledger );
	}
}

static void
insert_row( ofaLedgerStore *self, const ofoLedger *ledger )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( self ), &iter, -1 );
	set_row_by_iter( self, ledger, &iter );
}

static void
set_row_by_iter( ofaLedgerStore *self, const ofoLedger *ledger, GtkTreeIter *iter )
{
	ofaLedgerStorePrivate *priv;
	static const gchar *thisfn = "ofa_ledger_store_set_row";
	gchar *sdentry, *sdclose, *stamp;
	const gchar *notes;
	const GDate *dclose;
	GDate dentry;
	GError *error;
	GdkPixbuf *notes_png;

	priv = ofa_ledger_store_get_instance_private( self );

	ofo_ledger_get_last_entry( ledger, &dentry );
	sdentry = my_date_to_str( &dentry, ofa_prefs_date_display( priv->hub ));
	dclose = ofo_ledger_get_last_close( ledger );
	sdclose = my_date_to_str( dclose, ofa_prefs_date_display( priv->hub ));
	stamp  = my_utils_stamp_to_str( ofo_ledger_get_upd_stamp( ledger ), MY_STAMP_DMYYHM );

	notes = ofo_ledger_get_notes( ledger );
	error = NULL;
	notes_png = gdk_pixbuf_new_from_resource( my_strlen( notes ) ? st_resource_notes_png : st_resource_filler_png, &error );
	if( error ){
		g_warning( "%s: gdk_pixbuf_new_from_resource: %s", thisfn, error->message );
		g_error_free( error );
	}

	gtk_list_store_set(
			GTK_LIST_STORE( self ),
			iter,
			LEDGER_COL_MNEMO,      ofo_ledger_get_mnemo( ledger ),
			LEDGER_COL_LABEL,      ofo_ledger_get_label( ledger ),
			LEDGER_COL_LAST_ENTRY, sdentry,
			LEDGER_COL_LAST_CLOSE, sdclose,
			LEDGER_COL_NOTES,      notes,
			LEDGER_COL_NOTES_PNG,  notes_png,
			LEDGER_COL_UPD_USER,   ofo_ledger_get_upd_user( ledger ),
			LEDGER_COL_UPD_STAMP,  stamp,
			LEDGER_COL_OBJECT,     ledger,
			-1 );

	g_object_unref( notes_png );
	g_free( stamp );
	g_free( sdclose );
	g_free( sdentry );
}

/*
 * connect to the dossier signaling system
 */
static void
hub_connect_to_signaling_system( ofaLedgerStore *self )
{
	ofaLedgerStorePrivate *priv;
	gulong handler;

	priv = ofa_ledger_store_get_instance_private( self );

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
hub_on_new_object( ofaHub *hub, ofoBase *object, ofaLedgerStore *self )
{
	static const gchar *thisfn = "ofa_ledger_store_hub_on_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_LEDGER( object )){
		insert_row( self, OFO_LEDGER( object ));
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaLedgerStore *self )
{
	static const gchar *thisfn = "ofa_ledger_store_hub_on_updated_object";
	GtkTreeIter iter;
	const gchar *mnemo, *new_id;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_LEDGER( object )){
		new_id = ofo_ledger_get_mnemo( OFO_LEDGER( object ));
		mnemo = prev_id ? prev_id : new_id;
		if( find_ledger_by_mnemo( self, mnemo, &iter )){
			set_row_by_iter( self, OFO_LEDGER( object ), &iter);
		}

	} else if( OFO_IS_CURRENCY( object )){
		new_id = ofo_currency_get_code( OFO_CURRENCY( object ));
		if( prev_id && g_utf8_collate( prev_id, new_id )){
			hub_on_updated_currency( self, prev_id, new_id );
		}
	}
}

static gboolean
find_ledger_by_mnemo( ofaLedgerStore *self, const gchar *mnemo, GtkTreeIter *iter )
{
	gchar *str;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, LEDGER_COL_MNEMO, &str, -1 );
			cmp = g_utf8_collate( str, mnemo );
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

static void
hub_on_updated_currency( ofaLedgerStore *self, const gchar *prev_id, const gchar *new_id )
{
	GtkTreeIter iter;
	ofoLedger *ledger;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), &iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), &iter, LEDGER_COL_OBJECT, &ledger, -1 );
			g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));
			g_object_unref( ledger );

			ofo_ledger_update_currency( ledger, prev_id, new_id );

			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), &iter )){
				break;
			}
		}
	}
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaLedgerStore *self )
{
	static const gchar *thisfn = "ofa_ledger_store_hub_on_deleted_object";
	GtkTreeIter iter;

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_LEDGER( object )){
		if( find_ledger_by_mnemo( self,
				ofo_ledger_get_mnemo( OFO_LEDGER( object )), &iter )){

			gtk_list_store_remove( GTK_LIST_STORE( self ), &iter );
		}
	}
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
hub_on_reload_dataset( ofaHub *hub, GType type, ofaLedgerStore *self )
{
	static const gchar *thisfn = "ofa_ledger_store_hub_on_reload_dataset";

	g_debug( "%s: hub=%p, type=%lu, self=%p",
			thisfn, ( void * ) hub, type, ( void * ) self );

	if( type == OFO_TYPE_LEDGER ){
		gtk_list_store_clear( GTK_LIST_STORE( self ));
		load_dataset( self );
	}
}
