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
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idoc.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iexporter.h"
#include "api/ofa-igetter.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-isignalable.h"
#include "api/ofa-isignaler.h"
#include "api/ofa-prefs.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-concil.h"
#include "api/ofo-counters.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofs-account-balance.h"
#include "api/ofs-currency.h"
#include "api/ofs-ledger-balance.h"

#include "core/ofa-iconcil.h"
#include "core/ofo-entry-fec.h"

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
	ENT_IPERIOD,
	ENT_STATUS,
	ENT_RULE,
	ENT_UPD_USER,
	ENT_UPD_STAMP,
	ENT_OPE_NUMBER,
	ENT_STLMT_NUMBER,
	ENT_STLMT_USER,
	ENT_STLMT_STAMP,
	ENT_TIERS,
	ENT_NOTES,
	ENT_DOC_ID,
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order.
 * So:
 * 1/ the class default import should expect these fields in this same
 *    order.
 * 2/ new datas should be added to the end of the list.
 * 3/ a removed column should be replaced by an empty one to stay
 *    compatible with the class default import.
 *
 * Adding a field here should be reported in iexportable_export_fec()
 * function (Fichier des Ecritures Comptables), and the corresponding
 * document should be updated accordingly.
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
				OFA_TYPE_STRING,
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
		{ OFA_BOX_CSV( ENT_RULE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ENT_NOTES),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ENT_TIERS),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ENT_IPERIOD ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ 0 }
};

static const ofsBoxDef st_doc_defs[] = {
		{ OFA_BOX_CSV( ENT_NUMBER ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ENT_DOC_ID ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ 0 }
};

#define ENTRY_TABLES_COUNT              2
#define ENTRY_EXPORT_VERSION            2

typedef struct {
	gboolean  import_settled;
	GList    *docs;
}
	ofoEntryPrivate;

/* manage the period indicator
 * - the identifier is from a public enum (easier for the code)
 * - a non-localized char stored in dbms
 * - a localized char (short string for treeviews)
 * - a localized label
 */
typedef struct {
	ofeEntryPeriod id;
	const gchar   *dbms;
	const gchar   *abr;
	const gchar   *label;
}
	sPeriod;

static sPeriod st_period[] = {
		{ ENT_PERIOD_PAST,    "P", N_( "P" ), N_( "Past" ) },
		{ ENT_PERIOD_CURRENT, "C", N_( "C" ), N_( "Current" ) },
		{ ENT_PERIOD_FUTURE,  "F", N_( "F" ), N_( "Future" ) },
		{ 0 },
};

/* manage the status
 * - the identifier is from a public enum (easier for the code)
 * - a non-localized char stored in dbms
 * - a localized char (short string for treeviews)
 * - a localized label
 */
typedef struct {
	ofeEntryStatus id;
	const gchar   *dbms;
	const gchar   *abr;
	const gchar   *label;
}
	sStatus;

static sStatus st_status[] = {
		{ ENT_STATUS_ROUGH,     "R", N_( "R" ), N_( "Rough" ) },
		{ ENT_STATUS_VALIDATED, "V", N_( "V" ), N_( "Validated" ) },
		{ ENT_STATUS_DELETED,   "D", N_( "D" ), N_( "Deleted" ) },
		{ 0 },
};

/* manage the rule
 * - the identifier is from a public enum (easier for the code)
 * - a non-localized char stored in dbms
 * - a localized char (short string for treeviews)
 * - a localized label
 */
typedef struct {
	ofeEntryRule   id;
	const gchar   *dbms;
	const gchar   *abr;
	const gchar   *label;
}
	sRule;

static sRule st_rule[] = {
		{ ENT_RULE_NORMAL,  "N", N_( "N" ), N_( "Normal" ) },
		{ ENT_RULE_FORWARD, "F", N_( "F" ), N_( "Forward" ) },
		{ ENT_RULE_CLOSE,   "C", N_( "C" ), N_( "Closing" ) },
		{ 0 },
};

static gchar              *effect_in_exercice( ofaIGetter *getter );
static GList              *entry_load_dataset( ofaIGetter *getter, const gchar *where, const gchar *order );
static GDate              *entry_get_min_deffect( const ofoEntry *entry, GDate *date, ofaIGetter *getter );
static gboolean            entry_get_import_settled( ofoEntry *entry );
static void                entry_set_number( ofoEntry *entry, ofxCounter number );
static void                entry_set_period( ofoEntry *entry, ofeEntryPeriod period );
static void                entry_set_status( ofoEntry *entry, ofeEntryStatus status );
static void                entry_set_upd_user( ofoEntry *entry, const gchar *upd_user );
static void                entry_set_upd_stamp( ofoEntry *entry, const GTimeVal *upd_stamp );
static void                entry_set_settlement_user( ofoEntry *entry, const gchar *user );
static void                entry_set_settlement_stamp( ofoEntry *entry, const GTimeVal *stamp );
static void                entry_set_import_settled( ofoEntry *entry, gboolean settled );
static gboolean            entry_compute_status( ofoEntry *entry, gboolean set_deffect, ofaIGetter *getter );
static GList              *get_orphans( ofaIGetter *getter, const gchar *table );
static gboolean            entry_do_insert( ofoEntry *entry, ofaIGetter *getter );
static void                error_ledger( const gchar *ledger );
static void                error_ope_template( const gchar *model );
static void                error_currency( const gchar *currency );
static void                error_acc_number( void );
static void                error_account( const gchar *number );
static void                error_acc_currency( const gchar *currency, ofoAccount *account );
static void                error_amounts( ofxAmount debit, ofxAmount credit );
static gboolean            entry_do_update( ofoEntry *entry, ofaIGetter *getter );
static gboolean            do_update_settlement( ofoEntry *entry, const ofaIDBConnect *connect, ofxCounter number );
static gboolean            do_validate_by_ope( ofaIGetter *getter, ofxCounter openum );
static void                icollectionable_iface_init( myICollectionableInterface *iface );
static guint               icollectionable_get_interface_version( void );
static GList              *icollectionable_load_collection( void *user_data );
static void                iconcil_iface_init( ofaIConcilInterface *iface );
static guint               iconcil_get_interface_version( void );
static ofxCounter          iconcil_get_object_id( const ofaIConcil *instance );
static const gchar        *iconcil_get_object_type( const ofaIConcil *instance );
static void                idoc_iface_init( ofaIDocInterface *iface );
static guint               idoc_get_interface_version( void );
static void                iexportable_iface_init( ofaIExportableInterface *iface );
static guint               iexportable_get_interface_version( void );
static gchar              *iexportable_get_label( const ofaIExportable *instance );
static gboolean            iexportable_get_published( const ofaIExportable *instance );
static gboolean            iexportable_export( ofaIExportable *exportable, const gchar *format_id );
static gboolean            iexportable_export_default( ofaIExportable *exportable );
static void                iexporter_iface_init( ofaIExporterInterface *iface );
static guint               iexporter_get_interface_version( void );
static ofsIExporterFormat *iexporter_get_formats( ofaIExporter *exporter, GType type, ofaIGetter *getter );
static void                iimportable_iface_init( ofaIImportableInterface *iface );
static guint               iimportable_get_interface_version( void );
static gchar              *iimportable_get_label( const ofaIImportable *instance );
static guint               iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static GList              *iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static void                iimportable_import_concil( ofaIImporter *importer, ofsImporterParms *parms, ofoEntry *entry, GSList **fields );
static void                iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean            entry_drop_content( const ofaIDBConnect *connect );
static void                isignalable_iface_init( ofaISignalableInterface *iface );
static void                isignalable_connect_to( ofaISignaler *signaler );
static gboolean            signaler_on_deletable_object( ofaISignaler *signaler, ofoBase *object, void *empty );
static gboolean            signaler_is_deletable_account( ofaISignaler *signaler, ofoAccount *account );
static gboolean            signaler_is_deletable_currency( ofaISignaler *signaler, ofoCurrency *currency );
static gboolean            signaler_is_deletable_ledger( ofaISignaler *signaler, ofoLedger *ledger );
static gboolean            signaler_is_deletable_ope_template( ofaISignaler *signaler, ofoOpeTemplate *template );
static void                signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, void *empty );
static void                signaler_on_deleted_entry( ofaISignaler *signaler, ofoEntry *entry );
static void                signaler_on_exe_dates_changed( ofaISignaler *signaler, const GDate *prev_begin, const GDate *prev_end, void *empty );
static gint                check_for_changed_begin_exe_dates( ofaIGetter *getter, const GDate *prev_begin, const GDate *new_begin, gboolean remediate );
static gint                check_for_changed_end_exe_dates( ofaIGetter *getter, const GDate *prev_end, const GDate *new_end, gboolean remediate );
static gint                remediate_status( ofaIGetter *getter, const gchar *where, ofeEntryPeriod new_period );
static void                signaler_on_entry_period_status_changed( ofaISignaler *signaler, ofoEntry *entry, ofeEntryPeriod prev_period, ofeEntryStatus prev_status, ofeEntryPeriod new_period, ofeEntryStatus new_status, void *empty );
static void                signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, void *empty );
static void                signaler_on_updated_account_number( ofaISignaler *signaler, const gchar *prev_id, const gchar *number );
static void                signaler_on_updated_currency_code( ofaISignaler *signaler, const gchar *prev_id, const gchar *code );
static void                signaler_on_updated_ledger_mnemo( ofaISignaler *signaler, const gchar *prev_id, const gchar *mnemo );
static void                signaler_on_updated_model_mnemo( ofaISignaler *signaler, const gchar *prev_id, const gchar *mnemo );

