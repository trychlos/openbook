/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-dossier-misc.h"
#include "api/ofa-idbms.h"
#include "api/ofa-settings.h"

#include "ofa-mysql.h"
#include "ofa-mysql-backup.h"
#include "ofa-mysql-idbms.h"
#include "ofa-mysql-connect-display-piece.h"
#include "ofa-mysql-connect-enter-piece.h"

/*
 * this structure is attached to the GtkContainer parent of the grid
 * (i.e. the container from the DossierNewDlg dialog box)
 */
typedef struct {
	const ofaIDbms *module;
	mysqlInfos     sInfos;
	gboolean        connect_ok;			/* connection (host, port, socket, account, password) is ok */
	GtkLabel       *msg;
	GtkButton      *browse_btn;
	gboolean        db_ok;				/* database name is set */
	gboolean        db_exists;
	gint            db_exists_mode;

	/* setup when appyling
	 */
	const gchar    *dname;
	const gchar    *account;
	const gchar    *password;
}
	sPrivate;

static guint      idbms_get_interface_version( const ofaIDbms *instance );
static void      *idbms_connect( const ofaIDbms *instance,const gchar *dname, const gchar *dbname, const gchar *account, const gchar *password );
static gboolean   idbms_connect_ex( const ofaIDbms *instance,void *infos, const gchar *account, const gchar *password );
static void       setup_infos( mysqlInfos *infos );
static void       idbms_close( const ofaIDbms *instance, void *handle );
static GSList    *idbms_get_exercices( const ofaIDbms *instance, const gchar *dname );
static gchar     *idbms_get_current( const ofaIDbms *instance, const gchar *dname );
static gchar     *exercice_get_description( const gchar *dname, const gchar *key );
static void       idbms_set_current( const ofaIDbms *instance, const gchar *dname, const GDate *begin, const GDate *end );
static gboolean   idbms_query( const ofaIDbms *instance, void *handle, const gchar *query );
static gboolean   idbms_query_ex( const ofaIDbms *instance, void *handle, const gchar *query, GSList **result );
static gchar     *idbms_last_error( const ofaIDbms *instance, void *handle );
static gchar     *find_new_database( mysqlInfos *infos, const gchar *dbname );
static gboolean   local_get_db_exists( MYSQL *mysql, const gchar *dbname );
static gboolean   idbms_new_dossier( const ofaIDbms *instance, const gchar *dname, const gchar *root_account, const gchar *root_password );
static gboolean   idbms_grant_user( const ofaIDbms *instance, const gchar *dname, const gchar *root_account, const gchar *root_password, const gchar *user_account, const gchar *user_password );

/*
 * #ofaIDbms interface setup
 */
void
ofa_mysql_idbms_iface_init( ofaIDbmsInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_idbms_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idbms_get_interface_version;
	iface->get_provider_name = ofa_mysql_idbms_get_provider_name;
	iface->connect = idbms_connect;
	iface->connect_ex = idbms_connect_ex;
	iface->close = idbms_close;
	iface->get_exercices = idbms_get_exercices;
	iface->get_current = idbms_get_current;
	iface->set_current = idbms_set_current;
	iface->query = idbms_query;
	iface->query_ex = idbms_query_ex;
	iface->last_error = idbms_last_error;
	iface->connect_display_attach_to = ofa_mysql_connect_display_piece_attach_to;
	iface->connect_enter_attach_to = ofa_mysql_connect_enter_piece_attach_to;
	iface->connect_enter_is_valid = ofa_mysql_connect_enter_piece_is_valid;
	iface->connect_enter_get_database = ofa_mysql_connect_enter_piece_get_database;
	iface->connect_enter_apply = ofa_mysql_connect_enter_piece_apply;
	iface->new_dossier = idbms_new_dossier;
	iface->grant_user = idbms_grant_user;
	iface->backup = ofa_mysql_backup;
	iface->restore = ofa_mysql_restore;
	iface->archive = ofa_mysql_archive;
#if 0
	iface->get_dossier_host = idbms_get_dossier_host;
	iface->get_dossier_dbname = idbms_get_dossier_dbname;
	iface->delete_dossier = idbms_delete_dossier;
#endif
}

