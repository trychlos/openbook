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
#include "api/ofa-idataset.h"
#include "api/ofa-idbconnect.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-concil.h"
#include "api/ofo-dossier.h"
#include "api/ofo-bat.h"
#include "api/ofo-bat-line.h"

/* priv instance data
 */
struct _ofoBatPrivate {

	/* dbms data
	 */
	ofxCounter id;						/* bat (imported file) id */
	gchar     *uri;
	gchar     *format;
	GDate      begin;
	GDate      end;
	gchar     *rib;
	gchar     *currency;
	ofxAmount  begin_solde;
	gboolean   begin_solde_set;
	ofxAmount  end_solde;
	gboolean   end_solde_set;
	gchar     *notes;
	gchar     *account;
	gchar     *upd_user;
	GTimeVal   upd_stamp;
};

G_DEFINE_TYPE( ofoBat, ofo_bat, OFO_TYPE_BASE )

static GList      *bat_load_dataset( ofoDossier *dossier );
static ofoBat     *bat_find_by_id( GList *set, ofxCounter id );
static void        bat_set_id( ofoBat *bat, ofxCounter id );
static void        bat_set_upd_user( ofoBat *bat, const gchar *upd_user );
static void        bat_set_upd_stamp( ofoBat *bat, const GTimeVal *upd_stamp );
static gboolean    bat_do_insert( ofoBat *bat, const ofaIDBConnect *cnx, const gchar *user );
static gboolean    bat_insert_main( ofoBat *bat, const ofaIDBConnect *cnx, const gchar *user );
static gboolean    bat_do_update( ofoBat *bat, const ofaIDBConnect *cnx, const gchar *user );
static gboolean    bat_do_delete_main( ofoBat *bat, const ofaIDBConnect *cnx );
static gboolean    bat_do_delete_lines( ofoBat *bat, const ofaIDBConnect *cnx );
static gint        bat_cmp_by_id( const ofoBat *a, ofxCounter id );
static gint        bat_cmp_by_ptr( const ofoBat *a, const ofoBat *b );

OFA_IDATASET_LOAD( BAT, bat );

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
	g_free( priv->account );
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

static GList *
bat_load_dataset( ofoDossier *dossier )
{
	const ofaIDBConnect *cnx;
	GSList *result, *irow, *icol;
	ofoBat *bat;
	GList *dataset;
	GTimeVal timeval;

	dataset = NULL;
	cnx = ofo_dossier_get_connect( dossier );

	if( ofa_idbconnect_query_ex( cnx,
			"SELECT BAT_ID,BAT_URI,BAT_FORMAT,BAT_ACCOUNT,"
			"	BAT_BEGIN,BAT_END,BAT_RIB,BAT_CURRENCY,BAT_SOLDE_BEGIN,BAT_SOLDE_END,"
			"	BAT_NOTES,BAT_UPD_USER,BAT_UPD_STAMP "
			"	FROM OFA_T_BAT "
			"	ORDER BY BAT_UPD_STAMP ASC", &result, TRUE )){

		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			bat = ofo_bat_new();
			bat_set_id( bat, atol(( gchar * ) icol->data ));
			icol = icol->next;
			ofo_bat_set_uri( bat, ( gchar * ) icol->data );
			icol = icol->next;
			if( icol->data ){
				ofo_bat_set_format( bat, ( gchar * ) icol->data );
			}
			icol = icol->next;
			if( icol->data ){
				ofo_bat_set_account( bat, ( const gchar * ) icol->data );
			}
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
				ofo_bat_set_solde_begin( bat,
						my_double_set_from_sql(( const gchar * ) icol->data ));
				ofo_bat_set_solde_begin_set( bat, TRUE );
			} else {
				ofo_bat_set_solde_begin_set( bat, FALSE );
			}
			icol = icol->next;
			if( icol->data ){
				ofo_bat_set_solde_end( bat,
						my_double_set_from_sql(( const gchar * ) icol->data ));
				ofo_bat_set_solde_end_set( bat, TRUE );
			} else {
				ofo_bat_set_solde_end_set( bat, FALSE );
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

		ofa_idbconnect_free_results( result );
	}

	return( g_list_reverse( dataset ));
}

