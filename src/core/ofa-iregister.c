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

#include "api/ofa-extender-collection.h"
#include "api/ofa-hub.h"
#include "api/ofa-iregister.h"
#include "api/ofa-igetter.h"

#define IREGISTER_LAST_VERSION            1

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIRegisterInterface *klass );
static void  interface_base_finalize( ofaIRegisterInterface *klass );

/**
 * ofa_iregister_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iregister_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iregister_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iregister_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIRegisterInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIRegister", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIRegisterInterface *klass )
{
	static const gchar *thisfn = "ofa_iregister_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIRegisterInterface *klass )
{
	static const gchar *thisfn = "ofa_iregister_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iregister_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iregister_get_interface_last_version( void )
{
	return( IREGISTER_LAST_VERSION );
}

/**
 * ofa_iregister_get_interface_version:
 * @instance: this #ofaIRegister instance.
 *
 * Returns: the version number of this interface implemented by the
 * application.
 */
guint
ofa_iregister_get_interface_version( const ofaIRegister *instance )
{
	static const gchar *thisfn = "ofa_iregister_get_interface_version";

	g_return_val_if_fail( instance && OFA_IS_IREGISTER( instance ), 0 );

	if( OFA_IREGISTER_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_IREGISTER_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: ofaIRegister's %s implementation does not provide 'get_interface_version()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( 1 );
}

/**
 * ofa_iregister_get_for_type:
 * @instance: this #ofaIRegister instance.
 * @type: a GType.
 *
 * Returns: a list of new references to objects which implement this
 * @type.
 *
 * This method is directly meants for the plugins, so that they are able
 * to advertize the properties of their objects.
 *
 * The interface will take ownership of the returned list, and will
 * #g_list_free_full( list, ( GDestroyNotify ) g_object_unref ) after
 * use.
 */
GList *
ofa_iregister_get_for_type( ofaIRegister *instance, GType type )
{
	static const gchar *thisfn = "ofa_iregister_get_for_type";

	g_return_val_if_fail( instance && OFA_IS_IREGISTER( instance ), NULL );

	if( OFA_IREGISTER_GET_INTERFACE( instance )->get_for_type ){
		return( OFA_IREGISTER_GET_INTERFACE( instance )->get_for_type( instance, type ));
	}

	g_info( "%s: ofaIRegister's %s implementation does not provide 'get_for_type()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_iregister_get_all_for_type:
 * @hub: the #ofaHub object of the application.
 * @type: a GType.
 *
 * Returns: a list of new references to objects which implement the
 * @type, concatenating both those from the core library, and those
 * advertized by the plugins.
 *
 * It is expected that the caller takes ownership of the returned list,
 * and #g_list_free_full( list, ( GDestroyNotify ) g_object_unref )
 * after use.
 */
GList *
ofa_iregister_get_all_for_type( ofaHub *hub, GType type )
{
	GList *objects, *registers;
	GList *it, *it_objects;
	ofaExtenderCollection *extenders;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	/* requests the objects first from core library
	 * from ofaHub as ofaIRegister */
	objects = ofa_iregister_get_for_type( OFA_IREGISTER( hub ), type );

	/* requests the ofaIExportables from ofaIRegister modules */
	extenders = ofa_hub_get_extender_collection( hub );
	registers = ofa_extender_collection_get_for_type( extenders, OFA_TYPE_IREGISTER );

	for( it=registers ; it ; it=it->next ){
		it_objects = ofa_iregister_get_for_type( OFA_IREGISTER( it->data ), type );
		objects = g_list_concat( objects, it_objects );
	}

	ofa_extender_collection_free_types( registers );

	return( objects );
}
