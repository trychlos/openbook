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

#include "my/my-file-monitor.h"
#include "my/my-isettings.h"
#include "my/my-utils.h"

#include "api/ofa-dossier-collection.h"
#include "api/ofa-extender-collection.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-settings.h"

/* private instance data
 */
typedef struct {
	gboolean       dispose_has_run;

	/* initialization
	 */
	ofaHub        *hub;

	/* runtime data
	 */
	myISettings   *settings;
	myFileMonitor *monitor;
	GList         *list;
	gboolean       ignore_next;
}
	ofaDossierCollectionPrivate;

#define DOSSIER_COLLECTION_SIGNAL_CHANGED       "changed"
#define DOSSIER_COLLECTION_DOSSIER_GROUP_PREFIX "Dossier "
#define DOSSIER_COLLECTION_PROVIDER_KEY         "ofa-DBMSProvider"

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void               setup_settings( ofaDossierCollection *self );
static void               on_settings_changed( myFileMonitor *monitor, const gchar *filename, ofaDossierCollection *self );
static GList             *load_dossiers( ofaDossierCollection *self, GList *previous_list );
static void               dossier_collection_free_list( ofaDossierCollection *self );
static ofaIDBDossierMeta *get_dossier_by_name( GList *list, const gchar *dossier_name );
static void               free_dossiers_list( GList *list );

G_DEFINE_TYPE_EXTENDED( ofaDossierCollection, ofa_dossier_collection, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaDossierCollection ))

static void
dossier_collection_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_collection_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_COLLECTION( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_collection_parent_class )->finalize( instance );
}

static void
dossier_collection_dispose( GObject *instance )
{
	ofaDossierCollectionPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_COLLECTION( instance ));

	priv = ofa_dossier_collection_get_instance_private( OFA_DOSSIER_COLLECTION( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->settings );
		g_clear_object( &priv->monitor );
		dossier_collection_free_list( OFA_DOSSIER_COLLECTION( instance ));
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_collection_parent_class )->dispose( instance );
}

static void
ofa_dossier_collection_init( ofaDossierCollection *self )
{
	static const gchar *thisfn = "ofa_dossier_collection_init";
	ofaDossierCollectionPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_COLLECTION( self ));

	priv = ofa_dossier_collection_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_dossier_collection_class_init( ofaDossierCollectionClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_collection_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_collection_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_collection_finalize;

	/**
	 * ofaDossierCollection::changed:
	 *
	 * This signal is sent when the dossiers collection has changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDossierCollection *collection,
	 * 						guint               count,
	 * 						gpointer            user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				DOSSIER_COLLECTION_SIGNAL_CHANGED,
				OFA_TYPE_DOSSIER_COLLECTION,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_UINT );
}

/**
 * ofa_dossier_collection_new:
 * @hub: the #ofaHub object of the application.
 *
 * Returns: a new reference to an #ofaDossierCollection object which should be
 * #g_object_unref() by the caller.
 */
ofaDossierCollection *
ofa_dossier_collection_new( ofaHub *hub )
{
	ofaDossierCollection *collection;
	ofaDossierCollectionPrivate *priv;

	collection = g_object_new( OFA_TYPE_DOSSIER_COLLECTION, NULL );

	priv = ofa_dossier_collection_get_instance_private( collection );

	priv->hub = hub;

	setup_settings( collection );

	return( collection );
}

static void
setup_settings( ofaDossierCollection *self )
{
	ofaDossierCollectionPrivate *priv;
	gchar *filename;

	priv = ofa_dossier_collection_get_instance_private( self );

	priv->settings = g_object_ref( ofa_settings_get_settings( SETTINGS_TARGET_DOSSIER ));

	filename = my_isettings_get_filename( priv->settings );
	priv->monitor = my_file_monitor_new( filename );
	g_free( filename );

	g_signal_connect( priv->monitor, "changed", G_CALLBACK( on_settings_changed ), self );
	on_settings_changed( priv->monitor, NULL, self );
}

/**
 * ofa_dossier_collection_get_list:
 * @collection: this #ofaDossierCollection instance.
 *
 * Returns: a list of defined dossiers as a #GList of GObject -derived
 * objects which implement the #ofaIDBDossierMeta interface.
 *
 * The returned list is owned by the @collection instance, and should
 * not be released by the caller.
 */
GList *
ofa_dossier_collection_get_list( ofaDossierCollection *collection )
{
	ofaDossierCollectionPrivate *priv;

	g_return_val_if_fail( collection && OFA_IS_DOSSIER_COLLECTION( collection ), NULL );

	priv = ofa_dossier_collection_get_instance_private( collection );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->list );
}

