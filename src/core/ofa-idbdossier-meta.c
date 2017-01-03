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
	gchar          *dossier_name;

	/* second stage setup
	 */
	myISettings    *settings_iface;			/* dossier settings management interface */
	gchar          *settings_group;			/* managed by ofaDossierCollection */

	/* runtime
	 */
	GList          *periods;				/* ofaIDBExerciceMeta */
}
	sIDBMeta;

#define IDBDOSSIER_META_LAST_VERSION       1
#define IDBDOSSIER_META_DATA              "idbdossier-meta-data"
#define IDBDOSSIER_META_PERIOD_KEY_PREFIX "ofa-Exercice-"

static guint st_initializations          = 0;	/* interface initialization count */

static GType               register_type( void );
static void                interface_base_init( ofaIDBDossierMetaInterface *klass );
static void                interface_base_finalize( ofaIDBDossierMetaInterface *klass );
static void                set_exercices_from_settings( ofaIDBDossierMeta *self, sIDBMeta *sdata );
static ofaIDBExerciceMeta *find_exercice( ofaIDBDossierMeta *self, sIDBMeta *sdata, ofaIDBExerciceMeta *exercice_meta );
static void                get_exercice_key( ofaIDBDossierMeta *self, gchar **key, gchar **key_id );
static sIDBMeta           *get_instance_data( const ofaIDBDossierMeta *self );
static void                on_instance_finalized( sIDBMeta *sdata, GObject *finalized_meta );

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
	sIDBMeta *sdata;

	g_return_val_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ), NULL );

	sdata = get_instance_data( meta );

	return( g_object_ref( sdata->provider ));
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
	sIDBMeta *sdata;

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));
	g_return_if_fail( instance && OFA_IS_IDBPROVIDER( instance ));

	sdata = get_instance_data( meta );

	g_clear_object( &sdata->provider );
	sdata->provider = g_object_ref(( gpointer ) instance );
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
	sIDBMeta *sdata;

	g_return_val_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ), NULL );

	sdata = get_instance_data( meta );

	return(( const gchar * ) sdata->dossier_name );
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
	sIDBMeta *sdata;

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));

	sdata = get_instance_data( meta );

	g_free( sdata->dossier_name );
	sdata->dossier_name = g_strdup( dossier_name );
}

/**
 * ofa_idbdossier_meta_get_settings_iface:
 * @meta: this #ofaIDBDossierMeta instance.
 *
 * Returns: the #myISettings object.
 *
 * The returned reference is owned by the interface, and should
 * not be freed by the caller.
 */
myISettings *
ofa_idbdossier_meta_get_settings_iface( const ofaIDBDossierMeta *meta )
{
	sIDBMeta *sdata;

	g_return_val_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ), NULL );

	sdata = get_instance_data( meta );

	return( sdata->settings_iface );
}

/**
 * ofa_idbdossier_meta_set_settings_iface:
 * @meta: this #ofaIDBDossierMeta instance.
 * @settings: the dossier settings interface.
 *
 * Set the @settings.
 */
void
ofa_idbdossier_meta_set_settings_iface( ofaIDBDossierMeta *meta, myISettings *settings )
{
	sIDBMeta *sdata;

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));
	g_return_if_fail( settings && MY_IS_ISETTINGS( settings ));

	sdata = get_instance_data( meta );

	sdata->settings_iface = settings;
}

/**
 * ofa_idbdossier_meta_get_settings_group:
 * @meta: this #ofaIDBDossierMeta instance.
 *
 * Returns: the name of the group which holds all dossier informations
 * in the settings file, as a newly allocated string which should be
 * g_free() by the caller.
 */
gchar *
ofa_idbdossier_meta_get_settings_group( const ofaIDBDossierMeta *meta )
{
	sIDBMeta *sdata;

	g_return_val_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ), NULL );

	sdata = get_instance_data( meta );

	return( g_strdup( sdata->settings_group ));
}

/**
 * ofa_idbdossier_meta_set_settings_group:
 * @meta: this #ofaIDBDossierMeta instance.
 * @group_name: the group of this @meta in the settings.
 *
 * Set the @group_name.
 */
void
ofa_idbdossier_meta_set_settings_group( ofaIDBDossierMeta *meta, const gchar *group_name )
{
	sIDBMeta *sdata;

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));
	g_return_if_fail( my_strlen( group_name ));

	sdata = get_instance_data( meta );

	g_free( sdata->settings_group );
	sdata->settings_group = g_strdup( group_name );
}

