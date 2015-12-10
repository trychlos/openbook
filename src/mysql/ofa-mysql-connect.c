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
#include <stdlib.h>

#include "api/my-utils.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-idbperiod.h"

#include "ofa-mysql.h"
#include "ofa-mysql-cmdline.h"
#include "ofa-mysql-connect.h"
#include "ofa-mysql-editor-enter.h"

/* priv instance data
 */
struct _ofaMySQLConnectPrivate {
	gboolean dispose_has_run;

	/* connection data
	 */
	MYSQL   *mysql;
	gchar   *host;
	gchar   *socket;
	guint    port;
	gchar   *database;
};

static void     idbconnect_iface_init( ofaIDBConnectInterface *iface );
static guint    idbconnect_get_interface_version( const ofaIDBConnect *instance );
static gboolean idbconnect_open_with_editor( ofaIDBConnect *instance, const gchar *account, const gchar *password, const ofaIDBEditor *editor, gboolean server_only );
static gboolean idbconnect_open_with_meta( ofaIDBConnect *instance, const gchar *account, const gchar *password, const ofaIDBMeta *meta, const ofaIDBPeriod *period );
static gboolean connect_open( ofaMySQLConnect *connect, const gchar *account, const gchar *password, const gchar *host, const gchar *socket, guint port, const gchar *database, gchar **msg );
static gboolean idbconnect_query( const ofaIDBConnect *instance, const gchar *query );
static gboolean idbconnect_query_ex( const ofaIDBConnect *instance, const gchar *query, GSList **result );
static gchar   *idbconnect_get_last_error( const ofaIDBConnect *instance );
static gboolean idbconnect_archive_and_new( const ofaIDBConnect *instance, const gchar *root_account, const gchar *root_password, const GDate *begin_next, const GDate *end_next );
static gboolean idbconnect_create_dossier( const ofaIDBConnect *instance, ofaIDBMeta *meta );
static gboolean idbconnect_grant_user( const ofaIDBConnect *instance, ofaIDBMeta *meta, const gchar *account, const gchar *password );
static MYSQL   *connect_to( const gchar *host, const gchar *socket, guint port, const gchar *dbname, const gchar *account, const gchar *password, gchar **msg );
static gchar   *find_new_database( ofaMySQLConnect *connect, const gchar *prev_database );
static gboolean local_get_db_exists( ofaMySQLConnect *connect, const gchar *dbname );

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
	g_free( priv->host );
	g_free( priv->socket );
	g_free( priv->database );

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
	iface->open_with_editor = idbconnect_open_with_editor;
	iface->open_with_meta = idbconnect_open_with_meta;
	iface->query = idbconnect_query;
	iface->query_ex = idbconnect_query_ex;
	iface->get_last_error = idbconnect_get_last_error;
	iface->archive_and_new = idbconnect_archive_and_new;
	iface->create_dossier = idbconnect_create_dossier;
	iface->grant_user = idbconnect_grant_user;
}

static guint
idbconnect_get_interface_version( const ofaIDBConnect *instance )
{
	return( 1 );
}

/*
 * tries to establish the connection with informations provided by
 * the @editor
 */
static gboolean
idbconnect_open_with_editor( ofaIDBConnect *instance, const gchar *account, const gchar *password, const ofaIDBEditor *editor, gboolean server_only )
{
	ofaMySQLConnectPrivate *priv;
	gboolean ok;
	const gchar *host, *socket, *database;
	guint port;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_CONNECT( instance ), FALSE );
	g_return_val_if_fail( editor && OFA_IS_MYSQL_EDITOR_ENTER( editor ), FALSE );

	priv = OFA_MYSQL_CONNECT( instance )->priv;

	if( !priv->dispose_has_run ){

		host = ofa_mysql_editor_enter_get_host( OFA_MYSQL_EDITOR_ENTER( editor ));
		socket = ofa_mysql_editor_enter_get_socket( OFA_MYSQL_EDITOR_ENTER( editor ));
		port = ofa_mysql_editor_enter_get_port( OFA_MYSQL_EDITOR_ENTER( editor ));
		database = server_only ? NULL : ofa_mysql_editor_enter_get_database( OFA_MYSQL_EDITOR_ENTER( editor ));

		ok = connect_open( OFA_MYSQL_CONNECT( instance ), account, password, host, socket, port, database, NULL );

		return( ok );
	}

	g_return_val_if_reached( FALSE );
}

