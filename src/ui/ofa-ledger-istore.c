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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofo-ledger.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-ledger-istore.h"

/* data associated to each implementor object
 */
typedef struct {

	/* static data
	 * to be set at initialization time
	 */
	ofaLedgerColumns columns;
	ofoDossier      *dossier;

	/* runtime data
	 */
	GtkListStore    *store;
	GList           *handlers;
}
	sIStore;

/* columns ordering in the store
 */
enum {
	COL_MNEMO = 0,
	COL_LABEL,
	COL_LAST_ENTRY,
	COL_LAST_CLOSE,
	COL_NOTES,
	COL_UPD_USER,
	COL_UPD_STAMP,
	COL_OBJECT,
	N_COLUMNS
};

#define LEDGER_ISTORE_LAST_VERSION      1
#define LEDGER_ISTORE_DATA              "ofa-ledger-istore-data"

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static guint st_initializations         = 0;	/* interface initialization count */

static GType    register_type( void );
static void     interface_base_init( ofaLedgerIStoreInterface *klass );
static void     interface_base_finalize( ofaLedgerIStoreInterface *klass );
static void     load_dataset( ofaLedgerIStore *instance, sIStore *sdata );
static void     insert_row( ofaLedgerIStore *instance, sIStore *sdata, const ofoLedger *ledger );
static void     set_row( ofaLedgerIStore *instance, sIStore *sdata, GtkTreeIter *iter, const ofoLedger *ledger );
static void     setup_signaling_connect( ofaLedgerIStore *instance, sIStore *sdata );
static void     on_new_object( ofoDossier *dossier, ofoBase *object, ofaLedgerIStore *instance );
static void     on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaLedgerIStore *instance );
static gboolean find_ledger_by_mnemo( ofaLedgerIStore *instance, sIStore *sdata, const gchar *code, GtkTreeIter *iter );
static void     on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaLedgerIStore *instance );
static void     on_reload_dataset( ofoDossier *dossier, GType type, ofaLedgerIStore *instance );
static void     on_parent_finalized( ofaLedgerIStore *instance, gpointer finalized_parent );
static void     on_object_finalized( sIStore *sdata, gpointer finalized_object );
static sIStore *get_istore_data( ofaLedgerIStore *instance );

/**
 * ofa_ledger_istore_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_ledger_istore_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_ledger_istore_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_ledger_istore_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaLedgerIStoreInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaLedgerIStore", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaLedgerIStoreInterface *klass )
{
	static const gchar *thisfn = "ofa_ledger_istore_interface_base_init";
	GType interface_type = G_TYPE_FROM_INTERFACE( klass );

	g_debug( "%s: klass=%p (%s), st_initializations=%d",
			thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ), st_initializations );

	if( !st_initializations ){
		/* declare interface default methods here */

		/**
		 * ofaLedgerIStore::changed:
		 *
		 * This signal is sent by the views when the selection is
		 * changed.
		 *
		 * Arguments is the selected ledger mnemo.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaLedgerIStore *store,
		 * 						const gchar     *mnemo,
		 * 						gpointer         user_data );
		 */
		st_signals[ CHANGED ] = g_signal_new_class_handler(
					"changed",
					interface_type,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					1,
					G_TYPE_STRING );

		/**
		 * ofaLedgerIStore::activated:
		 *
		 * This signal is sent by the views when the selection is
		 * activated.
		 *
		 * Arguments is the selected ledger mnemo.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaLedgerIStore *store,
		 * 						const gchar     *mnemo,
		 * 						gpointer         user_data );
		 */
		st_signals[ ACTIVATED ] = g_signal_new_class_handler(
					"activated",
					interface_type,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					1,
					G_TYPE_STRING );
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaLedgerIStoreInterface *klass )
{
	static const gchar *thisfn = "ofa_ledger_istore_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_ledger_istore_get_interface_last_version:
 * @instance: this #ofaLedgerIStore instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_ledger_istore_get_interface_last_version( const ofaLedgerIStore *instance )
{
	return( LEDGER_ISTORE_LAST_VERSION );
}

/**
 * ofa_ledger_istore_attach_to:
 * @instance: this #ofaLedgerIStore instance.
 * @parent: the #GtkContainer to which the widget should be attached.
 *
 * Attach the widget to its parent.
 *
 * A weak notify reference is put on the parent, so that the GObject
 * will be unreffed when the parent will be destroyed.
 */
void
ofa_ledger_istore_attach_to( ofaLedgerIStore *instance, GtkContainer *parent )
{
	sIStore *sdata;

	g_return_if_fail( instance && G_IS_OBJECT( instance ) && OFA_IS_LEDGER_ISTORE( instance ));
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	g_object_weak_ref( G_OBJECT( parent ), ( GWeakNotify ) on_parent_finalized, instance );

	if( OFA_LEDGER_ISTORE_GET_INTERFACE( instance )->attach_to ){
		OFA_LEDGER_ISTORE_GET_INTERFACE( instance )->attach_to( instance, parent );
	}

	gtk_widget_show_all( GTK_WIDGET( parent ));
}

/**
 * ofa_ledger_istore_set_columns:
 * @instance: this #ofaLedgerIStore instance.
 * @columns: the columns to be displayed from the #GtkListStore.
 */
void
ofa_ledger_istore_set_columns( ofaLedgerIStore *instance, ofaLedgerColumns columns )
{
	sIStore *sdata;

	g_return_if_fail( instance && G_IS_OBJECT( instance ) && OFA_IS_LEDGER_ISTORE( instance ));

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	sdata->columns = columns;

	sdata->store = gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* code, label, last_entry */
			G_TYPE_STRING, G_TYPE_STRING, 					/* last_close, notes */
			G_TYPE_STRING, G_TYPE_STRING,					/* upd_user, upd_stamp */
			G_TYPE_OBJECT );								/* the #ofoLedger itself */

	if( OFA_LEDGER_ISTORE_GET_INTERFACE( instance )->set_columns ){
		OFA_LEDGER_ISTORE_GET_INTERFACE( instance )->set_columns( instance, sdata->store, columns );
	}

	g_object_unref( sdata->store );
}

