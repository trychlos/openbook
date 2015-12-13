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

#include <glib/gi18n.h>

#include "api/my-utils.h"
#include "api/ofa-idbmeta.h"

/* some data attached to each IDBMeta instance
 * we store here the data provided by the application
 * which do not depend of a specific implementation
 */
typedef struct {
	ofaIDBProvider *prov_instance;
	gchar          *dossier_name;
	mySettings     *settings;
	gchar          *group_name;
	GList          *periods;
}
	sIDBMeta;

#define IDBMETA_LAST_VERSION         1
#define IDBMETA_DATA                 "idbmeta-data"

static guint st_initializations         = 0;	/* interface initialization count */

static GType       register_type( void );
static void        interface_base_init( ofaIDBMetaInterface *klass );
static void        interface_base_finalize( ofaIDBMetaInterface *klass );
static sIDBMeta *get_idbmeta_data( const ofaIDBMeta *meta );
static void        on_meta_finalized( sIDBMeta *data, GObject *finalized_meta );

/**
 * ofa_idbmeta_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_idbmeta_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_idbmeta_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_idbmeta_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIDBMetaInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIDBMeta", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIDBMetaInterface *klass )
{
	static const gchar *thisfn = "ofa_idbmeta_interface_base_init";

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIDBMetaInterface *klass )
{
	static const gchar *thisfn = "ofa_idbmeta_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_idbmeta_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_idbmeta_get_interface_last_version( void )
{
	return( IDBMETA_LAST_VERSION );
}

/**
 * ofa_idbmeta_get_interface_version:
 * @meta: this #ofaIDBMeta instance.
 *
 * Returns: the version number implemented by the object.
 *
 * Defaults to 1.
 */
guint
ofa_idbmeta_get_interface_version( const ofaIDBMeta *meta )
{
	static const gchar *thisfn = "ofa_idbmeta_get_interface_version";

	g_debug( "%s: meta=%p", thisfn, ( void * ) meta );

	g_return_val_if_fail( meta && OFA_IS_IDBMETA( meta ), 0 );

	if( OFA_IDBMETA_GET_INTERFACE( meta )->get_interface_version ){
		return( OFA_IDBMETA_GET_INTERFACE( meta )->get_interface_version( meta ));
	}

	g_info( "%s: ofaIDBMeta instance %p does not provide 'get_interface_version()' method",
			thisfn, ( void * ) meta );
	return( 1 );
}

/**
 * ofa_idbmeta_get_provider:
 * @meta: this #ofaIDBMeta instance.
 *
 * Returns: a new reference to the provider instance which should be
 * g_object_unref() by the caller.
 */
ofaIDBProvider *
ofa_idbmeta_get_provider( const ofaIDBMeta *meta )
{
	sIDBMeta *data;

	g_return_val_if_fail( meta && OFA_IS_IDBMETA( meta ), NULL );

	data = get_idbmeta_data( meta );
	return( g_object_ref( data->prov_instance ));
}

/**
 * ofa_idbmeta_set_provider:
 * @meta: this #ofaIDBMeta instance.
 * @instance: the #ofaIDBProvider which manages the dossier.
 *
 * The interface takes a reference on the @instance object, to make
 * sure it stays available. This reference will be automatically
 * released on @meta finalization.
 */
void
ofa_idbmeta_set_provider( ofaIDBMeta *meta, const ofaIDBProvider *instance )
{
	sIDBMeta *data;

	g_return_if_fail( meta && OFA_IS_IDBMETA( meta ));

	data = get_idbmeta_data( meta );
	g_clear_object( &data->prov_instance );
	data->prov_instance = g_object_ref(( gpointer ) instance );
}

/**
 * ofa_idbmeta_get_dossier_name:
 * @meta: this #ofaIDBMeta instance.
 *
 * Returns: the identifier name of the dossier as a newly allocated
 * string which should be g_free() by the caller.
 */
gchar *
ofa_idbmeta_get_dossier_name( const ofaIDBMeta *meta )
{
	sIDBMeta *data;

	g_return_val_if_fail( meta && OFA_IS_IDBMETA( meta ), NULL );

	data = get_idbmeta_data( meta );
	return( g_strdup( data->dossier_name ));
}

/**
 * ofa_idbmeta_set_dossier_name:
 * @meta: this #ofaIDBMeta instance.
 * @dossier_name: the name of the dossier.
 *
 * Stores the name of the dossier as an interface data.
 */
void
ofa_idbmeta_set_dossier_name( ofaIDBMeta *meta, const gchar *dossier_name )
{
	sIDBMeta *data;

	g_return_if_fail( meta && OFA_IS_IDBMETA( meta ));

	data = get_idbmeta_data( meta );
	g_free( data->dossier_name );
	data->dossier_name = g_strdup( dossier_name );
}

