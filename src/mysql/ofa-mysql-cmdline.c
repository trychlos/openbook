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

#include "api/my-utils.h"
#include "api/ofa-dossier-misc.h"

#include "ofa-mysql.h"
#include "ofa-mysql-cmdline.h"
#include "ofa-mysql-connect.h"
#include "ofa-mysql-idbms.h"
#include "ofa-mysql-ipreferences.h"
#include "ofa-mysql-meta.h"
#include "ofa-mysql-period.h"

#define BUFSIZE 4096

typedef struct {
	GtkWidget *window;
	GtkWidget *textview;
	GtkWidget *close_btn;
	gboolean   backup_ok;
	gulong     out_line;
	gulong     err_line;
	gboolean   verbose;
	GMainLoop *loop;
}
	backupInfos;

static const gchar *st_window_name = "MySQLBackupWindow";

static void        create_fake_database( const ofaIDbms *instance, mysqlInfos *infos );
static gboolean    do_backup_restore( const mysqlInfos *infos, const gchar *cmdline, const gchar *fname, const gchar *window_title, GChildWatchFunc pfn, gboolean verbose );
static gchar      *build_cmdline_infos( const mysqlInfos *infos, const gchar *cmdline, const gchar *fname, const gchar *new_dbname );
static gchar      *build_cmdline_ex( const gchar *host, const gchar *socket, guint port, const gchar *account, const gchar *password, const gchar *dbname, const gchar *def_cmdline, const gchar *fname, const gchar *new_dbname );
static void        create_window( backupInfos *infos, const gchar *window_title );
static GPid        exec_command( const gchar *cmdline, backupInfos *infos );
static GIOChannel *set_up_io_channel( gint fd, GIOFunc func, backupInfos *infos );
static gboolean    stdout_fn( GIOChannel *ioc, GIOCondition cond, backupInfos *infos );
static gboolean    stdout_done( GIOChannel *ioc );
static gboolean    stderr_fn( GIOChannel *ioc, GIOCondition cond, backupInfos *infos );
static gboolean    stderr_done( GIOChannel *ioc );
static void        display_output( const gchar *str, backupInfos *infos );
static void        exit_backup_cb( GPid child_pid, gint status, backupInfos *infos );
static void        exit_restore_cb( GPid child_pid, gint status, backupInfos *infos );
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
 * ofa_mysql_cmdline_backup_run:
 *
 * Backup the currently connected database.
 *
 * The outputed SQL file doesn't contain any CREATE DATABASE nor USE,
 * so that we will be able to reload the data to any database name.
 */
gboolean
ofa_mysql_cmdline_backup_run( const ofaIDbms *instance, void *handle, const gchar *fname, gboolean verbose )
{
	mysqlInfos *infos;
	gchar *cmdline;
	gboolean ok;

	infos = ( mysqlInfos * ) handle;

	cmdline = ofa_mysql_ipreferences_get_backup_command();

	ofa_mysql_query( instance, infos, "FLUSH TABLES WITH READ LOCK" );

	ok = do_backup_restore(
				infos,
				cmdline,
				fname,
				_( "Openbook backup" ),
				( GChildWatchFunc ) exit_backup_cb,
				verbose );

	g_free( cmdline );

	ofa_mysql_query( instance, infos, "UNLOCK TABLES" );

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
			"mysql %O -u%U -p%P -e 'create database %B'; "
			"gzip -cd %F | mysql --verbose %O -u%U -p%P %B" );
}

/**
 * ofa_mysql_cmdline_restore_run:
 *
 * Restore a backup file on a named dossier.
 */
gboolean
ofa_mysql_cmdline_restore_run( const ofaIDbms *instance,
						const gchar *dname, const gchar *furi,
						const gchar *root_account, const gchar *root_password )
{
	mysqlInfos *infos;
	gboolean ok;
	gchar *fname, *cmdline;

	infos = ofa_mysql_get_connect_infos( dname );
	infos->account = g_strdup( root_account );
	infos->password = g_strdup( root_password );

	create_fake_database( instance, infos );
	fname = g_filename_from_uri( furi, NULL, NULL );

	cmdline = ofa_mysql_ipreferences_get_restore_command();

	ok = do_backup_restore(
				( const mysqlInfos * ) infos,
				cmdline,
				fname,
				_( "Openbook restore" ),
				( GChildWatchFunc ) exit_restore_cb,
				TRUE );

	g_free( cmdline );
	g_free( fname );

	ofa_mysql_free_connect_infos( infos );

	return( ok );
}

/*
 * It happens that MySQL has some issues with dropping an non-existant
 * database - so create it first
 */
