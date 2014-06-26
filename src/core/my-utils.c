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
#include <stdlib.h>
#include <string.h>

#include <time.h>

#include "core/my-utils.h"

static GTimeVal st_timeval;
static GDate    st_date;

static void   on_date_entry_insert_text( GtkEditable *editable, gchar *new_text, gint new_text_length, gpointer position, gpointer user_data );
static gchar *date_entry_insert_text_ddmm( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position );
static void   on_date_entry_changed( GtkEditable *editable, gpointer user_data );
static void   date_entry_parse_ddmm( GDate *date, const gchar *text );

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
 * my_utils_date_set_from_sql:
 * @dest: must be a pointer to the destination GDate structure.
 * @sql_string: a pointer to a SQL string 'yyyy-mm-dd', or NULL;
 *  the SQL string may be zero '0000-00-00' or a valid date.
 *
 * Parse a SQL string, putting the result in @dest.
 */
void
my_utils_date_set_from_sql( GDate *dest, const gchar *sql_string )
{
	g_return_if_fail( dest );

	g_date_clear( dest, 1 );

	if( sql_string &&
			g_utf8_strlen( sql_string, -1 ) &&
			g_utf8_collate( sql_string, "0000-00-00" )){

		g_date_set_parse( dest, sql_string );
	}
}

/**
 * my_utils_date_set_from_date:
 * @dest: must be a pointer to the destination GDate structure.
 * @src: a pointer to a source GDate structure, or NULL.
 *
 * Copy one date to another.
 */
void
my_utils_date_set_from_date( GDate *dest, const GDate *src)
{
	g_return_if_fail( dest );

	g_date_clear( dest, 1 );

	if( src && g_date_valid( src )){
		*dest = *src;
	}
}

/**
 * my_utils_date_to_str:
 *
 * Returns the date with the requested format, suitable for display or
 * SQL insertion, in a newly allocated string that the user must
 * g_free(), or a new empty string if the date is invalid.
 */
gchar *
my_utils_date_to_str( const GDate *date, myDateFormat format )
{
	static const gchar *st_month[] = {
			N_( "jan." ),
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

			/* d mmm yyyy - display as a label */
			case MY_DATE_DMMM:
				g_free( str );
				str = g_strdup_printf( "%d %s %4.4d",
						g_date_get_day( date ),
						gettext( st_month[g_date_get_month( date )-1] ),
						g_date_get_year( date ));
				break;

			/* dd/mm/yyyy - display for entry */
			case MY_DATE_DDMM:
				g_free( str );
				str = g_strdup_printf( "%2.2d/%2.2d/%4.4d",
						g_date_get_day( date ),
						g_date_get_month( date ),
						g_date_get_year( date ));
				break;

			/* yyyy-mm-dd - suitable for SQL insertion */
			case MY_DATE_SQL:
				g_free( str );
				str = g_strdup_printf( "%4.4d-%2.2d-%2.2d",
						g_date_get_year( date ),
						g_date_get_month( date ),
						g_date_get_day( date ));
				break;
		}
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
 * my_utils_date_parse_from_entry:
 * @entry:
 * @format_entry: ignored for now, only considering input as dd/mm/yyyy
 * @label: [allow-none]:
 * @label_entry:
 * date:
 */
void
my_utils_date_parse_from_entry( const myDateParse *parms )
{
	gchar *str;

	g_return_if_fail( parms->entry && GTK_IS_ENTRY( parms->entry ));
	g_return_if_fail( !parms->label || GTK_IS_LABEL( parms->label ));

	g_object_set_data( G_OBJECT( parms->entry ), "date-format-entry", GINT_TO_POINTER( parms->entry_format ));
	g_object_set_data( G_OBJECT( parms->entry ), "date-label", parms->label );
	g_object_set_data( G_OBJECT( parms->entry ), "date-format-label", GINT_TO_POINTER( parms->label_format ));
	g_object_set_data( G_OBJECT( parms->entry ), "date-date", parms->date );

	g_signal_connect( G_OBJECT( parms->entry ), "insert-text", G_CALLBACK( on_date_entry_insert_text ), NULL );
	g_signal_connect( G_OBJECT( parms->entry ), "changed", G_CALLBACK( on_date_entry_changed ), NULL );

	if( parms->on_changed_cb ){
		g_signal_connect( G_OBJECT( parms->entry ), "changed", parms->on_changed_cb, parms->user_data );
	}

	if( parms->date && g_date_valid( parms->date )){
		str = my_utils_date_to_str( parms->date, parms->entry_format );
		gtk_entry_set_text( GTK_ENTRY( parms->entry ), str );
		g_free( str );
	}
}

/*
 * if we are typing in the entry, then new_text is only the typed char
 * if we are pasting from the clipboard, then new_text is the whole pasted text
 *
 * accept almost any separator, replacing it with /
 */
static void
on_date_entry_insert_text( GtkEditable *editable, gchar *new_text, gint new_text_length, gpointer position, gpointer user_data )
{
	myDateFormat entry_format;
	gchar *str;

	/*g_debug( "on_date_entry_insert_text: editable=%p, new_text='%s', newt_text_length=%d, position=%p (%d), user_data=%p",
			( void * ) editable, new_text, new_text_length, ( void * ) position, *( gint * ) position, ( void * ) user_data );*/

	str = NULL;
	entry_format = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( editable ), "date-format-entry" ));

	if( entry_format == MY_DATE_DDMM ){
		str = date_entry_insert_text_ddmm( editable, new_text, new_text_length, ( gint * ) position );
	}

	if( str ){
		g_signal_handlers_block_by_func( editable, ( gpointer ) on_date_entry_insert_text, user_data );
		gtk_editable_insert_text( editable, str, g_utf8_strlen( str, -1 ), position );
		g_signal_handlers_unblock_by_func( editable, ( gpointer ) on_date_entry_insert_text, user_data );
		g_free( str );
	}

	g_signal_stop_emission_by_name( editable, "insert-text" );
}

