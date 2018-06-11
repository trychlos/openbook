/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <archive.h>
#include <archive_entry.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-utils.h"

#include "api/ofa-backup-header.h"

static gchar   *st_data = "";						/* name of the data stream to be dumped */
static gchar   *st_header = "";						/* name of the header stream to be dumped */
static gboolean st_list = TRUE;						/* whether to list stream (default) */
static gchar   *st_uri = "";						/* URI of the file */

static GOptionEntry st_entries[] = {

	{ "data",   'd', 0, G_OPTION_ARG_STRING,   &st_data,
			N_( "Dump the specified data stream" ), "<name>" },
	{ "header", 'h', 0, G_OPTION_ARG_STRING,   &st_header,
			N_( "Dump the specified header stream" ), "<name>" },
	{ "list",   'l', 0, G_OPTION_ARG_NONE,     &st_list,
			N_( "List the embedded streams" ), NULL },
	{ "uri",    'u', 0, G_OPTION_ARG_FILENAME, &st_uri,
			N_( "The URI of the file to be examined" ), "<uri>" },
	{ NULL }
};

/*
 * ofa-unzip is a small cmdline utility (no GUI) to extract the DAT member
 * of a ZIP archive.
 *
 * It takes the file uri as command-line argument, and output the decompressed
 * data on stdout.
 */

#define BUFSIZE 32768

static gint
list_archive( const gchar *basename, const gchar *uri )
{
	static const gchar *thisfn = "ofa_unzip_list_archive";
	GFile *file;
	gchar *pathname, *buffer;
	struct archive *archive;
	struct archive_entry *entry;
	const gchar *cname;
	gboolean found;

	found = FALSE;
	archive = archive_read_new();
	archive_read_support_filter_all( archive );
	archive_read_support_format_all( archive );

	file = g_file_new_for_uri( uri );
	pathname = g_file_get_path( file );
	if( archive_read_open_filename( archive, pathname, 16384 ) != ARCHIVE_OK ){
		g_warning( "%s archive_read_open_filename: path=%s, %s", thisfn, pathname, archive_error_string( archive ));

	} else {
		buffer = g_new0( gchar, BUFSIZE );
		while( archive_read_next_header( archive, &entry ) == ARCHIVE_OK ){
			found = TRUE;
			cname = archive_entry_pathname( entry );
			g_print( _( "[%s] found stream '%s'\n" ), basename, cname );
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

static gint
dump_header( const gchar *basename, const gchar *uri, const gchar *name )
{
	static const gchar *thisfn = "ofa_unzip_dump_header";
	GFile *file;
	gchar *pathname, *buffer, *searched;
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
		searched = g_strdup_printf( "%s%s", OFA_BACKUP_HEADER_HEADER, name );
		while( archive_read_next_header( archive, &entry ) == ARCHIVE_OK ){
			cname = archive_entry_pathname( entry );
			if( !g_strcmp0( cname, searched )){
				found = TRUE;
				g_print( _( "[%s] dumping stream '%s':\n" ), basename, cname );
				while(( bytes_read = archive_read_data( archive, buffer, BUFSIZE )) > 0 ){
					g_print( "%*.*s", ( gint ) bytes_read, ( gint ) bytes_read, buffer );
				}
				g_print( "\n" );
				break;
			}
			archive_read_data_skip( archive );
		}
		g_free( searched );
		g_free( buffer );
	}

	g_free( pathname );
	g_object_unref( file );
	archive_read_close( archive );
	archive_read_free( archive );

	return( found ? 0 : 1 );
}

static gint
dump_data( const gchar *basename, const gchar *uri, const gchar *name )
{
	static const gchar *thisfn = "ofa_unzip_dump_data";
	GFile *file;
	gchar *pathname, *buffer, *searched;
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
		searched = g_strdup_printf( "%s%s", OFA_BACKUP_HEADER_HEADER, name );
		while( archive_read_next_header( archive, &entry ) == ARCHIVE_OK ){
			cname = archive_entry_pathname( entry );
			if( !g_strcmp0( cname, searched )){
				found = TRUE;
				g_print( _( "[%s] dumping stream '%s':\n" ), basename, cname );
				while(( bytes_read = archive_read_data( archive, buffer, BUFSIZE )) > 0 ){
					g_print( "%*.*s", ( gint ) bytes_read, ( gint ) bytes_read, buffer );
				}
				g_print( "\n" );
				break;
			}
			archive_read_data_skip( archive );
		}
		g_free( searched );
		g_free( buffer );
	}

	g_free( pathname );
	g_object_unref( file );
	archive_read_close( archive );
	archive_read_free( archive );

	return( found ? 0 : 1 );
}

#if 0
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
#endif

int
main( int argc, char *argv[] )
{
	gchar *basename, *msg;
	gint status;
	GOptionContext *context;
	GError *error;

	if( argc < 1 ){
		g_warning( _( "Expected to get program name from first 'argv' argument, which is empty.\nAborting." ));
		exit( 1 );
	}

	basename = g_path_get_basename( argv[0 ] );
	status = 0;
	error = NULL;

	context = g_option_context_new( _( "- dump a zip file" ));
	g_option_context_add_main_entries( context, st_entries, GETTEXT_PACKAGE );
	g_option_context_add_group( context, gtk_get_option_group( TRUE ));

	if( argc < 2 ){
		msg = g_option_context_get_help( context, TRUE, NULL );
		g_print( _( "%s: %s\n" ), basename, msg );
		g_free( msg );
		exit( 0 );
	}

	if( !g_option_context_parse( context, &argc, &argv, &error )){
		g_print( _( "%s: option parsing failed: %s\n" ), basename, error->message );
		exit( 1 );
	}

	if( !my_strlen( st_uri )){
		g_warning( _( "%s: the URI of the file to be dumped is mandatory\n" ), basename );
		exit( 1 );
	}

	g_debug( "URI='%s'", st_uri );

	if( my_strlen( st_header )){
		status += dump_header( basename, st_uri, st_header );

	} else if( my_strlen( st_data )){
		status += dump_data( basename, st_uri, st_data );

	} else if( st_list ){
		status += list_archive( basename, st_uri );
	}

	g_free( basename );

	exit( status );
}
