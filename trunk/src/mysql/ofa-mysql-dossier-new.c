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
#include <string.h>

#include "api/my-utils.h"
#include "api/ofa-settings.h"

#include "ofa-mysql-dossier-new.h"

/*
 * this structure is attached to the GtkContainer parent of the grid
 * (i.e. the container from the DossierNewDlg dialog box)
 */
typedef struct {
	const ofaIDbms *module;
	mysqlNew        sNew;
	gboolean        success;
	GtkLabel       *msg;
	GtkButton      *browse_btn;
	gboolean        db_is_ok;
	gboolean        db_exists;
	gint            db_exists_mode;

	/* setup when appyling
	 */
	const gchar    *label;
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

#define MYSQL_NEW                 "mysql-data-new"

static const gchar *st_ui_xml   = PROVIDER_DATADIR "/ofa-mysql-dossier-new.piece.ui";
static const gchar *st_ui_mysql = "MySQLWindow";

static void       on_container_weak_notify( sPrivate *priv, GObject *was_the_container );
static GtkWidget *window_set_parent( const ofaIDbms *instance, GtkContainer *parent, GtkSizeGroup *group );
static void       window_init_entries( const ofaIDbms *instance, GtkContainer *parent, sPrivate *priv );
static void       window_init_db( const ofaIDbms *instance, GtkContainer *parent, sPrivate *priv );
static void       on_host_changed( GtkEntry *entry, sPrivate *priv );
static void       on_port_changed( GtkEntry *entry, sPrivate *priv );
static void       on_socket_changed( GtkEntry *entry, sPrivate *priv );
static void       on_root_account_changed( GtkEntry *entry, sPrivate *priv );
static void       on_root_password_changed( GtkEntry *entry, sPrivate *priv );
static void       check_for_dbserver_connection( sPrivate *priv );
static gboolean   test_dbserver_connect( mysqlConnect *connect );
static void       window_init_db( const ofaIDbms *instance, GtkContainer *parent, sPrivate *priv );
static void       on_db_name_changed( GtkEntry *entry, sPrivate *priv );
static void       on_db_find_clicked( GtkButton *button, sPrivate *priv );
static void       check_for_db( sPrivate *priv );
static void       on_db_exists_mode_changed( GtkComboBox *combo, sPrivate *priv );
static gboolean   do_apply( sPrivate *priv );
static gboolean   setup_new_dossier( sPrivate *priv );
static gboolean   create_db_as_root( sPrivate *priv );
static gboolean   create_user_as_root( sPrivate *priv );
static gboolean   init_db( sPrivate *priv );
static gboolean   confirm_database_reinit( sPrivate *priv, const gchar *dbname );

/*
 * @parent: the GtkContainer in the DossierNewDlg dialog box which will
 *  contain the provider properties grid
 */
void
ofa_mysql_properties_new_init( const ofaIDbms *instance, GtkContainer *parent, GtkSizeGroup *group )
{
	sPrivate *priv;
	GtkWidget *widget;

	priv = g_new0( sPrivate, 1 );
	g_object_set_data( G_OBJECT( parent ), MYSQL_NEW, priv );
	g_object_weak_ref( G_OBJECT( parent ), ( GWeakNotify ) on_container_weak_notify, priv );

	priv->module = instance;

	widget = window_set_parent( instance, parent, group );
	if( widget ){
		window_init_db( instance, GTK_CONTAINER( widget ), priv );
		window_init_entries( instance, GTK_CONTAINER( widget ), priv );
	}
}

static void
on_container_weak_notify( sPrivate *priv, GObject *was_the_container )
{
	static const gchar *thisfn = "ofa_mysql_dossier_new_on_container_weak_notify";

	g_debug( "%s: priv=%p, the_container_was=%p",
			thisfn, ( void * ) priv, ( void * ) was_the_container );

	ofa_mysql_free_connect( &priv->sNew.sConnect );
	g_free( priv );
}

static GtkWidget *
window_set_parent( const ofaIDbms *instance, GtkContainer *parent, GtkSizeGroup *group )
{
	GtkWidget *window;
	GtkWidget *grid;
	GtkWidget *label;

	/* attach our sgdb provider grid */
	window = my_utils_builder_load_from_path( st_ui_xml, st_ui_mysql );
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
window_init_entries( const ofaIDbms *instance, GtkContainer *parent, sPrivate *priv )
{
	GtkWidget *label, *entry;
	gchar *value;
	gint ivalue;

	label = my_utils_container_get_child_by_name( parent, "p2-message" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->msg = GTK_LABEL( label );

	entry = my_utils_container_get_child_by_name( parent, "p2-host" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_host_changed ), priv );
	value = ofa_settings_get_string( "DossierNewDlg-MySQL-host" );
	if( value && g_utf8_strlen( value, -1 )){
		gtk_entry_set_text( GTK_ENTRY( entry ), value );
	}
	g_free( value );

	entry = my_utils_container_get_child_by_name( parent, "p2-port" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_port_changed ), priv );
	ivalue = ofa_settings_get_uint( "DossierNewDlg-MySQL-port" );
	if( ivalue > 0 ){
		value = g_strdup_printf( "%u", ivalue );
		gtk_entry_set_text( GTK_ENTRY( entry ), value );
		g_free( value );
	}

	entry = my_utils_container_get_child_by_name( parent, "p2-socket" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_socket_changed ), priv );
	value = ofa_settings_get_string( "DossierNewDlg-MySQL-socket" );
	if( value && g_utf8_strlen( value, -1 )){
		gtk_entry_set_text( GTK_ENTRY( entry ), value );
	}
	g_free( value );

	entry = my_utils_container_get_child_by_name( parent, "p2-account" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_root_account_changed ), priv );
	value = ofa_settings_get_string( "DossierNewDlg-MySQL-account" );
	if( value && g_utf8_strlen( value, -1 )){
		gtk_entry_set_text( GTK_ENTRY( entry ), value );
	}
	g_free( value );

	entry = my_utils_container_get_child_by_name( parent, "p2-password" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_root_password_changed ), priv );
}

