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

#include "my/my-file-monitor.h"
#include "my/my-isettings.h"
#include "my/my-utils.h"

#include "api/ofa-extender-collection.h"
#include "api/ofa-portfolio-collection.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-idbmeta.h"
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
	ofaPortfolioCollectionPrivate;

#define PORTFOLIO_COLLECTION_SIGNAL_CHANGED       "changed"
#define PORTFOLIO_COLLECTION_DOSSIER_GROUP_PREFIX "Dossier "
#define PORTFOLIO_COLLECTION_PROVIDER_KEY         "ofa-DBMSProvider"

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void        setup_settings( ofaPortfolioCollection *dir );
static void        on_settings_changed( myFileMonitor *monitor, const gchar *filename, ofaPortfolioCollection *dir );
static GList      *load_dossiers( ofaPortfolioCollection *dir, GList *previous_list );
static ofaIDBMeta *file_dir_get_meta( const gchar *dossier_name, GList *list );

G_DEFINE_TYPE_EXTENDED( ofaPortfolioCollection, ofa_portfolio_collection, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaPortfolioCollection ))

static void
portfolio_collection_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_portfolio_collection_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PORTFOLIO_COLLECTION( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_portfolio_collection_parent_class )->finalize( instance );
}

static void
portfolio_collection_dispose( GObject *instance )
{
	ofaPortfolioCollectionPrivate *priv;

	g_return_if_fail( instance && OFA_IS_PORTFOLIO_COLLECTION( instance ));

	priv = ofa_portfolio_collection_get_instance_private( OFA_PORTFOLIO_COLLECTION( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->settings );
		g_clear_object( &priv->monitor );
		ofa_portfolio_collection_free_dossiers( priv->list );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_portfolio_collection_parent_class )->dispose( instance );
}

