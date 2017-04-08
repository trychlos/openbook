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

#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-igetter.h"
#include "api/ofa-isignaler.h"
#include "api/ofa-prefs.h"
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-currency.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"

#include "ofa-accentry-store.h"

/* private instance data
 */
typedef struct {
	gboolean     dispose_has_run;

	/* initialization
	 */
	ofaIGetter  *getter;

	/* runtime
	 */
	GList       *signaler_handlers;

	/* while inserting entries
	 */
	gchar       *currency_code;
	ofoCurrency *currency;
}
	ofaAccentryStorePrivate;

/* store data types
 */
static GType st_col_types[ACCENTRY_N_COLUMNS] = {
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,		/* account, label, currency */
	G_TYPE_STRING, G_TYPE_STRING,						/* upd_user, upd_stamp */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,		/* acc_settleable, acc_keep_unsettled, acc_reconciliable */
	G_TYPE_STRING,										/* acc_keep_unreconciliated */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,		/* ent_dope, ent_deffect, ent_ref */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,		/* ent_ledger, ent_ope_template, ent_debit */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,		/* ent_credit, ent_openum, ent_entnum,  */
	G_TYPE_INT, G_TYPE_STRING, 							/* ent_entnum_i, ent_status */
	G_TYPE_OBJECT										/* the #ofoEntry or #ofoAccount */
};

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaAccentryStore *self );
static void     load_store( ofaAccentryStore *self );
static void     account_insert_row( ofaAccentryStore *self, ofoAccount *account );
static void     account_set_row_by_iter( ofaAccentryStore *self, ofoAccount *account, GtkTreeIter *iter );
static void     entry_insert_row( ofaAccentryStore *self, const ofoEntry *entry );
static void     entry_set_row_by_iter( ofaAccentryStore *store, const ofoEntry *entry, GtkTreeIter *iter );
static gboolean find_account_by_number( ofaAccentryStore *self, const gchar *account, GtkTreeIter *iter );
static gboolean find_entry_by_number( ofaAccentryStore *self, ofxCounter number, GtkTreeIter *iter );
static gboolean find_entry_by_number_rec( ofaAccentryStore *self, ofxCounter number, GtkTreeIter *iter );
static void     set_currency_new_id( ofaAccentryStore *self, const gchar *prev_id, const gchar *new_id );
static void     set_ledger_new_id( ofaAccentryStore *self, const gchar *prev_id, const gchar *new_id );
static void     set_ope_template_new_id( ofaAccentryStore *self, const gchar *prev_id, const gchar *new_id );
static void     update_column( ofaAccentryStore *self, const gchar *prev_id, const gchar *new_id, guint column );
static void     update_column_rec( ofaAccentryStore *self, const gchar *prev_id, const gchar *new_id, guint column, GtkTreeIter *iter );
static void     signaler_connect_to_signaling_system( ofaAccentryStore *self );
static void     signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaAccentryStore *self );
static void     signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, ofaAccentryStore *self );

G_DEFINE_TYPE_EXTENDED( ofaAccentryStore, ofa_accentry_store, OFA_TYPE_TREE_STORE, 0,
		G_ADD_PRIVATE( ofaAccentryStore ))

static void
accentry_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_accentry_store_finalize";
	ofaAccentryStorePrivate *priv;

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCENTRY_STORE( instance ));

	/* free data members here */
	priv = ofa_accentry_store_get_instance_private( OFA_ACCENTRY_STORE( instance ));

	g_free( priv->currency_code );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_accentry_store_parent_class )->finalize( instance );
}

static void
accentry_store_dispose( GObject *instance )
{
	ofaAccentryStorePrivate *priv;
	ofaISignaler *signaler;

	g_return_if_fail( instance && OFA_IS_ACCENTRY_STORE( instance ));

	priv = ofa_accentry_store_get_instance_private( OFA_ACCENTRY_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* disconnect from ofaISignaler signaling system */
		signaler = ofa_igetter_get_signaler( priv->getter );
		ofa_isignaler_disconnect_handlers( signaler, &priv->signaler_handlers );

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_accentry_store_parent_class )->dispose( instance );
}

