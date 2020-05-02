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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "my/my-iscope-map.h"
#include "my/my-scope-mapper.h"
#include "my/my-utils.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;

	/* runtime
	 */
	GList   *registered;
}
	myScopeMapperPrivate;

/* The data structure registered for each mapped #GActionMap.
 */
typedef struct {
	gchar      *scope;
	GActionMap *action_map;
	GMenuModel *menu_model;
}
	sRegister;

static sRegister  *find_by_map( myScopeMapper *self, const GActionMap *map );
static sRegister  *find_by_scope( myScopeMapper *self, const gchar *scope );
static void        free_register( sRegister *sdata );
static void        iscope_map_iface_init( myIScopeMapInterface *iface );
static GMenuModel *iscope_get_menu_model( const myIScopeMap *instance, const GActionMap *map );
static GActionMap *iscope_lookup_by_scope( const myIScopeMap *instance, const gchar *scope );

G_DEFINE_TYPE_EXTENDED( myScopeMapper, my_scope_mapper, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( myScopeMapper )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ISCOPE_MAP, iscope_map_iface_init ))

static void
scope_mapper_finalize( GObject *instance )
{
	static const gchar *thisfn = "my_scope_mapper_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && MY_IS_SCOPE_MAPPER( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_scope_mapper_parent_class )->finalize( instance );
}

static void
scope_mapper_dispose( GObject *instance )
{
	myScopeMapperPrivate *priv;

	g_return_if_fail( instance && MY_IS_SCOPE_MAPPER( instance ));

	priv = my_scope_mapper_get_instance_private( MY_SCOPE_MAPPER( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_list_free_full( priv->registered, ( GDestroyNotify ) free_register );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_scope_mapper_parent_class )->dispose( instance );
}

static void
my_scope_mapper_init( myScopeMapper *self )
{
	static const gchar *thisfn = "my_scope_mapper_init";
	myScopeMapperPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && MY_IS_SCOPE_MAPPER( self ));

	priv = my_scope_mapper_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->registered = NULL;
}

static void
my_scope_mapper_class_init( myScopeMapperClass *klass )
{
	static const gchar *thisfn = "my_scope_mapper_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = scope_mapper_dispose;
	G_OBJECT_CLASS( klass )->finalize = scope_mapper_finalize;
}

/**
 * my_scope_mapper_new:
 *
 * Returns: a new #myScopeMapper object.
 */
myScopeMapper *
my_scope_mapper_new( void )
{
	myScopeMapper *self;

	self = g_object_new( MY_TYPE_SCOPE_MAPPER, NULL );

	return( self );
}

/**
 * my_scope_mapper_register:
 * @mapper: this #myScopeMapper object.
 * @scope: the scope of the @map.
 * @action_map: the #GActionMap to be registered.
 * @µenu_model: the #GMenuModel to be associated to the @action_map.
 *
 * Register the provided @scope for the @map.
 *
 * Returns: %TRUE if the registering has been successful.
 */
gboolean
my_scope_mapper_register( myScopeMapper *mapper, const gchar *scope, GActionMap *action_map, GMenuModel *menu_model )
{
	static const gchar *thisfn = "my_scope_mapper_register";
	myScopeMapperPrivate *priv;
	sRegister *sdata;
	gboolean ok;

	g_debug( "%s: mapper=%p, scope=%s, action_map=%p, menu_model=%p",
			thisfn, ( void * ) mapper, scope, ( void * ) action_map, ( void * ) menu_model );

	g_return_val_if_fail( mapper && MY_IS_SCOPE_MAPPER( mapper ), FALSE );
	g_return_val_if_fail( my_strlen( scope ) > 0, FALSE );
	g_return_val_if_fail( action_map && G_IS_ACTION_MAP( action_map ), FALSE );
	g_return_val_if_fail( menu_model && G_IS_MENU_MODEL( menu_model ), FALSE );

	priv = my_scope_mapper_get_instance_private( mapper );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	ok = FALSE;

	sdata = find_by_scope( mapper, scope );
	if( sdata ){
		g_warning( "%s: scope=%s is already registered: action_map=%p, menu_model=%p",
				thisfn, scope, ( void * ) sdata->action_map, ( void * ) sdata->menu_model );

	} else {
		sdata = find_by_map( mapper, action_map );
		if( sdata ){
			g_warning( "%s: action_map=%p is already registered: scope=%s, menu_model=%p",
					thisfn, ( void * ) sdata->action_map, scope, ( void * ) sdata->menu_model );

		} else {
			sdata = g_new0( sRegister, 1 );
			sdata->scope = g_strdup( scope );
			sdata->action_map = action_map;
			sdata->menu_model = menu_model;
			priv->registered = g_list_prepend( priv->registered, sdata );
			ok = TRUE;
		}
	}

	return( ok );
}

/*
 * Returns: the sRegister structure which registers the @map
 */
static sRegister *
find_by_map( myScopeMapper *self, const GActionMap *map )
{
	myScopeMapperPrivate *priv;
	GList *it;
	sRegister *sdata;

	priv = my_scope_mapper_get_instance_private( self );

	for( it=priv->registered ; it ; it=it->next ){
		sdata = ( sRegister * ) it->data;
		if( sdata->action_map == map ){
			return( sdata );
		}
	}

	return( NULL );
}

/*
 * Returns: the sRegister structure which registers the @scope
 */
static sRegister *
find_by_scope( myScopeMapper *self, const gchar *scope )
{
	myScopeMapperPrivate *priv;
	GList *it;
	sRegister *sdata;

	priv = my_scope_mapper_get_instance_private( self );

	for( it=priv->registered ; it ; it=it->next ){
		sdata = ( sRegister * ) it->data;
		if( my_collate( sdata->scope, scope ) == 0 ){
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

/*
 * myIScopeMap interface management
 */
static void
iscope_map_iface_init( myIScopeMapInterface *iface )
{
	static const gchar *thisfn = "my_scope_mapper_iscope_map_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_menu_model = iscope_get_menu_model;
	iface->lookup_by_scope = iscope_lookup_by_scope;
}

static GMenuModel *
iscope_get_menu_model( const myIScopeMap *instance, const GActionMap *map )
{
	sRegister *sdata;

	sdata = find_by_map( MY_SCOPE_MAPPER( instance ), map );

	return( sdata ? sdata->menu_model : NULL );
}

static GActionMap *
iscope_lookup_by_scope( const myIScopeMap *instance, const gchar *scope )
{
	sRegister *sdata;

	sdata = find_by_scope( MY_SCOPE_MAPPER( instance ), scope );

	return( sdata ? sdata->action_map : NULL );
}
