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

#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-igetter.h"
#include "api/ofo-dossier.h"
#include "api/ofo-counters.h"

/* priv instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;

	/* runtime
	 */
	ofxCounter  last_bat_id;
	ofxCounter  last_batline_id;
	ofxCounter  last_concil_id;
	ofxCounter  last_doc_id;
	ofxCounter  last_entry_id;
	ofxCounter  last_ope_id;
	ofxCounter  last_settlement_id;
	ofxCounter  last_tiers_id;
}
	ofoCountersPrivate;

/* known keys in alpha order */
#define st_bat_id           "last-bat-id"
#define st_batline_id       "last-batline-id"
#define st_concil_id        "last-conciliation-id"
#define st_doc_id           "last-document-id"
#define st_entry_id         "last-entry-id"
#define st_ope_id           "last-operation-id"
#define st_settlement_id    "last-settlement-id"
#define st_tiers_id         "last-tiers-id"

/* list of known keys in alpha order */
static const gchar *st_list[] = {
		st_bat_id,
		st_batline_id,
		st_concil_id,
		st_doc_id,
		st_entry_id,
		st_ope_id,
		st_settlement_id,
		st_tiers_id,
		NULL
};

static void        read_counters( ofoCounters *self );
static ofxCounter  read_counter_by_key( ofoCounters *self, ofaIDBConnect *connect, const gchar *key );
static ofxCounter  get_last_counter( ofaIGetter *getter, const gchar *key );
static ofxCounter *get_last_counter_ptr( ofoCounters *self, ofoCountersPrivate *priv, const gchar *key );
static ofxCounter  get_next_counter( ofaIGetter *getter, const gchar *key );

G_DEFINE_TYPE_EXTENDED( ofoCounters, ofo_counters, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofoCounters ))

static void
counters_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_counters_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFO_IS_COUNTERS( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_counters_parent_class )->finalize( instance );
}

static void
counters_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_COUNTERS( instance ));
	ofoCountersPrivate *priv;

	priv = ofo_counters_get_instance_private( OFO_COUNTERS( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_counters_parent_class )->dispose( instance );
}

