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

#include <gtk/gtk.h>

#include "my/my-icollector.h"

#define ICOLLECTOR_LAST_VERSION            1
#define ICOLLECTOR_DATA                   "my-icollector-data"

/* a data structure attached to the implementor instance
 * @typed_list: a #GList of collections of #myICollectionable
 *              (resp. single) objects, as sTyped data structures.
 */
typedef struct {
	GList   *typed_list;
	gboolean finalizing_instance;
}
	sCollector;

/* the data structure which defined the collection
 */
typedef struct {
	GType        type;
	gboolean     is_collection;
	union {
		GList   *list;					/* the collection */
		GObject *object;				/* the single object */
	} t;
}
	sTyped;

static guint st_initializations = 0;	/* interface initialization count */

static GType       register_type( void );
static void        interface_base_init( myICollectorInterface *klass );
static void        interface_base_finalize( myICollectorInterface *klass );
static sTyped     *get_collection( myICollector *instance, GType type, sCollector *sdata, void *user_data );
static sTyped     *load_collection( myICollector *instance, GType type, void *user_data );
static sTyped     *find_typed_by_type( sCollector *sdata, GType type );
static sCollector *get_collector_data( myICollector *instance );
static void        on_instance_finalized( sCollector *data, GObject *finalized_collector );
static void        on_single_object_finalized( sCollector *sdata, GObject *finalized_object );
static void        free_typed( sTyped *typed );

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
 * my_icollector_collection_get:
 * @instance: this #myICollector instance.
 * @type: the desired GType type.
 * @user_data: user data to be passer to #myICollectionable instance.
 *
 * Returns: a #GList of #myICollectionable objects, or %NULL.
 *
 * Loads the #myICollectionable collection if not already done.
 *
 * The returned list is owned by the @instance, and should not be
 * released by the caller.
 */
GList *
my_icollector_collection_get( myICollector *instance, GType type, void *user_data )
{
	sCollector *sdata;
	sTyped *typed;

	g_return_val_if_fail( instance && MY_IS_ICOLLECTOR( instance ), NULL );

	sdata = get_collector_data( instance );
	typed = get_collection( instance, type, sdata, user_data );

	if( typed ){
		g_return_val_if_fail( typed->is_collection, NULL );
	}

	return( typed ? typed->t.list : NULL );
}

/*
 * @user_data: [allow-none]: data passed to #myICollectionable when
 *  loading the collection (if not %NULL)
 */
static sTyped *
get_collection( myICollector *instance, GType type, sCollector *sdata, void *user_data )
{
	sTyped *typed;

	typed = find_typed_by_type( sdata, type );

	if( typed ){
		g_return_val_if_fail( typed->is_collection, NULL );
		return( typed );
	}

	if( user_data ){
		typed = load_collection( instance, type, user_data );
		if( typed ){
			sdata->typed_list = g_list_prepend( sdata->typed_list, typed );
		}
	}

	return( typed );
}

static sTyped *
load_collection( myICollector *instance, GType type, void *user_data )
{
	sTyped *typed;
	GList *dataset;

	dataset = my_icollectionable_load_collection( type, user_data );

	typed = g_new0( sTyped, 1 );
	typed->type = type;
	typed->is_collection = TRUE;
	typed->t.list = dataset;

	return( typed );
}

/**
 * my_icollector_collection_add_object:
 * @instance: this #myICollector instance.
 * @hub: the #ofaHub object.
 * @object: the #myICollactionable object to be added.
 * @func: [allow-none]: a #GCompareFunc to make sure the object is
 *  added in a sorted list.
 *
 * Adds the @object to the collection of objects of the same type.
 * The collection is maintained sorted with @func function.
 *
 * A new collection is defined if it did not exist yet.
 */
void
my_icollector_collection_add_object( myICollector *instance, myICollectionable *object, GCompareFunc func, void *user_data )
{
	sCollector *sdata;
	sTyped *typed;

	g_return_if_fail( instance && MY_IS_ICOLLECTOR( instance ));
	g_return_if_fail( object && MY_IS_ICOLLECTIONABLE( object ));

	sdata = get_collector_data( instance );
	typed = get_collection( instance, G_OBJECT_TYPE( object ), sdata, user_data );

	if( typed ){
		g_return_if_fail( typed->is_collection );

	} else {
		typed = g_new0( sTyped, 1 );
		typed->type = G_OBJECT_TYPE( object );
		typed->is_collection = TRUE;
		typed->t.list = NULL;
	}

	if( func ){
		typed->t.list = g_list_insert_sorted( typed->t.list, object, func );
	} else {
		typed->t.list = g_list_prepend( typed->t.list, object );
	}
}

/**
 * my_icollector_collection_remove_object:
 * @instance: this #myICollector instance.
 * @object: the #myICollactionable object to be removed.
 *
 * Removes the @object from the collection of objects of the same type.
 */
