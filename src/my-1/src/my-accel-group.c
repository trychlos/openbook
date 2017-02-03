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

#include <gio/gio.h>
#include <glib.h>

#include "my/my-accel-group.h"
#include "my/my-iaction-map.h"
#include "my/my-utils.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;

	/* runtime data
	 */
}
	myAccelGroupPrivate;

/* The data structure passed to the accelerator closure
 * - just for convenience, identifies the accelerator which was used
 * - identified the action: its scope (app/win) and its name
 */
typedef struct {
	gchar *keystr;
	gchar *scope;
	gchar *action;
}
	sAccel;

static void     setup_accels_rec( myAccelGroup *self, GMenuModel *model );
static void     install_accel( myAccelGroup *self, GVariant *action, GVariant *accel );
static void     split_detailed_action_name( const gchar *detailed_name, gchar **scope, gchar **action );
static gboolean on_accel_activated( myAccelGroup *group, GObject *acceleratable, guint keyval, GdkModifierType modifier, sAccel *accel_data );
static void     on_accel_data_finalized( sAccel *accel_data, GClosure *closure );

G_DEFINE_TYPE_EXTENDED( myAccelGroup, my_accel_group, GTK_TYPE_ACCEL_GROUP, 0,
		G_ADD_PRIVATE( myAccelGroup ))