G_DEFINE_TYPE_EXTENDED( ofoEntry, ofo_entry, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoEntry )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ICONCIL, iconcil_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDOC, idoc_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTER, iexporter_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNALABLE, isignalable_iface_init ))

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
	ofoEntryPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofo_entry_get_instance_private( self );

	priv->docs = NULL;
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
 * ofo_entry_get_dataset_account_balance:
 * @getter: a #ofaIGetter instance.
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
ofo_entry_get_dataset_account_balance( ofaIGetter *getter,
											const gchar *from_account, const gchar *to_account,
											const GDate *from_date, const GDate *to_date )
{
	static const gchar *thisfn = "ofo_entry_get_dataset_account_balance";
	ofaHub *hub;
	GList *dataset;
	GString *query;
	gboolean first;
	gchar *str;
	GSList *result, *irow, *icol;
	ofsAccountBalance *sbal;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

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
	g_string_append_printf( query, "ENT_STATUS!='%s' ", ofo_entry_status_get_dbms( ENT_STATUS_DELETED ));
	query = g_string_append( query, "GROUP BY ENT_ACCOUNT ORDER BY ENT_ACCOUNT ASC " );

	hub = ofa_igetter_get_hub( getter );

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
 * ofo_entry_get_dataset_ledger_balance:
 * @getter: a #ofaIGetter instance.
 * @ledger: the #ofoLedger mnemonic identifier.
 * @from_date: the starting effect date.
 * @to_date: the ending effect date.
 *
 * Returns the balances for non-deleted entries for the given
 * ledger, between the specified effect dates, as a GList of newly
 * allocated #ofsLedgerBalance structures, that the user should
 * #ofs_ledger_balance_list_free().
 *
 * The returned dataset is ordered by ascending currency.
 */
GList *
ofo_entry_get_dataset_ledger_balance( ofaIGetter *getter,
										const gchar *ledger,
										const GDate *from_date, const GDate *to_date )
{
	static const gchar *thisfn = "ofo_entry_get_dataset_ledger_balance";
	GList *dataset;
	GString *query;
	gboolean first;
	gchar *str;
	GSList *result, *irow, *icol;
	ofsLedgerBalance *sbal;
	ofaHub *hub;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	query = g_string_new(
				"SELECT ENT_LEDGER,ENT_CURRENCY,SUM(ENT_DEBIT),SUM(ENT_CREDIT) "
				"FROM OFA_T_ENTRIES WHERE " );
	first = FALSE;
	dataset = NULL;
	if( my_strlen( ledger )){
		g_string_append_printf( query, "ENT_LEDGER='%s' ", ledger );
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
	g_string_append_printf( query, "ENT_STATUS!='%s' ", ofo_entry_status_get_dbms( ENT_STATUS_DELETED ));
	query = g_string_append( query, "GROUP BY ENT_LEDGER,ENT_CURRENCY ORDER BY ENT_LEDGER,ENT_CURRENCY ASC " );

	hub = ofa_igetter_get_hub( getter );

	if( ofa_idbconnect_query_ex( ofa_hub_get_connect( hub ), query->str, &result, TRUE )){
		for( irow=result ; irow ; irow=irow->next ){
			sbal = g_new0( ofsLedgerBalance, 1 );
			icol = ( GSList * ) irow->data;
			sbal->ledger = g_strdup(( const gchar * ) icol->data );
			icol = icol->next;
			sbal->currency = g_strdup(( const gchar * ) icol->data );
			icol = icol->next;
			sbal->debit = my_double_set_from_sql(( const gchar * ) icol->data );
			icol = icol->next;
			sbal->credit = my_double_set_from_sql(( const gchar * ) icol->data );
			g_debug( "%s: account=%s, currency=%s, debit=%lf, credit=%lf",
					thisfn, sbal->ledger, sbal->currency, sbal->debit, sbal->credit );
			dataset = g_list_prepend( dataset, sbal );
		}
		ofa_idbconnect_free_results( result );
	}
	g_string_free( query, TRUE );

	return( g_list_reverse( dataset ));
}

/**
 * ofo_entry_get_dataset_for_print_by_account:
 * @getter: a #ofaIGetter instance.
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
ofo_entry_get_dataset_for_print_by_account( ofaIGetter *getter,
												const gchar *from_account, const gchar *to_account,
												const GDate *from_date, const GDate *to_date )
{
	GList *dataset;
	GString *query;
	gboolean first;
	gchar *str;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

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
	g_string_append_printf( query, "ENT_STATUS!='%s' ", ofo_entry_status_get_dbms( ENT_STATUS_DELETED ));

	dataset = entry_load_dataset( getter,  query->str,
			"ORDER BY ENT_ACCOUNT ASC,ENT_DOPE ASC,ENT_DEFFECT ASC,ENT_NUMBER ASC" );

	g_string_free( query, TRUE );

	return( dataset );
}

/**
 * ofo_entry_get_dataset_for_print_by_ledger:
 * @getter: a #ofaIGetter instance.
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
ofo_entry_get_dataset_for_print_by_ledger( ofaIGetter *getter,
												const GSList *mnemos,
												const GDate *from_date, const GDate *to_date )
{
	GList *dataset;
	GString *query;
	gboolean first;
	gchar *str;
	const GSList *it;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

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
	g_string_append_printf( query, "AND ENT_STATUS!='%s' ", ofo_entry_status_get_dbms( ENT_STATUS_DELETED ));

	dataset = entry_load_dataset( getter, query->str,
			"ORDER BY ENT_LEDGER ASC,ENT_DOPE ASC,ENT_DEFFECT ASC,ENT_NUMBER ASC" );

	g_string_free( query, TRUE );

	return( dataset );
}

/**
 * ofo_entry_get_dataset_for_print_reconcil:
 * @getter: a #ofaIGetter instance.
 * @account: the reconciliated account.
 * @date: the greatest effect date to be returned.
 *
 * Returns the dataset of un-reconciliated un-deleted entries for the
 * specified account, up to and including the specified effect date.
 *
 * The returned dataset is ordered by ascending dope/deffect/number.
 */
GList *
ofo_entry_get_dataset_for_print_reconcil( ofaIGetter *getter,
											const gchar *account, const GDate *date )
{
	GList *dataset;
	GString *where;
	gchar *str;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
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

	g_string_append_printf( where, " AND ENT_STATUS!='%s' ", ofo_entry_status_get_dbms( ENT_STATUS_DELETED ));

	dataset = entry_load_dataset( getter, where->str, NULL );

	g_string_free( where, TRUE );

	return( dataset );
}

/**
 * ofo_entry_get_dataset_for_exercice_by_status:
 * @hub: the current #ofaHub object.
 * @status: the requested status, or -1 for all status.
 *
 * Returns the dataset of entries on the exercice of the specified
 * status.
 *
 * The returned dataset is ordered by dope/deffect/number, and should
 * be #ofo_entry_free_dataset() by the caller.
 */
GList *
ofo_entry_get_dataset_for_exercice_by_status( ofaIGetter *getter, ofeEntryStatus status )
{
	GList *dataset;
	GString *where;
	gchar *str;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	str = effect_in_exercice( getter);
	where = g_string_new( str );
	g_free( str );

	if( status != -1 ){
		g_string_append_printf( where, " AND ENT_STATUS='%s' ", ofo_entry_status_get_dbms( status ));
	}

	dataset = entry_load_dataset( getter, where->str, NULL );

	g_string_free( where, TRUE );

	return( dataset );
}

/*
 * build a where string for the exercice on the effect date
 */
static gchar *
effect_in_exercice( ofaIGetter *getter )
{
	ofaHub *hub;
	ofoDossier *dossier;
	GString *where;
	const GDate *begin, *end;
	gchar *str;

	where = g_string_new( "" );
	hub = ofa_igetter_get_hub( getter );
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

/**
 * ofo_entry_get_dataset:
 * @getter: a #ofaIGetter instance.
 *
 * Returns *all* entries.
 *
 * The returned list is owned by the #myICollector of the application,
 * and should not be released by the caller.
 */
GList *
ofo_entry_get_dataset( ofaIGetter *getter )
{
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );

	return( my_icollector_collection_get( collector, OFO_TYPE_ENTRY, getter ));
}

/**
 * ofo_entry_get_count:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the total count of entries.
 */
ofxCounter
ofo_entry_get_count( ofaIGetter *getter )
{
	GList *dataset;
	ofxCounter count;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), 0 );

	dataset = ofo_entry_get_dataset( getter );
	count = ( ofxCounter ) g_list_length( dataset );

	return( count );
}

/**
 * ofo_entry_get_by_number:
 * @getter: a #ofaIGetter instance.
 * @number: the #ofoEntry identifier.
 *
 * Returns: the searched #ofoEntry, or %NULL.
 *
 * The returned #ofoEntry is owned by the #myICollector of the application,
 * and should not be released by the caller.
 */
ofoEntry *
ofo_entry_get_by_number( ofaIGetter *getter, ofxCounter number )
{
	GList *dataset, *it;
	ofoEntry *entry;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( number > 0, NULL );

	dataset = ofo_entry_get_dataset( getter );
	for( it=dataset ; it ; it=it->next ){
		entry = OFO_ENTRY( it->data );
		if( ofo_entry_get_number( entry ) == number ){
			return( entry );
		}
	}

	return( NULL );
}

/**
 * ofo_entry_get_by_ope_numbers:
 * @getter: a #ofaIGetter instance.
 * @ope_numbers: a #GList of operation numbers.
 *
 * Returns: a new #GList which contains the entries of this same
 * operation.
 *
 * The returned #GList should be g_list_free() by the caller.
 */
GList *
ofo_entry_get_by_ope_numbers( ofaIGetter *getter, GList *ope_numbers )
{
	GList *dataset, *it, *result;
	ofxCounter openum;
	ofoEntry *entry;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	result = NULL;
	dataset = ofo_entry_get_dataset( getter );
	for( it=dataset ; it ; it=it->next ){
		entry = OFO_ENTRY( it->data );
		openum = ofo_entry_get_ope_number( entry );
		if( g_list_find( ope_numbers, ( gpointer ) openum )){
			result = g_list_prepend( result, entry );
		}
	}

	return( result );
}

/**
 * ofo_entry_get_settlement_by_number:
 * @getter: a #ofaIGetter instance.
 * @number: the settlement number.
 * @user: [out][allow-none]: placeholder for the settlement user.
 * @stamp: [out][allow-none]: placeholder for the settlement timestamp.
 *
 * Set the provided @user and @stamp to the properties of the settlement.
 *
 * If set, the returned @user should be g_free() by the caller.
 */
void
ofo_entry_get_settlement_by_number( ofaIGetter *getter, ofxCounter number, gchar **user, GTimeVal *stamp )
{
	GList *dataset, *it;
	ofoEntry *entry;
	const GTimeVal *ent_stamp;

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( number > 0 );

	if( user ){
		*user = NULL;
	}
	if( stamp ){
		stamp->tv_sec = 0;
		stamp->tv_usec = 0;
	}

	dataset = ofo_entry_get_dataset( getter );
	for( it=dataset ; it ; it=it->next ){
		entry = OFO_ENTRY( it->data );
		if( ofo_entry_get_settlement_number( entry ) == number ){
			if( user ){
				*user = g_strdup( ofo_entry_get_settlement_user( entry ));
			}
			if( stamp ){
				ent_stamp = ofo_entry_get_settlement_stamp( entry );
				stamp->tv_sec = ent_stamp->tv_sec;
				stamp->tv_usec = ent_stamp->tv_usec;
			}
			break;;
		}
	}
}

/*
 * returns a #GList * of all #ofoEntry's which satisfy the @where clause
 */
static GList *
entry_load_dataset( ofaIGetter *getter, const gchar *where, const gchar *order )
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
					getter );

	g_string_free( query, TRUE );

	return( dataset );
}

/**
 * ofo_entry_use_account:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if a recorded entry makes use of the specified account.
 */
gboolean
ofo_entry_use_account( ofaIGetter *getter, const gchar *account )
{
	ofaHub *hub;
	gchar *query;
	gint count;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	if( !my_strlen( account )){
		return( FALSE );
	}

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM OFA_T_ENTRIES WHERE ENT_ACCOUNT='%s'",
			account );

	hub = ofa_igetter_get_hub( getter );

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count > 0 );
}

/**
 * ofo_entry_use_ledger:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if a recorded entry makes use of the specified ledger.
 */
