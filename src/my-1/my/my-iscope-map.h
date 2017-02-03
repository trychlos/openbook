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

#ifndef __MY_API_MY_ISCOPE_MAP_H__
#define __MY_API_MY_ISCOPE_MAP_H__

/**
 * SECTION: iscope_map
 * @title: myIScopeMap
 * @short_description: The IScopeMap Interface
 * @include: my/my-iaction-map.h
 *
 * The #myIScopeMap interface let complete the #GActionMap interface
 * (which associates names to actions inside of a scope) in order to
 * associate scopes to #GActionMap's.
 *
 * This association is for example used by the #myAccelGroup class when
 * defining the accelerators exposed by a menu.
 *
 * This interface is in particular implemented by #myScopeMapper class.
 */

#include <gio/gio.h>

G_BEGIN_DECLS

#define MY_TYPE_ISCOPE_MAP                      ( my_iscope_map_get_type())
#define MY_ISCOPE_MAP( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, MY_TYPE_ISCOPE_MAP, myIScopeMap ))
#define MY_IS_ISCOPE_MAP( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, MY_TYPE_ISCOPE_MAP ))
#define MY_ISCOPE_MAP_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), MY_TYPE_ISCOPE_MAP, myIScopeMapInterface ))

typedef struct _myIScopeMap                     myIScopeMap;

/**
 * myIScopeMapInterface:
 * @get_interface_version: [should] get the version of this interface
 *                                  that the plugin implements.
 * @get_menu_model: [should]: get the current menu model.
 *
 * This defines the interface that an #myIScopeMap should implement.
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
	 * @mapper: this #myIScopeMap instance.
	 * @action_map: the searched for #GActionMap.
	 *
	 * Returns: the #GMenuModel associated with the provided @action_map.
	 *
	 * Defaults to %NULL.
	 *
	 * Since: version 1.
	 */
	GMenuModel *         ( *get_menu_model )       ( const myIScopeMap *mapper,
														const GActionMap *action_map );

	/**
	 * lookup_by_scope:
	 * @mapper: this #myIScopeMap instance.
	 * @scope: the searched for scope.
	 *
	 * Returns: the #GActionMap associated with the provided @scope.
	 *
	 * Defaults to %NULL.
	 *
	 * Since: version 1.
	 */
	GActionMap *         ( *lookup_by_scope )      ( const myIScopeMap *mapper,
														const gchar *scope );
}
	myIScopeMapInterface;

/*
 * Interface-wide
 */
GType       my_iscope_map_get_type                  ( void );

guint       my_iscope_map_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint       my_iscope_map_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
GMenuModel *my_iscope_map_get_menu_model            ( const myIScopeMap *mapper,
															GActionMap *action_map );

GActionMap *my_iscope_map_lookup_by_scope           ( const myIScopeMap *mapper,
															const gchar *scope );

G_END_DECLS

#endif /* __MY_API_MY_ISCOPE_MAP_H__ */
