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

#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofa-dbms.h"
#include "api/ofa-iexportable.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"

/* priv instance data
 */
struct _ofoEntryPrivate {

	/* dbms data
	 */
	GDate          deffect;
	ofxCounter     number;
	GDate          dope;
	gchar         *label;
	gchar         *ref;
	gchar         *account;
	gchar         *currency;
	gchar         *ledger;
	gchar         *model;
	ofxAmount      debit;
	ofxAmount      credit;
	ofaEntryStatus status;
	gchar         *upd_user;
	GTimeVal       upd_stamp;
	GDate          concil_dval;
	gchar         *concil_user;
	GTimeVal       concil_stamp;
	ofxCounter     stlmt_number;
	gchar         *stlmt_user;
	GTimeVal       stlmt_stamp;
};

/* manage the abbreviated localized status
 */
typedef struct {
	ofaEntryStatus num;
	const gchar   *str;
}
	sStatus;

static sStatus st_status[] = {
		{ ENT_STATUS_ROUGH,     N_( "R" ) },
		{ ENT_STATUS_VALIDATED, N_( "V" ) },
		{ ENT_STATUS_DELETED,   N_( "D" ) },
		{ 0 },
};

static void         iexportable_iface_init( ofaIExportableInterface *iface );
static guint        iexportable_get_interface_version( const ofaIExportable *instance );
static void         on_updated_object( const ofoDossier *dossier, ofoBase *object, const gchar *prev_id, gpointer user_data );
static void         on_updated_object_account_number( const ofoDossier *dossier, const gchar *prev_id, const gchar *number );
static void         on_updated_object_currency_code( const ofoDossier *dossier, const gchar *prev_id, const gchar *code );
static void         on_updated_object_ledger_mnemo( const ofoDossier *dossier, const gchar *prev_id, const gchar *mnemo );
static void         on_updated_object_model_mnemo( const ofoDossier *dossier, const gchar *prev_id, const gchar *mnemo );
static gchar       *effect_in_exercice( const ofoDossier *dossier );
static GList       *entry_load_dataset( const ofaDbms *dbms, const gchar *where );
static const gchar *entry_list_columns( void );
static ofoEntry    *entry_parse_result( const GSList *row );
static gint         entry_count_for_currency( const ofaDbms *dbms, const gchar *currency );
static gint         entry_count_for_ledger( const ofaDbms *dbms, const gchar *ledger );
static gint         entry_count_for_ope_template( const ofaDbms *dbms, const gchar *model );
static gint         entry_count_for( const ofaDbms *dbms, const gchar *field, const gchar *mnemo );
static void         entry_set_upd_user( ofoEntry *entry, const gchar *upd_user );
static void         entry_set_upd_stamp( ofoEntry *entry, const GTimeVal *upd_stamp );
static void         entry_set_settlement_number( ofoEntry *entry, ofxCounter number );
static void         entry_set_settlement_user( ofoEntry *entry, const gchar *user );
static void         entry_set_settlement_stamp( ofoEntry *entry, const GTimeVal *stamp );
static gboolean     entry_do_insert( ofoEntry *entry, const ofaDbms *dbms, const gchar *user );
static void         error_ledger( const gchar *ledger );
static void         error_ope_template( const gchar *model );
static void         error_currency( const gchar *currency );
static void         error_acc_number( void );
static void         error_account( const gchar *number );
static void         error_acc_currency( ofoDossier *dossier, const gchar *currency, ofoAccount *account );
static void         error_amounts( ofxAmount debit, ofxAmount credit );
static void         error_entry( const gchar *message );
static gboolean     entry_do_update( ofoEntry *entry, const ofaDbms *dbms, const gchar *user );
static gboolean     do_update_concil( ofoEntry *entry, const gchar *user, const ofaDbms *dbms );
static gboolean     do_update_settlement( ofoEntry *entry, const gchar *user, const ofaDbms *dbms, ofxCounter number );
static gboolean     do_delete_entry( ofoEntry *entry, const ofaDbms *dbms, const gchar *user );
static gboolean     iexportable_export( ofaIExportable *exportable, const ofaExportSettings *settings, ofoDossier *dossier );

G_DEFINE_TYPE_EXTENDED( ofoEntry, ofo_entry, OFO_TYPE_BASE, 0, \
		G_IMPLEMENT_INTERFACE (OFA_TYPE_IEXPORTABLE, iexportable_iface_init ));

static void
entry_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_entry_finalize";
	ofoEntryPrivate *priv;

	priv = OFO_ENTRY( instance )->priv;

	g_debug( "%s: instance=%p (%s): %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			priv->label );

	g_return_if_fail( instance && OFO_IS_ENTRY( instance ));

	/* free data members here */
	g_free( priv->label );
	g_free( priv->currency );
	g_free( priv->ledger );
	g_free( priv->model );
	g_free( priv->ref );
	g_free( priv->upd_user );
	g_free( priv->concil_user );
	g_free( priv->stlmt_user );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_entry_parent_class )->finalize( instance );
}

static void
entry_dispose( GObject *instance )
{
	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_entry_parent_class )->dispose( instance );
}

static void
ofo_entry_init( ofoEntry *self )
{
	static const gchar *thisfn = "ofo_entry_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFO_TYPE_ENTRY, ofoEntryPrivate );

	self->priv->number = OFO_BASE_UNSET_ID;
	my_date_clear( &self->priv->deffect );
	my_date_clear( &self->priv->dope );
	my_date_clear( &self->priv->concil_dval );
}

static void
ofo_entry_class_init( ofoEntryClass *klass )
{
	static const gchar *thisfn = "ofo_entry_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = entry_dispose;
	G_OBJECT_CLASS( klass )->finalize = entry_finalize;

	g_type_class_add_private( klass, sizeof( ofoEntryPrivate ));
}

static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofo_entry_iexportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iexportable_get_interface_version;
	iface->export = iexportable_export;
}

static guint
iexportable_get_interface_version( const ofaIExportable *instance )
{
	return( 1 );
}

/**
 * ofo_entry_connect_handlers:
 *
 * This function is called once, when opening the dossier.
 */
void
ofo_entry_connect_handlers( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_entry_connect_handlers";

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	g_signal_connect( G_OBJECT( dossier ),
				SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), NULL );
}

/*
 * we try to report in recorded entries the modifications which may
 * happen on one of the externe identifiers - but only for the current
 * exercice
 *
 * nonetheless, this is never a good idea to modify an identifier which
 * is publicly known, and this always should be done with the greatest
 * attention
 */
static void
on_updated_object( const ofoDossier *dossier, ofoBase *object, const gchar *prev_id, gpointer user_data )
{
	static const gchar *thisfn = "ofo_entry_on_updated_object";
	const gchar *number;
	const gchar *code;
	const gchar *mnemo;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, user_data=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) user_data );

	if( OFO_IS_ACCOUNT( object )){
		if( prev_id && g_utf8_strlen( prev_id, -1 )){
			number = ofo_account_get_number( OFO_ACCOUNT( object ));
			if( g_utf8_collate( number, prev_id )){
				on_updated_object_account_number( dossier, prev_id, number );
			}
		}

	} else if( OFO_IS_CURRENCY( object )){
		if( prev_id && g_utf8_strlen( prev_id, -1 )){
			code = ofo_currency_get_code( OFO_CURRENCY( object ));
			if( g_utf8_collate( code, prev_id )){
				on_updated_object_currency_code( dossier, prev_id, code );
			}
		}

	} else if( OFO_IS_LEDGER( object )){
		if( prev_id && g_utf8_strlen( prev_id, -1 )){
			mnemo = ofo_ledger_get_mnemo( OFO_LEDGER( object ));
			if( g_utf8_collate( mnemo, prev_id )){
				on_updated_object_ledger_mnemo( dossier, prev_id, mnemo );
			}
		}

	} else if( OFO_IS_OPE_TEMPLATE( object )){
		if( prev_id && g_utf8_strlen( prev_id, -1 )){
			mnemo = ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object ));
			if( g_utf8_collate( mnemo, prev_id )){
				on_updated_object_model_mnemo( dossier, prev_id, mnemo );
			}
		}
	}
}

/*
 * an account number has been modified
 * all entries must be updated (even the unsettled or unreconciliated
 * from a previous exercice)
 */
static void
on_updated_object_account_number( const ofoDossier *dossier, const gchar *prev_id, const gchar *number )
{
	gchar *query;

	query = g_strdup_printf(
			"UPDATE OFA_T_ENTRIES "
			"	SET ENT_ACCOUNT='%s' WHERE ENT_ACCOUNT='%s' ", number, prev_id );

	ofa_dbms_query( ofo_dossier_get_dbms( dossier ), query, TRUE );
	g_free( query );
}

/*
 * a currency iso code has been modified (should be very rare)
 * all entries must be updated (even the unsettled or unreconciliated
 * from a previous exercice)
 */
static void
on_updated_object_currency_code( const ofoDossier *dossier, const gchar *prev_id, const gchar *code )
{
	gchar *query;

	query = g_strdup_printf(
			"UPDATE OFA_T_ENTRIES "
			"	SET ENT_CURRENCY='%s' WHERE ENT_CURRENCY='%s' ", code, prev_id );

	ofa_dbms_query( ofo_dossier_get_dbms( dossier ), query, TRUE );
	g_free( query );
}

/*
 * a ledger mnemonic has been modified
 * all entries must be updated (even the unsettled or unreconciliated
 * from a previous exercice)
 */
