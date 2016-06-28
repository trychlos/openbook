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
ofa_itheme_manager_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_ITHEME_MANAGER );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIThemeManagerInterface * ) iface )->get_interface_version ){
		version = (( ofaIThemeManagerInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIThemeManager::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
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

	g_info( "%s: ofaIThemeManager's %s implementation does not provide 'define()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
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

	g_info( "%s: ofaIThemeManager's %s implementation does not provide 'activate()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}
