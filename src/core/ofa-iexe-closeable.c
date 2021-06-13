/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#include <stdlib.h>

#include "my/my-utils.h"

#include "api/ofa-igetter.h"
#include "api/ofa-iexe-closeable.h"

#define IEXECLOSEABLE_LAST_VERSION	2

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIExeCloseableInterface *klass );
static void  interface_base_finalize( ofaIExeCloseableInterface *klass );

/**
 * ofa_iexe_closeable_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iexe_closeable_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iexe_closeable_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iexe_closeable_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIExeCloseableInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIExeCloseable", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIExeCloseableInterface *klass )
{
	static const gchar *thisfn = "ofa_iexe_closeable_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIExeCloseableInterface *klass )
{
	static const gchar *thisfn = "ofa_iexe_closeable_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iexe_closeable_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iexe_closeable_get_interface_last_version( void )
{
	return( IEXECLOSEABLE_LAST_VERSION );
}

/**
 * ofa_iexe_closeable_get_interface_version:
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
ofa_iexe_closeable_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IEXECLOSEABLE );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIExeCloseableInterface * ) iface )->get_interface_version ){
		version = (( ofaIExeCloseableInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIExeCloseable::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_iexe_closeable_add_row:
 * @instance: the #ofaIExeCloseable instance.
 * @closer: a #ofaIExeCloser instance, which should also be the #myIAssistant caller.
 * @rowtype: whether we insert on closing exercice N, or on opening
 *  exercice N+1.
 *
 * Ask @instance the text to be inserted as the row label if it wants
 * do some tasks at the moment specified by @rowtype.
 *
 * Returns: a string which should be g_free() by the caller.
 */
gchar *
ofa_iexe_closeable_add_row( ofaIExeCloseable *instance, ofaIExeCloser *closer, guint rowtype )
{
	static const gchar *thisfn = "ofa_iexe_closeable_add_row";

	g_debug( "%s: instance=%p, closer=%p, rowtype=%u", thisfn, ( void * ) instance, ( void * ) closer, rowtype );

	g_return_val_if_fail( instance && OFA_IS_IEXECLOSEABLE( instance ), NULL );

	if( OFA_IEXECLOSEABLE_GET_INTERFACE( instance )->add_row ){
		return( OFA_IEXECLOSEABLE_GET_INTERFACE( instance )->add_row( instance, closer, rowtype ));
	}

	g_info( "%s: ofaIExeCloseable's %s implementation does not provide 'add_row()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));

	return( NULL );
}

/**
 * ofa_iexe_closeable_do_task:
 * @instance: the #ofaIExeCloseable instance.
 * @closer: a #ofaIExeCloser instance, which should also be the #myIAssistant caller.
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
ofa_iexe_closeable_do_task( ofaIExeCloseable *instance, ofaIExeCloser *closer, guint rowtype, GtkWidget *box, ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_iexe_closeable_do_task";

	g_debug( "%s: instance=%p, closer=%p, rowtype=%u, box=%p, getter=%p",
			thisfn, ( void * ) instance, ( void * ) closer, rowtype, ( void * ) box, ( void * ) getter );

	g_return_val_if_fail( instance && OFA_IS_IEXECLOSEABLE( instance ), FALSE );
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	if( OFA_IEXECLOSEABLE_GET_INTERFACE( instance )->do_task ){
		return( OFA_IEXECLOSEABLE_GET_INTERFACE( instance )->do_task( instance, closer, rowtype, box, getter ));
	}

	g_info( "%s: ofaIExeCloseable's %s implementation does not provide 'do_task()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	/* returns %TRUE to let the process continue */
	return( TRUE );
}
