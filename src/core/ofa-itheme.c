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

#include <string.h>

#include "api/my-utils.h"
#include "api/ofa-itheme.h"

#define ITHEME_LAST_VERSION             1

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIThemeInterface *klass );
static void  interface_base_finalize( ofaIThemeInterface *klass );

/**
 * ofa_itheme_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_itheme_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_itheme_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_itheme_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIThemeInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaITheme", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIThemeInterface *klass )
{
	static const gchar *thisfn = "ofa_itheme_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIThemeInterface *klass )
{
	static const gchar *thisfn = "ofa_itheme_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_itheme_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_itheme_get_interface_last_version( void )
{
	return( ITHEME_LAST_VERSION );
}

/**
 * ofa_itheme_get_interface_version:
 * @instance: this #ofaITheme instance.
 *
 * Returns: the version number of this interface implemented by the
 * @instance.
 *
 * Defaults to 1.
 */
guint
ofa_itheme_get_interface_version( const ofaITheme *instance )
{
	static const gchar *thisfn = "ofa_itheme_get_interface_version";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_ITHEME( instance ), 1 );

	if( OFA_ITHEME_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_ITHEME_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: ofaITheme instance %p does not provide 'get_interface_version()' method",
			thisfn, ( void * ) instance );
	return( 1 );
}

/**
 * ofa_itheme_add_theme:
 * @instance: this #ofaITheme instance.
 * @name: the name of the theme; the main window implementation
 *  displays this @name as the label of the page tab of the main
 *  notebook.
 * @fntype: a pointer to the get_type() function of the page.
 * @with_entries: whether the page will allow a 'View entries'
 *  button.
 *
 * Defines and records a new theme.
 *
 * Returns: the theme identifier, or 0.
 */
guint
ofa_itheme_add_theme( ofaITheme *instance, const gchar *name, gpointer fntype, gboolean with_entries )
{
	static const gchar *thisfn = "ofa_itheme_add_theme";

	g_debug( "%s: instance=%p, name=%s, fntype=%p, with_entries=%s",
			thisfn, ( void * ) instance, name, fntype, with_entries ? "True":"False" );

	g_return_val_if_fail( instance && OFA_IS_ITHEME( instance ), 0 );
	g_return_val_if_fail( my_strlen( name ), 0 );
	g_return_val_if_fail( fntype, 0 );

	if( OFA_ITHEME_GET_INTERFACE( instance )->add_theme ){
		return( OFA_ITHEME_GET_INTERFACE( instance )->add_theme( instance, name, fntype, with_entries ));
	}

	g_info( "%s: ofaITheme instance %p does not provide 'add_theme()' method",
			thisfn, ( void * ) instance );
	return( 0 );
}

/**
 * ofa_itheme_activate_theme:
 * @instance: this #ofaITheme instance.
 * @theme: the theme identifier.
 *
 * Activate the page defined by this @theme.
 */
void
ofa_itheme_activate_theme( ofaITheme *instance, guint theme )
{
	static const gchar *thisfn = "ofa_itheme_activate_theme";

	g_debug( "%s: instance=%p, theme=%u", thisfn, ( void * ) instance, theme );

	g_return_if_fail( instance && OFA_IS_ITHEME( instance ));
	g_return_if_fail( theme > 0 );

	if( OFA_ITHEME_GET_INTERFACE( instance )->activate_theme ){
		return( OFA_ITHEME_GET_INTERFACE( instance )->activate_theme( instance, theme ));
	}

	g_info( "%s: ofaITheme instance %p does not provide 'activate_theme()' method",
			thisfn, ( void * ) instance );
}
