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

#define _GNU_SOURCE
#include <glib/gi18n.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "my/my-double.h"
#include "my/my-icollectionable.h"
#include "my/my-icollector.h"
#include "my/my-stamp.h"
#include "my/my-utils.h"

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

/* priv instance data
 */
enum {
	CUR_CODE = 1,
	CUR_LABEL,
	CUR_SYMBOL,
	CUR_DIGITS,
	CUR_NOTES,
	CUR_UPD_USER,
	CUR_UPD_STAMP,
	CUR_DOC_ID,
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
		{ OFA_BOX_CSV( CUR_CODE ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* amount, counter: export zero as empty */
		{ OFA_BOX_CSV( CUR_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( CUR_SYMBOL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( CUR_DIGITS ),
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( CUR_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( CUR_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( CUR_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				FALSE },
		{ 0 }
};

static const ofsBoxDef st_doc_defs[] = {
		{ OFA_BOX_CSV( CUR_CODE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( CUR_DOC_ID ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ 0 }
};

#define CURRENCY_TABLES_COUNT           2
#define CURRENCY_EXPORT_VERSION         1

typedef struct {
	GList *docs;
}
	ofoCurrencyPrivate;

static ofoCurrency *currency_find_by_code( GList *set, const gchar *code );
static gint         currency_cmp_by_code( const ofoCurrency *a, const gchar *code );
static void         currency_set_upd_user( ofoCurrency *currency, const gchar *user );
static void         currency_set_upd_stamp( ofoCurrency *currency, const GTimeVal *stamp );
static GList       *get_orphans( ofaIGetter *getter, const gchar *table );
static gboolean     currency_do_insert( ofoCurrency *currency, const ofaIDBConnect *connect );
static gboolean     currency_insert_main( ofoCurrency *currency, const ofaIDBConnect *connect );
static gboolean     currency_do_update( ofoCurrency *currency, const gchar *prev_code, const ofaIDBConnect *connect );
static gboolean     currency_do_delete( ofoCurrency *currency, const ofaIDBConnect *connect );
static gint         currency_cmp_by_code( const ofoCurrency *a, const gchar *code );
static void         icollectionable_iface_init( myICollectionableInterface *iface );
static guint        icollectionable_get_interface_version( void );
static GList       *icollectionable_load_collection( void *user_data );
static void         idoc_iface_init( ofaIDocInterface *iface );
static guint        idoc_get_interface_version( void );
static void         iexportable_iface_init( ofaIExportableInterface *iface );
static guint        iexportable_get_interface_version( void );
static gchar       *iexportable_get_label( const ofaIExportable *instance );
static gboolean     iexportable_get_published( const ofaIExportable *instance );
static gboolean     iexportable_export( ofaIExportable *exportable, const gchar *format_id );
static gboolean     iexportable_export_default( ofaIExportable *exportable );
static void         iimportable_iface_init( ofaIImportableInterface *iface );
static guint        iimportable_get_interface_version( void );
static gchar       *iimportable_get_label( const ofaIImportable *instance );
static guint        iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static GList       *iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static void         iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean     currency_get_exists( const ofoCurrency *currency, const ofaIDBConnect *connect );
static gboolean     currency_drop_content( const ofaIDBConnect *connect );
static void         isignalable_iface_init( ofaISignalableInterface *iface );
static void         isignalable_connect_to( ofaISignaler *signaler );

G_DEFINE_TYPE_EXTENDED( ofoCurrency, ofo_currency, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoCurrency )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDOC, idoc_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNALABLE, isignalable_iface_init ))

static void
currency_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_currency_finalize";

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, CUR_CODE ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, CUR_LABEL ));

	g_return_if_fail( instance && OFO_IS_CURRENCY( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_currency_parent_class )->finalize( instance );
}

static void
currency_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_CURRENCY( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_currency_parent_class )->dispose( instance );
}

static void
ofo_currency_init( ofoCurrency *self )
{
	static const gchar *thisfn = "ofo_currency_init";
	ofoCurrencyPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofo_currency_get_instance_private( self );

	priv->docs = NULL;
}

static void
ofo_currency_class_init( ofoCurrencyClass *klass )
{
	static const gchar *thisfn = "ofo_currency_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = currency_dispose;
	G_OBJECT_CLASS( klass )->finalize = currency_finalize;
}

/**
 * ofo_currency_get_dataset:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the full #ofoCurrency dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_currency_get_dataset( ofaIGetter *getter )
{
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );

	return( my_icollector_collection_get( collector, OFO_TYPE_CURRENCY, getter ));
}

/**
 * ofo_currency_get_by_code:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the searched currency, or %NULL.
 *
 * The returned object is owned by the #ofoCurrency class, and should
 * not be unreffed by the caller.
 */
ofoCurrency *
ofo_currency_get_by_code( ofaIGetter *getter, const gchar *code )
{
	GList *dataset;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( code ), NULL );

	dataset = ofo_currency_get_dataset( getter );

	return( currency_find_by_code( dataset, code ));
}

static ofoCurrency *
currency_find_by_code( GList *set, const gchar *code )
{
	GList *found;

	found = g_list_find_custom(
				set, code, ( GCompareFunc ) currency_cmp_by_code );
	if( found ){
		return( OFO_CURRENCY( found->data ));
	}

	return( NULL );
}

/**
 * ofo_currency_new:
 * @getter: a #ofaIGetter instance.
 */
ofoCurrency *
ofo_currency_new( ofaIGetter *getter )
{
	ofoCurrency *currency;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	currency = g_object_new( OFO_TYPE_CURRENCY, "ofo-base-getter", getter, NULL );

	OFO_BASE( currency )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( currency );
}

/**
 * ofo_currency_get_code:
 */
const gchar *
ofo_currency_get_code( const ofoCurrency *currency )
{
	ofo_base_getter( CURRENCY, currency, string, NULL, CUR_CODE );
}

/**
 * ofo_currency_get_label:
 */
const gchar *
ofo_currency_get_label( const ofoCurrency *currency )
{
	ofo_base_getter( CURRENCY, currency, string, NULL, CUR_LABEL );
}

/**
 * ofo_currency_get_symbol:
 */
const gchar *
ofo_currency_get_symbol( const ofoCurrency *currency )
{
	ofo_base_getter( CURRENCY, currency, string, NULL, CUR_SYMBOL );
}

/**
 * ofo_currency_get_digits:
 */
gint
ofo_currency_get_digits( const ofoCurrency *currency )
{
	ofo_base_getter( CURRENCY, currency, int, 0, CUR_DIGITS );
}

/**
 * ofo_currency_get_notes:
 */
const gchar *
ofo_currency_get_notes( const ofoCurrency *currency )
{
	ofo_base_getter( CURRENCY, currency, string, NULL, CUR_NOTES );
}

/**
 * ofo_currency_get_upd_user:
 */
const gchar *
ofo_currency_get_upd_user( const ofoCurrency *currency )
{
	ofo_base_getter( CURRENCY, currency, string, NULL, CUR_UPD_USER );
}

/**
 * ofo_currency_get_upd_stamp:
 */
const GTimeVal *
ofo_currency_get_upd_stamp( const ofoCurrency *currency )
{
	ofo_base_getter( CURRENCY, currency, timestamp, NULL, CUR_UPD_STAMP );
}

/**
 * ofo_currency_get_precision:
 * @currency: this #ofoCurrency currency.
 *
 * Returns: the precision to be used with this currency.
 */
const gdouble
ofo_currency_get_precision( const ofoCurrency *currency )
{
	gint digits;

	g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), 0 );
	g_return_val_if_fail( !OFO_BASE( currency )->prot->dispose_has_run, 0 );

	digits = ofo_currency_get_digits( currency );

	return( exp10( -digits ));
}

/**
 * ofo_currency_is_deletable:
 *
 * A currency should not be deleted while it is referenced by an
 * account, a journal, an entry (or the dossier is an archive).
 */
gboolean
ofo_currency_is_deletable( const ofoCurrency *currency )
{
	ofaIGetter *getter;
	ofaISignaler *signaler;
	gboolean deletable;

	g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), FALSE );
	g_return_val_if_fail( !OFO_BASE( currency )->prot->dispose_has_run, FALSE );

	deletable = TRUE;
	getter = ofo_base_get_getter( OFO_BASE( currency ));

	if( deletable ){
		signaler = ofa_igetter_get_signaler( getter );
		g_signal_emit_by_name( signaler, SIGNALER_BASE_IS_DELETABLE, currency, &deletable );
	}

	return( deletable );
}