static void
on_host_changed( GtkEntry *entry, sPrivate *priv )
{
	const gchar *host;

	host = gtk_entry_get_text( entry );
	g_free( priv->sNew.sConnect.host );
	priv->sNew.sConnect.host = g_strdup( host );

	priv->success = FALSE;
	check_for_dbserver_connection( priv );
}

static void
on_port_changed( GtkEntry *entry, sPrivate *priv )
{
	const gchar *port;

	port = gtk_entry_get_text( entry );
	if( port && g_utf8_strlen( port, -1 )){
		priv->sNew.sConnect.port = atoi( port );
	} else {
		priv->sNew.sConnect.port = 0;
	}

	priv->success = FALSE;
	check_for_dbserver_connection( priv );
}

static void
on_socket_changed( GtkEntry *entry, sPrivate *priv )
{
	const gchar *socket;

	socket = gtk_entry_get_text( entry );
	g_free( priv->sNew.sConnect.socket );
	priv->sNew.sConnect.socket = g_strdup( socket );

	priv->success = FALSE;
	check_for_dbserver_connection( priv );
}

static void
on_root_account_changed( GtkEntry *entry, sPrivate *priv )
{
	const gchar *account;

	account = gtk_entry_get_text( entry );
	g_free( priv->sNew.sConnect.account );
	priv->sNew.sConnect.account = g_strdup( account );

	priv->success = FALSE;
	check_for_dbserver_connection( priv );
}

static void
on_root_password_changed( GtkEntry *entry, sPrivate *priv )
{
	const gchar *password;

	password = gtk_entry_get_text( entry );
	g_free( priv->sNew.sConnect.password );
	priv->sNew.sConnect.password = g_strdup( password );

	priv->success = FALSE;
	check_for_dbserver_connection( priv );
}