static void
accel_group_finalize( GObject *object )
{
	static const gchar *thisfn = "my_accel_group_finalize";

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_return_if_fail( object && MY_IS_ACCEL_GROUP( object ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_accel_group_parent_class )->finalize( object );
}

static void
accel_group_dispose( GObject *object )
{
	myAccelGroupPrivate *priv;

	g_return_if_fail( object && MY_IS_ACCEL_GROUP( object ));

	priv = my_accel_group_get_instance_private( MY_ACCEL_GROUP( object ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		gtk_accel_group_disconnect( GTK_ACCEL_GROUP( object ), NULL );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_accel_group_parent_class )->dispose( object );
}

static void
my_accel_group_init( myAccelGroup *self )
{
	static const gchar *thisfn = "my_accel_group_init";
	myAccelGroupPrivate *priv;

	g_return_if_fail( self && MY_IS_ACCEL_GROUP( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = my_accel_group_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
my_accel_group_class_init( myAccelGroupClass *klass )
{
	static const gchar *thisfn = "my_accel_group_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = accel_group_dispose;
	G_OBJECT_CLASS( klass )->finalize = accel_group_finalize;
}

/**
 * my_accel_group_new:
 *
 * Returns: a new #myAccelGroup object which should be g_object_unref()
 * by the caller.
 */
myAccelGroup *
my_accel_group_new( void )
{
	myAccelGroup *group;

	group = g_object_new( MY_TYPE_ACCEL_GROUP, NULL );

	return( group );
}

/**
 * my_accel_group_setup_accels_from_menu:
 * @group: this #myAccelGroup instance.
 * @map: the #GActionMap which holds the menu.
 *
 * Parse the @menu, extracting actions which expose an accelerator,
 * and define these accelerators for the @map group.
 */
void
my_accel_group_setup_accels_from_menu( myAccelGroup *group, GMenuModel *menu )
{
	static const gchar *thisfn = "my_accel_group_setup_accels_from_menu";
	myAccelGroupPrivate *priv;

	g_debug( "%s: group=%p, menu=%p",
			thisfn, ( void * ) group, ( void * ) menu );

	g_return_if_fail( group && MY_IS_ACCEL_GROUP( group ));
	g_return_if_fail( menu && G_IS_MENU_MODEL( menu ));

	priv = my_accel_group_get_instance_private( group );

	g_return_if_fail( !priv->dispose_has_run );

	setup_accels_rec( group, menu );
}

static void
setup_accels_rec( myAccelGroup *self, GMenuModel *model )
{
	//static const gchar *thisfn = "my_accel_group_setup_accels_rec";
	GMenuLinkIter *lter;
	GMenuAttributeIter *ater;
	gint i;
	const gchar *aname, *lname;
	GMenuModel *lv;
	GVariant *vaccel, *vaction;

	/*
	g_debug( "%s: self=%p, map=%p, model=%p",
			thisfn, ( void * ) self, ( void * ) map, ( void * ) model );
			*/

	/* iterate through items and attributes to find accels
	 */
	for( i=0 ; i<g_menu_model_get_n_items( model ) ; ++i ){
		ater = g_menu_model_iterate_item_attributes( model, i );
		//g_debug( "ater=%p", ater );
		while( g_menu_attribute_iter_get_next( ater, &aname, &vaccel )){
			//g_debug( "aname=%s, value=%s", aname, g_variant_get_string( vaccel, NULL ));
			if( !g_strcmp0( aname, "accel" )){
				vaction = g_menu_model_get_item_attribute_value( model, i, "action", NULL );
				//g_debug( "%s: model=%p: found accel at i=%d: %s for '%s' action",
				//		thisfn, ( void * ) model, i, g_variant_get_string( vaccel, NULL ), g_variant_get_string( vaction, NULL ));
				install_accel( self, vaction, vaccel );
				g_variant_unref( vaction );
			}
			g_variant_unref( vaccel );
		}
		g_object_unref( ater );
		lter = g_menu_model_iterate_item_links( model, i );
		while( g_menu_link_iter_get_next( lter, &lname, &lv )){
			/*g_debug( "ofa_main_window_extract_accels: model=%p: found link at i=%d to %p", ( void * ) model, i, ( void * ) lv );*/
			setup_accels_rec( self, lv );
			g_object_unref( lv );
		}
		g_object_unref( lter );
	}
}

static void
install_accel( myAccelGroup *self, GVariant *action, GVariant *accel )
{
	static const gchar *thisfn = "my_accel_group_install_accel";
	guint accel_key;
	GdkModifierType accel_mods;
	const gchar *action_name, *accel_str;
	GClosure *closure;
	sAccel *accel_data;

	/* get the detailed name */
	action_name = g_variant_get_string( action, NULL );
	g_return_if_fail( my_strlen( action_name ));

	accel_str = g_variant_get_string( accel, NULL );
	g_return_if_fail( my_strlen( accel_str ));

	accel_data = g_new0( sAccel, 1 );
	accel_data->keystr = g_strdup( accel_str );
	split_detailed_action_name( action_name, &accel_data->scope, &accel_data->action );

	gtk_accelerator_parse( accel_str, &accel_key, &accel_mods );

	g_debug( "%s: self=%p, installing accel '%s' for '%s' action",
			thisfn, ( void * ) self, accel_str, action_name );

	closure = g_cclosure_new(
			G_CALLBACK( on_accel_activated ),
			accel_data, ( GClosureNotify ) on_accel_data_finalized );

	gtk_accel_group_connect( GTK_ACCEL_GROUP( self ), accel_key, accel_mods, GTK_ACCEL_VISIBLE, closure );

	g_closure_unref( closure );
}

static void
split_detailed_action_name( const gchar *detailed_name, gchar **scope, gchar **action )
{
	gchar **tokens, **it;

	if( scope ){
		*scope = NULL;
	}
	if( action ){
		*action = NULL;
	}
	if( my_strlen( detailed_name )){
		tokens = g_strsplit( detailed_name, ".", -1 );
		it = tokens;
		if( *it ){
			if( scope ){
				*scope = g_strdup( *it );
			}
			it++;
			if( *it ){
				if( action ){
					*action = g_strdup( *it );
				}
			}
		}
		g_strfreev( tokens );
	}
}

static gboolean
on_accel_activated( myAccelGroup *group, GObject *acceleratable, guint keyval, GdkModifierType modifier, sAccel *accel_data )
{
	static const gchar *thisfn = "my_accel_group_on_accel_activated";
	GAction *action;
	myIActionMap *map;
	const GVariantType *type;

	g_debug( "%s: group=%p, acceleratable=%p, keyval=%u, modified=%d, accel_data=%p, action=%s.%s",
			thisfn, ( void * ) group, ( void * ) acceleratable, keyval, modifier,
			( void * ) accel_data, accel_data->scope, accel_data->action );

	map = my_iaction_map_lookup_map( accel_data->scope );
	if( !map ){
		g_debug( "%s: group=%p, acceleratable=%p, scope=%s: no myIActionMap found (not registered ?)",
				thisfn, ( void * ) group, ( void * ) acceleratable, accel_data->scope );
		return( FALSE );
	}

	action = g_action_map_lookup_action( G_ACTION_MAP( map ), accel_data->action );
	if( !action ){
		g_debug( "%s: group=%p, map=%p, action=%s: action not found",
				thisfn, ( void * ) group, ( void * ) map, accel_data->action );
		return( FALSE );
	}

	if( g_action_get_enabled( action )){
		type = g_action_get_parameter_type( action );
		if( type != NULL ){
			g_warning( "%s: unmanaged action parameter type for '%s' action", thisfn, accel_data->action );
			return( FALSE );
		}

		g_action_activate( action, NULL );
	}

	return( TRUE );
}

static void
on_accel_data_finalized( sAccel *accel_data, GClosure *closure )
{
	static const gchar *thisfn = "my_accel_group_on_accel_data_finalized";

	g_debug( "%s: accel_data=%p, scope=%s, action=%s, closure=%p",
			thisfn, ( void * ) accel_data, accel_data->scope, accel_data->action, ( void * ) closure );

	g_free( accel_data->scope );
	g_free( accel_data->action );
	g_free( accel_data );
}
