/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 *
 * Most of this code is taken from:
 * http://scentric.net/tmp/spawn-async-with-pipes-gtk.c
 * (C) 2004 by Tim-Philipp Müller
 *
 *   This code demonstrates the usage of g_spawn_async_with_pipes() and
 *    GIOChannels as part of the Gtk+ main loop (ie. it demonstrates how
 *    to spawn a child process, read its output, and at the same time keep
 *    the GUI responsive).
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

#ifdef G_OS_UNIX
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"

#include "ofa-mysql-cmdline.h"
#include "ofa-mysql-connect.h"
#include "ofa-mysql-dossier-meta.h"
#include "ofa-mysql-exercice-meta.h"
#include "ofa-mysql-user-prefs.h"

#define BUFSIZE 4096

typedef struct {
	GtkWidget    *window;
	GtkWidget    *textview;
	GtkWidget    *close_btn;
	gboolean      command_ok;
	gulong        out_line;
	gulong        err_line;
	gboolean      verbose;
	myISettings  *settings;
	GMainLoop    *loop;
	ofaMsgCb      msg_cb;
	ofaDataCb     data_cb;
	void         *user_data;
}
	sExecuteInfos;

static const gchar *st_window_name = "MySQLExecutionWindow";

static gchar      *cmdline_build_from_connect( const gchar *template, const ofaMysqlConnect *connect, ofaMysqlExerciceMeta *period, const gchar *filename, const gchar *database );
static gchar      *cmdline_build_from_args( const gchar *template, const gchar *host, const gchar *socket, guint port, const gchar *account, const gchar *password, const gchar *dbname, const gchar *fname, const gchar *new_dbname );
static gboolean    do_execute_sync( const gchar *template, const ofaMysqlConnect *connect, ofaMysqlExerciceMeta *period, const gchar *fname, const gchar *newdb );
static gboolean    do_execute_async( const gchar *template, const ofaMysqlConnect *connect, ofaMysqlExerciceMeta *period, const gchar *fname, const gchar *window_title, GChildWatchFunc pfn, gboolean verbose );
static gboolean    do_execute_async2( const gchar *template, const ofaMysqlConnect *connect, ofaMysqlExerciceMeta *period, ofaMsgCb msg_cb, ofaDataCb data_cb, void *user_data );
static void        async_create_window( sExecuteInfos *infos, const gchar *window_title );
static GPid        async_exec_command( const gchar *cmdline, sExecuteInfos *infos );
static GPid        async_exec_command2( const gchar *cmdline, sExecuteInfos *infos );
static GIOChannel *async_setup_io_channel( gint fd, GIOFunc func, sExecuteInfos *infos );
static gboolean    async_stdout_fn( GIOChannel *ioc, GIOCondition cond, sExecuteInfos *infos );
static gboolean    async_stdout_fn2( GIOChannel *ioc, GIOCondition cond, sExecuteInfos *infos );
static gboolean    async_stdout_done( GIOChannel *ioc );
static gboolean    async_stderr_fn( GIOChannel *ioc, GIOCondition cond, sExecuteInfos *infos );
static gboolean    async_stderr_fn2( GIOChannel *ioc, GIOCondition cond, sExecuteInfos *infos );
static gboolean    async_stderr_done( GIOChannel *ioc );
static void        async_display_output( const gchar *str, sExecuteInfos *infos );
static void        backup_exit_cb( GPid child_pid, gint status, sExecuteInfos *infos );
static void        restore_exit_cb( GPid child_pid, gint status, sExecuteInfos *infos );
static gboolean    do_duplicate_grants( ofaIDBConnect *cnx, const gchar *host, const gchar *user_account, const gchar *prev_dbname, const gchar *new_dbname );

/**
 * ofa_mysql_cmdline_backup_get_default_command:
 *
 * Returns: the default command for backuping a database.
 */
const gchar *
ofa_mysql_cmdline_backup_get_default_command( void )
{
	return( "mysqldump --verbose %O -u%U -p%P %B | gzip -c > %F" );
}

/**
 * ofa_mysql_cmdline_backup_db_run:
 * @connect: a #ofaIDBConnect object which handles a user connection
 *  on the dossier/exercice to be backuped.
 * @msg_cb: a callback for messages to be displayed.
 * @data_cb: a callback for datas to be archived.
 * @user_data: user data to be provided to the callbacks.
 *
 * Backup the currently connected database.
 *
 * The outputed SQL file doesn't contain any CREATE DATABASE nor USE,
 * so that we will be able to reload the data to any database name.
 *
 * Returns: %TRUE if the database has been successfully backuped,
 * %FALSE else.
 */
