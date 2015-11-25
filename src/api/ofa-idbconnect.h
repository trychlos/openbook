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

#ifndef __OPENBOOK_API_OFA_IDBCONNECT_H__
#define __OPENBOOK_API_OFA_IDBCONNECT_H__

/**
 * SECTION: idbconnect
 * @title: ofaIDBConnect
 * @short_description: The DMBS Connection Interface
 * @include: openbook/ofa-idbconnect.h
 *
 * The ofaIDB interfaces serie let the user choose and manage different
 * DBMS backends.
 * This #ofaIDBConnect is the interface a connection object instanciated
 * by a DBMS backend should implement for the needs of the application.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_IDBCONNECT                      ( ofa_idbconnect_get_type())
#define OFA_IDBCONNECT( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBCONNECT, ofaIDBConnect ))
#define OFA_IS_IDBCONNECT( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBCONNECT ))
#define OFA_IDBCONNECT_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBCONNECT, ofaIDBConnectInterface ))

typedef struct _ofaIDBConnect                    ofaIDBConnect;

/**
 * ofaIDBConnectInterface:
 * @get_interface_version: [should]: returns the implemented version number.
 *
 * This defines the interface that an #ofaIDBConnect should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIDBConnect provider.
	 *
	 * The application calls this method each time it needs to know
	 * which version of this interface the plugin implements.
	 *
	 * If this method is not implemented by the plugin,
	 * the application considers that the plugin only implements
	 * the version 1 of the ofaIDBConnect interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 */
	guint ( *get_interface_version )( const ofaIDBConnect *instance );
}
	ofaIDBConnectInterface;

GType ofa_idbconnect_get_type( void );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBCONNECT_H__ */
