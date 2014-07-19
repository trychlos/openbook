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

#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-dossier.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-sgbd.h"

#include "core/my-date.h"
#include "core/my-utils.h"

/* priv instance data
 */
struct _ofoBatLinePrivate {

	/* sgbd data
	 */
	gint       bat_id;						/* bat (imported file) id */
	gint       id;							/* line id */
	GDate      valeur;
	GDate      ope;
	gchar     *ref;
	gchar     *label;
	gchar     *currency;
	gdouble    montant;
	gint       ecr;
	gchar     *maj_user;
	GTimeVal   maj_stamp;
};

G_DEFINE_TYPE( ofoBatLine, ofo_bat_line, OFO_TYPE_BASE )

static GList      *bat_line_load_dataset( gint bat_id, const ofoSgbd *sgbd );
static gboolean    bat_line_do_insert( ofoBatLine *bat, const ofoSgbd *sgbd, const gchar *user );
static gboolean    bat_line_insert_main( ofoBatLine *bat, const ofoSgbd *sgbd, const gchar *user );
static gboolean    bat_line_get_back_id( ofoBatLine *bat, const ofoSgbd *sgbd );
static gboolean    bat_line_do_update( ofoBatLine *bat, const ofoSgbd *sgbd, const gchar *user );

static void
ofo_bat_line_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_bat_line_finalize";
	ofoBatLinePrivate *priv;

	priv = OFO_BAT_LINE( instance )->private;

	g_debug( "%s: instance=%p (%s): %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), priv->label );

	g_return_if_fail( instance && OFO_IS_BAT_LINE( instance ));

	/* free data members here */
	g_free( priv->ref );
	g_free( priv->label );
	g_free( priv->currency );
	g_free( priv->maj_user );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_bat_line_parent_class )->finalize( instance );
}

static void
ofo_bat_line_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_BAT_LINE( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_bat_line_parent_class )->dispose( instance );
}

static void
ofo_bat_line_init( ofoBatLine *self )
{
	static const gchar *thisfn = "ofo_bat_line_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofoBatLinePrivate, 1 );

	self->private->bat_id = OFO_BASE_UNSET_ID;
	self->private->id = OFO_BASE_UNSET_ID;
	g_date_clear( &self->private->valeur, 1 );
	g_date_clear( &self->private->ope, 1 );
}

static void
ofo_bat_line_class_init( ofoBatLineClass *klass )
{
	static const gchar *thisfn = "ofo_bat_line_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	g_type_class_add_private( klass, sizeof( ofoBatLinePrivate ));

	G_OBJECT_CLASS( klass )->dispose = ofo_bat_line_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_bat_line_finalize;
}

/**
 * ofo_bat_line_new:
 */
ofoBatLine *
ofo_bat_line_new( gint bat_id )
{
	ofoBatLine *bat;

	bat = g_object_new( OFO_TYPE_BAT_LINE, NULL );
	bat->private->bat_id = bat_id;

	return( bat );
}

/**
 * ofo_bat_line_get_dataset:
 * @dossier: the currently opened #ofoDossier dossier.
 *
 * Returns: the list of lines imported in the specified bank account
 * transaction list.
 */
GList *
ofo_bat_line_get_dataset( const ofoDossier *dossier, gint bat_id )
{
	static const gchar *thisfn = "ofo_bat_line_get_dataset";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	return( bat_line_load_dataset( bat_id, ofo_dossier_get_sgbd( dossier )));
}

