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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-settings.h"

#include "ofa-mysql.h"
#include "ofa-mysql-backup.h"
#include "ofa-mysql-dossier-new.h"
#include "ofa-mysql-idbms.h"

#define SETTINGS_DATABASE               "MySQLDatabase"
#define SETTINGS_HOST                   "MySQLHost"
#define SETTINGS_PORT                   "MySQLPort"
#define SETTINGS_SOCKET                 "MySQLSocket"

static const gchar  *st_ui_xml          = PROVIDER_DATADIR "/ofa-mysql-connect-infos.piece.ui";
static const gchar  *st_ui_mysql        = "MySQLConnectInfosWindow";

static guint        idbms_get_interface_version( const ofaIDbms *instance );
static void        *idbms_connect( const ofaIDbms *instance,const gchar *dname, const gchar *dbname, const gchar *account, const gchar *password );
static void         setup_infos( sMySQLInfos *infos );
static gboolean     connect_with_infos( const ofaIDbms *instance, sMySQLInfos *infos, const gchar *password );
static void         free_infos( sMySQLInfos *infos );
static void         idbms_close( const ofaIDbms *instance, void *handle );
static GSList      *idbms_get_exercices( const ofaIDbms *instance, const gchar *dname );
static gint         cmp_exercices( const gchar *line_a, const gchar *line_b );
static gboolean     idbms_query( const ofaIDbms *instance, void *handle, const gchar *query );
static GSList      *idbms_query_ex( const ofaIDbms *instance, void *handle, const gchar *query );
static gchar       *idbms_last_error( const ofaIDbms *instance, void *handle );

static gchar       *idbms_get_dossier_host( const ofaIDbms *instance, const gchar *label );
static gchar       *idbms_get_dossier_dbname( const ofaIDbms *instance, const gchar *label );
/*static void        *idbms_connect_conv( const ofaIDbms *instance, mysqlInfos *infos, const gchar *label, const gchar *account, const gchar *password );*/
static gboolean     idbms_delete_dossier( const ofaIDbms *instance, const gchar *label, const gchar *account, const gchar *password, gboolean drop_db, gboolean drop_accounts );
/*static void         idbms_drop_database( const mysqlInfos *infos );*/
static gboolean     local_get_db_exists( MYSQL *mysql, const gchar *dbname );
static void         idbms_display_connect_infos( const ofaIDbms *instance, GtkWidget *container, const gchar *label );

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
	iface->close = idbms_close;
	iface->get_exercices = idbms_get_exercices;
	iface->query = idbms_query;
	iface->query_ex = idbms_query_ex;
	iface->last_error = idbms_last_error;

	iface->properties_new_init = ofa_mysql_properties_new_init;
	iface->properties_new_check = ofa_mysql_properties_new_check;
	iface->properties_new_apply = ofa_mysql_properties_new_apply;
	iface->get_dossier_host = idbms_get_dossier_host;
	iface->get_dossier_dbname = idbms_get_dossier_dbname;
	iface->delete_dossier = idbms_delete_dossier;
	iface->backup = ofa_mysql_backup;
	iface->restore = ofa_mysql_restore;
	iface->display_connect_infos = idbms_display_connect_infos;
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
	sMySQLInfos *infos;

	infos = g_new0( sMySQLInfos, 1 );
	infos->dname = g_strdup( dname );
	infos->dbname = g_strdup( dbname );
	infos->account = g_strdup( account );

	setup_infos( infos );
	if( !connect_with_infos( instance, infos, password )){
		free_infos( infos );
		infos = NULL;
	}

	return( infos );
}

/*
 * Populate the sMySQL structure with informations from the settings
 * NB: the database name is an intrant
 */
static void
setup_infos( sMySQLInfos *infos )
{
	infos->host = ofa_settings_dossier_get_string( infos->dname, SETTINGS_HOST );
	infos->socket = ofa_settings_dossier_get_string( infos->dname, SETTINGS_SOCKET );
	infos->port = ofa_settings_dossier_get_int( infos->dname, SETTINGS_PORT );
	if( infos->port == -1 ){
		infos->port = 0;
	}
}

/*
 * dbname is already set, whether it has been explicitely specified or
 * it comes from the dossier
 */
