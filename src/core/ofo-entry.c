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
#include "api/ofa-file-format.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iimportable.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofs-currency.h"

/* priv instance data
 */
enum {
	ENT_NUMBER = 1,
	ENT_DOPE,
	ENT_DEFFECT,
	ENT_LABEL,
	ENT_REF,
	ENT_ACCOUNT,
	ENT_CURRENCY,
	ENT_LEDGER,
	ENT_OPE_TEMPLATE,
	ENT_DEBIT,
	ENT_CREDIT,
	ENT_STATUS,
	ENT_UPD_USER,
	ENT_UPD_STAMP,
	ENT_CONCIL_DVAL,
	ENT_CONCIL_USER,
	ENT_CONCIL_STAMP,
	ENT_STLMT_NUMBER,
	ENT_STLMT_USER,
	ENT_STLMT_STAMP,
};

static const ofsBoxDef st_boxed_defs[] = {
		{ OFA_BOX_CSV( ENT_NUMBER ),
				OFA_TYPE_COUNTER,
				FALSE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( ENT_DOPE ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ENT_DEFFECT ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ENT_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ENT_REF ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ENT_ACCOUNT ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ENT_CURRENCY ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ENT_LEDGER ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ENT_OPE_TEMPLATE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ENT_DEBIT ),
				OFA_TYPE_AMOUNT,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ENT_CREDIT ),
				OFA_TYPE_AMOUNT,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ENT_STATUS ),
				OFA_TYPE_INTEGER,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ENT_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ENT_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ OFA_BOX_CSV( ENT_CONCIL_DVAL ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ENT_CONCIL_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ENT_CONCIL_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ OFA_BOX_CSV( ENT_STLMT_NUMBER ),
				OFA_TYPE_COUNTER,
				TRUE,
				TRUE },
		{ OFA_BOX_CSV( ENT_STLMT_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ENT_STLMT_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ 0 }
};

struct _ofoEntryPrivate {
	void *empty;
};

/* manage the abbreviated localized status
 */
typedef struct {
	ofaEntryStatus num;
	const gchar   *str;
}
	sStatus;

static sStatus st_status[] = {
		{ ENT_STATUS_PAST,      N_( "P" ) },
		{ ENT_STATUS_ROUGH,     N_( "R" ) },
		{ ENT_STATUS_VALIDATED, N_( "V" ) },
		{ ENT_STATUS_DELETED,   N_( "D" ) },
		{ ENT_STATUS_FUTURE,    N_( "F" ) },
		{ 0 },
};

static ofoBaseClass *ofo_entry_parent_class = NULL;

static void         on_updated_object( const ofoDossier *dossier, ofoBase *object, const gchar *prev_id, gpointer user_data );
static void         on_updated_object_account_number( const ofoDossier *dossier, const gchar *prev_id, const gchar *number );
static void         on_updated_object_currency_code( const ofoDossier *dossier, const gchar *prev_id, const gchar *code );
static void         on_updated_object_ledger_mnemo( const ofoDossier *dossier, const gchar *prev_id, const gchar *mnemo );
static void         on_updated_object_model_mnemo( const ofoDossier *dossier, const gchar *prev_id, const gchar *mnemo );
static gchar       *effect_in_exercice( const ofoDossier *dossier );
static GList       *entry_load_dataset( const ofaDbms *dbms, const gchar *where );
static gint         entry_count_for_account( const ofaDbms *dbms, const gchar *account );
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
static void         iexportable_iface_init( ofaIExportableInterface *iface );
static guint        iexportable_get_interface_version( const ofaIExportable *instance );
static gboolean     iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofoDossier *dossier );
static void         iimportable_iface_init( ofaIImportableInterface *iface );
static guint        iimportable_get_interface_version( const ofaIImportable *instance );
static gboolean     iimportable_import( ofaIImportable *exportable, GSList *lines, ofoDossier *dossier );

static void
entry_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_entry_finalize";

	g_debug( "%s: instance=%p (%s): %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, ENT_LABEL ));

	g_return_if_fail( instance && OFO_IS_ENTRY( instance ));

	/* free data members here */

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
}

static void
ofo_entry_class_init( ofoEntryClass *klass )
{
	static const gchar *thisfn = "ofo_entry_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	ofo_entry_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = entry_dispose;
	G_OBJECT_CLASS( klass )->finalize = entry_finalize;

	g_type_class_add_private( klass, sizeof( ofoEntryPrivate ));
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofo_entry_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofoEntryClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) ofo_entry_class_init,
		NULL,
		NULL,
		sizeof( ofoEntry ),
		0,
		( GInstanceInitFunc ) ofo_entry_init
	};

	static const GInterfaceInfo iexportable_iface_info = {
		( GInterfaceInitFunc ) iexportable_iface_init,
		NULL,
		NULL
	};

	static const GInterfaceInfo iimportable_iface_info = {
		( GInterfaceInitFunc ) iimportable_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFO_TYPE_BASE, "ofoEntry", &info, 0 );

	g_type_add_interface_static( type, OFA_TYPE_IEXPORTABLE, &iexportable_iface_info );

	g_type_add_interface_static( type, OFA_TYPE_IIMPORTABLE, &iimportable_iface_info );

	return( type );
}