static void
on_updated_object_ledger_mnemo( const ofoDossier *dossier, const gchar *prev_id, const gchar *mnemo )
{
	gchar *query;

	query = g_strdup_printf(
			"UPDATE OFA_T_ENTRIES"
			"	SET ENT_LEDGER='%s' WHERE ENT_LEDGER='%s' ", mnemo, prev_id );

	ofa_dbms_query( ofo_dossier_get_dbms( dossier ), query, TRUE );
	g_free( query );
}

/*
 * an operation template mnemonic has been modified
 * all entries must be updated (even the unsettled or unreconciliated
 * from a previous exercice)
 */
static void
on_updated_object_model_mnemo( const ofoDossier *dossier, const gchar *prev_id, const gchar *mnemo )
{
	gchar *query;

	query = g_strdup_printf(
			"UPDATE OFA_T_ENTRIES"
			"	SET ENT_OPE_TEMPLATE='%s' WHERE ENT_OPE_TEMPLATE='%s' ", mnemo, prev_id );

	ofa_dbms_query( ofo_dossier_get_dbms( dossier ), query, TRUE );
	g_free( query );
}

/**
 * ofo_entry_new:
 */
ofoEntry *
ofo_entry_new( void )
{
	return( g_object_new( OFO_TYPE_ENTRY, NULL ));
}

/**
 * ofo_entry_get_dataset_by_concil:
 * @dossier: the dossier.
 * @account: the searched account number.
 * @mode: the conciliation status.
 *
 * Returns the dataset on the current exercice, for the given account,
 * with the given reconciliation status.
 *
 * The returned dataset only contains rough or validated entries; it
 * doesn't contain deleted entries.
 */
GList *
ofo_entry_get_dataset_by_concil( const ofoDossier *dossier, const gchar *account, ofaEntryConcil mode )
{
	GList *dataset;
	GString *where;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( account && g_utf8_strlen( account, -1 ), NULL );

	where = g_string_new( "" );
	g_string_append_printf( where, "ENT_ACCOUNT='%s' ", account );

	switch( mode ){
		case ENT_CONCILED_YES:
			g_string_append_printf( where,
				"	AND ENT_CONCIL_DVAL!=0" );
			break;
		case ENT_CONCILED_NO:
			g_string_append_printf( where,
				"	AND (ENT_CONCIL_DVAL=0 OR ENT_CONCIL_DVAL IS NULL)" );
			break;
		case ENT_CONCILED_ALL:
			break;
		default:
			break;
	}

	g_string_append_printf( where, " AND ENT_STATUS!=%d ", ENT_STATUS_DELETED );

	dataset = entry_load_dataset( ofo_dossier_get_dbms( dossier ), where->str );

	g_string_free( where, TRUE );

	return( dataset );
}

/**
 * ofo_entry_get_dataset_by_account:
 * @dossier: the dossier.
 * @account: the searched account number.
 * @from: an operation date.
 * @to: an operation date.
 *
 * Returns the dataset for the given account, with an operation date
 * between the @from et @to specified
 */
GList *
ofo_entry_get_dataset_by_account( const ofoDossier *dossier, const gchar *account, const GDate *from, const GDate *to )
{
	GString *where;
	gchar *str;
	GList *dataset;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( account && g_utf8_strlen( account, -1 ), NULL );

	where = g_string_new( "" );
	g_string_append_printf( where, "ENT_ACCOUNT='%s' ", account );

	if( my_date_is_valid( from )){
		str = my_date_to_str( from, MY_DATE_SQL );
		g_string_append_printf( where, "AND ENT_DOPE>='%s' ", str );
		g_free( str );
	}

	if( my_date_is_valid( to )){
		str = my_date_to_str( to, MY_DATE_SQL );
		g_string_append_printf( where, "AND ENT_DOPE<='%s' ", str );
		g_free( str );
	}

	dataset = entry_load_dataset( ofo_dossier_get_dbms( dossier ), where->str );

	g_string_free( where, TRUE );

	return( dataset );
}

/**
 * ofo_entry_get_dataset_by_ledger:
 * @dossier: the dossier.
 * @ledger: the ledger.
 * @from: an operation date.
 * @to: an operation date.
 *
 * Returns the dataset for the given ledger, with operation date between
 * @from and @to.
 */
GList *ofo_entry_get_dataset_by_ledger( const ofoDossier *dossier, const gchar *ledger, const GDate *from, const GDate *to )
{
	GString *where;
	gchar *str;
	GList *dataset;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( ledger && g_utf8_strlen( ledger, -1 ), NULL );

	where = g_string_new( "" );
	g_string_append_printf( where, "ENT_LEDGER='%s' ", ledger );

	if( my_date_is_valid( from )){
		str = my_date_to_str( from, MY_DATE_SQL );
		g_string_append_printf( where, "AND ENT_DOPE>='%s' ", str );
		g_free( str );
	}

	if( my_date_is_valid( to )){
		str = my_date_to_str( to, MY_DATE_SQL );
		g_string_append_printf( where, "AND ENT_DOPE<='%s' ", str );
		g_free( str );
	}

	dataset = entry_load_dataset( ofo_dossier_get_dbms( dossier ), where->str );

	g_string_free( where, TRUE );

	return( dataset );
}

/**
 * ofo_entry_get_dataset_for_print_balance:
 * @dossier: the current dossier.
 * @from_account: the starting account.
 * @to_account: the ending account.
 * @from_date: the starting effect date.
 * @to_date: the ending effect date.
 *
 * Returns the dataset of entries for the given accounts, between the
 * specified effect dates, as a GList of newly allocated
 * #ofsAccountBalance structures, that the user should
 *  ofo_account_free_balances().
 *
 * The returned dataset doesn't contain deleted entries.
 */
GList *
ofo_entry_get_dataset_for_print_balance( const ofoDossier *dossier,
											const gchar *from_account, const gchar *to_account,
											const GDate *from_date, const GDate *to_date )
{
	static const gchar *thisfn = "ofo_entry_get_dataset_for_print_balance";
	GList *dataset;
	GString *query;
	gboolean first;
	gchar *str;
	GSList *result, *irow, *icol;
	ofsAccountBalance *sbal;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	query = g_string_new(
				"SELECT ENT_ACCOUNT,ENT_CURRENCY,SUM(ENT_DEBIT),SUM(ENT_CREDIT) "
				"FROM OFA_T_ENTRIES WHERE " );
	first = FALSE;
	dataset = NULL;

	if( from_account && g_utf8_strlen( from_account, -1 )){
		g_string_append_printf( query, "ENT_ACCOUNT>='%s' ", from_account );
		first = TRUE;
	}
	if( to_account && g_utf8_strlen( to_account, -1 )){
		if( first ){
			query = g_string_append( query, "AND " );
		}
		g_string_append_printf( query, "ENT_ACCOUNT<='%s' ", to_account );
		first = TRUE;
	}
	if( my_date_is_valid( from_date )){
		if( first ){
			query = g_string_append( query, "AND " );
		}
		str = my_date_to_str( from_date, MY_DATE_SQL );
		g_string_append_printf( query, "ENT_DEFFECT>='%s' ", str );
		g_free( str );
		first = TRUE;
	}
	if( my_date_is_valid( to_date )){
		if( first ){
			query = g_string_append( query, "AND " );
		}
		str = my_date_to_str( to_date, MY_DATE_SQL );
		g_string_append_printf( query, "ENT_DEFFECT<='%s' ", str );
		g_free( str );
		first = TRUE;
	}
	if( first ){
		query = g_string_append( query, "AND " );
	}
	g_string_append_printf( query, "ENT_STATUS!=%u ", ENT_STATUS_DELETED );
	query = g_string_append( query, "GROUP BY ENT_ACCOUNT ORDER BY ENT_ACCOUNT ASC " );

	if( ofa_dbms_query_ex( ofo_dossier_get_dbms( dossier ), query->str, &result, TRUE )){
		for( irow=result ; irow ; irow=irow->next ){
			sbal = g_new0( ofsAccountBalance, 1 );
			icol = ( GSList * ) irow->data;
			sbal->account = g_strdup(( const gchar * ) icol->data );
			icol = icol->next;
			sbal->currency = g_strdup(( const gchar * ) icol->data );
			icol = icol->next;
			sbal->debit = my_double_set_from_sql(( const gchar * ) icol->data );
			icol = icol->next;
			sbal->credit = my_double_set_from_sql(( const gchar * ) icol->data );
			g_debug( "%s: account=%s, debit=%lf, credit=%lf",
					thisfn, sbal->account, sbal->debit, sbal->credit );
			dataset = g_list_prepend( dataset, sbal );
		}
		ofa_dbms_free_results( result );
	}
	g_string_free( query, TRUE );

	return( g_list_reverse( dataset ));
}

/**
 * ofo_entry_get_dataset_for_print_general_books:
 * @dossier: the current dossier.
 * @from_account: the starting account.
 * @to_account: the ending account.
 * @from_date: the starting effect date.
 * @to_date: the ending effect date.
 *
 * Returns the dataset of entries for the given accounts, between the
 * specified effect dates, as a GList of #ofoEntry, that the user
 * should g_list_free_full( list, ( GDestroyNotify ) g_object_unref ).
 *
 * All entries (validated or rough) are considered.
 *
 * The returned dataset doesn't contain deleted entries.
 */
