/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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
#include "api/ofa-igetter.h"
#include "api/ofa-isignaler.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-concil.h"
#include "api/ofo-counters.h"
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

static void       concil_set_id( ofoConcil *concil, ofxCounter id );
static void       concil_add_other_id( ofoConcil *concil, const gchar *type, ofxCounter id );
static GList     *get_orphans( ofaIGetter *getter, const gchar *table );
static GList     *get_other_orphans( ofaIGetter *getter, const gchar *type, const gchar *column, const gchar *table );
static gboolean   concil_do_insert( ofoConcil *concil, const ofaIDBConnect *connect );
static gboolean   concil_do_insert_id( ofoConcil *concil, const gchar *type, ofxCounter id, const ofaIDBConnect *connect );
static gboolean   concil_do_delete( ofoConcil *concil, const ofaIDBConnect *connect );
static void       icollectionable_iface_init( myICollectionableInterface *iface );
static guint      icollectionable_get_interface_version( void );
static GList     *icollectionable_load_collection( void *user_data );

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
 * ofo_concil_get_dataset:
 * @getter: a #ofaIGetter instance.
 *
 * Returns *all* conciliation lines.
 *
 * The returned list is owned by the #myICollector of the application,
 * and should not be released by the caller.
 */
GList *
ofo_concil_get_dataset( ofaIGetter *getter )
{
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );

	return( my_icollector_collection_get( collector, OFO_TYPE_CONCIL, getter ));
}

/**
 * ofo_concil_get_by_id:
 * @getter: a #ofaIGetter instance.
 * @rec_id: the identifier of the requested conciliation group.
 *
 * Returns: the conciliation group, or %NULL.
 *
 * The returned conciliation group is owned by the #myICollector instance,
 * and should be released by the caller.
 */
ofoConcil *
ofo_concil_get_by_id( ofaIGetter *getter, ofxCounter rec_id )
{
	GList *dataset, *it;
	ofoConcil *concil;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	dataset = ofo_concil_get_dataset( getter );

	for( it=dataset ; it ; it=it->next ){
		concil = OFO_CONCIL( it->data );
		if( ofo_concil_get_id( concil ) == rec_id ){
			return( concil );
		}
	}

	return( NULL );
}

/**
 * ofo_concil_get_by_other_id:
 * @getter: a #ofaIGetter instance.
 * @type: the seatched type, an entry or a BAT line.
 * @other_id: the identifier of the searched type.
 *
 * Returns: the conciliation group, or %NULL.
 *
 * The returned conciliation group is owned by the #myICollector instance,
 * and should be released by the caller.
 */
ofoConcil *
ofo_concil_get_by_other_id( ofaIGetter *getter, const gchar *type, ofxCounter other_id )
{
	GList *dataset, *it;
	ofoConcil *concil;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( type ), NULL );

	dataset = ofo_concil_get_dataset( getter );

	for( it=dataset ; it ; it=it->next ){
		concil = OFO_CONCIL( it->data );
		if( ofo_concil_has_member( concil, type, other_id )){
			return( concil );
		}
	}

	return( NULL );
}

/**
 * ofo_concil_new:
 * @getter: a #ofaIGetter instance.
 */
ofoConcil *
ofo_concil_new( ofaIGetter *getter )
{
	ofoConcil *concil;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	concil = g_object_new( OFO_TYPE_CONCIL, "ofo-base-getter", getter, NULL );

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
 * ofo_concil_get_upd_user:
 * @concil:
 */
const gchar *
ofo_concil_get_upd_user( ofoConcil *concil )
{
	ofoConcilPrivate *priv;

	g_return_val_if_fail( concil && OFO_IS_CONCIL( concil ), NULL );
	g_return_val_if_fail( !OFO_BASE( concil )->prot->dispose_has_run, NULL );

	priv = ofo_concil_get_instance_private( concil );

	return(( const gchar * ) priv->user );
}

/**
 * ofo_concil_get_upd_stamp:
 * @concil:
 */
const GTimeVal *
ofo_concil_get_upd_stamp( ofoConcil *concil )
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
 * ofo_concil_set_upd_user:
 * @concil:
 * @user:
 */
void
ofo_concil_set_upd_user( ofoConcil *concil, const gchar *user )
{
	ofoConcilPrivate *priv;

	g_return_if_fail( concil && OFO_IS_CONCIL( concil ));
	g_return_if_fail( !OFO_BASE( concil )->prot->dispose_has_run );

	priv = ofo_concil_get_instance_private( concil );

	g_free( priv->user );
	priv->user = g_strdup( user );
}

/**
 * ofo_concil_set_upd_stamp:
 * @concil:
 * @stamp:
 */
void
ofo_concil_set_upd_stamp( ofoConcil *concil, const GTimeVal *stamp )
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
 * ofo_concil_get_concil_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of conciliation group identifiers which are
 * referenced in conciliation members, but do not (or no more) exist.
 *
 * The returned list should not be #ofo_concil_free_concil_orphans() by
 * the caller.
 */
GList *
ofo_concil_get_concil_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "OFA_T_CONCIL_IDS" ));
}

