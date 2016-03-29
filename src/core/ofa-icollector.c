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

#include "api/ofa-hub.h"
#include "api/ofa-icollectionable.h"
#include "api/ofa-icollector.h"

#define ICOLLECTOR_LAST_VERSION           1

#define ICOLLECTOR_DATA                   "ofa-icollector-data"

/* a data structure attached to the implementation
 * @collections: the collections of #ofaICollectionable objects.
 */
typedef struct {
	GList *collections;
}
	sICollector;

/* the data structure which defined the collection
 */
typedef struct {
	GType  type;
	GList *list;
}
	sCollection;

static guint st_initializations = 0;	/* interface initialization count */

static GType        register_type( void );
static void         interface_base_init( ofaICollectorInterface *klass );
static void         interface_base_finalize( ofaICollectorInterface *klass );
static sICollector *get_collector_data( ofaICollector *instance );
static sCollection *get_collection( ofaICollector *instance, ofaHub *hub, GType type, sICollector *data );
static sCollection *load_collection( ofaICollector *instance, ofaHub *hub, GType type );
static void         on_instance_finalized( sICollector *data, GObject *finalized_collector );
static void         free_collection( sCollection *collection );
//static GObject *icollector_get_object_by_type( ofaICollector *collector, GType type );

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

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

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
 * @instance: this #ofaICollector instance.
 *
 * Returns: the version number of this interface implemented by the
 * @collector instance.
 *
 * Defaults to 1.
 */