GList *
ofo_entry_get_dataset_for_print_general_books( const ofoDossier *dossier,
												const gchar *from_account, const gchar *to_account,
												const GDate *from_date, const GDate *to_date )
{
	GList *dataset;
	GString *query;
	gboolean first;
	gchar *str;
	GSList *result, *irow;
	ofoEntry *entry;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	query = g_string_new( "" );
	g_string_append_printf( query, "SELECT %s FROM OFA_T_ENTRIES WHERE ", entry_list_columns());
	first = FALSE;
	dataset = NULL;

	if( from_account && g_utf8_strlen( from_account, -1 )){
		g_string_append_printf( query, "ENT_ACCOUNT>='%s' ", from_account );
		first = TRUE;
	}
	if( to_account && g_utf8_strlen( to_account, -1 )){
		if( first ){
			query = g_string_append( query, "AND " );
		}
		g_string_append_printf( query, "ENT_ACCOUNT<='%s' ", to_account );
		first = TRUE;
	}
	if( my_date_is_valid( from_date )){
		if( first ){
			query = g_string_append( query, "AND " );
		}
		str = my_date_to_str( from_date, MY_DATE_SQL );
		g_string_append_printf( query, "ENT_DEFFECT>='%s' ", str );
		g_free( str );
		first = TRUE;
	}
	if( my_date_is_valid( to_date )){
		if( first ){
			query = g_string_append( query, "AND " );
		}
		str = my_date_to_str( to_date, MY_DATE_SQL );
		g_string_append_printf( query, "ENT_DEFFECT<='%s' ", str );
		g_free( str );
		first = TRUE;
	}
	if( first ){
		query = g_string_append( query, "AND " );
	}
	g_string_append_printf( query, "ENT_STATUS!=%u ", ENT_STATUS_DELETED );

	query = g_string_append( query, "ORDER BY "
			"ENT_ACCOUNT ASC, ENT_DOPE ASC, ENT_DEFFECT ASC, ENT_NUMBER ASC " );

	if( ofa_dbms_query_ex( ofo_dossier_get_dbms( dossier ), query->str, &result, TRUE )){
		for( irow=result ; irow ; irow=irow->next ){
			entry = entry_parse_result( irow );
			if( entry ){
				dataset = g_list_prepend( dataset, entry );
			}
		}
		ofa_dbms_free_results( result );
	}
	g_string_free( query, TRUE );

	return( g_list_reverse( dataset ));
}

/**
 * ofo_entry_get_dataset_for_print_ledgers:
 * @dossier: the current dossier.
 * @mnemos: a list of requested ledger mnemos.
 * @from_date: the starting effect date.
 * @to_date: the ending effect date.
 *
 * Returns the dataset of entries for the given ledgers, between the
 * specified effect dates, as a GList of #ofoEntry, that the user
 * should g_list_free_full( list, ( GDestroyNotify ) g_object_unref ).
 *
 * The returned dataset doesn't contain deleted entries.
 */
GList *
ofo_entry_get_dataset_for_print_ledgers( const ofoDossier *dossier,
												const GSList *mnemos,
												const GDate *from_date, const GDate *to_date )
{
	GList *dataset;
	GString *query;
	gboolean first;
	gchar *str;
	const GSList *it;
	GSList *result, *irow;
	ofoEntry *entry;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	query = g_string_new( "" );
	g_string_append_printf( query, "SELECT %s FROM OFA_T_ENTRIES WHERE ", entry_list_columns());
	dataset = NULL;

	/* (ENT_LEDGER=xxxx or ENT_LEDGER=xxx or ENT_LEDGER=xxx) */
	query = g_string_append_c( query, '(' );
	for( it=mnemos, first=TRUE ; it ; it=it->next ){
		if( !first ){
			query = g_string_append( query, "OR " );
		}
		g_string_append_printf( query, "ENT_LEDGER='%s' ", ( const gchar * ) it->data );
		first = FALSE;
	}
	query = g_string_append( query, ") " );
	if( my_date_is_valid( from_date )){
		str = my_date_to_str( from_date, MY_DATE_SQL );
		g_string_append_printf( query, "AND ENT_DEFFECT>='%s' ", str );
		g_free( str );
	}
	if( my_date_is_valid( to_date )){
		str = my_date_to_str( to_date, MY_DATE_SQL );
		g_string_append_printf( query, "AND ENT_DEFFECT<='%s' ", str );
		g_free( str );
	}
	g_string_append_printf( query, "AND ENT_STATUS!=%u ", ENT_STATUS_DELETED );

	query = g_string_append( query, "ORDER BY "
			"ENT_LEDGER ASC, ENT_DOPE ASC, ENT_DEFFECT ASC, ENT_NUMBER ASC " );

	if( ofa_dbms_query_ex( ofo_dossier_get_dbms( dossier ), query->str, &result, TRUE )){
		for( irow=result ; irow ; irow=irow->next ){
			entry = entry_parse_result( irow );
			if( entry ){
				dataset = g_list_prepend( dataset, entry );
			}
		}
		ofa_dbms_free_results( result );
	}
	g_string_free( query, TRUE );

	return( g_list_reverse( dataset ));
}

/**
 * ofo_entry_get_dataset_for_print_reconcil:
 * @dossier: the current dossier.
 * @account: the reconciliated account.
 * @date: the greatest effect date to be returned.
 *
 * Returns the dataset of unreconciliated entries for the given account,
 * until the specified effect date.
 *
 * The returned dataset doesn't contain deleted entries.
 */
GList *
ofo_entry_get_dataset_for_print_reconcil( const ofoDossier *dossier,
											const gchar *account, const GDate *date )
{
	GList *dataset;
	GString *where;
	gchar *str;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( account && g_utf8_strlen( account, -1 ), NULL );

	where = g_string_new( "" );
	g_string_append_printf( where, "ENT_ACCOUNT='%s' ", account );
	g_string_append_printf( where, "AND ENT_CONCIL_DVAL IS NULL " );

	if( my_date_is_valid( date )){
		str = my_date_to_str( date, MY_DATE_SQL );
		g_string_append_printf( where, "AND ENT_DEFFECT <= '%s'", str );
		g_free( str );
	}

	g_string_append_printf( where, " AND ENT_STATUS!=%u ", ENT_STATUS_DELETED );

	dataset = entry_load_dataset( ofo_dossier_get_dbms( dossier ), where->str );

	g_string_free( where, TRUE );

	return( dataset );
}

/**
 * ofo_entry_get_dataset_remaining_for_val:
 * @dossier: the current dossier.
 *
 * Returns the dataset of rough remaining entries on the exercice.
 */
GList *
ofo_entry_get_dataset_remaining_for_val( const ofoDossier *dossier )
{
	GList *dataset;
	GString *where;
	gchar *str;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	where = g_string_new( "" );

	str = effect_in_exercice( dossier);
	g_string_append_printf( where, "%s AND ENT_STATUS=%u ", str, ENT_STATUS_ROUGH );
	g_free( str );

	dataset = entry_load_dataset( ofo_dossier_get_dbms( dossier ), where->str );

	g_string_free( where, TRUE );

	return( dataset );
}

/**
 * ofo_entry_get_unreconciliated:
 * @dossier: the current dossier.
 *
 * Returns the dataset of unreconciliated entries to be renew in the
 * next exercice.
 */
GList *
ofo_entry_get_unreconciliated( const ofoDossier *dossier )
{
	GList *dataset;
	GString *query;
	gchar *str;
	GSList *result, *irow;
	ofoEntry *entry;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	dataset = NULL;
	query = g_string_new( "" );

	str = effect_in_exercice( dossier);
	g_string_append_printf( query,
			"SELECT %s FROM OFA_T_ENTRIES,OFA_T_ACCOUNTS "
			"	WHERE %s "
			"		AND ENT_CONCIL_DVAL IS NULL "
			"		AND ENT_ACCOUNT=ACC_NUMBER "
			"		AND ACC_RECONCILIABLE='%s' ", entry_list_columns(), str, ACCOUNT_RECONCILIABLE );
	g_free( str );

	if( ofa_dbms_query_ex( ofo_dossier_get_dbms( dossier ), query->str, &result, TRUE )){
		for( irow=result ; irow ; irow=irow->next ){
			entry = entry_parse_result( irow );
			if( entry ){
				dataset = g_list_prepend( dataset, entry );
			}
		}
		ofa_dbms_free_results( result );
	}
	g_string_free( query, TRUE );

	return( dataset );
}

/**
 * ofo_entry_get_unsettled:
 * @dossier: the current dossier.
 *
 * Returns the dataset of unsettled entries to be renew in the next
 * exercice.
 */
GList *
ofo_entry_get_unsettled( const ofoDossier *dossier )
{
	GList *dataset;
	GString *query;
	gchar *str;
	GSList *result, *irow;
	ofoEntry *entry;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	dataset = NULL;
	query = g_string_new( "" );

	str = effect_in_exercice( dossier);
	g_string_append_printf( query,
			"SELECT %s FROM OFA_T_ENTRIES,OFA_T_ACCOUNTS "
			"	WHERE %s "
			"		AND (ENT_STLMT_NUMBER=0 OR ENT_STLMT_NUMBER IS NULL) "
			"		AND ENT_ACCOUNT=ACC_NUMBER "
			"		AND ACC_SETTLEABLE='%s' ", entry_list_columns(), str, ACCOUNT_SETTLEABLE );
	g_free( str );

	if( ofa_dbms_query_ex( ofo_dossier_get_dbms( dossier ), query->str, &result, TRUE )){
		for( irow=result ; irow ; irow=irow->next ){
			entry = entry_parse_result( irow );
			if( entry ){
				dataset = g_list_prepend( dataset, entry );
			}
		}
		ofa_dbms_free_results( result );
	}
	g_string_free( query, TRUE );

	return( dataset );
}