GType
ofo_entry_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
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
	ofoEntry *entry;

	entry = g_object_new( OFO_TYPE_ENTRY, NULL );
	OFO_BASE( entry )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	ofo_entry_set_number( entry, OFO_BASE_UNSET_ID );
	ofo_entry_set_status( entry, ENT_STATUS_ROUGH );

	return( entry );
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
 *
 * Returns all entries for the given account (if specified).
 *
 * The returned dataset is sorted by ascending dope/deffect/number.
 */
GList *
ofo_entry_get_dataset_by_account( const ofoDossier *dossier, const gchar *account )
{
	static const gchar *thisfn = "ofo_entry_get_dataset_by_account";
	GString *where;
	GList *dataset;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	where = g_string_new( "" );

	if( account && g_utf8_strlen( account, -1 )){
		g_string_append_printf( where, "ENT_ACCOUNT='%s' ", account );
	}

	dataset = entry_load_dataset( ofo_dossier_get_dbms( dossier ), where->str );
	g_debug( "%s: count=%d", thisfn, g_list_length( dataset ));

	g_string_free( where, TRUE );

	return( dataset );
}

/**
 * ofo_entry_get_dataset_by_ledger:
 * @dossier: the dossier.
 * @ledger: the ledger.
 *
 * Returns all entries for the given ledger (if specified).
 *
 * The returned dataset is sorted by ascending dope/deffect/number.
 */
GList *ofo_entry_get_dataset_by_ledger( const ofoDossier *dossier, const gchar *ledger )
{
	static const gchar *thisfn = "ofo_entry_get_dataset_by_ledger";
	GString *where;
	GList *dataset;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	where = g_string_new( "" );

	if( ledger && g_utf8_strlen( ledger, -1 )){
		g_string_append_printf( where, "ENT_LEDGER='%s' ", ledger );
	}

	dataset = entry_load_dataset( ofo_dossier_get_dbms( dossier ), where->str );
	g_debug( "%s: count=%d", thisfn, g_list_length( dataset ));

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

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	query = g_string_new( "OFA_T_ENTRIES WHERE " );
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

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					ofo_dossier_get_dbms( dossier ),
					query->str,
					OFO_TYPE_ENTRY );

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

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	query = g_string_new( "OFA_T_ENTRIES WHERE " );
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

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					ofo_dossier_get_dbms( dossier ),
					query->str,
					OFO_TYPE_ENTRY );

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

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	dataset = NULL;
	query = g_string_new( "" );

	str = effect_in_exercice( dossier);
	g_string_append_printf( query,
			"OFA_T_ENTRIES,OFA_T_ACCOUNTS "
			"	WHERE %s "
			"		AND ENT_CONCIL_DVAL IS NULL "
			"		AND ENT_ACCOUNT=ACC_NUMBER "
			"		AND ACC_RECONCILIABLE='%s' ", str, ACCOUNT_RECONCILIABLE );
	g_free( str );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					ofo_dossier_get_dbms( dossier ),
					query->str,
					OFO_TYPE_ENTRY );

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

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	dataset = NULL;
	query = g_string_new( "" );

	str = effect_in_exercice( dossier);
	g_string_append_printf( query,
			"OFA_T_ENTRIES,OFA_T_ACCOUNTS "
			"	WHERE %s "
			"		AND (ENT_STLMT_NUMBER=0 OR ENT_STLMT_NUMBER IS NULL) "
			"		AND ENT_ACCOUNT=ACC_NUMBER "
			"		AND ACC_SETTLEABLE='%s' ", str, ACCOUNT_SETTLEABLE );
	g_free( str );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					ofo_dossier_get_dbms( dossier ),
					query->str,
					OFO_TYPE_ENTRY );

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

	dataset = NULL;
	query = g_string_new( "OFA_T_ENTRIES " );

	if( where && g_utf8_strlen( where, -1 )){
		g_string_append_printf( query, "WHERE %s ", where );
	}

	query = g_string_append( query,
					"ORDER BY ENT_DOPE ASC,ENT_DEFFECT ASC,ENT_NUMBER ASC" );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					dbms,
					query->str,
					OFO_TYPE_ENTRY );

	g_string_free( query, TRUE );

	return( g_list_reverse( dataset ));
}

/**
 * ofo_entry_free_dataset:
 */
void
ofo_entry_free_dataset( GList *dataset )
{
	static const gchar *thisfn = "ofo_entry_free_dataset";

	g_debug( "%s: dataset=%p, count=%d", thisfn, ( void * ) dataset, g_list_length( dataset ));

	g_list_free_full( dataset, ( GDestroyNotify ) g_object_unref );
}

/**
 * ofo_entry_use_account:
 *
 * Returns: %TRUE if a recorded entry makes use of the specified account.
 */
gboolean
ofo_entry_use_account( const ofoDossier *dossier, const gchar *account )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	return( entry_count_for_account( ofo_dossier_get_dbms( dossier ), account ) > 0 );
}