/*
 * the version of the #ofaIDbms interface implemented by the module
 */
static guint
idbms_get_interface_version( const ofaIDbms *instance )
{
	return( 1 );
}

/*
 * the provider name identifier
 */
const gchar *
ofa_mysql_idbms_get_provider_name( const ofaIDbms *instance )
{
	return( "MySQL" );
}

/*
 * connect to the specified instance and database
 */
static void *
idbms_connect( const ofaIDbms *instance,
								const gchar *dname, const gchar *dbname,
								const gchar *account, const gchar *password )
{
	mysqlInfos *infos;

	infos = g_new0( mysqlInfos, 1 );
	infos->dname = g_strdup( dname );
	infos->dbname = g_strdup( dbname ? dbname : "mysql" );
	infos->account = g_strdup( account );
	infos->password = g_strdup( password );

	setup_infos( infos );
	if( !ofa_mysql_connect_with_infos( infos )){
		ofa_mysql_free_connect_infos( infos );
		g_free( infos );
		infos = NULL;
	}

	return( infos );
}

/*
 * Populate the sMySQL structure with informations from the settings
 * NB: the database name is an intrant
 */
static void
setup_infos( mysqlInfos *infos )
{
	infos->host = ofa_settings_dossier_get_string( infos->dname, SETTINGS_HOST );
	infos->socket = ofa_settings_dossier_get_string( infos->dname, SETTINGS_SOCKET );
	infos->port = ofa_settings_dossier_get_int( infos->dname, SETTINGS_PORT );
	if( infos->port == -1 ){
		infos->port = 0;
	}
}

/*
 * check the DBMS connection
 */
static gboolean
idbms_connect_ex( const ofaIDbms *instance,
								void *infos,
								const gchar *account, const gchar *password )
{
	static const gchar *thisfn = "ofa_mysql_idbms_idbms_connect_ex";
	mysqlInfos *src_infos, *dest_infos;
	gboolean ok;

	g_debug( "%s: instance=%p, infos=%p", thisfn, ( void * ) instance, ( void * ) infos );

	ok = FALSE;

	if( infos ){
		src_infos = ( mysqlInfos * ) infos;
		dest_infos = g_new0( mysqlInfos, 1 );
		dest_infos->host = g_strdup( src_infos->host );
		dest_infos->port = src_infos->port;
		dest_infos->socket = g_strdup( src_infos->socket );
		dest_infos->account = g_strdup( account );
		dest_infos->password = g_strdup( password );
		dest_infos->dbname = g_strdup( "mysql" );

		ok = ofa_mysql_connect_with_infos( dest_infos );

		ofa_mysql_free_connect_infos( dest_infos );
		g_free( dest_infos );
	}

	return( ok );
}

/*
 * dbname is already set, whether it has been explicitely specified or
 * it comes from the dossier
 */
gboolean
ofa_mysql_connect_with_infos( mysqlInfos *infos )
{
	static const gchar *thisfn = "ofa_mysql_connect_with_infos";
	MYSQL *mysql;

	mysql = g_new0( MYSQL, 1 );
	mysql_init( mysql );

	if( !mysql_real_connect( mysql,
							infos->host,
							infos->account,
							infos->password,
							infos->dbname,
							infos->port,
							infos->socket,
							CLIENT_MULTI_RESULTS )){
		g_free( mysql );
		g_debug( "%s: dname=%s, dbname=%s, account=%s, password=%s, host=%s, port=%d, socket=%s: unable to connect",
				thisfn, infos->dname, infos->dbname, infos->account, infos->password, infos->host, infos->port, infos->socket );
		return( FALSE );
	}

	g_debug( "%s: connect OK: database=%s, account=%s", thisfn, infos->dbname, infos->account );
	infos->mysql = mysql;

	return( TRUE );
}

/**
 * ofa_mysql_free_connect_infos:
 *
 * Fully free the mysqlInfos structure (but not including the structure
 * iself).
 */
void
ofa_mysql_free_connect_infos( mysqlInfos *infos )
{
	g_free( infos->dname );
	g_free( infos->dbname );
	g_free( infos->account );
	g_free( infos->password );
	g_free( infos->host );
	g_free( infos->socket );
}