static void
ofo_counters_init( ofoCounters *self )
{
	static const gchar *thisfn = "ofo_counters_init";
	ofoCountersPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofo_counters_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofo_counters_class_init( ofoCountersClass *klass )
{
	static const gchar *thisfn = "ofo_counters_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = counters_dispose;
	G_OBJECT_CLASS( klass )->finalize = counters_finalize;
}

/**
 * ofo_counters_new:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: a new #ofoCounters object.
 */
ofoCounters *
ofo_counters_new( ofaIGetter *getter )
{
	ofoCounters *counters;
	ofoCountersPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	counters = g_object_new( OFO_TYPE_COUNTERS, NULL );

	priv = ofo_counters_get_instance_private( counters );

	priv->getter = getter;

	read_counters( counters );

	return( counters );
}

static void
read_counters( ofoCounters *self )
{
	ofoCountersPrivate *priv;
	ofaHub *hub;
	ofaIDBConnect *connect;

	priv = ofo_counters_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	connect = ofa_hub_get_connect( hub );
	g_return_if_fail( connect && OFA_IS_IDBCONNECT( connect ));

	priv->last_bat_id        = read_counter_by_key( self, connect, st_bat_id );
	priv->last_batline_id    = read_counter_by_key( self, connect, st_batline_id );
	priv->last_concil_id     = read_counter_by_key( self, connect, st_concil_id );
	priv->last_doc_id        = read_counter_by_key( self, connect, st_doc_id );
	priv->last_entry_id      = read_counter_by_key( self, connect, st_entry_id );
	priv->last_ope_id        = read_counter_by_key( self, connect, st_ope_id );
	priv->last_settlement_id = read_counter_by_key( self, connect, st_settlement_id );
	priv->last_tiers_id      = read_counter_by_key( self, connect, st_tiers_id );
}

static ofxCounter
read_counter_by_key( ofoCounters *self, ofaIDBConnect *connect, const gchar *key )
{
	ofxCounter number;
	gchar *query;
	GSList *results, *irow;

	number = 0;

	query = g_strdup_printf(
			"SELECT DOS_IDS_LAST FROM OFA_T_DOSSIER_IDS "
			"	WHERE DOS_ID=%u AND DOS_IDS_KEY='%s'", DOSSIER_ROW_ID, key );

	if( ofa_idbconnect_query_ex( connect, query, &results, TRUE )){
		irow = results->data;
		number = atol(( const gchar * ) irow->data );
		ofa_idbconnect_free_results( results );
	}
	g_free( query );

	return( number );
}

/**
 * ofo_counters_get_last_bat_id:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the last used BAT identifier.
 */
ofxCounter
ofo_counters_get_last_bat_id( ofaIGetter *getter )
{
	return( get_last_counter( getter, st_bat_id ));
}

/**
 * ofo_counters_get_next_bat_id:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the next available BAT identifier.
 */
ofxCounter
ofo_counters_get_next_bat_id( ofaIGetter *getter )
{
	return( get_next_counter( getter, st_bat_id ));
}

/**
 * ofo_counters_get_last_batline_id:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the last used BATLine identifier.
 */
ofxCounter
ofo_counters_get_last_batline_id( ofaIGetter *getter )
{
	return( get_last_counter( getter, st_batline_id ));
}

/**
 * ofo_counters_get_next_batline_id:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the next available BATLine identifier.
 */
ofxCounter
ofo_counters_get_next_batline_id( ofaIGetter *getter )
{
	return( get_next_counter( getter, st_batline_id ));
}

/**
 * ofo_counters_get_last_concil_id:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the last used Conciliation identifier.
 */
ofxCounter
ofo_counters_get_last_concil_id( ofaIGetter *getter )
{
	return( get_last_counter( getter, st_concil_id ));
}

/**
 * ofo_counters_get_next_concil_id:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the next available Conciliation identifier.
 */
ofxCounter
ofo_counters_get_next_concil_id( ofaIGetter *getter )
{
	return( get_next_counter( getter, st_concil_id ));
}

/**
 * ofo_counters_get_last_doc_id:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the last used Document identifier.
 */
ofxCounter
ofo_counters_get_last_doc_id( ofaIGetter *getter )
{
	return( get_last_counter( getter, st_doc_id ));
}

/**
 * ofo_counters_get_next_doc_id:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the next available Document identifier.
 */
ofxCounter
ofo_counters_get_next_doc_id( ofaIGetter *getter )
{
	return( get_next_counter( getter, st_doc_id ));
}

/**
 * ofo_counters_get_last_entry_id:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the last used Entry identifier.
 */
ofxCounter
ofo_counters_get_last_entry_id( ofaIGetter *getter )
{
	return( get_last_counter( getter, st_entry_id ));
}

/**
 * ofo_counters_get_next_entry_id:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the next available Entry identifier.
 */
ofxCounter
ofo_counters_get_next_entry_id( ofaIGetter *getter )
{
	return( get_next_counter( getter, st_entry_id ));
}

/**
 * ofo_counters_get_last_ope_id:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the last used Operation identifier.
 */
ofxCounter
ofo_counters_get_last_ope_id( ofaIGetter *getter )
{
	return( get_last_counter( getter, st_ope_id ));
}

/**
 * ofo_counters_get_next_ope_id:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the next available Operation identifier.
 */
ofxCounter
ofo_counters_get_next_ope_id( ofaIGetter *getter )
{
	return( get_next_counter( getter, st_ope_id ));
}

/**
 * ofo_counters_get_last_settlement_id:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the last used Settlement identifier.
 */
ofxCounter
ofo_counters_get_last_settlement_id( ofaIGetter *getter )
{
	return( get_last_counter( getter, st_settlement_id ));
}

/**
 * ofo_counters_get_next_settlement_id:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the next available Settlement identifier.
 */
ofxCounter
ofo_counters_get_next_settlement_id( ofaIGetter *getter )
{
	return( get_next_counter( getter, st_settlement_id ));
}

/**
 * ofo_counters_get_last_tiers_id:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the last used Tiers identifier.
 */
ofxCounter
ofo_counters_get_last_tiers_id( ofaIGetter *getter )
{
	return( get_last_counter( getter, st_tiers_id ));
}

/**
 * ofo_counters_get_next_tiers_id:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the next available Tiers identifier.
 */
ofxCounter
ofo_counters_get_next_tiers_id( ofaIGetter *getter )
{
	return( get_next_counter( getter, st_tiers_id ));
}

static ofxCounter
get_last_counter( ofaIGetter *getter, const gchar *key )
{
	ofoCounters *counters;
	ofoCountersPrivate *priv;
	ofaHub *hub;
	ofxCounter *pnumber;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), 0 );
	g_return_val_if_fail( my_strlen( key ), 0 );

	hub = ofa_igetter_get_hub( getter );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), 0 );

	counters = ofa_hub_get_counters( hub );
	g_return_val_if_fail( counters && OFO_IS_COUNTERS( counters ), 0 );

	priv = ofo_counters_get_instance_private( counters );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	pnumber = get_last_counter_ptr( counters, priv, key );

	return( *pnumber );
}

