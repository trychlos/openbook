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

#include "my/my-utils.h"

#include "api/ofa-iactioner.h"

#define IACTIONER_LAST_VERSION            1

/* a data structure attached to the instance
 */
#define IACTIONER_DATA                   "ofa-iactioner-data"

/* a pointer to the ofaIActionable is attached to each action
 */
#define IACTIONER_IACTIONABLE            "ofa-iactioner-iactionable"

typedef struct {
	GList   *actionables;
	gboolean is_proxying;
	GList   *user_datas;
}
	sIActioner;

/* a common prototype for g_action_activate() and g_action_change_state()
 */
typedef void IActionerFn ( GAction *, GVariant * );

/* a data structure to be passed as signal handler user data
 */
typedef struct {
	ofaIActioner *instance;
	IActionerFn   *cb;
}
	sUserData;

/* a data structure to be passed as callback argument
 */
typedef struct {
	ofaIActioner  *instance;
	IActionerFn    *cb;
	GSimpleAction *action;
	GVariant      *parameter;
}
	sCBArg;

static guint st_initializations         = 0;	/* interface initialization count */

static GType       register_type( void );
static void        interface_base_init( ofaIActionerInterface *klass );
static void        interface_base_finalize( ofaIActionerInterface *klass );
static void        register_actionable_cb( ofaIActionable *actionable, const gchar *name, GActionGroup *group, ofaIActioner *instance );
static void        on_action_signaled( GSimpleAction *action, GVariant *parameter, sUserData *user_data );
static void        action_signaled_cb( ofaIActionable *actionable, const gchar *name, GActionGroup *group, sCBArg *sarg );
static sIActioner *get_instance_data( ofaIActioner *instance );
static void        on_instance_finalized( sIActioner *sdata, GObject *instance );

/**
 * ofa_iactioner_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iactioner_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iactioner_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iactioner_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIActionerInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIActioner", &info, 0 );

	g_type_interface_add_prerequisite( type, GTK_TYPE_WIDGET );

	return( type );
}

static void
interface_base_init( ofaIActionerInterface *klass )
{
	static const gchar *thisfn = "ofa_iactioner_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIActionerInterface *klass )
{
	static const gchar *thisfn = "ofa_iactioner_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iactioner_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iactioner_get_interface_last_version( void )
{
	return( IACTIONER_LAST_VERSION );
}

/**
 * ofa_iactioner_get_interface_version:
 * @instance: this #ofaIActioner instance.
 *
 * Returns: the version number of this interface implemented by the
 * @instance.
 *
 * Defaults to 1.
 */
