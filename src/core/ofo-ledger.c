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
#include <stdlib.h>
#include <string.h>

#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-icollectionable.h"
#include "my/my-icollector.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-isignal-hub.h"
#include "api/ofa-stream-format.h"
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
enum {
	LED_MNEMO = 1,
	LED_LABEL,
	LED_NOTES,
	LED_UPD_USER,
	LED_UPD_STAMP,
	LED_LAST_CLO,
	LED_CURRENCY,
	LED_VAL_DEBIT,
	LED_VAL_CREDIT,
	LED_ROUGH_DEBIT,
	LED_ROUGH_CREDIT,
	LED_FUT_DEBIT,
	LED_FUT_CREDIT,
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order. So:
 * 1/ put in in an order compatible with import
 * 2/ no more modify it
 * 3/ take attention to be able to support the import of a previously
 *    exported file
 */
static const ofsBoxDef st_boxed_defs[] = {
		{ OFA_BOX_CSV( LED_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* amount, counter: export zero as empty */
		{ OFA_BOX_CSV( LED_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( LED_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( LED_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( LED_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( LED_LAST_CLO ),
				OFA_TYPE_DATE,
				FALSE,
				FALSE },
		{ 0 }
};

static const ofsBoxDef st_balance_defs[] = {
		{ LED_MNEMO,
				"LED_MNEMO",
				NULL,
				OFA_TYPE_STRING,
				FALSE,					/* importable */
				FALSE },				/* export zero as empty */
		{ LED_CURRENCY,
				"LED_CUR_CODE",
				"LedCurrency",
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ LED_VAL_DEBIT,
				"LED_CUR_VAL_DEBIT",
				"LedCurValDebit",
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ LED_VAL_CREDIT,
				"LED_CUR_VAL_CREDIT",
				"LedCurValCredit",
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ LED_ROUGH_DEBIT,
				"LED_CUR_ROUGH_DEBIT",
				"LedCurRoughDebit",
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ LED_ROUGH_CREDIT,
				"LED_CUR_ROUGH_CREDIT",
				"LedCurRoughCredit",
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ LED_FUT_DEBIT,
				"LED_CUR_FUT_DEBIT",
				"LedCurFutureDebit",
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ LED_FUT_CREDIT,
				"LED_CUR_FUT_CREDIT",
				"LedCurFutureCredit",
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ 0 }
};

typedef struct {

	/* the balances per currency as a GList of GList fields
	 */
	GList *balances;
}
	ofoLedgerPrivate;

static ofoLedger *ledger_find_by_mnemo( GList *set, const gchar *mnemo );
static gint       ledger_count_for_currency( const ofaIDBConnect *connect, const gchar *currency );
static gint       ledger_count_for( const ofaIDBConnect *connect, const gchar *field, const gchar *mnemo );
static gint       cmp_currencies( const gchar *a_currency, const gchar *b_currency );
static GList     *ledger_find_balance_by_code( const ofoLedger *ledger, const gchar *currency );
static GList     *ledger_new_balance_with_code( ofoLedger *ledger, const gchar *currency );
static GList     *ledger_add_balance_rough( ofoLedger *ledger, const gchar *currency, ofxAmount debit, ofxAmount credit );
static GList     *ledger_add_balance_validated( ofoLedger *ledger, const gchar *currency, ofxAmount debit, ofxAmount credit );
static GList     *ledger_add_balance_future( ofoLedger *ledger, const gchar *currency, ofxAmount debit, ofxAmount credit );
static GList     *ledger_add_to_balance( ofoLedger *ledger, const gchar *currency, gint debit_id, ofxAmount debit, gint credit_id, ofxAmount credit );
static void       ledger_set_upd_user( ofoLedger *ledger, const gchar *upd_user );
static void       ledger_set_upd_stamp( ofoLedger *ledger, const GTimeVal *upd_stamp );
static void       ledger_set_last_clo( ofoLedger *ledger, const GDate *date );
static gboolean   ledger_do_insert( ofoLedger *ledger, const ofaIDBConnect *connect );
static gboolean   ledger_insert_main( ofoLedger *ledger, const ofaIDBConnect *connect );
static gboolean   ledger_do_update( ofoLedger *ledger, const gchar *prev_mnemo, const ofaIDBConnect *connect );
static gboolean   ledger_do_update_balance( const ofoLedger *ledger, GList *balance, ofaHub *hub );
static gboolean   ledger_do_delete( ofoLedger *ledger, const ofaIDBConnect *connect );
static gint       ledger_cmp_by_mnemo( const ofoLedger *a, const gchar *mnemo );
static gint       ledger_cmp_by_ptr( const ofoLedger *a, const ofoLedger *b );
static void       icollectionable_iface_init( myICollectionableInterface *iface );
static guint      icollectionable_get_interface_version( void );
static GList     *icollectionable_load_collection( void *user_data );
static void       iexportable_iface_init( ofaIExportableInterface *iface );
static guint      iexportable_get_interface_version( void );
static gchar     *iexportable_get_label( const ofaIExportable *instance );
static gboolean   iexportable_export( ofaIExportable *exportable, const ofaStreamFormat *settings, ofaHub *hub );
static gchar     *export_cb( const ofsBoxData *box_data, const ofaStreamFormat *format, const gchar *text, ofoCurrency *currency );
static void       iimportable_iface_init( ofaIImportableInterface *iface );
static guint      iimportable_get_interface_version( void );
static gchar     *iimportable_get_label( const ofaIImportable *instance );
static guint      iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static GList     *iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static void       iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean   ledger_get_exists( const ofoLedger *ledger, const ofaIDBConnect *connect );
static gboolean   ledger_drop_content( const ofaIDBConnect *connect );
static void       isignal_hub_iface_init( ofaISignalHubInterface *iface );
static void       isignal_hub_connect( ofaHub *hub );
static void       on_hub_new_object( ofaHub *hub, ofoBase *object, void *empty );
static void       on_new_ledger_entry( ofaHub *hub, ofoEntry *entry );
static void       on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, void *empty );
static void       on_updated_object_currency_code( ofaHub *hub, const gchar *prev_id, const gchar *code );
static void       on_hub_entry_status_change( ofaHub *hub, ofoEntry *entry, ofaEntryStatus prev_status, ofaEntryStatus new_status, void *empty );

G_DEFINE_TYPE_EXTENDED( ofoLedger, ofo_ledger, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoLedger )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNAL_HUB, isignal_hub_iface_init ))

static void
free_detail_cur( GList *fields )
{
	ofa_box_free_fields_list( fields );
}

static void
free_detail_currencies( ofoLedger *ledger )
{
	ofoLedgerPrivate *priv;

	priv = ofo_ledger_get_instance_private( ledger );

	g_list_free_full( priv->balances, ( GDestroyNotify ) free_detail_cur );
	priv->balances = NULL;
}

static void
ledger_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_ledger_finalize";

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, LED_MNEMO ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, LED_LABEL ));

	g_return_if_fail( instance && OFO_IS_LEDGER( instance ));

	/* free data members here */

	free_detail_currencies( OFO_LEDGER( instance ));

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_ledger_parent_class )->finalize( instance );
}

