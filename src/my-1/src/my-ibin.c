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

#include "my/my-ibin.h"
#include "my/my-utils.h"

#define IBIN_LAST_VERSION           1

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( myIBinInterface *klass );
static void  interface_base_finalize( myIBinInterface *klass );

/**
 * my_ibin_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
my_ibin_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * my_ibin_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "my_ibin_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( myIBinInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "myIBin", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( myIBinInterface *klass )
{
	static const gchar *thisfn = "my_ibin_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( myIBinInterface *klass )
{
	static const gchar *thisfn = "my_ibin_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * my_ibin_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
my_ibin_get_interface_last_version( void )
{
	return( IBIN_LAST_VERSION );
}

/**
 * my_ibin_get_interface_version:
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
my_ibin_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, MY_TYPE_IBIN );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( myIBinInterface * ) iface )->get_interface_version ){
		version = (( myIBinInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'myIBin::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * my_ibin_get_size_group:
 * @instance: this #myIBin instance.
 * @column: the desired column.
 *
 * Returns: the horizontal #GtkSizeGroup for the specified @column.
 *
 * Rationale:
 *   As this is a composite widget, it is probable that we will want
 *   align it with other composites or widgets in a dialog box.
 *   Having a size group prevents us to have to determine the longest
 *   label, which should be computed dynamically as this may depend of
 *   the translation.
 *   Here, the .xml UI definition defines a dedicated GtkSizeGroup that
 *   we have just to return as is.
 */
GtkSizeGroup *
my_ibin_get_size_group( const myIBin *instance, guint column )
{
	static const gchar *thisfn = "my_ibin_get_size_group";

	g_debug( "%s: instance=%p, column=%u", thisfn, ( void * ) instance, column );

	g_return_val_if_fail( instance && MY_IS_IBIN( instance ), NULL );

	if( MY_IBIN_GET_INTERFACE( instance )->get_size_group ){
		return( MY_IBIN_GET_INTERFACE( instance )->get_size_group( instance, column ));
	}

	g_info( "%s: myIBin's %s implementation does not provide 'get_size_group()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * my_ibin_is_valid:
 * @instance: this #myIBin instance.
 * @msgerr: [out][allow-none]: a placeholder for an error message.
 *
 * Returns: %TRUE if the @instance is valid.
 *
 * Defaults to %TRUE.
 */
gboolean
my_ibin_is_valid( const myIBin *instance, gchar **msgerr )
{
	static const gchar *thisfn = "my_ibin_is_valid";

	g_debug( "%s: instance=%p, msgerr=%p", thisfn, ( void * ) instance, ( void * ) msgerr );

	g_return_val_if_fail( instance && MY_IS_IBIN( instance ), FALSE );

	if( MY_IBIN_GET_INTERFACE( instance )->is_valid ){
		return( MY_IBIN_GET_INTERFACE( instance )->is_valid( instance, msgerr ));
	}

	g_info( "%s: myIBin's %s implementation does not provide 'is_valid()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( TRUE );
}

/**
 * my_ibin_apply:
 * @instance: this #myIBin instance.
 *
 * Apply the pending updates.
 */
void
my_ibin_apply( myIBin *instance )
{
	static const gchar *thisfn = "my_ibin_apply";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_if_fail( instance && MY_IS_IBIN( instance ));

	if( MY_IBIN_GET_INTERFACE( instance )->apply ){
		MY_IBIN_GET_INTERFACE( instance )->apply( instance );
		return;
	}

	g_info( "%s: myIBin's %s implementation does not provide 'apply()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}