static gint
entry_count_for_account( const ofaDbms *dbms, const gchar *account )
{
	return( entry_count_for( dbms, "ENT_ACCOUNT", account ));
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
	ofo_base_getter( ENTRY, entry, counter, 0, ENT_NUMBER );
}

/**
 * ofo_entry_get_label:
 */
const gchar *
ofo_entry_get_label( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, string, NULL, ENT_LABEL );
}

/**
 * ofo_entry_get_deffect:
 */
const GDate *
ofo_entry_get_deffect( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, date, NULL, ENT_DEFFECT );
}

/**
 * ofo_entry_get_dope:
 */
const GDate *
ofo_entry_get_dope( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, date, NULL, ENT_DOPE );
}

/**
 * ofo_entry_get_ref:
 */
const gchar *
ofo_entry_get_ref( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, string, NULL, ENT_REF );
}

/**
 * ofo_entry_get_account:
 */
const gchar *
ofo_entry_get_account( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, string, NULL, ENT_ACCOUNT );
}

/**
 * ofo_entry_get_currency:
 */
const gchar *
ofo_entry_get_currency( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, string, NULL, ENT_CURRENCY );
}

/**
 * ofo_entry_get_ledger:
 */
const gchar *
ofo_entry_get_ledger( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, string, NULL, ENT_LEDGER );
}

/**
 * ofo_entry_get_ope_template:
 */
const gchar *
ofo_entry_get_ope_template( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, string, NULL, ENT_OPE_TEMPLATE );
}

/**
 * ofo_entry_get_debit:
 */
ofxAmount
ofo_entry_get_debit( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, amount, 0, ENT_DEBIT );
}

/**
 * ofo_entry_get_credit:
 */
ofxAmount
ofo_entry_get_credit( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, amount, 0, ENT_CREDIT );
}

/**
 * ofo_entry_get_status:
 */
ofaEntryStatus
ofo_entry_get_status( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, int, 0, ENT_STATUS );
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
	ofo_base_getter( ENTRY, entry, string, NULL, ENT_UPD_USER );
}

/**
 * ofo_entry_get_upd_stamp:
 */
const GTimeVal *
ofo_entry_get_upd_stamp( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, timestamp, NULL, ENT_UPD_STAMP );
}

/**
 * ofo_entry_get_concil_dval:
 *
 * The returned #GDate object is never %NULL, but may be invalid.
 */
const GDate *
ofo_entry_get_concil_dval( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, date, NULL, ENT_CONCIL_DVAL );
}

/**
 * ofo_entry_get_concil_user:
 */
const gchar *
ofo_entry_get_concil_user( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, string, NULL, ENT_CONCIL_USER );
}

/**
 * ofo_entry_get_concil_stamp:
 */
const GTimeVal *
ofo_entry_get_concil_stamp( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, timestamp, NULL, ENT_CONCIL_STAMP );
}

/**
 * ofo_entry_get_settlement_number:
 */
ofxCounter
ofo_entry_get_settlement_number( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, counter, 0, ENT_STLMT_NUMBER );
}

/**
 * ofo_entry_get_settlement_user:
 */
const gchar *
ofo_entry_get_settlement_user( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, string, NULL, ENT_STLMT_USER );
}

/**
 * ofo_entry_get_settlement_stamp:
 */
const GTimeVal *
ofo_entry_get_settlement_stamp( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, timestamp, NULL, ENT_STLMT_STAMP );
}

/**
 * ofo_entry_get_min_deffect:
 *
 * Returns: the minimal allowed effect date on the dossier for this
 * ledger.
 *
 * Ledger should be set before calling this function.
 *
 * The returned value may be %NULL. Else, it should be g_free() by the
 * caller.
 */
GDate *
ofo_entry_get_min_deffect( const ofoEntry *entry, ofoDossier *dossier )
{
	GDate *date;
	const gchar *mnemo;
	ofoLedger *ledger;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), NULL );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	date = NULL;

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		ledger = NULL;
		mnemo = ofo_entry_get_ledger( entry );
		if( mnemo && g_utf8_strlen( mnemo, -1 )){
			ledger = ofo_ledger_get_by_mnemo( dossier, mnemo );
			g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), NULL );
		}

		date = g_new0( GDate, 1 );
		ofo_dossier_get_min_deffect( date, dossier, ledger );
	}

	return( date );
}

/**
 * ofo_entry_get_max_val_deffect:
 *
 * Returns: the greatest effect date of validated entries on the account.
 *
 * The returned value may be %NULL.
 */
GDate *
ofo_entry_get_max_val_deffect( ofoDossier *dossier, const gchar *account, GDate *date )
{
	gchar *query;
	GSList *result, *icol;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( account && g_utf8_strlen( account, -1 ), NULL );
	g_return_val_if_fail( date, NULL );

	my_date_clear( date );

	query = g_strdup_printf(
			"SELECT MAX(ENT_DEFFECT) FROM OFA_T_ENTRIES WHERE "
			"	ENT_ACCOUNT='%s' AND ENT_STATUS=%d",
				account, ENT_STATUS_VALIDATED );

	if( ofa_dbms_query_ex( ofo_dossier_get_dbms( dossier ), query, &result, TRUE )){
		if( result ){
			icol = ( GSList * ) result->data;
			if( icol && icol->data ){
				my_date_set_from_sql( date, ( const gchar * ) icol->data );
			}
		}
		ofa_dbms_free_results( result );
	}
	g_free( query );

	return( date );
}

