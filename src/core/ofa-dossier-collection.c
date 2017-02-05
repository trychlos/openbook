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

#include <glib/gi18n.h>

#include "my/my-file-monitor.h"
#include "my/my-isettings.h"
#include "my/my-utils.h"

#include "api/ofa-dossier-collection.h"
#include "api/ofa-extender-collection.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-igetter.h"

/* private instance data
 */
typedef struct {
	gboolean       dispose_has_run;

	/* initialization
	 */
	ofaIGetter    *getter;

	/* runtime data
	 */
	myISettings   *dossier_settings;
	myFileMonitor *monitor;
	GList         *list;						/* list of #ofaIDBDossierMeta */
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
static void               set_dossier_meta_properties( ofaDossierCollection *self, ofaIDBDossierMeta *meta, const gchar *group_name );
static ofaIDBDossierMeta *get_dossier_by_name( GList *list, const gchar *dossier_name );
static void               collection_dump( ofaDossierCollection *collection, GList *list );

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
		g_clear_object( &priv->monitor );
		g_list_free_full( priv->list, ( GDestroyNotify ) g_object_unref );
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
 * @getter: a #ofaIGetter instance.
 *
 * Returns: a new reference to an #ofaDossierCollection object which should be
 * #g_object_unref() by the caller.
 */
ofaDossierCollection *
ofa_dossier_collection_new( ofaIGetter *getter )
{
	ofaDossierCollection *collection;
	ofaDossierCollectionPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collection = g_object_new( OFA_TYPE_DOSSIER_COLLECTION, NULL );

	priv = ofa_dossier_collection_get_instance_private( collection );

	priv->getter = getter;

	setup_settings( collection );
	priv->list = load_dossiers( collection, NULL );

	return( collection );
}

static void
setup_settings( ofaDossierCollection *self )
{
	ofaDossierCollectionPrivate *priv;
	gchar *filename;

	priv = ofa_dossier_collection_get_instance_private( self );

	priv->dossier_settings = ofa_igetter_get_dossier_settings( priv->getter );

	filename = my_isettings_get_filename( priv->dossier_settings );
	priv->monitor = my_file_monitor_new( filename );
	g_free( filename );

	g_signal_connect( priv->monitor, "changed", G_CALLBACK( on_settings_changed ), self );
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
	static const gchar *thisfn = "ofa_dossier_collection_on_settings_changed";
	ofaDossierCollectionPrivate *priv;
	GList *prev_list;

	g_debug( "%s: monitor=%p, filename=%s, collection=%p",
			thisfn, ( void * ) monitor, filename, ( void * ) collection );

	priv = ofa_dossier_collection_get_instance_private( collection );

	/* we ignore next update signal emitted by the monitor when we
	 * update the settings ourselves (so that the store may be
	 * synchronized without having to wait for the timeout) */
	if( priv->ignore_next ){
		g_debug( "%s: ignoring message", thisfn );
		priv->ignore_next = FALSE;

	} else {
		prev_list = priv->list;
		priv->list = load_dossiers( collection, prev_list );
		g_list_free_full( prev_list, ( GDestroyNotify ) g_object_unref );
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
	inlist = my_isettings_get_groups( priv->dossier_settings );

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
			prov_name = my_isettings_get_string( priv->dossier_settings, cstr, DOSSIER_COLLECTION_PROVIDER_KEY );
			if( !my_strlen( prov_name )){
				g_info( "%s: found empty DBMS provider name in group '%s', skipping", thisfn, cstr );
				g_free( dos_name );
				continue;
			}
			g_debug( "%s: dossier_name=%s is new, provider=%s", thisfn, dos_name, prov_name );
			idbprovider = ofa_idbprovider_get_by_name( priv->getter, prov_name );
			if( idbprovider ){
				meta = ofa_idbprovider_new_dossier_meta( idbprovider, dos_name );
				set_dossier_meta_properties( self, meta, cstr );
			} else {
				g_info( "%s: provider=%s not found", thisfn, prov_name );
				continue;
			}
			g_free( prov_name );
		}
		ofa_idbdossier_meta_set_from_settings( meta );
		outlist = g_list_prepend( outlist, meta );
		g_free( dos_name );
	}

	collection_dump( self, outlist );
	my_isettings_free_groups( priv->dossier_settings, inlist );
	g_signal_emit_by_name( self, DOSSIER_COLLECTION_SIGNAL_CHANGED, g_list_length( outlist ));

	return( g_list_reverse( outlist ));
}

