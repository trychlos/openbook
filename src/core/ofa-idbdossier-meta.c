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

#include <glib/gi18n.h>

#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"

/* some data attached to each ofaIDBDossierMeta instance
 * we store here the data provided by the application
 * which do not depend of a specific implementation
 */
typedef struct {

	/* initialization
	 */
	ofaIDBProvider *provider;
	ofaHub         *hub;
	gchar          *dossier_name;

	/* runtime
	 */
	myISettings    *settings;
	gchar          *group_name;
	GList          *periods;
}
	sIDBMeta;

#define IDBDOSSIER_META_LAST_VERSION      1
#define IDBDOSSIER_META_DATA             "idbdossier-meta-data"

static guint st_initializations         = 0;	/* interface initialization count */

static GType     register_type( void );
static void      interface_base_init( ofaIDBDossierMetaInterface *klass );
static void      interface_base_finalize( ofaIDBDossierMetaInterface *klass );
static sIDBMeta *get_idbdossier_meta_data( const ofaIDBDossierMeta *meta );
static void      on_meta_finalized( sIDBMeta *data, GObject *finalized_meta );

/**
 * ofa_idbdossier_meta_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_idbdossier_meta_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_idbdossier_meta_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_idbdossier_meta_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIDBDossierMetaInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIDBDossierMeta", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIDBDossierMetaInterface *klass )
{
	static const gchar *thisfn = "ofa_idbdossier_meta_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIDBDossierMetaInterface *klass )
{
	static const gchar *thisfn = "ofa_idbdossier_meta_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_idbdossier_meta_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_idbdossier_meta_get_interface_last_version( void )
{
	return( IDBDOSSIER_META_LAST_VERSION );
}

/**
 * ofa_idbdossier_meta_get_interface_version:
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
ofa_idbdossier_meta_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IDBDOSSIER_META );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIDBDossierMetaInterface * ) iface )->get_interface_version ){
		version = (( ofaIDBDossierMetaInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIDBDossierMeta::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_idbdossier_meta_get_provider:
 * @meta: this #ofaIDBDossierMeta instance.
 *
 * Returns: a new reference to the provider instance which should be
 * g_object_unref() by the caller.
 */
ofaIDBProvider *
ofa_idbdossier_meta_get_provider( const ofaIDBDossierMeta *meta )
{
	sIDBMeta *data;

	g_return_val_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ), NULL );

	data = get_idbdossier_meta_data( meta );

	return( g_object_ref( data->provider ));
}

/**
 * ofa_idbdossier_meta_set_provider:
 * @meta: this #ofaIDBDossierMeta instance.
 * @instance: the #ofaIDBProvider which manages the dossier.
 *
 * The interface takes a reference on the @instance object, to make
 * sure it stays available. This reference will be automatically
 * released on @meta finalization.
 */
void
ofa_idbdossier_meta_set_provider( ofaIDBDossierMeta *meta, const ofaIDBProvider *instance )
{
	sIDBMeta *data;

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));
	g_return_if_fail( instance && OFA_IS_IDBPROVIDER( instance ));

	data = get_idbdossier_meta_data( meta );

	g_clear_object( &data->provider );
	data->provider = g_object_ref(( gpointer ) instance );
}

/**
 * ofa_idbdossier_meta_get_hub:
 * @meta: this #ofaIDBDossierMeta instance.
 *
 * Returns: the #ofaHub object of the application if it has been
 * previously set.
 *
 * The returned reference is owned by the @meta instance, and should not
 * be released by the caller.
 */
ofaHub *
ofa_idbdossier_meta_get_hub( const ofaIDBDossierMeta *meta )
{
	sIDBMeta *data;

	g_return_val_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ), NULL );

	data = get_idbdossier_meta_data( meta );

	return( data->hub );
}

/**
 * ofa_idbdossier_meta_set_hub:
 * @meta: this #ofaIDBDossierMeta instance.
 * @hub: the #ofaHub object of the application.
 *
 * Set the @hub object.
 */
void
ofa_idbdossier_meta_set_hub( ofaIDBDossierMeta *meta, ofaHub *hub )
{
	sIDBMeta *data;

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	data = get_idbdossier_meta_data( meta );

	data->hub = hub;
}

