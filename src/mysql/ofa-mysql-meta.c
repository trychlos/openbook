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

#include "ofa-mysql-editor-enter.h"
#include "ofa-mysql-idbprovider.h"
#include "ofa-mysql-meta.h"
#include "ofa-mysql-period.h"

/* priv instance data
 */
struct _ofaMySQLMetaPrivate {
	gboolean dispose_has_run;

	/* server connection infos
	 */
	gchar   *host;
	gchar   *socket;
	guint    port;
};

#define MYSQL_HOST_KEY                  "mysql-host"
#define MYSQL_SOCKET_KEY                "mysql-socket"
#define MYSQL_PORT_KEY                  "mysql-port"

static void            ifile_meta_iface_init( ofaIFileMetaInterface *iface );
static guint           ifile_meta_get_interface_version( const ofaIFileMeta *instance );
static void            ifile_meta_set_from_settings( ofaIFileMeta *instance, mySettings *settings, const gchar *group );
static void            ifile_meta_set_from_editor( ofaIFileMeta *instance, const ofaIDBEditor *editor, mySettings *settings, const gchar *group );
static GList          *load_periods( ofaIFileMeta *meta, mySettings *settings, const gchar *group );
static ofaMySQLPeriod *find_period( ofaMySQLPeriod *period, GList *list );
static void            ifile_meta_update_period( ofaIFileMeta *instance, ofaIFilePeriod *period, gboolean current, const GDate *begin, const GDate *end );
static void            ifile_meta_dump( const ofaIFileMeta *instance );

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
	iface->set_from_settings = ifile_meta_set_from_settings;
	iface->set_from_editor = ifile_meta_set_from_editor;
	iface->update_period = ifile_meta_update_period;
	iface->dump = ifile_meta_dump;
}

static guint
ifile_meta_get_interface_version( const ofaIFileMeta *instance )
{
	return( 1 );
}

static void
ifile_meta_set_from_settings( ofaIFileMeta *meta, mySettings *settings, const gchar *group )
{
	ofaMySQLMetaPrivate *priv;
	gint port;
	GList *periods;

	g_return_if_fail( meta && OFA_IS_MYSQL_META( meta ));

	priv = OFA_MYSQL_META( meta )->priv;

	if( !priv->dispose_has_run ){

		/* read connection informations from settings */
		priv->host = my_settings_get_string( settings, group, MYSQL_HOST_KEY );
		priv->socket = my_settings_get_string( settings, group, MYSQL_SOCKET_KEY );
		port = my_settings_get_uint( settings, group, MYSQL_PORT_KEY );
		priv->port = ( port >= 0 ? port : 0 );

		/* reload defined periods */
		periods = load_periods( meta, settings, group );
		ofa_ifile_meta_set_periods( meta, periods );
		ofa_ifile_meta_free_periods( periods );
	}
}

/*
 * returns the list the defined periods, making its best to reuse
 *  existing references
 */
static GList *
load_periods( ofaIFileMeta *meta, mySettings *settings, const gchar *group )
{
	GList *outlist, *prev_list;
	GList *keys, *itk;
	const gchar *cstr;
	ofaMySQLPeriod *new_period, *exist_period, *period;

	keys = my_settings_get_keys( settings, group );
	prev_list = ofa_ifile_meta_get_periods( meta );
	outlist = NULL;

	for( itk=keys ; itk ; itk=itk->next ){
		cstr = ( const gchar * ) itk->data;
		/* define a new period with the settings */
		new_period = ofa_mysql_period_new_from_settings( settings, group, cstr );
		if( new_period ){
			/* search for this period in the previous list */
			exist_period = find_period( new_period, prev_list );
			if( exist_period ){
				period = exist_period;
				g_object_unref( new_period );
			} else {
				period = new_period;
			}
			outlist = g_list_prepend( outlist, period );
		}
	}

	ofa_ifile_meta_free_periods( prev_list );
	my_settings_free_keys( keys );

	return( g_list_reverse( outlist ));
}

static ofaMySQLPeriod *
find_period( ofaMySQLPeriod *period, GList *list )
{
	GList *it;
	ofaMySQLPeriod *current;

	for( it=list ; it ; it=it->next ){
		current = ( ofaMySQLPeriod * ) it->data;
		if( ofa_ifile_period_compare( OFA_IFILE_PERIOD( current ), OFA_IFILE_PERIOD( period )) == 0 ){
			return( g_object_ref( current ));
		}
	}

	return( NULL );
}

