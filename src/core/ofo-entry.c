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
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-isignal-hub.h"
#include "api/ofa-preferences.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-concil.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofs-account-balance.h"
#include "api/ofs-currency.h"

#include "core/ofa-iconcil.h"

/* priv instance data
 */
enum {
	ENT_DOPE = 1,
	ENT_DEFFECT,
	ENT_LABEL,
	ENT_REF,
	ENT_CURRENCY,
	ENT_LEDGER,
	ENT_OPE_TEMPLATE,
	ENT_ACCOUNT,
	ENT_DEBIT,
	ENT_CREDIT,
	ENT_NUMBER,
	ENT_STATUS,
	ENT_UPD_USER,
	ENT_UPD_STAMP,
	ENT_OPE_NUMBER,
	ENT_STLMT_NUMBER,
	ENT_STLMT_USER,
	ENT_STLMT_STAMP,
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order. So:
 * 1/ put in in an import-compatible order
 * 2/ no more modify it
 * 3/ take attention to be able to support the import of a previously
 *    exported file
 */
static const ofsBoxDef st_boxed_defs[] = {
		{ OFA_BOX_CSV( ENT_DOPE ),
				OFA_TYPE_DATE,
				TRUE,					/* importable */
				FALSE },				/* amount, counter: export zero as empty */
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
		{ OFA_BOX_CSV( ENT_ACCOUNT ),
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
		{ OFA_BOX_CSV( ENT_OPE_NUMBER ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ENT_STLMT_NUMBER ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
										/* below data are not imported */
		{ OFA_BOX_CSV( ENT_STLMT_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ENT_STLMT_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ENT_NUMBER ),
				OFA_TYPE_COUNTER,
				FALSE,
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
				FALSE },
		{ 0 }
};

typedef struct {
	gboolean   import_settled;
}
	ofoEntryPrivate;

#define ENTRY_IE_FORMAT                 1

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

static gchar       *effect_in_exercice( ofaHub *hub );
static GList       *entry_load_dataset( ofaHub *hub, const gchar *where, const gchar *order );
static GDate       *entry_get_min_deffect( const ofoEntry *entry, GDate *date, ofaHub *hub );
static gboolean     entry_get_import_settled( ofoEntry *entry );
static void         entry_set_number( ofoEntry *entry, ofxCounter number );
static void         entry_set_status( ofoEntry *entry, ofaEntryStatus status );
static void         entry_set_upd_user( ofoEntry *entry, const gchar *upd_user );
static void         entry_set_upd_stamp( ofoEntry *entry, const GTimeVal *upd_stamp );
static void         entry_set_settlement_user( ofoEntry *entry, const gchar *user );
static void         entry_set_settlement_stamp( ofoEntry *entry, const GTimeVal *stamp );
static void         entry_set_import_settled( ofoEntry *entry, gboolean settled );
static gboolean     entry_compute_status( ofoEntry *entry, gboolean set_deffect, ofaHub *hub );
static gboolean     entry_do_insert( ofoEntry *entry, ofaHub *hub );
static void         error_ledger( const gchar *ledger );
static void         error_ope_template( const gchar *model );
static void         error_currency( const gchar *currency );
static void         error_acc_number( void );
static void         error_account( const gchar *number );
static void         error_acc_currency( const gchar *currency, ofoAccount *account );
static void         error_amounts( ofxAmount debit, ofxAmount credit );
static gboolean     entry_do_update( ofoEntry *entry, ofaHub *hub );
static gboolean     do_update_settlement( ofoEntry *entry, const ofaIDBConnect *connect, ofxCounter number );
static gboolean     do_delete_entry( ofoEntry *entry, const ofaIDBConnect *connect );
static void         iconcil_iface_init( ofaIConcilInterface *iface );
static guint        iconcil_get_interface_version( void );
static ofxCounter   iconcil_get_object_id( const ofaIConcil *instance );
static const gchar *iconcil_get_object_type( const ofaIConcil *instance );
static void         iexportable_iface_init( ofaIExportableInterface *iface );
static guint        iexportable_get_interface_version( void );
static gchar       *iexportable_get_label( const ofaIExportable *instance );
static gboolean     iexportable_export( ofaIExportable *exportable, ofaStreamFormat *settings, ofaHub *hub );
static gchar       *export_cb( const ofsBoxData *box_data, ofaStreamFormat *format, const gchar *text, ofoCurrency *currency );
static void         iimportable_iface_init( ofaIImportableInterface *iface );
static guint        iimportable_get_interface_version( void );
static gchar       *iimportable_get_label( const ofaIImportable *instance );
static guint        iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static GList       *iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static void         iimportable_import_concil( ofaIImporter *importer, ofsImporterParms *parms, ofoEntry *entry, GSList **fields );
static void         iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean     entry_drop_content( const ofaIDBConnect *connect );
static void         isignal_hub_iface_init( ofaISignalHubInterface *iface );
static void         isignal_hub_connect( ofaHub *hub );
static gboolean     hub_on_deletable_object( ofaHub *hub, ofoBase *object, void *empty );
static gboolean     hub_is_deletable_account( ofaHub *hub, ofoAccount *account );
static gboolean     hub_is_deletable_account_by_mnemo( ofaHub *hub, const gchar *mnemo );
static gboolean     hub_is_deletable_currency( ofaHub *hub, ofoCurrency *currency );
static gboolean     hub_is_deletable_ledger( ofaHub *hub, ofoLedger *ledger );
static gboolean     hub_is_deletable_ledger_by_mnemo( ofaHub *hub, const gchar *mnemo );
static gboolean     hub_is_deletable_ope_template( ofaHub *hub, ofoOpeTemplate *template );
static void         hub_on_deleted_object( const ofaHub *hub, ofoBase *object, void *empty );
static void         hub_on_exe_dates_changed( ofaHub *hub, const GDate *prev_begin, const GDate *prev_end, void *empty );
static gint         check_for_changed_begin_exe_dates( ofaHub *hub, const GDate *prev_begin, const GDate *new_begin, gboolean remediate );
static gint         check_for_changed_end_exe_dates( ofaHub *hub, const GDate *prev_end, const GDate *new_end, gboolean remediate );
static gint         remediate_status( ofaHub *hub, gboolean remediate, const gchar *where, ofaEntryStatus new_status );
static void         hub_on_entry_status_change( ofaHub *hub, ofoEntry *entry, ofaEntryStatus prev_status, ofaEntryStatus new_status, void *empty );
static void         hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, void *empty );
static void         hub_on_updated_account_number( ofaHub *hub, const gchar *prev_id, const gchar *number );
static void         hub_on_updated_currency_code( ofaHub *hub, const gchar *prev_id, const gchar *code );
static void         hub_on_updated_ledger_mnemo( ofaHub *hub, const gchar *prev_id, const gchar *mnemo );
static void         hub_on_updated_model_mnemo( ofaHub *hub, const gchar *prev_id, const gchar *mnemo );

G_DEFINE_TYPE_EXTENDED( ofoEntry, ofo_entry, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoEntry )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ICONCIL, iconcil_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNAL_HUB, isignal_hub_iface_init ))

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
}

static void
ofo_entry_class_init( ofoEntryClass *klass )
{
	static const gchar *thisfn = "ofo_entry_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = entry_dispose;
	G_OBJECT_CLASS( klass )->finalize = entry_finalize;
}

/**
 * ofo_entry_get_dataset_by_account:
 * @hub: the current #ofaHub object.
 * @account: the searched account number.
 *
 * Returns all entries either for the specified @account (if any),
 * or for all accounts.
 *
 * The returned dataset is sorted by ascending account/dope/deffect/number.
 */
GList *
ofo_entry_get_dataset_by_account( ofaHub *hub, const gchar *account )
{
	static const gchar *thisfn = "ofo_entry_get_dataset_by_account";
	GString *where;
	GList *dataset;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	where = g_string_new( "" );

	if( my_strlen( account )){
		g_string_append_printf( where, "ENT_ACCOUNT='%s' ", account );
	}

	dataset = entry_load_dataset( hub, where->str,
			"ORDER BY ENT_ACCOUNT ASC,ENT_DOPE ASC,ENT_DEFFECT ASC,ENT_NUMBER ASC");

	g_debug( "%s: count=%d", thisfn, g_list_length( dataset ));

	g_string_free( where, TRUE );

	return( dataset );
}

/**
 * ofo_entry_get_dataset_by_ledger:
 * @hub: the current #ofaHub object.
 * @ledger: the ledger.
 *
 * Returns all entries either for the specified @ledger (if any),
 * or for all ledgers.
 *
 * The returned dataset is sorted by ascending ledger/dope/deffect/number.
 */
GList *ofo_entry_get_dataset_by_ledger( ofaHub *hub, const gchar *ledger )
{
	static const gchar *thisfn = "ofo_entry_get_dataset_by_ledger";
	GString *where;
	GList *dataset;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	where = g_string_new( "" );

	if( my_strlen( ledger )){
		g_string_append_printf( where, "ENT_LEDGER='%s' ", ledger );
	}

	dataset = entry_load_dataset( hub, where->str,
			"ORDER BY ENT_LEDGER ASC,ENT_DOPE ASC,ENT_DEFFECT ASC,ENT_NUMBER ASC");

	g_debug( "%s: count=%d", thisfn, g_list_length( dataset ));

	g_string_free( where, TRUE );

	return( dataset );
}

/**
 * ofo_entry_get_dataset_for_print_balance:
 * @hub: the current #ofaHub object.
 * @from_account: the starting account.
 * @to_account: the ending account.
 * @from_date: the starting effect date.
 * @to_date: the ending effect date.
 *
 * Returns the dataset of non-deleted entries for the given accounts,
 * between the specified effect dates, as a GList of newly allocated
 * #ofsAccountBalance structures, that the user should
 * #ofs_account_balance_list_free().
 *
 * The returned dataset is ordered by ascending account.
 */