guint
ofa_iactioner_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IACTIONER );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIActionerInterface * ) iface )->get_interface_version ){
		version = (( ofaIActionerInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIActioner::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_iactioner_register_actionable:
 * @instance: this #ofaIActioner instance.
 * @actionable: an #ofaIActionable to be proxyied and kept sync.
 *
 * Records a new actionable.
 *
 * After an @actionable has been registered, all its action messages are
 * centralized and proxyied to other registered #ofaIActionable instances,
 * after having changed the action group name.
 */
void
ofa_iactioner_register_actionable( ofaIActioner *instance, ofaIActionable *actionable )
{
	static const gchar *thisfn = "ofa_iactioner_register_actionable";
	sIActioner *sdata;
	GList *it;

	g_debug( "%s: instance=%p, actionable=%p",
			thisfn, ( void * ) instance, ( void * ) actionable );

	g_return_if_fail( instance && OFA_IS_IACTIONER( instance ));
	g_return_if_fail( actionable && OFA_IS_IACTIONABLE( actionable ));

	sdata = get_instance_data( instance );

	/* search for already registered; do not do anything if found */
	for( it=sdata->actionables ; it ; it=it->next ){
		if( it->data == ( void * ) actionable ){
			return;
		}
	}

	/* register and connect to all actions */
	sdata->actionables = g_list_prepend( sdata->actionables, actionable );
	ofa_iactionable_enum_action_groups( actionable, ( ofaIActionableEnumCb ) register_actionable_cb, instance );
}

static void
register_actionable_cb( ofaIActionable *actionable, const gchar *name, GActionGroup *group, ofaIActioner *instance )
{
	sIActioner *sdata;
	gchar **actions_list, **it;
	GAction *action;
	const GVariantType *type;
	sUserData *user_data;

	sdata = get_instance_data( instance );

	/* returns actions_list non-prefixed names */
	actions_list = g_action_group_list_actions( group );
	it = actions_list;

	while( *it ){
		action = g_action_map_lookup_action( G_ACTION_MAP( group ), *it );
		g_return_if_fail( action && G_IS_ACTION( action ));

		g_object_set_data( G_OBJECT( action ), IACTIONER_IACTIONABLE, actionable );
		type = g_action_group_get_action_state_type( group, *it );

		user_data = g_new0( sUserData, 1 );
		user_data->instance = instance;
		sdata->user_datas = g_list_prepend( sdata->user_datas, user_data );

		if( type && g_variant_type_equal( type, G_VARIANT_TYPE_BOOLEAN )){
			user_data->cb = ( IActionerFn * ) g_action_change_state;
			/* g_debug( "user_data->cb=%p", ( void * ) user_data->cb ); */
			g_signal_connect( action, "change-state", G_CALLBACK( on_action_signaled ), user_data );

		} else {
			user_data->cb = ( IActionerFn * ) g_action_activate;
			/* g_debug( "user_data->cb=%p", ( void * ) user_data->cb ); */
			g_signal_connect( action, "activate", G_CALLBACK( on_action_signaled ), user_data );
		}

		it++;
	}

	g_strfreev( actions_list );
}

static void
on_action_signaled( GSimpleAction *action, GVariant *parameter, sUserData *user_data )
{
	static const gchar *thisfn = "ofa_iactioner_on_action_signaled";
	sIActioner *sdata;
	ofaIActionable *actionable;
	GList *it;
	sCBArg sarg;

	sdata = get_instance_data( user_data->instance );

	/* prevent from weird loop */
	if( sdata->is_proxying ){
		return;
	}

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, ( void * ) action, ( void * ) parameter, ( void * ) user_data );

	sdata->is_proxying = TRUE;

	actionable = ( ofaIActionable * ) g_object_get_data( G_OBJECT( action ), IACTIONER_IACTIONABLE );

	/* proxy to all registered actionables, but this one */
	sarg.instance = user_data->instance;
	sarg.action = action;
	sarg.parameter = parameter;
	sarg.cb = user_data->cb;

	for( it=sdata->actionables ; it ; it=it->next ){
		if( it->data != ( void * ) actionable ){
			ofa_iactionable_enum_action_groups(
					OFA_IACTIONABLE( it->data ), ( ofaIActionableEnumCb ) action_signaled_cb, &sarg );
		}
	}

	sdata->is_proxying = FALSE;
}

/*
 * an action from another IActionable has been activated
 * proxyies the activation to same action of other groups
 */
static void
action_signaled_cb( ofaIActionable *actionable, const gchar *name, GActionGroup *group, sCBArg *sarg )
{
	gchar **actions_list, **it;
	const gchar *cname;
	GAction *action;

	cname = g_action_get_name( G_ACTION( sarg->action ));

	actions_list = g_action_group_list_actions( group );
	it = actions_list;
	/* both g_action_get_name() and list_actions() returns short
	 * (non-prefixed) names */
	while( *it ){
		if( !my_collate( cname, *it )){
			action = g_action_map_lookup_action( G_ACTION_MAP( group ), cname );
			g_return_if_fail( action && G_IS_ACTION( action ));
			/*
			g_debug( "sarg->cb=%p, action=%p (%s), sarg->parameter=%p",
					( void * ) sarg->cb, ( void * ) action, G_OBJECT_TYPE_NAME( action ),
					( void * ) sarg->parameter );
					*/
			( *sarg->cb )( action, sarg->parameter );
		}
		it++;
	}

	g_strfreev( actions_list );
}

/*
 * returns the sIActioner data structure attached to the instance
 */
static sIActioner *
get_instance_data( ofaIActioner *instance )
{
	sIActioner *sdata;

	sdata = ( sIActioner * ) g_object_get_data( G_OBJECT( instance ), IACTIONER_DATA );

	if( !sdata ){
		sdata = g_new0( sIActioner, 1 );
		sdata->actionables = NULL;
		sdata->is_proxying = FALSE;

		g_object_set_data( G_OBJECT( instance ), IACTIONER_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );
	}

	return( sdata );
}

static void
on_instance_finalized( sIActioner *sdata, GObject *instance )
{
	static const gchar *thisfn = "ofa_iactioner_on_instance_finalized";

	g_debug( "%s: sdata=%p, instance=%p", thisfn, ( void * ) sdata, ( void * ) instance );

	g_list_free( sdata->actionables );
	g_list_free_full( sdata->user_datas, ( GDestroyNotify ) g_free );

	g_free( sdata );
}
