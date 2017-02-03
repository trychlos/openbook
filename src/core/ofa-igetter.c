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
 * ofa_igetter_get_application:
 * @getter: this #ofaIGetter instance.
 *
 * Returns: the #GApplication.
 *
 * The returned reference is owned by the @getter instance, and should
 * not be released by the caller.
 */
GApplication *
ofa_igetter_get_application( const ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_igetter_get_application";

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	if( OFA_IGETTER_GET_INTERFACE( getter )->get_application ){
		return( OFA_IGETTER_GET_INTERFACE( getter )->get_application( getter ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_application()' method",
			thisfn, G_OBJECT_TYPE_NAME( getter ));
	return( NULL );
}

/**
 * ofa_igetter_get_auth_settings:
 * @getter: this #ofaIGetter instance.
 *
 * Returns: the #myISettings interface which manages the #ofaIAuth datas.
 *
 * The returned reference is owned by the @getter instance, and should
 * not be released by the caller.
 */
myISettings *
ofa_igetter_get_auth_settings( const ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_igetter_get_auth_settings";

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	if( OFA_IGETTER_GET_INTERFACE( getter )->get_auth_settings ){
		return( OFA_IGETTER_GET_INTERFACE( getter )->get_auth_settings( getter ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_auth_settings()' method",
			thisfn, G_OBJECT_TYPE_NAME( getter ));
	return( NULL );
}

/**
 * ofa_igetter_get_collector:
 * @getter: this #ofaIGetter instance.
 *
 * Returns: the #myICollector interface.
 *
 * The returned reference is owned by the @getter instance, and should
 * not be released by the caller.
 */
myICollector *
ofa_igetter_get_collector( const ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_igetter_get_collector";

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	if( OFA_IGETTER_GET_INTERFACE( getter )->get_collector ){
		return( OFA_IGETTER_GET_INTERFACE( getter )->get_collector( getter ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_collector()' method",
			thisfn, G_OBJECT_TYPE_NAME( getter ));
	return( NULL );
}

/**
 * ofa_igetter_get_dossier_collection:
 * @getter: this #ofaIGetter instance.
 *
 * Returns: the #ofaDossierCollection object which manages the
 * collection of dossiers (aka portfolios) from the settings point of
 * view.
 *
 * The returned reference is owned by the @getter instance, and should
 * not be released by the caller.
 */
ofaDossierCollection *
ofa_igetter_get_dossier_collection( const ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_igetter_get_dossier_collection";

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	if( OFA_IGETTER_GET_INTERFACE( getter )->get_dossier_collection ){
		return( OFA_IGETTER_GET_INTERFACE( getter )->get_dossier_collection( getter ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_dossier_collection()' method",
			thisfn, G_OBJECT_TYPE_NAME( getter ));
	return( NULL );
}

/**
 * ofa_igetter_get_dossier_settings:
 * @getter: this #ofaIGetter instance.
 *
 * Returns: the #myISettings interface which manages the dossiers
 * (aka portfolios) settings.
 *
 * The returned reference is owned by the @getter instance, and should
 * not be released by the caller.
 */
myISettings *
ofa_igetter_get_dossier_settings( const ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_igetter_get_dossier_settings";

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	if( OFA_IGETTER_GET_INTERFACE( getter )->get_dossier_settings ){
		return( OFA_IGETTER_GET_INTERFACE( getter )->get_dossier_settings( getter ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_dossier_settings()' method",
			thisfn, G_OBJECT_TYPE_NAME( getter ));
	return( NULL );
}

/**
 * ofa_igetter_get_extender_collection:
 * @getter: this #ofaIGetter instance.
 *
 * Returns: the extenders collection.
 *
 * The returned reference is owned by the @getter instance, and should
 * not be released by the caller.
 */
ofaExtenderCollection *
ofa_igetter_get_extender_collection( const ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_igetter_get_extender_collection";

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	if( OFA_IGETTER_GET_INTERFACE( getter )->get_extender_collection ){
		return( OFA_IGETTER_GET_INTERFACE( getter )->get_extender_collection( getter ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_extender_collection()' method",
			thisfn, G_OBJECT_TYPE_NAME( getter ));
	return( NULL );
}

/**
 * ofa_igetter_get_for_type:
 * @getter: this #ofaIGetter instance.
 *
 * Returns: a list of GObject's which implement the @type, concatenating
 * both those from the core library, and those advertized by the plugins.
 *
 * The returned references are owned by the @getter instance, and should
 * not be released by the caller.
 *
 * The returned list itself should still be #g_list_free() by the caller.
 */
GList *
ofa_igetter_get_for_type( const ofaIGetter *getter, GType type )
{
	static const gchar *thisfn = "ofa_igetter_get_for_type";

	g_debug( "%s: getter=%p, type=%lu", thisfn, ( void * ) getter, type );

	if( OFA_IGETTER_GET_INTERFACE( getter )->get_for_type ){
		return( OFA_IGETTER_GET_INTERFACE( getter )->get_for_type( getter, type ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_for_type()' method",
			thisfn, G_OBJECT_TYPE_NAME( getter ));
	return( NULL );
}

/**
 * ofa_igetter_get_hub:
 * @instance: this #ofaIGetter instance.
 *
 * Returns: the #ofaHub object of the application.
 *
 * The returned reference is owned by the @getter instance, and should
 * not be released by the caller.
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
 * ofa_igetter_get_openbook_props:
 * @getter: this #ofaIGetter instance.
 *
 * Returns: the #ofaOpenbookProps object.
 *
 * The returned reference is owned by the @getter instance, and should
 * not be released by the caller.
 */
ofaOpenbookProps *
ofa_igetter_get_openbook_props( const ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_igetter_get_openbook_props";

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	if( OFA_IGETTER_GET_INTERFACE( getter )->get_openbook_props ){
		return( OFA_IGETTER_GET_INTERFACE( getter )->get_openbook_props( getter ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_openbook_props()' method",
			thisfn, G_OBJECT_TYPE_NAME( getter ));
	return( NULL );
}

/**
 * ofa_igetter_get_runtime_dir:
 * @getter: this #ofaIGetter instance.
 *
 * Returns: the runtime directory from which the current program has
 * been run.
 *
 * The returned reference is owned by the @getter instance, and should
 * not be released by the caller.
 */
const gchar *
ofa_igetter_get_runtime_dir( const ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_igetter_get_runtime_dir";

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	if( OFA_IGETTER_GET_INTERFACE( getter )->get_runtime_dir ){
		return( OFA_IGETTER_GET_INTERFACE( getter )->get_runtime_dir( getter ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_runtime_dir()' method",
			thisfn, G_OBJECT_TYPE_NAME( getter ));
	return( NULL );
}

/**
 * ofa_igetter_get_signaler:
 * @getter: this #ofaIGetter instance.
 *
 * Returns: the #ofaISignaler instance.
 *
 * The returned reference is owned by the @getter instance, and should
 * not be released by the caller.
 */
ofaISignaler *
ofa_igetter_get_signaler( const ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_igetter_get_signaler";

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	if( OFA_IGETTER_GET_INTERFACE( getter )->get_signaler ){
		return( OFA_IGETTER_GET_INTERFACE( getter )->get_signaler( getter ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_signaler()' method",
			thisfn, G_OBJECT_TYPE_NAME( getter ));
	return( NULL );
}

/**
 * ofa_igetter_get_user_settings:
 * @getter: this #ofaIGetter instance.
 *
 * Returns: the #myISettings interface which manages the user preferences.
 *
 * The returned reference is owned by the @getter instance, and should
 * not be released by the caller.
 */
myISettings *
ofa_igetter_get_user_settings( const ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_igetter_get_user_settings";

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	if( OFA_IGETTER_GET_INTERFACE( getter )->get_user_settings ){
		return( OFA_IGETTER_GET_INTERFACE( getter )->get_user_settings( getter ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_user_settings()' method",
			thisfn, G_OBJECT_TYPE_NAME( getter ));
	return( NULL );
}

/**
 * ofa_igetter_get_main_window:
 * @getter: this #ofaIGetter instance.
 *
 * Returns: the main window of the application, if any, or %NULL if the
 * application does not have any main window (is not UI-related).
 *
 * The returned reference is owned by the @getter instance, and should
 * not be released by the caller.
 */
GtkApplicationWindow *
ofa_igetter_get_main_window( const ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_igetter_get_main_window";

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	if( OFA_IGETTER_GET_INTERFACE( getter )->get_main_window ){
		return( OFA_IGETTER_GET_INTERFACE( getter )->get_main_window( getter ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_main_window()' method",
			thisfn, G_OBJECT_TYPE_NAME( getter ));
	return( NULL );
}

/**
 * ofa_igetter_get_menu_manager:
 * @instance: this #ofaIGetter instance.
 *
 * Returns: the #ofaIPageManager instance of the application, if any,
 * or %NULL if the application does not have any main window (is not
 * UI-related).
 *
 * The returned reference is owned by the @getter instance, and should
 * not be released by the caller.
 */
myMenuManager *
ofa_igetter_get_menu_manager( const ofaIGetter *instance )
{
	static const gchar *thisfn = "ofa_igetter_get_menu_manager";

	g_return_val_if_fail( instance && OFA_IS_IGETTER( instance ), NULL );

	if( OFA_IGETTER_GET_INTERFACE( instance )->get_menu_manager ){
		return( OFA_IGETTER_GET_INTERFACE( instance )->get_menu_manager( instance ));
	}

	g_info( "%s: ofaIGetter's %s implementation does not provide 'get_menu_manager()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_igetter_get_page_manager:
 * @instance: this #ofaIGetter instance.
 *
 * Returns: the #ofaIPageManager instance of the application, if any,
 * or %NULL if the application does not have any main window (is not
 * UI-related).
 *
 * The returned reference is owned by the @getter instance, and should
 * not be released by the caller.
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