static void
ledger_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_LEDGER( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_ledger_parent_class )->dispose( instance );
}

static void
ofo_ledger_init( ofoLedger *self )
{
	static const gchar *thisfn = "ofo_ledger_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
}

static void
ofo_ledger_class_init( ofoLedgerClass *klass )
{
	static const gchar *thisfn = "ofo_ledger_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledger_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledger_finalize;
}

/**
 * ofo_ledger_get_dataset:
 * @hub: the current #ofaHub object.
 *
 * Returns: the full #ofoLedger dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_ledger_get_dataset( ofaHub *hub )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	return( my_icollector_collection_get( ofa_hub_get_collector( hub ), OFO_TYPE_LEDGER, hub ));
}

/**
 * ofo_ledger_get_by_mnemo:
 *
 * Returns: the searched ledger, or %NULL.
 *
 * The returned object is owned by the #ofoLedger class, and should
 * not be unreffed by the caller.
 */
ofoLedger *
ofo_ledger_get_by_mnemo( ofaHub *hub, const gchar *mnemo )
{
	GList *dataset;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( my_strlen( mnemo ), NULL );

	dataset = ofo_ledger_get_dataset( hub );

	return( ledger_find_by_mnemo( dataset, mnemo ));
}

static ofoLedger *
ledger_find_by_mnemo( GList *set, const gchar *mnemo )
{
	GList *found;

	found = g_list_find_custom(
				set, mnemo, ( GCompareFunc ) ledger_cmp_by_mnemo );
	if( found ){
		return( OFO_LEDGER( found->data ));
	}

	return( NULL );
}

/**
 * ofo_ledger_use_currency:
 *
 * Returns: %TRUE if a recorded ledger makes use of the specified
 * currency.
 */
gboolean
ofo_ledger_use_currency( ofaHub *hub, const gchar *currency )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

	/* make sure the collection is loaded */
	ofo_ledger_get_dataset( hub );

	return( ledger_count_for_currency( ofa_hub_get_connect( hub ), currency ) > 0 );
}

static gint
ledger_count_for_currency( const ofaIDBConnect *connect, const gchar *currency )
{
	return( ledger_count_for( connect, "LED_CUR_CODE", currency ));
}

static gint
ledger_count_for( const ofaIDBConnect *connect, const gchar *field, const gchar *mnemo )
{
	gint count;
	gchar *query;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_LEDGERS_CUR WHERE %s='%s'", field, mnemo );

	ofa_idbconnect_query_int( connect, query, &count, TRUE );

	g_free( query );

	return( count );
}

/**
 * ofo_ledger_new:
 */
ofoLedger *
ofo_ledger_new( void )
{
	ofoLedger *ledger;

	ledger = g_object_new( OFO_TYPE_LEDGER, NULL );
	OFO_BASE( ledger )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( ledger );
}

/**
 * ofo_ledger_get_mnemo:
 */
const gchar *
ofo_ledger_get_mnemo( const ofoLedger *ledger )
{
	ofo_base_getter( LEDGER, ledger, string, NULL, LED_MNEMO );
}

/**
 * ofo_ledger_get_label:
 */
const gchar *
ofo_ledger_get_label( const ofoLedger *ledger )
{
	ofo_base_getter( LEDGER, ledger, string, NULL, LED_LABEL );
}

/**
 * ofo_ledger_get_notes:
 */
const gchar *
ofo_ledger_get_notes( const ofoLedger *ledger )
{
	ofo_base_getter( LEDGER, ledger, string, NULL, LED_NOTES );
}

/**
 * ofo_ledger_get_upd_user:
 */
const gchar *
ofo_ledger_get_upd_user( const ofoLedger *ledger )
{
	ofo_base_getter( LEDGER, ledger, string, NULL, LED_UPD_USER );
}

/**
 * ofo_ledger_get_upd_stamp:
 */
const GTimeVal *
ofo_ledger_get_upd_stamp( const ofoLedger *ledger )
{
	ofo_base_getter( LEDGER, ledger, timestamp, NULL, LED_UPD_STAMP );
}

/**
 * ofo_ledger_get_last_close:
 *
 * Returns the last closing date for this ledger.
 * The returned date is not %NULL, but may be invalid if the ledger
 * has not been closed yet during the exercice.
 */
const GDate *
ofo_ledger_get_last_close( const ofoLedger *ledger )
{
	ofo_base_getter( LEDGER, ledger, date, NULL, LED_LAST_CLO );
}

/**
 * ofo_ledger_get_last_entry:
 * @ledger: this #ofoLedger object.
 * @date: [out]: a #GDate to receive the output date.
 *
 * Set @date to the most recent effect date on this ledger,
 * or %NULL if no entry has been found for this ledger.
 *
 * Returns: a pointer to @date.
 */
GDate *
ofo_ledger_get_last_entry( const ofoLedger *ledger, GDate *date )
{
	ofaHub *hub;
	gchar *query;
	GSList *result, *icol;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), NULL );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, NULL );

	hub = ofo_base_get_hub( OFO_BASE( ledger ));

	query = g_strdup_printf(
			"SELECT MAX(ENT_DEFFECT) FROM OFA_T_ENTRIES "
			"	WHERE ENT_LEDGER='%s'", ofo_ledger_get_mnemo( ledger ));

	if( ofa_idbconnect_query_ex( ofa_hub_get_connect( hub ), query, &result, TRUE )){
		icol = ( GSList * ) result->data;
		my_date_set_from_sql( date, ( const gchar * ) icol->data );
		ofa_idbconnect_free_results( result );
	}
	g_free( query );

	return( date );
}

/**
 * ofo_ledger_get_currencies:
 *
 * Returns: the list of currency codes ISO 3A used by this ledger.
 *
 * The content of returned list is owned by the ledger, and should only
 * be g_list_free() by the caller.
 */
GList *
ofo_ledger_get_currencies( const ofoLedger *ledger )
{
	ofoLedgerPrivate *priv;
	GList *list, *it, *balance;
	const gchar *currency;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), NULL );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, NULL );

	priv = ofo_ledger_get_instance_private( ledger );
	list = NULL;

	for( it=priv->balances ; it ; it=it->next ){
		balance = ( GList * ) it->data;
		currency = ofa_box_get_string( balance, LED_CURRENCY );
		list = g_list_insert_sorted( list, ( gpointer ) currency, ( GCompareFunc ) cmp_currencies );
	}

	return( list );
}

static gint
cmp_currencies( const gchar *a_currency, const gchar *b_currency )
{
	return( g_utf8_collate( a_currency, b_currency ));
}

/**
 * ofo_ledger_get_val_debit:
 * @ledger:
 * @currency:
 *
 * Returns the debit balance of this ledger for validated entries of
 * the exercice, or zero if not found.
 */
