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

#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-igetter.h"
#include "api/ofo-dossier.h"
#include "api/ofo-counter.h"

/* priv instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;
}
	ofoCounterPrivate;

static ofxCounter get_last_counter( ofoCounter *self, const gchar *key );
static ofxCounter get_next_counter( ofoCounter *self, const gchar *key );

G_DEFINE_TYPE_EXTENDED( ofoCounter, ofo_counter, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofoCounter ))

static void
counter_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_counter_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFO_IS_COUNTER( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_counter_parent_class )->finalize( instance );
}

static void
counter_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_COUNTER( instance ));
	ofoCounterPrivate *priv;

	priv = ofo_counter_get_instance_private( OFO_COUNTER( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_counter_parent_class )->dispose( instance );
}

static void
ofo_counter_init( ofoCounter *self )
{
	static const gchar *thisfn = "ofo_counter_init";
	ofoCounterPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofo_counter_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofo_counter_class_init( ofoCounterClass *klass )
{
	static const gchar *thisfn = "ofo_counter_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = counter_dispose;
	G_OBJECT_CLASS( klass )->finalize = counter_finalize;
}

/**
 * ofo_counter_new:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: a new #ofoCounter object.
 */
ofoCounter *
ofo_counter_new( ofaIGetter *getter )
{
	ofoCounter *counters;
	ofoCounterPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	counters = g_object_new( OFO_TYPE_COUNTER, NULL );

	priv = ofo_counter_get_instance_private( counters );

	priv->getter = getter;

	return( counters );
}

/**
 * ofo_counter_get_last_bat_id:
 * @counters: this #ofoCounter object.
 *
 * Returns: the last used BAT identifier.
 */
ofxCounter
ofo_counter_get_last_bat_id( ofoCounter *counters )
{
	return( get_last_counter( counters, "last-bat-id" ));
}

/**
 * ofo_counter_get_next_bat_id:
 * @counters: this #ofoCounter object.
 *
 * Returns: the next available BAT identifier.
 */
ofxCounter
ofo_counter_get_next_bat_id( ofoCounter *counters )
{
	return( get_next_counter( counters, "last-bat-id" ));
}

/**
 * ofo_counter_get_last_batline_id:
 * @counters: this #ofoCounter object.
 *
 * Returns: the last used BATLine identifier.
 */
ofxCounter
ofo_counter_get_last_batline_id( ofoCounter *counters )
{
	return( get_last_counter( counters, "last-batline-id" ));
}

/**
 * ofo_counter_get_next_batline_id:
 * @counters: this #ofoCounter object.
 *
 * Returns: the next available BATLine identifier.
 */
ofxCounter
ofo_counter_get_next_batline_id( ofoCounter *counters )
{
	return( get_next_counter( counters, "last-batline-id" ));
}

/**
 * ofo_counter_get_last_concil_id:
 * @counters: this #ofoCounter object.
 *
 * Returns: the last used Conciliation identifier.
 */
ofxCounter
ofo_counter_get_last_concil_id( ofoCounter *counters )
{
	return( get_last_counter( counters, "last-conciliation-id" ));
}

/**
 * ofo_counter_get_next_concil_id:
 * @counters: this #ofoCounter object.
 *
 * Returns: the next available Conciliation identifier.
 */
ofxCounter
ofo_counter_get_next_concil_id( ofoCounter *counters )
{
	return( get_next_counter( counters, "last-conciliation-id" ));
}

/**
 * ofo_counter_get_last_doc_id:
 * @counters: this #ofoCounter object.
 *
 * Returns: the last used Document identifier.
 */
ofxCounter
ofo_counter_get_last_doc_id( ofoCounter *counters )
{
	return( get_last_counter( counters, "last-document-id" ));
}

/**
 * ofo_counter_get_next_doc_id:
 * @counters: this #ofoCounter object.
 *
 * Returns: the next available Document identifier.
 */
ofxCounter
ofo_counter_get_next_doc_id( ofoCounter *counters )
{
	return( get_next_counter( counters, "last-document-id" ));
}

/**
 * ofo_counter_get_last_entry_id:
 * @counters: this #ofoCounter object.
 *
 * Returns: the last used Entry identifier.
 */
ofxCounter
ofo_counter_get_last_entry_id( ofoCounter *counters )
{
	return( get_last_counter( counters, "last-entry-id" ));
}