GList *
ofo_entry_get_dataset_for_print_balance( ofaHub *hub,
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

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	query = g_string_new(
				"SELECT ENT_ACCOUNT,ENT_CURRENCY,SUM(ENT_DEBIT),SUM(ENT_CREDIT) "
				"FROM OFA_T_ENTRIES WHERE " );
	first = FALSE;
	dataset = NULL;

	if( my_strlen( from_account )){
		g_string_append_printf( query, "ENT_ACCOUNT>='%s' ", from_account );
		first = TRUE;
	}
	if( my_strlen( to_account )){
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

	if( ofa_idbconnect_query_ex( ofa_hub_get_connect( hub ), query->str, &result, TRUE )){
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
		ofa_idbconnect_free_results( result );
	}
	g_string_free( query, TRUE );

	return( g_list_reverse( dataset ));
}

/**
 * ofo_entry_get_dataset_balance:
 * @hub: the current #ofaHub object.
 * @from_account: the starting account.
 * @to_account: the ending account.
 * @from_date: the starting effect date.
 * @to_date: the ending effect date.
 *
 * Returns the balances for non-deleted entries for the given
 * accounts, between the specified effect dates, as a GList of newly
 * allocated #ofsAccountBalance structures, that the user should
 * #ofs_account_balance_list_free().
 *
 * The returned dataset is ordered by ascending account.
 */
GList *
ofo_entry_get_dataset_balance( ofaHub *hub,
											const gchar *from_account, const gchar *to_account,
											const GDate *from_date, const GDate *to_date )
{
	static const gchar *thisfn = "ofo_entry_get_dataset_balance";
	GList *dataset;
	GString *query;
	gboolean first;
	gchar *str;
	GSList *result, *irow, *icol;
	ofsAccountBalance *sbal;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	query = g_string_new(
				"SELECT ENT_ACCOUNT,ENT_CURRENCY,SUM(ENT_DEBIT),SUM(ENT_CREDIT) "
				"FROM OFA_T_ENTRIES WHERE " );
	first = FALSE;
	dataset = NULL;

	if( my_strlen( from_account )){
		g_string_append_printf( query, "ENT_ACCOUNT>='%s' ", from_account );
		first = TRUE;
	}
	if( my_strlen( to_account )){
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

	if( ofa_idbconnect_query_ex( ofa_hub_get_connect( hub ), query->str, &result, TRUE )){
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
		ofa_idbconnect_free_results( result );
	}
	g_string_free( query, TRUE );

	return( g_list_reverse( dataset ));
}

/**
 * ofo_entry_get_dataset_for_print_general_books:
 * @hub: the current #ofaHub object.
 * @from_account: the starting account.
 * @to_account: the ending account.
 * @from_date: the starting effect date.
 * @to_date: the ending effect date.
 *
 * Returns the dataset of non-deleted entries for the given accounts,
 * between the specified effect dates, as a GList of #ofoEntry,
 * that the user should #ofo_entry_free_dataset().
 *
 * The returned dataset is ordered by ascending account/dope/deffect/number.
 */
GList *
ofo_entry_get_dataset_for_print_general_books( ofaHub *hub,
												const gchar *from_account, const gchar *to_account,
												const GDate *from_date, const GDate *to_date )
{
	GList *dataset;
	GString *query;
	gboolean first;
	gchar *str;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	query = g_string_new( "" );
	first = TRUE;
	dataset = NULL;

	if( my_strlen( from_account )){
		g_string_append_printf( query, "ENT_ACCOUNT>='%s' ", from_account );
		first = FALSE;
	}
	if( my_strlen( to_account )){
		if( !first ){
			query = g_string_append( query, "AND " );
		}
		g_string_append_printf( query, "ENT_ACCOUNT<='%s' ", to_account );
		first = FALSE;
	}
	if( my_date_is_valid( from_date )){
		if( !first ){
			query = g_string_append( query, "AND " );
		}
		str = my_date_to_str( from_date, MY_DATE_SQL );
		g_string_append_printf( query, "ENT_DEFFECT>='%s' ", str );
		g_free( str );
		first = FALSE;
	}
	if( my_date_is_valid( to_date )){
		if( !first ){
			query = g_string_append( query, "AND " );
		}
		str = my_date_to_str( to_date, MY_DATE_SQL );
		g_string_append_printf( query, "ENT_DEFFECT<='%s' ", str );
		g_free( str );
		first = FALSE;
	}
	if( !first ){
		query = g_string_append( query, "AND " );
	}
	g_string_append_printf( query, "ENT_STATUS!=%u ", ENT_STATUS_DELETED );

	dataset = entry_load_dataset( hub,  query->str,
			"ORDER BY ENT_ACCOUNT ASC,ENT_DOPE ASC,ENT_DEFFECT ASC,ENT_NUMBER ASC" );

	g_string_free( query, TRUE );

	return( dataset );
}

/**
 * ofo_entry_get_dataset_for_print_ledgers:
 * @hub: the current #ofaHub object.
 * @mnemos: a list of requested ledger mnemos.
 * @from_date: the starting effect date.
 * @to_date: the ending effect date.
 *
 * Returns the dataset of non-deleted entries for the ledgers specified
 * by their mnemo, between the specified effect dates, as a #GList of
 * #ofoEntry, that the user should #ofo_entry_free_dataset().
 *
 * The returned dataset is ordered by ascending ledger/dope/deffect/number.
 */
GList *
ofo_entry_get_dataset_for_print_ledgers( ofaHub *hub,
												const GSList *mnemos,
												const GDate *from_date, const GDate *to_date )
{
	GList *dataset;
	GString *query;
	gboolean first;
	gchar *str;
	const GSList *it;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	query = g_string_new( "" );
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

	dataset = entry_load_dataset( hub, query->str,
			"ORDER BY ENT_LEDGER ASC,ENT_DOPE ASC,ENT_DEFFECT ASC,ENT_NUMBER ASC" );

	g_string_free( query, TRUE );

	return( dataset );
}

/**
 * ofo_entry_get_dataset_for_print_reconcil:
 * @hub: the current #ofaHub object.
 * @account: the reconciliated account.
 * @date: the greatest effect date to be returned.
 *
 * Returns the dataset of un-reconciliated un-deleted entries for the
 * specified account, up to and including the specified effect date.
 *
 * The returned dataset is ordered by ascending dope/deffect/number.
 */
GList *
ofo_entry_get_dataset_for_print_reconcil( ofaHub *hub,
											const gchar *account, const GDate *date )
{
	GList *dataset;
	GString *where;
	gchar *str;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( my_strlen( account ), NULL );

	where = g_string_new( "" );
	g_string_append_printf( where, "ENT_ACCOUNT='%s' ", account );
	g_string_append_printf( where, "AND ENT_NUMBER NOT IN "
			"(SELECT REC_IDS_OTHER FROM OFA_T_CONCIL_IDS WHERE REC_IDS_TYPE='%s') ",
			CONCIL_TYPE_ENTRY );

	if( my_date_is_valid( date )){
		str = my_date_to_str( date, MY_DATE_SQL );
		g_string_append_printf( where, "AND ENT_DEFFECT<='%s'", str );
		g_free( str );
	}

	g_string_append_printf( where, " AND ENT_STATUS!=%u ", ENT_STATUS_DELETED );

	dataset = entry_load_dataset( hub, where->str, NULL );

	g_string_free( where, TRUE );

	return( dataset );
}

/**
 * ofo_entry_get_dataset_for_exercice_by_status:
 * @hub: the current #ofaHub object.
 * @status: the requested status
 *
 * Returns the dataset of entries on the exercice of the specified
 * status.
 *
 * The returned dataset is ordered by dope/deffect/number, and should
 * be #ofo_entry_free_dataset() by the caller.
 */
GList *
ofo_entry_get_dataset_for_exercice_by_status( ofaHub *hub, ofaEntryStatus status )
{
	GList *dataset;
	GString *where;
	gchar *str;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	where = g_string_new( "" );

	str = effect_in_exercice( hub);
	g_string_append_printf( where, "%s AND ENT_STATUS=%u ", str, status );
	g_free( str );

	dataset = entry_load_dataset( hub, where->str, NULL );

	g_string_free( where, TRUE );

	return( dataset );
}

/*
 * build a where string for the exercice on the effect date
 */
static gchar *
effect_in_exercice( ofaHub *hub )
{
	ofoDossier *dossier;
	GString *where;
	const GDate *begin, *end;
	gchar *str;

	where = g_string_new( "" );
	dossier = ofa_hub_get_dossier( hub );

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
entry_load_dataset( ofaHub *hub, const gchar *where, const gchar *order )
{
	GList *dataset;
	GString *query;
	const gchar *real_order;

	dataset = NULL;
	query = g_string_new( "OFA_T_ENTRIES " );

	if( my_strlen( where )){
		g_string_append_printf( query, "WHERE %s ", where );
	}

	if( my_strlen( order )){
		real_order = order;
	} else {
		real_order = "ORDER BY ENT_DOPE ASC,ENT_DEFFECT ASC,ENT_NUMBER ASC";
	}

	query = g_string_append( query, real_order );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					query->str,
					OFO_TYPE_ENTRY,
					hub );

	g_string_free( query, TRUE );

	return( dataset );
}

/**
 * ofo_entry_use_account:
 *
 * Returns: %TRUE if a recorded entry makes use of the specified account.
 */
gboolean
ofo_entry_use_account( ofaHub *hub, const gchar *account )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

	return( !hub_is_deletable_account_by_mnemo( hub, account ));
}

/**
 * ofo_entry_use_ledger:
 *
 * Returns: %TRUE if a recorded entry makes use of the specified ledger.
 */
gboolean
ofo_entry_use_ledger( ofaHub *hub, const gchar *ledger )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

	return( !hub_is_deletable_ledger_by_mnemo( hub, ledger ));
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

	entry_set_number( entry, OFO_BASE_UNSET_ID );
	entry_set_status( entry, ENT_STATUS_ROUGH );

	return( entry );
}

/**
 * ofo_entry_dump:
 */
void
ofo_entry_dump( const ofoEntry *entry )
{
	g_return_if_fail( entry && OFO_IS_ENTRY( entry ));
	ofa_box_dump_fields_list( "ofo_entry_dump", OFO_BASE( entry )->prot->fields );
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

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), NULL );
	g_return_val_if_fail( !OFO_BASE( entry )->prot->dispose_has_run, NULL );

	status = ofo_entry_get_status( entry );
	for( i=0 ; st_status[i].num ; ++i ){
		if( st_status[i].num == status ){
			return( gettext( st_status[i].str ));
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

	g_return_val_if_fail( my_strlen( abr_status ), ENT_STATUS_ROUGH );

	for( i=0 ; st_status[i].num ; ++i ){
		if( !g_utf8_collate( st_status[i].str, abr_status )){
			return( st_status[i].num );
		}
	}

	return( ENT_STATUS_ROUGH );
}

/**
 * ofo_entry_get_ope_number:
 * @entry: this #ofoEntry instance.
 *
 * Returns: the number of the source operation, or zero.
 */
ofxCounter
ofo_entry_get_ope_number( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, counter, 0, ENT_OPE_NUMBER );
}

/**
 * ofo_entry_get_settlement_number:
 * @entry: this #ofoEntry instance.
 *
 * Returns: the number of the settlement group, or zero.
 */
ofxCounter
ofo_entry_get_settlement_number( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, counter, 0, ENT_STLMT_NUMBER );
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
 * ofo_entry_get_exe_changed_count:
 */
gint
ofo_entry_get_exe_changed_count( ofaHub *hub,
									const GDate *prev_begin, const GDate *prev_end,
									const GDate *new_begin, const GDate *new_end )
{
	gint count_begin, count_end;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), 0 );

	count_begin = check_for_changed_begin_exe_dates( hub, prev_begin, new_begin, FALSE );
	count_end = check_for_changed_end_exe_dates( hub, prev_end, new_end, FALSE );

	return( count_begin+count_end );
}

