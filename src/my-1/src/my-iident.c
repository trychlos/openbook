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

#include <glib.h>

#include "my/my-iident.h"

#define IIDENT_LAST_VERSION               1

static guint st_initializations         = 0;		/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( myIIdentInterface *klass );
static void  interface_base_finalize( myIIdentInterface *klass );

/**
 * my_iident_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
my_iident_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * my_iident_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "my_iident_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( myIIdentInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "myIIdent", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( myIIdentInterface *klass )
{
	static const gchar *thisfn = "my_iident_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( myIIdentInterface *klass )
{
	static const gchar *thisfn = "my_iident_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * my_iident_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
my_iident_get_interface_last_version( void )
{
	return( IIDENT_LAST_VERSION );
}

/**
 * my_iident_get_interface_version:
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
my_iident_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, MY_TYPE_IIDENT );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( myIIdentInterface * ) iface )->get_interface_version ){
		version = (( myIIdentInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'myIIdent::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * my_iextender_get_canon_name:
 * @instance: this #myIIdent instance.
 * @user_data: some data to be passed to the final handler.
 *
 * Returns: the canonical name of the loadable module, as a newly
 * allocated string which should be g_free() by the caller.
 *
 * It is expected that the canonical name be stable among executions
 * and among versions. It should be usable as an identifier for the
 * object instance.
 */
gchar *
my_iident_get_canon_name( const myIIdent *instance, void *user_data )
{
	static const gchar *thisfn = "my_iident_get_canon_name";

	g_debug( "%s: instance=%p, user_data=%p", thisfn, ( void * ) instance, user_data );

	g_return_val_if_fail( instance && MY_IS_IIDENT( instance ), NULL );

	if( MY_IIDENT_GET_INTERFACE( instance )->get_canon_name ){
		return( MY_IIDENT_GET_INTERFACE( instance )->get_canon_name( instance, user_data ));
	}

	g_info( "%s: myIIdent's %s implementation does not provide 'get_canon_name()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * my_iextender_get_display_name:
 * @instance: this #myIIdent instance.
 * @user_data: some data to be passed to the final handler.
 *
 * Returns: the displayable name of the loadable module, as a newly
 * allocated string which should be g_free() by the caller.
 *
 * The displayable name of the module is expected to be used for display
 * to the user. It defaults to the canonical name.
 */
gchar *
my_iident_get_display_name( const myIIdent *instance, void *user_data )
{
	static const gchar *thisfn = "my_iident_get_display_name";

	g_debug( "%s: instance=%p, user_data=%p", thisfn, ( void * ) instance, user_data );

	g_return_val_if_fail( instance && MY_IS_IIDENT( instance ), NULL );

	if( MY_IIDENT_GET_INTERFACE( instance )->get_display_name ){
		return( MY_IIDENT_GET_INTERFACE( instance )->get_display_name( instance, user_data ));
	}

	g_info( "%s: myIIdent's %s implementation does not provide 'get_display_name()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));

	if( MY_IIDENT_GET_INTERFACE( instance )->get_canon_name ){
		return( my_iident_get_canon_name( instance, user_data ));
	}

	return( NULL );
}

/**
 * my_iextender_get_version:
 * @instance: this #myIIdent instance.
 * @user_data: some data to be passed to the final handler.
 *
 * Returns: the internal version of the loadable module, as a newly
 * allocated string which should be g_free() by the caller.
 *
 * This version number of the module is expected to be used for display
 * to the user.
 */
gchar *
my_iident_get_version( const myIIdent *instance, void *user_data )
{
	static const gchar *thisfn = "my_iident_get_version";

	g_debug( "%s: instance=%p, user_data=%p", thisfn, ( void * ) instance, user_data );

	g_return_val_if_fail( instance && MY_IS_IIDENT( instance ), NULL );

	if( MY_IIDENT_GET_INTERFACE( instance )->get_version ){
		return( MY_IIDENT_GET_INTERFACE( instance )->get_version( instance, user_data ));
	}

	g_info( "%s: myIIdent's %s implementation does not provide 'get_version()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}