gboolean
ofa_mysql_cmdline_backup_db_run( ofaMysqlConnect *connect, ofaMsgCb msg_cb, ofaDataCb data_cb, void *user_data )
{
	gchar *template;
	ofaIDBDossierMeta *dossier_meta;
	ofaIDBExerciceMeta *exercice_meta;
	ofaIDBProvider *provider;
	ofaHub *hub;
	gboolean ok;

	g_return_val_if_fail( connect && OFA_IS_MYSQL_CONNECT( connect ), FALSE );

	ofa_mysql_connect_query( connect, "FLUSH TABLES WITH READ LOCK" );

	dossier_meta = ofa_idbconnect_get_dossier_meta( OFA_IDBCONNECT( connect ));
	provider = ofa_idbdossier_meta_get_provider( dossier_meta );
	hub = ofa_idbprovider_get_hub( provider );
	template = ofa_mysql_user_prefs_get_backup_command( hub );
	template = g_strdup( "mysqldump --verbose %O -u%U -p%P %B" );
	exercice_meta = ofa_idbconnect_get_exercice_meta( OFA_IDBCONNECT( connect ));

	ok = do_execute_async2(
				template,
				connect,
				OFA_MYSQL_EXERCICE_META( exercice_meta ),
				msg_cb,
				data_cb,
				user_data );

	g_free( template );

	ofa_mysql_connect_query( connect, "UNLOCK TABLES" );

	return( ok );
}

/**
 * ofa_mysql_cmdline_restore_get_default_command:
 *
 * Returns: the default command for restoring a database.
 */
const gchar *
ofa_mysql_cmdline_restore_get_default_command( void )
{
	return( "mysql %O -u%U -p%P -e 'drop database if exists %B'; "
			"mysql %O -u%U -p%P -e 'create database %B character set utf8'; "
			"gzip -cd %F | mysql --verbose %O -u%U -p%P %B" );
}

/**
 * ofa_mysql_cmdline_restore_db_run:
 * @connect: a #ofaIDBConnect object which handles an opened superuser
 *  connection on the DBMS server. This object is expected to hold root
 *  account and password, and a non-%NULL #ofaIDBDossierMeta which
 *  identifies the target dossier.
 * @period: the #ofaIDBExerciceMeta object which qualifies the target
 *  exercice.
 * @uri: the URI of the file to be restored.
 *
 * Restores a backup file on an identifier dossier and exercice.
 *
 * Returns: %TRUE if the file has been successfully restored, %FALSE
 * else.
 */
gboolean
ofa_mysql_cmdline_restore_db_run( ofaMysqlConnect *connect,
									ofaMysqlExerciceMeta *period, const gchar *uri,
									ofaMsgCb msg_cb, ofaDataCb data_cb, void *user_data )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_restore";
	gboolean ok;
	gchar *fname, *template;
	ofaIDBDossierMeta *dossier_meta;
	ofaIDBProvider *provider;
	ofaHub *hub;

	g_debug( "%s: connect=%p, period=%p, uri=%s",
			thisfn, ( void * ) connect, ( void * ) period, uri );

	g_return_val_if_fail( connect && OFA_IS_MYSQL_CONNECT( connect ), FALSE );
	g_return_val_if_fail( period && OFA_IS_MYSQL_EXERCICE_META( period ), FALSE );
	g_return_val_if_fail( my_strlen( uri ), FALSE );

	/* It happens that MySQL has some issues with dropping an non-existant
	 * database - so create it first */
	do_execute_sync(
			"/bin/sh \"mysql -u%U -p%P -e 'create database %B'\"", connect, period, NULL, NULL );

	fname = g_filename_from_uri( uri, NULL, NULL );

	dossier_meta = ofa_idbconnect_get_dossier_meta( OFA_IDBCONNECT( connect ));
	provider = ofa_idbdossier_meta_get_provider( dossier_meta );
	hub = ofa_idbprovider_get_hub( provider );
	template = ofa_mysql_user_prefs_get_restore_command( hub );

	ok = do_execute_async(
				template,
				connect,
				period,
				fname,
				_( "Openbook restore" ),
				( GChildWatchFunc ) restore_exit_cb,
				TRUE );

	g_free( template );
	g_free( fname );

	return( ok );
}

/**
 * ofa_mysql_cmdline_archive_and_new:
 * @connect: an active #ofaIDBConnect user connection on the closed
 *  exercice (and the dossier settings have already been updated
 *  accordingly).
 * @root_account: administrator root account.
 * @root_password: administrator root password.
 * @begin_next: the beginning date of the next exercice.
 * @end_next: the ending date of the next exercice.
 *
 * Duplicate the corresponding database to a new one, creating the
 * corresponding line accordingly in the dossier settings.
 */
