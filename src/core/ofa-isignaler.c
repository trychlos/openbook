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
#include "api/ofa-isignaler.h"

#define ISIGNALER_LAST_VERSION              1

/* signals defined here
 */
enum {
	/* non-UI related */
	NEW_OBJECT = 0,

	/* UI-related */
	PAGE_MANAGER_AVAILABLE,
	MENU_AVAILABLE,
	N_SIGNALS
};

static gint st_signals[ N_SIGNALS ]     = { 0 };

static guint st_initializations         =   0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaISignalerInterface *klass );
static void  interface_base_finalize( ofaISignalerInterface *klass );

/**
 * ofa_isignaler_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_isignaler_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_isignaler_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_isignaler_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaISignalerInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaISignaler", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaISignalerInterface *klass )
{
	static const gchar *thisfn = "ofa_isignaler_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/**
		 * ofaISignaler::ofa-signaler-page-manager-available:
		 *
		 * The signal is emitted when the #ofaIPageManager is available
		 * to register new themes.
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaISignaler     *signaler,
		 * 							ofaIPageManager *manager,
		 * 							gpointer         user_data );
		 */
		st_signals[ PAGE_MANAGER_AVAILABLE ] = g_signal_new_class_handler(
					"ofa-signaler-page-manager-available",
					OFA_TYPE_ISIGNALER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					1,
					G_TYPE_POINTER );

		/**
		 * ofaISignaler::ofa-signaler-menu-available:
		 *
		 * The signal is emitted each time a new menu has been registered
		 * by the #myMenuManager.
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaISignaler *signaler,
		 * 							const gchar *scope,
		 * 							GActionMap  *action_map,
		 * 							gpointer     user_data );
		 */
		st_signals[ MENU_AVAILABLE ] = g_signal_new_class_handler(
					"ofa-signaler-menu-available",
					OFA_TYPE_ISIGNALER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					2,
					G_TYPE_STRING, G_TYPE_POINTER );
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaISignalerInterface *klass )
{
	static const gchar *thisfn = "ofa_isignaler_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_isignaler_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_isignaler_get_interface_last_version( void )
{
	return( ISIGNALER_LAST_VERSION );
}

/**
 * ofa_isignaler_get_interface_version:
 * @instance: this #ofaISignaler instance.
 *
 * Returns: the version number of this interface implemented by the
 * @instance.
 *
 * Defaults to 1.
 */
guint
ofa_isignaler_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_ISIGNALER );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaISignalerInterface * ) iface )->get_interface_version ){
		version = (( ofaISignalerInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaISignaler::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

#if 0
/**
 * ofa_isignaler_get_getter:
 * @instance: this #ofaISignaler instance.
 *
 * Returns: the previously attached #ofaIGetter.
 */
ofaIGetter *
ofa_isignaler_get_getter( ofaISignaler *instance )
{
	static const gchar *thisfn = "ofa_isignaler_get_getter";

	g_return_val_if_fail( instance && OFA_IS_ISIGNALER( instance ), NULL );

	if( OFA_ISIGNALER_GET_INTERFACE( instance )->get_getter ){
		return( OFA_ISIGNALER_GET_INTERFACE( instance )->get_getter( instance ));
	}

	g_info( "%s: ofaISignaler's %s implementation does not provide 'get_getter()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_isignaler_set_getter:
 * @instance: this #ofaISignaler instance.
 * @getter: a #ofaIGetter of the application.
 *
 * Attach a @getter to the @instance.
 */
void
ofa_isignaler_set_getter( ofaISignaler *instance, ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_isignaler_set_getter";
	ofaIGetter *permanent_getter;

	g_return_if_fail( instance && OFA_IS_ISIGNALER( instance ));
	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	if( OFA_ISIGNALER_GET_INTERFACE( instance )->set_getter ){
		permanent_getter = getter;
		OFA_ISIGNALER_GET_INTERFACE( instance )->set_getter( instance, permanent_getter );
		return;
	}

	g_info( "%s: ofaISignaler's %s implementation does not provide 'set_getter()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}
#endif