/*
 * build a where string for the exercice on the effect date
 */
static gchar *
effect_in_exercice( const ofoDossier *dossier )
{
	GString *where;
	const GDate *begin, *end;
	gchar *str;

	where = g_string_new( "" );

	begin = ofo_dossier_get_exe_begin( dossier );
	g_return_val_if_fail( my_date_is_valid( begin ), NULL );
	str = my_date_to_str( begin, MY_DATE_SQL );
	g_string_append_printf( where, "ENT_DEFFECT>='%s' ", str );
	g_free( str );

	end = ofo_dossier_get_exe_end( dossier );
	g_return_val_if_fail( my_date_is_valid( end ), NULL );
	str = my_date_to_str( end, MY_DATE_SQL );
	g_string_append_printf( where, " AND ENT_DEFFECT<='%s' ", str );
	g_free( str );

	return( g_string_free( where, FALSE ));
}

/*
 * returns a GList * of ofoEntries
 */
static GList *
entry_load_dataset( const ofaDbms *dbms, const gchar *where )
{
	GList *dataset;
	GString *query;
	GSList *result, *irow;
	ofoEntry *entry;

	dataset = NULL;
	query = g_string_new( "" );

	g_string_append_printf( query,
					"SELECT %s FROM OFA_T_ENTRIES ",
							entry_list_columns());

	if( where && g_utf8_strlen( where, -1 )){
		g_string_append_printf( query, "WHERE %s ", where );
	}

	query = g_string_append( query,
					"ORDER BY ENT_DOPE ASC,ENT_DEFFECT ASC,ENT_NUMBER ASC" );

	if( ofa_dbms_query_ex( dbms, query->str, &result, TRUE )){
		for( irow=result ; irow ; irow=irow->next ){
			entry = entry_parse_result( irow );
			if( entry ){
				dataset = g_list_prepend( dataset, entry );
			}
		}
		ofa_dbms_free_results( result );
	}
	g_string_free( query, TRUE );

	return( g_list_reverse( dataset ));
}

static const gchar *
entry_list_columns( void )
{
	return( "ENT_DOPE,ENT_DEFFECT,ENT_NUMBER,ENT_LABEL,ENT_REF,"
			"	ENT_ACCOUNT,"
			"	ENT_CURRENCY,ENT_LEDGER,ENT_OPE_TEMPLATE,"
			"	ENT_DEBIT,ENT_CREDIT,"
			"	ENT_STATUS,ENT_UPD_USER,ENT_UPD_STAMP,"
			"	ENT_CONCIL_DVAL,ENT_CONCIL_USER,ENT_CONCIL_STAMP,"
			"	ENT_STLMT_NUMBER,ENT_STLMT_USER,ENT_STLMT_STAMP " );
}

static ofoEntry *
entry_parse_result( const GSList *row )
{
	GSList *icol;
	ofoEntry *entry;
	GTimeVal timeval;

	entry = NULL;

	if( row ){
		icol = ( GSList * ) row->data;
		entry = ofo_entry_new();
		my_date_set_from_sql( &entry->priv->dope, ( const gchar * ) icol->data );
		icol = icol->next;
		my_date_set_from_sql( &entry->priv->deffect, ( const gchar * ) icol->data );
		icol = icol->next;
		ofo_entry_set_number( entry, atol(( gchar * ) icol->data ));
		icol = icol->next;
		ofo_entry_set_label( entry, ( gchar * ) icol->data );
		icol = icol->next;
		if( icol->data ){
			ofo_entry_set_ref( entry, ( gchar * ) icol->data );
		}
		icol = icol->next;
		ofo_entry_set_account( entry, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_entry_set_currency( entry, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_entry_set_ledger( entry, ( gchar * ) icol->data );
		icol = icol->next;
		if( icol->data ){
			ofo_entry_set_ope_template( entry, ( gchar * ) icol->data );
		}
		icol = icol->next;
		ofo_entry_set_debit( entry,
				my_double_set_from_sql(( const gchar * ) icol->data ));
		icol = icol->next;
		ofo_entry_set_credit( entry,
				my_double_set_from_sql(( const gchar * ) icol->data ));
		icol = icol->next;
		ofo_entry_set_status( entry, atoi(( gchar * ) icol->data ));
		icol = icol->next;
		entry_set_upd_user( entry, ( gchar * ) icol->data );
		icol = icol->next;
		entry_set_upd_stamp( entry,
				my_utils_stamp_set_from_sql( &timeval, ( const gchar * ) icol->data ));
		icol = icol->next;
		if( icol->data ){
			my_date_set_from_sql( &entry->priv->concil_dval, ( const gchar * ) icol->data );
		}
		icol = icol->next;
		if( icol->data ){
			ofo_entry_set_concil_user( entry, ( gchar * ) icol->data );
		}
		icol = icol->next;
		if( icol->data ){
			ofo_entry_set_concil_stamp( entry,
					my_utils_stamp_set_from_sql( &timeval, ( const gchar * ) icol->data ));
		}
		icol = icol->next;
		if( icol->data ){
			entry->priv->stlmt_number = atol(( const gchar * ) icol->data );
		}
		icol = icol->next;
		if( icol->data ){
			entry_set_settlement_user( entry, ( gchar * ) icol->data );
		}
		icol = icol->next;
		if( icol->data ){
			entry_set_settlement_stamp( entry,
					my_utils_stamp_set_from_sql( &timeval, ( const gchar * ) icol->data ));
		}
	}
	return( entry );
}

/**
 * ofo_entry_free_dataset:
 */
void
ofo_entry_free_dataset( GList *dataset )
{
	static const gchar *thisfn = "ofo_entry_free_dataset";

	g_debug( "%s: dataset=%p, count=%d", thisfn, ( void * ) dataset, g_list_length( dataset ));

	g_list_free_full( dataset, g_object_unref );
}

/**
 * ofo_entry_use_currency:
 *
 * Returns: %TRUE if a recorded entry makes use of the specified currency.
 */
gboolean
ofo_entry_use_currency( const ofoDossier *dossier, const gchar *currency )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	return( entry_count_for_currency( ofo_dossier_get_dbms( dossier ), currency ) > 0 );
}

static gint
entry_count_for_currency( const ofaDbms *dbms, const gchar *currency )
{
	return( entry_count_for( dbms, "ENT_CURRENCY", currency ));
}

/**
 * ofo_entry_use_ledger:
 *
 * Returns: %TRUE if a recorded entry makes use of the specified ledger.
 */
gboolean
ofo_entry_use_ledger( const ofoDossier *dossier, const gchar *ledger )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	return( entry_count_for_ledger( ofo_dossier_get_dbms( dossier ), ledger ) > 0 );
}

static gint
entry_count_for_ledger( const ofaDbms *dbms, const gchar *ledger )
{
	return( entry_count_for( dbms, "ENT_LEDGER", ledger ));
}

/**
 * ofo_entry_use_ope_template:
 *
 * Returns: %TRUE if a recorded entry makes use of the specified
 * operation template.
 */
gboolean
ofo_entry_use_ope_template( const ofoDossier *dossier, const gchar *model )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	return( entry_count_for_ope_template( ofo_dossier_get_dbms( dossier ), model ) > 0 );
}

static gint
entry_count_for_ope_template( const ofaDbms *dbms, const gchar *model )
{
	return( entry_count_for( dbms, "ENT_OPE_TEMPLATE", model ));
}

static gint
entry_count_for( const ofaDbms *dbms, const gchar *field, const gchar *mnemo )
{
	gint count;
	gchar *query;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_ENTRIES WHERE %s='%s'", field, mnemo );

	ofa_dbms_query_int( dbms, query, &count, TRUE );

	g_free( query );

	return( count );
}

/**
 * ofo_entry_get_number:
 */
ofxCounter
ofo_entry_get_number( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), OFO_BASE_UNSET_ID );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return( entry->priv->number );
	}

	return( OFO_BASE_UNSET_ID );
}

/**
 * ofo_entry_get_label:
 */
const gchar *
ofo_entry_get_label( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return( entry->priv->label );
	}

	return( NULL );
}

/**
 * ofo_entry_get_deffect:
 */
const GDate *
ofo_entry_get_deffect( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return(( const GDate * ) &entry->priv->deffect );
	}

	return( NULL );
}

/**
 * ofo_entry_get_dope:
 */
const GDate *
ofo_entry_get_dope( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return(( const GDate * ) &entry->priv->dope );
	}

	return( NULL );
}

/**
 * ofo_entry_get_ref:
 */
const gchar *
ofo_entry_get_ref( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return( entry->priv->ref );
	}

	return( NULL );
}

/**
 * ofo_entry_get_account:
 */
const gchar *
ofo_entry_get_account( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return( entry->priv->account );
	}

	return( NULL );
}

/**
 * ofo_entry_get_currency:
 */
const gchar *
ofo_entry_get_currency( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return(( const gchar * ) entry->priv->currency );
	}

	return( NULL );
}

/**
 * ofo_entry_get_ledger:
 */
const gchar *
ofo_entry_get_ledger( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return(( const gchar * ) entry->priv->ledger );
	}

	return( NULL );
}

/**
 * ofo_entry_get_ope_template:
 */
const gchar *
ofo_entry_get_ope_template( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return(( const gchar * ) entry->priv->model );
	}

	return( NULL );
}

/**
 * ofo_entry_get_debit:
 */
