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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofa-dbms.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-dossier.h"
#include "api/ofo-bat-line.h"

/* priv instance data
 */
struct _ofoBatLinePrivate {

	/* dbms data
	 */
	ofxCounter bat_id;						/* bat (imported file) id */
	ofxCounter line_id;						/* line id */
	GDate      deffect;
	GDate      dope;
	gchar     *ref;
	gchar     *label;
	gchar     *currency;
	ofxAmount  amount;

	GList     *concils;
};

/* a entry reconciliation
 * a bat line may have been used to reconciliate several entries
 */
typedef struct {
	ofxCounter entry;
	gchar     *upd_user;
	GTimeVal   upd_stamp;
}
	sConcil;

G_DEFINE_TYPE( ofoBatLine, ofo_bat_line, OFO_TYPE_BASE )

static GList      *bat_line_load_dataset( ofxCounter bat_id, const ofaDbms *dbms );
static void        bat_line_add_concil( ofoBatLine *batline, const sConcil *concil );
static void        bat_line_set_line_id( ofoBatLine *batline, ofxCounter id );
static gboolean    bat_line_do_insert( ofoBatLine *bat, const ofaDbms *dbms, const gchar *user );
static gboolean    bat_line_insert_main( ofoBatLine *bat, const ofaDbms *dbms, const gchar *user );
static gboolean    bat_line_do_add_entry( ofoBatLine *bat, const ofaDbms *dbms, const sConcil *concil );
static gboolean    bat_line_do_remove_entries( ofoBatLine *bat, const ofaDbms *dbms );

static void
concil_free( sConcil *concil )
{
	g_free( concil->upd_user );
	g_free( concil );
}

static void
bat_line_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_bat_line_finalize";
	ofoBatLinePrivate *priv;

	priv = OFO_BAT_LINE( instance )->priv;

	g_debug( "%s: instance=%p (%s): %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), priv->label );

	g_return_if_fail( instance && OFO_IS_BAT_LINE( instance ));

	/* free data members here */
	g_free( priv->ref );
	g_free( priv->label );
	g_free( priv->currency );
	g_list_free_full( priv->concils, ( GDestroyNotify ) concil_free );

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

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFO_TYPE_BAT_LINE, ofoBatLinePrivate );

	self->priv->bat_id = OFO_BASE_UNSET_ID;
	self->priv->line_id = OFO_BASE_UNSET_ID;
	my_date_clear( &self->priv->deffect );
	my_date_clear( &self->priv->dope );
}

static void
ofo_bat_line_class_init( ofoBatLineClass *klass )
{
	static const gchar *thisfn = "ofo_bat_line_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_line_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_line_finalize;

	g_type_class_add_private( klass, sizeof( ofoBatLinePrivate ));
}

/**
 * ofo_bat_line_new:
 */
ofoBatLine *
ofo_bat_line_new( gint bat_id )
{
	ofoBatLine *bat;

	bat = g_object_new( OFO_TYPE_BAT_LINE, NULL );
	bat->priv->bat_id = bat_id;

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

	return( bat_line_load_dataset( bat_id, ofo_dossier_get_dbms( dossier )));
}

