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

#ifndef __OPENBOOK_API_OFA_IDBCUSTOMER_H__
#define __OPENBOOK_API_OFA_IDBCUSTOMER_H__

/**
 * SECTION: idbcustomer
 * @title: ofaIDBCustomer
 * @short_description: The DMBS Customer Interface
 * @include: openbook/ofa-idbcustomer.h
 *
 * This #ofaIDBCustomer lets a plugin announces that it makes use of
 * the DBMS. More precisely, it lets the plugin update the DDL model.
 */

#include "ofa-idbconnect.h"

G_BEGIN_DECLS

#define OFA_TYPE_IDBCUSTOMER                      ( ofa_idbcustomer_get_type())
#define OFA_IDBCUSTOMER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBCUSTOMER, ofaIDBCustomer ))
#define OFA_IS_IDBCUSTOMER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBCUSTOMER ))
#define OFA_IDBCUSTOMER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBCUSTOMER, ofaIDBCustomerInterface ))

typedef struct _ofaIDBCustomer                    ofaIDBCustomer;

/**
 * ofaIDBCustomerInterface:
 * @get_interface_version: [should]: returns the implemented version number.
 * @get_name: [should]: returns the name of the customer.
 * @needs_ddl_update: [should]: returns whether the DB model needs an update.
 * @ddl_update: [should]: returns whether the DB model has been successfully updated.
 *
 * This defines the interface that an #ofaIDBCustomer should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIDBCustomer provider.
	 *
	 * The application calls this method each time it needs to know
	 * which version of this interface the plugin implements.
	 *
	 * If this method is not implemented by the plugin,
	 * the application considers that the plugin only implements
	 * the version 1 of the ofaIDBCustomer interface.
	 *
	 * Returns: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1
	 */
	guint           ( *get_interface_version )( const ofaIDBCustomer *instance );

	/**
	 * get_name:
	 * @instance: the #ofaIDBCustomer provider.
	 *
	 * Returns: the name of @instance.
	 *
	 * The returned string is owned by the @instance, and should not be
	 * released by the caller.
	 *
	 * Since: version 1
	 */
	const gchar *   ( *get_name )             ( const ofaIDBCustomer *instance );

	/**
	 * needs_ddl_update:
	 * @instance: the #ofaIDBCustomer provider.
	 * @connect: the opened #ofaIDBConnect connection on the DBMS.
	 *
	 * Returns: %TRUE if the DB model needs an update, %FALSE else.
	 *
	 * Defaults to %FALSE.
	 *
	 * Since: version 1
	 */
	gboolean        ( *needs_ddl_update )     ( const ofaIDBCustomer *instance,
													const ofaIDBConnect *connect );

	/**
	 * ddl_update:
	 * @instance: the #ofaIDBCustomer provider.
	 * @connect: the opened #ofaIDBConnect connection on the DBMS.
	 *
	 * Returns: %TRUE if the DB model has been successfully updated,
	 * %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean        ( *ddl_update )           ( const ofaIDBCustomer *instance,
													const ofaIDBConnect *connect );
}
	ofaIDBCustomerInterface;

GType           ofa_idbcustomer_get_type                  ( void );

guint           ofa_idbcustomer_get_interface_last_version( void );

guint           ofa_idbcustomer_get_interface_version     ( const ofaIDBCustomer *instance );

const gchar    *ofa_idbcustomer_get_name                  ( const ofaIDBCustomer *instance );

gboolean        ofa_idbcustomer_needs_ddl_update          ( const ofaIDBCustomer *instance,
																const ofaIDBConnect *connect );

gboolean        ofa_idbcustomer_ddl_update                ( const ofaIDBCustomer *instance,
																const ofaIDBConnect *connect );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBCUSTOMER_H__ */