static void
check_for_dbserver_connection( sPrivate *priv )
{
	mysqlConnect *scnt;
	const gchar *msg;
	GdkRGBA color;

	if( !priv->success ){

		/* test a connexion without the database */
		scnt = g_new0( mysqlConnect, 1 );
		memcpy( scnt, &priv->sNew.sConnect, sizeof( mysqlConnect ));
		scnt->dbname = NULL;
		priv->success = test_dbserver_connect( scnt );
		g_free( scnt );

		msg = priv->success ?
				_( "DB server connection is OK" ) : _( "Unable to connect to DB server" );
		gtk_label_set_text( priv->msg, msg );
		if( gdk_rgba_parse( &color, priv->success ? "#000000" : "#FF0000" )){
			gtk_widget_override_color( GTK_WIDGET( priv->msg ), GTK_STATE_FLAG_NORMAL, &color );
		}
	}

	g_return_if_fail( priv->browse_btn && GTK_IS_BUTTON( priv->browse_btn ));
	gtk_widget_set_sensitive( GTK_WIDGET( priv->browse_btn ), priv->success );
}

static gboolean
test_dbserver_connect( mysqlConnect *connect )
{
	MYSQL *mysql;
	gboolean connect_ok;

	connect_ok = FALSE;
	mysql = ofa_mysql_connect( connect );

	if( mysql ){
		connect_ok = TRUE;
		mysql_close( mysql );
		g_free( mysql );
	}

	return( connect_ok );
}

/*
 * @parent: the provider grid
 */
static void
window_init_db( const ofaIDbms *instance, GtkContainer *parent, sPrivate *priv )
{
	GtkWidget *button, *widget;
	GtkComboBox *combo;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeIter iter;
	gint i, value, idx;

	button = my_utils_container_get_child_by_name( parent, "p2-browse" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_db_find_clicked ), priv );
	priv->browse_btn = GTK_BUTTON( button );

	widget = my_utils_container_get_child_by_name( parent, "p2-dbname" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_db_name_changed ), priv );

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
	value = ofa_settings_get_uint( "DossierNewDlg-dbexists_mode" );
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

	g_signal_connect( G_OBJECT( combo ), "changed", G_CALLBACK( on_db_exists_mode_changed ), priv );

	if( idx >= 0 ){
		gtk_combo_box_set_active( combo, idx );
	}
}

static void
on_db_name_changed( GtkEntry *entry, sPrivate *priv )
{
	const gchar *name;

	name = gtk_entry_get_text( entry );
	g_free( priv->sNew.sConnect.dbname );
	priv->sNew.sConnect.dbname = g_strdup( name );

	priv->db_is_ok = FALSE;
	check_for_db( priv );
}

static void
on_db_find_clicked( GtkButton *button, sPrivate *priv )
{

}

static void
check_for_db( sPrivate *priv )
{
	static const gchar *thisfn = "ofa_mysql_dossier_new_check_for_db";
	gboolean ok;

	if( !priv->db_is_ok ){

		ok = priv->sNew.sConnect.dbname &&
				g_utf8_strlen( priv->sNew.sConnect.dbname, -1 );

		priv->db_exists = ofa_mysql_get_db_exists( &priv->sNew.sConnect );

		ok &= priv->success &&
				( !priv->db_exists || priv->db_exists_mode > DBMODE_EMPTY );

		priv->db_is_ok = ok;
	}

	g_debug( "%s: db_is_ok=%s", thisfn, priv->db_is_ok ? "True":"False" );
}