/**
 * ofo_entry_get_max_rough_deffect:
 *
 * Returns: the greatest effect date of rough entries on the account,
 * taking care to not consider future entries.
 *
 * The returned value may be %NULL.
 */
GDate *
ofo_entry_get_max_rough_deffect( ofoDossier *dossier, const gchar *account, GDate *date )
{
	const GDate *exe_end;
	GString *query;
	GSList *result, *icol;
	gchar *sdate;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( account && g_utf8_strlen( account, -1 ), NULL );
	g_return_val_if_fail( date, NULL );

	my_date_clear( date );

	query = g_string_new(
			"SELECT MAX(ENT_DEFFECT) FROM OFA_T_ENTRIES WHERE " );

	g_string_append_printf( query,
			"ENT_ACCOUNT='%s' AND ENT_STATUS=%d ", account, ENT_STATUS_ROUGH );

	exe_end = ofo_dossier_get_exe_end( dossier );
	if( my_date_is_valid( exe_end )){
		sdate = my_date_to_str( exe_end, MY_DATE_SQL );
		g_string_append_printf( query,
				"AND ENT_DEFFECT<='%s'", sdate );
		g_free( sdate );
	}

	if( ofa_dbms_query_ex( ofo_dossier_get_dbms( dossier ), query->str, &result, TRUE )){
		if( result ){
			icol = ( GSList * ) result->data;
			if( icol && icol->data ){
				my_date_set_from_sql( date, ( const gchar * ) icol->data );
			}
		}
		ofa_dbms_free_results( result );
	}
	g_string_free( query, TRUE );

	return( date );
}

/**
 * ofo_entry_get_max_futur_deffect:
 *
 * Returns: the greatest effect date of future entries on the account.
 *
 * The returned value may be %NULL.
 */
GDate *
ofo_entry_get_max_futur_deffect( ofoDossier *dossier, const gchar *account, GDate *date )
{
	const GDate *exe_end;
	GString *query;
	GSList *result, *icol;
	gchar *sdate;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( account && g_utf8_strlen( account, -1 ), NULL );
	g_return_val_if_fail( date, NULL );

	my_date_clear( date );

	exe_end = ofo_dossier_get_exe_end( dossier );
	if( my_date_is_valid( exe_end )){

		query = g_string_new(
				"SELECT MAX(ENT_DEFFECT) FROM OFA_T_ENTRIES WHERE " );

		sdate = my_date_to_str( exe_end, MY_DATE_SQL );

		g_string_append_printf( query,
				"ENT_ACCOUNT='%s' AND ENT_STATUS=%d AND ENT_DEFFECT>'%s'",
				account, ENT_STATUS_ROUGH, sdate );

		g_free( sdate );

		if( ofa_dbms_query_ex( ofo_dossier_get_dbms( dossier ), query->str, &result, TRUE )){
			if( result ){
				icol = ( GSList * ) result->data;
				if( icol && icol->data ){
					my_date_set_from_sql( date, ( const gchar * ) icol->data );
				}
			}
			ofa_dbms_free_results( result );
		}
		g_string_free( query, TRUE );
	}

	return( date );
}

/**
 * ofo_entry_get_currencies:
 *
 * Returns: a #GSList of currency mnemos used by the entries.
 *
 * The returned value should be ofo_entry_free_currencies() by the caller.
 */
GSList *
ofo_entry_get_currencies( const ofoDossier *dossier )
{
	GSList *result, *irow, *icol;
	GSList *list;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( ofa_dbms_query_ex(
			ofo_dossier_get_dbms( dossier ),
			"SELECT DISTINCT(ENT_CURRENCY) FROM OFA_T_ENTRIES ORDER BY ENT_CURRENCY ASC",
			&result, TRUE )){

		list = NULL;
		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			if( icol && icol->data ){
				list = g_slist_append( list, g_strdup( icol->data ));
			}
		}
		ofa_dbms_free_results( result );
		return( list );
	}

	return( NULL );
}

/**
 * ofo_entry_set_number:
 */
void
ofo_entry_set_number( ofoEntry *entry, ofxCounter number )
{
	ofo_base_setter( ENTRY, entry, counter, ENT_NUMBER, number );
}

/**
 * ofo_entry_set_label:
 */
void
ofo_entry_set_label( ofoEntry *entry, const gchar *label )
{
	ofo_base_setter( ENTRY, entry, string, ENT_LABEL, label );
}

/**
 * ofo_entry_set_deffect:
 */
void
ofo_entry_set_deffect( ofoEntry *entry, const GDate *deffect )
{
	ofo_base_setter( ENTRY, entry, date, ENT_DEFFECT, deffect );
}

