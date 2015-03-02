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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gmodule.h>

#include "api/my-utils.h"

#include "core/ofa-plugin.h"

/* private instance data
 */
struct _ofaPluginPrivate {
	gboolean  dispose_has_run;

	gchar    *path;						/* full pathname of the plugin library */
	gchar    *name;						/* basename without the extension */
	GModule  *library;
	GList    *objects;

	/* api                                                             v1
	 */
	gboolean     ( *startup )           ( GTypeModule *module );	/* mandatory */
	guint        ( *get_api_version )   ( void );					/* opt. */
	const gchar *( *get_name )          ( void );					/* opt. */
	const gchar *( *get_version_number )( void );					/* opt. */
	gint         ( *list_types )        ( const GType **types );	/* mandatory */
	void         ( *shutdown )          ( void );					/* opt. */
	void         ( *preferences_run )   ( void );					/* opt. */
};

/* the list of loaded modules is statically maintained
 */
static GList *st_modules = NULL;

G_DEFINE_TYPE( ofaPlugin, ofa_plugin, G_TYPE_TYPE_MODULE )

static ofaPlugin *plugin_new( const gchar *filename );
static gboolean   v_plugin_load( GTypeModule *gmodule );
static gboolean   is_an_ofa_plugin( ofaPlugin *plugin );
static gboolean   plugin_check( ofaPlugin *plugin, const gchar *symbol, gpointer *pfn );
static void       register_module_types( ofaPlugin *plugin );
static void       add_module_type( ofaPlugin *plugin, GType type );
static void       on_object_finalized( ofaPlugin *plugin, GObject *finalized_object );
static void       v_plugin_unload( GTypeModule *gmodule );

static void
plugin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_plugin_finalize";
	ofaPluginPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PLUGIN( instance ));

	/* free data members here */
	priv = OFA_PLUGIN( instance )->priv;
	g_free( priv->path );
	g_free( priv->name );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_plugin_parent_class )->finalize( instance );
}