static GList *
get_orphans( ofaIGetter *getter, const gchar *table )
{
	ofaHub *hub;
	ofaIDBConnect *connect;
	GList *orphans;
	GSList *result, *irow, *icol;
	gchar *query;
	ofxCounter recid;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( table ), NULL );

	orphans = NULL;
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf( "SELECT DISTINCT(REC_ID) FROM %s "
			"	WHERE REC_ID NOT IN (SELECT REC_ID FROM OFA_T_CONCIL)", table );

	if( ofa_idbconnect_query_ex( connect, query, &result, FALSE )){
		for( irow=result ; irow ; irow=irow->next ){
			icol = irow->data;
			recid = atol(( const gchar * ) icol->data );
			orphans = g_list_prepend( orphans, ( gpointer ) recid );
		}
		ofa_idbconnect_free_results( result );
	}

	g_free( query );

	return( orphans );
}

/**
 * ofo_concil_get_bat_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of conciliation group identifiers which
 * referenced a BatLine in conciliation members, but this BatLine does
 * not (or no more) exists.
 *
 * The returned list should not be #ofo_concil_free_bat_orphans() by
 * the caller.
 */
GList *
ofo_concil_get_bat_orphans( ofaIGetter *getter )
{
	return( get_other_orphans( getter, CONCIL_TYPE_BAT, "BAT_LINE_ID", "OFA_T_BAT_LINES" ));
}

/**
 * ofo_concil_get_entry_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of conciliation group identifiers which
 * referenced an entries in conciliation members, but this entry does
 * not (or no more) exists.
 *
 * The returned list should not be #ofo_concil_free_entry_orphans() by
 * the caller.
 */
GList *
ofo_concil_get_entry_orphans( ofaIGetter *getter )
{
	return( get_other_orphans( getter, CONCIL_TYPE_ENTRY, "ENT_NUMBER", "OFA_T_ENTRIES" ));
}

static GList *
get_other_orphans( ofaIGetter *getter, const gchar *type, const gchar *column, const gchar *table )
{
	ofaHub *hub;
	ofaIDBConnect *connect;
	GList *orphans;
	GSList *result, *irow, *icol;
	gchar *query;
	ofxCounter recid;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( table ), NULL );

	orphans = NULL;
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf( "SELECT DISTINCT(REC_ID) FROM OFA_T_CONCIL_IDS "
			"	WHERE REC_IDS_TYPE='%s' AND REC_IDS_OTHER NOT IN (SELECT %s FROM %s)", type, column, table );

	if( ofa_idbconnect_query_ex( connect, query, &result, FALSE )){
		for( irow=result ; irow ; irow=irow->next ){
			icol = irow->data;
			recid = atol(( const gchar * ) icol->data );
			orphans = g_list_prepend( orphans, ( gpointer ) recid );
		}
		ofa_idbconnect_free_results( result );
	}

	g_free( query );

	return( orphans );
}

/**
 * ofo_concil_insert:
 * @concil:
 */
