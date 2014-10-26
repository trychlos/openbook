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

#include <stdlib.h>
#include <string.h>

#include "api/my-utils.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-sgbd.h"

/* priv instance data
 */
struct _ofoCurrencyPrivate {

	/* sgbd data
	 */
	gchar     *code;
	gchar     *label;
	gchar     *symbol;
	gint       digits;
	gchar     *notes;
	gchar     *upd_user;
	GTimeVal   upd_stamp;
};

G_DEFINE_TYPE( ofoCurrency, ofo_currency, OFO_TYPE_BASE )

OFO_BASE_DEFINE_GLOBAL( st_global, currency )

static GList       *currency_load_dataset( void );
static ofoCurrency *currency_find_by_code( GList *set, const gchar *code );
static gint         currency_cmp_by_code( const ofoCurrency *a, const gchar *code );
static void         currency_set_upd_user( ofoCurrency *currency, const gchar *user );
static void         currency_set_upd_stamp( ofoCurrency *currency, const GTimeVal *stamp );
static gboolean     currency_do_insert( ofoCurrency *currency, const ofoSgbd *sgbd, const gchar *user );
static gboolean     currency_insert_main( ofoCurrency *currency, const ofoSgbd *sgbd, const gchar *user );
static gboolean     currency_do_update( ofoCurrency *currency, const gchar *prev_code, const ofoSgbd *sgbd, const gchar *user );
static gboolean     currency_do_delete( ofoCurrency *currency, const ofoSgbd *sgbd );
static gint         currency_cmp_by_code( const ofoCurrency *a, const gchar *code );
static gboolean     currency_do_drop_content( const ofoSgbd *sgbd );

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

	G_OBJECT_CLASS( klass )->dispose = currency_dispose;
	G_OBJECT_CLASS( klass )->finalize = currency_finalize;

	g_type_class_add_private( klass, sizeof( ofoCurrencyPrivate ));
}

/**
 * ofo_currency_get_dataset:
 * @dossier: the currently opened #ofoDossier dossier.
 *
 * Returns: The list of #ofoCurrency currencys, ordered by ascending
 * mnemonic. The returned list is owned by the #ofoCurrency class, and
 * should not be freed by the caller.
 */
GList *
ofo_currency_get_dataset( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_currency_get_dataset";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	OFO_BASE_SET_GLOBAL( st_global, dossier, currency );

	return( st_global->dataset );
}