/*
 * entry_get_min_deffect:
 * @entry: this #ofoEntry entry.
 * @date: [out]: the outputed date.
 * @hub: the current #ofaHub object.
 *
 * Returns: the minimal allowed effect date on the dossier for the
 * ledger on which the entry is imputed.
 *
 * NOTE: This method may be called for a new entry which has never been
 * yet serialized to the database (and so for which ofo_base_get_hub()
 * would not work).
 */
static GDate *
entry_get_min_deffect( const ofoEntry *entry, GDate *date, ofaHub *hub )
{
	ofoDossier *dossier;
	const gchar *mnemo;
	ofoLedger *ledger;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), NULL );
	g_return_val_if_fail( !OFO_BASE( entry )->prot->dispose_has_run, NULL );

	g_date_clear( date, 1 );
	ledger = NULL;

	dossier = ofa_hub_get_dossier( hub );
	mnemo = ofo_entry_get_ledger( entry );
	if( my_strlen( mnemo )){
		ledger = ofo_ledger_get_by_mnemo( hub, mnemo );
		g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), NULL );
		ofo_dossier_get_min_deffect( dossier, ledger, date );
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
ofo_entry_get_max_val_deffect( ofaHub *hub, const gchar *account, GDate *date )
{
	gchar *query;
	GSList *result, *icol;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( my_strlen( account ), NULL );
	g_return_val_if_fail( date, NULL );

	my_date_clear( date );

	query = g_strdup_printf(
			"SELECT MAX(ENT_DEFFECT) FROM OFA_T_ENTRIES WHERE "
			"	ENT_ACCOUNT='%s' AND ENT_STATUS=%d",
				account, ENT_STATUS_VALIDATED );

	if( ofa_idbconnect_query_ex( ofa_hub_get_connect( hub ), query, &result, TRUE )){
		if( result ){
			icol = ( GSList * ) result->data;
			if( icol && icol->data ){
				my_date_set_from_sql( date, ( const gchar * ) icol->data );
			}
		}
		ofa_idbconnect_free_results( result );
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
ofo_entry_get_max_rough_deffect( ofaHub *hub, const gchar *account, GDate *date )
{
	const GDate *exe_end;
	GString *query;
	GSList *result, *icol;
	gchar *sdate;
	ofoDossier *dossier;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( my_strlen( account ), NULL );
	g_return_val_if_fail( date, NULL );

	my_date_clear( date );

	query = g_string_new(
			"SELECT MAX(ENT_DEFFECT) FROM OFA_T_ENTRIES WHERE " );

	g_string_append_printf( query,
			"ENT_ACCOUNT='%s' AND ENT_STATUS=%d ", account, ENT_STATUS_ROUGH );

	dossier = ofa_hub_get_dossier( hub );
	exe_end = ofo_dossier_get_exe_end( dossier );
	if( my_date_is_valid( exe_end )){
		sdate = my_date_to_str( exe_end, MY_DATE_SQL );
		g_string_append_printf( query,
				"AND ENT_DEFFECT<='%s'", sdate );
		g_free( sdate );
	}

	if( ofa_idbconnect_query_ex( ofa_hub_get_connect( hub ), query->str, &result, TRUE )){
		if( result ){
			icol = ( GSList * ) result->data;
			if( icol && icol->data ){
				my_date_set_from_sql( date, ( const gchar * ) icol->data );
			}
		}
		ofa_idbconnect_free_results( result );
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
ofo_entry_get_max_futur_deffect( ofaHub *hub, const gchar *account, GDate *date )
{
	const GDate *exe_end;
	GString *query;
	GSList *result, *icol;
	gchar *sdate;
	ofoDossier *dossier;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( my_strlen( account ), NULL );
	g_return_val_if_fail( date, NULL );

	my_date_clear( date );
	dossier = ofa_hub_get_dossier( hub );
	exe_end = ofo_dossier_get_exe_end( dossier );

	/* if the ending date of the exercice is not set, then there is no
	 *  future entries */
	if( my_date_is_valid( exe_end )){

		query = g_string_new(
				"SELECT MAX(ENT_DEFFECT) FROM OFA_T_ENTRIES WHERE " );

		sdate = my_date_to_str( exe_end, MY_DATE_SQL );

		g_string_append_printf( query,
				"ENT_ACCOUNT='%s' AND ENT_STATUS=%d AND ENT_DEFFECT>'%s'",
				account, ENT_STATUS_FUTURE, sdate );

		g_free( sdate );

		if( ofa_idbconnect_query_ex( ofa_hub_get_connect( hub ), query->str, &result, TRUE )){
			if( result ){
				icol = ( GSList * ) result->data;
				if( icol && icol->data ){
					my_date_set_from_sql( date, ( const gchar * ) icol->data );
				}
			}
			ofa_idbconnect_free_results( result );
		}
		g_string_free( query, TRUE );
	}

	return( date );
}

/**
 * ofo_entry_get_currencies:
 *
 * Returns: a #GSList of currency identifiers (ISO 3A code) used by the
 * entries.
 *
 * The returned value should be ofo_entry_free_currencies() by the caller.
 */
GSList *
ofo_entry_get_currencies( ofaHub *hub )
{
	GSList *result, *irow, *icol;
	GSList *list;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	if( ofa_idbconnect_query_ex(
			ofa_hub_get_connect( hub ),
			"SELECT DISTINCT(ENT_CURRENCY) FROM OFA_T_ENTRIES ORDER BY ENT_CURRENCY ASC",
			&result, TRUE )){

		list = NULL;
		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			if( icol && icol->data ){
				list = g_slist_append( list, g_strdup( icol->data ));
			}
		}
		ofa_idbconnect_free_results( result );
		return( list );
	}

	return( NULL );
}

/*
 * ofo_entry_get_import_settled:
 */
static gboolean
entry_get_import_settled( ofoEntry *entry )
{
	ofoEntryPrivate *priv;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( !OFO_BASE( entry )->prot->dispose_has_run, FALSE );

	priv = ofo_entry_get_instance_private( entry );

	return( priv->import_settled );
}

/**
 * ofo_entry_is_editable:
 * @entry:
 *
 * Returns: %TRUE if the @entry may be edited.
 *
 * An entry may be edited if its status is either rough or future.
 * Past, validated or deleted entries cannot be edited.
 */
gboolean
ofo_entry_is_editable( const ofoEntry *entry )
{
	gboolean editable;
	ofaEntryStatus status;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( !OFO_BASE( entry )->prot->dispose_has_run, FALSE );

	status = ofo_entry_get_status( entry );
	editable = ( status == ENT_STATUS_ROUGH || status == ENT_STATUS_FUTURE );

	return( editable );
}

/*
 * entry_set_number:
 */
static void
entry_set_number( ofoEntry *entry, ofxCounter number )
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

/*
 * entry_set_status:
 */
static void
entry_set_status( ofoEntry *entry, ofaEntryStatus status )
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
 * ofo_entry_set_ope_number:
 */
void
ofo_entry_set_ope_number( ofoEntry *entry, ofxCounter number )
{
	ofo_base_setter( ENTRY, entry, counter, ENT_OPE_NUMBER, number );
}

/**
 * ofo_entry_set_settlement_number:
 *
 * The reconciliation may be unset by setting @number to 0.
 */
void
ofo_entry_set_settlement_number( ofoEntry *entry, ofxCounter number )
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
 * ofo_entry_set_import_settled:
 */
static void
entry_set_import_settled( ofoEntry *entry, gboolean settled )
{
	ofoEntryPrivate *priv;

	g_return_if_fail( entry && OFO_IS_ENTRY( entry ));
	g_return_if_fail( !OFO_BASE( entry )->prot->dispose_has_run );

	priv = ofo_entry_get_instance_private( entry );

	priv->import_settled = settled;
}

/*
 * entry_compute_status:
 * @entry:
 * @set_deffect: if %TRUE, then the effect date is modified to the
 *  minimum allowed.
 * @dossier:
 *
 * Set the entry status depending of the exercice beginning and ending
 * dates of the dossier. If the entry is inside the current exercice,
 * then the set status is ENT_STATUS_ROUGH.
 *
 * Returns: %FALSE if the effect date is not valid regarding the last
 * closing date of the associated ledger.
 * This never happens when @set_deffect is %TRUE.
 *
 * NOTE: This method may be called for a new entry which has never been
 * yet serialized to the database (and so for which ofo_base_get_hub()
 * would not work).
 */
gboolean
entry_compute_status( ofoEntry *entry, gboolean set_deffect, ofaHub *hub )
{
	static const gchar *thisfn = "entry_compute_status";
	const GDate *exe_begin, *exe_end, *deffect;
	ofoDossier *dossier;
	GDate min_deffect;
	gboolean is_valid;
	gchar *sdeffect, *sdmin;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( !OFO_BASE( entry )->prot->dispose_has_run, FALSE );

	is_valid = TRUE;
	dossier = ofa_hub_get_dossier( hub );

	exe_begin = ofo_dossier_get_exe_begin( dossier );
	exe_end = ofo_dossier_get_exe_end( dossier );
	deffect = ofo_entry_get_deffect( entry );
	g_return_val_if_fail( my_date_is_valid( deffect ), FALSE );

	/* what to do regarding the effect date ? */
	if( my_date_is_valid( exe_begin ) && my_date_compare( deffect, exe_begin ) < 0 ){
		/* entry is in the past */
		entry_set_status( entry, ENT_STATUS_PAST );

	} else if( my_date_is_valid( exe_end ) && my_date_compare( deffect, exe_end ) > 0 ){
		/* entry is in the future */
		entry_set_status( entry, ENT_STATUS_FUTURE );

	} else {
		entry_get_min_deffect( entry, &min_deffect, hub );
		is_valid = !my_date_is_valid( &min_deffect ) ||
				my_date_compare( deffect, &min_deffect ) >= 0;

		if( !is_valid && set_deffect ){
			ofo_entry_set_deffect( entry, &min_deffect );
			is_valid = TRUE;
		}

		if( !is_valid ){
			sdeffect = my_date_to_str( deffect, ofa_prefs_date_display());
			sdmin = my_date_to_str( &min_deffect, ofa_prefs_date_display());
			g_warning(
					"%s: entry effect date %s is lesser than minimal allowed %s",
					thisfn, sdeffect, sdmin );
			g_free( sdmin );
			g_free( sdeffect );

		} else {
			entry_set_status( entry, ENT_STATUS_ROUGH );
		}
	}

	return( is_valid );
}

/**
 * ofo_entry_is_valid_data:
 */
gboolean
ofo_entry_is_valid_data( ofaHub *hub,
							const GDate *deffect, const GDate *dope, const gchar *label,
							const gchar *account, const gchar *currency, const gchar *ledger,
							const gchar *model, ofxAmount debit, ofxAmount credit,
							gchar **msgerr)
{
	ofoAccount *account_obj;
	gboolean ok;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

	ok = TRUE;

	if( !my_strlen( ledger ) || !ofo_ledger_get_by_mnemo( hub, ledger )){
		error_ledger( ledger );
		ok &= FALSE;
	}
	if( !my_strlen( model ) || !ofo_ope_template_get_by_mnemo( hub, model )){
		error_ope_template( model );
		ok &= FALSE;
	}
	if( !my_strlen( currency ) || !ofo_currency_get_by_code( hub, currency )){
		error_currency( currency );
		ok &= FALSE;
	}
	if( !my_strlen( account )){
		error_acc_number();
		ok &= FALSE;
	} else {
		account_obj = ofo_account_get_by_number( hub, account );
		if( !account_obj ){
			error_account( account );
			ok &= FALSE;

		} else if( g_utf8_collate( currency, ofo_account_get_currency( account_obj ))){
			error_acc_currency( currency, account_obj );
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
ofo_entry_new_with_data( ofaHub *hub,
							const GDate *deffect, const GDate *dope, const gchar *label,
							const gchar *ref, const gchar *account,
							const gchar *currency, const gchar *ledger,
							const gchar *model, ofxAmount debit, ofxAmount credit )
{
	ofoEntry *entry;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	if( !ofo_entry_is_valid_data(
			hub, deffect, dope, label, account, currency, ledger, model, debit, credit, NULL )){
		return( NULL );
	}

	entry = ofo_entry_new();

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

	entry_compute_status( entry, FALSE, hub );

	return( entry );
}

/**
 * ofo_entry_insert:
 *
 * Allocates a sequential number to the entry, and records it in the
 * dbms. Send the corresponding advertising messages if no error occurs.
 */
gboolean
ofo_entry_insert( ofoEntry *entry, ofaHub *hub )
{
	static const gchar *thisfn = "ofo_entry_insert";
	gboolean ok;
	ofoDossier *dossier;

	g_debug( "%s: entry=%p, hub=%p", thisfn, ( void * ) entry, ( void * ) hub );

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( !OFO_BASE( entry )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	dossier = ofa_hub_get_dossier( hub );

	entry_set_number( entry, ofo_dossier_get_next_entry( dossier ));
	entry_compute_status( entry, FALSE, hub );

	if( entry_do_insert( entry, hub )){
		ofo_base_set_hub( OFO_BASE( entry ), hub );
		if( ofo_entry_get_status( entry ) != ENT_STATUS_PAST ){
			g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_NEW, entry );
		}
		ok = TRUE;
	}

	return( ok );
}

static gboolean
entry_do_insert( ofoEntry *entry, ofaHub *hub )
{
	GString *query;
	gchar *label, *ref;
	gchar *sdeff, *sdope, *sdebit, *scredit, *userid;
	gboolean ok;
	GTimeVal stamp;
	gchar *stamp_str;
	const gchar *model, *cur_code;
	ofoCurrency *cur_obj;
	const ofaIDBConnect *connect;
	ofxCounter ope_number;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );

	cur_code = ofo_entry_get_currency( entry );
	cur_obj = ofo_currency_get_by_code( hub, cur_code );
	g_return_val_if_fail( cur_obj && OFO_IS_CURRENCY( cur_obj ), FALSE );

	connect = ofa_hub_get_connect( hub );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_entry_get_label( entry ));
	ref = my_utils_quote_sql( ofo_entry_get_ref( entry ));
	sdeff = my_date_to_str( ofo_entry_get_deffect( entry ), MY_DATE_SQL );
	sdope = my_date_to_str( ofo_entry_get_dope( entry ), MY_DATE_SQL );
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_ENTRIES " );

	g_string_append_printf( query,
			"	(ENT_DEFFECT,ENT_NUMBER,ENT_DOPE,ENT_LABEL,ENT_REF,ENT_ACCOUNT,"
			"	ENT_CURRENCY,ENT_LEDGER,ENT_OPE_TEMPLATE,"
			"	ENT_DEBIT,ENT_CREDIT,ENT_STATUS,ENT_OPE_NUMBER,"
			"	ENT_UPD_USER, ENT_UPD_STAMP) "
			"	VALUES ('%s',%ld,'%s','%s',",
			sdeff,
			ofo_entry_get_number( entry ),
			sdope,
			label );

	if( my_strlen( ref )){
		g_string_append_printf( query, "'%s',", ref );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
				"'%s','%s','%s',",
				ofo_entry_get_account( entry ),
				cur_code,
				ofo_entry_get_ledger( entry ));

	model = ofo_entry_get_ope_template( entry );
	if( my_strlen( model )){
		g_string_append_printf( query, "'%s',", model );
	} else {
		query = g_string_append( query, "NULL," );
	}

	sdebit = ofa_amount_to_sql( ofo_entry_get_debit( entry ), cur_obj );
	scredit = ofa_amount_to_sql( ofo_entry_get_credit( entry ), cur_obj );

	g_string_append_printf( query,
				"%s,%s,%d,",
				sdebit,
				scredit,
				ofo_entry_get_status( entry ));

	ope_number = ofo_entry_get_ope_number( entry );
	if( ope_number > 0 ){
		g_string_append_printf( query, "%ld,", ope_number );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
				"'%s','%s')",
				userid,
				stamp_str );

	if( ofa_idbconnect_query( connect, query->str, TRUE )){

		entry_set_upd_user( entry, userid );
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
	g_free( userid );

	return( ok );
}

static void
error_ledger( const gchar *ledger )
{
	gchar *str;

	str = g_strdup_printf( _( "Invalid ledger identifier: %s" ), ledger );
	my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );

	g_free( str );
}

static void
error_ope_template( const gchar *model )
{
	gchar *str;

	str = g_strdup_printf( _( "Invalid operation template identifier: %s" ), model );
	my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );

	g_free( str );
}

static void
error_currency( const gchar *currency )
{
	gchar *str;

	str = g_strdup_printf( _( "Invalid currency ISO 3A code: %s" ), currency );
	my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );

	g_free( str );
}

static void
error_acc_number( void )
{
	gchar *str;

	str = g_strdup( _( "Empty account number" ));
	my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );

	g_free( str );
}

static void
error_account( const gchar *number )
{
	gchar *str;

	str = g_strdup_printf( _( "Invalid account number: %s" ), number );
	my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );

	g_free( str );
}