ofxAmount
ofo_ledger_get_val_debit( const ofoLedger *ledger, const gchar *currency )
{
	GList *sdev;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), 0 );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, 0 );

	sdev = ledger_find_balance_by_code( ledger, currency );
	if( sdev ){
		return( ofa_box_get_amount( sdev, LED_VAL_DEBIT ));
	}

	return( 0 );
}

/**
 * ofo_ledger_get_val_credit:
 * @ledger:
 * @currency:
 *
 * Returns the credit balance of this ledger for validated entries of
 * the exercice, or zero if not found.
 */
ofxAmount
ofo_ledger_get_val_credit( const ofoLedger *ledger, const gchar *currency )
{
	GList *sdev;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), 0 );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, 0 );

	sdev = ledger_find_balance_by_code( ledger, currency );
	if( sdev ){
		return( ofa_box_get_amount( sdev, LED_VAL_CREDIT ));
	}

	return( 0 );
}

/**
 * ofo_ledger_get_rough_debit:
 * @ledger:
 * @currency:
 *
 * Returns the current debit balance of this ledger for
 * the currency specified in the exercice, or zero if not found.
 */
ofxAmount
ofo_ledger_get_rough_debit( const ofoLedger *ledger, const gchar *currency )
{
	GList *sdev;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), 0 );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, 0 );

	sdev = ledger_find_balance_by_code( ledger, currency );
	if( sdev ){
		return( ofa_box_get_amount( sdev, LED_ROUGH_DEBIT ));
	}

	return( 0 );
}

/**
 * ofo_ledger_get_rough_credit:
 * @ledger:
 * @currency:
 *
 * Returns the current credit balance of this ledger for
 * the currency specified in the exercice, or zero if not found.
 */
ofxAmount
ofo_ledger_get_rough_credit( const ofoLedger *ledger, const gchar *currency )
{
	GList *sdev;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), 0 );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, 0 );

	sdev = ledger_find_balance_by_code( ledger, currency );
	if( sdev ){
		return( ofa_box_get_amount( sdev, LED_ROUGH_CREDIT ));
	}

	return( 0 );
}

/**
 * ofo_ledger_get_futur_debit:
 * @ledger:
 * @currency:
 *
 * Returns the debit balance of this ledger for
 * the currency specified from entries in the future, or zero if not
 * found.
 */
ofxAmount
ofo_ledger_get_futur_debit( const ofoLedger *ledger, const gchar *currency )
{
	GList *sdev;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), 0 );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, 0 );

	sdev = ledger_find_balance_by_code( ledger, currency );
	if( sdev ){
		return( ofa_box_get_amount( sdev, LED_FUT_DEBIT ));
	}

	return( 0 );
}

/**
 * ofo_ledger_get_futur_credit:
 * @ledger:
 * @currency:
 *
 * Returns the current credit balance of this ledger for
 * the currency specified from entries in the future, or zero if not
 * found.
 */
ofxAmount
ofo_ledger_get_futur_credit( const ofoLedger *ledger, const gchar *currency )
{
	GList *sdev;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), 0 );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, 0 );

	sdev = ledger_find_balance_by_code( ledger, currency );
	if( sdev ){
		return( ofa_box_get_amount( sdev, LED_FUT_CREDIT ));
	}

	return( 0 );
}

static GList *
ledger_find_balance_by_code( const ofoLedger *ledger, const gchar *currency )
{
	static const gchar *thisfn = "ofo_ledger_find_balance_by_code";
	ofoLedgerPrivate *priv;
	GList *it;
	GList *balance;
	const gchar *bal_code;

	priv = ofo_ledger_get_instance_private( ledger );

	for( it=priv->balances ; it ; it=it->next ){
		balance = ( GList * ) it->data;
		bal_code = ofa_box_get_string( balance, LED_CURRENCY );
		if( !g_utf8_collate( bal_code, currency )){
			return( balance );
		}
	}

	g_debug( "%s: ledger=%s, currency=%s not found",
			thisfn, ofo_ledger_get_mnemo( ledger ), currency );

	return( NULL );
}

static GList *
ledger_new_balance_with_code( ofoLedger *ledger, const gchar *currency )
{
	ofoLedgerPrivate *priv;
	GList *balance;

	balance = ledger_find_balance_by_code( ledger, currency );

	if( !balance ){
		balance = ofa_box_init_fields_list( st_balance_defs );
		ofa_box_set_string( balance, LED_CURRENCY, currency );
		ofa_box_set_amount( balance, LED_VAL_DEBIT, 0 );
		ofa_box_set_amount( balance, LED_VAL_CREDIT, 0 );
		ofa_box_set_amount( balance, LED_ROUGH_DEBIT, 0 );
		ofa_box_set_amount( balance, LED_ROUGH_CREDIT, 0 );
		ofa_box_set_amount( balance, LED_FUT_DEBIT, 0 );
		ofa_box_set_amount( balance, LED_FUT_CREDIT, 0 );

		priv = ofo_ledger_get_instance_private( ledger );
		priv->balances = g_list_prepend( priv->balances, balance );
	}

	return( balance );
}

/*
 * add debit/credit to rough balance for the currency, creating the
 * new record if needed
 */
static GList *
ledger_add_balance_rough( ofoLedger *ledger, const gchar *currency, ofxAmount debit, ofxAmount credit )
{
	return( ledger_add_to_balance( ledger, currency, LED_ROUGH_DEBIT, debit, LED_ROUGH_CREDIT, credit ));
}

/*
 * add debit/credit to validated balance for the currency, creating the
 * new record if needed
 */
static GList *
ledger_add_balance_validated( ofoLedger *ledger, const gchar *currency, ofxAmount debit, ofxAmount credit )
{
	return( ledger_add_to_balance( ledger, currency, LED_VAL_DEBIT, debit, LED_VAL_CREDIT, credit ));
}

/*
 * add debit/credit to future balance for the currency, creating the
 * new record if needed
 */
static GList *
ledger_add_balance_future( ofoLedger *ledger, const gchar *currency, ofxAmount debit, ofxAmount credit )
{
	return( ledger_add_to_balance( ledger, currency, LED_FUT_DEBIT, debit, LED_FUT_CREDIT, credit ));
}

static GList *
ledger_add_to_balance( ofoLedger *ledger, const gchar *currency, gint debit_id, ofxAmount debit, gint credit_id, ofxAmount credit )
{
	GList *balance;
	ofxAmount amount;

	g_return_val_if_fail(( debit && !credit ) || ( !debit && credit ), NULL );

	balance = ledger_new_balance_with_code( ledger, currency );
	if( debit ){
		amount = ofa_box_get_amount( balance, debit_id );
		ofa_box_set_amount( balance, debit_id, amount+debit );
	} else {
		amount = ofa_box_get_amount( balance, credit_id );
		ofa_box_set_amount( balance, credit_id, amount+credit );
	}

	return( balance );
}

