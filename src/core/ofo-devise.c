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

#include "core/my-utils.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-devise.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-journal.h"
#include "api/ofo-sgbd.h"

/* priv instance data
 */
struct _ofoDevisePrivate {

	/* sgbd data
	 */
	gchar     *code;
	gchar     *label;
	gchar     *symbol;
	gint       digits;
	gchar     *notes;
	gchar     *maj_user;
	GTimeVal   maj_stamp;
};

G_DEFINE_TYPE( ofoDevise, ofo_devise, OFO_TYPE_BASE )

OFO_BASE_DEFINE_GLOBAL( st_global, devise )

static GList     *devise_load_dataset( void );
static ofoDevise *devise_find_by_code( GList *set, const gchar *code );
static gint       devise_cmp_by_code( const ofoDevise *a, const gchar *code );
static gboolean   devise_do_insert( ofoDevise *devise, const ofoSgbd *sgbd, const gchar *user );
static gboolean   devise_insert_main( ofoDevise *devise, const ofoSgbd *sgbd, const gchar *user );
static gboolean   devise_do_update( ofoDevise *devise, const gchar *prev_code, const ofoSgbd *sgbd, const gchar *user );
static gboolean   devise_do_delete( ofoDevise *devise, const ofoSgbd *sgbd );
static gint       devise_cmp_by_code( const ofoDevise *a, const gchar *code );
static gboolean   devise_do_drop_content( const ofoSgbd *sgbd );

static void
ofo_devise_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_devise_finalize";
	ofoDevisePrivate *priv;

	priv = OFO_DEVISE( instance )->private;

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			priv->code, priv->label );

	/* free data members here */
	g_free( priv->code );
	g_free( priv->label );
	g_free( priv->symbol );
	g_free( priv->notes );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_devise_parent_class )->finalize( instance );
}