static void
error_acc_currency( const gchar *currency, ofoAccount *account )
{
	ofaHub *hub;
	gchar *str;
	const gchar *acc_currency;
	ofoCurrency *acc_dev, *ent_dev;

	hub = ofo_base_get_hub( OFO_BASE( account ));
	acc_currency = ofo_account_get_currency( account );
	acc_dev = ofo_currency_get_by_code( hub, acc_currency );
	ent_dev = ofo_currency_get_by_code( hub, currency );

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
	my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );

	g_free( str );
}

static void
error_amounts( ofxAmount debit, ofxAmount credit )
{
	gchar *str;

	str = g_strdup_printf(
					"Invalid amounts: debit=%.lf, credit=%.lf: one and only one must be non zero",
					debit, credit );

	my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );

	g_free( str );
}

/**
 * ofo_entry_update:
 *
 * Update a rough entry.
 */
gboolean
ofo_entry_update( ofoEntry *entry )
{
	ofaHub *hub;
	gboolean ok;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( !OFO_BASE( entry )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( entry ));

	if( entry_do_update( entry, hub )){
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_UPDATED, entry, NULL );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
entry_do_update( ofoEntry *entry, ofaHub *hub )
{
	GString *query;
	gchar *sdeff, *sdope, *sdeb, *scre, *userid;
	gchar *stamp_str, *label, *ref;
	GTimeVal stamp;
	gboolean ok;
	const gchar *model, *cstr;
	const gchar *cur_code;
	ofoCurrency *cur_obj;
	const ofaIDBConnect *connect;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );

	cur_code = ofo_entry_get_currency( entry );
	cur_obj = ofo_currency_get_by_code( hub, cur_code );
	g_return_val_if_fail( cur_obj && OFO_IS_CURRENCY( cur_obj ), FALSE );

	connect = ofa_hub_get_connect( hub );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_entry_get_label( entry ));
	sdope = my_date_to_str( ofo_entry_get_dope( entry ), MY_DATE_SQL );
	sdeff = my_date_to_str( ofo_entry_get_deffect( entry ), MY_DATE_SQL );
	sdeb = ofa_amount_to_sql( ofo_entry_get_debit( entry ), cur_obj );
	scre = ofa_amount_to_sql( ofo_entry_get_credit( entry ), cur_obj );
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_ENTRIES " );

	g_string_append_printf( query,
			"	SET ENT_DEFFECT='%s',ENT_DOPE='%s',ENT_LABEL='%s',",
			sdeff, sdope,
			label );

	cstr = ofo_entry_get_ref( entry );
	ref = my_strlen( cstr ) ? my_utils_quote_sql( cstr ) : NULL;
	if( my_strlen( ref )){
		g_string_append_printf( query, " ENT_REF='%s',", ref );
	} else {
		query = g_string_append( query, " ENT_REF=NULL," );
	}

	g_string_append_printf( query,
			"	ENT_ACCOUNT='%s',ENT_CURRENCY='%s',ENT_LEDGER='%s',",
			ofo_entry_get_account( entry ),
			cur_code,
			ofo_entry_get_ledger( entry ));

	model = ofo_entry_get_ope_template( entry );
	if( !my_strlen( model )){
		query = g_string_append( query, " ENT_OPE_TEMPLATE=NULL," );
	} else {
		g_string_append_printf( query, " ENT_OPE_TEMPLATE='%s',", model );
	}

	g_string_append_printf( query,
			"	ENT_DEBIT=%s,ENT_CREDIT=%s,"
			"	ENT_UPD_USER='%s',ENT_UPD_STAMP='%s' "
			"	WHERE ENT_NUMBER=%ld",
			sdeb, scre, userid, stamp_str, ofo_entry_get_number( entry ));

	if( ofa_idbconnect_query( connect, query->str, TRUE )){
		entry_set_upd_user( entry, userid );
		entry_set_upd_stamp( entry, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( label );
	g_free( ref );
	g_free( sdeff );
	g_free( sdope );
	g_free( sdeb );
	g_free( scre );
	g_free( stamp_str );
	g_free( userid );

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
ofo_entry_update_settlement( ofoEntry *entry, ofxCounter number )
{
	ofaHub *hub;
	gboolean ok;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( !OFO_BASE( entry )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( entry ));

	if( do_update_settlement( entry, ofa_hub_get_connect( hub ), number )){
		g_signal_emit_by_name( hub, SIGNAL_HUB_UPDATED, entry, NULL );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
do_update_settlement( ofoEntry *entry, const ofaIDBConnect *connect, ofxCounter number )
{
	GString *query;
	gchar *stamp_str, *userid;
	GTimeVal stamp;
	gboolean ok;

	userid = ofa_idbconnect_get_account( connect );

	query = g_string_new( "UPDATE OFA_T_ENTRIES SET " );

	if( number > 0 ){
		ofo_entry_set_settlement_number( entry, number );
		entry_set_settlement_user( entry, userid );
		entry_set_settlement_stamp( entry, my_utils_stamp_set_now( &stamp ));

		stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );
		g_string_append_printf( query,
				"ENT_STLMT_NUMBER=%ld,ENT_STLMT_USER='%s',ENT_STLMT_STAMP='%s' ",
				number, userid, stamp_str );
		g_free( stamp_str );

	} else {
		ofo_entry_set_settlement_number( entry, 0 );
		entry_set_settlement_user( entry, NULL );
		entry_set_settlement_stamp( entry, NULL );

		g_string_append_printf( query,
				"ENT_STLMT_NUMBER=NULL,ENT_STLMT_USER=NULL,ENT_STLMT_STAMP=NULL " );
	}

	g_string_append_printf( query, "WHERE ENT_NUMBER=%ld", ofo_entry_get_number( entry ));
	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	g_string_free( query, TRUE );
	g_free( userid );

	return( ok );
}

/**
 * ofo_entry_unsettle_by_number:
 * @hub: the current #ofaHub object.
 * @number: the identifier of the settlement group to be cancelled.
 *
 * Cancel the identified settlement group by updating all member
 * entries. Each entry will receive a 'updated' message through the
 * dossier signaling system
 */
void
ofo_entry_unsettle_by_number( ofaHub *hub, ofxCounter number )
{
	GList *entries, *it;
	gchar *where;

	g_return_if_fail( hub && OFA_IS_HUB( hub ));
	g_return_if_fail( number > 0 );

	/* get the list of entries */
	where = g_strdup_printf( "ENT_STLMT_NUMBER=%ld", number );
	entries = entry_load_dataset( hub, where, NULL );
	g_free( where );

	/* update the entries, simultaneously sending messages */
	for( it=entries ; it ; it=it->next ){
		ofo_entry_update_settlement( OFO_ENTRY( it->data ), 0 );
	}
	ofo_entry_free_dataset( entries );
}

/**
 * ofo_entry_validate:
 *
 * entry must be in 'rough' status
 */
gboolean
ofo_entry_validate( ofoEntry *entry )
{
	ofaHub *hub;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( !OFO_BASE( entry )->prot->dispose_has_run, FALSE );

	hub = ofo_base_get_hub( OFO_BASE( entry ));
	g_signal_emit_by_name( hub,
			SIGNAL_HUB_STATUS_CHANGE, entry, ENT_STATUS_ROUGH, ENT_STATUS_VALIDATED );

	return( TRUE );
}

/**
 * ofo_entry_validate_by_ledger:
 * @hub: the current #ofaHub object.
 * @mnemo: the #ofoLedger identifier.
 * @deffect: the effect date.
 *
 * Validate all rough entries which are imputed on the specified @mnemo
 * ledger, until up an dincluding the @deffect effect date.
 *
 * Returns: TRUE if success, even if there is no entries at all, while
 * no error is detected.
 */
gboolean
ofo_entry_validate_by_ledger( ofaHub *hub, const gchar *mnemo, const GDate *deffect )
{
	gchar *query, *sdate;
	ofoEntry *entry;
	GList *dataset, *it;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

	sdate = my_date_to_str( deffect, MY_DATE_SQL );
	query = g_strdup_printf(
					"OFA_T_ENTRIES WHERE ENT_LEDGER='%s' AND ENT_STATUS=%d AND ENT_DEFFECT<='%s'",
					mnemo, ENT_STATUS_ROUGH, sdate );
	g_free( sdate );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					query,
					OFO_TYPE_ENTRY,
					hub );

	g_free( query );

	g_signal_emit_by_name( hub, SIGNAL_HUB_STATUS_COUNT, ENT_STATUS_VALIDATED, g_list_length( dataset ));

	for( it=dataset ; it ; it=it->next ){
		entry = OFO_ENTRY( it->data );
		g_signal_emit_by_name( hub,
				SIGNAL_HUB_STATUS_CHANGE, entry, ENT_STATUS_ROUGH, ENT_STATUS_VALIDATED );
	}

	ofo_entry_free_dataset( dataset );

	return( TRUE );
}

/**
 * ofo_entry_delete:
 */
gboolean
ofo_entry_delete( ofoEntry *entry )
{
	ofaHub *hub;
	gboolean ok;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( !OFO_BASE( entry )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( entry ));

	if( do_delete_entry( entry, ofa_hub_get_connect( hub ))){
		g_signal_emit_by_name(
				hub, SIGNAL_HUB_DELETED, entry );
		g_signal_emit_by_name(
				hub, SIGNAL_HUB_STATUS_CHANGE, entry, ENT_STATUS_ROUGH, ENT_STATUS_DELETED );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
do_delete_entry( ofoEntry *entry, const ofaIDBConnect *connect )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
				"UPDATE OFA_T_ENTRIES SET "
				"	ENT_STATUS=%d WHERE ENT_NUMBER=%ld",
						ENT_STATUS_DELETED, ofo_entry_get_number( entry ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );

}

/*
 * ofaIConcil interface management
 */
static void
iconcil_iface_init( ofaIConcilInterface *iface )
{
	static const gchar *thisfn = "ofo_entry_iconcil_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iconcil_get_interface_version;
	iface->get_object_id = iconcil_get_object_id;
	iface->get_object_type = iconcil_get_object_type;
}

static guint
iconcil_get_interface_version( void )
{
	return( 1 );
}

static ofxCounter
iconcil_get_object_id( const ofaIConcil *instance )
{
	return( ofo_entry_get_number( OFO_ENTRY( instance )));
}

static const gchar *
iconcil_get_object_type( const ofaIConcil *instance )
{
	return( CONCIL_TYPE_ENTRY );
}

/*
 * ofaIExportable interface management
 */
static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofo_entry_iexportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iexportable_get_interface_version;
	iface->get_label = iexportable_get_label;
	iface->export = iexportable_export;
}

static guint
iexportable_get_interface_version( void )
{
	return( 1 );
}

static gchar *
iexportable_get_label( const ofaIExportable *instance )
{
	return( g_strdup( _( "_Entries" )));
}

/*
 * iexportable_export:
 *
 * Exports the entries line by line.
 *
 * Returns: TRUE at the end if no error has been detected
 *
 * As a first - probably bad - approach, we load all the entries in
 * memory ! An alternative may be to use a cursor, but this later is
 * only available from a stored program in the DBMS (as for MySQL at
 * least), and this would imply that the exact list of columns be
 * written in this stored program ?
 *
 * v0.38: as the conciliation information have moved to another table,
 * and because we want to stay able to export/import them, we have to
 * add to the dataset the informations got from conciliation groups.
 *
 * v0.56: introduce a format version number prefix (for now: 1).
 * Starting from now, the added conciliation group comes just after this
 * version number, and the entry comes after. This let us add new fields
 * at the end of the entry.
 */
static gboolean
iexportable_export( ofaIExportable *exportable, ofaStreamFormat *settings, ofaHub *hub )
{
	GList *result, *it;
	gboolean ok, with_headers;
	gchar field_sep;
	gulong count;
	gchar *str, *str2, *sdate, *suser, *sstamp;
	ofoConcil *concil;
	const gchar *acc_id, *cur_code;
	ofoAccount *account;
	ofoCurrency *currency;

	result = entry_load_dataset( hub, NULL, NULL );

	with_headers = ofa_stream_format_get_with_headers( settings );
	field_sep = ofa_stream_format_get_field_sep( settings );

	count = ( gulong ) g_list_length( result );
	if( with_headers ){
		count += 1;
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = ofa_box_csv_get_header( st_boxed_defs, settings );
		str2 = g_strdup_printf( "%s%c%s%c%s%c%s%c%s",
				"Version", field_sep,
				"ConcilDval", field_sep, "ConcilUser", field_sep, "ConcilStamp", field_sep,
				str );
		ok = ofa_iexportable_set_line( exportable, str2 );
		g_free( str2 );
		g_free( str );
		if( !ok ){
			return( FALSE );
		}
	}

	for( it=result ; it ; it=it->next ){
		acc_id = ofo_entry_get_account( OFO_ENTRY( it->data ));
		g_return_val_if_fail( acc_id && my_strlen( acc_id ), FALSE );
		account = ofo_account_get_by_number( hub, acc_id );
		g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
		cur_code = ofo_account_get_currency( account );
		g_return_val_if_fail( cur_code && my_strlen( cur_code ), FALSE );
		currency = ofo_currency_get_by_code( hub, cur_code );
		g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), FALSE );
		str = ofa_box_csv_get_line_ex( OFO_BASE( it->data )->prot->fields, settings, ( CSVExportFunc ) export_cb, currency );

		concil = ofa_iconcil_get_concil( OFA_ICONCIL( it->data ));
		sdate = concil ? my_date_to_str( ofo_concil_get_dval( concil ), MY_DATE_SQL ) : g_strdup( "" );
		suser = g_strdup( concil ? ofo_concil_get_user( concil ) : "" );
		sstamp = concil ? my_utils_stamp_to_str( ofo_concil_get_stamp( concil ), MY_STAMP_YYMDHMS ) : g_strdup( "" );

		str2 = g_strdup_printf( "%u%c%s%c%s%c%s%c%s",
				ENTRY_IE_FORMAT, field_sep,
				sdate, field_sep, suser, field_sep, sstamp, field_sep,
				str );
		ok = ofa_iexportable_set_line( exportable, str2 );
		g_free( str2 );
		g_free( str );
		g_free( sdate );
		g_free( suser );
		g_free( sstamp );
		if( !ok ){
			return( FALSE );
		}
	}

	ofo_entry_free_dataset( result );

	return( TRUE );
}