/**
 * ofo_ledger_get_max_last_close:
 * @date: [out]: the date to be set
 * @dossier:
 *
 * Set the @date to the max of all closing dates for the ledgers.
 *
 * Return: this same @date.
 */
GDate *
ofo_ledger_get_max_last_close( GDate *date, ofaHub *hub )
{
	GSList *result, *icol;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	my_date_clear( date );

	if( ofa_idbconnect_query_ex(
			ofa_hub_get_connect( hub ),
			"SELECT MAX(LED_LAST_CLO) FROM OFA_T_LEDGERS", &result, TRUE )){

		icol = result ? result->data : NULL;
		my_date_set_from_sql( date, icol ? icol->data : NULL );
		ofa_idbconnect_free_results( result );
	}

	return( date );
}

/**
 * ofo_ledger_has_entries:
 */
gboolean
ofo_ledger_has_entries( const ofoLedger *ledger )
{
	ofaHub *hub;
	gboolean ok;
	const gchar *mnemo;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, FALSE );

	hub = ofo_base_get_hub( OFO_BASE( ledger ));
	mnemo = ofo_ledger_get_mnemo( ledger );
	ok = ofo_entry_use_ledger( hub, mnemo );

	return( ok );
}

/**
 * ofo_ledger_is_deletable:
 *
 * A ledger is considered to be deletable if no entry has been recorded
 * during the current exercice - This means that all its amounts must be
 * nuls for all currencies.
 *
 * There is no need to test for the last closing date as this is not
 * relevant here: even if set, they does not mean that there has been
 * any entries recorded on the ledger.
 *
 * More: a ledger should not be deleted while it is referenced by a
 * model or an entry or the dossier itself (or the dossier is an
 * archive).
 */
gboolean
ofo_ledger_is_deletable( const ofoLedger *ledger )
{
	ofaHub *hub;
	ofoDossier *dossier;
	gboolean deletable;
	const gchar *mnemo;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, FALSE );

	hub = ofo_base_get_hub( OFO_BASE( ledger ));
	dossier = ofa_hub_get_dossier( hub );
	mnemo = ofo_ledger_get_mnemo( ledger );

	deletable = !ofo_dossier_use_ledger( dossier, mnemo ) &&
			!ofo_entry_use_ledger( hub, mnemo ) &&
			!ofo_ope_template_use_ledger( hub, mnemo );

	deletable &= ofa_idbmodel_get_is_deletable( hub, OFO_BASE( ledger ));

	return( deletable );
}

/**
 * ofo_ledger_is_valid_data:
 *
 * Returns: %TRUE if the provided data makes the ofoLedger a valid
 * object.
 *
 * Note that this does NOT check for key duplicate.
 */
gboolean
ofo_ledger_is_valid_data( const gchar *mnemo, const gchar *label, gchar **msgerr )
{
	if( msgerr ){
		*msgerr = NULL;
	}
	if( !my_strlen( mnemo )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Empty mnemonic" ));
		}
		return( FALSE );
	}
	if( !my_strlen( label )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Empty label" ));
		}
		return( FALSE );
	}
	return( TRUE );
}

/**
 * ofo_ledger_set_mnemo:
 */
void
ofo_ledger_set_mnemo( ofoLedger *ledger, const gchar *mnemo )
{
	ofo_base_setter( LEDGER, ledger, string, LED_MNEMO, mnemo );
}

/**
 * ofo_ledger_set_label:
 */
void
ofo_ledger_set_label( ofoLedger *ledger, const gchar *label )
{
	ofo_base_setter( LEDGER, ledger, string, LED_LABEL, label );
}

/**
 * ofo_ledger_set_notes:
 */
void
ofo_ledger_set_notes( ofoLedger *ledger, const gchar *notes )
{
	ofo_base_setter( LEDGER, ledger, string, LED_NOTES, notes );
}

/*
 * ledger_set_upd_user:
 */
static void
ledger_set_upd_user( ofoLedger *ledger, const gchar *upd_user )
{
	ofo_base_setter( LEDGER, ledger, string, LED_UPD_USER, upd_user );
}

/*
 * ledger_set_upd_stamp:
 */
static void
ledger_set_upd_stamp( ofoLedger *ledger, const GTimeVal *upd_stamp )
{
	ofo_base_setter( LEDGER, ledger, timestamp, LED_UPD_STAMP, upd_stamp );
}

/*
 * ledger_set_last_clo:
 * @ledger:
 *
 * Set the closing date for the ledger.
 */
static void
ledger_set_last_clo( ofoLedger *ledger, const GDate *date )
{
	ofo_base_setter( LEDGER, ledger, date, LED_LAST_CLO, date );
}

/**
 * ofo_ledger_set_val_debit:
 * @ledger:
 * @amount:
 * @currency:
 *
 * Set the debit balance of this ledger from validated entries for
 * the currency specified.
 *
 * Creates an occurrence of the detail record if it didn't exist yet.
 */
void
ofo_ledger_set_val_debit( ofoLedger *ledger, ofxAmount amount, const gchar *currency )
{
	GList *balance;

	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));
	g_return_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run );

	balance = ledger_new_balance_with_code( ledger, currency );
	g_return_if_fail( balance );

	ofa_box_set_amount( balance, LED_VAL_DEBIT, amount );
}

/**
 * ofo_ledger_set_val_credit:
 * @ledger:
 * @amount:
 * @currency:
 *
 * Set the credit balance of this ledger from validated entries for
 * the currency specified.
 *
 * Creates an occurrence of the detail record if it didn't exist yet.
 */
void
ofo_ledger_set_val_credit( ofoLedger *ledger, ofxAmount amount, const gchar *currency )
{
	GList *balance;

	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));
	g_return_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run );

	balance = ledger_new_balance_with_code( ledger, currency );
	g_return_if_fail( balance );

	ofa_box_set_amount( balance, LED_VAL_CREDIT, amount );
}

/**
 * ofo_ledger_set_rough_debit:
 * @ledger:
 * @amount:
 * @currency:
 *
 * Set the current debit balance of this ledger for
 * the currency specified.
 *
 * Creates an occurrence of the detail record if it didn't exist yet.
 */
void
ofo_ledger_set_rough_debit( ofoLedger *ledger, ofxAmount amount, const gchar *currency )
{
	GList *balance;

	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));
	g_return_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run );

	balance = ledger_new_balance_with_code( ledger, currency );
	g_return_if_fail( balance );

	ofa_box_set_amount( balance, LED_ROUGH_DEBIT, amount );
}

/**
 * ofo_ledger_set_rough_credit:
 * @ledger:
 * @amount:
 * @currency:
 *
 * Set the current credit balance of this ledger for
 * the currency specified.
 *
 * Creates an occurrence of the detail record if it didn't exist yet.
 */
void
ofo_ledger_set_rough_credit( ofoLedger *ledger, ofxAmount amount, const gchar *currency )
{
	GList *balance;

	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));
	g_return_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run );

	balance = ledger_new_balance_with_code( ledger, currency );
	g_return_if_fail( balance );

	ofa_box_set_amount( balance, LED_ROUGH_CREDIT, amount );
}

