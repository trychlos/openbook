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

#include "api/my-utils.h"
#include "api/ofa-ifile-meta.h"

#include "ofa-mysql-idbprovider.h"
#include "ofa-mysql-meta.h"

/* priv instance data
 */
struct _ofaMySQLMetaPrivate {
	gboolean              dispose_has_run;

	/* initialization data
	 */
	const ofaIDBProvider *prov_instance;
	gchar                *dos_name;
	mySettings           *settings;
	gchar                *group_name;
};

static void            ifile_meta_iface_init( ofaIFileMetaInterface *iface );
static guint           ifile_meta_get_interface_version( const ofaIFileMeta *instance );
static gchar          *ifile_meta_get_dossier_name( const ofaIFileMeta *instance );
static gchar          *ifile_meta_get_provider_name( const ofaIFileMeta *instance );
static ofaIDBProvider *ifile_meta_get_provider_instance( const ofaIFileMeta *instance );
static mySettings     *ifile_meta_get_settings( const ofaIFileMeta *instance );
static gchar          *ifile_meta_get_group_name( const ofaIFileMeta *instance );

G_DEFINE_TYPE_EXTENDED( ofaMySQLMeta, ofa_mysql_meta, G_TYPE_OBJECT, 0, \
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IFILE_META, ifile_meta_iface_init ));

static void
mysql_meta_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_meta_finalize";
	ofaMySQLMetaPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_META( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	priv = OFA_MYSQL_META( instance )->priv;

	g_free( priv->dos_name );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_meta_parent_class )->finalize( instance );
}

static void
mysql_meta_dispose( GObject *instance )
{
	ofaMySQLMetaPrivate *priv;

	priv = OFA_MYSQL_META( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_meta_parent_class )->dispose( instance );
}

static void
ofa_mysql_meta_init( ofaMySQLMeta *self )
{
	static const gchar *thisfn = "ofa_mysql_meta_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_MYSQL_META, ofaMySQLMetaPrivate );
}

static void
ofa_mysql_meta_class_init( ofaMySQLMetaClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_meta_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_meta_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_meta_finalize;

	g_type_class_add_private( klass, sizeof( ofaMySQLMetaPrivate ));
}

/*
 * ofaIFileMeta interface management
 */
static void
ifile_meta_iface_init( ofaIFileMetaInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_meta_ifile_meta_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = ifile_meta_get_interface_version;
	iface->get_dossier_name = ifile_meta_get_dossier_name;
	iface->get_provider_name = ifile_meta_get_provider_name;
	iface->get_provider_instance = ifile_meta_get_provider_instance;
	iface->get_settings = ifile_meta_get_settings;
	iface->get_group_name = ifile_meta_get_group_name;
}

static guint
ifile_meta_get_interface_version( const ofaIFileMeta *instance )
{
	return( 1 );
}

static gchar *
ifile_meta_get_dossier_name( const ofaIFileMeta *instance )
{
	ofaMySQLMetaPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_META( instance ), NULL );

	priv = OFA_MYSQL_META( instance )->priv;

	if( !priv->dispose_has_run ){
		return( g_strdup( priv->dos_name ));
	}

	return( NULL );
}

static gchar *
ifile_meta_get_provider_name( const ofaIFileMeta *instance )
{
	ofaMySQLMetaPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_META( instance ), NULL );

	priv = OFA_MYSQL_META( instance )->priv;

	if( !priv->dispose_has_run ){
		return( g_strdup( ofa_mysql_idbprovider_get_provider_name( priv->prov_instance )));
	}

	return( NULL );
}

static mySettings *
ifile_meta_get_settings( const ofaIFileMeta *instance )
{
	ofaMySQLMetaPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_META( instance ), NULL );

	priv = OFA_MYSQL_META( instance )->priv;

	if( !priv->dispose_has_run ){
		return( priv->settings );
	}

	return( NULL );
}

static gchar *
ifile_meta_get_group_name( const ofaIFileMeta *instance )
{
	ofaMySQLMetaPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_META( instance ), NULL );

	priv = OFA_MYSQL_META( instance )->priv;

	if( !priv->dispose_has_run ){
		return( g_strdup( priv->group_name ));
	}

	return( NULL );
}

static ofaIDBProvider *
ifile_meta_get_provider_instance( const ofaIFileMeta *instance )
{
	ofaMySQLMetaPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_META( instance ), NULL );

	priv = OFA_MYSQL_META( instance )->priv;

	if( !priv->dispose_has_run ){
		return( g_object_ref(( gpointer ) priv->prov_instance ));
	}

	return( NULL );
}

/**
 * ofa_mysql_meta_new:
 * @instance: the #ofaIDBProvider which manages this dossier.
 * @dossier_name: the name of the dossier.
 * @settings: the dossier settings file provided by the application.
 * @group: the settings group name.
 *
 * Returns: a new reference to a #ofaMySQLMeta instance, which should
 * be #g_object_unref() by the caller.
 */
ofaMySQLMeta *
ofa_mysql_meta_new( const ofaIDBProvider *instance, const gchar *dossier_name, mySettings *settings, const gchar *group )
{
	ofaMySQLMeta *meta;
	ofaMySQLMetaPrivate *priv;

	meta = g_object_new( OFA_TYPE_MYSQL_META, NULL );
	priv = meta->priv;
	priv->prov_instance = instance;
	priv->dos_name = g_strdup( dossier_name );
	priv->settings = settings;
	priv->group_name = g_strdup( group );

	return( meta );
}
