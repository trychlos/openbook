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

#ifndef __MY_API_MY_MENU_MANAGER_H__
#define __MY_API_MY_MENU_MANAGER_H__

/**
 * SECTION: my_menu_manager
 * @short_description: #myMenuManager class definition.
 * @include: my/my-menu-manager.h
 *
 * A convenience class to manage the menus of the applications by
 * registering here all managed menus.
 *
 * This class is expected to have only one object which groups all
 * IActionMap's et MenuModel's.
 *
 * Defined signals
 *
 * - 'my-menu-manager-register': send each time a new #myIActionMap
 *   has been registered; this means that a new menu is now known and
 *   that is a good time for updating it (if needed).
 */

#include <gio/gio.h>

#include "my/my-iaction-map.h"

G_BEGIN_DECLS

#define MY_TYPE_MENU_MANAGER                ( my_menu_manager_get_type())
#define MY_MENU_MANAGER( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_MENU_MANAGER, myMenuManager ))
#define MY_MENU_MANAGER_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_MENU_MANAGER, myMenuManagerClass ))
#define MY_IS_MENU_MANAGER( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_MENU_MANAGER ))
#define MY_IS_MENU_MANAGER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_MENU_MANAGER ))
#define MY_MENU_MANAGER_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_MENU_MANAGER, myMenuManagerClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	myMenuManager;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	myMenuManagerClass;

GType          my_menu_manager_get_type( void ) G_GNUC_CONST;

myMenuManager *my_menu_manager_new     ( void );

void           my_menu_manager_register( myMenuManager *manager,
												myIActionMap *map,
												const gchar *scope,
												GMenuModel *menu );

G_END_DECLS

#endif /* __MY_API_MY_MENU_MANAGER_H__ */