static void
ifile_meta_set_from_editor( ofaIFileMeta *meta, const ofaIDBEditor *editor, mySettings *settings, const gchar *group )
{
	ofaMySQLMetaPrivate *priv;
	const gchar *host, *socket, *database;
	guint port;
	ofaMySQLPeriod *period;
	GList *periods;

	g_return_if_fail( meta && OFA_IS_MYSQL_META( meta ));
	g_return_if_fail( editor && OFA_IS_MYSQL_EDITOR_ENTER( editor ));

	priv = OFA_MYSQL_META( meta )->priv;

	if( !priv->dispose_has_run ){

		/* write connection informations to settings */
		host = ofa_mysql_editor_enter_get_host( OFA_MYSQL_EDITOR_ENTER( editor ));
		if( my_strlen( host )){
			my_settings_set_string( settings, group, MYSQL_HOST_KEY, host );
		}
		socket = ofa_mysql_editor_enter_get_socket( OFA_MYSQL_EDITOR_ENTER( editor ));
		if( my_strlen( socket )){
			my_settings_set_string( settings, group, MYSQL_SOCKET_KEY, socket );
		}
		port = ofa_mysql_editor_enter_get_port( OFA_MYSQL_EDITOR_ENTER( editor ));
		if( port > 0 ){
			my_settings_set_uint( settings, group, MYSQL_PORT_KEY, port );
		}

		/* initialize a new current period */
		database = ofa_mysql_editor_enter_get_database( OFA_MYSQL_EDITOR_ENTER( editor ));
		period = ofa_mysql_period_new_to_settings( settings, group, TRUE, NULL, NULL, database );
		periods = g_list_append( NULL, period );
		ofa_ifile_meta_set_periods( meta, periods );
		ofa_ifile_meta_free_periods( periods );
	}
}

static void
ifile_meta_update_period( ofaIFileMeta *instance,
		ofaIFilePeriod *period, gboolean current, const GDate *begin, const GDate *end )
{
	mySettings *settings;
	gchar *group;

	g_return_if_fail( instance && OFA_IS_MYSQL_META( instance ));
	g_return_if_fail( period && OFA_IS_MYSQL_PERIOD( period ));

	settings = ofa_ifile_meta_get_settings( instance );
	group = ofa_ifile_meta_get_group_name( instance );
	ofa_mysql_period_update( OFA_MYSQL_PERIOD( period ), settings, group, current, begin, end );

	g_free( group );
}

static void
ifile_meta_dump( const ofaIFileMeta *instance )
{
	static const gchar *thisfn = "ofa_mysql_meta_dump";
	ofaMySQLMetaPrivate *priv;

	priv = OFA_MYSQL_META( instance )->priv;
	g_debug( "%s: meta=%p", thisfn, ( void * ) instance );
	g_debug( "%s:   host=%s", thisfn, priv->host );
	g_debug( "%s:   socket=%s", thisfn, priv->socket );
	g_debug( "%s:   port=%u", thisfn, priv->port );
}

/**
 * ofa_mysql_meta_new:
 *
 * Returns: a newly allocated #ofaMySQLMeta object, which should
 * be #g_object_unref() by the caller.
 */
ofaMySQLMeta *
ofa_mysql_meta_new( void )
{
	ofaMySQLMeta *meta;

	meta = g_object_new( OFA_TYPE_MYSQL_META, NULL );

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

	g_return_val_if_reached( NULL );
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

	g_return_val_if_reached( NULL );
}

/**
 * ofa_mysql_meta_get_port:
 * @meta: this #ofaMySQLMeta object.
 *
 * Returns: the listening port of the dataserver, or zero for the
 * default value.
 */
guint
ofa_mysql_meta_get_port( const ofaMySQLMeta *meta )
{
	ofaMySQLMetaPrivate *priv;

	g_return_val_if_fail( meta && OFA_IS_MYSQL_META( meta ), 0 );

	priv = meta->priv;

	if( !priv->dispose_has_run ){

		return( priv->port );
	}

	g_return_val_if_reached( 0 );
}

/**
 * ofa_mysql_meta_add_period:
 * @meta: this #ofaMySQLMeta instance.
 * @current: whether the financial period (exercice) is current.
 * @begin: [allow-none]: the beginning date.
 * @end: [allow-none]: the ending date.
 * @database: the database name.
 *
 * Defines a new financial period with the provided datas.
 */
void
ofa_mysql_meta_add_period( ofaMySQLMeta *meta,
							gboolean current, const GDate *begin, const GDate *end, const gchar *database )
{
	ofaMySQLPeriod *period;
	mySettings *settings;
	gchar *group;

	g_return_if_fail( meta && OFA_IS_IFILE_META( meta ));

	settings = ofa_ifile_meta_get_settings( OFA_IFILE_META( meta ));
	group = ofa_ifile_meta_get_group_name( OFA_IFILE_META( meta ));

	period = ofa_mysql_period_new_to_settings( settings, group, current, begin, end, database );
	ofa_ifile_meta_add_period( OFA_IFILE_META( meta ), OFA_IFILE_PERIOD( period ));
	g_object_unref( period );

	g_free( group );
}