gboolean
ofa_mysql_cmdline_archive_and_new( ofaMysqlConnect *connect,
						const gchar *root_account, const gchar *root_password,
						const GDate *begin_next, const GDate *end_next )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_archive_and_new";
	ofaMysqlConnect *server_cnx;
	ofaIDBDossierMeta *dossier_meta;
	ofaIDBExerciceMeta *exercice_meta;
	const gchar *host, *socket, *prev_dbname, *prev_account;
	guint port;
	gchar *new_db;
	gboolean ok;
	gchar *cmdline, *cmd, *stdout, *stderr;
	gint status;

	/* meta informations on the current dossier */
	dossier_meta = ofa_idbconnect_get_dossier_meta( OFA_IDBCONNECT( connect ));
	g_return_val_if_fail( dossier_meta && OFA_IS_MYSQL_DOSSIER_META( dossier_meta ), FALSE );

	/* open a superuser new connection at DBMS server level */
	server_cnx = ofa_mysql_connect_new();
	if( !ofa_mysql_connect_open_with_meta(
				server_cnx, root_account, root_password, OFA_MYSQL_DOSSIER_META( dossier_meta ), NULL )){
		g_warning( "%s: unable to get a root connection on the DB server", thisfn );
		return( FALSE );
	}

	/* get previous database from current connection on closed exercice */
	exercice_meta = ofa_idbconnect_get_exercice_meta( OFA_IDBCONNECT( connect ));
	g_return_val_if_fail( exercice_meta && OFA_IS_MYSQL_EXERCICE_META( exercice_meta ), FALSE );

	prev_dbname = ofa_mysql_exercice_meta_get_database( OFA_MYSQL_EXERCICE_META( exercice_meta ));
	new_db = ofa_mysql_connect_get_new_database( server_cnx, prev_dbname );

	if( !my_strlen( new_db )){
		g_warning( "%s: unable to get a new database name", thisfn );
		g_object_unref( server_cnx );
		return( FALSE );
	}

	host = ofa_mysql_dossier_meta_get_host( OFA_MYSQL_DOSSIER_META( dossier_meta ));
	socket = ofa_mysql_dossier_meta_get_socket( OFA_MYSQL_DOSSIER_META( dossier_meta ));
	port = ofa_mysql_dossier_meta_get_port( OFA_MYSQL_DOSSIER_META( dossier_meta ));

	cmdline = cmdline_build_from_args(
					"mysql %O -u%U -p%P -e 'drop database if exists %N'; "
					"mysql %O -u%U -p%P -e 'create database %N'; "
					"mysqldump %O -u%U -p%P %B | mysql %O -u%U -p%P %N",
					host, socket, port, root_account, root_password, prev_dbname,
					NULL, new_db );

	cmd = g_strdup_printf( "/bin/sh -c \"%s\"", cmdline );
	g_debug( "%s: cmd=%s", thisfn, cmd );

	stdout = NULL;
	stderr = NULL;
	ok = g_spawn_command_line_sync( cmd, &stdout, &stderr, &status, NULL );
	g_debug( "%s: first try: exit_status=%d", thisfn, status );
	g_free( stdout );
	g_free( stderr );

	if( status > 0 ){
		stdout = NULL;
		stderr = NULL;
		ok = g_spawn_command_line_sync( cmd, &stdout, &stderr, &status, NULL );
		g_debug( "%s: second try: exit_status=%d", thisfn, status );
		g_free( stdout );
		g_free( stderr );
	}

	ok &= ( status == 0 );
	g_free( cmdline );
	g_free( cmd );

	if( ok ){
		exercice_meta = ofa_idbdossier_meta_new_period( dossier_meta, TRUE );
		ofa_idbexercice_meta_set_current( exercice_meta, TRUE );
		ofa_idbexercice_meta_set_begin_date( exercice_meta, begin_next );
		ofa_idbexercice_meta_set_end_date( exercice_meta, end_next );
		ofa_mysql_exercice_meta_set_database( OFA_MYSQL_EXERCICE_META( exercice_meta ), new_db );
		ofa_idbexercice_meta_update_settings( exercice_meta );
		prev_account = ofa_idbconnect_get_account( OFA_IDBCONNECT( connect ));
		do_duplicate_grants( OFA_IDBCONNECT( server_cnx ), host, prev_account, prev_dbname, new_db );
	}

	g_free( new_db );
	g_object_unref( server_cnx );

	return( ok );
}

/*
 * see mysql.h for a list of placeholders
 */
static gchar *
cmdline_build_from_connect( const gchar *template,
								const ofaMysqlConnect *connect, ofaMysqlExerciceMeta *period,
								const gchar *filename, const gchar *database )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_build_from_connect";
	gchar *cmdline;
	ofaIDBDossierMeta *dossier_meta;
	const gchar *host, *socket, *connect_database, *account, *password;
	guint port;

	g_debug( "%s: connect=%p, period=%p, template=%s, filename=%s, database=%s",
					thisfn, ( void * ) connect, ( void * ) period, template, filename, database );

	dossier_meta = ofa_idbconnect_get_dossier_meta( OFA_IDBCONNECT( connect ));
	g_return_val_if_fail( dossier_meta && OFA_IS_MYSQL_DOSSIER_META( dossier_meta ), NULL );

	host = ofa_mysql_dossier_meta_get_host( OFA_MYSQL_DOSSIER_META( dossier_meta ));
	socket = ofa_mysql_dossier_meta_get_socket( OFA_MYSQL_DOSSIER_META( dossier_meta ));
	port = ofa_mysql_dossier_meta_get_port( OFA_MYSQL_DOSSIER_META( dossier_meta ));

	connect_database = ofa_mysql_exercice_meta_get_database( period );

	account = ofa_idbconnect_get_account( OFA_IDBCONNECT( connect ));
	password = ofa_idbconnect_get_password( OFA_IDBCONNECT( connect ));

	cmdline = cmdline_build_from_args( template,
					host, socket, port, account, password, connect_database, filename, database );

	return( cmdline );
}