static ofxCounter*
get_last_counter_ptr( ofoCounters *self, ofoCountersPrivate *priv, const gchar *key )
{
	if( !my_collate( key, st_bat_id )){
		return( &priv->last_bat_id );

	} else if( !my_collate( key, st_batline_id )){
		return( &priv->last_batline_id );

	} else if( !my_collate( key, st_concil_id )){
		return( &priv->last_concil_id );

	} else if( !my_collate( key, st_doc_id )){
		return( &priv->last_doc_id );

	} else if( !my_collate( key, st_entry_id )){
		return( &priv->last_entry_id );

	} else if( !my_collate( key, st_ope_id )){
		return( &priv->last_ope_id );

	} else if( !my_collate( key, st_settlement_id )){
		return( &priv->last_settlement_id );

	} else if( !my_collate( key, st_tiers_id )){
		return( &priv->last_tiers_id );
	}

	return( 0 );
}

static ofxCounter
get_next_counter( ofaIGetter *getter, const gchar *key )
{
	ofoCounters *counters;
	ofoCountersPrivate *priv;
	ofaHub *hub;
	ofaIDBConnect *connect;
	gchar *query;
	ofxCounter *pnumber;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), 0 );
	g_return_val_if_fail( my_strlen( key ), 0 );

	hub = ofa_igetter_get_hub( getter );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), 0 );

	counters = ofa_hub_get_counters( hub );
	g_return_val_if_fail( counters && OFO_IS_COUNTERS( counters ), 0 );

	priv = ofo_counters_get_instance_private( counters );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	connect = ofa_hub_get_connect( hub );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), 0 );

	pnumber = get_last_counter_ptr( counters, priv, key );
	*pnumber += 1;

	query = g_strdup_printf(
			"UPDATE OFA_T_DOSSIER_IDS "
			"	SET DOS_IDS_LAST=%lu "
			"	WHERE DOS_ID=%u AND DOS_IDS_KEY='%s'", *pnumber, DOSSIER_ROW_ID, key );
	ofa_idbconnect_query( connect, query, TRUE );
	g_free( query );

	return( *pnumber );
}

/**
 * ofo_counters_get_count:
 *
 * Returns: the count of defined internal identifiers.
 */
guint
ofo_counters_get_count( void )
{
	guint i;

	for( i=0 ; st_list[i] ; ++ i )
		;

	return( i );
}

/**
 * ofo_counters_get_key:
 * @getter: a #ofaIGetter instance.
 * idx: the index of the requested key, counted from zero.
 *
 * Returns: the key at @idx.
 */
const gchar *
ofo_counters_get_key( ofaIGetter *getter, guint idx )
{
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	return( st_list[idx] );
}

/**
 * ofo_counters_get_last_value:
 * @getter: a #ofaIGetter instance.
 * idx: the index of the requested key, counted from zero.
 *
 * Returns: the value of the key at @idx.
 */
ofxCounter
ofo_counters_get_last_value( ofaIGetter *getter, guint idx )
{
	static const gchar *thisfn = "ofo_counters_get_value";
	const gchar *key;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), 0 );

	key = ofo_counters_get_key( getter, idx );

	if( !my_collate( key, st_bat_id )){
		return( ofo_counters_get_last_bat_id( getter ));

	} else if( !my_collate( key, st_batline_id )){
		return( ofo_counters_get_last_batline_id( getter ));

	} else if( !my_collate( key, st_concil_id )){
		return( ofo_counters_get_last_concil_id( getter ));

	} else if( !my_collate( key, st_doc_id )){
		return( ofo_counters_get_last_doc_id( getter ));

	} else if( !my_collate( key, st_entry_id )){
		return( ofo_counters_get_last_entry_id( getter ));

	} else if( !my_collate( key, st_ope_id )){
		return( ofo_counters_get_last_ope_id( getter ));

	} else if( !my_collate( key, st_settlement_id )){
		return( ofo_counters_get_last_settlement_id( getter ));

	} else if( !my_collate( key, st_tiers_id )){
		return( ofo_counters_get_last_tiers_id( getter ));
	}

	g_warning( "%s: unknown or invalid index idx=%u", thisfn, idx );
	return( 0 );
}
