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

#include "api/ofa-icollectionable.h"

#define ICOLLECTIONABLE_LAST_VERSION    1

#define ICOLLECTIONABLE_DATA            "ofa-icollectionable-data"

/* a data structure attached to the GObject implementation
 */
typedef struct {
	GList *objects;
}
	sICollectionable;

static guint st_initializations = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaICollectionableInterface *klass );
static void  interface_base_finalize( ofaICollectionableInterface *klass );

/**
 * ofa_icollectionable_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_icollectionable_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_icollectionable_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_icollectionable_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaICollectionableInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaICollectionable", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaICollectionableInterface *klass )
{
	static const gchar *thisfn = "ofa_icollectionable_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaICollectionableInterface *klass )
{
	static const gchar *thisfn = "ofa_icollectionable_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_icollectionable_get_interface_last_version:
 * @instance: this #ofaICollectionable instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_icollectionable_get_interface_last_version( void )
{
	return( ICOLLECTIONABLE_LAST_VERSION );
}

/**
 * ofa_icollectionable_get_interface_version:
 * @instance: this #ofaICollectionable instance.
 *
 * Returns: the version number of this interface implemented by the
 * @instance.
 *
 * Defaults to 1.
 */
guint
ofa_icollectionable_get_interface_version( const ofaICollectionable *instance )
{
	static const gchar *thisfn = "ofa_icollectionable_get_interface_version";

	g_return_val_if_fail( instance && OFA_IS_ICOLLECTIONABLE( instance ), 1 );

	if( OFA_ICOLLECTIONABLE_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_ICOLLECTIONABLE_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: ofaIDBModel instance %p does not provide 'get_last_version()' method",
			thisfn, ( void * ) instance );
	return( 1 );
}

/**
 * ofa_icollectionable_load_collection:
 * @instance: this #ofaICollectionable instance.
 * @hub: the #ofaHub object.
 *
 * Returns: the collection of desired objects, or %NULL.
 */
GList *
ofa_icollectionable_load_collection( const ofaICollectionable *instance, ofaHub *hub )
{
	static const gchar *thisfn = "ofa_icollectionable_load_collection";

	g_return_val_if_fail( instance && OFA_IS_ICOLLECTIONABLE( instance ), NULL );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	if( OFA_ICOLLECTIONABLE_GET_INTERFACE( instance )->load_collection ){
		return( OFA_ICOLLECTIONABLE_GET_INTERFACE( instance )->load_collection( instance, hub ));
	}

	g_info( "%s: ofaIDBModel instance %p does not provide 'load_collection()' method",
			thisfn, ( void * ) instance );
	return( NULL );
}