/**
 * ofo_counter_get_next_entry_id:
 * @counters: this #ofoCounter object.
 *
 * Returns: the next available Entry identifier.
 */
ofxCounter
ofo_counter_get_next_entry_id( ofoCounter *counters )
{
	return( get_next_counter( counters, "last-entry-id" ));
}

/**
 * ofo_counter_get_last_ope_id:
 * @counters: this #ofoCounter object.
 *
 * Returns: the last used Operation identifier.
 */
ofxCounter
ofo_counter_get_last_ope_id( ofoCounter *counters )
{
	return( get_last_counter( counters, "last-operation-id" ));
}

/**
 * ofo_counter_get_next_ope_id:
 * @counters: this #ofoCounter object.
 *
 * Returns: the next available Operation identifier.
 */
ofxCounter
ofo_counter_get_next_ope_id( ofoCounter *counters )
{
	return( get_next_counter( counters, "last-operation-id" ));
}

/**
 * ofo_counter_get_last_settlement_id:
 * @counters: this #ofoCounter object.
 *
 * Returns: the last used Settlement identifier.
 */
ofxCounter
ofo_counter_get_last_settlement_id( ofoCounter *counters )
{
	return( get_last_counter( counters, "last-settlement-id" ));
}

/**
 * ofo_counter_get_next_settlement_id:
 * @counters: this #ofoCounter object.
 *
 * Returns: the next available Settlement identifier.
 */
ofxCounter
ofo_counter_get_next_settlement_id( ofoCounter *counters )
{
	return( get_next_counter( counters, "last-settlement-id" ));
}

/**
 * ofo_counter_get_last_tiers_id:
 * @counters: this #ofoCounter object.
 *
 * Returns: the last used Tiers identifier.
 */
ofxCounter
ofo_counter_get_last_tiers_id( ofoCounter *counters )
{
	return( get_last_counter( counters, "last-tiers-id" ));
}

/**
 * ofo_counter_get_next_tiers_id:
 * @counters: this #ofoCounter object.
 *
 * Returns: the next available Tiers identifier.
 */
ofxCounter
ofo_counter_get_next_tiers_id( ofoCounter *counters )
{
	return( get_next_counter( counters, "last-tiers-id" ));
}

static ofxCounter
get_last_counter( ofoCounter *self, const gchar *key )
{
	ofoCounterPrivate *priv;
	ofaHub *hub;
	ofaIDBConnect *connect;
	gchar *query;
	GSList *results, *irow;
	ofxCounter counter;

	g_return_val_if_fail( self && OFO_IS_COUNTER( self ), 0 );
	g_return_val_if_fail( key && g_utf8_strlen( key, -1 ) > 0, 0 );

	priv = ofo_counter_get_instance_private( self );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	hub = ofa_igetter_get_hub( priv->getter );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), 0 );

	connect = ofa_hub_get_connect( hub );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), 0 );

	counter = 0;

	query = g_strdup_printf(
			"SELECT DOS_IDS_LAST FROM OFA_T_DOSSIER_IDS "
			"	WHERE DOS_ID=%u AND DOS_IDS_KEY='%s'", DOSSIER_ROW_ID, key );

	if( ofa_idbconnect_query_ex( connect, query, &results, TRUE )){
		irow = results->data;
		counter = atol(( const gchar * ) irow->data );
		ofa_idbconnect_free_results( results );
	}
	g_free( query );

	return( counter );
}

static ofxCounter
get_next_counter( ofoCounter *self, const gchar *key )
{
	ofoCounterPrivate *priv;
	ofaHub *hub;
	ofaIDBConnect *connect;
	gchar *query;
	ofxCounter counter;

	g_return_val_if_fail( self && OFO_IS_COUNTER( self ), 0 );
	g_return_val_if_fail( key && g_utf8_strlen( key, -1 ) > 0, 0 );

	priv = ofo_counter_get_instance_private( self );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	hub = ofa_igetter_get_hub( priv->getter );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), 0 );

	connect = ofa_hub_get_connect( hub );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), 0 );

	counter = get_last_counter( self, key );
	counter += 1;

	query = g_strdup_printf(
			"UPDATE OFA_T_DOSSIER_IDS "
			"	SET DOS_IDS_LAST=%lu "
			"	WHERE DOS_ID=%u AND DOS_IDS_KEY='%s'", counter, DOSSIER_ROW_ID, key );
	ofa_idbconnect_query( connect, query, TRUE );
	g_free( query );

	return( counter );
}
