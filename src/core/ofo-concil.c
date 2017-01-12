/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-icollectionable.h"
#include "my/my-icollector.h"
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-concil.h"
#include "api/ofo-dossier.h"
#include "api/ofs-concil-id.h"

/* priv instance data
 */
typedef struct {

	/* OFA_T_CONCIL table content
	 */
	ofxCounter id;
	GDate      dval;
	gchar     *user;
	GTimeVal   stamp;

	/* OFA_T_CONCIL_IDS table content
	 */
	GList     *ids;						/* a list of ofsConcilId records */
}
	ofoConcilPrivate;

static ofoConcil *concil_get_by_query( const gchar *query, ofaHub *hub );
static void       concil_set_id( ofoConcil *concil, ofxCounter id );
static void       concil_add_other_id( ofoConcil *concil, const gchar *type, ofxCounter id );
static gboolean   concil_do_insert( ofoConcil *concil, const ofaIDBConnect *connect );
static gint       concil_cmp_by_ptr( ofoConcil *a, ofoConcil *b );
static gboolean   concil_do_insert_id( ofoConcil *concil, const gchar *type, ofxCounter id, const ofaIDBConnect *connect );
static gboolean   concil_do_delete( ofoConcil *concil, const ofaIDBConnect *connect );
static void       icollectionable_iface_init( myICollectionableInterface *iface );
static guint      icollectionable_get_interface_version( void );

G_DEFINE_TYPE_EXTENDED( ofoConcil, ofo_concil, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoConcil )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init ))

static void
concil_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_concil_finalize";
	ofoConcilPrivate *priv;
	gchar *sdate;

	priv = ofo_concil_get_instance_private( OFO_CONCIL( instance ));

	sdate = my_date_to_str( &priv->dval, MY_DATE_SQL );
	g_debug( "%s: instance=%p (%s): %ld: %s %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			priv->id, sdate, priv->user );
	g_free( sdate );

	g_return_if_fail( instance && OFO_IS_CONCIL( instance ));

	/* free data members here */

	g_free( priv->user );
	g_list_free_full( priv->ids, ( GDestroyNotify ) ofs_concil_id_free );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_concil_parent_class )->finalize( instance );
}

static void
concil_dispose( GObject *instance )
{
	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_concil_parent_class )->dispose( instance );
}

static void
ofo_concil_init( ofoConcil *self )
{
	static const gchar *thisfn = "ofo_concil_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
}

static void
ofo_concil_class_init( ofoConcilClass *klass )
{
	static const gchar *thisfn = "ofo_concil_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = concil_dispose;
	G_OBJECT_CLASS( klass )->finalize = concil_finalize;
}

/**
 * ofo_concil_get_orphans:
 * @hub: the current #ofaHub object.
 *
 * Returns: the list of conciliation children identifiers which no more
 * have a parent.
 *
 * The returned list should not be #ofo_concil_free_orphans() by
 * the caller.
 */
GList *
ofo_concil_get_orphans( ofaHub *hub )
{
	GList *orphans;
	GSList *result, *irow, *icol;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	if( !ofa_idbconnect_query_ex(
			ofa_hub_get_connect( hub ),
			"SELECT DISTINCT(REC_ID) FROM OFA_T_CONCIL_IDS "
			"	WHERE REC_ID NOT IN (SELECT DISTINCT(REC_ID) FROM OFA_T_CONCIL)"
			"	ORDER BY REC_ID DESC", &result, FALSE )){
		return( NULL );
	}
	orphans = NULL;
	for( irow=result ; irow ; irow=irow->next ){
		icol = irow->data;
		orphans = g_list_prepend( orphans, g_strdup(( gchar * ) icol->data ));
	}
	ofa_idbconnect_free_results( result );

	return( orphans );
}

/**
 * ofo_concil_get_by_id:
 * @hub: the current #ofaHub object.
 * @rec_id: the identifier of the requested conciliation group.
 *
 * Returns: a newly allocated object, or %NULL.
 *
 * Only the group header is loaded. The list of individuals are not.
 */