static void
plugin_dispose( GObject *instance )
{
	ofaPluginPrivate *priv;

	g_return_if_fail( instance && OFA_IS_PLUGIN( instance ));

	priv = OFA_PLUGIN( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	G_OBJECT_CLASS( ofa_plugin_parent_class )->dispose( instance );
}

static void
ofa_plugin_init( ofaPlugin *self )
{
	static const gchar *thisfn = "ofa_plugin_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PLUGIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_PLUGIN, ofaPluginPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
ofa_plugin_class_init( ofaPluginClass *klass )
{
	static const gchar *thisfn = "ofa_plugin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = plugin_dispose;
	G_OBJECT_CLASS( klass )->finalize = plugin_finalize;

	G_TYPE_MODULE_CLASS( klass )->load = v_plugin_load;
	G_TYPE_MODULE_CLASS( klass )->unload = v_plugin_unload;

	g_type_class_add_private( klass, sizeof( ofaPluginPrivate ));
}

/*
 * ofa_plugin_dump:
 * @module: this #ofaPlugin instance.
 *
 * Dumps the content of the module.
 */
void
ofa_plugin_dump( const ofaPlugin *plugin )
{
	static const gchar *thisfn = "ofa_plugin_dump";
	ofaPluginPrivate *priv;
	GList *iobj;

	priv = plugin->priv;

	g_debug( "%s:    path=%s", thisfn, priv->path );
	g_debug( "%s:    name=%s", thisfn, priv->name );
	g_debug( "%s: library=%p", thisfn, ( void * ) priv->library );
	g_debug( "%s: objects=%p (count=%d)", thisfn, ( void * ) priv->objects, g_list_length( priv->objects ));
	for( iobj = priv->objects ; iobj ; iobj = iobj->next ){
		g_debug( "%s:    iobj=%p (%s)",
				thisfn, ( void * ) iobj->data, G_OBJECT_TYPE_NAME( iobj->data ));
	}
}

/*
 * ofa_plugin_load_modules:
 *
 * Load availables dynamically loadable extension libraries (plugins).
 *
 * The list of successfully loaded libraries is maintained as a #GList
 * in the global static 'st_modules' variable.
 * This list should be #ofa_plugin_release_modules() by the caller
 * after use (at the end of the program).
 *
 * Returns: the count of sucessfully loaded libraries, each of them
 * being managed by an #ofaPlugin instance.
 */
gint
ofa_plugin_load_modules( void )
{
	static const gchar *thisfn = "ofa_plugin_load_modules";
	const gchar *dirname = PKGLIBDIR;
	const gchar *suffix = ".so";
	GDir *api_dir;
	GError *error;
	const gchar *entry;
	gchar *fname;
	ofaPlugin *plugin;

	g_debug( "%s", thisfn );

	st_modules = NULL;
	error = NULL;

	api_dir = g_dir_open( dirname, 0, &error );
	if( error ){
		g_warning( "%s: g_dir_open: %s", thisfn, error->message );
		g_error_free( error );
		error = NULL;

	} else {
		while(( entry = g_dir_read_name( api_dir )) != NULL ){
			if( g_str_has_suffix( entry, suffix )){
				fname = g_build_filename( dirname, entry, NULL );
				plugin = plugin_new( fname );
				if( plugin ){
					plugin->priv->name = my_utils_str_remove_suffix( entry, suffix );
					st_modules = g_list_append( st_modules, plugin );
					g_debug( "%s: module %s successfully loaded", thisfn, entry );
				}
				g_free( fname );
			}
		}
		g_dir_close( api_dir );
	}

	return( g_list_length( st_modules ));
}

/*
 * ofa_plugin_release_modules:
 * @modules: the list of loaded modules.
 *
 * Release resources allocated to the loaded modules.
 */
void
ofa_plugin_release_modules( void )
{
	static const gchar *thisfn = "ofa_plugin_release_modules";
	ofaPlugin *plugin;
	GList *imod;
	GList *iobj;

	g_debug( "%s: st_modules=%p, count=%u",
			thisfn, ( void * ) st_modules, g_list_length( st_modules ));

	for( imod = st_modules ; imod ; imod = imod->next ){
		plugin = OFA_PLUGIN( imod->data );

		g_debug( "%s: objects=%p, count=%u",
				thisfn, ( void * ) plugin->priv->objects, g_list_length( plugin->priv->objects ));

#if 0
		for( iobj = plugin->priv->objects ; iobj ; iobj = iobj->next ){
			if( G_IS_OBJECT( iobj->data )){
				g_object_unref( iobj->data );
			} else {
				g_warning( "%s: object=%p is not a GObject", thisfn, ( void * ) iobj->data );
			}
		}
#endif
		while( plugin->priv->objects ){
			iobj = plugin->priv->objects;
			if( G_IS_OBJECT( iobj->data )){
				g_object_unref( iobj->data );
			} else {
				g_warning( "%s: object=%p is not a GObject", thisfn, ( void * ) iobj->data );
			}
		}

		g_type_module_unuse( G_TYPE_MODULE( plugin ));
	}

	g_list_free( st_modules );
	st_modules = NULL;
}

/*
 * @fname: full pathname of the being-loaded dynamic library.
 */
static ofaPlugin *
plugin_new( const gchar *fname )
{
	ofaPlugin *plugin;

	plugin = g_object_new( OFA_TYPE_PLUGIN, NULL );
	plugin->priv->path = g_strdup( fname );

	if( !g_type_module_use( G_TYPE_MODULE( plugin )) || !is_an_ofa_plugin( plugin )){
		g_object_unref( plugin );
		return( NULL );
	}

	register_module_types( plugin );

	return( plugin );
}

/*
 * triggered by GTypeModule base class when first loading the library,
 * which is itself triggered by module_new:g_type_module_use()
 *
 * returns: %TRUE if the module is successfully loaded
 */
static gboolean
v_plugin_load( GTypeModule *gmodule )
{
	static const gchar *thisfn = "ofa_plugin_v_plugin_load";
	ofaPlugin *plugin;
	gboolean loaded;

	g_return_val_if_fail( G_IS_TYPE_PLUGIN( gmodule ), FALSE );

	g_debug( "%s: gmodule=%p", thisfn, ( void * ) gmodule );

	loaded = FALSE;
	plugin = OFA_PLUGIN( gmodule );

	plugin->priv->library = g_module_open(
			plugin->priv->path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL );

	if( !plugin->priv->library ){
		g_warning( "%s: g_module_open: path=%s, error=%s",
						thisfn, plugin->priv->path, g_module_error());

	} else {
		loaded = TRUE;
	}

	return( loaded );
}

/*
 * the module has been successfully loaded
 * is it an OFA plugin ?
 * if ok, we ask the plugin to initialize itself
 *
 * As of API v 1:
 * - only ofa_extension_list_types() is mandatory, and MUST be
 *   implemented by the plugin
 * - ofa_extension_startup() and ofa_extension_shutdown() are optional,
 *   and will be called on plugin startup (resp. shutdown) if they exist.
 * - ofa_extension_get_api_version is optional, and defaults to 1.
 * - ofa_extension_get_version_number is optional, and defaults to NULL.
 */
static gboolean
is_an_ofa_plugin( ofaPlugin *plugin )
{
	static const gchar *thisfn = "ofa_plugin_is_an_ofa_plugin";
	gboolean ok;

	ok =
		plugin_check( plugin, "ofa_extension_startup" , ( gpointer * ) &plugin->priv->startup ) &&
		plugin_check( plugin, "ofa_extension_list_types" , ( gpointer * ) &plugin->priv->list_types ) &&
		plugin->priv->startup( G_TYPE_MODULE( plugin ));

	if( ok ){
		g_debug( "%s: %s: ok", thisfn, plugin->priv->path );
	}

	return( ok );
}

static gboolean
plugin_check( ofaPlugin *plugin, const gchar *symbol, gpointer *pfn )
{
	static const gchar *thisfn = "ofa_plugin_plugin_check";
	gboolean ok;

	ok = g_module_symbol( plugin->priv->library, symbol, pfn );

	if( !ok ){
		g_debug("%s: %s: %s: symbol not found", thisfn, plugin->priv->path, symbol );
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
register_module_types( ofaPlugin *plugin )
{
	const GType *types;
	guint count, i;

	count = plugin->priv->list_types( &types );
	plugin->priv->objects = NULL;

	for( i = 0 ; i < count ; i++ ){
		if( types[i] ){
			add_module_type( plugin, types[i] );
		}
	}
}

static void
add_module_type( ofaPlugin *plugin, GType type )
{
	GObject *object;

	object = g_object_new( type, NULL );
	g_debug( "ofa_plugin_add_module_type: allocating object=%p (%s)",
						( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_object_weak_ref( object, ( GWeakNotify ) on_object_finalized, plugin );

	plugin->priv->objects = g_list_prepend( plugin->priv->objects, object );
}

static void
on_object_finalized( ofaPlugin *plugin, GObject *finalized_object )
{
	static const gchar *thisfn = "ofa_plugin_object_finalized";

	g_debug( "%s: plugin=%p, finalized_object=%p (%s)",
			thisfn, ( void * ) plugin,
			( void * ) finalized_object, G_OBJECT_TYPE_NAME( finalized_object ));

	plugin->priv->objects = g_list_remove( plugin->priv->objects, finalized_object );

	g_debug( "%s: new objects list after remove is %p", thisfn, ( void * ) plugin->priv->objects );
}

/*
 * 'unload' is triggered by the last 'unuse' call
 * which is itself called in ofa_plugin::instance_dispose
 */
static void
v_plugin_unload( GTypeModule *gmodule )
{
	static const gchar *thisfn = "ofa_plugin_v_plugin_unload";
	ofaPlugin *plugin;

	g_return_if_fail( G_IS_TYPE_PLUGIN( gmodule ));

	g_debug( "%s: gmodule=%p", thisfn, ( void * ) gmodule );

	plugin = OFA_PLUGIN( gmodule );

	if( plugin->priv->shutdown ){
		plugin->priv->shutdown();
	}

	if( plugin->priv->library ){
		g_module_close( plugin->priv->library );
	}

	/* reinitialise the mandatory API */
	plugin->priv->startup = NULL;
	plugin->priv->list_types = NULL;
}

/*
 * ofa_plugin_get_extensions_for_type:
 * @type: the serched GType.
 *
 * Returns: a list of objects instanciated by loaded modules which are
 *  willing to deal with requested @type.
 *
 * The returned list should be ofa_plugin_free_extensions_list() by the caller.
 */
GList *
ofa_plugin_get_extensions_for_type( GType type )
{
	GList *willing_to, *im, *io;
	ofaPlugin *plugin;

	willing_to = NULL;

	for( im = st_modules; im ; im = im->next ){
		plugin = OFA_PLUGIN( im->data );
		for( io = plugin->priv->objects ; io ; io = io->next ){
			if( G_TYPE_CHECK_INSTANCE_TYPE( G_OBJECT( io->data ), type )){
				willing_to = g_list_prepend( willing_to, g_object_ref( io->data ));
			}
		}
	}

	return( willing_to );
}

/*
 * ofa_plugin_free_extensions:
 * @extensions: a #GList as returned by #ofa_plugin_get_extensions_for_type().
 *
 * Free the previously returned list.
 */
void
ofa_plugin_free_extensions( GList *extensions )
{
	g_list_foreach( extensions, ( GFunc ) g_object_unref, NULL );
	g_list_free( extensions );
}

/**
 * ofa_plugin_get_modules:
 *
 * Returns the current list of #ofaPlugin objects, which correspond to
 * the list of dynamically loaded libraries.
 *
 * We're reminding that each #ofaPlugin object returned in this list
 * itself maintains a list of GObjects which themselves implement one
 * or more application interfaces.
 */
const GList *
ofa_plugin_get_modules( void )
{
	return(( const GList * ) st_modules );
}

/**
 * ofa_plugin_implements_type:
 */
gboolean
ofa_plugin_implements_type( const ofaPlugin *plugin, GType type )
{
	ofaPluginPrivate *priv;
	GList *io;

	g_return_val_if_fail( plugin && OFA_IS_PLUGIN( plugin ), FALSE );

	priv = plugin->priv;

	if( !priv->dispose_has_run ){

		for( io = priv->objects ; io ; io = io->next ){
			if( G_TYPE_CHECK_INSTANCE_TYPE( G_OBJECT( io->data ), type )){
				return( TRUE );
			}
		}
	}

	return( FALSE );
}

/**
 * ofa_plugin_has_object:
 * @plugin:
 * @instance: an instanciated object, for which we are searching for
 *  its host plugin
 */
gboolean
ofa_plugin_has_object( const ofaPlugin *plugin, GObject *instance )
{
	ofaPluginPrivate *priv;
	GList *io;

	g_return_val_if_fail( plugin && OFA_IS_PLUGIN( plugin ), FALSE );

	priv = plugin->priv;

	if( !priv->dispose_has_run ){

		for( io = priv->objects ; io ; io = io->next ){
			if( G_OBJECT( io->data ) == instance ){
				return( TRUE );
			}
		}
	}

	return( FALSE );
}

/**
 * ofa_plugin_get_name:
 */
const gchar *
ofa_plugin_get_name( ofaPlugin *plugin )
{
	ofaPluginPrivate *priv;

	g_return_val_if_fail( plugin && OFA_IS_PLUGIN( plugin ), NULL );

	priv = plugin->priv;

	if( !priv->dispose_has_run ){

		if( !priv->get_name ){
			plugin_check( plugin, "ofa_extension_get_name", ( gpointer * ) &priv->get_name );
		}

		if( priv->get_name ){
			return( priv->get_name());
		}
	}

	return( NULL );
}

/**
 * ofa_plugin_get_version_number:
 */
const gchar *
ofa_plugin_get_version_number( ofaPlugin *plugin )
{
	ofaPluginPrivate *priv;

	g_return_val_if_fail( plugin && OFA_IS_PLUGIN( plugin ), NULL );

	priv = plugin->priv;

	if( !priv->dispose_has_run ){

		if( !priv->get_version_number ){
			plugin_check( plugin, "ofa_extension_get_version_number", ( gpointer * ) &priv->get_version_number );
		}

		if( priv->get_version_number ){
			return( priv->get_version_number());
		}
	}

	return( NULL );
}