/**
 * ofo_bat_get_by_id:
 *
 * Returns: the searched BAT object, or %NULL.
 *
 * The returned object is owned by the #ofoBat class, and should
 * not be unreffed by the caller.
 */
ofoBat *
ofo_bat_get_by_id( ofoDossier *dossier, ofxCounter id )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( id > 0, NULL );

	OFA_IDATASET_GET( dossier, BAT, bat );

	return( bat_find_by_id( bat_dataset, id ));
}

static ofoBat *
bat_find_by_id( GList *set, ofxCounter id )
{
	GList *found;

	found = g_list_find_custom( set, ( gconstpointer ) id, ( GCompareFunc ) bat_cmp_by_id );
	if( found ){
		return( OFO_BAT( found->data ));
	}

	return( NULL );
}

/**
 * ofo_bat_get_id:
 */
ofxCounter
ofo_bat_get_id( const ofoBat *bat )
{
	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), OFO_BASE_UNSET_ID );

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
	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );

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
	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const gchar * ) bat->priv->format );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_get_begin:
 */
const GDate *
ofo_bat_get_begin( const ofoBat *bat )
{
	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );

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
	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );

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
	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );

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
	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const gchar * ) bat->priv->currency );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_get_solde_begin:
 */
ofxAmount
ofo_bat_get_solde_begin( const ofoBat *bat )
{
	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), 0 );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return( bat->priv->begin_solde );
	}

	g_assert_not_reached();
	return( 0 );
}

/**
 * ofo_bat_get_solde_begin_set:
 */
gboolean
ofo_bat_get_solde_begin_set( const ofoBat *bat )
{
	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), FALSE );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return( bat->priv->begin_solde_set );
	}

	g_assert_not_reached();
	return( FALSE );
}

/**
 * ofo_bat_get_solde_end:
 */
ofxAmount
ofo_bat_get_solde_end( const ofoBat *bat )
{
	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), 0 );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return( bat->priv->end_solde );
	}

	g_assert_not_reached();
	return( 0 );
}

/**
 * ofo_bat_get_solde_end_set:
 */
gboolean
ofo_bat_get_solde_end_set( const ofoBat *bat )
{
	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), FALSE );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return( bat->priv->end_solde_set );
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
	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const gchar * ) bat->priv->notes );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_get_account:
 */
const gchar *
ofo_bat_get_account( const ofoBat *bat )
{
	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return(( const gchar * ) bat->priv->account );
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
	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );

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
	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );

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
	gint count;
	gchar *primary, *secondary;
	GtkWidget *dialog;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	exists = FALSE;

	sbegin = my_date_to_str( begin, MY_DATE_SQL );
	send = my_date_to_str( end, MY_DATE_SQL );

	query = g_string_new( "SELECT COUNT(*) FROM OFA_T_BAT WHERE ");

	g_string_append_printf( query, "BAT_RIB='%s' ", rib );

	if( my_date_is_valid( begin )){
		g_string_append_printf( query, "AND BAT_BEGIN='%s' ", sbegin );
	} else {
		g_string_append_printf( query, "AND BAT_BEGIN IS NULL " );
	}

	if( my_date_is_valid( end )){
		g_string_append_printf( query, "AND BAT_END='%s'", send );
	} else {
		g_string_append_printf( query, "AND BAT_END IS NULL" );
	}

	ofa_idbconnect_query_int( ofo_dossier_get_connect( dossier ), query->str, &count, TRUE );

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

	g_string_free( query, TRUE );
	g_free( send );
	g_free( sbegin );

	return( exists );
}

/**
 * ofo_bat_is_deletable:
 *
 * An imported BAT file may be removed from the database if none of its
 * lines has been yet reconciliated (and the dossier is current).
 */
gboolean
ofo_bat_is_deletable( const ofoBat *bat, const ofoDossier *dossier )
{
	gint count;
	gboolean is_current;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		count = ofo_bat_get_used_count( bat, dossier );
		is_current = ofo_dossier_is_current( dossier );
		return( is_current && count == 0 );
	}

	return( FALSE );
}

