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

#include "my/my-isizegroup.h"
#include "my/my-utils.h"

#define ISIZEGROUP_LAST_VERSION           1

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( myISizegroupInterface *klass );
static void  interface_base_finalize( myISizegroupInterface *klass );

/**
 * my_isizegroup_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
my_isizegroup_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * my_isizegroup_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "my_isizegroup_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( myISizegroupInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "myISizegroup", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( myISizegroupInterface *klass )
{
	static const gchar *thisfn = "my_isizegroup_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( myISizegroupInterface *klass )
{
	static const gchar *thisfn = "my_isizegroup_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * my_isizegroup_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
my_isizegroup_get_interface_last_version( void )
{
	return( ISIZEGROUP_LAST_VERSION );
}

/**
 * my_isizegroup_get_interface_version:
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
my_isizegroup_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, MY_TYPE_ISIZEGROUP );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( myISizegroupInterface * ) iface )->get_interface_version ){
		version = (( myISizegroupInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'myISizegroup::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * my_isizegroup_get_size_group:
 * @instance: this #myISizegroup instance.
 * @column: the desired column.
 *
 * Returns: the #GtkSizeGroup of the specified @column.
 */
GtkSizeGroup *
my_isizegroup_get_size_group( const myISizegroup *instance, guint column )
{
	static const gchar *thisfn = "my_isizegroup_get_size_group";

	g_debug( "%s: instance=%p, column=%u", thisfn, ( void * ) instance, column );

	g_return_val_if_fail( instance && MY_IS_ISIZEGROUP( instance ), NULL );

	if( MY_ISIZEGROUP_GET_INTERFACE( instance )->get_size_group ){
		return( MY_ISIZEGROUP_GET_INTERFACE( instance )->get_size_group( instance, column ));
	}

	g_info( "%s: myISizegroup's %s implementation does not provide 'get_size_group()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}
