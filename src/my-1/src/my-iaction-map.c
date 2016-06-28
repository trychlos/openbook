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

static GList *st_registered             = NULL;

static guint  st_initializations        = 0;	/* interface initialization count */

static GType         register_type( void );
static void          interface_base_init( myIActionMapInterface *klass );
static void          interface_base_finalize( myIActionMapInterface *klass );
static myIActionMap *find_by_target( const gchar *target );
static const gchar  *iaction_map_get_action_target( const myIActionMap *instance );
static void          on_instance_finalized( GList **list, GObject *finalized_instance );

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
my_iaction_map_get_interface_last_version( const myIActionMap *instance )
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
 * my_iaction_map_register:
 * @instance: this #myIActionMap instance.
 *
 * Let the instance be registered as a #myIActionMap implementation.
 * This is needed in order to be able to request all #myIActionMap when
 * searching for an action handler.
 */
void
my_iaction_map_register( const myIActionMap *instance )
{
	static const gchar *thisfn = "my_iaction_map_register";

	g_debug( "%s: instance=%p (%s)", thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && MY_IS_IACTION_MAP( instance ));

	st_registered = g_list_prepend( st_registered, ( gpointer ) instance );

	g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, &st_registered);
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

	if( MY_IACTION_MAP_GET_INTERFACE( instance )->get_menu_model ){
		return( MY_IACTION_MAP_GET_INTERFACE( instance )->get_menu_model( instance ));
	}

	g_info( "%s: myIActionMap's %s implementation does not provide 'get_menu_model()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * my_iaction_map_lookup_action:
 * @instance: this #myIActionMap instance.
 * @detailed_name: the detailed name of the action.
 *
 * Returns: the corresponding #GAction, or %NULL.
 *
 * The provided @instance is those which has registered the accelerator, but
 * maybe not those which handles it.
 */
GAction *
my_iaction_map_lookup_action( myIActionMap *instance, const gchar *detailed_name )
{
	static const gchar *thisfn = "my_iaction_map_lookup_action";
	GAction *action;
	gchar *act_target, *act_name;
	gchar **tokens, **it;
	myIActionMap *map;

	g_return_val_if_fail( instance && MY_IS_IACTION_MAP( instance ), NULL );
	g_return_val_if_fail( my_strlen( detailed_name ), NULL );

	action = NULL;

	/* split target and action */
	act_target = NULL;
	act_name = NULL;
	tokens = g_strsplit( detailed_name, ".", -1 );
	it = tokens;
	if( *it ){
		act_target = g_strdup( *it );
		it++;
		if( *it ){
			act_name = g_strdup( *it );
		}
	}
	g_strfreev( tokens );

	map = NULL;
	/* get the action map for this target */
	if( my_strlen( act_target ) && my_strlen( act_name )){
		map = find_by_target( act_target );
		g_debug( "%s: act_target=%s, map=%p (%s)", thisfn, act_target, ( void * ) map, G_OBJECT_TYPE_NAME( map ));
	}

	/* and get the action */
	if( map ){
		action = g_action_map_lookup_action( G_ACTION_MAP( map ), act_name );
		g_debug( "%s: act_name=%s, action=%p", thisfn, act_name, ( void * ) action );
	}

	g_free( act_name );
	g_free( act_target );

	return( action );
}

static myIActionMap *
find_by_target( const gchar *target )
{
	GList *it;
	myIActionMap *map;
	const gchar *map_target;

	for( it=st_registered ; it ; it=it->next ){
		map = MY_IACTION_MAP( it->data );
		map_target = iaction_map_get_action_target( map );
		if( !my_collate( map_target, target )){
			return( map );
		}
	}

	return( NULL );
}

/*
 * my_iaction_map_get_action_target:
 * @instance: this #myIActionMap instance.
 *
 * Returns: the action target of the @instance.
 */
static const gchar *
iaction_map_get_action_target( const myIActionMap *instance )
{
	static const gchar *thisfn = "my_iaction_map_get_action_target";

	if( MY_IACTION_MAP_GET_INTERFACE( instance )->get_action_target ){
		return( MY_IACTION_MAP_GET_INTERFACE( instance )->get_action_target( instance ));
	}

	g_info( "%s: myIActionMap's %s implementation does not provide 'get_action_target()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

static void
on_instance_finalized( GList **list, GObject *finalized_instance )
{
	static const gchar *thisfn = "my_iaction_map_on_instance_finalized";

	g_debug( "%s: list=%p, finalized_instance=%p (%s)",
			thisfn, ( void * ) list, ( void * ) finalized_instance, G_OBJECT_TYPE_NAME( finalized_instance ));

	*list = g_list_remove( *list, finalized_instance );
}
