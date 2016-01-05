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

#include "api/ofa-hub.h"
#include "api/ofa-ihubber.h"

#define IHUBBER_LAST_VERSION            1

static guint st_initializations         = 0;	/* interface initialization count */

/* signals defined here
 */
enum {
	HUB_OPENED = 0,
	HUB_CLOSED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static GType register_type( void );
static void  interface_base_init( ofaIHubberInterface *klass );
static void  interface_base_finalize( ofaIHubberInterface *klass );
static void  on_hub_finalized( ofaIHubber *instance, GObject *finalized_hub );

/**
 * ofa_ihubber_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_ihubber_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_ihubber_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_ihubber_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIHubberInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIHubber", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIHubberInterface *klass )
{
	static const gchar *thisfn = "ofa_ihubber_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/**
		 * ofaIHubber::hub-opened
		 *
		 * This signal is sent on the #ofaIHubber instance after a new
		 * #ofaHub object has been instanciated.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaIHubber *instance,
		 * 						ofaHub   *hub,
		 * 						gpointer  user_data );
		 */
		st_signals[ HUB_OPENED ] = g_signal_new_class_handler(
					SIGNAL_HUBBER_NEW,
					OFA_TYPE_IHUBBER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					1,
					G_TYPE_POINTER );

		/**
		 * ofaIHubber::hub-closed
		 *
		 * This signal is sent on the #ofaIHubber instance after an
		 * existing #ofaHub object has been unreffed.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaIHubber *instance,
		 * 						gpointer  user_data );
		 */
		st_signals[ HUB_CLOSED ] = g_signal_new_class_handler(
					SIGNAL_HUBBER_CLOSED,
					OFA_TYPE_IHUBBER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					0 );
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIHubberInterface *klass )
{
	static const gchar *thisfn = "ofa_ihubber_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_ihubber_get_interface_last_version:
 *
 * Returns: the last version of this interface.
 */
guint
ofa_ihubber_get_interface_last_version( void )
{
	return( IHUBBER_LAST_VERSION );
}

/**
 * ofa_ihubber_get_interface_version:
 * @instance: this #ofaIHubber instance.
 *
 * Returns: the version number of this interface the @instance
 * implements.
 *
 * Defaults to 1.
 */
guint
ofa_ihubber_get_interface_version( const ofaIHubber *instance )
{
	static const gchar *thisfn = "ofa_ihubber_get_interface_version";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IHUBBER( instance ), 0 );

	if( OFA_IHUBBER_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_IHUBBER_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: ofaIHubber instance %p does not provide 'get_interface_version()' method",
			thisfn, ( void * ) instance );
	return( 1 );
}

/**
 * ofa_ihubber_new_hub:
 * @instance: this #ofaIHubber instance.
 * @connect: a valid #ofaIDBConnect connection on the targeted
 *  database.
 *
 * Instanciates a new #ofaHub object, unreffing the previous one
 * if it exists.
 *
 * Returns: the newly instanciated #ofaHub object on success,
 * or %NULL.
 */
ofaHub *
ofa_ihubber_new_hub( ofaIHubber *instance, const ofaIDBConnect *connect )
{
	static const gchar *thisfn = "ofa_ihubber_new_hub";
	ofaHub *hub;

	g_debug( "%s: instance=%p, connect=%p", thisfn, ( void * ) instance, ( void * ) connect );

	g_return_val_if_fail( instance && OFA_IS_IHUBBER( instance ), NULL );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), NULL );

	if( OFA_IHUBBER_GET_INTERFACE( instance )->new_hub ){
		hub = OFA_IHUBBER_GET_INTERFACE( instance )->new_hub( instance, connect );
		if( hub ){
			g_signal_emit_by_name( instance, SIGNAL_HUBBER_NEW, hub );
			g_object_weak_ref( G_OBJECT( hub ), ( GWeakNotify ) on_hub_finalized, instance );
		}
		return( hub );
	}

	g_info( "%s: ofaIHubber instance %p does not provide 'new_hub()' method",
			thisfn, ( void * ) instance );
	return( NULL );
}

/**
 * ofa_ihubber_get_hub:
 * @instance: this #ofaIHubber instance.
 *
 * Returns: the main #ofaHub that the @instance maintains.
 */
ofaHub *
ofa_ihubber_get_hub( const ofaIHubber *instance )
{
	static const gchar *thisfn = "ofa_ihubber_get_hub";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IHUBBER( instance ), NULL );

	if( OFA_IHUBBER_GET_INTERFACE( instance )->get_hub ){
		return( OFA_IHUBBER_GET_INTERFACE( instance )->get_hub( instance ));
	}

	g_info( "%s: ofaIHubber instance %p does not provide 'get_hub()' method",
			thisfn, ( void * ) instance );
	return( NULL );
}

/**
 * ofa_ihubber_clear_hub:
 * @instance: this #ofaIHubber instance.
 *
 * Clears the #ofaHub object.
 */
void
ofa_ihubber_clear_hub( ofaIHubber *instance )
{
	static const gchar *thisfn = "ofa_ihubber_clear_hub";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_if_fail( instance && OFA_IS_IHUBBER( instance ));

	if( OFA_IHUBBER_GET_INTERFACE( instance )->clear_hub ){
		OFA_IHUBBER_GET_INTERFACE( instance )->clear_hub( instance );
		return;
	}

	g_info( "%s: ofaIHubber instance %p does not provide 'clear_hub()' method",
			thisfn, ( void * ) instance );
}

static void
on_hub_finalized( ofaIHubber *instance, GObject *finalized_hub )
{
	static const gchar *thisfn = "ofa_ihubber_on_hub_finalized";

	g_debug( "%s: instance=%p, finalized_hub=%p",
			thisfn, ( void * ) instance, ( void * ) finalized_hub );

	g_signal_emit_by_name( instance, SIGNAL_HUBBER_CLOSED );
}