/**
 * ofo_currency_is_valid_data:
 *
 * Returns: %TRUE if the provided data makes the ofoCurrency a valid
 * object.
 *
 * Note that this does NOT check for key duplicate.
 */
gboolean
ofo_currency_is_valid_data( const gchar *code, const gchar *label, const gchar *symbol, gint digits, gchar **msgerr )
{
	if( msgerr ){
		*msgerr = NULL;
	}
	if( !my_strlen( code )){
		if( msgerr ){
			*msgerr = g_strdup( _( "ISO 3A code is empty" ));
		}
		return( FALSE );
	}
	if( !my_strlen( label )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Label is empty" ));
		}
		return( FALSE );
	}
	if( !my_strlen( symbol )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Symbol is empty" ));
		}
		return( FALSE );
	}
	if( digits < 0 ){
		if( msgerr ){
			*msgerr = g_strdup( _( "Digits count is invalid" ));
		}
		return( FALSE );
	}
	return( TRUE );
}

/**
 * ofo_currency_set_code:
 */
void
ofo_currency_set_code( ofoCurrency *currency, const gchar *code )
{
	ofo_base_setter( CURRENCY, currency, string, CUR_CODE, code );
}

/**
 * ofo_currency_set_label:
 */
void
ofo_currency_set_label( ofoCurrency *currency, const gchar *label )
{
	ofo_base_setter( CURRENCY, currency, string, CUR_LABEL, label );
}

