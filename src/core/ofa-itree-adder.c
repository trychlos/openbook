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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/ofa-extender-collection.h"
#include "api/ofa-hub.h"
#include "api/ofa-itree-adder.h"

#define ITREE_ADDER_LAST_VERSION          1

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaITreeAdderInterface *klass );
static void  interface_base_finalize( ofaITreeAdderInterface *klass );
static void  itree_adder_get_types( ofaITreeAdder *instance, ofaIStore *store, TreeAdderTypeCb cb, void *cb_data );
static void  itree_adder_set_values( ofaITreeAdder *instance, ofaIStore *store, ofaHub *hub, GtkTreeIter *iter, void *object );

/**
 * ofa_itree_adder_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_itree_adder_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_itree_adder_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_itree_adder_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaITreeAdderInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaITreeAdder", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaITreeAdderInterface *klass )
{
	static const gchar *thisfn = "ofa_itree_adder_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaITreeAdderInterface *klass )
{
	static const gchar *thisfn = "ofa_itree_adder_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_itree_adder_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_itree_adder_get_interface_last_version( void )
{
	return( ITREE_ADDER_LAST_VERSION );
}

/**
 * ofa_itree_adder_get_types:
 * @hub: the #ofaHub object of the application.
 * @store: the target #ofaIStore.
 * @cb: the callback function.
 * @cb_data: user data for the callback.
 *
 * Defines GType's to be added to a store.
 */
void
ofa_itree_adder_get_types( ofaHub *hub, ofaIStore *store, TreeAdderTypeCb cb, void *cb_data )
{
	ofaExtenderCollection *collection;
	GList *modules, *it;

	g_return_if_fail( hub && OFA_IS_HUB( hub ));
	g_return_if_fail( store && OFA_IS_ISTORE( store ));

	collection = ofa_hub_get_extender_collection( hub );
	modules = ofa_extender_collection_get_for_type( collection, OFA_TYPE_ITREE_ADDER );
	for( it=modules ; it ; it=it->next ){
		itree_adder_get_types( OFA_ITREE_ADDER( it->data ), store, cb, cb_data );
	}
	ofa_extender_collection_free_types( modules );
}

/**
 * ofa_itree_adder_set_values:
 * @hub: the #ofaHub object of the application.
 * @store: the target #ofaIStore.
 * @iter: the current #GtkTreeIter.
 * @object: the current object.
 *
 * Let an implementation set its values in the current row.
 */
void
ofa_itree_adder_set_values( ofaHub *hub, ofaIStore *store, GtkTreeIter *iter, void *object )
{
	ofaExtenderCollection *collection;
	GList *modules, *it;

	g_return_if_fail( hub && OFA_IS_HUB( hub ));
	g_return_if_fail( store && OFA_IS_ISTORE( store ));

	collection = ofa_hub_get_extender_collection( hub );
	modules = ofa_extender_collection_get_for_type( collection, OFA_TYPE_ITREE_ADDER );
	for( it=modules ; it ; it=it->next ){
		itree_adder_set_values( OFA_ITREE_ADDER( it->data ), store, hub, iter, object );
	}
	ofa_extender_collection_free_types( modules );
}

/**
 * ofa_itree_adder_get_interface_version:
 * @type: the implementation's GType.
 *
 * Returns: the version number of this interface which is managed by
 * the @type implementation.
 *
 * Defaults to 1.
 *
 * Since: version 1.
 */
guint
ofa_itree_adder_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_ITREE_ADDER );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaITreeAdderInterface * ) iface )->get_interface_version ){
		version = (( ofaITreeAdderInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaITreeAdder::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/*
 * itree_adder_get_types:
 * @instance: the #ofaITreeAdder instance.
 * @cb: the callback function.
 * @cb_data: user data for the callback.
 *
 * Defines GType's to be added to a store.
 */
static void
itree_adder_get_types( ofaITreeAdder *instance, ofaIStore *store, TreeAdderTypeCb cb, void *cb_data )
{
	static const gchar *thisfn = "ofa_itree_adder_get_types";

	if( OFA_ITREE_ADDER_GET_INTERFACE( instance )->get_types ){
		OFA_ITREE_ADDER_GET_INTERFACE( instance )->get_types( instance, store, cb, cb_data );
		return;
	}

	g_info( "%s: ofaITreeAdder's %s implementation does not provide 'get_types()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}

/*
 * itree_adder_set_values:
 * @instance: the #ofaITreeAdder instance.
 * @iter:
 * @object:
 *
 * Set values in a row.
 */
static void
itree_adder_set_values( ofaITreeAdder *instance, ofaIStore *store, ofaHub *hub, GtkTreeIter *iter, void *object )
{
	static const gchar *thisfn = "ofa_itree_adder_set_values";

	if( OFA_ITREE_ADDER_GET_INTERFACE( instance )->set_values ){
		OFA_ITREE_ADDER_GET_INTERFACE( instance )->set_values( instance, store, hub, iter, object );
		return;
	}

	g_info( "%s: ofaITreeAdder's %s implementation does not provide 'set_values()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}
