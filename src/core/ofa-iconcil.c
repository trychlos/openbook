/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-icollector.h"
#include "api/ofo-base.h"
#include "api/ofo-concil.h"
#include "api/ofo-dossier.h"

#include "core/ofa-iconcil.h"

/* this structure is held by the object
 * this is primarily thought to optimize the ofa_iconcil_get_concil()
 * method.
 */
typedef struct {
	ofoConcil  *concil;
	gchar      *type;
}
	sIConcil;

#define ICONCIL_LAST_VERSION            1
#define ICONCIL_DATA                    "ofa-iconcil-data"

static guint st_initializations         = 0;	/* interface initialization count */

static GType        register_type( void );
static void         interface_base_init( ofaIConcilInterface *klass );
static void         interface_base_finalize( ofaIConcilInterface *klass );
static const gchar *iconcil_get_type( const ofaIConcil *instance );
static ofxCounter   iconcil_get_id( const ofaIConcil *instance );
static ofoConcil   *get_concil_from_collection( GList *collection, const gchar *type, ofxCounter id );
static sIConcil    *get_iconcil_data( const ofaIConcil *instance, gboolean search );
static void         on_instance_finalized( sIConcil *sdata, GObject *finalized_instance );

/**
 * ofa_iconcil_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iconcil_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iconcil_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iconcil_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIConcilInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIConcil", &info, 0 );

	g_type_interface_add_prerequisite( type, OFO_TYPE_BASE );

	return( type );
}

static void
interface_base_init( ofaIConcilInterface *klass )
{
	static const gchar *thisfn = "ofa_iconcil_interface_base_init";

	if( !st_initializations ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	if( st_initializations == 0 ){

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIConcilInterface *klass )
{
	static const gchar *thisfn = "ofa_iconcil_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iconcil_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iconcil_get_interface_last_version( void )
{
	return( ICONCIL_LAST_VERSION );
}

/**
 * ofa_iconcil_get_interface_version:
 * @instance: this #ofaIConcil instance.
 *
 * Returns: the version number implemented by the object.
 *
 * Defaults to 1.
 */
