/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_ISIGNAL_HUB_H__
#define __OPENBOOK_API_OFA_ISIGNAL_HUB_H__

/**
 * SECTION: isignal_hub
 * @title: ofaISignalHub
 * @short_description: The Hub signaling system interface.
 * @include: openbook/ofa-isignal-hub.h
 *
 * The #ofaISignalHub lets a implementing class connect to hub signaling
 * system. Candidate classes are of two types:
 * - either a core class, and it must so be registered via the
 *   #ofa_hub_register_types() method at startup time;
 * - or a class provided by a plugin, and it is so dynamically requested
 *   at startup time.
 *
 * From the maintainer point of view, defining an interface which has to
 * be implemented by client classes let us report the coding charge to
 * the client class only without having to explicitely connect to hub
 * signaling system from the ofaHub code (as long as core type has been
 * registered).
 */

#include <glib-object.h>

#include "api/ofa-hub-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ISIGNAL_HUB                      ( ofa_isignal_hub_get_type())
#define OFA_ISIGNAL_HUB( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ISIGNAL_HUB, ofaISignalHub ))
#define OFA_IS_ISIGNAL_HUB( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ISIGNAL_HUB ))
#define OFA_ISIGNAL_HUB_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ISIGNAL_HUB, ofaISignalHubInterface ))

typedef struct _ofaISignalHub                     ofaISignalHub;

/**
 * ofaISignalHubInterface:
 * @get_interface_version: [may]: returns the implemented version number.
 *
 * This defines the interface that an #ofaISignalHub may/should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/*** implementation-wide ***/
	/**
	 * get_interface_version:
	 *
	 * Returns: the version number of this interface which is managed
	 * by the implementation.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1.
	 */
	guint ( *get_interface_version )( void );

	/**
	 * connect:
	 * @hub: the #ofaHub object of the application.
	 *
	 * Connect to the hub signaling system.
	 *
	 * Since: version 1.
	 */
	void  ( *connect )              ( ofaHub *hub );

	/*** instance-wide ***/
}
	ofaISignalHubInterface;

/*
 * Interface-wide
 */
GType ofa_isignal_hub_get_type                  ( void );

guint ofa_isignal_hub_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint ofa_isignal_hub_get_interface_version     ( GType type );

/*
 * Instance-wide
 */

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ISIGNAL_HUB_H__ */