static GList *
bat_line_load_dataset( ofxCounter bat_id, const ofaDbms *dbms)
{
	GString *query;
	GSList *result, *irow, *icol;
	GList *dataset, *it;
	ofoBatLine *line;
	sConcil *concil;

	dataset = NULL;

	query = g_string_new(
					"SELECT BAT_LINE_ID,BAT_LINE_DEFFECT,BAT_LINE_DOPE,"
					"	BAT_LINE_LABEL,BAT_LINE_REF,BAT_LINE_CURRENCY,"
					"	BAT_LINE_AMOUNT "
					"	FROM OFA_T_BAT_LINES " );

	g_string_append_printf( query, "WHERE BAT_ID=%ld ", bat_id );

	query = g_string_append( query, "ORDER BY BAT_LINE_DEFFECT ASC" );

	if( ofa_dbms_query_ex( dbms, query->str, &result, TRUE )){
		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			line = ofo_bat_line_new( bat_id );
			bat_line_set_line_id( line, atol(( gchar * ) icol->data ));
			icol = icol->next;
			my_date_set_from_sql( &line->priv->deffect, ( const gchar * ) icol->data );
			icol = icol->next;
			if( icol->data ){
				my_date_set_from_sql( &line->priv->dope, ( const gchar * ) icol->data );
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

			dataset = g_list_prepend( dataset, line );
		}
		ofa_dbms_free_results( result );
	}
	g_string_free( query, TRUE );

	for( it=dataset ; it ; it=it->next ){
		line = OFO_BAT_LINE( it->data );

		query = g_string_new(
						"SELECT "
						"	BAT_REC_ENTRY,BAT_REC_UPD_USER,BAT_REC_UPD_STAMP "
						"	FROM OFA_T_BAT_CONCIL " );
		g_string_append_printf( query, "WHERE BAT_LINE_ID=%ld ", ofo_bat_line_get_line_id( line ));
		query = g_string_append( query, "ORDER BY BAT_REC_ENTRY ASC" );

		if( ofa_dbms_query_ex( dbms, query->str, &result, TRUE )){
			for( irow=result ; irow ; irow=irow->next ){
				concil = g_new0( sConcil, 1 );

				icol = ( GSList * ) irow->data;
				concil->entry = atol(( const gchar * ) icol->data );
				icol = icol->next;
				concil->upd_user = g_strdup(( const gchar * ) icol->data );
				icol = icol->next;
				my_utils_stamp_set_from_sql( &concil->upd_stamp, ( const gchar * ) icol->data );

				bat_line_add_concil( line, concil );
			}
			ofa_dbms_free_results( result );
		}
		g_string_free( query, TRUE );
	}

	return( g_list_reverse( dataset ));
}

static void
bat_line_add_concil( ofoBatLine *batline, const sConcil *concil )
{
	ofoBatLinePrivate *priv;

	priv = batline->priv;

	priv->concils = g_list_append( priv->concils, ( gpointer ) concil );
}

/**
 * ofo_bat_line_get_bat_id:
 */
ofxCounter
ofo_bat_line_get_bat_id( const ofoBatLine *bat )
{
	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), OFO_BASE_UNSET_ID );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return( bat->priv->bat_id );
	}

	g_assert_not_reached();
	return( OFO_BASE_UNSET_ID );
}

/**
 * ofo_bat_line_get_line_id:
 */
ofxCounter
ofo_bat_line_get_line_id( const ofoBatLine *bat )
{
	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), OFO_BASE_UNSET_ID );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return( bat->priv->line_id );
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

		return(( const GDate * ) &bat->priv->deffect );
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

		return(( const GDate * ) &bat->priv->dope );
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

		return(( const gchar * ) bat->priv->ref );
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

		return(( const gchar * ) bat->priv->label );
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

		return(( const gchar * ) bat->priv->currency );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_bat_line_get_amount:
 */
ofxAmount
ofo_bat_line_get_amount( const ofoBatLine *bat )
{
	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), 0 );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		return( bat->priv->amount );
	}

	g_assert_not_reached();
	return( 0 );
}

/**
 * ofo_bat_line_get_entries:
 *
 * Returns: a #GList of entries which have been reconciliated against
 * this bat line (most often, only one entry).
 *
 * The returned value should be #g_list_free() by the caller.
 */
GList *
ofo_bat_line_get_entries( const ofoBatLine *bat )
{
	GList *entries, *it;
	sConcil *concil;

	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), 0 );

	entries = NULL;

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		for( it=bat->priv->concils ; it ; it=it->next ){
			concil = ( sConcil * ) it->data;
			entries = g_list_append( entries, ( gpointer ) concil->entry );
		}
	}

	return( entries );
}

/**
 * ofo_bat_line_get_upd_user:
 *
 * We only look at the first record as all should have been
 * reconciliated simultaneously.
 */
const gchar *
ofo_bat_line_get_upd_user( const ofoBatLine *bat )
{
	GList *concils;
	sConcil *concil;
	const gchar *cstr;

	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), NULL );

	cstr = NULL;

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		concils = bat->priv->concils;
		concil = concils ? ( sConcil * ) concils->data : NULL;
		cstr = concil ? concil->upd_user : NULL;
	}

	return( cstr );
}

/**
 * ofo_bat_line_is_used:
 *
 * Returns: %TRUE if the BAT line has been used to reconciliate some
 * entry.
 */
gboolean
ofo_bat_line_is_used( const ofoBatLine *bat )
{
	GList *concils;

	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		concils = bat->priv->concils;
		return( concils != NULL );
	}

	g_assert_not_reached();
	return( FALSE );
}

/**
 * ofo_bat_line_has_entry:
 *
 * Returns: %TRUE if the BAT line has been used to reconciliate this
 * entry.
 */
