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

#include <stdlib.h>
#include <string.h>

#ifdef G_OS_UNIX
#include <gio/gunixoutputstream.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include "my/my-utils.h"

#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-idbsuperuser.h"
#include "api/ofa-igetter.h"

#include "ofa-mysql-cmdline.h"
#include "ofa-mysql-connect.h"
#include "ofa-mysql-dossier-meta.h"
#include "ofa-mysql-exercice-meta.h"
#include "ofa-mysql-user-prefs.h"

/*
 * whether we are running a backup or a restore operation
 */
enum {
	RUN_BACKUP = 1,
	RUN_RESTORE
};

/*
 * when outputing from an IOChannel, whether we expect to deal with a
 * message to be displayed or with data flow
 */
enum {
	OUTPUT_MSG = 1,
	OUTPUT_DATA
};

typedef struct {
	gboolean   command_ok;
	GMainLoop *loop;
	guint      runtype;
	ofaMsgCb   msg_cb;
	ofaDataCb  data_cb;
	void      *user_data;
}
	sExecuteInfos;

static void        do_create_database( ofaMysqlConnect *connect, ofaMysqlExerciceMeta *period );
static gchar      *cmdline_build_from_connect( const gchar *template, const ofaMysqlConnect *connect, ofaMysqlExerciceMeta *period, const gchar *uri, const gchar *new_dbname );
static gchar      *cmdline_build_from_args( const gchar *template, const gchar *host, const gchar *socket, guint port, const gchar *account, const gchar *password, const gchar *dbname, const gchar *runtime_dir, const gchar *uri, const gchar *new_dbname );
static gboolean    do_execute( const gchar *cmdline );
static gboolean    do_execute_async( const gchar *template, const ofaMysqlConnect *connect, ofaMysqlExerciceMeta *period, const gchar *uri, guint runtype, ofaMsgCb msg_cb, ofaDataCb data_cb, void *user_data );
static GPid        async_exec_command( const gchar *cmdline, sExecuteInfos *infos );
static GIOChannel *async_setup_out_channel( gint fd, GIOFunc func, sExecuteInfos *infos );
static gboolean    async_stdout_fn( GIOChannel *ioc, GIOCondition cond, sExecuteInfos *infos );
static gboolean    async_stderr_fn( GIOChannel *ioc, GIOCondition cond, sExecuteInfos *infos );
static gboolean    async_output_fn( GIOChannel *ioc, GIOCondition cond, sExecuteInfos *infos, guint datatype );
static void        exit_cb( GPid child_pid, gint status, sExecuteInfos *infos );
static gboolean    do_duplicate_grants( ofaIDBConnect *cnx, const gchar *host, const gchar *user_account, const gchar *prev_dbname, const gchar *new_dbname );

/**
 * ofa_mysql_cmdline_backup_get_default_command:
 *
 * Returns: the default command for backuping a database.
 */
const gchar *
ofa_mysql_cmdline_backup_get_default_command( void )
{
	return( "mysqldump --verbose %O -u%Ca -p%Cp %Db" );
}