guint
ofa_iconcil_get_interface_version( const ofaIConcil *instance )
{
	g_return_val_if_fail( instance && OFA_IS_ICONCIL( instance ), 0 );

	if( OFA_ICONCIL_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_ICONCIL_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	return( 1 );
}

/**
 * ofa_iconcil_get_concil:
 * @instance: a #ofaIConcil object.
 *
 * Returns: the reconciliation group this instance belongs to, or %NULL.
 *
 * The reconciliation group is first searched in the attached sIConcil
 * structure.
 * When first searching for this structure, the conciliation group is
 * searched for in the in-memory collection which is attached to the
 * ICollector, then only is requested from the database.
 */
ofoConcil *
ofa_iconcil_get_concil( const ofaIConcil *instance )
{
	sIConcil *sdata;

	g_return_val_if_fail( instance && OFA_IS_ICONCIL( instance ), NULL );

	sdata = get_iconcil_data( instance, TRUE );

	return( sdata->concil );
}

/**
 * ofa_iconcil_new_concil:
 * @instance:
 * @dval:
 *
 * Returns: a newly created conciliation group from the @instance.
 */
ofoConcil *
ofa_iconcil_new_concil( ofaIConcil *instance, const GDate *dval )
{
	ofaHub *hub;
	ofoConcil *concil;
	const ofaIDBConnect *connect;
	GTimeVal stamp;
	gchar *userid;

	g_return_val_if_fail( instance && OFA_IS_ICONCIL( instance ), NULL );

	hub = ofo_base_get_hub( OFO_BASE( instance ));
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	connect = ofa_hub_get_connect( hub );
	userid = ofa_idbconnect_get_account( connect );

	concil = ofo_concil_new();
	ofo_concil_set_dval( concil, dval );
	ofo_concil_set_user( concil, userid );
	ofo_concil_set_stamp( concil, my_utils_stamp_set_now( &stamp ));

	ofa_iconcil_new_concil_ex( instance, concil );

	g_free( userid );

	return( concil );
}

/**
 * ofa_iconcil_new_concil_ex:
 * @instance:
 * @concil:
 *
 * An #ofoConcil object is already set with dval, user and stamp.
 * Add the ofaIConcil instance to it and write in DBMS.
 */
void
ofa_iconcil_new_concil_ex( ofaIConcil *instance, ofoConcil *concil )
{
	ofaHub *hub;
	sIConcil *sdata;

	g_return_if_fail( instance && OFA_IS_ICONCIL( instance ));
	g_return_if_fail( concil && OFO_IS_CONCIL( concil ));

	hub = ofo_base_get_hub( OFO_BASE( instance ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	sdata = get_iconcil_data( instance, FALSE );
	g_return_if_fail( !sdata->concil );

	ofo_concil_insert( concil, hub );
	ofo_concil_add_id( concil, iconcil_get_type( instance ), iconcil_get_id( instance ));

	sdata->concil = concil;
}

/**
 * ofa_iconcil_add_to_concil:
 * @instance:
 * @concil:
 */
void
ofa_iconcil_add_to_concil( ofaIConcil *instance, ofoConcil *concil )
{
	sIConcil *sdata;

	g_return_if_fail( instance && OFA_IS_ICONCIL( instance ));
	g_return_if_fail( concil && OFO_IS_CONCIL( concil ));

	sdata = get_iconcil_data( instance, FALSE );
	g_return_if_fail( !sdata->concil );

	ofo_concil_add_id( concil, iconcil_get_type( instance ), iconcil_get_id( instance ));

	sdata->concil = concil;
}

/**
 * ofa_iconcil_remove_concil:
 * @instance:
 * @concil: [allow-none]:
 *
 * Clear the data attached to the @instance, and delete the @concil from
 * the database.
 */
void
ofa_iconcil_remove_concil( ofaIConcil *instance, ofoConcil *concil )
{
	sIConcil *sdata;

	g_return_if_fail( instance && OFA_IS_ICONCIL( instance ));

	sdata = get_iconcil_data( instance, FALSE );
	sdata->concil = NULL;

	if( concil ){
		g_return_if_fail( OFO_IS_CONCIL( concil ));
		ofo_concil_delete( concil );
	}
}

/**
 * ofa_iconcil_get_instance_type:
 * @instance: a #ofaIConcil object.
 *
 * Returns: the type of the @instance.
 */
const gchar *
ofa_iconcil_get_instance_type( const ofaIConcil *instance )
{
	return( iconcil_get_type( instance ));
}

static const gchar *
iconcil_get_type( const ofaIConcil *instance )
{
	const gchar *type;

	g_return_val_if_fail( OFA_ICONCIL_GET_INTERFACE( instance )->get_object_type, NULL );

	type = OFA_ICONCIL_GET_INTERFACE( instance )->get_object_type( instance );

	return( type );
}

/**
 * ofa_iconcil_get_instance_id:
 * @instance: a #ofaIConcil object.
 *
 * Returns: the type of the @instance.
 */
ofxCounter
ofa_iconcil_get_instance_id( const ofaIConcil *instance )
{
	return( iconcil_get_id( instance ));
}

static ofxCounter
iconcil_get_id( const ofaIConcil *instance )
{
	ofxCounter id;

	g_return_val_if_fail( OFA_ICONCIL_GET_INTERFACE( instance )->get_object_id, -1 );

	id = OFA_ICONCIL_GET_INTERFACE( instance )->get_object_id( instance );

	return( id );
}

static ofoConcil *
get_concil_from_collection( GList *collection, const gchar *type, ofxCounter id )
{
	GList *it;
	ofoConcil *concil;

	for( it=collection ; it ; it=it->next ){
		concil = ( ofoConcil * ) it->data;
		g_return_val_if_fail( concil && OFO_IS_CONCIL( concil ), NULL );
		if( ofo_concil_has_member( concil, type, id )){
			return( concil );
		}
	}

	return( NULL );
}

static sIConcil *
get_iconcil_data( const ofaIConcil *instance, gboolean search )
{
	sIConcil *sdata;
	ofaHub *hub;
	GList *collection;

	sdata = ( sIConcil * ) g_object_get_data( G_OBJECT( instance ), ICONCIL_DATA );

	if( !sdata ){
		sdata = g_new0( sIConcil, 1 );
		g_object_set_data( G_OBJECT( instance ), ICONCIL_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );
		sdata->type = g_strdup( iconcil_get_type( instance ));

		if( search ){
			hub = ofo_base_get_hub( OFO_BASE( instance ));
			g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

			collection = ofa_icollector_get_collection( OFA_ICOLLECTOR( hub ), hub, OFO_TYPE_CONCIL );
			sdata->concil = get_concil_from_collection( collection, sdata->type, iconcil_get_id( instance ));

			if( !sdata->concil ){
				sdata->concil = ofo_concil_get_by_other_id( hub, sdata->type, iconcil_get_id( instance ));
			}
		}
	}

	return( sdata );
}

/*
 * ofaIConcil instance finalization
 */
static void
on_instance_finalized( sIConcil *sdata, GObject *finalized_instance )
{
	static const gchar *thisfn = "ofa_iconcil_on_instance_finalized";

	g_debug( "%s: sdata=%p, finalized_instance=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_instance );

	g_free( sdata->type );
	g_free( sdata );
}
