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

#include "my/my-utils.h"

#include "api/ofa-igetter.h"
#include "api/ofa-isetter.h"

#define ISETTER_LAST_VERSION              1

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaISetterInterface *klass );
static void  interface_base_finalize( ofaISetterInterface *klass );

/**
 * ofa_isetter_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_isetter_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_isetter_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_isetter_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaISetterInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaISetter", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaISetterInterface *klass )
{
	static const gchar *thisfn = "ofa_isetter_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaISetterInterface *klass )
{
	static const gchar *thisfn = "ofa_isetter_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_isetter_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_isetter_get_interface_last_version( void )
{
	return( ISETTER_LAST_VERSION );
}

/**
 * ofa_isetter_get_interface_version:
 * @instance: this #ofaISetter instance.
 *
 * Returns: the version number of this interface implemented by the
 * @instance.
 *
 * Defaults to 1.
 */
guint
ofa_isetter_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_ISETTER );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaISetterInterface * ) iface )->get_interface_version ){
		version = (( ofaISetterInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaISetter::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_isetter_get_getter:
 * @instance: this #ofaISetter instance.
 *
 * Returns: the previously attached #ofaIGetter.
 */
ofaIGetter *
ofa_isetter_get_getter( ofaISetter *instance )
{
	static const gchar *thisfn = "ofa_isetter_get_getter";

	g_return_val_if_fail( instance && OFA_IS_ISETTER( instance ), NULL );

	if( OFA_ISETTER_GET_INTERFACE( instance )->get_getter ){
		return( OFA_ISETTER_GET_INTERFACE( instance )->get_getter( instance ));
	}

	g_info( "%s: ofaISetter's %s implementation does not provide 'get_getter()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_isetter_set_getter:
 * @instance: this #ofaISetter instance.
 * @getter: a #ofaIGetter of the application.
 *
 * Attach a @getter to the @instance.
 */
void
ofa_isetter_set_getter( ofaISetter *instance, ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_isetter_set_getter";
	ofaIGetter *permanent_getter;

	g_return_if_fail( instance && OFA_IS_ISETTER( instance ));
	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	if( OFA_ISETTER_GET_INTERFACE( instance )->set_getter ){
		permanent_getter = ofa_igetter_get_permanent_getter( getter );
		OFA_ISETTER_GET_INTERFACE( instance )->set_getter( instance, permanent_getter );
		return;
	}

	g_info( "%s: ofaISetter's %s implementation does not provide 'set_getter()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}
