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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>
#include <glib/gstdio.h>

#include "ui/my-utils.h"

static gchar **string_to_array( const gchar *text, const gchar *separator );
static void    list_perms( const gchar *path, const gchar *message, const gchar *command );
static void    print_free_line( const gchar *message, const gchar *prefix, gchar *line );

/**
 * my_utils_boolean_from_string
 * @string: a string to be converted.
 *
 * Returns: %TRUE if the string evaluates to "true" (case insensitive),
 * %FALSE else.
 *
 * https://developer.gnome.org/glib/stable/glib-Key-value-file-parser.html:
 * - Key files are always encoded in UTF-8.
 * - In key files, only true and false (in lower case) are allowed."
 *
 * We accept here also 'Yes' or a non-zero numeric value as a non-
 * documented facility.
 */
gboolean
my_utils_boolean_from_string( const gchar *string )
{
	if( !string ) return( FALSE );

	return( g_utf8_collate( string, "true" ) == 0 ||
			g_ascii_strcasecmp( string, "yes" ) == 0 ||
			atoi( string ) != 0 );
}

/**
 * my_utils_slist_dump:
 * @prefix: a string to be used as a prefix for each outputed line.
 * @list: a list of strings.
 *
 * Dumps the content of a list of strings.
 */
void
my_utils_slist_dump( const gchar *prefix, GSList *list )
{
	static const gchar *thisfn = "my_utils_slist_dump";
	GSList *i;
	int c;
	const gchar *thispfx;

	thispfx = ( prefix && strlen( prefix )) ? prefix : thisfn;

	g_debug( "%s: list at %p has %d element(s)", thispfx, ( void * ) list, g_slist_length( list ));

	for( i=list, c=0 ; i ; i=i->next ){
		g_debug( "%s: [%2d] %s (%lu)",
				thispfx, c++, ( gchar * ) i->data, g_utf8_strlen( ( gchar * ) i->data, -1 ));
	}
}

/**
 * my_utils_slist_free:
 * @slist: a #GSList list of strings.
 *
 * Releases the strings and the list itself.
 */
void
my_utils_slist_free( GSList *slist )
{
	g_slist_foreach( slist, ( GFunc ) g_free, NULL );
	g_slist_free( slist );
}

/**
 * my_utils_slist_duplicate:
 * @slist: the #GSList to be duplicated.
 *
 * Returns: a #GSList of strings.
 *
 * The returned list should be my_utils_slist_free() by the caller.
 */
GSList *
my_utils_slist_duplicate( GSList *slist )
{
	GSList *dest_slist, *it;

	dest_slist = NULL;

	for( it = slist ; it != NULL ; it = it->next ){
		dest_slist = g_slist_prepend( dest_slist, g_strdup(( gchar * ) it->data ) );
	}

	dest_slist = g_slist_reverse( dest_slist );

	return( dest_slist );
}

/**
 * my_utils_slist_from_split:
 * @text: a string to be splitted.
 * @separator: the string to be used as the separator.
 *
 * Returns: a #GSList with the list of strings after having been splitted.
 *
 * The returned #GSList should be my_utils_slist_free() by the caller.
 */
GSList *
my_utils_slist_from_split( const gchar *text, const gchar *separator )
{
	GSList *slist;
	gchar **tokens;

	tokens = string_to_array( text, separator );
	if( !tokens ){
		return( NULL );
	}

	slist = my_utils_slist_from_array(( const gchar ** ) tokens );
	g_strfreev( tokens );

	return( slist );
}

/*
 * string_to_array
 */
static gchar **
string_to_array( const gchar *text, const gchar *separator )
{
	gchar **tokens;
	gchar *source, *tmp;

	if( !text ){
		return( NULL );
	}

	source = g_strdup( text );
	tmp = g_strstrip( source );

	if( !g_utf8_strlen( tmp, -1 )){
		return( NULL );
	}

	tokens = g_strsplit( tmp, separator, -1 );

	g_free( source );

	return( tokens );
}

/**
 * my_utils_slist_from_array:
 * @str_array: an NULL-terminated array of strings.
 *
 * Returns: a #GSList list of strings, which should be #my_utils_slist_free()
 * by the caller.
 */
GSList *
my_utils_slist_from_array( const gchar **str_array )
{
	GSList *slist;
	gchar **idx;

	slist = NULL;
	idx = ( gchar ** ) str_array;

	while( *idx ){
		slist = g_slist_prepend( slist, g_strstrip( g_strdup( *idx )));
		idx++;
	}

	return( g_slist_reverse( slist ));
}

/**
 * my_utils_intlist_duplicate:
 * @list: the #GList to be duplicated.
 *
 * Returns: a #GList of int.
 *
 * The returned list should be g_list_free() by the caller.
 */
GList *
my_utils_intlist_duplicate( GList *ilist )
{
	GList *dest_list, *it;

	dest_list = NULL;

	for( it = ilist ; it != NULL ; it = it->next ){
		dest_list = g_list_prepend( dest_list, it->data );
	}

	dest_list = g_list_reverse( dest_list );

	return( dest_list );
}

/**
 * my_utils_intlist_from_split
 */
GList *
my_utils_intlist_from_split( const gchar *string, const gchar *separator )
{
	GList *list;
	gchar **array;
	gchar **i;

	array = string_to_array( string, ";" );
	if( !array ){
		return( NULL );
	}

	list = NULL;
	i = ( gchar ** ) array;
	while( *i ){
		list = g_list_prepend( list, GINT_TO_POINTER( atoi( *i )));
		i++;
	}
	list = g_list_reverse( list );

	g_strfreev( array );

	return( list );
}

/**
 * my_utils_intlist_to_string:
 */
gchar *
my_utils_intlist_to_string( GList *list, gchar *separator )
{
	GList *is;
	GString *str = g_string_new( "" );
	gboolean first;

	first = TRUE;
	for( is = list ; is ; is = is->next ){
		if( !first ){
			str = g_string_append( str, separator );
		}
		g_string_append_printf( str, "%u", GPOINTER_TO_UINT( is->data ));
		first = FALSE;
	}

	return( g_string_free( str, FALSE ));
}

/**
 * na_core_utils_file_list_perms:
 * @path: the path of the file to be tested.
 * @message: a message to be printed if not %NULL.
 *
 * Displays the permissions of the file on debug output.
 */
void
my_utils_file_list_perms( const gchar *path, const gchar *message )
{
	list_perms( path, message, "ls -l" );
}

static void
list_perms( const gchar *path, const gchar *message, const gchar *command )
{
	static const gchar *thisfn = "na_core_utils_list_perms";
	gchar *cmd;
	gchar *out, *err;
	GError *error;

	error = NULL;
	cmd = g_strdup_printf( "%s %s", command, path );

	if( !g_spawn_command_line_sync( cmd, &out, &err, NULL, &error )){
		g_warning( "%s: %s", thisfn, error->message );
		g_error_free( error );

	} else {
		print_free_line( message, "out", out );
		print_free_line( message, "err", err );
	}

	g_free( cmd );
}

/*
 * just remove ending newline character
 */
static void
print_free_line( const gchar *message, const gchar *prefix, gchar *line )
{
	gchar *newln = g_utf8_strrchr( line, -1, "U+000A" );
	if( newln ){
		newln = '\x00';
	}
	g_debug( "%s: %s=%s", message, prefix, line );
	g_free( line );
}