static gchar *
date_entry_insert_text_ddmm( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position )
{
	static const int st_days[] = { 31,29,31,30,31,30,31,31,30,31,30,31 };
	gunichar ichar;
	gint ival;
	gint pos;
	gint vnum, month;
	gchar *str;

	str = NULL;

	/* typing text in the entry - check for each typed char */
	if( new_text_length == 1 ){
		pos = *position;
		ichar = g_utf8_get_char( new_text );
		if( g_unichar_isdigit( ichar )){
			ival = g_unichar_digit_value( ichar );
			if( pos == 0 ){
				if( ival <= 3 ){
					str = g_strdup( new_text );
				} else {
					str = g_strdup_printf( "0%d/", ival );
				}
			} else if( pos == 1 ){
				vnum = 10 * atoi( gtk_editable_get_chars( editable, 0, 1 )) + ival;
				if( vnum <= 31 ){
					month = atoi( gtk_editable_get_chars( editable, 3, 5 ));
					if( month <= 0 || vnum <= st_days[month-1 ]){
						str = g_strdup( new_text );
					}
				}
			} else if( pos == 3 ){
				if( ival <= 1 ){
					str = g_strdup( new_text );
				} else {
					str = g_strdup_printf( "0%d/", ival );
				}
			} else if( pos == 4 ){
				vnum = 10 * atoi( gtk_editable_get_chars( editable, 3, 4 )) + ival;
				if( vnum <= 12 ){
					str = g_strdup( new_text );
				}
			} else if( pos == 6 ){
				if( ival <= 5 ){
					str = g_strdup_printf( "20%d", ival );
				} else {
					str = g_strdup_printf( "19%d", ival );
				}
			} else if( pos <= 9 ){
				str = g_strdup( new_text );
			}
		} else if( pos == 2  || pos == 5 ){
			str = g_strdup( "/" );
		}
	}

	return( str );
}

/*
 * this callback is called after the above one, i.e. after the newly
 * entered character has been inserted in the field
 */