gboolean
ofo_entry_use_ledger( ofaIGetter *getter, const gchar *ledger )
{
	ofaHub *hub;
	gchar *query;
	gint count;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	if( !my_strlen( ledger )){
		return( FALSE );
	}

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM OFA_T_ENTRIES WHERE ENT_LEDGER='%s'",
			ledger );

	hub = ofa_igetter_get_hub( getter );

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count > 0 );
}

/**
 * ofo_entry_new:
 * @getter: a #ofaIGetter instance.
 */
ofoEntry *
ofo_entry_new( ofaIGetter *getter )
{
	ofoEntry *entry;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	entry = g_object_new( OFO_TYPE_ENTRY, "ofo-base-getter", getter, NULL );
	OFO_BASE( entry )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	entry_set_number( entry, OFO_BASE_UNSET_ID );
	entry_set_period( entry, ENT_PERIOD_CURRENT );
	entry_set_status( entry, ENT_STATUS_ROUGH );
	ofo_entry_set_rule( entry, ENT_RULE_NORMAL );

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
 * ofo_entry_get_period:
 */
ofeEntryPeriod
ofo_entry_get_period( const ofoEntry *entry )
{
	static const gchar *thisfn = "ofo_entry_get_period";
	const gchar *cstr;
	gint i;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), 0 );
	g_return_val_if_fail( !OFO_BASE( entry )->prot->dispose_has_run, 0 );

	cstr = ofa_box_get_string( OFO_BASE( entry )->prot->fields, ENT_IPERIOD );

	for( i=0 ; st_period[i].id ; ++i ){
		if( !my_collate( st_period[i].dbms, cstr )){
			return( st_period[i].id );
		}
	}

	g_warning( "%s: unknown or invalid dbms period indicator: %s", thisfn, cstr );

	return( 0 );
}

/**
 * ofo_entry_period_get_dbms:
 *
 * Returns: the dbms string corresponding to the period.
 */
const gchar *
ofo_entry_period_get_dbms( ofeEntryPeriod period )
{
	static const gchar *thisfn = "ofo_entry_period_get_dbms";
	gint i;

	for( i=0 ; st_period[i].id ; ++i ){
		if( st_period[i].id == period ){
			return( st_period[i].dbms );
		}
	}

	g_warning( "%s: unknown or invalid period identifier: %u", thisfn, period );

	return( NULL );
}

/**
 * ofo_entry_period_get_abr:
 *
 * Returns: the abbreviated localized string corresponding to the period.
 */
const gchar *
ofo_entry_period_get_abr( ofeEntryPeriod period )
{
	static const gchar *thisfn = "ofo_entry_period_get_abr";
	gint i;

	for( i=0 ; st_period[i].id ; ++i ){
		if( st_period[i].id == period ){
			return( gettext( st_period[i].abr ));
		}
	}

	g_warning( "%s: unknown or invalid period identifier: %u", thisfn, period );

	return( "" );
}

/**
 * ofo_entry_period_get_label:
 *
 * Returns: the abbreviated localized label corresponding to the period.
 */
const gchar *
ofo_entry_period_get_label( ofeEntryPeriod period )
{
	static const gchar *thisfn = "ofo_entry_period_get_label";
	gint i;

	for( i=0 ; st_period[i].id ; ++i ){
		if( st_period[i].id == period ){
			return( gettext( st_period[i].label ));
		}
	}

	g_warning( "%s: unknown or invalid period identifier: %u", thisfn, period );

	return( "" );
}

/**
 * ofo_entry_get_status:
 */
ofeEntryStatus
ofo_entry_get_status( const ofoEntry *entry )
{
	static const gchar *thisfn = "ofo_entry_get_status";
	const gchar *cstr;
	gint i;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), 0 );
	g_return_val_if_fail( !OFO_BASE( entry )->prot->dispose_has_run, 0 );

	cstr = ofa_box_get_string( OFO_BASE( entry )->prot->fields, ENT_STATUS );

	for( i=0 ; st_status[i].id ; ++i ){
		if( !my_collate( st_status[i].dbms, cstr )){
			return( st_status[i].id );
		}
	}

	g_warning( "%s: unknown or invalid dbms status: %s", thisfn, cstr );

	return( 0 );
}

/**
 * ofo_entry_status_get_dbms:
 *
 * Returns: the dbms string corresponding to the status.
 */
const gchar *
ofo_entry_status_get_dbms( ofeEntryStatus status )
{
	static const gchar *thisfn = "ofo_entry_status_get_dbms";
	gint i;

	for( i=0 ; st_status[i].id ; ++i ){
		if( st_status[i].id == status ){
			return( st_status[i].dbms );
		}
	}

	g_warning( "%s: unknown or invalid status identifier: %u", thisfn, status );

	return( NULL );
}

/**
 * ofo_entry_status_get_abr:
 *
 * Returns: the abbreviated localized string corresponding to the status.
 */
const gchar *
ofo_entry_status_get_abr( ofeEntryStatus status )
{
	static const gchar *thisfn = "ofo_entry_status_get_abr";
	gint i;

	for( i=0 ; st_status[i].id ; ++i ){
		if( st_status[i].id == status ){
			return( gettext( st_status[i].abr ));
		}
	}

	g_warning( "%s: unknown or invalid status identifier: %u", thisfn, status );

	return( "" );
}

/**
 * ofo_entry_status_get_label:
 *
 * Returns: the abbreviated localized label corresponding to the status.
 */
const gchar *
ofo_entry_status_get_label( ofeEntryStatus status )
{
	static const gchar *thisfn = "ofo_entry_status_get_label";
	gint i;

	for( i=0 ; st_status[i].id ; ++i ){
		if( st_status[i].id == status ){
			return( gettext( st_status[i].label ));
		}
	}

	g_warning( "%s: unknown or invalid status identifier: %u", thisfn, status );

	return( "" );
}

/**
 * ofo_entry_get_rule:
 *
 * Returns: the #ofeEntryRule enum for this @entry.
 */
ofeEntryRule
ofo_entry_get_rule( const ofoEntry *entry )
{
	static const gchar *thisfn = "ofo_entry_get_rule";
	const gchar *cstr;
	gint i;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), 0 );
	g_return_val_if_fail( !OFO_BASE( entry )->prot->dispose_has_run, 0 );

	cstr = ofa_box_get_string( OFO_BASE( entry )->prot->fields, ENT_RULE );

	for( i=0 ; st_rule[i].id ; ++i ){
		if( !my_collate( st_rule[i].dbms, cstr )){
			return( st_rule[i].id );
		}
	}

	g_warning( "%s: unknown or invalid dbms rule: %s", thisfn, cstr );

	return( 0 );
}

/**
 * ofo_entry_rule_get_dbms:
 * @rule: a #ofeEntryRule rule.
 *
 * Returns: the dbms indicator corresponding to @rule.
 */
const gchar *
ofo_entry_rule_get_dbms( ofeEntryRule rule )
{
	static const gchar *thisfn = "ofo_entry_rule_get_dbms";
	gint i;

	for( i=0 ; st_rule[i].id ; ++i ){
		if( st_rule[i].id == rule ){
			return( st_rule[i].dbms );
		}
	}

	g_warning( "%s: unknown or invalid rule identifier: %u", thisfn, rule );

	return( NULL );
}

/**
 * ofo_entry_rule_get_abr:
 *
 * Returns: the localized rule indicator for this @rule.
 */
const gchar *
ofo_entry_rule_get_abr( ofeEntryRule rule )
{
	static const gchar *thisfn = "ofo_entry_rule_get_abr";
	gint i;

	for( i=0 ; st_rule[i].id ; ++i ){
		if( st_rule[i].id == rule ){
			return( gettext( st_rule[i].abr ));
		}
	}

	g_warning( "%s: unknown or invalid rule identifier: %u", thisfn, rule );

	return( "" );
}

/**
 * ofo_entry_get_rule_label:
 *
 * Returns: a localized label for the rule indicator.
 *
 * Use case: properties
 */
const gchar *
ofo_entry_rule_get_label( ofeEntryRule rule )
{
	static const gchar *thisfn = "ofo_entry_rule_get_abr";
	gint i;

	for( i=0 ; st_rule[i].id ; ++i ){
		if( st_rule[i].id == rule ){
			return( gettext( st_rule[i].label ));
		}
	}

	g_warning( "%s: unknown or invalid rule identifier: %u", thisfn, rule );

	return( "" );
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
 * ofo_entry_get_tiers:
 */
ofxCounter
ofo_entry_get_tiers( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, counter, 0, ENT_TIERS );
}

/**
 * ofo_entry_get_notes:
 */
const gchar *
ofo_entry_get_notes( const ofoEntry *entry )
{
	ofo_base_getter( ENTRY, entry, string, NULL, ENT_NOTES );
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
 * ofo_entry_get_exe_changed_count:
 * @getter: a #ofaIGetter instance.
 * @prev_begin:
 * @prev_end:
 * @new_begin:
 * @new_end:
 *
 * Returns: the count of entries whose the status is changed by the
 * closing of the financial period.
 */
gint
ofo_entry_get_exe_changed_count( ofaIGetter *getter,
									const GDate *prev_begin, const GDate *prev_end,
									const GDate *new_begin, const GDate *new_end )
{
	gint count_begin, count_end;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), 0 );

	count_begin = check_for_changed_begin_exe_dates( getter, prev_begin, new_begin, FALSE );
	count_end = check_for_changed_end_exe_dates( getter, prev_end, new_end, FALSE );

	return( count_begin+count_end );
}

/*
 * entry_get_min_deffect:
 * @entry: this #ofoEntry entry.
 * @date: [out]: the outputed date.
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the minimal allowed effect date on the dossier for the
 * ledger on which the entry is imputed.
 *
 * NOTE: This method may be called for a new entry which has never been
 * yet serialized to the database (and so for which ofo_base_get_hub()
 * would not work).
 */
static GDate *
entry_get_min_deffect( const ofoEntry *entry, GDate *date, ofaIGetter *getter )
{
	ofaHub *hub;
	ofoDossier *dossier;
	const gchar *mnemo;
	ofoLedger *ledger;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), NULL );
	g_return_val_if_fail( !OFO_BASE( entry )->prot->dispose_has_run, NULL );

	g_date_clear( date, 1 );
	ledger = NULL;

	hub = ofa_igetter_get_hub( getter );
	dossier = ofa_hub_get_dossier( hub );
	mnemo = ofo_entry_get_ledger( entry );

	if( my_strlen( mnemo )){
		ledger = ofo_ledger_get_by_mnemo( getter, mnemo );
		g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), NULL );
		ofo_dossier_get_min_deffect( dossier, ledger, date );
	}

	return( date );
}

/**
 * ofo_entry_get_currencies:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: a #GSList of distinct currency identifiers (ISO 3A code)
 * recorded in the entries.
 *
 * The returned value should be ofo_entry_free_currencies() by the caller.
 */
