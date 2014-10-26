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

#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-dossier.h"
#include "api/ofo-bat.h"
#include "api/ofo-sgbd.h"

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
	gboolean   solde_set;
	gchar     *notes;
	gchar     *upd_user;
	GTimeVal   upd_stamp;
};

G_DEFINE_TYPE( ofoBat, ofo_bat, OFO_TYPE_BASE )

OFO_BASE_DEFINE_GLOBAL( st_global, bat )

static void        init_global_handlers( const ofoDossier *dossier );
static GList      *bat_load_dataset( void );
static void        bat_set_upd_user( ofoBat *bat, const gchar *upd_user );
static void        bat_set_upd_stamp( ofoBat *bat, const GTimeVal *upd_stamp );
static gboolean    bat_do_insert( ofoBat *bat, const ofoSgbd *sgbd, const gchar *user );
static gboolean    bat_insert_main( ofoBat *bat, const ofoSgbd *sgbd, const gchar *user );
static gboolean    bat_do_update( ofoBat *bat, const ofoSgbd *sgbd, const gchar *user );
static gboolean    bat_do_delete_main( ofoBat *bat, const ofoSgbd *sgbd );
static gboolean    bat_do_delete_lines( ofoBat *bat, const ofoSgbd *sgbd );

static void
bat_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_bat_finalize";
	ofoBatPrivate *priv;

	priv = OFO_BAT( instance )->priv;

	g_debug( "%s: instance=%p (%s): %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), priv->uri );

	g_return_if_fail( instance && OFO_IS_BAT( instance ));

	/* free data members here */
	g_free( priv->uri );
	g_free( priv->format );
	g_free( priv->rib );
	g_free( priv->currency );
	g_free( priv->notes );
	g_free( priv->upd_user );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_bat_parent_class )->finalize( instance );
}

static void
bat_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_BAT( instance ));

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

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFO_TYPE_BAT, ofoBatPrivate );

	self->priv->id = OFO_BASE_UNSET_ID;
	my_date_clear( &self->priv->begin );
	my_date_clear( &self->priv->end );
}

static void
ofo_bat_class_init( ofoBatClass *klass )
{
	static const gchar *thisfn = "ofo_bat_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_finalize;

	g_type_class_add_private( klass, sizeof( ofoBatPrivate ));
}

static void
init_global_handlers( const ofoDossier *dossier )
{
	OFO_BASE_SET_GLOBAL( st_global, dossier, bat );
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
 * Returns: The list of #ofoBat bats, ordered by ascending
 * import date. The returned list is owned by the #ofoBat class, and
 * should not be freed by the caller.
 */
GList *
ofo_bat_get_dataset( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_bat_get_dataset";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	init_global_handlers( dossier );

	return( st_global->dataset );
}

static GList *
bat_load_dataset( void )
{
	const ofoSgbd *sgbd;
	GSList *result, *irow, *icol;
	ofoBat *bat;
	GList *dataset;
	GTimeVal timeval;

	sgbd = ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier ));

	result = ofo_sgbd_query_ex( sgbd,
			"SELECT BAT_ID,BAT_URI,BAT_FORMAT,BAT_COUNT,"
			"	BAT_BEGIN,BAT_END,BAT_RIB,BAT_CURRENCY,BAT_SOLDE,"
			"	BAT_NOTES,BAT_UPD_USER,BAT_UPD_STAMP "
			"	FROM OFA_T_BAT "
			"	ORDER BY BAT_UPD_STAMP ASC", TRUE );

	dataset = NULL;

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		bat = ofo_bat_new();
		ofo_bat_set_id( bat, atoi(( gchar * ) icol->data ));
		icol = icol->next;
		ofo_bat_set_uri( bat, ( gchar * ) icol->data );
		icol = icol->next;
		if( icol->data ){
			ofo_bat_set_format( bat, ( gchar * ) icol->data );
		}
		icol = icol->next;
		ofo_bat_set_count( bat, atoi(( gchar * ) icol->data ));
		icol = icol->next;
		if( icol->data ){
			my_date_set_from_sql( &bat->priv->begin, ( const gchar * ) icol->data );
		}
		icol = icol->next;
		if( icol->data ){
			my_date_set_from_sql( &bat->priv->end, ( const gchar * ) icol->data );
		}
		icol = icol->next;
		if( icol->data ){
			ofo_bat_set_rib( bat, ( gchar * ) icol->data );
		}
		icol = icol->next;
		if( icol->data ){
			ofo_bat_set_currency( bat, ( gchar * ) icol->data );
		}
		icol = icol->next;
		if( icol->data ){
			ofo_bat_set_solde( bat,
					my_double_set_from_sql(( const gchar * ) icol->data ));
			ofo_bat_set_solde_set( bat, TRUE );
		} else {
			ofo_bat_set_solde_set( bat, FALSE );
		}
		icol = icol->next;
		if( icol->data ){
			ofo_bat_set_notes( bat, ( gchar * ) icol->data );
		}
		icol = icol->next;
		bat_set_upd_user( bat, ( gchar * ) icol->data );
		icol = icol->next;
		bat_set_upd_stamp( bat,
				my_utils_stamp_set_from_sql( &timeval, ( const gchar * ) icol->data ));

		dataset = g_list_prepend( dataset, bat );
	}

	ofo_sgbd_free_result( result );

	return( g_list_reverse( dataset ));
}

