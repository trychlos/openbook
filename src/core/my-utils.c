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

#include "api/my-utils.h"
#include "api/ofa-settings.h"

static gunichar st_thousand_sep = '\0';
static gunichar st_decimal_sep  = '\0';
static GRegex  *st_double_regex = NULL;

static void    double_set_locale( void );
static gdouble double_parse_str( const gchar *text, gunichar thousand_sep, gunichar decimal_sep );
static void    int_list_to_position( GList *list, gint *x, gint *y, gint *width, gint *height );
static GList  *position_to_int_list( gint x, gint y, gint width, gint height );

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
 * my_utils_stamp_set_from_sql:
 * @timeval: a pointer to a GTimeVal structure
 * @str: [allow-none]:
 *
 * SQL timestamp is returned as a string '2014-05-24 20:05:46'
 */
GTimeVal *
my_utils_stamp_set_from_sql( GTimeVal *timeval, const gchar *str )
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

	timeval->tv_sec = mktime( &broken );
	timeval->tv_usec = 0;

	return( timeval );
}

/**
 * my_utils_stamp_to_str:
 */
gchar *
my_utils_stamp_to_str( const GTimeVal *stamp, myStampFormat format )
{
	GDateTime *dt;
	gchar *str;

	str = NULL;
	dt = g_date_time_new_from_timeval_local( stamp );
	switch( format ){
		case MY_STAMP_YYMDHMS:
			str = g_date_time_format( dt, "%Y-%m-%d %H:%M:%S" );
			break;
		case MY_STAMP_DMYYHM:
			str = g_date_time_format( dt, "%d/%m/%Y %H:%M" );
			break;
	}
	g_date_time_unref( dt );

	return( str );
}

/**
 * my_utils_stamp_get_now:
 *
 * Returns a newly allocated string 'yyyy-mm-dd hh:mi:ss' suitable for
 * inserting as a timestamp into a sgbd
 */
