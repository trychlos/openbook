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

#define _GNU_SOURCE
#include <glib/gi18n.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "my/my-double.h"
#include "my/my-utils.h"

#include "api/ofa-file-format.h"
#include "api/ofa-hub.h"
#include "api/ofa-icollectionable.h"
#include "api/ofa-icollector.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-preferences.h"
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
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order. So:
 * 1/ put in in an order compatible with import
 * 2/ no more modify it
 * 3/ take attention to be able to support the import of a previously
 *    exported file
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
										/* below data are not imported */
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

typedef struct {
	void *empty;						/* so that gcc -pedantic is happy */
}
	ofoCurrencyPrivate;

static ofoCurrency *currency_find_by_code( GList *set, const gchar *code );
static gint         currency_cmp_by_code( const ofoCurrency *a, const gchar *code );
static void         currency_set_upd_user( ofoCurrency *currency, const gchar *user );
static void         currency_set_upd_stamp( ofoCurrency *currency, const GTimeVal *stamp );
static gboolean     currency_do_insert( ofoCurrency *currency, const ofaIDBConnect *connect );
static gboolean     currency_insert_main( ofoCurrency *currency, const ofaIDBConnect *connect );
static gboolean     currency_do_update( ofoCurrency *currency, const gchar *prev_code, const ofaIDBConnect *connect );
static gboolean     currency_do_delete( ofoCurrency *currency, const ofaIDBConnect *connect );
static gint         currency_cmp_by_code( const ofoCurrency *a, const gchar *code );
static gint         currency_cmp_by_ptr( const ofoCurrency *a, const ofoCurrency *b );
static void         icollectionable_iface_init( ofaICollectionableInterface *iface );
static guint        icollectionable_get_interface_version( const ofaICollectionable *instance );
static GList       *icollectionable_load_collection( const ofaICollectionable *instance, ofaHub *hub );
static void         iexportable_iface_init( ofaIExportableInterface *iface );
static guint        iexportable_get_interface_version( const ofaIExportable *instance );
static gchar       *iexportable_get_label( const ofaIExportable *instance );
static gboolean     iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofaHub *hub );
static void         iimportable_iface_init( ofaIImportableInterface *iface );
static guint        iimportable_get_interface_version( const ofaIImportable *instance );
static gchar       *iimportable_get_label( const ofaIImportable *instance );
static gboolean     iimportable_import( ofaIImportable *exportable, GSList *lines, const ofaFileFormat *settings, ofaHub *hub );
static gboolean     currency_do_drop_content( const ofaIDBConnect *connect );

G_DEFINE_TYPE_EXTENDED( ofoCurrency, ofo_currency, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoCurrency )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init ))

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

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
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
 * ofo_currency_connect_to_hub_signaling_system:
 * @hub: the #ofaHub object.
 *
 * Connect to the @hub signaling system.
 */
void
ofo_currency_connect_to_hub_signaling_system( const ofaHub *hub )
{
	static const gchar *thisfn = "ofo_currency_connect_to_hub_signaling_system";

	g_debug( "%s: hub=%p", thisfn, ( void * ) hub );

	g_return_if_fail( hub && OFA_IS_HUB( hub ));
}

/**
 * ofo_currency_get_dataset:
 * @hub: the current #ofaHub object.
 *
 * Returns: the full #ofoCurrency dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_currency_get_dataset( ofaHub *hub )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	return( ofa_icollector_get_collection( OFA_ICOLLECTOR( hub ), hub, OFO_TYPE_CURRENCY ));
}

/**
 * ofo_currency_get_by_code:
 * @hub: the current #ofaHub object.
 *
 * Returns: the searched currency, or %NULL.
 *
 * The returned object is owned by the #ofoCurrency class, and should
 * not be unreffed by the caller.
 */
