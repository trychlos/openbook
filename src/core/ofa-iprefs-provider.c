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

#include "api/ofa-iprefs-provider.h"

#define IPREFS_PROVIDER_LAST_VERSION    1

static guint st_initializations = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIPrefsProviderInterface *klass );
static void  interface_base_finalize( ofaIPrefsProviderInterface *klass );

/**
 * ofa_iprefs_provider_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iprefs_provider_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iprefs_provider_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iprefs_provider_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIPrefsProviderInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIPrefsProvider", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIPrefsProviderInterface *klass )
{
	static const gchar *thisfn = "ofa_iprefs_provider_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/*klass->get_interface_version = ipreferences_get_interface_version;*/
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIPrefsProviderInterface *klass )
{
	static const gchar *thisfn = "ofa_iprefs_provider_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iprefs_provider_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iprefs_provider_get_interface_last_version( void )
{
	return( IPREFS_PROVIDER_LAST_VERSION );
}

/**
 * ofa_iprefs_provider_get_interface_version:
 * @instance: this #ofaIPrefsProvider instance.
 *
 * Returns: the version number of this interface the plugin implements.
 */
guint
ofa_iprefs_provider_get_interface_version( const ofaIPrefsProvider *instance )
{
	static const gchar *thisfn = "ofa_iprefs_provider_get_interface_version";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IPREFS_PROVIDER( instance ), 0 );

	if( OFA_IPREFS_PROVIDER_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_IPREFS_PROVIDER_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: ofaIPrefsProvider instance %p does not provide 'get_interface_version()' method",
			thisfn, ( void * ) instance );
	return( 1 );
}

/**
 * ofa_iprefs_provider_new_page:
 * @instance: this #ofaIPrefsProvider instance.
 *
 * Returns: a newly allocated #ofaIPrefsPage object, which should be
 * g_object_unref() by the caller.
 */
ofaIPrefsPage *
ofa_iprefs_provider_new_page( ofaIPrefsProvider *instance )
{
	static const gchar *thisfn = "ofa_iprefs_provider_new_page";
	ofaIPrefsPage *page;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IPREFS_PROVIDER( instance ), NULL );

	if( OFA_IPREFS_PROVIDER_GET_INTERFACE( instance )->new_page ){
		page = OFA_IPREFS_PROVIDER_GET_INTERFACE( instance )->new_page();
		ofa_iprefs_page_set_provider( page, instance );
		return( page );
	}

	g_info( "%s: ofaIPrefsProvider instance %p does not provide 'new_page()' method",
			thisfn, ( void * ) instance );
	return( NULL );
}
