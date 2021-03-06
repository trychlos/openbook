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

#include "api/ofa-igetter.h"
#include "api/ofa-isignalable.h"
#include "api/ofa-isignaler.h"

#define ISIGNALABLE_LAST_VERSION          1

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaISignalableInterface *klass );
static void  interface_base_finalize( ofaISignalableInterface *klass );

/**
 * ofa_isignalable_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_isignalable_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_isignalable_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_isignalable_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaISignalableInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaISignalable", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaISignalableInterface *klass )
{
	static const gchar *thisfn = "ofa_isignalable_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaISignalableInterface *klass )
{
	static const gchar *thisfn = "ofa_isignalable_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_isignalable_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_isignalable_get_interface_last_version( void )
{
	return( ISIGNALABLE_LAST_VERSION );
}

/**
 * ofa_isignalable_get_interface_version:
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
ofa_isignalable_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_ISIGNALABLE );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaISignalableInterface * ) iface )->get_interface_version ){
		version = (( ofaISignalableInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaISignalable::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_isignalable_connect_to:
 * @type: the implementation's GType.
 * @signaler: the #ofaISignaler instance to connect to.
 *
 * Let the implementation class connect to #ofaISignaler signaling system.
 */
void
ofa_isignalable_connect_to( GType type, ofaISignaler *signaler )
{
	gpointer klass, iface;

	g_return_if_fail( signaler && OFA_IS_ISIGNALER( signaler ));

	klass = g_type_class_ref( type );
	g_return_if_fail( klass );

	iface = g_type_interface_peek( klass, OFA_TYPE_ISIGNALABLE );

	if( iface && (( ofaISignalableInterface * ) iface )->connect_to ){
		(( ofaISignalableInterface * ) iface )->connect_to( signaler );

	} else {
		g_info( "%s implementation does not provide 'ofaISignalable::connect_to()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );
}