/*
 * close the opened instance
 *
 * This function is supposed to be the matching of idbms_connect()
 * which allocates a new mysqlInfos structure. So we free it here.
 */
static void
idbms_close( const ofaIDbms *instance, void *handle )
{
	mysqlInfos *infos;

	infos = ( mysqlInfos * ) handle;

	mysql_close( infos->mysql );
	ofa_mysql_free_connect_infos( infos );
	g_free( infos );
}

/*
 * returned the known exercices
 *
 * MySQLDatabase=<database>;<begin>;<end>;
 * MySQLDatabase_<end>=<database>;<begin>;
 *
 * where <begin> and <end> are 'yyyymmdd' dates.
 *
 * Rationale:
 * 1/ in order to keep the database size at a stable size, the database
 *    only contains one exercice
 * 2/ as a consequence, each exercice is contained in its own database
 * 3/ we want propose to the user to open each database/exercice by
 *    showing him the status of the exercice: whether it is opened or
 *    archived, and the begin and end dates
 * 4/ we so required that these datas be available in the settings
 *
 * When restoring:
 * 1/ user may choose between restoring in an existing database, thus
 *    overwriting its content, or restoring to a new database
 * 2/ when restoring to an existing database, the settings must be
 *    updated to reflect the status of the restored content
 * 3/ idem when restoring to a new database
 * 4/ as a consequence, the two options may lead to a situation where
 *    the user will have several databases with the same content
 *
 * or:
 * Openbook software may force the user to only restore to its current
 * database, which happens to be the simplest solution.
 */
static GSList *
idbms_get_exercices( const ofaIDbms *instance, const gchar *dname )
{
	GSList *keys_list, *it;
	GSList *out_list;
	const gchar *cstr;
	gchar *line;

	out_list = NULL;
	keys_list = ofa_settings_dossier_get_keys( dname );

	for( it=keys_list ; it ; it=it->next ){
		cstr = ( const gchar * ) it->data;
		if( g_str_has_prefix( cstr, SETTINGS_DATABASE )){
			line = exercice_get_description( dname, cstr );
			out_list = g_slist_prepend( out_list, line );
		}
	}

	ofa_settings_dossier_free_keys( keys_list );

	return( out_list );
}

/*
 * returned the current exercice description as a semi-colon separated
 * string.
 */
static gchar *
idbms_get_current( const ofaIDbms *instance, const gchar *dname )
{
	gchar *out_value;

	out_value = exercice_get_description( dname, SETTINGS_DATABASE );

	return( out_value );
}

/*
 * returned the current exercice description as a semi-colon separated
 * string:
 * - a displayable label
 * - the database name
 * - the begin of exercice yyyy-mm-dd
 * - the end of exercice yyyy-mm-dd
 * - the status
 */
static gchar *
exercice_get_description( const gchar *dname, const gchar *key )
{
	GList *strlist, *it;
	const gchar *sdb, *sbegin;
	gchar *send;
	gchar **array;
	gboolean is_current;
	GDate begin, end;
	gchar *label, *svalue, *sdbegin, *sdend, *status;

	strlist = ofa_settings_dossier_get_string_list( dname, key );

	sdb = sbegin = send = NULL;
	it = strlist;
	sdb = ( const gchar * ) it->data;
	it = it->next;
	if( it ){
		sbegin = ( const gchar * ) it->data;
	}
	if( g_utf8_collate( key, SETTINGS_DATABASE )){
		array = g_strsplit( key, "_", -1 );
		send = g_strdup( *(array+1 ));
		g_strfreev( array );
		is_current = FALSE;
	} else {
		it = it ? it->next : NULL;
		if( it ){
			send = g_strdup(( const gchar * ) it->data );
		}
		is_current = TRUE;
	}

	status = g_strdup( is_current ? _( "Current" ) : _( "Archived" ));

	my_date_set_from_str( &begin, sbegin, MY_DATE_YYMD );
	sdbegin = my_date_to_str( &begin, MY_DATE_SQL );

	my_date_set_from_str( &end, send, MY_DATE_YYMD );
	sdend = my_date_to_str( &end, MY_DATE_SQL );

	label = ofa_dossier_misc_get_exercice_label( &begin, &end, is_current );

	svalue = g_strdup_printf( "%s;%s;%s;%s;%s;", label, sdb, sdbegin, sdend, status );

	g_free( label );
	g_free( send );
	g_free( sdbegin );
	g_free( sdend );
	g_free( status );

	ofa_settings_free_string_list( strlist );

	return( svalue );
}