static gchar *
cmdline_build_from_args( const gchar *template,
							const gchar *host, const gchar *socket, guint port,
							const gchar *account, const gchar *password,
							const gchar *dbname, const gchar *fname, const gchar *new_dbname )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_build_from_args";
	gchar *sysfname, *cmdline;
	GString *options;
	GRegex *regex;
	gchar *newcmd;
	gchar *quoted;

	g_debug( "%s: host=%s, socket=%s, port=%u, "
				"account=%s, password=%s, dbname=%s, template=%s, fname=%s, new_dbname=%s",
					thisfn, host, socket, port,
					account, password ? "******":password, dbname, template, fname, new_dbname );

	cmdline = g_strdup( template );

	regex = g_regex_new( "%B", 0, 0, NULL );
	newcmd = g_regex_replace_literal( regex, cmdline, -1, 0, dbname, 0, NULL );
	g_regex_unref( regex );
	g_free( cmdline );
	cmdline = newcmd;

	if( my_strlen( fname )){
		sysfname = my_utils_filename_from_utf8( fname );
		quoted = g_shell_quote( sysfname );
		regex = g_regex_new( "%F", 0, 0, NULL );
		newcmd = g_regex_replace_literal( regex, cmdline, -1, 0, quoted, 0, NULL );
		g_regex_unref( regex );
		g_free( quoted );
		g_free( sysfname );
		g_free( cmdline );
		cmdline = newcmd;
	}

	if( my_strlen( new_dbname )){
		regex = g_regex_new( "%N", 0, 0, NULL );
		newcmd = g_regex_replace_literal( regex, cmdline, -1, 0, new_dbname, 0, NULL );
		g_regex_unref( regex );
		g_free( cmdline );
		cmdline = newcmd;
	}

	options = g_string_new( "" );
	if( my_strlen( host )){
		g_string_append_printf( options, "--host=%s ", host );
	}
	if( port > 0 ){
		g_string_append_printf( options, "--port=%u ", port );
	}
	if( my_strlen( socket )){
		g_string_append_printf( options, "--socket=%s ", socket );
	}

	regex = g_regex_new( "%O", 0, 0, NULL );
	newcmd = g_regex_replace_literal( regex, cmdline, -1, 0, options->str, 0, NULL );
	g_regex_unref( regex );
	g_free( cmdline );
	cmdline = newcmd;
	g_string_free( options, TRUE );

	regex = g_regex_new( "%P", 0, 0, NULL );
	newcmd = g_regex_replace_literal( regex, cmdline, -1, 0, password, 0, NULL );
	g_regex_unref( regex );
	g_free( cmdline );
	cmdline = newcmd;

	regex = g_regex_new( "%U", 0, 0, NULL );
	newcmd = g_regex_replace_literal( regex, cmdline, -1, 0, account, 0, NULL );
	g_regex_unref( regex );
	g_free( cmdline );
	cmdline = newcmd;

	return( cmdline );
}

static gboolean
do_execute_sync( const gchar *template,
					const ofaMysqlConnect *connect, ofaMysqlExerciceMeta *period,
					const gchar *fname, const gchar *newdb )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_do_execute_sync";
	gchar *cmdline, *stdout, *stderr;
	gboolean ok;

	cmdline = cmdline_build_from_connect( template, connect, period, fname, newdb );
	/* this may display the root password on debug output */
	g_debug( "%s: cmdline=%s", thisfn, cmdline );

	stdout = NULL;
	stderr = NULL;
	ok = FALSE;

	if( my_strlen( cmdline )){
		ok = g_spawn_command_line_sync( cmdline, &stdout, &stderr, NULL, NULL );
	}

	g_free( stdout );
	g_free( stderr );
	g_free( cmdline );

	return( ok );
}

static gboolean
do_execute_async( const gchar *template,
					const ofaMysqlConnect *connect, ofaMysqlExerciceMeta *period,
					const gchar *fname, const gchar *window_title, GChildWatchFunc pfn,
					gboolean verbose )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_do_execute_async";
	ofaIDBDossierMeta *dossier_meta;
	sExecuteInfos *infos;
	gchar *cmdline;
	GPid child_pid;
	gboolean ok;
	guint source_id;
	ofaIDBProvider *provider;
	ofaHub *hub;
	myISettings *settings;

	cmdline = cmdline_build_from_connect( template, connect, period, fname, NULL );
	g_debug( "%s: cmdline=%s", thisfn, cmdline );

	dossier_meta = ofa_idbconnect_get_dossier_meta( OFA_IDBCONNECT( connect ));
	provider = ofa_idbdossier_meta_get_provider( dossier_meta );
	hub = ofa_idbprovider_get_hub( provider );
	settings = ofa_hub_get_user_settings( hub );

	infos = g_new0( sExecuteInfos, 1 );
	infos->command_ok = FALSE;
	infos->out_line = 0;
	infos->err_line = 0;
	infos->verbose = verbose;
	infos->settings = settings;

	if( infos->verbose ){
		async_create_window( infos, window_title );
		g_debug( "%s: window=%p, textview=%p",
				thisfn, ( void * ) infos->window, ( void * ) infos->textview );
	} else {
		infos->loop = g_main_loop_new( NULL, FALSE );
	}

	child_pid = async_exec_command( cmdline, infos );
	g_debug("%s: child_pid=%lu", thisfn, ( gulong ) child_pid );

	if( child_pid != ( GPid ) 0 ){
		/* Watch the child, so we get the exit status */
		source_id = g_child_watch_add( child_pid, pfn, infos );
		g_debug( "%s: returning from g_child_watch_add, source_id=%u", thisfn, source_id );

		if( infos->verbose ){
			g_debug( "%s: running the display dialog", thisfn );
			gtk_dialog_run( GTK_DIALOG( infos->window ));

			my_utils_window_position_save( GTK_WINDOW( infos->window ), settings, st_window_name );

		} else {
			g_main_loop_run( infos->loop );
			g_main_loop_unref( infos->loop );
		}
	}

	if( infos->verbose ){
		g_debug( "%s: destroying the display dialog", thisfn );
		gtk_widget_destroy( infos->window );
	}

	ok = infos->command_ok;

	g_free( infos );
	g_free( cmdline );

	g_debug( "%s: returning %s", thisfn, ok ? "True":"False" );
	return( ok );
}