static void
create_fake_database( const ofaIDbms *instance, mysqlInfos *infos )
{
	static const gchar *thisfn = "mfa_mysql_backup_create_fake_database";
	gchar *cmdline, *stdout, *stderr;

	cmdline = build_cmdline_infos( infos, "/bin/sh \"mysql -u%U -p%P -e 'create database %B'\"", NULL, NULL );
	g_debug( "%s: cmdline=%s", thisfn, cmdline );
	stdout = NULL;
	stderr = NULL;

	g_spawn_command_line_sync( cmdline, &stdout, &stderr, NULL, NULL );

	g_free( stdout );
	g_free( stderr );
	g_free( cmdline );
}

/**
 * ofa_mysql_cmdline_archive_and_new:
 * @connect: an active #ofaIDBConnect connection on the closed exercice.
 *  The dossier settings has been updated accordingly.
 * @root_account: administrator root account.
 * @root_password: administrator root password.
 * @begin_next: the beginning date of the next exercice.
 * @end_next: the ending date of the next exercice.
 *
 * Duplicate the corresponding database to a new one, creating the
 * corresponding line accordingly in the dossier settings.
 */
gboolean
ofa_mysql_cmdline_archive_and_new( const ofaIDBConnect *connect,
						const gchar *root_account, const gchar *root_password,
						const GDate *begin_next, const GDate *end_next )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_backup_run_archive_and_new";
	ofaMySQLConnect *new_cnx;
	ofaIDBMeta *meta;
	const gchar *host, *socket, *prev_dbname;
	guint port;
	ofaIDBPeriod *period;
	gchar *new_db, *prev_account;
	gboolean ok;
	gchar *cmdline, *cmd, *stdout, *stderr;
	gint status;

	/* meta informations on the current dossier */
	meta = ofa_idbconnect_get_meta( connect );
	g_return_val_if_fail( meta && OFA_IS_MYSQL_META( meta ), FALSE );

	host = ofa_mysql_meta_get_host( OFA_MYSQL_META( meta ));
	socket = ofa_mysql_meta_get_socket( OFA_MYSQL_META( meta ));
	port = ofa_mysql_meta_get_port( OFA_MYSQL_META( meta ));

	new_cnx = ofa_mysql_connect_new_for_server(
					host, socket, port, root_account, root_password, NULL );

	if( !new_cnx ){
		g_warning( "%s: unable to get a root connection on the DB server", thisfn );
		g_object_unref( meta );
		return( FALSE );
	}

	/* get previous database from current connection on closed exercice */
	period = ofa_idbconnect_get_period( connect );
	g_return_val_if_fail( period && OFA_IS_MYSQL_PERIOD( period ), FALSE );

	prev_dbname = ofa_mysql_period_get_database( OFA_MYSQL_PERIOD( period ));

	new_db = ofa_mysql_connect_get_new_database( new_cnx, prev_dbname );

	g_object_unref( period );

	if( !my_strlen( new_db )){
		g_warning( "%s: unable to get a new database name", thisfn );
		g_object_unref( meta );
		g_object_unref( new_cnx );
		return( FALSE );
	}

	cmdline = build_cmdline_ex(
					host, socket, port, root_account, root_password, prev_dbname,
					"mysql %O -u%U -p%P -e 'drop database if exists %N'; "
					"mysql %O -u%U -p%P -e 'create database %N'; "
					"mysqldump %O -u%U -p%P %B | mysql %O -u%U -p%P %N",
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
		ofa_mysql_meta_add_period( OFA_MYSQL_META( meta ), TRUE, begin_next, end_next, new_db );
		//ofa_dossier_misc_set_new_exercice( dname, infos->dbname, begin_next, end_next );
		prev_account = ofa_idbconnect_get_account( connect );
		do_duplicate_grants( OFA_IDBCONNECT( new_cnx ), host, prev_account, prev_dbname, new_db );
		g_free( prev_account );
	}

	g_free( new_db );
	g_object_unref( new_cnx );

	return( ok );
}