static GList *
bat_line_load_dataset( gint bat_id, const ofoSgbd *sgbd)
{
	GString *query;
	GSList *result, *irow, *icol;
	GList *dataset;
	ofoBatLine *line;
	GDate date;
	GTimeVal timeval;

	dataset = NULL;

	query = g_string_new(
					"SELECT BAT_LINE_ID,BAT_LINE_VALEUR,BAT_LINE_OPE,"
					"	BAT_LINE_LABEL,BAT_LINE_REF,BAT_LINE_DEVISE,"
					"	BAT_LINE_MONTANT,"
					"	BAT_LINE_ECR,BAT_LINE_MAJ_USER,BAT_LINE_MAJ_STAMP "
					"	FROM OFA_T_BAT_LINES " );

	g_string_append_printf( query, "WHERE BAT_ID=%d ", bat_id );

	query = g_string_append( query, "ORDER BY BAT_LINE_VALEUR ASC" );

	result = ofo_sgbd_query_ex( sgbd, query->str );
	g_string_free( query, TRUE );

	if( result ){
		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			line = ofo_bat_line_new( bat_id );
			ofo_bat_line_set_id( line, atoi(( gchar * ) icol->data ));
			icol = icol->next;
			my_date_set_from_sql( &date, ( const gchar * ) icol->data );
			ofo_bat_line_set_valeur( line, &date );
			icol = icol->next;
			if( icol->data ){
				my_date_set_from_sql( &date, ( const gchar * ) icol->data );
				ofo_bat_line_set_ope( line, &date );
			}
			icol = icol->next;
			ofo_bat_line_set_label( line, ( gchar * ) icol->data );
			icol = icol->next;
			if( icol->data ){
				ofo_bat_line_set_ref( line, ( gchar * ) icol->data );
			}
			icol = icol->next;
			if( icol->data ){
				ofo_bat_line_set_currency( line, ( gchar * ) icol->data );
			}
			icol = icol->next;
			ofo_bat_line_set_montant( line,
					my_utils_double_set_from_sql(( const gchar * ) icol->data ));
			icol = icol->next;
			if( icol->data ){
				ofo_bat_line_set_ecr( line, atoi(( gchar * ) icol->data ));
			}
			icol = icol->next;
			if( icol->data ){
				ofo_bat_line_set_maj_user( line, ( gchar * ) icol->data );
			}
			icol = icol->next;
			if( icol->data ){
				ofo_bat_line_set_maj_stamp( line,
						my_utils_stamp_set_from_sql( &timeval, ( const gchar * ) icol->data ));
			}

			dataset = g_list_prepend( dataset, line );
		}

		ofo_sgbd_free_result( result );
	}

	return( g_list_reverse( dataset ));
}

/**
 * ofo_bat_line_get_id:
 */
gint
ofo_bat_line_get_id( const ofoBatLine *bat )
{
	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), OFO_BASE_UNSET_ID );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return( bat->private->id );
	}

	g_assert_not_reached();
	return( OFO_BASE_UNSET_ID );
}

/**
 * ofo_bat_line_get_bat_id:
 */
gint
ofo_bat_line_get_bat_id( const ofoBatLine *bat )
{
	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), OFO_BASE_UNSET_ID );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return( bat->private->bat_id );
	}

	g_assert_not_reached();
	return( OFO_BASE_UNSET_ID );
}

/**
 * ofo_bat_line_get_valeur:
 */
const GDate *
ofo_bat_line_get_valeur( const ofoBatLine *bat )
{
	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const GDate * ) &bat->private->valeur );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_line_get_ope:
 */
const GDate *
ofo_bat_line_get_ope( const ofoBatLine *bat )
{
	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const GDate * ) &bat->private->ope );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_line_get_ref:
 */
const gchar *
ofo_bat_line_get_ref( const ofoBatLine *bat )
{
	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const gchar * ) bat->private->ref );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_line_get_label:
 */
const gchar *
ofo_bat_line_get_label( const ofoBatLine *bat )
{
	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const gchar * ) bat->private->label );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_line_get_currency:
 */
const gchar *
ofo_bat_line_get_currency( const ofoBatLine *bat )
{
	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const gchar * ) bat->private->currency );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_line_get_montant:
 */
gdouble
ofo_bat_line_get_montant( const ofoBatLine *bat )
{
	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), 0 );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return( bat->private->montant );
	}

	g_assert_not_reached();
	return( 0 );
}

/**
 * ofo_bat_line_get_ecr:
 */
gint
ofo_bat_line_get_ecr( const ofoBatLine *bat )
{
	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), 0 );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return( bat->private->ecr );
	}

	g_assert_not_reached();
	return( 0 );
}

/**
 * ofo_bat_line_get_maj_user:
 */
