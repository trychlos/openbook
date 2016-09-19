/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include <glib/gi18n.h>

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-preferences.h"
#include "api/ofo-account.h"
#include "api/ofo-concil.h"
#include "api/ofo-currency.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofs-concil-id.h"

#include "ofa-entry-store.h"
#include "core/ofa-iconcil.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;

	/* runtime
	 */
	ofaHub  *hub;
	GList   *hub_handlers;
}
	ofaEntryStorePrivate;

/* store data types
 */
static GType st_col_types[ENTRY_N_COLUMNS] = {
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* dope, deffect, label */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* ref, currency, ledger */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* ope_template, account, debit */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* credit, ope_number, stlmt_number */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* stlmt_user, stlmt_stamp, ent_number_str */
	G_TYPE_INT,										/* ent_number_int */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* upd_user, upd_stamp, concil_number */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,		/* concil_date, status_str, status_int */
	G_TYPE_OBJECT									/* the #ofoEntry itself */
};

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaEntryStore *store );
static void     insert_row( ofaEntryStore *store, ofaHub *hub, const ofoEntry *entry );
static void     set_row( ofaEntryStore *store, ofaHub *hub, const ofoEntry *entry, GtkTreeIter *iter );
static void     set_row_concil( ofaEntryStore *store, ofoConcil *concil, GtkTreeIter *iter );
static gboolean find_row_by_number( ofaEntryStore *store, ofxCounter number, GtkTreeIter *iter );
static void     do_update_concil( ofaEntryStore *store, ofoConcil *concil, gboolean is_deleted );
static void     hub_setup_signaling_system( ofaEntryStore *store, ofaHub *hub );
static void     hub_on_new_object( ofaHub *hub, ofoBase *object, ofaEntryStore *store );
static void     hub_on_new_entry( ofaEntryStore *store, ofoEntry *entry );
static void     hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaEntryStore *store );
static void     hub_do_update_account_number( ofaEntryStore *store, const gchar *prev, const gchar *number );
static void     hub_do_update_currency_code( ofaEntryStore *store, const gchar *prev, const gchar *code );
static void     hub_do_update_ledger_mnemo( ofaEntryStore *store, const gchar *prev, const gchar *mnemo );
static void     hub_do_update_ope_template_mnemo( ofaEntryStore *store, const gchar *prev, const gchar *mnemo );
static void     hub_on_updated_concil( ofaEntryStore *store, ofoConcil *concil );
static void     hub_on_updated_entry( ofaEntryStore *store, ofoEntry *entry );
static void     hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaEntryStore *store );
static void     hub_on_deleted_concil( ofaEntryStore *store, ofoConcil *concil );
static void     hub_on_deleted_entry( ofaEntryStore *store, ofoEntry *entry );

G_DEFINE_TYPE_EXTENDED( ofaEntryStore, ofa_entry_store, GTK_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaEntryStore ))

static void
entry_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_entry_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ENTRY_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_entry_store_parent_class )->finalize( instance );
}

static void
entry_store_dispose( GObject *instance )
{
	ofaEntryStorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_ENTRY_STORE( instance ));

	priv = ofa_entry_store_get_instance_private( OFA_ENTRY_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		ofa_hub_disconnect_handlers( priv->hub, &priv->hub_handlers );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_entry_store_parent_class )->dispose( instance );
}

static void
ofa_entry_store_init( ofaEntryStore *self )
{
	static const gchar *thisfn = "ofa_entry_store_init";
	ofaEntryStorePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ENTRY_STORE( self ));

	priv = ofa_entry_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->hub_handlers = NULL;
}

static void
ofa_entry_store_class_init( ofaEntryStoreClass *klass )
{
	static const gchar *thisfn = "ofa_entry_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = entry_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = entry_store_finalize;
}

/**
 * ofa_entry_store_new:
 * hub: the #ofaHub object of the application.
 *
 * Returns: a reference to a new #ofaEntryStore, which should be
 * released by the caller.
 */
