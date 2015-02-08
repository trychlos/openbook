/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#include "api/my-utils.h"
#include "api/ofa-dbms.h"
#include "api/ofa-file-format.h"
#include "api/ofa-idataset.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iimportable.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"

/* priv instance data
 */
struct _ofoCurrencyPrivate {

	/* dbms data
	 */
	gchar     *code;
	gchar     *label;
	gchar     *symbol;
	gint       digits;
	gchar     *notes;
	gchar     *upd_user;
	GTimeVal   upd_stamp;
};

static ofoBaseClass *ofo_currency_parent_class = NULL;

static GList       *currency_load_dataset( ofoDossier *dossier );
static ofoCurrency *currency_find_by_code( GList *set, const gchar *code );
static gint         currency_cmp_by_code( const ofoCurrency *a, const gchar *code );
static void         currency_set_upd_user( ofoCurrency *currency, const gchar *user );
static void         currency_set_upd_stamp( ofoCurrency *currency, const GTimeVal *stamp );
static gboolean     currency_do_insert( ofoCurrency *currency, const ofaDbms *dbms, const gchar *user );
static gboolean     currency_insert_main( ofoCurrency *currency, const ofaDbms *dbms, const gchar *user );
static gboolean     currency_do_update( ofoCurrency *currency, const gchar *prev_code, const ofaDbms *dbms, const gchar *user );
static gboolean     currency_do_delete( ofoCurrency *currency, const ofaDbms *dbms );
static gint         currency_cmp_by_code( const ofoCurrency *a, const gchar *code );
static gint         currency_cmp_by_ptr( const ofoCurrency *a, const ofoCurrency *b );
static void         iexportable_iface_init( ofaIExportableInterface *iface );
static guint        iexportable_get_interface_version( const ofaIExportable *instance );
static gboolean     iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofoDossier *dossier );
static void         iimportable_iface_init( ofaIImportableInterface *iface );
static guint        iimportable_get_interface_version( const ofaIImportable *instance );
static gboolean     iimportable_import( ofaIImportable *exportable, GSList *lines, const ofaFileFormat *settings, ofoDossier *dossier );
static gboolean     currency_do_drop_content( const ofaDbms *dbms );

OFA_IDATASET_LOAD( CURRENCY, currency );

static void
currency_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_currency_finalize";
	ofoCurrencyPrivate *priv;

	priv = OFO_CURRENCY( instance )->priv;

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			priv->code, priv->label );

	g_return_if_fail( instance && OFO_IS_CURRENCY( instance ));

	/* free data members here */
	g_free( priv->code );
	g_free( priv->label );
	g_free( priv->symbol );
	g_free( priv->notes );

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

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFO_TYPE_CURRENCY, ofoCurrencyPrivate );
}

static void
ofo_currency_class_init( ofoCurrencyClass *klass )
{
	static const gchar *thisfn = "ofo_currency_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	ofo_currency_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = currency_dispose;
	G_OBJECT_CLASS( klass )->finalize = currency_finalize;

	g_type_class_add_private( klass, sizeof( ofoCurrencyPrivate ));
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofo_currency_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofoCurrencyClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) ofo_currency_class_init,
		NULL,
		NULL,
		sizeof( ofoCurrency ),
		0,
		( GInstanceInitFunc ) ofo_currency_init
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

	type = g_type_register_static( OFO_TYPE_BASE, "ofoCurrency", &info, 0 );

	g_type_add_interface_static( type, OFA_TYPE_IEXPORTABLE, &iexportable_iface_info );

	g_type_add_interface_static( type, OFA_TYPE_IIMPORTABLE, &iimportable_iface_info );

	return( type );
}

GType
ofo_currency_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

static GList *
currency_load_dataset( ofoDossier *dossier )
{
	GSList *result, *irow, *icol;
	ofoCurrency *currency;
	GList *dataset;
	const ofaDbms *dbms;
	GTimeVal timeval;

	dataset = NULL;
	dbms = ofo_dossier_get_dbms( dossier );

	if( ofa_dbms_query_ex( dbms,
			"SELECT CUR_CODE,CUR_LABEL,CUR_SYMBOL,CUR_DIGITS,"
			"	CUR_NOTES,CUR_UPD_USER,CUR_UPD_STAMP "
			"	FROM OFA_T_CURRENCIES", &result, TRUE )){

		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			currency = ofo_currency_new();
			ofo_currency_set_code( currency, ( gchar * ) icol->data );
			icol = icol->next;
			ofo_currency_set_label( currency, ( gchar * ) icol->data );
			icol = icol->next;
			ofo_currency_set_symbol( currency, ( gchar * ) icol->data );
			icol = icol->next;
			ofo_currency_set_digits( currency, icol->data ? atoi(( gchar * ) icol->data ) : CUR_DEFAULT_DIGITS );
			icol = icol->next;
			ofo_currency_set_notes( currency, ( gchar * ) icol->data );
			icol = icol->next;
			currency_set_upd_user( currency, ( gchar * ) icol->data );
			icol = icol->next;
			currency_set_upd_stamp( currency,
					my_utils_stamp_set_from_sql( &timeval, ( const gchar * ) icol->data ));

			dataset = g_list_prepend( dataset, currency );
		}

		ofa_dbms_free_results( result );
	}

	return( g_list_reverse( dataset ));
}

