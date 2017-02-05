/*
x * Open Firm Accounting
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

#include <glib.h>
#include <gmodule.h>

#include "my/my-iident.h"

#include "api/ofa-extender-module.h"
#include "api/ofa-extension.h"
#include "api/ofa-iextender-setter.h"
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

	/* api					                                                                             v1         v2
	 */
	gboolean ( *startup )   ( GTypeModule *module, ofaIGetter *getter );							/* mandatory  mandatory  */
	gint     ( *list_types )( const GType **types );												/* mandatory  deprecated */
	void     ( *enum_types )( GTypeModule *module, ofaExtensionEnumTypesCb cb, void *user_data );	/*     -      mandatory  */
	void     ( *shutdown )  ( GTypeModule *module );												/* opt.       opt.       */
}
	ofaExtenderModulePrivate;

static gboolean module_v_load( GTypeModule *module );
static void     module_v_unload( GTypeModule *module );
static gboolean plugin_is_valid( ofaExtenderModule *self );
static gboolean plugin_check( ofaExtenderModule *self, const gchar *symbol, gpointer *pfn );
static void     plugin_register_types( ofaExtenderModule *self );
static void     plugin_enum_type( GType type, ofaExtenderModule *self );
static void     plugin_add_type( ofaExtenderModule *self, GType type );

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
		g_list_free_full( priv->objects, ( GDestroyNotify ) g_object_unref );

		/* it may happens that use_count = 0 when g_type_module_use() has
		 *  been unsuccessful (unable to load the module) */
		if( G_TYPE_MODULE( object )->use_count > 0 ){
			g_type_module_unuse( G_TYPE_MODULE( object ));
		}
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

	g_debug( "%s: module=%p", thisfn, ( void * ) module );

	priv = ofa_extender_module_get_instance_private( OFA_EXTENDER_MODULE( module ));

	loaded = TRUE;
	priv->library = g_module_open( priv->filename, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL );

	if( !priv->library ){
		g_warning( "%s: g_module_open: path=%s, error=%s", thisfn, priv->filename, g_module_error());
		loaded = FALSE;

	} else if( !plugin_is_valid( OFA_EXTENDER_MODULE( module ))){
		g_module_close( priv->library );
		priv->library = NULL;
		loaded = FALSE;
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

	g_debug( "%s: module=%p", thisfn, ( void * ) module );

	priv = ofa_extender_module_get_instance_private( OFA_EXTENDER_MODULE( module ));

	if( priv->shutdown ){
		priv->shutdown( module );
	}

	if( priv->library ){
		g_module_close( priv->library );
		priv->library = NULL;
	}

	/* reinitialise the mandatory API */
	priv->startup = NULL;
	priv->list_types = NULL;
	priv->enum_types = NULL;
}

/**
 * ofa_extender_module_new:
 * @getter: the global #ofaIGetter of the application.
 * @filename: the full path to the module file.
 *
 * Returns: a new reference to a #ofaExtenderModule object, or %NULL if
 * the candidate library is not a valid dynamically loadable module
 * compatible with the defined extension API.
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

	if( !g_type_module_use( G_TYPE_MODULE( module ))){
		g_object_unref( module );
		return( NULL );
	}

	plugin_register_types( module );

	/* so that the last references for keeping the module loaded
	 *  are those of the instanciated GObjects's themselves */

	/* NB: it is not enough to have instanciated objects to keep the
	 *  module loaded, unless these objects exhibit dynamic types */
	//g_type_module_unuse( G_TYPE_MODULE( module ));

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
 */
static gboolean
plugin_is_valid( ofaExtenderModule *self )
{
	static const gchar *thisfn = "ofa_extender_module_plugin_is_valid";
	ofaExtenderModulePrivate *priv;
	gboolean ok;

	priv = ofa_extender_module_get_instance_private( self );

	ok = plugin_check( self, "ofa_extension_startup" , ( gpointer * ) &priv->startup );

	/* ofa_extension_list_types in v1 is deprecated by ofa_extension_enum_types in v2 */
	if( ok ){
		ok = plugin_check( self, "ofa_extension_list_types" , ( gpointer * ) &priv->list_types ) ||
			plugin_check( self, "ofa_extension_enum_types" , ( gpointer * ) &priv->enum_types );
	}

	if( ok ){
		g_debug( "%s: %s: ok", thisfn, priv->filename );
		g_module_symbol( priv->library, "ofa_extension_shutdown", ( gpointer * ) &priv->shutdown );
		ok = priv->startup( G_TYPE_MODULE( self ), priv->getter );
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

	if( priv->enum_types ){
		priv->enum_types(
				G_TYPE_MODULE( self ), ( ofaExtensionEnumTypesCb ) plugin_enum_type, self );

	} else {
		count = priv->list_types( &types );
		priv->objects = NULL;

		for( i = 0 ; i < count ; i++ ){
			if( types[i] ){
				plugin_add_type( self, types[i] );
			}
		}
	}
}

static void
plugin_enum_type( GType type, ofaExtenderModule *self )
{
	plugin_add_type( self, type );
}

/*
 * The plugin has returned a list of the primary GType it provides.
 * Allocate a new object for each of these.
 *
 * For those who implement the #ofaIExtenderSetter interface, calls it.
 *
 * Note that there is no need to pur a weak reference on these objects,
 * to release some resources in finalization. These objects will only
 * be released on ExtenderModule dispose(), itself only being called
 * on ExtenderCollection dispose().
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

	if( OFA_IS_IEXTENDER_SETTER( object )){
		ofa_iextender_setter_set_getter( OFA_IEXTENDER_SETTER( object ), priv->getter );
	}

	/* keep the order provided by the module */
	priv->objects = g_list_append( priv->objects, object );
}

/*
 * ofa_extender_module_get_for_objects:
 * @module: this #ofaExtenderModule instance.
 *
 * Returns: the full list of objects instanciated by @module.
 *
 * The returned list is owned by the @module, and should not be
 * released by the caller.
 */
const GList *
ofa_extender_module_get_objects( ofaExtenderModule *module )
{
	ofaExtenderModulePrivate *priv;

	g_return_val_if_fail( module && OFA_IS_EXTENDER_MODULE( module ), NULL );

	priv = ofa_extender_module_get_instance_private( module );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->objects );
}

/*
 * ofa_extender_module_get_for_type:
 * @module: this #ofaExtenderModule instance.
 * @type: the serched GType.
 *
 * Returns: a list of objects instanciated by @module which are
 *  willing to deal with requested @type.
 *
 * The returned references are owned by @module and should not be
 * released by the caller. The returned list should still be
 * #g_list_free().
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
			objects = g_list_prepend( objects, it->data );
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
ofa_extender_module_get_canon_name( ofaExtenderModule *module )
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
ofa_extender_module_get_display_name( ofaExtenderModule *module )
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
ofa_extender_module_get_version( ofaExtenderModule *module )
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
ofa_extender_module_has_object( ofaExtenderModule *module, GObject *instance )
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