void
my_icollector_collection_remove_object( myICollector *instance, const myICollectionable *object )
{
	sCollector *sdata;
	sTyped *typed;

	g_return_if_fail( instance && MY_IS_ICOLLECTOR( instance ));
	g_return_if_fail( object && MY_IS_ICOLLECTIONABLE( object ));

	sdata = get_collector_data( instance );
	typed = get_collection( instance, G_OBJECT_TYPE( object ), sdata, NULL );

	if( typed ){
		g_return_if_fail( typed->is_collection );
		typed->t.list = g_list_remove( typed->t.list, object );
	}
}

/**
 * my_icollector_collection_sort:
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
my_icollector_collection_sort( myICollector *instance, GType type, GCompareFunc func )
{
	sCollector *sdata;
	sTyped *typed;

	g_return_if_fail( instance && MY_IS_ICOLLECTOR( instance ));

	sdata = get_collector_data( instance );
	typed = get_collection( instance, type, sdata, NULL );

	if( typed ){
		g_return_if_fail( typed->is_collection );
		typed->t.list = g_list_sort( typed->t.list, func );
	}
}

/**
 * my_icollector_collection_free:
 * @instance: this #myICollector instance.
 * @type: the GType of the collection to be re-sorted.
 *
 * Free the collection of objects.
 */
void
my_icollector_collection_free( myICollector *instance, GType type )
{
	sCollector *sdata;
	sTyped *typed;

	g_return_if_fail( instance && MY_IS_ICOLLECTOR( instance ));

	sdata = get_collector_data( instance );
	typed = get_collection( instance, type, sdata, NULL );

	if( typed ){
		g_return_if_fail( typed->is_collection );
		sdata->typed_list = g_list_remove( sdata->typed_list, typed );
		free_typed( typed );
	}
}

/**
 * my_icollector_collection_get_list:
 * @instance: this #myICollector instance.
 *
 * Returns: a list of currently maintained collections.
 *
 * Items of the returned list may be requested with the
 * #my_icollector_item_get_xxx() methods.
 *
 * The returned list should be #g_list_free() by the caller.
 */
GList *
my_icollector_collection_get_list( myICollector *instance )
{
	GList *list, *it;
	sCollector *sdata;
	sTyped *typed;

	g_return_val_if_fail( instance && MY_IS_ICOLLECTOR( instance ), NULL );

	list = NULL;
	sdata = get_collector_data( instance );

	for( it=sdata->typed_list ; it ; it=it->next ){
		typed = ( sTyped * ) it->data;
		if( typed->is_collection ){
			list = g_list_prepend( list, typed );
		}
	}

	return( list );
}

/**
 * my_icollector_single_get_object:
 * @instance: this #myICollector instance.
 * @type: the desired GType type.
 *
 * Returns: a #GObject of @type, or %NULL.
 *
 * The returned object is owned by the @instance, and should not be
 * released by the caller.
 */
GObject *
my_icollector_single_get_object( myICollector *instance, GType type )
{
	sCollector *sdata;
	sTyped *typed;
	GObject *found;

	g_return_val_if_fail( instance && MY_IS_ICOLLECTOR( instance ), NULL );

	sdata = get_collector_data( instance );
	typed = find_typed_by_type( sdata, type );

	if( typed ){
		g_return_val_if_fail( !typed->is_collection, NULL );
	}

	found = typed ? typed->t.object : NULL;

	return( found );
}

/**
 * my_icollector_single_set_object:
 * @instance: this #myICollector instance.
 * @object: [allow-none]: the object to be added.
 *
 * Let the @instance keep the @object.
 */
void
my_icollector_single_set_object( myICollector *instance, void *object )
{
	sCollector *sdata;
	sTyped *typed;

	g_return_if_fail( instance && MY_IS_ICOLLECTOR( instance ));
	g_return_if_fail( !object || G_IS_OBJECT( object ));

	sdata = get_collector_data( instance );
	typed = find_typed_by_type( sdata, G_OBJECT_TYPE( object ));

	if( typed ){
		g_return_if_fail( !typed->is_collection );
		g_clear_object( &typed->t.object );
		typed->t.object = object;

	} else {
		typed = g_new0( sTyped, 1 );
		typed->type = G_OBJECT_TYPE( object );
		typed->is_collection = FALSE;
		typed->t.object = object;
		sdata->typed_list = g_list_prepend( sdata->typed_list, typed );
		g_object_weak_ref( G_OBJECT( object ), ( GWeakNotify ) on_single_object_finalized, sdata );
	}
}

/**
 * my_icollector_single_get_list:
 * @instance: this #myICollector instance.
 *
 * Returns: a list of currently maintained single objects.
 *
 * Items of the returned list may be requested with the
 * #my_icollector_item_get_xxx() methods.
 *
 * The returned list should be #g_list_free() by the caller.
 */
GList *
my_icollector_single_get_list( myICollector *instance )
{
	GList *list, *it;
	sCollector *sdata;
	sTyped *typed;

	g_return_val_if_fail( instance && MY_IS_ICOLLECTOR( instance ), NULL );

	list = NULL;
	sdata = get_collector_data( instance );

	for( it=sdata->typed_list ; it ; it=it->next ){
		typed = ( sTyped * ) it->data;
		if( !typed->is_collection ){
			list = g_list_prepend( list, typed );
		}
	}

	return( list );
}