/**
 * ofa_ledger_istore_set_dossier:
 * @instance: this #ofaLedgerIStore instance.
 * @dossier: the opened #ofoDossier.
 *
 * Set the dossier and load the corresponding dataset.
 * Connect to the dossier signaling system in order to maintain the
 * dataset up to date.
 */
void
ofa_ledger_istore_set_dossier( ofaLedgerIStore *instance, ofoDossier *dossier )
{
	sIStore *sdata;

	g_return_if_fail( instance && G_IS_OBJECT( instance ) && OFA_IS_LEDGER_ISTORE( instance ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	sdata->dossier = dossier;

	load_dataset( instance, sdata );
	setup_signaling_connect( instance, sdata );
}

static void
load_dataset( ofaLedgerIStore *instance, sIStore *sdata )
{
	const GList *dataset, *it;
	ofoLedger *ledger;

	dataset = ofo_ledger_get_dataset( sdata->dossier );

	for( it=dataset ; it ; it=it->next ){
		ledger = OFO_LEDGER( it->data );
		insert_row( instance, sdata, ledger );
	}
}

static void
insert_row( ofaLedgerIStore *instance, sIStore *sdata, const ofoLedger *ledger )
{
	GtkTreeIter iter;

	gtk_list_store_insert( sdata->store, &iter, -1 );
	set_row( instance, sdata, &iter, ledger );
}

static void
set_row( ofaLedgerIStore *instance, sIStore *sdata, GtkTreeIter *iter, const ofoLedger *ledger )
{
	gchar *stamp, *sdentry, *sdclose;
	GDate *dentry;
	const GDate *dclose;

	dentry = ofo_ledger_get_last_entry( ledger, sdata->dossier );
	sdentry = my_date_to_str( dentry, MY_DATE_DMYY );
	g_free( dentry );
	dclose = ofo_ledger_get_last_close( ledger );
	sdclose = my_date_to_str( dclose, MY_DATE_DMYY );
	stamp  = my_utils_stamp_to_str( ofo_ledger_get_upd_stamp( ledger ), MY_STAMP_DMYYHM );

	gtk_list_store_set(
			sdata->store,
			iter,
			COL_MNEMO,      ofo_ledger_get_mnemo( ledger ),
			COL_LABEL,      ofo_ledger_get_label( ledger ),
			COL_LAST_ENTRY, sdentry,
			COL_LAST_CLOSE, sdclose,
			COL_UPD_USER,   ofo_ledger_get_upd_user( ledger ),
			COL_UPD_STAMP,  stamp,
			COL_OBJECT,     ledger,
			-1 );

	g_free( stamp );
	g_free( sdclose );
	g_free( sdentry );
}

static void
setup_signaling_connect( ofaLedgerIStore *instance, sIStore *sdata )
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
on_new_object( ofoDossier *dossier, ofoBase *object, ofaLedgerIStore *instance )
{
	static const gchar *thisfn = "ofa_ledger_istore_on_new_object";
	sIStore *sdata;

	g_debug( "%s: dossier=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) instance );

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	if( OFO_IS_LEDGER( object )){
		insert_row( instance, sdata, OFO_LEDGER( object ));
	}
}

static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaLedgerIStore *instance )
{
	static const gchar *thisfn = "ofa_ledger_istore_on_updated_object";
	sIStore *sdata;
	GtkTreeIter iter;
	const gchar *mnemo, *new_mnemo;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, instance=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) instance );

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	if( OFO_IS_LEDGER( object )){
		new_mnemo = ofo_ledger_get_mnemo( OFO_LEDGER( object ));
		mnemo = prev_id ? prev_id : new_mnemo;
		if( find_ledger_by_mnemo( instance, sdata, mnemo, &iter )){
			set_row( instance, sdata, &iter, OFO_LEDGER( object ));
		}
	}
}