/*
 * a callback to adjust the decimal digits count to the precision of the
 * currency of the account of the entry
 */
static gchar *
export_cb( const ofsBoxData *box_data, ofaStreamFormat *format, const gchar *text, ofoCurrency *currency )
{
	const ofsBoxDef *box_def;
	gchar *str;

	box_def = ofa_box_data_get_def( box_data );
	if( box_def->type == OFA_TYPE_AMOUNT ){
		str = ofa_amount_to_csv( ofa_box_data_get_amount( box_data ), currency, format );
	} else {
		str = g_strdup( text );
	}
	return( str );
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
	iface->get_label = iimportable_get_label;
	iface->import = iimportable_import;
}

static guint
iimportable_get_interface_version( void )
{
	return( 1 );
}

static gchar *
iimportable_get_label( const ofaIImportable *instance )
{
	return( iexportable_get_label( OFA_IEXPORTABLE( instance )));
}

/*
 * ofo_entry_iimportable_import:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - maybe a format number (else format=0)
 * If format >= 1
 *   - reconciliation date: yyyy-mm-dd
 *   - exported reconciliation user (defaults to current user)
 *   - exported reconciliation timestamp (defaults to now)
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
 * - ope.number (starting with format=1)
 * - settlement: "True" or a settlement number if the entry has been
 *   settled, or empty
 * - ignored (settlement user on export)
 * - ignored (settlement timestamp on export)
 * - ignored (entry number on export)
 * - ignored (entry status on export)
 * - ignored (creation user on export)
 * - ignored (creation timestamp on export)
 * If format = 0
 * - reconciliation date: yyyy-mm-dd
 * - exported reconciliation user (defaults to current user)
 * - exported reconciliation timestamp (defaults to now)
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
 *
 * Returns: the total count of errors.
 *
 * As the table may have been dropped between import phase and insert
 * phase, if an error occurs during insert phase, then the table is
 * changed and only contains the successfully inserted records.
 */
