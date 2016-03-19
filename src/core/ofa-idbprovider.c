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

#include <glib/gi18n.h>

#include "my/my-utils.h"

#include "api/ofa-idbprovider.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-plugin.h"

#define IDBPROVIDER_LAST_VERSION        1

static guint st_initializations         = 0;	/* interface initialization count */

static GType           register_type( void );
static void            interface_base_init( ofaIDBProviderInterface *klass );
static void            interface_base_finalize( ofaIDBProviderInterface *klass );
static ofaIDBProvider *get_provider_by_name( GList *modules, const gchar *name );
static GList          *get_providers_list( GList *modules );

/**
 * ofa_idbprovider_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_idbprovider_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_idbprovider_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_idbprovider_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIDBProviderInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIDBProvider", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIDBProviderInterface *klass )
{
	static const gchar *thisfn = "ofa_idbprovider_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIDBProviderInterface *klass )
{
	static const gchar *thisfn = "ofa_idbprovider_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_idbprovider_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_idbprovider_get_interface_last_version( void )
{
	return( IDBPROVIDER_LAST_VERSION );
}

/**
 * ofa_idbprovider_get_interface_version:
 * @instance: this #ofaIDBProvider instance.
 *
 * Returns: the version number of this interface the plugin implements.
 */
guint
ofa_idbprovider_get_interface_version( const ofaIDBProvider *instance )
{
	static const gchar *thisfn = "ofa_idbprovider_get_interface_version";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IDBPROVIDER( instance ), 0 );

	if( OFA_IDBPROVIDER_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_IDBPROVIDER_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: ofaIDBProvider instance %p does not provide 'get_interface_version()' method",
			thisfn, ( void * ) instance );
	return( 1 );
}

/**
 * ofa_idbprovider_new_meta:
 * @instance: this #ofaIDBProvider instance.
 *
 * Returns: a newly allocated #ofaIDBMeta object, which should be
 * g_object_unref() by the caller.
 */
ofaIDBMeta *
ofa_idbprovider_new_meta( const ofaIDBProvider *instance )
{
	static const gchar *thisfn = "ofa_idbprovider_new_meta";
	ofaIDBMeta *meta;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IDBPROVIDER( instance ), NULL );

	if( OFA_IDBPROVIDER_GET_INTERFACE( instance )->new_meta ){
		meta = OFA_IDBPROVIDER_GET_INTERFACE( instance )->new_meta();
		ofa_idbmeta_set_provider( meta, instance );
		return( meta );
	}

	g_info( "%s: ofaIDBProvider instance %p does not provide 'new_meta()' method",
			thisfn, ( void * ) instance );
	return( NULL );
}

/**
 * ofa_idbprovider_new_connect:
 * @instance: this #ofaIDBProvider instance.
 *
 * Returns: a newly allocated #ofaIDBConnect object, which should be
 * g_object_unref() by the caller.
 */
ofaIDBConnect *
ofa_idbprovider_new_connect( const ofaIDBProvider *instance )
{
	static const gchar *thisfn = "ofa_idbprovider_new_connect";
	ofaIDBConnect *connect;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IDBPROVIDER( instance ), NULL );

	if( OFA_IDBPROVIDER_GET_INTERFACE( instance )->new_connect ){
		connect = OFA_IDBPROVIDER_GET_INTERFACE( instance )->new_connect();
		ofa_idbconnect_set_provider( connect, instance );
		return( connect );
	}

	g_info( "%s: ofaIDBProvider instance %p does not provide 'new_connect()' method",
			thisfn, ( void * ) instance );
	return( NULL );
}

/**
 * ofa_idbprovider_new_editor:
 * @instance: this #ofaIDBProvider instance.
 * @editable: whether the informations in the container have to be
 *  editable.
 *
 * Returns: a composite GTK container widget intended to hold the
 * informations needed to fully identify the DBMS server which manages
 * a dossier and a financial period.
 * Depending of @editable flag, the DB provider must expect these
 * informations to be edited, or may propose a display-only user
 * interface.
 *
 * The returned container will be added to a GtkWindow and must be
 * destroyable with this same window. In other words, the DBMS provider
 * should not keep any reference on this container.
 */