/**
 * ofa_idbdossier_meta_set_from_settings:
 * @meta: this #ofaIDBDossierMeta instance.
 *
 * Reads from dossier settings informations relative to the @meta dossier.
 */
void
ofa_idbdossier_meta_set_from_settings( ofaIDBDossierMeta *meta )
{
	static const gchar *thisfn = "ofa_idbdossier_meta_set_from_settings";
	sIDBMeta *sdata;

	g_debug( "%s: meta=%p", thisfn, ( void * ) meta );

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));

	sdata = get_instance_data( meta );

	if( OFA_IDBDOSSIER_META_GET_INTERFACE( meta )->set_from_settings ){
		OFA_IDBDOSSIER_META_GET_INTERFACE( meta )->set_from_settings( meta );
		set_exercices_from_settings( meta, sdata );
		return;
	}

	g_info( "%s: ofaIDBDossierMeta's %s implementation does not provide 'set_from_settings()' method",
			thisfn, G_OBJECT_TYPE_NAME( meta ));
}

/*
 * load the defined exercices from the settings
 */
static void
set_exercices_from_settings( ofaIDBDossierMeta *self, sIDBMeta *sdata )
{
	GList *keys, *itk, *new_list;
	const gchar *key;
	ofaIDBExerciceMeta *exercice_meta, *period;
	glong lenstr;

	new_list = NULL;
	keys = my_isettings_get_keys( sdata->settings_iface, sdata->settings_group );
	lenstr = my_strlen( IDBDOSSIER_META_PERIOD_KEY_PREFIX );

	for( itk=keys ; itk ; itk=itk->next ){
		key = ( const gchar * ) itk->data;
		if( g_str_has_prefix( key, IDBDOSSIER_META_PERIOD_KEY_PREFIX )){
			exercice_meta = ofa_idbdossier_meta_new_exercice_meta( self );
			ofa_idbexercice_meta_set_from_settings( exercice_meta, key, key+lenstr );
			period = find_exercice( self, sdata, exercice_meta );
			if( period ){
				g_object_ref( period );
				g_object_unref( exercice_meta );
			} else {
				period = exercice_meta;
			}
			new_list = g_list_prepend( new_list, period );
		}
	}

	ofa_idbdossier_meta_free_periods( sdata->periods );
	sdata->periods = new_list;
	my_isettings_free_keys( sdata->settings_iface, keys );
}

/*
 * Search for @exercice_meta in defined exercices
 * Returns: the found ofaIDBExerciceMeta or NULL.
 */
static ofaIDBExerciceMeta *
find_exercice( ofaIDBDossierMeta *self, sIDBMeta *sdata, ofaIDBExerciceMeta *exercice_meta )
{
	GList *it;
	ofaIDBExerciceMeta *current;

	for( it=sdata->periods ; it ; it=it->next ){
		current = ( ofaIDBExerciceMeta * ) it->data;
		if( ofa_idbexercice_meta_compare( current, exercice_meta ) == 0 ){
			return( current );
		}
	}

	return( NULL );
}

/**
 * ofa_idbdossier_meta_set_from_editor:
 * @meta: this #ofaIDBDossierMeta instance.
 * @editor: the #ofaIDBDossierEditor which handles the connection information.
 *
 * Records in dossier settings the informations relative to the @meta dossier.
 */
void
ofa_idbdossier_meta_set_from_editor( ofaIDBDossierMeta *meta, ofaIDBDossierEditor *editor )
{
	static const gchar *thisfn = "ofa_idbdossier_meta_set_from_editor";

	g_debug( "%s: meta=%p, editor=%p",
			thisfn, ( void * ) meta, ( void * ) editor );

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));
	g_return_if_fail( editor && OFA_IS_IDBDOSSIER_EDITOR( editor ));

	if( OFA_IDBDOSSIER_META_GET_INTERFACE( meta )->set_from_editor ){
		OFA_IDBDOSSIER_META_GET_INTERFACE( meta )->set_from_editor( meta, editor );
		return;
	}

	g_info( "%s: ofaIDBDossierMeta's %s implementation does not provide 'set_from_editor()' method",
			thisfn, G_OBJECT_TYPE_NAME( meta ));
}