ofxAmount
ofo_entry_get_debit( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), 0.0 );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return( entry->priv->debit );
	}

	return( 0.0 );
}

/**
 * ofo_entry_get_credit:
 */
ofxAmount
ofo_entry_get_credit( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), 0.0 );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return( entry->priv->credit );
	}

	return( 0.0 );
}

/**
 * ofo_entry_get_status:
 */
ofaEntryStatus
ofo_entry_get_status( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), OFO_BASE_UNSET_ID );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return( entry->priv->status );
	}

	return( OFO_BASE_UNSET_ID );
}

/**
 * ofo_entry_get_abr_status:
 *
 * Returns an abbreviated localized string for the status.
 * Use case: view entries.
 */
const gchar *
ofo_entry_get_abr_status( const ofoEntry *entry )
{
	ofaEntryStatus status;
	gint i;

	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		status = ofo_entry_get_status( entry );
		for( i=0 ; st_status[i].num ; ++i ){
			if( st_status[i].num == status ){
				return( gettext( st_status[i].str ));
			}
		}
	}

	return( NULL );
}

/**
 * ofo_entry_get_status_from_abr:
 *
 * Returns an abbreviated localized string for the status.
 * Use case: view entries.
 */
ofaEntryStatus
ofo_entry_get_status_from_abr( const gchar *abr_status )
{
	gint i;

	g_return_val_if_fail( abr_status && g_utf8_strlen( abr_status, -1 ), ENT_STATUS_ROUGH );

	for( i=0 ; st_status[i].num ; ++i ){
		if( !g_utf8_collate( st_status[i].str, abr_status )){
			return( st_status[i].num );
		}
	}

	return( ENT_STATUS_ROUGH );
}

/**
 * ofo_entry_get_upd_user:
 */
const gchar *
ofo_entry_get_upd_user( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return(( const gchar * ) entry->priv->upd_user );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_entry_get_upd_stamp:
 */
const GTimeVal *
ofo_entry_get_upd_stamp( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &entry->priv->upd_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_entry_get_concil_dval:
 *
 * The returned #GDate object is never %NULL, but may be invalid.
 */
const GDate *
ofo_entry_get_concil_dval( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return(( const GDate * ) &entry->priv->concil_dval );
	}

	return( NULL );
}

/**
 * ofo_entry_get_concil_user:
 */
const gchar *
ofo_entry_get_concil_user( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return(( const gchar * ) entry->priv->concil_user );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_entry_get_concil_stamp:
 */
const GTimeVal *
ofo_entry_get_concil_stamp( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &entry->priv->concil_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_entry_get_settlement_number:
 */
ofxCounter
ofo_entry_get_settlement_number( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), -1 );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return( entry->priv->stlmt_number );
	}

	g_assert_not_reached();
	return( -1 );
}

/**
 * ofo_entry_get_settlement_user:
 */
const gchar *
ofo_entry_get_settlement_user( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return(( const gchar * ) entry->priv->stlmt_user );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_entry_get_settlement_stamp:
 */
const GTimeVal *
ofo_entry_get_settlement_stamp( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &entry->priv->stlmt_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_entry_set_number:
 */
void
ofo_entry_set_number( ofoEntry *entry, ofxCounter number )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));
	g_return_if_fail( number > 0 );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		entry->priv->number = number;
	}
}

/**
 * ofo_entry_set_label:
 */
void
ofo_entry_set_label( ofoEntry *entry, const gchar *label )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));
	g_return_if_fail( label && g_utf8_strlen( label, -1 ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		g_free( entry->priv->label );
		entry->priv->label = g_strdup( label );
	}
}

/**
 * ofo_entry_set_deffect:
 */
void
ofo_entry_set_deffect( ofoEntry *entry, const GDate *deffect )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));
	g_return_if_fail( my_date_is_valid( deffect ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		my_date_set_from_date( &entry->priv->deffect, deffect );
	}
}

/**
 * ofo_entry_set_dope:
 */
void
ofo_entry_set_dope( ofoEntry *entry, const GDate *dope )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));
	g_return_if_fail( my_date_is_valid( dope ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		my_date_set_from_date( &entry->priv->dope, dope );
	}
}

/**
 * ofo_entry_set_ref:
 */
void
ofo_entry_set_ref( ofoEntry *entry, const gchar *ref )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		g_free( entry->priv->ref );
		entry->priv->ref = g_strdup( ref );
	}
}

/**
 * ofo_entry_set_account:
 */
void
ofo_entry_set_account( ofoEntry *entry, const gchar *account )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));
	g_return_if_fail( account && g_utf8_strlen( account, -1 ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		g_free( entry->priv->account );
		entry->priv->account = g_strdup( account );
	}
}

/**
 * ofo_entry_set_currency:
 */
void
ofo_entry_set_currency( ofoEntry *entry, const gchar *currency )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));
	g_return_if_fail( currency > 0 );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		g_free( entry->priv->currency );
		entry->priv->currency = g_strdup( currency );
	}
}

/**
 * ofo_entry_set_ledger:
 */
void
ofo_entry_set_ledger( ofoEntry *entry, const gchar *ledger )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));
	g_return_if_fail( ledger && g_utf8_strlen( ledger, -1 ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		g_free( entry->priv->ledger );
		entry->priv->ledger = g_strdup( ledger );
	}
}

/**
 * ofo_entry_set_ope_template:
 */
void
ofo_entry_set_ope_template( ofoEntry *entry, const gchar *model )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));
	g_return_if_fail( model && g_utf8_strlen( model, -1 ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		g_free( entry->priv->model );
		entry->priv->model = g_strdup( model );
	}
}

/**
 * ofo_entry_set_debit:
 */
void
ofo_entry_set_debit( ofoEntry *entry, ofxAmount debit )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		entry->priv->debit = debit;
	}
}

/**
 * ofo_entry_set_credit:
 */
void
ofo_entry_set_credit( ofoEntry *entry, ofxAmount credit )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		entry->priv->credit = credit;
	}
}

/**
 * ofo_entry_set_status:
 */
void
ofo_entry_set_status( ofoEntry *entry, ofaEntryStatus status )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));
	g_return_if_fail( status > 0 );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		entry->priv->status = status;
	}
}

/*
 * ofo_entry_set_upd_user:
 */
static void
entry_set_upd_user( ofoEntry *entry, const gchar *upd_user )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		g_free( entry->priv->upd_user );
		entry->priv->upd_user = g_strdup( upd_user );
	}
}

/*
 * ofo_entry_set_upd_stamp:
 */
static void
entry_set_upd_stamp( ofoEntry *entry, const GTimeVal *upd_stamp )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		my_utils_stamp_set_from_stamp( &entry->priv->upd_stamp, upd_stamp );
	}
}

/**
 * ofo_entry_set_concil_dval:
 *
 * The reconciliation may be unset by setting @drappro to %NULL.
 */
void
ofo_entry_set_concil_dval( ofoEntry *entry, const GDate *drappro )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		my_date_set_from_date( &entry->priv->concil_dval, drappro );
	}
}

/**
 * ofo_entry_set_concil_user:
 */
void
ofo_entry_set_concil_user( ofoEntry *entry, const gchar *user )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		g_free( entry->priv->concil_user );
		entry->priv->concil_user = g_strdup( user );
	}
}

/**
 * ofo_entry_set_concil_stamp:
 */
void
ofo_entry_set_concil_stamp( ofoEntry *entry, const GTimeVal *stamp )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		my_utils_stamp_set_from_stamp( &entry->priv->concil_stamp, stamp );
	}
}

/*
 * ofo_entry_set_settlement_number:
 *
 * The reconciliation may be unset by setting @number to 0.
 */
static void
entry_set_settlement_number( ofoEntry *entry, ofxCounter number )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		entry->priv->stlmt_number = number;
	}
}

/*
 * ofo_entry_set_settlement_user:
 */
static void
entry_set_settlement_user( ofoEntry *entry, const gchar *user )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		g_free( entry->priv->stlmt_user );
		entry->priv->stlmt_user = g_strdup( user );
	}
}

/*
 * ofo_entry_set_settlement_stamp:
 */
static void
entry_set_settlement_stamp( ofoEntry *entry, const GTimeVal *stamp )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		my_utils_stamp_set_from_stamp( &entry->priv->stlmt_stamp, stamp );
	}
}

/**
 * ofo_entry_is_valid:
 */
gboolean
ofo_entry_is_valid( ofoDossier *dossier,
							const GDate *deffect, const GDate *dope, const gchar *label,
							const gchar *account, const gchar *currency, const gchar *ledger,
							const gchar *model, ofxAmount debit, ofxAmount credit )
{
	ofoAccount *account_obj;
	gboolean ok;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	ok = TRUE;

	if( !ledger || !g_utf8_strlen( ledger, -1 ) || !ofo_ledger_get_by_mnemo( dossier, ledger )){
		error_ledger( ledger );
		ok &= FALSE;
	}
	if( !model || !g_utf8_strlen( model, -1 ) || !ofo_ope_template_get_by_mnemo( dossier, model )){
		error_ope_template( model );
		ok &= FALSE;
	}
	if( !currency || !g_utf8_strlen( currency, -1 ) || !ofo_currency_get_by_code( dossier, currency )){
		error_currency( currency );
		ok &= FALSE;
	}
	if( !account || !g_utf8_strlen( account, -1 )){
		error_acc_number();
		ok &= FALSE;
	} else {
		account_obj = ofo_account_get_by_number( dossier, account );
		if( !account_obj ){
			error_account( account );
			ok &= FALSE;

		} else if( g_utf8_collate( currency, ofo_account_get_currency( account_obj ))){
			error_acc_currency( dossier, currency, account_obj );
			ok &= FALSE;
		}
	}
	if(( debit && credit ) || ( !debit && !credit )){
		error_amounts( debit, credit );
		ok &= FALSE;
	}

	return( ok );
}

