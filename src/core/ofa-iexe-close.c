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

#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-utils.h"

#include "api/ofa-igetter.h"
#include "api/ofa-iexe-close.h"

#define IEXECLOSE_LAST_VERSION    1

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIExeCloseInterface *klass );
static void  interface_base_finalize( ofaIExeCloseInterface *klass );

/**
 * ofa_iexe_close_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iexe_close_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iexe_close_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iexe_close_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIExeCloseInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIExeClose", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIExeCloseInterface *klass )
{
	static const gchar *thisfn = "ofa_iexe_close_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIExeCloseInterface *klass )
{
	static const gchar *thisfn = "ofa_iexe_close_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iexe_close_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iexe_close_get_interface_last_version( void )
{
	return( IEXECLOSE_LAST_VERSION );
}

/**
 * ofa_iexe_close_get_interface_version:
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
ofa_iexe_close_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IEXECLOSE );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIExeCloseInterface * ) iface )->get_interface_version ){
		version = (( ofaIExeCloseInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIExeClose::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_iexe_close_add_row:
 * @instance: the #ofaIExeClose instance.
 * @rowtype: whether we insert on closing exercice N, or on opening
 *  exercice N+1.
 *
 * Ask @instance the text to be inserted as the row label if it wants
 * do some tasks at the moment specified by @rowtype.
 *
 * Returns: a string which will be #g_free() by the caller.
 */
gchar *
ofa_iexe_close_add_row( ofaIExeClose *instance, guint rowtype )
{
	static const gchar *thisfn = "ofa_iexe_close_add_row";

	g_debug( "%s: instance=%p, rowtype=%u", thisfn, ( void * ) instance, rowtype );

	g_return_val_if_fail( instance && OFA_IS_IEXECLOSE( instance ), NULL );

	if( OFA_IEXECLOSE_GET_INTERFACE( instance )->add_row ){
		return( OFA_IEXECLOSE_GET_INTERFACE( instance )->add_row( instance, rowtype ));
	}

	g_info( "%s: ofaIExeClose's %s implementation does not provide 'add_row()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_iexe_close_do_task:
 * @instance: the #ofaIExeClose instance.
 * @rowtype: whether we insert on closing exercice N, or on opening
 *  exercice N+1.
 * @box: a #GtkBox in which the plugin may display a text, a progress
 *  bar, or whatever...
 * @getter: a #ofaIGetter instance.
 *
 * Ask @instance to do its tasks.
 *
 * Returns: %TRUE if the plugin tasks are successful, %FALSE else.
 */
gboolean
ofa_iexe_close_do_task( ofaIExeClose *instance, guint rowtype, GtkWidget *box, ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_iexe_close_do_task";

	g_debug( "%s: instance=%p, rowtype=%u, box=%p, getter=%p",
			thisfn, ( void * ) instance, rowtype, ( void * ) box, ( void * ) getter );

	g_return_val_if_fail( instance && OFA_IS_IEXECLOSE( instance ), FALSE );
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	if( OFA_IEXECLOSE_GET_INTERFACE( instance )->do_task ){
		return( OFA_IEXECLOSE_GET_INTERFACE( instance )->do_task( instance, rowtype, box, getter ));
	}

	g_info( "%s: ofaIExeClose's %s implementation does not provide 'do_task()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	/* returns %TRUE to let the process continue */
	return( TRUE );
}
