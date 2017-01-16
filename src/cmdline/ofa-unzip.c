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
 * To display debug messages, run the command:
 *   $ G_MESSAGES_DEBUG=OFA _install/bin/openbook
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <archive.h>
#include <archive_entry.h>
#include <glib.h>
#include <stdlib.h>

#include "my/my-utils.h"

#include "api/ofa-backup-header.h"

/*
 * ofa-unzip is a small cmdline utility (no GUI) to extract the DAT member
 * of a ZIP archive.
 *
 * It takes the file uri as command-line argument, and output the decompressed
 * data on stdout.
 */

#define BUFSIZE 32768

static gint
extract_archive( const gchar *uri )
{
	static const gchar *thisfn = "ofa_unzip_extract_archive";
	GFile *file;
	gchar *pathname, *buffer;
	struct archive *archive;
	struct archive_entry *entry;
	const gchar *cname;
	gboolean found;
	gsize bytes_read;

	found = FALSE;
	archive = archive_read_new();
	archive_read_support_filter_all( archive );
	archive_read_support_format_all( archive );

	file = g_file_new_for_uri( uri );
	pathname = g_file_get_path( file );
	if( archive_read_open_filename( archive, pathname, 16384 ) != ARCHIVE_OK ){
		g_warning( "%s: archive_read_open_filename: path=%s, %s", thisfn, pathname, archive_error_string( archive ));

	} else {
		buffer = g_new0( gchar, BUFSIZE );
		while( archive_read_next_header( archive, &entry ) == ARCHIVE_OK ){
			cname = archive_entry_pathname( entry );
			if( g_str_has_prefix( cname, OFA_BACKUP_HEADER_DATA )){
				found = TRUE;
				while(( bytes_read = archive_read_data( archive, buffer, BUFSIZE )) > 0 ){
					g_print( "%*.*s", ( gint ) bytes_read, ( gint ) bytes_read, buffer );
				}
				break;
			}
			archive_read_data_skip( archive );
		}
		g_free( buffer );
	}

	g_free( pathname );
	g_object_unref( file );
	archive_read_close( archive );
	archive_read_free( archive );

	return( found ? 0 : 1 );
}

int
main( int argc, char *argv[] )
{
	gchar *basename;
	gint status;

	if( argc < 1 ){
		g_warning( "Expected to get program name from first 'argv' argument, which is empty.\nAborting." );
		exit( 1 );
	}

	basename = g_path_get_basename( argv[0 ] );
	status = 0;

	if( argc != 2 ){
		g_print( "\n"
				" %s: extract data member of an archive file to stdout.\n"
				" Usage:\n"
				"   %s <archive_uri>\n"
				"\n",
				basename, basename );
	} else {
		status = extract_archive( argv[1] );
	}

	g_free( basename );

	exit( status );
}