/*
 * set the settings with the dates of the current exercice
 */
static void
idbms_set_current( const ofaIDbms *instance, const gchar *dname, const GDate *begin, const GDate *end )
{
	GList *list, *it;
	gchar *dbname, *sbegin, *send, *str;

	list = ofa_settings_dossier_get_string_list( dname, SETTINGS_DATABASE );
	dbname = NULL;
	it = list;
	if( it ){
		dbname = g_strdup(( const gchar * ) it->data );
	}
	ofa_settings_free_string_list( list );

	sbegin = my_date_to_str( begin, MY_DATE_YYMD );
	send = my_date_to_str( end, MY_DATE_YYMD );
	str = g_strdup_printf( "%s;%s;%s;", dbname, sbegin, send );

	ofa_settings_dossier_set_string( dname, SETTINGS_DATABASE, str );

	g_free( dbname );
	g_free( str );
	g_free( sbegin );
	g_free( send );
}

/*
 * move the current exercice as an archived one
 * define a new current exercice with the provided dates
 */
void
ofa_mysql_set_new_exercice( const ofaIDbms *instance, const gchar *dname, const gchar *dbname, const GDate *begin, const GDate *end )
{
	GList *slist, *it;
	const gchar *sdb, *sbegin, *send;
	gchar *key, *content, *sbegin_next, *send_next;

	/* move current exercice to archived */
	slist = ofa_settings_dossier_get_string_list( dname, SETTINGS_DATABASE );

	sdb = sbegin = send = NULL;
	it = slist;
	sdb = ( const gchar * ) it->data;
	it = it ? it->next : NULL;
	sbegin = it ? ( const gchar * ) it->data : NULL;
	it = it ? it->next : NULL;
	send = it ? ( const gchar * ) it->data : NULL;
	g_return_if_fail( sdb && sbegin && send );

	key = g_strdup_printf( "%s_%s", SETTINGS_DATABASE, send );
	content = g_strdup_printf( "%s;%s;", sdb, sbegin );

	ofa_settings_dossier_set_string( dname, key, content );

	ofa_settings_free_string_list( slist );
	g_free( key );
	g_free( content );

	/* define new current exercice */
	sbegin_next = my_date_to_str( begin, MY_DATE_YYMD );
	send_next = my_date_to_str( end, MY_DATE_YYMD );
	content = g_strdup_printf( "%s;%s;%s;", dbname, sbegin_next, send_next );

	ofa_settings_dossier_set_string( dname, SETTINGS_DATABASE, content );

	g_free( content );
	g_free( sbegin_next );
	g_free( send_next );
}

static gboolean
idbms_query( const ofaIDbms *instance, void *handle, const gchar *query )
{
	static const gchar *thisfn = "ofa_mysql_idbms_query";
	gboolean query_ok;
	mysqlInfos *infos;

	query_ok = FALSE;
	infos = ( mysqlInfos * ) handle;

	if( infos && infos->mysql ){
		query_ok = ( mysql_query( infos->mysql, query ) == 0 );
	} else {
		g_warning( "%s: trying to querying a non-opened connection", thisfn );
	}

	return( query_ok );
}

