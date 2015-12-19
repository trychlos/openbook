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

#include "api/ofa-idbconnect.h"

#include "ofa-tva.h"
#include "ofa-tva-dbcustomer.h"

static guint    idbcustomer_get_interface_version( const ofaIDBCustomer *instance );
static gboolean idbcustomer_needs_ddl_update( const ofaIDBCustomer *instance, const ofaIDBConnect *connect );
static gboolean idbcustomer_ddl_update( const ofaIDBCustomer *instance, const ofaIDBConnect *connect );

/*
 * #ofaIDBCustomer interface setup
 */
void
ofa_tva_dbcustomer_iface_init( ofaIDBCustomerInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_idbcustomer_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idbcustomer_get_interface_version;
	iface->needs_ddl_update = idbcustomer_needs_ddl_update;
	iface->ddl_update = idbcustomer_ddl_update;
}

/*
 * the version of the #ofaIDBCustomer interface implemented by the module
 */
static guint
idbcustomer_get_interface_version( const ofaIDBCustomer *instance )
{
	return( 1 );
}

static gboolean
idbcustomer_needs_ddl_update( const ofaIDBCustomer *instance, const ofaIDBConnect *connect )
{
	return( FALSE );
}

static gboolean
idbcustomer_ddl_update( const ofaIDBCustomer *instance, const ofaIDBConnect *connect )
{
	return( FALSE );
}