static GList *
currency_load_dataset( void )
{
	GSList *result, *irow, *icol;
	ofoCurrency *currency;
	GList *dataset;
	const ofoSgbd *sgbd;
	GTimeVal timeval;

	sgbd = ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier ));

	result = ofo_sgbd_query_ex( sgbd,
			"SELECT CUR_CODE,CUR_LABEL,CUR_SYMBOL,CUR_DIGITS,"
			"	CUR_NOTES,CUR_UPD_USER,CUR_UPD_STAMP "
			"	FROM OFA_T_CURRENCIES", TRUE );

	dataset = NULL;

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

	ofo_sgbd_free_result( result );

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
ofo_currency_get_by_code( const ofoDossier *dossier, const gchar *code )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( code && g_utf8_strlen( code, -1 ), NULL );

	OFO_BASE_SET_GLOBAL( st_global, dossier, currency );

	return( currency_find_by_code( st_global->dataset, code ));
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
ofo_currency_is_deletable( const ofoCurrency *currency )
{
	ofoDossier *dossier;
	const gchar *dev_code;

	g_return_val_if_fail( OFO_IS_CURRENCY( currency ), FALSE );

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		dossier = OFO_DOSSIER( st_global->dossier );
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
ofo_currency_insert( ofoCurrency *currency )
{
	static const gchar *thisfn = "ofo_currency_insert";

	g_return_val_if_fail( OFO_IS_CURRENCY( currency ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		g_debug( "%s: currency=%p", thisfn, ( void * ) currency );

		if( currency_do_insert(
					currency,
					ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )),
					ofo_dossier_get_user( OFO_DOSSIER( st_global->dossier )))){

			OFO_BASE_ADD_TO_DATASET( st_global, currency );

			return( TRUE );
		}
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
currency_do_insert( ofoCurrency *currency, const ofoSgbd *sgbd, const gchar *user )
{
	return( currency_insert_main( currency, sgbd, user ));
}

static gboolean
currency_insert_main( ofoCurrency *currency, const ofoSgbd *sgbd, const gchar *user )
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

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
			"'%s','%s')",
			user, stamp_str );

	if( ofo_sgbd_query( sgbd, query->str, TRUE )){
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
ofo_currency_update( ofoCurrency *currency, const gchar *prev_code )
{
	static const gchar *thisfn = "ofo_currency_update";

	g_return_val_if_fail( OFO_IS_CURRENCY( currency ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		g_debug( "%s: currency=%p, prev_code=%s", thisfn, ( void * ) currency, prev_code );

		if( currency_do_update(
					currency,
					prev_code,
					ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )),
					ofo_dossier_get_user( OFO_DOSSIER( st_global->dossier )))){

			OFO_BASE_UPDATE_DATASET( st_global, currency, prev_code );

			return( TRUE );
		}
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
currency_do_update( ofoCurrency *currency, const gchar *prev_code, const ofoSgbd *sgbd, const gchar *user )
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

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "CUR_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "CUR_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	CUR_MAJ_USER='%s',CUR_MAJ_STAMP='%s'"
			"	WHERE CUR_CODE='%s'", user, stamp_str, prev_code );

	if( ofo_sgbd_query( sgbd, query->str, TRUE )){
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
ofo_currency_delete( ofoCurrency *currency )
{
	static const gchar *thisfn = "ofo_currency_delete";

	g_return_val_if_fail( OFO_IS_CURRENCY( currency ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );
	g_return_val_if_fail( ofo_currency_is_deletable( currency ), FALSE );

	if( !OFO_BASE( currency )->prot->dispose_has_run ){

		g_debug( "%s: currency=%p", thisfn, ( void * ) currency );

		if( currency_do_delete(
					currency,
					ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )))){

			OFO_BASE_REMOVE_FROM_DATASET( st_global, currency );

			return( TRUE );
		}
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
currency_do_delete( ofoCurrency *currency, const ofoSgbd *sgbd )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_CURRENCIES"
			"	WHERE CUR_CODE='%s'",
					ofo_currency_get_code( currency ));

	ok = ofo_sgbd_query( sgbd, query, TRUE );

	g_free( query );

	return( ok );
}

static gint
currency_cmp_by_code( const ofoCurrency *a, const gchar *code )
{
	return( g_utf8_collate( ofo_currency_get_code( a ), code ));
}

/**
 * ofo_currency_get_csv:
 */
GSList *
ofo_currency_get_csv( const ofoDossier *dossier )
{
	GList *set;
	GSList *lines;
	gchar *str, *stamp, *notes;
	ofoCurrency *currency;
	const gchar *muser;

	OFO_BASE_SET_GLOBAL( st_global, dossier, currency );

	lines = NULL;

	str = g_strdup_printf( "Code;Label;Symbol;Digits;Notes;MajUser;MajStamp" );
	lines = g_slist_prepend( lines, str );

	for( set=st_global->dataset ; set ; set=set->next ){
		currency = OFO_CURRENCY( set->data );

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

		lines = g_slist_prepend( lines, str );
	}

	return( g_slist_reverse( lines ));
}

/**
 * ofo_currency_import_csv:
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
 */
void
ofo_currency_import_csv( const ofoDossier *dossier, GSList *lines, gboolean with_header )
{
	static const gchar *thisfn = "ofo_currency_import_csv";
	ofoCurrency *currency;
	GSList *ili, *ico;
	GList *new_set, *ise;
	gint count;
	gint errors;
	const gchar *str;
	gchar *splitted;

	g_debug( "%s: dossier=%p, lines=%p (count=%d), with_header=%s",
			thisfn,
			( void * ) dossier,
			( void * ) lines, g_slist_length( lines ),
			with_header ? "True":"False" );

	OFO_BASE_SET_GLOBAL( st_global, dossier, currency );

	new_set = NULL;
	count = 0;
	errors = 0;

	for( ili=lines ; ili ; ili=ili->next ){
		count += 1;
		if( !( count == 1 && with_header )){
			currency = ofo_currency_new();
			ico=ili->data;

			/* currency code */
			str = ( const gchar * ) ico->data;
			if( !str || !g_utf8_strlen( str, -1 )){
				g_warning( "%s: (line %d) empty code", thisfn, count );
				errors += 1;
				continue;
			}
			ofo_currency_set_code( currency, str );

			/* currency label */
			ico = ico->next;
			str = ( const gchar * ) ico->data;
			if( !str || !g_utf8_strlen( str, -1 )){
				g_warning( "%s: (line %d) empty label", thisfn, count );
				errors += 1;
				continue;
			}
			ofo_currency_set_label( currency, str );

			/* currency symbol */
			ico = ico->next;
			str = ( const gchar * ) ico->data;
			if( !str || !g_utf8_strlen( str, -1 )){
				g_warning( "%s: (line %d) empty symbol", thisfn, count );
				errors += 1;
				continue;
			}
			ofo_currency_set_symbol( currency, str );

			/* currency digits */
			ico = ico->next;
			str = ( const gchar * ) ico->data;
			ofo_currency_set_digits( currency,
					( str && g_utf8_strlen( str, -1 ) ? atoi( str ) : CUR_DEFAULT_DIGITS ));

			/* notes
			 * we are tolerant on the last field... */
			ico = ico->next;
			if( ico ){
				str = ( const gchar * ) ico->data;
				if( str && g_utf8_strlen( str, -1 )){
					splitted = my_utils_import_multi_lines( str );
					ofo_currency_set_notes( currency, splitted );
					g_free( splitted );
				}
			} else {
				continue;
			}

			new_set = g_list_prepend( new_set, currency );
		}
	}

	if( !errors ){
		st_global->send_signal_new = FALSE;

		currency_do_drop_content( ofo_dossier_get_sgbd( dossier ));

		for( ise=new_set ; ise ; ise=ise->next ){
			currency_do_insert(
					OFO_CURRENCY( ise->data ),
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ));
		}

		g_list_free( new_set );

		if( st_global ){
			g_list_free_full( st_global->dataset, ( GDestroyNotify ) g_object_unref );
			st_global->dataset = NULL;
		}
		g_signal_emit_by_name( G_OBJECT( dossier ), OFA_SIGNAL_RELOAD_DATASET, OFO_TYPE_CURRENCY );

		st_global->send_signal_new = TRUE;
	}
}

static gboolean
currency_do_drop_content( const ofoSgbd *sgbd )
{
	return( ofo_sgbd_query( sgbd, "DELETE FROM OFA_T_CURRENCIES", TRUE ));
}
