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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "my/my-menu-manager.h"
#include "my/my-utils.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;

	/* runtime
	 */
	GList   *registered;
}
	myMenuManagerPrivate;

/* The data structure registered for each #myIActionMap.
 * This means one scope, one menu per map.
 */
typedef struct {
	myIActionMap *map;
	GMenuModel   *menu;
	gchar        *scope;
}
	sRegister;

/* signals defined here
 */
enum {
	REGISTER = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static sRegister *find_by_map( myMenuManager *self, myIActionMap *map );
static void       free_register( sRegister *sdata );

G_DEFINE_TYPE_EXTENDED( myMenuManager, my_menu_manager, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( myMenuManager ))

static void
menu_manager_finalize( GObject *instance )
{
	static const gchar *thisfn = "my_menu_manager_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && MY_IS_MENU_MANAGER( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_menu_manager_parent_class )->finalize( instance );
}

static void
menu_manager_dispose( GObject *instance )
{
	myMenuManagerPrivate *priv;

	g_return_if_fail( instance && MY_IS_MENU_MANAGER( instance ));

	priv = my_menu_manager_get_instance_private( MY_MENU_MANAGER( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_list_free_full( priv->registered, ( GDestroyNotify ) free_register );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_menu_manager_parent_class )->dispose( instance );
}

static void
my_menu_manager_init( myMenuManager *self )
{
	static const gchar *thisfn = "my_menu_manager_init";
	myMenuManagerPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && MY_IS_MENU_MANAGER( self ));

	priv = my_menu_manager_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->registered = NULL;
}

static void
my_menu_manager_class_init( myMenuManagerClass *klass )
{
	static const gchar *thisfn = "my_menu_manager_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = menu_manager_dispose;
	G_OBJECT_CLASS( klass )->finalize = menu_manager_finalize;

	/**
	 * myMenuManager::my-menu-manager-register:
	 *
	 * This signal is sent each time a new IActionMap has been registered.
	 *
	 * Handler is of type:
	 * void ( *handler )( myMenuManager  *manager,
	 * 						myIActionMap *map,
	 * 						const gchar  *scope,
	 * 						GMenuModel   *menu,
	 * 						gpointer      user_data );
	 */
	st_signals[ REGISTER ] = g_signal_new_class_handler(
				"my-menu-manager-register",
				MY_TYPE_MENU_MANAGER,
				G_SIGNAL_ACTION,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				3,
				G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_POINTER );
}

/**
 * my_menu_manager_new:
 *
 * Returns: a new #myMenuManager object.
 */
myMenuManager *
my_menu_manager_new( void )
{
	myMenuManager *self;

	self = g_object_new( MY_TYPE_MENU_MANAGER, NULL );

	return( self );
}

/**
 * my_menu_manager_register:
 * @manager: this #myMenuManager object.
 * @map: the #myIActionMap to be registered.
 * @scope: the scope of the @map.
 * @menu: the menu to be associated by the @map.
 *
 * Register the provided @map.
 *
 * This function takes care of initializing the #myIActionMap (thus
 * taking its own reference on the @menu).
 *
 * The 'my-menu-manager-register' signal is sent.
 */
void
my_menu_manager_register( myMenuManager *manager, myIActionMap *map, const gchar *scope, GMenuModel *menu )
{
	static const gchar *thisfn = "my_menu_manager_register";
	myMenuManagerPrivate *priv;
	sRegister *sdata;

	g_debug( "%s: manager=%p, map=%p, scope=%s, menu=%p",
			thisfn, ( void * ) manager, ( void * ) map, scope, ( void * ) menu );

	g_return_if_fail( manager && MY_IS_MENU_MANAGER( manager ));
	g_return_if_fail( map && MY_IS_IACTION_MAP( map ));
	g_return_if_fail( my_strlen( scope ) > 0 );
	g_return_if_fail( menu && G_IS_MENU_MODEL( menu ));

	priv = my_menu_manager_get_instance_private( manager );

	g_return_if_fail( !priv->dispose_has_run );

	sdata = find_by_map( manager, map );

	if( sdata ){
		g_warning( "%s: map=%p is already registered: scope=%s, menu=%p",
				thisfn, ( void * ) map, scope, ( void * ) menu );

	} else {
		sdata = g_new0( sRegister, 1 );
		sdata->map = map;
		priv->registered = g_list_prepend( priv->registered, sdata );

		my_iaction_map_register( map, scope, menu );

		g_signal_emit_by_name( manager, "my-menu-manager-register", map, scope, menu );
	}
}

/*
 * Returns: the sRegister structure which registers the @map
 */
static sRegister *
find_by_map( myMenuManager *self, myIActionMap *map )
{
	myMenuManagerPrivate *priv;
	GList *it;
	sRegister *sdata;

	priv = my_menu_manager_get_instance_private( self );

	for( it=priv->registered ; it ; it=it->next ){
		sdata = ( sRegister * ) it->data;
		if( sdata->map == map ){
			return( sdata );
		}
	}

	return( NULL );
}

static void
free_register( sRegister *sdata )
{
	g_free( sdata->scope );
	g_free( sdata );
}