/*
 * tries to establish the connection to the @period exercice of @meta
 * dossier
 */
static gboolean
idbconnect_open_with_meta( ofaIDBConnect *instance, const gchar *account, const gchar *password, const ofaIDBMeta *meta, const ofaIDBPeriod *period )
{
	ofaMySQLConnectPrivate *priv;
	gboolean ok;
	const gchar *host, *socket, *database;
	guint port;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_CONNECT( instance ), FALSE );
	g_return_val_if_fail( meta && OFA_IS_MYSQL_META( meta ), FALSE );
	g_return_val_if_fail( !period || OFA_IS_MYSQL_PERIOD( period ), FALSE );

	priv = OFA_MYSQL_CONNECT( instance )->priv;

	if( !priv->dispose_has_run ){

		host = ofa_mysql_meta_get_host( OFA_MYSQL_META( meta ));
		socket = ofa_mysql_meta_get_socket( OFA_MYSQL_META( meta ));
		port = ofa_mysql_meta_get_port( OFA_MYSQL_META( meta ));
		database = period ? ofa_mysql_period_get_database( OFA_MYSQL_PERIOD( period )) : NULL;

		ok = connect_open( OFA_MYSQL_CONNECT( instance ), account, password, host, socket, port, database, NULL );

		return( ok );
	}

	g_return_val_if_reached( FALSE );
}

/*
 * ofa_mysql_connect_open:
 * @connect: this #ofaMySQLConnect object.
 * @account: the user account.
 * @password: [allow-none]: the user password.
 * @host: [allow-none]: the DBMS host.
 * @socket: [allow-none]: the DBMS listening socket.
 * @port: [allow-none]: the DBMS listening port.
 * @database: [allow-none]: the database to be connected to.
 * @msg: [allow-none]: a placeholder for an error message.
 *
 * Establish (open) the connection to the named @database, or to the
 * DBMS server itself if @database is %NULL.
 *
 * Returns: %TRUE if the connection has been successfully opened.
 */
static gboolean
connect_open( ofaMySQLConnect *connect, const gchar *account, const gchar *password,
							const gchar *host, const gchar *socket, guint port, const gchar *database,
							gchar **msg )
{
	static const gchar *thisfn = "ofa_mysql_connect_open";
	ofaMySQLConnectPrivate *priv;
	MYSQL *mysql;
	gboolean ok;

	g_return_val_if_fail( connect && OFA_IS_MYSQL_CONNECT( connect ), FALSE );

	priv = connect->priv;

	if( !priv->dispose_has_run ){

		if( priv->mysql ){
			if( msg ){
				*msg = g_strdup_printf( _( "%s: connect=%p is already opened" ), thisfn, ( void * ) connect );
			}
			/* though the connection is supposed actually opened,
			 *  we signal the error */
			return( FALSE );
		}

		ok = FALSE;
		mysql = g_new0( MYSQL, 1 );
		mysql_init( mysql );
		mysql_options( mysql, MYSQL_SET_CHARSET_NAME, "utf8" );

		if( msg ){
			*msg = NULL;
		}

		if( mysql_real_connect( mysql, host, account, password, database, port, socket, CLIENT_MULTI_RESULTS )){
			ok = TRUE;
			priv->mysql = mysql;
			priv->host = g_strdup( host );
			priv->socket = g_strdup( socket );
			priv->database = g_strdup( database );
			g_debug( "%s: connection OK: database=%s, account=%s", thisfn, database, account );

		} else {
			if( msg ){
				*msg = g_strdup( mysql_error( mysql ));
			}
			g_free( mysql );
		}

		return( ok );
	}

	g_return_val_if_reached( FALSE );
}

/*
 * an insert/update/delete/drop query (does not return any result)
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

/**
 * ofa_mysql_connect_query:
 * @connect: this #ofaIDBConnect object.
 * @query: the query to be executed.
 *
 * Returns: %TRUE, if the query has been successfully executed, %FALSE
 * else.
 */
gboolean
ofa_mysql_connect_query( const ofaMySQLConnect *connect, const gchar *query )
{
	ofaMySQLConnectPrivate *priv;

	g_return_val_if_fail( connect && OFA_IS_MYSQL_CONNECT( connect ), FALSE );

	priv = connect->priv;

	if( !priv->dispose_has_run ){
		return( idbconnect_query( OFA_IDBCONNECT( connect ), query ));
	}

	g_return_val_if_reached( FALSE );
}

