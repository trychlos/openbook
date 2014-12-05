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
#include "api/ofa-idbms.h"
#include "api/ofa-settings.h"

#include "ofa-mysql.h"
#include "ofa-mysql-backup.h"
#include "ofa-mysql-idbms.h"

/*
 * this structure is attached to the GtkContainer parent of the grid
 * (i.e. the container from the DossierNewDlg dialog box)
 */
typedef struct {
	const ofaIDbms *module;
	sMySQLInfos     sInfos;
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

/* columns in DB exists mode combo box
 */
enum {
	DB_COL_MODE = 0,
	DB_COL_LABEL,
	DB_N_COLUMNS
};

typedef struct {
	gint         mode;
	const gchar *label;
}
	DBMode;

static DBMode st_db_mode[] = {
		{ DBMODE_REINIT,      N_( "Reinitialize the existing DB" )},
		{ DBMODE_LEAVE_AS_IS, N_( "Keep the existing DB as is" )},
		{ 0 },
};

#define SETTINGS_DATABASE               "MySQLDatabase"
#define SETTINGS_HOST                   "MySQLHost"
#define SETTINGS_PORT                   "MySQLPort"
#define SETTINGS_SOCKET                 "MySQLSocket"

#define IDBMS_DATA                      "mysql-IDBMS-data"

static const gchar *st_ui_xml           = PROVIDER_DATADIR "/ofa-mysql-connect-infos.piece.ui";
static const gchar *st_ui_mysql         = "MySQLConnectInfosWindow";

static const gchar *st_newui_xml        = PROVIDER_DATADIR "/ofa-mysql-dossier-new.piece.ui";
static const gchar *st_newui_mysql      = "MySQLWindow";

static guint      idbms_get_interface_version( const ofaIDbms *instance );
static void      *idbms_connect( const ofaIDbms *instance,const gchar *dname, const gchar *dbname, const gchar *account, const gchar *password );
static void       setup_infos( sMySQLInfos *infos );
static gboolean   connect_with_infos( sMySQLInfos *infos );
static void       idbms_close( const ofaIDbms *instance, void *handle );
static GSList    *idbms_get_exercices( const ofaIDbms *instance, const gchar *dname );
static gchar     *idbms_get_current( const ofaIDbms *instance, const gchar *dname );
static gchar     *exercice_get_description( const gchar *dbname, const gchar *sbegin, const gchar *send, gboolean is_current );
static gint       exercice_cmp( const gchar *line_a, const gchar *line_b );
static gboolean   idbms_query( const ofaIDbms *instance, void *handle, const gchar *query );
static gboolean   idbms_query_ex( const ofaIDbms *instance, void *handle, const gchar *query, GSList **result );
static gchar     *idbms_last_error( const ofaIDbms *instance, void *handle );
static void       idbms_new_attach_to( const ofaIDbms *instance, GtkContainer *parent, GtkSizeGroup *group );
static void       new_on_parent_weak_notify( sPrivate *priv, GObject *finalized_container );
static GtkWidget *new_set_parent( const ofaIDbms *instance, GtkContainer *parent, GtkSizeGroup *group );
static void       new_init_combo_mode( const ofaIDbms *instance, GtkContainer *parent, sPrivate *priv );
static void       new_init_dialog( const ofaIDbms *instance, GtkContainer *parent, sPrivate *priv );
static void       new_on_host_changed( GtkEntry *entry, sPrivate *priv );
static void       new_on_port_changed( GtkEntry *entry, sPrivate *priv );
static void       new_on_socket_changed( GtkEntry *entry, sPrivate *priv );
static void       new_on_root_account_changed( GtkEntry *entry, sPrivate *priv );
static void       new_on_root_password_changed( GtkEntry *entry, sPrivate *priv );
static void       new_check_for_dbserver_connection( sPrivate *priv );
static void       new_on_db_name_changed( GtkEntry *entry, sPrivate *priv );
static void       new_on_db_find_clicked( GtkButton *button, sPrivate *priv );
/*static void       new_check_for_db( sPrivate *priv );*/
static void       new_on_db_exists_mode_changed( GtkComboBox *combo, sPrivate *priv );
static gboolean   idbms_new_check( const ofaIDbms *instance, GtkContainer *parent );
static gboolean   idbms_new_apply( const ofaIDbms *instance, GtkContainer *parent, const gchar *dname, const gchar *account, const gchar *password );
static gboolean   new_do_apply( sPrivate *priv );
static gboolean   new_setup_dossier( sPrivate *priv );
static gboolean   new_create_db_as_root( sPrivate *priv );
static gboolean   new_create_user_as_root( sPrivate *priv );
static gboolean   new_init_db( sPrivate *priv );
static gboolean   confirm_database_reinit( sPrivate *priv, const gchar *dbname );
static void       mysql_display_error( sMySQLInfos *infos );
static gboolean   mysql_get_db_exists( sMySQLInfos *infos );
static gboolean   local_get_db_exists( MYSQL *mysql, const gchar *dbname );

static gchar       *idbms_get_dossier_host( const ofaIDbms *instance, const gchar *label );
static gchar       *idbms_get_dossier_dbname( const ofaIDbms *instance, const gchar *label );
/*static void        *idbms_connect_conv( const ofaIDbms *instance, mysqlInfos *infos, const gchar *label, const gchar *account, const gchar *password );*/
static gboolean     idbms_delete_dossier( const ofaIDbms *instance, const gchar *label, const gchar *account, const gchar *password, gboolean drop_db, gboolean drop_accounts );
/*static void         idbms_drop_database( const mysqlInfos *infos );*/
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
	iface->get_current = idbms_get_current;
	iface->query = idbms_query;
	iface->query_ex = idbms_query_ex;
	iface->last_error = idbms_last_error;
	iface->new_attach_to = idbms_new_attach_to;
	iface->new_check = idbms_new_check;
	iface->new_apply = idbms_new_apply;

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
	infos->password = g_strdup( password );

	setup_infos( infos );
	if( !connect_with_infos( infos )){
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
connect_with_infos( sMySQLInfos *infos )
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
 * Fully free the sMySQLInfos structure (but not including the structure
 * iself).
 */
void
ofa_mysql_free_connect_infos( sMySQLInfos *infos )
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
 * which allocates a new sMySQLInfos structure. So we free it here.
 */
static void
idbms_close( const ofaIDbms *instance, void *handle )
{
	sMySQLInfos *infos;

	infos = ( sMySQLInfos * ) handle;

	mysql_close( infos->mysql );
	ofa_mysql_free_connect_infos( infos );
	g_free( infos );
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
	GList *str_list, *is;
	const gchar *cstr;
	const gchar *sdb, *sbegin, *send;
	gboolean is_current;
	gchar *line;

	out_list = NULL;
	keys_list = ofa_settings_dossier_get_keys( dname );

	for( it=keys_list ; it ; it=it->next ){
		cstr = ( const gchar * ) it->data;
		if( g_str_has_prefix( cstr, SETTINGS_DATABASE )){
			str_list = ofa_settings_dossier_get_string_list( dname, cstr );
			sdb = sbegin = send = NULL;
			is = str_list;
			if( is ){
				sdb = ( const gchar * ) is->data;
				is = is->next;
				if( is ){
					sbegin = ( const gchar * ) is->data;
					is = is->next;
					if( is ){
						send = ( const gchar * ) is->data;
					}
				}
			}
			is_current = g_utf8_collate( cstr, SETTINGS_DATABASE ) == 0;
			line = exercice_get_description( sdb, sbegin, send, is_current );
			out_list = g_slist_insert_sorted( out_list, line, ( GCompareFunc ) exercice_cmp );
			ofa_settings_free_string_list( str_list );
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
	GList *list, *it;
	gchar *out_value;
	const gchar *sdb, *sbegin, *send;

	list = ofa_settings_dossier_get_string_list( dname, SETTINGS_DATABASE );
	sdb = sbegin = send = NULL;
	it = list;
	if( it ){
		sdb = ( const gchar * ) it->data;
		it = it->next;
		if( it ){
			sbegin = ( const gchar * ) it->data;
			it = it->next;
			if( it ){
				send = ( const gchar * ) it->data;
			}
		}
	}
	out_value = exercice_get_description( sdb, sbegin, send, TRUE );
	ofa_settings_free_string_list( list );

	return( out_value );
}

/*
 * returned the current exercice description as a semi-colon separated
 * string.
 */
static gchar *
exercice_get_description( const gchar *dbname, const gchar *sbegin, const gchar *send, gboolean is_current )
{
	GString *svalue;
	GDate date;
	gchar *sdate;

	svalue = g_string_new( is_current ? _( "Current exercice" ) : _( "Archived exercice" ));

	my_date_set_from_str( &date, sbegin, MY_DATE_YYMD );
	if( my_date_is_valid( &date )){
		sdate = my_date_to_str( &date, MY_DATE_DMYY );
		g_string_append_printf( svalue, _( " from %s" ), sdate );
		g_free( sdate );
	}

	my_date_set_from_str( &date, send, MY_DATE_YYMD );
	if( my_date_is_valid( &date )){
		sdate = my_date_to_str( &date, MY_DATE_DMYY );
		g_string_append_printf( svalue, _( " to %s" ), sdate );
		g_free( sdate );
	}

	g_string_append_printf( svalue, ";%s;", dbname );

	return( g_string_free( svalue, FALSE ));
}

/*
 * sort in reverse order so that Current is before archived
 * and archived are in decreasing order (from most recent to oldest)
 */
static gint
exercice_cmp( const gchar *line_a, const gchar *line_b )
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

static gboolean
idbms_query_ex( const ofaIDbms *instance, void *handle, const gchar *query, GSList **result )
{
	gboolean ok;
	sMySQLInfos *infos;
	MYSQL_RES *res;
	MYSQL_ROW row;
	gint fields_count, i;

	ok = FALSE;
	*result = NULL;

	if( idbms_query( instance, handle, query )){

		ok = TRUE;
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

/*
 * @parent: the GtkContainer in the DossierNewDlg dialog box which will
 *  contain the provider properties grid
 */
static void
idbms_new_attach_to( const ofaIDbms *instance, GtkContainer *parent, GtkSizeGroup *group )
{
	sPrivate *priv;
	GtkWidget *widget;

	priv = g_new0( sPrivate, 1 );
	g_object_set_data( G_OBJECT( parent ), IDBMS_DATA, priv );
	g_object_weak_ref( G_OBJECT( parent ), ( GWeakNotify ) new_on_parent_weak_notify, priv );

	priv->module = instance;

	widget = new_set_parent( instance, parent, group );
	if( widget ){
		new_init_combo_mode( instance, GTK_CONTAINER( widget ), priv );
		new_init_dialog( instance, GTK_CONTAINER( widget ), priv );
	}
}

static void
new_on_parent_weak_notify( sPrivate *priv, GObject *finalized_container )
{
	static const gchar *thisfn = "ofa_mysql_idbms_new_on_parent_weak_notify";

	g_debug( "%s: priv=%p, finalized_container=%p",
			thisfn, ( void * ) priv, ( void * ) finalized_container );

	ofa_mysql_free_connect_infos( &priv->sInfos );
	g_free( priv );
}

static GtkWidget *
new_set_parent( const ofaIDbms *instance, GtkContainer *parent, GtkSizeGroup *group )
{
	GtkWidget *window;
	GtkWidget *grid;
	GtkWidget *label;

	/* attach our sgdb provider grid */
	window = my_utils_builder_load_from_path( st_newui_xml, st_newui_mysql );
	g_return_val_if_fail( window && GTK_IS_WINDOW( window ), NULL );

	grid = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "mysql-properties" );
	g_return_val_if_fail( grid && GTK_IS_GRID( grid ), NULL );
	gtk_widget_reparent( grid, GTK_WIDGET( parent ));

	if( group ){
		label = my_utils_container_get_child_by_name( GTK_CONTAINER( grid ), "mysql-label" );
		g_return_val_if_fail( label && GTK_IS_LABEL( label ), NULL );
		gtk_size_group_add_widget( group, label );
	}

	return( grid );
}

/*
 * @parent: the provider grid
 */
static void
new_init_combo_mode( const ofaIDbms *instance, GtkContainer *parent, sPrivate *priv )
{
	GtkWidget *button, *widget;
	GtkComboBox *combo;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeIter iter;
	gint i, value, idx;

	button = my_utils_container_get_child_by_name( parent, "p2-browse" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( new_on_db_find_clicked ), priv );
	priv->browse_btn = GTK_BUTTON( button );

	widget = my_utils_container_get_child_by_name( parent, "p2-dbname" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( new_on_db_name_changed ), priv );

	combo = ( GtkComboBox * ) my_utils_container_get_child_by_name( parent, "p2-db-exists" );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			DB_N_COLUMNS,
			G_TYPE_INT, G_TYPE_STRING ));
	gtk_combo_box_set_model( combo, tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", DB_COL_LABEL );

	idx = -1;
	value = ofa_settings_get_int( "DossierNewDlg-dbexists_mode" );
	if( value < 0 ){
		value = DBMODE_REINIT;
	}

	for( i=0 ; st_db_mode[i].mode ; ++i ){
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				DB_COL_MODE,  st_db_mode[i].mode,
				DB_COL_LABEL, gettext( st_db_mode[i].label ),
				-1 );
		if( value == st_db_mode[i].mode ){
			idx = i;
		}
	}

	g_signal_connect( G_OBJECT( combo ), "changed", G_CALLBACK( new_on_db_exists_mode_changed ), priv );

	if( idx >= 0 ){
		gtk_combo_box_set_active( combo, idx );
	}
}

/*
 * @parent: the provider grid
 */
static void
new_init_dialog( const ofaIDbms *instance, GtkContainer *parent, sPrivate *priv )
{
	GtkWidget *label, *entry;
	gchar *value;
	gint ivalue;

	label = my_utils_container_get_child_by_name( parent, "p2-message" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->msg = GTK_LABEL( label );

	entry = my_utils_container_get_child_by_name( parent, "p2-host" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( new_on_host_changed ), priv );
	value = ofa_settings_get_string( "DossierNewDlg-MySQL-host" );
	if( value && g_utf8_strlen( value, -1 )){
		gtk_entry_set_text( GTK_ENTRY( entry ), value );
	}
	g_free( value );

	entry = my_utils_container_get_child_by_name( parent, "p2-port" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( new_on_port_changed ), priv );
	ivalue = ofa_settings_get_int( "DossierNewDlg-MySQL-port" );
	if( ivalue > 0 ){
		value = g_strdup_printf( "%u", ivalue );
		gtk_entry_set_text( GTK_ENTRY( entry ), value );
		g_free( value );
	}

	entry = my_utils_container_get_child_by_name( parent, "p2-socket" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( new_on_socket_changed ), priv );
	value = ofa_settings_get_string( "DossierNewDlg-MySQL-socket" );
	if( value && g_utf8_strlen( value, -1 )){
		gtk_entry_set_text( GTK_ENTRY( entry ), value );
	}
	g_free( value );

	entry = my_utils_container_get_child_by_name( parent, "p2-account" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( new_on_root_account_changed ), priv );
	value = ofa_settings_get_string( "DossierNewDlg-MySQL-account" );
	if( value && g_utf8_strlen( value, -1 )){
		gtk_entry_set_text( GTK_ENTRY( entry ), value );
	}
	g_free( value );

	entry = my_utils_container_get_child_by_name( parent, "p2-password" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( new_on_root_password_changed ), priv );
}

static void
new_on_host_changed( GtkEntry *entry, sPrivate *priv )
{
	const gchar *host;

	host = gtk_entry_get_text( entry );
	g_free( priv->sInfos.host );
	priv->sInfos.host = g_strdup( host );

	priv->connect_ok = FALSE;
	new_check_for_dbserver_connection( priv );
}

static void
new_on_port_changed( GtkEntry *entry, sPrivate *priv )
{
	const gchar *port;

	port = gtk_entry_get_text( entry );
	if( port && g_utf8_strlen( port, -1 )){
		priv->sInfos.port = atoi( port );
	} else {
		priv->sInfos.port = 0;
	}

	priv->connect_ok = FALSE;
	new_check_for_dbserver_connection( priv );
}

static void
new_on_socket_changed( GtkEntry *entry, sPrivate *priv )
{
	const gchar *socket;

	socket = gtk_entry_get_text( entry );
	g_free( priv->sInfos.socket );
	priv->sInfos.socket = g_strdup( socket );

	priv->connect_ok = FALSE;
	new_check_for_dbserver_connection( priv );
}

static void
new_on_root_account_changed( GtkEntry *entry, sPrivate *priv )
{
	const gchar *account;

	account = gtk_entry_get_text( entry );
	g_free( priv->sInfos.account );
	priv->sInfos.account = g_strdup( account );

	priv->connect_ok = FALSE;
	new_check_for_dbserver_connection( priv );
}

static void
new_on_root_password_changed( GtkEntry *entry, sPrivate *priv )
{
	const gchar *password;

	password = gtk_entry_get_text( entry );
	g_free( priv->sInfos.password );
	priv->sInfos.password = g_strdup( password );

	priv->connect_ok = FALSE;
	new_check_for_dbserver_connection( priv );
}

static void
new_check_for_dbserver_connection( sPrivate *priv )
{
	sMySQLInfos *infos;
	const gchar *msg;
	GdkRGBA color;

	if( !priv->connect_ok ){

		/* test a connexion without the database
		 * this is a short copy, so do not free the strings */
		infos = g_new0( sMySQLInfos, 1 );
		memcpy( infos, &priv->sInfos, sizeof( sMySQLInfos ));
		infos->dbname = NULL;
		priv->connect_ok = connect_with_infos( infos );
		if( priv->connect_ok ){
			mysql_close( infos->mysql );
		}
		g_free( infos );

		msg = priv->connect_ok ?
				_( "DB server connection is OK" ) : _( "Unable to connect to DB server" );

		gtk_label_set_text( priv->msg, msg );
		if( gdk_rgba_parse( &color, priv->connect_ok ? "#000000" : "#FF0000" )){
			gtk_widget_override_color( GTK_WIDGET( priv->msg ), GTK_STATE_FLAG_NORMAL, &color );
		}
	}

	/* let the user select the database mode even if he doesn't have yet
	 * fill up the connection informations */
	/*g_return_if_fail( priv->browse_btn && GTK_IS_BUTTON( priv->browse_btn ));
	gtk_widget_set_sensitive( GTK_WIDGET( priv->browse_btn ), priv->connect_ok );*/

	g_signal_emit_by_name(( gpointer ) priv->module, "changed", priv->connect_ok, priv->db_ok );
}

static void
new_on_db_name_changed( GtkEntry *entry, sPrivate *priv )
{
	const gchar *name;

	name = gtk_entry_get_text( entry );
	g_free( priv->sInfos.dbname );
	priv->sInfos.dbname = g_strdup( name );

	priv->db_ok = ( priv->sInfos.dbname && g_utf8_strlen( priv->sInfos.dbname, -1 ));

	g_signal_emit_by_name(( gpointer ) priv->module, "changed", priv->connect_ok, priv->db_ok );
}

static void
new_on_db_find_clicked( GtkButton *button, sPrivate *priv )
{

}

#if 0
static void
new_check_for_db( sPrivate *priv )
{
	static const gchar *thisfn = "ofa_mysql_idbms_new_check_for_db";
	gboolean ok;

	if( !priv->db_ok ){

		ok = priv->sInfos.dbname && g_utf8_strlen( priv->sInfos.dbname, -1 );

		priv->db_exists = mysql_get_db_exists( &priv->sInfos );

		ok &= priv->connect_ok &&
				( !priv->db_exists || priv->db_exists_mode > 0 );

		priv->db_ok = ok;
	}

	g_debug( "%s: db_ok=%s", thisfn, priv->db_ok ? "True":"False" );
}
#endif

static void
new_on_db_exists_mode_changed( GtkComboBox *combo, sPrivate *priv )
{
	static const gchar *thisfn = "ofa_mysql_idbms_new_on_db_exists_mode_changed";
	GtkTreeIter iter;
	GtkTreeModel *tmodel;

	g_debug( "%s: combo=%p, self=%p", thisfn, ( void * ) combo, ( void * ) priv );

	priv->db_exists_mode = 0;

	if( gtk_combo_box_get_active_iter( combo, &iter )){
		tmodel = gtk_combo_box_get_model( combo );
		gtk_tree_model_get( tmodel, &iter,
				DB_COL_MODE, &priv->db_exists_mode,
				-1 );
	}

	g_debug( "%s: db_exists_mode=%u", thisfn, priv->db_exists_mode );
	/*new_check_for_db( priv );*/
}

/*
 * idbms_new_check:
 */
static gboolean
idbms_new_check( const ofaIDbms *instance, GtkContainer *parent )
{
	/*static const gchar *thisfn = "ofa_mysql_idbms_new_check";*/
	sPrivate *priv;

	priv = ( sPrivate * ) g_object_get_data( G_OBJECT( parent ), IDBMS_DATA );
	g_return_val_if_fail( priv, FALSE );

	/*g_debug( "%s: instance=%p, parent=%p, success=%s, db_ok=%s",
			thisfn, ( void * ) instance, ( void * ) parent,
			priv->connect_ok ? "True":"False", priv->db_ok ? "True":"False" );*/

	return( priv->connect_ok && priv->db_ok );
}

/*
 * idbms_new_apply:
 * @instance: this #ofaIDbms instance.
 * @parent: the #GtkWidget to which we have attached our data structure.
 * @dname: new dossier name
 * @account: new dossier administrative account
 * @password: new dossier administrative password
 */
static gboolean
idbms_new_apply( const ofaIDbms *instance, GtkContainer *parent,
									const gchar *dname, const gchar *account, const gchar *password )
{
	static const gchar *thisfn = "ofa_mysql_properties_new_apply";
	sPrivate *priv;
	sMySQLInfos *infos;
	gboolean ok;

	priv = ( sPrivate * ) g_object_get_data( G_OBJECT( parent ), IDBMS_DATA );
	infos = &priv->sInfos;

	g_debug( "%s: instance=%p, parent=%p, label=%s, account=%s, priv=%p",
			thisfn, ( void * ) instance, ( void * ) parent, dname, account, ( void * ) priv );

	g_return_val_if_fail( priv, FALSE );

	if( !priv->connect_ok || !priv->db_ok ){
		return( FALSE );
	}

	priv->dname = dname;
	priv->account = account;
	priv->password = password;

	priv->db_exists = mysql_get_db_exists( infos );
	if( priv->db_exists && priv->db_exists_mode == DBMODE_REINIT ){
		if( !confirm_database_reinit( priv, priv->sInfos.dbname )){
			return( FALSE );
		}
	}

	ok = new_do_apply( priv );

	if( ok ){
		if( infos->host && g_utf8_strlen( infos->host, -1 )){
			ofa_settings_set_string( "DossierNewDlg-MySQL-host", infos->host );
		}
		if( infos->port > 0 ){
			ofa_settings_set_int( "DossierNewDlg-MySQL-port", infos->port );
		}
		if( infos->socket && g_utf8_strlen( infos->socket, -1 )){
			ofa_settings_set_string( "DossierNewDlg-MySQL-socket", infos->socket );
		}
		if( infos->account && g_utf8_strlen( infos->account, -1 )){
			ofa_settings_set_string( "DossierNewDlg-MySQL-account", infos->account );
		}
		if( priv->db_exists_mode > 0 ){
			ofa_settings_set_int( "DossierNewDlg-dbexists_mode", priv->db_exists_mode );
		}
	}

	return( ok );
}

/*
 * DB model will be setup at the first connection
 */
static gboolean
new_do_apply( sPrivate *priv )
{
	gboolean apply_ok;
	/*gboolean drop_db, drop_accounts;*/

	/* setup first the dossier in configuration file, so that it will
	 * later be usable when deleting the dossier in case of error */
	apply_ok = new_setup_dossier( priv ) &&
				new_create_db_as_root( priv ) &&
				new_create_user_as_root( priv ) &&
				new_init_db( priv );

	if( !apply_ok ){
		/*drop_db = ( !priv->db_exists || priv->db_exists_mode == DBMODE_REINIT );
		drop_accounts = TRUE;
		ofa_idbms_delete_dossier(
				priv->module,
				priv->label, priv->sNew.sConnect.account, priv->sNew.sConnect.password,
				drop_db, drop_accounts, FALSE );*/
	}

	return( apply_ok );
}

static gboolean
new_setup_dossier( sPrivate *priv )
{
	static const gchar *thisfn = "ofa_mysql_idbms_new_setup_dossier";
	gboolean setup_ok;
	gchar *str;

	str = g_strdup_printf( "%s;;;", priv->sInfos.dbname );

	setup_ok = ofa_settings_create_dossier(
						priv->dname,
						SETTINGS_DBMS_PROVIDER, SETTINGS_TYPE_STRING, ofa_mysql_idbms_get_provider_name( NULL ),
						"MySQLHost",            SETTINGS_TYPE_STRING, priv->sInfos.host,
						"MySQLPort",            SETTINGS_TYPE_INT,    priv->sInfos.port,
						"MySQLSocket",          SETTINGS_TYPE_STRING, priv->sInfos.socket,
						"MySQLDatabase",        SETTINGS_TYPE_STRING, str,
						NULL );

	g_free( str );

	g_debug( "%s: setup=%s", thisfn, setup_ok ? "True":"False" );

	return( setup_ok );
}

/*
 * Create the empty database through a global connection to dataserver
 * - create database
 */
static gboolean
new_create_db_as_root( sPrivate *priv )
{
	static const gchar *thisfn = "ofa_mysql_idbms_new_create_db_as_root";
	sMySQLInfos *infos;
	GString *stmt;
	gboolean db_created;

	g_debug( "%s: priv=%p", thisfn, ( void * ) priv );

	if( priv->db_exists && priv->db_exists_mode == DBMODE_LEAVE_AS_IS ){
		return( TRUE );
	}

	infos = g_new0( sMySQLInfos, 1 );
	memcpy( infos, &priv->sInfos, sizeof( sMySQLInfos ));
	infos->dbname = "mysql";

	if( !connect_with_infos( infos )){
		mysql_display_error( infos );
		g_free( infos );
		return( FALSE );
	}

	db_created = FALSE;
	stmt = g_string_new( "" );

	g_string_printf( stmt, "DROP DATABASE %s IF EXISTS", priv->sInfos.dbname );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	mysql_query( infos->mysql, stmt->str );

	g_string_printf( stmt, "CREATE DATABASE %s", priv->sInfos.dbname );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	if( mysql_query( infos->mysql, stmt->str )){
		mysql_display_error( infos );
		goto free_stmt;
	}

	db_created = TRUE;

free_stmt:
	mysql_close( infos->mysql );
	g_free( infos );
	g_string_free( stmt, TRUE );

	g_debug( "%s: db_created=%s", thisfn, db_created ? "True":"False" );

	return( db_created );
}

/*
 * Create the empty database with a global connection to dataserver
 * - create admin user
 * - grant admin user
 */
static gboolean
new_create_user_as_root( sPrivate *priv )
{
	static const gchar *thisfn = "ofa_mysql_idbms_new_create_user_as_root";
	sMySQLInfos *infos;
	GString *stmt;
	gboolean user_created;
	gchar *hostname;

	g_debug( "%s: self=%p", thisfn, ( void * ) priv );

	infos = g_new0( sMySQLInfos, 1 );
	memcpy( infos, &priv->sInfos, sizeof( sMySQLInfos ));
	infos->dbname = "mysql";

	if( !connect_with_infos( infos )){
		mysql_display_error( infos );
		g_free( infos );
		return( FALSE );
	}

	user_created = FALSE;
	stmt = g_string_new( "" );

	hostname = g_strdup( priv->sInfos.host );
	if( !hostname || !g_utf8_strlen( hostname, -1 )){
		g_free( hostname );
		hostname = g_strdup( "localhost" );
	}

	/* doesn't trap error on create user as the user may already exist */
	g_string_printf( stmt,
			"CREATE USER '%s'@'%s' IDENTIFIED BY '%s'",
				priv->account,
				hostname,
				priv->password );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	mysql_query( infos->mysql, stmt->str );

	g_string_printf( stmt,
			"GRANT ALL ON %s.* TO '%s'@'%s' WITH GRANT OPTION",
				priv->sInfos.dbname,
				priv->account,
				hostname );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	if( mysql_query( infos->mysql, stmt->str )){
		mysql_display_error( infos );
		goto free_stmt;
	}

	g_string_printf( stmt,
			"GRANT CREATE USER, FILE ON *.* TO '%s'@'%s'",
				priv->account,
				hostname );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	if( mysql_query( infos->mysql, stmt->str )){
		mysql_display_error( infos );
		goto free_stmt;
	}

	user_created = TRUE;

free_stmt:
	mysql_close( infos->mysql );
	g_free( infos );
	g_string_free( stmt, TRUE );
	g_free( hostname );

	g_debug( "%s: user_created=%s", thisfn, user_created ? "True":"False" );

	return( user_created );
}

static gboolean
new_init_db( sPrivate *priv )
{
	static const gchar *thisfn = "ofa_mysql_idbms_new_init_db";
	sMySQLInfos *infos;
	GString *stmt;
	gboolean db_initialized;

	g_debug( "%s: priv=%p", thisfn, ( void * ) priv );

	/* make sure the desired table exist and are well set even if the
	 * user has asked for keepint its data */
	/*if( priv->db_exists && priv->db_exists_mode == DBMODE_LEAVE_AS_IS ){
		return( TRUE );
	}*/

	infos = g_new0( sMySQLInfos, 1 );
	memcpy( infos, &priv->sInfos, sizeof( sMySQLInfos ));
	infos->account = ( gchar * ) priv->account;
	infos->password = ( gchar * ) priv->password;

	if( !connect_with_infos( infos )){
		mysql_display_error( infos );
		g_free( infos );
		return( FALSE );
	}

	db_initialized = FALSE;
	stmt = g_string_new( "" );

	g_string_printf( stmt,
			"CREATE TABLE IF NOT EXISTS %s.OFA_T_AUDIT ("
			"	AUD_ID    INTEGER AUTO_INCREMENT NOT NULL UNIQUE COMMENT 'Intern identifier',"
			"	AUD_STAMP TIMESTAMP              NOT NULL        COMMENT 'Query actual timestamp',"
			"	AUD_QUERY VARCHAR(4096)          NOT NULL        COMMENT 'Query') "
			"CHARACTER SET utf8",
					infos->dbname );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	if( mysql_query( infos->mysql, stmt->str )){
		mysql_display_error( infos );
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
		mysql_display_error( infos );
		goto free_stmt;
	}

	g_string_printf( stmt,
			"INSERT IGNORE INTO %s.OFA_T_ROLES "
			"	(ROL_USER, ROL_IS_ADMIN) VALUES ('%s',1)",
				infos->dbname, priv->account );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	if( mysql_query( infos->mysql, stmt->str )){
		mysql_display_error( infos );
		goto free_stmt;
	}

	db_initialized = TRUE;

free_stmt:
	mysql_close( infos->mysql );
	g_free( infos );
	g_string_free( stmt, TRUE );

	g_debug( "%s: db_initialized=%s", thisfn, db_initialized ? "True":"False" );

	return( db_initialized );
}

#if 0
static void
drop_admin_account( mysqlConnect *connect, const gchar *account )
{
	MYSQL *mysql;
	mysqlConnect *cnt;

	cnt = g_new0( mysqlConnect, 1 );
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
#endif

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
mysql_display_error( sMySQLInfos *infos )
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

/*
 * mysql_get_db_exists:
 */
static gboolean
mysql_get_db_exists( sMySQLInfos *infos )
{
	gboolean exists;
	sMySQLInfos *temp;

	exists = FALSE;
	temp = g_new0( sMySQLInfos, 1 );
	memcpy( temp, infos, sizeof( sMySQLInfos ));
	temp->dbname = NULL;

	if( connect_with_infos( temp )){
		exists = local_get_db_exists( temp->mysql, infos->dbname );
		mysql_close( temp->mysql );
	}
	g_free( temp );

	return( exists );
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
 * ofa_mysql_get_connect_infos:
 * @infos: the connection structure to be filled.
 * @dname: the name of the dossier in user's settings
 *
 * Fill the connection structure with connection informations for the
 * current exercice, i.e. for Mysql: host, port and socket, database.
 *
 * Return: the connection structure itself.
 */
sMySQLInfos *
ofa_mysql_get_connect_infos( sMySQLInfos *infos, const gchar *label )
{
	infos->host = ofa_settings_dossier_get_string( label, SETTINGS_HOST );
	infos->socket = ofa_settings_dossier_get_string( label, SETTINGS_SOCKET );
	infos->port = ofa_settings_dossier_get_int( label, SETTINGS_PORT );
	if( infos->port == -1 ){
		infos->port = 0;
	}

	return( infos );
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