static guint
iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines )
{
	GList *dataset;
	gchar *bck_table;

	dataset = iimportable_import_parse( importer, parms, lines );

	if( parms->parse_errs == 0 && parms->parsed_count > 0 ){
		bck_table = ofa_idbconnect_table_backup( ofa_hub_get_connect( parms->hub ), "OFA_T_ENTRIES" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs > 0 ){
			ofa_idbconnect_table_restore( ofa_hub_get_connect( parms->hub ), bck_table, "OFA_T_ENTRIES" );
		}

		g_free( bck_table );
	}

	if( dataset ){
		g_list_free_full( dataset, ( GDestroyNotify ) g_object_unref );
	}

	return( parms->parse_errs+parms->insert_errs );
}

static GList *
iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines )
{
	static const gchar *thisfn = "ofo_entry_iimportable_import";
	GList *dataset;
	const gchar *cstr;
	GSList *itl, *fields, *itf;
	ofoEntry *entry;
	guint numline, total, format;
	GDate date;
	gchar *currency, *str, *sdeb, *scre;
	ofoAccount *account;
	ofoLedger *ledger;
	gdouble debit, credit;
	ofaEntryStatus status;
	GList *past, *exe, *fut, *it;
	ofsCurrency *sdet;
	ofoCurrency *cur_object;
	ofxCounter counter;
	myDateFormat date_format;
	ofoDossier *dossier;

	numline = 0;
	dataset = NULL;
	total = g_slist_length( lines );

	ofa_iimporter_progress_start( importer, parms );

	past = NULL;
	exe = NULL;
	fut = NULL;
	date_format = ofa_stream_format_get_date_format( parms->format );
	dossier = ofa_hub_get_dossier( parms->hub );

	for( itl=lines ; itl ; itl=itl->next ){

		if( parms->stop && parms->parse_errs > 0 ){
			break;
		}

		numline += 1;
		fields = ( GSList * ) itl->data;
		entry = ofo_entry_new();
		debit = 0;
		credit = 0;

		/* first field is a version number or the operation date */
		itf = fields;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		my_date_set_from_str( &date, cstr, date_format );
		format = atoi( cstr );

		/* valid format >= 1 */
		if( !my_date_is_valid( &date ) && format > 0 && format <= ENTRY_IE_FORMAT ){

			/* conciliation group */
			iimportable_import_concil( importer, parms, entry, &itf );

			/* operation date */
			itf = itf ? itf->next : NULL;
			cstr = itf ? ( const gchar * ) itf->data : NULL;
			my_date_set_from_str( &date, cstr, date_format );
			if( !my_date_is_valid( &date )){
				str = g_strdup_printf( _( "invalid entry operation date: %s" ), cstr );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				g_free( str );
				parms->parse_errs += 1;
				continue;
			}
			ofo_entry_set_dope( entry, &date );

		/* valid format = 0 */
		} else if( my_date_is_valid( &date )){
			ofo_entry_set_dope( entry, &date );

		} else {
			str = g_strdup_printf( _( "invalid first field while version number or operation date was expected: %s" ), cstr );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_free( str );
			parms->parse_errs += 1;
			continue;
		}

		/* effect date */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		my_date_set_from_str( &date, cstr, date_format );
		if( !my_date_is_valid( &date )){
			str = g_strdup_printf( _( "invalid entry effect date: %s" ), cstr );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_free( str );
			parms->parse_errs += 1;
			continue;
		}
		ofo_entry_set_deffect( entry, &date );

		/* entry label */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty entry label" ));
			parms->parse_errs += 1;
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

		/* ledger - default is from the dossier */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			str = g_strdup( ofo_dossier_get_import_ledger( dossier ));
			if( !my_strlen( str )){
				ofa_iimporter_progress_num_text( importer, parms, numline, _( "dossier is missing a default import ledger" ));
				parms->parse_errs += 1;
				continue;
			}
		}
		ledger = ofo_ledger_get_by_mnemo( parms->hub, cstr );
		if( !ledger ){
			str = g_strdup_printf( _( "entry ledger not found: %s" ), cstr );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_free( str );
			parms->parse_errs += 1;
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
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty entry account" ));
			parms->parse_errs += 1;
			continue;
		}
		account = ofo_account_get_by_number( parms->hub, cstr );
		if( !account ){
			str = g_strdup_printf( _( "entry account not found: %s" ), cstr );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_free( str );
			parms->parse_errs += 1;
			continue;
		}
		if( ofo_account_is_root( account )){
			str = g_strdup_printf( _( "entry account is a root account: %s" ), cstr );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_free( str );
			parms->parse_errs += 1;
			continue;
		}
		if( ofo_account_is_closed( account )){
			str = g_strdup_printf( _( "entry account is closed: %s" ), cstr );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_free( str );
			parms->parse_errs += 1;
			continue;
		}
		ofo_entry_set_account( entry, cstr );

		cstr = ofo_account_get_currency( account );
		if( !my_strlen( currency )){
			g_free( currency );
			currency = g_strdup( cstr );
		} else if( my_collate( currency, cstr )){
			str = g_strdup_printf(
					_( "entry currency: %s is not the same than those of the account: %s" ),
					currency, cstr );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_free( str );
			parms->parse_errs += 1;
			continue;
		}
		cur_object = ofo_currency_get_by_code( parms->hub, currency );
		if( !cur_object ){
			str = g_strdup_printf( _( "unregistered currency: %s" ), currency );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_free( str );
			parms->parse_errs += 1;
			continue;
		}
		ofo_entry_set_currency( entry, currency );

		/* debit */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		debit = my_double_set_from_csv( cstr, ofa_stream_format_get_decimal_sep( parms->format ));

		/* credit */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		credit = my_double_set_from_csv( cstr, ofa_stream_format_get_decimal_sep( parms->format ));

		if( 0 ){
			g_debug( "%s: debit=%.2lf, credit=%.2lf", thisfn, debit, credit );
		}
		if(( debit && !credit ) || ( !debit && credit )){
			ofo_entry_set_debit( entry, debit );
			ofo_entry_set_credit( entry, credit );
		} else {
			str = g_strdup_printf(
					_( "invalid entry amounts: debit=%'.5lf, credit=%'.5lf" ), debit, credit );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_free( str );
			parms->parse_errs += 1;
			continue;
		}

		/* format >= 1: operation number */
		if( format >= 1 ){
			itf = itf ? itf->next : NULL;
			cstr = itf ? ( const gchar * ) itf->data : NULL;
			counter = my_strlen( cstr ) ? atol( cstr ) : 0;
			if( counter ){
				ofo_entry_set_ope_number( entry, counter );
			}
		}

		/* settlement (number or True)
		 * do not allocate a settlement number from the dossier here
		 * in case where the entries import would not be inserted */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr ) && ofo_account_is_settleable( account )){
			counter = atol( cstr );
			if( counter ){
				entry_set_import_settled( entry, TRUE );
			} else {
				entry_set_import_settled( entry, my_utils_boolean_from_str( cstr ));
			}
		}

		/* ignored (settlement user from export) */
		itf = itf ? itf->next : NULL;

		/* ignored (settlement timestamp from export) */
		itf = itf ? itf->next : NULL;

		/* ignored (entry number from export) */
		itf = itf ? itf->next : NULL;

		/* ignored (entry status from export) */
		itf = itf ? itf->next : NULL;

		/* ignored (creation user from export) */
		itf = itf ? itf->next : NULL;

		/* ignored (creation timestamp from export) */
		itf = itf ? itf->next : NULL;

		if( format == 0 ){
			iimportable_import_concil( importer, parms, entry, &itf );
		}

		/* what to do regarding the effect date ?
		 * we force it to be valid regarding exercice beginning and
		 * ledger last closing dates, so that the entry is in ROUGH
		 * status */
		entry_compute_status( entry, TRUE, parms->hub );
		status = ofo_entry_get_status( entry );
		switch( status ){
			case ENT_STATUS_PAST:
				ofs_currency_add_by_object( &past, cur_object, debit, credit );
				break;

			case ENT_STATUS_ROUGH:
				ofs_currency_add_by_object( &exe, cur_object, debit, credit );
				break;

			case ENT_STATUS_FUTURE:
				ofs_currency_add_by_object( &fut, cur_object, debit, credit );
				break;

			default:
				str = g_strdup_printf( _( "invalid entry status: %d" ), status );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				g_free( str );
				parms->parse_errs += 1;
				continue;
		}

		g_free( currency );

		dataset = g_list_prepend( dataset, entry );
		parms->parsed_count += 1;
		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->parsed_count, ( gulong ) total );
	}

	/* rough and future entries must be balanced:
	 * as we are storing 5 decimal digits in the DBMS, so this is the
	 * maximal rounding error accepted */
	for( it=past ; it ; it=it->next ){
		sdet = ( ofsCurrency * ) it->data;
		sdeb = ofa_amount_to_str( sdet->debit, sdet->currency );
		scre = ofa_amount_to_str( sdet->credit, sdet->currency );
		str = g_strdup_printf( "PAST [%s] tot_debits=%s, tot_credits=%s",
					ofo_currency_get_code( sdet->currency ), sdeb, scre );
		ofa_iimporter_progress_num_text( importer, parms, numline, str );
		g_free( str );
		g_free( sdeb );
		g_free( scre );
	}
	for( it=exe ; it ; it=it->next ){
		sdet = ( ofsCurrency * ) it->data;
		sdeb = ofa_amount_to_str( sdet->debit, sdet->currency );
		scre = ofa_amount_to_str( sdet->credit, sdet->currency );
		str = g_strdup_printf( "EXE [%s] tot_debits=%s, tot_credits=%s",
					ofo_currency_get_code( sdet->currency ), sdeb, scre );
		ofa_iimporter_progress_num_text( importer, parms, numline, str );
		g_free( str );
		g_free( sdeb );
		g_free( scre );
		if( !ofs_currency_is_balanced( sdet )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "entries for the current exercice are not balanced" ));
			parms->parse_errs += 1;
		}
	}
	for( it=fut ; it ; it=it->next ){
		sdet = ( ofsCurrency * ) it->data;
		sdeb = ofa_amount_to_str( sdet->debit, sdet->currency );
		scre = ofa_amount_to_str( sdet->credit, sdet->currency );
		str = g_strdup_printf( "FUTURE [%s] tot_debits=%s, tot_credits=%s",
					ofo_currency_get_code( sdet->currency ), sdeb, scre );
		ofa_iimporter_progress_num_text( importer, parms, numline, str );
		g_free( str );
		g_free( sdeb );
		g_free( scre );
		if( !ofs_currency_is_balanced( sdet )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "entries in the future are not balanced" ));
			parms->parse_errs += 1;
		}
	}

	ofs_currency_list_free( &past );
	ofs_currency_list_free( &exe );
	ofs_currency_list_free( &fut );

	return( dataset );
}