/**
 * ofo_bat_get_lines_count:
 */
gint
ofo_bat_get_lines_count( const ofoBat *bat, const ofoDossier *dossier )
{
	gchar *query;
	gint count;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), -1 );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), -1 );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		query = g_strdup_printf(
						"SELECT COUNT(*) FROM OFA_T_BAT_LINES WHERE BAT_ID=%ld",
						ofo_bat_get_id( bat ));
		ofa_idbconnect_query_int( ofo_dossier_get_connect( dossier ), query, &count, TRUE );
		g_free( query );

		return( count );
	}

	g_assert_not_reached();
	return( -1 );
}

/**
 * ofo_bat_get_used_count:
 *
 * Returns the count of used lines from this BAT file.
 */
gint
ofo_bat_get_used_count( const ofoBat *bat, const ofoDossier *dossier )
{
	gchar *query;
	gint count;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), -1 );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), -1 );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_CONCIL_IDS WHERE "
				"	REC_IDS_TYPE='%s' AND REC_IDS_OTHER IN "
				"		(SELECT BAT_LINE_ID FROM OFA_T_BAT_LINES WHERE BAT_ID=%ld)",
				CONCIL_TYPE_BAT, ofo_bat_get_id( bat ));
		ofa_idbconnect_query_int( ofo_dossier_get_connect( dossier ), query, &count, TRUE );
		g_free( query );

		return( count );
	}

	g_assert_not_reached();
	return( -1 );
}

/*
 * bat_set_id:
 */
static void
bat_set_id( ofoBat *bat, ofxCounter id )
{
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

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
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

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
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_free( bat->priv->format );
		bat->priv->format = g_strdup( format );
	}
}

/**
 * ofo_bat_set_begin:
 */
void
ofo_bat_set_begin( ofoBat *bat, const GDate *date )
{
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

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
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

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
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

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
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_free( bat->priv->currency );
		bat->priv->currency = g_strdup( currency );
	}
}

/**
 * ofo_bat_set_solde_begin:
 */
void
ofo_bat_set_solde_begin( ofoBat *bat, ofxAmount solde )
{
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		bat->priv->begin_solde = solde;
	}
}

/**
 * ofo_bat_set_solde_begin_set:
 */
void
ofo_bat_set_solde_begin_set( ofoBat *bat, gboolean set )
{
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		bat->priv->begin_solde_set = set;
	}
}

/**
 * ofo_bat_set_solde_end:
 */
void
ofo_bat_set_solde_end( ofoBat *bat, ofxAmount solde )
{
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		bat->priv->end_solde = solde;
	}
}

/**
 * ofo_bat_set_solde_end_set:
 */
void
ofo_bat_set_solde_end_set( ofoBat *bat, gboolean set )
{
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		bat->priv->end_solde_set = set;
	}
}

/**
 * ofo_bat_set_notes:
 */
void
ofo_bat_set_notes( ofoBat *bat, const gchar *notes )
{
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_free( bat->priv->notes );
		bat->priv->notes = g_strdup( notes );
	}
}

/**
 * ofo_bat_set_account:
 */
void
ofo_bat_set_account( ofoBat *bat, const gchar *account )
{
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_free( bat->priv->account );
		bat->priv->account = g_strdup( account );
	}
}

/*
 * ofo_bat_set_upd_user:
 */
static void
bat_set_upd_user( ofoBat *bat, const gchar *upd_user )
{
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

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
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

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
ofo_bat_insert( ofoBat *bat, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_bat_insert";

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_debug( "%s: bat=%p, dossier=%p",
				thisfn, ( void * ) bat, ( void * ) dossier );

		bat->priv->id = ofo_dossier_get_next_bat( dossier );

		return( bat_do_insert(
					bat,
					ofo_dossier_get_connect( dossier ),
					ofo_dossier_get_user( dossier )));
	}

	return( FALSE );
}

static gboolean
bat_do_insert( ofoBat *bat, const ofaIDBConnect *cnx, const gchar *user )
{
	return( bat_insert_main( bat, cnx, user ));
}

