/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#include "my/my-icollectionable.h"

#define ICOLLECTIONABLE_LAST_VERSION    1

#define ICOLLECTIONABLE_DATA            "my-icollectionable-data"

/* a data structure attached to the GObject implementation
 */
typedef struct {
	GList *objects;
}
	sICollectionable;

static guint st_initializations = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( myICollectionableInterface *klass );
static void  interface_base_finalize( myICollectionableInterface *klass );

/**
 * my_icollectionable_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
my_icollectionable_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * my_icollectionable_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "my_icollectionable_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( myICollectionableInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "myICollectionable", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( myICollectionableInterface *klass )
{
	static const gchar *thisfn = "my_icollectionable_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( myICollectionableInterface *klass )
{
	static const gchar *thisfn = "my_icollectionable_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * my_icollectionable_get_interface_last_version:
 * @instance: this #myICollectionable instance.
 *
 * Returns: the last version number of this interface.
 */
guint
my_icollectionable_get_interface_last_version( void )
{
	return( ICOLLECTIONABLE_LAST_VERSION );
}

/**
 * my_icollectionable_get_interface_version:
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
my_icollectionable_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, MY_TYPE_ICOLLECTIONABLE );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( myICollectionableInterface * ) iface )->get_interface_version ){
		version = (( myICollectionableInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'myICollectionable::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * my_icollectionable_load_collection:
 * @type: the implementation's GType.
 * @user_data: user data to be passed to the @instance.
 *
 * Returns: the collection of desired objects, or %NULL.
 */
GList *
my_icollectionable_load_collection( GType type, void *user_data )
{
	gpointer klass, iface;
	GList *list;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, NULL );

	iface = g_type_interface_peek( klass, MY_TYPE_ICOLLECTIONABLE );

	list = NULL;

	if( iface && (( myICollectionableInterface * ) iface )->load_collection ){
		list = (( myICollectionableInterface * ) iface )->load_collection( user_data );

	} else {
		g_info( "%s implementation does not provide 'myICollectionable::load_collection()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( list );
}