/**
 * ofo_currency_get_by_code:
 *
 * Returns: the searched currency, or %NULL.
 *
 * The returned object is owned by the #ofoCurrency class, and should
 * not be unreffed by the caller.
 */
ofoCurrency *
ofo_currency_get_by_code( ofoDossier *dossier, const gchar *code )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( code && g_utf8_strlen( code, -1 ), NULL );

	OFA_IDATASET_GET( dossier, CURRENCY, currency );

	return( currency_find_by_code( currency_dataset, code ));
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

	return( currency );
}

/**
 * ofo_currency_get_code:
 */
const gchar *
ofo_currency_get_code( const ofoCurrency *currency )
{
	g_return_val_if_fail( OFO_IS_CURRENCY( currency ), NULL );

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		return(( const gchar * ) currency->priv->code );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_currency_get_label:
 */
const gchar *
ofo_currency_get_label( const ofoCurrency *currency )
{
	g_return_val_if_fail( OFO_IS_CURRENCY( currency ), NULL );

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		return(( const gchar * ) currency->priv->label );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_currency_get_symbol:
 */
const gchar *
ofo_currency_get_symbol( const ofoCurrency *currency )
{
	g_return_val_if_fail( OFO_IS_CURRENCY( currency ), NULL );

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		return(( const gchar * ) currency->priv->symbol );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_currency_get_digits:
 */
gint
ofo_currency_get_digits( const ofoCurrency *currency )
{
	g_return_val_if_fail( OFO_IS_CURRENCY( currency ), 0 );

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		return( currency->priv->digits );
	}

	g_assert_not_reached();
	return( 0 );
}

/**
 * ofo_currency_get_notes:
 */
const gchar *
ofo_currency_get_notes( const ofoCurrency *currency )
{
	g_return_val_if_fail( OFO_IS_CURRENCY( currency ), NULL );

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		return(( const gchar * ) currency->priv->notes );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_currency_get_upd_user:
 */
const gchar *
ofo_currency_get_upd_user( const ofoCurrency *currency )
{
	g_return_val_if_fail( OFO_IS_CURRENCY( currency ), NULL );

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		return(( const gchar * ) currency->priv->upd_user );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_currency_get_upd_stamp:
 */
const GTimeVal *
ofo_currency_get_upd_stamp( const ofoCurrency *currency )
{
	g_return_val_if_fail( OFO_IS_CURRENCY( currency ), NULL );

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &currency->priv->upd_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_currency_is_deletable:
 *
 * A currency should not be deleted while it is referenced by an
 * account, a journal, an entry.
 */
gboolean
ofo_currency_is_deletable( const ofoCurrency *currency, ofoDossier *dossier )
{
	const gchar *dev_code;

	g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		dev_code = ofo_currency_get_code( currency );

		return( !ofo_dossier_use_currency( dossier, dev_code ) &&
				!ofo_entry_use_currency( dossier, dev_code ) &&
				!ofo_ledger_use_currency( dossier, dev_code ) &&
				!ofo_account_use_currency( dossier, dev_code ));
	}

	g_assert_not_reached();
	return( FALSE );
}

/**
 * ofo_currency_is_valid:
 *
 * Returns: %TRUE if the provided data makes the ofoCurrency a valid
 * object.
 *
 * Note that this does NOT check for key duplicate.
 */
gboolean
ofo_currency_is_valid( const gchar *code, const gchar *label, const gchar *symbol, gint digits )
{
	return( code && g_utf8_strlen( code, -1 ) &&
			label && g_utf8_strlen( label, -1 ) &&
			symbol && g_utf8_strlen( symbol, -1 ) &&
			digits > 0 );
}

/**
 * ofo_currency_set_code:
 */
void
ofo_currency_set_code( ofoCurrency *currency, const gchar *code )
{
	g_return_if_fail( OFO_IS_CURRENCY( currency ));

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		g_free( currency->priv->code );
		currency->priv->code = g_strdup( code );
	}
}

/**
 * ofo_currency_set_label:
 */
void
ofo_currency_set_label( ofoCurrency *currency, const gchar *label )
{
	g_return_if_fail( OFO_IS_CURRENCY( currency ));

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		g_free( currency->priv->label );
		currency->priv->label = g_strdup( label );
	}
}

/**
 * ofo_currency_set_symbol:
 */
void
ofo_currency_set_symbol( ofoCurrency *currency, const gchar *symbol )
{
	g_return_if_fail( OFO_IS_CURRENCY( currency ));

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		g_free( currency->priv->symbol );
		currency->priv->symbol = g_strdup( symbol );
	}
}

/**
 * ofo_currency_set_digits:
 */
void
ofo_currency_set_digits( ofoCurrency *currency, gint digits )
{
	g_return_if_fail( OFO_IS_CURRENCY( currency ));

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		currency->priv->digits = digits;
	}
}