static gboolean
idbms_query_ex( const ofaIDbms *instance, void *handle, const gchar *query, GSList **result )
{
	gboolean ok;
	mysqlInfos *infos;
	MYSQL_RES *res;
	MYSQL_ROW row;
	gint fields_count, i;

	ok = FALSE;
	*result = NULL;

	if( idbms_query( instance, handle, query )){

		ok = TRUE;
		infos = ( mysqlInfos * ) handle;
		res = mysql_store_result( infos->mysql );
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
idbms_last_error( const ofaIDbms *instance, void *handle )
{
	static const gchar *thisfn = "ofa_mysql_idbms_error";
	gchar *msg;
	mysqlInfos *infos;

	msg = NULL;
	infos = ( mysqlInfos * ) handle;

	if( infos && infos->mysql ){
		msg = g_strdup( mysql_error( infos->mysql ));
	} else {
		g_warning( "%s: trying to querying a non-opened connection", thisfn );
	}

	return( msg );
}

/**
 * ofa_mysql_get_connect_infos:
 * @dname: the name of the dossier in user's settings
 *
 * Allocate a new connection structure, filling it with the connection
 * informations got from the settings, up to and including the database
 * for the current exercice.
 *
 * Return: a newly allocated connection structure, which should be
 * #ofa_mysql_free_connect_infos() and #g_free() by the caller.
 */
mysqlInfos *
ofa_mysql_get_connect_infos( const gchar *dname )
{
	mysqlInfos *infos;
	GList *slist;

	infos = g_new0( mysqlInfos, 1 );
	infos->dname = g_strdup( dname );
	setup_infos( infos );

	slist = ofa_settings_dossier_get_string_list( dname, SETTINGS_DATABASE );
	infos->dbname = g_strdup(( const gchar * ) slist->data );
	ofa_settings_free_string_list( slist );

	return( infos );
}

/**
 * ofa_mysql_get_connect_newdb_infos:
 * @dname: the name of the dossier in user's settings
 * @root_account:
 * @root_password:
 * @prev_dbname: [out]:
 *
 * Allocate a new connection structure, filling it with the connection
 * informations got from the settings, allocating a new database name
 * for a new exercice.
 *
 * Return: a newly allocated connection structure, which should be
 * #ofa_mysql_free_connect_infos() and #g_free() by the caller.
 */
mysqlInfos *
ofa_mysql_get_connect_newdb_infos( const gchar *dname, const gchar *root_account, const gchar *root_password, gchar **prev_dbname )
{
	mysqlInfos *infos;
	gchar *dbname, *newdb;

	infos = ofa_mysql_get_connect_infos( dname );
	dbname = infos->dbname;
	*prev_dbname = g_strdup( infos->dbname );

	infos->dbname = NULL;
	infos->account = g_strdup( root_account );
	infos->password = g_strdup( root_password );

	newdb = find_new_database( infos, dbname );
	g_free( dbname );
	infos->dbname = newdb;

	return( infos );
}

static gchar *
find_new_database( mysqlInfos *infos, const gchar *dbname )
{
	static const gchar *thisfn = "ofa_mysql_idbms_find_new_database";
	gchar *candidate, *prefix, *newdb, *p;
	gboolean exists;
	gint i;

	newdb = NULL;

	if( ofa_mysql_connect_with_infos( infos )){

		p = g_strrstr( dbname, "_" );
		if( p ){
			prefix = g_strdup( dbname );
			prefix[p-dbname] = '\0';
			i = atoi( p+1 );
		} else {
			prefix = g_strdup( dbname );
			i = 0;
		}
		while( TRUE ){
			i += 1;
			candidate = g_strdup_printf( "%s_%d", prefix, i );
			exists = local_get_db_exists( infos->mysql, candidate );
			g_debug( "%s: candidate=%s, exists=%s", thisfn, candidate, exists ? "True":"False" );
			if( !exists ){
				newdb = candidate;
				break;
			}
			g_free( candidate );
		}

		g_free( prefix );
		mysql_close( infos->mysql );
		infos->mysql = NULL;
	}

	return( newdb );
}

static gboolean
local_get_db_exists( MYSQL *mysql, const gchar *dbname )
{
	gboolean exists;
	MYSQL_RES *result;

	exists = FALSE;

	result = mysql_list_dbs( mysql, dbname );
	if( result ){
		if( mysql_fetch_row( result )){
			exists = TRUE;
		}
		mysql_free_result( result );
	}

	return( exists );
}

/*
 * Create the empty database through a global connection to dataserver
 * - create database
 */
static gboolean
idbms_new_dossier( const ofaIDbms *instance, const gchar *dname, const gchar *root_account, const gchar *root_password )
{
	static const gchar *thisfn = "ofa_mysql_idbms_new_dossier";
	mysqlInfos *infos;
	gchar *dbname;
	GString *stmt;
	gboolean db_created;

	g_debug( "%s: instance=%p, dname=%s, root_account=%s",
			thisfn, ( void * ) instance, dname, root_account );

	/* first, connect to 'mysql' central DB to define the database
	 * wrapper */
	infos = ofa_mysql_get_connect_infos( dname );
	dbname = infos->dbname;
	infos->dbname = g_strdup( "mysql" );
	infos->account = g_strdup( root_account );
	infos->password = g_strdup( root_password );

	if( !ofa_mysql_connect_with_infos( infos )){
		idbms_close( instance, infos );
		return( FALSE );
	}

	db_created = FALSE;
	stmt = g_string_new( "" );

	g_string_printf( stmt, "DROP DATABASE IF EXISTS %s", dbname );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	mysql_query( infos->mysql, stmt->str );

	g_string_printf( stmt, "CREATE DATABASE %s", dbname );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	if( mysql_query( infos->mysql, stmt->str )){
		g_warning( "%s: %s", thisfn, mysql_error( infos->mysql ));
		goto free_stmt;
	}

	idbms_close( instance, infos );
	g_free( dbname );

	/* then connect to the dossier database to initialize the service
	 * tables */
	infos = ofa_mysql_get_connect_infos( dname );
	infos->account = g_strdup( root_account );
	infos->password = g_strdup( root_password );

	if( !ofa_mysql_connect_with_infos( infos )){
		idbms_close( instance, infos );
		return( FALSE );
	}

	g_string_printf( stmt,
			"CREATE TABLE IF NOT EXISTS %s.OFA_T_AUDIT ("
			"	AUD_ID    INTEGER AUTO_INCREMENT NOT NULL UNIQUE COMMENT 'Intern identifier',"
			"	AUD_STAMP TIMESTAMP              NOT NULL        COMMENT 'Query timestamp',"
			"	AUD_QUERY VARCHAR(4096)          NOT NULL        COMMENT 'Query content') "
			"CHARACTER SET utf8",
					infos->dbname );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	if( mysql_query( infos->mysql, stmt->str )){
		g_warning( "%s: %s", thisfn, mysql_error( infos->mysql ));
		goto free_stmt;
	}

	g_string_printf( stmt,
			"CREATE TABLE IF NOT EXISTS %s.OFA_T_ROLES ("
				"ROL_USER     VARCHAR(20) BINARY NOT NULL UNIQUE COMMENT 'User account',"
				"ROL_IS_ADMIN INTEGER                            COMMENT 'Whether the user has administration role') "
			"CHARACTER SET utf8",
				infos->dbname );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	if( mysql_query( infos->mysql, stmt->str )){
		g_warning( "%s: %s", thisfn, mysql_error( infos->mysql ));
		goto free_stmt;
	}

	db_created = TRUE;

free_stmt:
	idbms_close( instance, infos );
	g_string_free( stmt, TRUE );

	g_debug( "%s: db_created=%s", thisfn, db_created ? "True":"False" );

	return( db_created );
}

static gboolean
idbms_grant_user( const ofaIDbms *instance, const gchar *dname,
											const gchar *root_account, const gchar *root_password,
											const gchar *user_account, const gchar *user_password )
{
	static const gchar *thisfn = "ofa_mysql_idbms_grant_user";
	mysqlInfos *infos;
	gchar *dbname;
	GString *stmt;
	gboolean user_granted;
	gchar *hostname;

	g_debug( "%s: instance=%p, dname=%s, root_account=%s, user_account=%s",
			thisfn, ( void * ) instance, dname, root_account, user_account );

	infos = ofa_mysql_get_connect_infos( dname );
	dbname = infos->dbname;
	infos->dbname = g_strdup( "mysql" );
	infos->account = g_strdup( root_account );
	infos->password = g_strdup( root_password );

	if( !ofa_mysql_connect_with_infos( infos )){
		idbms_close( instance, infos );
		return( FALSE );
	}

	user_granted = FALSE;
	stmt = g_string_new( "" );

	hostname = g_strdup( infos->host );
	if( !hostname || !g_utf8_strlen( hostname, -1 )){
		g_free( hostname );
		hostname = g_strdup( "localhost" );
	}

	/* doesn't trap error on create user as the user may already exist */
	g_string_printf( stmt,
			"CREATE USER '%s'@'%s' IDENTIFIED BY '%s'",
				user_account,
				hostname,
				user_password );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	mysql_query( infos->mysql, stmt->str );

	g_string_printf( stmt,
			"GRANT ALL ON %s.* TO '%s'@'%s' WITH GRANT OPTION",
				dbname,
				user_account,
				hostname );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	if( mysql_query( infos->mysql, stmt->str )){
		g_warning( "%s: %s", thisfn, mysql_error( infos->mysql ));
		goto free_stmt;
	}

	g_string_printf( stmt,
			"GRANT CREATE USER, FILE ON *.* TO '%s'@'%s'",
				user_account,
				hostname );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	if( mysql_query( infos->mysql, stmt->str )){
		g_warning( "%s: %s", thisfn, mysql_error( infos->mysql ));
		goto free_stmt;
	}

	user_granted = TRUE;

free_stmt:
	idbms_close( instance, infos );
	g_string_free( stmt, TRUE );
	g_free( hostname );
	g_free( dbname );

	g_debug( "%s: user_granted=%s", thisfn, user_granted ? "True":"False" );

	return( user_granted );
}

/*
 * infos structure must have been already filled up with DBMS root
 * credentials and target database
 */
gboolean
ofa_mysql_duplicate_grants( const ofaIDbms *instance, mysqlInfos *infos, const gchar *user_account, const gchar *prev_dbname )
{
	static const gchar *thisfn = "ofa_mysql_duplicate_grants";
	gchar *hostname, *query, *dbname, *str;
	GSList *result, *irow, *icol;
	GRegex *regex;
	const gchar *cstr;
	gboolean ok;

	g_debug( "%s: instance=%p, infos=%p, user_account=%s",
			thisfn, ( void * ) instance, ( void * ) infos, user_account );

	dbname = infos->dbname;
	infos->dbname = g_strdup( "mysql" );

	if( !ofa_mysql_connect_with_infos( infos )){
		mysql_close( infos->mysql );
		return( FALSE );
	}

	str = g_strdup_printf( "\\b%s\\b", prev_dbname );
	regex = g_regex_new( str, 0, 0, NULL );
	g_free( str );

	hostname = g_strdup( infos->host );
	if( !hostname || !g_utf8_strlen( hostname, -1 )){
		g_free( hostname );
		hostname = g_strdup( "localhost" );
	}

	query = g_strdup_printf( "SHOW GRANTS FOR '%s'@'%s'", user_account, hostname );
	ok = idbms_query_ex( instance, infos, query, &result );
	g_free( query );
	if( !ok ){
		g_warning( "%s: %s", thisfn, mysql_error( infos->mysql ));
		goto free_stmt;
	}

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		cstr = ( const gchar * ) icol->data;
		g_debug( "%s: cstr=%s", thisfn, cstr );
		if( g_regex_match( regex, cstr, 0, NULL )){
			query = g_regex_replace_literal( regex, cstr, -1, 0, dbname, 0, NULL );
			g_debug( "%s: query=%s", thisfn, query );
			mysql_query( infos->mysql, query );
			g_free( query );
		}
	}

free_stmt:
	mysql_close( infos->mysql );
	g_free( hostname );
	g_free( dbname );
	g_regex_unref( regex );

	return( ok );
}

#if 0
static void
drop_admin_account( mysqlConnect_oldv1_oldv1_oldv1 *connect, const gchar *account )
{
	MYSQL *mysql;
	mysqlConnect_oldv1_oldv1_oldv1 *cnt;

	cnt = g_new0( mysqlConnect_oldv1_oldv1_oldv1, 1 );
	cnt->host = connect->host;
	cnt->port = connect->port;
	cnt->socket = connect->socket;
	cnt->dbname = "mysql";
	cnt->account = connect->account;
	cnt->password = connect->password;

	mysql = ofa_mysql_connect( cnt );

	/* #303: unable to remove administrative accounts */
	if( mysql && FALSE ){
		do_drop_account( mysql, cnt->host, account );
		mysql_close( mysql );
		g_free( mysql );
	}

	g_free( cnt );
}

static void
do_drop_account( MYSQL *mysql, const gchar *host, const gchar *account )
{
	gchar *query;
	gchar *hostname;

	query = g_strdup_printf( "delete from mysql.user where user='%s'", account );
	mysql_query( mysql, query );
	g_free( query );

	hostname = g_strdup( host );
	if( !host || !g_utf8_strlen( host, -1 )){
		g_free( hostname );
		hostname = g_strdup( "localhost" );
	}

	query = g_strdup_printf( "drop user '%s'@'%s'", account, hostname );
	mysql_query( mysql, query );
	g_free( query );
	g_free( hostname );

	mysql_query( mysql, "flush privileges" );
}

static gboolean
confirm_database_reinit( sPrivate *priv, const gchar *dbname )
{
	GtkWidget *dialog;
	gchar *msg;
	gint response;

	msg = g_strdup_printf( _( "You are about to reinitialize the '%s' database.\n"
			"This operation will cannot be recovered.\n"
			"Are you sure ?" ), dbname );

	dialog = gtk_message_dialog_new(
			NULL,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			"%s", msg );

	g_free( msg );

	gtk_dialog_add_buttons( GTK_DIALOG( dialog ),
			_( "_Cancel" ), GTK_RESPONSE_CANCEL,
			_( "_Reinitialize" ), GTK_RESPONSE_OK,
			NULL );

	response = gtk_dialog_run( GTK_DIALOG( dialog ));

	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
}

static void
mysql_display_error( mysqlInfos *infos )
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(
			NULL,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			"%s", mysql_error( infos->mysql ));

	gtk_dialog_run( GTK_DIALOG( dialog ));
	gtk_widget_destroy( dialog );
}