/**
 * ofo_entry_new_with_data:
 *
 * Create a new entry with the provided data.
 * The entry is - at this time - unnumbered and does not have sent any
 * advertising message. For the moment, this is only a 'project' of
 * entry...
 *
 * Returns: the #ofoEntry entry object, of %NULL in case of an error.
 */
ofoEntry *
ofo_entry_new_with_data( ofoDossier *dossier,
							const GDate *deffect, const GDate *dope, const gchar *label,
							const gchar *ref, const gchar *account,
							const gchar *currency, const gchar *ledger,
							const gchar *model, ofxAmount debit, ofxAmount credit )
{
	ofoEntry *entry;

	if( !ofo_entry_is_valid( dossier, deffect, dope, label, account, currency, ledger, model, debit, credit )){
		return( NULL );
	}

	entry = g_object_new( OFO_TYPE_ENTRY, NULL );

	my_date_set_from_date( &entry->priv->deffect, deffect );
	my_date_set_from_date( &entry->priv->dope, dope );
	entry->priv->label = g_strdup( label );
	entry->priv->ref = g_strdup( ref );
	entry->priv->account = g_strdup( account );
	entry->priv->currency = g_strdup( currency );
	entry->priv->ledger = g_strdup( ledger );
	entry->priv->model = g_strdup( model );
	entry->priv->debit = debit;
	entry->priv->credit = credit;
	entry->priv->status = ENT_STATUS_ROUGH;

	return( entry );
}

/**
 * ofo_entry_insert:
 *
 * Allocates a sequential number to the entry, and records it in the
 * dbms. Send the corresponding advertising messages if no error occurs.
 */
gboolean
ofo_entry_insert( ofoEntry *entry, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_entry_insert";
	gboolean ok;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	g_debug( "%s: entry=%p, dossier=%p", thisfn, ( void * ) entry, ( void * ) dossier );

	ok = FALSE;

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		entry->priv->number = ofo_dossier_get_next_entry( dossier );

		if( entry_do_insert( entry,
					ofo_dossier_get_dbms( dossier ),
					ofo_dossier_get_user( dossier ))){

			g_signal_emit_by_name( G_OBJECT( dossier ), SIGNAL_DOSSIER_NEW_OBJECT, g_object_ref( entry ));

			ok = TRUE;
		}
	}

	return( ok );
}