/**
 * ofa_idbdossier_meta_new_exercice_meta:
 * @dossier_meta: this #ofaIDBDossierMeta dossier.
 *
 * Returns: a newly allocated #ofaIDBExerciceMeta object, which should be
 * g_object_unref() by the caller.
 */
ofaIDBExerciceMeta *
ofa_idbdossier_meta_new_exercice_meta( ofaIDBDossierMeta *dossier_meta )
{
	static const gchar *thisfn = "ofa_idbdossier_meta_new_exercice_meta";
	ofaIDBExerciceMeta *exercice_meta;

	g_debug( "%s: dossier_meta=%p",
			thisfn, ( void * ) dossier_meta );

	g_return_val_if_fail( dossier_meta && OFA_IS_IDBDOSSIER_META( dossier_meta ), NULL );

	if( OFA_IDBDOSSIER_META_GET_INTERFACE( dossier_meta )->new_exercice_meta ){
		exercice_meta = OFA_IDBDOSSIER_META_GET_INTERFACE( dossier_meta )->new_exercice_meta( dossier_meta );
		ofa_idbexercice_meta_set_dossier_meta( exercice_meta, dossier_meta );
		return( exercice_meta );
	}

	g_info( "%s: ofaIDBDossierMeta's %s implementation does not provide 'new_exercice_meta()' method",
			thisfn, G_OBJECT_TYPE_NAME( dossier_meta ));
	return( NULL );
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
	sIDBMeta *sdata;

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));

	sdata = get_instance_data( meta );

	my_isettings_remove_group( sdata->settings_iface, sdata->settings_group );
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
	sIDBMeta *sdata;

	g_return_val_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ), NULL );

	sdata = get_instance_data( meta );

	return( g_list_copy_deep( sdata->periods, ( GCopyFunc ) g_object_ref, NULL ));
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
	sIDBMeta *sdata;

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));

	sdata = get_instance_data( meta );

	ofa_idbdossier_meta_free_periods( sdata->periods );
	sdata->periods = g_list_copy_deep( periods, ( GCopyFunc ) g_object_ref, NULL );
}

/**
 * ofa_idbdossier_meta_add_period:
 * @meta: this #ofaIDBDossierMeta instance.
 * @period: the new #ofaIDBExerciceMeta to be added.
 * @key: [out]: the key to be used by @period in dossier settings.
 * @key_id: [out]: the identifier of the key.
 *
 * Takes the ownership of the provided @period, and adds it to the list
 * of defined financial periods.
 */
void
ofa_idbdossier_meta_add_period( ofaIDBDossierMeta *meta, ofaIDBExerciceMeta *period, gchar **key, gchar **key_id )
{
	static const gchar *thisfn = "ofa_idbdossier_meta_add_period";
	sIDBMeta *sdata;

	g_debug( "%s: meta=%p, period=%p, key=%p, key_id=%p",
			thisfn, ( void * ) meta, ( void * ) period, ( void * ) key, ( void * ) key_id );

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));
	g_return_if_fail( period && OFA_IS_IDBEXERCICE_META( period ));
	g_return_if_fail( key );
	g_return_if_fail( key_id );

	sdata = get_instance_data( meta );
	sdata->periods = g_list_prepend( sdata->periods, period );
	get_exercice_key( meta, key, key_id );
}

/*
 * Returns: a new key as an alphanumeric string
 */