/**
 * ofo_ledger_set_futur_debit:
 * @ledger:
 * @amount:
 * @currency:
 *
 * Set the debit balance of this ledger for
 * the currency specified from future entries.
 *
 * Creates an occurrence of the detail record if it didn't exist yet.
 */
void
ofo_ledger_set_futur_debit( ofoLedger *ledger, ofxAmount amount, const gchar *currency )
{
	GList *balance;

	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));
	g_return_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run );

	balance = ledger_new_balance_with_code( ledger, currency );
	g_return_if_fail( balance );

	ofa_box_set_amount( balance, LED_FUT_DEBIT, amount );
}

/**
 * ofo_ledger_set_futur_credit:
 * @ledger:
 * @amount:
 * @currency:
 *
 * Set the credit balance of this ledger for
 * the currency specified from future entries.
 *
 * Creates an occurrence of the detail record if it didn't exist yet.
 */
void
ofo_ledger_set_futur_credit( ofoLedger *ledger, ofxAmount amount, const gchar *currency )
{
	GList *balance;

	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));
	g_return_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run );

	balance = ledger_new_balance_with_code( ledger, currency );
	g_return_if_fail( balance );

	ofa_box_set_amount( balance, LED_FUT_CREDIT, amount );
}

/**
 * ofo_ledger_close:
 *
 * - all entries in rough status and whose effect date in less or equal
 *   to the closing date, and which are written in this ledger, are
 *   validated
 */