GTimeVal *
my_utils_stamp_get_now( GTimeVal *timeval )
{
	GDateTime *dt;

	dt = g_date_time_new_now_local();
	g_date_time_to_timeval( dt, timeval );
	g_date_time_unref( dt );

	return( timeval );
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
 * my_utils_boolean_set_from_str:
 *
 * Parse a string to a boolean.
 * If unset ou empty, the string evaluates to FALSE.
 * Else, the string is compared to True/False/Yes/No in an insensitive
 * manner. The value 1 and 0 are also accepted.
 *
 * Returns TRUE if the string has been successfully parsed, FALSE else.
 */
gboolean
my_utils_boolean_set_from_str( const gchar *str, gboolean *bvar )
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
 * my_utils_double_undecorate:
 *
 * Remove from the given string all decoration added for the display
 * of a double, returning so a 'brut' double, without thousand separator
 * and with a dot as the decimal point
 */
gchar *
my_utils_double_undecorate( const gchar *text )
{
	GRegex *regex;
	gchar *dest1, *dest2;

	regex = g_regex_new( " ", 0, 0, NULL );
	dest1 = g_regex_replace_literal( regex, text, -1, 0, "", 0, NULL );
	g_regex_unref( regex );

	regex = g_regex_new( ",", 0, 0, NULL );
	dest2 = g_regex_replace_literal( regex, dest1, -1, 0, ".", 0, NULL );
	g_regex_unref( regex );

	g_free( dest1 );

	return( dest2 );
}

/**
 * my_utils_double_from_string:
 *
 * In v1, we only target fr locale, so with space as thousand separator
 * and comma as decimal one on display -
 * When parsing a string - and because we want be able to re-parse a
 * string that we have previously displayed - we accept both
 */
gdouble
my_utils_double_from_string( const gchar *string )
{
	gchar *text;
	gdouble d;

	if( string && g_utf8_strlen( string, -1 )){
		text = my_utils_double_undecorate( string );
		d = g_strtod( text, NULL );
		g_free( text );

	} else {
		d = 0.0;
	}

	return( d );
}

/**
 * my_utils_double_set_from_sql:
 *
 * SQl amount is returned as a stringified number, without thousand
 * separator, and with dot '.' as decimal separator
 */
gdouble
my_utils_double_set_from_sql( const gchar *sql_string )
{
	double_set_locale();

	if( !sql_string || !g_utf8_strlen( sql_string, -1 )){
		return( 0.0 );
	}

	return( double_parse_str( sql_string, '\0', '.' ));
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

/*
 * when run for the first time, evaluate the thousant separator and the
 * decimal separator for the current locale
 *
 * they are those which will be outputed by printf(),
 * and that g_ascii_strtod() is able to sucessfully parse.
 */
static void
double_set_locale( void )
{
	gchar *str, *p, *srev;

	if( !st_thousand_sep ){
		str = g_strdup_printf( "%'.1lf", 1000.0 );
		p = g_utf8_next_char( str );
		st_thousand_sep = g_utf8_get_char( p );
		srev = g_utf8_strreverse( str, -1 );
		p = g_utf8_next_char( srev );
		st_decimal_sep = g_utf8_get_char( p );

		g_debug( "gdouble_set_locale: thousand_sep='%c', decimal_sep='%c'",
					st_thousand_sep, st_decimal_sep );

		str = g_strdup_printf( "%c", st_thousand_sep );
		st_double_regex = g_regex_new( str, 0, 0, NULL );
		g_free( str );
	}
}

static gdouble
double_parse_str( const gchar *text, gunichar thousand_sep, gunichar decimal_sep )
{
	static const gchar *thisfn = "my_utils_double_parse_str";
	gchar *dup;
	gchar *str_delims;
	gdouble amount;
	GError *error;

	if( thousand_sep ){
		error = NULL;
		dup = g_regex_replace_literal( st_double_regex, text, -1, 0, "", 0, &error );
		if( !dup ){
			g_warning( "%s: %s", thisfn, error->message );
			g_error_free( error );
		}
	} else {
		dup = g_strdup( text );
	}

	if( decimal_sep && decimal_sep != '.' ){
		str_delims = g_strdup_printf( "%c", decimal_sep );
		dup = g_strdelimit( dup, str_delims, '.' );
		g_free( str_delims );
	}

	amount = g_ascii_strtod( dup, NULL );
	g_free( dup );

	return( amount );
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

	str_stamp = my_utils_stamp_to_str( stamp, MY_STAMP_YYMDHMS );
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

/**
 * my_utils_window_restore_position:
 */
void
my_utils_window_restore_position( GtkWindow *toplevel, const gchar *name )
{
	static const gchar *thisfn = "my_utils_window_restore_position";
	gchar *key;
	GList *list;
	gint x=0, y=0, width=0, height=0;

	/*g_debug( "%s: toplevel=%p (%s), name=%s",
			thisfn, ( void * ) toplevel, G_OBJECT_TYPE_NAME( toplevel ), name );*/

	key = g_strdup_printf( "%s-pos", name );
	list = ofa_settings_get_uint_list( key );
	g_free( key );
	/*g_debug( "%s: list=%p (count=%d)", thisfn, ( void * ) list, g_list_length( list ));*/

	if( list ){
		int_list_to_position( list, &x, &y, &width, &height );
		g_debug( "%s: name=%s, x=%d, y=%d, width=%d, height=%d", thisfn, name, x, y, width, height );
		g_list_free( list );

		gtk_window_move( toplevel, x, y );
		gtk_window_resize( toplevel, width, height );

	} else {
		g_debug( "%s: list=%p (count=%d)", thisfn, ( void * ) list, g_list_length( list ));
	}
}

/*
 * extract the position of the window from the list of unsigned integers
 */
static void
int_list_to_position( GList *list, gint *x, gint *y, gint *width, gint *height )
{
	GList *it;
	int i;

	g_assert( x );
	g_assert( y );
	g_assert( width );
	g_assert( height );

	for( it=list, i=0 ; it ; it=it->next, i+=1 ){
		switch( i ){
			case 0:
				*x = GPOINTER_TO_UINT( it->data );
				break;
			case 1:
				*y = GPOINTER_TO_UINT( it->data );
				break;
			case 2:
				*width = GPOINTER_TO_UINT( it->data );
				break;
			case 3:
				*height = GPOINTER_TO_UINT( it->data );
				break;
		}
	}
}

/**
 * my_utils_window_save_position:
 */
void
my_utils_window_save_position( GtkWindow *toplevel, const gchar *name )
{
	static const gchar *thisfn = "my_utils_window_save_position";
	gint x, y, width, height;
	gchar *key;
	GList *list;

	gtk_window_get_position( toplevel, &x, &y );
	gtk_window_get_size( toplevel, &width, &height );
	g_debug( "%s: wsp_name=%s, x=%d, y=%d, width=%d, height=%d", thisfn, name, x, y, width, height );

	list = position_to_int_list( x, y, width, height );

	key = g_strdup_printf( "%s-pos", name );
	ofa_settings_set_uint_list( key, list );

	g_free( key );
	g_list_free( list );
}

/*
 * the returned list should be g_list_free() by the caller
 */
static GList *
position_to_int_list( gint x, gint y, gint width, gint height )
{
	GList *list = NULL;

	list = g_list_append( list, GUINT_TO_POINTER( x ));
	list = g_list_append( list, GUINT_TO_POINTER( y ));
	list = g_list_append( list, GUINT_TO_POINTER( width ));
	list = g_list_append( list, GUINT_TO_POINTER( height ));

	return( list );
}