/*
 * a select query (returns a result, not audited)
 */
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

static gboolean
idbconnect_archive_and_new( const ofaIDBConnect *instance, const gchar *root_account, const gchar *root_password, const GDate *begin_next, const GDate *end_next )
{
	return( ofa_mysql_cmdline_archive_and_new( instance, root_account, root_password, begin_next, end_next ));
}

/*
 * @instance: a superuser connection on the DBMS server
 */
static gboolean
idbconnect_create_dossier( const ofaIDBConnect *instance, ofaIDBMeta *meta )
{
	static const gchar *thisfn = "ofa_mysql_connect_idbconnect_create_dossier";
	ofaMySQLConnectPrivate *priv;
	GString *query;
	ofaIDBPeriod *period;
	const gchar *database;
	gboolean ok;
	gchar *msg;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_CONNECT( instance ), FALSE );
	g_return_val_if_fail( meta && OFA_IS_IDBMETA( meta ), FALSE );

	priv = OFA_MYSQL_CONNECT( instance )->priv;

	if( !priv->dispose_has_run ){

		period = ofa_idbmeta_get_current_period( meta );
		g_return_val_if_fail( period && OFA_IS_IDBPERIOD( period ), FALSE );

		database = ofa_mysql_period_get_database( OFA_MYSQL_PERIOD( period ));
		query = g_string_new( "" );
		ok = FALSE;

		g_string_printf( query, "DROP DATABASE IF EXISTS %s", database );
		g_debug( "%s: %s", thisfn, query->str );
		if( idbconnect_query( instance, query->str )){
			g_string_printf( query, "CREATE DATABASE %s", database );
			g_debug( "%s: %s", thisfn, query->str );
			ok = idbconnect_query( instance, query->str );
			if( !ok ){
				msg = idbconnect_get_last_error( instance );
				g_warning( "%s: %s", thisfn, msg );
				g_free( msg );
			}
		}
		g_object_unref( period );
		return( ok );
	}

	g_return_val_if_reached( FALSE );
}

/*
 * @instance: a superuser connection on the dossier
 */
static gboolean
idbconnect_grant_user( const ofaIDBConnect *instance, ofaIDBMeta *meta, const gchar *account, const gchar *password )
{
	static const gchar *thisfn = "ofa_mysql_connect_idbconnect_grant_user";
	ofaMySQLConnectPrivate *priv;
	GString *query;
	gchar *hostname, *msg;
	ofaIDBPeriod *period;
	const gchar *database;
	gboolean ok;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_CONNECT( instance ), FALSE );
	g_return_val_if_fail( meta && OFA_IS_IDBMETA( meta ), FALSE );
	g_return_val_if_fail( my_strlen( account ), FALSE );

	priv = OFA_MYSQL_CONNECT( instance )->priv;

	if( !priv->dispose_has_run ){

		query = g_string_new( "" );

		hostname = g_strdup( ofa_mysql_meta_get_host( OFA_MYSQL_META( meta )));
		if( !my_strlen( hostname )){
			g_free( hostname );
			hostname = g_strdup( "localhost" );
		}

		/* doesn't trap error on create user as the user may already exist */
		g_string_printf( query,
				"CREATE USER '%s'@'%s' IDENTIFIED BY '%s'",
					account,
					hostname,
					password );
		g_debug( "%s: %s", thisfn, query->str );
		idbconnect_query( instance, query->str );
		ok = TRUE;

		period = ofa_idbmeta_get_current_period( meta );
		g_return_val_if_fail( period && OFA_IS_IDBPERIOD( period ), FALSE );
		database = ofa_mysql_period_get_database( OFA_MYSQL_PERIOD( period ));

		g_string_printf( query,
				"GRANT ALL ON %s.* TO '%s'@'%s' WITH GRANT OPTION",
					database,
					account,
					hostname );
		g_debug( "%s: %s", thisfn, query->str );
		if( !idbconnect_query( instance, query->str )){
			msg = idbconnect_get_last_error( instance );
			g_warning( "%s: %s", thisfn, msg );
			g_free( msg );
			ok = FALSE;
		}
		if( ok ){
			g_string_printf( query,
					"GRANT CREATE USER, FILE ON *.* TO '%s'@'%s'",
						account,
						hostname );
			g_debug( "%s: %s", thisfn, query->str );
			if( !idbconnect_query( instance, query->str )){
				msg = idbconnect_get_last_error( instance );
				g_warning( "%s: %s", thisfn, msg );
				g_free( msg );
				ok = FALSE;
			}
		}
		if( ok ){
			g_string_printf( query,
					"FLUSH PRIVILEGES" );
			g_debug( "%s: %s", thisfn, query->str );
			if( !idbconnect_query( instance, query->str )){
				msg = idbconnect_get_last_error( instance );
				g_warning( "%s: %s", thisfn, msg );
				g_free( msg );
				ok = FALSE;
			}
		}
		g_object_unref( period );
		g_string_free( query, TRUE );
		return( ok );
	}

	g_return_val_if_reached( FALSE );
}

