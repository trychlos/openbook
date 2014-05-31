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

#include "ui/my-utils.h"
#include "ui/ofo-base.h"
#include "ui/ofo-base-prot.h"
#include "ui/ofo-account.h"
#include "ui/ofo-devise.h"
#include "ui/ofo-dossier.h"
#include "ui/ofo-entry.h"
#include "ui/ofo-journal.h"
#include "ui/ofo-sgbd.h"

/* priv instance data
 */
struct _ofoDevisePrivate {

	/* sgbd data
	 */
	gint       id;
	gchar     *code;
	gchar     *label;
	gchar     *symbol;
	gchar     *notes;
	gchar     *maj_user;
	GTimeVal   maj_stamp;
};

G_DEFINE_TYPE( ofoDevise, ofo_devise, OFO_TYPE_BASE )

OFO_BASE_DEFINE_GLOBAL( st_global, devise )

static GList     *devise_load_dataset( void );
static ofoDevise *devise_find_by_code( GList *set, const gchar *code );
static ofoDevise *devise_find_by_id( GList *set, gint id );
static gint       devise_cmp_by_code( const ofoDevise *a, const gchar *code );
static gint       devise_cmp_by_id( const ofoDevise *a, gpointer pid );
static gboolean   devise_do_insert( ofoDevise *devise, const ofoSgbd *sgbd, const gchar *user );
static gboolean   devise_insert_main( ofoDevise *devise, const ofoSgbd *sgbd, const gchar *user );
static gboolean   devise_get_back_id( ofoDevise *devise, const ofoSgbd *sgbd );
static gboolean   devise_do_update( ofoDevise *devise, const ofoSgbd *sgbd, const gchar *user );
static gboolean   devise_do_delete( ofoDevise *devise, const ofoSgbd *sgbd );
static gint       devise_cmp_by_code( const ofoDevise *a, const gchar *code );
static gint       devise_cmp_by_ptr( const ofoDevise *a, const ofoDevise *b );

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

	self->private->id = OFO_BASE_UNSET_ID;
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
 *
 * Note: The list is returned (and maintained) sorted for debug
 * facility only. Any way, the display treeview (#ofoDevisesSet class)
 * makes use of a sortable model which doesn't care of the order of the
 * provided dataset.
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
			"SELECT DEV_ID,DEV_CODE,DEV_LABEL,DEV_SYMBOL,"
			"	DEV_NOTES,DEV_MAJ_USER,DEV_MAJ_STAMP "
			"	FROM OFA_T_DEVISES "
			"	ORDER BY DEV_CODE ASC" );

	dataset = NULL;

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		devise = ofo_devise_new();
		ofo_devise_set_id( devise, atoi(( gchar * ) icol->data ));
		icol = icol->next;
		ofo_devise_set_code( devise, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_devise_set_label( devise, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_devise_set_symbol( devise, ( gchar * ) icol->data );
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
 * ofo_devise_get_by_id:
 *
 * Returns: the searched currency, or %NULL.
 *
 * The returned object is owned by the #ofoDevise class, and should
 * not be unreffed by the caller.
 */
ofoDevise *
ofo_devise_get_by_id( const ofoDossier *dossier, gint id )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	OFO_BASE_SET_GLOBAL( st_global, dossier, devise );

	return( devise_find_by_id( st_global->dataset, id ));
}

static ofoDevise *
devise_find_by_id( GList *set, gint id )
{
	GList *found;

	found = g_list_find_custom(
				set, GINT_TO_POINTER( id ), ( GCompareFunc ) devise_cmp_by_id );
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
 * ofo_devise_get_id:
 */
gint
ofo_devise_get_id( const ofoDevise *devise )
{
	g_return_val_if_fail( OFO_IS_DEVISE( devise ), OFO_BASE_UNSET_ID );

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		return( devise->private->id );
	}

	g_assert_not_reached();
	return( OFO_BASE_UNSET_ID );
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

	g_return_val_if_fail( OFO_IS_DEVISE( devise ), FALSE );
	/* a devise whose internal identifier is not set is deletable,
	 * but this should never appear */
	g_return_val_if_fail( ofo_devise_get_id( devise ) > 0, TRUE );

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		dossier = OFO_DOSSIER( st_global->dossier );

		return( !ofo_dossier_use_devise( dossier, ofo_devise_get_id( devise )) &&
				!ofo_entry_use_devise( dossier, ofo_devise_get_id( devise )) &&
				!ofo_journal_use_devise( dossier, ofo_devise_get_id( devise )) &&
				!ofo_account_use_devise( dossier, ofo_devise_get_id( devise )));
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
ofo_devise_is_valid( const gchar *code, const gchar *label, const gchar *symbol )
{
	return( code && g_utf8_strlen( code, -1 ) &&
			label && g_utf8_strlen( label, -1 ) &&
			symbol && g_utf8_strlen( symbol, -1 ));
}

/**
 * ofo_devise_set_id:
 */
void
ofo_devise_set_id( ofoDevise *devise, gint id )
{
	g_return_if_fail( OFO_IS_DEVISE( devise ));

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		devise->private->id = id;
	}
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
ofo_devise_insert( ofoDevise *devise, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_devise_insert";

	g_return_val_if_fail( OFO_IS_DEVISE( devise ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		g_debug( "%s: devise=%p, dossier=%p",
				thisfn, ( void * ) devise, ( void * ) dossier );

		OFO_BASE_SET_GLOBAL( st_global, dossier, devise );

		if( devise_do_insert(
					devise,
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ))){

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
	return( devise_insert_main( devise, sgbd, user ) &&
			devise_get_back_id( devise, sgbd ));
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
			"	(DEV_CODE,DEV_LABEL,DEV_SYMBOL,"
			"	DEV_NOTES,DEV_MAJ_USER,DEV_MAJ_STAMP)"
			"	VALUES ('%s','%s','%s',",
			ofo_devise_get_code( devise ),
			label,
			symbol );

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

static gboolean
devise_get_back_id( ofoDevise *devise, const ofoSgbd *sgbd )
{
	gboolean ok;
	GSList *result, *icol;

	ok = FALSE ;
	result = ofo_sgbd_query_ex( sgbd, "SELECT LAST_INSERT_ID()" );

	if( result ){
		icol = ( GSList * ) result->data;
		ofo_devise_set_id( devise, atoi(( gchar * ) icol->data ));
		ofo_sgbd_free_result( result );
		ok = TRUE;
	}

	return( ok );
}

/**
 * ofo_devise_update:
 */
gboolean
ofo_devise_update( ofoDevise *devise, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_devise_update";

	g_return_val_if_fail( OFO_IS_DEVISE( devise ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		g_debug( "%s: devise=%p, dossier=%p",
				thisfn, ( void * ) devise, ( void * ) dossier );

		OFO_BASE_SET_GLOBAL( st_global, dossier, devise );

		if( devise_do_update(
					devise,
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFO_BASE_UPDATE_DATASET( st_global, devise );
			return( TRUE );
		}
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
devise_do_update( ofoDevise *devise, const ofoSgbd *sgbd, const gchar *user )
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
			"	DEV_CODE='%s',DEV_LABEL='%s',DEV_SYMBOL='%s',",
			ofo_devise_get_code( devise ),
			label,
			ofo_devise_get_symbol( devise ));

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "DEV_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "DEV_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	DEV_MAJ_USER='%s',DEV_MAJ_STAMP='%s'"
			"	WHERE DEV_ID=%d", user, stamp, ofo_devise_get_id( devise ));

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
ofo_devise_delete( ofoDevise *devise, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_devise_delete";

	g_return_val_if_fail( OFO_IS_DEVISE( devise ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( ofo_devise_is_deletable( devise ), FALSE );

	if( !OFO_BASE( devise )->prot->dispose_has_run ){

		g_debug( "%s: devise=%p, dossier=%p",
				thisfn, ( void * ) devise, ( void * ) dossier );

		OFO_BASE_SET_GLOBAL( st_global, dossier, devise );

		if( devise_do_delete(
					devise,
					ofo_dossier_get_sgbd( dossier ))){

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
			"	WHERE DEV_ID=%d",
					ofo_devise_get_id( devise ));

	ok = ofo_sgbd_query( sgbd, query );

	g_free( query );

	return( ok );
}

static gint
devise_cmp_by_code( const ofoDevise *a, const gchar *code )
{
	return( g_utf8_collate( ofo_devise_get_code( a ), code ));
}

static gint
devise_cmp_by_id( const ofoDevise *a, gpointer pid )
{
	gint aid, bid;

	aid = ofo_devise_get_id( a );
	bid = GPOINTER_TO_INT( pid );

	if( aid < bid ){
		return( -1 );
	}
	if( aid > bid ){
		return( 1 );
	}
	return( 0 );
}

static gint
devise_cmp_by_ptr( const ofoDevise *a, const ofoDevise *b )
{
	return( g_utf8_collate( ofo_devise_get_code( a ), ofo_devise_get_code( b )));
}