static gboolean
connect_with_infos( const ofaIDbms *instance, sMySQLInfos *infos, const gchar *password )
{
	static const gchar *thisfn = "ofa_mysql_connect_with_infos";
	MYSQL *mysql;

	mysql = g_new0( MYSQL, 1 );
	mysql_init( mysql );

	if( !mysql_real_connect( mysql,
							infos->host,
							infos->account,
							password,
							infos->dbname,
							infos->port,
							infos->socket,
							CLIENT_MULTI_RESULTS )){
		g_free( mysql );
		g_debug( "%s: dname=%s, dbname=%s, account=%s, password=%s, host=%s, port=%d, socket=%s: unable to connect",
				thisfn, infos->dname, infos->dbname, infos->account, password, infos->host, infos->port, infos->socket );
		return( FALSE );
	}

	g_debug( "%s: connect OK", thisfn );
	infos->mysql = mysql;

	return( TRUE );
}

static void
free_infos( sMySQLInfos *infos )
{
	g_free( infos->dname );
	g_free( infos->dbname );
	g_free( infos->account );
	g_free( infos->host );
	g_free( infos->socket );
	g_free( infos );
}

/*
 * close the opened instance
 */
static void
idbms_close( const ofaIDbms *instance, void *handle )
{
	sMySQLInfos *infos;

	infos = ( sMySQLInfos * ) handle;

	mysql_close( infos->mysql );
	free_infos( infos );
}

/*
 * returned the known exercices
 *
 * MySQLDatabase=<database>;<begin>;<end>;
 * MySQLDatabase_<begin>=<database>;<end>;
 *
 * where <begin> and <end> are 'yyyymmdd' dates.
 */
static GSList *
idbms_get_exercices( const ofaIDbms *instance, const gchar *dname )
{
	GSList *keys_list, *it;
	GSList *out_list;
	const gchar *cstr, *sstr;
	GList *str_list, *is;
	gchar *dbname, *sdate, *line;
	GString *label;
	GDate date;
	gchar **array, **iter;

	out_list = NULL;
	keys_list = ofa_settings_dossier_get_keys( dname );

	for( it=keys_list ; it ; it=it->next ){
		cstr = ( const gchar * ) it->data;
		if( g_str_has_prefix( cstr, SETTINGS_DATABASE )){
			str_list = ofa_settings_dossier_get_string_list( dname, cstr );
			is = str_list;

			/* database is the first item */
			dbname = g_strdup(( const gchar * ) is->data );
			is = is->next;

			/* current exercice : key = database; begin; end; */
			if( !g_utf8_collate( cstr, SETTINGS_DATABASE )){
				label = g_string_new( _( "Current exercice" ));
				sstr = is ? ( const gchar * ) is->data : NULL;
				if( sstr && g_utf8_strlen( sstr, -1 )){
					my_date_set_from_str( &date, sstr, MY_DATE_YYMD );
					if( my_date_is_valid( &date )){
						sdate = my_date_to_str( &date, MY_DATE_DMYY );
						g_string_append_printf( label, _( " from %s" ), sdate );
						g_free( sdate );
					}
				}
				is = is ? is->next : NULL;
				sstr = is ? ( const gchar * ) is->data : NULL;
				if( sstr && g_utf8_strlen( sstr, -1 )){
					my_date_set_from_str( &date, sstr, MY_DATE_YYMD );
					if( my_date_is_valid( &date )){
						sdate = my_date_to_str( &date, MY_DATE_DMYY );
						g_string_append_printf( label, _( " to %s" ), sdate );
						g_free( sdate );
					}
				}

			/* archived exercice : key_begin = database; end; */
			} else {
				array = g_strsplit( cstr, "_", -1 );
				iter = array;
				iter++;
				label = g_string_new( _( "Archived exercice" ));
				if( *iter && g_utf8_strlen( *iter, -1 )){
					my_date_set_from_str( &date, *iter, MY_DATE_YYMD );
					if( my_date_is_valid( &date )){
						sdate = my_date_to_str( &date, MY_DATE_DMYY );
						g_string_append_printf( label, _( " from %s" ), sdate );
						g_free( sdate );
					}
				}
				g_strfreev( array );
				sstr = ( const gchar * ) is->data;
				if( is->data && g_utf8_strlen( sstr, -1 )){
					my_date_set_from_str( &date, sstr, MY_DATE_YYMD );
					if( my_date_is_valid( &date )){
						sdate = my_date_to_str( &date, MY_DATE_DMYY );
						g_string_append_printf( label, _( " to %s" ), sdate );
						g_free( sdate );
					}
				}
			}
			line = g_strdup_printf( "%s;%s", label->str, dbname );
			out_list = g_slist_insert_sorted( out_list, line, ( GCompareFunc ) cmp_exercices );

			g_string_free( label, TRUE );
			g_free( dbname );
			ofa_settings_free_string_list( str_list );
		}
	}
	ofa_settings_dossier_free_keys( keys_list );

	return( out_list );
}

/*
 * sort in reverse order so that Current is before archived
 * and archived are in decreasing order (from most recent to oldest)
 */
