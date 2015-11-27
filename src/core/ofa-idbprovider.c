/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#include "api/my-utils.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-ifile-meta.h"
#include "api/ofa-plugin.h"

#define IDBPROVIDER_LAST_VERSION        1

static guint st_initializations         = 0;	/* interface initialization count */

static GType           register_type( void );
static void            interface_base_init( ofaIDBProviderInterface *klass );
static void            interface_base_finalize( ofaIDBProviderInterface *klass );
static ofaIDBProvider *get_provider_by_name( GList *modules, const gchar *name );
static const gchar    *get_provider_name( const ofaIDBProvider *instance );

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
	g_return_val_if_fail( instance && OFA_IS_IDBPROVIDER( instance ), 0 );

	if( OFA_IDBPROVIDER_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_IDBPROVIDER_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	return( 1 );
}

/**
 * ofa_idbprovider_get_dossier_meta:
 * @instance: this #ofaIDBProvider instance.
 * @dossier_name: the name of the dossier.
 * @settings: the #mySettings instance in which the application has
 *  chosen to store its dossiers meta datas
 * @group: the group name identifying the desired dossier.
 *
 * Returns: an #ofaIFileMeta object which should be g_object_unref() by
 * the caller.
 *
 * The interface keeps a reference on @settings object and a copy of
 * @group string, so that these same informations will be available
 * later. It takes care of releasing the reference / freeing the copy
 * on #ofaIFileMeta finalization.
 *
 * As a consequence, the #ofaIDBProvider instance does not need to
 * store itself a copy of these informations.
 */
ofaIFileMeta *
ofa_idbprovider_get_dossier_meta( const ofaIDBProvider *instance, const gchar *dossier_name, mySettings *settings, const gchar *group )
{
	ofaIFileMeta *meta;

	g_return_val_if_fail( instance && OFA_IS_IDBPROVIDER( instance ), NULL );
	g_return_val_if_fail( my_strlen( dossier_name ), NULL );
	g_return_val_if_fail( settings && MY_IS_SETTINGS( settings ), NULL );
	g_return_val_if_fail( my_strlen( group ), NULL );

	if( OFA_IDBPROVIDER_GET_INTERFACE( instance )->get_dossier_meta ){
		meta = OFA_IDBPROVIDER_GET_INTERFACE( instance )->get_dossier_meta( instance, dossier_name, settings, group );
		ofa_ifile_meta_set_provider_instance( meta, instance );
		ofa_ifile_meta_set_dossier_name( meta, dossier_name );
		ofa_ifile_meta_set_settings( meta, settings );
		ofa_ifile_meta_set_group_name( meta, group );
		return( meta );
	}

	return( NULL );
}

/**
 * ofa_idbprovider_connect_dossier:
 * @instance: this #ofaIDBProvider instance.
 * @meta: the #ofaIFileMeta instance which manages the dossier.
 * @period: the #ofaIFilePeriod which specifies the exercice.
 * @account: the user account.
 * @password: the user password.
 * @msg: a placeholder for error message.
 *
 * Returns: a handle to the opened connection if user credentials are
 * valid, or %NULL.

 * When the connection is successfully opened, the interface keeps a
 * reference on @meta and @period objects, and user account (user
 * password is not kept for security reasons).
 *
 * The implementation is expected to take care of gracefully closing
 * the DB connection when the object is released.
 */
ofaIDBConnect *
ofa_idbprovider_connect_dossier( const ofaIDBProvider *instance,
		ofaIFileMeta *meta, ofaIFilePeriod *period, const gchar *account, const gchar *password, gchar **msg )
{
	ofaIDBConnect *connect;

	g_return_val_if_fail( instance && OFA_IS_IDBPROVIDER( instance ), NULL );

	if( OFA_IDBPROVIDER_GET_INTERFACE( instance )->connect_dossier ){
		connect = OFA_IDBPROVIDER_GET_INTERFACE( instance )->connect_dossier( instance, meta, period, account, password, msg );
		if( connect ){
			ofa_idbconnect_set_meta( connect, meta );
			ofa_idbconnect_set_period( connect, period );
			ofa_idbconnect_set_account( connect, account );
		}
		return( connect );
	}

	if( msg ){
		*msg = g_strdup( _( "The IDBProvider does not provide 'connect_dossier' interface" ));
	}
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
	GList *modules;
	ofaIDBProvider *module;

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
		provider_name = get_provider_name( OFA_IDBPROVIDER( im->data ));
		if( !g_utf8_collate( provider_name, name )){
			instance = g_object_ref( OFA_IDBPROVIDER( im->data ));
			break;
		}
	}

	return( instance );
}

static const gchar *
get_provider_name( const ofaIDBProvider *instance )
{
	if( OFA_IDBPROVIDER_GET_INTERFACE( instance )->get_provider_name ){
		return( OFA_IDBPROVIDER_GET_INTERFACE( instance )->get_provider_name( instance ));
	}

	return( NULL );
}
