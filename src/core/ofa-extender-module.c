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

#include <glib.h>
#include <gmodule.h>

#include "my/my-iident.h"

#include "api/ofa-extender-module.h"
#include "api/ofa-igetter.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;
	gchar      *filename;

	/* runtime
	 */
	GModule    *library;
	GList      *objects;

	/* api                                                             v1
	 */
	gboolean   ( *startup )           ( GTypeModule *module, ofaIGetter *getter );
	gint       ( *list_types )        ( const GType **types );	/* mandatory */
	void       ( *shutdown )          ( void );					/* opt. */
}
	ofaExtenderModulePrivate;

static gboolean module_v_load( GTypeModule *module );
static void     module_v_unload( GTypeModule *module );
static gboolean plugin_is_valid( ofaExtenderModule *self );
static gboolean plugin_check( ofaExtenderModule *self, const gchar *symbol, gpointer *pfn );
static void     plugin_register_types( ofaExtenderModule *self );
static void     plugin_add_type( ofaExtenderModule *self, GType type );
static void     on_object_finalized( ofaExtenderModule *self, GObject *finalized_object );

G_DEFINE_TYPE_EXTENDED( ofaExtenderModule, ofa_extender_module, G_TYPE_TYPE_MODULE, 0,
		G_ADD_PRIVATE( ofaExtenderModule ))

static void
extender_module_finalize( GObject *object )
{
	static const gchar *thisfn = "ofa_extender_module_finalize";
	ofaExtenderModulePrivate *priv;

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_return_if_fail( object && OFA_IS_EXTENDER_MODULE( object ));

	/* free data members here */
	priv = ofa_extender_module_get_instance_private( OFA_EXTENDER_MODULE( object ));

	g_free( priv->filename );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_extender_module_parent_class )->finalize( object );
}