static void
get_exercice_key( ofaIDBDossierMeta *self, gchar **key, gchar **key_id )
{
	sIDBMeta *sdata;
	gboolean exists;

	*key = NULL;
	*key_id = NULL;
	exists = TRUE;
	sdata = get_instance_data( self );

	while( exists ){
		g_free( *key );
		g_free( *key_id );
		*key_id = g_strdup_printf( "%6x", g_random_int());
		*key = g_strdup_printf( "%s%s", IDBDOSSIER_META_PERIOD_KEY_PREFIX, *key_id );
		exists = my_isettings_has_key( sdata->settings_iface, sdata->settings_group, *key );
		//g_debug( "id=%s, exists=%s", *key_id, exists ? "True":"False" );
	}
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
	sIDBMeta *sdata;

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));
	g_return_if_fail( period && OFA_IS_IDBEXERCICE_META( period ));

	sdata = get_instance_data( meta );

	if( g_list_length( sdata->periods ) == 1 ){
		ofa_idbdossier_meta_remove_meta( meta );

	} else {
		sdata->periods = g_list_remove( sdata->periods, period );
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
	sIDBMeta *sdata;
	GList *it;
	ofaIDBExerciceMeta *period;

	g_return_val_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ), NULL );

	sdata = get_instance_data( meta );

	for( it=sdata->periods ; it ; it=it->next ){
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
	sIDBMeta *sdata;
	GList *it;
	ofaIDBExerciceMeta *period;

	g_debug( "%s: meta=%p, begin=%p, end=%p",
			thisfn, ( void * ) meta, ( void * ) begin, ( void * ) end );

	g_return_val_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ), NULL );

	sdata = get_instance_data( meta );

	for( it=sdata->periods ; it ; it=it->next ){
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
	const gchar *b_name;

	g_return_val_if_fail( a && OFA_IS_IDBDOSSIER_META( a ), 0 );
	g_return_val_if_fail( b && OFA_IS_IDBDOSSIER_META( b ), 0 );

	b_name = ofa_idbdossier_meta_get_dossier_name( b );

	return( ofa_idbdossier_meta_compare_by_name( a, b_name ));
}

/**
 * ofa_idbdossier_meta_compare_by_name:
 * @a: an #ofaIDBDossierMeta instance.
 * @name: the name of a dossier.
 *
 * Returns: -1 if @a < @name, +1 if @a > @name, 0 if they are equal.
 *
 * This comparison relies only on the respective dossier name of the
 * instances.
 */
gint
ofa_idbdossier_meta_compare_by_name( const ofaIDBDossierMeta *a, const gchar *name )
{
	const gchar *a_name;
	gint cmp;

	g_return_val_if_fail( a && OFA_IS_IDBDOSSIER_META( a ), 0 );
	g_return_val_if_fail( my_strlen( name ), 0 );

	a_name = ofa_idbdossier_meta_get_dossier_name( a );

	cmp = my_collate( a_name, name );

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
	sIDBMeta *sdata;

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));

	sdata = get_instance_data( meta );

	g_debug( "%s: meta=%p (%s)", thisfn, ( void * ) meta, G_OBJECT_TYPE_NAME( meta ));
	g_debug( "%s:   provider=%p", thisfn, ( void * ) sdata->provider );
	g_debug( "%s:   dossier_name=%s", thisfn, sdata->dossier_name );
	g_debug( "%s:   settings=%p", thisfn, ( void * ) sdata->settings_iface );
	g_debug( "%s:   group_name=%s", thisfn, sdata->settings_group );
	g_debug( "%s:   periods=%p (count=%u)", thisfn, ( void * ) sdata->periods, g_list_length( sdata->periods ));

	if( OFA_IDBDOSSIER_META_GET_INTERFACE( meta )->dump ){
		OFA_IDBDOSSIER_META_GET_INTERFACE( meta )->dump( meta );
	}
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
	sIDBMeta *sdata;
	GList *it;

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));

	ofa_idbdossier_meta_dump( meta );
	sdata = get_instance_data( meta );
	for( it=sdata->periods ; it ; it=it->next ){
		ofa_idbexercice_meta_dump( OFA_IDBEXERCICE_META( it->data ));
	}
}

static sIDBMeta *
get_instance_data( const ofaIDBDossierMeta *self )
{
	sIDBMeta *sdata;

	sdata = ( sIDBMeta * ) g_object_get_data( G_OBJECT( self ), IDBDOSSIER_META_DATA );

	if( !sdata ){
		sdata = g_new0( sIDBMeta, 1 );
		g_object_set_data( G_OBJECT( self ), IDBDOSSIER_META_DATA, sdata );
		g_object_weak_ref( G_OBJECT( self ), ( GWeakNotify ) on_instance_finalized, sdata );
	}

	return( sdata );
}

static void
on_instance_finalized( sIDBMeta *sdata, GObject *finalized_meta )
{
	static const gchar *thisfn = "ofa_idbdossier_meta_on_instance_finalized";

	g_debug( "%s: sdata=%p, finalized_meta=%p", thisfn, ( void * ) sdata, ( void * ) finalized_meta );

	g_clear_object( &sdata->provider );
	g_free( sdata->dossier_name );
	g_free( sdata->settings_group );
	ofa_idbdossier_meta_free_periods( sdata->periods );
	g_free( sdata );
}