static void
ofa_accentry_store_init( ofaAccentryStore *self )
{
	static const gchar *thisfn = "ofa_accentry_store_init";
	ofaAccentryStorePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ACCENTRY_STORE( self ));

	priv = ofa_accentry_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->signaler_handlers = NULL;
}

static void
ofa_accentry_store_class_init( ofaAccentryStoreClass *klass )
{
	static const gchar *thisfn = "ofa_accentry_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = accentry_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = accentry_store_finalize;
}

/**
 * ofa_accentry_store_new:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: a reference to a new #ofaAccentryStore, which should be
 * released by the caller.
 */
ofaAccentryStore *
ofa_accentry_store_new( ofaIGetter *getter )
{
	ofaAccentryStore *store;
	ofaAccentryStorePrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	store = g_object_new( OFA_TYPE_ACCENTRY_STORE, NULL );

	priv = ofa_accentry_store_get_instance_private( store );

	priv->getter = getter;

	gtk_tree_store_set_column_types(
			GTK_TREE_STORE( store ), ACCENTRY_N_COLUMNS, st_col_types );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( store ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	signaler_connect_to_signaling_system( store );
	load_store( store );

	return( store );
}

/*
 * sorting the store per account/entry number ascending
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaAccentryStore *self )
{
	gchar *acca, *enta, *accb, *entb;
	gulong numa, numb;
	gint cmp;

	gtk_tree_model_get( tmodel, a, ACCENTRY_COL_ACCOUNT, &acca, ACCENTRY_COL_ENT_NUMBER, &enta, -1 );
	gtk_tree_model_get( tmodel, a, ACCENTRY_COL_ACCOUNT, &accb, ACCENTRY_COL_ENT_NUMBER, &entb, -1 );

	cmp = my_collate( acca, accb );

	if( cmp == 0 ){
		numa = enta ? atol( enta ) : 0;
		numb = entb ? atol( entb ) : 0;
		cmp = numa < numb ? -1 : ( numa > numb ? 1 : 0 );
	}

	g_free( acca );
	g_free( enta );

	g_free( accb );
	g_free( entb );

	return( cmp );
}

/*
 * load_store:
 * @self: this #ofaAccentryStore instance.
 */
static void
load_store( ofaAccentryStore *self )
{
	ofaAccentryStorePrivate *priv;
	GList *account_dataset, *entry_dataset, *it;

	priv = ofa_accentry_store_get_instance_private( self );

	account_dataset = ofo_account_get_dataset( priv->getter );
	entry_dataset = ofo_entry_get_dataset( priv->getter );

	for( it=account_dataset ; it ; it=it->next ){
		account_insert_row( self, OFO_ACCOUNT( it->data ));
	}

	for( it=entry_dataset ; it ; it=it->next ){
		entry_insert_row( self, OFO_ENTRY( it->data ));
	}
}

static void
account_insert_row( ofaAccentryStore *self, ofoAccount *account )
{
	GtkTreeIter iter;

	gtk_tree_store_insert( GTK_TREE_STORE( self ), &iter, NULL, -1 );
	account_set_row_by_iter( self, account, &iter );
}

static void
account_set_row_by_iter( ofaAccentryStore *self, ofoAccount *account, GtkTreeIter *iter )
{
	gchar *stamp_str;

	stamp_str = my_stamp_to_str( ofo_account_get_upd_stamp( account ), MY_STAMP_YYMDHMS );

	gtk_tree_store_set(
				GTK_TREE_STORE( self ),
				iter,
				ACCENTRY_COL_ACCOUNT,              ofo_account_get_number( account ),
				ACCENTRY_COL_LABEL,                ofo_account_get_label( account ),
				ACCENTRY_COL_CURRENCY,             ofo_account_get_currency( account ),
				ACCENTRY_COL_UPD_USER,             ofo_account_get_upd_user( account ),
				ACCENTRY_COL_UPD_STAMP,            stamp_str,
				ACCENTRY_COL_SETTLEABLE,           ofo_account_is_settleable( account ) ? _( "Y" ): _( "N" ),
				ACCENTRY_COL_KEEP_UNSETTLED,       ofo_account_get_keep_unsettled( account ) ? _( "Y" ): _( "N" ),
				ACCENTRY_COL_RECONCILIABLE,        ofo_account_is_reconciliable( account ) ? _( "Y" ): _( "N" ),
				ACCENTRY_COL_KEEP_UNRECONCILIATED, ofo_account_get_keep_unreconciliated( account ) ? _( "Y" ): _( "N" ),
				ACCENTRY_COL_ENT_NUMBER_I,         0,
				ACCENTRY_COL_OBJECT,               account,
				-1 );

	g_free( stamp_str );
}

static void
entry_insert_row( ofaAccentryStore *self, const ofoEntry *entry )
{
	static const gchar *thisfn = "ofa_accentry_store_entry_insert_row";
	GtkTreeIter iter, parent_iter;
	const gchar *account;

	account = ofo_entry_get_account( entry );

	if( find_account_by_number( self, account, &parent_iter )){
		gtk_tree_store_insert( GTK_TREE_STORE( self ), &iter, &parent_iter, -1 );
		entry_set_row_by_iter( self, entry, &iter );

	} else {
		g_warning( "%s: unable to find the account %s", thisfn, account );
	}
}

static void
entry_set_row_by_iter( ofaAccentryStore *self, const ofoEntry *entry, GtkTreeIter *iter )
{
	ofaAccentryStorePrivate *priv;
	gchar *sdope, *sdeff, *sdeb, *scre, *sopenum, *sentnum, *supdstamp;
	const gchar *cstr, *cref, *cupduser, *ccur;
	ofxCounter counter, entnum;
	ofxAmount amount;
	ofeEntryStatus status;

	priv = ofa_accentry_store_get_instance_private( self );

	ccur = ofo_entry_get_currency( entry );
	if( !my_collate( ccur, priv->currency_code )){
		g_free( priv->currency_code );
		priv->currency_code = g_strdup( ccur );
		priv->currency = ofo_currency_get_by_code( priv->getter, ccur );
	}

	sdope = my_date_to_str( ofo_entry_get_dope( entry ), ofa_prefs_date_get_display_format( priv->getter ));
	sdeff = my_date_to_str( ofo_entry_get_deffect( entry ), ofa_prefs_date_get_display_format( priv->getter ));

	cstr = ofo_entry_get_ref( entry );
	cref = cstr ? cstr : "";

	amount = ofo_entry_get_debit( entry );
	sdeb = amount ? ofa_amount_to_str( amount, priv->currency, priv->getter ) : g_strdup( "" );

	amount = ofo_entry_get_credit( entry );
	scre = amount ? ofa_amount_to_str( amount, priv->currency, priv->getter ) : g_strdup( "" );

	counter = ofo_entry_get_ope_number( entry );
	sopenum = counter ? g_strdup_printf( "%lu", counter ) : g_strdup( "" );

	entnum = ofo_entry_get_number( entry );
	sentnum = g_strdup_printf( "%lu", entnum );

	cstr = ofo_entry_get_upd_user( entry );
	cupduser = cstr ? cstr : "";
	supdstamp = my_stamp_to_str( ofo_entry_get_upd_stamp( entry ), MY_STAMP_DMYYHM );

	status = ofo_entry_get_status( entry );

	gtk_tree_store_set(
				GTK_TREE_STORE( self ),
				iter,
				ACCENTRY_COL_ACCOUNT,              ofo_entry_get_account( entry ),
				ACCENTRY_COL_LABEL,                ofo_entry_get_label( entry ),
				ACCENTRY_COL_CURRENCY,             ccur,
				ACCENTRY_COL_UPD_USER,             cupduser,
				ACCENTRY_COL_UPD_STAMP,            supdstamp,
				ACCENTRY_COL_DOPE,                 sdope,
				ACCENTRY_COL_DEFFECT,              sdeff,
				ACCENTRY_COL_REF,                  cref,
				ACCENTRY_COL_LEDGER,               ofo_entry_get_ledger( entry ),
				ACCENTRY_COL_OPE_TEMPLATE,         ofo_entry_get_ope_template( entry ),
				ACCENTRY_COL_DEBIT,                sdeb,
				ACCENTRY_COL_CREDIT,               scre,
				ACCENTRY_COL_OPE_NUMBER,           sopenum,
				ACCENTRY_COL_ENT_NUMBER,           sentnum,
				ACCENTRY_COL_ENT_NUMBER_I,         entnum,
				ACCENTRY_COL_STATUS,               ofo_entry_status_get_abr( status ),
				ACCENTRY_COL_OBJECT,               entry,
				-1 );

	g_free( supdstamp );
	g_free( sentnum );
	g_free( sopenum );
	g_free( scre );
	g_free( sdeb );
	g_free( sdeff );
	g_free( sdope );
}

/**
 * ofa_accentry_store_is_empty:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if the store is empty.
 */
gboolean
ofa_accentry_store_is_empty( ofaAccentryStore *store )
{
	ofaAccentryStorePrivate *priv;
	GtkTreeIter iter;
	gboolean has_row;

	g_return_val_if_fail( store && OFA_IS_ACCENTRY_STORE( store ), TRUE );

	priv = ofa_accentry_store_get_instance_private( store );

	g_return_val_if_fail( !priv->dispose_has_run, TRUE );

	has_row = gtk_tree_model_get_iter_first( GTK_TREE_MODEL( store ), &iter );

	return( !has_row );
}

/*
 * Returns: %TRUE if the account has been found.
 */
static gboolean
find_account_by_number( ofaAccentryStore *self, const gchar *account, GtkTreeIter *iter )
{
	gchar *row_account;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, ACCENTRY_COL_ACCOUNT, &row_account, -1 );
			cmp = my_collate( account, row_account );
			g_free( row_account );
			if( cmp <= 0 ){
				return( cmp == 0 );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), iter )){
				break;
			}
		}
	}

	return( FALSE );
}

