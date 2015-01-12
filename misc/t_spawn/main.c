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
 *
 * To display debug messages, run the command:
 *   $ G_MESSAGES_DEBUG=OFA _install/bin/openbook
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*
 * As of version 0.25, a spawned command-line to clone the database
 * doesn't work
 */
#include <gtk/gtk.h>

int
main( int argc, char *argv[] )
{
	const gchar *cmd = "mysql  -uroot -proot -e 'drop database if exists oclose_10'; mysql  -uroot -proot -e 'create database oclose_10'; mysqldump  -uroot -proot oclose | mysql  -uroot -proot oclose_10";
	gchar *cmdline;
	gint status;

	gtk_init( &argc, &argv );

	cmdline = g_strdup_printf( "/bin/sh -c \"%s\"", cmd );
	g_debug( "cmdline=%s", cmdline );
	g_spawn_command_line_sync( cmdline, NULL, NULL, &status, NULL );
	g_debug( "status=%d", status );

	return 0;
}