/**
 * ofo_currency_set_symbol:
 */
void
ofo_currency_set_symbol( ofoCurrency *currency, const gchar *symbol )
{
	ofo_base_setter( CURRENCY, currency, string, CUR_SYMBOL, symbol );
}

/**
 * ofo_currency_set_digits:
 */
void
ofo_currency_set_digits( ofoCurrency *currency, gint digits )
{
	ofo_base_setter( CURRENCY, currency, int, CUR_DIGITS, digits );
}

/**
 * ofo_currency_set_notes:
 */
void
ofo_currency_set_notes( ofoCurrency *currency, const gchar *notes )
{
	ofo_base_setter( CURRENCY, currency, string, CUR_NOTES, notes );
}

/*
 * currency_set_upd_user:
 */
static void
currency_set_upd_user( ofoCurrency *currency, const gchar *user )
{
	ofo_base_setter( CURRENCY, currency, string, CUR_UPD_USER, user );
}

/*
 * currency_set_upd_stamp:
 */
static void
currency_set_upd_stamp( ofoCurrency *currency, const GTimeVal *stamp )
{
	ofo_base_setter( CURRENCY, currency, timestamp, CUR_UPD_STAMP, stamp );
}

/**
 * ofo_currency_doc_get_count:
 * @currency: this #ofoCurrency object.
 *
 * Returns: the count of attached documents.
 */
guint
ofo_currency_doc_get_count( ofoCurrency *currency )
{
	ofoCurrencyPrivate *priv;

	g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), 0 );
	g_return_val_if_fail( !OFO_BASE( currency )->prot->dispose_has_run, 0 );

	priv = ofo_currency_get_instance_private( currency );

	return( g_list_length( priv->docs ));
}

/**
 * ofo_currency_doc_get_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown currencies in OFA_T_CURRENCIES_DOC
 * child table.
 *
 * The returned list should be #ofo_currency_doc_free_orphans() by the
 * caller.
 */
GList *
ofo_currency_doc_get_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "OFA_T_CURRENCIES_DOC" ));
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

	query = g_strdup_printf( "SELECT DISTINCT(CUR_CODE) FROM %s "
			"	WHERE CUR_CODE NOT IN (SELECT CUR_CODE FROM OFA_T_CURRENCIES)", table );

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
 * ofo_currency_insert:
 * @currency:
 */
