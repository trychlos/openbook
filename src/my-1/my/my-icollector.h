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

#ifndef __MY_API_MY_ICOLLECTOR_H__
#define __MY_API_MY_ICOLLECTOR_H__

/**
 * SECTION: icollector
 * @title: myICollector
 * @short_description: The ICollector Interface
 * @include: my/my-icollector.h
 *
 * The #myICollector interface lets an object manage collection(s)
 * (resp. single) objects.
 * It works by associating a #GList of objects (resp. a #GObject)
 * to a GType.
 *
 * It is expected that these other objects (whose collections are
 * managed by this #myICollector interface) implement themselves the
 * #myICollectionable interface.
 *
 * For Openbook needs, the #myICollector interface is implemented by
 * the #ofaHub class, so that it is able to manage the collections of
 * accounts, classes, currencies, and so on.
 */

#include "my/my-icollectionable.h"

G_BEGIN_DECLS

#define MY_TYPE_ICOLLECTOR                      ( my_icollector_get_type())
#define MY_ICOLLECTOR( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, MY_TYPE_ICOLLECTOR, myICollector ))
#define MY_IS_ICOLLECTOR( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, MY_TYPE_ICOLLECTOR ))
#define MY_ICOLLECTOR_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), MY_TYPE_ICOLLECTOR, myICollectorInterface ))

typedef struct _myICollector                    myICollector;

/**
 * myICollectorInterface:
 * @get_interface_version: [should]: get the version number of the
 *                                   interface implementation.
 *
 * This defines the interface that an #myICollector may/should
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
	myICollectorInterface;

/*
 * Interface-wide
 */
GType    my_icollector_get_type                  ( void );

guint    my_icollector_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint    my_icollector_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
GList   *my_icollector_collection_get            ( myICollector *instance,
														GType type,
														void *user_data );

void     my_icollector_collection_add_object     ( myICollector *instance,
														myICollectionable *object,
														GCompareFunc func,
														void *user_data );

void     my_icollector_collection_remove_object  ( myICollector *instance,
														const myICollectionable *object );

void     my_icollector_collection_sort           ( myICollector *instance,
														GType type,
														GCompareFunc func );

void     my_icollector_collection_free           ( myICollector *instance,
														GType type );

GList   *my_icollector_collection_get_list       ( myICollector *instance );

GObject *my_icollector_single_get_object         ( myICollector *instance,
														GType type );

void     my_icollector_single_set_object         ( myICollector *instance,
														void *object );

GList   *my_icollector_single_get_list           ( myICollector *instance );

gchar   *my_icollector_item_get_name             ( myICollector *instance,
														void *item );

guint    my_icollector_item_get_count            ( myICollector *instance,
														void *item );

void     my_icollector_free_all                  ( myICollector *instance );

G_END_DECLS

#endif /* __MY_API_MY_ICOLLECTOR_H__ */
