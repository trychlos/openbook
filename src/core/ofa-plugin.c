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

#include <gmodule.h>

#include "api/my-utils.h"
#include "api/ofa-plugin.h"

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
	gboolean     ( *startup )           ( GTypeModule *module, GApplication *application );
																	/* mandatory */
	guint        ( *get_api_version )   ( void );					/* opt. */
	const gchar *( *get_name )          ( void );					/* opt. */
	const gchar *( *get_version_number )( void );					/* opt. */
	gint         ( *list_types )        ( const GType **types );	/* mandatory */
	void         ( *shutdown )          ( void );					/* opt. */
	void         ( *preferences_run )   ( void );					/* opt. */
};

/* the list of loaded modules is statically maintained
 */
static GList *st_modules                = NULL;

static ofaPlugin *plugin_new( const gchar *filename, GApplication *application );
static gboolean   v_plugin_load( GTypeModule *gmodule );
static gboolean   is_an_ofa_plugin( ofaPlugin *plugin, GApplication *application );
static gboolean   plugin_check( ofaPlugin *plugin, const gchar *symbol, gpointer *pfn );
static void       register_module_types( ofaPlugin *plugin );
static void       add_module_type( ofaPlugin *plugin, GType type );
static void       on_object_finalized( ofaPlugin *plugin, GObject *finalized_object );
static void       v_plugin_unload( GTypeModule *gmodule );

G_DEFINE_TYPE_EXTENDED( ofaPlugin, ofa_plugin, G_TYPE_TYPE_MODULE, 0, \
		G_ADD_PRIVATE( ofaPlugin ));

static void
plugin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_plugin_finalize";
	ofaPluginPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PLUGIN( instance ));

	/* free data members here */
	priv = ofa_plugin_get_instance_private( OFA_PLUGIN( instance ));

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

	priv = ofa_plugin_get_instance_private( OFA_PLUGIN( instance ));

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
	ofaPluginPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PLUGIN( self ));

	priv = ofa_plugin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
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

	priv = ofa_plugin_get_instance_private( plugin );

	g_debug( "%s:    path=%s", thisfn, priv->path );
	g_debug( "%s:    name=%s", thisfn, priv->name );
	g_debug( "%s: library=%p", thisfn, ( void * ) priv->library );
	g_debug( "%s: objects=%p (count=%d)", thisfn, ( void * ) priv->objects, g_list_length( priv->objects ));
	for( iobj = priv->objects ; iobj ; iobj = iobj->next ){
		g_debug( "%s:    iobj=%p (%s)", thisfn, ( void * ) iobj->data, G_OBJECT_TYPE_NAME( iobj->data ));
	}
}

/**
 * ofa_plugin_load_modules:
 * @application: the #GApplication application instance.
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
 * Returns: -1 if an error has occured.
 */