gboolean
ofo_ledger_close( ofoLedger *ledger, const GDate *closing )
{
	static const gchar *thisfn = "ofo_ledger_close";
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: ledger=%p, closing=%p", thisfn, ( void * ) ledger, ( void * ) closing );

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( closing && my_date_is_valid( closing ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( ledger ));

	if( ofo_entry_validate_by_ledger( hub, ofo_ledger_get_mnemo( ledger ), closing )){
		ledger_set_last_clo( ledger, closing );

		if( ofo_ledger_update( ledger, ofo_ledger_get_mnemo( ledger ))){
			g_signal_emit_by_name( hub, SIGNAL_HUB_UPDATED, ledger, NULL );
			ok = TRUE;
		}
	}

	return( ok );
}

/**
 * ofo_ledger_insert:
 *
 * Only insert here a new ledger, so only the main properties
 */
gboolean
ofo_ledger_insert( ofoLedger *ledger, ofaHub *hub )
{
	static const gchar *thisfn = "ofo_ledger_insert";
	gboolean ok;

	g_debug( "%s: ledger=%p, hub=%p",
			thisfn, ( void * ) ledger, ( void * ) hub );

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, FALSE );

	ok = FALSE;

	if( ledger_do_insert( ledger, ofa_hub_get_connect( hub ))){
		ofo_base_set_hub( OFO_BASE( ledger ), hub );
		my_icollector_collection_add_object(
				ofa_hub_get_collector( hub ),
				MY_ICOLLECTIONABLE( ledger ), ( GCompareFunc ) ledger_cmp_by_ptr, hub );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_NEW, ledger );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
ledger_do_insert( ofoLedger *ledger, const ofaIDBConnect *connect )
{
	return( ledger_insert_main( ledger, connect ));
}

static gboolean
ledger_insert_main( ofoLedger *ledger, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *label, *notes, *userid;
	gboolean ok;
	gchar *stamp_str;
	GTimeVal stamp;

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_ledger_get_label( ledger ));
	notes = my_utils_quote_sql( ofo_ledger_get_notes( ledger ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_LEDGERS" );

	g_string_append_printf( query,
			"	(LED_MNEMO,LED_LABEL,LED_NOTES,"
			"	LED_UPD_USER, LED_UPD_STAMP) VALUES ('%s','%s',",
			ofo_ledger_get_mnemo( ledger ),
			label );

	if( my_strlen( notes )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
			"'%s','%s')",
			userid, stamp_str );

	if( ofa_idbconnect_query( connect, query->str, TRUE )){
		ledger_set_upd_user( ledger, userid );
		ledger_set_upd_stamp( ledger, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );
	g_free( userid );

	return( ok );
}

/**
 * ofo_ledger_update:
 *
 * We only update here the user properties, so do not care with the
 * details of balances per currency.
 */
gboolean
ofo_ledger_update( ofoLedger *ledger, const gchar *prev_mnemo )
{
	static const gchar *thisfn = "ofo_ledger_update";
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: ledger=%p, prev_mnemo=%s",
			thisfn, ( void * ) ledger, prev_mnemo );

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( my_strlen( prev_mnemo ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( ledger ));

	if( ledger_do_update( ledger, prev_mnemo, ofa_hub_get_connect( hub ))){
		my_icollector_collection_sort(
				ofa_hub_get_collector( hub ),
				OFO_TYPE_LEDGER, ( GCompareFunc ) ledger_cmp_by_ptr );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_UPDATED, ledger, prev_mnemo );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
ledger_do_update( ofoLedger *ledger, const gchar *prev_mnemo, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *label, *notes, *userid;
	gboolean ok;
	gchar *stamp_str, *sdate;
	GTimeVal stamp;
	const GDate *last_clo;

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_ledger_get_label( ledger ));
	notes = my_utils_quote_sql( ofo_ledger_get_notes( ledger ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_LEDGERS SET " );

	g_string_append_printf( query, "LED_MNEMO='%s',", ofo_ledger_get_mnemo( ledger ));
	g_string_append_printf( query, "LED_LABEL='%s',", label );

	if( my_strlen( notes )){
		g_string_append_printf( query, "LED_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "LED_NOTES=NULL," );
	}

	last_clo = ofo_ledger_get_last_close( ledger );
	if( my_date_is_valid( last_clo )){
		sdate = my_date_to_str( last_clo, MY_DATE_SQL );
		g_string_append_printf( query, "LED_LAST_CLO='%s',", sdate );
		g_free( sdate );
	} else {
		query = g_string_append( query, "LED_LAST_CLO=NULL," );
	}

	g_string_append_printf( query,
			"	LED_UPD_USER='%s',LED_UPD_STAMP='%s'"
			"	WHERE LED_MNEMO='%s'", userid, stamp_str, prev_mnemo );

	if( ofa_idbconnect_query( connect, query->str, TRUE )){
		ledger_set_upd_user( ledger, userid );
		ledger_set_upd_stamp( ledger, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );
	g_free( userid );

	if( ok && g_utf8_collate( prev_mnemo, ofo_ledger_get_mnemo( ledger ))){
		query = g_string_new( "UPDATE OFA_T_LEDGERS_CUR SET " );
		g_string_append_printf( query,
				"LED_MNEMO='%s' WHERE LED_MNEMO='%s'", ofo_ledger_get_mnemo( ledger ), prev_mnemo );
		ok &= ofa_idbconnect_query( connect, query->str, TRUE );
		g_string_free( query, TRUE );
	}

	return( ok );
}

/**
 * ofo_ledger_update_balance:
 */
gboolean
ofo_ledger_update_balance( ofoLedger *ledger, const gchar *currency )
{
	static const gchar *thisfn = "ofo_ledger_update_balance";
	GList *balance;
	gboolean ok;
	ofaHub *hub;

	g_debug( "%s: ledger=%p, currency=%s", thisfn, ( void * ) ledger, currency );

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( my_strlen( currency ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( ledger ));
	balance = ledger_find_balance_by_code( ledger, currency );
	g_return_val_if_fail( balance, FALSE );

	if( ledger_do_update_balance( ledger, balance, hub )){
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_UPDATED, ledger, NULL );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
ledger_do_update_balance( const ofoLedger *ledger, GList *balance, ofaHub *hub )
{
	gchar *query;
	const gchar *currency;
	gchar *sval_debit, *sval_credit, *srough_debit, *srough_credit, *sfutur_debit, *sfutur_credit;
	gboolean ok;
	ofoCurrency *cur_obj;
	const ofaIDBConnect *connect;

	connect = ofa_hub_get_connect( hub );

	currency = ofa_box_get_string( balance, LED_CURRENCY );
	cur_obj = ofo_currency_get_by_code( hub, currency );
	g_return_val_if_fail( cur_obj && OFO_IS_CURRENCY( cur_obj ), FALSE );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_LEDGERS_CUR "
			"	WHERE LED_MNEMO='%s' AND LED_CUR_CODE='%s'",
					ofo_ledger_get_mnemo( ledger ),
					currency );

	ofa_idbconnect_query( connect, query, FALSE );
	g_free( query );

	sval_debit = ofa_amount_to_sql( ofo_ledger_get_val_debit( ledger, currency ), cur_obj );
	sval_credit = ofa_amount_to_sql( ofo_ledger_get_val_credit( ledger, currency ), cur_obj );
	srough_debit = ofa_amount_to_sql( ofo_ledger_get_rough_debit( ledger, currency ), cur_obj );
	srough_credit = ofa_amount_to_sql( ofo_ledger_get_rough_credit( ledger, currency ), cur_obj );
	sfutur_debit = ofa_amount_to_sql( ofo_ledger_get_futur_debit( ledger, currency ), cur_obj );
	sfutur_credit = ofa_amount_to_sql( ofo_ledger_get_futur_credit( ledger, currency ), cur_obj );

	query = g_strdup_printf(
					"INSERT INTO OFA_T_LEDGERS_CUR "
					"	(LED_MNEMO,LED_CUR_CODE,"
					"	LED_CUR_VAL_DEBIT,LED_CUR_VAL_CREDIT,"
					"	LED_CUR_ROUGH_DEBIT,LED_CUR_ROUGH_CREDIT,"
					"	LED_CUR_FUT_DEBIT,LED_CUR_FUT_CREDIT) VALUES "
					"	('%s','%s',%s,%s,%s,%s,%s,%s)",
							ofo_ledger_get_mnemo( ledger ),
							currency,
							sval_debit,
							sval_credit,
							srough_debit,
							srough_credit,
							sfutur_debit,
							sfutur_credit );

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( sval_debit );
	g_free( sval_credit );
	g_free( srough_debit );
	g_free( srough_credit );
	g_free( sfutur_debit );
	g_free( sfutur_credit );
	g_free( query );

	return( ok );
}

/**
 * ofo_ledger_delete:
 *
 * Take care of deleting both main and detail records.
 */
gboolean
ofo_ledger_delete( ofoLedger *ledger )
{
	static const gchar *thisfn = "ofo_ledger_delete";
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: ledger=%p", thisfn, ( void * ) ledger );

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( ofo_ledger_is_deletable( ledger ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( ledger ));

	if( ledger_do_delete( ledger, ofa_hub_get_connect( hub ))){
		g_object_ref( ledger );
		my_icollector_collection_remove_object( ofa_hub_get_collector( hub ), MY_ICOLLECTIONABLE( ledger ));
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_DELETED, ledger );
		g_object_unref( ledger );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
ledger_do_delete( ofoLedger *ledger, const ofaIDBConnect *connect )
{
	gchar *query;
	gboolean ok;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_LEDGERS WHERE LED_MNEMO='%s'",
					ofo_ledger_get_mnemo( ledger ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_LEDGERS_CUR WHERE LED_MNEMO='%s'",
					ofo_ledger_get_mnemo( ledger ));

	ok &= ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

static gint
ledger_cmp_by_mnemo( const ofoLedger *a, const gchar *mnemo )
{
	return( g_utf8_collate( ofo_ledger_get_mnemo( a ), mnemo ));
}

static gint
ledger_cmp_by_ptr( const ofoLedger *a, const ofoLedger *b )
{
	return( ledger_cmp_by_mnemo( a, ofo_ledger_get_mnemo( b )));
}

/*
 * myICollectionable interface management
 */
static void
icollectionable_iface_init( myICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_ledger_icollectionable_iface_init";

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
	ofoLedgerPrivate *priv;
	GList *dataset, *it;
	ofoLedger *ledger;
	gchar *from;

	g_return_val_if_fail( user_data && OFA_IS_HUB( user_data ), NULL );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					"OFA_T_LEDGERS ORDER BY LED_MNEMO ASC",
					OFO_TYPE_LEDGER,
					OFA_HUB( user_data ));

	for( it=dataset ; it ; it=it->next ){
		ledger = OFO_LEDGER( it->data );
		priv = ofo_ledger_get_instance_private( ledger );
		from = g_strdup_printf(
					"OFA_T_LEDGERS_CUR WHERE LED_MNEMO='%s'", ofo_ledger_get_mnemo( ledger ));
		priv->balances = ofo_base_load_rows( st_balance_defs, ofa_hub_get_connect( OFA_HUB( user_data )), from );
		g_free( from );
	}

	return( dataset );
}

/*
 * ofaIExportable interface management
 */
static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofo_ledger_iexportable_iface_init";

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
	return( g_strdup( _( "Reference : _ledgers" )));
}

/*
 * iexportable_export:
 *
 * Exports the classes line by line.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const ofaStreamFormat *settings, ofaHub *hub )
{
	ofoLedgerPrivate *priv;
	GList *dataset, *it, *ic, *bal;
	gchar *str, *str2;
	gboolean ok, with_headers;
	gulong count;
	gchar field_sep;
	ofoLedger *ledger;
	const gchar *cur_code;
	ofoCurrency *currency;

	dataset = ofo_ledger_get_dataset( hub );

	with_headers = ofa_stream_format_get_with_headers( settings );
	field_sep = ofa_stream_format_get_field_sep( settings );

	count = ( gulong ) g_list_length( dataset );
	if( with_headers ){
		count += 2;
	}
	for( it=dataset ; it ; it=it->next ){
		ledger = OFO_LEDGER( it->data );
		priv = ofo_ledger_get_instance_private( ledger );
		count += g_list_length( priv->balances );
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = ofa_box_csv_get_header( st_boxed_defs, settings );
		str2 = g_strdup_printf( "1%c%s", field_sep, str );
		ok = ofa_iexportable_set_line( exportable, str2 );
		g_free( str2 );
		g_free( str );
		if( !ok ){
			return( FALSE );
		}

		str = ofa_box_csv_get_header( st_balance_defs, settings );
		str2 = g_strdup_printf( "2%c%s", field_sep, str );
		ok = ofa_iexportable_set_line( exportable, str2 );
		g_free( str2 );
		g_free( str );
		if( !ok ){
			return( FALSE );
		}
	}

	for( it=dataset ; it ; it=it->next ){
		str = ofa_box_csv_get_line( OFO_BASE( it->data )->prot->fields, settings );
		str2 = g_strdup_printf( "1%c%s", field_sep, str );
		ok = ofa_iexportable_set_line( exportable, str2 );
		g_free( str2 );
		g_free( str );
		if( !ok ){
			return( FALSE );
		}

		ledger = OFO_LEDGER( it->data );
		priv = ofo_ledger_get_instance_private( ledger );

		for( ic=priv->balances ; ic ; ic=ic->next ){
			bal = ( GList * ) ic->data;
			cur_code = ofa_box_get_string( bal, LED_CURRENCY );
			g_return_val_if_fail( cur_code && my_strlen( cur_code ), FALSE );
			currency = ofo_currency_get_by_code( hub, cur_code );
			g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), FALSE );
			str = ofa_box_csv_get_line_ex( bal, settings, ( CSVExportFunc ) export_cb, currency );
			str2 = g_strdup_printf( "2%c%s", field_sep, str );
			ok = ofa_iexportable_set_line( exportable, str2 );
			g_free( str2 );
			g_free( str );
			if( !ok ){
				return( FALSE );
			}
		}
	}

	return( TRUE );
}

/*
 * a callback to adjust the decimal digits count to the precision of the
 * currency
 */
static gchar *
export_cb( const ofsBoxData *box_data, const ofaStreamFormat *format, const gchar *text, ofoCurrency *currency )
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
	static const gchar *thisfn = "ofo_ledger_iimportable_iface_init";

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
 * ofo_ledger_iimportable_import:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - ledger mnemo
 * - label
 * - notes (opt)
 *
 * Replace the main table with the provided datas, initializing the
 * balances to zero.
 *
 * In order to be able to import a previously exported file:
 * - we accept that the first field of the first line be "1" or "2"
 * - we silently ignore other lines.
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
	gchar *bck_table, *bck_det_table;

	dataset = iimportable_import_parse( importer, parms, lines );

	if( parms->parse_errs == 0 && parms->parsed_count > 0 ){
		bck_table = ofa_idbconnect_table_backup( ofa_hub_get_connect( parms->hub ), "OFA_T_LEDGERS" );
		bck_det_table = ofa_idbconnect_table_backup( ofa_hub_get_connect( parms->hub ), "OFA_T_LEDGERS_CUR" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs == 0 ){
			my_icollector_collection_free( ofa_hub_get_collector( parms->hub ), OFO_TYPE_LEDGER );
			g_signal_emit_by_name( G_OBJECT( parms->hub ), SIGNAL_HUB_RELOAD, OFO_TYPE_LEDGER );

		} else {
			ofa_idbconnect_table_restore( ofa_hub_get_connect( parms->hub ), bck_table, "OFA_T_LEDGERS" );
			ofa_idbconnect_table_restore( ofa_hub_get_connect( parms->hub ), bck_det_table, "OFA_T_LEDGERS_CUR" );
		}

		g_free( bck_table );
		g_free( bck_det_table );
	}

	if( dataset ){
		ofo_ledger_free_dataset( dataset );
	}

	return( parms->parse_errs+parms->insert_errs );
}

/*
 * parse to a dataset
 */
static GList *
iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines )
{
	GList *dataset;
	GSList *itl, *fields, *itf;
	const gchar *cstr;
	guint numline, total;
	gchar *splitted, *str;
	gboolean have_prefix;
	ofoLedger *ledger;

	numline = 0;
	dataset = NULL;
	total = g_slist_length( lines );
	have_prefix = FALSE;

	ofa_iimporter_progress_start( importer, parms );

	for( itl=lines ; itl ; itl=itl->next ){

		if( parms->stop && parms->parse_errs > 0 ){
			break;
		}

		numline += 1;
		fields = ( GSList * ) itl->data;
		ledger = ofo_ledger_new();

		/* ledger mnemo or "1" */
		itf = fields;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( numline == 1 ){
			have_prefix = ( my_strlen( cstr ) &&
					( !my_collate( cstr, "1" ) || !my_collate( cstr, "2" )));
		}
		if( have_prefix ){
			if( my_collate( cstr, "1" )){
				str = g_strdup_printf( _( "ignoring line with prefix=%s" ), cstr );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				total -= 1;
				g_free( str );
				continue;
			}
			itf = itf ? itf->next : NULL;
			cstr = itf ? ( const gchar * ) itf->data : NULL;
		}
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty ledger mnemo" ));
			parms->parse_errs += 1;
			continue;
		}
		ofo_ledger_set_mnemo( ledger, cstr );

		/* ledger label */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty ledger label" ));
			parms->parse_errs += 1;
			continue;
		}
		ofo_ledger_set_label( ledger, cstr );

		/* notes
		 * we are tolerant on the last field... */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		splitted = my_utils_import_multi_lines( cstr );
		ofo_ledger_set_notes( ledger, splitted );
		g_free( splitted );

		dataset = g_list_prepend( dataset, ledger );
		parms->parsed_count += 1;
		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->parsed_count, ( gulong ) total );
	}

	return( dataset );
}

static void
iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset )
{
	GList *it;
	const ofaIDBConnect *connect;
	gboolean insert;
	guint total;
	gchar *str;
	ofoLedger *ledger;
	const gchar *led_id;

	total = g_list_length( dataset );
	connect = ofa_hub_get_connect( parms->hub );
	ofa_iimporter_progress_start( importer, parms );

	if( parms->empty && total > 0 ){
		ledger_drop_content( connect );
	}

	for( it=dataset ; it ; it=it->next ){

		if( parms->stop && parms->insert_errs > 0 ){
			break;
		}

		str = NULL;
		insert = TRUE;
		ledger = OFO_LEDGER( it->data );

		if( ledger_get_exists( ledger, connect )){
			parms->duplicate_count += 1;
			led_id = ofo_ledger_get_mnemo( ledger );

			switch( parms->mode ){
				case OFA_IDUPLICATE_REPLACE:
					str = g_strdup_printf( _( "%s: duplicate ledger, replacing previous one" ), led_id );
					ledger_do_delete( ledger, connect );
					break;
				case OFA_IDUPLICATE_IGNORE:
					str = g_strdup_printf( _( "%s: duplicate ledger, ignored (skipped)" ), led_id );
					insert = FALSE;
					total -= 1;
					break;
				case OFA_IDUPLICATE_ABORT:
					str = g_strdup_printf( _( "%s: erroneous duplicate ledger" ), led_id );
					insert = FALSE;
					total -= 1;
					parms->insert_errs += 1;
					break;
			}

			ofa_iimporter_progress_text( importer, parms, str );
			g_free( str );
		}

		if( insert ){
			if( ledger_do_insert( ledger, connect )){
				parms->inserted_count += 1;
			} else {
				parms->insert_errs += 1;
			}
		}

		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->inserted_count, ( gulong ) total );
	}
}

static gboolean
ledger_get_exists( const ofoLedger *ledger, const ofaIDBConnect *connect )
{
	const gchar *ledger_id;
	gint count;
	gchar *str;

	count = 0;
	ledger_id = ofo_ledger_get_mnemo( ledger );
	str = g_strdup_printf( "SELECT COUNT(*) FROM OFA_T_LEDGERS WHERE LED_MNEMO='%s'", ledger_id );
	ofa_idbconnect_query_int( connect, str, &count, FALSE );

	return( count > 0 );
}

static gboolean
ledger_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM OFA_T_LEDGERS", TRUE ) &&
			ofa_idbconnect_query( connect, "DELETE FROM OFA_T_LEDGERS_CUR", TRUE ));
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

	g_signal_connect( hub, SIGNAL_HUB_NEW, G_CALLBACK( on_hub_new_object ), NULL );
	g_signal_connect( hub, SIGNAL_HUB_UPDATED, G_CALLBACK( on_hub_updated_object ), NULL );
	g_signal_connect( hub, SIGNAL_HUB_STATUS_CHANGE, G_CALLBACK( on_hub_entry_status_change ), NULL );
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
on_hub_new_object( ofaHub *hub, ofoBase *object, void *empty )
{
	static const gchar *thisfn = "ofo_ledger_on_hub_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), empty=%p",
			thisfn, ( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	if( OFO_IS_ENTRY( object )){
		on_new_ledger_entry( hub, OFO_ENTRY( object ));
	}
}

/*
 * we are recording a new entry (so necessarily on the current exercice)
 * thus update the balances
 */
static void
on_new_ledger_entry( ofaHub *hub, ofoEntry *entry )
{
	ofaEntryStatus status;
	const gchar *mnemo, *currency;
	ofoLedger *ledger;
	GList *balance;

	/* the only case where an entry is created with a 'past' status
	 *  is an imported entry in the past (before the beginning of the
	 *  exercice) - in this case, the 'new_object' message should not be
	 *  sent
	 * if not in the past, only allowed status are 'rough' or 'future' */
	status = ofo_entry_get_status( entry );
	g_return_if_fail( status != ENT_STATUS_PAST );
	g_return_if_fail( status == ENT_STATUS_ROUGH || status == ENT_STATUS_FUTURE );

	mnemo = ofo_entry_get_ledger( entry );
	ledger = ofo_ledger_get_by_mnemo( hub, mnemo );
	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));

	currency = ofo_entry_get_currency( entry );

	switch( status ){
		case ENT_STATUS_ROUGH:
			balance = ledger_add_balance_rough(
							ledger, currency, ofo_entry_get_debit( entry ), ofo_entry_get_credit( entry ));
			break;

		case ENT_STATUS_FUTURE:
			balance = ledger_add_balance_future(
							ledger, currency, ofo_entry_get_debit( entry ), ofo_entry_get_credit( entry ));
			break;

		default:
			g_return_if_reached();
	}

	if( ledger_do_update_balance( ledger, balance, hub )){
		g_signal_emit_by_name( hub, SIGNAL_HUB_UPDATED, ledger, NULL );
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, void *empty )
{
	static const gchar *thisfn = "ofo_ledger_on_hub_updated_object";
	const gchar *code;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, empty=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) empty );

	if( OFO_IS_CURRENCY( object )){
		if( my_strlen( prev_id )){
			code = ofo_currency_get_code( OFO_CURRENCY( object ));
			if( g_utf8_collate( code, prev_id )){
				on_updated_object_currency_code( hub, prev_id, code );
			}
		}
	}
}