/**
 * ofa_idbmeta_get_settings:
 * @meta: this #ofaIDBMeta instance.
 *
 * Returns: the #mySettings object.
 *
 * The returned reference is owned by the interface, and should
 * not be freed by the caller.
 */
mySettings *
ofa_idbmeta_get_settings( const ofaIDBMeta *meta )
{
	sIDBMeta *data;

	g_return_val_if_fail( meta && OFA_IS_IDBMETA( meta ), NULL );

	data = get_idbmeta_data( meta );

	return( data->settings );
}

/**
 * ofa_idbmeta_get_group_name:
 * @meta: this #ofaIDBMeta instance.
 *
 * Returns: the name of the group which holds all dossier informations
 * in the settings file, as a newly allocated string which should be
 * g_free() by the caller.
 */
gchar *
ofa_idbmeta_get_group_name( const ofaIDBMeta *meta )
{
	sIDBMeta *data;

	g_return_val_if_fail( meta && OFA_IS_IDBMETA( meta ), NULL );

	data = get_idbmeta_data( meta );

	return( g_strdup( data->group_name ));
}

/**
 * ofa_idbmeta_set_from_settings:
 * @meta: this #ofaIDBMeta instance.
 * @settings: the #mySettings which manages the dossier settings file.
 * @group_name: the group name for the dossier.
 */
void
ofa_idbmeta_set_from_settings( ofaIDBMeta *meta, mySettings *settings, const gchar *group_name )
 {
 	static const gchar *thisfn = "ofa_idbmeta_set_from_settings";
 	sIDBMeta *data;

 	g_debug( "%s: meta=%p, settings=%p, group_name=%s",
 			thisfn, ( void * ) meta, ( void * ) settings, group_name );

 	g_return_if_fail( meta && OFA_IS_IDBMETA( meta ));

 	data = get_idbmeta_data( meta );
 	g_clear_object( &data->settings );
 	data->settings = g_object_ref( settings );
 	g_free( data->group_name );
 	data->group_name = g_strdup( group_name );

 	if( OFA_IDBMETA_GET_INTERFACE( meta )->set_from_settings ){
 		OFA_IDBMETA_GET_INTERFACE( meta )->set_from_settings( meta, settings, group_name );
 		return;
 	}

 	g_info( "%s: ofaIDBMeta instance %p does not provide 'set_from_settings()' method",
 			thisfn, ( void * ) meta );
 }

/**
 * ofa_idbmeta_set_from_editor:
 * @meta: this #ofaIDBMeta instance.
 * @editor: the #ofaIDBEditor which handles the connection information.
 * @settings: the #mySettings which manages the dossier settings file.
 * @group_name: the group name for the dossier.
 */
void
ofa_idbmeta_set_from_editor( ofaIDBMeta *meta, const ofaIDBEditor *editor, mySettings *settings, const gchar *group_name )
{
	static const gchar *thisfn = "ofa_idbmeta_set_from_editor";
	sIDBMeta *data;

	g_debug( "%s: meta=%p, editor=%p, settings=%p, group_name=%s",
			thisfn, ( void * ) meta, ( void * ) editor, ( void * ) settings, group_name );

	g_return_if_fail( meta && OFA_IS_IDBMETA( meta ));
	g_return_if_fail( editor && OFA_IS_IDBEDITOR( editor ));

	data = get_idbmeta_data( meta );
	g_clear_object( &data->settings );
	data->settings = g_object_ref( settings );
	g_free( data->group_name );
	data->group_name = g_strdup( group_name );

	if( OFA_IDBMETA_GET_INTERFACE( meta )->set_from_editor ){
		OFA_IDBMETA_GET_INTERFACE( meta )->set_from_editor( meta, editor, settings, group_name );
		return;
	}

	g_info( "%s: ofaIDBMeta instance %p does not provide 'set_from_editor()' method",
			thisfn, ( void * ) meta );
}

/**
 * ofa_idbmeta_remove_meta:
 * @meta: this #ofaIDBMeta instance.
 *
 * Remove @meta from the dossier settings file.
 *
 * The #ofaIDBMeta object itself will be finalized by the #ofaFileDir
 * directory which auto-updates.
 */
void
ofa_idbmeta_remove_meta( ofaIDBMeta *meta )
{
	sIDBMeta *data;

	g_return_if_fail( meta && OFA_IS_IDBMETA( meta ));

	data = get_idbmeta_data( meta );
	my_settings_remove_group( data->settings, data->group_name );
}

