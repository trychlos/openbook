/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#define IACTION_MAP_LAST_VERSION          1

static guint  st_initializations        = 0;	/* interface initialization count */
static GList *st_registered             = NULL;

/* a data structure allocated for each instance, and attached to the
 * registered list
 */
typedef struct {
	myIActionMap *map;
	gchar        *target;
}
	sMap;

static GType   register_type( void );
static void    interface_base_init( myIActionMapInterface *klass );
static void    interface_base_finalize( myIActionMapInterface *klass );
static void    on_instance_finalized( sMap *smap, GObject *finalized_instance );

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
 * @target: the searched target.
 *
 * Returns: the #myIActionMap instance which manages this @target.
 */
myIActionMap *
my_iaction_map_lookup_map( const gchar *target )
{
	GList *it;
	sMap *smap;

	g_return_val_if_fail( my_strlen( target ), NULL );

	for( it=st_registered ; it ; it=it->next ){
		smap = ( sMap * ) it->data;
		if( !my_collate( smap->target, target )){
			return( smap->map );
		}
	}

	return( NULL );
}

/**
 * my_iaction_map_register:
 * @instance: this #myIActionMap instance.
 * @target: the target which is managed by this @instance.
 *
 * Register the @instance for this @target.
 */
void
my_iaction_map_register( myIActionMap *instance, const gchar *target )
{
	static const gchar *thisfn = "my_iaction_map_register";
	sMap *smap;

	g_debug( "%s: instance=%p (%s), target=%s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), target );

	g_return_if_fail( instance && MY_IS_IACTION_MAP( instance ));
	g_return_if_fail( my_strlen( target ));

	smap = g_new0( sMap, 1 );
	smap->map = instance;
	smap->target = g_strdup( target );
	g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, smap );

	st_registered = g_list_prepend( st_registered, smap );
}

/**
 * my_iaction_map_get_menu_model:
 * @instance: this #myIActionMap instance.
 *
 * Returns: the #GMenuModel menu of the @instance.
 */
GMenuModel *
my_iaction_map_get_menu_model( const myIActionMap *instance )
{
	static const gchar *thisfn = "my_iaction_map_get_menu_model";

	g_return_val_if_fail( instance && MY_IS_IACTION_MAP( instance ), NULL );

	if( MY_IACTION_MAP_GET_INTERFACE( instance )->get_menu_model ){
		return( MY_IACTION_MAP_GET_INTERFACE( instance )->get_menu_model( instance ));
	}

	g_info( "%s: myIActionMap's %s implementation does not provide 'get_menu_model()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

static void
on_instance_finalized( sMap *smap, GObject *finalized_instance )
{
	static const gchar *thisfn = "my_iaction_map_on_instance_finalized";

	g_debug( "%s: smap=%p, finalized_instance=%p",
			thisfn, ( void * ) smap, ( void * ) finalized_instance );

	st_registered = g_list_remove( st_registered, smap );

	g_free( smap->target );
	g_free( smap );
}