static gboolean
do_backup_restore( const mysqlInfos *sql_infos,
						const gchar *def_cmdline, const gchar *fname, const gchar *window_title,
						GChildWatchFunc pfn, gboolean verbose )
{
	static const gchar *thisfn = "ofa_mysql_do_backup_restore";
	backupInfos *infos;
	gchar *cmdline;
	GPid child_pid;
	gboolean ok;
	guint source_id;

	cmdline = build_cmdline_infos( sql_infos, def_cmdline, fname, NULL );
	g_debug( "%s: cmdline=%s", thisfn, cmdline );

	infos = g_new0( backupInfos, 1 );
	infos->backup_ok = FALSE;
	infos->out_line = 0;
	infos->err_line = 0;
	infos->verbose = verbose;

	if( infos->verbose ){
		create_window( infos, window_title );
		g_debug( "%s: window=%p, textview=%p",
				thisfn, ( void * ) infos->window, ( void * ) infos->textview );
	} else {
		infos->loop = g_main_loop_new( NULL, FALSE );
	}

	child_pid = exec_command( cmdline, infos );
	g_debug("%s: child_pid=%lu", thisfn, ( gulong ) child_pid );

	if( child_pid != ( GPid ) 0 ){
		/* Watch the child, so we get the exit status */
		source_id = g_child_watch_add( child_pid, pfn, infos );
		g_debug( "%s: returning from g_child_watch_add, source_id=%u", thisfn, source_id );

		if( infos->verbose ){
			g_debug( "%s: running the display dialog", thisfn );
			gtk_dialog_run( GTK_DIALOG( infos->window ));
			my_utils_window_save_position( GTK_WINDOW( infos->window ), st_window_name );

		} else {
			g_main_loop_run( infos->loop );
			g_main_loop_unref( infos->loop );
		}
	}

	if( infos->verbose ){
		g_debug( "%s: destroying the display dialog", thisfn );
		gtk_widget_destroy( infos->window );
	}

	ok = infos->backup_ok;
	g_free( infos );

	g_debug( "%s: returning %s", thisfn, ok ? "True":"False" );
	return( ok );
}

/*
 * %B: current database name
 * %F: filename
 * %N: new database name
 * %O: connection options (host, port, socket)
 * %P: password
 * %U: account
 */
static gchar *
build_cmdline_infos( const mysqlInfos *infos, const gchar *def_cmdline, const gchar *fname, const gchar *new_dbname )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_backup_run_build_cmdline_infos";
	gchar *sysfname, *cmdline;
	GString *options;
	GRegex *regex;
	gchar *newcmd;
	gchar *quoted;

	cmdline = g_strdup( def_cmdline );
	g_debug( "%s: def_cmdline=%s", thisfn, cmdline );

	regex = g_regex_new( "%B", 0, 0, NULL );
	newcmd = g_regex_replace_literal( regex, cmdline, -1, 0, infos->dbname, 0, NULL );
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
	if( my_strlen( infos->host )){
		g_string_append_printf( options, "--host=%s ", infos->host );
	}
	if( infos->port > 0 ){
		g_string_append_printf( options, "--port=%u ", infos->port );
	}
	if( my_strlen( infos->socket )){
		g_string_append_printf( options, "--socket=%s ", infos->socket );
	}

	regex = g_regex_new( "%O", 0, 0, NULL );
	newcmd = g_regex_replace_literal( regex, cmdline, -1, 0, options->str, 0, NULL );
	g_regex_unref( regex );
	g_free( cmdline );
	cmdline = newcmd;
	g_string_free( options, TRUE );

	regex = g_regex_new( "%P", 0, 0, NULL );
	newcmd = g_regex_replace_literal( regex, cmdline, -1, 0, infos->password, 0, NULL );
	g_regex_unref( regex );
	g_free( cmdline );
	cmdline = newcmd;

	regex = g_regex_new( "%U", 0, 0, NULL );
	newcmd = g_regex_replace_literal( regex, cmdline, -1, 0, infos->account, 0, NULL );
	g_regex_unref( regex );
	g_free( cmdline );
	cmdline = newcmd;

	return( cmdline );
}

/*
 * %B: current database name
 * %F: filename
 * %N: new database name
 * %O: connection options (host, port, socket)
 * %P: password
 * %U: account
 */
static gchar *
build_cmdline_ex( const gchar *host, const gchar *socket, guint port,
					const gchar *account, const gchar *password,
					const gchar *dbname,
					const gchar *def_cmdline, const gchar *fname, const gchar *new_dbname )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_backup_run_build_cmdline_ex";
	gchar *sysfname, *cmdline;
	GString *options;
	GRegex *regex;
	gchar *newcmd;
	gchar *quoted;

	cmdline = g_strdup( def_cmdline );
	g_debug( "%s: def_cmdline=%s", thisfn, cmdline );

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

/*
 * the dialog is only created when running verbosely
 */
static void
create_window( backupInfos *infos, const gchar *window_title )
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
	gtk_grid_attach( GTK_GRID( grid ), scrolled, 0, 0, 1, 1 );

	infos->textview = gtk_text_view_new();
	gtk_widget_set_hexpand( infos->textview, TRUE );
	gtk_widget_set_vexpand( infos->textview, TRUE );
	gtk_text_view_set_editable( GTK_TEXT_VIEW( infos->textview ), FALSE );
	gtk_container_add( GTK_CONTAINER( scrolled ), infos->textview );

	infos->close_btn = gtk_dialog_get_widget_for_response( GTK_DIALOG( infos->window ), GTK_RESPONSE_ACCEPT );
	gtk_widget_set_sensitive( infos->close_btn, FALSE );

	my_utils_window_restore_position( GTK_WINDOW( infos->window ), st_window_name );

	gtk_widget_show_all( infos->window );
}