static void
ofo_devise_dispose( GObject *instance )
{
	g_return_if_fail( OFO_IS_DEVISE( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_devise_parent_class )->dispose( instance );
}

static void
ofo_devise_init( ofoDevise *self )
{
	static const gchar *thisfn = "ofo_devise_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofoDevisePrivate, 1 );
}

static void
ofo_devise_class_init( ofoDeviseClass *klass )
{
	static const gchar *thisfn = "ofo_devise_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ofo_devise_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_devise_finalize;
}

/**
 * ofo_devise_get_dataset:
 * @dossier: the currently opened #ofoDossier dossier.
 *
 * Returns: The list of #ofoDevise devises, ordered by ascending
 * mnemonic. The returned list is owned by the #ofoDevise class, and
 * should not be freed by the caller.
 */
GList *
ofo_devise_get_dataset( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_devise_get_dataset";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	OFO_BASE_SET_GLOBAL( st_global, dossier, devise );

	return( st_global->dataset );
}

static GList *
devise_load_dataset( void )
{
	GSList *result, *irow, *icol;
	ofoDevise *devise;
	GList *dataset;
	const ofoSgbd *sgbd;

	sgbd = ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier ));

	result = ofo_sgbd_query_ex( sgbd,
			"SELECT DEV_CODE,DEV_LABEL,DEV_SYMBOL,DEV_DIGITS,"
			"	DEV_NOTES,DEV_MAJ_USER,DEV_MAJ_STAMP "
			"	FROM OFA_T_DEVISES" );

	dataset = NULL;

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		devise = ofo_devise_new();
		ofo_devise_set_code( devise, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_devise_set_label( devise, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_devise_set_symbol( devise, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_devise_set_digits( devise, icol->data ? atoi(( gchar * ) icol->data ) : DEV_DEFAULT_DIGITS );
		icol = icol->next;
		ofo_devise_set_notes( devise, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_devise_set_maj_user( devise, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_devise_set_maj_stamp( devise, my_utils_stamp_from_str(( gchar * ) icol->data ));

		dataset = g_list_prepend( dataset, devise );
	}

	ofo_sgbd_free_result( result );

	return( g_list_reverse( dataset ));
}

/**
 * ofo_devise_get_by_code:
 *
 * Returns: the searched currency, or %NULL.
 *
 * The returned object is owned by the #ofoDevise class, and should
 * not be unreffed by the caller.
 */
ofoDevise *
ofo_devise_get_by_code( const ofoDossier *dossier, const gchar *code )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( code && g_utf8_strlen( code, -1 ), NULL );

	OFO_BASE_SET_GLOBAL( st_global, dossier, devise );

	return( devise_find_by_code( st_global->dataset, code ));
}

static ofoDevise *
devise_find_by_code( GList *set, const gchar *code )
{
	GList *found;

	found = g_list_find_custom(
				set, code, ( GCompareFunc ) devise_cmp_by_code );
	if( found ){
		return( OFO_DEVISE( found->data ));
	}

	return( NULL );
}

/**
 * ofo_devise_new:
 */
ofoDevise *
ofo_devise_new( void )
{
	ofoDevise *devise;

	devise = g_object_new( OFO_TYPE_DEVISE, NULL );

	return( devise );
}

/**
 * ofo_devise_get_code:
 */
const gchar *
ofo_devise_get_code( const ofoDevise *devise )
{
	g_return_val_if_fail( OFO_IS_DEVISE( devise ), NULL );

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		return(( const gchar * ) devise->private->code );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_devise_get_label:
 */
const gchar *
ofo_devise_get_label( const ofoDevise *devise )
{
	g_return_val_if_fail( OFO_IS_DEVISE( devise ), NULL );

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		return(( const gchar * ) devise->private->label );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_devise_get_symbol:
 */
const gchar *
ofo_devise_get_symbol( const ofoDevise *devise )
{
	g_return_val_if_fail( OFO_IS_DEVISE( devise ), NULL );

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		return(( const gchar * ) devise->private->symbol );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_devise_get_digits:
 */
gint
ofo_devise_get_digits( const ofoDevise *devise )
{
	g_return_val_if_fail( OFO_IS_DEVISE( devise ), 0 );

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		return( devise->private->digits );
	}

	g_assert_not_reached();
	return( 0 );
}

/**
 * ofo_devise_get_notes:
 */
const gchar *
ofo_devise_get_notes( const ofoDevise *devise )
{
	g_return_val_if_fail( OFO_IS_DEVISE( devise ), NULL );

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		return(( const gchar * ) devise->private->notes );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_devise_get_maj_user:
 */
const gchar *
ofo_devise_get_maj_user( const ofoDevise *devise )
{
	g_return_val_if_fail( OFO_IS_DEVISE( devise ), NULL );

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		return(( const gchar * ) devise->private->maj_user );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_devise_get_maj_stamp:
 */
const GTimeVal *
ofo_devise_get_maj_stamp( const ofoDevise *devise )
{
	g_return_val_if_fail( OFO_IS_DEVISE( devise ), NULL );

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &devise->private->maj_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_devise_is_deletable:
 *
 * A currency should not be deleted while it is referenced by an
 * account, a journal, an entry.
 */
gboolean
ofo_devise_is_deletable( const ofoDevise *devise )
{
	ofoDossier *dossier;
	const gchar *dev_code;

	g_return_val_if_fail( OFO_IS_DEVISE( devise ), FALSE );

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		dossier = OFO_DOSSIER( st_global->dossier );
		dev_code = ofo_devise_get_code( devise );

		return( !ofo_dossier_use_devise( dossier, dev_code ) &&
				!ofo_entry_use_devise( dossier, dev_code ) &&
				!ofo_journal_use_devise( dossier, dev_code ) &&
				!ofo_account_use_devise( dossier, dev_code ));
	}

	g_assert_not_reached();
	return( FALSE );
}

/**
 * ofo_devise_is_valid:
 *
 * Returns: %TRUE if the provided data makes the ofoDevise a valid
 * object.
 *
 * Note that this does NOT check for key duplicate.
 */
gboolean
ofo_devise_is_valid( const gchar *code, const gchar *label, const gchar *symbol, gint digits )
{
	return( code && g_utf8_strlen( code, -1 ) &&
			label && g_utf8_strlen( label, -1 ) &&
			symbol && g_utf8_strlen( symbol, -1 ) &&
			digits > 0 );
}

/**
 * ofo_devise_set_code:
 */
void
ofo_devise_set_code( ofoDevise *devise, const gchar *code )
{
	g_return_if_fail( OFO_IS_DEVISE( devise ));

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		g_free( devise->private->code );
		devise->private->code = g_strdup( code );
	}
}

/**
 * ofo_devise_set_label:
 */
void
ofo_devise_set_label( ofoDevise *devise, const gchar *label )
{
	g_return_if_fail( OFO_IS_DEVISE( devise ));

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		g_free( devise->private->label );
		devise->private->label = g_strdup( label );
	}
}

/**
 * ofo_devise_set_symbol:
 */
void
ofo_devise_set_symbol( ofoDevise *devise, const gchar *symbol )
{
	g_return_if_fail( OFO_IS_DEVISE( devise ));

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		g_free( devise->private->symbol );
		devise->private->symbol = g_strdup( symbol );
	}
}

/**
 * ofo_devise_set_digits:
 */
void
ofo_devise_set_digits( ofoDevise *devise, gint digits )
{
	g_return_if_fail( OFO_IS_DEVISE( devise ));

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		devise->private->digits = digits;
	}
}

/**
 * ofo_devise_set_notes:
 */
void
ofo_devise_set_notes( ofoDevise *devise, const gchar *notes )
{
	g_return_if_fail( OFO_IS_DEVISE( devise ));

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		g_free( devise->private->notes );
		devise->private->notes = g_strdup( notes );
	}
}

/**
 * ofo_devise_set_maj_user:
 */
void
ofo_devise_set_maj_user( ofoDevise *devise, const gchar *user )
{
	g_return_if_fail( OFO_IS_DEVISE( devise ));

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		g_free( devise->private->maj_user );
		devise->private->maj_user = g_strdup( user );
	}
}