static void
extender_module_dispose( GObject *object )
{
	ofaExtenderModulePrivate *priv;

	g_return_if_fail( object && OFA_IS_EXTENDER_MODULE( object ));

	priv = ofa_extender_module_get_instance_private( OFA_EXTENDER_MODULE( object ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_extender_module_parent_class )->dispose( object );
}

static void
ofa_extender_module_init( ofaExtenderModule *self )
{
	static const gchar *thisfn = "ofa_extender_module_init";
	ofaExtenderModulePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXTENDER_MODULE( self ));

	priv = ofa_extender_module_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_extender_module_class_init( ofaExtenderModuleClass *klass )
{
	static const gchar *thisfn = "ofa_extender_module_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = extender_module_dispose;
	G_OBJECT_CLASS( klass )->finalize = extender_module_finalize;

	G_TYPE_MODULE_CLASS( klass )->load = module_v_load;
	G_TYPE_MODULE_CLASS( klass )->unload = module_v_unload;
}

/*
 * triggered by GTypeModule base class when first loading the library,
 * which is itself triggered by #ofa_extender_module_new::g_type_module_use()
 *
 * returns: %TRUE if the module is successfully loaded
 */
static gboolean
module_v_load( GTypeModule *module )
{
	static const gchar *thisfn = "ofa_extender_module_module_v_load";
	ofaExtenderModulePrivate *priv;
	gboolean loaded;

	g_debug( "%s: gmodule=%p", thisfn, ( void * ) module );

	priv = ofa_extender_module_get_instance_private( OFA_EXTENDER_MODULE( module ));

	loaded = FALSE;
	priv->library = g_module_open( priv->filename, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL );

	if( !priv->library ){
		g_warning( "%s: g_module_open: path=%s, error=%s", thisfn, priv->filename, g_module_error());
	} else {
		loaded = TRUE;
	}

	return( loaded );
}

/*
 * 'unload' is triggered by the last 'unuse' call
 */
static void
module_v_unload( GTypeModule *module )
{
	static const gchar *thisfn = "ofa_extender_module_module_v_unload";
	ofaExtenderModulePrivate *priv;

	g_debug( "%s: gmodule=%p", thisfn, ( void * ) module );

	priv = ofa_extender_module_get_instance_private( OFA_EXTENDER_MODULE( module ));

	if( priv->shutdown ){
		priv->shutdown();
	}

	if( priv->library ){
		g_module_close( priv->library );
	}

	/* reinitialise the mandatory API */
	priv->startup = NULL;
	priv->list_types = NULL;
}

/**
 * ofa_extender_module_new:
 * @getter: the global #ofaIGetter of the application.
 * @filename: the full path to the module file.
 *
 * Returns: a new reference to a #ofaExtenderModule object, or %NULL if
 * the candidate lilbrary is not a valid dynamically loadable module
 * compatible with the defined extendion API.
 */
ofaExtenderModule *
ofa_extender_module_new( ofaIGetter *getter, const gchar *filename )
{
	ofaExtenderModule *module;
	ofaExtenderModulePrivate *priv;

	module = g_object_new( OFA_TYPE_EXTENDER_MODULE, NULL );

	priv = ofa_extender_module_get_instance_private( module );

	priv->getter = getter;
	priv->filename = g_strdup( filename );

	if( !g_type_module_use( G_TYPE_MODULE( module )) || !plugin_is_valid( module )){
		g_object_unref( module );
		return( NULL );
	}

	plugin_register_types( module );

	g_type_module_unuse( G_TYPE_MODULE( module ));

	return( module );
}

/*
 * the module has been successfully loaded
 * is it a valid plugin ?
 * if ok, we ask the plugin to initialize itself
 *
 * As of API v 1:
 * - ofa_extension_startup() and ofa_extension_list_types() are
 *   mandatory and MUST be implemented by the plugin; they are
 *   successively called for each plugin
 * - ofa_extension_shutdown() is optional and will be called on plugin
 *   shutdown if it exists.
 * - ofa_extension_get_api_version is optional, and defaults to 1.
 * - ofa_extension_get_version_number is optional, and defaults to NULL.
 */
static gboolean
plugin_is_valid( ofaExtenderModule *self )
{
	static const gchar *thisfn = "ofa_extender_module_plugin_is_valid";
	ofaExtenderModulePrivate *priv;
	gboolean ok;

	priv = ofa_extender_module_get_instance_private( self );

	ok =
		plugin_check( self, "ofa_extension_startup" , ( gpointer * ) &priv->startup ) &&
		plugin_check( self, "ofa_extension_list_types" , ( gpointer * ) &priv->list_types ) &&
		priv->startup( G_TYPE_MODULE( self ), priv->getter );

	if( ok ){
		g_debug( "%s: %s: ok", thisfn, priv->filename );
		g_module_symbol( priv->library, "ofa_extension_shutdown", ( gpointer * ) &priv->shutdown );
	}

	return( ok );
}

static gboolean
plugin_check( ofaExtenderModule *self, const gchar *symbol, gpointer *pfn )
{
	static const gchar *thisfn = "ofa_extender_module_plugin_check";
	ofaExtenderModulePrivate *priv;
	gboolean ok;

	priv = ofa_extender_module_get_instance_private( self );

	ok = g_module_symbol( priv->library, symbol, pfn );

	if( !ok ){
		g_debug("%s: %s: %s: symbol not found", thisfn, priv->filename, symbol );
	}

	return( ok );
}

/*
 * The 'extension_startup()' function of the plugin has been already
 * called ; the GType types the plugin provides have so already been
 * registered in the GType system
 *
 * We ask here the plugin to give us a list of these GTypes.
 * For each GType, we allocate a new object of the given class
 * and keep this object in the module's list
 */
static void
plugin_register_types( ofaExtenderModule *self )
{
	ofaExtenderModulePrivate *priv;
	const GType *types;
	guint count, i;

	priv = ofa_extender_module_get_instance_private( self );

	count = priv->list_types( &types );
	priv->objects = NULL;

	for( i = 0 ; i < count ; i++ ){
		if( types[i] ){
			plugin_add_type( self, types[i] );
		}
	}
}

/*
 * The plugin has returned a list of the primary GType it provides.
 * Allocate a new object for each of these.
 */
static void
plugin_add_type( ofaExtenderModule *self, GType type )
{
	static const gchar *thisfn = "ofa_extender_module_plugin_add_type";
	ofaExtenderModulePrivate *priv;
	GObject *object;

	priv = ofa_extender_module_get_instance_private( self );

	object = g_object_new( type, NULL );

	g_debug( "%s: object=%p (%s)", thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_object_weak_ref( object, ( GWeakNotify ) on_object_finalized, self );

	priv->objects = g_list_prepend( priv->objects, object );
}

static void
on_object_finalized( ofaExtenderModule *self, GObject *finalized_object )
{
	static const gchar *thisfn = "ofa_extender_module_on_object_finalized";
	ofaExtenderModulePrivate *priv;

	g_debug( "%s: self=%p, finalized_object=%p (%s)",
			thisfn, ( void * ) self,
			( void * ) finalized_object, G_OBJECT_TYPE_NAME( finalized_object ));

	priv = ofa_extender_module_get_instance_private( self );

	priv->objects = g_list_remove( priv->objects, finalized_object );

	g_debug( "%s: new objects list after remove is %p, count=%d",
			thisfn, ( void * ) priv->objects, g_list_length( priv->objects ));
}

/*
 * ofa_extender_module_get_for_type:
 * @module: this #ofaExtenderModule instance.
 * @type: the serched GType.
 *
 * Returns: a list of objects instanciated by @module which are
 *  willing to deal with requested @type.
 *
 * The returned list is meant to be #ofa_extender_collection_free_types()
 * by the caller.
 */
GList *
ofa_extender_module_get_for_type( ofaExtenderModule *module, GType type )
{
	ofaExtenderModulePrivate *priv;
	GList *objects, *it;

	g_return_val_if_fail( module && OFA_IS_EXTENDER_MODULE( module ), NULL );

	priv = ofa_extender_module_get_instance_private( module );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	objects = NULL;

	for( it=priv->objects ; it ; it=it->next ){
		if( G_TYPE_CHECK_INSTANCE_TYPE( G_OBJECT( it->data ), type )){
			objects = g_list_prepend( objects, g_object_ref( it->data ));
		}
	}

	return( objects );
}

/*
 * ofa_extender_module_get_canon_name:
 * @module: this #ofaExtenderModule instance.
 *
 * Returns: the canonical name of the @module instance, as a newly
 * allocated string which should be g_free() by the caller, or %NULL.
 *
 * This method relies on the #myIIdent identification interface,
 * which is expected to be implemented by the loadable library.
 *
 * If the library implements and advertizes several primary GType, we
 * return here the name as provided by the first GObject which
 * implements the interface.
 */
gchar *
ofa_extender_module_get_canon_name( const ofaExtenderModule *module )
{
	ofaExtenderModulePrivate *priv;
	GList *it;

	g_return_val_if_fail( module && OFA_IS_EXTENDER_MODULE( module ), NULL );

	priv = ofa_extender_module_get_instance_private( module );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	for( it=priv->objects ; it ; it=it->next ){
		if( MY_IS_IIDENT( it->data )){
			return( my_iident_get_canon_name( MY_IIDENT( it->data ), NULL ));
		}
	}

	return( NULL );
}

/*
 * ofa_extender_module_get_display_name:
 * @module: this #ofaExtenderModule instance.
 *
 * Returns: the displayable name of the @module instance, as a newly
 * allocated string which should be g_free() by the caller, or %NULL.
 *
 * The displayable name finally defaults to the basename of the library.
 *
 * This method relies on the #myIIdent identification interface,
 * which is expected to be implemented by the loadable library.
 *
 * If the library implements and advertizes several primary GType, we
 * return here the full name as provided by the first GObject which
 * implements the interface.
 */
gchar *
ofa_extender_module_get_display_name( const ofaExtenderModule *module )
{
	ofaExtenderModulePrivate *priv;
	GList *it;

	g_return_val_if_fail( module && OFA_IS_EXTENDER_MODULE( module ), NULL );

	priv = ofa_extender_module_get_instance_private( module );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	for( it=priv->objects ; it ; it=it->next ){
		if( MY_IS_IIDENT( it->data )){
			return( my_iident_get_display_name( MY_IIDENT( it->data ), NULL ));
		}
	}

	return( g_path_get_basename( priv->filename ));
}

/*
 * ofa_extender_module_get_version:
 * @module: this #ofaExtenderModule instance.
 *
 * Returns: the version string of the @module instance, as a newly
 * allocated string which should be g_free() by the caller, or %NULL.
 *
 * This method relies on the #myIIdent identification interface,
 * which is expected to be implemented by the loadable library.
 *
 * If the library implements and advertizes several primary GType, we
 * return here the version string as provided by the first GObject which
 * implements the interface.
 */
gchar *
ofa_extender_module_get_version( const ofaExtenderModule *module )
{
	ofaExtenderModulePrivate *priv;
	GList *it;

	g_return_val_if_fail( module && OFA_IS_EXTENDER_MODULE( module ), NULL );

	priv = ofa_extender_module_get_instance_private( module );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	for( it=priv->objects ; it ; it=it->next ){
		if( MY_IS_IIDENT( it->data )){
			return( my_iident_get_version( MY_IIDENT( it->data ), NULL ));
		}
	}

	return( NULL );
}

/**
 * ofa_extender_module_has_object:
 * @module:
 * @instance: an instanciated object, for which we are searching for
 *  its host plugin
 */
gboolean
ofa_extender_module_has_object( const ofaExtenderModule *module, GObject *instance )
{
	ofaExtenderModulePrivate *priv;
	GList *it;

	g_return_val_if_fail( module && OFA_IS_EXTENDER_MODULE( module ), FALSE );

	priv = ofa_extender_module_get_instance_private( module );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	for( it=priv->objects ; it ; it=it->next ){
		if( G_OBJECT( it->data ) == instance ){
			return( TRUE );
		}
	}

	return( FALSE );
}
