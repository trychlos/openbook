/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#include "my/my-iscope-map.h"
#include "my/my-utils.h"

#define ISCOPE_MAP_LAST_VERSION          1

static guint  st_initializations        = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( myIScopeMapInterface *klass );
static void  interface_base_finalize( myIScopeMapInterface *klass );

/**
 * my_iscope_map_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
my_iscope_map_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * my_iscope_map_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "my_iscope_map_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( myIScopeMapInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "myIScopeMap", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( myIScopeMapInterface *klass )
{
	static const gchar *thisfn = "my_iscope_map_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( myIScopeMapInterface *klass )
{
	static const gchar *thisfn = "my_iscope_map_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * my_iscope_map_get_interface_last_version:
 * @instance: this #myIScopeMap instance.
 *
 * Returns: the last version number of this interface.
 */
guint
my_iscope_map_get_interface_last_version( void )
{
	return( ISCOPE_MAP_LAST_VERSION );
}

/**
 * my_iscope_map_get_interface_version:
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
my_iscope_map_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, MY_TYPE_ISCOPE_MAP );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( myIScopeMapInterface * ) iface )->get_interface_version ){
		version = (( myIScopeMapInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'myIScopeMap::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * my_iscope_map_get_menu_model:
 * @mapper: this #myIScopeMap instance.
 * @action_map: a registered #GActionMap.
 *
 * Returns: the #GMenuModel associated to the @action_map.
 */
GMenuModel *
my_iscope_map_get_menu_model( const myIScopeMap *mapper, GActionMap *action_map )
{
	static const gchar *thisfn = "my_iscope_map_get_menu_model";

	g_debug( "%s: mapper=%p (%s), action_map=%p",
			thisfn, ( void * ) mapper, G_OBJECT_TYPE_NAME( mapper ), (  void * ) action_map );

	g_return_val_if_fail( mapper && MY_IS_ISCOPE_MAP( mapper ), NULL );
	g_return_val_if_fail( action_map && G_IS_ACTION_MAP( action_map ), NULL );

	if( MY_ISCOPE_MAP_GET_INTERFACE( mapper )->get_menu_model ){
		return( MY_ISCOPE_MAP_GET_INTERFACE( mapper )->get_menu_model( mapper, action_map ));
	}

	g_info( "%s: myIScopeMap's %s implementation does not provide 'get_menu_model()' method",
			thisfn, G_OBJECT_TYPE_NAME( mapper ));
	return( NULL );
}

/**
 * my_iscope_map_lookup_by_scope:
 * @mapper: this #myIScopeMap instance.
 * @scope: the requested scope.
 *
 * Returns: the #GActionMap associated to this @scope.
 */
GActionMap *
my_iscope_map_lookup_by_scope( const myIScopeMap *mapper, const gchar *scope )
{
	static const gchar *thisfn = "my_iscope_map_lookup_by_scope";

	g_debug( "%s: mapper=%p (%s), scope=%s",
			thisfn, ( void * ) mapper, G_OBJECT_TYPE_NAME( mapper ), scope );

	g_return_val_if_fail( mapper && MY_IS_ISCOPE_MAP( mapper ), NULL );
	g_return_val_if_fail( my_strlen( scope ), NULL );

	if( MY_ISCOPE_MAP_GET_INTERFACE( mapper )->lookup_by_scope ){
		return( MY_ISCOPE_MAP_GET_INTERFACE( mapper )->lookup_by_scope( mapper, scope ));
	}

	g_info( "%s: myIScopeMap's %s implementation does not provide 'lookup_by_scope()' method",
			thisfn, G_OBJECT_TYPE_NAME( mapper ));
	return( NULL );
}