/*
 * import conciliation informations
 * which happend to be at the end of the line (format=0) or at the start
 * of the line (format>=1)
 */
static void
iimportable_import_concil( ofaIImporter *importer, ofsImporterParms *parms, ofoEntry *entry, GSList **fields )
{
	static const gchar *thisfn = "ofo_entry_iimportable_import_concil";
	GSList *itf;
	const gchar *cstr;
	GDate date;
	GTimeVal stamp;
	ofoConcil *concil;
	gchar *userid;
	const ofaIDBConnect *connect;
	myDateFormat date_format;

	concil = NULL;
	itf = *fields;
	date_format = ofa_stream_format_get_date_format( parms->format );
	connect = ofa_hub_get_connect( parms->hub );

	/* reconciliation date */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	my_date_set_from_str( &date, cstr, date_format );
	if( my_date_is_valid( &date )){
		concil = ofo_concil_new();
		g_object_set_data( G_OBJECT( entry ), "entry-concil", concil );
		ofo_concil_set_dval( concil, &date );
		g_debug( "%s: new concil dval=%s", thisfn, cstr );
	}

	/* exported reconciliation user (defaults to current user) */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( concil ){
		userid = g_strdup( cstr );
		if( !my_strlen( userid )){
			g_free( userid );
			userid = ofa_idbconnect_get_account( connect );
		}
		ofo_concil_set_user( concil, userid );
		g_debug( "%s: new concil user=%s", thisfn, userid );
		g_free( userid );
	}

	/* exported reconciliation timestamp (defaults to now) */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( concil ){
		if( !my_strlen( cstr )){
			my_utils_stamp_set_now( &stamp );
		} else {
			my_utils_stamp_set_from_str( &stamp, cstr );
		}
		ofo_concil_set_stamp( concil, &stamp );
		g_debug( "%s: new concil stamp=%s", thisfn, cstr );
	}

	*fields = itf;
}

static void
iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset )
{
	GList *it;
	const ofaIDBConnect *connect;
	guint total;
	ofoEntry *entry;
	ofoDossier *dossier;
	ofxCounter counter;
	ofoConcil *concil;

	total = g_list_length( dataset );
	dossier = ofa_hub_get_dossier( parms->hub );
	connect = ofa_hub_get_connect( parms->hub );
	ofa_iimporter_progress_start( importer, parms );

	if( parms->empty && total > 0 ){
		entry_drop_content( connect );
	}

	for( it=dataset ; it ; it=it->next ){

		if( parms->stop && parms->insert_errs > 0 ){
			break;
		}

		entry = OFO_ENTRY( it->data );
		entry_set_number( entry, ofo_dossier_get_next_entry( dossier ));

		if( entry_do_insert( entry, parms->hub )){
			ofo_base_set_hub( OFO_BASE( entry ), parms->hub );

			if( entry_get_import_settled( entry )){
				counter = ofo_dossier_get_next_settlement( dossier );
				ofo_entry_update_settlement( entry, counter );
			}
			concil = ( ofoConcil * ) g_object_get_data( G_OBJECT( entry ), "entry-concil" );
			if( concil ){
				/* gives the ownership to the collection */
				ofa_iconcil_new_concil_ex( OFA_ICONCIL( entry ), concil );
			}
			if( ofo_entry_get_status( entry ) != ENT_STATUS_PAST ){
				g_signal_emit_by_name( G_OBJECT( parms->hub ), SIGNAL_HUB_NEW, entry );
			}
			parms->inserted_count += 1;

		} else {
			parms->insert_errs += 1;
		}

		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->inserted_count, ( gulong ) total );
	}
}

static gboolean
entry_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM OFA_T_ENTRIES", TRUE ));
}

/*
 * ofaISignalHub interface management
 */
static void
isignal_hub_iface_init( ofaISignalHubInterface *iface )
{
	static const gchar *thisfn = "ofo_entry_isignal_hub_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect = isignal_hub_connect;
}

static void
isignal_hub_connect( ofaHub *hub )
{
	static const gchar *thisfn = "ofo_entry_isignal_hub_connect";

	g_debug( "%s: hub=%p", thisfn, ( void * ) hub );

	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	g_signal_connect( hub, SIGNAL_HUB_DELETABLE, G_CALLBACK( hub_on_deletable_object ), NULL );
	g_signal_connect( hub, SIGNAL_HUB_DELETED, G_CALLBACK( hub_on_deleted_object ), NULL );
	g_signal_connect( hub, SIGNAL_HUB_EXE_DATES_CHANGED, G_CALLBACK( hub_on_exe_dates_changed ), NULL );
	g_signal_connect( hub, SIGNAL_HUB_STATUS_CHANGE, G_CALLBACK( hub_on_entry_status_change ), NULL );
	g_signal_connect( hub, SIGNAL_HUB_UPDATED, G_CALLBACK( hub_on_updated_object ), NULL );
}

/*
 * SIGNAL_HUB_DELETABLE signal handler
 */
static gboolean
hub_on_deletable_object( ofaHub *hub, ofoBase *object, void *empty )
{
	static const gchar *thisfn = "ofo_entry_hub_on_deletable_object";
	gboolean deletable;

	g_debug( "%s: hub=%p, object=%p (%s), empty=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	deletable = TRUE;

	if( OFO_IS_ACCOUNT( object )){
		deletable = hub_is_deletable_account( hub, OFO_ACCOUNT( object ));

	} else if( OFO_IS_CURRENCY( object )){
		deletable = hub_is_deletable_currency( hub, OFO_CURRENCY( object ));

	} else if( OFO_IS_LEDGER( object )){
		deletable = hub_is_deletable_ledger( hub, OFO_LEDGER( object ));

	} else if( OFO_IS_OPE_TEMPLATE( object )){
		deletable = hub_is_deletable_ope_template( hub, OFO_OPE_TEMPLATE( object ));
	}

	return( deletable );
}

static gboolean
hub_is_deletable_account( ofaHub *hub, ofoAccount *account )
{
	return( hub_is_deletable_account_by_mnemo( hub, ofo_account_get_number( account )));
}

static gboolean
hub_is_deletable_account_by_mnemo( ofaHub *hub, const gchar *mnemo )
{
	gchar *query;
	gint count;

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM OFA_T_ENTRIES WHERE ENT_ACCOUNT='%s'",
			mnemo );

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count == 0 );
}

static gboolean
hub_is_deletable_currency( ofaHub *hub, ofoCurrency *currency )
{
	gchar *query;
	gint count;

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM OFA_T_ENTRIES WHERE ENT_CURRENCY='%s'",
			ofo_currency_get_code( currency ));

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count == 0 );
}

static gboolean
hub_is_deletable_ledger( ofaHub *hub, ofoLedger *ledger )
{
	return( hub_is_deletable_ledger_by_mnemo( hub, ofo_ledger_get_mnemo( ledger )));
}

static gboolean
hub_is_deletable_ledger_by_mnemo( ofaHub *hub, const gchar *mnemo )
{
	gchar *query;
	gint count;

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM OFA_T_ENTRIES WHERE ENT_LEDGER='%s'",
			mnemo );

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count == 0 );
}