static gint
cmp_exercices( const gchar *line_a, const gchar *line_b )
{
	return( -1*g_utf8_collate( line_a, line_b ));
}

static gboolean
idbms_query( const ofaIDbms *instance, void *handle, const gchar *query )
{
	static const gchar *thisfn = "ofa_mysql_idbms_query";
	gboolean query_ok;
	sMySQLInfos *infos;

	query_ok = FALSE;
	infos = ( sMySQLInfos * ) handle;

	if( infos && infos->mysql ){
		query_ok = ( mysql_query( infos->mysql, query ) == 0 );
	} else {
		g_warning( "%s: trying to querying a non-opened connection", thisfn );
	}

	return( query_ok );
}

static GSList *
idbms_query_ex( const ofaIDbms *instance, void *handle, const gchar *query )
{
	GSList *result;
	sMySQLInfos *infos;
	MYSQL_RES *res;
	MYSQL_ROW row;
	gint fields_count, i;

	result = NULL;

	if( idbms_query( instance, handle, query )){

		infos = ( sMySQLInfos * ) handle;
		res = mysql_store_result( infos->mysql );
		if( res ){
			fields_count = mysql_num_fields( res );
			while(( row = mysql_fetch_row( res ))){
				GSList *col = NULL;
				for( i=0 ; i<fields_count ; ++i ){
					col = g_slist_prepend( col, row[i] ? g_strdup( row[i] ) : NULL );
				}
				col = g_slist_reverse( col );
				result = g_slist_prepend( result, col );
			}
			result = g_slist_reverse( result );
		}
	}

	return( result );
}

static gchar *
idbms_last_error( const ofaIDbms *instance, void *handle )
{
	static const gchar *thisfn = "ofa_mysql_idbms_error";
	gchar *msg;
	sMySQLInfos *infos;

	msg = NULL;
	infos = ( sMySQLInfos * ) handle;

	if( infos && infos->mysql ){
		msg = g_strdup( mysql_error( infos->mysql ));
	} else {
		g_warning( "%s: trying to querying a non-opened connection", thisfn );
	}

	return( msg );
}

/* ... */

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

/**
 * ofa_mysql_get_connect_infos:
 * @cnt: the connection structure to be filled.
 * @label: the name of the dossier in user's settings
 *
 * Fill the connection structure with connection informations, but
 * not the database, i.e. for Mysql: host, port and socket.
 *
 * Return: the connection structure itself.
 */
mysqlConnect *
ofa_mysql_get_connect_infos( mysqlConnect *cnt, const gchar *label )
{
	cnt->host = ofa_settings_dossier_get_string( label, "MySQLHost" );
	cnt->socket = ofa_settings_dossier_get_string( label, "MySQLSocket" );
	cnt->port = ofa_settings_dossier_get_int( label, "MySQLPort" );
	if( cnt->port == -1 ){
		cnt->port = 0;
	}

	return( cnt );
}

/*
 * dbname is already set, whether it has been explicitely specified or
 * it comes from the dossier
 */
#if 0
static void *
idbms_connect_conv( const ofaIDbms *instance, mysqlInfos *infos, const gchar *label, const gchar *account, const gchar *password )
{
	MYSQL *mysql;
	mysqlConnect *cnt;

	cnt = &infos->connect;

	ofa_mysql_get_connect_infos( cnt, label );

	cnt->account = g_strdup( account );
	cnt->password = g_strdup( password );

	mysql = ofa_mysql_connect( cnt );
	if( !mysql ){
		ofa_mysql_free_connect( cnt );
		g_free( infos );
		return( NULL );
	}

	infos->label = g_strdup( label );
	infos->mysql = mysql;

	return( infos );
}
#endif

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

/**
 * ofa_mysql_get_db_exists:
 */
gboolean
ofa_mysql_get_db_exists( mysqlConnect *sConnect )
{
	gboolean exists;
	mysqlConnect *scnt;
	MYSQL *mysql;

	exists = FALSE;
	scnt = g_new0( mysqlConnect, 1 );
	memcpy( scnt, sConnect, sizeof( mysqlConnect ));
	scnt->dbname = NULL;

	mysql = ofa_mysql_connect( scnt );
	if( mysql ){
		exists = local_get_db_exists( mysql, sConnect->dbname );
		mysql_close( mysql );
		g_free( mysql );
	}

	return( exists );
}

/**
 * ofa_mysql_connect:
 */