/**
 * ofa_idbdossier_meta_get_dossier_name:
 * @meta: this #ofaIDBDossierMeta instance.
 *
 * Returns: the identifier name of the dossier.
 *
 * The returned string is owned by the @meta instance, and should not
 * be released by the caller.
 */
const gchar *
ofa_idbdossier_meta_get_dossier_name( const ofaIDBDossierMeta *meta )
{
	sIDBMeta *data;

	g_return_val_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ), NULL );

	data = get_idbdossier_meta_data( meta );
	return(( const gchar * ) data->dossier_name );
}

/**
 * ofa_idbdossier_meta_set_dossier_name:
 * @meta: this #ofaIDBDossierMeta instance.
 * @dossier_name: the name of the dossier.
 *
 * Stores the name of the dossier as an interface data.
 */
void
ofa_idbdossier_meta_set_dossier_name( ofaIDBDossierMeta *meta, const gchar *dossier_name )
{
	sIDBMeta *data;

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));

	data = get_idbdossier_meta_data( meta );
	g_free( data->dossier_name );
	data->dossier_name = g_strdup( dossier_name );
}

/**
 * ofa_idbdossier_meta_get_settings:
 * @meta: this #ofaIDBDossierMeta instance.
 *
 * Returns: the #myISettings object.
 *
 * The returned reference is owned by the interface, and should
 * not be freed by the caller.
 */
myISettings *
ofa_idbdossier_meta_get_settings( const ofaIDBDossierMeta *meta )
{
	sIDBMeta *data;

	g_return_val_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ), NULL );

	data = get_idbdossier_meta_data( meta );

	return( data->settings );
}

/**
 * ofa_idbdossier_meta_get_group_name:
 * @meta: this #ofaIDBDossierMeta instance.
 *
 * Returns: the name of the group which holds all dossier informations
 * in the settings file, as a newly allocated string which should be
 * g_free() by the caller.
 */
gchar *
ofa_idbdossier_meta_get_group_name( const ofaIDBDossierMeta *meta )
{
	sIDBMeta *data;

	g_return_val_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ), NULL );

	data = get_idbdossier_meta_data( meta );

	return( g_strdup( data->group_name ));
}

/**
 * ofa_idbdossier_meta_set_from_settings:
 * @meta: this #ofaIDBDossierMeta instance.
 * @settings: the #myISettings which manages the dossier settings file.
 * @group_name: the group name for the dossier.
 */
void
ofa_idbdossier_meta_set_from_settings( ofaIDBDossierMeta *meta, myISettings *settings, const gchar *group_name )
{
	static const gchar *thisfn = "ofa_idbdossier_meta_set_from_settings";
	sIDBMeta *data;

	g_debug( "%s: meta=%p, settings=%p, group_name=%s",
			thisfn, ( void * ) meta, ( void * ) settings, group_name );

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));
	g_return_if_fail( settings && MY_IS_ISETTINGS( settings ));

	data = get_idbdossier_meta_data( meta );
	g_clear_object( &data->settings );
	data->settings = g_object_ref( settings );
	g_free( data->group_name );
	data->group_name = g_strdup( group_name );

	if( OFA_IDBDOSSIER_META_GET_INTERFACE( meta )->set_from_settings ){
		OFA_IDBDOSSIER_META_GET_INTERFACE( meta )->set_from_settings( meta, settings, group_name );
		return;
	}

	g_info( "%s: ofaIDBDossierMeta's %s implementation does not provide 'set_from_settings()' method",
			thisfn, G_OBJECT_TYPE_NAME( meta ));
}

/**
 * ofa_idbdossier_meta_set_from_editor:
 * @meta: this #ofaIDBDossierMeta instance.
 * @editor: the #ofaIDBEditor which handles the connection information.
 * @settings: the #myISettings which manages the dossier settings file.
 * @group_name: the group name for the dossier.
 */