ofoConcil *
ofo_concil_get_by_id( ofaHub *hub, ofxCounter rec_id )
{
	ofoConcil *concil;
	gchar *query;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	query = g_strdup_printf(
				"SELECT REC_ID,REC_DVAL,REC_USER,REC_STAMP FROM OFA_T_CONCIL "
				"	WHERE REC_ID=%ld", rec_id );

	concil = concil_get_by_query( query, hub );

	g_free( query );

	return( concil );
}

/**
 * ofo_concil_get_by_other_id:
 * @hub: the current #ofaHub object.
 * @type: the seatched type, an entry or a BAT line.
 * @other_id: the identifier of the searched type.
 *
 * Returns: a newly allocated object which maintains the conciliations
 * list associated to the specified @other_id, or %NULL.
 */
ofoConcil *
ofo_concil_get_by_other_id( ofaHub *hub, const gchar *type, ofxCounter other_id )
{
	ofoConcil *concil;
	gchar *query;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( my_strlen( type ), NULL );

	query = g_strdup_printf(
				"SELECT REC_ID,REC_DVAL,REC_USER,REC_STAMP FROM OFA_T_CONCIL "
				"	WHERE REC_ID="
				"		(SELECT DISTINCT(REC_ID) FROM OFA_T_CONCIL_IDS "
				"		WHERE REC_IDS_TYPE='%s' AND REC_IDS_OTHER=%ld)", type, other_id );

	concil = concil_get_by_query( query, hub );

	g_free( query );

	return( concil );
}

/*
 */
static ofoConcil *
concil_get_by_query( const gchar *query, ofaHub *hub )
{
	const ofaIDBConnect *connect;
	ofoConcil *concil;
	GSList *result, *irow, *icol;
	GDate date;
	GTimeVal stamp;
	gchar *query2;
	gchar *type;
	ofxCounter id;

	concil = NULL;
	connect = ofa_hub_get_connect( hub );

	if( ofa_idbconnect_query_ex( connect, query, &result, TRUE )){
		if( g_slist_length( result ) == 1 ){
			irow=result;
			concil = ofo_concil_new();
			icol = ( GSList * ) irow->data;
			concil_set_id( concil, atol(( gchar * ) icol->data ));
			icol = icol->next;
			ofo_concil_set_dval( concil,
					my_date_set_from_sql( &date, ( const gchar * ) icol->data ));
			icol = icol->next;
			ofo_concil_set_user( concil, ( const gchar * ) icol->data );
			icol = icol->next;
			ofo_concil_set_stamp( concil,
					my_stamp_set_from_sql( &stamp, ( const gchar * ) icol->data ));
			ofo_base_set_hub( OFO_BASE( concil ), hub );
		}
		ofa_idbconnect_free_results( result );
	}

	if( concil ){
		query2 = g_strdup_printf(
				"SELECT REC_IDS_TYPE,REC_IDS_OTHER FROM OFA_T_CONCIL_IDS "
				"	WHERE REC_ID=%ld", ofo_concil_get_id( concil ));
		if( ofa_idbconnect_query_ex( connect, query2, &result, TRUE )){
			for( irow=result ; irow ; irow=irow->next ){
				icol = ( GSList * ) irow->data;
				type = g_strdup(( const gchar * ) icol->data );
				icol = icol->next;
				id = atol(( const gchar * ) icol->data );
				concil_add_other_id( concil, type, id );
				g_free( type );
			}
			ofa_idbconnect_free_results( result );
		}
		g_free( query2 );

		my_icollector_collection_add_object(
				ofa_hub_get_collector( hub ),
				MY_ICOLLECTIONABLE( concil ), ( GCompareFunc ) concil_cmp_by_ptr, hub );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_NEW, concil );
	}

	return( concil );
}

/**
 * ofo_concil_new:
 */