/**
 * ofo_entry_set_dope:
 */
void
ofo_entry_set_dope( ofoEntry *entry, const GDate *dope )
{
	ofo_base_setter( ENTRY, entry, date, ENT_DOPE, dope );
}

/**
 * ofo_entry_set_ref:
 */
void
ofo_entry_set_ref( ofoEntry *entry, const gchar *ref )
{
	ofo_base_setter( ENTRY, entry, string, ENT_REF, ref );
}

/**
 * ofo_entry_set_account:
 */
void
ofo_entry_set_account( ofoEntry *entry, const gchar *account )
{
	ofo_base_setter( ENTRY, entry, string, ENT_ACCOUNT, account );
}

/**
 * ofo_entry_set_currency:
 */
void
ofo_entry_set_currency( ofoEntry *entry, const gchar *currency )
{
	ofo_base_setter( ENTRY, entry, string, ENT_CURRENCY, currency );
}

/**
 * ofo_entry_set_ledger:
 */
void
ofo_entry_set_ledger( ofoEntry *entry, const gchar *ledger )
{
	ofo_base_setter( ENTRY, entry, string, ENT_LEDGER, ledger );
}

/**
 * ofo_entry_set_ope_template:
 */
void
ofo_entry_set_ope_template( ofoEntry *entry, const gchar *model )
{
	ofo_base_setter( ENTRY, entry, string, ENT_OPE_TEMPLATE, model );
}

/**
 * ofo_entry_set_debit:
 */
void
ofo_entry_set_debit( ofoEntry *entry, ofxAmount debit )
{
	ofo_base_setter( ENTRY, entry, amount, ENT_DEBIT, debit );
}

/**
 * ofo_entry_set_credit:
 */
void
ofo_entry_set_credit( ofoEntry *entry, ofxAmount credit )
{
	ofo_base_setter( ENTRY, entry, amount, ENT_CREDIT, credit );
}

/**
 * ofo_entry_set_status:
 */
void
ofo_entry_set_status( ofoEntry *entry, ofaEntryStatus status )
{
	ofo_base_setter( ENTRY, entry, int, ENT_STATUS, status );
}

/*
 * ofo_entry_set_upd_user:
 */
static void
entry_set_upd_user( ofoEntry *entry, const gchar *upd_user )
{
	ofo_base_setter( ENTRY, entry, string, ENT_UPD_USER, upd_user );
}

/*
 * ofo_entry_set_upd_stamp:
 */
static void
entry_set_upd_stamp( ofoEntry *entry, const GTimeVal *upd_stamp )
{
	ofo_base_setter( ENTRY, entry, timestamp, ENT_UPD_STAMP, upd_stamp );
}

/**
 * ofo_entry_set_concil_dval:
 *
 * The reconciliation may be unset by setting @drappro to %NULL.
 */
void
ofo_entry_set_concil_dval( ofoEntry *entry, const GDate *drappro )
{
	ofo_base_setter( ENTRY, entry, date, ENT_CONCIL_DVAL, drappro );
}

/**
 * ofo_entry_set_concil_user:
 */
void
ofo_entry_set_concil_user( ofoEntry *entry, const gchar *user )
{
	ofo_base_setter( ENTRY, entry, string, ENT_CONCIL_USER, user );
}

/**
 * ofo_entry_set_concil_stamp:
 */
void
ofo_entry_set_concil_stamp( ofoEntry *entry, const GTimeVal *stamp )
{
	ofo_base_setter( ENTRY, entry, timestamp, ENT_CONCIL_STAMP, stamp );
}

/*
 * ofo_entry_set_settlement_number:
 *
 * The reconciliation may be unset by setting @number to 0.
 */
static void
entry_set_settlement_number( ofoEntry *entry, ofxCounter number )
{
	ofo_base_setter( ENTRY, entry, counter, ENT_STLMT_NUMBER, number );
}

/*
 * ofo_entry_set_settlement_user:
 */
static void
entry_set_settlement_user( ofoEntry *entry, const gchar *user )
{
	ofo_base_setter( ENTRY, entry, string, ENT_STLMT_USER, user );
}

/*
 * ofo_entry_set_settlement_stamp:
 */
static void
entry_set_settlement_stamp( ofoEntry *entry, const GTimeVal *stamp )
{
	ofo_base_setter( ENTRY, entry, timestamp, ENT_STLMT_STAMP, stamp );
}

/*
 * ofo_entry_compute_status:
 *
 * Set the entry status depending of the exercice beginning and ending
 * dates of the dossier. If the entry is inside the current exercice,
 * then the set status is ENT_STATUS_ROUGH.
 *
 * Returns: %FALSE if the effect date is not valid regarding the last
 * closing date of the associated ledger.
 */