GSList *
ofo_entry_get_currencies( ofaIGetter *getter )
{
	ofaHub *hub;
	GSList *result, *irow, *icol;
	GSList *list;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	hub = ofa_igetter_get_hub( getter );

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
 * An entry may be edited if its status is rough.
 * validated or deleted entries cannot be edited.
 */
gboolean
ofo_entry_is_editable( const ofoEntry *entry )
{
	gboolean editable;
	ofeEntryStatus status;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( !OFO_BASE( entry )->prot->dispose_has_run, FALSE );

	status = ofo_entry_get_status( entry );
	editable = ( status == ENT_STATUS_ROUGH );

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
 * entry_set_period:
 */
static void
entry_set_period( ofoEntry *entry, ofeEntryPeriod period )
{
	const gchar *cstr;

	g_return_if_fail( entry && OFO_IS_ENTRY( entry ));
	g_return_if_fail( !OFO_BASE( entry )->prot->dispose_has_run );

	cstr = ofo_entry_period_get_dbms( period );
	ofa_box_set_string( OFO_BASE( entry )->prot->fields, ENT_IPERIOD, cstr );
}

/*
 * entry_set_status:
 */
static void
entry_set_status( ofoEntry *entry, ofeEntryStatus status )
{
	const gchar *cstr;

	g_return_if_fail( entry && OFO_IS_ENTRY( entry ));
	g_return_if_fail( !OFO_BASE( entry )->prot->dispose_has_run );

	cstr = ofo_entry_status_get_dbms( status );
	ofa_box_set_string( OFO_BASE( entry )->prot->fields, ENT_STATUS, cstr );
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

/**
 * ofo_entry_set_rule:
 */
void
ofo_entry_set_rule( ofoEntry *entry, ofeEntryRule rule )
{
	const gchar *cstr;

	g_return_if_fail( entry && OFO_IS_ENTRY( entry ));
	g_return_if_fail( !OFO_BASE( entry )->prot->dispose_has_run );

	cstr = ofo_entry_rule_get_dbms( rule );
	ofa_box_set_string( OFO_BASE( entry )->prot->fields, ENT_RULE, cstr );
}

/**
 * ofo_entry_set_tiers:
 */
void
ofo_entry_set_tiers( ofoEntry *entry, ofxCounter tiers )
{
	ofo_base_setter( ENTRY, entry, counter, ENT_TIERS, tiers );
}

/**
 * ofo_entry_set_notes:
 */
void
ofo_entry_set_notes( ofoEntry *entry, const gchar *notes )
{
	ofo_base_setter( ENTRY, entry, string, ENT_NOTES, notes );
}

/*
 * entry_compute_status:
 * @entry: this #ofoEntry object.
 * @set_deffect: if %TRUE, then the effect date is modified to the
 *  minimum allowed.
 * @getter: a #ofaIGetter instance.
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
 * yet serialized to the database (and so for which ofo_base_get_getter()
 * would not work).
 */
static gboolean
entry_compute_status( ofoEntry *entry, gboolean set_deffect, ofaIGetter *getter )
{
	static const gchar *thisfn = "entry_compute_status";
	const GDate *exe_begin, *exe_end, *deffect;
	ofaHub *hub;
	ofoDossier *dossier;
	GDate min_deffect;
	gboolean is_valid;
	gchar *sdeffect, *sdmin;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );
	g_return_val_if_fail( !OFO_BASE( entry )->prot->dispose_has_run, FALSE );

	is_valid = TRUE;
	hub = ofa_igetter_get_hub( getter );
	dossier = ofa_hub_get_dossier( hub );

	exe_begin = ofo_dossier_get_exe_begin( dossier );
	exe_end = ofo_dossier_get_exe_end( dossier );
	deffect = ofo_entry_get_deffect( entry );
	g_return_val_if_fail( my_date_is_valid( deffect ), FALSE );

	/* what to do regarding the effect date ? */
	if( my_date_is_valid( exe_begin ) && my_date_compare( deffect, exe_begin ) < 0 ){
		/* entry is in the past
		 * it is set to validated */
		entry_set_period( entry, ENT_PERIOD_PAST );
		entry_set_status( entry, ENT_STATUS_VALIDATED );

	} else if( my_date_is_valid( exe_end ) && my_date_compare( deffect, exe_end ) > 0 ){
		/* entry is in the future
		 * it is set to rough */
		entry_set_period( entry, ENT_PERIOD_FUTURE );
		entry_set_status( entry, ENT_STATUS_ROUGH );

	} else {
		entry_get_min_deffect( entry, &min_deffect, getter );
		is_valid = !my_date_is_valid( &min_deffect ) ||
				my_date_compare( deffect, &min_deffect ) >= 0;

		if( !is_valid && set_deffect ){
			ofo_entry_set_deffect( entry, &min_deffect );
			is_valid = TRUE;
		}

		if( !is_valid ){
			sdeffect = my_date_to_str( deffect, ofa_prefs_date_get_display_format( getter ));
			sdmin = my_date_to_str( &min_deffect, ofa_prefs_date_get_display_format( getter ));
			g_warning(
					"%s: entry effect date %s is lesser than minimal allowed %s",
					thisfn, sdeffect, sdmin );
			g_free( sdmin );
			g_free( sdeffect );

		} else {
			entry_set_period( entry, ENT_PERIOD_CURRENT );
			entry_set_status( entry, ENT_STATUS_ROUGH );
		}
	}

	return( is_valid );
}

/**
 * ofo_entry_is_valid_data:
 * @getter: a #ofaIGetter instance.
 */
gboolean
ofo_entry_is_valid_data( ofaIGetter *getter,
							const GDate *deffect, const GDate *dope, const gchar *label,
							const gchar *account, const gchar *currency, const gchar *ledger,
							const gchar *model, ofxAmount debit, ofxAmount credit,
							gchar **msgerr)
{
	ofoAccount *account_obj;
	gboolean ok;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	ok = TRUE;

	if( !my_strlen( ledger ) || !ofo_ledger_get_by_mnemo( getter, ledger )){
		error_ledger( ledger );
		ok &= FALSE;
	}
	if( !my_strlen( model ) || !ofo_ope_template_get_by_mnemo( getter, model )){
		error_ope_template( model );
		ok &= FALSE;
	}
	if( !my_strlen( currency ) || !ofo_currency_get_by_code( getter, currency )){
		error_currency( currency );
		ok &= FALSE;
	}
	if( !my_strlen( account )){
		error_acc_number();
		ok &= FALSE;
	} else {
		account_obj = ofo_account_get_by_number( getter, account );
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
 * @getter: a #ofaIGetter instance.
 *
 * Create a new entry with the provided data.
 * The entry is - at this time - unnumbered and does not have sent any
 * advertising message. For the moment, this is only a 'project' of
 * entry...
 *
 * Returns: the #ofoEntry entry object, of %NULL in case of an error.
 */
ofoEntry *
ofo_entry_new_with_data( ofaIGetter *getter,
							const GDate *deffect, const GDate *dope, const gchar *label,
							const gchar *ref, const gchar *account,
							const gchar *currency, const gchar *ledger,
							const gchar *model, ofxAmount debit, ofxAmount credit )
{
	ofoEntry *entry;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	if( !ofo_entry_is_valid_data(
			getter, deffect, dope, label, account, currency, ledger, model, debit, credit, NULL )){
		return( NULL );
	}

	entry = ofo_entry_new( getter );

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

	entry_compute_status( entry, FALSE, getter );

	return( entry );
}

/**
 * ofo_entry_doc_get_count:
 * @entry: this #ofoEntry object.
 *
 * Returns: the count of attached documents.
 */
guint
ofo_entry_doc_get_count( ofoEntry *entry )
{
	ofoEntryPrivate *priv;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), 0 );
	g_return_val_if_fail( !OFO_BASE( entry )->prot->dispose_has_run, 0 );

	priv = ofo_entry_get_instance_private( entry );

	return( g_list_length( priv->docs ));
}

/**
 * ofo_entry_doc_get_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown ENT_NUMBER in OFA_T_ENTRIES_DOC child table.
 *
 * The returned list should be #ofo_entry_doc_free_orphans() by the
 * caller.
 */
GList *
ofo_entry_doc_get_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "OFA_T_ENTRIES_DOC" ));
}

static GList *
get_orphans( ofaIGetter *getter, const gchar *table )
{
	ofaHub *hub;
	ofaIDBConnect *connect;
	GList *orphans;
	GSList *result, *irow, *icol;
	gchar *query;
	ofxCounter entnum;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( table ), NULL );

	orphans = NULL;
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf( "SELECT DISTINCT(ENT_NUMBER) FROM %s "
			"	WHERE ENT_NUMBER NOT IN (SELECT ENT_NUMBER FROM OFA_T_ENTRIES)", table );

	if( ofa_idbconnect_query_ex( connect, query, &result, FALSE )){
		for( irow=result ; irow ; irow=irow->next ){
			icol = irow->data;
			entnum = atol(( const gchar * ) icol->data );
			orphans = g_list_prepend( orphans, ( gpointer ) entnum );
		}
		ofa_idbconnect_free_results( result );
	}

	g_free( query );

	return( orphans );
}

/**
 * ofo_entry_insert:
 *
 * Allocates a sequential number to the entry, and records it in the
 * dbms. Send the corresponding advertising messages if no error occurs.
 */
gboolean
ofo_entry_insert( ofoEntry *entry )
{
	static const gchar *thisfn = "ofo_entry_insert";
	gboolean ok;
	ofaIGetter *getter;
	ofaISignaler *signaler;

	g_debug( "%s: entry=%p", thisfn, ( void * ) entry );

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( !OFO_BASE( entry )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( entry ));
	signaler = ofa_igetter_get_signaler( getter );

	entry_set_number( entry, ofo_counters_get_next_entry_id( getter ));
	entry_compute_status( entry, FALSE, getter );

	/* rationale: see ofo-account.c */
	ofo_entry_get_dataset( getter );

	if( entry_do_insert( entry, getter )){
		if( ofo_entry_get_period( entry ) != ENT_PERIOD_PAST ){
			my_icollector_collection_add_object(
					ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( entry ), NULL, getter );
			g_signal_emit_by_name( signaler, SIGNALER_BASE_NEW, entry );
		}
		ok = TRUE;
	}

	return( ok );
}