/**
 * ofo_bat_get_id:
 */
gint
ofo_bat_get_id( const ofoBat *bat )
{
	g_return_val_if_fail( OFO_IS_BAT( bat ), OFO_BASE_UNSET_ID );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return( bat->priv->id );
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

		return(( const gchar * ) bat->priv->uri );
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

		return(( const gchar * ) bat->priv->format );
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

		return( bat->priv->count );
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

		return(( const GDate * ) &bat->priv->begin );
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

		return(( const GDate * ) &bat->priv->end );
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

		return(( const gchar * ) bat->priv->rib );
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

		return(( const gchar * ) bat->priv->currency );
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

		return( bat->priv->solde );
	}

	g_assert_not_reached();
	return( 0 );
}

/**
 * ofo_bat_get_solde_set:
 */
gboolean
ofo_bat_get_solde_set( const ofoBat *bat )
{
	g_return_val_if_fail( OFO_IS_BAT( bat ), FALSE );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return( bat->priv->solde_set );
	}

	g_assert_not_reached();
	return( FALSE );
}

/**
 * ofo_bat_get_notes:
 */
const gchar *
ofo_bat_get_notes( const ofoBat *bat )
{
	g_return_val_if_fail( OFO_IS_BAT( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const gchar * ) bat->priv->notes );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_get_upd_user:
 */
const gchar *
ofo_bat_get_upd_user( const ofoBat *bat )
{
	g_return_val_if_fail( OFO_IS_BAT( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const gchar * ) bat->priv->upd_user );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_get_upd_stamp:
 */
const GTimeVal *
ofo_bat_get_upd_stamp( const ofoBat *bat )
{
	g_return_val_if_fail( OFO_IS_BAT( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &bat->priv->upd_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_exists:
 * @dossier:
 * @rib:
 * @begin:
 * @end:
 *
 * Returns %TRUE if the Bank Account Transaction file has already been
 * imported,.
 */
gboolean
ofo_bat_exists( const ofoDossier *dossier, const gchar *rib, const GDate *begin, const GDate *end )
{
	gboolean exists;
	gchar *sbegin, *send;
	GString *query;
	GSList *result, *icol;
	gint count;
	gchar *primary, *secondary;
	GtkWidget *dialog;

	exists = FALSE;

	sbegin = my_date_to_str( begin, MY_DATE_SQL );
	send = my_date_to_str( end, MY_DATE_SQL );

	query = g_string_new( "SELECT COUNT(*) FROM OFA_T_BAT WHERE ");

	g_string_append_printf( query, "BAT_RIB='%s' ", rib );

	if( my_date_is_valid( begin )){
		g_string_append_printf( query, "AND BAT_BEGIN='%s' ", sbegin );
	} else {
		g_string_append_printf( query, "AND BAT_BEGIN=NULL " );
	}

	if( my_date_is_valid( end )){
		g_string_append_printf( query, "AND BAT_END='%s'", send );
	} else {
		g_string_append_printf( query, "AND BAT_END=NULL" );
	}

	result = ofo_sgbd_query_ex( ofo_dossier_get_sgbd( dossier ), query->str, TRUE );
	if( result ){
		icol = ( GSList * ) result->data;
		count = atoi(( gchar * ) icol->data );
		if( count > 0 ){
			exists = TRUE;
			primary = g_strdup(
					_( "The candidate Bank Account Transaction file has already been imported.\n"
							"A new import is refused." ));
			secondary = g_strdup_printf(
					_( "\tRIB\t\t= '%s'\n"
							"\tBegin\t= '%s'\n"
							"\tEnd\t\t= '%s'" ), rib, sbegin, send );
			dialog = gtk_message_dialog_new(
							NULL,
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_MESSAGE_WARNING,
							GTK_BUTTONS_CLOSE,
							"%s", primary );
			gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG( dialog ), "%s", secondary );
			gtk_dialog_run( GTK_DIALOG( dialog ));
			gtk_widget_destroy( dialog );
			g_free( secondary );
			g_free( primary );
		}
		ofo_sgbd_free_result( result );
	}

	g_string_free( query, TRUE );
	g_free( send );
	g_free( sbegin );

	return( exists );
}

/**
 * ofo_bat_is_deletable:
 */
gboolean
ofo_bat_is_deletable( const ofoBat *bat )
{
	g_return_val_if_fail( OFO_IS_BAT( bat ), FALSE );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return( TRUE );
	}

	g_assert_not_reached();
	return( FALSE );
}

/**
 * ofo_bat_set_id:
 */
void
ofo_bat_set_id( ofoBat *bat, gint id )
{
	g_return_if_fail( OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		bat->priv->id = id;
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

		g_free( bat->priv->uri );
		bat->priv->uri = g_strdup( uri );
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

		g_free( bat->priv->format );
		bat->priv->format = g_strdup( format );
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

		bat->priv->count = count;
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

		my_date_set_from_date( &bat->priv->begin, date );
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

		my_date_set_from_date( &bat->priv->end, date );
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

		g_free( bat->priv->rib );
		bat->priv->rib = g_strdup( rib );
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

		g_free( bat->priv->currency );
		bat->priv->currency = g_strdup( currency );
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

		bat->priv->solde = solde;
	}
}

/**
 * ofo_bat_set_solde_set:
 */
void
ofo_bat_set_solde_set( ofoBat *bat, gboolean set )
{
	g_return_if_fail( OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		bat->priv->solde_set = set;
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

		g_free( bat->priv->notes );
		bat->priv->notes = g_strdup( notes );
	}
}

/*
 * ofo_bat_set_upd_user:
 */
static void
bat_set_upd_user( ofoBat *bat, const gchar *upd_user )
{
	g_return_if_fail( OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_free( bat->priv->upd_user );
		bat->priv->upd_user = g_strdup( upd_user );
	}
}

/*
 * ofo_bat_set_upd_stamp:
 */
static void
bat_set_upd_stamp( ofoBat *bat, const GTimeVal *upd_stamp )
{
	g_return_if_fail( OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		my_utils_stamp_set_from_stamp( &bat->priv->upd_stamp, upd_stamp );
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

		bat->priv->id = ofo_dossier_get_next_bat_number( dossier );

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
	return( bat_insert_main( bat, sgbd, user ));
}

static gboolean
bat_insert_main( ofoBat *bat, const ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *str;
	const GDate *begin, *end;
	gboolean ok;
	gchar *stamp_str;
	GTimeVal stamp;

	ok = FALSE;
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_BAT" );

	g_string_append_printf( query,
			"	(BAT_ID,BAT_URI,BAT_FORMAT,BAT_COUNT,BAT_BEGIN,BAT_END,"
			"	 BAT_RIB,BAT_CURRENCY,BAT_SOLDE,"
			"	 BAT_NOTES,BAT_UPD_USER,BAT_UPD_STAMP) VALUES (%d,'%s',",
					ofo_bat_get_id( bat ),
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
	if( my_date_is_valid( begin )){
		str = my_date_to_str( begin, MY_DATE_SQL );
		g_string_append_printf( query, "'%s',", str );
		g_free( str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	end = ofo_bat_get_end( bat );
	if( my_date_is_valid( end )){
		str = my_date_to_str( end, MY_DATE_SQL );
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

	if( ofo_bat_get_solde_set( bat )){
		str = my_double_to_sql( ofo_bat_get_solde( bat ));
		g_string_append_printf( query, "%s,", str );
		g_free( str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	str = my_utils_quote( ofo_bat_get_notes( bat ));
	if( str && g_utf8_strlen( str, -1 )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( str );

	g_string_append_printf( query, "'%s','%s')", user, stamp_str );

	if( ofo_sgbd_query( sgbd, query->str, TRUE )){
		bat_set_upd_user( bat, user );
		bat_set_upd_stamp( bat, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_bat_update:
 */
gboolean
ofo_bat_update( ofoBat *bat, const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_bat_update";

	g_return_val_if_fail( OFO_IS_BAT( bat ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_debug( "%s: bat=%p, dossier=%p",
				thisfn, ( void * ) bat, ( void * ) dossier );

		OFO_BASE_SET_GLOBAL( st_global, dossier, bat );

		if( bat_do_update(
					bat,
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFO_BASE_UPDATE_DATASET( st_global, bat, NULL );
			return( TRUE );
		}
	}

	g_assert_not_reached();
	return( FALSE );
}

/*
 * only notes may be updated
 */
static gboolean
bat_do_update( ofoBat *bat, const ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *notes;
	gboolean ok;
	GTimeVal stamp;
	gchar *stamp_str;

	ok = FALSE;
	notes = my_utils_quote( ofo_bat_get_notes( bat ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_BAT SET " );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "BAT_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "BAT_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	BAT_UPD_USER='%s',BAT_UPD_STAMP='%s'"
			"	WHERE BAT_ID=%d", user, stamp_str, ofo_bat_get_id( bat ));

	if( ofo_sgbd_query( sgbd, query->str, TRUE )){
		bat_set_upd_user( bat, user );
		bat_set_upd_stamp( bat, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_bat_delete:
 */
gboolean
ofo_bat_delete( ofoBat *bat, const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_bat_delete";

	g_return_val_if_fail( OFO_IS_BAT( bat ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( ofo_bat_is_deletable( bat ), FALSE );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_debug( "%s: bat=%p, dossier=%p",
				thisfn, ( void * ) bat, ( void * ) dossier );

		OFO_BASE_SET_GLOBAL( st_global, dossier, bat );

		if( bat_do_delete_main( bat, ofo_dossier_get_sgbd( dossier ))){

			bat_do_delete_lines( bat, ofo_dossier_get_sgbd( dossier ));
			OFO_BASE_REMOVE_FROM_DATASET( st_global, bat );
			return( TRUE );
		}
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
bat_do_delete_main( ofoBat *bat, const ofoSgbd *sgbd )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_BAT"
			"	WHERE BAT_ID=%d",
					ofo_bat_get_id( bat ));

	ok = ofo_sgbd_query( sgbd, query, TRUE );

	g_free( query );

	return( ok );
}

static gboolean
bat_do_delete_lines( ofoBat *bat, const ofoSgbd *sgbd )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_BAT_LINES"
			"	WHERE BAT_ID=%d",
					ofo_bat_get_id( bat ));

	ok = ofo_sgbd_query( sgbd, query, TRUE );

	g_free( query );

	return( ok );
}