/*
 * Returns: %TRUE if the entry has been found.
 */
static gboolean
find_entry_by_number( ofaAccentryStore *self, ofxCounter number, GtkTreeIter *iter )
{
	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), iter )){
		if( find_entry_by_number_rec( self, number, iter )){
			return( TRUE );
		}
	}
	return( FALSE );
}

static gboolean
find_entry_by_number_rec( ofaAccentryStore *self, ofxCounter number, GtkTreeIter *iter )
{
	ofxCounter row_id;
	GtkTreeIter child_iter;

	while( TRUE ){
		if( gtk_tree_model_iter_children( GTK_TREE_MODEL( self ), &child_iter, iter )){
			if( find_entry_by_number_rec( self, number, &child_iter )){
				*iter = child_iter;
				return( TRUE );
			}
		}
		gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, ACCENTRY_COL_ENT_NUMBER_I, &row_id, -1 );
		if( row_id == number ){
			return( TRUE );
		}
		if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), iter )){
			break;
		}
	}

	return( FALSE );
}

static void
set_currency_new_id( ofaAccentryStore *self, const gchar *prev_id, const gchar *new_id )
{
	update_column( self, prev_id, new_id, ACCENTRY_COL_CURRENCY );
}

static void
set_ledger_new_id( ofaAccentryStore *self, const gchar *prev_id, const gchar *new_id )
{
	update_column( self, prev_id, new_id, ACCENTRY_COL_LEDGER );
}