static gboolean
entry_do_insert( ofoEntry *entry, ofaIGetter *getter )
{
	GString *query;
	gchar *label, *ref;
	gchar *sdeff, *sdope, *sdebit, *scredit, *stamp_str, *notes;
	gboolean ok;
	GTimeVal stamp;
	const gchar *model, *cur_code, *userid, *rule, *status, *period;
	ofoCurrency *cur_obj;
	const ofaIDBConnect *connect;
	ofxCounter ope_number, tiers;
	ofaHub *hub;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );

	cur_code = ofo_entry_get_currency( entry );
	cur_obj = ofo_currency_get_by_code( getter, cur_code );
	g_return_val_if_fail( cur_obj && OFO_IS_CURRENCY( cur_obj ), FALSE );

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_entry_get_label( entry ));
	ref = my_utils_quote_sql( ofo_entry_get_ref( entry ));
	sdeff = my_date_to_str( ofo_entry_get_deffect( entry ), MY_DATE_SQL );
	sdope = my_date_to_str( ofo_entry_get_dope( entry ), MY_DATE_SQL );
	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_ENTRIES " );

	g_string_append_printf( query,
			"	(ENT_DEFFECT,ENT_NUMBER,ENT_DOPE,ENT_LABEL,ENT_REF,ENT_ACCOUNT,"
			"	ENT_CURRENCY,ENT_LEDGER,ENT_OPE_TEMPLATE,"
			"	ENT_DEBIT,ENT_CREDIT,ENT_STATUS,ENT_OPE_NUMBER,ENT_RULE,"
			"	ENT_TIERS,ENT_IPERIOD,"
			"	ENT_NOTES,ENT_UPD_USER, ENT_UPD_STAMP) "
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

	status = ofa_box_get_string( OFO_BASE( entry )->prot->fields, ENT_STATUS );
	g_return_val_if_fail( my_strlen( status ) == 1, FALSE );

	g_string_append_printf( query,
				"%s,%s,'%s',",
				sdebit,
				scredit,
				status );

	ope_number = ofo_entry_get_ope_number( entry );
	if( ope_number > 0 ){
		g_string_append_printf( query, "%ld,", ope_number );
	} else {
		query = g_string_append( query, "NULL," );
	}

	rule = ofa_box_get_string( OFO_BASE( entry )->prot->fields, ENT_RULE );
	g_return_val_if_fail( my_strlen( rule ) == 1, FALSE );
	g_string_append_printf( query, "'%s',", rule );

	tiers = ofo_entry_get_tiers( entry );
	if( tiers > 0 ){
		g_string_append_printf( query, "%lu,", tiers );
	} else {
		query = g_string_append( query, "NULL," );
	}

	period = ofa_box_get_string( OFO_BASE( entry )->prot->fields, ENT_IPERIOD );
	g_return_val_if_fail( my_strlen( period ) == 1, FALSE );
	g_string_append_printf( query, "'%s',", period );

	notes = my_utils_quote_sql( ofo_entry_get_notes( entry ));
	if( my_strlen( notes )){
		g_string_append_printf( query, "'%s',", notes );
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
	g_free( notes );
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

	str = g_strdup_printf( _( "Ledger identifier '%s' is invalid" ), ledger );
	my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );

	g_free( str );
}

static void
error_ope_template( const gchar *model )
{
	gchar *str;

	str = g_strdup_printf( _( "Operation template identifier '%s' is invalid" ), model );
	my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );

	g_free( str );
}

static void
error_currency( const gchar *currency )
{
	gchar *str;

	str = g_strdup_printf( _( "ISO 3A currency code '%s' is invalid" ), currency );
	my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );

	g_free( str );
}

static void
error_acc_number( void )
{
	gchar *str;

	str = g_strdup( _( "Account number is empty" ));
	my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );

	g_free( str );
}

static void
error_account( const gchar *number )
{
	gchar *str;

	str = g_strdup_printf( _( "Account number '%s' is invalid" ), number );
	my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );

	g_free( str );
}

