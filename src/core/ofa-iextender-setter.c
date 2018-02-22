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

#include "my/my-utils.h"

#include "api/ofa-iextender-setter.h"
#include "api/ofa-igetter.h"

#define IEXTENDER_SETTER_LAST_VERSION     1

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIExtenderSetterInterface *klass );
static void  interface_base_finalize( ofaIExtenderSetterInterface *klass );

/**
 * ofa_iextender_setter_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iextender_setter_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iextender_setter_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iextender_setter_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIExtenderSetterInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIExtenderSetter", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIExtenderSetterInterface *klass )
{
	static const gchar *thisfn = "ofa_iextender_setter_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIExtenderSetterInterface *klass )
{
	static const gchar *thisfn = "ofa_iextender_setter_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iextender_setter_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iextender_setter_get_interface_last_version( void )
{
	return( IEXTENDER_SETTER_LAST_VERSION );
}

/**
 * ofa_iextender_setter_get_interface_version:
 * @instance: this #ofaIExtenderSetter instance.
 *
 * Returns: the version number of this interface implemented by the
 * @instance.
 *
 * Defaults to 1.
 */
guint
ofa_iextender_setter_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IEXTENDER_SETTER );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIExtenderSetterInterface * ) iface )->get_interface_version ){
		version = (( ofaIExtenderSetterInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIExtenderSetter::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_iextender_setter_get_getter:
 * @instance: this #ofaIExtenderSetter instance.
 *
 * Returns: the previously attached #ofaIGetter.
 */
ofaIGetter *
ofa_iextender_setter_get_getter( ofaIExtenderSetter *instance )
{
	static const gchar *thisfn = "ofa_iextender_setter_get_getter";

	g_return_val_if_fail( instance && OFA_IS_IEXTENDER_SETTER( instance ), NULL );

	if( OFA_IEXTENDER_SETTER_GET_INTERFACE( instance )->get_getter ){
		return( OFA_IEXTENDER_SETTER_GET_INTERFACE( instance )->get_getter( instance ));
	}

	g_info( "%s: ofaIExtenderSetter's %s implementation does not provide 'get_getter()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_iextender_setter_set_getter:
 * @instance: this #ofaIExtenderSetter instance.
 * @getter: a #ofaIGetter of the application.
 *
 * Attach a @getter to the @instance.
 */
void
ofa_iextender_setter_set_getter( ofaIExtenderSetter *instance, ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_iextender_setter_set_getter";
	ofaIGetter *permanent_getter;

	g_return_if_fail( instance && OFA_IS_IEXTENDER_SETTER( instance ));
	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	if( OFA_IEXTENDER_SETTER_GET_INTERFACE( instance )->set_getter ){
		permanent_getter = getter;
		OFA_IEXTENDER_SETTER_GET_INTERFACE( instance )->set_getter( instance, permanent_getter );
		return;
	}

	g_info( "%s: ofaIExtenderSetter's %s implementation does not provide 'set_getter()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}
