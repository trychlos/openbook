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

#include "api/ofa-idbconnect.h"

#include "ofa-mysql.h"
#include "ofa-mysql-connect.h"

/* priv instance data
 */
struct _ofaMySQLConnectPrivate {
	gboolean dispose_has_run;

	/* connection data
	 */
	MYSQL   *mysql;
	gchar   *account;
};

static void     idbconnect_iface_init( ofaIDBConnectInterface *iface );
static guint    idbconnect_get_interface_version( const ofaIDBConnect *instance );
static gboolean idbconnect_query( const ofaIDBConnect *instance, const gchar *query );
static gboolean idbconnect_query_ex( const ofaIDBConnect *instance, const gchar *query, GSList **result );
static gchar   *idbconnect_get_last_error( const ofaIDBConnect *instance );

G_DEFINE_TYPE_EXTENDED( ofaMySQLConnect, ofa_mysql_connect, G_TYPE_OBJECT, 0, \
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDBCONNECT, idbconnect_iface_init ));

static void
mysql_connect_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_connect_finalize";
	ofaMySQLConnectPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_CONNECT( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	priv = OFA_MYSQL_CONNECT( instance )->priv;

	/* free data members here */
	g_free( priv->mysql );
	g_free( priv->account );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_connect_parent_class )->finalize( instance );
}

static void
mysql_connect_dispose( GObject *instance )
{
	ofaMySQLConnectPrivate *priv;

	priv = OFA_MYSQL_CONNECT( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		mysql_close( priv->mysql );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_connect_parent_class )->dispose( instance );
}

static void
ofa_mysql_connect_init( ofaMySQLConnect *self )
{
	static const gchar *thisfn = "ofa_mysql_connect_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_MYSQL_CONNECT, ofaMySQLConnectPrivate );
}

static void
ofa_mysql_connect_class_init( ofaMySQLConnectClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_connect_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_connect_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_connect_finalize;

	g_type_class_add_private( klass, sizeof( ofaMySQLConnectPrivate ));
}

/*
 * ofaIDBConnect interface management
 */
static void
idbconnect_iface_init( ofaIDBConnectInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_connect_idbconnect_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idbconnect_get_interface_version;
	iface->query = idbconnect_query;
	iface->query_ex = idbconnect_query_ex;
	iface->get_last_error = idbconnect_get_last_error;
}

static guint
idbconnect_get_interface_version( const ofaIDBConnect *instance )
{
	return( 1 );
}

/*
 * an update/delete query (does not return any result)
 */
static gboolean
idbconnect_query( const ofaIDBConnect *instance, const gchar *query )
{
	ofaMySQLConnect *connect;
	gboolean ok;

	connect = OFA_MYSQL_CONNECT( instance );
	ok = ( mysql_query( connect->priv->mysql, query ) == 0 );

	return( ok );
}

static gboolean
idbconnect_query_ex( const ofaIDBConnect *instance, const gchar *query, GSList **result )
{
	ofaMySQLConnect *connect;
	gboolean ok;
	MYSQL_RES *res;
	MYSQL_ROW row;
	gint fields_count, i;

	ok = FALSE;
	*result = NULL;

	if( idbconnect_query( instance, query )){
		ok = TRUE;
		connect = OFA_MYSQL_CONNECT( instance );
		res = mysql_store_result( connect->priv->mysql );
		if( res ){
			fields_count = mysql_num_fields( res );
			while(( row = mysql_fetch_row( res ))){
				GSList *col = NULL;
				for( i=0 ; i<fields_count ; ++i ){
					col = g_slist_prepend( col, row[i] ? g_strdup( row[i] ) : NULL );
				}
				col = g_slist_reverse( col );
				*result = g_slist_prepend( *result, col );
			}
			*result = g_slist_reverse( *result );
		}
		mysql_free_result( res );
	}

	return( ok );
}

static gchar *
idbconnect_get_last_error( const ofaIDBConnect *instance )
{
	ofaMySQLConnect *connect;
	gchar *msg;

	connect = OFA_MYSQL_CONNECT( instance );
	msg = g_strdup( mysql_error( connect->priv->mysql ));

	return( msg );
}

/**
 * ofa_mysql_connect_new:
 * @meta: the #ofaMySQLMeta object which manages the dossier.
 * @period: the #ofaMySQLPeriod object which handles the exercice.
 * @account: the user account.
 * @password: the user password.
 * @msg: an error message placeholder.
 *
 * Returns: a reference to a new #ofaMySQLConnect object, or %NULL if
 * we are unable to establish a valid connection.
 *
 * The connection will be gracefully closed on g_object_unref().
 */
ofaMySQLConnect *
ofa_mysql_connect_new( const ofaMySQLMeta *meta, const ofaMySQLPeriod *period, const gchar *account, const gchar *password, gchar **msg )
{
	static const gchar *thisfn = "ofa_mysql_connect_new";
	ofaMySQLConnect *connect;
	ofaMySQLConnectPrivate *priv;
	MYSQL *mysql;
	const gchar *dbname;

	mysql = g_new0( MYSQL, 1 );
	mysql_init( mysql );
	dbname = ofa_mysql_period_get_database( period );

	if( !mysql_real_connect( mysql,
							ofa_mysql_meta_get_host( meta ),
							account,
							password,
							dbname,
							ofa_mysql_meta_get_port( meta ),
							ofa_mysql_meta_get_socket( meta ),
							CLIENT_MULTI_RESULTS )){

		/*
		g_debug( "%s: error: host=%s, socket=%s, port=%d, database=%s, account=%s, password=%s",
				thisfn, ofa_mysql_meta_get_host( meta ), ofa_mysql_meta_get_socket( meta ),
				ofa_mysql_meta_get_port( meta ), dbname, account, password );
				*/

		g_free( mysql );
		return( NULL );
	}

	g_debug( "%s: connect OK: database=%s, account=%s", thisfn, dbname, account );
	connect = g_object_new( OFA_TYPE_MYSQL_CONNECT, NULL );
	priv = connect->priv;
	priv->mysql = mysql;
	priv->account = g_strdup( account );

	return( connect );
}