/**
 * ofo_devise_set_maj_stamp:
 */
void
ofo_devise_set_maj_stamp( ofoDevise *devise, const GTimeVal *stamp )
{
	g_return_if_fail( OFO_IS_DEVISE( devise ));

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		memcpy( &devise->private->maj_stamp, stamp, sizeof( GTimeVal ));
	}
}

/**
 * ofo_devise_insert:
 */
gboolean
ofo_devise_insert( ofoDevise *devise )
{
	static const gchar *thisfn = "ofo_devise_insert";

	g_return_val_if_fail( OFO_IS_DEVISE( devise ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		g_debug( "%s: devise=%p", thisfn, ( void * ) devise );

		if( devise_do_insert(
					devise,
					ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )),
					ofo_dossier_get_user( OFO_DOSSIER( st_global->dossier )))){

			OFO_BASE_ADD_TO_DATASET( st_global, devise );

			return( TRUE );
		}
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
devise_do_insert( ofoDevise *devise, const ofoSgbd *sgbd, const gchar *user )
{
	return( devise_insert_main( devise, sgbd, user ));
}

static gboolean
devise_insert_main( ofoDevise *devise, const ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes, *stamp;
	const gchar *symbol;
	gboolean ok;

	ok = FALSE;
	label = my_utils_quote( ofo_devise_get_label( devise ));
	symbol = ofo_devise_get_symbol( devise );
	notes = my_utils_quote( ofo_devise_get_notes( devise ));
	stamp = my_utils_timestamp();

	query = g_string_new( "" );
	g_string_append_printf( query,
			"INSERT INTO OFA_T_DEVISES "
			"	(DEV_CODE,DEV_LABEL,DEV_SYMBOL,DEV_DIGITS,"
			"	DEV_NOTES,DEV_MAJ_USER,DEV_MAJ_STAMP)"
			"	VALUES ('%s','%s','%s',%d,",
			ofo_devise_get_code( devise ),
			label,
			symbol,
			ofo_devise_get_digits( devise ));

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
			"'%s','%s')",
			user, stamp );

	if( ofo_sgbd_query( sgbd, query->str )){

		ofo_devise_set_maj_user( devise, user );
		ofo_devise_set_maj_stamp( devise, my_utils_stamp_from_str( stamp ));
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( label );
	g_free( notes );
	g_free( stamp );

	return( ok );
}

/**
 * ofo_devise_update:
 */
gboolean
ofo_devise_update( ofoDevise *devise, const gchar *prev_code )
{
	static const gchar *thisfn = "ofo_devise_update";

	g_return_val_if_fail( OFO_IS_DEVISE( devise ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		g_debug( "%s: devise=%p, prev_code=%s", thisfn, ( void * ) devise, prev_code );

		if( devise_do_update(
					devise,
					prev_code,
					ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )),
					ofo_dossier_get_user( OFO_DOSSIER( st_global->dossier )))){

			OFO_BASE_UPDATE_DATASET( st_global, devise, prev_code );

			return( TRUE );
		}
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
devise_do_update( ofoDevise *devise, const gchar *prev_code, const ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes, *stamp;
	gboolean ok;

	ok = FALSE;
	label = my_utils_quote( ofo_devise_get_label( devise ));
	notes = my_utils_quote( ofo_devise_get_notes( devise ));
	stamp = my_utils_timestamp();

	query = g_string_new( "UPDATE OFA_T_DEVISES SET " );

	g_string_append_printf( query,
			"	DEV_CODE='%s',DEV_LABEL='%s',DEV_SYMBOL='%s',DEV_DIGITS=%d,",
			ofo_devise_get_code( devise ),
			label,
			ofo_devise_get_symbol( devise ),
			ofo_devise_get_digits( devise ));

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "DEV_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "DEV_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	DEV_MAJ_USER='%s',DEV_MAJ_STAMP='%s'"
			"	WHERE DEV_CODE='%s'", user, stamp, prev_code );

	if( ofo_sgbd_query( sgbd, query->str )){

		ofo_devise_set_maj_user( devise, user );
		ofo_devise_set_maj_stamp( devise, my_utils_stamp_from_str( stamp ));
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( label );
	g_free( notes );
	g_free( stamp );

	return( ok );
}

/**
 * ofo_devise_delete:
 */
gboolean
ofo_devise_delete( ofoDevise *devise )
{
	static const gchar *thisfn = "ofo_devise_delete";

	g_return_val_if_fail( OFO_IS_DEVISE( devise ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );
	g_return_val_if_fail( ofo_devise_is_deletable( devise ), FALSE );

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		g_debug( "%s: devise=%p", thisfn, ( void * ) devise );

		if( devise_do_delete(
					devise,
					ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )))){

			OFO_BASE_REMOVE_FROM_DATASET( st_global, devise );

			return( TRUE );
		}
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
devise_do_delete( ofoDevise *devise, const ofoSgbd *sgbd )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_DEVISES"
			"	WHERE DEV_CODE='%s'",
					ofo_devise_get_code( devise ));

	ok = ofo_sgbd_query( sgbd, query );

	g_free( query );

	return( ok );
}

