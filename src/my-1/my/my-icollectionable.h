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

#ifndef __MY_API_MY_ICOLLECTIONABLE_H__
#define __MY_API_MY_ICOLLECTIONABLE_H__

/**
 * SECTION: icollectionable
 * @title: myICollectionable
 * @short_description: The ICollectionable Interface
 * @include: my/my-icollectionable.h
 *
 * The #myICollectionable interface should be implemented by all the
 * classes which wish the the #myICollector implementation maintains
 * a collection of their objects.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define MY_TYPE_ICOLLECTIONABLE                      ( my_icollectionable_get_type())
#define MY_ICOLLECTIONABLE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, MY_TYPE_ICOLLECTIONABLE, myICollectionable ))
#define MY_IS_ICOLLECTIONABLE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, MY_TYPE_ICOLLECTIONABLE ))
#define MY_ICOLLECTIONABLE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), MY_TYPE_ICOLLECTIONABLE, myICollectionableInterface ))

typedef struct _myICollectionable                    myICollectionable;

/**
 * myICollectionableInterface:
 * @get_interface_version: [should]: get the version number of the
 *                                   interface implementation.
 *
 * This defines the interface that an #myICollectionable may/should
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
	guint   ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * load_collection:
	 * @instance: a fake #myICollectionable instance.
	 * @user_data: user data to be passed to the @instance.
	 *
	 * Returns: the list of objects of the collection, or %NULL.
	 *
	 * Since: version 1.
	 */
	GList * ( *load_collection )      ( void *user_data );
}
	myICollectionableInterface;

/*
 * Interface-wide
 */
GType  my_icollectionable_get_type                  ( void );

guint  my_icollectionable_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint  my_icollectionable_get_interface_version     ( GType type );

GList *my_icollectionable_load_collection           ( GType type,
															void *user_data );

/*
 * Instance-wide
 */
G_END_DECLS

#endif /* __MY_API_MY_ICOLLECTIONABLE_H__ */
