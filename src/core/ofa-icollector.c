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

#include "core/ofa-icollector.h"

#define ICOLLECTOR_LAST_VERSION           1
#define ICOLLECTOR_DATA                   "ofa-icollector-data"

/* a data structure attached to the GObject implementation
 */
typedef struct {
	GList *objects;
}
	sICollector;

static guint st_initializations = 0;	/* interface initialization count */

static GType    register_type( void );
static void     interface_base_init( ofaICollectorInterface *klass );
static void     interface_base_finalize( ofaICollectorInterface *klass );
static GObject *icollector_get_object_by_type( ofaICollector *collector, GType type );

/**
 * ofa_icollector_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_icollector_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_icollector_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_icollector_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaICollectorInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaICollector", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaICollectorInterface *klass )
{
	static const gchar *thisfn = "ofa_icollector_interface_base_init";

	if( !st_initializations ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	if( st_initializations == 0 ){

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaICollectorInterface *klass )
{
	static const gchar *thisfn = "ofa_icollector_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_icollector_get_interface_last_version:
 * @instance: this #ofaICollector instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_icollector_get_interface_last_version( void )
{
	return( ICOLLECTOR_LAST_VERSION );
}

/**
 * ofa_icollector_get_interface_version:
 * @collector:
 *
 * Returns: the version number of this interface implemented by the
 * @collector instance.
 *
 * Defaults to 1.
 */
guint
ofa_icollector_get_interface_version( const ofaICollector *collector )
{
	guint version;

	version = 1;

	if( OFA_ICOLLECTOR_GET_INTERFACE( collector )->get_interface_version ){
		version = OFA_ICOLLECTOR_GET_INTERFACE( collector )->get_interface_version( collector );
	}

	return( version );
}

/**
 * ofa_icollector_get_object:
 * @collector:
 * @type:
 *
 * Returns: the object attached to the @collector for this @type,
 * creating a new one if none exists yet.
 */
GObject *
ofa_icollector_get_object( ofaICollector *collector, GType type )
{
	GObject *object;

	g_return_val_if_fail( collector && OFA_IS_ICOLLECTOR( collector ), NULL );

	object = icollector_get_object_by_type( collector, type );

	return( object );
}

/**
 * ofa_icollector_init:
 * @collector:
 *
 * Initialize the internal data structures of the interface.
 *
 * This should be called by the implementation at instance
 * initialization time.
 */
void
ofa_icollector_init( ofaICollector *collector )
{
	g_return_if_fail( collector && OFA_IS_ICOLLECTOR( collector ));
}

/**
 * ofa_icollector_dispose:
 * @collector:
 *
 * Free the internal data structures of the interface.
 *
 * This should be called by the implementation at instance (first)
 * dispose time.
 */
void
ofa_icollector_dispose( ofaICollector *collector )
{
	sICollector *sdata;

	g_return_if_fail( collector && OFA_IS_ICOLLECTOR( collector ));

	sdata = ( sICollector * ) g_object_get_data( G_OBJECT( collector ), ICOLLECTOR_DATA );

	if( sdata ){
		g_list_free_full( sdata->objects, ( GDestroyNotify ) g_object_unref );
		g_free( sdata );
		g_object_set_data( G_OBJECT( collector ), ICOLLECTOR_DATA, NULL );
	}
}

static GObject *
icollector_get_object_by_type( ofaICollector *collector, GType type )
{
	sICollector *sdata;
	GList *it;
	GObject *found;

	sdata = ( sICollector * ) g_object_get_data( G_OBJECT( collector ), ICOLLECTOR_DATA );
	if( !sdata ){
		sdata = g_new0( sICollector, 1 );
		g_object_set_data( G_OBJECT( collector ), ICOLLECTOR_DATA, sdata );
	}

	found = NULL;

	for( it=sdata->objects ; it ; it=it->next ){
		if( G_OBJECT_TYPE( it->data ) == type ){
			found = it->data;
			break;
		}
	}

	if( !found ){
		found = g_object_new( type, NULL );
		sdata->objects = g_list_prepend( sdata->objects, found );
	}

	return( found );
}
