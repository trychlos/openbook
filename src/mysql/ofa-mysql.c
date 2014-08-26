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

#include <string.h>

#include <api/ofa-settings.h>

#include "ofa-mysql.h"
#include "ofa-mysql-dossier-new.h"
#include "ofa-mysql-prefs.h"

/* private instance data
 */
struct _ofaMysqlPrivate {
	gboolean dispose_has_run;
};

/*
 * a structure which associates the ofoSgbd connection to its properties
 *
 * the structure is dynamically allocated on a new connection, and its
 * handle is returned to the caller.
 */
typedef struct {
	gchar        *label;
	MYSQL        *mysql;
	mysqlConnect  connect;
}
	SgbdInfos;

static GType         st_module_type = 0;
static GObjectClass *st_parent_class = NULL;

static void         class_init( ofaMysqlClass *klass );
static void         instance_init( GTypeInstance *instance, gpointer klass );
static void         instance_dispose( GObject *object );
static void         instance_finalize( GObject *object );

static void         idbms_iface_init( ofaIDbmsInterface *iface );
static guint        idbms_get_interface_version( const ofaIDbms *instance );
static gchar       *idbms_get_dossier_host( const ofaIDbms *instance, const gchar *label );
static gchar       *idbms_get_dossier_dbname( const ofaIDbms *instance, const gchar *label );
static void        *idbms_connect( const ofaIDbms *instance, const gchar *label, const gchar *account, const gchar *password );
static void        *idbms_connect_ex( const ofaIDbms *instance, const gchar *label, const gchar *dbname, const gchar *account, const gchar *password );
static void        *idbms_connect_conv( const ofaIDbms *instance, SgbdInfos *infos, const gchar *label, const gchar *account, const gchar *password );
static void         idbms_close( const ofaIDbms *instance, void *handle );
static gboolean     idbms_query( const ofaIDbms *instance, void *handle, const gchar *query );
static GSList      *idbms_query_ex( const ofaIDbms *instance, void *handle, const gchar *query );
static gchar       *idbms_error( const ofaIDbms *instance, void *handle );
static gboolean     idbms_delete_dossier( const ofaIDbms *instance, const gchar *label, const gchar *account, const gchar *password, gboolean drop_db, gboolean drop_accounts );
static void         idbms_drop_database( const SgbdInfos *infos );
static gboolean     local_get_db_exists( MYSQL *mysql, const gchar *dbname );

static void         ipreferences_iface_init( ofaIPreferencesInterface *iface );
static guint        ipreferences_get_interface_version( const ofaIPreferences *instance );

GType
ofa_mysql_get_type( void )
{
	return( st_module_type );
}