static void
set_dossier_meta_properties( ofaDossierCollection *self, ofaIDBDossierMeta *meta, const gchar *group_name )
{
	ofaDossierCollectionPrivate *priv;

	priv = ofa_dossier_collection_get_instance_private( self );

	ofa_idbdossier_meta_set_settings_iface( meta, priv->dossier_settings );
	ofa_idbdossier_meta_set_settings_group( meta, group_name );
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
 * ofa_dossier_collection_add_meta:
 * @collection: this #ofaDossierCollection instance.
 * @meta: the #ofaIDBDossierMeta to be registered.
 *
 * Register the @meta informations in the dossier settings.
 */
void
ofa_dossier_collection_add_meta( ofaDossierCollection *collection, ofaIDBDossierMeta *meta )
{
	static const gchar *thisfn = "ofa_dossier_collection_add_meta";
	ofaDossierCollectionPrivate *priv;
	gchar *group, *prov_name;
	ofaIDBProvider *prov_instance;
	const gchar *dossier_name;

	g_debug( "%s: collection=%p, meta=%p",
			thisfn, ( void * ) collection, ( void * ) meta );

	g_return_if_fail( collection && OFA_IS_DOSSIER_COLLECTION( collection ));
	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));

	priv = ofa_dossier_collection_get_instance_private( collection );

	g_return_if_fail( !priv->dispose_has_run );

	priv->list = g_list_prepend( priv->list, meta );

	dossier_name = ofa_idbdossier_meta_get_dossier_name( meta );
	group = g_strdup_printf( "%s%s", DOSSIER_COLLECTION_DOSSIER_GROUP_PREFIX, dossier_name );
	set_dossier_meta_properties( collection, meta, group );

	prov_instance = ofa_idbdossier_meta_get_provider( meta );
	prov_name = ofa_idbprovider_get_canon_name( prov_instance );
	my_isettings_set_string( priv->dossier_settings, group, DOSSIER_COLLECTION_PROVIDER_KEY, prov_name );

	g_free( prov_name );
	g_free( group );
}

/*
 * find the #ofaIDBDossierMeta by dossier name if exists
 */
static ofaIDBDossierMeta *
get_dossier_by_name( GList *list, const gchar *dossier_name )
{
	GList *it;
	ofaIDBDossierMeta *meta;

	for( it=list ; it ; it=it->next ){
		meta = ( ofaIDBDossierMeta * ) it->data;
		g_return_val_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ), NULL );
		if( ofa_idbdossier_meta_compare_by_name( meta, dossier_name ) == 0 ){
			return( meta );
		}
	}

	return( NULL );
}

/**
 * ofa_dossier_collection_delete_period:
 * @collection: this #ofaDossierCollection instance.
 * @connect: a superuser connection to the DBMS;
 *  the #ofaIDBDossierMeta member is expected to be set to the target dossier;
 *  the #ofaIDBExerciceMeta member is ignored.
 * @period: [allow-none]: the financial period to be deleted;
 *  if %NULL, all existing periods will be deleted.
 * @delete_dossier_on_last: whether to also delete the dossier when empty.
 * @msgerr: [out][allow-none]: a placeholder for an error message
 *
 * Remove the @period informations from the @collection.
 * Delete the whole @period from the DBMS.
 * Update the dossier settings accordingly.
 *
 * This method does not release the @period (nor the dossier) object(s).
 * These objects will be automatically released on @collection automatic
 * update.
 *
 * This function is expected to be the entry point for all deletion
 * operations. Full code path is:
 *
 *   ofa_dossier_collection_delete_period()
 *     ofa_idbdossier_meta_delete_period()
 *       ofaIDBDossierMeta::delete_period()
 *         ofaMysqlDossierMeta::idbdossier_meta_delete_period()
 *       ofa_idbexercice_meta_delete()
 *         ofaIDBExerciceMeta::delete()
 *           ofaMysqlExerciceMeta::ofa_idbexercice_meta_delete()
 *             ofa_mysql_connect_drop_database()
 */
gboolean
ofa_dossier_collection_delete_period( ofaDossierCollection *collection,
		ofaIDBConnect *connect, ofaIDBExerciceMeta *period, gboolean delete_dossier_on_last, gchar **msgerr )
{
	static const gchar *thisfn = "ofa_dossier_collection_delete_period";
	ofaDossierCollectionPrivate *priv;
	ofaIDBDossierMeta *dossier_meta;
	gboolean ok;

	g_debug( "%s: collection=%p, connect=%p, period=%p, delete_dossier_on_last=%s, msgerr=%p",
			thisfn, ( void * ) collection, ( void * ) connect, ( void * ) period,
			delete_dossier_on_last ? "True":"False", ( void * ) msgerr );

	g_return_val_if_fail( collection && OFA_IS_DOSSIER_COLLECTION( collection ), FALSE );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );
	g_return_val_if_fail( !period || OFA_IS_IDBEXERCICE_META( period ), FALSE );

	priv = ofa_dossier_collection_get_instance_private( collection );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	if( msgerr ){
		*msgerr = NULL;
	}

	dossier_meta = ofa_idbconnect_get_dossier_meta( connect );
	if( !dossier_meta ){
		if( msgerr ){
			*msgerr = g_strdup( _( "The provided connection does not handle any dossier information" ));
			return( FALSE );
		}
	}
	g_return_val_if_fail( OFA_IS_IDBDOSSIER_META( dossier_meta ), FALSE );

	ok = ofa_idbdossier_meta_delete_period( dossier_meta, connect, period, delete_dossier_on_last, msgerr );

	return( ok );
}

/**
 * ofa_dossier_collection_dump:
 * @collection: this #ofaDossierCollection instance.
 *
 * Dump the collection.
 */
void
ofa_dossier_collection_dump( ofaDossierCollection *collection )
{
	ofaDossierCollectionPrivate *priv;

	g_return_if_fail( collection && OFA_IS_DOSSIER_COLLECTION( collection ));

	priv = ofa_dossier_collection_get_instance_private( collection );

	g_return_if_fail( !priv->dispose_has_run );

	collection_dump( collection, priv->list );
}

static void
collection_dump( ofaDossierCollection *collection, GList *list )
{
	GList *it;

	for( it=list ; it ; it=it->next ){
		ofa_idbdossier_meta_dump_full( OFA_IDBDOSSIER_META( it->data ));
	}
}
