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

#include "api/ofa-itheme-manager.h"

#define ITHEME_MANAGER_LAST_VERSION       1

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIThemeManagerInterface *klass );
static void  interface_base_finalize( ofaIThemeManagerInterface *klass );

/**
 * ofa_itheme_manager_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_itheme_manager_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_itheme_manager_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_itheme_manager_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIThemeManagerInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIThemeManager", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIThemeManagerInterface *klass )
{
	static const gchar *thisfn = "ofa_itheme_manager_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIThemeManagerInterface *klass )
{
	static const gchar *thisfn = "ofa_itheme_manager_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_itheme_manager_get_interface_last_version:
 * @instance: this #ofaIThemeManager instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_itheme_manager_get_interface_last_version( void )
{
	return( ITHEME_MANAGER_LAST_VERSION );
}

/**
 * ofa_itheme_manager_get_interface_version:
 * @instance: this #ofaIThemeManager instance.
 *
 * Returns: the version number of this interface implemented by the
 * @instance.
 *
 * Defaults to 1.
 */
guint
ofa_itheme_manager_get_interface_version( const ofaIThemeManager *instance )
{
	static const gchar *thisfn = "ofa_itheme_manager_get_interface_version";

	g_return_val_if_fail( instance && OFA_IS_ITHEME_MANAGER( instance ), 1 );

	if( OFA_ITHEME_MANAGER_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_ITHEME_MANAGER_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: ofaIThemeManager instance %p does not provide 'get_interface_version()' method",
			thisfn, ( void * ) instance );
	return( 1 );
}

/**
 * ofa_itheme_manager_define:
 * @instance: the #ofaIThemeManager instance.
 * @type: the desired GType type which happens to be used as the theme
 *  identifier.
 * @label: the tab title of the corresponding notebook page.
 *
 * Returns: the new theme identifier, strictly greater than zero.
 */
void
ofa_itheme_manager_define( ofaIThemeManager *instance, GType type, const gchar *label )
{
	static const gchar *thisfn = "ofa_itheme_manager_define";

	g_return_if_fail( instance && OFA_IS_ITHEME_MANAGER( instance ));

	if( OFA_ITHEME_MANAGER_GET_INTERFACE( instance )->define ){
		OFA_ITHEME_MANAGER_GET_INTERFACE( instance )->define( instance, type, label );
		return;
	}

	g_info( "%s: ofaIThemeManager instance %p does not provide 'define()' method",
			thisfn, ( void * ) instance );
}

/**
 * ofa_itheme_manager_activate:
 * @instance: the #ofaIThemeManager instance.
 * @type: the desired GType type which happens to be used as the theme
 *  identifier.
 *
 * Activates the page, creating it if needed.
 *
 * Returns: the theme's page.
 */
ofaPage *
ofa_itheme_manager_activate( ofaIThemeManager *instance, GType type )
{
	static const gchar *thisfn = "ofa_itheme_manager_activate";

	g_return_val_if_fail( instance && OFA_IS_ITHEME_MANAGER( instance ), NULL );

	if( OFA_ITHEME_MANAGER_GET_INTERFACE( instance )->activate ){
		return( OFA_ITHEME_MANAGER_GET_INTERFACE( instance )->activate( instance, type ));
	}

	g_info( "%s: ofaIThemeManager instance %p does not provide 'activate()' method",
			thisfn, ( void * ) instance );
	return( NULL );
}
