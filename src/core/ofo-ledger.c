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
#include <string.h>

#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-icollectionable.h"
#include "my/my-icollector.h"
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-idoc.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iexporter.h"
#include "api/ofa-igetter.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-isignalable.h"
#include "api/ofa-isignaler.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofs-ledger-balance.h"

/* priv instance data
 */
enum {
	LED_MNEMO = 1,
	LED_CRE_USER,
	LED_CRE_STAMP,
	LED_LABEL,
	LED_NOTES,
	LED_UPD_USER,
	LED_UPD_STAMP,
	LED_LAST_CLO,
	LED_CURRENCY,
	LED_CV_DEBIT,
	LED_CV_CREDIT,
	LED_CR_DEBIT,
	LED_CR_CREDIT,
	LED_FR_DEBIT,
	LED_FR_CREDIT,
	LED_FV_DEBIT,
	LED_FV_CREDIT,
	LED_ARC_CURRENCY,
	LED_ARC_DATE,
	LED_ARC_DEBIT,
	LED_ARC_CREDIT,
	LED_DOC_ID,
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order.
 * So:
 * 1/ the class default import should expect these fields in this same
 *    order.
 * 2/ new datas should be added to the end of the list.
 * 3/ a removed column should be replaced by an empty one to stay
 *    compatible with the class default import.
 */
static const ofsBoxDef st_boxed_defs[] = {
		{ OFA_BOX_CSV( LED_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* amount, counter: export zero as empty */
		{ OFA_BOX_CSV( LED_CRE_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( LED_CRE_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				FALSE },
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
		{ LED_CR_DEBIT,
				"LED_CUR_CR_DEBIT",
				"LedCurCurrentRoughDebit",
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ LED_CR_CREDIT,
				"LED_CUR_CR_CREDIT",
				"LedCurCurrentRoughCredit",
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ LED_CV_DEBIT,
				"LED_CUR_CV_DEBIT",
				"LedCurCurrentValDebit",
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ LED_CV_CREDIT,
				"LED_CUR_CV_CREDIT",
				"LedCurCurrentValCredit",
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ LED_FR_DEBIT,
				"LED_CUR_FR_DEBIT",
				"LedCurFutureRoughDebit",
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ LED_FR_CREDIT,
				"LED_CUR_FR_CREDIT",
				"LedCurFutureRoughCredit",
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ LED_FV_DEBIT,
				"LED_CUR_FV_DEBIT",
				"LedCurFutureValDebit",
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ LED_FV_CREDIT,
				"LED_CUR_FV_CREDIT",
				"LedCurFutureValCredit",
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ 0 }
};

static const ofsBoxDef st_archive_defs[] = {
		{ OFA_BOX_CSV( LED_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( LED_ARC_CURRENCY ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( LED_ARC_DATE ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( LED_ARC_DEBIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( LED_ARC_CREDIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ 0 }
};

static const ofsBoxDef st_doc_defs[] = {
		{ OFA_BOX_CSV( LED_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( LED_DOC_ID ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ 0 }
};

#define LEDGER_TABLES_COUNT             4
#define LEDGER_EXPORT_VERSION           2

typedef struct {
	GList *balances;					/* the balances per currency as a GList of GList fields */
	GList *archives;					/* archived balances of the ledger */
	GList *docs;
}
	ofoLedgerPrivate;

static ofoLedger *ledger_find_by_mnemo( GList *set, const gchar *mnemo );
static void       ledger_set_cre_user( ofoLedger *ledger, const gchar *user );
static void       ledger_set_cre_stamp( ofoLedger *ledger, const GTimeVal *stamp );
static void       ledger_set_upd_user( ofoLedger *ledger, const gchar *user );
static void       ledger_set_upd_stamp( ofoLedger *ledger, const GTimeVal *stamp );
static void       ledger_set_last_clo( ofoLedger *ledger, const GDate *date );
static gint       cmp_currencies( const gchar *a_currency, const gchar *b_currency );
static GList     *ledger_find_balance_by_code( ofoLedger *ledger, const gchar *currency );
static GList     *ledger_new_balance_with_code( ofoLedger *ledger, const gchar *currency );
static GList     *ledger_add_balance_current_rough( ofoLedger *ledger, const gchar *currency, ofxAmount debit, ofxAmount credit );
static GList     *ledger_add_balance_current_val( ofoLedger *ledger, const gchar *currency, ofxAmount debit, ofxAmount credit );
static GList     *ledger_add_balance_futur_rough( ofoLedger *ledger, const gchar *currency, ofxAmount debit, ofxAmount credit );
static GList     *ledger_add_balance_futur_val( ofoLedger *ledger, const gchar *currency, ofxAmount debit, ofxAmount credit );
static GList     *ledger_add_to_balance( ofoLedger *ledger, const gchar *currency, gint debit_id, ofxAmount debit, gint credit_id, ofxAmount credit );
static GList     *get_orphans( ofaIGetter *getter, const gchar *table );
static gboolean   do_add_archive_dbms( ofoLedger *ledger, const gchar *currency, const GDate *date, ofxAmount debit, ofxAmount credit );
static void       do_add_archive_list( ofoLedger *ledger, const gchar *currency, const GDate *date, ofxAmount debit, ofxAmount credit );
static gint       get_archive_index( ofoLedger *ledger, const gchar *currency, const GDate *date );
static void       get_last_archive_date( ofoLedger *ledger, GDate *date );
static gboolean   ledger_do_insert( ofoLedger *ledger, const ofaIDBConnect *connect );
static gboolean   ledger_insert_main( ofoLedger *ledger, const ofaIDBConnect *connect );
static gboolean   ledger_do_update( ofoLedger *ledger, const gchar *prev_mnemo, const ofaIDBConnect *connect );
static gboolean   ledger_do_update_balance( ofoLedger *ledger, GList *balance, ofaIGetter *getter );
static gboolean   ledger_do_delete( ofoLedger *ledger, const ofaIDBConnect *connect );
static gint       ledger_cmp_by_mnemo( const ofoLedger *a, const gchar *mnemo );
static void       icollectionable_iface_init( myICollectionableInterface *iface );
static guint      icollectionable_get_interface_version( void );
static GList     *icollectionable_load_collection( void *user_data );
static void       idoc_iface_init( ofaIDocInterface *iface );
static guint      idoc_get_interface_version( void );
static void       iexportable_iface_init( ofaIExportableInterface *iface );
static guint      iexportable_get_interface_version( void );
static gchar     *iexportable_get_label( const ofaIExportable *instance );
static gboolean   iexportable_get_published( const ofaIExportable *instance );
static gboolean   iexportable_export( ofaIExportable *exportable, const gchar *format_id );
static gboolean   iexportable_export_default( ofaIExportable *exportable );
static void       iimportable_iface_init( ofaIImportableInterface *iface );
static guint      iimportable_get_interface_version( void );
static gchar     *iimportable_get_label( const ofaIImportable *instance );
static guint      iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static GList     *iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static void       iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean   ledger_get_exists( const ofoLedger *ledger, const ofaIDBConnect *connect );
static gboolean   ledger_drop_content( const ofaIDBConnect *connect );
static void       isignalable_iface_init( ofaISignalableInterface *iface );
static void       isignalable_connect_to( ofaISignaler *signaler );
static gboolean   signaler_on_deletable_object( ofaISignaler *signaler, ofoBase *object, void *empty );
static gboolean   signaler_is_deletable_currency( ofaISignaler *signaler, ofoCurrency *currency );
static void       signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, void *empty );
static void       signaler_on_new_ledger_entry( ofaISignaler *signaler, ofoEntry *entry );
static void       signaler_on_entry_period_status_changed( ofaISignaler *signaler, ofoEntry *entry, ofeEntryPeriod prev_period, ofeEntryStatus prev_status, ofeEntryPeriod new_period, ofeEntryStatus new_status, void *empty );
static void       signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, void *empty );
static void       signaler_on_updated_currency_code( ofaISignaler *signaler, const gchar *prev_id, const gchar *code );

G_DEFINE_TYPE_EXTENDED( ofoLedger, ofo_ledger, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoLedger )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDOC, idoc_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNALABLE, isignalable_iface_init ))

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
	ofoLedgerPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofo_ledger_get_instance_private( self );

	priv->balances = NULL;
	priv->archives = NULL;
	priv->docs = NULL;
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
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the full #ofoLedger dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_ledger_get_dataset( ofaIGetter *getter )
{
	myICollector *collector;
	GList *collection;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );
	collection = my_icollector_collection_get( collector, OFO_TYPE_LEDGER, getter );

	return( collection );
}

/**
 * ofo_ledger_get_by_mnemo:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the searched ledger, or %NULL.
 *
 * The returned object is owned by the #ofoLedger class, and should
 * not be unreffed by the caller.
 */
ofoLedger *
ofo_ledger_get_by_mnemo( ofaIGetter *getter, const gchar *mnemo )
{
	GList *dataset;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( mnemo ), NULL );

	dataset = ofo_ledger_get_dataset( getter );

	return( ledger_find_by_mnemo( dataset, mnemo ));
}

static ofoLedger *
ledger_find_by_mnemo( GList *set, const gchar *mnemo )
{
	GList *found;

	found = g_list_find_custom( set, mnemo, ( GCompareFunc ) ledger_cmp_by_mnemo );

	if( found ){
		return( OFO_LEDGER( found->data ));
	}

	return( NULL );
}

/**
 * ofo_ledger_new:
 * @getter: a #ofaIGetter instance.
 */
ofoLedger *
ofo_ledger_new( ofaIGetter *getter )
{
	ofoLedger *ledger;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	ledger = g_object_new( OFO_TYPE_LEDGER, "ofo-base-getter", getter, NULL );
	OFO_BASE( ledger )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( ledger );
}

/**
 * ofo_ledger_get_cre_user:
 */
const gchar *
ofo_ledger_get_cre_user( const ofoLedger *ledger )
{
	ofo_base_getter( LEDGER, ledger, string, NULL, LED_CRE_USER );
}

/**
 * ofo_ledger_get_cre_stamp:
 */
const GTimeVal *
ofo_ledger_get_cre_stamp( const ofoLedger *ledger )
{
	ofo_base_getter( LEDGER, ledger, timestamp, NULL, LED_CRE_STAMP );
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
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;
	GSList *result, *icol;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), NULL );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, NULL );

	getter = ofo_base_get_getter( OFO_BASE( ledger ));
	hub = ofa_igetter_get_hub( getter );

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
 * ofo_ledger_get_max_last_close:
 * @getter: a #ofaIGetter instance.
 * @date: [out]: the date to be set
 *
 * Set the @date to the max of all closing dates for the ledgers.
 *
 * Return: this same @date.
 */
GDate *
ofo_ledger_get_max_last_close( ofaIGetter *getter, GDate *date )
{
	GSList *result, *icol;
	ofaHub *hub;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( date, NULL );

	my_date_clear( date );
	hub = ofa_igetter_get_hub( getter );

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
	ofaIGetter *getter;
	gboolean ok;
	const gchar *mnemo;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, FALSE );

	getter = ofo_base_get_getter( OFO_BASE( ledger ));
	mnemo = ofo_ledger_get_mnemo( ledger );
	ok = ofo_entry_use_ledger( getter, mnemo );

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
	ofaIGetter *getter;
	ofaISignaler *signaler;
	gboolean deletable;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, FALSE );

	deletable = TRUE;
	getter = ofo_base_get_getter( OFO_BASE( ledger ));
	signaler = ofa_igetter_get_signaler( getter );

	if( deletable ){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_IS_DELETABLE, ledger, &deletable );
	}

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
			*msgerr = g_strdup( _( "Mnemonic is empty" ));
		}
		return( FALSE );
	}
	if( !my_strlen( label )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Label is empty" ));
		}
		return( FALSE );
	}
	return( TRUE );
}

