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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "my/my-utils.h"

#include "api/ofa-iactionable.h"

#define IACTIONABLE_LAST_VERSION   1

/* a data structure attached to the instance
 * - action_groups are indexed by their name.
 */
#define IACTIONABLE_DATA          "ofa-iactionable-data"

typedef struct {
	GHashTable           *action_groups;

	/* used during enumeration
	 */
	ofaIActionable       *enum_instance;
	ofaIActionableEnumCb  enum_cb;
	void                 *enum_data;
}
	sIActionable;

typedef struct {
	GMenu              *menu;
	GSimpleActionGroup *action_group;
}
	sActionGroup;

static guint st_initializations         = 0;	/* interface initialization count */

static GType         register_type( void );
static void          interface_base_init( ofaIActionableInterface *klass );
static void          interface_base_finalize( ofaIActionableInterface *klass );
static void          enum_action_groups_cb( const gchar *key, sActionGroup *sgroup, sIActionable *sdata );
static void          set_action_group( ofaIActionable *instance, sIActionable *sdata, const gchar *group_name, GAction *action );
static sActionGroup *get_group_by_name( ofaIActionable *instance, sIActionable *sdata, const gchar *group_name );
static sIActionable *get_instance_data( ofaIActionable *instance );
static void          on_instance_finalized( sIActionable *sdata, GObject *finalized_instance );
static void          free_action_group( sActionGroup *sgroup );

/**
 * ofa_iactionable_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iactionable_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iactionable_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iactionable_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIActionableInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIActionable", &info, 0 );

	g_type_interface_add_prerequisite( type, GTK_TYPE_WIDGET );

	return( type );
}

static void
interface_base_init( ofaIActionableInterface *klass )
{
	static const gchar *thisfn = "ofa_iactionable_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIActionableInterface *klass )
{
	static const gchar *thisfn = "ofa_iactionable_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iactionable_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iactionable_get_interface_last_version( void )
{
	return( IACTIONABLE_LAST_VERSION );
}

/**
 * ofa_iactionable_get_interface_version:
 * @instance: this #ofaIActionable instance.
 *
 * Returns: the version number of this interface implemented by the
 * @instance.
 *
 * Defaults to 1.
 */
