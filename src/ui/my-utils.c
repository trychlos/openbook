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

static void list_perms( const gchar *path, const gchar *message, const gchar *command );

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
 * my_utils_g_value_compare:
 * @a: a #GValue
 * @b: a #GValue
 *
 * Compare the two GValues, returning:
 * - -1 if @a is lesser than @b
 * -  0 if @a and @b are equal
 * - +1 if @a is greater than @b
 *
 * The result of comparing two GValues of different types is undefined.
 */
gint
my_utils_g_value_compare( const GValue *a, const GValue *b )
{
	return( 0 );
}

/**
 * my_utils_g_value_dup:
 * @a: a #GValue
 *
 * Returns a deep copy of @a.
 */
GValue *
my_utils_g_value_dup( const GValue *a )
{
	return( NULL );
}

/**
 * my_utils_g_value_dump:
 * @a: a #GValue
 *
 * Dump the content of the @a #GValue.
 */
void
my_utils_g_value_dump( const GValue *a )
{
}

/**
 * my_utils_g_value_new_from_string:
 * @type: the #GType of the #GValue to be returned.
 * @str: the source string.
 *
 * Returns a new #GValue of #GType @type, which contents the result of
 * the interpretation of @str.
 */
GValue *
my_utils_g_value_new_from_string( GType type, const gchar *str )
{
	return( NULL );
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
		g_debug( "%s: out=%s", message, out );
		g_debug( "%s: err=%s", message, err );
		g_free( out );
		g_free( err );
	}

	g_free( cmd );
}
