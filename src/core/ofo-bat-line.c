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
 * Open Freelance Accounting is distributed in the hdope that it will be
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

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-dossier.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-sgbd.h"

/* priv instance data
 */
struct _ofoBatLinePrivate {

	/* sgbd data
	 */
	gint       bat_id;						/* bat (imported file) id */
	gint       id;							/* line id */
	GDate      deffect;
	GDate      dope;
	gchar     *ref;
	gchar     *label;
	gchar     *currency;
	gdouble    amount;
	gint       entry;
	gchar     *upd_user;
	GTimeVal   upd_stamp;
};

G_DEFINE_TYPE( ofoBatLine, ofo_bat_line, OFO_TYPE_BASE )

static GList      *bat_line_load_dataset( gint bat_id, const ofoSgbd *sgbd );
static void        bat_line_set_upd_user( ofoBatLine *bat, const gchar *upd_user );
static void        bat_line_set_upd_stamp( ofoBatLine *bat, const GTimeVal *upd_stamp );
static gboolean    bat_line_do_insert( ofoBatLine *bat, const ofoSgbd *sgbd, const gchar *user );
static gboolean    bat_line_insert_main( ofoBatLine *bat, const ofoSgbd *sgbd, const gchar *user );
static gboolean    bat_line_get_back_id( ofoBatLine *bat, const ofoSgbd *sgbd );
static gboolean    bat_line_do_update( ofoBatLine *bat, const ofoSgbd *sgbd, const gchar *user );

static void
bat_line_finalize( GObject *instance )
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
	g_free( priv->upd_user );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_bat_line_parent_class )->finalize( instance );
}

static void
bat_line_dispose( GObject *instance )
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
	my_date_clear( &self->private->deffect );
	my_date_clear( &self->private->dope );
}

static void
ofo_bat_line_class_init( ofoBatLineClass *klass )
{
	static const gchar *thisfn = "ofo_bat_line_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	g_type_class_add_private( klass, sizeof( ofoBatLinePrivate ));

	G_OBJECT_CLASS( klass )->dispose = bat_line_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_line_finalize;
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
 * @dossier: the currently dopened #ofoDossier dossier.
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
	GTimeVal timeval;

	dataset = NULL;

	query = g_string_new(
					"SELECT BAT_LINE_ID,BAT_LINE_DEFFECT,BAT_LINE_DOPE,"
					"	BAT_LINE_LABEL,BAT_LINE_REF,BAT_LINE_CURRENCY,"
					"	BAT_LINE_AMOUNT,BAT_LINE_ENTRY,"
					"	BAT_LINE_UPD_USER,BAT_LINE_UPD_STAMP "
					"	FROM OFA_T_BAT_LINES " );

	g_string_append_printf( query, "WHERE BAT_ID=%d ", bat_id );

	query = g_string_append( query, "ORDER BY BAT_LINE_VALEUR ASC" );

	result = ofo_sgbd_query_ex( sgbd, query->str, TRUE );
	g_string_free( query, TRUE );

	if( result ){
		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			line = ofo_bat_line_new( bat_id );
			ofo_bat_line_set_id( line, atoi(( gchar * ) icol->data ));
			icol = icol->next;
			my_date_set_from_sql( &line->private->deffect, ( const gchar * ) icol->data );
			icol = icol->next;
			if( icol->data ){
				my_date_set_from_sql( &line->private->dope, ( const gchar * ) icol->data );
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
			ofo_bat_line_set_amount( line,
					my_double_set_from_sql(( const gchar * ) icol->data ));
			icol = icol->next;
			if( icol->data ){
				ofo_bat_line_set_entry( line, atoi(( gchar * ) icol->data ));
			}
			icol = icol->next;
			if( icol->data ){
				bat_line_set_upd_user( line, ( gchar * ) icol->data );
			}
			icol = icol->next;
			if( icol->data ){
				bat_line_set_upd_stamp( line,
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
 * ofo_bat_line_get_deffect:
 */
const GDate *
ofo_bat_line_get_deffect( const ofoBatLine *bat )
{
	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const GDate * ) &bat->private->deffect );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_line_get_dope:
 */
const GDate *
ofo_bat_line_get_dope( const ofoBatLine *bat )
{
	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const GDate * ) &bat->private->dope );
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
 * ofo_bat_line_get_amount:
 */
gdouble
ofo_bat_line_get_amount( const ofoBatLine *bat )
{
	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), 0 );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return( bat->private->amount );
	}

	g_assert_not_reached();
	return( 0 );
}

/**
 * ofo_bat_line_get_entry:
 */
