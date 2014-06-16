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

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>

#include <time.h>

#include "core/my-utils.h"

static GTimeVal st_timeval;
static GDate    st_date;

/**
 * my_utils_quote:
 *
 * Replace "'" quote characters with "\\'" before executing SQL queries
 */
gchar *
my_utils_quote( const gchar *str )
{
	static const gchar *thisfn = "my_utils_quote";
	GRegex *regex;
	GError *error;
	gchar *new_str;

	error = NULL;
	new_str = NULL;

	if( str && g_utf8_strlen( str, -1 )){

		regex = g_regex_new( "'", 0, 0, &error );
		if( error ){
			g_warning( "%s: g_regex_new=%s", thisfn, error->message );
			g_error_free( error );
			return( NULL );
		}

		new_str = g_regex_replace_literal( regex, str, -1, 0, "\\'", 0, &error );
		if( error ){
			g_warning( "%s: g_regex_replace_literal=%s", thisfn, error->message );
			g_error_free( error );
			g_regex_unref( regex );
			return( NULL );
		}

		g_regex_unref( regex );
	}

	return( new_str );
}

/**
 * my_utils_date_from_str:
 */
const GDate *
my_utils_date_from_str( const gchar *str )
{
	g_date_clear( &st_date, 1 );
	if( str ){
		g_date_set_parse( &st_date, str );
	}
	return( &st_date );
}

/**
 * my_utils_display_from_date:
 *
 * returns the date as 'dd mmm yyyy' suitable for display in a newly
 * allocated string that the user must g_free().
 * or a new empty string if the date is invalid
 */
gchar *
my_utils_display_from_date( const GDate *date, myUtilsDateFormat format )
{
	static const gchar *st_month[] = {
			N_( "janv." ),
			N_( "feb." ),
			N_( "mar." ),
			N_( "apr." ),
			N_( "may" ),
			N_( "jun." ),
			N_( "jul." ),
			N_( "aug." ),
			N_( "sept." ),
			N_( "oct." ),
			N_( "nov." ),
			N_( "dec." )
	};
	gchar *str;

	str = g_strdup( "" );

	if( date && g_date_valid( date )){
		switch( format ){

			case MY_UTILS_DATE_DMMM:
				g_free( str );
				str = g_strdup_printf( "%d %s %4.4d",
						g_date_get_day( date ),
						gettext( st_month[g_date_get_month( date )-1] ),
						g_date_get_year( date ));
				break;

			case MY_UTILS_DATE_DDMM:
				g_free( str );
				str = g_strdup_printf( "%2.2d/%2.2d/%4.4d",
						g_date_get_day( date ),
						g_date_get_month( date ),
						g_date_get_year( date ));
				break;
		}
	}

	return( str );
}

/**
 * my_utils_sql_from_date:
 *
 * returns the date as 'yyyy-mm-dd' suitable for SQL insertion in a newly
 * allocated string that the user must g_free().
 * or NULL if the date is invalid
 */
gchar *
my_utils_sql_from_date( const GDate *date )
{
	gchar *str;

	str = NULL ;

	if( g_date_valid( date )){
		str = g_strdup_printf( "%4.4d-%2.2d-%2.2d",
				g_date_get_year( date ),
				g_date_get_month( date ),
				g_date_get_day( date ));
	}

	return( str );
}

/**
 * my_utils_date_cmp:
 * @infinite_is_past: if %TRUE, then an infinite value (i.e. an invalid
 *  date) is considered lesser than anything, but another infinite value.
 *  Else, an invalid value is considered infinite in the future.
 *
 * Compare the two dates, returning -1, 0 or 1 if a less than, equal or
 * greater than b.
 * An invalid date is considered infinite.
 *
 * Returns: -1, 0 or 1.
 */
gint
my_utils_date_cmp( const GDate *a, const GDate *b, gboolean infinite_is_past )
{
	if( !a || !g_date_valid( a )){
		if( !b || !g_date_valid( b)){
			/* a and b are infinite: returns equal */
			return( 0 );
		}
		/* a is infinite, but not b */
		return( infinite_is_past ? -1 : 1 );
	}

	if( !b || !g_date_valid( b )){
		/* b is infinite, but not a */
		return( infinite_is_past ? 1 : -1 );
	}

	/* a and b are valid */
	return( g_date_compare( a, b ));
}