void
ofa_idbdossier_meta_set_from_editor( ofaIDBDossierMeta *meta, const ofaIDBEditor *editor, myISettings *settings, const gchar *group_name )
{
	static const gchar *thisfn = "ofa_idbdossier_meta_set_from_editor";
	sIDBMeta *data;

	g_debug( "%s: meta=%p, editor=%p, settings=%p, group_name=%s",
			thisfn, ( void * ) meta, ( void * ) editor, ( void * ) settings, group_name );

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));
	g_return_if_fail( editor && OFA_IS_IDBEDITOR( editor ));
	g_return_if_fail( settings && MY_IS_ISETTINGS( settings ));

	data = get_idbdossier_meta_data( meta );
	g_clear_object( &data->settings );
	data->settings = g_object_ref( settings );
	g_free( data->group_name );
	data->group_name = g_strdup( group_name );

	if( OFA_IDBDOSSIER_META_GET_INTERFACE( meta )->set_from_editor ){
		OFA_IDBDOSSIER_META_GET_INTERFACE( meta )->set_from_editor( meta, editor, settings, group_name );
		return;
	}

	g_info( "%s: ofaIDBDossierMeta's %s implementation does not provide 'set_from_editor()' method",
			thisfn, G_OBJECT_TYPE_NAME( meta ));
}

/**
 * ofa_idbdossier_meta_remove_meta:
 * @meta: this #ofaIDBDossierMeta instance.
 *
 * Remove @meta from the dossier settings file.
 *
 * The #ofaIDBDossierMeta object itself will be finalized on automatic update
 * of the dossiers collection.
 */
void
ofa_idbdossier_meta_remove_meta( ofaIDBDossierMeta *meta )
{
	sIDBMeta *data;

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));

	data = get_idbdossier_meta_data( meta );
	my_isettings_remove_group( data->settings, data->group_name );
}

/**
 * ofa_idbdossier_meta_get_periods:
 * @meta: this #ofaIDBDossierMeta instance.
 *
 * Returns: a list of defined financial periods (exercices) for this
 * file (dossier), as a #GList of #ofaIDBExerciceMeta object, which should
 * be #ofa_idbdossier_meta_free_periods() by the caller.
 */
GList *
ofa_idbdossier_meta_get_periods( const ofaIDBDossierMeta *meta )
{
	sIDBMeta *data;

	g_return_val_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ), NULL );

	data = get_idbdossier_meta_data( meta );
	return( g_list_copy_deep( data->periods, ( GCopyFunc ) g_object_ref, NULL ));
}

/**
 * ofa_idbdossier_meta_set_periods:
 * @meta: this #ofaIDBDossierMeta instance.
 * @list: the list of the periods for the dossier.
 *
 * Stores the list of the defined financial periods (exercices) of the
 * dossier, as a deep copy of the provided @list.
 */
void
ofa_idbdossier_meta_set_periods( ofaIDBDossierMeta *meta, GList *periods )
{
	sIDBMeta *data;
	GList *it;
	ofaIDBExerciceMeta *exercice_meta;

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));

	data = get_idbdossier_meta_data( meta );

	ofa_idbdossier_meta_free_periods( data->periods );
	data->periods = g_list_copy_deep( periods, ( GCopyFunc ) g_object_ref, NULL );

	for( it=data->periods ; it ; it=it->next ){
		exercice_meta = OFA_IDBEXERCICE_META( it->data );
		ofa_idbexercice_meta_set_hub( exercice_meta, data->hub );
	}
}

/**
 * ofa_idbdossier_meta_add_period:
 * @meta: this #ofaIDBDossierMeta instance.
 * @period: the new #ofaIDBExerciceMeta to be added.
 *
 * Takes a reference on the provided @period, and adds it to the list
 * of defined financial periods.
 */
void
ofa_idbdossier_meta_add_period( ofaIDBDossierMeta *meta, ofaIDBExerciceMeta *period )
{
	sIDBMeta *data;

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));
	g_return_if_fail( period && OFA_IS_IDBEXERCICE_META( period ));

	data = get_idbdossier_meta_data( meta );
	data->periods = g_list_prepend( data->periods, g_object_ref( period ));
}

/**
 * ofa_idbdossier_meta_update_period:
 * @meta: this #ofaIDBDossierMeta instance.
 * @period: the #ofaIDBExerciceMeta to be updated.
 * @current: whether the financial period (exercice) is current.
 * @begin: [allow-none]: the beginning date.
 * @end: [allow-none]: the ending date.
 *
 * Update the dossier settings for this @period with the specified datas.
 */