static void
error_acc_currency( const gchar *currency, ofoAccount *account )
{
	ofaIGetter *getter;
	gchar *str;
	const gchar *acc_currency;
	ofoCurrency *acc_dev, *ent_dev;

	getter = ofo_base_get_getter( OFO_BASE( account ));
	acc_currency = ofo_account_get_currency( account );
	acc_dev = ofo_currency_get_by_code( getter, acc_currency );
	ent_dev = ofo_currency_get_by_code( getter, currency );

	if( !acc_dev ){
		str = g_strdup_printf( "Currency '%s' is invalid for the account '%s'",
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
	ofaIGetter *getter;
	ofaISignaler *signaler;
	gboolean ok;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( !OFO_BASE( entry )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( entry ));
	signaler = ofa_igetter_get_signaler( getter );

	if( entry_do_update( entry, getter )){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, entry, NULL );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
entry_do_update( ofoEntry *entry, ofaIGetter *getter )
{
	ofaHub *hub;
	GString *query;
	gchar *sdeff, *sdope, *sdeb, *scre, *notes;
	gchar *stamp_str, *label, *ref;
	GTimeVal stamp;
	gboolean ok;
	const gchar *model, *cstr, *userid, *rule, *period;
	const gchar *cur_code;
	ofoCurrency *cur_obj;
	const ofaIDBConnect *connect;
	ofxCounter tiers;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );

	cur_code = ofo_entry_get_currency( entry );
	cur_obj = ofo_currency_get_by_code( getter, cur_code );
	g_return_val_if_fail( cur_obj && OFO_IS_CURRENCY( cur_obj ), FALSE );

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_entry_get_label( entry ));
	sdope = my_date_to_str( ofo_entry_get_dope( entry ), MY_DATE_SQL );
	sdeff = my_date_to_str( ofo_entry_get_deffect( entry ), MY_DATE_SQL );
	sdeb = ofa_amount_to_sql( ofo_entry_get_debit( entry ), cur_obj );
	scre = ofa_amount_to_sql( ofo_entry_get_credit( entry ), cur_obj );
	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

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

	rule = ofa_box_get_string( OFO_BASE( entry )->prot->fields, ENT_RULE );
	g_return_val_if_fail( my_strlen( rule ) == 1, FALSE );
	g_string_append_printf( query, "ENT_RULE='%s',", rule );

	tiers = ofo_entry_get_tiers( entry );
	if( tiers > 0 ){
		g_string_append_printf( query, "ENT_TIERS=%lu,", tiers );
	} else {
		query = g_string_append( query, "ENT_TIERS=NULL," );
	}

	period = ofa_box_get_string( OFO_BASE( entry )->prot->fields, ENT_IPERIOD );
	g_return_val_if_fail( my_strlen( period ) == 1, FALSE );
	g_string_append_printf( query, "ENT_IPERIOD='%s',", period );

	notes = my_utils_quote_sql( ofo_entry_get_notes( entry ));
	if( my_strlen( notes )){
		g_string_append_printf( query, "ENT_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "ENT_NOTES=NULL," );
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
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( !OFO_BASE( entry )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( entry ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( do_update_settlement( entry, ofa_hub_get_connect( hub ), number )){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, entry, NULL );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
do_update_settlement( ofoEntry *entry, const ofaIDBConnect *connect, ofxCounter number )
{
	GString *query;
	gchar *stamp_str;
	GTimeVal stamp;
	gboolean ok;
	const gchar *userid;

	userid = ofa_idbconnect_get_account( connect );

	query = g_string_new( "UPDATE OFA_T_ENTRIES SET " );

	if( number > 0 ){
		ofo_entry_set_settlement_number( entry, number );
		entry_set_settlement_user( entry, userid );
		entry_set_settlement_stamp( entry, my_stamp_set_now( &stamp ));

		stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );
		g_string_append_printf( query,
				"ENT_STLMT_NUMBER=%ld,ENT_STLMT_USER='%s',ENT_STLMT_STAMP='%s' ",
				number, userid, stamp_str );
		g_free( stamp_str );

	} else {
		ofo_entry_set_settlement_number( entry, 0 );
		entry_set_settlement_user( entry, NULL );
		entry_set_settlement_stamp( entry, NULL );

		g_string_append_printf( query,
				"ENT_STLMT_NUMBER=NULL,ENT_STLMT_USER=NULL,ENT_STLMT_STAMP=0 " );
	}

	g_string_append_printf( query, "WHERE ENT_NUMBER=%ld", ofo_entry_get_number( entry ));
	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	g_string_free( query, TRUE );

	return( ok );
}

/**
 * ofo_entry_unsettle_by_number:
 * @getter: a #ofaIGetter instance.
 * @stlmt_number: the identifier of the settlement group to be cancelled.
 *
 * Cancel the identified settlement group by updating all member
 * entries. Each entry will receive a 'updated' message through the
 * dossier signaling system
 */
void
ofo_entry_unsettle_by_number( ofaIGetter *getter, ofxCounter stlmt_number )
{
	GList *entries, *it;
	gchar *where;

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( stlmt_number > 0 );

	/* get the list of entries */
	where = g_strdup_printf( "ENT_STLMT_NUMBER=%ld", stlmt_number );
	entries = entry_load_dataset( getter, where, NULL );
	g_free( where );

	/* update the entries, simultaneously sending messages */
	for( it=entries ; it ; it=it->next ){
		ofo_entry_update_settlement( OFO_ENTRY( it->data ), 0 );
	}
	ofo_entry_free_dataset( entries );
}

/**
 * ofo_entry_validate:
 * @entry: this #ofoEntry object.
 *
 * Validate the entry, by changing its status to ENT_STATUS_VALIDATED.
 *
 * Entry must be in 'rough' status.
 * Period must be 'current' or 'future'.
 */
void
ofo_entry_validate( ofoEntry *entry )
{
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofeEntryStatus status;
	ofeEntryPeriod period;

	g_return_if_fail( entry && OFO_IS_ENTRY( entry ));
	g_return_if_fail( !OFO_BASE( entry )->prot->dispose_has_run );

	status = ofo_entry_get_status( entry );
	g_return_if_fail( status == ENT_STATUS_ROUGH );

	period = ofo_entry_get_period( entry );
	g_return_if_fail( period == ENT_PERIOD_CURRENT || period == ENT_PERIOD_FUTURE );

	getter = ofo_base_get_getter( OFO_BASE( entry ));
	signaler = ofa_igetter_get_signaler( getter );

	g_signal_emit_by_name( signaler, SIGNALER_PERIOD_STATUS_CHANGE, entry, -1, status, -1, ENT_STATUS_VALIDATED );
}

/**
 * ofo_entry_validate_by_ledger:
 * @getter: a #ofaIGetter instance.
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
ofo_entry_validate_by_ledger( ofaIGetter *getter, const gchar *mnemo, const GDate *deffect )
{
	gchar *query, *sdate;
	ofoEntry *entry;
	GList *dataset, *it;
	ofaISignaler *signaler;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	sdate = my_date_to_str( deffect, MY_DATE_SQL );
	query = g_strdup_printf(
					"OFA_T_ENTRIES WHERE "
					"	ENT_LEDGER='%s' AND "
					"	ENT_DEFFECT<='%s' AND "
					"	ENT_STATUS='%s'",
					mnemo, sdate,
					ofo_entry_status_get_dbms( ENT_STATUS_ROUGH ));
	g_free( sdate );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					query,
					OFO_TYPE_ENTRY,
					getter );

	g_free( query );

	signaler = ofa_igetter_get_signaler( getter );

	g_signal_emit_by_name( signaler, SIGNALER_CHANGE_COUNT, -1, ENT_STATUS_VALIDATED, ( gulong ) g_list_length( dataset ));

	for( it=dataset ; it ; it=it->next ){
		entry = OFO_ENTRY( it->data );
		ofo_entry_validate( entry );
	}

	ofo_entry_free_dataset( dataset );

	return( TRUE );
}

/**
 * ofo_entry_validate_by_opes:
 * @getter: a #ofaIGetter instance.
 * @opes: a #GList of operations number.
 *
 * Validate all rough or future entries which are related to @opes.
 *
 * Returns: TRUE if success, even if there is no entries at all, while
 * no error is detected.
 */
gboolean
ofo_entry_validate_by_opes( ofaIGetter *getter, GList *opes )
{
	GList *it;
	gboolean ok;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	ok = TRUE;

	for( it=opes ; it && ok ; it=it->next ){
		ok = do_validate_by_ope( getter, ( ofxCounter ) it->data );
	}

	return( ok );
}

static gboolean
do_validate_by_ope( ofaIGetter *getter, ofxCounter openum )
{
	gchar *query;
	ofoEntry *entry;
	GList *dataset, *it;
	ofaISignaler *signaler;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	query = g_strdup_printf( "OFA_T_ENTRIES WHERE ENT_OPE_NUMBER=%lu", openum );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					query,
					OFO_TYPE_ENTRY,
					getter );

	g_free( query );

	signaler = ofa_igetter_get_signaler( getter );

	g_signal_emit_by_name( signaler, SIGNALER_CHANGE_COUNT, -1, ENT_STATUS_VALIDATED, ( gulong ) g_list_length( dataset ));

	for( it=dataset ; it ; it=it->next ){
		entry = OFO_ENTRY( it->data );
		ofo_entry_validate( entry );
	}

	ofo_entry_free_dataset( dataset );

	return( TRUE );
}

/**
 * ofo_entry_delete:
 * @entry: this #ofoEntry object.
 *
 * Delete the entry, by changing its status to ENT_STATUS_DELETED.
 *
 * Entry must be in 'rough' status.
 * Period must be 'current' or 'future'.
 */
void
ofo_entry_delete( ofoEntry *entry )
{
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofeEntryStatus status;
	ofeEntryPeriod period;

	g_return_if_fail( entry && OFO_IS_ENTRY( entry ));
	g_return_if_fail( !OFO_BASE( entry )->prot->dispose_has_run );

	status = ofo_entry_get_status( entry );
	g_return_if_fail( status == ENT_STATUS_ROUGH );

	period = ofo_entry_get_period( entry );
	g_return_if_fail( period == ENT_PERIOD_CURRENT || period == ENT_PERIOD_FUTURE );

	getter = ofo_base_get_getter( OFO_BASE( entry ));
	signaler = ofa_igetter_get_signaler( getter );

	g_signal_emit_by_name( signaler, SIGNALER_PERIOD_STATUS_CHANGE, entry, -1, status, -1, ENT_STATUS_DELETED );
	g_signal_emit_by_name( signaler, SIGNALER_BASE_DELETED, entry );
}

/*
 * myICollectionable interface management
 */
static void
icollectionable_iface_init( myICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_entry_icollectionable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = icollectionable_get_interface_version;
	iface->load_collection = icollectionable_load_collection;
}

static guint
icollectionable_get_interface_version( void )
{
	return( 1 );
}

static GList *
icollectionable_load_collection( void *user_data )
{
	GList *list;

	g_return_val_if_fail( user_data && OFA_IS_IGETTER( user_data ), NULL );

	list = ofo_base_load_dataset(
					st_boxed_defs,
					"OFA_T_ENTRIES",
					OFO_TYPE_ENTRY,
					OFA_IGETTER( user_data ));

	return( list );
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
 * ofaIDoc interface management
 */
static void
idoc_iface_init( ofaIDocInterface *iface )
{
	static const gchar *thisfn = "ofo_entry_idoc_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idoc_get_interface_version;
}

static guint
idoc_get_interface_version( void )
{
	return( 1 );
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
	iface->get_published = iexportable_get_published;
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

static gboolean
iexportable_get_published( const ofaIExportable *instance )
{
	return( TRUE );
}

/*
 * iexportable_export:
 *
 * Exports the entries line by line.
 *
 * Returns: TRUE at the end if no error has been detected
 *
 * v0.38: as the conciliation information have moved to another table,
 * and because we want to stay able to export/import them, we have to
 * add to the dataset the informations got from conciliation groups.
 *
 * v0.56: introduce a format version number prefix (for now: 1).
 * Starting from now, the added conciliation group comes just after this
 * version number, and the entry comes after. This let us add new fields
 * at the end of the entry.
 *
 * v0.70: the version number is now in the very first line (for now: 2).
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const gchar *format_id )
{
	static const gchar *thisfn = "ofo_entry_iexportable_export";

	if( !my_collate( format_id, OFA_IEXPORTER_DEFAULT_FORMAT_ID )){
		return( iexportable_export_default( exportable ));
	}

	if( !my_collate( format_id, ENTRY_FEC_EXPORT_FORMAT )){
		return( ofo_entry_fec_export( exportable ));
	}

	g_warning( "%s: format_id=%s unmanaged here", thisfn, format_id );

	return( FALSE );
}

static gboolean
iexportable_export_default( ofaIExportable *exportable )
{
	ofaIGetter *getter;
	ofaStreamFormat *stformat;
	GList *dataset, *it, *itd;
	gboolean ok, with_headers;
	gchar field_sep;
	gulong count;
	gchar *str1, *str2, *sdate, *suser, *sstamp;
	ofoConcil *concil;
	const gchar *acc_id, *cur_code;
	ofoAccount *account;
	ofoCurrency *currency;
	ofoEntry *entry;
	ofoEntryPrivate *priv;

	getter = ofa_iexportable_get_getter( exportable );
	dataset = ofo_entry_get_dataset( getter );

	stformat = ofa_iexportable_get_stream_format( exportable );
	with_headers = ofa_stream_format_get_with_headers( stformat );
	field_sep = ofa_stream_format_get_field_sep( stformat );

	count = ( gulong ) g_list_length( dataset );
	if( with_headers ){
		count += ENTRY_TABLES_COUNT;
	}
	for( it=dataset ; it ; it=it->next ){
		entry = OFO_ENTRY( it->data );
		count += ofo_entry_doc_get_count( entry );
	}
	ofa_iexportable_set_count( exportable, count+2 );

	/* add version lines at the very beginning of the file */
	str1 = g_strdup_printf( "0%c0%cVersion", field_sep, field_sep );
	ok = ofa_iexportable_append_line( exportable, str1 );
	g_free( str1 );
	if( ok ){
		str1 = g_strdup_printf( "1%c0%c%u", field_sep, field_sep, ENTRY_EXPORT_VERSION );
		ok = ofa_iexportable_append_line( exportable, str1 );
		g_free( str1 );
	}

	/* export a specific version of the headers */
	if( ok && with_headers ){
		str1 = ofa_box_csv_get_header( st_boxed_defs, stformat );
		str2 = g_strdup_printf( "0%c1%c%s%c%s%c%s%c%s",
				field_sep,
				field_sep, "ConcilDval",
				field_sep, "ConcilUser",
				field_sep, "ConcilStamp",
				field_sep, str1 );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str1 );
	}
	if( ok && with_headers ){
		str1 = ofa_box_csv_get_header( st_doc_defs, stformat );
		str2 = g_strdup_printf( "0%c2%c%s", field_sep, field_sep, str1 );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str1 );
	}

	/* export the dataset */
	for( it=dataset ; it && ok ; it=it->next ){
		entry = OFO_ENTRY( it->data );

		acc_id = ofo_entry_get_account( entry );
		g_return_val_if_fail( acc_id && my_strlen( acc_id ), FALSE );
		account = ofo_account_get_by_number( getter, acc_id );
		g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
		cur_code = ofo_account_get_currency( account );
		g_return_val_if_fail( cur_code && my_strlen( cur_code ), FALSE );
		currency = ofo_currency_get_by_code( getter, cur_code );
		g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), FALSE );
		str1 = ofa_box_csv_get_line( OFO_BASE( it->data )->prot->fields, stformat, currency );

		concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ));
		sdate = concil ? my_date_to_str( ofo_concil_get_dval( concil ), MY_DATE_SQL ) : g_strdup( "" );
		suser = g_strdup( concil ? ofo_concil_get_upd_user( concil ) : "" );
		sstamp = concil ? my_stamp_to_str( ofo_concil_get_upd_stamp( concil ), MY_STAMP_YYMDHMS ) : g_strdup( "" );

		str2 = g_strdup_printf( "1%c1%c%s%c%s%c%s%c%s",
				field_sep,
				field_sep, sdate,
				field_sep, suser,
				field_sep, sstamp,
				field_sep, str1 );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str1 );
		g_free( sdate );
		g_free( suser );
		g_free( sstamp );

		priv = ofo_entry_get_instance_private( entry );

		for( itd=priv->docs ; itd && ok ; itd=itd->next ){
			str1 = ofa_box_csv_get_line( itd->data, stformat, NULL );
			str2 = g_strdup_printf( "1%c2%c%s", field_sep, field_sep, str1 );
			ok = ofa_iexportable_append_line( exportable, str2 );
			g_free( str2 );
			g_free( str1 );
		}
	}

	return( ok );
}

/*
 * ofaIExporter interface management
 */
static void
iexporter_iface_init( ofaIExporterInterface *iface )
{
	static const gchar *thisfn = "ofo_entry_iexporter_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iexporter_get_interface_version;
	iface->get_formats = iexporter_get_formats;
}

static guint
iexporter_get_interface_version( void )
{
	return( 1 );
}