/*
 * filename: may be %NULL when the handler is directly called
 * (and typically just after signal connection)
 */
static void
on_settings_changed( myFileMonitor *monitor, const gchar *filename, ofaDossierCollection *collection )
{
	ofaDossierCollectionPrivate *priv;
	GList *prev_list;

	priv = ofa_dossier_collection_get_instance_private( collection );

	/* we ignore next update signal emitted by the monitor when we
	 * update the settings ourselves (so that the store may be
	 * synchronized without having to wait for the timeout) */
	if( priv->ignore_next ){
		priv->ignore_next = FALSE;

	} else {
		prev_list = priv->list;
		priv->list = load_dossiers( collection, prev_list );
		free_dossiers_list( prev_list );
		g_signal_emit_by_name( collection, DOSSIER_COLLECTION_SIGNAL_CHANGED, g_list_length( priv->list ));
	}
}

/*
 * @prev_list: the list before reloading the dossiers
 */
static GList *
load_dossiers( ofaDossierCollection *self, GList *prev_list )
{
	static const gchar *thisfn = "ofa_dossier_collection_load_dossiers";
	ofaDossierCollectionPrivate *priv;
	GList *outlist;
	GList *inlist, *it;
	gulong prefix_len;
	const gchar *cstr;
	gchar *dos_name, *prov_name;
	ofaIDBProvider *idbprovider;
	ofaIDBDossierMeta *meta;

	priv = ofa_dossier_collection_get_instance_private( self );

	outlist = NULL;
	prefix_len = my_strlen( DOSSIER_COLLECTION_DOSSIER_GROUP_PREFIX );
	inlist = my_isettings_get_groups( priv->settings );

	for( it=inlist ; it ; it=it->next ){
		cstr = ( const gchar * ) it->data;
		g_debug( "%s: group=%s", thisfn, cstr );
		if( !g_str_has_prefix( cstr, DOSSIER_COLLECTION_DOSSIER_GROUP_PREFIX )){
			continue;
		}
		dos_name = g_strstrip( g_strdup( cstr+prefix_len ));
		if( !my_strlen( dos_name )){
			g_info( "%s: found empty dossier name in group '%s', skipping", thisfn, cstr );
			continue;
		}
		meta = get_dossier_by_name( prev_list, dos_name );
		if( meta ){
			g_debug( "%s: dossier_name=%s already exists with meta=%p, reusing it",
					thisfn, dos_name, ( void * ) meta );
			g_object_ref( meta );
		} else {
			prov_name = my_isettings_get_string( priv->settings, cstr, DOSSIER_COLLECTION_PROVIDER_KEY );
			if( !my_strlen( prov_name )){
				g_info( "%s: found empty DBMS provider name in group '%s', skipping", thisfn, cstr );
				g_free( dos_name );
				continue;
			}
			g_debug( "%s: dossier_name=%s is new, provider=%s", thisfn, dos_name, prov_name );
			idbprovider = ofa_idbprovider_get_by_name( priv->hub, prov_name );
			if( idbprovider ){
				meta = ofa_idbprovider_new_dossier_meta( idbprovider );
				ofa_idbdossier_meta_set_dossier_name( meta, dos_name );
			}
			g_free( prov_name );
		}
		ofa_idbdossier_meta_set_from_settings( meta, MY_ISETTINGS( priv->settings ), cstr );
		ofa_idbdossier_meta_dump_rec( meta );
		outlist = g_list_prepend( outlist, meta );
		g_free( dos_name );
	}

	my_isettings_free_groups( inlist );

	return( g_list_reverse( outlist ));
}

/**
 * ofa_dossier_collection_get_count:
 * @collection: this #ofaDossierCollection instance.
 *
 * Returns: the count of loaded dossiers.
 */