/**
 * my_utils_stamp_from_str:
 *
 * SQL timestamp is returned as a string '2014-05-24 20:05:46'
 */
const GTimeVal *
my_utils_stamp_from_str( const gchar *str )
{
	gint y, m, d, H, M, S;
	struct tm broken;

	sscanf( str, "%d-%d-%d %d:%d:%d", &y, &m, &d, &H, &M, &S );
	memset( &broken, '\0', sizeof( broken ));
	broken.tm_year = y - 1900;
	broken.tm_mon = m-1;	/* 0 to 11 */
	broken.tm_mday = d;
	broken.tm_hour = H;
	broken.tm_min = M;
	broken.tm_sec = S;
	broken.tm_isdst = -1;
	st_timeval.tv_sec = mktime( &broken );
	st_timeval.tv_usec = 0;
	return( &st_timeval );
}

/**
 * my_utils_str_from_stamp:
 */
gchar *
my_utils_str_from_stamp( const GTimeVal *stamp )
{
	GDateTime *dt;
	gchar *str;

	dt = g_date_time_new_from_timeval_local( stamp );
	str = g_date_time_format( dt, "%d-%m-%Y %H:%M:%S" );
	g_date_time_unref( dt );

	return( str );
}

/**
 * my_utils_timestamp:
 *
 * Returns a newly allocated string 'yyyy-mm-dd hh:mi:ss' suitable for
 * inserting as a timestamp into a sgbd
 */
gchar *
my_utils_timestamp( void )
{
	GDateTime *dt;
	gchar *str;

	dt = g_date_time_new_now_local();
	str = g_date_time_format( dt, "%F %T" );

	return( str );
}

/**
 * my_utils_export_multi_lines:
 *
 * Exports a multi-lines string by joining each line with a pipe '|'.
 *
 * Returns a newly allocated string that the caller should g_free().
 */
gchar *
my_utils_export_multi_lines( const gchar *str )
{
	static const gchar *thisfn = "my_utils_export_multi_lines";
	gchar *export;
	GRegex *regex;
	GError *error;

	export = NULL;
	error = NULL;

	if( str && g_utf8_strlen( str, -1 )){

		regex = g_regex_new( "\n", 0, 0, &error );
		if( error ){
			g_warning( "%s: g_regex_new=%s", thisfn, error->message );
			g_error_free( error );
			return( NULL );
		}

		export = g_regex_replace_literal( regex, str, -1, 0, "|", 0, &error );
		if( error ){
			g_warning( "%s: g_regex_replace_literal=%s", thisfn, error->message );
			g_error_free( error );
			g_regex_unref( regex );
			return( NULL );
		}

		g_regex_unref( regex );
	}

	return( export );
}

/**
 * my_utils_parse_boolean:
 *
 * Parse a string to a boolean.
 * If unset ou empty, the string evaluates to FALSE.
 * Else, the string is compared to True/False/Yes/No in an insensitive
 * manner. The value 1 and 0 are also accepted.
 *
 * Returns TRUE if the string has been successfully parsed, FALSE else.
 */
gboolean
my_utils_parse_boolean( const gchar *str, gboolean *bvar )
{
	g_return_val_if_fail( bvar, FALSE );

	*bvar = FALSE;

	if( str && g_utf8_strlen( str, -1 )){
		if( !g_utf8_collate( str, "1" )){
			*bvar = TRUE;
			return( TRUE );
		}
		if( !g_utf8_collate( str, "0" )){
			*bvar = FALSE;
			return( TRUE );
		}
		if( !g_ascii_strcasecmp( str, "True" )){
			*bvar = TRUE;
			return( TRUE );
		}
		if( !g_ascii_strcasecmp( str, "False" )){
			*bvar = FALSE;
			return( TRUE );
		}
		if( !g_ascii_strcasecmp( str, "Yes" )){
			*bvar = TRUE;
			return( TRUE );
		}
		if( !g_ascii_strcasecmp( str, "No" )){
			*bvar = FALSE;
			return( TRUE );
		}
	}
	return( FALSE );
}

/**
 * my_utils_sql_from_double:
 *
 * Returns: a newly allocated string which represents the specified
 * value, suitable for an SQL insertion.
 */
gchar *
my_utils_sql_from_double( gdouble value )
{
	gchar amount[1+G_ASCII_DTOSTR_BUF_SIZE];

	g_ascii_dtostr( amount, G_ASCII_DTOSTR_BUF_SIZE, value );

	return( g_strdup( amount ));
}

