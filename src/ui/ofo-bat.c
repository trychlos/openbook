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
#include "ui/ofo-dossier.h"
#include "ui/ofo-bat.h"
#include "ui/ofo-sgbd.h"

/* priv instance data
 */
struct _ofoBatPrivate {

	/* sgbd data
	 */
	gint       id;						/* bat (imported file) id */
	gchar     *uri;
	gchar     *format;
	gint       count;
	GDate      begin;
	GDate      end;
	gchar     *rib;
	gchar     *currency;
	gdouble    solde;
	gchar     *notes;
	gchar     *maj_user;
	GTimeVal   maj_stamp;
};

G_DEFINE_TYPE( ofoBat, ofo_bat, OFO_TYPE_BASE )

OFO_BASE_DEFINE_GLOBAL( st_global, bat )

static gboolean st_connected = FALSE;

static void        init_global_handlers( const ofoDossier *dossier );
static GList      *bat_load_dataset( void );
static gboolean    bat_do_insert( ofoBat *bat, const ofoSgbd *sgbd, const gchar *user );
static gboolean    bat_insert_main( ofoBat *bat, const ofoSgbd *sgbd, const gchar *user );
static gboolean    bat_get_back_id( ofoBat *bat, const ofoSgbd *sgbd );
static gint        bat_cmp_by_id( gint a, gint b );
static gint        bat_cmp_by_ptr( const ofoBat *a, const ofoBat *b );

static void
ofo_bat_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_bat_finalize";
	ofoBatPrivate *priv;

	priv = OFO_BAT( instance )->private;

	g_debug( "%s: instance=%p (%s): %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), priv->uri );

	/* free data members here */
	g_free( priv->uri );
	g_free( priv->format );
	g_free( priv->rib );
	g_free( priv->currency );
	g_free( priv->notes );
	g_free( priv->maj_user );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_bat_parent_class )->finalize( instance );
}

static void
ofo_bat_dispose( GObject *instance )
{
	g_return_if_fail( OFO_IS_BAT( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_bat_parent_class )->dispose( instance );
}

static void
ofo_bat_init( ofoBat *self )
{
	static const gchar *thisfn = "ofo_bat_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofoBatPrivate, 1 );

	self->private->id = OFO_BASE_UNSET_ID;
	g_date_clear( &self->private->begin, 1 );
	g_date_clear( &self->private->end, 1 );
}

static void
ofo_bat_class_init( ofoBatClass *klass )
{
	static const gchar *thisfn = "ofo_bat_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	g_type_class_add_private( klass, sizeof( ofoBatPrivate ));

	G_OBJECT_CLASS( klass )->dispose = ofo_bat_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_bat_finalize;
}

static void
init_global_handlers( const ofoDossier *dossier )
{
	OFO_BASE_SET_GLOBAL( st_global, dossier, bat );

	if( !st_connected ){
		/*g_signal_connect( G_OBJECT( dossier ),
					OFA_SIGNAL_NEW_ENTRY, G_CALLBACK( on_new_entry ), NULL );*/
		st_connected = TRUE;
	}
}

/**
 * ofo_bat_new:
 */
ofoBat *
ofo_bat_new( void )
{
	ofoBat *bat;

	bat = g_object_new( OFO_TYPE_BAT, NULL );

	return( bat );
}

/**
 * ofo_bat_get_dataset:
 * @dossier: the currently opened #ofoDossier dossier.
 *
 * Returns: %NULL.
 */
GList *
ofo_bat_get_dataset( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_bat_get_dataset";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	OFO_BASE_SET_GLOBAL( st_global, dossier, bat );

	return( st_global->dataset );
}

static GList *
bat_load_dataset( void )
{
	return( NULL );
}

/**
 * ofo_bat_get_id:
 */
gint
ofo_bat_get_id( const ofoBat *bat )
{
	g_return_val_if_fail( OFO_IS_BAT( bat ), OFO_BASE_UNSET_ID );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return( bat->private->id );
	}

	g_assert_not_reached();
	return( OFO_BASE_UNSET_ID );
}

/**
 * ofo_bat_get_uri:
 */
