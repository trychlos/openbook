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

#include <gio/gio.h>
#include <glib.h>

#include "my/my-accel-group.h"
#include "my/my-utils.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;

	/* runtime data
	 */
}
	myAccelGroupPrivate;

/* A data structure attached to each accelerator
 */
typedef struct {
	myIActionMap *map;
	gchar         *action;
}
	sAccel;

static void     setup_accels_rec( myAccelGroup *self, myIActionMap *map, GMenuModel *model );
static void     install_accel( myAccelGroup *self, myIActionMap *map, GVariant *action, GVariant *accel );
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
 * my_accel_group_setup_accels:
 * @group: this #myAccelGroup instance.
 * @map: the myIActionMap which holds the actions.
 *
 * Setup the defined accelerators in the @group accelerator group.
 */
void
my_accel_group_setup_accels( myAccelGroup *group, myIActionMap *map )
{
	GMenuModel *model;

	g_return_if_fail( group && MY_IS_ACCEL_GROUP( group ));
	g_return_if_fail( map && MY_IS_IACTION_MAP( map ));

	model = my_iaction_map_get_menu_model( map );
	g_return_if_fail( model && G_IS_MENU_MODEL( model ));

	setup_accels_rec( group, map, model );
}

static void
setup_accels_rec( myAccelGroup *self, myIActionMap *map, GMenuModel *model )
{
	static const gchar *thisfn = "my_accel_group_setup_accels_rec";
	GMenuLinkIter *lter;
	GMenuAttributeIter *ater;
	gint i;
	const gchar *aname, *lname;
	GMenuModel *lv;
	GVariant *vaccel, *vaction;

	g_debug( "%s: self=%p, map=%p, model=%p",
			thisfn, ( void * ) self, ( void * ) map, ( void * ) model );

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
				install_accel( self, map, vaction, vaccel );
				g_variant_unref( vaction );
			}
			g_variant_unref( vaccel );
		}
		g_object_unref( ater );
		lter = g_menu_model_iterate_item_links( model, i );
		while( g_menu_link_iter_get_next( lter, &lname, &lv )){
			/*g_debug( "ofa_main_window_extract_accels: model=%p: found link at i=%d to %p", ( void * ) model, i, ( void * ) lv );*/
			setup_accels_rec( self, map, lv );
			g_object_unref( lv );
		}
		g_object_unref( lter );
	}
}

static void
install_accel( myAccelGroup *self, myIActionMap *map, GVariant *action, GVariant *accel )
{
	static const gchar *thisfn = "my_accel_group_install_accel";
	guint accel_key;
	GdkModifierType accel_mods;
	const gchar *action_name, *accel_str;
	GClosure *closure;
	sAccel *accel_data;

	action_name = g_variant_get_string( action, NULL );
	g_return_if_fail( my_strlen( action_name ));

	accel_data = g_new0( sAccel, 1 );
	accel_data->map = map;
	accel_data->action = g_strdup( action_name );

	accel_str = g_variant_get_string( accel, NULL );
	g_return_if_fail( my_strlen( accel_str ));

	g_debug( "%s: map=%p, installing accel '%s' for '%s' action",
			thisfn, ( void * ) map, accel_str, action_name );

	gtk_accelerator_parse( accel_str, &accel_key, &accel_mods );

	closure = g_cclosure_new(
			G_CALLBACK( on_accel_activated ),
			accel_data, ( GClosureNotify ) on_accel_data_finalized );
	gtk_accel_group_connect( GTK_ACCEL_GROUP( self ), accel_key, accel_mods, GTK_ACCEL_VISIBLE, closure);
	g_closure_unref( closure );
}

static gboolean
on_accel_activated( myAccelGroup *group, GObject *acceleratable, guint keyval, GdkModifierType modifier, sAccel *accel_data )
{
	static const gchar *thisfn = "my_accel_group_on_accel_activated";
	GAction *action;
	const GVariantType *type;

	g_debug( "%s:  group=%p, acceleratable=%p, keyval=%u, modified=%d, accel_data=%p, action=%s",
			thisfn, ( void * ) group, ( void * ) acceleratable, keyval, modifier, ( void * ) accel_data, accel_data->action );

	action = my_iaction_map_lookup_action( accel_data->map, accel_data->action );
	if( !action ){
		return( FALSE );
	}

	g_debug( "%s: action=%p (%s)", thisfn, ( void * ) action, G_OBJECT_TYPE_NAME( action ));
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
	static const gchar *thisfn = "my_accel_group_menubar_on_accel_data_finalized";

	g_debug( "%s: accel_data=%p, action=%s, closure=%p",
			thisfn, ( void * ) accel_data, accel_data->action, ( void * ) closure );

	g_free( accel_data->action );
	g_free( accel_data );
}
