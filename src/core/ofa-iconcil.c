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

#include "api/my-utils.h"
#include "api/ofo-concil.h"
#include "api/ofo-dossier.h"

#include "core/ofa-concil-collection.h"
#include "core/ofa-icollector.h"
#include "core/ofa-iconcil.h"

/* this structure is held by the object
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
 * @instance:
 * @dossier:
 *
 * Returns: the reconciliation group this instance belongs to, or %NULL.
 *
 * The reconciliation group is first searched in the in-memory
 * collection which is attached to the dossier, then only is requested
 * from the database.
 */
ofoConcil *
ofa_iconcil_get_concil( const ofaIConcil *instance, ofoDossier *dossier )
{
	ofoConcil *concil;
	GObject *collection;

	g_return_val_if_fail( instance && OFA_IS_ICONCIL( instance ), NULL );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	collection = ofa_icollector_get_object( OFA_ICOLLECTOR( dossier ), OFA_TYPE_CONCIL_COLLECTION );
	g_return_val_if_fail( collection && OFA_IS_CONCIL_COLLECTION( collection ), NULL );

	concil = ofa_concil_collection_get_by_other_id(
					OFA_CONCIL_COLLECTION( collection ),
					iconcil_get_type( instance ),
					iconcil_get_id( instance ),
					dossier );

	return( concil );
}

/**
 * ofa_iconcil_new_concil:
 * @instance:
 * @dval:
 * @dossier:
 *
 * Returns: a newly created conciliation group from the @instance.
 */
ofoConcil *
ofa_iconcil_new_concil( ofaIConcil *instance, const GDate *dval, ofoDossier *dossier )
{
	ofoConcil *concil;
	GTimeVal stamp;

	g_return_val_if_fail( instance && OFA_IS_ICONCIL( instance ), NULL );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	concil = ofo_concil_new();

	ofo_concil_set_dval( concil, dval );
	ofo_concil_set_user( concil, ofo_dossier_get_user( dossier ));
	ofo_concil_set_stamp( concil, my_utils_stamp_set_now( &stamp ));

	ofa_iconcil_new_concil_ex( instance, concil, dossier );

	return( concil );
}

/**
 * ofa_iconcil_new_concil_ex:
 * @instance:
 * @concil:
 * @dossier:
 *
 * An #ofoConcil object is already set with dval, user and stamp.
 * Add the ofaIConcil instance to it and write in DBMS.
 */
void
ofa_iconcil_new_concil_ex( ofaIConcil *instance, ofoConcil *concil, ofoDossier *dossier )
{
	GObject *collection;

	g_return_if_fail( instance && OFA_IS_ICONCIL( instance ));
	g_return_if_fail( concil && OFO_IS_CONCIL( concil ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	ofo_concil_insert( concil, dossier );
	ofo_concil_add_id( concil, iconcil_get_type( instance ), iconcil_get_id( instance ), dossier );

	collection = ofa_icollector_get_object( OFA_ICOLLECTOR( dossier ), OFA_TYPE_CONCIL_COLLECTION );
	g_return_if_fail( collection && OFA_IS_CONCIL_COLLECTION( collection ));

	ofa_concil_collection_add( OFA_CONCIL_COLLECTION( collection ), concil );
}

/**
 * ofa_iconcil_add_to_concil:
 * @instance:
 * @concil:
 * @dossier:
 */
void
ofa_iconcil_add_to_concil( ofaIConcil *instance, ofoConcil *concil, ofoDossier *dossier )
{
	g_return_if_fail( instance && OFA_IS_ICONCIL( instance ));
	g_return_if_fail( concil && OFO_IS_CONCIL( concil ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	ofo_concil_add_id( concil, iconcil_get_type( instance ), iconcil_get_id( instance ), dossier );
}

/**
 * ofa_iconcil_remove_concil:
 * @concil:
 * @dossier:
 */
void
ofa_iconcil_remove_concil( ofoConcil *concil, ofoDossier *dossier )
{
	GObject *collection;

	g_return_if_fail( concil && OFO_IS_CONCIL( concil ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( ofo_concil_delete( concil, dossier )){

		collection = ofa_icollector_get_object( OFA_ICOLLECTOR( dossier ), OFA_TYPE_CONCIL_COLLECTION );
		g_return_if_fail( collection && OFA_IS_CONCIL_COLLECTION( collection ));

		ofa_concil_collection_remove( OFA_CONCIL_COLLECTION( collection ), concil );
	}
}

static const gchar *
iconcil_get_type( const ofaIConcil *instance )
{
	const gchar *type;

	g_return_val_if_fail( OFA_ICONCIL_GET_INTERFACE( instance )->get_object_type, -1 );

	type = OFA_ICONCIL_GET_INTERFACE( instance )->get_object_type( instance );

	return( type );
}

static ofxCounter
iconcil_get_id( const ofaIConcil *instance )
{
	ofxCounter id;

	g_return_val_if_fail( OFA_ICONCIL_GET_INTERFACE( instance )->get_object_id, -1 );

	id = OFA_ICONCIL_GET_INTERFACE( instance )->get_object_id( instance );

	return( id );
}