ofoConcil *
ofo_concil_new( void )
{
	ofoConcil *concil;

	concil = g_object_new( OFO_TYPE_CONCIL, NULL );

	return( concil );
}

/**
 * ofo_concil_get_id:
 * @concil:
 */
ofxCounter
ofo_concil_get_id( ofoConcil *concil )
{
	ofoConcilPrivate *priv;

	g_return_val_if_fail( concil && OFO_IS_CONCIL( concil ), -1 );
	g_return_val_if_fail( !OFO_BASE( concil )->prot->dispose_has_run, -1 );

	priv = ofo_concil_get_instance_private( concil );

	return( priv->id );
}

/**
 * ofo_concil_get_dval:
 * @concil:
 */
const GDate *
ofo_concil_get_dval( ofoConcil *concil )
{
	ofoConcilPrivate *priv;

	g_return_val_if_fail( concil && OFO_IS_CONCIL( concil ), NULL );
	g_return_val_if_fail( !OFO_BASE( concil )->prot->dispose_has_run, NULL );

	priv = ofo_concil_get_instance_private( concil );

	return(( const GDate * ) &priv->dval );
}

/**
 * ofo_concil_get_user:
 * @concil:
 */
const gchar *
ofo_concil_get_user( ofoConcil *concil )
{
	ofoConcilPrivate *priv;

	g_return_val_if_fail( concil && OFO_IS_CONCIL( concil ), NULL );
	g_return_val_if_fail( !OFO_BASE( concil )->prot->dispose_has_run, NULL );

	priv = ofo_concil_get_instance_private( concil );

	return(( const gchar * ) priv->user );
}

/**
 * ofo_concil_get_stamp:
 * @concil:
 */
const GTimeVal *
ofo_concil_get_stamp( ofoConcil *concil )
{
	ofoConcilPrivate *priv;

	g_return_val_if_fail( concil && OFO_IS_CONCIL( concil ), NULL );
	g_return_val_if_fail( !OFO_BASE( concil )->prot->dispose_has_run, NULL );

	priv = ofo_concil_get_instance_private( concil );

	return(( const GTimeVal * ) &priv->stamp );
}

/**
 * ofo_concil_get_ids:
 * @concil:
 *
 * Returns: the list of #ofsConcilId lines of the reconciliation group.
 *
 * The list is owned by the @concil object, and should not be released
 * by the caller.
 */
GList *
ofo_concil_get_ids( ofoConcil *concil )
{
	ofoConcilPrivate *priv;

	g_return_val_if_fail( concil && OFO_IS_CONCIL( concil ), NULL );
	g_return_val_if_fail( !OFO_BASE( concil )->prot->dispose_has_run, NULL );

	priv = ofo_concil_get_instance_private( concil );

	return( priv->ids );
}

/**
 * ofo_concil_has_member:
 * @concil:
 * @type:
 * @id:
 */
gboolean
ofo_concil_has_member( ofoConcil *concil, const gchar *type, ofxCounter id )
{
	ofoConcilPrivate *priv;
	gboolean found;
	GList *it;
	ofsConcilId *sid;

	g_return_val_if_fail( concil && OFO_IS_CONCIL( concil ), FALSE );
	g_return_val_if_fail( !OFO_BASE( concil )->prot->dispose_has_run, FALSE );

	found = FALSE;
	priv = ofo_concil_get_instance_private( concil );

	for( it=priv->ids ; it ; it=it->next ){
		sid = ( ofsConcilId * ) it->data;
		if( ofs_concil_id_is_equal( sid, type, id )){
			found = TRUE;
			break;
		}
	}

	return( found );
}

/**
 * ofo_concil_for_each_member:
 * @concil:
 * @fn:
 * @user_data:
 */
