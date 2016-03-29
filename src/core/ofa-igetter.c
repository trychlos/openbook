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

#include "api/ofa-igetter.h"

#define IGETTER_LAST_VERSION              1

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIGetterInterface *klass );
static void  interface_base_finalize( ofaIGetterInterface *klass );

/**
 * ofa_igetter_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_igetter_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_igetter_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_igetter_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIGetterInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIGetter", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIGetterInterface *klass )
{
	static const gchar *thisfn = "ofa_igetter_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIGetterInterface *klass )
{
	static const gchar *thisfn = "ofa_igetter_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_igetter_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_igetter_get_interface_last_version( void )
{
	return( IGETTER_LAST_VERSION );
}

/**
 * ofa_igetter_get_interface_version:
 * @instance: this #ofaIGetter instance.
 *
 * Returns: the version number of this interface the plugin implements.
 */
guint
ofa_igetter_get_interface_version( const ofaIGetter *instance )
{
	static const gchar *thisfn = "ofa_igetter_get_interface_version";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IGETTER( instance ), 0 );

	if( OFA_IGETTER_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_IGETTER_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_interface_version()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( 1 );
}

/**
 * ofa_igetter_get_application:
 * @instance: this #ofaIGetter instance.
 *
 * Returns: the #GApplication.
 */
GApplication *
ofa_igetter_get_application( const ofaIGetter *instance )
{
	static const gchar *thisfn = "ofa_igetter_get_application";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IGETTER( instance ), NULL );

	if( OFA_IGETTER_GET_INTERFACE( instance )->get_application ){
		return( OFA_IGETTER_GET_INTERFACE( instance )->get_application( instance ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_application()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_igetter_get_hub:
 * @instance: this #ofaIGetter instance.
 *
 * Returns: the #ofaHub global object of the application.
 *
 * It is not expected that the #ofaIGetter @instance maintains itself
 * the #ofaHub primary reference. It is enough for the @instance to be
 * able to return a reference to the object.
 *
 * From another point of view, the Openbook architecture makes almost
 * mandatory for any application to have one global #ofaHub object.
 */
ofaHub *
ofa_igetter_get_hub( const ofaIGetter *instance )
{
	static const gchar *thisfn = "ofa_igetter_get_hub";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IGETTER( instance ), NULL );

	if( OFA_IGETTER_GET_INTERFACE( instance )->get_hub ){
		return( OFA_IGETTER_GET_INTERFACE( instance )->get_hub( instance ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_hub()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_igetter_get_main_window:
 * @instance: this #ofaIGetter instance.
 *
 * Returns: the main window of the application, if any, or %NULL.
 */
GtkApplicationWindow *
ofa_igetter_get_main_window( const ofaIGetter *instance )
{
	static const gchar *thisfn = "ofa_igetter_get_main_window";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IGETTER( instance ), NULL );

	if( OFA_IGETTER_GET_INTERFACE( instance )->get_main_window ){
		return( OFA_IGETTER_GET_INTERFACE( instance )->get_main_window( instance ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_main_window()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_igetter_get_theme_manager:
 * @instance: this #ofaIGetter instance.
 *
 * Returns: the #ofaIThemeManager instance of the application, if any,
 * or %NULL.
 *
 * Only a GUI application has a ThemeManager usage.
 */
ofaIThemeManager *
ofa_igetter_get_theme_manager( const ofaIGetter *instance )
{
	static const gchar *thisfn = "ofa_igetter_get_theme_manager";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IGETTER( instance ), NULL );

	if( OFA_IGETTER_GET_INTERFACE( instance )->get_theme_manager ){
		return( OFA_IGETTER_GET_INTERFACE( instance )->get_theme_manager( instance ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_theme_manager()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}
