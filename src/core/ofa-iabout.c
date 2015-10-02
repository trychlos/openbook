/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#include <api/ofa-iabout.h>

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
 *
 */
guint
ofa_iabout_get_interface_last_version( void )
{
	return( IABOUT_LAST_VERSION );
}

/**
 * ofa_iabout_do_init:
 * @importer: this #ofaIAbout instance.
 *
 * Initialize the page to display the properties.
 */
GtkWidget *
ofa_iabout_do_init( const ofaIAbout *instance )
{
	static const gchar *thisfn = "ofa_iabout_do_init";
	GtkWidget *page;

	g_return_val_if_fail( instance && OFA_IS_IABOUT( instance ), NULL );

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	page = NULL;

	if( OFA_IABOUT_GET_INTERFACE( instance )->do_init ){
		page = OFA_IABOUT_GET_INTERFACE( instance )->do_init( instance );
	}

	return( page );
}
