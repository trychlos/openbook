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

#include <glib/gi18n.h>

#include "my/my-iident.h"
#include "my/my-utils.h"

#include "api/ofa-extender-collection.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-editor.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-igetter.h"
#include "api/ofa-isetter.h"

#define IDBPROVIDER_LAST_VERSION        1

static guint st_initializations         = 0;	/* interface initialization count */

static GType           register_type( void );
static void            interface_base_init( ofaIDBProviderInterface *klass );
static void            interface_base_finalize( ofaIDBProviderInterface *klass );
static ofaIDBProvider *provider_get_by_name( GList *modules, const gchar *name );

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
 * ofa_idbprovider_get_all:
 * @hub: the main #ofaHub object of the application.
 *
 * Returns: the list of available #ofaIDBProvider providers.
 *
 * The returned list should be g_list_free() by the caller.
 */
GList *
ofa_idbprovider_get_all( ofaHub *hub )
{
	static const gchar *thisfn = "ofa_idbprovider_get_all";
	ofaExtenderCollection *extenders;
	GList *modules, *it, *all;

	g_debug( "%s: hub=%p", thisfn, ( void * ) hub );

	all = NULL;
	extenders = ofa_hub_get_extender_collection( hub );
	modules = ofa_extender_collection_get_for_type( extenders, OFA_TYPE_IDBPROVIDER );

	for( it=modules ; it ; it=it->next ){
		if( !MY_IS_IIDENT( it->data )){
			g_info( "%s: %s class does not implement myIIdent interface",
					thisfn, G_OBJECT_TYPE_NAME( it->data ));
		} else if( !OFA_IS_ISETTER( it->data )){
			g_info( "%s: %s class does not implement ofaISetter interface",
					thisfn, G_OBJECT_TYPE_NAME( it->data ));
		} else {
			all = g_list_prepend( all, it->data );
		}
	}

	ofa_extender_collection_free_types( modules );

	return( all );
}

/**
 * ofa_idbprovider_get_by_name:
 * @hub: the main #ofaHub object of the application.
 * @provider_name: the name of the provider as published in the
 *  settings.
 *
 * Returns: the #ofaIDBProvider module instance which publishes this
 * canonical name.
 *
 * We check here:
 * - that the #ofaIDBProvider implementation also implements the
 *   #ofaISetter interface (which is a prerequisite).
 *
 * A provider which does not satisfy all prerequisites is not returned.
 *
 * The returned reference is owned by the provider, and should not be
 * unreffed by the caller.
 */
ofaIDBProvider *
ofa_idbprovider_get_by_name( ofaHub *hub, const gchar *provider_name )
{
	static const gchar *thisfn = "ofa_idbprovider_get_by_name";
	GList *modules;
	ofaIDBProvider *provider;

	g_debug( "%s: hub=%p, provider_name=%s", thisfn, ( void * ) hub, provider_name );

	modules = ofa_idbprovider_get_all( hub );
	provider = provider_get_by_name( modules, provider_name );
	g_list_free( modules );

	return( provider );
}

static ofaIDBProvider *
provider_get_by_name( GList *modules, const gchar *name )
{
	GList *it;
	gchar *it_name;
	gint cmp;

	for( it=modules ; it ; it=it->next ){
		it_name = ofa_idbprovider_get_canon_name( OFA_IDBPROVIDER( it->data ));
		cmp = my_collate( it_name, name );
		g_free( it_name );
		if( cmp == 0 ){
			return( OFA_IDBPROVIDER( it->data ));
		}
	}

	return( NULL );
}

/**
 * ofa_idbprovider_get_interface_version:
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
ofa_idbprovider_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IDBPROVIDER );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIDBProviderInterface * ) iface )->get_interface_version ){
		version = (( ofaIDBProviderInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIDBProvider::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/*
 * ofa_idbprovider_get_canon_name:
 * @provider: this #ofaIDBProvider provider.
 *
 * Returns: the canonical name of the @provider, as a newly
 * allocated string which should be g_free() by the caller, or %NULL.
 *
 * This method relies on the #myIIdent identification interface,
 * which is expected to be implemented by the @provider class.
 */