void
ofa_mysql_register_type( GTypeModule *module )
{
	static const gchar *thisfn = "ofa_mysql_register_type";

	static GTypeInfo info = {
		sizeof( ofaMysqlClass ),
		NULL,
		NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaMysql ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	static const GInterfaceInfo idbms_iface_info = {
		( GInterfaceInitFunc ) idbms_iface_init,
		NULL,
		NULL
	};

	static const GInterfaceInfo ipreferences_iface_info = {
		( GInterfaceInitFunc ) ipreferences_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	st_module_type = g_type_module_register_type( module, G_TYPE_OBJECT, "ofaMysql", &info, 0 );

	g_type_module_add_interface( module, st_module_type, OFA_TYPE_IDBMS, &idbms_iface_info );

	g_type_module_add_interface( module, st_module_type, OFA_TYPE_IPREFERENCES, &ipreferences_iface_info );
}

static void
class_init( ofaMysqlClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = instance_dispose;
	G_OBJECT_CLASS( klass )->finalize = instance_finalize;
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_mysql_instance_init";
	ofaMysql *self;

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn,
			( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			( void * ) klass );

	g_return_if_fail( instance && OFA_IS_MYSQL( instance ));

	self = OFA_MYSQL( instance );

	self->private = g_new0( ofaMysqlPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *object )
{
	ofaMysql *self;

	g_return_if_fail( object && OFA_IS_MYSQL( object ));

	self = OFA_MYSQL( object );

	if( !self->private->dispose_has_run ){

		self->private->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->dispose( object );
}

static void
instance_finalize( GObject *object )
{
	static const gchar *thisfn = "ofa_mysql_instance_finalize";
	ofaMysqlPrivate *priv;

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_return_if_fail( object && OFA_IS_MYSQL( object ));

	priv = OFA_MYSQL( object )->private;

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->finalize( object );
}

static void
idbms_iface_init( ofaIDbmsInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_idbms_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idbms_get_interface_version;
	iface->get_provider_name = ofa_mysql_get_provider_name;
	iface->properties_new_init = ofa_mysql_properties_new_init;
	iface->properties_new_check = ofa_mysql_properties_new_check;
	iface->properties_new_apply = ofa_mysql_properties_new_apply;
	iface->get_dossier_host = idbms_get_dossier_host;
	iface->get_dossier_dbname = idbms_get_dossier_dbname;
	iface->connect = idbms_connect;
	iface->connect_ex = idbms_connect_ex;
	iface->close = idbms_close;
	iface->query = idbms_query;
	iface->query_ex = idbms_query_ex;
	iface->error = idbms_error;
	iface->delete_dossier = idbms_delete_dossier;
}

static guint
idbms_get_interface_version( const ofaIDbms *instance )
{
	return( 1 );
}

const gchar *
ofa_mysql_get_provider_name( const ofaIDbms *instance )
{
	return( "MySQL" );
}

static gchar *
idbms_get_dossier_host( const ofaIDbms *instance, const gchar *label )
{
	gchar *host;

	host = ofa_settings_get_dossier_key_string( label, "Host" );

	return( host );
}

static gchar *
idbms_get_dossier_dbname( const ofaIDbms *instance, const gchar *label )
{
	gchar *dbname;

	dbname = ofa_settings_get_dossier_key_string( label, "Database" );

	return( dbname );
}

static void *
idbms_connect( const ofaIDbms *instance, const gchar *label, const gchar *account, const gchar *password )
{
	SgbdInfos *infos;

	infos = g_new0( SgbdInfos, 1 );
	infos->connect.dbname = ofa_settings_get_dossier_key_string( label, "Database" );

	return( idbms_connect_conv( instance, infos, label, account, password ));
}

static void *
idbms_connect_ex( const ofaIDbms *instance, const gchar *label, const gchar *dbname, const gchar *account, const gchar *password )
{
	SgbdInfos *infos;

	infos = g_new0( SgbdInfos, 1 );
	infos->connect.dbname = g_strdup( dbname );

	return( idbms_connect_conv( instance, infos, label, account, password ));
}

static void *
idbms_connect_conv( const ofaIDbms *instance, SgbdInfos *infos, const gchar *label, const gchar *account, const gchar *password )
{
	MYSQL *mysql;
	mysqlConnect *cnt;

	cnt = &infos->connect;

	cnt->host = ofa_settings_get_dossier_key_string( label, "Host" );
	cnt->socket = ofa_settings_get_dossier_key_string( label, "Socket" );
	cnt->port = ofa_settings_get_dossier_key_uint( label, "Port" );
	if( cnt->port == -1 ){
		cnt->port = 0;
	}
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

static void
idbms_close( const ofaIDbms *instance, void *handle )
{
	SgbdInfos *infos;

	infos = ( SgbdInfos * ) handle;

	mysql_close( infos->mysql );
	ofa_mysql_free_connect( &infos->connect );
	g_free( infos->label );
	g_free( infos->mysql );
	g_free( infos );
}

static gboolean
idbms_query( const ofaIDbms *instance, void *handle, const gchar *query )
{
	static const gchar *thisfn = "ofa_mysql_idbms_query";
	gboolean query_ok;
	SgbdInfos *infos;

	query_ok = FALSE;
	infos = ( SgbdInfos * ) handle;

	if( infos && infos->mysql ){
		query_ok = ( mysql_query( infos->mysql, query ) == 0 );
	} else {
		g_warning( "%s: trying to query a non-opened connection", thisfn );
	}

	return( query_ok );
}

static GSList *
idbms_query_ex( const ofaIDbms *instance, void *handle, const gchar *query )
{
	GSList *result;
	SgbdInfos *infos;
	MYSQL_RES *res;
	MYSQL_ROW row;
	gint fields_count, i;

	result = NULL;

	if( idbms_query( instance, handle, query )){

		infos = ( SgbdInfos * ) handle;
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
idbms_error( const ofaIDbms *instance, void *handle )
{
	static const gchar *thisfn = "ofa_mysql_idbms_error";
	gchar *msg;
	SgbdInfos *infos;

	msg = NULL;
	infos = ( SgbdInfos * ) handle;

	if( infos && infos->mysql ){
		msg = g_strdup( mysql_error( infos->mysql ));
	} else {
		g_warning( "%s: trying to query a non-opened connection", thisfn );
	}

	return( msg );
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
idbms_delete_dossier( const ofaIDbms *instance, const gchar *label, const gchar *account, const gchar *password, gboolean drop_db, gboolean drop_accounts )
{
	static const gchar *thisfn = "ofa_mysql_idbms_delete_dossier";
	SgbdInfos *infos;

	g_debug( "%s: instance=%p, label=%s, account=%s, drop_db=%s, drop_accounts=%s",
			thisfn,
			( void * ) instance, label, account,
			drop_db ? "True":"False", drop_accounts ? "True":"False" );

	infos = ( SgbdInfos * ) idbms_connect_ex( instance, label, "mysql", account, password );
	if( !infos ){
		return( FALSE );
	}

	g_free( infos->connect.dbname );
	infos->connect.dbname = ofa_settings_get_dossier_key_string( label, "Database" );

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
idbms_drop_database( const SgbdInfos *infos )
{
	static const gchar *thisfn = "ofa_mysql_idbms_drop_database";
	gchar *query;

	query = g_strdup_printf( "DROP DATABASE %s", infos->connect.dbname );
	g_debug( "%s: infos=%p, query=%s", thisfn, ( void * ) infos, query );
	if( mysql_query( infos->mysql, query )){
		g_debug( "%s: %s", thisfn, mysql_error( infos->mysql ));
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

static void
ipreferences_iface_init( ofaIPreferencesInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_ipreferences_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = ipreferences_get_interface_version;
	iface->run_init = ofa_mysql_prefs_init;
	iface->run_check = ofa_mysql_prefs_check;
	iface->run_done = ofa_mysql_prefs_apply;
}

static guint
ipreferences_get_interface_version( const ofaIPreferences *instance )
{
	return( 1 );
}