static gint
devise_cmp_by_code( const ofoDevise *a, const gchar *code )
{
	return( g_utf8_collate( ofo_devise_get_code( a ), code ));
}

/**
 * ofo_devise_get_csv:
 */
GSList *
ofo_devise_get_csv( const ofoDossier *dossier )
{
	GList *set;
	GSList *lines;
	gchar *str, *stamp;
	ofoDevise *devise;
	const gchar *notes, *muser;

	OFO_BASE_SET_GLOBAL( st_global, dossier, devise );

	lines = NULL;

	str = g_strdup_printf( "Code;Label;Symbol;Digits;Notes;MajUser;MajStamp" );
	lines = g_slist_prepend( lines, str );

	for( set=st_global->dataset ; set ; set=set->next ){
		devise = OFO_DEVISE( set->data );

		notes = ofo_devise_get_notes( devise );
		muser = ofo_devise_get_maj_user( devise );
		stamp = my_utils_str_from_stamp( ofo_devise_get_maj_stamp( devise ));

		str = g_strdup_printf( "%s;%s;%s;%d;%s;%s;%s",
				ofo_devise_get_code( devise ),
				ofo_devise_get_label( devise ),
				ofo_devise_get_symbol( devise ),
				ofo_devise_get_digits( devise ),
				notes ? notes : "",
				muser ? muser : "",
				muser ? stamp : "" );

		g_free( stamp );

		lines = g_slist_prepend( lines, str );
	}

	return( g_slist_reverse( lines ));
}