/**
 * my_utils_builder_load_from_path:
 *
 * Returns the loaded widget, or NULL.
 */
GtkWidget *
my_utils_builder_load_from_path( const gchar *path_xml, const gchar *widget_name )
{
	static const gchar *thisfn = "my_utils_builder_load_from_path";
	GtkWidget *widget;
	GError *error;
	GtkBuilder *builder;

	widget = NULL;
	error = NULL;
	builder = gtk_builder_new();

	if( gtk_builder_add_from_file( builder, path_xml, &error )){

		widget = ( GtkWidget * ) gtk_builder_get_object( builder, widget_name );

		if( !widget ){
			g_warning( "%s: unable to find '%s' object in '%s' file",
									thisfn, widget_name, path_xml );

		/* take a ref on non-toplevel widgets to survive to the unref
		 * of the builder */
		} else {
			if( GTK_IS_WIDGET( widget ) && !gtk_widget_is_toplevel( widget )){
				g_object_ref( widget );
			}
		}

	} else {
		g_warning( "%s: %s", thisfn, error->message );
		g_error_free( error );
	}

	g_object_unref( builder );

	return( widget );
}

/**
 * my_utils_str_remove_suffix:
 * @string: source string.
 * @suffix: suffix to be removed from @string.
 *
 * Returns: a newly allocated string, which is a copy of the source @string,
 * minus the removed @suffix if present. If @strings doesn't terminate with
 * @suffix, then the returned string is equal to source @string.
 *
 * The returned string should be g_free() by the caller.
 */
gchar *
my_utils_str_remove_suffix( const gchar *string, const gchar *suffix )
{
	gchar *removed;
	gchar *ptr;

	removed = g_strdup( string );

	if( g_str_has_suffix( string, suffix )){
		ptr = g_strrstr( removed, suffix );
		ptr[0] = '\0';
	}

	return( removed );
}

/**
 * my_utils_str_replace:
 *
 * Replace 'old' string with 'new' in string, returning a newly
 * allocated string which should be g_free() by the caller.
 */
gchar *
my_utils_str_replace( const gchar *string, const gchar *old, const gchar *new )
{
	gchar *new_str;
	GRegex *regex;

	new_str = NULL;

	if( string ){
		regex = g_regex_new( old, 0, 0, NULL );
		new_str = g_regex_replace( regex, string, -1, 0, new, 0, NULL );
		g_regex_unref( regex );
	}

	return( new_str );
}

/**
 * my_utils_entry_get_valid
 */
gboolean
my_utils_entry_get_valid( GtkEntry *entry )
{
	return( TRUE );
}

/**
 * my_utils_entry_set_valid
 */
void
my_utils_entry_set_valid( GtkEntry *entry, gboolean valid )
{
	static const gchar *thisfn = "my_utils_entry_set_valid";
	static GtkCssProvider *css_provider = NULL;
	GError *error;
	GtkStyleContext *style;

	if( !css_provider ){
		css_provider = gtk_css_provider_new();
		error = NULL;
		if( !gtk_css_provider_load_from_path( css_provider, PKGUIDIR "/ofa.css", &error )){
			g_warning( "%s: %s", thisfn, error->message );
			g_error_free( error );
			g_clear_object( &css_provider );
		}
	}

	if( css_provider ){
		style = gtk_widget_get_style_context( GTK_WIDGET( entry ));
		if( valid ){
			gtk_style_context_remove_class( style, "ofaInvalid" );
			gtk_style_context_add_class( style, "ofaValid" );
		} else {
			gtk_style_context_remove_class( style, "ofaValid" );
			gtk_style_context_add_class( style, "ofaInvalid" );
		}
		gtk_style_context_add_provider( style,
				GTK_STYLE_PROVIDER( css_provider ), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );
	}
}

/**
 * my_utils_container_get_child_by_name:
 */
