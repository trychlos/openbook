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

#include "api/ofa-iabout.h"
#include "api/ofa-igetter.h"

#define IABOUT_LAST_VERSION       1

static guint st_initializations = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIAboutInterface *klass );
static void  interface_base_finalize( ofaIAboutInterface *klass );

/**
 * ofa_iabout_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iabout_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iabout_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iabout_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIAboutInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIAbout", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIAboutInterface *klass )
{
	static const gchar *thisfn = "ofa_iabout_interface_base_init";

	if( !st_initializations ){

		g_debug( "%s: klass%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/*klass->get_interface_version = iabout_get_interface_version;*/
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIAboutInterface *klass )
{
	static const gchar *thisfn = "ofa_iabout_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iabout_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iabout_get_interface_last_version( void )
{
	return( IABOUT_LAST_VERSION );
}

/**
 * ofa_iabout_get_interface_version:
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
ofa_iabout_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IABOUT );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIAboutInterface * ) iface )->get_interface_version ){
		version = (( ofaIAboutInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIAbout::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_iabout_do_init:
 * @instance: this #ofaIAbout instance.
 * @getter: a #ofaIGetter of the application.
 *
 * Initialize the page to display the properties.
 */
GtkWidget *
ofa_iabout_do_init( ofaIAbout *instance, ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_iabout_do_init";

	g_return_val_if_fail( instance && OFA_IS_IABOUT( instance ), NULL );
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	g_debug( "%s: instance=%p (%s), getter=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) getter );

	if( OFA_IABOUT_GET_INTERFACE( instance )->do_init ){
		return( OFA_IABOUT_GET_INTERFACE( instance )->do_init( instance, getter ));
	}

	g_info( "%s: ofaIAbout's %s implementation does not provide 'do_init()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}