static gboolean
do_execute_async2( const gchar *template,
					const ofaMysqlConnect *connect, ofaMysqlExerciceMeta *period,
					ofaMsgCb msg_cb, ofaDataCb data_cb, void *user_data )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_do_execute_async2";
	gchar *cmdline;
	sExecuteInfos *infos;
	GPid child_pid;
	gboolean ok;
	guint source_id;

	cmdline = cmdline_build_from_connect( template, connect, period, NULL, NULL );
	g_debug( "%s: cmdline=%s", thisfn, cmdline );

	infos = g_new0( sExecuteInfos, 1 );
	infos->command_ok = FALSE;
	infos->out_line = 0;
	infos->err_line = 0;
	infos->msg_cb = msg_cb;
	infos->data_cb = data_cb;
	infos->user_data = user_data;

	child_pid = async_exec_command2( cmdline, infos );
	g_debug("%s: child_pid=%lu", thisfn, ( gulong ) child_pid );

	if( child_pid != ( GPid ) 0 ){
		infos->loop = g_main_loop_new( NULL, FALSE );
		/* Watch the child, so we get the exit status */
		source_id = g_child_watch_add( child_pid, ( GChildWatchFunc ) backup_exit_cb, infos );
		g_debug( "%s: returning from g_child_watch_add, source_id=%u", thisfn, source_id );
		g_main_loop_run( infos->loop );
		g_main_loop_unref( infos->loop );
	}

	ok = infos->command_ok;

	g_free( infos );
	g_free( cmdline );

	g_debug( "%s: returning %s", thisfn, ok ? "True":"False" );
	return( ok );
}

/*
 * the dialog is only created when running verbosely
 */
static void
async_create_window( sExecuteInfos *infos, const gchar *window_title )
{
	GtkWidget *content, *grid, *scrolled;

	infos->window = gtk_dialog_new_with_buttons(
							window_title,
							NULL,
							GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
							_( "_Close" ), GTK_RESPONSE_ACCEPT,
							NULL );

	content = gtk_dialog_get_content_area( GTK_DIALOG( infos->window ));

	grid = gtk_grid_new();
	gtk_container_add( GTK_CONTAINER( content ), grid );

	scrolled = gtk_scrolled_window_new( NULL, NULL );
	my_utils_widget_set_margins( scrolled, 4, 6, 4, 4 );
	gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( scrolled ), GTK_SHADOW_IN );
	gtk_grid_attach( GTK_GRID( grid ), scrolled, 0, 0, 1, 1 );

	infos->textview = gtk_text_view_new();
	gtk_widget_set_hexpand( infos->textview, TRUE );
	gtk_widget_set_vexpand( infos->textview, TRUE );
	my_utils_widget_set_margins( infos->textview, 2, 2, 2, 2 );
	gtk_text_view_set_editable( GTK_TEXT_VIEW( infos->textview ), FALSE );
	gtk_container_add( GTK_CONTAINER( scrolled ), infos->textview );

	infos->close_btn = gtk_dialog_get_widget_for_response( GTK_DIALOG( infos->window ), GTK_RESPONSE_ACCEPT );
	my_utils_widget_set_margins( infos->close_btn, 4, 4, 0, 8 );
	gtk_widget_set_sensitive( infos->close_btn, FALSE );

	my_utils_window_position_restore( GTK_WINDOW( infos->window ), infos->settings, st_window_name );

	gtk_widget_show_all( infos->window );
}

static GPid
async_exec_command( const gchar *cmdline, sExecuteInfos *infos )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_async_exec_command";
	gboolean ok;
	gchar *cmd;
	gint argc;
	gchar **argv;
	GError *error;
	GPid child_pid;
	gint stdout_fd;
	gint stderr_fd;

	ok = FALSE;
	error = NULL;
	child_pid = ( GPid ) 0;

	cmd = g_strdup_printf( "/bin/sh -c \"%s\"", cmdline );

	if( !g_shell_parse_argv( cmd, &argc, &argv, &error )){
		g_warning( "%s: g_shell_parse_argv: %s", thisfn, error->message );
		g_error_free( error );

	} else if( !g_spawn_async_with_pipes(
						NULL,
						argv,
						NULL,
						G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
						NULL,
						NULL,
						&child_pid,
						NULL,
						&stdout_fd,
						&stderr_fd,
						&error )){
		g_warning( "%s: g_spawn_async_with_pipes: %s", thisfn, error->message );
		g_error_free( error );
	} else {
		ok = TRUE;
	}

	g_strfreev( argv );
	g_free( cmd );

	if( ok ){
		/* Now use GIOChannels to monitor stdout and stderr */
		async_setup_io_channel( stdout_fd, ( GIOFunc ) async_stdout_fn, infos );
		async_setup_io_channel( stderr_fd, ( GIOFunc ) async_stderr_fn, infos );
	}

	return( ok ? child_pid : ( GPid ) 0 );
}