gboolean
ofo_entry_compute_status( ofoEntry *entry, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_entry_compute_status";
	const GDate *exe_begin, *exe_end, *deffect;
	GDate *min_deffect;
	gboolean is_valid;
	gchar *sdeffect, *sdmin;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );

	is_valid = FALSE;

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		is_valid = TRUE;
		exe_begin = ofo_dossier_get_exe_begin( dossier );
		exe_end = ofo_dossier_get_exe_end( dossier );
		deffect = ofo_entry_get_deffect( entry );
		g_return_val_if_fail( my_date_is_valid( deffect ), FALSE );

		/* what to do regarding the effect date ? */
		if( my_date_is_valid( exe_begin ) && my_date_compare( deffect, exe_begin ) < 0 ){
			/* entry is in the past */
			ofo_entry_set_status( entry, ENT_STATUS_PAST );

		} else if( my_date_is_valid( exe_end ) && my_date_compare( deffect, exe_end ) > 0 ){
			/* entry is in the future */
			ofo_entry_set_status( entry, ENT_STATUS_FUTURE );

		} else {
			min_deffect = ofo_entry_get_min_deffect( entry, dossier );
			is_valid = !my_date_is_valid( min_deffect ) || my_date_compare( deffect, min_deffect ) >= 0;
			g_free( min_deffect );
			if( !is_valid ){
				sdeffect = my_date_to_str( deffect, MY_DATE_DMYY );
				sdmin = my_date_to_str( min_deffect, MY_DATE_DMYY );
				g_warning(
						"%s: entry effect date %s is lesser than minimal allowed %s",
						thisfn, sdeffect, sdmin );
				g_free( sdmin );
				g_free( sdeffect );
				return( FALSE );
			}
			ofo_entry_set_status( entry, ENT_STATUS_ROUGH );
		}
	}

	return( is_valid );
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

	ofo_entry_set_deffect( entry, deffect );
	ofo_entry_set_dope( entry, dope );
	ofo_entry_set_label( entry, label );
	ofo_entry_set_ref( entry, ref );
	ofo_entry_set_account( entry, account );
	ofo_entry_set_currency( entry, currency );
	ofo_entry_set_ledger( entry, ledger );
	ofo_entry_set_ope_template( entry, model );
	ofo_entry_set_debit( entry, debit );
	ofo_entry_set_credit( entry, credit );
	ofo_entry_set_status( entry, ENT_STATUS_ROUGH );

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

		ofo_entry_set_number( entry, ofo_dossier_get_next_entry( dossier ));

		if( entry_do_insert( entry,
					ofo_dossier_get_dbms( dossier ),
					ofo_dossier_get_user( dossier ))){

			if( ofo_entry_get_status( entry ) != ENT_STATUS_PAST ){
				g_signal_emit_by_name( G_OBJECT( dossier ), SIGNAL_DOSSIER_NEW_OBJECT, g_object_ref( entry ));
			}

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
 *
 * entry must be in 'rough' status
 */
gboolean
ofo_entry_validate( ofoEntry *entry, const ofoDossier *dossier )
{
	ofaEntryStatus status;
	gchar *query;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		status = ofo_entry_get_status( entry );
		g_return_val_if_fail( status == ENT_STATUS_ROUGH, FALSE );

		ofo_entry_set_status( entry, ENT_STATUS_VALIDATED );

		query = g_strdup_printf(
						"UPDATE OFA_T_ENTRIES "
						"	SET ENT_STATUS=%d "
						"	WHERE ENT_NUMBER=%ld",
								ofo_entry_get_status( entry ),
								ofo_entry_get_number( entry ));

		ofa_dbms_query( ofo_dossier_get_dbms( dossier ), query, TRUE );
		g_free( query );

		/* use the dossier signaling system to update account and ledger */
		g_signal_emit_by_name( G_OBJECT( dossier ), SIGNAL_DOSSIER_VALIDATED_ENTRY, entry );
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
	gchar *where, *query, *str;
	ofoEntry *entry;
	GList *dataset, *it;

	str = my_date_to_str( deffect, MY_DATE_SQL );
	where = g_strdup_printf(
					"	WHERE ENT_LEDGER='%s' AND ENT_STATUS=%d AND ENT_DEFFECT<='%s'",
							mnemo, ENT_STATUS_ROUGH, str );
	g_free( str );

	query = g_strdup_printf(
					"OFA_T_ENTRIES %s", where );

	g_free( where );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					ofo_dossier_get_dbms( dossier ),
					query,
					OFO_TYPE_ENTRY );

	g_free( query );

	for( it=dataset ; it ; it=it->next ){
		entry = OFO_ENTRY( it->data );

		/* update data for each entry
		 * this let each account updates itself */
		query = g_strdup_printf(
						"UPDATE OFA_T_ENTRIES "
						"	SET ENT_STATUS=%d "
						"	WHERE ENT_NUMBER=%ld",
								ENT_STATUS_VALIDATED,
								ofo_entry_get_number( entry ));

		ofa_dbms_query( ofo_dossier_get_dbms( dossier ), query, TRUE );
		g_free( query );

		/* use the dossier signaling system to update the account */
		g_signal_emit_by_name( G_OBJECT( dossier ), SIGNAL_DOSSIER_VALIDATED_ENTRY, entry );
	}

	ofo_entry_free_dataset( dataset );

	return( TRUE );
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
iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofoDossier *dossier )
{
	GList *result, *it;
	gboolean ok, with_headers;
	gchar field_sep, decimal_sep;
	gulong count;
	gchar *str;
	GSList *lines;

	result = entry_load_dataset( ofo_dossier_get_dbms( dossier ), NULL );

	with_headers = ofa_file_format_has_headers( settings );
	field_sep = ofa_file_format_get_field_sep( settings );
	decimal_sep = ofa_file_format_get_decimal_sep( settings );

	count = ( gulong ) g_list_length( result );
	if( with_headers ){
		count += 1;
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = ofa_box_get_csv_header( st_boxed_defs, field_sep );
		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}
	}

	for( it=result ; it ; it=it->next ){
		str = ofa_box_get_csv_line( OFO_BASE( it->data )->prot->fields, field_sep, decimal_sep );
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

/*
 * ofaIImportable interface management
 */
static void
iimportable_iface_init( ofaIImportableInterface *iface )
{
	static const gchar *thisfn = "ofo_entry_iimportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iimportable_get_interface_version;
	iface->import = iimportable_import;
}

static guint
iimportable_get_interface_version( const ofaIImportable *instance )
{
	return( 1 );
}

/*
 * ofo_entry_iimportable_import:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - operation date (yyyy-mm-dd)
 * - effect date (yyyy-mm-dd)
 * - label
 * - piece's reference
 * - iso 3a code of the currency, default to those of the account
 * - ledger, default is IMPORT, must exist
 * - operation template, default to none
 * - account number, must exist and be a detail account
 * - debit
 * - credit (only one of the twos must be set)
 *
 * Note that amounts must not include thousand separator.
 *
 * Add the imported entries to the content of OFA_T_ENTRIES, while
 * keeping already existing entries.
 *
 * If the entry effect date is before the beginning of the exercice (if
 * set), then accounts and ledgers will not be imputed. The entry will
 * be set as 'past'.
 * Past entries do not need to be balanced.
 *
 * If the entry effect date is in the exercice, then it must be after
 * the last closing date of the ledger. The status will be let to
 * 'rough'.
 *
 * If the entry effect date is after the end of the exercice (if set),
 * then accounts and ledgers will not be imputed, and will be set as
 * 'future'.
 *
 * Both rough and future entries must be balanced per currency.
 */
static gint
iimportable_import( ofaIImportable *importable, GSList *lines, ofoDossier *dossier )
{
	GSList *itl, *fields, *itf;
	const gchar *cstr;
	ofoEntry *entry;
	GList *dataset, *it;
	guint errors, line;
	GDate date;
	gchar *currency;
	ofoAccount *account;
	ofoLedger *ledger;
	gdouble debit, credit;
	gchar *sdeffect, *msg;
	ofaEntryStatus status;
	GList *past, *exe, *fut;
	ofsCurrency *sdet;

	dataset = NULL;
	line = 0;
	errors = 0;
	past = NULL;
	exe = NULL;
	fut = NULL;

	for( itl=lines ; itl ; itl=itl->next ){

		line += 1;
		entry = ofo_entry_new();
		fields = ( GSList * ) itl->data;
		debit = 0;
		credit = 0;
		ofa_iimportable_increment_progress( importable, IMPORTABLE_PHASE_IMPORT, 1 );

		/* operation date */
		itf = fields;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		my_date_set_from_sql( &date, cstr );
		if( !my_date_is_valid( &date )){
			msg = g_strdup_printf( _( "invalid entry operation date: %s" ), cstr );
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, msg );
			g_free( msg );
			errors += 1;
			continue;
		}
		ofo_entry_set_dope( entry, &date );

		/* effect date */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		my_date_set_from_sql( &date, cstr );
		if( !my_date_is_valid( &date )){
			msg = g_strdup_printf( _( "invalid entry effect date: %s" ), cstr );
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, msg );
			g_free( msg );
			errors += 1;
			continue;
		}
		ofo_entry_set_deffect( entry, &date );

		/* entry label */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !cstr || !g_utf8_strlen( cstr, -1 )){
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, _( "empty entry label" ));
			errors += 1;
			continue;
		}
		ofo_entry_set_label( entry, cstr );

		/* entry piece's reference - may be empty */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		ofo_entry_set_ref( entry, cstr );

		/* entry currency - a default is provided by the account
		 *  so check and set is pushed back after having read it */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		currency = g_strdup( cstr );

		/* ledger - default to IMPORT */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !cstr || !g_utf8_strlen( cstr, -1 )){
			cstr = "IMPORT";
		}
		ledger = ofo_ledger_get_by_mnemo( dossier, cstr );
		if( !ledger ){
			msg = g_strdup_printf( _( "entry ledger not found: %s" ), cstr );
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, msg );
			g_free( msg );
			errors += 1;
			continue;
		}
		ofo_entry_set_ledger( entry, cstr );

		/* operation template - optional */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		ofo_entry_set_ope_template( entry, cstr );

		/* entry account */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !cstr || !g_utf8_strlen( cstr, -1 )){
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, _( "empty entry account" ));
			errors += 1;
			continue;
		}
		account = ofo_account_get_by_number( dossier, cstr );
		if( !account ){
			msg = g_strdup_printf( _( "entry account not found: %s" ), cstr );
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, msg );
			g_free( msg );
			errors += 1;
			continue;
		}
		if( ofo_account_is_root( account )){
			msg = g_strdup_printf( _( "entry account is a root account: %s" ), cstr );
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, msg );
			g_free( msg );
			errors += 1;
			continue;
		}
		ofo_entry_set_account( entry, cstr );

		cstr = ofo_account_get_currency( account );
		if( !currency || !g_utf8_strlen( currency, -1 )){
			g_free( currency );
			currency = g_strdup( cstr );
		} else if( g_utf8_collate( currency, cstr )){
			msg = g_strdup_printf(
					_( "entry currency: %s is not the same than those of the account: %s" ),
					currency, cstr );
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, msg );
			g_free( msg );
			errors += 1;
			continue;
		}
		ofo_entry_set_currency( entry, currency );

		/* debit */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		debit = my_double_set_from_sql( cstr );

		/* credit */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		credit = my_double_set_from_sql( cstr );

		/*g_debug( "%s: debit=%.2lf, credit=%.2lf", thisfn, debit, credit );*/
		if(( debit && !credit ) || ( !debit && credit )){
			ofo_entry_set_debit( entry, debit );
			ofo_entry_set_credit( entry, credit );
		} else {
			msg = g_strdup_printf(
					_( "invalid entry amounts: debit=%'.5lf, credit=%'.5lf" ), debit, credit );
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, msg );
			g_free( msg );
			errors += 1;
			continue;
		}

		/* what to do regarding the effect date ? */
		if( !ofo_entry_compute_status( entry, dossier )){
			sdeffect = my_date_to_str( ofo_entry_get_deffect( entry ), MY_DATE_DMYY );
			msg = g_strdup_printf(
					_( "entry effect date %s invalid regarding exercice beginning and ledger last closing dates" ),
					sdeffect );
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, msg );
			g_free( sdeffect );
			g_free( msg );
			errors += 1;
			continue;
		}

		status = ofo_entry_get_status( entry );
		switch( status ){
			case ENT_STATUS_PAST:
				ofs_currency_add_currency( &past, currency, debit, credit );
				break;

			case ENT_STATUS_ROUGH:
				ofs_currency_add_currency( &exe, currency, debit, credit );
				break;

			case ENT_STATUS_FUTURE:
				ofs_currency_add_currency( &fut, currency, debit, credit );
				break;

			default:
				msg = g_strdup_printf( _( "invalid entry status: %d" ), status );
				ofa_iimportable_set_message(
						importable, line, IMPORTABLE_MSG_ERROR, msg );
				g_free( msg );
				errors += 1;
				continue;
		}

		g_free( currency );

		dataset = g_list_prepend( dataset, entry );
	}

	/* rough and future entries must be balanced:
	 * as we are storing 5 decimal digits in the DBMS, so this is the
	 * maximal rounding error accepted */
	for( it=past ; it ; it=it->next ){
		sdet = ( ofsCurrency * ) it->data;
		msg = g_strdup_printf( "PAST [%s] tot_debits='%'.5lf, tot_credits=%'.5lf",
				sdet->currency, sdet->debit, sdet->credit );
		ofa_iimportable_set_message(
				importable, line, IMPORTABLE_MSG_STANDARD, msg );
		g_free( msg );
	}
	for( it=exe ; it ; it=it->next ){
		sdet = ( ofsCurrency * ) it->data;
		msg = g_strdup_printf( "EXE [%s] tot_debits='%'.5lf, tot_credits=%'.5lf",
				sdet->currency, sdet->debit, sdet->credit );
		ofa_iimportable_set_message(
				importable, line, IMPORTABLE_MSG_STANDARD, msg );
		g_free( msg );
		if( abs( sdet->debit - sdet->credit ) > 0.00001 ){
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, _( "entries are not balanced" ));
			errors += 1;
		}
	}
	for( it=fut ; it ; it=it->next ){
		sdet = ( ofsCurrency * ) it->data;
		msg = g_strdup_printf( "FUTURE [%s] tot_debits='%'.5lf, tot_credits=%'.5lf",
				sdet->currency, sdet->debit, sdet->credit );
		ofa_iimportable_set_message(
				importable, line, IMPORTABLE_MSG_STANDARD, msg );
		g_free( msg );
		if( abs( sdet->debit - sdet->credit ) > 0.00001 ){
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, _( "entries are not balanced" ));
			errors += 1;
		}
	}

	if( !errors ){
		for( it=dataset ; it ; it=it->next ){
			ofo_entry_insert( OFO_ENTRY( it->data ), dossier );
			ofa_iimportable_increment_progress( importable, IMPORTABLE_PHASE_INSERT, 1 );
			/*g_debug( "it=%p", ( void * ) it );*/
		}
	}

	g_list_free_full( dataset, ( GDestroyNotify ) g_object_unref );
	ofs_currency_list_free( &past );
	ofs_currency_list_free( &exe );
	ofs_currency_list_free( &fut );

	return( errors );
}