ofoCurrency *
ofo_currency_get_by_code( ofaHub *hub, const gchar *code )
{
	GList *dataset;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( my_strlen( code ), NULL );

	dataset = ofo_currency_get_dataset( hub );

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
 */
ofoCurrency *
ofo_currency_new( void )
{
	ofoCurrency *currency;

	currency = g_object_new( OFO_TYPE_CURRENCY, NULL );
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
	ofaHub *hub;
	const gchar *dev_code;
	ofoDossier *dossier;
	gboolean deletable;

	g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), FALSE );
	g_return_val_if_fail( !OFO_BASE( currency )->prot->dispose_has_run, FALSE );

	hub = ofo_base_get_hub( OFO_BASE( currency ));
	dossier = ofa_hub_get_dossier( hub );
	dev_code = ofo_currency_get_code( currency );

	deletable = !ofo_account_use_currency( hub, dev_code ) &&
				!ofo_dossier_use_currency( dossier, dev_code ) &&
				!ofo_entry_use_currency( hub, dev_code ) &&
				!ofo_ledger_use_currency( hub, dev_code );

	deletable &= ofa_idbmodel_get_is_deletable( hub, OFO_BASE( currency ));

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
			*msgerr = g_strdup( _( "Empty code" ));
		}
		return( FALSE );
	}
	if( !my_strlen( label )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Empty label" ));
		}
		return( FALSE );
	}
	if( !my_strlen( symbol )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Empty symbol" ));
		}
		return( FALSE );
	}
	if( digits < 0 ){
		if( msgerr ){
			*msgerr = g_strdup( _( "Invalid decimal digits count" ));
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
 * ofo_currency_insert:
 * @currency:
 * @hub: the current #ofaHub object.
 */
gboolean
ofo_currency_insert( ofoCurrency *currency, ofaHub *hub )
{
	static const gchar *thisfn = "ofo_currency_insert";
	gboolean ok;

	g_debug( "%s: currency=%p, hub=%p",
			thisfn, ( void * ) currency, ( void * ) hub );

	g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), FALSE );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( !OFO_BASE( currency )->prot->dispose_has_run, FALSE );

	if( currency_do_insert( currency, ofa_hub_get_connect( hub ))){
		ofo_base_set_hub( OFO_BASE( currency ), hub );
		ofa_icollector_add_object(
				OFA_ICOLLECTOR( hub ), hub, OFA_ICOLLECTIONABLE( currency ), ( GCompareFunc ) currency_cmp_by_ptr );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_NEW, currency );
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
	gchar *label, *notes, *stamp_str, *userid;
	GTimeVal stamp;
	const gchar *symbol;
	gboolean ok;

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_single( ofo_currency_get_label( currency ));
	symbol = ofo_currency_get_symbol( currency );
	notes = my_utils_quote_single( ofo_currency_get_notes( currency ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

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
	g_free( userid );

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
	ofaHub *hub;

	g_debug( "%s: currency=%p, prev_code=%s",
			thisfn, ( void * ) currency, prev_code );

	g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), FALSE );
	g_return_val_if_fail( !OFO_BASE( currency )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( currency ));

	if( currency_do_update( currency, prev_code, ofa_hub_get_connect( hub ))){
		ofa_icollector_sort_collection(
				OFA_ICOLLECTOR( hub ), OFO_TYPE_CURRENCY, ( GCompareFunc ) currency_cmp_by_ptr );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_UPDATED, currency, prev_code );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
currency_do_update( ofoCurrency *currency, const gchar *prev_code, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *label, *notes, *stamp_str, *userid;
	GTimeVal stamp;
	gboolean ok;

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_single( ofo_currency_get_label( currency ));
	notes = my_utils_quote_single( ofo_currency_get_notes( currency ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

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
	g_free( userid );

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
	ofaHub *hub;

	g_debug( "%s: currency=%p", thisfn, ( void * ) currency );

	g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), FALSE );
	g_return_val_if_fail( ofo_currency_is_deletable( currency ), FALSE );
	g_return_val_if_fail( !OFO_BASE( currency )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( currency ));

	if( currency_do_delete( currency, ofa_hub_get_connect( hub ))){
		g_object_ref( currency );
		ofa_icollector_remove_object( OFA_ICOLLECTOR( hub ), OFA_ICOLLECTIONABLE( currency ));
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_DELETED, currency );
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
	return( g_utf8_collate( ofo_currency_get_code( a ), code ));
}

static gint
currency_cmp_by_ptr( const ofoCurrency *a, const ofoCurrency *b )
{
	return( currency_cmp_by_code( a, ofo_currency_get_code( b )));
}

/*
 * ofaICollectionable interface management
 */
static void
icollectionable_iface_init( ofaICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_account_icollectionable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = icollectionable_get_interface_version;
	iface->load_collection = icollectionable_load_collection;
}

static guint
icollectionable_get_interface_version( const ofaICollectionable *instance )
{
	return( 1 );
}

static GList *
icollectionable_load_collection( const ofaICollectionable *instance, ofaHub *hub )
{
	GList *list;

	list = ofo_base_load_dataset(
					st_boxed_defs,
					"OFA_T_CURRENCIES ORDER BY CUR_CODE ASC",
					OFO_TYPE_CURRENCY,
					hub );

	return( list );
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
	iface->export = iexportable_export;
}

static guint
iexportable_get_interface_version( const ofaIExportable *instance )
{
	return( 1 );
}

static gchar *
iexportable_get_label( const ofaIExportable *instance )
{
	return( g_strdup( _( "Reference : _currencies" )));
}

/*
 * iexportable_export:
 *
 * Exports the classes line by line.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofaHub *hub )
{
	gchar *str;
	GList *dataset, *it;
	gboolean with_headers, ok;
	gulong count;

	dataset = ofo_currency_get_dataset( hub );
	with_headers = ofa_file_format_has_headers( settings );

	count = ( gulong ) g_list_length( dataset );
	if( with_headers ){
		count += 1;
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = ofa_box_csv_get_header( st_boxed_defs, settings );
		ok = ofa_iexportable_set_line( exportable, str );
		g_free( str );
		if( !ok ){
			return( FALSE );
		}
	}

	for( it=dataset ; it ; it=it->next ){
		str = ofa_box_csv_get_line( OFO_BASE( it->data )->prot->fields, settings );
		ok = ofa_iexportable_set_line( exportable, str );
		g_free( str );
		if( !ok ){
			return( FALSE );
		}
	}

	return( TRUE );
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
iimportable_get_interface_version( const ofaIImportable *instance )
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
 * Replace the whole table with the provided datas.
 *
 * Returns: 0 if no error has occurred, >0 if an error has been detected
 * during import phase (input file read), <0 if an error has occured
 * during insert phase.
 *
 * As the table is dropped between import phase and insert phase, if an
 * error occurs during insert phase, then the table is changed and only
 * contains the successfully inserted records.
 */
static gint
iimportable_import( ofaIImportable *importable, GSList *lines, const ofaFileFormat *settings, ofaHub *hub )
{
	GSList *itl, *fields, *itf;
	ofoCurrency *currency;
	GList *dataset, *it;
	guint errors, line;
	gchar *splitted, *str;
	const ofaIDBConnect *connect;

	line = 0;
	errors = 0;
	dataset = NULL;

	for( itl=lines ; itl ; itl=itl->next ){

		line += 1;
		currency = ofo_currency_new();
		fields = ( GSList * ) itl->data;
		ofa_iimportable_increment_progress( importable, IMPORTABLE_PHASE_IMPORT, 1 );
		itf = fields;

		/* currency code */
		str = ofa_iimportable_get_string( &itf, settings );
		if( !my_strlen( str )){
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, _( "empty ISO 3A currency code" ));
			errors += 1;
			continue;
		}
		ofo_currency_set_code( currency, str );
		g_free( str );

		/* currency label */
		str = ofa_iimportable_get_string( &itf, settings );
		if( !my_strlen( str )){
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, _( "empty currency label" ));
			errors += 1;
			continue;
		}
		ofo_currency_set_label( currency, str );
		g_free( str );

		/* currency symbol */
		str = ofa_iimportable_get_string( &itf, settings );
		if( !my_strlen( str )){
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, _( "empty currency symbol" ));
			errors += 1;
			continue;
		}
		ofo_currency_set_symbol( currency, str );
		g_free( str );

		/* currency digits - defaults to 2 */
		str = ofa_iimportable_get_string( &itf, settings );
		ofo_currency_set_digits( currency,
				( my_strlen( str ) ? atoi( str ) : CUR_DEFAULT_DIGITS ));
		g_free( str );

		/* notes
		 * we are tolerant on the last field... */
		str = ofa_iimportable_get_string( &itf, settings );
		splitted = my_utils_import_multi_lines( str );
		ofo_currency_set_notes( currency, splitted );
		g_free( splitted );
		g_free( str );

		dataset = g_list_prepend( dataset, currency );
	}

	if( !errors ){
		connect = ofa_hub_get_connect( hub );
		currency_do_drop_content( connect );

		for( it=dataset ; it ; it=it->next ){
			if( !currency_do_insert( OFO_CURRENCY( it->data ), connect )){
				errors -= 1;
			}
			ofa_iimportable_increment_progress( importable, IMPORTABLE_PHASE_INSERT, 1 );
		}

		ofo_currency_free_dataset( dataset );
		ofa_icollector_free_collection( OFA_ICOLLECTOR( hub ), OFO_TYPE_CURRENCY );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_RELOAD, OFO_TYPE_CURRENCY );
	}

	return( errors );
}

static gboolean
currency_do_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM OFA_T_CURRENCIES", TRUE ));
}