static ofsIExporterFormat *
iexporter_get_formats( ofaIExporter *instance, GType type, ofaIGetter *getter )
{
	return( ofo_entry_fec_get_exporter_formats( instance, type, getter ));
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
	ofaHub *hub;
	ofaIDBConnect *connect;

	dataset = iimportable_import_parse( importer, parms, lines );

	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );

	if( parms->parse_errs == 0 && parms->parsed_count > 0 ){
		bck_table = ofa_idbconnect_table_backup( connect, "OFA_T_ENTRIES" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs > 0 ){
			ofa_idbconnect_table_restore( connect, bck_table, "OFA_T_ENTRIES" );
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
	GList *past, *exe, *fut, *it;
	ofsCurrency *sdet;
	ofoCurrency *cur_object;
	ofxCounter counter, tiers;
	myDateFormat date_format;
	ofaHub *hub;
	ofoDossier *dossier;
	ofeEntryPeriod period;

	numline = 0;
	dataset = NULL;
	total = g_slist_length( lines );

	ofa_iimporter_progress_start( importer, parms );

	past = NULL;
	exe = NULL;
	fut = NULL;
	date_format = ofa_stream_format_get_date_format( parms->format );
	hub = ofa_igetter_get_hub( parms->getter );
	dossier = ofa_hub_get_dossier( hub );

	for( itl=lines ; itl ; itl=itl->next ){

		if( parms->stop && parms->parse_errs > 0 ){
			break;
		}

		numline += 1;
		fields = ( GSList * ) itl->data;
		entry = ofo_entry_new( parms->getter );
		debit = 0;
		credit = 0;

		/* first field is a version number or the operation date */
		itf = fields;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		my_date_set_from_str( &date, cstr, date_format );
		format = atoi( cstr );

		/* valid format >= 1 */
		if( !my_date_is_valid( &date ) && format > 0 && format <= ENTRY_EXPORT_VERSION ){

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
		ledger = ofo_ledger_get_by_mnemo( parms->getter, cstr );
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
		account = ofo_account_get_by_number( parms->getter, cstr );
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
		cur_object = ofo_currency_get_by_code( parms->getter, currency );
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

		} else {
			/* entry rule */
			itf = itf ? itf->next : NULL;
			cstr = itf ? ( const gchar * ) itf->data : NULL;
			if( my_strlen( cstr )){
				ofa_box_set_string( OFO_BASE( entry )->prot->fields, ENT_RULE, cstr );
			}

			/* notes */
			itf = itf ? itf->next : NULL;
			cstr = itf ? ( const gchar * ) itf->data : NULL;
			if( my_strlen( cstr )){
				ofo_entry_set_notes( entry, cstr );
			}

			/* tiers */
			itf = itf ? itf->next : NULL;
			cstr = itf ? ( const gchar * ) itf->data : NULL;
			if( my_strlen( cstr )){
				tiers = atol( cstr );
				if( tiers > 0 ){
					ofo_entry_set_tiers( entry, tiers );
				}
			}

			/* period indicator */
			itf = itf ? itf->next : NULL;
			cstr = itf ? ( const gchar * ) itf->data : NULL;
			if( my_strlen( cstr )){
				ofa_box_set_string( OFO_BASE( entry )->prot->fields, ENT_IPERIOD, cstr );
			}
		}

		/* what to do regarding the effect date ?
		 * we force it to be valid regarding exercice beginning and
		 * ledger last closing dates, so that the entry is in ROUGH
		 * status */
		entry_compute_status( entry, TRUE, parms->getter );
		period = ofo_entry_get_period( entry );
		switch( period ){
			case ENT_PERIOD_PAST:
				ofs_currency_add_by_object( &past, cur_object, debit, credit );
				break;

			case ENT_PERIOD_CURRENT:
				ofs_currency_add_by_object( &exe, cur_object, debit, credit );
				break;

			case ENT_PERIOD_FUTURE:
				ofs_currency_add_by_object( &fut, cur_object, debit, credit );
				break;

			default:
				str = g_strdup_printf( _( "invalid entry period: %u" ), period );
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
		sdeb = ofa_amount_to_str( sdet->debit, sdet->currency, parms->getter );
		scre = ofa_amount_to_str( sdet->credit, sdet->currency, parms->getter );
		str = g_strdup_printf( "PAST [%s] tot_debits=%s, tot_credits=%s",
					ofo_currency_get_code( sdet->currency ), sdeb, scre );
		ofa_iimporter_progress_num_text( importer, parms, numline, str );
		g_free( str );
		g_free( sdeb );
		g_free( scre );
	}
	for( it=exe ; it ; it=it->next ){
		sdet = ( ofsCurrency * ) it->data;
		sdeb = ofa_amount_to_str( sdet->debit, sdet->currency, parms->getter );
		scre = ofa_amount_to_str( sdet->credit, sdet->currency, parms->getter );
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
		sdeb = ofa_amount_to_str( sdet->debit, sdet->currency, parms->getter );
		scre = ofa_amount_to_str( sdet->credit, sdet->currency, parms->getter );
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
	const gchar *cstr, *userid;
	GDate date;
	GTimeVal stamp;
	ofoConcil *concil;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	myDateFormat date_format;

	concil = NULL;
	itf = *fields;
	date_format = ofa_stream_format_get_date_format( parms->format );
	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );

	/* reconciliation date */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	my_date_set_from_str( &date, cstr, date_format );
	if( my_date_is_valid( &date )){
		concil = ofo_concil_new( parms->getter );
		g_object_set_data( G_OBJECT( entry ), "entry-concil", concil );
		ofo_concil_set_dval( concil, &date );
		g_debug( "%s: new concil dval=%s", thisfn, cstr );
	}

	/* exported reconciliation user (defaults to current user) */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( concil ){
		userid = cstr;
		if( !my_strlen( userid )){
			userid = ofa_idbconnect_get_account( connect );
		}
		ofo_concil_set_upd_user( concil, userid );
		g_debug( "%s: new concil user=%s", thisfn, userid );
	}

	/* exported reconciliation timestamp (defaults to now)
	 * the timestamp is exported (and expected) as MY_STAMP_YYMDHMS */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( concil ){
		if( !my_strlen( cstr )){
			my_stamp_set_now( &stamp );
		} else {
			my_stamp_set_from_sql( &stamp, cstr );
		}
		ofo_concil_set_upd_stamp( concil, &stamp );
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
	ofxCounter counter;
	ofoConcil *concil;
	ofaISignaler *signaler;
	ofaHub *hub;

	total = g_list_length( dataset );

	signaler = ofa_igetter_get_signaler( parms->getter );
	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );
	ofa_iimporter_progress_start( importer, parms );

	if( parms->empty && total > 0 ){
		entry_drop_content( connect );
	}

	for( it=dataset ; it ; it=it->next ){

		if( parms->stop && parms->insert_errs > 0 ){
			break;
		}

		entry = OFO_ENTRY( it->data );
		entry_set_number( entry, ofo_counters_get_next_entry_id( parms->getter ));

		if( entry_do_insert( entry, parms->getter )){

			if( entry_get_import_settled( entry )){
				counter = ofo_counters_get_next_settlement_id( parms->getter );
				ofo_entry_update_settlement( entry, counter );
			}
			concil = ( ofoConcil * ) g_object_get_data( G_OBJECT( entry ), "entry-concil" );
			if( concil ){
				/* gives the ownership to the collection */
				ofa_iconcil_new_concil_ex( OFA_ICONCIL( entry ), concil );
			}
			if( ofo_entry_get_period( entry ) != ENT_PERIOD_PAST ){
				g_signal_emit_by_name( signaler, SIGNALER_BASE_NEW, entry );
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
 * ofaISignalable interface management
 */
static void
isignalable_iface_init( ofaISignalableInterface *iface )
{
	static const gchar *thisfn = "ofo_entry_isignalable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect_to = isignalable_connect_to;
}

static void
isignalable_connect_to( ofaISignaler *signaler )
{
	static const gchar *thisfn = "ofo_entry_isignalable_connect_to";

	g_debug( "%s: signaler=%p", thisfn, ( void * ) signaler );

	g_return_if_fail( signaler && OFA_IS_ISIGNALER( signaler ));

	g_signal_connect( signaler, SIGNALER_BASE_IS_DELETABLE, G_CALLBACK( signaler_on_deletable_object ), NULL );
	g_signal_connect( signaler, SIGNALER_BASE_DELETED, G_CALLBACK( signaler_on_deleted_base ), NULL );
	g_signal_connect( signaler, SIGNALER_EXERCICE_DATES_CHANGED, G_CALLBACK( signaler_on_exe_dates_changed ), NULL );
	g_signal_connect( signaler, SIGNALER_PERIOD_STATUS_CHANGE, G_CALLBACK( signaler_on_entry_period_status_changed ), NULL );
	g_signal_connect( signaler, SIGNALER_BASE_UPDATED, G_CALLBACK( signaler_on_updated_base ), NULL );
}

/*
 * SIGNALER_BASE_IS_DELETABLE signal handler
 */
static gboolean
signaler_on_deletable_object( ofaISignaler *signaler, ofoBase *object, void *empty )
{
	static const gchar *thisfn = "ofo_entry_signaler_on_deletable_object";
	gboolean deletable;

	g_debug( "%s: signaler=%p, object=%p (%s), empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	deletable = TRUE;

	if( OFO_IS_ACCOUNT( object )){
		deletable = signaler_is_deletable_account( signaler, OFO_ACCOUNT( object ));

	} else if( OFO_IS_CURRENCY( object )){
		deletable = signaler_is_deletable_currency( signaler, OFO_CURRENCY( object ));

	} else if( OFO_IS_LEDGER( object )){
		deletable = signaler_is_deletable_ledger( signaler, OFO_LEDGER( object ));

	} else if( OFO_IS_OPE_TEMPLATE( object )){
		deletable = signaler_is_deletable_ope_template( signaler, OFO_OPE_TEMPLATE( object ));
	}

	return( deletable );
}

static gboolean
signaler_is_deletable_account( ofaISignaler *signaler, ofoAccount *account )
{
	ofaIGetter *getter;

	getter = ofa_isignaler_get_getter( signaler );

	return( !ofo_entry_use_account( getter, ofo_account_get_number( account )));
}

static gboolean
signaler_is_deletable_currency( ofaISignaler *signaler, ofoCurrency *currency )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;
	gint count;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM OFA_T_ENTRIES WHERE ENT_CURRENCY='%s'",
			ofo_currency_get_code( currency ));

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count == 0 );
}

static gboolean
signaler_is_deletable_ledger( ofaISignaler *signaler, ofoLedger *ledger )
{
	ofaIGetter *getter;

	getter = ofa_isignaler_get_getter( signaler );

	return( !ofo_entry_use_ledger( getter, ofo_ledger_get_mnemo( ledger )));
}

static gboolean
signaler_is_deletable_ope_template( ofaISignaler *signaler, ofoOpeTemplate *template )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;
	gint count;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM OFA_T_ENTRIES WHERE ENT_OPE_TEMPLATE='%s'",
			ofo_ope_template_get_mnemo( template ));

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count == 0 );
}

/*
 * SIGNALER_BASE_DELETED signal handler
 */
static void
signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, void *empty )
{
	static const gchar *thisfn = "ofo_entry_signaler_on_deleted_base";

	g_debug( "%s: signaler=%p, object=%p (%s), empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	if( OFO_IS_ENTRY( object )){
		signaler_on_deleted_entry( signaler, OFO_ENTRY( object ));
	}
}

static void
signaler_on_deleted_entry( ofaISignaler *signaler, ofoEntry *entry )
{
	static const gchar *thisfn = "ofo_entry_signaler_on_deleted_entry";
	ofxCounter id;
	ofoConcil *concil;
	ofaIGetter *getter;

	getter = ofa_isignaler_get_getter( signaler );

	/* if entry was settled, then cleanup whole settlement group */
	id = ofo_entry_get_settlement_number( entry );
	if( id > 0 ){
		ofo_entry_unsettle_by_number( getter, id );
	}

	/* if entry was conciliated, then cleanup whole conciliation group */
	concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ));
	if( concil ){
		ofa_iconcil_remove_concil( OFA_ICONCIL( entry ), concil );
	} else {
		g_debug( "%s: entry=%p: conciliation group is null", thisfn, ( void * ) entry );
	}
}