static void
ofa_portfolio_collection_init( ofaPortfolioCollection *self )
{
	static const gchar *thisfn = "ofa_portfolio_collection_init";
	ofaPortfolioCollectionPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PORTFOLIO_COLLECTION( self ));

	priv = ofa_portfolio_collection_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_portfolio_collection_class_init( ofaPortfolioCollectionClass *klass )
{
	static const gchar *thisfn = "ofa_portfolio_collection_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = portfolio_collection_dispose;
	G_OBJECT_CLASS( klass )->finalize = portfolio_collection_finalize;

	/**
	 * ofaFiledir::changed:
	 *
	 * This signal is sent when the content of the dossiers directory
	 * has changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDossierDir *dir,
	 * 						guint        count,
	 * 						const gchar *filename,
	 * 						gpointer     user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				PORTFOLIO_COLLECTION_SIGNAL_CHANGED,
				OFA_TYPE_PORTFOLIO_COLLECTION,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_UINT, G_TYPE_STRING );
}

/**
 * ofa_portfolio_collection_new:
 * @hub: the #ofaHub object of the application.
 *
 * Returns: a new reference to an #ofaPortfolioCollection object which should be
 * #g_object_unref() by the caller.
 */
ofaPortfolioCollection *
ofa_portfolio_collection_new( ofaHub *hub )
{
	ofaPortfolioCollection *dir;
	ofaPortfolioCollectionPrivate *priv;

	dir = g_object_new( OFA_TYPE_PORTFOLIO_COLLECTION, NULL );

	priv = ofa_portfolio_collection_get_instance_private( dir );

	priv->hub = hub;

	setup_settings( dir );

	return( dir );
}

static void
setup_settings( ofaPortfolioCollection *dir )
{
	ofaPortfolioCollectionPrivate *priv;
	gchar *filename;

	priv = ofa_portfolio_collection_get_instance_private( dir );

	priv->settings = g_object_ref( ofa_settings_get_settings( SETTINGS_TARGET_DOSSIER ));

	filename = my_isettings_get_filename( priv->settings );
	priv->monitor = my_file_monitor_new( filename );
	g_free( filename );

	g_signal_connect( priv->monitor, "changed", G_CALLBACK( on_settings_changed ), dir );
	on_settings_changed( priv->monitor, NULL, dir );
}

/**
 * ofa_portfolio_collection_get_dossiers:
 * @dir: this #ofaPortfolioCollection instance.
 *
 * Returns: a list of defined dossiers as a #GList of GObject -derived
 * objects which implement the #ofaIDBMeta interface.
 *
 * The returned list should be
 *  #g_list_free_full( <list>, ( GDestroyNotify ) g_object_unref ) by
 * the caller. The macro #ofa_portfolio_collection_free_dossiers() may also be used.
 */
GList *
ofa_portfolio_collection_get_dossiers( ofaPortfolioCollection *dir )
{
	ofaPortfolioCollectionPrivate *priv;
	GList *list;

	g_return_val_if_fail( dir && OFA_IS_PORTFOLIO_COLLECTION( dir ), NULL );

	priv = ofa_portfolio_collection_get_instance_private( dir );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	list = g_list_copy_deep( priv->list, ( GCopyFunc ) g_object_ref, NULL );

	return( list );
}

/*
 * filename: may be %NULL when the handler is directly called
 * (and typically just after signal connection)
 */
static void
on_settings_changed( myFileMonitor *monitor, const gchar *filename, ofaPortfolioCollection *dir )
{
	ofaPortfolioCollectionPrivate *priv;
	GList *prev_list;
	gchar *fname;

	priv = ofa_portfolio_collection_get_instance_private( dir );

	/* we ignore next update signal emitted by the monitor when we
	 * update the settings ourselves (so that the store may be
	 * synchronized without having to wait for the timeout) */
	if( priv->ignore_next ){
		priv->ignore_next = FALSE;

	} else {
		prev_list = priv->list;
		priv->list = load_dossiers( dir, prev_list );
		ofa_portfolio_collection_free_dossiers( prev_list );
		fname = my_isettings_get_filename( priv->settings );
		g_signal_emit_by_name( dir, PORTFOLIO_COLLECTION_SIGNAL_CHANGED, g_list_length( priv->list ), fname );
		g_free( fname );
	}
}

/*
 * @prev_list: the list before reloading the dossiers
 */
static GList *
load_dossiers( ofaPortfolioCollection *dir, GList *prev_list )
{
	static const gchar *thisfn = "ofa_portfolio_collection_load_dossiers";
	ofaPortfolioCollectionPrivate *priv;
	GList *outlist;
	GList *inlist, *it;
	gulong prefix_len;
	const gchar *cstr;
	gchar *dos_name, *prov_name;
	ofaIDBProvider *idbprovider;
	ofaIDBMeta *meta;

	priv = ofa_portfolio_collection_get_instance_private( dir );

	outlist = NULL;
	prefix_len = my_strlen( PORTFOLIO_COLLECTION_DOSSIER_GROUP_PREFIX );
	inlist = my_isettings_get_groups( priv->settings );

	for( it=inlist ; it ; it=it->next ){
		cstr = ( const gchar * ) it->data;
		g_debug( "%s: group=%s", thisfn, cstr );
		if( !g_str_has_prefix( cstr, PORTFOLIO_COLLECTION_DOSSIER_GROUP_PREFIX )){
			continue;
		}
		dos_name = g_strstrip( g_strdup( cstr+prefix_len ));
		if( !my_strlen( dos_name )){
			g_info( "%s: found empty dossier name in group '%s', skipping", thisfn, cstr );
			continue;
		}
		meta = file_dir_get_meta( dos_name, prev_list );
		if( meta ){
			g_debug( "%s: dossier_name=%s already exists with meta=%p, reusing it",
					thisfn, dos_name, ( void * ) meta );
		} else {
			prov_name = my_isettings_get_string( priv->settings, cstr, PORTFOLIO_COLLECTION_PROVIDER_KEY );
			if( !my_strlen( prov_name )){
				g_info( "%s: found empty DBMS provider name in group '%s', skipping", thisfn, cstr );
				g_free( dos_name );
				continue;
			}
			g_debug( "%s: dossier_name=%s is new, provider=%s", thisfn, dos_name, prov_name );
			idbprovider = ofa_idbprovider_get_by_name( priv->hub, prov_name );
			if( idbprovider ){
				meta = ofa_idbprovider_new_meta( idbprovider );
				ofa_idbmeta_set_dossier_name( meta, dos_name );
			}
			g_free( prov_name );
		}
		ofa_idbmeta_set_from_settings( meta, MY_ISETTINGS( priv->settings ), cstr );
		ofa_idbmeta_dump_rec( meta );
		outlist = g_list_prepend( outlist, meta );
		g_free( dos_name );
	}

	my_isettings_free_groups( inlist );

	return( g_list_reverse( outlist ));
}

/**
 * ofa_portfolio_collection_get_dossiers_count:
 * @dir: this #ofaPortfolioCollection instance.
 *
 * Returns: the count of loaded dossiers.
 */
guint
ofa_portfolio_collection_get_dossiers_count( ofaPortfolioCollection *dir )
{
	ofaPortfolioCollectionPrivate *priv;
	guint count;

	g_return_val_if_fail( dir && OFA_IS_PORTFOLIO_COLLECTION( dir ), 0 );

	priv = ofa_portfolio_collection_get_instance_private( dir );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	count = g_list_length( priv->list );

	return( count );
}

/**
 * ofa_portfolio_collection_get_meta:
 * @dir: this #ofaPortfolioCollection instance.
 * @dossier_name: the named of the searched dossier.
 *
 * Returns: a new reference to the #ofaIDBMeta instance which holds
 * the meta datas for the specified @dossier_name, or %NULL if not
 * found.
 *
 * The returned reference should be g_object_unref() by the caller.
 */
ofaIDBMeta *
ofa_portfolio_collection_get_meta( ofaPortfolioCollection *dir, const gchar *dossier_name )
{
	ofaPortfolioCollectionPrivate *priv;
	ofaIDBMeta *meta;

	g_return_val_if_fail( dir && OFA_IS_PORTFOLIO_COLLECTION( dir ), NULL );

	priv = ofa_portfolio_collection_get_instance_private( dir );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	meta = file_dir_get_meta( dossier_name, priv->list );

	return( meta );
}

static ofaIDBMeta *
file_dir_get_meta( const gchar *dossier_name, GList *list )
{
	GList *it;
	ofaIDBMeta *meta;
	gchar *meta_dos_name;
	gint cmp;

	for( it=list ; it ; it=it->next ){
		meta = ( ofaIDBMeta * ) it->data;
		g_return_val_if_fail( meta && OFA_IS_IDBMETA( meta ), NULL );
		meta_dos_name = ofa_idbmeta_get_dossier_name( meta );
		cmp = g_utf8_collate( meta_dos_name, dossier_name );
		g_free( meta_dos_name );
		if( cmp == 0 ){
			return( g_object_ref( meta ));
		}
	}

	return( NULL );
}

/**
 * ofa_portfolio_collection_set_meta_from_editor:
 * @dir: this #ofaPortfolioCollection instance.
 * @meta: the #ofaIDBMeta to be set.
 * @editor: a #ofaIDBEditor instance which holds connection informations.
 *
 * Setup the @meta instance, writing informations to settings file.
 */
void
ofa_portfolio_collection_set_meta_from_editor( ofaPortfolioCollection *dir, ofaIDBMeta *meta, const ofaIDBEditor *editor )
{
	static const gchar *thisfn = "ofa_portfolio_collection_set_meta_from_editor";
	ofaPortfolioCollectionPrivate *priv;
	gchar *group, *dossier_name;
	ofaIDBProvider *prov_instance;
	gchar *prov_name;

	g_debug( "%s: dir=%p, meta=%p, editor=%p",
			thisfn, ( void * ) dir, ( void * ) meta, ( void * ) editor );

	g_return_if_fail( dir && OFA_IS_PORTFOLIO_COLLECTION( dir ));
	g_return_if_fail( meta && OFA_IS_IDBMETA( meta ));
	g_return_if_fail( editor && OFA_IS_IDBEDITOR( editor ));

	priv = ofa_portfolio_collection_get_instance_private( dir );

	g_return_if_fail( !priv->dispose_has_run );

	dossier_name = ofa_idbmeta_get_dossier_name( meta );
	group = g_strdup_printf( "%s%s", PORTFOLIO_COLLECTION_DOSSIER_GROUP_PREFIX, dossier_name );

	prov_instance = ofa_idbeditor_get_provider( editor );
	prov_name = ofa_idbprovider_get_canon_name( prov_instance );

	my_isettings_set_string( priv->settings, group, PORTFOLIO_COLLECTION_PROVIDER_KEY, prov_name );

	ofa_idbmeta_set_from_editor( meta, editor, MY_ISETTINGS( priv->settings ), group );

	g_free( prov_name );
	g_object_unref( prov_instance );
	g_free( group );
	g_free( dossier_name );

	on_settings_changed( priv->monitor, NULL, dir );
	priv->ignore_next = TRUE;
}
