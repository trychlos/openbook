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

#include <api/ofa-ifile-meta.h>

#define IFILE_META_LAST_VERSION           1

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIFileMetaInterface *klass );
static void  interface_base_finalize( ofaIFileMetaInterface *klass );

/**
 * ofa_ifile_meta_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_ifile_meta_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_ifile_meta_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_ifile_meta_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIFileMetaInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIFileMeta", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIFileMetaInterface *klass )
{
	static const gchar *thisfn = "ofa_ifile_meta_interface_base_init";

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIFileMetaInterface *klass )
{
	static const gchar *thisfn = "ofa_ifile_meta_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_ifile_meta_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_ifile_meta_get_interface_last_version( void )
{
	return( IFILE_META_LAST_VERSION );
}

/**
 * ofa_ifile_meta_get_interface_version:
 * @meta: this #ofaIFileMeta instance.
 *
 * Returns: the version number implemented by the object.
 *
 * Defaults to 1.
 */
guint
ofa_ifile_meta_get_interface_version( const ofaIFileMeta *meta )
{
	g_return_val_if_fail( meta && OFA_IS_IFILE_META( meta ), 0 );

	if( OFA_IFILE_META_GET_INTERFACE( meta )->get_interface_version ){
		return( OFA_IFILE_META_GET_INTERFACE( meta )->get_interface_version( meta ));
	}

	return( 1 );
}

/**
 * ofa_ifile_meta_get_dossier_name:
 * @meta: this #ofaIFileMeta instance.
 *
 * Returns: the identifier name of the dossier as a newly allocated
 * string which should be g_free() by the caller.
 */
gchar *
ofa_ifile_meta_get_dossier_name( const ofaIFileMeta *meta )
{
	g_return_val_if_fail( meta && OFA_IS_IFILE_META( meta ), NULL );

	if( OFA_IFILE_META_GET_INTERFACE( meta )->get_dossier_name ){
		return( OFA_IFILE_META_GET_INTERFACE( meta )->get_dossier_name( meta ));
	}

	return( NULL );
}

/**
 * ofa_ifile_meta_get_provider_name:
 * @meta: this #ofaIFileMeta instance.
 *
 * Returns: the provider name as a newly allocated
 * string which should be g_free() by the caller.
 */
gchar *
ofa_ifile_meta_get_provider_name( const ofaIFileMeta *meta )
{
	g_return_val_if_fail( meta && OFA_IS_IFILE_META( meta ), NULL );

	if( OFA_IFILE_META_GET_INTERFACE( meta )->get_provider_name ){
		return( OFA_IFILE_META_GET_INTERFACE( meta )->get_provider_name( meta ));
	}

	return( NULL );
}

/**
 * ofa_ifile_meta_get_provider_instance:
 * @meta: this #ofaIFileMeta instance.
 *
 * Returns: a new reference to the provider instance which should be
 * g_object_unref() by the caller.
 */
ofaIDBProvider *
ofa_ifile_meta_get_provider_instance( const ofaIFileMeta *meta )
{
	g_return_val_if_fail( meta && OFA_IS_IFILE_META( meta ), NULL );

	if( OFA_IFILE_META_GET_INTERFACE( meta )->get_provider_instance ){
		return( OFA_IFILE_META_GET_INTERFACE( meta )->get_provider_instance( meta ));
	}

	return( NULL );
}

/**
 * ofa_ifile_meta_get_periods:
 * @meta: this #ofaIFileMeta instance.
 *
 * Returns: a list of defined financial periods (exercices) for this
 * file (dossier), as a #GList of #ofaIFilePeriod object, which should
 * be #ofa_ifile_meta_free_periods() by the caller.
 */
GList *
ofa_ifile_meta_get_periods( const ofaIFileMeta *meta )
{
	static const gchar *thisfn = "ofa_ifile_meta_get_periods";
	ofaIDBProvider *provider;
	GList *list;

	g_return_val_if_fail( meta && OFA_IS_IFILE_META( meta ), NULL );

	provider = ofa_ifile_meta_get_provider_instance( meta );
	if( !provider ){
		g_warning( "%s: unable to get a provider instance", thisfn );
		return( NULL );
	}
	list = ofa_idbprovider_get_dossier_periods( provider, meta );
	g_object_unref( provider );

	return( list );
}

/**
 * ofa_ifile_meta_get_settings:
 * @meta: this #ofaIFileMeta instance.
 *
 * Returns: the #mySettings object which was set when instanciating
 * this @meta instance.
 *
 * The returned reference is owned by the implementation, and should
 * not be freed by the caller.
 */
mySettings *
ofa_ifile_meta_get_settings( const ofaIFileMeta *meta )
{
	g_return_val_if_fail( meta && OFA_IS_IFILE_META( meta ), NULL );

	if( OFA_IFILE_META_GET_INTERFACE( meta )->get_settings ){
		return( OFA_IFILE_META_GET_INTERFACE( meta )->get_settings( meta ));
	}

	return( NULL );
}

/**
 * ofa_ifile_meta_get_group_name:
 * @meta: this #ofaIFileMeta instance.
 *
 * Returns: the name of the group which holds all dossier informations
 * in the settings file. This name was first set when instanciating
 * this @meta instance.
 *
 * The name is returned as a newly allocated string which should be
 * g_free() by the caller.
 */
gchar *
ofa_ifile_meta_get_group_name( const ofaIFileMeta *meta )
{
	g_return_val_if_fail( meta && OFA_IS_IFILE_META( meta ), NULL );

	if( OFA_IFILE_META_GET_INTERFACE( meta )->get_group_name ){
		return( OFA_IFILE_META_GET_INTERFACE( meta )->get_group_name( meta ));
	}

	return( NULL );
}