/*
 * ledger_set_cre_user:
 */
static void
ledger_set_cre_user( ofoLedger *ledger, const gchar *user )
{
	ofo_base_setter( LEDGER, ledger, string, LED_CRE_USER, user );
}

/*
 * ledger_set_cre_stamp:
 */
static void
ledger_set_cre_stamp( ofoLedger *ledger, const GTimeVal *stamp )
{
	ofo_base_setter( LEDGER, ledger, timestamp, LED_CRE_STAMP, stamp );
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
ledger_set_upd_user( ofoLedger *ledger, const gchar *user )
{
	ofo_base_setter( LEDGER, ledger, string, LED_UPD_USER, user );
}

/*
 * ledger_set_upd_stamp:
 */
static void
ledger_set_upd_stamp( ofoLedger *ledger, const GTimeVal *stamp )
{
	ofo_base_setter( LEDGER, ledger, timestamp, LED_UPD_STAMP, stamp );
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
 * ofo_ledger_currency_get_list:
 *
 * Returns: the list of currency codes ISO 3A used by this ledger.
 *
 * The content of returned list is owned by the ledger.
 * The returned list itself should be g_list_free() by the caller.
 */
GList *
ofo_ledger_currency_get_list( ofoLedger *ledger )
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
 * ofo_ledger_currency_update_code:
 * @ledger: this #ofoLedger instance.
 * @prev_id: the previous currency code.
 * @new_id: the new currency code.
 *
 * Update the detail balances with this new currency code.
 */
void
ofo_ledger_currency_update_code( ofoLedger *ledger, const gchar *prev_id, const gchar *new_id )
{
	GList *balances;

	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));
	g_return_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run );
	g_return_if_fail( my_strlen( prev_id ) > 0 );
	g_return_if_fail( my_strlen( new_id ) > 0 );

	balances = ledger_find_balance_by_code( ledger, prev_id );
	if( balances ){
		ofa_box_set_string( balances, LED_CURRENCY, new_id );
	}
}

/**
 * ofo_ledger_get_current_rough_debit:
 * @ledger:
 * @currency:
 *
 * Returns the current debit balance of this ledger for
 * the currency specified in the exercice, or zero if not found.
 */
ofxAmount
ofo_ledger_get_current_rough_debit( ofoLedger *ledger, const gchar *currency )
{
	GList *sdev;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), 0 );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, 0 );

	sdev = ledger_find_balance_by_code( ledger, currency );
	if( sdev ){
		return( ofa_box_get_amount( sdev, LED_CR_DEBIT ));
	}

	return( 0 );
}

/**
 * ofo_ledger_get_current_rough_credit:
 * @ledger:
 * @currency:
 *
 * Returns the current credit balance of this ledger for
 * the currency specified in the exercice, or zero if not found.
 */
