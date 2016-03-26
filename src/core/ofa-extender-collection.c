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

#include "api/ofa-extender-collection.h"
#include "api/ofa-extender-module.h"

/* private instance data
 */
typedef struct {
	gboolean      dispose_has_run;

	/* initialization
	 */
	GApplication *application;
	gchar        *extension_dir;

	/* runtime
	 */
	GList        *modules;
}
	ofaExtenderCollectionPrivate;

#define EXTENDER_COLLECTION_SUFFIX      ".so"

static GList *load_modules( ofaExtenderCollection *self );

G_DEFINE_TYPE_EXTENDED( ofaExtenderCollection, ofa_extender_collection, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaExtenderCollection ))

static void
extender_collection_finalize( GObject *object )
{
	static const gchar *thisfn = "ofa_extender_collection_finalize";
	ofaExtenderCollectionPrivate *priv;

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_return_if_fail( object && OFA_IS_EXTENDER_COLLECTION( object ));

	/* free data members here */
	priv = ofa_extender_collection_get_instance_private( OFA_EXTENDER_COLLECTION( object ));

	g_free( priv->extension_dir );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_extender_collection_parent_class )->finalize( object );
}

static void
extender_collection_dispose( GObject *object )
{
	ofaExtenderCollectionPrivate *priv;

	g_return_if_fail( object && OFA_IS_EXTENDER_COLLECTION( object ));

	priv = ofa_extender_collection_get_instance_private( OFA_EXTENDER_COLLECTION( object ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_extender_collection_parent_class )->dispose( object );
}

static void
ofa_extender_collection_init( ofaExtenderCollection *self )
{
	static const gchar *thisfn = "ofa_extender_collection_init";
	ofaExtenderCollectionPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXTENDER_COLLECTION( self ));

	priv = ofa_extender_collection_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_extender_collection_class_init( ofaExtenderCollectionClass *klass )
{
	static const gchar *thisfn = "ofa_extender_collection_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = extender_collection_dispose;
	G_OBJECT_CLASS( klass )->finalize = extender_collection_finalize;
}

/**
 * ofa_extender_collection_new:
 * @application: the #GApplication caller.
 * @extension_dir: the path of the directory where the extension modules
 *  are to be loaded from.
 *
 * Returns: the collection of loaded modules.
 */
ofaExtenderCollection *
ofa_extender_collection_new( GApplication *application, const gchar *extension_dir )
{
	ofaExtenderCollection *collection;
	ofaExtenderCollectionPrivate *priv;

	collection = g_object_new( OFA_TYPE_EXTENDER_COLLECTION, NULL );

	priv = ofa_extender_collection_get_instance_private( collection );

	priv->application = application;
	priv->extension_dir = g_strdup( extension_dir );
	priv->modules = load_modules( collection );

	return( collection );
}

static GList *
load_modules( ofaExtenderCollection *self )
{
	static const gchar *thisfn = "ofa_extender_collection_load_modules";
	ofaExtenderCollectionPrivate *priv;
	GList *plugins;
	GDir *api_dir;
	GError *error;
	const gchar *entry;
	gchar *fname;
	ofaExtenderModule *plugin;

	priv = ofa_extender_collection_get_instance_private( self );

	plugins = NULL;
	error = NULL;

	api_dir = g_dir_open( priv->extension_dir, 0, &error );
	if( error ){
		g_warning( "%s: g_dir_open: %s", thisfn, error->message );
		g_error_free( error );
		return( NULL );
	}

	while(( entry = g_dir_read_name( api_dir )) != NULL ){
		if( g_str_has_suffix( entry, EXTENDER_COLLECTION_SUFFIX )){
			fname = g_build_filename( priv->extension_dir, entry, NULL );

			plugin = ofa_extender_module_new( priv->application, fname );

			if( plugin ){
				plugins = g_list_append( plugins, plugin );
				g_debug( "%s: module %s successfully loaded", thisfn, entry );
			}
			g_free( fname );
		}
	}
	g_dir_close( api_dir );

	return( plugins );
}

/*
 * ofa_extender_collection_get_for_type:
 * @collection: this #ofaExtenderCollection instance.
 * @type: the serched GType.
 *
 * Returns: a list of objects instanciated by loaded modules which are
 *  willing to deal with requested @type.
 *
 * The returned list should be #ofa_extender_collection_free_types() by
 * the caller.
 */
GList *
ofa_extender_collection_get_for_type( ofaExtenderCollection *collection, GType type )
{
	ofaExtenderCollectionPrivate *priv;
	GList *willing_to, *it, *list;

	g_return_val_if_fail( collection && OFA_IS_EXTENDER_COLLECTION( collection ), NULL );

	priv = ofa_extender_collection_get_instance_private( collection );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	willing_to = NULL;

	for( it=priv->modules ; it ; it=it->next ){
		list = ofa_extender_module_get_for_type( OFA_EXTENDER_MODULE( it->data ), type );
		if( list ){
			willing_to = g_list_concat( willing_to, list );
		}
	}

	return( willing_to );
}

/*
 * ofa_extender_collection_free_types:
 * @list: a #GList as returned by #ofa_extender_collection_get_for_type().
 *
 * Free the previously returned list.
 */
void
ofa_extender_collection_free_types( GList *list )
{
	g_list_foreach( list, ( GFunc ) g_object_unref, NULL );
	g_list_free( list );
}

/*
 * ofa_extender_collection_get_modules:
 * @collection: this #ofaExtenderCollection instance.
 *
 * Returns: the list of currently loaded #ofaExtenderModule objects.
 *
 * The returned list is owned by the @collection instance, and should
 * not be modified nor freed.
 */
const GList *
ofa_extender_collection_get_modules( const ofaExtenderCollection *collection )
{
	ofaExtenderCollectionPrivate *priv;

	g_return_val_if_fail( collection && OFA_IS_EXTENDER_COLLECTION( collection ), NULL );

	priv = ofa_extender_collection_get_instance_private( collection );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const GList * ) priv->modules );
}