gchar *
ofa_idbprovider_get_canon_name( const ofaIDBProvider *provider )
{
	g_return_val_if_fail( provider && OFA_IS_IDBPROVIDER( provider ), NULL );

	return( MY_IS_IIDENT( provider ) ? my_iident_get_canon_name( MY_IIDENT( provider ), NULL ) : NULL );
}

/*
 * ofa_idbprovider_get_display_name:
 * @provider: this #ofaIDBProvider provider.
 *
 * Returns: the displayable name of the @provider, as a newly
 * allocated string which should be g_free() by the caller, or %NULL.
 *
 * This method relies on the #myIIdent identification interface,
 * which is expected to be implemented by the @provider class.
 */
gchar *
ofa_idbprovider_get_display_name( const ofaIDBProvider *provider )
{
	g_return_val_if_fail( provider && OFA_IS_IDBPROVIDER( provider ), NULL );

	return( MY_IS_IIDENT( provider ) ? my_iident_get_display_name( MY_IIDENT( provider ), NULL ) : NULL );
}

/**
 * ofa_idbprovider_get_hub:
 * @provider: this #ofaIDBProvider provider.
 *
 * Returns: the #ofaHub object of the application.
 */
ofaHub *
ofa_idbprovider_get_hub( ofaIDBProvider *provider )
{
	ofaIGetter *getter;
	ofaHub *hub;

	g_return_val_if_fail( provider && OFA_IS_IDBPROVIDER( provider ), NULL );

	getter = ofa_isetter_get_getter( OFA_ISETTER( provider ));
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	hub = ofa_igetter_get_hub( getter );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	return( hub );
}

/**
 * ofa_idbprovider_new_dossier_meta:
 * @provider: this #ofaIDBProvider provider.
 * @dossier_name: the name of the dossier.
 *
 * Returns: a newly allocated #ofaIDBDossierMeta object, which should be
 * g_object_unref() by the caller.
 */
ofaIDBDossierMeta *
ofa_idbprovider_new_dossier_meta( ofaIDBProvider *provider, const gchar *dossier_name )
{
	static const gchar *thisfn = "ofa_idbprovider_new_dossier_meta";
	ofaIDBDossierMeta *meta;

	g_debug( "%s: provider=%p, dossier_name=%s",
			thisfn, ( void * ) provider, dossier_name );

	g_return_val_if_fail( provider && OFA_IS_IDBPROVIDER( provider ), NULL );
	g_return_val_if_fail( my_strlen( dossier_name ), NULL );

	if( OFA_IDBPROVIDER_GET_INTERFACE( provider )->new_dossier_meta ){
		meta = OFA_IDBPROVIDER_GET_INTERFACE( provider )->new_dossier_meta( provider );
		ofa_idbdossier_meta_set_provider( meta, provider );
		ofa_idbdossier_meta_set_dossier_name( meta, dossier_name );
		return( meta );
	}

	g_info( "%s: ofaIDBProvider's %s implementation does not provide 'new_dossier_meta()' method",
			thisfn, G_OBJECT_TYPE_NAME( provider ));
	return( NULL );
}

/**
 * ofa_idbprovider_new_dossier_editor:
 * @provider: this #ofaIDBProvider provider.
 * @settings_prefix: the prefix of a user preference key.
 * @rule: the usage of the editor.
 *
 * Returns: a composite GTK container widget intended to hold the
 * informations needed to fully identify the DBMS server which manages
 * a dossier.
 *
 * The returned container will be added to a GtkWindow and must be
 * destroyable with this same window. In other words, the DBMS provider
 * should not keep any reference on this container.
 */