ofxAmount
ofo_ledger_get_current_rough_credit( ofoLedger *ledger, const gchar *currency )
{
	GList *sdev;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), 0 );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, 0 );

	sdev = ledger_find_balance_by_code( ledger, currency );
	if( sdev ){
		return( ofa_box_get_amount( sdev, LED_CR_CREDIT ));
	}

	return( 0 );
}

/**
 * ofo_ledger_get_current_val_debit:
 * @ledger:
 * @currency:
 *
 * Returns the debit balance of this ledger for validated entries of
 * the exercice, or zero if not found.
 */
ofxAmount
ofo_ledger_get_current_val_debit( ofoLedger *ledger, const gchar *currency )
{
	GList *sdev;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), 0 );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, 0 );

	sdev = ledger_find_balance_by_code( ledger, currency );
	if( sdev ){
		return( ofa_box_get_amount( sdev, LED_CV_DEBIT ));
	}

	return( 0 );
}

/**
 * ofo_ledger_get_current_val_credit:
 * @ledger:
 * @currency:
 *
 * Returns the credit balance of this ledger for validated entries of
 * the exercice, or zero if not found.
 */
ofxAmount
ofo_ledger_get_current_val_credit( ofoLedger *ledger, const gchar *currency )
{
	GList *sdev;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), 0 );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, 0 );

	sdev = ledger_find_balance_by_code( ledger, currency );
	if( sdev ){
		return( ofa_box_get_amount( sdev, LED_CV_CREDIT ));
	}

	return( 0 );
}

/**
 * ofo_ledger_get_futur_val_debit:
 * @ledger:
 * @currency:
 *
 * Returns the debit balance of this ledger for
 * the currency specified from validated entries in the future, or zero
 * if not found.
 */
ofxAmount
ofo_ledger_get_futur_val_debit( ofoLedger *ledger, const gchar *currency )
{
	GList *sdev;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), 0 );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, 0 );

	sdev = ledger_find_balance_by_code( ledger, currency );
	if( sdev ){
		return( ofa_box_get_amount( sdev, LED_FV_DEBIT ));
	}

	return( 0 );
}

/**
 * ofo_ledger_get_futur_val_credit:
 * @ledger:
 * @currency:
 *
 * Returns the current credit balance of this ledger for
 * the currency specified from validated entries in the future, or zero
 * if not found.
 */
ofxAmount
ofo_ledger_get_futur_val_credit( ofoLedger *ledger, const gchar *currency )
{
	GList *sdev;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), 0 );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, 0 );

	sdev = ledger_find_balance_by_code( ledger, currency );
	if( sdev ){
		return( ofa_box_get_amount( sdev, LED_FV_CREDIT ));
	}

	return( 0 );
}

/**
 * ofo_ledger_get_futur_rough_debit:
 * @ledger:
 * @currency:
 *
 * Returns the debit balance of this ledger for
 * the currency specified from entries in the future, or zero if not
 * found.
 */
ofxAmount
ofo_ledger_get_futur_rough_debit( ofoLedger *ledger, const gchar *currency )
{
	GList *sdev;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), 0 );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, 0 );

	sdev = ledger_find_balance_by_code( ledger, currency );
	if( sdev ){
		return( ofa_box_get_amount( sdev, LED_FR_DEBIT ));
	}

	return( 0 );
}

/**
 * ofo_ledger_get_futur_rough_credit:
 * @ledger:
 * @currency:
 *
 * Returns the current credit balance of this ledger for
 * the currency specified from entries in the future, or zero if not
 * found.
 */
ofxAmount
ofo_ledger_get_futur_rough_credit( ofoLedger *ledger, const gchar *currency )
{
	GList *sdev;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), 0 );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, 0 );

	sdev = ledger_find_balance_by_code( ledger, currency );
	if( sdev ){
		return( ofa_box_get_amount( sdev, LED_FR_CREDIT ));
	}

	return( 0 );
}

static GList *
ledger_find_balance_by_code( ofoLedger *ledger, const gchar *currency )
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

/**
 * ofo_ledger_set_current_val_debit:
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
ofo_ledger_set_current_val_debit( ofoLedger *ledger, ofxAmount amount, const gchar *currency )
{
	GList *balance;

	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));
	g_return_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run );

	balance = ledger_new_balance_with_code( ledger, currency );
	g_return_if_fail( balance );

	ofa_box_set_amount( balance, LED_CV_DEBIT, amount );
}

/**
 * ofo_ledger_set_current_val_credit:
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
ofo_ledger_set_current_val_credit( ofoLedger *ledger, ofxAmount amount, const gchar *currency )
{
	GList *balance;

	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));
	g_return_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run );

	balance = ledger_new_balance_with_code( ledger, currency );
	g_return_if_fail( balance );

	ofa_box_set_amount( balance, LED_CV_CREDIT, amount );
}

/**
 * ofo_ledger_set_current_rough_debit:
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
ofo_ledger_set_current_rough_debit( ofoLedger *ledger, ofxAmount amount, const gchar *currency )
{
	GList *balance;

	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));
	g_return_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run );

	balance = ledger_new_balance_with_code( ledger, currency );
	g_return_if_fail( balance );

	ofa_box_set_amount( balance, LED_CR_DEBIT, amount );
}

/**
 * ofo_ledger_set_current_rough_credit:
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
ofo_ledger_set_current_rough_credit( ofoLedger *ledger, ofxAmount amount, const gchar *currency )
{
	GList *balance;

	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));
	g_return_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run );

	balance = ledger_new_balance_with_code( ledger, currency );
	g_return_if_fail( balance );

	ofa_box_set_amount( balance, LED_CR_CREDIT, amount );
}

/**
 * ofo_ledger_set_futur_rough_debit:
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
ofo_ledger_set_futur_rough_debit( ofoLedger *ledger, ofxAmount amount, const gchar *currency )
{
	GList *balance;

	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));
	g_return_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run );

	balance = ledger_new_balance_with_code( ledger, currency );
	g_return_if_fail( balance );

	ofa_box_set_amount( balance, LED_FR_DEBIT, amount );
}

/**
 * ofo_ledger_set_futur_rough_credit:
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
ofo_ledger_set_futur_rough_credit( ofoLedger *ledger, ofxAmount amount, const gchar *currency )
{
	GList *balance;

	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));
	g_return_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run );

	balance = ledger_new_balance_with_code( ledger, currency );
	g_return_if_fail( balance );

	ofa_box_set_amount( balance, LED_FR_CREDIT, amount );
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
		ofa_box_set_amount( balance, LED_CV_DEBIT, 0 );
		ofa_box_set_amount( balance, LED_CV_CREDIT, 0 );
		ofa_box_set_amount( balance, LED_CR_DEBIT, 0 );
		ofa_box_set_amount( balance, LED_CR_CREDIT, 0 );
		ofa_box_set_amount( balance, LED_FR_DEBIT, 0 );
		ofa_box_set_amount( balance, LED_FR_CREDIT, 0 );

		priv = ofo_ledger_get_instance_private( ledger );
		priv->balances = g_list_prepend( priv->balances, balance );
	}

	return( balance );
}

/*
 * add debit/credit to current+rough balance for the currency, creating
 * the new record if needed
 */