static void
on_db_exists_mode_changed( GtkComboBox *combo, sPrivate *priv )
{
	static const gchar *thisfn = "ofa_mysql_dossier_new_on_db_exists_mode_changed";
	GtkTreeIter iter;
	GtkTreeModel *tmodel;

	g_debug( "%s: combo=%p, self=%p", thisfn, ( void * ) combo, ( void * ) priv );

	priv->db_exists_mode = DBMODE_EMPTY;

	if( gtk_combo_box_get_active_iter( combo, &iter )){
		tmodel = gtk_combo_box_get_model( combo );
		gtk_tree_model_get( tmodel, &iter,
				DB_COL_MODE, &priv->db_exists_mode,
				-1 );
	}

	g_debug( "%s: db_exists_mode=%u", thisfn, priv->db_exists_mode );
	check_for_db( priv );
}

/**
 * ofa_mysql_properties_new_check:
 */
gboolean
ofa_mysql_properties_new_check( const ofaIDbms *instance, GtkContainer *parent )
{
	gboolean ok;
	sPrivate *priv;

	priv = ( sPrivate * ) g_object_get_data( G_OBJECT( parent ), MYSQL_NEW );
	g_return_val_if_fail( priv, FALSE );

	ok = priv->success && priv->db_is_ok;

	return( ok );
}

/**
 * ofa_mysql_properties_new_apply:
 */
gboolean
ofa_mysql_properties_new_apply( const ofaIDbms *instance, GtkContainer *parent,
									const gchar *label, const gchar *account, const gchar *password )
{
	static const gchar *thisfn = "ofa_mysql_properties_new_apply";
	sPrivate *priv;
	gboolean ok;
	mysqlConnect *cnt;

	priv = ( sPrivate * ) g_object_get_data( G_OBJECT( parent ), MYSQL_NEW );

	g_debug( "%s: instance=%p, parent=%p, label=%s, account=%s, priv=%p",
			thisfn, ( void * ) instance, ( void * ) parent, label, account, ( void * ) priv );

	g_return_val_if_fail( priv, FALSE );

	priv->label = label;
	priv->account = account;
	priv->password = password;

	if( priv->db_exists && priv->db_exists_mode == DBMODE_REINIT ){
		if( !confirm_database_reinit( priv, priv->sNew.sConnect.dbname )){
			return( FALSE );
		}
	}

	ok = do_apply( priv );

	if( ok ){
		cnt = &priv->sNew.sConnect;

		if( cnt->host && g_utf8_strlen( cnt->host, -1 )){
			ofa_settings_set_string( "DossierNewDlg-MySQL-host", cnt->host );
		}
		if( cnt->port > 0 ){
			ofa_settings_set_uint( "DossierNewDlg-MySQL-port", cnt->port );
		}
		if( cnt->socket && g_utf8_strlen( cnt->socket, -1 )){
			ofa_settings_set_string( "DossierNewDlg-MySQL-socket", cnt->socket );
		}
		if( cnt->account && g_utf8_strlen( cnt->account, -1 )){
			ofa_settings_set_string( "DossierNewDlg-MySQL-account", cnt->account );
		}
		if( priv->db_exists_mode > DBMODE_EMPTY ){
			ofa_settings_set_uint( "DossierNewDlg-dbexists_mode", priv->db_exists_mode );
		}
	}

	return( ok );
}

/*
 * DB model will be setup at the first connection
 */
static gboolean
do_apply( sPrivate *priv )
{
	gboolean apply_ok;
	gboolean drop_db, drop_accounts;

	/* setup first the dossier in configuration file, so that it will
	 * later be usable when deleting the dossier in case of error */
	apply_ok = setup_new_dossier( priv ) &&
				create_db_as_root( priv ) &&
				create_user_as_root( priv ) &&
				init_db( priv );

	if( !apply_ok ){
		drop_db = ( !priv->db_exists || priv->db_exists_mode == DBMODE_REINIT );
		drop_accounts = TRUE;
		ofa_idbms_delete_dossier(
				priv->module,
				priv->label, priv->sNew.sConnect.account, priv->sNew.sConnect.password,
				drop_db, drop_accounts, FALSE );
	}

	return( apply_ok );
}