guint
ofa_iactionable_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IACTIONABLE );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIActionableInterface * ) iface )->get_interface_version ){
		version = (( ofaIActionableInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIActionable::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_iactionable_enum_action_groups:
 * @instance: this #ofaIActionable instance.
 * @cb: the callback.
 * @user_data: user data.
 *
 * Enumerates the recorded action groups.
 */
void
ofa_iactionable_enum_action_groups( ofaIActionable *instance, ofaIActionableEnumCb cb, void *user_data )
{
	sIActionable *sdata;

	g_return_if_fail( instance && OFA_IS_IACTIONABLE( instance ));
	g_return_if_fail( cb );

	sdata = get_instance_data( instance );
	sdata->enum_instance = instance;
	sdata->enum_cb = cb;
	sdata->enum_data = user_data;

	g_hash_table_foreach( sdata->action_groups, ( GHFunc ) enum_action_groups_cb, sdata );
}

static void
enum_action_groups_cb( const gchar *key, sActionGroup *sgroup, sIActionable *sdata )
{
	( sdata->enum_cb )( sdata->enum_instance, key, G_ACTION_GROUP( sgroup->action_group ), sdata->enum_data );
}

/**
 * ofa_iactionable_get_action_group:
 * @instance: this #ofaIActionable instance.
 * @group_name: the name of the action group.
 *
 * Returns: the #GActionGroup attached to this @group_name.
 *
 * The returned reference is owned by the interface, and should not be
 * unreffed by the caller.
 */
GActionGroup *
ofa_iactionable_get_action_group( ofaIActionable *instance, const gchar *group_name )
{
	sIActionable *sdata;
	sActionGroup *sgroup;

	g_return_val_if_fail( instance && OFA_IS_IACTIONABLE( instance ), NULL );
	g_return_val_if_fail( my_strlen( group_name ), NULL );

	sdata = get_instance_data( instance );
	sgroup = get_group_by_name( instance, sdata, group_name );

	return( G_ACTION_GROUP( sgroup->action_group ));
}

/**
 * ofa_iactionable_get_menu:
 * @instance: this #ofaIActionable instance.
 * @group_name: the name of the action group.
 *
 * Returns: the menu attached to this @group_name.
 *
 * The returned reference is owned by the interface, and should not be
 * unreffed by the caller.
 */
GMenu *
ofa_iactionable_get_menu( ofaIActionable *instance, const gchar *group_name )
{
	sIActionable *sdata;
	sActionGroup *sgroup;

	g_return_val_if_fail( instance && OFA_IS_IACTIONABLE( instance ), NULL );
	g_return_val_if_fail( my_strlen( group_name ), NULL );

	sdata = get_instance_data( instance );
	sgroup = get_group_by_name( instance, sdata, group_name );

	return( sgroup->menu );
}

/**
 * ofa_iactionable_set_action:
 * @instance: this #ofaIActionable instance.
 * @group_name: the name of the action group.
 * @action: the action.
 *
 * Records an @action in the action group identified by @group_name.
 */
void
ofa_iactionable_set_action( ofaIActionable *instance, const gchar *group_name, GAction *action )
{
	sIActionable *sdata;

	g_return_if_fail( instance && OFA_IS_IACTIONABLE( instance ));
	g_return_if_fail( my_strlen( group_name ));
	g_return_if_fail( action && G_IS_ACTION( action ));

	sdata = get_instance_data( instance );
	set_action_group( instance, sdata, group_name, action );
}

/**
 * ofa_iactionable_set_menu_item:
 * @instance: this #ofaIActionable instance.
 * @group_name: the name of the action group.
 * @action: the action.
 * @item_label: the label of the item to be appended.
 *
 * Returns: the menu item appended to the menu for this action group.
 *
 * The returned menu item is owned by the interface, and should not be
 * unreffed by the caller.
 */
GMenuItem *
ofa_iactionable_set_menu_item( ofaIActionable *instance, const gchar *group_name, GAction *action, const gchar *item_label )
{
	sIActionable *sdata;
	sActionGroup *sgroup;
	gchar *action_name;
	GMenuItem *menu_item;

	g_return_val_if_fail( instance && OFA_IS_IACTIONABLE( instance ), NULL );
	g_return_val_if_fail( my_strlen( group_name ), NULL );
	g_return_val_if_fail( action && G_IS_ACTION( action ), NULL );
	g_return_val_if_fail( my_strlen( item_label ), NULL );

	sdata = get_instance_data( instance );
	set_action_group( instance, sdata, group_name, action );
	sgroup = get_group_by_name( instance, sdata, group_name );

	action_name = g_strdup_printf( "%s.%s", group_name, g_action_get_name( action ));
	menu_item = g_menu_item_new( item_label, action_name );
	g_menu_append_item( sgroup->menu, menu_item );
	g_object_unref( menu_item );
	g_free( action_name );

	return( menu_item );
}

/**
 * ofa_iactionable_set_button:
 * @instance: this #ofaIActionable instance.
 * @group_name: the name of the action group.
 * @action: the action.
 * @item_label: the label of the item to be appended.
 *
 * Returns: a new button associated to the @action.
 *
 * The returned button is not owned by anyone, and should be
 * #gtk_widget_destroy() by the caller.
 */
GtkWidget *
ofa_iactionable_set_button( ofaIActionable *instance, const gchar *group_name, GAction *action, const gchar *button_label )
{
	sIActionable *sdata;
	gchar *action_name;
	GtkWidget *button;

	g_return_val_if_fail( instance && OFA_IS_IACTIONABLE( instance ), NULL );
	g_return_val_if_fail( my_strlen( group_name ), NULL );
	g_return_val_if_fail( action && G_IS_ACTION( action ), NULL );
	g_return_val_if_fail( my_strlen( button_label ), NULL );

	sdata = get_instance_data( instance );
	set_action_group( instance, sdata, group_name, action );

	action_name = g_strdup_printf( "%s.%s", group_name, g_action_get_name( action ));
	button = gtk_button_new_with_mnemonic( button_label );
	gtk_actionable_set_action_name( GTK_ACTIONABLE( button ), action_name );
	g_free( action_name );

	return( button );
}

static void
set_action_group( ofaIActionable *instance, sIActionable *sdata, const gchar *group_name, GAction *action )
{
	sActionGroup *sgroup;

	sgroup = get_group_by_name( instance, sdata, group_name );

	if( !g_action_map_lookup_action( G_ACTION_MAP( sgroup->action_group ), g_action_get_name( action ))){
		g_action_map_add_action( G_ACTION_MAP( sgroup->action_group ), action );
	}
}

static sActionGroup *
get_group_by_name( ofaIActionable *instance, sIActionable *sdata, const gchar *group_name )
{
	sActionGroup *sgroup;

	sgroup = ( sActionGroup * ) g_hash_table_lookup( sdata->action_groups, group_name );

	if( !sgroup ){
		sgroup = g_new0( sActionGroup, 1 );
		sgroup->menu = g_menu_new();
		sgroup->action_group = g_simple_action_group_new();
		g_hash_table_insert( sdata->action_groups, g_strdup( group_name ), sgroup );
		gtk_widget_insert_action_group( GTK_WIDGET( instance ), group_name, G_ACTION_GROUP( sgroup->action_group ));
	}

	return( sgroup );
}

/*
 * returns the sIActionable data structure attached to the instance
 */
static sIActionable *
get_instance_data( ofaIActionable *instance )
{
	sIActionable *sdata;

	sdata = ( sIActionable * ) g_object_get_data( G_OBJECT( instance ), IACTIONABLE_DATA );

	if( !sdata ){
		sdata = g_new0( sIActionable, 1 );
		sdata->action_groups = g_hash_table_new_full( g_str_hash, g_str_equal, g_free, ( GDestroyNotify ) free_action_group );

		g_object_set_data( G_OBJECT( instance ), IACTIONABLE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );
	}

	return( sdata );
}

static void
on_instance_finalized( sIActionable *sdata, GObject *instance )
{
	static const gchar *thisfn = "ofa_iactionable_on_instance_finalized";

	g_debug( "%s: sdata=%p, instance=%p", thisfn, ( void * ) sdata, ( void * ) instance );

	g_hash_table_remove_all( sdata->action_groups );
	g_hash_table_unref( sdata->action_groups );

	g_free( sdata );
}

static void
free_action_group( sActionGroup *sgroup )
{
	g_object_unref( sgroup->menu );
	g_object_unref( sgroup->action_group );

	g_free( sgroup );
}