/**
 * ofa_mysql_connect_new:
 *
 * Returns: a newly instanciated #ofaMySQLConnect object, which should
 * be g_object_unref() by the caller.
 *
 * At the time of this instanciation, no DBMS connection is actually
 * opened. Once opened, the connection will be gracefully closed on
 * g_object_unref().
 */
ofaMySQLConnect *
ofa_mysql_connect_new( void )
{
	ofaMySQLConnect *connect;

	connect = g_object_new( OFA_TYPE_MYSQL_CONNECT, NULL );

	return( connect );
}

/**
 * ofa_mysql_connect_new_for_meta_period:
 * @meta: the #ofaMySQLMeta object which manages the dossier.
 * @period: [allow-none]: the #ofaMySQLPeriod object which handles the exercice.
 * @account: the user account.
 * @password: the user password.
 * @msg: an error message placeholder.
 *
 * Returns: a reference to a new #ofaMySQLConnect object, or %NULL if
 * we are unable to establish a valid connection.
 *
 * The connection will be gracefully closed on g_object_unref().
 *
 * @meta, @period, @account arguments are expected to be stored as
 * interface data by #ofaIDBProvider interface code.
 */
ofaMySQLConnect *
ofa_mysql_connect_new_for_meta_period( const ofaMySQLMeta *meta, const ofaMySQLPeriod *period,
											const gchar *account, const gchar *password, gchar **msg )
{
	ofaMySQLConnect *connect;
	MYSQL *mysql;

	g_return_val_if_fail( meta && OFA_IS_MYSQL_META( meta ), NULL );
	g_return_val_if_fail( !period || OFA_IS_MYSQL_PERIOD( period ), NULL );

	connect = NULL;

	mysql = connect_to(
					ofa_mysql_meta_get_host( meta ),
					ofa_mysql_meta_get_socket( meta ),
					ofa_mysql_meta_get_port( meta ),
					period ? ofa_mysql_period_get_database( period ) : NULL,
					account,
					password,
					msg );
	if( mysql ){
		connect = g_object_new( OFA_TYPE_MYSQL_CONNECT, NULL );
		connect->priv->mysql = mysql;
	}

	return( connect );
}

/**
 * ofa_mysql_connect_new_for_server:
 * @host: [allow-none]: the hostname which hosts the DB server.
 * @socket: [allow-none]: the listening socket path of the DB server.
 * @port: the listening port of the DB server
 * @account: the superuser account.
 * @password: the superuser password.
 * @msg: an error message placeholder.
 *
 * Returns: a reference to a new #ofaMySQLConnect object, or %NULL if
 * we are unable to establish a valid connection.
 *
 * The connection will be gracefully closed on g_object_unref().
 */
ofaMySQLConnect *
ofa_mysql_connect_new_for_server( const gchar *host, const gchar *socket, guint port,
									const gchar *account, const gchar *password, gchar **msg )
{
	ofaMySQLConnect *connect;
	MYSQL *mysql;

	connect = NULL;
	mysql = connect_to( host, socket, port, NULL, account, password, msg );
	if( mysql ){
		connect = g_object_new( OFA_TYPE_MYSQL_CONNECT, NULL );
		connect->priv->mysql = mysql;
	}

	return( connect );
}