static gboolean
setup_new_dossier( sPrivate *priv )
{
	gboolean setup_ok;

	setup_ok = ofa_settings_set_dossier(
						priv->label,
						"Provider",    SETTINGS_TYPE_STRING, ofa_mysql_get_provider_name( NULL ),
						"Host",        SETTINGS_TYPE_STRING, priv->sNew.sConnect.host,
						"Port",        SETTINGS_TYPE_INT,    priv->sNew.sConnect.port,
						"Socket",      SETTINGS_TYPE_STRING, priv->sNew.sConnect.socket,
						"Database",    SETTINGS_TYPE_STRING, priv->sNew.sConnect.dbname,
						NULL );

	return( setup_ok );
}

/*
 * Create the empty database through a global connection to dataserver
 * - create database
 */
static gboolean
create_db_as_root( sPrivate *priv )
{
	static const gchar *thisfn = "ofa_mysql_dossier_new_create_db_as_root";
	MYSQL *mysql;
	mysqlConnect *cnt;
	GString *stmt;
	gboolean db_created;

	g_debug( "%s: priv=%p", thisfn, ( void * ) priv );

	if( priv->db_exists && priv->db_exists_mode == DBMODE_LEAVE_AS_IS ){
		return( TRUE );
	}

	cnt = g_new0( mysqlConnect, 1 );
	cnt->host = priv->sNew.sConnect.host;
	cnt->port = priv->sNew.sConnect.port;
	cnt->socket = priv->sNew.sConnect.socket;
	cnt->dbname = "mysql";
	cnt->account = priv->sNew.sConnect.account;
	cnt->password = priv->sNew.sConnect.password;

	mysql = ofa_mysql_connect( cnt );

	if( !mysql ){
		g_free( cnt );
		g_debug( "%s: unable to connect", thisfn );
		return( FALSE );
	}

	db_created = FALSE;
	stmt = g_string_new( "" );

	g_string_printf( stmt, "DROP DATABASE %s", priv->sNew.sConnect.dbname );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	mysql_query( mysql, stmt->str );

	g_string_printf( stmt, "CREATE DATABASE %s", priv->sNew.sConnect.dbname );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	if( mysql_query( mysql, stmt->str )){
		g_debug( "%s: %s", thisfn, mysql_error( mysql ));
		goto free_stmt;
	}

	db_created = TRUE;

free_stmt:
	mysql_close( mysql );
	g_free( mysql );
	g_string_free( stmt, TRUE );
	g_free( cnt );

	g_debug( "%s: db_created=%s", thisfn, db_created ? "True":"False" );

	return( db_created );
}

/*
 * Create the empty database with a global connection to dataserver
 * - create admin user
 * - grant admin user
 */
static gboolean
create_user_as_root( sPrivate *priv )
{
	static const gchar *thisfn = "ofa_mysql_dossier_new_create_user_as_root";
	MYSQL *mysql;
	mysqlConnect *cnt;
	GString *stmt;
	gboolean user_created;
	gchar *hostname;

	g_debug( "%s: self=%p", thisfn, ( void * ) priv );

	cnt = g_new0( mysqlConnect, 1 );
	cnt->host = priv->sNew.sConnect.host;
	cnt->port = priv->sNew.sConnect.port;
	cnt->socket = priv->sNew.sConnect.socket;
	cnt->dbname = "mysql";
	cnt->account = priv->sNew.sConnect.account;
	cnt->password = priv->sNew.sConnect.password;

	mysql = ofa_mysql_connect( cnt );

	if( !mysql ){
		g_free( cnt );
		g_debug( "%s: unable to connect", thisfn );
		return( FALSE );
	}

	user_created = FALSE;
	stmt = g_string_new( "" );

	hostname = g_strdup( priv->sNew.sConnect.host );
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
	mysql_query( mysql, stmt->str );

	g_string_printf( stmt,
			"GRANT ALL ON %s.* TO '%s'@'%s' WITH GRANT OPTION",
				priv->sNew.sConnect.dbname,
				priv->account,
				hostname );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	if( mysql_query( mysql, stmt->str )){
		g_debug( "%s: %s", thisfn, mysql_error( mysql ));
		goto free_stmt;
	}

	g_string_printf( stmt,
			"GRANT CREATE USER, FILE ON *.* TO '%s'@'%s'",
				priv->account,
				hostname );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	if( mysql_query( mysql, stmt->str )){
		g_debug( "%s: %s", thisfn, mysql_error( mysql ));
		goto free_stmt;
	}

	user_created = TRUE;

free_stmt:
	mysql_close( mysql );
	g_free( mysql );
	g_string_free( stmt, TRUE );
	g_free( cnt );
	g_free( hostname );

	g_debug( "%s: user_created=%s", thisfn, user_created ? "True":"False" );

	return( user_created );
}