ofaEntryStore *
ofa_entry_store_new( ofaHub *hub )
{
	ofaEntryStore *store;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	store = g_object_new( OFA_TYPE_ENTRY_STORE, NULL );

	gtk_tree_store_set_column_types(
			GTK_TREE_STORE( store ), ENTRY_N_COLUMNS, st_col_types );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( store ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	hub_setup_signaling_system( store, hub );

	return( store );
}

/*
 * sorting the store per entry number
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaEntryStore *store )
{
	ofxCounter numa, numb;
	gint cmp;

	gtk_tree_model_get( tmodel, a, ENTRY_COL_ENT_NUMBER_I, &numa, -1 );
	gtk_tree_model_get( tmodel, b, ENTRY_COL_ENT_NUMBER_I, &numb, -1 );

	cmp = numa < numb ? -1 : ( numa > numb ? 1 : 0 );

	return( cmp );
}

/**
 * ofa_entry_store_load:
 * store: this #ofaEntryStore instance.
 * @account: [allow-none]: the account identifier to be loaded.
 * @ledger: [allow-none]: the ledger identifier to be loaded.
 *
 * Loads the entries which satisfy both the two conditions (if set).
 *
 * Returns: the count of loaded entries.
 */
ofxCounter
ofa_entry_store_load( ofaEntryStore *store, const gchar *account, const gchar *ledger )
{
	ofaEntryStorePrivate *priv;
	GList *dataset, *it;
	ofxCounter count;
	ofoEntry *entry;

g_return_val_if_fail( store && OFA_IS_ENTRY_STORE( store ), 0 );

	priv = ofa_entry_store_get_instance_private( store );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	count = 0;
	gtk_list_store_clear( GTK_LIST_STORE( store ));
	dataset = ofo_entry_get_dataset_for_store( priv->hub, account, ledger );

	for( it=dataset ; it ; it=it->next ){
		entry = OFO_ENTRY( it->data );
		insert_row( store, priv->hub, entry );
	}

	count = g_list_length( dataset );
	g_list_free_full( dataset, ( GDestroyNotify ) g_object_unref );

	return( count );
}

static void
insert_row( ofaEntryStore *store, ofaHub *hub, const ofoEntry *entry )
{
	GtkTreeIter iter;

	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( store ),
			&iter,
			-1,
			ENTRY_COL_ENT_NUMBER_I, ofo_entry_get_number( entry ),
			ENTRY_COL_OBJECT,       entry ,
			-1 );

	set_row( store, hub, entry, &iter );
}