static GList *
ledger_add_balance_current_rough( ofoLedger *ledger, const gchar *currency, ofxAmount debit, ofxAmount credit )
{
	return( ledger_add_to_balance( ledger, currency, LED_CR_DEBIT, debit, LED_CR_CREDIT, credit ));
}

/*
 * add debit/credit to current+validated balance for the currency,
 * creating the new record if needed
 */
static GList *
ledger_add_balance_current_val( ofoLedger *ledger, const gchar *currency, ofxAmount debit, ofxAmount credit )
{
	return( ledger_add_to_balance( ledger, currency, LED_CV_DEBIT, debit, LED_CV_CREDIT, credit ));
}

/*
 * add debit/credit to future+rough balance for the currency, creating
 * the new record if needed
 */
static GList *
ledger_add_balance_futur_rough( ofoLedger *ledger, const gchar *currency, ofxAmount debit, ofxAmount credit )
{
	return( ledger_add_to_balance( ledger, currency, LED_FR_DEBIT, debit, LED_FR_CREDIT, credit ));
}

/*
 * add debit/credit to future+validated balance for the currency,
 * creating the new record if needed
 */
static GList *
ledger_add_balance_futur_val( ofoLedger *ledger, const gchar *currency, ofxAmount debit, ofxAmount credit )
{
	return( ledger_add_to_balance( ledger, currency, LED_FV_DEBIT, debit, LED_FV_CREDIT, credit ));
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
 * ofo_ledger_currency_get_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown ledger mnemos in OFA_T_LEDGERS_CUR child table.
 *
 * The returned list should be #ofo_ledger_currency_free_orphans() by the
 * caller.
 */
GList *
ofo_ledger_currency_get_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "OFA_T_LEDGERS_CUR" ));
}

static GList *
get_orphans( ofaIGetter *getter, const gchar *table )
{
	ofaHub *hub;
	ofaIDBConnect *connect;
	GList *orphans;
	GSList *result, *irow, *icol;
	gchar *query;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( table ), NULL );

	orphans = NULL;
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf( "SELECT DISTINCT(LED_MNEMO) FROM %s "
			"	WHERE LED_MNEMO NOT IN (SELECT LED_MNEMO FROM OFA_T_LEDGERS)", table );

	if( ofa_idbconnect_query_ex( connect, query, &result, FALSE )){
		for( irow=result ; irow ; irow=irow->next ){
			icol = irow->data;
			orphans = g_list_prepend( orphans, g_strdup(( const gchar * ) icol->data ));
		}
		ofa_idbconnect_free_results( result );
	}

	g_free( query );

	return( orphans );
}

/**
 * ofo_ledger_archive_balances:
 * @ledger: this #ofoLedger object.
 * @date: the archived date.
 *
 * Archiving a ledger balance is only relevant when user is sure that
 * no more entries will be set on this ledger (e.g. because the user
 * has closed the period).
 *
 * If we have a last archive, then the new archive balance is the
 * previous balance + the balance of entries between the two dates.
 *
 * If we do not have a last archive, then we get all entries from the
 * beginning of the exercice until the asked date.
 */
gboolean
ofo_ledger_archive_balances( ofoLedger *ledger, const GDate *date )
{
	gboolean ok;
	ofaIGetter *getter;
	ofaHub *hub;
	ofxAmount debit, credit;
	GDate last_date, from_date;
	GList *list, *it, *currencies;
	ofsLedgerBalance *sbal;
	const gchar *led_id, *cur_id;
	ofoDossier *dossier;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, FALSE );

	ok = TRUE;
	getter = ofo_base_get_getter( OFO_BASE( ledger ));
	hub = ofa_igetter_get_hub( getter );

	my_date_clear( &from_date );
	get_last_archive_date( ledger, &last_date );
	if( my_date_is_valid( &last_date )){
		my_date_set_from_date( &from_date, &last_date );
		g_date_add_days( &from_date, 1 );

	} else {
		dossier = ofa_hub_get_dossier( hub );
		my_date_set_from_date( &from_date, ofo_dossier_get_exe_begin( dossier ));
	}

	/* renew currencies soldes for this ledger
	 * adding (if any) entries for the period
	 *
	 * get balance of entries between two dates
	 * they are grouped by ledger+currency, there is so one item per currency
	 * list may be empty
	 */
	led_id = ofo_ledger_get_mnemo( ledger );
	list = ofo_entry_get_dataset_ledger_balance( getter, led_id, &from_date, date );
	currencies = ofo_ledger_currency_get_list( ledger );

	for( it=currencies ; it ; it=it->next ){
		cur_id = ( const gchar * ) it->data;
		sbal = ofs_ledger_balance_find_currency( list, led_id, cur_id );
		debit = sbal ? sbal->debit : 0;
		credit = sbal ? sbal->credit : 0;
		if( my_date_is_valid( &last_date )){
			debit += ofo_ledger_archive_get_debit( ledger, cur_id, &last_date );
			credit += ofo_ledger_archive_get_credit( ledger, cur_id, &last_date );
		}
		ok = do_add_archive_dbms( ledger, cur_id, date, debit, credit );
		if( ok ){
			do_add_archive_list( ledger, cur_id, date, debit, credit );
		}
	}

	g_list_free( currencies );
	ofs_ledger_balance_list_free( &list );

	return( ok );
}

static gboolean
do_add_archive_dbms( ofoLedger *ledger, const gchar *currency, const GDate *date, ofxAmount debit, ofxAmount credit )
{
	ofaIGetter *getter;
	ofaHub *hub;
	ofoCurrency *cur_obj;
	const ofaIDBConnect *connect;
	gchar *query, *sdate, *sdebit, *scredit;
	gboolean ok;

	getter = ofo_base_get_getter( OFO_BASE( ledger ));
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	cur_obj = ofo_currency_get_by_code( getter, currency );
	g_return_val_if_fail( cur_obj && OFO_IS_CURRENCY( cur_obj ), FALSE );

	sdate = my_date_to_str( date, MY_DATE_SQL );
	sdebit = ofa_amount_to_sql( debit, cur_obj );
	scredit = ofa_amount_to_sql( credit, cur_obj );

	query = g_strdup_printf(
				"INSERT INTO OFA_T_LEDGERS_ARC "
				"	(LED_MNEMO,LED_ARC_CURRENCY,LED_ARC_DATE,LED_ARC_DEBIT,LED_ARC_CREDIT) VALUES "
				"	('%s','%s','%s',%s,%s)",
				ofo_ledger_get_mnemo( ledger ), currency,
				sdate, sdebit, scredit );

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( sdate );
	g_free( sdebit );
	g_free( scredit );
	g_free( query );

	return( ok );
}

static void
do_add_archive_list( ofoLedger *ledger, const gchar *currency, const GDate *date, ofxAmount debit, ofxAmount credit )
{
	ofoLedgerPrivate *priv;
	GList *fields;

	priv = ofo_ledger_get_instance_private( ledger );

	fields = ofa_box_init_fields_list( st_archive_defs );
	ofa_box_set_string( fields, LED_MNEMO, ofo_ledger_get_mnemo( ledger ));
	ofa_box_set_string( fields, LED_ARC_CURRENCY, currency );
	ofa_box_set_date( fields, LED_ARC_DATE, date );
	ofa_box_set_amount( fields, LED_ARC_DEBIT, debit );
	ofa_box_set_amount( fields, LED_ARC_CREDIT, credit );

	priv->archives = g_list_append( priv->archives, fields );
}