static gboolean
bat_insert_main( ofoBat *bat, const ofaIDBConnect *cnx, const gchar *user )
{
	GString *query;
	gchar *suri, *str;
	const GDate *begin, *end;
	gboolean ok;
	gchar *stamp_str;
	GTimeVal stamp;

	ok = FALSE;
	my_utils_stamp_set_now( &stamp );
	suri = my_utils_quote( ofo_bat_get_uri( bat ));
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_BAT" );

	g_string_append_printf( query,
			"	(BAT_ID,BAT_URI,BAT_FORMAT,BAT_BEGIN,BAT_END,"
			"	 BAT_RIB,BAT_CURRENCY,BAT_SOLDE_BEGIN,BAT_SOLDE_END,"
			"	 BAT_NOTES,BAT_UPD_USER,BAT_UPD_STAMP) VALUES (%ld,'%s',",
					ofo_bat_get_id( bat ),
					suri );

	g_free( suri );

	str = my_utils_quote( ofo_bat_get_format( bat ));
	if( my_strlen( str )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( str );

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
	if( my_strlen( str )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	str = ( gchar * ) ofo_bat_get_currency( bat );
	if( my_strlen( str )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( ofo_bat_get_solde_begin_set( bat )){
		str = my_double_to_sql( ofo_bat_get_solde_begin( bat ));
		g_string_append_printf( query, "%s,", str );
		g_free( str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( ofo_bat_get_solde_end_set( bat )){
		str = my_double_to_sql( ofo_bat_get_solde_end( bat ));
		g_string_append_printf( query, "%s,", str );
		g_free( str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	str = my_utils_quote( ofo_bat_get_notes( bat ));
	if( my_strlen( str )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( str );

	g_string_append_printf( query, "'%s','%s')", user, stamp_str );

	if( ofa_idbconnect_query( cnx, query->str, TRUE )){
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
ofo_bat_update( ofoBat *bat, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_bat_update";

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_debug( "%s: bat=%p, dossier=%p",
				thisfn, ( void * ) bat, ( void * ) dossier );

		if( bat_do_update(
					bat,
					ofo_dossier_get_connect( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFA_IDATASET_UPDATE( dossier, BAT, bat, NULL );

			return( TRUE );
		}
	}

	return( FALSE );
}

/*
 * only notes may be updated
 */
static gboolean
bat_do_update( ofoBat *bat, const ofaIDBConnect *cnx, const gchar *user )
{
	GString *query;
	gchar *notes;
	gboolean ok;
	GTimeVal stamp;
	gchar *stamp_str;
	const gchar *caccount;

	ok = FALSE;
	notes = my_utils_quote( ofo_bat_get_notes( bat ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_BAT SET " );

	if( my_strlen( notes )){
		g_string_append_printf( query, "BAT_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "BAT_NOTES=NULL," );
	}

	caccount = ofo_bat_get_account( bat );
	if( my_strlen( caccount )){
		g_string_append_printf( query, "BAT_ACCOUNT='%s',", notes );
	} else {
		query = g_string_append( query, "BAT_ACCOUNT=NULL," );
	}

	g_string_append_printf( query,
			"	BAT_UPD_USER='%s',BAT_UPD_STAMP='%s'"
			"	WHERE BAT_ID=%ld", user, stamp_str, ofo_bat_get_id( bat ));

	if( ofa_idbconnect_query( cnx, query->str, TRUE )){
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
ofo_bat_delete( ofoBat *bat, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_bat_delete";

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( ofo_bat_is_deletable( bat, dossier ), FALSE );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		g_debug( "%s: bat=%p, dossier=%p",
				thisfn, ( void * ) bat, ( void * ) dossier );

		if( bat_do_delete_main( bat, ofo_dossier_get_connect( dossier ))){

			bat_do_delete_lines( bat, ofo_dossier_get_connect( dossier ));

			OFA_IDATASET_REMOVE( dossier, BAT, bat );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
bat_do_delete_main( ofoBat *bat, const ofaIDBConnect *cnx )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_BAT"
			"	WHERE BAT_ID=%ld",
					ofo_bat_get_id( bat ));

	ok = ofa_idbconnect_query( cnx, query, TRUE );

	g_free( query );

	return( ok );
}

static gboolean
bat_do_delete_lines( ofoBat *bat, const ofaIDBConnect *cnx )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_BAT_LINES"
			"	WHERE BAT_ID=%ld",
					ofo_bat_get_id( bat ));

	ok = ofa_idbconnect_query( cnx, query, TRUE );

	g_free( query );

	return( ok );
}

static gint
bat_cmp_by_id( const ofoBat *a, ofxCounter id )
{
	ofxCounter a_id;

	a_id = ofo_bat_get_id( a );

	return( a_id < id ? -1 : ( a_id > id ? 1 : 0 ));
}

static gint
bat_cmp_by_ptr( const ofoBat *a, const ofoBat *b )
{
	gint id_a, id_b;

	id_a = ofo_bat_get_id( a );
	id_b = ofo_bat_get_id( b );

	return( id_a > id_b ? 1 : ( id_a < id_b ? -1 : 0 ));
}

/**
 * ofo_bat_import:
 * @importable:
 * @sbat:
 * @dossier:
 *
 * Import the provided #ofsBat structure.
 */
gboolean
ofo_bat_import( ofaIImportable *importable, ofsBat *sbat, ofoDossier *dossier, ofxCounter *id )
{
	gboolean ok;
	ofoBat *bat;
	ofoBatLine *bline;
	ofsBatDetail *sdet;
	GList *it;
	guint count;

	g_return_val_if_fail( importable && OFA_IS_IIMPORTABLE( importable ), FALSE );
	g_return_val_if_fail( sbat, FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	count = g_list_length( ofo_bat_get_dataset( dossier ));

	bat = ofo_bat_new();

	ofo_bat_set_uri( bat, sbat->uri );
	ofo_bat_set_format( bat, sbat->format );
	ofo_bat_set_begin( bat, &sbat->begin );
	ofo_bat_set_end( bat, &sbat->end );
	ofo_bat_set_rib( bat, sbat->rib );
	ofo_bat_set_currency( bat, sbat->currency );
	ofo_bat_set_solde_begin( bat, sbat->begin_solde );
	ofo_bat_set_solde_begin_set( bat, sbat->begin_solde_set );
	ofo_bat_set_solde_end( bat, sbat->end_solde );
	ofo_bat_set_solde_end_set( bat, sbat->end_solde_set );

	ok = ofo_bat_insert( bat, dossier );
	if( ok ){
		for( it=sbat->details ; it ; it=it->next ){
			sdet = ( ofsBatDetail * ) it->data;
			bline = ofo_bat_line_new( ofo_bat_get_id( bat ));

			ofo_bat_line_set_deffect( bline, &sdet->deffect );
			ofo_bat_line_set_dope( bline, &sdet->dope );
			ofo_bat_line_set_ref( bline, sdet->ref );
			ofo_bat_line_set_label( bline, sdet->label );
			ofo_bat_line_set_currency( bline, sdet->currency );
			ofo_bat_line_set_amount( bline, sdet->amount );

			ok &= ofo_bat_line_insert( bline, dossier );
			ofa_iimportable_increment_progress( importable, IMPORTABLE_PHASE_INSERT, 1 );
			g_object_unref( bline );
		}
	}

	if( ok ){
		if( count > 0 ){
			OFA_IDATASET_ADD( dossier, BAT, bat );
		} else {
			OFA_IDATASET_GET( dossier, BAT, bat );
			if( ofa_idataset_is_signal_new_allowed( dossier, OFO_TYPE_BAT )){
				g_signal_emit_by_name( dossier, SIGNAL_DOSSIER_NEW_OBJECT, g_object_ref( bat ));
			}
		}
		if( id ){
			*id = ofo_bat_get_id( bat );
		}
	}

	/* do not g_object_unref() the newly created BAT as the ownership
	 * has been given to the global dataset */

	return( ok );
}
