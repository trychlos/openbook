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

#include "my/my-iaction-map.h"
#include "my/my-utils.h"

#define IACTION_MAP_DATA                 "my-iaction-map-data"
#define IACTION_MAP_LAST_VERSION          1

static guint  st_initializations        = 0;	/* interface initialization count */
static GList *st_registered             = NULL;

/* a data structure attached to each instance, and attached to the
 * registered list
 */
typedef struct {
	myIActionMap *map;
	gchar        *scope;
	GMenuModel   *menu_model;
}
	sMap;

static GType register_type( void );
static void  interface_base_init( myIActionMapInterface *klass );
static void  interface_base_finalize( myIActionMapInterface *klass );
static sMap *do_lookup_for_scope( const gchar *scope );
//static sMap *do_lookup_for_map( const myIActionMap *self );
static sMap *get_instance_data( const myIActionMap *self, gboolean create );
static void  on_instance_finalized( sMap *smap, GObject *finalized_instance );

/**
 * my_iaction_map_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
my_iaction_map_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * my_iaction_map_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "my_iaction_map_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( myIActionMapInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "myIActionMap", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_ACTION_MAP );

	return( type );
}

static void
interface_base_init( myIActionMapInterface *klass )
{
	static const gchar *thisfn = "my_iaction_map_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( myIActionMapInterface *klass )
{
	static const gchar *thisfn = "my_iaction_map_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * my_iaction_map_get_interface_last_version:
 * @instance: this #myIActionMap instance.
 *
 * Returns: the last version number of this interface.
 */
guint
my_iaction_map_get_interface_last_version( void )
{
	return( IACTION_MAP_LAST_VERSION );
}

/**
 * my_iaction_map_get_interface_version:
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
my_iaction_map_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, MY_TYPE_IACTION_MAP );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( myIActionMapInterface * ) iface )->get_interface_version ){
		version = (( myIActionMapInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'myIActionMap::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * my_iaction_map_lookup_map:
 * @scope: the searched scope.
 *
 * Returns: the #myIActionMap instance which manages this @scope.
 */
myIActionMap *
my_iaction_map_lookup_map( const gchar *scope )
{
	sMap *smap;

	g_return_val_if_fail( my_strlen( scope ), NULL );

	smap = do_lookup_for_scope( scope );

	return( smap ? smap->map : NULL );
}

/*
 * my_iaction_map_lookup_map:
 * @scope: the searched scope.
 *
 * Returns: the #myIActionMap instance which manages this @scope.
 */
static sMap *
do_lookup_for_scope( const gchar *scope )
{
	GList *it;
	sMap *smap;

	for( it=st_registered ; it ; it=it->next ){
		smap = ( sMap * ) it->data;
		if( !my_collate( smap->scope, scope )){
			return( smap );
		}
	}

	return( NULL );
}

#if 0
/*
 * Search for the given myIActionMap
 */
static sMap *
do_lookup_for_map( const myIActionMap *map )
{
	GList *it;
	sMap *smap;

	for( it=st_registered ; it ; it=it->next ){
		smap = ( sMap * ) it->data;
		if( smap->map == map ){
			return( smap );
		}
	}

	return( NULL );
}
#endif

/**
 * my_iaction_map_register:
 * @map: this #myIActionMap map.
 * @menu_model: the #GMenuModel associated with the @map.
 * @scope: the scope which is managed by this @map.
 *
 * Register the @map for this @scope.
 *
 * The #myIActionMap interfaces takes its own reference on the provided
 * @menu_model, so that the caller may release it.
 */
void
my_iaction_map_register( myIActionMap *map, const gchar *scope, GMenuModel *menu_model )
{
	static const gchar *thisfn = "my_iaction_map_register";
	sMap *sdata;

	g_debug( "%s: map=%p (%s), scope=%s, menu_model=%p",
			thisfn, ( void * ) map, G_OBJECT_TYPE_NAME( map ), scope, ( void * ) menu_model );

	g_return_if_fail( map && MY_IS_IACTION_MAP( map ));
	g_return_if_fail( my_strlen( scope ));
	g_return_if_fail( menu_model && G_IS_MENU_MODEL( menu_model ));

	sdata = get_instance_data( map, FALSE );

	if( sdata ){
		g_warning( "%s: map=%p is already registered", thisfn, ( void * ) map );

	} else {
		sdata = get_instance_data( map, TRUE );
		sdata->map = map;
		sdata->scope = g_strdup( scope );
		sdata->menu_model = g_object_ref( menu_model );
	}
}

/**
 * my_iaction_map_get_menu_model:
 * @map: this #myIActionMap instance.
 *
 * Returns: the #GMenuModel menu of the @map.
 *
 * The returned reference is owned by the @map, and should not be released
 * by the caller.
 */
GMenuModel *
my_iaction_map_get_menu_model( const myIActionMap *map )
{
	sMap *sdata;

	g_return_val_if_fail( map && MY_IS_IACTION_MAP( map ), NULL );

	sdata = get_instance_data( map, FALSE );

	return( sdata ? sdata->menu_model : NULL );
}

static sMap *
get_instance_data( const myIActionMap *self, gboolean create )
{
	sMap *sdata;

	sdata = ( sMap * ) g_object_get_data( G_OBJECT( self ), IACTION_MAP_DATA );

	if( !sdata && create ){
		sdata = g_new0( sMap, 1 );
		g_object_set_data( G_OBJECT( self ), IACTION_MAP_DATA, sdata );
		g_object_weak_ref( G_OBJECT( self ), ( GWeakNotify ) on_instance_finalized, sdata );
	}

	return( sdata );
}

static void
on_instance_finalized( sMap *smap, GObject *finalized_instance )
{
	static const gchar *thisfn = "my_iaction_map_on_instance_finalized";

	g_debug( "%s: smap=%p, finalized_instance=%p",
			thisfn, ( void * ) smap, ( void * ) finalized_instance );

	st_registered = g_list_remove( st_registered, smap );

	g_free( smap->scope );
	g_object_unref( smap->menu_model );
	g_free( smap );
}