/**
 * ofa_mysql_cmdline_backup_db_run:
 * @connect: a #ofaIDBConnect object which handles a user connection
 *  on the dossier/exercice to be backuped.
 * @uri: the target uri.
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
ofa_mysql_cmdline_backup_db_run( ofaMysqlConnect *connect, const gchar *uri, ofaMsgCb msg_cb, ofaDataCb data_cb, void *user_data )
{
	gchar *template;
	ofaIDBDossierMeta *dossier_meta;
	ofaIDBExerciceMeta *exercice_meta;
	ofaIDBProvider *provider;
	ofaIGetter *getter;
	gboolean ok;

	g_return_val_if_fail( connect && OFA_IS_MYSQL_CONNECT( connect ), FALSE );

	ofa_mysql_connect_query( connect, "FLUSH TABLES WITH READ LOCK" );

	dossier_meta = ofa_idbconnect_get_dossier_meta( OFA_IDBCONNECT( connect ));
	provider = ofa_idbdossier_meta_get_provider( dossier_meta );
	getter = ofa_idbprovider_get_getter( provider );
	template = ofa_mysql_user_prefs_get_backup_command( getter );
	exercice_meta = ofa_idbconnect_get_exercice_meta( OFA_IDBCONNECT( connect ));

	/* the command we are executing here sends its data flow to stdout
	 * we get the messages to be displayed in stderr
	 */
	ok = do_execute_async(
				template,
				connect,
				OFA_MYSQL_EXERCICE_META( exercice_meta ),
				uri,
				RUN_BACKUP,
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
	return( "unzip -qc %Ap | mysql --verbose --comments %O -u%Ca -p%Cp %Db" );
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
ofa_mysql_cmdline_restore_db_run( ofaMysqlConnect *connect, ofaMysqlExerciceMeta *period, const gchar *uri, guint format,
									ofaMsgCb msg_cb, ofaDataCb data_cb, void *user_data )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_restore_db_run";
	gboolean ok;
	gchar *template;
	ofaIDBDossierMeta *dossier_meta;
	ofaIDBProvider *provider;
	ofaIGetter *getter;

	g_debug( "%s: connect=%p, period=%p, uri=%s, format=%u, msg_cb=%p, data_cb=%p, user_data=%p",
			thisfn, ( void * ) connect, ( void * ) period,
			uri, format, ( void * ) msg_cb, ( void * ) data_cb, user_data );

	g_return_val_if_fail( connect && OFA_IS_MYSQL_CONNECT( connect ), FALSE );
	g_return_val_if_fail( period && OFA_IS_MYSQL_EXERCICE_META( period ), FALSE );

	do_create_database( connect, period );

	dossier_meta = ofa_idbconnect_get_dossier_meta( OFA_IDBCONNECT( connect ));
	provider = ofa_idbdossier_meta_get_provider( dossier_meta );
	getter = ofa_idbprovider_get_getter( provider );
	template = ofa_mysql_user_prefs_get_restore_command( getter, format );

	/* the command we are executing here takes its input from stdin
	 * we get messages to be displayed both in stdout and stderr
	 */
	ok = do_execute_async(
				template,
				connect,
				period,
				uri,
				RUN_RESTORE,
				msg_cb,
				data_cb,
				user_data );

	g_free( template );

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
						ofaIDBSuperuser *su, const GDate *begin_next, const GDate *end_next )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_archive_and_new";
	ofaIDBConnect *server_cnx;
	ofaIDBDossierMeta *dossier_meta;
	ofaIDBExerciceMeta *exercice_meta;
	const gchar *host, *socket, *prev_dbname, *prev_account, *root_account, *root_password;
	guint port;
	gchar *new_db;
	gboolean ok;
	gchar *cmdline, *cmd, *stdout, *stderr;
	gint status;

	g_return_val_if_fail( su && OFA_IS_MYSQL_ROOT_BIN( su ), FALSE );

	/* meta informations on the current dossier */
	dossier_meta = ofa_idbconnect_get_dossier_meta( OFA_IDBCONNECT( connect ));
	g_return_val_if_fail( dossier_meta && OFA_IS_MYSQL_DOSSIER_META( dossier_meta ), FALSE );

	/* open a superuser new connection at DBMS server level */
	server_cnx = ofa_idbdossier_meta_new_connect( dossier_meta, NULL );
	root_account = ofa_mysql_root_bin_get_account( OFA_MYSQL_ROOT_BIN( su ));
	root_password = ofa_mysql_root_bin_get_password( OFA_MYSQL_ROOT_BIN( su ));

	if( !ofa_idbconnect_open_with_superuser( server_cnx, su )){
		g_warning( "%s: unable to open a super-user connection on the DBMS server", thisfn );
		return( FALSE );
	}

	/* get previous database from current connection on closed exercice */
	exercice_meta = ofa_idbconnect_get_exercice_meta( OFA_IDBCONNECT( connect ));
	g_return_val_if_fail( exercice_meta && OFA_IS_MYSQL_EXERCICE_META( exercice_meta ), FALSE );

	prev_dbname = ofa_mysql_exercice_meta_get_database( OFA_MYSQL_EXERCICE_META( exercice_meta ));
	new_db = ofa_mysql_connect_get_new_database( OFA_MYSQL_CONNECT( server_cnx ), prev_dbname );

	if( !my_strlen( new_db )){
		g_warning( "%s: unable to get a new database name", thisfn );
		g_object_unref( server_cnx );
		return( FALSE );
	}

	host = ofa_mysql_dossier_meta_get_host( OFA_MYSQL_DOSSIER_META( dossier_meta ));
	socket = ofa_mysql_dossier_meta_get_socket( OFA_MYSQL_DOSSIER_META( dossier_meta ));
	port = ofa_mysql_dossier_meta_get_port( OFA_MYSQL_DOSSIER_META( dossier_meta ));

	cmdline = cmdline_build_from_args(
					"mysql %O -u%Ca -p%Cp -e 'drop database if exists %Dn'; "
					"mysql %O -u%Ca -p%Cp -e 'create database %Dn character set utf8'; "
					"mysqldump %O -u%Ca -p%Cp %Db | mysql %O -u%Ca -p%Cp %Dn",
					host, socket, port, root_account, root_password, prev_dbname,
					NULL, NULL, new_db );

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
		do_duplicate_grants( server_cnx, host, prev_account, prev_dbname, new_db );
	}

	g_free( new_db );
	g_object_unref( server_cnx );

	return( ok );
}