ofaIDBEditor *
ofa_idbprovider_new_editor( const ofaIDBProvider *instance, gboolean editable )
{
	static const gchar *thisfn = "ofa_idbprovider_new_editor";
	ofaIDBEditor *editor;

	g_debug( "%s: instance=%p, editable=%s",
			thisfn,( void * ) instance, editable ? "True":"False" );

	g_return_val_if_fail( instance && OFA_IS_IDBPROVIDER( instance ), NULL );

	if( OFA_IDBPROVIDER_GET_INTERFACE( instance )->new_editor ){
		editor = OFA_IDBPROVIDER_GET_INTERFACE( instance )->new_editor( editable );
		ofa_idbeditor_set_provider( editor, instance );
		return( editor );
	}

	g_info( "%s: ofaIDBProvider instance %p does not provide 'get_editor()' method",
			thisfn, ( void * ) instance );
	return( NULL );
}

/**
 * ofa_idbprovider_get_name:
 * @instance: this #ofaIDBProvider instance.
 *
 * Returns: the name of this @instance.
 *
 * The returned string is owned by the #ofaIDBProvider instance, and
 * should not be released by the caller.
 */
const gchar *
ofa_idbprovider_get_name( const ofaIDBProvider *instance )
{
	static const gchar *thisfn = "ofa_idbprovider_get_name";

	g_return_val_if_fail( instance && OFA_IS_IDBPROVIDER( instance ), NULL );

	if( OFA_IDBPROVIDER_GET_INTERFACE( instance )->get_provider_name ){
		return( OFA_IDBPROVIDER_GET_INTERFACE( instance )->get_provider_name( instance ));
	}

	g_info( "%s: ofaIDBProvider instance %p does not provide 'get_provider_name()' method",
			thisfn, ( void * ) instance );
	return( NULL );
}

/**
 * ofa_idbprovider_get_instance_by_name:
 * @provider_name: the name of the provider as published in the
 *  settings.
 *
 * Returns: a new reference to the #ofaIDBProvider module instance
 * which publishes this name. This new reference should be
 * g_object_unref() by the caller.
 */
ofaIDBProvider *
ofa_idbprovider_get_instance_by_name( const gchar *provider_name )
{
	static const gchar *thisfn = "ofa_idbprovider_get_instance_by_name";
	GList *modules;
	ofaIDBProvider *module;

	g_debug( "%s: provider_name=%s", thisfn, provider_name );

	modules = ofa_plugin_get_extensions_for_type( OFA_TYPE_IDBPROVIDER );
	module = get_provider_by_name( modules, provider_name );
	ofa_plugin_free_extensions( modules );

	return( module );
}

static ofaIDBProvider *
get_provider_by_name( GList *modules, const gchar *name )
{
	GList *im;
	ofaIDBProvider *instance;
	const gchar *provider_name;

	instance = NULL;

	for( im=modules ; im ; im=im->next ){
		provider_name = ofa_idbprovider_get_name( OFA_IDBPROVIDER( im->data ));
		if( !g_utf8_collate( provider_name, name )){
			instance = g_object_ref( OFA_IDBPROVIDER( im->data ));
			break;
		}
	}

	return( instance );
}

/**
 * ofa_idbprovider_get_list:
 *
 * Returns: the #GList of #ofaIDBProvider names, as a newly allocated
 *  list of newly allocated strings. The returned list should be
 *  #ofa_idbprovider_free_list() by the caller.
 */
GList *
ofa_idbprovider_get_list( void )
{
	static const gchar *thisfn = "ofa_idbprovider_get_list";
	GList *modules, *names;

	g_debug( "%s:", thisfn );

	modules = ofa_plugin_get_extensions_for_type( OFA_TYPE_IDBPROVIDER );
	names = get_providers_list( modules );
	ofa_plugin_free_extensions( modules );

	return( names );
}

static GList *
get_providers_list( GList *modules )
{
	GList *im;
	GList *names;
	const gchar *provider_name;

	names = NULL;

	for( im=modules ; im ; im=im->next ){
		provider_name = ofa_idbprovider_get_name( OFA_IDBPROVIDER( im->data ));
		names = g_list_prepend( names, g_strdup( provider_name ));
	}

	return( names );
}
