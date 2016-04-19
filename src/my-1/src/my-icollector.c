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

#include "my/my-icollector.h"

#define ICOLLECTOR_LAST_VERSION           1

#define ICOLLECTOR_DATA                   "my-icollector-data"

/* a data structure attached to the implementation
 * @collections: the collections of #myICollectionable objects,
 *               maintained as a GList of sCollection data structures.
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
static void         interface_base_init( myICollectorInterface *klass );
static void         interface_base_finalize( myICollectorInterface *klass );
static sICollector *get_collector_data( myICollector *instance );
static sCollection *get_collection( myICollector *instance, GType type, sICollector *sdata, void *user_data );
static sCollection *load_collection( myICollector *instance, GType type, void *user_data );
static void         on_instance_finalized( sICollector *data, GObject *finalized_collector );
static void         free_collection( sCollection *collection );
//static GObject *icollector_get_object_by_type( myICollector *collector, GType type );

/**
 * my_icollector_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
my_icollector_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * my_icollector_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "my_icollector_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( myICollectorInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "myICollector", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( myICollectorInterface *klass )
{
	static const gchar *thisfn = "my_icollector_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( myICollectorInterface *klass )
{
	static const gchar *thisfn = "my_icollector_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * my_icollector_get_interface_last_version:
 * @instance: this #myICollector instance.
 *
 * Returns: the last version number of this interface.
 */
guint
my_icollector_get_interface_last_version( void )
{
	return( ICOLLECTOR_LAST_VERSION );
}

/**
 * my_icollector_get_interface_version:
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
my_icollector_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, MY_TYPE_ICOLLECTOR );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( myICollectorInterface * ) iface )->get_interface_version ){
		version = (( myICollectorInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'myICollector::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * my_icollector_get_collection:
 * @instance: this #myICollector instance.
 * @type: the desired GType type.
 * @user_data: user data to be passer to #myICollectionable instance.
 *
 * Returns: a #GList of #myICollectionable objects, or %NULL.
 *
 * The returned list is owned by the @instance, and should not be
 * released by the caller.
 */
GList *
my_icollector_get_collection( myICollector *instance, GType type, void *user_data )
{
	sICollector *sdata;
	sCollection *collection;

	g_return_val_if_fail( instance && MY_IS_ICOLLECTOR( instance ), NULL );

	sdata = get_collector_data( instance );
	collection = get_collection( instance, type, sdata, user_data );

	return( collection ? collection->list : NULL );
}

/*
 * @hub: may be %NULL: do not load the collection if not found
 */
static sCollection *
get_collection( myICollector *instance, GType type, sICollector *sdata, void *user_data )
{
	GList *it;
	sCollection *collection;

	for( it=sdata->collections ; it ; it=it->next ){
		collection = ( sCollection * ) it->data;
		if( collection->type == type ){
			return( collection );
		}
	}

	collection = NULL;

	if( user_data ){
		collection = load_collection( instance, type, user_data );
		if( collection ){
			sdata->collections = g_list_prepend( sdata->collections, collection );
		}
	}

	return( collection );
}

static sCollection *
load_collection( myICollector *instance, GType type, void *user_data )
{
	sCollection *collection;
	GObject *fake;
	GList *dataset;

	collection = NULL;
	fake = g_object_new( type, NULL );

	if( MY_IS_ICOLLECTIONABLE( fake )){
		dataset = my_icollectionable_load_collection( MY_ICOLLECTIONABLE( fake ), user_data );

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
 * my_icollector_add_object:
 * @instance: this #myICollector instance.
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
my_icollector_add_object( myICollector *instance, myICollectionable *object, GCompareFunc func, void *user_data )
{
	sICollector *sdata;
	sCollection *collection;

	g_return_if_fail( instance && MY_IS_ICOLLECTOR( instance ));
	g_return_if_fail( object && MY_IS_ICOLLECTIONABLE( object ));

	sdata = get_collector_data( instance );
	collection = get_collection( instance, G_OBJECT_TYPE( object ), sdata, user_data );
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
 * my_icollector_remove_object:
 * @instance: this #myICollector instance.
 * @object: the #ofaICollactionable object to be removed.
 *
 * Removes the @object from the collection of objects of the same type.
 */
void
my_icollector_remove_object( myICollector *instance, const myICollectionable *object )
{
	sICollector *sdata;
	sCollection *collection;

	g_return_if_fail( instance && MY_IS_ICOLLECTOR( instance ));
	g_return_if_fail( object && MY_IS_ICOLLECTIONABLE( object ));

	sdata = get_collector_data( instance );
	collection = get_collection( instance, G_OBJECT_TYPE( object ), sdata, NULL );
	if( collection ){
		collection->list = g_list_remove( collection->list, object );
	}
}

/**
 * my_icollector_sort_collection:
 * @instance: this #myICollector instance.
 * @type: the GType of the collection to be re-sorted.
 * @func: a #GCompareFunc to sort the list.
 *
 * Re-sort the collection of objects.
 *
 * This is of mainly used after an update of an object of the collection,
 * when the identifier (the sort key) may have been modified.
 */
void
my_icollector_sort_collection( myICollector *instance, GType type, GCompareFunc func )
{
	sICollector *sdata;
	sCollection *collection;

	g_return_if_fail( instance && MY_IS_ICOLLECTOR( instance ));

	sdata = get_collector_data( instance );
	collection = get_collection( instance, type, sdata, NULL );
	if( collection ){
		collection->list = g_list_sort( collection->list, func );
	}
}

/**
 * my_icollector_free_collection:
 * @instance: this #myICollector instance.
 * @type: the GType of the collection to be re-sorted.
 *
 * Free the collection of objects.
 */
void
my_icollector_free_collection( myICollector *instance, GType type )
{
	sICollector *sdata;
	sCollection *collection;

	g_return_if_fail( instance && MY_IS_ICOLLECTOR( instance ));

	sdata = get_collector_data( instance );
	collection = get_collection( instance, type, sdata, NULL );
	if( collection ){
		sdata->collections = g_list_remove( sdata->collections, collection );
		free_collection( collection );
	}
}

/**
 * my_icollector_free_all:
 * @instance: this #myICollector instance.
 *
 * Free all the current collections.
 */
void
my_icollector_free_all( myICollector *instance )
{
	sICollector *sdata;

	g_return_if_fail( instance && MY_IS_ICOLLECTOR( instance ));

	sdata = get_collector_data( instance );
	g_list_free_full( sdata->collections, ( GDestroyNotify ) free_collection );
	sdata->collections = NULL;
}

static sICollector *
get_collector_data( myICollector *instance )
{
	sICollector *sdata;

	sdata = ( sICollector * ) g_object_get_data( G_OBJECT( instance ), ICOLLECTOR_DATA );

	if( !sdata ){
		sdata = g_new0( sICollector, 1 );
		g_object_set_data( G_OBJECT( instance ), ICOLLECTOR_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );
	}

	return( sdata );
}

static void
on_instance_finalized( sICollector *data, GObject *finalized_collector )
{
	static const gchar *thisfn = "my_icollector_on_instance_finalized";

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