static gboolean
find_ledger_by_mnemo( ofaLedgerIStore *instance, sIStore *sdata, const gchar *mnemo, GtkTreeIter *iter )
{
	GtkTreeModel *tmodel;
	gchar *str;
	gint cmp;

	tmodel = GTK_TREE_MODEL( sdata->store );

	if( gtk_tree_model_get_iter_first( tmodel, iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, iter, COL_MNEMO, &str, -1 );
			cmp = g_utf8_collate( str, mnemo );
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
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaLedgerIStore *instance )
{
	static const gchar *thisfn = "ofa_ledger_istore_on_deleted_object";
	sIStore *sdata;
	GtkTreeIter iter;

	g_debug( "%s: dossier=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) instance );

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	if( OFO_IS_LEDGER( object )){
		if( find_ledger_by_mnemo( instance, sdata,
				ofo_ledger_get_mnemo( OFO_LEDGER( object )), &iter )){

			gtk_list_store_remove( sdata->store, &iter );
		}
	}
}

static void
on_reload_dataset( ofoDossier *dossier, GType type, ofaLedgerIStore *instance )
{
	static const gchar *thisfn = "ofa_ledger_istore_on_reload_dataset";
	sIStore *sdata;

	g_debug( "%s: dossier=%p, type=%lu, instance=%p",
			thisfn, ( void * ) dossier, type, ( void * ) instance );

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	if( type == OFO_TYPE_LEDGER ){
		gtk_list_store_clear( sdata->store );
		load_dataset( instance, sdata );
	}
}

/**
 * ofa_ledger_istore_get_column_number:
 * @instance: this #ofaLedgerIStore instance.
 * @column: the #ofaLedgerColumns identifier.
 *
 * Returns: the number of the column in the store, counted from zero,
 * or -1 if the column is unknown.
 */
gint
ofa_ledger_istore_get_column_number( const ofaLedgerIStore *instance, ofaLedgerColumns column )
{
	static const gchar *thisfn = "ofa_ledger_istore_get_column_number";

	switch( column ){
		case LEDGER_COL_MNEMO:
			return( COL_MNEMO );
			break;

		case LEDGER_COL_LABEL:
			return( COL_LABEL );
			break;

		case LEDGER_COL_LAST_ENTRY:
			return( COL_LAST_ENTRY );
			break;

		case LEDGER_COL_LAST_CLOSE:
			return( COL_LAST_CLOSE );
			break;

		case LEDGER_COL_NOTES:
			return( COL_NOTES );
			break;

		case LEDGER_COL_UPD_USER:
			return( COL_UPD_USER );
			break;

		case LEDGER_COL_UPD_STAMP:
			return( COL_UPD_STAMP );
			break;
	}

	g_warning( "%s: unknown column:%d", thisfn, column );
	return( -1 );
}

static void
on_parent_finalized( ofaLedgerIStore *instance, gpointer finalized_parent )
{
	static const gchar *thisfn = "ofa_ledger_istore_on_parent_finalized";

	g_debug( "%s: instance=%p, finalized_parent=%p",
			thisfn, ( void * ) instance, ( void * ) finalized_parent );

	g_return_if_fail( instance );

	g_object_unref( G_OBJECT( instance ));
}

static void
on_object_finalized( sIStore *sdata, gpointer finalized_object )
{
	static const gchar *thisfn = "ofa_ledger_istore_on_object_finalized";
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

static sIStore *
get_istore_data( ofaLedgerIStore *instance )
{
	sIStore *sdata;

	sdata = ( sIStore * ) g_object_get_data( G_OBJECT( instance ), LEDGER_ISTORE_DATA );

	if( !sdata ){
		sdata = g_new0( sIStore, 1 );
		g_object_set_data( G_OBJECT( instance ), LEDGER_ISTORE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_object_finalized, sdata );
	}

	return( sdata );
}
