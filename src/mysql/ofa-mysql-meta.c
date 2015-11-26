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
	gboolean dispose_has_run;

	/* server connection infos
	 */
	gchar   *host;
	gchar   *socket;
	gint     port;
};

#define MYSQL_HOST_KEY                  "mysql-host"
#define MYSQL_SOCKET_KEY                "mysql-socket"
#define MYSQL_PORT_KEY                  "mysql-port"

static void   ifile_meta_iface_init( ofaIFileMetaInterface *iface );
static guint  ifile_meta_get_interface_version( const ofaIFileMeta *instance );

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

	g_free( priv->host );
	g_free( priv->socket );

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
}

static guint
ifile_meta_get_interface_version( const ofaIFileMeta *instance )
{
	return( 1 );
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

	/* get server connection infos */
	priv->host = my_settings_get_string( settings, group, MYSQL_HOST_KEY );
	priv->socket = my_settings_get_string( settings, group, MYSQL_SOCKET_KEY );
	priv->port = my_settings_get_uint( settings, group, MYSQL_PORT_KEY );
	if( priv->port == -1 ){
		priv->port = 0;
	}

	return( meta );
}

/**
 * ofa_mysql_meta_get_host:
 * @meta: this #ofaMySQLMeta object.
 *
 * Returns: the hostname which hosts the dataserver.
 *
 * The returned string is owned by the @meta object, and should not be
 * freed by the caller.
 */
const gchar *
ofa_mysql_meta_get_host( const ofaMySQLMeta *meta )
{
	ofaMySQLMetaPrivate *priv;

	g_return_val_if_fail( meta && OFA_IS_MYSQL_META( meta ), NULL );

	priv = meta->priv;

	if( !priv->dispose_has_run ){
		return(( const gchar * ) priv->host );
	}

	return( NULL );
}

/**
 * ofa_mysql_meta_get_socket:
 * @meta: this #ofaMySQLMeta object.
 *
 * Returns: the listening socket path of the dataserver, or %NULL.
 *
 * The returned string is owned by the @meta object, and should not be
 * freed by the caller.
 */
const gchar *
ofa_mysql_meta_get_socket( const ofaMySQLMeta *meta )
{
	ofaMySQLMetaPrivate *priv;

	g_return_val_if_fail( meta && OFA_IS_MYSQL_META( meta ), NULL );

	priv = meta->priv;

	if( !priv->dispose_has_run ){
		return(( const gchar * ) priv->socket );
	}

	return( NULL );
}

/**
 * ofa_mysql_meta_get_port:
 * @meta: this #ofaMySQLMeta object.
 *
 * Returns: the listening port of the dataserver, or zero for the
 * default value.
 */
gint
ofa_mysql_meta_get_port( const ofaMySQLMeta *meta )
{
	ofaMySQLMetaPrivate *priv;

	g_return_val_if_fail( meta && OFA_IS_MYSQL_META( meta ), 0 );

	priv = meta->priv;

	if( !priv->dispose_has_run ){
		return( priv->port );
	}

	return( 0 );
}