const gchar *
ofo_bat_line_get_maj_user( const ofoBatLine *bat )
{
	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const gchar * ) bat->private->maj_user );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_line_get_maj_stamp:
 */
const GTimeVal *
ofo_bat_line_get_maj_stamp( const ofoBatLine *bat )
{
	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &bat->private->maj_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_line_set_id:
 */
void
ofo_bat_line_set_id( ofoBatLine *bat, gint id )
{
	g_return_if_fail( OFO_IS_BAT_LINE( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		bat->private->id = id;
	}
}

/**
 * ofo_bat_line_set_valeur:
 */
void
ofo_bat_line_set_valeur( ofoBatLine *bat, const GDate *date )
{
	g_return_if_fail( OFO_IS_BAT_LINE( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		if( !date ){
			g_date_clear( &bat->private->valeur, 1 );

		} else {
			memcpy( &bat->private->valeur, date, sizeof( GDate ));
		}
	}
}

/**
 * ofo_bat_line_set_ope:
 */
void
ofo_bat_line_set_ope( ofoBatLine *bat, const GDate *date )
{
	g_return_if_fail( OFO_IS_BAT_LINE( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		if( !date ){
			g_date_clear( &bat->private->ope, 1 );

		} else {
			memcpy( &bat->private->ope, date, sizeof( GDate ));
		}
	}
}

/**
 * ofo_bat_line_set_ref:
 */
void
ofo_bat_line_set_ref( ofoBatLine *bat, const gchar *ref )
{
	g_return_if_fail( OFO_IS_BAT_LINE( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_free( bat->private->ref );
		bat->private->ref = g_strdup( ref );
	}
}

/**
 * ofo_bat_line_set_label:
 */
void
ofo_bat_line_set_label( ofoBatLine *bat, const gchar *label )
{
	g_return_if_fail( OFO_IS_BAT_LINE( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_free( bat->private->label );
		bat->private->label = g_strdup( label );
	}
}

/**
 * ofo_bat_line_set_currency:
 */
void
ofo_bat_line_set_currency( ofoBatLine *bat, const gchar *currency )
{
	g_return_if_fail( OFO_IS_BAT_LINE( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_free( bat->private->currency );
		bat->private->currency = g_strdup( currency );
	}
}

/**
 * ofo_bat_line_set_montant:
 */
void
ofo_bat_line_set_montant( ofoBatLine *bat, gdouble montant )
{
	g_return_if_fail( OFO_IS_BAT_LINE( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		bat->private->montant = montant;
	}
}

/**
 * ofo_bat_line_set_ecr:
 */
void
ofo_bat_line_set_ecr( ofoBatLine *bat, gint number )
{
	g_return_if_fail( OFO_IS_BAT_LINE( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		bat->private->ecr = number;
	}
}

/**
 * ofo_bat_line_set_maj_user:
 */
void
ofo_bat_line_set_maj_user( ofoBatLine *bat, const gchar *maj_user )
{
	g_return_if_fail( OFO_IS_BAT_LINE( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_free( bat->private->maj_user );
		bat->private->maj_user = g_strdup( maj_user );
	}
}

/**
 * ofo_bat_line_set_maj_stamp:
 */
void
ofo_bat_line_set_maj_stamp( ofoBatLine *bat, const GTimeVal *maj_stamp )
{
	g_return_if_fail( OFO_IS_BAT_LINE( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		memcpy( &bat->private->maj_stamp, maj_stamp, sizeof( GTimeVal ));
	}
}

/**
 * ofo_bat_line_insert:
 *
 * When inserting a new BAT line, there has not yet been any
 *  reconciliation with an entry; this doesn't worth to try to insert
 *  these fields
 */
gboolean
ofo_bat_line_insert( ofoBatLine *bat_line, const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_bat_line_insert";

	g_return_val_if_fail( OFO_IS_BAT_LINE( bat_line ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( bat_line )->prot->dispose_has_run ){

		g_debug( "%s: bat=%p, dossier=%p",
				thisfn, ( void * ) bat_line, ( void * ) dossier );

		if( bat_line_do_insert(
					bat_line,
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ))){

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
bat_line_do_insert( ofoBatLine *bat, const ofoSgbd *sgbd, const gchar *user )
{
	return( bat_line_insert_main( bat, sgbd, user ) &&
			bat_line_get_back_id( bat, sgbd ));
}

static gboolean
bat_line_insert_main( ofoBatLine *bat, const ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *str;
	const GDate *ope;
	gboolean ok;

	query = g_string_new( "INSERT INTO OFA_T_BAT_LINES" );

	str = my_date_to_str( ofo_bat_line_get_valeur( bat ), MY_DATE_SQL );

	g_string_append_printf( query,
			"	(BAT_ID,BAT_LINE_VALEUR,BAT_LINE_OPE,BAT_LINE_REF,"
			"	 BAT_LINE_LABEL,BAT_LINE_DEVISE,BAT_LINE_MONTANT) "
			"	VALUES (%d,'%s',",
					ofo_bat_line_get_bat_id( bat ),
					str );
	g_free( str );

	ope = ofo_bat_line_get_ope( bat );
	if( g_date_valid( ope )){
		str = my_date_to_str( ope, MY_DATE_SQL );
		g_string_append_printf( query, "'%s',", str );
		g_free( str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	str = my_utils_quote( ofo_bat_line_get_ref( bat ));
	if( str && g_utf8_strlen( str, -1 )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( str );

	str = my_utils_quote( ofo_bat_line_get_label( bat ));
	if( str && g_utf8_strlen( str, -1 )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( str );

	str = ( gchar * ) ofo_bat_line_get_currency( bat );
	if( str && g_utf8_strlen( str, -1 )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	str = my_utils_sql_from_double( ofo_bat_line_get_montant( bat ));
	g_string_append_printf( query, "%s", str );
	g_free( str );

	query = g_string_append( query, ")" );

	ok = ofo_sgbd_query( sgbd, query->str );

	g_string_free( query, TRUE );

	return( ok );
}

static gboolean
bat_line_get_back_id( ofoBatLine *bat, const ofoSgbd *sgbd )
{
	gboolean ok;
	GSList *result, *icol;

	ok = FALSE;
	result = ofo_sgbd_query_ex( sgbd, "SELECT LAST_INSERT_ID()" );

	if( result ){
		icol = ( GSList * ) result->data;
		ofo_bat_line_set_id( bat, atoi(( gchar * ) icol->data ));
		ofo_sgbd_free_result( result );
		ok = TRUE;
	}

	return( ok );
}

/**
 * ofo_bat_line_update:
 *
 * Updating a BAT line means mainly updating the reconciliation properties
 */
gboolean
ofo_bat_line_update( ofoBatLine *bat_line, const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_bat_line_insert";

	g_return_val_if_fail( OFO_IS_BAT_LINE( bat_line ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( bat_line )->prot->dispose_has_run ){

		g_debug( "%s: bat=%p, dossier=%p",
				thisfn, ( void * ) bat_line, ( void * ) dossier );

		if( bat_line_do_update(
					bat_line,
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ))){

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
bat_line_do_update( ofoBatLine *bat, const ofoSgbd *sgbd, const gchar *user )
{
	gboolean ok;
	GString *query;
	gchar *stamp_str;
	GTimeVal stamp;
	gint ecr_number;

	my_utils_stamp_get_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	ecr_number = ofo_bat_line_get_ecr( bat );

	query = g_string_new( "UPDATE OFA_T_BAT_LINES SET " );

	if( ecr_number > 0 ){
		g_string_append_printf( query,
				"	BAT_LINE_ECR=%d,BAT_LINE_MAJ_USER='%s',BAT_LINE_MAJ_STAMP='%s' ",
					ecr_number, user, stamp_str );
	} else {
		query = g_string_append( query,
				"	BAT_LINE_ECR=NULL,BAT_LINE_MAJ_USER=NULL,BAT_LINE_MAJ_STAMP=0" );
	}

	g_string_append_printf( query,
			"	WHERE BAT_LINE_ID=%d", ofo_bat_line_get_id( bat ));

	ok = ofo_sgbd_query( sgbd, query->str );

	g_string_free( query, TRUE );
	g_free( stamp_str );

	return( ok );
}