static gboolean
hub_is_deletable_ope_template( ofaHub *hub, ofoOpeTemplate *template )
{
	gchar *query;
	gint count;

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM OFA_T_ENTRIES WHERE ENT_OPE_TEMPLATE='%s'",
			ofo_ope_template_get_mnemo( template ));

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count == 0 );
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
hub_on_deleted_object( const ofaHub *hub, ofoBase *object, void *empty )
{
	static const gchar *thisfn = "ofo_entry_hub_on_deleted_object";

	g_debug( "%s: hub=%p, object=%p (%s), empty=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	/* what is to do on the entries when a conciliation group is deleted ? */
	/*
	if( OFO_IS_CONCIL( object )){
		g_warning( "%s: conciliation group deleted: should update the entries", thisfn );
	}
	*/
}

/*
 * SIGNAL_HUB_EXE_DATES_CHANGED signal handler.
 *
 * The cases of remediation:
 *
 * 1/ entries were considered in the past, but are now in the exercice
 *    depending if the ledger is closed or not for the effect date of
 *    the entry, these entries will become rough or validated
 *
 * 2/ entries were considered in the past, but are now in the future
 *
 * 3/ entries were considered in the exercice, but are now in the past
 *    these entries will become past
 *    depending if the ledger is closed or not for the effect date of
 *    the entry, the account and ledger rough/validated balances will
 *    be updated
 *
 * 4/ entries were considered in the exercice, but are not in the future
 *    these entries will become future
 *    depending if the ledger is closed or not for the effect date of
 *    the entry, the account and ledger rough/validated balances will
 *    be updated
 *
 * 5/ these entries were considered in the future, but are now considered
 *    in the exercice
 *    depending if the ledger is closed or not for the effect date of
 *    the entry, these entries will become rough or validated
 *
 * 6/ entries were considered in the future, but are now set in the past
 */
static void
hub_on_exe_dates_changed( ofaHub *hub, const GDate *prev_begin, const GDate *prev_end, void *empty )
{
	ofoDossier *dossier;
	const GDate *new_begin, *new_end;

	dossier = ofa_hub_get_dossier( hub );

	new_begin = ofo_dossier_get_exe_begin( dossier );
	check_for_changed_begin_exe_dates( hub, prev_begin, new_begin, TRUE );

	new_end = ofo_dossier_get_exe_end( dossier );
	check_for_changed_end_exe_dates( hub, prev_end, new_end, TRUE );
}

static gint
check_for_changed_begin_exe_dates( ofaHub *hub, const GDate *prev_begin, const GDate *new_begin, gboolean remediate )
{
	gint count;
	gchar *sprev, *snew, *where;

	count = 0;
	sprev = my_date_to_str( prev_begin, MY_DATE_SQL );
	snew = my_date_to_str( new_begin, MY_DATE_SQL );
	where = NULL;

	if( !my_date_is_valid( prev_begin )){
		if( !my_date_is_valid( new_begin )){
			/* nothing to do here */

		} else {
			/* setting a beginning date for the exercice
			 * there may be entries which were considered in the
			 * exercice (either rough or validated) but are now
			 * considered in the past */
			/*count = move_from_exercice_to_past( dossier, prev_begin, new_begin, remediate );*/
			where = g_strdup_printf(
					"ENT_DEFFECT<'%s' AND ENT_STATUS!=%u", snew, ENT_STATUS_DELETED );
			count = remediate_status( hub, remediate, where, ENT_STATUS_PAST );
		}
	} else if( !my_date_is_valid( new_begin )){
		/* removing the beginning date of the exercice
		 * there may be entries which were considered in the past
		 * but are now considered in the exercice */
		/*count = move_from_past_to_exercice( dossier, prev_begin, new_begin, remediate );*/
		where = g_strdup_printf(
				"ENT_DEFFECT<'%s' AND ENT_STATUS!=%u", sprev, ENT_STATUS_DELETED );
		count = remediate_status( hub, remediate, where, ENT_STATUS_ROUGH );

	} else if( my_date_compare( prev_begin, new_begin ) < 0 ){
		/* there may be entries which were considered in the exercice
		 * but are now considered in the past */
		/*count = move_from_exercice_to_past( dossier, prev_begin, new_begin, remediate );*/
		where = g_strdup_printf(
				"ENT_DEFFECT>='%s' AND ENT_DEFFECT<'%s' AND ENT_STATUS!=%u",
				sprev, snew, ENT_STATUS_DELETED );
		count = remediate_status( hub, remediate, where, ENT_STATUS_PAST );

	} else if( my_date_compare( prev_begin, new_begin ) > 0 ){
		/* there may be entries which were considered in the past
		 * but are now considered in the exercice */
		/*count = move_from_past_to_exercice( dossier, prev_begin, new_begin, remediate );*/
		where = g_strdup_printf(
				"ENT_DEFFECT<'%s' AND ENT_DEFFECT>='%s' AND ENT_STATUS!=%u",
				sprev, snew, ENT_STATUS_DELETED );
		count = remediate_status( hub, remediate, where, ENT_STATUS_ROUGH );
	}

	g_free( sprev );
	g_free( snew );
	g_free( where );

	return( count );
}

static gint
check_for_changed_end_exe_dates( ofaHub *hub, const GDate *prev_end, const GDate *new_end, gboolean remediate )
{
	gint count;
	gchar *sprev, *snew, *where;

	count = 0;
	sprev = my_date_to_str( prev_end, MY_DATE_SQL );
	snew = my_date_to_str( new_end, MY_DATE_SQL );
	where = NULL;

	if( !my_date_is_valid( prev_end )){
		if( !my_date_is_valid( new_end )){
			/* nothing to do here */

		} else {
			/* setting an ending date for the exercice
			 * there may be entries which were considered in the
			 * exercice (either rough or validated) but are now
			 * considered in the future */
			/*count = move_from_exercice_to_future( dossier, prev_end, new_end, remediate );*/
			where = g_strdup_printf(
					"ENT_DEFFECT>'%s' AND ENT_STATUS!=%u", snew, ENT_STATUS_DELETED );
			count = remediate_status( hub, remediate, where, ENT_STATUS_FUTURE );
		}
	} else if( !my_date_is_valid( new_end )){
		/* removing the ending date of the exercice
		 * there may be entries which were considered in the future
		 * but are now considered in the exercice */
		/*count = move_from_future_to_exercice( dossier, prev_end, new_end, remediate );*/
		where = g_strdup_printf(
				"ENT_DEFFECT>'%s' AND ENT_STATUS!=%u", sprev, ENT_STATUS_DELETED );
		count = remediate_status( hub, remediate, where, ENT_STATUS_ROUGH );

	} else if( my_date_compare( prev_end, new_end ) < 0 ){
		/* there may be entries which were considered in the future
		 * but are now considered in the exercice */
		/*count = move_from_future_to_exercice( dossier, prev_end, new_end, remediate );*/
		where = g_strdup_printf(
				"ENT_DEFFECT>'%s' AND ENT_DEFFECT<='%s' AND ENT_STATUS!=%u",
				sprev, snew, ENT_STATUS_DELETED );
		count = remediate_status( hub, remediate, where, ENT_STATUS_ROUGH );

	} else if( my_date_compare( prev_end, new_end ) > 0 ){
		/* there may be entries which were considered in the exercice
		 * but are now considered in the future */
		/*count = move_from_exercice_to_future( dossier, prev_end, new_end, remediate );*/
		where = g_strdup_printf(
				"ENT_DEFFECT<='%s' AND ENT_DEFFECT>'%s' AND ENT_STATUS!=%u",
				sprev, snew, ENT_STATUS_DELETED );
		count = remediate_status( hub, remediate, where, ENT_STATUS_FUTURE );
	}

	return( count );
}

static gint
remediate_status( ofaHub *hub, gboolean remediate, const gchar *where, ofaEntryStatus new_status )
{
	static const gchar *thisfn = "ofo_entry_remediate_status";
	gint count;
	GList *dataset, *it;
	ofoEntry *entry;
	ofaEntryStatus prev_status;
	ofoLedger *ledger;
	const GDate *last_close, *deffect;

	count = 0;
	dataset = entry_load_dataset( hub, where, NULL );
	count = g_list_length( dataset );

	if( remediate ){
		g_signal_emit_by_name( hub, SIGNAL_HUB_STATUS_COUNT, new_status, count );

		for( it=dataset ; it ; it=it->next ){
			entry = OFO_ENTRY( it->data );
			prev_status = ofo_entry_get_status( entry );

			/* new status actually depends of the last closing date of
			 * the ledger of the entry */
			if( prev_status == ENT_STATUS_PAST && new_status == ENT_STATUS_ROUGH ){
				ledger = ofo_ledger_get_by_mnemo( hub, ofo_entry_get_ledger( entry ));
				if( !ledger || !OFO_IS_LEDGER( ledger )){
					g_warning( "%s: ledger %s no more exists", thisfn, ofo_entry_get_ledger( entry ));
					return( -1 );
				}
				deffect = ofo_entry_get_deffect( entry );
				last_close = ofo_ledger_get_last_close( ledger );
				if( my_date_is_valid( last_close ) && my_date_compare( deffect, last_close ) <= 0 ){
					new_status = ENT_STATUS_VALIDATED;
				}
			}

			g_signal_emit_by_name(
					G_OBJECT( hub ), SIGNAL_HUB_STATUS_CHANGE, entry, prev_status, new_status );
		}
	}
	ofo_entry_free_dataset( dataset );

	return( count );
}

/*
 * SIGNAL_HUB_STATUS_CHANGE signal handler
 */
static void
hub_on_entry_status_change( ofaHub *hub, ofoEntry *entry, ofaEntryStatus prev_status, ofaEntryStatus new_status, void *empty )
{
	static const gchar *thisfn = "ofo_entry_hub_on_entry_status_change";
	gchar *query;

	g_debug( "%s: hub=%p, entry=%p, prev_status=%u, new_status=%u, empty=%p",
			thisfn, ( void * ) hub, ( void * ) entry, prev_status, new_status, ( void * ) empty );

	entry_set_status( entry, new_status );

	query = g_strdup_printf(
					"UPDATE OFA_T_ENTRIES SET ENT_STATUS=%u WHERE ENT_NUMBER=%ld",
						new_status,
						ofo_entry_get_number( entry ));

	if( ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE )){
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_UPDATED, entry, NULL );
	}

	g_free( query );
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 *
 * we try to report in recorded entries the modifications which may
 * happen on one of the externe identifiers - but only for the current
 * exercice
 *
 * Nonetheless, this is never a good idea to modify an identifier which
 * is publicly known, and this always should be done with the greatest
 * attention
 */
static void
hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, void *empty )
{
	static const gchar *thisfn = "ofo_entry_hub_on_updated_object";
	const gchar *number;
	const gchar *code;
	const gchar *mnemo;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, empty=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) empty );

	if( OFO_IS_ACCOUNT( object )){
		if( my_strlen( prev_id )){
			number = ofo_account_get_number( OFO_ACCOUNT( object ));
			if( g_utf8_collate( number, prev_id )){
				hub_on_updated_account_number( hub, prev_id, number );
			}
		}

	} else if( OFO_IS_CURRENCY( object )){
		if( my_strlen( prev_id )){
			code = ofo_currency_get_code( OFO_CURRENCY( object ));
			if( g_utf8_collate( code, prev_id )){
				hub_on_updated_currency_code( hub, prev_id, code );
			}
		}

	} else if( OFO_IS_LEDGER( object )){
		if( my_strlen( prev_id )){
			mnemo = ofo_ledger_get_mnemo( OFO_LEDGER( object ));
			if( g_utf8_collate( mnemo, prev_id )){
				hub_on_updated_ledger_mnemo( hub, prev_id, mnemo );
			}
		}

	} else if( OFO_IS_OPE_TEMPLATE( object )){
		if( my_strlen( prev_id )){
			mnemo = ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object ));
			if( g_utf8_collate( mnemo, prev_id )){
				hub_on_updated_model_mnemo( hub, prev_id, mnemo );
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
hub_on_updated_account_number( ofaHub *hub, const gchar *prev_id, const gchar *number )
{
	gchar *query;

	query = g_strdup_printf(
			"UPDATE OFA_T_ENTRIES "
			"	SET ENT_ACCOUNT='%s' WHERE ENT_ACCOUNT='%s' ", number, prev_id );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );
	g_free( query );
}

/*
 * a currency iso code has been modified (should be very rare)
 * all entries must be updated (even the unsettled or unreconciliated
 * from a previous exercice)
 */
static void
hub_on_updated_currency_code( ofaHub *hub, const gchar *prev_id, const gchar *code )
{
	gchar *query;

	query = g_strdup_printf(
			"UPDATE OFA_T_ENTRIES "
			"	SET ENT_CURRENCY='%s' WHERE ENT_CURRENCY='%s' ", code, prev_id );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );
	g_free( query );
}

/*
 * a ledger mnemonic has been modified
 * all entries must be updated (even the unsettled or unreconciliated
 * from a previous exercice)
 */
static void
hub_on_updated_ledger_mnemo( ofaHub *hub, const gchar *prev_id, const gchar *mnemo )
{
	gchar *query;

	query = g_strdup_printf(
			"UPDATE OFA_T_ENTRIES"
			"	SET ENT_LEDGER='%s' WHERE ENT_LEDGER='%s' ", mnemo, prev_id );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );
	g_free( query );
}

/*
 * an operation template mnemonic has been modified
 * all entries must be updated (even the unsettled or unreconciliated
 * from a previous exercice)
 */
static void
hub_on_updated_model_mnemo( ofaHub *hub, const gchar *prev_id, const gchar *mnemo )
{
	gchar *query;

	query = g_strdup_printf(
			"UPDATE OFA_T_ENTRIES"
			"	SET ENT_OPE_TEMPLATE='%s' WHERE ENT_OPE_TEMPLATE='%s' ", mnemo, prev_id );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );
	g_free( query );
}