void
ofo_concil_for_each_member( ofoConcil *concil, ofoConcilEnumerate fn, void *user_data )
{
	ofoConcilPrivate *priv;
	GList *it;
	ofsConcilId *sid;

	g_return_if_fail( concil && OFO_IS_CONCIL( concil ));
	g_return_if_fail( !OFO_BASE( concil )->prot->dispose_has_run );

	priv = ofo_concil_get_instance_private( concil );

	for( it=priv->ids ; it ; it=it->next ){
		sid = ( ofsConcilId * ) it->data;
		fn( concil, sid->type, sid->other_id, user_data );
	}
}

static void
concil_set_id( ofoConcil *concil, ofxCounter id )
{
	ofoConcilPrivate *priv;

	g_return_if_fail( concil && OFO_IS_CONCIL( concil ));
	g_return_if_fail( !OFO_BASE( concil )->prot->dispose_has_run );

	priv = ofo_concil_get_instance_private( concil );

	priv->id = id;
}

/**
 * ofo_ofo_concil_set_dval:
 * @concil:
 * @dval:
 */
void
ofo_concil_set_dval( ofoConcil *concil, const GDate *dval )
{
	ofoConcilPrivate *priv;

	g_return_if_fail( concil && OFO_IS_CONCIL( concil ));
	g_return_if_fail( !OFO_BASE( concil )->prot->dispose_has_run );

	priv = ofo_concil_get_instance_private( concil );

	my_date_set_from_date( &priv->dval, dval );
}

/**
 * ofo_concil_set_user:
 * @concil:
 * @user:
 */
void
ofo_concil_set_user( ofoConcil *concil, const gchar *user )
{
	ofoConcilPrivate *priv;

	g_return_if_fail( concil && OFO_IS_CONCIL( concil ));
	g_return_if_fail( !OFO_BASE( concil )->prot->dispose_has_run );

	priv = ofo_concil_get_instance_private( concil );

	g_free( priv->user );
	priv->user = g_strdup( user );
}

/**
 * ofo_concil_set_stamp:
 * @concil:
 * @stamp:
 */
void
ofo_concil_set_stamp( ofoConcil *concil, const GTimeVal *stamp )
{
	ofoConcilPrivate *priv;

	g_return_if_fail( concil && OFO_IS_CONCIL( concil ));
	g_return_if_fail( !OFO_BASE( concil )->prot->dispose_has_run );

	priv = ofo_concil_get_instance_private( concil );

	my_stamp_set_from_stamp( &priv->stamp, stamp );
}

static void
concil_add_other_id( ofoConcil *concil, const gchar *type, ofxCounter id )
{
	ofoConcilPrivate *priv;
	ofsConcilId *sid;

	g_return_if_fail( concil && OFO_IS_CONCIL( concil ));
	g_return_if_fail( !OFO_BASE( concil )->prot->dispose_has_run );

	priv = ofo_concil_get_instance_private( concil );

	sid = g_new0( ofsConcilId, 1 );
	sid->type = g_strdup( type );
	sid->other_id = id;

	priv->ids = g_list_prepend( priv->ids, ( gpointer ) sid );
}

/**
 * ofo_concil_insert:
 * @concil:
 * @dossier:
 */