/*
 * @connect: must handle a superuser connection on the DBMS
 */
static void
do_create_database( ofaMysqlConnect *connect, ofaMysqlExerciceMeta *period )
{
	static const gchar *template =
			"/bin/sh -c \""
			"	mysql -u%Ca -p%Cp -e 'drop database %Db';"
			"	mysql -u%Ca -p%Cp -e 'create database %Db character set utf8'"
			"\"";

	gchar *command = cmdline_build_from_connect( template, connect, period, NULL, NULL );
	do_execute( command );
	g_free( command );
}

/*
 * see mysql.h for a list of placeholders
 */
static gchar *
cmdline_build_from_connect( const gchar *template,
								const ofaMysqlConnect *connect, ofaMysqlExerciceMeta *period,
								const gchar *uri, const gchar *new_dbname )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_build_from_connect";
	gchar *cmdline;
	ofaIDBDossierMeta *dossier_meta;
	ofaIDBProvider *provider;
	ofaIGetter *getter;
	const gchar *host, *socket, *period_database, *account, *password, *runtime_dir;
	guint port;

	g_debug( "%s: connect=%p, period=%p, template=%s, uri=%s, new_dbname=%s",
					thisfn, ( void * ) connect, ( void * ) period, template, uri, new_dbname );

	dossier_meta = ofa_idbconnect_get_dossier_meta( OFA_IDBCONNECT( connect ));
	g_return_val_if_fail( dossier_meta && OFA_IS_MYSQL_DOSSIER_META( dossier_meta ), NULL );

	provider = ofa_idbdossier_meta_get_provider( dossier_meta );
	getter = ofa_idbprovider_get_getter( provider );
	runtime_dir = ofa_igetter_get_runtime_dir( getter );

	host = ofa_mysql_dossier_meta_get_host( OFA_MYSQL_DOSSIER_META( dossier_meta ));
	socket = ofa_mysql_dossier_meta_get_socket( OFA_MYSQL_DOSSIER_META( dossier_meta ));
	port = ofa_mysql_dossier_meta_get_port( OFA_MYSQL_DOSSIER_META( dossier_meta ));

	period_database = ofa_mysql_exercice_meta_get_database( period );

	account = ofa_idbconnect_get_account( OFA_IDBCONNECT( connect ));
	password = ofa_idbconnect_get_password( OFA_IDBCONNECT( connect ));

	cmdline = cmdline_build_from_args( template,
					host, socket, port, account, password, period_database, runtime_dir, uri, new_dbname );

	return( cmdline );
}