static gboolean
entry_do_insert( ofoEntry *entry, const ofaDbms *dbms, const gchar *user )
{
	GString *query;
	gchar *label, *ref;
	gchar *sdeff, *sdope, *sdebit, *scredit;
	gboolean ok;
	GTimeVal stamp;
	gchar *stamp_str;
	const gchar *model;

	g_return_val_if_fail( OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( OFA_IS_DBMS( dbms ), FALSE );

	label = my_utils_quote( ofo_entry_get_label( entry ));
	ref = my_utils_quote( ofo_entry_get_ref( entry ));
	sdeff = my_date_to_str( ofo_entry_get_deffect( entry ), MY_DATE_SQL );
	sdope = my_date_to_str( ofo_entry_get_dope( entry ), MY_DATE_SQL );
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_ENTRIES " );

	g_string_append_printf( query,
			"	(ENT_DEFFECT,ENT_NUMBER,ENT_DOPE,ENT_LABEL,ENT_REF,ENT_ACCOUNT,"
			"	ENT_CURRENCY,ENT_LEDGER,ENT_OPE_TEMPLATE,"
			"	ENT_DEBIT,ENT_CREDIT,ENT_STATUS,"
			"	ENT_UPD_USER, ENT_UPD_STAMP) "
			"	VALUES ('%s',%ld,'%s','%s',",
			sdeff,
			ofo_entry_get_number( entry ),
			sdope,
			label );

	if( ref && g_utf8_strlen( ref, -1 )){
		g_string_append_printf( query, "'%s',", ref );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
				"'%s','%s','%s',",
				ofo_entry_get_account( entry ),
				ofo_entry_get_currency( entry ),
				ofo_entry_get_ledger( entry ));

	model = ofo_entry_get_ope_template( entry );
	if( model && g_utf8_strlen( model, -1 )){
		g_string_append_printf( query, "'%s',", model );
	} else {
		query = g_string_append( query, "NULL," );
	}

	sdebit = my_double_to_sql( ofo_entry_get_debit( entry ));
	scredit = my_double_to_sql( ofo_entry_get_credit( entry ));

	g_string_append_printf( query,
				"%s,%s,%d,'%s','%s')",
				sdebit,
				scredit,
				ofo_entry_get_status( entry ),
				user,
				stamp_str );

	if( ofa_dbms_query( dbms, query->str, TRUE )){

		entry_set_upd_user( entry, user );
		entry_set_upd_stamp( entry, &stamp );

		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( sdebit );
	g_free( scredit );
	g_free( sdeff );
	g_free( sdope );
	g_free( ref );
	g_free( label );
	g_free( stamp_str );

	return( ok );
}

static void
error_ledger( const gchar *ledger )
{
	gchar *str;

	str = g_strdup_printf( _( "Invalid ledger identifier: %s" ), ledger );
	error_entry( str );

	g_free( str );
}

static void
error_ope_template( const gchar *model )
{
	gchar *str;

	str = g_strdup_printf( _( "Invalid operation template identifier: %s" ), model );
	error_entry( str );

	g_free( str );
}

static void
error_currency( const gchar *currency )
{
	gchar *str;

	str = g_strdup_printf( _( "Invalid currency ISO 3A code: %s" ), currency );
	error_entry( str );

	g_free( str );
}

static void
error_acc_number( void )
{
	gchar *str;

	str = g_strdup( _( "Empty account number" ));
	error_entry( str );

	g_free( str );
}

static void
error_account( const gchar *number )
{
	gchar *str;

	str = g_strdup_printf( _( "Invalid account number: %s" ), number );
	error_entry( str );

	g_free( str );
}

static void
error_acc_currency( ofoDossier *dossier, const gchar *currency, ofoAccount *account )
{
	gchar *str;
	const gchar *acc_currency;
	ofoCurrency *acc_dev, *ent_dev;

	acc_currency = ofo_account_get_currency( account );
	acc_dev = ofo_currency_get_by_code( dossier, acc_currency );
	ent_dev = ofo_currency_get_by_code( dossier, currency );

	if( !acc_dev ){
		str = g_strdup_printf( "Invalid currency '%s' for the account '%s'",
					acc_currency, ofo_account_get_number( account ));
	} else if( !ent_dev ){
		str = g_strdup_printf( "Candidate entry makes use of invalid '%s' currency", currency );
	} else {
		str = g_strdup_printf( _( "Account %s is configured for accepting %s currency. "
				"But the candidate entry makes use of %s" ),
					ofo_account_get_number( account ),
					acc_currency,
					currency );
	}
	error_entry( str );

	g_free( str );
}

static void
error_amounts( ofxAmount debit, ofxAmount credit )
{
	gchar *str;

	str = g_strdup_printf(
					"Invalid amounts: debit=%.lf, credit=%.lf: one and only one must be non zero",
					debit, credit );

	error_entry( str );

	g_free( str );
}

static void
error_entry( const gchar *message )
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(
			NULL,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_CLOSE,
			"%s", message );

	gtk_dialog_run( GTK_DIALOG( dialog ));

	gtk_widget_destroy( dialog );
}

/**
 * ofo_entry_update:
 *
 * Update a rough entry.
 */
gboolean
ofo_entry_update( ofoEntry *entry, const ofoDossier *dossier )
{
	gboolean ok;

	ok = FALSE;

	if( entry_do_update( entry,
				ofo_dossier_get_dbms( dossier ),
				ofo_dossier_get_user( dossier ))){

		g_signal_emit_by_name(
				G_OBJECT( dossier ),
				SIGNAL_DOSSIER_UPDATED_OBJECT, g_object_ref( entry ), NULL );

		ok = TRUE;
	}

	return( ok );
}

static gboolean
entry_do_update( ofoEntry *entry, const ofaDbms *dbms, const gchar *user )
{
	GString *query;
	gchar *sdeff, *sdope, *sdeb, *scre;
	gchar *stamp_str;
	GTimeVal stamp;
	gboolean ok;
	const gchar *model;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( dbms && OFA_IS_DBMS( dbms ), FALSE );

	sdope = my_date_to_str( ofo_entry_get_dope( entry ), MY_DATE_SQL );
	sdeff = my_date_to_str( ofo_entry_get_deffect( entry ), MY_DATE_SQL );
	sdeb = my_double_to_sql( ofo_entry_get_debit( entry ));
	scre = my_double_to_sql( ofo_entry_get_credit( entry ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_ENTRIES " );

	g_string_append_printf( query,
			"	SET ENT_DEFFECT='%s',ENT_DOPE='%s',ENT_LABEL='%s',ENT_REF='%s',"
			"	ENT_ACCOUNT='%s',ENT_CURRENCY='%s',ENT_LEDGER='%s',",
			sdeff, sdope,
			ofo_entry_get_label( entry ),
			ofo_entry_get_ref( entry ),
			ofo_entry_get_account( entry ),
			ofo_entry_get_currency( entry ),
			ofo_entry_get_ledger( entry ));

	model = ofo_entry_get_ope_template( entry );
	if( !model || !g_utf8_strlen( model, -1 )){
		query = g_string_append( query, " ENT_OPE_TEMPLATE=NULL," );
	} else {
		g_string_append_printf( query, " ENT_OPE_TEMPLATE='%s',", model );
	}

	g_string_append_printf( query,
			"	ENT_DEBIT=%s,ENT_CREDIT=%s,"
			"	ENT_UPD_USER='%s',ENT_UPD_STAMP='%s' "
			"	WHERE ENT_NUMBER=%ld",
			sdeb, scre, user, stamp_str, ofo_entry_get_number( entry ));

	if( ofa_dbms_query( dbms, query->str, TRUE )){
		entry_set_upd_user( entry, user );
		entry_set_upd_stamp( entry, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( sdeff );
	g_free( sdope );
	g_free( sdeb );
	g_free( scre );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_entry_update_concil:
 */
gboolean
ofo_entry_update_concil( ofoEntry *entry, const ofoDossier *dossier )
{
	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return( do_update_concil(
						entry,
						ofo_dossier_get_user( dossier ),
						ofo_dossier_get_dbms( dossier )));
	}

	return( FALSE );
}

static gboolean
do_update_concil( ofoEntry *entry, const gchar *user, const ofaDbms *dbms )
{
	GString *query;
	gchar *where, *sdrappro, *stamp_str;
	const GDate *rappro;
	gboolean ok;
	GTimeVal stamp;

	rappro = ofo_entry_get_concil_dval( entry );
	query = g_string_new( "UPDATE OFA_T_ENTRIES SET " );
	where = g_strdup_printf( "WHERE ENT_NUMBER=%ld", ofo_entry_get_number( entry ));

	if( my_date_is_valid( rappro )){

		sdrappro = my_date_to_str( rappro, MY_DATE_SQL );
		my_utils_stamp_set_now( &stamp );
		stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );
		g_string_append_printf(
				query,
				"ENT_CONCIL_DVAL='%s',ENT_CONCIL_USER='%s',ENT_CONCIL_STAMP='%s'",
				sdrappro, user, stamp_str );
		g_free( stamp_str );
		g_free( sdrappro );

	} else {
		query = g_string_append(
						query,
						"ENT_CONCIL_DVAL=NULL,ENT_CONCIL_USER=NULL,ENT_CONCIL_STAMP=NULL" );
	}

	g_string_append_printf( query, " %s", where );
	ok = ofa_dbms_query( dbms, query->str, TRUE );

	g_free( where );
	g_string_free( query, TRUE );

	return( ok );
}

/**
 * ofo_entry_update_settlement:
 *
 * A group of entries has been flagged for settlement (resp. unsettlement).
 * The exact operation is indicated by @number:
 * - if >0, then settle with this number
 * - if <= 0, then unsettle
 *
 * We simultaneously update the ofoEntry object, and the DBMS.
 */
gboolean
ofo_entry_update_settlement( ofoEntry *entry, const ofoDossier *dossier, ofxCounter number )
{
	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return( do_update_settlement(
						entry,
						ofo_dossier_get_user( dossier ),
						ofo_dossier_get_dbms( dossier ),
						number ));
	}

	return( FALSE );
}

static gboolean
do_update_settlement( ofoEntry *entry, const gchar *user, const ofaDbms *dbms, ofxCounter number )
{
	GString *query;
	gchar *stamp_str;
	GTimeVal stamp;
	gboolean ok;

	query = g_string_new( "UPDATE OFA_T_ENTRIES SET " );

	if( number > 0 ){
		entry_set_settlement_number( entry, number );
		entry_set_settlement_user( entry, user );
		entry_set_settlement_stamp( entry, my_utils_stamp_set_now( &stamp ));

		stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );
		g_string_append_printf( query,
				"ENT_STLMT_NUMBER=%ld,ENT_STLMT_USER='%s',ENT_STLMT_STAMP='%s' ",
				number, user, stamp_str );
		g_free( stamp_str );

	} else {
		entry_set_settlement_number( entry, -1 );
		entry_set_settlement_user( entry, NULL );
		entry_set_settlement_stamp( entry, NULL );

		g_string_append_printf( query,
				"ENT_STLMT_NUMBER=NULL,ENT_STLMT_USER=NULL,ENT_STLMT_STAMP=NULL " );
	}

	g_string_append_printf( query, "WHERE ENT_NUMBER=%ld", ofo_entry_get_number( entry ));
	ok = ofa_dbms_query( dbms, query->str, TRUE );

	g_string_free( query, TRUE );

	return( ok );
}

/**
 * ofo_entry_validate:
 */
gboolean
ofo_entry_validate( ofoEntry *entry, const ofoDossier *dossier )
{
	const GDate *effect, *begin, *end;
	gchar *query;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		begin = ofo_dossier_get_exe_begin( dossier );
		g_return_val_if_fail( my_date_is_valid( begin ), FALSE );

		end = ofo_dossier_get_exe_end( dossier );
		g_return_val_if_fail( my_date_is_valid( end ), FALSE );

		effect = ofo_entry_get_deffect( entry );
		g_return_val_if_fail( my_date_is_valid( effect ), FALSE );

		if( my_date_compare( begin, effect ) <= 0 &&
				my_date_compare( effect, end ) <= 0 ){

			ofo_entry_set_status( entry, ENT_STATUS_VALIDATED );

			query = g_strdup_printf(
							"UPDATE OFA_T_ENTRIES "
							"	SET ENT_STATUS=%d "
							"	WHERE ENT_NUMBER=%ld",
									ofo_entry_get_status( entry ),
									ofo_entry_get_number( entry ));

			ofa_dbms_query( ofo_dossier_get_dbms( dossier ), query, TRUE );
			g_free( query );

			/* use the dossier signaling system to update the account */
			g_signal_emit_by_name( G_OBJECT( dossier ), SIGNAL_DOSSIER_VALIDATED_ENTRY, entry );
		}
	}

	return( FALSE );
}

/**
 * ofo_entry_validate_by_ledger:
 *
 * Must return TRUE even if there is no entries at all, while no error
 * is detected.
 */
gboolean
ofo_entry_validate_by_ledger( ofoDossier *dossier, const gchar *mnemo, const GDate *deffect )
{
	gchar *where;
	gchar *query, *str;
	GSList *result, *irow;
	ofoEntry *entry;
	gboolean ok;

	str = my_date_to_str( deffect, MY_DATE_SQL );
	where = g_strdup_printf(
					"	WHERE ENT_LEDGER='%s' AND ENT_STATUS=%d AND ENT_DEFFECT<='%s'",
							mnemo, ENT_STATUS_ROUGH, str );
	g_free( str );

	query = g_strdup_printf(
					"SELECT %s FROM OFA_T_ENTRIES %s", entry_list_columns(), where );

	ok = ofa_dbms_query_ex( ofo_dossier_get_dbms( dossier ), query, &result, TRUE );
	g_free( query );

	if( ok ){
		for( irow=result ; irow ; irow=irow->next ){
			entry = entry_parse_result( irow );

			/* update data for each entry
			 * this let each account updates itself */
			str = my_date_to_str( ofo_entry_get_deffect( entry ), MY_DATE_SQL );
			query = g_strdup_printf(
							"UPDATE OFA_T_ENTRIES "
							"	SET ENT_STATUS=%d "
							"	WHERE ENT_DEFFECT='%s' AND ENT_NUMBER=%ld",
									ENT_STATUS_VALIDATED,
									str,
									ofo_entry_get_number( entry ));
			g_free( str );
			ofa_dbms_query( ofo_dossier_get_dbms( dossier ), query, TRUE );
			g_free( query );

			/* use the dossier signaling system to update the account */
			g_signal_emit_by_name( G_OBJECT( dossier ), SIGNAL_DOSSIER_VALIDATED_ENTRY, entry );
		}
	}

	return( ok );
}

/**
 * ofo_entry_delete:
 */
gboolean
ofo_entry_delete( ofoEntry *entry, const ofoDossier *dossier )
{
	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		if( do_delete_entry(
					entry,
					ofo_dossier_get_dbms( dossier ),
					ofo_dossier_get_user( dossier ))){

			g_signal_emit_by_name(
						G_OBJECT( dossier ),
						SIGNAL_DOSSIER_DELETED_OBJECT, g_object_ref( entry ));

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
do_delete_entry( ofoEntry *entry, const ofaDbms *dbms, const gchar *user )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
				"UPDATE OFA_T_ENTRIES SET "
				"	ENT_STATUS=%d WHERE ENT_NUMBER=%ld",
						ENT_STATUS_DELETED, ofo_entry_get_number( entry ));

	ok = ofa_dbms_query( dbms, query, TRUE );

	g_free( query );

	return( ok );

}

/*
 * iexportable_export:
 *
 * Exports the classes line by line.
 *
 * Returns: TRUE at the end if no error has been detected
 *
 * As a first - probably bad - approach, we load all the entries in
 * memory ! An alternative may be to use a cursor, but this later is
 * only available from a stored program in the DBMS (as for MySQL at
 * least), and this would imply that the exact list of columns be
 * written in this stored program ?
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const ofaExportSettings *settings, ofoDossier *dossier )
{
	GList *result, *irow;
	GSList *lines;
	ofoEntry *entry;
	gchar *str;
	gchar *sdope, *sdeffect, *sdconcil, *stamp, *concil_stamp;
	gchar *settle_stamp, *settle_snum;
	const gchar *sref, *muser, *model, *concil_user;
	const gchar *settle_user;
	ofxCounter settle_number;
	gboolean has_settle, ok, with_headers;
	gulong count;

	result = entry_load_dataset( ofo_dossier_get_dbms( dossier ), NULL );

	with_headers = ofa_export_settings_get_headers( settings );

	count = ( gulong ) g_list_length( result );
	if( with_headers ){
		count += 1;
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = g_strdup( "Dope;Deffect;Number;Label;Ref;Currency;Journal;Operation;Account;Debit;Credit;MajUser;MajStamp;Status;RapproValue;RapproUser;RapproStamp;SettlementNumber;SettlementUser;SettlementStamp" );
		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}
	}

	for( irow=result ; irow ; irow=irow->next ){
		entry = OFO_ENTRY( irow->data );

		sdope = my_date_to_str( ofo_entry_get_dope( entry ), MY_DATE_SQL );
		sdeffect = my_date_to_str( ofo_entry_get_deffect( entry ), MY_DATE_SQL );
		sref = ofo_entry_get_ref( entry );
		muser = ofo_entry_get_upd_user( entry );
		stamp = my_utils_stamp_to_str( ofo_entry_get_upd_stamp( entry ), MY_STAMP_YYMDHMS );
		model = ofo_entry_get_ope_template( entry );
		concil_user = ofo_entry_get_concil_user( entry );
		concil_stamp = my_utils_stamp_to_str( ofo_entry_get_concil_stamp( entry ), MY_STAMP_YYMDHMS );
		sdconcil = my_date_to_str( ofo_entry_get_concil_dval( entry ), MY_DATE_SQL );
		has_settle = FALSE;
		settle_snum = NULL;
		settle_number = ofo_entry_get_settlement_number( entry );
		if( settle_number > 0 ){
			settle_snum = g_strdup_printf( "%ld", settle_number );
			has_settle = TRUE;
		}
		settle_user = ofo_entry_get_settlement_user( entry );
		settle_stamp = my_utils_stamp_to_str( ofo_entry_get_settlement_stamp( entry ), MY_STAMP_YYMDHMS );

		str = g_strdup_printf( "%s;%s;%ld;%s;%s;%s;%s;%s;%s;%.5lf;%.5lf;%s;%s;%d;%s;%s;%s;%s;%s;%s",
				sdope,
				sdeffect,
				ofo_entry_get_number( entry ),
				ofo_entry_get_label( entry ),
				sref ? sref : "",
				ofo_entry_get_currency( entry ),
				ofo_entry_get_ledger( entry ),
				model ? model : "",
				ofo_entry_get_account( entry ),
				ofo_entry_get_debit( entry ),
				ofo_entry_get_credit( entry ),
				muser ? muser : "",
				muser ? stamp : "",
				ofo_entry_get_status( entry ),
				sdconcil,
				concil_user ? concil_user : "",
				concil_stamp ? concil_stamp : "",
				has_settle ? settle_snum : "",
				has_settle ? settle_user : "",
				has_settle ? settle_stamp : "" );

		g_free( settle_snum );
		g_free( settle_stamp );
		g_free( sdconcil );
		g_free( sdeffect );
		g_free( sdope );
		g_free( stamp );

		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}
	}

	ofo_entry_free_dataset( result );

	return( TRUE );
}

/**
 * ofo_entry_import_csv:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - operation date (yyyy-mm-dd)
 * - effect date (yyyy-mm-dd)
 * - label
 * - piece's reference
 * - iso 3a code of the currency, default to those of the account
 * - ledger
 * - operation template
 * - account number, must exist
 * - debit
 * - credit (only one of the twos must be set)
 *
 * Note that the decimal separator must be a dot '.' and not a comma,
 * without any thousand separator, with LANG=C as well as LANG=fr_FR
 *
 * Add the imported entries to the content of OFA_T_ENTRIES, while
 * keeping already existing entries.
 */
void
ofo_entry_import_csv( ofoDossier *dossier, GSList *lines, gboolean with_header )
{
	static const gchar *thisfn = "ofo_entry_import_csv";
	ofoEntry *entry;
	GSList *ili, *ico;
	GList *new_set, *ise;
	gint count;
	gint errors;
	const gchar *str;
	GDate date;
	gchar *currency;
	ofoAccount *account;
	gdouble debit, credit;
	gdouble tot_debits, tot_credits;

	g_debug( "%s: dossier=%p, lines=%p (count=%d), with_header=%s",
			thisfn,
			( void * ) dossier,
			( void * ) lines, g_slist_length( lines ),
			with_header ? "True":"False" );

	new_set = NULL;
	count = 0;
	errors = 0;
	tot_debits = 0;
	tot_credits = 0;

	for( ili=lines ; ili ; ili=ili->next ){
		count += 1;
		if( !( count == 1 && with_header )){
			entry = ofo_entry_new();

			/* operation date */
			ico=ili->data;
			str = ( const gchar * ) ico->data;
			my_date_set_from_sql( &date, str );
			if( !my_date_is_valid( &date )){
				g_warning( "%s: (line %d) invalid operation date: %s", thisfn, count, str );
				errors += 1;
				continue;
			}
			entry->priv->dope = date;

			/* effect date */
			ico=ico->next;
			str = ( const gchar * ) ico->data;
			my_date_set_from_sql( &date, str );
			if( !my_date_is_valid( &date )){
				g_warning( "%s: (line %d) invalid effect date: %s", thisfn, count, str );
				errors += 1;
				continue;
			}
			entry->priv->deffect = date;

			/* entry label */
			ico = ico->next;
			str = ( const gchar * ) ico->data;
			if( !str || !g_utf8_strlen( str, -1 )){
				g_warning( "%s: (line %d) empty label", thisfn, count );
				errors += 1;
				continue;
			}
			ofo_entry_set_label( entry, str );

			/* entry piece's reference - may be empty */
			ico = ico->next;
			str = ( const gchar * ) ico->data;
			ofo_entry_set_ref( entry, str );

			/* entry currency - a default is provided by the account
			 *  so check and set is pushed back after having read it */
			ico = ico->next;
			str = ( const gchar * ) ico->data;
			currency = g_strdup( str );

			/* ledger - default to IMPORT */
			ico = ico->next;
			str = ( const gchar * ) ico->data;
			if( !str || !g_utf8_strlen( str, -1 )){
				ofo_entry_set_ledger( entry, "IMPORT" );
			} else if( !ofo_ledger_get_by_mnemo( dossier, str )){
				g_warning( "%s: import ledger not found: %s", thisfn, str );
				errors += 1;
				continue;
			} else {
				ofo_entry_set_ledger( entry, str );
			}

			/* operation template */
			ico = ico->next;
			str = ( const gchar * ) ico->data;
			if( str && g_utf8_strlen( str, -1 )){
				ofo_entry_set_ope_template( entry, str );
			}

			/* entry account */
			ico = ico->next;
			str = ( const gchar * ) ico->data;
			if( !str || !g_utf8_strlen( str, -1 )){
				g_warning( "%s: (line %d) empty account", thisfn, count );
				errors += 1;
				continue;
			}
			account = ofo_account_get_by_number( dossier, str );
			if( !account ){
				g_warning( "%s: (line %d) non existant account: %s", thisfn, count, str );
				errors += 1;
				continue;
			}
			if( ofo_account_is_root( account )){
				g_warning( "%s: (line %d) not a detail account: %s", thisfn, count, str );
				errors += 1;
				continue;
			}
			ofo_entry_set_account( entry, str );

			if( !currency || !g_utf8_strlen( currency, -1 )){
				g_free( currency );
				currency = g_strdup( ofo_account_get_currency( account ));
			}
			ofo_entry_set_currency( entry, currency );
			g_free( currency );

			/* debit */
			ico = ico->next;
			str = ( const gchar * ) ico->data;
			if( !str ){
				g_warning( "%s: (line %d) empty debit", thisfn, count );
				errors += 1;
				continue;
			}
			debit = my_double_set_from_sql( str );
			tot_debits += debit;

			/* credit */
			ico = ico->next;
			str = ( const gchar * ) ico->data;
			if( !str ){
				g_warning( "%s: (line %d) empty credit", thisfn, count );
				errors += 1;
				continue;
			}
			credit = my_double_set_from_sql( str );
			tot_credits += credit;

			/*g_debug( "%s: debit=%.2lf, credit=%.2lf", thisfn, debit, credit );*/
			if(( debit && !credit ) || ( !debit && credit )){
				ofo_entry_set_debit( entry, debit );
				ofo_entry_set_credit( entry, credit );
			} else {
				g_warning( "%s: (line %d) invalid amounts: debit=%.lf, credit=%.lf", thisfn, count, debit, credit );
				errors += 1;
				continue;
			}

			ofo_entry_set_status( entry, ENT_STATUS_ROUGH );

			new_set = g_list_prepend( new_set, entry );
		}
	}

	if( abs( tot_debits - tot_credits ) > 0.000001 ){
		g_warning( "%s: entries are not balanced: tot_debits=%lf, tot_credits=%lf", thisfn, tot_debits, tot_credits );
		errors += 1;
	}

	if( !errors ){
		for( ise=new_set ; ise ; ise=ise->next ){
			ofo_entry_insert( OFO_ENTRY( ise->data ), dossier );
		}
	}

	g_list_free_full( new_set, ( GDestroyNotify ) g_object_unref );
}
