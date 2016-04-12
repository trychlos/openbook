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

#ifndef __MY_API_MY_IACTION_MAP_H__
#define __MY_API_MY_IACTION_MAP_H__

/**
 * SECTION: iaction_map
 * @title: myIActionMap
 * @short_description: The IActionMap Interface
 * @include: my/my-iaction-map.h
 *
 * The #myIActionMap interface let complete the GActionMap action interface.
 */

#include <gio/gio.h>

G_BEGIN_DECLS

#define MY_TYPE_IACTION_MAP                      ( my_iaction_map_get_type())
#define MY_IACTION_MAP( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, MY_TYPE_IACTION_MAP, myIActionMap ))
#define MY_IS_IACTION_MAP( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, MY_TYPE_IACTION_MAP ))
#define MY_IACTION_MAP_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), MY_TYPE_IACTION_MAP, myIActionMapInterface ))

typedef struct _myIActionMap                     myIActionMap;

/**
 * myIActionMapInterface:
 *
 * This defines the interface that an #myIActionMap should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

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
	guint                ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * get_menu_model:
	 * @instance: the #myIActionMap instance.
	 *
	 * Returns: the #GMenuModel of the @instance.
	 *
	 * Since: version 1.
	 */
	GMenuModel *         ( *get_menu_model )       ( const myIActionMap *instance );

	/**
	 * get_action_target:
	 * @instance: the #myIActionMap instance.
	 *
	 * Returns: the action target of the @instance.
	 *
	 * Since: version 1.
	 */
	const gchar *        ( *get_action_target )    ( const myIActionMap *instance );
}
	myIActionMapInterface;

/*
 * Interface-wide
 */
GType               my_iaction_map_get_type                  ( void );

guint               my_iaction_map_get_interface_last_version( const myIActionMap *instance );

/*
 * Implementation-wide
 */
guint               my_iaction_map_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
GMenuModel         *my_iaction_map_get_menu_model            ( const myIActionMap *instance );

GAction            *my_iaction_map_lookup_action             ( myIActionMap *instance,
																	const gchar *name );

G_END_DECLS

#endif /* __MY_API_MY_IACTION_MAP_H__ */