void
ofa_idbdossier_meta_update_period( ofaIDBDossierMeta *meta,
		ofaIDBExerciceMeta *period, gboolean current, const GDate *begin, const GDate *end )
{
	static const gchar *thisfn = "ofa_idbdossier_meta_update_period";

	g_debug( "%s: meta=%p, period=%p, current=%s, begin=%p, end=%p",
			thisfn, ( void * ) meta, ( void * ) period,
			current ? "True":"False", ( void * ) begin, ( void * ) end );

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));
	g_return_if_fail( period && OFA_IS_IDBEXERCICE_META( period ));

	if( OFA_IDBDOSSIER_META_GET_INTERFACE( meta )->update_period ){
		OFA_IDBDOSSIER_META_GET_INTERFACE( meta )->update_period( meta, period, current, begin, end );
		return;
	}

	g_info( "%s: ofaIDBDossierMeta's %s implementation does not provide 'update_period()' method",
			thisfn, G_OBJECT_TYPE_NAME( meta ));
}

/**
 * ofa_idbdossier_meta_remove_period:
 * @meta: this #ofaIDBDossierMeta instance.
 * @period: the new #ofaIDBExerciceMeta to be removed.
 *
 * Remove @period from the list of financial periods of @meta.
 * Also remove @meta from the settings when removing the last period.
 */
void
ofa_idbdossier_meta_remove_period( ofaIDBDossierMeta *meta, ofaIDBExerciceMeta *period )
{
	static const gchar *thisfn = "ofa_idbdossier_meta_remove_period";
	sIDBMeta *data;

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));
	g_return_if_fail( period && OFA_IS_IDBEXERCICE_META( period ));

	data = get_idbdossier_meta_data( meta );
	if( g_list_length( data->periods ) == 1 ){
		ofa_idbdossier_meta_remove_meta( meta );

	} else {
		data->periods = g_list_remove( data->periods, period );
		if( OFA_IDBDOSSIER_META_GET_INTERFACE( meta )->remove_period ){
			OFA_IDBDOSSIER_META_GET_INTERFACE( meta )->remove_period( meta, period );
		} else {
			g_info( "%s: ofaIDBDossierMeta's %s implementation does not provide 'remove_period()' method",
					thisfn, G_OBJECT_TYPE_NAME( meta ));
		}
	}
}

/**
 * ofa_idbdossier_meta_get_current_period:
 * @meta: this #ofaIDBDossierMeta instance.
 *
 * Returns: a new reference of the #ofaIDBExerciceMeta which identifies the
 * current financial period. This reference should be g_object_unref()
 * by the caller.
 */
ofaIDBExerciceMeta *
ofa_idbdossier_meta_get_current_period( const ofaIDBDossierMeta *meta )
{
	sIDBMeta *data;
	GList *it;
	ofaIDBExerciceMeta *period;

	g_return_val_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ), NULL );

	data = get_idbdossier_meta_data( meta );
	for( it=data->periods ; it ; it=it->next ){
		period = ( ofaIDBExerciceMeta * ) it->data;
		g_return_val_if_fail( period && OFA_IS_IDBEXERCICE_META( period ), NULL );
		if( ofa_idbexercice_meta_get_current( period )){
			return( g_object_ref( period ));
		}
	}

	return( NULL );
}

/**
 * ofa_idbdossier_meta_get_period:
 * @meta: this #ofaIDBDossierMeta instance.
 * @begin: [allow-none]: the beginning date.
 * @end: [allow-none]: the ending date.
 *
 * Returns: a #ofaIDBExerciceMeta which corresponds to the specified @begin
 * and @end dates.
 */