gboolean
ofo_bat_line_has_entry( const ofoBatLine *bat, ofxCounter entry )
{
	GList *it;

	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), NULL );

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		for( it=bat->priv->concils ; it ; it=it->next ){
			if( entry == (( sConcil * ) it->data )->entry ){
				return( TRUE );
			}
		}
	}

	return( FALSE );
}

/**
 * ofo_bat_line_get_entries_group:
 *
 * Returns: the list of entries which have been reconciliated against
 * the same BAT line that the given entry.
 *
 * The returned list should be #g_list_free_full( list, ( GDestroyNotify ) g_free )
 * by the caller.
 */
GList *
ofo_bat_line_get_entries_group( const ofoDossier *dossier, ofxCounter entry )
{
	gchar *query;
	GSList *result, *irow, *icol;
	ofxCounter *pnumber;
	GList *entries;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );

	entries = NULL;
	query = g_strdup_printf(
			"SELECT BAT_REC_ENTRY FROM OFA_T_BAT_CONCIL "
			"	WHERE BAT_LINE_ID="
			"		(SELECT BAT_LINE_ID FROM OFA_T_BAT_CONCIL WHERE BAT_REC_ENTRY=%ld)", entry );

	if( ofa_dbms_query_ex( ofo_dossier_get_dbms( dossier ), query, &result, TRUE )){
		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			pnumber = g_new0( ofxCounter, 1 );
			*pnumber = atol(( gchar * ) icol->data );
			entries = g_list_prepend( entries, pnumber );
		}
		ofa_dbms_free_results( result );
	}
	g_free( query );

	return( entries );
}

/**
 * ofo_bat_line_get_upd_stamp:
 *
 * We only look at the first record as all should have been
 * reconciliated simultaneously.
 */
const GTimeVal *
ofo_bat_line_get_upd_stamp( const ofoBatLine *bat )
{
	GList *concils;
	sConcil *concil;
	const GTimeVal *stamp;

	g_return_val_if_fail( OFO_IS_BAT_LINE( bat ), NULL );

	stamp = NULL;

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		concils = bat->priv->concils;
		concil = concils ? ( sConcil * ) concils->data : NULL;
		stamp = concil ? ( const GTimeVal * ) &concil->upd_stamp : NULL;
	}

	return( stamp );
}

/*
 * bat_line_set_line_id:
 */