MYSQL *
ofa_mysql_connect( mysqlConnect *sConnect )
{
	static const gchar *thisfn = "ofa_mysql_connect";
	MYSQL *mysql;

	mysql = g_new0( MYSQL, 1 );
	mysql_init( mysql );

	if( !mysql_real_connect( mysql,
							sConnect->host,
							sConnect->account,
							sConnect->password,
							sConnect->dbname,
							sConnect->port,
							sConnect->socket,
							CLIENT_MULTI_RESULTS )){
		g_free( mysql );
		g_debug( "%s: host=%s, account=%s, password=%s, dbname=%s, port=%u, socket=%s: unable to connect",
				thisfn, sConnect->host, sConnect->account, sConnect->password, sConnect->dbname, sConnect->port, sConnect->socket );
		return( NULL );
	}

	g_debug( "%s: host=%s, account=%s, dbname=%s, port=%u, socket=%s: connect ok",
			thisfn, sConnect->host, sConnect->account, sConnect->dbname, sConnect->port, sConnect->socket );
	return( mysql );
}

static gboolean
idbms_delete_dossier( const ofaIDbms *instance, const gchar *name, const gchar *account, const gchar *password, gboolean drop_db, gboolean drop_accounts )
{
#if 0
	static const gchar *thisfn = "ofa_mysql_idbms_delete_dossier";
	sMySQLInfos *infos;

	g_debug( "%s: instance=%p, label=%s, account=%s, drop_db=%s, drop_accounts=%s",
			thisfn,
			( void * ) instance, label, account,
			drop_db ? "True":"False", drop_accounts ? "True":"False" );

	infos = ( mysqlInfos * ) idbms_connect( instance, label, "mysql", TRUE, account, password );
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
#endif
	return( TRUE );
}

#if 0
static void
idbms_drop_database( const mysqlInfos *infos )
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

static void
idbms_display_connect_infos( const ofaIDbms *instance, GtkWidget *container, const gchar *label )
{
	GtkWidget *window;
	GtkWidget *grid;
	GtkWidget *wlabel;
	gchar *text;
	gint port_num;

	window = my_utils_builder_load_from_path( st_ui_xml, st_ui_mysql );
	g_return_if_fail( window && GTK_IS_WINDOW( window ));

	grid = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "infos-grid" );
	g_return_if_fail( grid && GTK_IS_GRID( grid ));
	gtk_widget_reparent( grid, container );

	wlabel = my_utils_container_get_child_by_name( GTK_CONTAINER( grid ), "provider" );
	g_return_if_fail( wlabel && GTK_IS_LABEL( wlabel ));
	text = ofa_settings_get_dossier_provider( label );
	gtk_label_set_text( GTK_LABEL( wlabel ), text );
	g_free( text );

	wlabel = my_utils_container_get_child_by_name( GTK_CONTAINER( grid ), "host" );
	g_return_if_fail( wlabel && GTK_IS_LABEL( wlabel ));
	text = ofa_settings_dossier_get_string( label, "MySQLHost" );
	if( !text || !g_utf8_strlen( text, -1 )){
		g_free( text );
		text = g_strdup( "localhost" );
	}
	gtk_label_set_text( GTK_LABEL( wlabel ), text );
	g_free( text );

	wlabel = my_utils_container_get_child_by_name( GTK_CONTAINER( grid ), "socket" );
	g_return_if_fail( wlabel && GTK_IS_LABEL( wlabel ));
	text = ofa_settings_dossier_get_string( label, "MySQLSocket" );
	if( text && g_utf8_strlen( text, -1 )){
		gtk_label_set_text( GTK_LABEL( wlabel ), text );
	}
	g_free( text );

	wlabel = my_utils_container_get_child_by_name( GTK_CONTAINER( grid ), "database" );
	g_return_if_fail( wlabel && GTK_IS_LABEL( wlabel ));
	text = ofa_settings_dossier_get_string( label, "MySQLDatabase" );
	gtk_label_set_text( GTK_LABEL( wlabel ), text );
	g_free( text );

	wlabel = my_utils_container_get_child_by_name( GTK_CONTAINER( grid ), "port" );
	g_return_if_fail( wlabel && GTK_IS_LABEL( wlabel ));
	port_num = ofa_settings_dossier_get_int( label, "MySQLPort" );
	if( port_num > 0 ){
		text = g_strdup_printf( "%d", port_num );
		gtk_label_set_text( GTK_LABEL( wlabel ), text );
		g_free( text );
	}
}

/**
 * ofa_mysql_free_connect:
 */
void
ofa_mysql_free_connect( mysqlConnect *sConnect )
{
	g_free( sConnect->host );
	g_free( sConnect->socket );
	g_free( sConnect->dbname );
	g_free( sConnect->account );
	g_free( sConnect->password );
}