/**
 * ofa_idbmeta_get_periods:
 * @meta: this #ofaIDBMeta instance.
 *
 * Returns: a list of defined financial periods (exercices) for this
 * file (dossier), as a #GList of #ofaIDBPeriod object, which should
 * be #ofa_idbmeta_free_periods() by the caller.
 */
GList *
ofa_idbmeta_get_periods( const ofaIDBMeta *meta )
{
	sIDBMeta *data;

	g_return_val_if_fail( meta && OFA_IS_IDBMETA( meta ), NULL );

	data = get_idbmeta_data( meta );
	return( g_list_copy_deep( data->periods, ( GCopyFunc ) g_object_ref, NULL ));
}

/**
 * ofa_idbmeta_set_periods:
 * @meta: this #ofaIDBMeta instance.
 * @list: the list of the periods for the dossier.
 *
 * Stores the list of the defined financial periods (exercices) of the
 * dossier, as a deep copy of the provided @list.
 */
void
ofa_idbmeta_set_periods( ofaIDBMeta *meta, GList *periods )
{
	sIDBMeta *data;

	g_return_if_fail( meta && OFA_IS_IDBMETA( meta ));

	data = get_idbmeta_data( meta );
	ofa_idbmeta_free_periods( data->periods );
	data->periods = g_list_copy_deep( periods, ( GCopyFunc ) g_object_ref, NULL );
}

/**
 * ofa_idbmeta_add_period:
 * @meta: this #ofaIDBMeta instance.
 * @period: the new #ofaIDBPeriod to be added.
 *
 * Takes a reference on the provided @period, and adds it to the list
 * of defined financial periods.
 */
void
ofa_idbmeta_add_period( ofaIDBMeta *meta, ofaIDBPeriod *period )
{
	sIDBMeta *data;

	g_return_if_fail( meta && OFA_IS_IDBMETA( meta ));
	g_return_if_fail( period && OFA_IS_IDBPERIOD( period ));

	data = get_idbmeta_data( meta );
	data->periods = g_list_prepend( data->periods, g_object_ref( period ));
}

/**
 * ofa_idbmeta_update_period:
 * @meta: this #ofaIDBMeta instance.
 * @period: the #ofaIDBPeriod to be updated.
 * @current: whether the financial period (exercice) is current.
 * @begin: [allow-none]: the beginning date.
 * @end: [allow-none]: the ending date.
 *
 * Update the dossier settings for this @period with the specified datas.
 */
void
ofa_idbmeta_update_period( ofaIDBMeta *meta,
		ofaIDBPeriod *period, gboolean current, const GDate *begin, const GDate *end )
{
	static const gchar *thisfn = "ofa_idbmeta_update_period";

	g_debug( "%s: meta=%p, period=%p, current=%s, begin=%p, end=%p",
			thisfn, ( void * ) meta, ( void * ) period,
			current ? "True":"False", ( void * ) begin, ( void * ) end );

	g_return_if_fail( meta && OFA_IS_IDBMETA( meta ));
	g_return_if_fail( period && OFA_IS_IDBPERIOD( period ));

	if( OFA_IDBMETA_GET_INTERFACE( meta )->update_period ){
		OFA_IDBMETA_GET_INTERFACE( meta )->update_period( meta, period, current, begin, end );
		return;
	}

	g_info( "%s: ofaIDBMeta instance %p does not provide 'update_period()' method",
			thisfn, ( void * ) meta );
}

/**
 * ofa_idbmeta_remove_period:
 * @meta: this #ofaIDBMeta instance.
 * @period: the new #ofaIDBPeriod to be removed.
 *
 * Remove @period from the list of financial periods of @meta.
 * Also remove @meta from the settings when removing the last period.
 */
void
ofa_idbmeta_remove_period( ofaIDBMeta *meta, ofaIDBPeriod *period )
{
	sIDBMeta *data;

	g_return_if_fail( meta && OFA_IS_IDBMETA( meta ));
	g_return_if_fail( period && OFA_IS_IDBPERIOD( period ));

	data = get_idbmeta_data( meta );
	if( g_list_length( data->periods ) == 1 ){
		ofa_idbmeta_remove_meta( meta );

	} else {
		data->periods = g_list_remove( data->periods, period );
		if( OFA_IDBMETA_GET_INTERFACE( meta )->remove_period ){
			OFA_IDBMETA_GET_INTERFACE( meta )->remove_period( meta, period );
		}
	}
}

/**
 * ofa_idbmeta_get_current_period:
 * @meta: this #ofaIDBMeta instance.
 *
 * Returns: a new reference of the #ofaIDBPeriod which identifies the
 * current financial period. This reference should be g_object_unref()
 * by the caller.
 */