ofaIDBExerciceMeta *
ofa_idbdossier_meta_get_period( const ofaIDBDossierMeta *meta, const GDate *begin, const GDate *end )
{
	static const gchar *thisfn = "ofa_idbdossier_meta_get_period";
	sIDBMeta *data;
	GList *it;
	ofaIDBExerciceMeta *period;

	g_debug( "%s: meta=%p, begin=%p, end=%p",
			thisfn, ( void * ) meta, ( void * ) begin, ( void * ) end );

	g_return_val_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ), NULL );

	data = get_idbdossier_meta_data( meta );
	for( it=data->periods ; it ; it=it->next ){
		period = ( ofaIDBExerciceMeta * ) it->data;
		if( ofa_idbexercice_meta_is_suitable( period, begin, end )){
			return( period );
		}
	}

	return( NULL );
}

/**
 * ofa_idbdossier_meta_compare:
 * @a: an #ofaIDBDossierMeta instance.
 * @b: another #ofaIDBDossierMeta instance.
 *
 * Returns: -1 if @a < @b, +1 if @a > @b, 0 if they are equal.
 *
 * This comparison relies only on the respective dossier name of the
 * instances.
 */
gint
ofa_idbdossier_meta_compare( const ofaIDBDossierMeta *a, const ofaIDBDossierMeta *b )
{
	const gchar *a_name, *b_name;
	gint cmp;

	g_return_val_if_fail( a && OFA_IS_IDBDOSSIER_META( a ), FALSE );
	g_return_val_if_fail( b && OFA_IS_IDBDOSSIER_META( b ), FALSE );

	a_name = ofa_idbdossier_meta_get_dossier_name( a );
	b_name = ofa_idbdossier_meta_get_dossier_name( b );

	cmp = g_utf8_collate( a_name, b_name );

	return( cmp );
}

/**
 * ofa_idbdossier_meta_dump:
 * @meta: this #ofaIDBDossierMeta instance.
 *
 * Dumps data.
 */
void
ofa_idbdossier_meta_dump( const ofaIDBDossierMeta *meta )
{
	static const gchar *thisfn = "ofa_idbdossier_meta_dump";
	sIDBMeta *data;

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));

	data = get_idbdossier_meta_data( meta );

	g_debug( "%s: meta=%p (%s)", thisfn, ( void * ) meta, G_OBJECT_TYPE_NAME( meta ));
	g_debug( "%s:   provider=%p", thisfn, ( void * ) data->provider );
	g_debug( "%s:   dossier_name=%s", thisfn, data->dossier_name );
	g_debug( "%s:   settings=%p", thisfn, ( void * ) data->settings );
	g_debug( "%s:   group_name=%s", thisfn, data->group_name );
	g_debug( "%s:   periods=%p (length=%u)", thisfn, ( void * ) data->periods, g_list_length( data->periods ));
}

/**
 * ofa_idbdossier_meta_dump_full:
 * @meta: this #ofaIDBDossierMeta instance.
 *
 * Recursively dumps data.
 */
void
ofa_idbdossier_meta_dump_full( const ofaIDBDossierMeta *meta )
{
	sIDBMeta *data;
	GList *it;

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));

	ofa_idbdossier_meta_dump( meta );
	data = get_idbdossier_meta_data( meta );
	for( it=data->periods ; it ; it=it->next ){
		ofa_idbexercice_meta_dump( OFA_IDBEXERCICE_META( it->data ));
	}
}

static sIDBMeta *
get_idbdossier_meta_data( const ofaIDBDossierMeta *meta )
{
	sIDBMeta *data;

	data = ( sIDBMeta * ) g_object_get_data( G_OBJECT( meta ), IDBDOSSIER_META_DATA );

	if( !data ){
		data = g_new0( sIDBMeta, 1 );
		g_object_set_data( G_OBJECT( meta ), IDBDOSSIER_META_DATA, data );
		g_object_weak_ref( G_OBJECT( meta ), ( GWeakNotify ) on_meta_finalized, data );
	}

	return( data );
}

static void
on_meta_finalized( sIDBMeta *data, GObject *finalized_meta )
{
	static const gchar *thisfn = "ofa_idbdossier_meta_on_meta_finalized";

	g_debug( "%s: data=%p, finalized_meta=%p", thisfn, ( void * ) data, ( void * ) finalized_meta );

	g_clear_object( &data->provider );
	g_free( data->dossier_name );
	g_clear_object( &data->settings );
	g_free( data->group_name );
	ofa_idbdossier_meta_free_periods( data->periods );
	g_free( data );
}