gint
ofo_bat_line_get_entry( const ofoBatLine *bat )
{
	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), 0 );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return( bat->private->entry );
	}

	g_assert_not_reached();
	return( 0 );
}

/**
 * ofo_bat_line_get_upd_user:
 */
const gchar *
ofo_bat_line_get_upd_user( const ofoBatLine *bat )
{
	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const gchar * ) bat->private->upd_user );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_line_get_upd_stamp:
 */
const GTimeVal *
ofo_bat_line_get_upd_stamp( const ofoBatLine *bat )
{
	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &bat->private->upd_stamp );
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
 * ofo_bat_line_set_deffect:
 */
void
ofo_bat_line_set_deffect( ofoBatLine *bat, const GDate *date )
{
	g_return_if_fail( OFO_IS_BAT_LINE( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		my_date_set_from_date( &bat->private->deffect, date );
	}
}

/**
 * ofo_bat_line_set_dope:
 */
void
ofo_bat_line_set_dope( ofoBatLine *bat, const GDate *date )
{
	g_return_if_fail( OFO_IS_BAT_LINE( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		my_date_set_from_date( &bat->private->dope, date );
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
 * ofo_bat_line_set_amount:
 */
void
ofo_bat_line_set_amount( ofoBatLine *bat, gdouble amount )
{
	g_return_if_fail( OFO_IS_BAT_LINE( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		bat->private->amount = amount;
	}
}

/**
 * ofo_bat_line_set_entry:
 */
void
ofo_bat_line_set_entry( ofoBatLine *bat, gint number )
{
	g_return_if_fail( OFO_IS_BAT_LINE( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		bat->private->entry = number;
	}
}

/*
 * ofo_bat_line_set_upd_user:
 */
static void
bat_line_set_upd_user( ofoBatLine *bat, const gchar *upd_user )
{
	g_return_if_fail( OFO_IS_BAT_LINE( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_free( bat->private->upd_user );
		bat->private->upd_user = g_strdup( upd_user );
	}
}

/*
 * ofo_bat_line_set_upd_stamp:
 */
static void
bat_line_set_upd_stamp( ofoBatLine *bat, const GTimeVal *upd_stamp )
{
	g_return_if_fail( OFO_IS_BAT_LINE( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		my_utils_stamp_set_from_stamp( &bat->private->upd_stamp, upd_stamp );
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
	const GDate *dope;
	gboolean ok;

	query = g_string_new( "INSERT INTO OFA_T_BAT_LINES" );

	str = my_date_to_str( ofo_bat_line_get_deffect( bat ), MY_DATE_SQL );

	g_string_append_printf( query,
			"	(BAT_ID,BAT_LINE_DEFFECT,BAT_LINE_DOPE,BAT_LINE_REF,"
			"	 BAT_LINE_LABEL,BAT_LINE_CURRENCY,BAT_LINE_AMOUNT) "
			"	VALUES (%d,'%s',",
					ofo_bat_line_get_bat_id( bat ),
					str );
	g_free( str );

	dope = ofo_bat_line_get_dope( bat );
	if( my_date_is_valid( dope )){
		str = my_date_to_str( dope, MY_DATE_SQL );
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

	str = my_double_to_sql( ofo_bat_line_get_amount( bat ));
	g_string_append_printf( query, "%s", str );
	g_free( str );

	query = g_string_append( query, ")" );

	ok = ofo_sgbd_query( sgbd, query->str, TRUE );

	g_string_free( query, TRUE );

	return( ok );
}

static gboolean
bat_line_get_back_id( ofoBatLine *bat, const ofoSgbd *sgbd )
{
	gboolean ok;
	GSList *result, *icol;

	ok = FALSE;
	result = ofo_sgbd_query_ex( sgbd, "SELECT LAST_INSERT_ID()", TRUE );

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
	gint entry_number;

	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	entry_number = ofo_bat_line_get_entry( bat );

	query = g_string_new( "UPDATE OFA_T_BAT_LINES SET " );

	if( entry_number > 0 ){
		g_string_append_printf( query,
				"	BAT_LINE_ENTRY=%d,BAT_LINE_UPD_USER='%s',BAT_LINE_UPD_STAMP='%s' ",
					entry_number, user, stamp_str );
	} else {
		query = g_string_append( query,
				"	BAT_LINE_ENTRY=NULL,BAT_LINE_UPD_USER=NULL,BAT_LINE_UPD_STAMP=0" );
	}

	g_string_append_printf( query,
			"	WHERE BAT_LINE_ID=%d", ofo_bat_line_get_id( bat ));

	ok = ofo_sgbd_query( sgbd, query->str, TRUE );

	g_string_free( query, TRUE );
	g_free( stamp_str );

	return( ok );
}