const gchar *
ofo_bat_get_uri( const ofoBat *bat )
{
	g_return_val_if_fail( OFO_IS_BAT( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const gchar * ) bat->private->uri );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_get_format:
 */
const gchar *
ofo_bat_get_format( const ofoBat *bat )
{
	g_return_val_if_fail( OFO_IS_BAT( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const gchar * ) bat->private->format );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_get_count:
 */
gint
ofo_bat_get_count( const ofoBat *bat )
{
	g_return_val_if_fail( OFO_IS_BAT( bat ), -1 );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return( bat->private->count );
	}

	g_assert_not_reached();
	return( -1 );
}

/**
 * ofo_bat_get_begin:
 */
const GDate *
ofo_bat_get_begin( const ofoBat *bat )
{
	g_return_val_if_fail( OFO_IS_BAT( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const GDate * ) &bat->private->begin );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_get_end:
 */
const GDate *
ofo_bat_get_end( const ofoBat *bat )
{
	g_return_val_if_fail( OFO_IS_BAT( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const GDate * ) &bat->private->end );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_get_rib:
 */
const gchar *
ofo_bat_get_rib( const ofoBat *bat )
{
	g_return_val_if_fail( OFO_IS_BAT( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const gchar * ) bat->private->rib );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_get_currency:
 */
const gchar *
ofo_bat_get_currency( const ofoBat *bat )
{
	g_return_val_if_fail( OFO_IS_BAT( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const gchar * ) bat->private->currency );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_get_solde:
 */
gdouble
ofo_bat_get_solde( const ofoBat *bat )
{
	g_return_val_if_fail( OFO_IS_BAT( bat ), 0 );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return( bat->private->solde );
	}

	g_assert_not_reached();
	return( 0 );
}

/**
 * ofo_bat_get_notes:
 */
const gchar *
ofo_bat_get_notes( const ofoBat *bat )
{
	g_return_val_if_fail( OFO_IS_BAT( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const gchar * ) bat->private->notes );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_get_maj_user:
 */
const gchar *
ofo_bat_get_maj_user( const ofoBat *bat )
{
	g_return_val_if_fail( OFO_IS_BAT( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const gchar * ) bat->private->maj_user );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_get_maj_stamp:
 */
const GTimeVal *
ofo_bat_get_maj_stamp( const ofoBat *bat )
{
	g_return_val_if_fail( OFO_IS_BAT( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &bat->private->maj_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_set_id:
 */
void
ofo_bat_set_id( ofoBat *bat, gint id )
{
	g_return_if_fail( OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		bat->private->id = id;
	}
}

/**
 * ofo_bat_set_uri:
 */
void
ofo_bat_set_uri( ofoBat *bat, const gchar *uri )
{
	g_return_if_fail( OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_free( bat->private->uri );
		bat->private->uri = g_strdup( uri );
	}
}

/**
 * ofo_bat_set_format:
 */
void
ofo_bat_set_format( ofoBat *bat, const gchar *format )
{
	g_return_if_fail( OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_free( bat->private->format );
		bat->private->format = g_strdup( format );
	}
}

/**
 * ofo_bat_set_count:
 */
void
ofo_bat_set_count( ofoBat *bat, gint count )
{
	g_return_if_fail( OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		bat->private->count = count;
	}
}

/**
 * ofo_bat_set_begin:
 */
void
ofo_bat_set_begin( ofoBat *bat, const GDate *date )
{
	g_return_if_fail( OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		if( !date ){
			g_date_clear( &bat->private->begin, 1 );

		} else {
			memcpy( &bat->private->begin, date, sizeof( GDate ));
		}
	}
}

/**
 * ofo_bat_set_end:
 */
void
ofo_bat_set_end( ofoBat *bat, const GDate *date )
{
	g_return_if_fail( OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		if( !date ){
			g_date_clear( &bat->private->end, 1 );

		} else {
			memcpy( &bat->private->end, date, sizeof( GDate ));
		}
	}
}

/**
 * ofo_bat_set_rib:
 */
void
ofo_bat_set_rib( ofoBat *bat, const gchar *rib )
{
	g_return_if_fail( OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_free( bat->private->rib );
		bat->private->rib = g_strdup( rib );
	}
}

/**
 * ofo_bat_set_currency:
 */
void
ofo_bat_set_currency( ofoBat *bat, const gchar *currency )
{
	g_return_if_fail( OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_free( bat->private->currency );
		bat->private->currency = g_strdup( currency );
	}
}

/**
 * ofo_bat_set_solde:
 */
void
ofo_bat_set_solde( ofoBat *bat, gdouble solde )
{
	g_return_if_fail( OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		bat->private->solde = solde;
	}
}

/**
 * ofo_bat_set_notes:
 */
void
ofo_bat_set_notes( ofoBat *bat, const gchar *notes )
{
	g_return_if_fail( OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_free( bat->private->notes );
		bat->private->notes = g_strdup( notes );
	}
}

/**
 * ofo_bat_set_maj_user:
 */
void
ofo_bat_set_maj_user( ofoBat *bat, const gchar *maj_user )
{
	g_return_if_fail( OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_free( bat->private->maj_user );
		bat->private->maj_user = g_strdup( maj_user );
	}
}

/**
 * ofo_bat_set_maj_stamp:
 */
void
ofo_bat_set_maj_stamp( ofoBat *bat, const GTimeVal *maj_stamp )
{
	g_return_if_fail( OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		memcpy( &bat->private->maj_stamp, maj_stamp, sizeof( GTimeVal ));
	}
}

/**
 * ofo_bat_insert:
 *
 *
 */
gboolean
ofo_bat_insert( ofoBat *bat, const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_bat_insert";

	g_return_val_if_fail( OFO_IS_BAT( bat ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_debug( "%s: bat=%p, dossier=%p",
				thisfn, ( void * ) bat, ( void * ) dossier );

		init_global_handlers( dossier );

		if( bat_do_insert(
					bat,
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFO_BASE_ADD_TO_DATASET( st_global, bat );
			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
bat_do_insert( ofoBat *bat, const ofoSgbd *sgbd, const gchar *user )
{
	return( bat_insert_main( bat, sgbd, user ) &&
			bat_get_back_id( bat, sgbd ));
}

static gboolean
bat_insert_main( ofoBat *bat, const ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *str;
	const GDate *begin, *end;
	gboolean ok;
	gchar *stamp;

	ok = FALSE;
	stamp = my_utils_timestamp();

	query = g_string_new( "INSERT INTO OFA_T_BAT" );

	g_string_append_printf( query,
			"	(BAT_URI,BAT_FORMAT,BAT_COUNT,BAT_BEGIN,BAT_END,"
			"	 BAT_RIB,BAT_DEVISE,BAT_SOLDE,"
			"	 BAT_NOTES,BAT_MAJ_USER,BAT_MAJ_STAMP) VALUES ('%s',",
					ofo_bat_get_uri( bat ));

	str = my_utils_quote( ofo_bat_get_format( bat ));
	if( str && g_utf8_strlen( str, -1 )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( str );

	g_string_append_printf( query, "%d,", ofo_bat_get_count( bat ));

	begin = ofo_bat_get_begin( bat );
	if( g_date_valid( begin )){
		str = my_utils_sql_from_date( begin );
		g_string_append_printf( query, "'%s',", str );
		g_free( str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	end = ofo_bat_get_end( bat );
	if( g_date_valid( end )){
		str = my_utils_sql_from_date( end );
		g_string_append_printf( query, "'%s',", str );
		g_free( str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	str = ( gchar * ) ofo_bat_get_rib( bat );
	if( str && g_utf8_strlen( str, -1 )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	str = ( gchar * ) ofo_bat_get_currency( bat );
	if( str && g_utf8_strlen( str, -1 )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	str = my_utils_sql_from_double( ofo_bat_get_solde( bat ));
	g_string_append_printf( query, "%s,", str );
	g_free( str );

	str = my_utils_quote( ofo_bat_get_notes( bat ));
	if( str && g_utf8_strlen( str, -1 )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( str );

	g_string_append_printf( query, "'%s','%s')", user, stamp );

	if( ofo_sgbd_query( sgbd, query->str )){

		ofo_bat_set_maj_user( bat, user );
		ofo_bat_set_maj_stamp( bat, my_utils_stamp_from_str( stamp ));
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( stamp );

	return( ok );
}

static gboolean
bat_get_back_id( ofoBat *bat, const ofoSgbd *sgbd )
{
	gboolean ok;
	GSList *result, *icol;

	ok = FALSE;
	result = ofo_sgbd_query_ex( sgbd, "SELECT LAST_INSERT_ID()" );

	if( result ){
		icol = ( GSList * ) result->data;
		ofo_bat_set_id( bat, atoi(( gchar * ) icol->data ));
		ofo_sgbd_free_result( result );
		ok = TRUE;
	}

	return( ok );
}

static gint
bat_cmp_by_id( gint a, gint b )
{
	return( a < b ? -1 : ( a > b ? 1 : 0 ));
}

static gint
bat_cmp_by_ptr( const ofoBat *a, const ofoBat *b )
{
	return( bat_cmp_by_id( ofo_bat_get_id( a ), ofo_bat_get_id( b )));
}