static void
set_ope_template_new_id( ofaAccentryStore *self, const gchar *prev_id, const gchar *new_id )
{
	update_column( self, prev_id, new_id, ACCENTRY_COL_OPE_TEMPLATE );
}

static void
update_column( ofaAccentryStore *self, const gchar *prev_id, const gchar *new_id, guint column )
{
	GtkTreeIter iter;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), &iter )){
		update_column_rec( self, prev_id, new_id, column, &iter );
	}
}

static void
update_column_rec( ofaAccentryStore *self, const gchar *prev_id, const gchar *new_id, guint column, GtkTreeIter *iter )
{
	GtkTreeIter child_iter;
	gchar *row_id;

	while( TRUE ){
		if( gtk_tree_model_iter_children( GTK_TREE_MODEL( self ), &child_iter, iter )){
			update_column_rec( self, prev_id, new_id, column, &child_iter );
		}
		gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, column, &row_id, -1 );
		if( !my_collate( row_id, prev_id )){
			gtk_tree_store_set( GTK_TREE_STORE( self ), iter, column, new_id, -1 );
		}
		g_free( row_id );
		if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), iter )){
			break;
		}
	}
}

/*
 * Connect to ofaISignaler signaling system
 */
static void
signaler_connect_to_signaling_system( ofaAccentryStore *self )
{
	ofaAccentryStorePrivate *priv;
	ofaISignaler *signaler;
	gulong handler;

	priv = ofa_accentry_store_get_instance_private( self );

	signaler = ofa_igetter_get_signaler( priv->getter );

	handler = g_signal_connect( signaler, SIGNALER_BASE_NEW, G_CALLBACK( signaler_on_new_base ), self );
	priv->signaler_handlers = g_list_prepend( priv->signaler_handlers, ( gpointer ) handler );

	handler = g_signal_connect( signaler, SIGNALER_BASE_UPDATED, G_CALLBACK( signaler_on_updated_base ), self );
	priv->signaler_handlers = g_list_prepend( priv->signaler_handlers, ( gpointer ) handler );
}