guint
ofa_icollector_get_interface_version( const ofaICollector *instance )
{
	static const gchar *thisfn = "ofa_icollector_get_interface_version";

	g_return_val_if_fail( instance && OFA_IS_ICOLLECTOR( instance ), 1 );

	if( OFA_ICOLLECTOR_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_ICOLLECTOR_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: ofaICollector's %s implementation does not provide 'get_interface_version()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( 1 );
}

/**
 * ofa_icollector_get_collection:
 * @instance: this #ofaICollector instance.
 * @hub: the #ofaHub object.
 * @type: the desired GType type.
 *
 * Returns: a #GList of #ofaICollectionable objects, or %NULL.
 *
 * The returned list is owned by the @instance, and should not be
 * released by the caller.
 */
GList *
ofa_icollector_get_collection( ofaICollector *instance, ofaHub *hub, GType type )
{
	sICollector *data;
	sCollection *collection;

	g_return_val_if_fail( instance && OFA_IS_ICOLLECTOR( instance ), NULL );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	data = get_collector_data( instance );
	collection = get_collection( instance, hub, type, data );

	return( collection ? collection->list : NULL );
}

/*
 * @hub: may be %NULL: do not load the collection if not found
 */
static sCollection *
get_collection( ofaICollector *instance, ofaHub *hub, GType type, sICollector *data )
{
	GList *it;
	sCollection *collection;

	for( it=data->collections ; it ; it=it->next ){
		collection = ( sCollection * ) it->data;
		if( collection->type == type ){
			return( collection );
		}
	}

	collection = NULL;

	if( hub ){
		collection = load_collection( instance, hub, type );
		if( collection ){
			data->collections = g_list_prepend( data->collections, collection );
		}
	}

	return( collection );
}

static sCollection *
load_collection( ofaICollector *instance, ofaHub *hub, GType type )
{
	sCollection *collection;
	GObject *fake;
	GList *dataset;

	collection = NULL;
	fake = g_object_new( type, NULL );

	if( OFA_IS_ICOLLECTIONABLE( fake )){
		dataset = ofa_icollectionable_load_collection( OFA_ICOLLECTIONABLE( fake ), hub );

		if( dataset && g_list_length( dataset )){
			collection = g_new0( sCollection, 1 );
			collection->type = type;
			collection->list = dataset;
		}
	}
	g_object_unref( fake );

	return( collection );
}

/**
 * ofa_icollector_add_object:
 * @instance: this #ofaICollector instance.
 * @hub: the #ofaHub object.
 * @object: the #ofaICollactionable object to be added.
 * @func: [allow-none]: a #GCompareFunc to make sure the object is
 *  added in a sorted list.
 *
 * Adds the @object to the collection of objects of the same type.
 * The collection is maintained sorted with @func function.
 *
 * A new collection is defined if it did not exist yet.
 */
void
ofa_icollector_add_object( ofaICollector *instance, ofaHub *hub, ofaICollectionable *object, GCompareFunc func )
{
	sICollector *data;
	sCollection *collection;

	g_return_if_fail( instance && OFA_IS_ICOLLECTOR( instance ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));
	g_return_if_fail( object && OFA_IS_ICOLLECTIONABLE( object ));

	data = get_collector_data( instance );
	collection = get_collection( instance, hub, G_OBJECT_TYPE( object ), data );
	if( !collection ){
		collection = g_new0( sCollection, 1 );
		collection->type = G_OBJECT_TYPE( object );
		collection->list = NULL;
	}

	if( func ){
		collection->list = g_list_insert_sorted( collection->list, object, func );
	} else {
		collection->list = g_list_prepend( collection->list, object );
	}
}

/**
 * ofa_icollector_remove_object:
 * @instance: this #ofaICollector instance.
 * @object: the #ofaICollactionable object to be removed.
 *
 * Removes the @object from the collection of objects of the same type.
 */
void
ofa_icollector_remove_object( ofaICollector *instance, const ofaICollectionable *object )
{
	sICollector *data;
	sCollection *collection;

	g_return_if_fail( instance && OFA_IS_ICOLLECTOR( instance ));
	g_return_if_fail( object && OFA_IS_ICOLLECTIONABLE( object ));

	data = get_collector_data( instance );
	collection = get_collection( instance, NULL, G_OBJECT_TYPE( object ), data );
	if( collection ){
		collection->list = g_list_remove( collection->list, object );
	}
}

/**
 * ofa_icollector_sort_collection:
 * @instance: this #ofaICollector instance.
 * @type: the GType of the collection to be re-sorted.
 * @func: a #GCompareFunc to sort the list.
 *
 * Re-sort the collection of objects.
 *
 * This is of mainly used after an update of an object of the collection,
 * when the identifier (the sort key) may have been modified.
 */
void
ofa_icollector_sort_collection( ofaICollector *instance, GType type, GCompareFunc func )
{
	sICollector *data;
	sCollection *collection;

	g_return_if_fail( instance && OFA_IS_ICOLLECTOR( instance ));

	data = get_collector_data( instance );
	collection = get_collection( instance, NULL, type, data );
	if( collection ){
		collection->list = g_list_sort( collection->list, func );
	}
}

/**
 * ofa_icollector_free_collection:
 * @instance: this #ofaICollector instance.
 * @type: the GType of the collection to be re-sorted.
 *
 * Free the collection of objects.
 */
void
ofa_icollector_free_collection( ofaICollector *instance, GType type )
{
	sICollector *data;
	sCollection *collection;

	g_return_if_fail( instance && OFA_IS_ICOLLECTOR( instance ));

	data = get_collector_data( instance );
	collection = get_collection( instance, NULL, type, data );
	if( collection ){
		data->collections = g_list_remove( data->collections, collection );
		free_collection( collection );
	}
}

static sICollector *
get_collector_data( ofaICollector *instance )
{
	sICollector *data;

	data = ( sICollector * ) g_object_get_data( G_OBJECT( instance ), ICOLLECTOR_DATA );

	if( !data ){
		data = g_new0( sICollector, 1 );
		g_object_set_data( G_OBJECT( instance ), ICOLLECTOR_DATA, data );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, data );
	}

	return( data );
}

static void
on_instance_finalized( sICollector *data, GObject *finalized_collector )
{
	static const gchar *thisfn = "ofa_icollector_on_instance_finalized";

	g_debug( "%s: data=%p, finalized_collector=%p",
			thisfn, ( void * ) data, ( void * ) finalized_collector );

	g_list_free_full( data->collections, ( GDestroyNotify ) free_collection );
	g_free( data );
}

static void
free_collection( sCollection *collection )
{
	g_list_free_full( collection->list, ( GDestroyNotify ) g_object_unref );
	g_free( collection );
}