/*
 * a currency iso code has been modified (this should be very rare)
 * so update our ledger records
 */
static void
on_updated_object_currency_code( ofaHub *hub, const gchar *prev_id, const gchar *code )
{
	gchar *query;

	query = g_strdup_printf(
					"UPDATE OFA_T_LEDGERS_CUR "
					"	SET LED_CUR_CODE='%s' WHERE LED_CUR_CODE='%s'", code, prev_id );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );

	g_free( query );

	my_icollector_collection_free( ofa_hub_get_collector( hub ), OFO_TYPE_LEDGER );

	g_signal_emit_by_name( hub, SIGNAL_HUB_RELOAD, OFO_TYPE_LEDGER );
}

/*
 * SIGNAL_HUB_STATUS_CHANGE signal handler
 */
static void
on_hub_entry_status_change( ofaHub *hub, ofoEntry *entry, ofaEntryStatus prev_status, ofaEntryStatus new_status, void *empty )
{
	static const gchar *thisfn = "ofo_ledger_on_hub_entry_status_change";
	const gchar *currency;
	ofoLedger *ledger;
	ofxAmount debit, credit;
	GList *balance;

	g_debug( "%s: hub=%p, entry=%p, prev_status=%u, new_status=%u, empty=%p",
			thisfn, ( void * ) hub, ( void * ) entry, prev_status, new_status, ( void * ) empty );

	ledger = ofo_ledger_get_by_mnemo( hub, ofo_entry_get_ledger( entry ));
	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));

	currency = ofo_entry_get_currency( entry );

	debit = ofo_entry_get_debit( entry );
	credit = ofo_entry_get_credit( entry );

	switch( prev_status ){
		case ENT_STATUS_ROUGH:
			ledger_add_balance_rough( ledger, currency, -debit, -credit );
			break;
		case ENT_STATUS_VALIDATED:
			ledger_add_balance_validated( ledger, currency, -debit, -credit );
			break;
		case ENT_STATUS_FUTURE:
			ledger_add_balance_future( ledger, currency, -debit, -credit );
			break;
		default:
			break;
	}

	switch( new_status ){
		case ENT_STATUS_ROUGH:
			ledger_add_balance_rough( ledger, currency, debit, credit );
			break;
		case ENT_STATUS_VALIDATED:
			ledger_add_balance_validated( ledger, currency, debit, credit );
			break;
		case ENT_STATUS_FUTURE:
			ledger_add_balance_future( ledger, currency, debit, credit );
			break;
		default:
			break;
	}

	balance = ledger_find_balance_by_code( ledger, currency );

	if( ledger_do_update_balance( ledger, balance, hub )){
		g_signal_emit_by_name( hub, SIGNAL_HUB_UPDATED, ledger, NULL );
	}
}