/*
 * SIGNALER_EXERCICE_DATES_CHANGED signal handler.
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
signaler_on_exe_dates_changed( ofaISignaler *signaler, const GDate *prev_begin, const GDate *prev_end, void *empty )
{
	ofaIGetter *getter;
	ofaHub *hub;
	ofoDossier *dossier;
	const GDate *new_begin, *new_end;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );
	dossier = ofa_hub_get_dossier( hub );

	new_begin = ofo_dossier_get_exe_begin( dossier );
	check_for_changed_begin_exe_dates( getter, prev_begin, new_begin, TRUE );

	new_end = ofo_dossier_get_exe_end( dossier );
	check_for_changed_end_exe_dates( getter, prev_end, new_end, TRUE );
}

/*
 * Returns: the count of remediated entries
 */
static gint
check_for_changed_begin_exe_dates( ofaIGetter *getter, const GDate *prev_begin, const GDate *new_begin, gboolean remediate )
{
	gint count;
	gchar *sprev, *snew, *where;

	count = 0;

	if( remediate ){
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
						"ENT_DEFFECT<'%s' AND ENT_STATUS!='%s'",
						snew, ofo_entry_status_get_dbms( ENT_STATUS_DELETED ));
				count = remediate_status( getter, where, ENT_PERIOD_PAST );
			}
		} else if( !my_date_is_valid( new_begin )){
			/* removing the beginning date of the exercice
			 * there may be entries which were considered in the past
			 * but are now considered in the exercice */
			/*count = move_from_past_to_exercice( dossier, prev_begin, new_begin, remediate );*/
			where = g_strdup_printf(
					"ENT_DEFFECT<'%s' AND ENT_STATUS!='%s'",
					sprev, ofo_entry_status_get_dbms( ENT_STATUS_DELETED ));
			count = remediate_status( getter, where, ENT_PERIOD_CURRENT );

		} else if( my_date_compare( prev_begin, new_begin ) < 0 ){
			/* there may be entries which were considered in the exercice
			 * but are now considered in the past */
			/*count = move_from_exercice_to_past( dossier, prev_begin, new_begin, remediate );*/
			where = g_strdup_printf(
					"ENT_DEFFECT>='%s' AND ENT_DEFFECT<'%s' AND ENT_STATUS!='%s'",
					sprev, snew, ofo_entry_status_get_dbms( ENT_STATUS_DELETED ));
			count = remediate_status( getter, where, ENT_PERIOD_PAST );

		} else if( my_date_compare( prev_begin, new_begin ) > 0 ){
			/* there may be entries which were considered in the past
			 * but are now considered in the exercice */
			/*count = move_from_past_to_exercice( dossier, prev_begin, new_begin, remediate );*/
			where = g_strdup_printf(
					"ENT_DEFFECT<'%s' AND ENT_DEFFECT>='%s' AND ENT_STATUS!='%s'",
					sprev, snew, ofo_entry_status_get_dbms( ENT_STATUS_DELETED ));
			count = remediate_status( getter, where, ENT_PERIOD_CURRENT );
		}

		g_free( sprev );
		g_free( snew );
		g_free( where );
	}

	return( count );
}

/*
 * Returns: the count of remediated entries
 */
static gint
check_for_changed_end_exe_dates( ofaIGetter *getter, const GDate *prev_end, const GDate *new_end, gboolean remediate )
{
	gint count;
	gchar *sprev, *snew, *where;

	count = 0;

	if( remediate ){
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
						"ENT_DEFFECT>'%s' AND ENT_STATUS!='%s'",
						snew, ofo_entry_status_get_dbms( ENT_STATUS_DELETED ));
				count = remediate_status( getter, where, ENT_PERIOD_FUTURE );
			}
		} else if( !my_date_is_valid( new_end )){
			/* removing the ending date of the exercice
			 * there may be entries which were considered in the future
			 * but are now considered in the exercice */
			/*count = move_from_future_to_exercice( dossier, prev_end, new_end, remediate );*/
			where = g_strdup_printf(
					"ENT_DEFFECT>'%s' AND ENT_STATUS!='%s'",
					sprev, ofo_entry_status_get_dbms( ENT_STATUS_DELETED ));
			count = remediate_status( getter, where, ENT_PERIOD_CURRENT );

		} else if( my_date_compare( prev_end, new_end ) < 0 ){
			/* there may be entries which were considered in the future
			 * but are now considered in the exercice */
			/*count = move_from_future_to_exercice( dossier, prev_end, new_end, remediate );*/
			where = g_strdup_printf(
					"ENT_DEFFECT>'%s' AND ENT_DEFFECT<='%s' AND ENT_STATUS!='%s'",
					sprev, snew, ofo_entry_status_get_dbms( ENT_STATUS_DELETED ));
			count = remediate_status( getter, where, ENT_PERIOD_CURRENT );

		} else if( my_date_compare( prev_end, new_end ) > 0 ){
			/* there may be entries which were considered in the exercice
			 * but are now considered in the future */
			/*count = move_from_exercice_to_future( dossier, prev_end, new_end, remediate );*/
			where = g_strdup_printf(
					"ENT_DEFFECT<='%s' AND ENT_DEFFECT>'%s' AND ENT_STATUS!='%s'",
					sprev, snew, ofo_entry_status_get_dbms( ENT_STATUS_DELETED ));
			count = remediate_status( getter, where, ENT_PERIOD_FUTURE );
		}
	}

	return( count );
}

/*
 * Due to a change of the beginning or ending exercice date, entries may
 * have to be remediated:
 * Past    (validated)          -> current (validated)
 * Current (rough or validated) -> past    (validated) or future (rough or validated)
 * Future  (rough or validated) -> current (rough or validated)
 *
 * There is so one case where the entry changes simultaneously its period
 * indicator and its status.
 */
static gint
remediate_status( ofaIGetter *getter, const gchar *where, ofeEntryPeriod new_period )
{
	gulong count;
	GList *dataset, *it;
	ofoEntry *entry;
	ofeEntryPeriod prev_period;
	ofeEntryStatus prev_status, new_status;
	ofaISignaler *signaler;

	count = 0;
	dataset = entry_load_dataset( getter, where, NULL );
	count = g_list_length( dataset );

	signaler = ofa_igetter_get_signaler( getter );
	g_signal_emit_by_name( signaler, SIGNALER_CHANGE_COUNT, new_period, -1, count );

	for( it=dataset ; it ; it=it->next ){
		entry = OFO_ENTRY( it->data );
		prev_period = ofo_entry_get_period( entry );
		prev_status = -1;
		new_status = -1;

		/* should we also remediate the status ? */
		if( prev_period == ENT_PERIOD_CURRENT && new_period == ENT_PERIOD_PAST ){
			prev_status = ofo_entry_get_status( entry );
			if( prev_status == ENT_STATUS_ROUGH ){
				new_status = ENT_STATUS_VALIDATED;
			} else {
				g_return_val_if_fail( prev_status == ENT_STATUS_VALIDATED, 0 );
				prev_status = -1;
			}
		}

		g_signal_emit_by_name( signaler, SIGNALER_PERIOD_STATUS_CHANGE, entry, prev_period, prev_status, new_period, new_status );
	}

	ofo_entry_free_dataset( dataset );

	return( count );
}

/*
 * SIGNALER_PERIOD_STATUS_CHANGE signal handler
 */
static void
signaler_on_entry_period_status_changed( ofaISignaler *signaler, ofoEntry *entry,
											ofeEntryPeriod prev_period, ofeEntryStatus prev_status,
											ofeEntryPeriod new_period, ofeEntryStatus new_status,
											void *empty )
{
	static const gchar *thisfn = "ofo_entry_signaler_on_entry_period_status_changed";
	gchar *query;
	ofaIGetter *getter;
	ofaHub *hub;
	const gchar *cperiod, *cstatus;

	g_debug( "%s: signaler=%p, entry=%p, prev_period=%d, prev_status=%d, new_period=%d, new_status=%d, empty=%p",
			thisfn, ( void * ) signaler, ( void * ) entry,
			prev_period, prev_status, new_period, new_status, ( void * ) empty );

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

	if( new_period != -1 ){
		entry_set_period( entry, new_period );
	}
	cperiod = ofa_box_get_string( OFO_BASE( entry )->prot->fields, ENT_IPERIOD );

	if( new_status != -1 ){
		entry_set_status( entry, new_status );
	}
	cstatus = ofa_box_get_string( OFO_BASE( entry )->prot->fields, ENT_STATUS );

	query = g_strdup_printf(
					"UPDATE OFA_T_ENTRIES SET ENT_IPERIOD='%s',ENT_STATUS='%s' WHERE ENT_NUMBER=%ld",
						cperiod, cstatus, ofo_entry_get_number( entry ));

	if( ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE )){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, entry, NULL );
	}

	g_free( query );
}

/*
 * SIGNALER_BASE_UPDATED signal handler
 *
 * we try to report in recorded entries the modifications which may
 * happen on one of the external identifiers - but only for the current
 * exercice
 *
 * Nonetheless, this is never a good idea to modify an identifier which
 * is publicly known, and this always should be done with the greatest
 * attention
 */
static void
signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, void *empty )
{
	static const gchar *thisfn = "ofo_entry_signaler_on_updated_base";
	const gchar *number;
	const gchar *code;
	const gchar *mnemo;

	g_debug( "%s: signaler=%p, object=%p (%s), prev_id=%s, empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) empty );

	if( OFO_IS_ACCOUNT( object )){
		if( my_strlen( prev_id )){
			number = ofo_account_get_number( OFO_ACCOUNT( object ));
			if( g_utf8_collate( number, prev_id )){
				signaler_on_updated_account_number( signaler, prev_id, number );
			}
		}

	} else if( OFO_IS_CURRENCY( object )){
		if( my_strlen( prev_id )){
			code = ofo_currency_get_code( OFO_CURRENCY( object ));
			if( g_utf8_collate( code, prev_id )){
				signaler_on_updated_currency_code( signaler, prev_id, code );
			}
		}

	} else if( OFO_IS_LEDGER( object )){
		if( my_strlen( prev_id )){
			mnemo = ofo_ledger_get_mnemo( OFO_LEDGER( object ));
			if( g_utf8_collate( mnemo, prev_id )){
				signaler_on_updated_ledger_mnemo( signaler, prev_id, mnemo );
			}
		}

	} else if( OFO_IS_OPE_TEMPLATE( object )){
		if( my_strlen( prev_id )){
			mnemo = ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object ));
			if( g_utf8_collate( mnemo, prev_id )){
				signaler_on_updated_model_mnemo( signaler, prev_id, mnemo );
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
signaler_on_updated_account_number( ofaISignaler *signaler, const gchar *prev_id, const gchar *number )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

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
signaler_on_updated_currency_code( ofaISignaler *signaler, const gchar *prev_id, const gchar *code )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

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
signaler_on_updated_ledger_mnemo( ofaISignaler *signaler, const gchar *prev_id, const gchar *mnemo )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

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
signaler_on_updated_model_mnemo( ofaISignaler *signaler, const gchar *prev_id, const gchar *mnemo )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

	query = g_strdup_printf(
			"UPDATE OFA_T_ENTRIES"
			"	SET ENT_OPE_TEMPLATE='%s' WHERE ENT_OPE_TEMPLATE='%s' ", mnemo, prev_id );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );
	g_free( query );
}