gint
ofa_plugin_load_modules( GApplication *application )
{
	static const gchar *thisfn = "ofa_plugin_load_modules";
	const gchar *dirname = PKGLIBDIR;
	const gchar *suffix = ".so";
	GDir *api_dir;
	GError *error;
	const gchar *entry;
	gchar *fname;
	ofaPlugin *plugin;
	ofaPluginPrivate *priv;

	g_debug( "%s", thisfn );

	st_modules = NULL;
	error = NULL;

	api_dir = g_dir_open( dirname, 0, &error );
	if( error ){
		g_warning( "%s: g_dir_open: %s", thisfn, error->message );
		g_error_free( error );
		return( -1 );
	}

	while(( entry = g_dir_read_name( api_dir )) != NULL ){
		if( g_str_has_suffix( entry, suffix )){
			fname = g_build_filename( dirname, entry, NULL );
			plugin = plugin_new( fname, application );
			if( plugin ){
				priv = ofa_plugin_get_instance_private( plugin );
				priv->name = my_utils_str_remove_suffix( entry, suffix );
				st_modules = g_list_append( st_modules, plugin );
				g_debug( "%s: module %s successfully loaded", thisfn, entry );
			}
			g_free( fname );
		}
	}
	g_dir_close( api_dir );

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
	ofaPluginPrivate *priv;
	GList *imod;
	GList *iobj;

	g_debug( "%s: st_modules=%p, count=%u",
			thisfn, ( void * ) st_modules, g_list_length( st_modules ));

	for( imod = st_modules ; imod ; imod = imod->next ){
		plugin = OFA_PLUGIN( imod->data );
		priv = ofa_plugin_get_instance_private( plugin );

		g_debug( "%s: objects=%p, count=%u",
				thisfn, ( void * ) priv->objects, g_list_length( priv->objects ));

		while( priv->objects ){
			iobj = priv->objects;
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
plugin_new( const gchar *fname, GApplication *application )
{
	ofaPlugin *plugin;
	ofaPluginPrivate *priv;

	plugin = g_object_new( OFA_TYPE_PLUGIN, NULL );
	priv = ofa_plugin_get_instance_private( plugin );
	priv->path = g_strdup( fname );

	if( !g_type_module_use( G_TYPE_MODULE( plugin )) || !is_an_ofa_plugin( plugin, application )){
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
	ofaPluginPrivate *priv;
	gboolean loaded;

	g_return_val_if_fail( G_IS_TYPE_PLUGIN( gmodule ), FALSE );

	g_debug( "%s: gmodule=%p", thisfn, ( void * ) gmodule );

	loaded = FALSE;
	plugin = OFA_PLUGIN( gmodule );
	priv = ofa_plugin_get_instance_private( plugin );
	priv->library = g_module_open( priv->path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL );

	if( !priv->library ){
		g_warning( "%s: g_module_open: path=%s, error=%s",
						thisfn, priv->path, g_module_error());
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
 * - ofa_extension_startup() and ofa_extension_list_types() are
 *   mandatory and MUST be implemented by the plugin; they are
 *   successively called for each plugin
 * - ofa_extension_shutdown() is optional and will be called on plugin
 *   shutdown if it exists.
 * - ofa_extension_get_api_version is optional, and defaults to 1.
 * - ofa_extension_get_version_number is optional, and defaults to NULL.
 */
static gboolean
is_an_ofa_plugin( ofaPlugin *plugin, GApplication *application )
{
	static const gchar *thisfn = "ofa_plugin_is_an_ofa_plugin";
	ofaPluginPrivate *priv;
	gboolean ok;

	priv = ofa_plugin_get_instance_private( plugin );

	ok =
		plugin_check( plugin, "ofa_extension_startup" , ( gpointer * ) &priv->startup ) &&
		plugin_check( plugin, "ofa_extension_list_types" , ( gpointer * ) &priv->list_types ) &&
		priv->startup( G_TYPE_MODULE( plugin ), application );

	if( ok ){
		g_debug( "%s: %s: ok", thisfn, priv->path );
	}

	return( ok );
}

static gboolean
plugin_check( ofaPlugin *plugin, const gchar *symbol, gpointer *pfn )
{
	static const gchar *thisfn = "ofa_plugin_plugin_check";
	ofaPluginPrivate *priv;
	gboolean ok;

	priv = ofa_plugin_get_instance_private( plugin );

	ok = g_module_symbol( priv->library, symbol, pfn );

	if( !ok ){
		g_debug("%s: %s: %s: symbol not found", thisfn, priv->path, symbol );
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
	ofaPluginPrivate *priv;
	const GType *types;
	guint count, i;

	priv = ofa_plugin_get_instance_private( plugin );

	count = priv->list_types( &types );
	priv->objects = NULL;

	for( i = 0 ; i < count ; i++ ){
		if( types[i] ){
			add_module_type( plugin, types[i] );
		}
	}
}

static void
add_module_type( ofaPlugin *plugin, GType type )
{
	ofaPluginPrivate *priv;
	GObject *object;

	priv = ofa_plugin_get_instance_private( plugin );

	object = g_object_new( type, NULL );
	g_debug( "ofa_plugin_add_module_type: allocating object=%p (%s)",
						( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_object_weak_ref( object, ( GWeakNotify ) on_object_finalized, plugin );

	priv->objects = g_list_prepend( priv->objects, object );
}

static void
on_object_finalized( ofaPlugin *plugin, GObject *finalized_object )
{
	static const gchar *thisfn = "ofa_plugin_object_finalized";
	ofaPluginPrivate *priv;

	g_debug( "%s: plugin=%p, finalized_object=%p (%s)",
			thisfn, ( void * ) plugin,
			( void * ) finalized_object, G_OBJECT_TYPE_NAME( finalized_object ));

	priv = ofa_plugin_get_instance_private( plugin );

	priv->objects = g_list_remove( priv->objects, finalized_object );

	g_debug( "%s: new objects list after remove is %p", thisfn, ( void * ) priv->objects );
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
	ofaPluginPrivate *priv;

	g_return_if_fail( gmodule && G_IS_TYPE_PLUGIN( gmodule ));

	g_debug( "%s: gmodule=%p", thisfn, ( void * ) gmodule );

	plugin = OFA_PLUGIN( gmodule );
	priv = ofa_plugin_get_instance_private( plugin );

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
	ofaPluginPrivate *priv;

	willing_to = NULL;

	for( im = st_modules; im ; im = im->next ){
		plugin = OFA_PLUGIN( im->data );
		priv = ofa_plugin_get_instance_private( plugin );

		for( io = priv->objects ; io ; io = io->next ){
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
 * ofa_plugin_get_object_for_type:
 * @plugin: this #ofaPlugin module
 * @type: the searched type.
 *
 * Returns: the #GObject which implements the requested @type, or %NULL.
 *
 * The returned reference is owned by the #ofaPlugin class, and should
 * not be released by the caller. The #ofaPlugin class makes sure that
 * the reference will be valid during the program execution.
 */
GObject *
ofa_plugin_get_object_for_type( const ofaPlugin *plugin, GType type )
{
	ofaPluginPrivate *priv;
	GList *io;

	g_return_val_if_fail( plugin && OFA_IS_PLUGIN( plugin ), NULL );

	priv = ofa_plugin_get_instance_private( plugin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	for( io = priv->objects ; io ; io = io->next ){
		if( G_TYPE_CHECK_INSTANCE_TYPE( G_OBJECT( io->data ), type )){
			return( G_OBJECT( io->data ));
		}
	}

	return( NULL );
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

	priv = ofa_plugin_get_instance_private( plugin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	for( io = priv->objects ; io ; io = io->next ){
		if( G_OBJECT( io->data ) == instance ){
			return( TRUE );
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

	priv = ofa_plugin_get_instance_private( plugin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( !priv->get_name ){
		plugin_check( plugin, "ofa_extension_get_name", ( gpointer * ) &priv->get_name );
	}
	if( priv->get_name ){
		return( priv->get_name());
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

	priv = ofa_plugin_get_instance_private( plugin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( !priv->get_version_number ){
		plugin_check( plugin, "ofa_extension_get_version_number", ( gpointer * ) &priv->get_version_number );
	}
	if( priv->get_version_number ){
		return( priv->get_version_number());
	}

	return( NULL );
}
