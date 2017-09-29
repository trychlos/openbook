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

#include "my/my-icollector.h"
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-igetter.h"
#include "api/ofo-base.h"
#include "api/ofo-concil.h"
#include "api/ofo-dossier.h"

#include "core/ofa-iconcil.h"

#define ICONCIL_LAST_VERSION            1

static guint st_initializations         = 0;	/* interface initialization count */

static GType        register_type( void );
static void         interface_base_init( ofaIConcilInterface *klass );
static void         interface_base_finalize( ofaIConcilInterface *klass );
static const gchar *iconcil_get_type( const ofaIConcil *instance );
static ofxCounter   iconcil_get_id( const ofaIConcil *instance );

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
 * @type: the implementation's GType.
 *
 * Returns: the version number of this interface which is managed by
 * the @type implementation.
 *
 * Defaults to 1.
 *
 * Since: version 1.
 */
guint
ofa_iconcil_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_ICONCIL );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIConcilInterface * ) iface )->get_interface_version ){
		version = (( ofaIConcilInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIConcil::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_iconcil_get_concil:
 * @instance: an #ofaIConcil instance.
 *
 * Returns: the reconciliation group this @instance belongs to, or %NULL.
 */
ofoConcil *
ofa_iconcil_get_concil( const ofaIConcil *instance )
{
	ofaIGetter *getter;
	ofoConcil *concil;

	g_return_val_if_fail( instance && OFA_IS_ICONCIL( instance ), NULL );

	getter = ofo_base_get_getter( OFO_BASE( instance ));
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	concil = ofo_concil_get_by_other_id( getter, iconcil_get_type( instance ), iconcil_get_id( instance ));

	return( concil );
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
	ofaIGetter *getter;
	ofoConcil *concil;
	const ofaIDBConnect *connect;
	GTimeVal stamp;
	const gchar *userid;
	ofaHub *hub;

	g_return_val_if_fail( instance && OFA_IS_ICONCIL( instance ), NULL );

	getter = ofo_base_get_getter( OFO_BASE( instance ));
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );
	userid = ofa_idbconnect_get_account( connect );

	concil = ofo_concil_new( getter );
	ofo_concil_set_dval( concil, dval );
	ofo_concil_set_upd_user( concil, userid );
	ofo_concil_set_upd_stamp( concil, my_stamp_set_now( &stamp ));

	ofa_iconcil_new_concil_ex( instance, concil );

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
	g_return_if_fail( instance && OFA_IS_ICONCIL( instance ));
	g_return_if_fail( concil && OFO_IS_CONCIL( concil ));

	ofo_concil_insert( concil );
	ofo_concil_add_id( concil, iconcil_get_type( instance ), iconcil_get_id( instance ));
}

/**
 * ofa_iconcil_add_to_concil:
 * @instance:
 * @concil:
 */
void
ofa_iconcil_add_to_concil( ofaIConcil *instance, ofoConcil *concil )
{
	g_return_if_fail( instance && OFA_IS_ICONCIL( instance ));
	g_return_if_fail( concil && OFO_IS_CONCIL( concil ));

	ofo_concil_add_id( concil, iconcil_get_type( instance ), iconcil_get_id( instance ));
}

/**
 * ofa_iconcil_get_instance_type:
 * @instance: a #ofaIConcil object.
 *
 * Returns: the type of the @instance:
 * - 'E' for an entry,
 * - 'B' for a BAT line.
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
