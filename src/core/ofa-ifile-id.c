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

#include "api/ofa-ifile-id.h"

#define IFILE_ID_LAST_VERSION           1

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIFileIdInterface *klass );
static void  interface_base_finalize( ofaIFileIdInterface *klass );

/**
 * ofa_ifile_id_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_ifile_id_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_ifile_id_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_ifile_id_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIFileIdInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIFileId", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIFileIdInterface *klass )
{
	static const gchar *thisfn = "ofa_ifile_id_interface_base_init";

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIFileIdInterface *klass )
{
	static const gchar *thisfn = "ofa_ifile_id_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_ifile_id_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_ifile_id_get_interface_last_version( void )
{
	return( IFILE_ID_LAST_VERSION );
}

/**
 * ofa_ifile_id_get_interface_version:
 * @instance: this #ofaIFileId instance.
 *
 * Returns: the version number implemented by the object.
 *
 * Defaults to 1.
 */
guint
ofa_ifile_id_get_interface_version( const ofaIFileId *instance )
{
	g_return_val_if_fail( instance && OFA_IS_IFILE_ID( instance ), 0 );

	if( OFA_IFILE_ID_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_IFILE_ID_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	return( 1 );
}

/**
 * ofa_ifile_id_get_dossier_name:
 * @instance: this #ofaIFileId instance.
 *
 * Returns: the identifier name of the dossier as a newly allocated
 * string which should be g_free() by the caller.
 */
gchar *
ofa_ifile_id_get_dossier_name( const ofaIFileId *instance )
{
	g_return_val_if_fail( instance && OFA_IS_IFILE_ID( instance ), NULL );

	if( OFA_IFILE_ID_GET_INTERFACE( instance )->get_dossier_name ){
		return( OFA_IFILE_ID_GET_INTERFACE( instance )->get_dossier_name( instance ));
	}

	return( NULL );
}

/**
 * ofa_ifile_id_get_provider_name:
 * @instance: this #ofaIFileId instance.
 *
 * Returns: the provider name as a newly allocated
 * string which should be g_free() by the caller.
 */
gchar *
ofa_ifile_id_get_provider_name( const ofaIFileId *instance )
{
	g_return_val_if_fail( instance && OFA_IS_IFILE_ID( instance ), NULL );

	if( OFA_IFILE_ID_GET_INTERFACE( instance )->get_provider_name ){
		return( OFA_IFILE_ID_GET_INTERFACE( instance )->get_provider_name( instance ));
	}

	return( NULL );
}

/**
 * ofa_ifile_id_get_provider_instance:
 * @instance: this #ofaIFileId instance.
 *
 * Returns: a new reference to the provider instance which should be
 * g_object_unref() by the caller.
 */
ofaIDbms *
ofa_ifile_id_get_provider_instance( const ofaIFileId *instance )
{
	g_return_val_if_fail( instance && OFA_IS_IFILE_ID( instance ), NULL );

	if( OFA_IFILE_ID_GET_INTERFACE( instance )->get_provider_instance ){
		return( OFA_IFILE_ID_GET_INTERFACE( instance )->get_provider_instance( instance ));
	}

	return( NULL );
}

/**
 * ofa_ifile_id_get_periods:
 * @instance: this #ofaIFileId instance.
 *
 * Returns: the list of defined financial periods as a #GList of any
 * GObject -derived object, as long as they implement the
 * #ofaIFilePeriod interface.
 *
 * The returned list should be #ofa_ifile_id_free_periods() by the
 * caller.
 */
GList *
ofa_ifile_id_get_periods( const ofaIFileId *instance )
{
	g_return_val_if_fail( instance && OFA_IS_IFILE_ID( instance ), NULL );

	if( OFA_IFILE_ID_GET_INTERFACE( instance )->get_periods ){
		return( OFA_IFILE_ID_GET_INTERFACE( instance )->get_periods( instance ));
	}

	return( NULL );
}