ofaIDBPeriod *
ofa_idbmeta_get_current_period( const ofaIDBMeta *meta )
{
	sIDBMeta *data;
	GList *it;
	ofaIDBPeriod *period;

	g_return_val_if_fail( meta && OFA_IS_IDBMETA( meta ), NULL );

	data = get_idbmeta_data( meta );
	for( it=data->periods ; it ; it=it->next ){
		period = ( ofaIDBPeriod * ) it->data;
		g_return_val_if_fail( period && OFA_IS_IDBPERIOD( period ), NULL );
		if( ofa_idbperiod_get_current( period )){
			return( g_object_ref( period ));
		}
	}

	return( NULL );
}

/**
 * ofa_idbmeta_get_period:
 * @meta: this #ofaIDBMeta instance.
 * @begin: [allow-none]: the beginning date.
 * @end: [allow-none]: the ending date.
 *
 * Returns: a #ofaIDBPeriod which corresponds to the specified @begin
 * and @end dates.
 */
ofaIDBPeriod *
ofa_idbmeta_get_period( const ofaIDBMeta *meta, const GDate *begin, const GDate *end )
{
	static const gchar *thisfn = "ofa_idbmeta_get_period";
	sIDBMeta *data;
	GList *it;
	ofaIDBPeriod *period;

	g_debug( "%s: meta=%p, begin=%p, end=%p",
			thisfn, ( void * ) meta, ( void * ) begin, ( void * ) end );

	g_return_val_if_fail( meta && OFA_IS_IDBMETA( meta ), NULL );

	data = get_idbmeta_data( meta );
	for( it=data->periods ; it ; it=it->next ){
		period = ( ofaIDBPeriod * ) it->data;
		if( ofa_idbperiod_is_suitable( period, begin, end )){
			return( period );
		}
	}

	return( NULL );
}

/**
 * ofa_idbmeta_dump:
 * @meta: this #ofaIDBMeta instance.
 *
 * Dumps data.
 */
void
ofa_idbmeta_dump( const ofaIDBMeta *meta )
{
	static const gchar *thisfn = "ofa_idbmeta_dump";
	sIDBMeta *data;

	g_return_if_fail( meta && OFA_IS_IDBMETA( meta ));

	data = get_idbmeta_data( meta );

	g_debug( "%s: meta=%p (%s)", thisfn, ( void * ) meta, G_OBJECT_TYPE_NAME( meta ));
	g_debug( "%s:   prov_instance=%p", thisfn, ( void * ) data->prov_instance );
	g_debug( "%s:   dossier_name=%s", thisfn, data->dossier_name );
	g_debug( "%s:   settings=%p", thisfn, ( void * ) data->settings );
	g_debug( "%s:   group_name=%s", thisfn, data->group_name );
	g_debug( "%s:   periods=%p (length=%u)", thisfn, ( void * ) data->periods, g_list_length( data->periods ));
}

/**
 * ofa_idbmeta_dump_rec:
 * @meta: this #ofaIDBMeta instance.
 *
 * Recursively dumps data.
 */
void
ofa_idbmeta_dump_rec( const ofaIDBMeta *meta )
{
	sIDBMeta *data;
	GList *it;

	g_return_if_fail( meta && OFA_IS_IDBMETA( meta ));

	ofa_idbmeta_dump( meta );
	data = get_idbmeta_data( meta );
	for( it=data->periods ; it ; it=it->next ){
		ofa_idbperiod_dump( OFA_IDBPERIOD( it->data ));
	}
}

static sIDBMeta *
get_idbmeta_data( const ofaIDBMeta *meta )
{
	sIDBMeta *data;

	data = ( sIDBMeta * ) g_object_get_data( G_OBJECT( meta ), IDBMETA_DATA );

	if( !data ){
		data = g_new0( sIDBMeta, 1 );
		g_object_set_data( G_OBJECT( meta ), IDBMETA_DATA, data );
		g_object_weak_ref( G_OBJECT( meta ), ( GWeakNotify ) on_meta_finalized, data );
	}

	return( data );
}

static void
on_meta_finalized( sIDBMeta *data, GObject *finalized_meta )
{
	static const gchar *thisfn = "ofa_idbmeta_on_meta_finalized";

	g_debug( "%s: data=%p, finalized_meta=%p", thisfn, ( void * ) data, ( void * ) finalized_meta );

	g_clear_object( &data->prov_instance );
	g_free( data->dossier_name );
	g_clear_object( &data->settings );
	g_free( data->group_name );
	ofa_idbmeta_free_periods( data->periods );
	g_free( data );
}
