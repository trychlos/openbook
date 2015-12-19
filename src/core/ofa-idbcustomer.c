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

#include <glib/gi18n.h>

#include "api/my-utils.h"
#include "api/ofa-idbcustomer.h"

#define IDBCUSTOMER_LAST_VERSION        1

static guint st_initializations         = 0;	/* interface initialization count */

static GType           register_type( void );
static void            interface_base_init( ofaIDBCustomerInterface *klass );
static void            interface_base_finalize( ofaIDBCustomerInterface *klass );

/**
 * ofa_idbcustomer_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_idbcustomer_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_idbcustomer_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_idbcustomer_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIDBCustomerInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIDBCustomer", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIDBCustomerInterface *klass )
{
	static const gchar *thisfn = "ofa_idbcustomer_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIDBCustomerInterface *klass )
{
	static const gchar *thisfn = "ofa_idbcustomer_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_idbcustomer_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_idbcustomer_get_interface_last_version( void )
{
	return( IDBCUSTOMER_LAST_VERSION );
}

/**
 * ofa_idbcustomer_get_interface_version:
 * @instance: this #ofaIDBCustomer instance.
 *
 * Returns: the version number of this interface the plugin implements.
 */
guint
ofa_idbcustomer_get_interface_version( const ofaIDBCustomer *instance )
{
	static const gchar *thisfn = "ofa_idbcustomer_get_interface_version";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IDBCUSTOMER( instance ), 0 );

	if( OFA_IDBCUSTOMER_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_IDBCUSTOMER_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: ofaIDBCustomer instance %p does not provide 'get_interface_version()' method",
			thisfn, ( void * ) instance );
	return( 1 );
}

/**
 * ofa_idbcustomer_needs_ddl_update:
 * @instance: this #ofaIDBCustomer instance.
 * @connect: the opened #ofaIDBConnect connection.
 *
 * Returns: %TRUE if the DB model needs a DDL update, %FALSE else.
 */
gboolean
ofa_idbcustomer_needs_ddl_update( const ofaIDBCustomer *instance, const ofaIDBConnect *connect )
{
	static const gchar *thisfn = "ofa_idbcustomer_needs_ddl_update";

	g_debug( "%s: instance=%p, connect=%p", thisfn, ( void * ) instance, ( void * ) connect );

	g_return_val_if_fail( instance && OFA_IS_IDBCUSTOMER( instance ), FALSE );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	if( OFA_IDBCUSTOMER_GET_INTERFACE( instance )->needs_ddl_update ){
		return( OFA_IDBCUSTOMER_GET_INTERFACE( instance )->needs_ddl_update( instance, connect ));
	}

	g_info( "%s: ofaIDBCustomer instance %p does not provide 'needs_ddl_update()' method",
			thisfn, ( void * ) instance );
	return( FALSE );
}

/**
 * ofa_idbcustomer_ddl_update:
 * @instance: this #ofaIDBCustomer instance.
 * @connect: the opened #ofaIDBConnect connection.
 *
 * Returns: %TRUE if the DB model has been successfully updated, %FALSE
 * else.
 */
gboolean
ofa_idbcustomer_ddl_update( const ofaIDBCustomer *instance, const ofaIDBConnect *connect )
{
	static const gchar *thisfn = "ofa_idbcustomer_ddl_update";

	g_debug( "%s: instance=%p, connect=%p", thisfn, ( void * ) instance, ( void * ) connect );

	g_return_val_if_fail( instance && OFA_IS_IDBCUSTOMER( instance ), FALSE );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	if( OFA_IDBCUSTOMER_GET_INTERFACE( instance )->ddl_update ){
		return( OFA_IDBCUSTOMER_GET_INTERFACE( instance )->ddl_update( instance, connect ));
	}

	g_info( "%s: ofaIDBCustomer instance %p does not provide 'ddl_update()' method",
			thisfn, ( void * ) instance );
	return( FALSE );
}
