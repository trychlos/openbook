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

#ifndef __OPENBOOK_API_OFA_ISIGNALABLE_H__
#define __OPENBOOK_API_OFA_ISIGNALABLE_H__

/**
 * SECTION: isignalable
 * @title: ofaISignalable
 * @short_description: The Hub signaling system interface.
 * @include: openbook/ofa-isignalable.h
 *
 * The #ofaISignalable lets a implementing class connect to
 * #ofaISignaler signaling system.
 *
 * Candidate classes are typically of two types:
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

#include "api/ofa-igetter-def.h"
#include "api/ofa-isignaler.h"

G_BEGIN_DECLS

#define OFA_TYPE_ISIGNALABLE                      ( ofa_isignalable_get_type())
#define OFA_ISIGNALABLE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ISIGNALABLE, ofaISignalable ))
#define OFA_IS_ISIGNALABLE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ISIGNALABLE ))
#define OFA_ISIGNALABLE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ISIGNALABLE, ofaISignalableInterface ))

typedef struct _ofaISignalable                     ofaISignalable;

/**
 * ofaISignalableInterface:
 * @get_interface_version: [may]: returns the implemented version number.
 * @connect_to: [may]: let the class connect to the #ofaISignaler system.
 *
 * This defines the interface that an #ofaISignalable may/should implement.
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
	 * connect_to:
	 * @signaler: the #ofaISignaler instance to connect to.
	 *
	 * Connect to the #ofaISignaler signaling system.
	 *
	 * Since: version 1.
	 */
	void  ( *connect_to )           ( ofaISignaler *signaler );

	/*** instance-wide ***/
}
	ofaISignalableInterface;

/*
 * Interface-wide
 */
GType ofa_isignalable_get_type                  ( void );

guint ofa_isignalable_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint ofa_isignalable_get_interface_version     ( GType type );

void  ofa_isignalable_connect_to                ( GType type,
														ofaISignaler *signaler );

/*
 * Instance-wide
 */

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ISIGNALABLE_H__ */
