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

#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-iexeclose-close.h"
#include "api/ofa-plugin.h"
#include "api/ofa-settings.h"

#define IEXECLOSE_CLOSE_LAST_VERSION    1

static guint st_initializations         = 0;	/* interface initialization count */

static GType    register_type( void );
static void     interface_base_init( ofaIExeCloseCloseInterface *klass );
static void     interface_base_finalize( ofaIExeCloseCloseInterface *klass );

/**
 * ofa_iexeclose_close_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iexeclose_close_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iexeclose_close_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iexeclose_close_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIExeCloseCloseInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIExeCloseClose", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIExeCloseCloseInterface *klass )
{
	static const gchar *thisfn = "ofa_iexeclose_close_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIExeCloseCloseInterface *klass )
{
	static const gchar *thisfn = "ofa_iexeclose_close_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iexeclose_close_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iexeclose_close_get_interface_last_version( void )
{
	return( IEXECLOSE_CLOSE_LAST_VERSION );
}

/**
 * ofa_iexeclose_close_get_interface_version:
 * @instance: this #ofaIExeCloseClose instance.
 *
 * Returns: the version number of this interface the plugin implements.
 */
guint
ofa_iexeclose_close_get_interface_version( const ofaIExeCloseClose *instance )
{
	static const gchar *thisfn = "ofa_iexeclose_close_get_interface_version";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IEXECLOSE_CLOSE( instance ), 0 );

	if( OFA_IEXECLOSE_CLOSE_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_IEXECLOSE_CLOSE_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: ofaIExeCloseClose instance %p does not provide 'get_interface_version()' method",
			thisfn, ( void * ) instance );
	return( 1 );
}

/**
 * ofa_iexeclose_close_add_row:
 * @instance: the #ofaIExeCloseClose instance.
 * @rowtype: whether we insert on closing exercice N, or on opening
 *  exercice N+1.
 *
 * Ask @instance the text to be inserted as the row label if it wants
 * do some tasks at the moment specified by @rowtype.
 *
 * Returns: a string which will be #g_free() by the caller.
 */
gchar *
ofa_iexeclose_close_add_row( ofaIExeCloseClose *instance, guint rowtype )
{
	static const gchar *thisfn = "ofa_iexeclose_close_add_row";

	g_debug( "%s: instance=%p, rowtype=%u", thisfn, ( void * ) instance, rowtype );

	g_return_val_if_fail( instance && OFA_IS_IEXECLOSE_CLOSE( instance ), NULL );

	if( OFA_IEXECLOSE_CLOSE_GET_INTERFACE( instance )->add_row ){
		return( OFA_IEXECLOSE_CLOSE_GET_INTERFACE( instance )->add_row( instance, rowtype ));
	}

	g_info( "%s: ofaIExeCloseClose instance %p does not provide 'add_row()' method",
			thisfn, ( void * ) instance );
	return( NULL );
}

/**
 * ofa_iexeclose_close_do_task:
 * @instance: the #ofaIExeCloseClose instance.
 * @rowtype: whether we insert on closing exercice N, or on opening
 *  exercice N+1.
 * @box: a #GtkBox in which the plugin may display a text, a progress
 *  bar, or whatever...
 * @hub: the current #ofaHub object.
 *
 * Ask @instance to do its tasks.
 *
 * Returns: %TRUE if the plugin tasks are successful, %FALSE else.
 */
gboolean
ofa_iexeclose_close_do_task( ofaIExeCloseClose *instance, guint rowtype, GtkWidget *box, ofaHub *hub )
{
	static const gchar *thisfn = "ofa_iexeclose_close_do_task";

	g_debug( "%s: instance=%p, rowtype=%u, box=%p, hub=%p",
			thisfn, ( void * ) instance, rowtype, ( void * ) box, ( void * ) hub );

	g_return_val_if_fail( instance && OFA_IS_IEXECLOSE_CLOSE( instance ), FALSE );

	if( OFA_IEXECLOSE_CLOSE_GET_INTERFACE( instance )->do_task ){
		return( OFA_IEXECLOSE_CLOSE_GET_INTERFACE( instance )->do_task( instance, rowtype, box, hub ));
	}

	g_info( "%s: ofaIExeCloseClose instance %p does not provide 'do_task()' method",
			thisfn, ( void * ) instance );
	/* returns %TRUE to let the process continue */
	return( TRUE );
}