/**
 * ofo_currency_set_notes:
 */
void
ofo_currency_set_notes( ofoCurrency *currency, const gchar *notes )
{
	g_return_if_fail( OFO_IS_CURRENCY( currency ));

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		g_free( currency->priv->notes );
		currency->priv->notes = g_strdup( notes );
	}
}

/*
 * currency_set_upd_user:
 */
static void
currency_set_upd_user( ofoCurrency *currency, const gchar *user )
{
	g_return_if_fail( OFO_IS_CURRENCY( currency ));

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		g_free( currency->priv->upd_user );
		currency->priv->upd_user = g_strdup( user );
	}
}

/*
 * currency_set_upd_stamp:
 */
static void
currency_set_upd_stamp( ofoCurrency *currency, const GTimeVal *stamp )
{
	g_return_if_fail( OFO_IS_CURRENCY( currency ));

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		my_utils_stamp_set_from_stamp( &currency->priv->upd_stamp, stamp );
	}
}

/**
 * ofo_currency_insert:
 */
gboolean
ofo_currency_insert( ofoCurrency *currency, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_currency_insert";

	g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		g_debug( "%s: currency=%p, dossier=%p",
				thisfn, ( void * ) currency, ( void * ) dossier );

		if( currency_do_insert(
					currency,
					ofo_dossier_get_dbms( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFA_IDATASET_ADD( dossier, CURRENCY, currency );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
currency_do_insert( ofoCurrency *currency, const ofaDbms *dbms, const gchar *user )
{
	return( currency_insert_main( currency, dbms, user ));
}

static gboolean
currency_insert_main( ofoCurrency *currency, const ofaDbms *dbms, const gchar *user )
{
	GString *query;
	gchar *label, *notes, *stamp_str;
	GTimeVal stamp;
	const gchar *symbol;
	gboolean ok;

	ok = FALSE;
	label = my_utils_quote( ofo_currency_get_label( currency ));
	symbol = ofo_currency_get_symbol( currency );
	notes = my_utils_quote( ofo_currency_get_notes( currency ));
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
			user, stamp_str );

	if( ofa_dbms_query( dbms, query->str, TRUE )){
		currency_set_upd_user( currency, user );
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
ofo_currency_update( ofoCurrency *currency, ofoDossier *dossier, const gchar *prev_code )
{
	static const gchar *thisfn = "ofo_currency_update";

	g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		g_debug( "%s: currency=%p, dossier=%p, prev_code=%s",
				thisfn, ( void * ) currency, ( void * ) dossier, prev_code );

		if( currency_do_update(
					currency,
					prev_code,
					ofo_dossier_get_dbms( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFA_IDATASET_UPDATE( dossier, CURRENCY, currency, prev_code );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
currency_do_update( ofoCurrency *currency, const gchar *prev_code, const ofaDbms *dbms, const gchar *user )
{
	GString *query;
	gchar *label, *notes, *stamp_str;
	GTimeVal stamp;
	gboolean ok;

	ok = FALSE;
	label = my_utils_quote( ofo_currency_get_label( currency ));
	notes = my_utils_quote( ofo_currency_get_notes( currency ));
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
			"	WHERE CUR_CODE='%s'", user, stamp_str, prev_code );

	if( ofa_dbms_query( dbms, query->str, TRUE )){
		currency_set_upd_user( currency, user );
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
ofo_currency_delete( ofoCurrency *currency, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_currency_delete";

	g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( ofo_currency_is_deletable( currency, dossier ), FALSE );

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		g_debug( "%s: currency=%p, dossier=%p",
				thisfn, ( void * ) currency, ( void * ) dossier );

		if( currency_do_delete(
					currency,
					ofo_dossier_get_dbms( dossier ))){

			OFA_IDATASET_REMOVE( dossier, CURRENCY, currency );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
currency_do_delete( ofoCurrency *currency, const ofaDbms *dbms )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_CURRENCIES"
			"	WHERE CUR_CODE='%s'",
					ofo_currency_get_code( currency ));

	ok = ofa_dbms_query( dbms, query, TRUE );

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
 * ofaIExportable interface management
 */
static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofo_currency_iexportable_iface_init";

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
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofoDossier *dossier )
{
	GList *it;
	GSList *lines;
	gchar *str, *stamp, *notes;
	ofoCurrency *currency;
	const gchar *muser;
	gboolean ok, with_headers;
	gulong count;

	OFA_IDATASET_GET( dossier, CURRENCY, currency );

	with_headers = ofa_file_format_has_headers( settings );

	count = ( gulong ) g_list_length( currency_dataset );
	if( with_headers ){
		count += 1;
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = g_strdup_printf( "Code;Label;Symbol;Digits;Notes;MajUser;MajStamp" );
		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}
	}

	for( it=currency_dataset ; it ; it=it->next ){
		currency = OFO_CURRENCY( it->data );

		notes = my_utils_export_multi_lines( ofo_currency_get_notes( currency ));
		muser = ofo_currency_get_upd_user( currency );
		stamp = my_utils_stamp_to_str( ofo_currency_get_upd_stamp( currency ), MY_STAMP_YYMDHMS );

		str = g_strdup_printf( "%s;%s;%s;%d;%s;%s;%s",
				ofo_currency_get_code( currency ),
				ofo_currency_get_label( currency ),
				ofo_currency_get_symbol( currency ),
				ofo_currency_get_digits( currency ),
				notes ? notes : "",
				muser ? muser : "",
				muser ? stamp : "" );

		g_free( notes );
		g_free( stamp );

		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
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
	iface->import = iimportable_import;
}

static guint
iimportable_get_interface_version( const ofaIImportable *instance )
{
	return( 1 );
}

/**
 * ofo_currency_iimportable_importv:
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
iimportable_import( ofaIImportable *importable, GSList *lines, const ofaFileFormat *settings, ofoDossier *dossier )
{
	GSList *itl, *fields, *itf;
	const gchar *cstr;
	ofoCurrency *currency;
	GList *dataset, *it;
	guint errors, line;
	gchar *splitted;

	line = 0;
	errors = 0;
	dataset = NULL;

	for( itl=lines ; itl ; itl=itl->next ){

		line += 1;
		currency = ofo_currency_new();
		fields = ( GSList * ) itl->data;
		ofa_iimportable_increment_progress( importable, IMPORTABLE_PHASE_IMPORT, 1 );

		/* currency code */
		itf = fields;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !cstr || !g_utf8_strlen( cstr, -1 )){
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, _( "empty ISO 3A currency code" ));
			errors += 1;
			continue;
		}
		ofo_currency_set_code( currency, cstr );

		/* currency label */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !cstr || !g_utf8_strlen( cstr, -1 )){
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, _( "empty currency label" ));
			errors += 1;
			continue;
		}
		ofo_currency_set_label( currency, cstr );

		/* currency symbol */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !cstr || !g_utf8_strlen( cstr, -1 )){
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, _( "empty currency symbol" ));
			errors += 1;
			continue;
		}
		ofo_currency_set_symbol( currency, cstr );

		/* currency digits - defaults to 2 */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		ofo_currency_set_digits( currency,
				( cstr && g_utf8_strlen( cstr, -1 ) ? atoi( cstr ) : CUR_DEFAULT_DIGITS ));

		/* notes
		 * we are tolerant on the last field... */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		splitted = my_utils_import_multi_lines( cstr );
		ofo_currency_set_notes( currency, splitted );
		g_free( splitted );

		dataset = g_list_prepend( dataset, currency );
	}

	if( !errors ){
		ofa_idataset_set_signal_new_allowed( dossier, OFO_TYPE_CURRENCY, FALSE );

		currency_do_drop_content( ofo_dossier_get_dbms( dossier ));

		for( it=dataset ; it ; it=it->next ){
			if( !currency_do_insert(
					OFO_CURRENCY( it->data ),
					ofo_dossier_get_dbms( dossier ),
					ofo_dossier_get_user( dossier ))){
				errors -= 1;
			}
			ofa_iimportable_increment_progress( importable, IMPORTABLE_PHASE_INSERT, 1 );
		}

		g_list_free_full( dataset, ( GDestroyNotify ) g_object_unref );
		ofa_idataset_free_dataset( dossier, OFO_TYPE_CURRENCY );

		g_signal_emit_by_name(
				G_OBJECT( dossier ), SIGNAL_DOSSIER_RELOAD_DATASET, OFO_TYPE_CURRENCY );

		ofa_idataset_set_signal_new_allowed( dossier, OFO_TYPE_CURRENCY, TRUE );
	}

	return( errors );
}

static gboolean
currency_do_drop_content( const ofaDbms *dbms )
{
	return( ofa_dbms_query( dbms, "DELETE FROM OFA_T_CURRENCIES", TRUE ));
}