gboolean
ofo_concil_insert( ofoConcil *concil, ofaHub *hub )
{
	static const gchar *thisfn = "ofo_concil_insert";
	ofoDossier *dossier;
	gboolean ok;

	g_debug( "%s: concil=%p, hub=%p",
			thisfn, ( void * ) concil, ( void * ) hub );

	g_return_val_if_fail( concil && OFO_IS_CONCIL( concil ), FALSE );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( !OFO_BASE( concil )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	dossier = ofa_hub_get_dossier( hub );
	concil_set_id( concil, ofo_dossier_get_next_concil( dossier ));

	if( concil_do_insert( concil, ofa_hub_get_connect( hub ))){
		ofo_base_set_hub( OFO_BASE( concil ), hub );
		my_icollector_collection_add_object(
				ofa_hub_get_collector( hub ), MY_ICOLLECTIONABLE( concil ), NULL, hub );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_NEW, concil );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
concil_do_insert( ofoConcil *concil, const ofaIDBConnect *connect )
{
	gchar *query, *sdate, *stamp;
	gboolean ok;

	sdate = my_date_to_str( ofo_concil_get_dval( concil ), MY_DATE_SQL );
	stamp = my_stamp_to_str( ofo_concil_get_stamp( concil ), MY_STAMP_YYMDHMS );

	query = g_strdup_printf(
			"INSERT INTO OFA_T_CONCIL "
			"	(REC_ID,REC_DVAL,REC_USER,REC_STAMP) VALUES "
			"	(%ld,'%s','%s','%s')",
			ofo_concil_get_id( concil ), sdate, ofo_concil_get_user( concil ), stamp );

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( stamp );
	g_free( sdate );
	g_free( query );

	return( ok );
}

static gint
concil_cmp_by_ptr( ofoConcil *a, ofoConcil *b )
{
	ofxCounter id_a, id_b;

	id_a = ofo_concil_get_id( a );
	id_b = ofo_concil_get_id( b );

	return( id_a < id_b ? -1 : ( id_a > id_b ? 1 : 0 ));
}

/**
 * ofo_concil_add_id:
 * @concil:
 * @type:
 * @id:
 *
 * Add an individual line to an existing conciliation group.
 *
 * Returns: %TRUE if success.
 */
gboolean
ofo_concil_add_id( ofoConcil *concil, const gchar *type, ofxCounter id )
{
	static const gchar *thisfn = "ofo_concil_add_id";
	gboolean ok;
	ofaHub *hub;

	g_debug( "%s: concil=%p, type=%s, id=%lu",
			thisfn, ( void * ) concil, type, id );

	g_return_val_if_fail( concil && OFO_IS_CONCIL( concil ), FALSE );
	g_return_val_if_fail( my_strlen( type ), FALSE );
	g_return_val_if_fail( !OFO_BASE( concil )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( concil ));
	concil_add_other_id( concil, type, id );

	if( concil_do_insert_id( concil, type, id, ofa_hub_get_connect( hub ))){
		g_signal_emit_by_name( G_OBJECT( hub ),SIGNAL_HUB_UPDATED, concil, NULL );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
concil_do_insert_id( ofoConcil *concil, const gchar *type, ofxCounter id, const ofaIDBConnect *connect )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"INSERT INTO OFA_T_CONCIL_IDS "
			"	(REC_ID,REC_IDS_TYPE,REC_IDS_OTHER) VALUES "
			"	(%ld,'%s',%ld)",
			ofo_concil_get_id( concil ), type, id );

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

/**
 * ofo_concil_delete:
 * @concil:
 */
gboolean
ofo_concil_delete( ofoConcil *concil )
{
	static const gchar *thisfn = "ofo_concil_delete";
	gboolean ok;
	ofaHub *hub;

	g_debug( "%s: concil=%p", thisfn, ( void * ) concil );

	g_return_val_if_fail( concil && OFO_IS_CONCIL( concil ), FALSE );
	g_return_val_if_fail( !OFO_BASE( concil )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( concil ));

	if( concil_do_delete( concil, ofa_hub_get_connect( hub ))){
		g_object_ref( concil );
		my_icollector_collection_remove_object( ofa_hub_get_collector( hub ), MY_ICOLLECTIONABLE( concil ));
		g_signal_emit_by_name( hub, SIGNAL_HUB_DELETED, concil );
		g_object_unref( concil );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
concil_do_delete( ofoConcil *concil, const ofaIDBConnect *connect )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_CONCIL"
			"	WHERE REC_ID=%ld",
					ofo_concil_get_id( concil ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_CONCIL_IDS"
			"	WHERE REC_ID=%ld",
					ofo_concil_get_id( concil ));

	ok &= ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

/*
 * myICollectionable interface management
 */
static void
icollectionable_iface_init( myICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_concil_icollectionable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = icollectionable_get_interface_version;
}

static guint
icollectionable_get_interface_version( void )
{
	return( 1 );
}