static gboolean
init_db( sPrivate *priv )
{
	static const gchar *thisfn = "ofa_mysql_dossier_new_init_db";
	MYSQL *mysql;
	mysqlConnect *cnt;
	GString *stmt;
	gboolean db_initialized;

	g_debug( "%s: priv=%p", thisfn, ( void * ) priv );

	if( priv->db_exists && priv->db_exists_mode == DBMODE_LEAVE_AS_IS ){
		return( TRUE );
	}

	cnt = g_new0( mysqlConnect, 1 );
	cnt->host = priv->sNew.sConnect.host;
	cnt->port = priv->sNew.sConnect.port;
	cnt->socket = priv->sNew.sConnect.socket;
	cnt->dbname = priv->sNew.sConnect.dbname;
	cnt->account = ( gchar * ) priv->account;
	cnt->password = ( gchar * ) priv->password;

	mysql = ofa_mysql_connect( cnt );

	if( !mysql ){
		g_free( cnt );
		g_debug( "%s: unable to connect", thisfn );
		return( FALSE );
	}

	db_initialized = FALSE;
	stmt = g_string_new( "" );

	g_string_printf( stmt,
			"CREATE TABLE IF NOT EXISTS %s.OFA_T_AUDIT ("
			"	AUD_ID    INTEGER AUTO_INCREMENT NOT NULL UNIQUE COMMENT 'Intern identifier',"
			"	AUD_STAMP TIMESTAMP              NOT NULL        COMMENT 'Query actual timestamp',"
			"	AUD_QUERY VARCHAR(4096)          NOT NULL        COMMENT 'Query')",
					priv->sNew.sConnect.dbname );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	if( mysql_query( mysql, stmt->str )){
		g_debug( "%s: %s", thisfn, mysql_error( mysql ));
		goto free_stmt;
	}

	g_string_printf( stmt,
			"CREATE TABLE IF NOT EXISTS %s.OFA_T_ROLES ("
				"ROL_USER     VARCHAR(20) BINARY NOT NULL UNIQUE COMMENT 'User account',"
				"ROL_IS_ADMIN INTEGER                            COMMENT 'Whether the user has administration role')",
					priv->sNew.sConnect.dbname );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	if( mysql_query( mysql, stmt->str )){
		g_debug( "%s: %s", thisfn, mysql_error( mysql ));
		goto free_stmt;
	}

	g_string_printf( stmt,
			"INSERT IGNORE INTO %s.OFA_T_ROLES "
			"	(ROL_USER, ROL_IS_ADMIN) VALUES ('%s',1)",
				priv->sNew.sConnect.dbname, priv->account );
	g_debug( "%s: query=%s", thisfn, stmt->str );
	if( mysql_query( mysql, stmt->str )){
		g_debug( "%s: %s", thisfn, mysql_error( mysql ));
		goto free_stmt;
	}

	db_initialized = TRUE;

free_stmt:
	mysql_close( mysql );
	g_free( mysql );
	g_string_free( stmt, TRUE );
	g_free( cnt );

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
			"This operation will cannot be recoverable.\n"
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
