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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/my-file-monitor.h"
#include "api/my-settings.h"
#include "api/my-utils.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-ifile-meta.h"

#include "ofa-file-dir.h"

/* private instance data
 */
struct _ofaFileDirPrivate {
	gboolean       dispose_has_run;

	/* runtime data
	 */
	mySettings    *settings;
	myFileMonitor *monitor;
	GList         *list;
};

#define FILE_DIR_SIGNAL_CHANGED         "changed"
#define FILE_DIR_DOSSIER_GROUP_PREFIX   "Dossier "
#define FILE_DIR_PROVIDER_KEY           "ofa-DBMSProvider"

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void          setup_settings( ofaFileDir *dir );
static void          on_settings_changed( myFileMonitor *monitor, const gchar *filename, ofaFileDir *dir );
static GList        *load_dossiers( ofaFileDir *dir, GList *previous_list );
static ofaIFileMeta *file_dir_get_meta( const gchar *dossier_name, GList *list );

G_DEFINE_TYPE( ofaFileDir, ofa_file_dir, G_TYPE_OBJECT )

static void
file_dir_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_file_dir_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_FILE_DIR( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_file_dir_parent_class )->finalize( instance );
}

static void
file_dir_dispose( GObject *instance )
{
	ofaFileDirPrivate *priv;

	g_return_if_fail( instance && OFA_IS_FILE_DIR( instance ));

	priv = OFA_FILE_DIR( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->settings );
		g_clear_object( &priv->monitor );
		ofa_file_dir_free_dossiers( priv->list );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_file_dir_parent_class )->dispose( instance );
}