static gchar *
cmdline_build_from_args( const gchar *template,
							const gchar *host, const gchar *socket, guint port,
							const gchar *account, const gchar *password,
							const gchar *dbname, const gchar *runtime_dir, const gchar *uri, const gchar *new_dbname )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_build_from_args";
	GFile *file;
	gchar *pathname, *sysfname, *cmdline, *quoted, *newcmd;
	GString *options;
	GRegex *regex;

	g_debug( "%s: template=%s, host=%s, socket=%s, port=%u, "
				"account=%s, password=%s, dbname=%s, runtime_dir=%s, uri=%s, new_dbname=%s",
					thisfn, template, host, socket, port,
					account, password ? "******":password, dbname, runtime_dir, uri, new_dbname );

	cmdline = g_strdup( template );

	/* %Ap: the archive full pathname */
	file = my_strlen( uri ) ? g_file_new_for_uri( uri ) : NULL;
	pathname = file ? g_file_get_path( file ) : NULL;
	sysfname = pathname ? my_utils_filename_from_utf8( pathname ) : NULL;
	quoted = sysfname ? g_shell_quote( sysfname ) : g_strdup( "" );
	regex = g_regex_new( "%Ap", 0, 0, NULL );
	newcmd = g_regex_replace_literal( regex, cmdline, -1, 0, quoted, 0, NULL );
	g_regex_unref( regex );
	g_free( quoted );
	g_free( sysfname );
	g_free( pathname );
	g_clear_object( &file );
	g_free( cmdline );
	cmdline = newcmd;

	/* %Au: the archive uri */
	regex = g_regex_new( "%Au", 0, 0, NULL );
	newcmd = g_regex_replace_literal( regex, cmdline, -1, 0, uri ? uri : "", 0, NULL );
	g_regex_unref( regex );
	g_free( cmdline );
	cmdline = newcmd;

	/* %Ca: the connection account */
	regex = g_regex_new( "%Ca", 0, 0, NULL );
	newcmd = g_regex_replace_literal( regex, cmdline, -1, 0, account ? account : "", 0, NULL );
	g_regex_unref( regex );
	g_free( cmdline );
	cmdline = newcmd;

	/* %Cp: the connection password */
	regex = g_regex_new( "%Cp", 0, 0, NULL );
	newcmd = g_regex_replace_literal( regex, cmdline, -1, 0, password ? password : "", 0, NULL );
	g_regex_unref( regex );
	g_free( cmdline );
	cmdline = newcmd;

	/* %Db: the database name which comes from the ExerciceMeta */
	regex = g_regex_new( "%Db", 0, 0, NULL );
	newcmd = g_regex_replace_literal( regex, cmdline, -1, 0, dbname ? dbname : "", 0, NULL );
	g_regex_unref( regex );
	g_free( cmdline );
	cmdline = newcmd;

	/* %Dn: the new DB name */
	regex = g_regex_new( "%Dn", 0, 0, NULL );
	newcmd = g_regex_replace_literal( regex, cmdline, -1, 0, new_dbname ? new_dbname : "", 0, NULL );
	g_regex_unref( regex );
	g_free( cmdline );
	cmdline = newcmd;

	/* %O: options --host --port --socket */
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

	/* %Xd: directory where Openbook binaries are executed from */
	regex = g_regex_new( "%Xd", 0, 0, NULL );
	newcmd = g_regex_replace_literal( regex, cmdline, -1, 0, runtime_dir ? runtime_dir : "", 0, NULL );
	g_regex_unref( regex );
	g_free( cmdline );
	cmdline = newcmd;

	return( cmdline );
}

/*
 * Sync. execution: the function returns after the command returns.
 */
static gboolean
do_execute( const gchar *cmdline )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_do_execute";
	gchar *stdout, *stderr;
	gint exit_status;
	GError *error;
	gboolean ok;

	/* this may display the root password on debug output */
	g_debug( "%s: cmdline=%s", thisfn, cmdline );

	stdout = NULL;
	stderr = NULL;
	error = NULL;
	ok = FALSE;

	if( my_strlen( cmdline )){
		ok = g_spawn_command_line_sync( cmdline, &stdout, &stderr, &exit_status, &error );
		if( !ok ){
			g_warning( "%s: %s", thisfn, error->message );
			g_error_free( error );
		}
		g_debug( "%s: exit_status=%d, stdout=%s, stderr=%s", thisfn, exit_status, stdout, stderr );
	}

	g_free( stdout );
	g_free( stderr );

	return( ok );
}

