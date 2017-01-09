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
ofa_igetter_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IGETTER );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIGetterInterface * ) iface )->get_interface_version ){
		version = (( ofaIGetterInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIGetter::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_igetter_get_permanent_getter:
 * @instance: this #ofaIGetter instance.
 *
 * Returns: a permanent instance of an #ofaIGetter implementation.
 *
 * The exact returned implementation is only garanteed to be at least
 * the same life than those of the #ofoDossier.
 *
 * This is to be used when the life of the user is longer that those of
 * the provided #ofaIGetter.
 *
 * Example: a #ofaReconciliation page opens a #ofaAccountSelect composite
 * widget, providing it with its own #ofaIGetter. At some time then, this
 * #ofaReconciliation page is closed by the user, thus invalidating the
 * provided #ofaIGetter instance.
 *
 * But the #ofaAccountSelect life is as long as those of the #ofoDossier.
 * Next time it tries to access its initially-provided #ofaIGetter
 * interface, it will crash.
 *
 * Updating the #ofaIGetter instance stored by the composite widget is
 * not enough, because the inital instance may have been propagated to
 * another child widget, and so on.
 *
 * The most secure way is just for the composite widget which "knows"
 * that it will have a long life to store a permanent instance of a
 * #ofaIGetter implementation.
 */
ofaIGetter *
ofa_igetter_get_permanent_getter( const ofaIGetter *instance )
{
	static const gchar *thisfn = "ofa_igetter_get_permanent";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	if( OFA_IGETTER_GET_INTERFACE( instance )->get_permanent ){
		return( OFA_IGETTER_GET_INTERFACE( instance )->get_permanent( instance ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_permanent()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
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

	g_return_val_if_fail( instance && OFA_IS_IGETTER( instance ), NULL );

	if( OFA_IGETTER_GET_INTERFACE( instance )->get_main_window ){
		return( OFA_IGETTER_GET_INTERFACE( instance )->get_main_window( instance ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_main_window()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_igetter_get_page_manager:
 * @instance: this #ofaIGetter instance.
 *
 * Returns: the #ofaIPageManager instance of the application, if any,
 * or %NULL.
 *
 * Only a GUI application has a PageManager usage.
 */
ofaIPageManager *
ofa_igetter_get_page_manager( const ofaIGetter *instance )
{
	static const gchar *thisfn = "ofa_igetter_get_page_manager";

	g_return_val_if_fail( instance && OFA_IS_IGETTER( instance ), NULL );

	if( OFA_IGETTER_GET_INTERFACE( instance )->get_page_manager ){
		return( OFA_IGETTER_GET_INTERFACE( instance )->get_page_manager( instance ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_page_manager()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}