gboolean
ofo_currency_insert( ofoCurrency *currency )
{
	static const gchar *thisfn = "ofo_currency_insert";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: currency=%p", thisfn, ( void * ) currency );

	g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), FALSE );
	g_return_val_if_fail( !OFO_BASE( currency )->prot->dispose_has_run, FALSE );

	getter = ofo_base_get_getter( OFO_BASE( currency ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	/* rationale: see ofo-account.c */
	ofo_currency_get_dataset( getter );

	if( currency_do_insert( currency, ofa_hub_get_connect( hub ))){
		my_icollector_collection_add_object(
				ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( currency ), NULL, getter );
		g_signal_emit_by_name( signaler, SIGNALER_BASE_NEW, currency );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
currency_do_insert( ofoCurrency *currency, const ofaIDBConnect *connect )
{
	return( currency_insert_main( currency, connect ));
}

static gboolean
currency_insert_main( ofoCurrency *currency, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *label, *notes, *stamp_str;
	GTimeVal stamp;
	const gchar *symbol, *userid;
	gboolean ok;

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_currency_get_label( currency ));
	symbol = ofo_currency_get_symbol( currency );
	notes = my_utils_quote_sql( ofo_currency_get_notes( currency ));
	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "" );
	g_string_append_printf( query,
			"INSERT INTO OFA_T_CURRENCIES "
			"	(CUR_CODE,CUR_LABEL,CUR_SYMBOL,CUR_DIGITS,"
			"	CUR_NOTES,CUR_UPD_USER,CUR_UPD_STAMP)"
			"	VALUES ('%s','%s','%s',%d,",
			ofo_currency_get_code( currency ),
			label,
			symbol,
			ofo_currency_get_digits( currency ));

	if( my_strlen( notes )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
			"'%s','%s')",
			userid, stamp_str );

	if( ofa_idbconnect_query( connect, query->str, TRUE )){
		currency_set_upd_user( currency, userid );
		currency_set_upd_stamp( currency, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( label );
	g_free( notes );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_currency_update:
 */
gboolean
ofo_currency_update( ofoCurrency *currency, const gchar *prev_code )
{
	static const gchar *thisfn = "ofo_currency_update";
	gboolean ok;
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;

	g_debug( "%s: currency=%p, prev_code=%s",
			thisfn, ( void * ) currency, prev_code );

	g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), FALSE );
	g_return_val_if_fail( !OFO_BASE( currency )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( currency ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( currency_do_update( currency, prev_code, ofa_hub_get_connect( hub ))){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, currency, prev_code );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
currency_do_update( ofoCurrency *currency, const gchar *prev_code, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *label, *notes, *stamp_str;
	GTimeVal stamp;
	gboolean ok;
	const gchar *userid;

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_currency_get_label( currency ));
	notes = my_utils_quote_sql( ofo_currency_get_notes( currency ));
	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_CURRENCIES SET " );

	g_string_append_printf( query,
			"	CUR_CODE='%s',CUR_LABEL='%s',CUR_SYMBOL='%s',CUR_DIGITS=%d,",
			ofo_currency_get_code( currency ),
			label,
			ofo_currency_get_symbol( currency ),
			ofo_currency_get_digits( currency ));

	if( my_strlen( notes )){
		g_string_append_printf( query, "CUR_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "CUR_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	CUR_UPD_USER='%s',CUR_UPD_STAMP='%s'"
			"	WHERE CUR_CODE='%s'", userid, stamp_str, prev_code );

	if( ofa_idbconnect_query( connect, query->str, TRUE )){
		currency_set_upd_user( currency, userid );
		currency_set_upd_stamp( currency, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( label );
	g_free( notes );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_currency_delete:
 */
gboolean
ofo_currency_delete( ofoCurrency *currency )
{
	static const gchar *thisfn = "ofo_currency_delete";
	gboolean ok;
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;

	g_debug( "%s: currency=%p", thisfn, ( void * ) currency );

	g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), FALSE );
	g_return_val_if_fail( ofo_currency_is_deletable( currency ), FALSE );
	g_return_val_if_fail( !OFO_BASE( currency )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( currency ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( currency_do_delete( currency, ofa_hub_get_connect( hub ))){
		g_object_ref( currency );
		my_icollector_collection_remove_object( ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( currency ));
		g_signal_emit_by_name( signaler, SIGNALER_BASE_DELETED, currency );
		g_object_unref( currency );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
currency_do_delete( ofoCurrency *currency, const ofaIDBConnect *connect )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_CURRENCIES"
			"	WHERE CUR_CODE='%s'",
					ofo_currency_get_code( currency ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

static gint
currency_cmp_by_code( const ofoCurrency *a, const gchar *code )
{
	return( my_collate( ofo_currency_get_code( a ), code ));
}

/*
 * myICollectionable interface management
 */
static void
icollectionable_iface_init( myICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_currency_icollectionable_iface_init";

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
					"OFA_T_CURRENCIES",
					OFO_TYPE_CURRENCY,
					OFA_IGETTER( user_data ));

	return( list );
}

/*
 * ofaIDoc interface management
 */
static void
idoc_iface_init( ofaIDocInterface *iface )
{
	static const gchar *thisfn = "ofo_currency_idoc_iface_init";

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
	static const gchar *thisfn = "ofo_currency_iexportable_iface_init";

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
	return( g_strdup( _( "Reference : _currencies" )));
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
 * Exports all the currencies.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const gchar *format_id )
{
	static const gchar *thisfn = "ofo_currency_iexportable_export";

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
	GList *dataset, *it, *itd;
	gchar *str1, *str2;
	gboolean ok;
	gulong count;
	gchar field_sep;
	ofoCurrency *currency;
	ofoCurrencyPrivate *priv;

	getter = ofa_iexportable_get_getter( exportable );
	dataset = ofo_currency_get_dataset( getter );

	stformat = ofa_iexportable_get_stream_format( exportable );
	field_sep = ofa_stream_format_get_field_sep( stformat );

	count = ( gulong ) g_list_length( dataset );
	if( ofa_stream_format_get_with_headers( stformat )){
		count += CURRENCY_TABLES_COUNT;
	}
	for( it=dataset ; it ; it=it->next ){
		currency = OFO_CURRENCY( it->data );
		count += ofo_currency_doc_get_count( currency );
	}
	ofa_iexportable_set_count( exportable, count+2 );

	/* add version lines at the very beginning of the file */
	str1 = g_strdup_printf( "0%c0%cVersion", field_sep, field_sep );
	ok = ofa_iexportable_append_line( exportable, str1 );
	g_free( str1 );
	if( ok ){
		str1 = g_strdup_printf( "1%c0%c%u", field_sep, field_sep, CURRENCY_EXPORT_VERSION );
		ok = ofa_iexportable_append_line( exportable, str1 );
		g_free( str1 );
	}

	/* export headers */
	if( ok ){
		/* add new ofsBoxDef array at the end of the list */
		ok = ofa_iexportable_append_headers( exportable,
					CURRENCY_TABLES_COUNT, st_boxed_defs, st_doc_defs );
	}

	/* export the dataset */
	for( it=dataset ; it && ok ; it=it->next ){
		currency = OFO_CURRENCY( it->data );

		str1 = ofa_box_csv_get_line( OFO_BASE( it->data )->prot->fields, stformat, NULL );
		str2 = g_strdup_printf( "1%c1%c%s", field_sep, field_sep, str1 );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str1 );

		priv = ofo_currency_get_instance_private( currency );

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
 * ofaIImportable interface management
 */
static void
iimportable_iface_init( ofaIImportableInterface *iface )
{
	static const gchar *thisfn = "ofo_currency_iimportable_iface_init";

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

/**
 * ofo_currency_iimportable_import:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - currency code iso 3a
 * - label
 * - symbol
 * - digits
 * - notes (opt)
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
	gchar *bck_table;

	dataset = iimportable_import_parse( importer, parms, lines );

	signaler = ofa_igetter_get_signaler( parms->getter );
	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );

	if( parms->parse_errs == 0 && parms->parsed_count > 0 ){
		bck_table = ofa_idbconnect_table_backup( connect, "OFA_T_CURRENCIES" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs == 0 ){
			my_icollector_collection_free( ofa_igetter_get_collector( parms->getter ), OFO_TYPE_CURRENCY );
			g_signal_emit_by_name( signaler, SIGNALER_COLLECTION_RELOAD, OFO_TYPE_CURRENCY );

		} else {
			ofa_idbconnect_table_restore( connect, bck_table, "OFA_T_CURRENCIES" );
		}

		g_free( bck_table );
	}

	if( dataset ){
		ofo_currency_free_dataset( dataset );
	}

	return( parms->parse_errs+parms->insert_errs );
}

static GList *
iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines )
{
	GList *dataset;
	guint numline, total;
	GSList *itl, *fields, *itf;
	const gchar *cstr;
	gchar *splitted;
	ofoCurrency *currency;

	numline = 0;
	dataset = NULL;
	total = g_slist_length( lines );

	ofa_iimporter_progress_start( importer, parms );

	for( itl=lines ; itl ; itl=itl->next ){

		if( parms->stop && parms->parse_errs > 0 ){
			break;
		}

		numline += 1;
		fields = ( GSList * ) itl->data;
		currency = ofo_currency_new( parms->getter );

		/* currency code */
		itf = fields;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty ISO 3A currency code" ));
			parms->parse_errs += 1;
			continue;
		}
		ofo_currency_set_code( currency, cstr );

		/* currency label */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty currency label" ));
			parms->parse_errs += 1;
			continue;
		}
		ofo_currency_set_label( currency, cstr );

		/* currency symbol */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty currency symbol" ));
			parms->parse_errs += 1;
			continue;
		}
		ofo_currency_set_symbol( currency, cstr );

		/* currency digits - defaults to 2 */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		ofo_currency_set_digits( currency, ( my_strlen( cstr ) ? atoi( cstr ) : HUB_DEFAULT_DECIMALS_AMOUNT ));

		/* notes
		 * we are tolerant on the last field... */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		splitted = my_utils_import_multi_lines( cstr );
		ofo_currency_set_notes( currency, splitted );
		g_free( splitted );

		dataset = g_list_prepend( dataset, currency );
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
	ofoCurrency *currency;
	const gchar *cur_id;

	total = g_list_length( dataset );
	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );
	ofa_iimporter_progress_start( importer, parms );

	if( parms->empty && total > 0 ){
		currency_drop_content( connect );
	}

	for( it=dataset ; it ; it=it->next ){

		if( parms->stop && parms->insert_errs > 0 ){
			break;
		}

		str = NULL;
		insert = TRUE;
		currency = OFO_CURRENCY( it->data );

		if( currency_get_exists( currency, connect )){
			parms->duplicate_count += 1;
			cur_id = ofo_currency_get_code( currency );
			type = MY_PROGRESS_NORMAL;

			switch( parms->mode ){
				case OFA_IDUPLICATE_REPLACE:
					str = g_strdup_printf( _( "%s: duplicate currency, replacing previous one" ), cur_id );
					currency_do_delete( currency, connect );
					break;
				case OFA_IDUPLICATE_IGNORE:
					str = g_strdup_printf( _( "%s: duplicate currency, ignored (skipped)" ), cur_id );
					insert = FALSE;
					total -= 1;
					break;
				case OFA_IDUPLICATE_ABORT:
					str = g_strdup_printf( _( "%s: erroneous duplicate currency" ), cur_id );
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
			if( currency_do_insert( currency, connect )){
				parms->inserted_count += 1;
			} else {
				parms->insert_errs += 1;
			}
		}

		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->inserted_count, ( gulong ) total );
	}
}

static gboolean
currency_get_exists( const ofoCurrency *currency, const ofaIDBConnect *connect )
{
	const gchar *cur_id;
	gint count;
	gchar *str;

	count = 0;
	cur_id = ofo_currency_get_code( currency );
	str = g_strdup_printf( "SELECT COUNT(*) FROM OFA_T_CURRENCIES WHERE CUR_CODE='%s'", cur_id );
	ofa_idbconnect_query_int( connect, str, &count, FALSE );

	return( count > 0 );
}

static gboolean
currency_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM OFA_T_CURRENCIES", TRUE ));
}

/*
 * ofaISignalable interface management
 */
static void
isignalable_iface_init( ofaISignalableInterface *iface )
{
	static const gchar *thisfn = "ofo_currency_isignalable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect_to = isignalable_connect_to;
}

static void
isignalable_connect_to( ofaISignaler *signaler )
{
	static const gchar *thisfn = "ofo_currency_isignalable_connect_to";

	g_debug( "%s: signaler=%p", thisfn, ( void * ) signaler );

	g_return_if_fail( signaler && OFA_IS_ISIGNALER( signaler ));
}
