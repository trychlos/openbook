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
#include "api/ofa-iexe-closer.h"

#define IEXECLOSER_LAST_VERSION    1

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIExeCloserInterface *klass );
static void  interface_base_finalize( ofaIExeCloserInterface *klass );

/**
 * ofa_iexe_closer_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iexe_closer_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iexe_closer_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iexe_closer_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIExeCloserInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIExeCloser", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIExeCloserInterface *klass )
{
	static const gchar *thisfn = "ofa_iexe_closer_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIExeCloserInterface *klass )
{
	static const gchar *thisfn = "ofa_iexe_closer_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iexe_closer_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iexe_closer_get_interface_last_version( void )
{
	return( IEXECLOSER_LAST_VERSION );
}

/**
 * ofa_iexe_closer_get_interface_version:
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
ofa_iexe_closer_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IEXECLOSER );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIExeCloserInterface * ) iface )->get_interface_version ){
		version = (( ofaIExeCloserInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIExeCloser::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_iexe_closer_get_prev_begin_date:
 * @instance: the #ofaIExeCloser instance.
 *
 * Returns: the beginning date of the exercice N, or %NULL.
 */
const GDate *
ofa_iexe_closer_get_prev_begin_date( ofaIExeCloser *instance )
{
	static const gchar *thisfn = "ofa_iexe_closer_get_prev_begin_date";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IEXECLOSER( instance ), NULL );

	if( OFA_IEXECLOSER_GET_INTERFACE( instance )->get_prev_begin_date ){
		return( OFA_IEXECLOSER_GET_INTERFACE( instance )->get_prev_begin_date( instance ));
	}

	g_info( "%s: ofaIExeCloser's %s implementation does not provide 'get_prev_begin_date()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));

	return( NULL );
}

/**
 * ofa_iexe_closer_get_prev_end_date:
 * @instance: the #ofaIExeCloser instance.
 *
 * Returns: the ending date of the exercice N, or %NULL.
 */
const GDate *
ofa_iexe_closer_get_prev_end_date( ofaIExeCloser *instance )
{
	static const gchar *thisfn = "ofa_iexe_closer_get_prev_end_date";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IEXECLOSER( instance ), NULL );

	if( OFA_IEXECLOSER_GET_INTERFACE( instance )->get_prev_end_date ){
		return( OFA_IEXECLOSER_GET_INTERFACE( instance )->get_prev_end_date( instance ));
	}

	g_info( "%s: ofaIExeCloser's %s implementation does not provide 'get_prev_end_date()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));

	return( NULL );
}

/**
 * ofa_iexe_closer_get_next_begin_date:
 * @instance: the #ofaIExeCloser instance.
 *
 * Returns: the beginning date of the exercice N+1, or %NULL.
 */
const GDate *
ofa_iexe_closer_get_next_begin_date( ofaIExeCloser *instance )
{
	static const gchar *thisfn = "ofa_iexe_closer_get_next_begin_date";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IEXECLOSER( instance ), NULL );

	if( OFA_IEXECLOSER_GET_INTERFACE( instance )->get_next_begin_date ){
		return( OFA_IEXECLOSER_GET_INTERFACE( instance )->get_next_begin_date( instance ));
	}

	g_info( "%s: ofaIExeCloser's %s implementation does not provide 'get_next_begin_date()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));

	return( NULL );
}

/**
 * ofa_iexe_closer_get_next_end_date:
 * @instance: the #ofaIExeCloser instance.
 *
 * Returns: the ending date of the exercice N+1, or %NULL.
 */
const GDate *
ofa_iexe_closer_get_next_end_date( ofaIExeCloser *instance )
{
	static const gchar *thisfn = "ofa_iexe_closer_get_next_end_date";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IEXECLOSER( instance ), NULL );

	if( OFA_IEXECLOSER_GET_INTERFACE( instance )->get_next_end_date ){
		return( OFA_IEXECLOSER_GET_INTERFACE( instance )->get_next_end_date( instance ));
	}

	g_info( "%s: ofaIExeCloser's %s implementation does not provide 'get_next_end_date()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));

	return( NULL );
}
