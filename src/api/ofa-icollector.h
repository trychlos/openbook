/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_ICOLLECTOR_H__
#define __OPENBOOK_API_OFA_ICOLLECTOR_H__

/**
 * SECTION: icollector
 * @title: ofaICollector
 * @short_description: The ICollector Interface
 * @include: openbook/ofa-icollector.h
 *
 * The #ofaICollector interface lets an object manage collection(s) of
 * other objects.
 * It works by associating a GList of objects to a GType.
 *
 * It is expected that these other objects (whose collections are
 * managed by this #ofaICollector interface) implement themselves the
 * #ofaICollectionable interface.
 *
 * For Openbook needs, the #ofaICollector interface is implemented by
 * the #ofaHub class, so that it is able to manage the collections of
 * accounts, classes, currencies, and so on.
 *
 * Rather see #ofaISingleKeeper interface to associate a GType with a
 * single object.
 */

#include "api/ofa-hub-def.h"
#include "api/ofa-icollectionable.h"

G_BEGIN_DECLS

#define OFA_TYPE_ICOLLECTOR                      ( ofa_icollector_get_type())
#define OFA_ICOLLECTOR( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ICOLLECTOR, ofaICollector ))
#define OFA_IS_ICOLLECTOR( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ICOLLECTOR ))
#define OFA_ICOLLECTOR_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ICOLLECTOR, ofaICollectorInterface ))

typedef struct _ofaICollector                    ofaICollector;

/**
 * ofaICollectorInterface:
 * @get_interface_version: [should]: get the version number of the
 *                                   interface implementation.
 *
 * This defines the interface that an #ofaICollector may/should
 * implement.
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

	/*** instance-wide ***/
}
	ofaICollectorInterface;

/*
 * Interface-wide
 */
GType    ofa_icollector_get_type                  ( void );

guint    ofa_icollector_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint    ofa_icollector_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
GList   *ofa_icollector_get_collection            ( ofaICollector *instance,
														ofaHub *hub,
														GType type );

void     ofa_icollector_add_object                ( ofaICollector *instance,
														ofaHub *hub,
														ofaICollectionable *object,
														GCompareFunc func );

void     ofa_icollector_remove_object             ( ofaICollector *instance,
														const ofaICollectionable *object );

void     ofa_icollector_sort_collection           ( ofaICollector *instance,
														GType type,
														GCompareFunc func );

void     ofa_icollector_free_collection           ( ofaICollector *instance,
														GType type );

void     ofa_icollector_free_all                  ( ofaICollector *instance );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ICOLLECTOR_H__ */
