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

#include <glib.h>

#include "my/my-iextender-id.h"

#define IEXTENDER_ID_LAST_VERSION         1

static guint    st_initializations      = 0;		/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( myIExtenderIdInterface *klass );
static void  interface_base_finalize( myIExtenderIdInterface *klass );

/**
 * my_iextender_id_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
my_iextender_id_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * my_iextender_id_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "my_iextender_id_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( myIExtenderIdInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "myIExtenderId", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( myIExtenderIdInterface *klass )
{
	static const gchar *thisfn = "my_iextender_id_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( myIExtenderIdInterface *klass )
{
	static const gchar *thisfn = "my_iextender_id_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * my_iextender_id_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
my_iextender_id_get_interface_last_version( void )
{
	return( IEXTENDER_ID_LAST_VERSION );
}

/**
 * my_iextender_id_get_interface_version:
 * @instance: this #myIExtenderId instance.
 *
 * Returns: the version number implemented of this interface implemented
 * by the loadable module.
 *
 * Defaults to 1.
 */
guint
my_iextender_id_get_interface_version( const myIExtenderId *instance )
{
	static const gchar *thisfn = "my_iextender_id_get_interface_version";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && MY_IS_IEXTENDER_ID( instance ), 0 );

	if( MY_IEXTENDER_ID_GET_INTERFACE( instance )->get_interface_version ){
		return( MY_IEXTENDER_ID_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: myIExtenderId instance %p does not provide 'get_interface_version()' method",
			thisfn, ( void * ) instance );
	return( 1 );
}

/**
 * my_iextender_get_name:
 * @instance: this #myIExtenderId instance.
 *
 * Returns: the canonical name of the loadable module, as a newly
 * allocated string which should be g_free() by the caller.
 *
 * It is expected that the canonical name be stable among executions
 * and among versions. It should be usable as an identifier for the
 * module.
 */
gchar *
my_iextender_id_get_name( const myIExtenderId *instance )
{
	static const gchar *thisfn = "my_iextender_id_get_name";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && MY_IS_IEXTENDER_ID( instance ), NULL );

	if( MY_IEXTENDER_ID_GET_INTERFACE( instance )->get_name ){
		return( MY_IEXTENDER_ID_GET_INTERFACE( instance )->get_name( instance ));
	}

	g_info( "%s: myIExtenderId instance %p does not provide 'get_name()' method",
			thisfn, ( void * ) instance );
	return( NULL );
}

/**
 * my_iextender_get_full_name:
 * @instance: this #myIExtenderId instance.
 *
 * Returns: the full name of the loadable module, as a newly
 * allocated string which should be g_free() by the caller.
 *
 * The full name of the module is expected to be used for display
 * to the user.
 */
gchar *
my_iextender_id_get_full_name( const myIExtenderId *instance )
{
	static const gchar *thisfn = "my_iextender_id_get_full_name";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && MY_IS_IEXTENDER_ID( instance ), NULL );

	if( MY_IEXTENDER_ID_GET_INTERFACE( instance )->get_full_name ){
		return( MY_IEXTENDER_ID_GET_INTERFACE( instance )->get_full_name( instance ));
	}

	g_info( "%s: myIExtenderId instance %p does not provide 'get_full_name()' method",
			thisfn, ( void * ) instance );
	return( NULL );
}

/**
 * my_iextender_get_version:
 * @instance: this #myIExtenderId instance.
 *
 * Returns: the internal version of the loadable module, as a newly
 * allocated string which should be g_free() by the caller.
 *
 * This version number of the module is expected to be used for display
 * to the user.
 */
gchar *
my_iextender_id_get_version( const myIExtenderId *instance )
{
	static const gchar *thisfn = "my_iextender_id_get_version";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && MY_IS_IEXTENDER_ID( instance ), NULL );

	if( MY_IEXTENDER_ID_GET_INTERFACE( instance )->get_version ){
		return( MY_IEXTENDER_ID_GET_INTERFACE( instance )->get_version( instance ));
	}

	g_info( "%s: myIExtenderId instance %p does not provide 'get_version()' method",
			thisfn, ( void * ) instance );
	return( NULL );
}