static void
set_row( ofaEntryStore *store, ofaHub *hub, const ofoEntry *entry, GtkTreeIter *iter )
{
	ofaEntryStorePrivate *priv;
	gchar *sdope, *sdeff, *sdeb, *scre, *sopenum, *ssetnum, *ssetstamp, *sentnum, *supdstamp;
	const gchar *cstr, *cref, *cur_code, *csetuser, *cupduser;
	ofoCurrency *cur_obj;
	ofxAmount amount;
	ofxCounter counter;
	ofoConcil *concil;

	priv = ofa_entry_store_get_instance_private( store );

	sdope = my_date_to_str( ofo_entry_get_dope( entry ), ofa_prefs_date_display());
	sdeff = my_date_to_str( ofo_entry_get_deffect( entry ), ofa_prefs_date_display());

	cstr = ofo_entry_get_ref( entry );
	cref = cstr ? cstr : "";

	cur_code = ofo_entry_get_currency( entry );
	g_return_if_fail( my_strlen( cur_code ));
	cur_obj = ofo_currency_get_by_code( priv->hub, cur_code );
	g_return_if_fail( cur_obj && OFO_IS_CURRENCY( cur_obj ));

	amount = ofo_entry_get_debit( entry );
	sdeb = amount ? ofa_amount_to_str( amount, cur_obj ) : g_strdup( "" );
	amount = ofo_entry_get_credit( entry );
	scre = amount ? ofa_amount_to_str( amount, cur_obj ) : g_strdup( "" );

	counter = ofo_entry_get_ope_number( entry );
	sopenum = counter ? g_strdup_printf( "%lu", counter ) : g_strdup( "" );

	counter = ofo_entry_get_settlement_number( entry );
	ssetnum = counter ? g_strdup_printf( "%lu", counter ) : g_strdup( "" );

	cstr = ofo_entry_get_settlement_user( entry );
	csetuser = cstr ? cstr : "";
	ssetstamp = my_utils_stamp_to_str( ofo_entry_get_settlement_stamp( entry ), MY_STAMP_DMYYHM );

	sentnum = g_strdup_printf( "%lu", ofo_entry_get_number( entry ));

	cstr = ofo_entry_get_upd_user( entry );
	cupduser = cstr ? cstr : "";
	supdstamp = my_utils_stamp_to_str( ofo_entry_get_upd_stamp( entry ), MY_STAMP_DMYYHM );


	gtk_list_store_set(
				GTK_LIST_STORE( store ),
				iter,
				ENTRY_COL_DOPE,          sdope,
				ENTRY_COL_DEFFECT,       sdeff,
				ENTRY_COL_LABEL,         ofo_entry_get_label( entry ),
				ENTRY_COL_REF,           cref,
				ENTRY_COL_CURRENCY,      cur_code,
				ENTRY_COL_LEDGER,        ofo_entry_get_ledger( entry ),
				ENTRY_COL_OPE_TEMPLATE,  ofo_entry_get_ope_template( entry ),
				ENTRY_COL_ACCOUNT,       ofo_entry_get_account( entry ),
				ENTRY_COL_DEBIT,         sdeb,
				ENTRY_COL_CREDIT,        scre,
				ENTRY_COL_OPE_NUMBER,    sopenum,
				ENTRY_COL_STLMT_NUMBER,  ssetnum,
				ENTRY_COL_STLMT_USER,    csetuser,
				ENTRY_COL_STLMT_STAMP,   ssetstamp,
				ENTRY_COL_ENT_NUMBER,    sentnum,
				ENTRY_COL_UPD_USER,      cupduser,
				ENTRY_COL_UPD_STAMP,     supdstamp,
				ENTRY_COL_CONCIL_NUMBER, "",
				ENTRY_COL_CONCIL_DATE,   "",
				ENTRY_COL_STATUS,        ofo_entry_get_abr_status( entry ),
				ENTRY_COL_STATUS_I,      ofo_entry_get_status( entry ),
				ENTRY_COL_OBJECT,        entry,
				ENTRY_COL_MSGERR,        "",
				ENTRY_COL_MSGWARN,       "",
				ENTRY_COL_DOPE_SET,      FALSE,
				ENTRY_COL_DEFFECT_SET,   FALSE,
				ENTRY_COL_CURRENCY_SET,  FALSE,
				-1 );

	g_free( supdstamp );
	g_free( sentnum );
	g_free( ssetstamp );
	g_free( ssetnum );
	g_free( sopenum );
	g_free( scre );
	g_free( sdeb );
	g_free( sdeff );
	g_free( sdope );

	concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ));
	if( concil ){
		set_row_concil( store, concil, iter );
	}
}

/*
 * iter is on the list store
 */
static void
set_row_concil( ofaEntryStore *store, ofoConcil *concil, GtkTreeIter *iter )
{
	gchar *srappro, *snum;

	srappro = concil ?
				my_date_to_str( ofo_concil_get_dval( concil ), ofa_prefs_date_display()) :
				g_strdup( "" );
	snum = concil ?
				g_strdup_printf( "%lu", ofo_concil_get_id( concil )) :
				g_strdup( "" );

	gtk_list_store_set(
				GTK_LIST_STORE( store ),
				iter,
				ENTRY_COL_CONCIL_NUMBER, snum,
				ENTRY_COL_CONCIL_DATE,   srappro,
				-1 );

	g_free( snum );
	g_free( srappro );
}

/*
 * find_row_by_number:
 * @store: this #ofaEntryStore
 * @number: [in]: the entry number we are searching for.
 * @iter: [out]: the found iter if %TRUE.
 *
 * Rows are sorted by entry number.
 * We exit the search as soon as we get a number greater than the
 * searched one, or the end of the list.
 *
 * Returns TRUE if we have found an exact match, and @iter addresses
 * this exact match.
 */