/*
 * host: may be %NULL, defaults to localhost
 * socket: may be %NULL (defaults to ?)
 * port, may be zero, defaults to 3306
 * dbname: may be %NULL, defaults to none (db server connection)
 * account: may be %NULL, defaults to none
 * password: may be %NULL, defaults to empty
 * msg: may be %NULL, default to no returned message if an error occurs
 */
static MYSQL *
connect_to( const gchar *host, const gchar *socket, guint port, const gchar *dbname,
				const gchar *account, const gchar *password, gchar **msg )
{
	static const gchar *thisfn = "ofa_mysql_connect_connect_to";
	MYSQL *mysql;

	mysql = g_new0( MYSQL, 1 );
	mysql_init( mysql );
	mysql_options( mysql, MYSQL_SET_CHARSET_NAME, "utf8" );
	if( msg ){
		*msg = NULL;
	}

	if( !mysql_real_connect(
			mysql, host, account, password, dbname, port, socket, CLIENT_MULTI_RESULTS )){

		if( msg ){
			*msg = g_strdup( mysql_error( mysql ));
		}
		/*
		g_debug( "%s: error: host=%s, socket=%s, port=%d, database=%s, account=%s, password=%s",
				thisfn, ofa_mysql_meta_get_host( meta ), ofa_mysql_meta_get_socket( meta ),
				ofa_mysql_meta_get_port( meta ), dbname, account, password );
				*/

		g_free( mysql );
		return( NULL );
	}

	g_debug( "%s: connection OK: database=%s, account=%s", thisfn, dbname, account );

	return( mysql );
}

/**
 * ofa_mysql_connect_get_new_database:
 * @connect: this #ofaMySQLConnect *instance.
 *  The @connect is expected to come from a server-level root connection,
 *  so does not have any meta nor period data members set. Only the
 *  MySQL connection itself is active.
 * @prev_database: the previous database name, which serves here as a
 *  template.
 *
 * Returns: the name of a new database (which does not yet exists), as
 * a newly allocated string which should be g_free() by the caller.
 */
gchar *
ofa_mysql_connect_get_new_database( ofaMySQLConnect *connect, const gchar *prev_database )
{
	ofaMySQLConnectPrivate *priv;

	g_return_val_if_fail( connect && OFA_IS_MYSQL_CONNECT( connect ), NULL );
	g_return_val_if_fail( my_strlen( prev_database ), NULL );

	priv = connect->priv;

	if( !priv->dispose_has_run ){
		return( find_new_database( connect, prev_database ));
	}

	return( NULL );
}

/*
 * Search for a suitable new database name with same radical and '_[0-9]'
 * sufix. If current dbname is already sufixed with '_[0-9]', then just
 * increment the existing sufix.
 */
static gchar *
find_new_database( ofaMySQLConnect *connect, const gchar *prev_database )
{
	static const gchar *thisfn = "ofa_mysql_connect_find_new_database";
	gchar *candidate, *prefix, *newdb, *p;
	gboolean exists;
	gint i;

	newdb = NULL;

	p = g_strrstr( prev_database, "_" );
	/* if the original db name contains itself some underscores,
	 * then ignore them */
	if( p && atoi( p+1 ) == 0 ){
		p = NULL;
	}
	if( p ){
		prefix = g_strdup( prev_database );
		prefix[p-prev_database] = '\0';
		i = atoi( p+1 );
	} else {
		prefix = g_strdup( prev_database );
		i = 0;
	}
	g_debug( "%s: dbname=%s, prefix=%s, i=%d", thisfn, prev_database, prefix, i );
	while( TRUE ){
		i += 1;
		candidate = g_strdup_printf( "%s_%d", prefix, i );
		exists = local_get_db_exists( connect, candidate );
		g_debug( "%s: candidate=%s, exists=%s", thisfn, candidate, exists ? "True":"False" );
		if( !exists ){
			newdb = candidate;
			break;
		}
		g_free( candidate );
	}

	g_free( prefix );

	return( newdb );
}

static gboolean
local_get_db_exists( ofaMySQLConnect *connect, const gchar *dbname )
{
	ofaMySQLConnectPrivate *priv;
	gboolean exists;
	MYSQL_RES *result;

	priv = connect->priv;
	exists = FALSE;

	result = mysql_list_dbs( priv->mysql, dbname );
	if( result ){
		if( mysql_fetch_row( result )){
			exists = TRUE;
		}
		mysql_free_result( result );
	}

	return( exists );
}