static void
ofa_file_dir_init( ofaFileDir *self )
{
	static const gchar *thisfn = "ofa_file_dir_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_FILE_DIR( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_FILE_DIR, ofaFileDirPrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
ofa_file_dir_class_init( ofaFileDirClass *klass )
{
	static const gchar *thisfn = "ofa_file_dir_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = file_dir_dispose;
	G_OBJECT_CLASS( klass )->finalize = file_dir_finalize;

	g_type_class_add_private( klass, sizeof( ofaFileDirPrivate ));

	/**
	 * ofaFiledir::changed:
	 *
	 * This signal is sent when the content of the dossiers directory
	 * has changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDossierDir *dir,
	 * 						guint        count,
	 * 						gpointer     user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				FILE_DIR_SIGNAL_CHANGED,
				OFA_TYPE_FILE_DIR,
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
 * ofa_file_dir_new:
 *
 * Returns: a new reference to an #ofaFileDir object which should be
 * #g_object_unref() by the caller.
 */
ofaFileDir *
ofa_file_dir_new( void )
{
	ofaFileDir *dir;

	dir = g_object_new( OFA_TYPE_FILE_DIR, NULL );
	setup_settings( dir );

	return( dir );
}

static void
setup_settings( ofaFileDir *dir )
{
	ofaFileDirPrivate *priv;
	gchar *filename;

	priv = dir->priv;
	priv->settings = my_settings_new_user_config( "dossier.conf", "OFA_DOSSIER_CONF" );

	filename = my_settings_get_filename( priv->settings );
	priv->monitor = my_file_monitor_new( filename );
	g_free( filename );

	g_signal_connect( priv->monitor, "changed", G_CALLBACK( on_settings_changed ), dir );
	on_settings_changed( priv->monitor, NULL, dir );
}

/**
 * ofa_file_dir_get_dossiers:
 * @dir: this #ofaFileDir instance.
 *
 * Returns: a list of defined dossiers as a #GList of GObject -derived
 * objects which implement the #ofaIFileMeta interface.
 *
 * The returned list should be
 *  #g_list_free_full( <list>, ( GDestroyNotify ) g_object_unref ) by
 * the caller. The macro #ofa_file_dir_free_dossiers() may also be used.
 */
GList *
ofa_file_dir_get_dossiers( ofaFileDir *dir )
{
	ofaFileDirPrivate *priv;
	GList *list;

	g_return_val_if_fail( dir && OFA_IS_FILE_DIR( dir ), NULL );

	priv = dir->priv;

	if( !priv->dispose_has_run ){

		list = g_list_copy_deep( priv->list, ( GCopyFunc ) g_object_ref, NULL );

		return( list );
	}

	g_return_val_if_reached( NULL );
}

/*
 * filename: may be %NULL when the handler is directly called
 * (and typically just after signal connection)
 */
static void
on_settings_changed( myFileMonitor *monitor, const gchar *filename, ofaFileDir *dir )
{
	ofaFileDirPrivate *priv;
	GList *prev_list;

	priv = dir->priv;
	prev_list = priv->list;
	priv->list = load_dossiers( dir, prev_list );
	ofa_file_dir_free_dossiers( prev_list );

	g_signal_emit_by_name( dir, FILE_DIR_SIGNAL_CHANGED, g_list_length( priv->list ));
}

/*
 * @prev_list: the list before reloading the dossiers
 */
static GList *
load_dossiers( ofaFileDir *dir, GList *prev_list )
{
	static const gchar *thisfn = "ofa_file_dir_load_dossiers";
	ofaFileDirPrivate *priv;
	GList *outlist;
	GList *inlist, *it;
	gulong prefix_len;
	const gchar *cstr;
	gchar *dos_name, *prov_name;
	ofaIDBProvider *idbprovider;
	ofaIFileMeta *meta;

	priv = dir->priv;
	outlist = NULL;
	prefix_len = my_strlen( FILE_DIR_DOSSIER_GROUP_PREFIX );
	inlist = my_settings_get_groups( priv->settings );

	for( it=inlist ; it ; it=it->next ){
		cstr = ( const gchar * ) it->data;
		g_debug( "%s: group=%s", thisfn, cstr );
		if( !g_str_has_prefix( cstr, FILE_DIR_DOSSIER_GROUP_PREFIX )){
			continue;
		}
		dos_name = g_strstrip( g_strdup( cstr+prefix_len ));
		if( !my_strlen( dos_name )){
			g_warning( "%s: found empty dossier name in group '%s', skipping", thisfn, cstr );
			continue;
		}
		meta = file_dir_get_meta( dos_name, prev_list );
		if( meta ){
			g_debug( "%s: dossier_name=%s already exists with meta=%p, reusing it",
					thisfn, dos_name, ( void * ) meta );
		} else {
			prov_name = my_settings_get_string( priv->settings, cstr, FILE_DIR_PROVIDER_KEY );
			if( !my_strlen( prov_name )){
				g_warning( "%s: found empty DBMS provider name in group '%s', skipping", thisfn, cstr );
				g_free( dos_name );
				continue;
			}
			g_debug( "%s: dossier_name=%s is new, provider=%s", thisfn, dos_name, prov_name );
			idbprovider = ofa_idbprovider_get_instance_by_name( prov_name );
			meta = ofa_idbprovider_new_meta( idbprovider );
			ofa_ifile_meta_set_dossier_name( meta, dos_name );
			g_object_unref( idbprovider );
			g_free( prov_name );
		}
		ofa_ifile_meta_set_from_settings( meta, priv->settings, cstr );
		ofa_ifile_meta_dump_rec( meta );
		outlist = g_list_prepend( outlist, meta );
		g_free( dos_name );
	}

	my_settings_free_groups( inlist );

	return( g_list_reverse( outlist ));
}

/**
 * ofa_file_dir_get_dossiers_count:
 * @dir: this #ofaFileDir instance.
 *
 * Returns: the count of loaded dossiers.
 */
guint
ofa_file_dir_get_dossiers_count( const ofaFileDir *dir )
{
	ofaFileDirPrivate *priv;
	guint count;

	g_return_val_if_fail( dir && OFA_IS_FILE_DIR( dir ), 0 );

	priv = dir->priv;

	if( !priv->dispose_has_run ){

		count = g_list_length( priv->list );

		return( count );
	}

	g_return_val_if_reached( 0 );
}

/**
 * ofa_file_dir_get_meta:
 * @dir: this #ofaFileDir instance.
 * @dossier_name: the named of the searched dossier.
 *
 * Returns: a new reference to the #ofaIFileMeta instance which holds
 * the meta datas for the specified @dossier_name, or %NULL if not
 * found.
 *
 * The returned reference should be g_object_unref() by the caller.
 */
ofaIFileMeta *
ofa_file_dir_get_meta( const ofaFileDir *dir, const gchar *dossier_name )
{
	ofaFileDirPrivate *priv;
	ofaIFileMeta *meta;

	g_return_val_if_fail( dir && OFA_IS_FILE_DIR( dir ), NULL );

	priv = dir->priv;

	if( !priv->dispose_has_run ){

		meta = file_dir_get_meta( dossier_name, priv->list );

		return( meta );
	}

	g_return_val_if_reached( NULL );
}

static ofaIFileMeta *
file_dir_get_meta( const gchar *dossier_name, GList *list )
{
	GList *it;
	ofaIFileMeta *meta;
	gchar *meta_dos_name;
	gint cmp;

	for( it=list ; it ; it=it->next ){
		meta = ( ofaIFileMeta * ) it->data;
		g_return_val_if_fail( meta && OFA_IS_IFILE_META( meta ), NULL );
		meta_dos_name = ofa_ifile_meta_get_dossier_name( meta );
		cmp = g_utf8_collate( meta_dos_name, dossier_name );
		g_free( meta_dos_name );
		if( cmp == 0 ){
			return( g_object_ref( meta ));
		}
	}

	return( NULL );
}

/**
 * ofa_file_dir_set_meta_from_editor:
 * @dir: this #ofaFileDir instance.
 * @meta: the #ofaIFileMeta to be set.
 * @editor: a #ofaIDBEditor instance which holds connection informations.
 *
 * Setup the @meta instance, writing informations to settings file.
 */
void
ofa_file_dir_set_meta_from_editor( const ofaFileDir *dir, ofaIFileMeta *meta, const ofaIDBEditor *editor )
{
	static const gchar *thisfn = "ofa_file_dir_set_meta_from_editor";
	ofaFileDirPrivate *priv;
	gchar *group, *dossier_name;
	ofaIDBProvider *prov_instance;
	const gchar *prov_name;

	g_debug( "%s: dir=%p, meta=%p, editor=%p",
			thisfn, ( void * ) dir, ( void * ) meta, ( void * ) editor );

	g_return_if_fail( dir && OFA_IS_FILE_DIR( dir ));
	g_return_if_fail( meta && OFA_IS_IFILE_META( meta ));
	g_return_if_fail( editor && OFA_IS_IDBEDITOR( editor ));

	priv = dir->priv;

	if( !priv->dispose_has_run ){

		dossier_name = ofa_ifile_meta_get_dossier_name( meta );
		group = g_strdup_printf( "%s%s", FILE_DIR_DOSSIER_GROUP_PREFIX, dossier_name );
		prov_instance = ofa_idbeditor_get_provider( editor );
		prov_name = ofa_idbprovider_get_name( prov_instance );
		my_settings_set_string( priv->settings, group, FILE_DIR_PROVIDER_KEY, prov_name );

		ofa_ifile_meta_set_from_editor( meta, editor, priv->settings, group );

		g_object_unref( prov_instance );
		g_free( group );
		g_free( dossier_name );
		return;
	}

	g_return_if_reached();
}