GtkWidget *
my_utils_container_get_child_by_name( GtkContainer *container, const gchar *name )
{
	GList *children = gtk_container_get_children( container );
	GList *ic;
	GtkWidget *found = NULL;
	GtkWidget *child;
	const gchar *child_name;

	for( ic = children ; ic && !found ; ic = ic->next ){

		if( GTK_IS_WIDGET( ic->data )){
			child = GTK_WIDGET( ic->data );
			child_name = gtk_buildable_get_name( GTK_BUILDABLE( child ));
			if( child_name && strlen( child_name ) && !g_ascii_strcasecmp( name, child_name )){
				found = child;
				break;
			}
			if( GTK_IS_CONTAINER( child )){
				found = my_utils_container_get_child_by_name( GTK_CONTAINER( child ), name );
			}
		}
	}

	g_list_free( children );
	return( found );
}

/**
 * my_utils_container_get_child_by_type:
 */
GtkWidget *
my_utils_container_get_child_by_type( GtkContainer *container, GType type )
{
	GList *children = gtk_container_get_children( container );
	GList *ic;
	GtkWidget *found = NULL;
	GtkWidget *child;

	for( ic = children ; ic && !found ; ic = ic->next ){

		if( GTK_IS_WIDGET( ic->data )){
			child = GTK_WIDGET( ic->data );
			if( G_OBJECT_TYPE( ic->data ) == type ){
				found = GTK_WIDGET( ic->data );
				break;
			}
			if( GTK_IS_CONTAINER( child )){
				found = my_utils_container_get_child_by_type( GTK_CONTAINER( child ), type );
			}
		}
	}

	g_list_free( children );
	return( found );
}

/**
 * my_utils_init_notes:
 *
 * This function is mostly used through the "my_utils_init_notes_ex()"
 * macro.
 */
void
my_utils_init_notes( GtkContainer *container, const gchar *widget_name, const gchar *notes )
{
	GtkTextView *text;
	GtkTextBuffer *buffer;
	gchar *str;

	if( notes ){
		str = g_strdup( notes );
	} else {
		str = g_strdup( "" );
	}

	text = GTK_TEXT_VIEW( my_utils_container_get_child_by_name( container, widget_name ));
	buffer = gtk_text_buffer_new( NULL );
	gtk_text_buffer_set_text( buffer, str, -1 );
	gtk_text_view_set_buffer( text, buffer );

	g_object_unref( buffer );
	g_free( str );
}

/**
 * my_utils_init_maj_user_stamp:
 *
 * This function is mostly used through the
 * "my_utils_init_maj_user_stamp_ex()" macro.
 */
void
my_utils_init_maj_user_stamp( GtkContainer *container,
								const gchar *label_name, const GTimeVal *stamp, const gchar *user )
{
	GtkLabel *label;
	gchar *str_stamp, *str;

	label = ( GtkLabel * ) my_utils_container_get_child_by_name( container, label_name );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	str_stamp = my_utils_str_from_stamp( stamp );
	str = g_strdup_printf( "%s (%s)", str_stamp, user );

	gtk_label_set_text( label, str );

	g_free( str );
	g_free( str_stamp );
}

/**
 * my_utils_output_stream_new:
 */
gboolean
my_utils_output_stream_new( const gchar *uri, GFile **file, GOutputStream **stream )
{
	static const gchar *thisfn = "my_utils_output_stream_new";
	GError *error;

	g_return_val_if_fail( uri && g_utf8_strlen( uri, -1 ), FALSE );
	g_return_val_if_fail( file, FALSE );
	g_return_val_if_fail( stream, FALSE );

	*file = g_file_new_for_uri( uri );
	error = NULL;
	*stream = ( GOutputStream * ) g_file_create( *file, G_FILE_CREATE_REPLACE_DESTINATION, NULL, &error );
	if( !*stream ){
		if( error->code == G_IO_ERROR_EXISTS ){
			g_error_free( error );
			error = NULL;
			if( !g_file_delete( *file, NULL, &error )){
				g_warning( "%s: g_file_delete: %s", thisfn, error->message );
				g_error_free( error );
				g_object_unref( *file );
				*file = NULL;
				return( FALSE );
			} else {
				*stream = ( GOutputStream * ) g_file_create( *file, G_FILE_CREATE_REPLACE_DESTINATION, NULL, &error );
				if( !*stream ){
					g_warning( "%s: g_file_create (post delete): %s", thisfn, error->message );
					g_error_free( error );
					g_object_unref( *file );
					*file = NULL;
					return( FALSE );
				}
			}
		} else {
			g_warning( "%s: g_file_create: %s", thisfn, error->message );
			g_error_free( error );
			g_object_unref( *file );
			*file = NULL;
			return( FALSE );
		}
	}

	return( TRUE );
}