/**
 * my_icollector_item_get_name:
 * @instance: this #myICollector instance.
 * @item: an element of the list returned by
 *  #my_icollector_collection_get_list() or
 *  #my_icollector_single_get_list() methods.
 *
 * Returns: the name of the item class, as a newly allocated string
 * which should be #g_free() by the caller.
 */
gchar *
my_icollector_item_get_name( myICollector *instance, void *item )
{
	gchar *name;
	sTyped *typed;

	g_return_val_if_fail( instance && MY_IS_ICOLLECTOR( instance ), NULL );
	g_return_val_if_fail( item, NULL );

	typed = ( sTyped * ) item;
	name = g_strdup( g_type_name( typed->type ));

	return( name );
}

/**
 * my_icollector_item_get_count:
 * @instance: this #myICollector instance.
 * @item: an element of the list returned by
 *  #my_icollector_collection_get_list() or
 *  #my_icollector_single_get_list() methods.
 *
 * Returns: the count of the items for this collection, or 1 if this is
 * a single object.
 */
guint
my_icollector_item_get_count( myICollector *instance, void *item )
{
	guint count;
	sTyped *typed;

	g_return_val_if_fail( instance && MY_IS_ICOLLECTOR( instance ), 0 );
	g_return_val_if_fail( item, 0 );

	typed = ( sTyped * ) item;
	count = typed->is_collection ? g_list_length( typed->t.list ) : 1;

	return( count );
}

/**
 * my_icollector_free_all:
 * @instance: this #myICollector instance.
 *
 * Free all the current collections+single objects.
 */
void
my_icollector_free_all( myICollector *instance )
{
	static const gchar *thisfn = "my_icollector_free_all";
	sCollector *sdata;
	sTyped *typed;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_if_fail( instance && MY_IS_ICOLLECTOR( instance ));

	sdata = get_collector_data( instance );

	while( sdata->typed_list ){
		typed = ( sTyped * ) sdata->typed_list->data;
		sdata->typed_list = g_list_remove( sdata->typed_list, typed );
		free_typed( typed );
	}

	sdata->typed_list = NULL;
}

static sTyped *
find_typed_by_type( sCollector *sdata, GType type )
{
	GList *it;
	sTyped *typed;

	for( it=sdata->typed_list ; it ; it=it->next ){
		typed = ( sTyped * ) it->data;
		if( typed && typed->type == type ){
			return( typed );
		}
	}

	return( NULL );
}

static sCollector *
get_collector_data( myICollector *instance )
{
	sCollector *sdata;

	sdata = ( sCollector * ) g_object_get_data( G_OBJECT( instance ), ICOLLECTOR_DATA );

	if( !sdata ){
		sdata = g_new0( sCollector, 1 );
		sdata->finalizing_instance = FALSE;
		g_object_set_data( G_OBJECT( instance ), ICOLLECTOR_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );
	}

	return( sdata );
}

static void
on_instance_finalized( sCollector *sdata, GObject *finalized_collector )
{
	static const gchar *thisfn = "my_icollector_on_instance_finalized";

	g_debug( "%s: sdata=%p, finalized_collector=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_collector );

	sdata->finalizing_instance = TRUE;
	g_list_free_full( sdata->typed_list, ( GDestroyNotify ) free_typed );
	g_free( sdata );
}

static void
on_single_object_finalized( sCollector *sdata, GObject *finalized_object )
{
	static const gchar *thisfn = "my_icollector_on_object_finalized";
	sTyped *typed;

	g_debug( "%s: sdata=%p, finalized_object=%p (%s)",
			thisfn, ( void * ) sdata, ( void * ) finalized_object, G_OBJECT_TYPE_NAME( finalized_object ));

	if( !sdata->finalizing_instance ){
		typed = find_typed_by_type( sdata, G_OBJECT_TYPE( finalized_object ));
		if( typed && !typed->is_collection ){
			sdata->typed_list = g_list_remove( sdata->typed_list, typed );
			g_free( typed );
		}
	}
}

static void
free_typed( sTyped *typed )
{
	static const gchar *thisfn = "my_icollector_free_typed";

	if( typed->is_collection ){
		g_debug( "%s: about to unref %s collection (count=%d)",
				thisfn, g_type_name( typed->type ), g_list_length( typed->t.list ));

		g_list_free_full( typed->t.list, ( GDestroyNotify ) g_object_unref );

	} else {
		g_debug( "%s: about to unref single %s at %p",
				thisfn, g_type_name( typed->type ), ( void * ) typed->t.object );

		/* rather destroy a widget than just unreffing it */
		if( GTK_IS_WIDGET( typed->t.object )){
			gtk_widget_destroy( GTK_WIDGET( typed->t.object ));

		} else {
			g_object_unref( typed->t.object );
		}
	}

	g_free( typed );
}