gboolean
ofo_concil_insert( ofoConcil *concil )
{
	static const gchar *thisfn = "ofo_concil_insert";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: concil=%p", thisfn, ( void * ) concil );

	g_return_val_if_fail( concil && OFO_IS_CONCIL( concil ), FALSE );
	g_return_val_if_fail( !OFO_BASE( concil )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( concil ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );
	concil_set_id( concil, ofo_counters_get_next_concil_id( getter ));

	/* rationale: see ofo-account.c */
	ofo_concil_get_dataset( getter );

	if( concil_do_insert( concil, ofa_hub_get_connect( hub ))){
		my_icollector_collection_add_object(
				ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( concil ), NULL, getter );
		g_signal_emit_by_name( signaler, SIGNALER_BASE_NEW, concil );
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
	stamp = my_stamp_to_str( ofo_concil_get_upd_stamp( concil ), MY_STAMP_YYMDHMS );

	query = g_strdup_printf(
			"INSERT INTO OFA_T_CONCIL "
			"	(REC_ID,REC_DVAL,REC_USER,REC_STAMP) VALUES "
			"	(%ld,'%s','%s','%s')",
			ofo_concil_get_id( concil ), sdate, ofo_concil_get_upd_user( concil ), stamp );

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( stamp );
	g_free( sdate );
	g_free( query );

	return( ok );
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
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: concil=%p, type=%s, id=%lu",
			thisfn, ( void * ) concil, type, id );

	g_return_val_if_fail( concil && OFO_IS_CONCIL( concil ), FALSE );
	g_return_val_if_fail( my_strlen( type ), FALSE );
	g_return_val_if_fail( !OFO_BASE( concil )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( concil ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	concil_add_other_id( concil, type, id );

	if( concil_do_insert_id( concil, type, id, ofa_hub_get_connect( hub ))){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, concil, NULL );
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
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: concil=%p", thisfn, ( void * ) concil );

	g_return_val_if_fail( concil && OFO_IS_CONCIL( concil ), FALSE );
	g_return_val_if_fail( !OFO_BASE( concil )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( concil ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( concil_do_delete( concil, ofa_hub_get_connect( hub ))){
		g_object_ref( concil );
		my_icollector_collection_remove_object( ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( concil ));
		g_signal_emit_by_name( signaler, SIGNALER_BASE_DELETED, concil );
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
	iface->load_collection = icollectionable_load_collection;
}

static guint
icollectionable_get_interface_version( void )
{
	return( 1 );
}

static GList *
icollectionable_load_collection( void *user_data )
{
	GList *list;
	ofaIGetter *getter;
	ofaHub *hub;
	ofaIDBConnect *connect;
	GSList *result, *irow, *icol;
	ofxCounter prev_id, id, other;
	gchar *type;
	GDate date;
	GTimeVal stamp;
	ofoConcil *concil;

	g_return_val_if_fail( user_data && OFA_IS_IGETTER( user_data ), NULL );

	getter = OFA_IGETTER( user_data );
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );
	prev_id = 0;
	concil = NULL;
	list = NULL;

	if( ofa_idbconnect_query_ex( connect, "SELECT "
			"a.REC_ID,b.REC_IDS_TYPE,b.REC_IDS_OTHER,a.REC_DVAL,a.REC_USER,a.REC_STAMP"
			"	FROM OFA_T_CONCIL a, OFA_T_CONCIL_IDS b WHERE a.REC_ID=b.REC_ID"
			"	ORDER BY a.REC_ID ASC", &result, TRUE )){

		for( irow=result ; irow ; irow=irow->next ){

			icol = ( GSList * ) irow->data;
			id = atol(( const gchar * ) icol->data );
			icol = icol->next;
			type = g_strdup(( const gchar * ) icol->data );
			icol = icol->next;
			other = atol(( const gchar * ) icol->data );

			if( id != prev_id ){
				concil = ofo_concil_new( getter );
				list = g_list_prepend( list, concil );
				concil_set_id( concil, id );
				icol = icol->next;
				ofo_concil_set_dval( concil,
						my_date_set_from_sql( &date, ( const gchar * ) icol->data ));
				icol = icol->next;
				ofo_concil_set_upd_user( concil, ( const gchar * ) icol->data );
				icol = icol->next;
				ofo_concil_set_upd_stamp( concil,
						my_stamp_set_from_sql( &stamp, ( const gchar * ) icol->data ));
				prev_id = id;
			}

			g_return_val_if_fail( concil && OFO_IS_CONCIL( concil ), NULL );
			concil_add_other_id( concil, type, other );
			g_free( type );
		}

		ofa_idbconnect_free_results( result );
	}

	return( list );
}