static GPid
async_exec_command2( const gchar *cmdline, sExecuteInfos *infos )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_async_exec_command2";
	gboolean ok;
	gchar *cmd;
	gint argc;
	gchar **argv;
	GError *error;
	GPid child_pid;
	gint stdout_fd;
	gint stderr_fd;
	GIOChannel *ioc;

	ok = FALSE;
	error = NULL;
	child_pid = ( GPid ) 0;

	cmd = g_strdup_printf( "/bin/sh -c \"%s\"", cmdline );

	if( !g_shell_parse_argv( cmd, &argc, &argv, &error )){
		g_warning( "%s: g_shell_parse_argv: %s", thisfn, error->message );
		g_error_free( error );

	} else if( !g_spawn_async_with_pipes(
						NULL,
						argv,
						NULL,
						G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
						NULL,
						NULL,
						&child_pid,
						NULL,
						&stdout_fd,
						&stderr_fd,
						&error )){
		g_warning( "%s: g_spawn_async_with_pipes: %s", thisfn, error->message );
		g_error_free( error );
	} else {
		ok = TRUE;
	}

	g_strfreev( argv );
	g_free( cmd );

	if( ok ){
		/* Now use GIOChannels to monitor stdout and stderr */
		ioc = async_setup_io_channel( stdout_fd, ( GIOFunc ) async_stdout_fn2, infos );
		g_io_channel_set_buffer_size( ioc, 16384 );	// have a 16k buffer on stdout for data stream
		async_setup_io_channel( stderr_fd, ( GIOFunc ) async_stderr_fn2, infos );
	}

	return( ok ? child_pid : ( GPid ) 0 );
}

/*
 * sets up callback to call whenever something interesting
 *   happens on a pipe
 */
static GIOChannel *
async_setup_io_channel( gint fd, GIOFunc func, sExecuteInfos *infos )
{
	GIOChannel *ioc;

	/* set up handler for data */
	ioc = g_io_channel_unix_new( fd );

	/* Set IOChannel encoding to none to make
	 *  it fit for binary data */
	g_io_channel_set_encoding( ioc, NULL, NULL );
	/* GLib-WARNING **: Need to have NULL encoding to set
	 * the buffering state of the channel. */
	g_io_channel_set_buffered( ioc, FALSE );

	/* Tell the io channel to close the file descriptor
	 *  when the io channel gets destroyed */
	g_io_channel_set_close_on_unref( ioc, TRUE );

	/* g_io_add_watch() adds its own reference,
	 *  which will be dropped when the watch source
	 *  is removed from the main loop (which happens
	 *  when we return FALSE from the callback) */
	g_io_add_watch( ioc, G_IO_IN|G_IO_PRI|G_IO_ERR|G_IO_HUP|G_IO_NVAL, func, infos );
	g_io_channel_unref( ioc );

	return( ioc );
}

/*
 * called when there's data to read from the stdout pipe
 *   or when the pipe is closed (ie. the program terminated)
 */
static gboolean
async_stdout_fn( GIOChannel *ioc, GIOCondition cond, sExecuteInfos *infos )
{
	GIOStatus ret;
	gchar buf[BUFSIZE];
	gsize len;
	gchar *str;

	/* data for us to read? */
	if( cond & (G_IO_IN | G_IO_PRI )){
		if( infos->verbose ){
			len = 0;
			memset( buf, 0x00, BUFSIZE );
			ret = g_io_channel_read_chars( ioc, buf, BUFSIZE, &len, NULL );
			if( len <= 0 || ret != G_IO_STATUS_NORMAL ){
				return( async_stdout_done( ioc ));	/* = return FALSE */
			}
			str = g_strdup_printf( "[stdout %lu] %s\n", ++infos->out_line, buf );
			async_display_output( str, infos );
			g_free( str );
		}
	}

	if( cond & ( G_IO_ERR | G_IO_HUP | G_IO_NVAL )){
		return( async_stdout_done( ioc ));		/* = return FALSE */
	}

	return( TRUE );
}

/*
 * called when there's data to read from the stdout pipe
 *   or when the pipe is closed (ie. the program terminated)
 * => transfer the available data to the output data_stream
 *
 * return FALSE if the event source should be removed
 */
static gboolean
async_stdout_fn2( GIOChannel *ioc, GIOCondition cond, sExecuteInfos *infos )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_async_stdout_fn";
	GIOStatus ret;
	gsize bufsize, read;
	gchar *iobuf;

	/* data for us to read? */
	if( cond & (G_IO_IN | G_IO_PRI )){
		bufsize = g_io_channel_get_buffer_size( ioc );
		iobuf = g_new0( gchar, bufsize );
		ret = g_io_channel_read_chars( ioc, iobuf, bufsize, &read, NULL );
		//g_debug( "%s: read=%lu", thisfn, read );
		if( read <= 0 || ret != G_IO_STATUS_NORMAL ){
			g_warning( "%s: aborting with read=%ld, ret=%d", thisfn, read, ( gint ) ret );
			return( async_stdout_done( ioc ));	/* = return FALSE */
		}

		/* the buffer is released by the callback */
		infos->data_cb( iobuf, read, infos->user_data );
	}

	if( cond & ( G_IO_ERR | G_IO_HUP | G_IO_NVAL )){
		return( async_stdout_done( ioc ));		/* = return FALSE */
	}

	return( TRUE );
}