static gchar *
idbms_get_dossier_host( const ofaIDbms *instance, const gchar *label )
{
	gchar *host;

	host = ofa_settings_dossier_get_string( label, "MySQLHost" );

	return( host );
}

static gchar *
idbms_get_dossier_dbname( const ofaIDbms *instance, const gchar *label )
{
	gchar *dbname;

	dbname = ofa_settings_dossier_get_string( label, "MySQLDatabase" );

	return( dbname );
}

static gboolean
idbms_delete_dossier( const ofaIDbms *instance, const gchar *name, const gchar *account, const gchar *password, gboolean drop_db, gboolean drop_accounts )
{
	static const gchar *thisfn = "ofa_mysql_idbms_delete_dossier";
	mysqlInfos *infos;

	g_debug( "%s: instance=%p, label=%s, account=%s, drop_db=%s, drop_accounts=%s",
			thisfn,
			( void * ) instance, label, account,
			drop_db ? "True":"False", drop_accounts ? "True":"False" );

	infos = ( mysqlConnect_oldv2 * ) idbms_connect( instance, label, "mysql", TRUE, account, password );
	if( !infos ){
		return( FALSE );
	}

	g_free( infos->connect.dbname );
	infos->connect.dbname = ofa_settings_dossier_get_string( label, "MySQLDatabase" );

	/* #303: unable to delete accounts without cross-checking with
	 * other dossiers */
	/*
	if( drop_accounts ){
		idbms_drop_accounts( infos );
	}
	*/

	if( drop_db ){
		idbms_drop_database( infos );
	}

	idbms_close( instance, infos );
	return( TRUE );
}

static void
idbms_drop_database( const mysqlConnect_oldv2 *infos )
{
	static const gchar *thisfn = "ofa_mysql_idbms_drop_database";
	gchar *query;

	query = g_strdup_printf( "DROP DATABASE %s", infos->connect.dbname );
	g_debug( "%s: infos=%p, query=%s", thisfn, ( void * ) infos, query );
	if( mysql_query( infos->mysql, query )){
		g_debug( "%s: %s", thisfn, mysql_error( infos->mysql ));
	}
}
#endif