static void
bat_line_set_line_id( ofoBatLine *bat, ofxCounter id )
{
	g_return_if_fail( OFO_IS_BAT_LINE( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		bat->priv->line_id = id;
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

		my_date_set_from_date( &bat->priv->deffect, date );
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

		my_date_set_from_date( &bat->priv->dope, date );
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

		g_free( bat->priv->ref );
		bat->priv->ref = g_strdup( ref );
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

		g_free( bat->priv->label );
		bat->priv->label = g_strdup( label );
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

		g_free( bat->priv->currency );
		bat->priv->currency = g_strdup( currency );
	}
}

/**
 * ofo_bat_line_set_amount:
 */
void
ofo_bat_line_set_amount( ofoBatLine *bat, ofxAmount amount )
{
	g_return_if_fail( OFO_IS_BAT_LINE( bat ));

	if( !OFO_BASE( bat )->prot->dispose_has_run ){

		bat->priv->amount = amount;
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
ofo_bat_line_insert( ofoBatLine *bat_line, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_bat_line_insert";

	g_return_val_if_fail( OFO_IS_BAT_LINE( bat_line ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( bat_line )->prot->dispose_has_run ){

		g_debug( "%s: bat=%p, dossier=%p",
				thisfn, ( void * ) bat_line, ( void * ) dossier );

		bat_line->priv->line_id = ofo_dossier_get_next_batline( dossier );

		if( bat_line_do_insert(
					bat_line,
					ofo_dossier_get_dbms( dossier ),
					ofo_dossier_get_user( dossier ))){

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
bat_line_do_insert( ofoBatLine *bat, const ofaDbms *dbms, const gchar *user )
{
	return( bat_line_insert_main( bat, dbms, user ));
}

static gboolean
bat_line_insert_main( ofoBatLine *bat, const ofaDbms *dbms, const gchar *user )
{
	GString *query;
	gchar *str;
	const GDate *dope;
	gboolean ok;

	query = g_string_new( "INSERT INTO OFA_T_BAT_LINES" );

	str = my_date_to_str( ofo_bat_line_get_deffect( bat ), MY_DATE_SQL );

	g_string_append_printf( query,
			"	(BAT_ID,BAT_LINE_ID,BAT_LINE_DEFFECT,BAT_LINE_DOPE,BAT_LINE_REF,"
			"	 BAT_LINE_LABEL,BAT_LINE_CURRENCY,BAT_LINE_AMOUNT) "
			"	VALUES (%ld,%ld,'%s',",
					ofo_bat_line_get_bat_id( bat ),
					ofo_bat_line_get_line_id( bat ),
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
	if( my_strlen( str )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( str );

	str = my_utils_quote( ofo_bat_line_get_label( bat ));
	if( my_strlen( str )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( str );

	str = ( gchar * ) ofo_bat_line_get_currency( bat );
	if( my_strlen( str )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	str = my_double_to_sql( ofo_bat_line_get_amount( bat ));
	g_string_append_printf( query, "%s", str );
	g_free( str );

	query = g_string_append( query, ")" );

	ok = ofa_dbms_query( dbms, query->str, TRUE );

	g_string_free( query, TRUE );

	return( ok );
}

/**
 * ofo_bat_line_add_entry:
 *
 * Add a new reconciliated entry against this BAT line.
 * This update both the DBMS and the #ofoBatLine object.
 */
gboolean
ofo_bat_line_add_entry( ofoBatLine *bat_line, const ofoDossier *dossier, ofxCounter entry )
{
	static const gchar *thisfn = "ofo_bat_line_insert";
	sConcil *concil;

	g_return_val_if_fail( OFO_IS_BAT_LINE( bat_line ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( bat_line )->prot->dispose_has_run ){

		g_debug( "%s: bat=%p, dossier=%p",
				thisfn, ( void * ) bat_line, ( void * ) dossier );

		concil = g_new0( sConcil, 1 );
		concil->entry = entry;
		concil->upd_user = g_strdup( ofo_dossier_get_user( dossier ));
		my_utils_stamp_set_now( &concil->upd_stamp );
		bat_line_add_concil( bat_line, concil );

		if( bat_line_do_add_entry(
					bat_line,
					ofo_dossier_get_dbms( dossier ),
					concil )){

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
bat_line_do_add_entry( ofoBatLine *bat, const ofaDbms *dbms, const sConcil *concil )
{
	gboolean ok;
	GString *query;
	gchar *stamp_str;

	stamp_str = my_utils_stamp_to_str( &concil->upd_stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_BAT_CONCIL " );

	g_string_append_printf( query,
			"	(BAT_LINE_ID,BAT_REC_ENTRY,BAT_REC_UPD_USER,BAT_REC_UPD_STAMP) "
			"	VALUES (%ld,%ld,'%s','%s')",
					ofo_bat_line_get_line_id( bat ), concil->entry, concil->upd_user, stamp_str );

	ok = ofa_dbms_query( dbms, query->str, TRUE );

	g_string_free( query, TRUE );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_bat_line_remove_entry:
 *
 * Un-reconciliate a previously reconciliated BAT line.
 * This update both the DBMS and the #ofoBatLine object.
 */
gboolean
ofo_bat_line_remove_entries( ofoBatLine *bat_line, const ofoDossier *dossier, ofxCounter entry )
{
	static const gchar *thisfn = "ofo_bat_line_insert";
	ofoBatLinePrivate *priv;

	g_return_val_if_fail( OFO_IS_BAT_LINE( bat_line ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( bat_line )->prot->dispose_has_run ){

		g_debug( "%s: bat=%p, dossier=%p",
				thisfn, ( void * ) bat_line, ( void * ) dossier );

		priv = bat_line->priv;
		g_list_free_full( priv->concils, ( GDestroyNotify ) concil_free );
		priv->concils = NULL;

		if( bat_line_do_remove_entries(
					bat_line,
					ofo_dossier_get_dbms( dossier ))){

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
bat_line_do_remove_entries( ofoBatLine *bat, const ofaDbms *dbms )
{
	gboolean ok;
	GString *query;

	query = g_string_new( "DELETE FROM OFA_T_BAT_CONCIL " );

	g_string_append_printf( query,
			"	WHERE BAT_LINE_ID=%ld",
					ofo_bat_line_get_line_id( bat ));

	ok = ofa_dbms_query( dbms, query->str, TRUE );

	g_string_free( query, TRUE );

	return( ok );
}