guint
ofa_dossier_collection_get_count( ofaDossierCollection *collection )
{
	ofaDossierCollectionPrivate *priv;
	guint count;

	g_return_val_if_fail( collection && OFA_IS_DOSSIER_COLLECTION( collection ), 0 );

	priv = ofa_dossier_collection_get_instance_private( collection );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	count = g_list_length( priv->list );

	return( count );
}

/**
 * ofa_dossier_collection_get_by_name:
 * @collection: this #ofaDossierCollection instance.
 * @dossier_name: the named of the searched dossier.
 *
 * Returns: a new reference to the #ofaIDBDossierMeta instance which holds
 * the meta datas for the specified @dossier_name, or %NULL if not
 * found.
 *
 * The returned reference is owned by the @collection instance and
 * should not be released by the caller.
 */
ofaIDBDossierMeta *
ofa_dossier_collection_get_by_name( ofaDossierCollection *collection, const gchar *dossier_name )
{
	ofaDossierCollectionPrivate *priv;
	ofaIDBDossierMeta *meta;

	g_return_val_if_fail( collection && OFA_IS_DOSSIER_COLLECTION( collection ), NULL );

	priv = ofa_dossier_collection_get_instance_private( collection );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	meta = get_dossier_by_name( priv->list, dossier_name );

	return( meta );
}

/**
 * ofa_dossier_collection_set_meta_from_editor:
 * @collection: this #ofaDossierCollection instance.
 * @meta: the #ofaIDBDossierMeta to be set.
 * @editor: a #ofaIDBEditor instance which holds connection informations.
 *
 * Setup the @meta instance, writing informations to settings file.
 */
void
ofa_dossier_collection_set_meta_from_editor( ofaDossierCollection *collection, ofaIDBDossierMeta *meta, const ofaIDBEditor *editor )
{
	static const gchar *thisfn = "ofa_dossier_collection_set_meta_from_editor";
	ofaDossierCollectionPrivate *priv;
	gchar *group, *prov_name;
	ofaIDBProvider *prov_instance;
	const gchar *dossier_name;

	g_debug( "%s: collection=%p, meta=%p, editor=%p",
			thisfn, ( void * ) collection, ( void * ) meta, ( void * ) editor );

	g_return_if_fail( collection && OFA_IS_DOSSIER_COLLECTION( collection ));
	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));
	g_return_if_fail( editor && OFA_IS_IDBEDITOR( editor ));

	priv = ofa_dossier_collection_get_instance_private( collection );

	g_return_if_fail( !priv->dispose_has_run );

	dossier_name = ofa_idbdossier_meta_get_dossier_name( meta );
	group = g_strdup_printf( "%s%s", DOSSIER_COLLECTION_DOSSIER_GROUP_PREFIX, dossier_name );

	prov_instance = ofa_idbeditor_get_provider( editor );
	prov_name = ofa_idbprovider_get_canon_name( prov_instance );

	my_isettings_set_string( priv->settings, group, DOSSIER_COLLECTION_PROVIDER_KEY, prov_name );

	ofa_idbdossier_meta_set_from_editor( meta, editor, MY_ISETTINGS( priv->settings ), group );

	g_free( prov_name );
	g_object_unref( prov_instance );
	g_free( group );

	on_settings_changed( priv->monitor, NULL, collection );
	priv->ignore_next = TRUE;
}

/*
 * free the list of dossiers
 */
static void
dossier_collection_free_list( ofaDossierCollection *self )
{
	ofaDossierCollectionPrivate *priv;

	priv = ofa_dossier_collection_get_instance_private( self );

	free_dossiers_list( priv->list );
	priv->list = NULL;
}

/*
 * find the #ofaIDBDossierMeta by dossier name if exists
 */
static ofaIDBDossierMeta *
get_dossier_by_name( GList *list, const gchar *dossier_name )
{
	GList *it;
	ofaIDBDossierMeta *meta;
	const gchar *meta_name;

	for( it=list ; it ; it=it->next ){
		meta = ( ofaIDBDossierMeta * ) it->data;
		g_return_val_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ), NULL );
		meta_name = ofa_idbdossier_meta_get_dossier_name( meta );
		if( my_collate( meta_name, dossier_name ) == 0 ){
			return( meta );
		}
	}

	return( NULL );
}

/*
 * free the list of dossiers
 */
static void
free_dossiers_list( GList *list )
{
	g_list_free_full( list, ( GDestroyNotify ) g_object_unref );
}