/*
 * Called when the pipe is closed (ie. command terminated)
 *  Always returns FALSE (to make it nicer to use in the callback)
 */
static gboolean
async_stdout_done( GIOChannel *ioc )
{
	return( FALSE );
}

static gboolean
async_stderr_fn( GIOChannel *ioc, GIOCondition cond, sExecuteInfos *infos )
{
	GIOStatus ret;
	gchar buf[BUFSIZE];
	gsize len;
	gchar *str;

	/* data for us to read? */
	if( cond & (G_IO_IN | G_IO_PRI )){
		len = 0;
		memset( buf, 0x00, BUFSIZE );
		ret = g_io_channel_read_chars( ioc, buf, BUFSIZE, &len, NULL );
		if( len <= 0 || ret != G_IO_STATUS_NORMAL ){
			return( async_stderr_done( ioc )); /* = return FALSE */
		}
		str = g_strdup_printf( "[stderr %lu] %s\n", ++infos->err_line, buf );
		if( infos->verbose ){
			async_display_output( str, infos );
		} else {
			g_debug( "async_stderr_fn: str=%s", str );
		}
		g_free( str );
	}

	if( cond & ( G_IO_ERR | G_IO_HUP | G_IO_NVAL )){
		return( async_stderr_done( ioc )); /* = return FALSE */
	}

	return( TRUE );
}

static gboolean
async_stderr_fn2( GIOChannel *ioc, GIOCondition cond, sExecuteInfos *infos )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_async_stderr_fn2";
	GIOStatus ret;
	gsize bufsize, read;
	gchar *iobuf;

	/* data for us to read? */
	if( cond & (G_IO_IN | G_IO_PRI )){
		bufsize = g_io_channel_get_buffer_size( ioc );
		iobuf = g_new0( gchar, 1+bufsize );
		ret = g_io_channel_read_chars( ioc, iobuf, bufsize, &read, NULL );
		//g_debug( "%s: read=%lu", thisfn, read );
		if( read <= 0 || ret != G_IO_STATUS_NORMAL ){
			g_warning( "%s: aborting with read=%ld, ret=%d", thisfn, read, ( gint ) ret );
			return( async_stdout_done( ioc ));	/* = return FALSE */
		}

		/* the buffer is released by the callback */
		infos->msg_cb( iobuf, infos->user_data );
	}

	if( cond & ( G_IO_ERR | G_IO_HUP | G_IO_NVAL )){
		return( async_stderr_done( ioc )); /* = return FALSE */
	}

	return( TRUE );
}

static gboolean
async_stderr_done( GIOChannel *ioc )
{
	return( FALSE );
}

/*
 * this is only called when running verbosely
 */
static void
async_display_output( const gchar *str, sExecuteInfos *infos )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_async_display_output";
	GtkTextBuffer *textbuf;
	GtkTextIter enditer;
	const gchar *charset;
	gchar *utf8;

	textbuf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( infos->textview ));
	gtk_text_buffer_get_end_iter( textbuf, &enditer );

	/* Check if messages are in UTF-8. If not, assume
	 *  they are in current locale and try to convert.
	 *  We assume we're getting the stream in a 1-byte
	 *   encoding here, ie. that we do not have cut-off
	 *   characters at the end of our buffer (=BAD)
	 */
	if( g_utf8_validate( str, -1, NULL )){
		gtk_text_buffer_insert( textbuf, &enditer, str, -1 );

	} else {
		g_get_charset( &charset );
		utf8 = g_convert_with_fallback( str, -1, "UTF-8", charset, NULL, NULL, NULL, NULL );
		if( utf8 ){
			gtk_text_buffer_insert( textbuf, &enditer, utf8, -1 );
			g_free(utf8);

		} else {
			g_debug( "%s: message output is not in UTF-8 nor in locale charset", thisfn );
		}
	}

	/* A bit awkward, but better than nothing. Scroll text view to end */
	gtk_text_buffer_get_end_iter( textbuf, &enditer );
	gtk_text_buffer_move_mark_by_name( textbuf, "insert", &enditer );
	gtk_text_view_scroll_to_mark(
			GTK_TEXT_VIEW( infos->textview),
			gtk_text_buffer_get_mark( textbuf, "insert" ),
			0.0, FALSE, 0.0, 0.0 );

	/* let Gtk update the display */
	while( gtk_events_pending()){
		gtk_main_iteration();
	}
}

static void
backup_exit_cb( GPid child_pid, gint status, sExecuteInfos *infos )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_backup_exit_cb";
	gchar *msg;

	g_debug("%s: child_pid=%lu, exit status=%d", thisfn, ( gulong ) child_pid, status );
	msg = NULL;

	/* Not sure how the exit status works on win32. Unix code follows */