static GPid
exec_command( const gchar *cmdline, backupInfos *infos )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_backup_run_exec_command";
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
		set_up_io_channel( stdout_fd, ( GIOFunc ) stdout_fn, infos );
		set_up_io_channel( stderr_fd, ( GIOFunc ) stderr_fn, infos );
	}

	return( ok ? child_pid : ( GPid ) 0 );
}

/*
 * sets up callback to call whenever something interesting
 *   happens on a pipe
 */
static GIOChannel *
set_up_io_channel( gint fd, GIOFunc func, backupInfos *infos )
{
	GIOChannel *ioc;

	/* set up handler for data */
	ioc = g_io_channel_unix_new( fd );

	/* Set IOChannel encoding to none to make
	 *  it fit for binary data */
	/*g_io_channel_set_encoding( ioc, NULL, NULL );*/
	/* GLib-WARNING **: Need to have NULL encoding to set
	 * the buffering state of the channel. */
	/*g_io_channel_set_buffered( ioc, FALSE );*/

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
 * called when there's data to read from the stderr pipe
 *   or when the pipe is closed (ie. the program terminated)
 */
static gboolean
stdout_fn( GIOChannel *ioc, GIOCondition cond, backupInfos *infos )
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
				return( stdout_done( ioc ));	/* = return FALSE */
			}
			str = g_strdup_printf( "[stdout %lu] %s\n", ++infos->out_line, buf );
			display_output( str, infos );
			g_free( str );
		}
	}

	if( cond & ( G_IO_ERR | G_IO_HUP | G_IO_NVAL )){
		return( stdout_done( ioc ));		/* = return FALSE */
	}

	return( TRUE );
}

/*
 * Called when the pipe is closed (ie. command terminated)
 *  Always returns FALSE (to make it nicer to use in the callback)
 */
static gboolean
stdout_done( GIOChannel *ioc )
{
	return( FALSE );
}

static gboolean
stderr_fn( GIOChannel *ioc, GIOCondition cond, backupInfos *infos )
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
				return( stderr_done( ioc )); /* = return FALSE */
			}
			str = g_strdup_printf( "[stderr %lu] %s\n", ++infos->err_line, buf );
			display_output( str, infos );
			g_free( str );
		}
	}

	if( cond & ( G_IO_ERR | G_IO_HUP | G_IO_NVAL )){
		return( stderr_done( ioc )); /* = return FALSE */
	}

	return( TRUE );
}

static gboolean
stderr_done( GIOChannel *ioc )
{
	return( FALSE );
}

/*
 * this is only called when running verbosely
 */
static void
display_output( const gchar *str, backupInfos *infos )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_backup_run_display_output";
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
exit_backup_cb( GPid child_pid, gint status, backupInfos *infos )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_backup_run_exit_backup_cb";
	GtkWidget *dlg;
	gchar *msg;

	g_debug("%s: child_pid=%lu, exit status=%d", thisfn, ( gulong ) child_pid, status );
	msg = NULL;

	/* Not sure how the exit status works on win32. Unix code follows */
#ifdef G_OS_UNIX
	if( WIFEXITED( status )){			/* did child terminate in a normal way? */

		if( WEXITSTATUS( status ) == EXIT_SUCCESS ){
			msg = g_strdup( _( "Backup has successfully run" ));
			infos->backup_ok = TRUE;
			g_debug( "%s: setting backup_ok to TRUE", thisfn );

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

	if( infos->verbose ){
		if( !my_strlen( msg )){
			if( infos->backup_ok ){
				msg = g_strdup( _( "Dossier successfully backuped" ));
			} else {
				msg = g_strdup( _( "An error occured while backuping the dossier" ));
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

static void
exit_restore_cb( GPid child_pid, gint status, backupInfos *infos )
{
	static const gchar *thisfn = "ofa_mysql_cmdline_backup_run_exit_restore_cb";
	GtkWidget *dlg;
	gchar *msg;

	g_debug("%s: child_pid=%lu, exit status=%d", thisfn, ( gulong ) child_pid, status );
	msg = NULL;

	/* Not sure how the exit status works on win32. Unix code follows */
#ifdef G_OS_UNIX
	if( WIFEXITED( status )){			/* did child terminate in a normal way? */

		if( WEXITSTATUS( status ) == EXIT_SUCCESS ){
			msg = g_strdup( _( "Restore has successfully run" ));
			infos->backup_ok = TRUE;
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
			if( infos->backup_ok ){
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
	static const gchar *thisfn = "ofa_mysql_cmdline_backup_run_do_duplicate_grants";
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
