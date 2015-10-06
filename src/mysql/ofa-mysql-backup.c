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
#include "api/ofa-settings.h"

#include "ofa-mysql.h"
#include "ofa-mysql-backup.h"
#include "ofa-mysql-idbms.h"

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
static gchar      *build_cmdline( const mysqlInfos *infos, const gchar *cmdline, const gchar *fname, const gchar *new_dbname );
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

/**
 * ofa_mysql_get_def_backup_cmd:
 */
const gchar *
ofa_mysql_get_def_backup_cmd( const ofaIDbms *instance )
{
	return( "mysqldump --verbose %O -u%U -p%P %B | gzip -c > %F" );
}

/**
 * ofa_mysql_backup:
 *
 * Backup the currently connected database.
 *
 * The outputed SQL file doesn't contain any CREATE DATABASE nor USE,
 * so that we will be able to reload the data to any database name.
 */
gboolean
ofa_mysql_backup( const ofaIDbms *instance, void *handle, const gchar *fname, gboolean verbose )
{
	mysqlInfos *infos;
	gchar *cmdline;
	gboolean ok;

	infos = ( mysqlInfos * ) handle;

	cmdline = ofa_settings_get_string_ex( SETTINGS_TARGET_USER, PREFS_GROUP, PREFS_BACKUP_CMDLINE );
	if( !my_strlen( cmdline )){
		cmdline = g_strdup( ofa_mysql_get_def_backup_cmd( instance ));
	}

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
 * ofa_mysql_get_def_restore_cmd:
 */
const gchar *
ofa_mysql_get_def_restore_cmd( const ofaIDbms *instance )
{
	return( "mysql %O -u%U -p%P -e 'drop database if exists %B'; mysql %O -u%U -p%P -e 'create database %B'; gzip -cd %F | mysql --verbose %O -u%U -p%P %B" );
}

/**
 * ofa_mysql_restore:
 *
 * Restore a backup file on a named dossier.
 */
gboolean
ofa_mysql_restore( const ofaIDbms *instance,
						const gchar *dname, const gchar *fname,
						const gchar *root_account, const gchar *root_password )
{
	mysqlInfos *infos;
	gboolean ok;
	gchar *cmdline;

	infos = ofa_mysql_get_connect_infos( dname );
	infos->account = g_strdup( root_account );
	infos->password = g_strdup( root_password );

	create_fake_database( instance, infos );

	cmdline = ofa_settings_get_string_ex( SETTINGS_TARGET_USER, PREFS_GROUP, PREFS_RESTORE_CMDLINE );
	if( !my_strlen( cmdline )){
		cmdline = g_strdup( ofa_mysql_get_def_restore_cmd( instance ));
	}

	ok = do_backup_restore(
				( const mysqlInfos * ) infos,
				cmdline,
				fname,
				_( "Openbook restore" ),
				( GChildWatchFunc ) exit_restore_cb,
				TRUE );

	g_free( cmdline );

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

	cmdline = build_cmdline( infos, "/bin/sh \"mysql -u%U -p%P -e 'create database %B'\"", NULL, NULL );
	g_debug( "%s: cmdline=%s", thisfn, cmdline );
	stdout = NULL;
	stderr = NULL;

	g_spawn_command_line_sync( cmdline, &stdout, &stderr, NULL, NULL );

	g_free( stdout );
	g_free( stderr );
	g_free( cmdline );
}

/**
 * ofa_mysql_archive:
 *
 * In the settings, the current exercice points to the database which
 * contains the newly closed exercice. Its status has been set as
 * 'Closed'.
 * Duplicate the corresponding database to a new one, updating the
 * settings accordingly.
 */
gboolean
ofa_mysql_archive( const ofaIDbms *instance,
						const gchar *dname,
						const gchar *root_account, const gchar *root_password,
						const gchar *user_account,
						const GDate *begin_next, const GDate *end_next )
{
	static const gchar *thisfn = "ofa_mysql_backup_archive";
	mysqlInfos *infos;
	gboolean ok;
	gchar *prev_dbname, *new_dbname, *stdout, *stderr;
	gchar *cmdline, *cmd;
	gint status;

	infos = ofa_mysql_get_connect_newdb_infos( dname, root_account, root_password, &prev_dbname );
	if( !my_strlen( infos->dbname )){
		return( FALSE );
	}

	new_dbname = infos->dbname;
	infos->dbname = prev_dbname;

	cmdline = build_cmdline( infos,
					"mysql %O -u%U -p%P -e 'drop database if exists %N'; "
					"mysql %O -u%U -p%P -e 'create database %N'; "
					"mysqldump %O -u%U -p%P %B | mysql %O -u%U -p%P %N",
					NULL, new_dbname );

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

	prev_dbname = infos->dbname;
	infos->dbname = new_dbname;

	if( ok ){
		ofa_dossier_misc_set_new_exercice( dname, infos->dbname, begin_next, end_next );
		ofa_mysql_duplicate_grants( instance, infos, user_account, prev_dbname );
	}

	g_free( prev_dbname );
	ofa_mysql_free_connect_infos( infos );

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

	cmdline = build_cmdline( sql_infos, def_cmdline, fname, NULL );
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
build_cmdline( const mysqlInfos *infos, const gchar *def_cmdline, const gchar *fname, const gchar *new_dbname )
{
	static const gchar *thisfn = "ofa_mysql_backup_build_cmdline";
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
	static const gchar *thisfn = "ofa_mysql_backup_exec_command";
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
	static const gchar *thisfn = "ofa_mysql_backup_display_output";
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
	static const gchar *thisfn = "ofa_mysql_backup_exit_backup_cb";
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
	static const gchar *thisfn = "ofa_mysql_backup_exit_restore_cb";
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