/**
 * ofo_ledger_archive_get_count:
 * @ledger: the #ofoLedger ledger.
 *
 * Returns: the count of archived balances.
 */
guint
ofo_ledger_archive_get_count( ofoLedger *ledger )
{
	ofoLedgerPrivate *priv;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), 0 );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, 0 );

	priv = ofo_ledger_get_instance_private( ledger );

	return( g_list_length( priv->archives ));
}

/**
 * ofo_ledger_archive_get_currency:
 * @ledger: the #ofoLedger ledger.
 * @idx: the desired index, counted from zero.
 *
 * Returns: the currency iso code of the balance.
 */
const gchar *
ofo_ledger_archive_get_currency( ofoLedger *ledger, guint idx )
{
	ofoLedgerPrivate *priv;
	GList *nth;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), NULL );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, NULL );

	priv = ofo_ledger_get_instance_private( ledger );

	nth = g_list_nth( priv->archives, idx );
	if( nth ){
		return( ofa_box_get_string( nth->data, LED_ARC_CURRENCY ));
	}

	return( NULL );
}

/**
 * ofo_ofo_ledger_archive_get_date:
 * @ledger: the #ofoLedger ledger.
 * @idx: the desired index, counted from zero.
 *
 * Returns: the effect date of the balance.
 */
const GDate *
ofo_ledger_archive_get_date( ofoLedger *ledger, guint idx )
{
	ofoLedgerPrivate *priv;
	GList *nth;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), NULL );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, NULL );

	priv = ofo_ledger_get_instance_private( ledger );

	nth = g_list_nth( priv->archives, idx );
	if( nth ){
		return( ofa_box_get_date( nth->data, LED_ARC_DATE ));
	}

	return( NULL );
}

/**
 * ofo_ledger_archive_get_debit:
 * @ledger: the #ofoLedger ledger.
 * @idx: the desired index, counted from zero.
 *
 * Returns: the archived debit.
 */
ofxAmount
ofo_ledger_archive_get_debit( ofoLedger *ledger, const gchar *currency, const GDate *date )
{
	ofoLedgerPrivate *priv;
	gint idx;
	GList *nth;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), 0 );
	g_return_val_if_fail( my_strlen( currency ), 0 );
	g_return_val_if_fail( my_date_is_valid( date ), 0 );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, 0 );

	priv = ofo_ledger_get_instance_private( ledger );

	idx = get_archive_index( ledger, currency, date );
	if( idx >= 0 ){
		nth = g_list_nth( priv->archives, idx );
		if( nth ){
			return( ofa_box_get_amount( nth->data, LED_ARC_DEBIT ));
		}
	}

	return( 0 );
}

/**
 * ofo_ledger_archive_get_credit:
 * @ledger: the #ofoLedger ledger.
 * @idx: the desired index, counted from zero.
 *
 * Returns: the archived credit.
 */
ofxAmount
ofo_ledger_archive_get_credit( ofoLedger *ledger, const gchar *currency, const GDate *date )
{
	ofoLedgerPrivate *priv;
	gint idx;
	GList *nth;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), 0 );
	g_return_val_if_fail( my_strlen( currency ), 0 );
	g_return_val_if_fail( my_date_is_valid( date ), 0 );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, 0 );

	priv = ofo_ledger_get_instance_private( ledger );

	idx = get_archive_index( ledger, currency, date );
	if( idx >= 0 ){
		nth = g_list_nth( priv->archives, idx );
		if( nth ){
			return( ofa_box_get_amount( nth->data, LED_ARC_CREDIT ));
		}
	}

	return( 0 );
}

static gint
get_archive_index( ofoLedger *ledger, const gchar *currency, const GDate *date )
{
	ofoLedgerPrivate *priv;
	GList *it, *fields;
	const gchar *itcur;
	const GDate *itdate;
	gint i;

	priv = ofo_ledger_get_instance_private( ledger );

	for( it=priv->archives, i=0 ; it ; it=it->next, ++i ){
		fields = ( GList * ) it->data;
		itcur = ofa_box_get_string( fields, LED_ARC_CURRENCY );
		itdate = ofa_box_get_date( fields, LED_ARC_DATE );
		if( !my_collate( itcur, currency ) && !my_date_compare( itdate, date )){
			return( i );
		}
	}

	return( -1 );
}

/*
 * Set the most recent archived date
 */
static void
get_last_archive_date( ofoLedger *ledger, GDate *date )
{
	ofoLedgerPrivate *priv;
	const GDate *it_date;
	gint count, i;

	priv = ofo_ledger_get_instance_private( ledger );

	my_date_clear( date );
	count = g_list_length( priv->archives );

	for( i=0 ; i<count ; ++i ){
		it_date = ofo_ledger_archive_get_date( ledger, i );
		if( my_date_compare_ex( date, it_date, TRUE ) < 0 ){
			my_date_set_from_date( date, it_date );
		}
	}
}

/**
 * ofo_ledger_archive_get_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown ledger mnemos in OFA_T_LEDGERS_ARC child table.
 *
 * The returned list should be #ofo_ledger_archive_free_orphans() by the
 * caller.
 */
GList *
ofo_ledger_archive_get_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "OFA_T_LEDGERS_ARC" ));
}

/**
 * ofo_ledger_doc_get_count:
 * @ledger: this #ofoLedger object.
 *
 * Returns: the count of attached documents.
 */
guint
ofo_ledger_doc_get_count( ofoLedger *ledger )
{
	ofoLedgerPrivate *priv;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), 0 );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, 0 );

	priv = ofo_ledger_get_instance_private( ledger );

	return( g_list_length( priv->docs ));
}

/**
 * ofo_ledger_doc_get_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown ledger mnemos in OFA_T_LEDGERS_DOC child table.
 *
 * The returned list should be #ofo_ledger_doc_free_orphans() by the
 * caller.
 */