static gboolean
find_row_by_number( ofaEntryStore *store, ofxCounter number, GtkTreeIter *iter )
{
	ofxCounter row_number;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( store ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( store ), iter, ENTRY_COL_ENT_NUMBER_I, &row_number, -1 );
			if( row_number == number ){
				return( TRUE );
			}
			if( row_number > number ){
				break;
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( store ), iter )){
				break;
			}
		}
	}

	return( FALSE );
}

/*
 * a conciliation group is updated
 * -> update the entry row if needed
 */
static void
do_update_concil( ofaEntryStore *store, ofoConcil *concil, gboolean is_deleted )
{
	GList *ids, *it;
	ofsConcilId *sid;
	GtkTreeIter iter;

	ids = ofo_concil_get_ids( concil );
	for( it=ids ; it ; it=it->next ){
		sid = ( ofsConcilId * ) it->data;
		if( !g_strcmp0( sid->type, CONCIL_TYPE_ENTRY ) &&
				find_row_by_number( store, sid->other_id, &iter )){
			set_row_concil( store, is_deleted ? NULL : concil, &iter );
		}
	}
}

/*
 * connect to the hub signaling system
 */
static void
hub_setup_signaling_system( ofaEntryStore *store, ofaHub *hub )
{
	ofaEntryStorePrivate *priv;
	gulong handler;

	priv = ofa_entry_store_get_instance_private( store );

	priv->hub = hub;

	handler = g_signal_connect( hub, SIGNAL_HUB_NEW, G_CALLBACK( hub_on_new_object ), store );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( hub, SIGNAL_HUB_UPDATED, G_CALLBACK( hub_on_updated_object ), store );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( hub, SIGNAL_HUB_DELETED, G_CALLBACK( hub_on_deleted_object ), store );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
hub_on_new_object( ofaHub *hub, ofoBase *object, ofaEntryStore *store )
{
	static const gchar *thisfn = "ofa_entry_store_hub_on_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), store=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) store );

	if( OFO_IS_ENTRY( object )){
		hub_on_new_entry( store, OFO_ENTRY( object ));
	}
}

/*
 * A new entry has been created
 * update our dataset if the entry in all case (the filter model will
 * eventually take care of displaying it or not)
 */
static void
hub_on_new_entry( ofaEntryStore *store, ofoEntry *entry )
{
	ofaEntryStorePrivate *priv;

	priv = ofa_entry_store_get_instance_private( store );

	insert_row( store, priv->hub, entry );
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaEntryStore *store )
{
	static const gchar *thisfn = "ofa_entry_store_hub_on_updated_object";

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, store=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) store );

	if( prev_id ){
		if( OFO_IS_ACCOUNT( object )){
			hub_do_update_account_number( store, prev_id, ofo_account_get_number( OFO_ACCOUNT( object )));

		} else if( OFO_IS_CURRENCY( object )){
			hub_do_update_currency_code( store, prev_id, ofo_currency_get_code( OFO_CURRENCY( object )));

		} else if( OFO_IS_LEDGER( object )){
			hub_do_update_ledger_mnemo( store, prev_id, ofo_ledger_get_mnemo( OFO_LEDGER( object )));

		} else if( OFO_IS_OPE_TEMPLATE( object )){
			hub_do_update_ope_template_mnemo( store, prev_id, ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object )));
		}
	} else if( OFO_IS_CONCIL( object )){
		hub_on_updated_concil( store, OFO_CONCIL( object ));

	} else if( OFO_IS_ENTRY( object )){
		hub_on_updated_entry( store, OFO_ENTRY( object ));
	}
}

static void
hub_do_update_account_number( ofaEntryStore *store, const gchar *prev, const gchar *number )
{
	GtkTreeIter iter;
	gchar *str;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( store ), &iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( store ), &iter, ENTRY_COL_ACCOUNT, &str, -1 );
			cmp = g_utf8_collate( str, prev );
			g_free( str );
			if( cmp == 0 ){
				gtk_list_store_set( GTK_LIST_STORE( store ), &iter, ENTRY_COL_ACCOUNT, number, -1 );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( store ), &iter )){
				break;
			}
		}
	}
}