/*
 *               stdin            stdout                stderr
 *               ---------------  --------------------  ---------------
 * Backup        NULL             Datas (16384)         messages
 * Restore       NULL             messages              messages
 *
 * pwi 2017- 1-16: unable to have a working pipe with a callback
 *  providing data flow to stdin on restore operations - so get
 *  stuck with a full pipe
 */
static gboolean
do_execute_async( const gchar *template,
					const ofaMysqlConnect *connect, ofaMysqlExerciceMeta *period, const gchar *uri,
					guint runtype, ofaMsgCb msg_cb, ofaDataCb data_cb, void *user_data )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_do_execute_async";
	gchar *cmdline;
	sExecuteInfos *infos;
	GPid child_pid;
	gboolean ok;
	guint source_id;

	cmdline = cmdline_build_from_connect( template, connect, period, uri, NULL );
	g_debug( "%s: cmdline=%s", thisfn, cmdline );

	infos = g_new0( sExecuteInfos, 1 );
	infos->command_ok = FALSE;
	infos->runtype = runtype;
	infos->msg_cb = msg_cb;
	infos->data_cb = data_cb;
	infos->user_data = user_data;
	infos->loop = g_main_loop_new( NULL, FALSE );

	child_pid = async_exec_command( cmdline, infos );
	g_debug("%s: child_pid=%lu", thisfn, ( gulong ) child_pid );

	if( child_pid != ( GPid ) 0 ){
		/* Watch the child, so we get the exit status */
		source_id = g_child_watch_add( child_pid, ( GChildWatchFunc ) exit_cb, infos );
		g_debug( "%s: returning from g_child_watch_add, source_id=%u", thisfn, source_id );
		/* block into g_main_loop_run() until exit_cb() is called */
		g_main_loop_run( infos->loop );
		g_main_loop_unref( infos->loop );
	}

	ok = infos->command_ok;

	g_free( infos );
	g_free( cmdline );

	g_debug( "%s: returning %s", thisfn, ok ? "True":"False" );
	return( ok );
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

	/* Now use GIOChannels to monitor stdout / stderr */
	if( ok ){
		ioc = async_setup_out_channel( stdout_fd, ( GIOFunc ) async_stdout_fn, infos );

		/* on backup operations, configure the stdout IO channel with a
		 *  bigger data flow buffer */
		if( infos->runtype == RUN_BACKUP ){
			g_io_channel_set_buffer_size( ioc, 16384 );
		}

		async_setup_out_channel( stderr_fd, ( GIOFunc ) async_stderr_fn, infos );
	}

	return( ok ? child_pid : ( GPid ) 0 );
}

/*
 * sets up callback to call whenever something interesting
 *   happens on a pipe
 */
static GIOChannel *
async_setup_out_channel( gint fd, GIOFunc func, sExecuteInfos *infos )
{
	GIOChannel *ioc;

	/* set up handler for data */
	ioc = g_io_channel_unix_new( fd );

	/* Tell the io channel to close the file descriptor
	 *  when the io channel gets destroyed */
	g_io_channel_set_close_on_unref( ioc, TRUE );

	/* standard size for message processing */
	g_io_channel_set_buffer_size( ioc, 4096 );

	/* Set IOChannel encoding to none to make it fit for binary data */
	g_io_channel_set_encoding( ioc, NULL, NULL );

	/* GLib-WARNING **: Need to have NULL encoding to set
	 * the buffering state of the channel. */
	g_io_channel_set_buffered( ioc, FALSE );

	/* define a callback when something is available on the channel */
	g_io_add_watch( ioc, G_IO_IN|G_IO_PRI|G_IO_ERR|G_IO_HUP|G_IO_NVAL, func, infos );

	/* g_io_add_watch() adds its own reference, which will be dropped
	 *  when the watch source is removed from the main loop (which
	 *  happens when we return FALSE from the callback) */
	g_io_channel_unref( ioc );

	return( ioc );
}

/*
 * called when there's data to read from the stdout pipe
 *   or when the pipe is closed (ie. the program terminated)
 *
 * return FALSE if the event source should be removed
 */