#ifdef G_OS_UNIX
	if( WIFEXITED( status )){			/* did child terminate in a normal way? */

		if( WEXITSTATUS( status ) == EXIT_SUCCESS ){
			msg = g_strdup( _( "Backup has successfully run" ));
			infos->command_ok = TRUE;
			g_debug( "%s: setting command_ok to TRUE", thisfn );

		} else {
			msg = g_strdup_printf(
						_( "Backup has exited with error code=%d" ), WEXITSTATUS( status ));
		}
	} else if( WIFSIGNALED( status )){	/* was it terminated by a signal ? */
		msg = g_strdup_printf(
						_( "Backup has exited with signal %d" ), WTERMSIG( status ));
	} else {
		msg = g_strdup( _( "Backup was terminated with errors" ));
	}
#endif /* G_OS_UNIX */

	g_spawn_close_pid( child_pid );		/* does nothing on unix, needed on win32 */

	if( !msg ){
		if( infos->command_ok ){
			msg = g_strdup( _( "Dossier successfully backuped" ));
		} else {
			msg = g_strdup( _( "An error occured while backuping the dossier" ));
		}
	}
	g_info( "%s: msg=%s", thisfn, msg );
	g_free( msg );

	g_main_loop_quit( infos->loop );
}

static void
restore_exit_cb( GPid child_pid, gint status, sExecuteInfos *infos )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_restore_exit_cb";
	GtkWidget *dlg;
	gchar *msg;

	g_debug("%s: child_pid=%lu, exit status=%d", thisfn, ( gulong ) child_pid, status );
	msg = NULL;

	/* Not sure how the exit status works on win32. Unix code follows */
#ifdef G_OS_UNIX
	if( WIFEXITED( status )){			/* did child terminate in a normal way? */

		if( WEXITSTATUS( status ) == EXIT_SUCCESS ){
			msg = g_strdup( _( "Restore has successfully run" ));
			infos->command_ok = TRUE;
			g_debug( "%s: setting restore_ok to TRUE", thisfn );

		} else {
			msg = g_strdup_printf(
						_( "Restore has exited with error code=%d" ), WEXITSTATUS( status ));
		}
	} else if( WIFSIGNALED( status )){	/* was it terminated by a signal */
		msg = g_strdup_printf(
						_( "Restore has exited with signal %d" ), WTERMSIG( status ));
	} else {
		msg = g_strdup( _( "Database was restored with errors" ));
	}
#endif /* G_OS_UNIX */

	g_spawn_close_pid( child_pid );		/* does nothing on unix, needed on win32 */

	if( infos->verbose ){
		if( !my_strlen( msg )){
			if( infos->command_ok ){
				msg = g_strdup(
						_( "Dossier successfully restored" ));
			} else {
				msg = g_strdup(
						_( "An error occured while restoring the dossier.\n"
							"If this the first time you are seeing this error, "
							"and you do not see any specific reason for that, "
							"you could take the chance of just retrying..." ));
			}
		}

		dlg = gtk_message_dialog_new(
					NULL,
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_INFO,
					GTK_BUTTONS_CLOSE,
					"%s", msg );

		gtk_dialog_run( GTK_DIALOG( dlg ));
		gtk_widget_destroy( dlg );

		gtk_widget_set_sensitive( infos->close_btn, TRUE );

	} else {
		g_main_loop_quit( infos->loop );
	}

	g_free( msg );
}

/*
 * infos structure must have been already filled up with DBMS root
 * credentials and target database
 */
static gboolean
do_duplicate_grants( ofaIDBConnect *connect, const gchar *host, const gchar *user_account, const gchar *prev_dbname, const gchar *new_dbname )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_do_duplicate_grants";
	gchar *hostname, *query, *str;
	GSList *result, *irow, *icol;
	GRegex *regex;
	const gchar *cstr;
	gboolean ok;

	hostname = g_strdup( host );
	if( !my_strlen( hostname )){
		g_free( hostname );
		hostname = g_strdup( "localhost" );
	}

	query = g_strdup_printf( "SHOW GRANTS FOR '%s'@'%s'", user_account, hostname );
	ok = ofa_idbconnect_query_ex( connect, query, &result, FALSE );
	g_free( query );
	if( !ok ){
		g_warning( "%s: %s", thisfn, ofa_idbconnect_get_last_error( connect ));
		goto free_stmt;
	}

	str = g_strdup_printf( " `(%s)`\\.\\* ", prev_dbname );
	regex = g_regex_new( str, 0, 0, NULL );
	g_free( str );

	str = g_strdup_printf( " `%s`.* ", new_dbname );
	g_debug( "%s: str=%s", thisfn, str );

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		cstr = ( const gchar * ) icol->data;
		g_debug( "%s: cstr=%s", thisfn, cstr );
		if( g_regex_match( regex, cstr, 0, NULL )){
			query = g_regex_replace_literal( regex, cstr, -1, 0, str, 0, NULL );
			g_debug( "%s: query=%s", thisfn, query );
			ofa_idbconnect_query( connect, query, FALSE );
			g_free( query );
		}
	}

	g_free( str );
	g_regex_unref( regex );

free_stmt:
	g_free( hostname );

	return( ok );
}