GList *
ofo_ledger_doc_get_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "OFA_T_LEDGERS_DOC" ));
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
	ofaIGetter *getter;
	ofaISignaler *signaler;
	gboolean ok;

	g_debug( "%s: ledger=%p, closing=%p", thisfn, ( void * ) ledger, ( void * ) closing );

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( closing && my_date_is_valid( closing ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( ledger ));
	signaler = ofa_igetter_get_signaler( getter );

	if( ofo_entry_validate_by_ledger( getter, ofo_ledger_get_mnemo( ledger ), closing )){
		ledger_set_last_clo( ledger, closing );
		if( ofo_ledger_update( ledger, ofo_ledger_get_mnemo( ledger ))){
			g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, ledger, NULL );
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
ofo_ledger_insert( ofoLedger *ledger )
{
	static const gchar *thisfn = "ofo_ledger_insert";
	gboolean ok;
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;

	g_debug( "%s: ledger=%p", thisfn, ( void * ) ledger );

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( ledger ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	/* rationale: see ofo-account.c */
	ofo_ledger_get_dataset( getter );

	if( ledger_do_insert( ledger, ofa_hub_get_connect( hub ))){
		my_icollector_collection_add_object(
				ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( ledger ), NULL, getter );
		g_signal_emit_by_name( signaler, SIGNALER_BASE_NEW, ledger );
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
	gchar *label, *notes, *stamp_str;
	gboolean ok;
	GTimeVal stamp;
	const gchar *userid;

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_ledger_get_label( ledger ));
	notes = my_utils_quote_sql( ofo_ledger_get_notes( ledger ));
	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_LEDGERS" );

	g_string_append_printf( query,
			"	(LED_MNEMO,LED_CRE_USER,LED_CRE_STAMP,LED_LABEL,LED_NOTES)"
			"	VALUES ('%s','%s','%s','%s',",
			ofo_ledger_get_mnemo( ledger ),
			userid,
			stamp_str,
			label );

	if( my_strlen( notes )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( ofa_idbconnect_query( connect, query->str, TRUE )){
		ledger_set_cre_user( ledger, userid );
		ledger_set_cre_stamp( ledger, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );

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
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: ledger=%p, prev_mnemo=%s",
			thisfn, ( void * ) ledger, prev_mnemo );

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( my_strlen( prev_mnemo ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( ledger ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( ledger_do_update( ledger, prev_mnemo, ofa_hub_get_connect( hub ))){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, ledger, prev_mnemo );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
ledger_do_update( ofoLedger *ledger, const gchar *prev_mnemo, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp_str, *sdate;
	GTimeVal stamp;
	const GDate *last_clo;
	const gchar *userid;

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_ledger_get_label( ledger ));
	notes = my_utils_quote_sql( ofo_ledger_get_notes( ledger ));
	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

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
	ofaIGetter *getter;
	ofaISignaler *signaler;

	g_debug( "%s: ledger=%p, currency=%s", thisfn, ( void * ) ledger, currency );

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( my_strlen( currency ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( ledger ));
	signaler = ofa_igetter_get_signaler( getter );

	balance = ledger_find_balance_by_code( ledger, currency );
	g_return_val_if_fail( balance, FALSE );

	if( ledger_do_update_balance( ledger, balance, getter )){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, ledger, NULL );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
ledger_do_update_balance( ofoLedger *ledger, GList *balance, ofaIGetter *getter )
{
	gchar *query;
	const gchar *currency;
	gchar *scurough_debit, *scurough_credit, *scurval_debit, *scurval_credit,
			*sfutrough_debit, *sfutrough_credit, *sfutval_debit, *sfutval_credit;
	gboolean ok;
	ofoCurrency *cur_obj;
	ofaHub *hub;
	const ofaIDBConnect *connect;

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	currency = ofa_box_get_string( balance, LED_CURRENCY );
	cur_obj = ofo_currency_get_by_code( getter, currency );
	g_return_val_if_fail( cur_obj && OFO_IS_CURRENCY( cur_obj ), FALSE );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_LEDGERS_CUR "
			"	WHERE LED_MNEMO='%s' AND LED_CUR_CODE='%s'",
					ofo_ledger_get_mnemo( ledger ),
					currency );

	ofa_idbconnect_query( connect, query, FALSE );
	g_free( query );

	scurough_debit = ofa_amount_to_sql( ofo_ledger_get_current_rough_debit( ledger, currency ), cur_obj );
	scurough_credit = ofa_amount_to_sql( ofo_ledger_get_current_rough_credit( ledger, currency ), cur_obj );
	scurval_debit = ofa_amount_to_sql( ofo_ledger_get_current_val_debit( ledger, currency ), cur_obj );
	scurval_credit = ofa_amount_to_sql( ofo_ledger_get_current_val_credit( ledger, currency ), cur_obj );
	sfutrough_debit = ofa_amount_to_sql( ofo_ledger_get_futur_rough_debit( ledger, currency ), cur_obj );
	sfutrough_credit = ofa_amount_to_sql( ofo_ledger_get_futur_rough_credit( ledger, currency ), cur_obj );
	sfutval_debit = ofa_amount_to_sql( ofo_ledger_get_futur_val_debit( ledger, currency ), cur_obj );
	sfutval_credit = ofa_amount_to_sql( ofo_ledger_get_futur_val_credit( ledger, currency ), cur_obj );

	query = g_strdup_printf(
					"INSERT INTO OFA_T_LEDGERS_CUR "
					"	(LED_MNEMO,LED_CUR_CODE,"
					"	LED_CUR_CR_DEBIT,LED_CUR_CR_CREDIT,"
					"	LED_CUR_CV_DEBIT,LED_CUR_CV_CREDIT,"
					"	LED_CUR_FR_DEBIT,LED_CUR_FR_CREDIT,"
					"	LED_CUR_FV_DEBIT,LED_CUR_FV_CREDIT) VALUES "
					"	('%s','%s',%s,%s,%s,%s,%s,%s,%s,%s)",
							ofo_ledger_get_mnemo( ledger ),
							currency,
							scurough_debit, scurough_credit,
							scurval_debit, scurval_credit,
							sfutrough_debit, sfutrough_credit,
							sfutval_debit, sfutval_credit );

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( scurough_debit );
	g_free( scurough_credit );
	g_free( scurval_debit );
	g_free( scurval_credit );
	g_free( sfutrough_debit );
	g_free( sfutrough_credit );
	g_free( sfutval_debit );
	g_free( sfutval_credit );
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
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: ledger=%p", thisfn, ( void * ) ledger );

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( ofo_ledger_is_deletable( ledger ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ledger )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( ledger ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( ledger_do_delete( ledger, ofa_hub_get_connect( hub ))){
		g_object_ref( ledger );
		my_icollector_collection_remove_object( ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( ledger ));
		g_signal_emit_by_name( signaler, SIGNALER_BASE_DELETED, ledger );
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
	const gchar *mnemo_a;

	mnemo_a = ofo_ledger_get_mnemo( a );

	return( my_collate( mnemo_a, mnemo ));
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

	g_return_val_if_fail( user_data && OFA_IS_IGETTER( user_data ), NULL );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					"OFA_T_LEDGERS",
					OFO_TYPE_LEDGER,
					OFA_IGETTER( user_data ));

	for( it=dataset ; it ; it=it->next ){
		ledger = OFO_LEDGER( it->data );
		priv = ofo_ledger_get_instance_private( ledger );
		/* ledger per currency */
		from = g_strdup_printf(
					"OFA_T_LEDGERS_CUR WHERE LED_MNEMO='%s'", ofo_ledger_get_mnemo( ledger ));
		priv->balances = ofo_base_load_rows( st_balance_defs, ofa_hub_get_connect( OFA_HUB( user_data )), from );
		g_free( from );
		/* ledger archives */
		from = g_strdup_printf(
					"OFA_T_LEDGERS_ARC WHERE LED_MNEMO='%s'", ofo_ledger_get_mnemo( ledger ));
		priv->archives = ofo_base_load_rows( st_archive_defs, ofa_hub_get_connect( OFA_HUB( user_data )), from );
		g_free( from );
	}

	return( dataset );
}

/*
 * ofaIDoc interface management
 */
static void
idoc_iface_init( ofaIDocInterface *iface )
{
	static const gchar *thisfn = "ofo_ledger_idoc_iface_init";

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
	static const gchar *thisfn = "ofo_ledger_iexportable_iface_init";

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
	return( g_strdup( _( "Reference : _ledgers" )));
}

static gboolean
iexportable_get_published( const ofaIExportable *instance )
{
	return( TRUE );
}

/*
 * iexportable_export:
 * @format_id: is 'DEFAULT' for the standard class export.
 *
 * Exports all the ledgers.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const gchar *format_id )
{
	static const gchar *thisfn = "ofo_ledger_iexportable_export";

	if( !my_collate( format_id, OFA_IEXPORTER_DEFAULT_FORMAT_ID )){
		return( iexportable_export_default( exportable ));
	}

	g_warning( "%s: format_id=%s unmanaged here", thisfn, format_id );

	return( FALSE );
}

static gboolean
iexportable_export_default( ofaIExportable *exportable )
{
	ofaIGetter *getter;
	ofaStreamFormat *stformat;
	ofoLedgerPrivate *priv;
	GList *dataset, *it, *ic, *bal, *itd;
	gchar *str1, *str2;
	gboolean ok;
	gulong count;
	gchar field_sep;
	ofoLedger *ledger;
	const gchar *cur_code;
	ofoCurrency *currency;

	getter = ofa_iexportable_get_getter( exportable );
	dataset = ofo_ledger_get_dataset( getter );

	stformat = ofa_iexportable_get_stream_format( exportable );
	field_sep = ofa_stream_format_get_field_sep( stformat );

	count = ( gulong ) g_list_length( dataset );
	if( ofa_stream_format_get_with_headers( stformat )){
		count += LEDGER_TABLES_COUNT;
	}
	for( it=dataset ; it ; it=it->next ){
		ledger = OFO_LEDGER( it->data );
		priv = ofo_ledger_get_instance_private( ledger );
		count += g_list_length( priv->balances );
		count += ofo_ledger_archive_get_count( ledger );
		count += ofo_ledger_doc_get_count( ledger );
	}
	ofa_iexportable_set_count( exportable, count+2 );

	/* add version lines at the very beginning of the file */
	str1 = g_strdup_printf( "0%c0%cVersion", field_sep, field_sep );
	ok = ofa_iexportable_append_line( exportable, str1 );
	g_free( str1 );
	if( ok ){
		str1 = g_strdup_printf( "1%c0%c%u", field_sep, field_sep, LEDGER_EXPORT_VERSION );
		ok = ofa_iexportable_append_line( exportable, str1 );
		g_free( str1 );
	}

	/* export headers */
	if( ok ){
		/* add new ofsBoxDef array at the end of the list */
		ok = ofa_iexportable_append_headers( exportable,
					LEDGER_TABLES_COUNT, st_boxed_defs, st_balance_defs, st_archive_defs, st_doc_defs );
	}

	/* export the dataset */
	for( it=dataset ; it && ok ; it=it->next ){
		ledger = OFO_LEDGER( it->data );

		str1 = ofa_box_csv_get_line( OFO_BASE( ledger )->prot->fields, stformat, NULL );
		str2 = g_strdup_printf( "1%c1%c%s", field_sep, field_sep, str1 );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str1 );

		priv = ofo_ledger_get_instance_private( ledger );

		for( ic=priv->balances ; ic && ok ; ic=ic->next ){
			bal = ( GList * ) ic->data;
			cur_code = ofa_box_get_string( bal, LED_CURRENCY );
			g_return_val_if_fail( cur_code && my_strlen( cur_code ), FALSE );
			currency = ofo_currency_get_by_code( getter, cur_code );
			g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), FALSE );
			str1 = ofa_box_csv_get_line( bal, stformat, currency );
			str2 = g_strdup_printf( "1%c2%c%s", field_sep, field_sep, str1 );
			ok = ofa_iexportable_append_line( exportable, str2 );
			g_free( str2 );
			g_free( str1 );
		}

		for( ic=priv->archives ; ic && ok ; ic=ic->next ){
			bal = ( GList * ) ic->data;
			cur_code = ofa_box_get_string( bal, LED_ARC_CURRENCY );
			g_return_val_if_fail( cur_code && my_strlen( cur_code ), FALSE );
			currency = ofo_currency_get_by_code( getter, cur_code );
			g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), FALSE );
			str1 = ofa_box_csv_get_line( bal, stformat, currency );
			str2 = g_strdup_printf( "1%c3%c%s", field_sep, field_sep, str1 );
			ok = ofa_iexportable_append_line( exportable, str2 );
			g_free( str2 );
			g_free( str1 );
		}

		for( itd=priv->docs ; itd && ok ; itd=itd->next ){
			str1 = ofa_box_csv_get_line( itd->data, stformat, NULL );
			str2 = g_strdup_printf( "1%c4%c%s", field_sep, field_sep, str1 );
			ok = ofa_iexportable_append_line( exportable, str2 );
			g_free( str2 );
			g_free( str1 );
		}
	}

	return( ok );
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
	ofaISignaler *signaler;
	ofaHub *hub;
	ofaIDBConnect *connect;
	GList *dataset;
	gchar *bck_table, *bck_det_table;

	dataset = iimportable_import_parse( importer, parms, lines );

	signaler = ofa_igetter_get_signaler( parms->getter );
	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );

	if( parms->parse_errs == 0 && parms->parsed_count > 0 ){
		bck_table = ofa_idbconnect_table_backup( connect, "OFA_T_LEDGERS" );
		bck_det_table = ofa_idbconnect_table_backup( connect, "OFA_T_LEDGERS_CUR" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs == 0 ){
			my_icollector_collection_free( ofa_igetter_get_collector( parms->getter ), OFO_TYPE_LEDGER );
			g_signal_emit_by_name( signaler, SIGNALER_COLLECTION_RELOAD, OFO_TYPE_LEDGER );

		} else {
			ofa_idbconnect_table_restore( connect, bck_table, "OFA_T_LEDGERS" );
			ofa_idbconnect_table_restore( connect, bck_det_table, "OFA_T_LEDGERS_CUR" );
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
	GTimeVal stamp;

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
		ledger = ofo_ledger_new( parms->getter );

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

		/* creation user */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			ledger_set_cre_user( ledger, cstr );
		}

		/* creation timestamp */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			my_stamp_set_from_sql( &stamp, cstr );
			ledger_set_cre_stamp( ledger, &stamp );
		}

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
	ofaHub *hub;
	const ofaIDBConnect *connect;
	gboolean insert;
	guint total, type;
	gchar *str;
	ofoLedger *ledger;
	const gchar *led_id;

	total = g_list_length( dataset );
	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );

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
			type = MY_PROGRESS_NORMAL;

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
					type = MY_PROGRESS_ERROR;
					insert = FALSE;
					total -= 1;
					parms->insert_errs += 1;
					break;
			}

			ofa_iimporter_progress_text( importer, parms, type, str );
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
 * ofaISignalable interface management
 */
static void
isignalable_iface_init( ofaISignalableInterface *iface )
{
	static const gchar *thisfn = "ofo_ledger_isignalable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect_to = isignalable_connect_to;
}

static void
isignalable_connect_to( ofaISignaler *signaler )
{
	static const gchar *thisfn = "ofo_ledger_isignalable_connect_to";

	g_debug( "%s: signaler=%p", thisfn, ( void * ) signaler );

	g_return_if_fail( signaler && OFA_IS_ISIGNALER( signaler ));

	g_signal_connect( signaler, SIGNALER_BASE_IS_DELETABLE, G_CALLBACK( signaler_on_deletable_object ), NULL );
	g_signal_connect( signaler, SIGNALER_BASE_NEW, G_CALLBACK( signaler_on_new_base ), NULL );
	g_signal_connect( signaler, SIGNALER_PERIOD_STATUS_CHANGE, G_CALLBACK( signaler_on_entry_period_status_changed ), NULL );
	g_signal_connect( signaler, SIGNALER_BASE_UPDATED, G_CALLBACK( signaler_on_updated_base ), NULL );
}

/*
 * SIGNALER_BASE_IS_DELETABLE signal handler
 */
static gboolean
signaler_on_deletable_object( ofaISignaler *signaler, ofoBase *object, void *empty )
{
	static const gchar *thisfn = "ofo_ledger_signaler_on_deletable_object";
	gboolean deletable;

	g_debug( "%s: signaler=%p, object=%p (%s), empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	deletable = TRUE;

	if( OFO_IS_CURRENCY( object )){
		deletable = signaler_is_deletable_currency( signaler, OFO_CURRENCY( object ));
	}

	return( deletable );
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
			"SELECT COUNT(*) FROM OFA_T_LEDGERS_CUR WHERE LED_CUR_CODE='%s'",
			ofo_currency_get_code( currency ));

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count == 0 );
}

/*
 * SIGNALER_BASE_NEW signal handler
 */
static void
signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, void *empty )
{
	static const gchar *thisfn = "ofo_ledger_signaler_on_new_base";

	g_debug( "%s: signaler=%p, object=%p (%s), empty=%p",
			thisfn, ( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	if( OFO_IS_ENTRY( object )){
		signaler_on_new_ledger_entry( signaler, OFO_ENTRY( object ));
	}
}

/*
 * we are recording a new entry (so necessarily on the current exercice)
 * thus update the balances
 */
static void
signaler_on_new_ledger_entry( ofaISignaler *signaler, ofoEntry *entry )
{
	ofaIGetter *getter;
	ofeEntryStatus status;
	ofeEntryPeriod period;
	const gchar *mnemo, *currency;
	ofoLedger *ledger;
	GList *balance;

	getter = ofo_base_get_getter( OFO_BASE( entry ));

	/* the only case where an entry is created with a 'past' status
	 *  is an imported entry in the past (before the beginning of the
	 *  exercice) - in this case, the 'new_object' message should not be
	 *  sent
	 * if not in the past, only allowed periods are 'current' or 'future'
	 * in these two cases, status must be 'rought' */
	period = ofo_entry_get_period( entry );
	g_return_if_fail( period != ENT_PERIOD_PAST );
	g_return_if_fail( period == ENT_PERIOD_CURRENT || period == ENT_PERIOD_FUTURE );

	status = ofo_entry_get_status( entry );
	g_return_if_fail( status == ENT_STATUS_ROUGH );

	mnemo = ofo_entry_get_ledger( entry );
	ledger = ofo_ledger_get_by_mnemo( getter, mnemo );
	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));

	currency = ofo_entry_get_currency( entry );

	switch( period ){
		case ENT_PERIOD_CURRENT:
			balance = ledger_add_balance_current_rough(
							ledger, currency, ofo_entry_get_debit( entry ), ofo_entry_get_credit( entry ));
			break;

		case ENT_PERIOD_FUTURE:
			balance = ledger_add_balance_futur_rough(
							ledger, currency, ofo_entry_get_debit( entry ), ofo_entry_get_credit( entry ));
			break;

		default:
			break;
	}

	if( ledger_do_update_balance( ledger, balance, getter )){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, ledger, NULL );
	}
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
	static const gchar *thisfn = "ofo_ledger_signaler_on_entry_period_status_changed";
	const gchar *mnemo, *currency;
	ofoLedger *ledger;
	ofxAmount debit, credit;
	GList *balance;
	ofaIGetter *getter;
	ofeEntryStatus status;
	ofeEntryPeriod period;

	g_debug( "%s: signaler=%p, entry=%p, prev_period=%d, prev_status=%d, new_period=%d, new_status=%d, empty=%p",
			thisfn, ( void * ) signaler, ( void * ) entry,
			prev_period, prev_status, new_period, new_status, ( void * ) empty );

	getter = ofo_base_get_getter( OFO_BASE( entry ));

	mnemo = ofo_entry_get_ledger( entry );
	ledger = ofo_ledger_get_by_mnemo( getter, mnemo );
	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));

	currency = ofo_entry_get_currency( entry );

	debit = ofo_entry_get_debit( entry );
	credit = ofo_entry_get_credit( entry );

	period = ( prev_period == -1 ? ofo_entry_get_period( entry ) : prev_period );
	status = ( prev_status == -1 ? ofo_entry_get_status( entry ) : prev_status );

	switch( period ){
		case ENT_PERIOD_CURRENT:
			switch( status ){
				case ENT_STATUS_ROUGH:
					ledger_add_balance_current_rough( ledger, currency, -debit, -credit );
					break;
				case ENT_STATUS_VALIDATED:
					ledger_add_balance_current_val( ledger, currency, -debit, -credit );
					break;
				default:
					break;
			}
			break;
		case ENT_PERIOD_FUTURE:
			switch( status ){
				case ENT_STATUS_ROUGH:
					ledger_add_balance_futur_rough( ledger, currency, -debit, -credit );
					break;
				case ENT_STATUS_VALIDATED:
					ledger_add_balance_futur_val( ledger, currency, -debit, -credit );
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}

	period = ( prev_period == -1 ? ofo_entry_get_period( entry ) : new_period );
	status = ( prev_status == -1 ? ofo_entry_get_status( entry ) : new_status );

	switch( period ){
		case ENT_PERIOD_CURRENT:
			switch( status ){
				case ENT_STATUS_ROUGH:
					ledger_add_balance_current_rough( ledger, currency, debit, credit );
					break;
				case ENT_STATUS_VALIDATED:
					ledger_add_balance_current_val( ledger, currency, debit, credit );
					break;
				default:
					break;
			}
			break;
		case ENT_PERIOD_FUTURE:
			switch( status ){
				case ENT_STATUS_ROUGH:
					ledger_add_balance_futur_rough( ledger, currency, debit, credit );
					break;
				case ENT_STATUS_VALIDATED:
					ledger_add_balance_futur_val( ledger, currency, debit, credit );
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}

	balance = ledger_find_balance_by_code( ledger, currency );
	if( ledger_do_update_balance( ledger, balance, getter )){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, ledger, NULL );
	}
}

/*
 * SIGNALER_BASE_UPDATED signal handler
 */
static void
signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, void *empty )
{
	static const gchar *thisfn = "ofo_ledger_signaler_on_updated_base";
	const gchar *code;

	g_debug( "%s: signaler=%p, object=%p (%s), prev_id=%s, empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) empty );

	if( OFO_IS_CURRENCY( object )){
		if( my_strlen( prev_id )){
			code = ofo_currency_get_code( OFO_CURRENCY( object ));
			if( g_utf8_collate( code, prev_id )){
				signaler_on_updated_currency_code( signaler, prev_id, code );
			}
		}
	}
}

/*
 * a currency iso code has been modified (this should be very rare)
 * so update our ledger records
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
					"UPDATE OFA_T_LEDGERS_CUR "
					"	SET LED_CUR_CODE='%s' WHERE LED_CUR_CODE='%s'", code, prev_id );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );

	g_free( query );
}