static gboolean
async_stdout_fn( GIOChannel *ioc, GIOCondition cond, sExecuteInfos *infos )
{
	return( infos ?
				async_output_fn( ioc, cond, infos, infos->runtype == RUN_BACKUP ? OUTPUT_DATA : OUTPUT_MSG ) :
				G_SOURCE_REMOVE );
}

static gboolean
async_stderr_fn( GIOChannel *ioc, GIOCondition cond, sExecuteInfos *infos )
{
	return( infos ?
				async_output_fn( ioc, cond, infos, OUTPUT_MSG ) :
				G_SOURCE_REMOVE );
}

static gboolean
async_output_fn( GIOChannel *ioc, GIOCondition cond, sExecuteInfos *infos, guint datatype )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_async_output_fn";
	GIOStatus ret;
	gsize iosize, ioread;
	gchar *iobuf, *str;
	GError *error;
	gchar **lines, **it;

	if( cond & ( G_IO_ERR | G_IO_HUP | G_IO_NVAL )){
		return( G_SOURCE_REMOVE );
	}

	/* data for us to read? */
	if( cond & (G_IO_IN | G_IO_PRI )){
		error = NULL;
		iosize = g_io_channel_get_buffer_size( ioc );
		iobuf = g_new0( gchar, 1+iosize );
		ret = g_io_channel_read_chars( ioc, iobuf, iosize, &ioread, &error );
		//g_debug( "%s: iosize=%lu, ioread=%lu", thisfn, iosize, ioread );

		if( ioread <= 0 || ret != G_IO_STATUS_NORMAL ){
			g_warning( "%s: aborting with read=%ld, ret=%d: %s", thisfn, ioread, ( gint ) ret, error->message );
			g_error_free( error );
			return( G_SOURCE_REMOVE );
		}

		if( datatype == OUTPUT_DATA ){
			infos->data_cb( iobuf, ioread, infos->user_data );

		/* on restore, only display the comments */
		} else if( infos->runtype == RUN_RESTORE ){
			//g_debug( "async_output_fn: iobuf_len=%ld", my_strlen( iobuf ));
			lines = g_strsplit( iobuf, "\n", -1 );
			for( it=lines ; *it ; it++ ){
				//g_debug( "%s", *it );
				if( g_strstr_len( *it, -1, " for table " )){
					//g_debug( "async_output_fn: for_table_len=%ld", my_strlen( *it ));
					str = g_strdup_printf( "%s\n", *it );
					infos->msg_cb( str, infos->user_data );
					g_free( str );
				}
			}
			g_strfreev( lines );

		/* on backup display all */
		} else {
			infos->msg_cb( iobuf, infos->user_data );
		}

		g_free( iobuf );
	}

	return( G_SOURCE_CONTINUE );
}

/*
 * callback called when the async child terminates
 * see g_child_watch_add() function
 */
static void
exit_cb( GPid child_pid, gint status, sExecuteInfos *infos )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_exit_cb";

	g_debug("%s: child_pid=%lu, exit status=%d", thisfn, ( gulong ) child_pid, status );

	/* Not sure how the exit status works on win32. Unix code follows */
#ifdef G_OS_UNIX
	if( WIFEXITED( status )){			/* did child terminate in a normal way? */

		if( WEXITSTATUS( status ) == EXIT_SUCCESS ){
			infos->command_ok = TRUE;
			g_debug( "%s: setting command_ok to TRUE", thisfn );

		} else {
			g_debug( "%s: exit with error code=%d", thisfn, WEXITSTATUS( status ));
		}
	} else if( WIFSIGNALED( status )){	/* was it terminated by a signal ? */
		g_debug( "%s: exit with signal %d", thisfn, WTERMSIG( status ));
	} else {
		g_debug( "%s: exit with undetermined error(s)", thisfn );
	}
#else
	g_warning( "%s: unmanaged operating system" );
#endif /* G_OS_UNIX */

	g_spawn_close_pid( child_pid );		/* does nothing on unix, needed on win32 */

	g_main_loop_quit( infos->loop );
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