static void
on_date_entry_changed( GtkEditable *editable, gpointer user_data )
{
	myDateFormat entry_format, label_format;
	GtkWidget *label;
	GDate *date;
	gchar *str, *markup;

	entry_format = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( editable ), "date-format-entry" ));
	date = g_object_get_data( G_OBJECT( editable ), "date-date" );

	if( entry_format == MY_DATE_DDMM ){
		date_entry_parse_ddmm( date, gtk_entry_get_text( GTK_ENTRY( editable )));
	}

	label = g_object_get_data( G_OBJECT( editable ), "date-label" );
	label_format = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( editable ), "date-format-label" ));
	if( label && GTK_IS_LABEL( label )){
		gtk_widget_set_sensitive( label, FALSE );
		if( date && g_date_valid( date )){
			str = my_utils_date_to_str( date, label_format );
			markup = g_markup_printf_escaped( "<span style=\"italic\">%s</span>", str );
			g_free( str );
		} else {
			markup = g_strdup( "" );
		}
		gtk_label_set_markup( GTK_LABEL( label ), markup );
		g_free( markup );
	}
}

static void
date_entry_parse_ddmm( GDate *date, const gchar *text )
{
	gchar **tokens, **iter;
	gint dd, mm, yy;

	dd = mm = yy = 0;
	if( text && g_utf8_strlen( text, -1 )){
		tokens = g_strsplit( text, "/", -1 );
		iter = tokens;
		if( *iter && g_utf8_strlen( *iter, -1 )){
			dd = atoi( *iter );
			iter++;
			if( *iter && g_utf8_strlen( *iter, -1 )){
				mm = atoi( *iter );
				iter++;
				if( *iter && g_utf8_strlen( *iter, -1 )){
					yy = atoi( *iter );
				}
			}
		}
		g_strfreev( tokens );
	}

	if( g_date_valid_dmy( dd, mm, yy )){
		g_date_set_dmy( date, dd, mm, yy );
	}
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
 * my_utils_import_multi_lines:
 *
 * Imports a multi-lines string by splitting on a pipe '|' character.
 *
 * Returns a newly allocated string that the caller should g_free().
 */
gchar *
my_utils_import_multi_lines( const gchar *str )
{
	GString *import;
	gchar **array, **iter;

	import = NULL;

	if( str && g_utf8_strlen( str, -1 )){

		array = g_strsplit( str, "|", -1 );
		iter = array;

		while( *iter ){
			if( import ){
				import = g_string_append( import, "\n" );
			} else {
				import = g_string_new( "" );
			}
			import = g_string_append( import, *iter );
			iter++;
		}

		g_strfreev( array );
	}

	return( import ? g_string_free( import, FALSE ) : NULL );
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
		/*g_debug( "%s: css=%s", thisfn, PKGUIDIR "/ofa.css" );*/
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

/**
 * my_utils_pango_layout_ellipsize:
 *
 * Cannot make the sequence:
 *   pango_layout_set_text( priv->body_layout, ofo_entry_get_label( entry ), -1 );
 *   pango_layout_set_width( priv->body_layout, priv->body_label_max_size );
 *   pango_layout_set_ellipsize( priv->body_layout, PANGO_ELLIPSIZE_END );
 * works when printing. So have decided to write this some utility :(
 */
void
my_utils_pango_layout_ellipsize( PangoLayout *layout, gint max_width )
{
	static const gchar *thisfn = "my_utils_pango_layout_ellipsize";
	PangoRectangle rc;
	const gchar *end;
	gchar *strref, *newend, *newstr;

	strref = g_strdup( pango_layout_get_text( layout ));
	if( g_utf8_validate( strref, -1, &end )){
		pango_layout_get_extents( layout, NULL, &rc );
		/*g_debug( "%s rc.width=%u, max_size=%u", strref, rc.width, max_width );*/
		while( rc.width > max_width ){
			newend = g_utf8_prev_char( end );
			/*g_debug( "newend=%p", ( void * ) newend );*/
			newend[0] = '\0';
			newstr = g_strdup_printf( "%s...", strref );
			pango_layout_set_text( layout, newstr, -1 );
			g_free( newstr );
			pango_layout_get_extents( layout, NULL, &rc );
			if( !g_utf8_validate( strref, -1, &end )){
				break;
			}
			/*g_debug( "%s rc.width=%u, max_size=%u", strref, rc.width, max_width );*/
		}
	} else {
		g_warning( "%s: not a valid UTF8 string: %s", thisfn, pango_layout_get_text( layout ));
	}
	g_free( strref );
}