/*
 * SIGNALER_BASE_NEW signal handler
 */
static void
signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaAccentryStore *self )
{
	static const gchar *thisfn = "ofa_accentry_store_signaler_on_new_base";

	g_debug( "%s: signaler=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_ACCOUNT( object )){
		account_insert_row( self, OFO_ACCOUNT( object ));

	} else if( OFO_IS_ENTRY( object )){
		entry_insert_row( self, OFO_ENTRY( object ));
	}
}

/*
 * SIGNALER_BASE_UPDATED signal handler
 */
static void
signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, ofaAccentryStore *self )
{
	static const gchar *thisfn = "ofa_accentry_store_signaler_on_updated_base";
	GtkTreeIter iter;

	g_debug( "%s: signaler=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_ACCOUNT( object )){
		if( find_account_by_number( self, prev_id, &iter )){
			account_set_row_by_iter( self, OFO_ACCOUNT( object ), &iter );
		}

	} else if( OFO_IS_ENTRY( object )){
		if( find_entry_by_number( self, ofo_entry_get_number( OFO_ENTRY( object )), &iter )){
			entry_set_row_by_iter( self, OFO_ENTRY( object ), &iter );
		}

	} else if( prev_id ){
		if( OFO_IS_CURRENCY( object )){
			set_currency_new_id( self, prev_id, ofo_currency_get_code( OFO_CURRENCY( object )));

		} else if( OFO_IS_LEDGER( object )){
			set_ledger_new_id( self, prev_id, ofo_ledger_get_mnemo( OFO_LEDGER( object )));

		} else if( OFO_IS_OPE_TEMPLATE( object )){
			set_ope_template_new_id( self, prev_id, ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object )));
		}
	}
}