/**
 * ofo_devise_import_csv:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - devise code iso 3a
 * - label
 * - symbol
 * - digits
 * - notes (opt)
 *
 * Replace the whole table with the provided datas.
 */
void
ofo_devise_import_csv( const ofoDossier *dossier, GSList *lines, gboolean with_header )
{
	static const gchar *thisfn = "ofo_devise_import_csv";
	ofoDevise *devise;
	GSList *ili, *ico;
	GList *new_set, *ise;
	gint count;
	gint errors;
	const gchar *str;

	g_debug( "%s: dossier=%p, lines=%p (count=%d), with_header=%s",
			thisfn,
			( void * ) dossier,
			( void * ) lines, g_slist_length( lines ),
			with_header ? "True":"False" );

	OFO_BASE_SET_GLOBAL( st_global, dossier, devise );

	new_set = NULL;
	count = 0;
	errors = 0;

	for( ili=lines ; ili ; ili=ili->next ){
		count += 1;
		if( !( count == 1 && with_header )){
			devise = ofo_devise_new();
			ico=ili->data;

			/* devise code */
			str = ( const gchar * ) ico->data;
			if( !str || !g_utf8_strlen( str, -1 )){
				g_warning( "%s: (line %d) empty code", thisfn, count );
				errors += 1;
				continue;
			}
			ofo_devise_set_code( devise, str );

			/* devise label */
			ico = ico->next;
			str = ( const gchar * ) ico->data;
			if( !str || !g_utf8_strlen( str, -1 )){
				g_warning( "%s: (line %d) empty label", thisfn, count );
				errors += 1;
				continue;
			}
			ofo_devise_set_label( devise, str );

			/* devise symbol */
			ico = ico->next;
			str = ( const gchar * ) ico->data;
			if( !str || !g_utf8_strlen( str, -1 )){
				g_warning( "%s: (line %d) empty symbol", thisfn, count );
				errors += 1;
				continue;
			}
			ofo_devise_set_symbol( devise, str );

			/* devise digits */
			ico = ico->next;
			str = ( const gchar * ) ico->data;
			ofo_devise_set_digits( devise,
					( str && g_utf8_strlen( str, -1 ) ? atoi( str ) : DEV_DEFAULT_DIGITS ));

			/* notes
			 * we are tolerant on the last field... */
			ico = ico->next;
			if( ico ){
				str = ( const gchar * ) ico->data;
				if( str && g_utf8_strlen( str, -1 )){
					ofo_devise_set_notes( devise, str );
				}
			} else {
				continue;
			}

			new_set = g_list_prepend( new_set, devise );
		}
	}

	if( !errors ){
		st_global->send_signal_new = FALSE;

		devise_do_drop_content( ofo_dossier_get_sgbd( dossier ));

		for( ise=new_set ; ise ; ise=ise->next ){
			devise_do_insert(
					OFO_DEVISE( ise->data ),
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ));
		}

		g_list_free( new_set );

		if( st_global ){
			g_list_free_full( st_global->dataset, ( GDestroyNotify ) g_object_unref );
			st_global->dataset = NULL;
		}
		g_signal_emit_by_name( G_OBJECT( dossier ), OFA_SIGNAL_RELOAD_DATASET, OFO_TYPE_DEVISE );

		st_global->send_signal_new = TRUE;
	}
}

static gboolean
devise_do_drop_content( const ofoSgbd *sgbd )
{
	return( ofo_sgbd_query( sgbd, "DELETE FROM OFA_T_DEVISES" ));
}
