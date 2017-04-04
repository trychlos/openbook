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

#include "my/my-iident.h"
#include "my/my-utils.h"

#include "api/ofa-extender-collection.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-editor.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-editor.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-idbsuperuser.h"
#include "api/ofa-iextender-setter.h"
#include "api/ofa-igetter.h"

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
 * @getter: an #ofaIGetter instance.
 *
 * Returns: the list of available #ofaIDBProvider providers.
 *
 * The returned list should be g_list_free() by the caller.
 */
GList *
ofa_idbprovider_get_all( ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_idbprovider_get_all";
	ofaExtenderCollection *extenders;
	GList *modules, *it, *all;

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	all = NULL;
	extenders = ofa_igetter_get_extender_collection( getter );
	modules = ofa_extender_collection_get_for_type( extenders, OFA_TYPE_IDBPROVIDER );

	for( it=modules ; it ; it=it->next ){
		if( !MY_IS_IIDENT( it->data )){
			g_info( "%s: %s class does not implement myIIdent interface",
					thisfn, G_OBJECT_TYPE_NAME( it->data ));
		} else if( !OFA_IS_IEXTENDER_SETTER( it->data )){
			g_info( "%s: %s class does not implement ofaIExtenderSetter interface",
					thisfn, G_OBJECT_TYPE_NAME( it->data ));
		} else {
			all = g_list_prepend( all, it->data );
		}
	}

	g_list_free( modules );

	return( all );
}

/**
 * ofa_idbprovider_get_by_name:
 * @getter: an #ofaIGetter instance.
 * @provider_name: the name of the provider as published in the
 *  settings.
 *
 * Returns: the #ofaIDBProvider module instance which publishes this
 * canonical name.
 *
 * We check here:
 * - that the #ofaIDBProvider implementation also implements the
 *   #ofaIExtenderSetter interface (which is a prerequisite).
 *
 * A provider which does not satisfy all prerequisites is not returned.
 *
 * The returned reference is owned by the provider, and should not be
 * unreffed by the caller.
 */
ofaIDBProvider *
ofa_idbprovider_get_by_name( ofaIGetter *getter, const gchar *provider_name )
{
	static const gchar *thisfn = "ofa_idbprovider_get_by_name";
	GList *modules;
	ofaIDBProvider *provider;

	g_debug( "%s: getter=%p, provider_name=%s", thisfn, ( void * ) getter, provider_name );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	modules = ofa_idbprovider_get_all( getter );
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
 * ofa_idbprovider_get_getter:
 * @provider: this #ofaIDBProvider provider.
 *
 * Returns: a #ofaIGetter instance.
 */
ofaIGetter *
ofa_idbprovider_get_getter( ofaIDBProvider *provider )
{
	ofaIGetter *getter;

	g_return_val_if_fail( provider && OFA_IS_IDBPROVIDER( provider ), NULL );

	getter = ofa_iextender_setter_get_getter( OFA_IEXTENDER_SETTER( provider ));
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	return( getter );
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
 * @with_su: whether the editor should display the super-user widget.
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
ofa_idbprovider_new_dossier_editor( ofaIDBProvider *provider, const gchar *settings_prefix, guint rule, gboolean with_su )
{
	static const gchar *thisfn = "ofa_idbprovider_new_dossier_editor";
	ofaIDBDossierEditor *editor;

	g_debug( "%s: provider=%p, settings_prefix=%s, rule=%u, with_su=%s",
			thisfn,( void * ) provider, settings_prefix, rule, with_su ? "True":"False" );

	g_return_val_if_fail( provider && OFA_IS_IDBPROVIDER( provider ), NULL );

	if( OFA_IDBPROVIDER_GET_INTERFACE( provider )->new_dossier_editor ){
		editor = OFA_IDBPROVIDER_GET_INTERFACE( provider )->new_dossier_editor( provider, settings_prefix, rule, with_su );
		ofa_idbdossier_editor_set_provider( editor, provider );
		return( editor );
	}

	g_info( "%s: ofaIDBProvider's %s implementation does not provide 'new_dossier_editor()' method",
			thisfn, G_OBJECT_TYPE_NAME( provider ));
	return( NULL );
}

/**
 * ofa_idbprovider_new_exercice_editor:
 * @editor: this #ofaIDBProvider provider.
 * @settings_prefix: the prefix of a user preference key.
 * @rule: the usage of the editor.
 *
 * Returns: a composite GTK container widget intended to hold the
 * informations needed to fully identify the DBMS server which manages
 * a exercice.
 *
 * The returned container will be added to a GtkWindow and must be
 * destroyable with this same window. In other words, the DBMS provider
 * should not keep any reference on this container.
 */
ofaIDBExerciceEditor *
ofa_idbprovider_new_exercice_editor( ofaIDBProvider *provider, const gchar *settings_prefix, guint rule )
{
	static const gchar *thisfn = "ofa_idbprovider_new_exercice_editor";
	ofaIDBExerciceEditor *exercice_editor;

	g_debug( "%s: provider=%p, settings_prefix=%s, rule=%u",
			thisfn,( void * ) provider, settings_prefix, rule );

	g_return_val_if_fail( provider && OFA_IS_IDBPROVIDER( provider ), NULL );

	if( OFA_IDBPROVIDER_GET_INTERFACE( provider )->new_exercice_editor ){
		exercice_editor = OFA_IDBPROVIDER_GET_INTERFACE( provider )->new_exercice_editor( provider, settings_prefix, rule );
		ofa_idbexercice_editor_set_provider( exercice_editor, provider );
		return( exercice_editor );
	}

	g_info( "%s: ofaIDBProvider's %s implementation does not provide 'new_exercice_editor()' method",
			thisfn, G_OBJECT_TYPE_NAME( provider ));
	return( NULL );
}

/**
 * ofa_idbprovider_new_superuser_bin:
 * @provider: this #ofaIDBProvider provider.
 * @rule: the usage of this widget.
 *
 * Returns: a composite GTK container widget intended to hold the
 * informations needed to fully identify the super-user privileges.
 *
 * The returned container will be added to a GtkWindow and must be
 * destroyable with this same window. In other words, the DBMS provider
 * should not keep any reference on this container.
 */
ofaIDBSuperuser *
ofa_idbprovider_new_superuser_bin( ofaIDBProvider *provider, guint rule )
{
	static const gchar *thisfn = "ofa_idbprovider_new_superuser_bin";
	ofaIDBSuperuser *bin;

	g_debug( "%s: provider=%p, rule=%u",
			thisfn,( void * ) provider, rule );

	g_return_val_if_fail( provider && OFA_IS_IDBPROVIDER( provider ), NULL );

	if( OFA_IDBPROVIDER_GET_INTERFACE( provider )->new_superuser_bin ){
		bin = OFA_IDBPROVIDER_GET_INTERFACE( provider )->new_superuser_bin( provider, rule );
		if( bin ){
			ofa_idbsuperuser_set_provider( bin, provider );
		}
		return( bin );
	}

	g_info( "%s: ofaIDBProvider's %s implementation does not provide 'new_superuser_bin()' method",
			thisfn, G_OBJECT_TYPE_NAME( provider ));
	return( NULL );
}