ofaIDBDossierEditor *
ofa_idbprovider_new_dossier_editor( ofaIDBProvider *provider, const gchar *settings_prefix, guint rule )
{
	static const gchar *thisfn = "ofa_idbprovider_new_dossier_editor";
	ofaIDBDossierEditor *editor;

	g_debug( "%s: provider=%p, settings_prefix=%s, rule=%u",
			thisfn,( void * ) provider, settings_prefix, rule );

	g_return_val_if_fail( provider && OFA_IS_IDBPROVIDER( provider ), NULL );

	if( OFA_IDBPROVIDER_GET_INTERFACE( provider )->new_dossier_editor ){
		editor = OFA_IDBPROVIDER_GET_INTERFACE( provider )->new_dossier_editor( provider, settings_prefix, rule );
		ofa_idbdossier_editor_set_provider( editor, provider );
		return( editor );
	}

	g_info( "%s: ofaIDBProvider's %s implementation does not provide 'new_dossier_editor()' method",
			thisfn, G_OBJECT_TYPE_NAME( provider ));
	return( NULL );
}

/**
 * ofa_idbprovider_new_connect:
 * @provider: this #ofaIDBProvider provider.
 * @account: the account of the connection.
 * @password: the password of the connection.
 * @dossier_meta: the #ofaIDBDossierMeta object.
 * @exercice_meta: [allow-none]: the #ofaIDBEXerciceMeta object;
 *  if %NULL, the connection is established at server level.
 *
 * Returns: a newly defined #ofaIDBConnect object, or %NULL if the
 * connection could not be established.
 */
ofaIDBConnect *
ofa_idbprovider_new_connect( ofaIDBProvider *provider, const gchar *account, const gchar *password,
									ofaIDBDossierMeta *dossier_meta, ofaIDBExerciceMeta *exercice_meta )
{
	static const gchar *thisfn = "ofa_idbprovider_new_connect";
	ofaIDBConnect *connect;

	g_debug( "%s: provider=%p, account=%s, password=%s, dossier_meta=%p, exercice_meta=%p",
			thisfn, ( void * ) provider, account, "******", ( void * ) dossier_meta, ( void * ) exercice_meta );

	g_return_val_if_fail( provider && OFA_IS_IDBPROVIDER( provider ), NULL );
	g_return_val_if_fail( my_strlen( account ), NULL );
	g_return_val_if_fail( my_strlen( password ), NULL );
	g_return_val_if_fail( dossier_meta && OFA_IS_IDBDOSSIER_META( dossier_meta ), NULL );
	g_return_val_if_fail( !exercice_meta || OFA_IS_IDBEXERCICE_META( exercice_meta ), NULL );

	if( OFA_IDBPROVIDER_GET_INTERFACE( provider )->new_connect ){
		connect = OFA_IDBPROVIDER_GET_INTERFACE( provider )->new_connect( provider, account, password, dossier_meta, exercice_meta );
		if( connect ){
			ofa_idbconnect_set_account( connect, account );
			ofa_idbconnect_set_password( connect, password );
			ofa_idbconnect_set_dossier_meta( connect, dossier_meta );
			ofa_idbconnect_set_exercice_meta( connect, exercice_meta );
		}
		return( connect );
	}

	g_info( "%s: ofaIDBProvider's %s implementation does not provide 'new_connect()' method",
			thisfn, G_OBJECT_TYPE_NAME( provider ));
	return( NULL );
}

/**
 * ofa_idbprovider_new_editor:
 * @provider: this #ofaIDBProvider provider.
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
ofa_idbprovider_new_editor( ofaIDBProvider *provider, gboolean editable )
{
	static const gchar *thisfn = "ofa_idbprovider_new_editor";
	ofaIDBEditor *editor;

	g_debug( "%s: provider=%p, editable=%s",
			thisfn,( void * ) provider, editable ? "True":"False" );

	g_return_val_if_fail( provider && OFA_IS_IDBPROVIDER( provider ), NULL );

	if( OFA_IDBPROVIDER_GET_INTERFACE( provider )->new_editor ){
		editor = OFA_IDBPROVIDER_GET_INTERFACE( provider )->new_editor( provider, editable );
		ofa_idbeditor_set_provider( editor, provider );
		return( editor );
	}

	g_info( "%s: ofaIDBProvider's %s implementation does not provide 'get_editor()' method",
			thisfn, G_OBJECT_TYPE_NAME( provider ));
	return( NULL );
}