static void
hub_do_update_currency_code( ofaEntryStore *store, const gchar *prev, const gchar *code )
{
	GtkTreeIter iter;
	gchar *str;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( store ), &iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( store ), &iter, ENTRY_COL_CURRENCY, &str, -1 );
			cmp = g_utf8_collate( str, prev );
			g_free( str );
			if( cmp == 0 ){
				gtk_list_store_set( GTK_LIST_STORE( store ), &iter, ENTRY_COL_CURRENCY, code, -1 );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( store ), &iter )){
				break;
			}
		}
	}
}

static void
hub_do_update_ledger_mnemo( ofaEntryStore *store, const gchar *prev, const gchar *mnemo )
{
	GtkTreeIter iter;
	gchar *str;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( store ), &iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( store ), &iter, ENTRY_COL_LEDGER, &str, -1 );
			cmp = g_utf8_collate( str, prev );
			g_free( str );
			if( cmp == 0 ){
				gtk_list_store_set( GTK_LIST_STORE( store ), &iter, ENTRY_COL_LEDGER, mnemo, -1 );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( store ), &iter )){
				break;
			}
		}
	}
}

static void
hub_do_update_ope_template_mnemo( ofaEntryStore *store, const gchar *prev, const gchar *mnemo )
{
	GtkTreeIter iter;
	gchar *str;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( store ), &iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( store ), &iter, ENTRY_COL_OPE_TEMPLATE, &str, -1 );
			cmp = g_utf8_collate( str, prev );
			g_free( str );
			if( cmp == 0 ){
				gtk_list_store_set( GTK_LIST_STORE( store ), &iter, ENTRY_COL_OPE_TEMPLATE, mnemo, -1 );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( store ), &iter )){
				break;
			}
		}
	}
}

/*
 * a conciliation group is updated
 * -> update the entry row if needed
 */
static void
hub_on_updated_concil( ofaEntryStore *store, ofoConcil *concil )
{
	do_update_concil( store, concil, FALSE );
}

static void
hub_on_updated_entry( ofaEntryStore *store, ofoEntry *entry )
{
	ofaEntryStorePrivate *priv;
	GtkTreeIter iter;

	priv = ofa_entry_store_get_instance_private( store );

	if( find_row_by_number( store, ofo_entry_get_number( entry ), &iter )){
		set_row( store, priv->hub, entry, &iter );
	}
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaEntryStore *store )
{
	static const gchar *thisfn = "ofa_entry_store_hub_on_deleted_object";

	g_debug( "%s: hub=%p, object=%p (%s), store=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) store );

	if( OFO_IS_CONCIL( object )){
		hub_on_deleted_concil( store, OFO_CONCIL( object ));

	} else if( OFO_IS_ENTRY( object )){
		hub_on_deleted_entry( store, OFO_ENTRY( object ));
	}
}

static void
hub_on_deleted_concil( ofaEntryStore *store, ofoConcil *concil )
{
	do_update_concil( store, concil, TRUE );
}

static void
hub_on_deleted_entry( ofaEntryStore *store, ofoEntry *entry )
{
	static const gchar *thisfn = "ofa_entry_store_hub_on_deleted_entry";
	ofaEntryStorePrivate *priv;
	ofxCounter id;
	ofoConcil *concil;

	g_debug( "%s: store=%p, entry=%p", thisfn, ( void * ) store, ( void * ) entry );

	priv = ofa_entry_store_get_instance_private( store );

	/* if entry was settled, then cancel all settlement group */
	id = ofo_entry_get_settlement_number( entry );
	if( id > 0 ){
		ofo_entry_unsettle_by_number( priv->hub, id );
	}

	/* if entry was conciliated, then remove all conciliation group */
	concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ));
	if( concil ){
		ofa_iconcil_remove_concil( OFA_ICONCIL( entry ), concil );
	}
}