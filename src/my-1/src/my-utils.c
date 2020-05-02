/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>

#include "my/my-isettings.h"
#include "my/my-stamp.h"
#include "my/my-style.h"
#include "my/my-utils.h"

typedef struct {
	gchar        *name;
	GtkBuildable *buildable;
}
	sBuildableByName;

static GRegex      *st_quote_single_regex      = NULL;
static GRegex      *st_long_dash_regex         = NULL;
static const gchar *st_save_restore_group      = "orgtrychlosmy";

static gchar      *quote_backslashes( const gchar *str );
static gboolean    utils_quote_cb( const GMatchInfo *info, GString *res, gpointer data );
static gboolean    utils_unquote_cb( const GMatchInfo *info, GString *res, gpointer data );
static void        child_set_editable_cb( GtkWidget *widget, gpointer data );
static void        my_utils_container_dump_rec( GtkContainer *container, const gchar *prefix );
static void        on_notes_changed( GtkTextBuffer *buffer, void *user_data );
static GMenuModel *menu_get_model_rec( GMenuModel *menu, const gchar *id, gint *pos );
static gboolean    position_get_geometry( myISettings *settings, const gchar *name, gint *x, gint *y, gint *width, gint *height );
static void        position_save_geometry( myISettings *settings, const gchar *name, gint x, gint y, gint width, gint height );
static gchar      *position_get_key( const gchar *name );
static GSList     *split_by_line( const gchar *content );
static gboolean    is_dir( GFile *file );
static gboolean    is_readable_gfile( GFile *file );

/**
 * my_casefold:
 * @str:
 *
 * Returns: a case-insensitive version of @str.
 *
 * Note that this function is just a work-around to the not-null
 * assertion of g_utf8_casefold().
 */
gchar *
my_casefold( const gchar *str )
{
	return( str ? g_utf8_casefold( str, -1 ) : g_strdup( "" ));
}

/**
 * my_collate:
 * @a:
 * @b:
 *
 * Returns: 1 if a > b, -1 if a < b 0 if equals.
 *
 * Note that this function is just a work-around to the not-null
 * assertion of g_utf8_collate().
 */
gint
my_collate( const gchar *a, const gchar *b )
{
	return( a && b ? g_utf8_collate( a, b ) : ( a ? 1 : ( b ? -1 : 0 )));
}

/**
 * my_strlen:
 * @str:
 */
glong
my_strlen( const gchar *str )
{
	return( str ? g_utf8_strlen( str, -1 ) : 0 );
}

/**
 * my_utils_dump_gslist:
 *
 * Dump a GSList list of GSList datas.
 */
void
my_utils_dump_gslist( const GSList *list )
{
	static const gchar *thisfn = "my_utils_dump_gslist";
	const GSList *itl, *fields, *itf;
	const gchar *cstr;
	gint numline;

	numline = 0;

	for( itl=list ; itl ; itl=itl->next ){
		numline += 1;
		fields = ( const GSList * ) itl->data;
		for( itf=fields ; itf ; itf=itf->next ){
			cstr = ( const gchar * ) itf->data;
			g_debug( "%s: numline=%d, str='%s'", thisfn, numline, cstr );
		}
	}
}

void
my_utils_dump_gslist_str( const GSList *lines )
{
	static const gchar *thisfn = "my_utils_dump_gslist_str";
	const GSList *itl, *fields, *itf;
	GString *s;
	guint numline;

	numline = 0;

	for( itl=lines ; itl ; itl=itl->next ){
		fields = ( const GSList * ) itl->data;
		s = NULL;
		for( itf=fields ; itf ; itf=itf->next ){
			if( !s ){
				s = g_string_new(( const gchar * ) itf->data );
			} else {
				g_string_append_printf( s, ";%s", ( const gchar * ) itf->data );
			}
		}
		g_debug( "%s [%6u]: %s", thisfn, ++numline, s->str );
		g_string_free( s, TRUE );
	}
}

/**
 * my_utils_quote_sql:
 *
 * Backslash:
 * - backslashes
 * - single quotes
 */
gchar *
my_utils_quote_sql( const gchar *str )
{
	gchar *str1, *str2;

	str1 = quote_backslashes( str );
	str2 = my_utils_quote_single( str1 );

	g_free( str1 );

	return( str2 );
}

/*
 * Replace every single backslash by two backslashes
 */
static gchar *
quote_backslashes( const gchar *str )
{
	static const gchar *thisfn = "my_utils_quote_backslashes";
	static GRegex *st_regex;
	GError *error;
	gchar *res;

	res = NULL;

	if( str ){
		error = NULL;
		if( !st_regex ){
			st_regex = g_regex_new( "\\\\", 0, 0, &error );
			if( error ){
				g_warning( "%s: g_regex_new=%s", thisfn, error->message );
				g_error_free( error );
				return( NULL );
			}
		}
		g_return_val_if_fail( st_regex, NULL );
		res = g_regex_replace_literal( st_regex, str, -1, 0, "\\\\", 0, &error );
		if( error ){
			g_warning( "%s: g_regex_replace_literal=%s", thisfn, error->message );
			g_error_free( error );
			return( NULL );
		}
	}

	return( res );
}

/**
 * my_utils_quote_single:
 *
 * Replace "'" quote characters with "\'" before executing SQL queries
 */
gchar *
my_utils_quote_single( const gchar *str )
{
	static const gchar *thisfn = "my_utils_quote_single";
	GError *error;
	gchar *new_str;

	new_str = NULL;

	if( str ){
		error = NULL;
		if( !st_quote_single_regex ){
			st_quote_single_regex = g_regex_new( "'", 0, 0, &error );
			if( error ){
				g_warning( "%s: g_regex_new=%s", thisfn, error->message );
				g_error_free( error );
				return( NULL );
			}
		}
		g_return_val_if_fail( st_quote_single_regex, NULL );
		new_str = g_regex_replace_literal( st_quote_single_regex, str, -1, 0, "\\'", 0, &error );
		if( error ){
			g_warning( "%s: g_regex_replace_literal=%s", thisfn, error->message );
			g_error_free( error );
			return( NULL );
		}
	}

	return( new_str );
}

/**
 * my_utils_quote_regexp:
 *
 * Backslash characters in the provided regular expression.
 *
 * Use case: strings export.
 */
gchar *
my_utils_quote_regexp( const gchar *str, const gchar *regexp )
{
	static const gchar *thisfn = "my_utils_quote_regexp";
	GRegex *regex;
	GError *error;
	gchar *out_str;

	out_str = NULL;

	if( str ){
		error = NULL;
		regex = g_regex_new( regexp, 0, 0, &error );
		if( error || !regex ){
			g_warning( "%s: g_regex_new=%s (%s)", thisfn, error->message, regexp );
			g_error_free( error );
			return( NULL );
		}
		out_str = g_regex_replace_eval( regex, str, -1, 0, 0, utils_quote_cb, NULL, &error );
		if( error ){
			g_warning( "%s: g_regex_replace_literal=%s (%s)", thisfn, error->message, regexp );
			g_error_free( error );
			return( NULL );
		}
		/* debug */
		if( 0 ){
			g_debug( "%s: regexp='%s', str='%s', new_str='%s'", thisfn, regexp, str, out_str );
		}
		g_regex_unref( regex );
	}

	return( out_str );
}

static gboolean
utils_quote_cb( const GMatchInfo *info, GString *res, gpointer data )
{
	gchar *match;

	match = g_match_info_fetch( info, 0 );
	g_string_append_printf( res, "\\%s", match );

	return( FALSE );
}

/**
 * my_utils_unquote_regexp:
 *
 * Unbackslash characters in the provided regular expression.
 *
 * Use case: strings import.
 */
gchar *
my_utils_unquote_regexp( const gchar *str, const gchar *regexp )
{
	static const gchar *thisfn = "my_utils_unquote_regexp";
	GRegex *regex;
	GError *error;
	gchar *out_str;

	out_str = NULL;

	if( str ){
		error = NULL;
		regex = g_regex_new( regexp, 0, 0, &error );
		if( error || !regex ){
			g_warning( "%s: g_regex_new=%s (%s)", thisfn, error->message, regexp );
			g_error_free( error );
			return( NULL );
		}
		out_str = g_regex_replace_eval( regex, str, -1, 0, 0, utils_unquote_cb, NULL, &error );
		if( error ){
			g_warning( "%s: g_regex_replace_literal=%s (%s)", thisfn, error->message, regexp );
			g_error_free( error );
			return( NULL );
		}
		g_regex_unref( regex );
	}

	return( out_str );
}

static gboolean
utils_unquote_cb( const GMatchInfo *info, GString *res, gpointer data )
{
	gchar *match;

	match = g_match_info_fetch( info, 0 );
	if( 0 ){
		g_debug( "string='%s', count=%d, match='%s'",
				g_match_info_get_string( info ), g_match_info_get_match_count( info ), match );
	}
	g_string_append_printf( res, "%s", match );

	return( FALSE );
}

/**
 * my_utils_subst_long_dash:
 * @str:
 *
 * Returns: the same string, after having substituted long dash with
 * simple dash.
 *
 * Rationale: g_convert() doesn't known how to convert long dash from
 * UTF-8 to ISO-8859-15
 */
gchar *
my_utils_subst_long_dash( const gchar *str )
{
	static const gchar *thisfn = "my_utils_subst_long_dash";
	GError *error;
	gchar *out_str;

	out_str = NULL;

	if( str ){
		error = NULL;
		if( !st_long_dash_regex ){
			st_long_dash_regex = g_regex_new( "–", 0, 0, &error );
			if( error ){
				g_warning( "%s: g_regex_new=%s", thisfn, error->message );
				g_error_free( error );
				return( NULL );
			}
		}
		out_str = g_regex_replace( st_long_dash_regex, str, -1, 0, "-", 0, &error );
		if( error ){
			g_warning( "%s: g_regex_replace=%s", thisfn, error->message );
			g_error_free( error );
			return( NULL );
		}
	}

	return( out_str );
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

	if( my_strlen( str )){

		regex = g_regex_new( "\n", 0, 0, &error );
		if( error ){
			g_warning( "%s: g_regex_new=%s", thisfn, error->message );
			g_error_free( error );
			return( NULL );
		}

		export = g_regex_replace_literal( regex, str, -1, 0, "][", 0, &error );
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

	if( my_strlen( str )){

		array = g_strsplit( str, "][", -1 );
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
 * my_utils_boolean_from_str:
 *
 * Parse a string to a boolean.
 * If unset ou empty, the string evaluates to FALSE.
 * Else, the string is compared to True/False/Yes/No in an insensitive
 * manner. The value 1 and 0 are also accepted.
 */
gboolean
my_utils_boolean_from_str( const gchar *str )
{
	if( my_strlen( str )){
		if( !g_utf8_collate( str, "1" )){
			return( TRUE );
		}
		if( !g_utf8_collate( str, "0" )){
			return( FALSE );
		}
		if( !g_ascii_strcasecmp( str, "True" )){
			return( TRUE );
		}
		if( !g_ascii_strcasecmp( str, "False" )){
			return( FALSE );
		}
		if( !g_ascii_strcasecmp( str, "Y" )){
			return( TRUE );
		}
		if( !g_ascii_strcasecmp( str, "N" )){
			return( FALSE );
		}
		if( !g_ascii_strcasecmp( str, "Yes" )){
			return( TRUE );
		}
		if( !g_ascii_strcasecmp( str, "No" )){
			return( FALSE );
		}
	}
	return( FALSE );
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
 * my_utils_builder_load_from_resource:
 *
 * Returns the loaded widget, or NULL.
 */
GtkWidget *
my_utils_builder_load_from_resource( const gchar *resource, const gchar *widget_name )
{
	static const gchar *thisfn = "my_utils_builder_load_from_resource";
	GtkWidget *widget;
	GtkBuilder *builder;

	builder = gtk_builder_new_from_resource( resource );

	widget = ( GtkWidget * ) gtk_builder_get_object( builder, widget_name );
	if( widget ){
		/* take a ref on non-toplevel widgets to survive to the unref of the builder */
		if( GTK_IS_WIDGET( widget ) && !gtk_widget_is_toplevel( widget )){
			g_object_ref( widget );
		}
	} else {
		g_warning( "%s: unable to find '%s' object in '%s' resource",
								thisfn, widget_name, resource );
	}

	g_object_unref( builder );

	return( widget );
}

/**
 * my_utils_msg_dialog:
 * @parent: [allow-none]: the #GtkWindow parent.
 * @type: the type of the displayed #GtkMessageDialog.
 * @msg: the message to be displayed.
 *
 * Display a message dialog.
 */
void
my_utils_msg_dialog( GtkWindow *parent, GtkMessageType type, const gchar *msg )
{
	GtkWidget *dialog;

	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	dialog = gtk_message_dialog_new(
					parent,
					GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					type,
					GTK_BUTTONS_CLOSE,
					"%s", msg );

	gtk_dialog_run( GTK_DIALOG( dialog ));
	gtk_widget_destroy( dialog );
}

/**
 * my_utils_dialog_question:
 *
 * Returns: %TRUE if OK.
 */
gboolean
my_utils_dialog_question( GtkWindow *parent, const gchar *msg, const gchar *ok_text )
{
	GtkWidget *dialog;
	gint response;

	g_return_val_if_fail( !parent || GTK_IS_WINDOW( parent ), FALSE );

	dialog = gtk_message_dialog_new(
			parent,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE, "%s", msg );

	gtk_dialog_add_buttons( GTK_DIALOG( dialog ),
			_( "_Cancel" ), GTK_RESPONSE_CANCEL,
			ok_text, GTK_RESPONSE_OK,
			NULL );

	response = gtk_dialog_run( GTK_DIALOG( dialog ));

	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
}

/**
 * my_utils_char_replace:
 *
 * Replace @old_ch char with @new_ch in string, returning a newly
 * allocated string which should be g_free() by the caller.
 */
gchar *
my_utils_char_replace( const gchar *string, gchar old_ch, gchar new_ch )
{
	gchar *str;
	gint i;

	str = g_strdup( string );

	if( my_strlen( str ) && g_utf8_validate( str, -1, NULL )){
		for( i=0 ; str[i] ; ++i ){
			if( str[i] == old_ch ){
				str[i] = new_ch;
			}
		}
	}

	return( str );
}

/**
 * my_utils_str_in_list:
 * @str: the string to be searched for.
 * @list: the #GList list.
 *
 * Returns: %TRUE if @str is found in @list.
 */
gboolean
my_utils_str_in_list( const gchar *str, const GList *list )
{
	gboolean ok;
	const GList *it;

	ok = FALSE;

	for( it=list ; it ; it=it->next ){
		if( my_collate(( const gchar * ) it->data, str ) == 0 ){
			ok = TRUE;
			break;
		}
	}

	return( ok );
}

/**
 * my_utils_str_first_word:
 * @string: the string to be searched for.
 *
 * Returns: The first word in the @string string, as a newly allocated
 * string which should be g_free() by the caller.
 */
gchar *
my_utils_str_first_word( const gchar *string )
{
	gchar *new_str;
	GRegex *regex;

	new_str = NULL;

	if( string ){
		regex = g_regex_new( "^\\s*(\\w+).*$", 0, 0, NULL );
		new_str = g_regex_replace( regex, string, -1, 0, "\\1", 0, NULL );
		g_regex_unref( regex );
	}
	/*g_debug( "my_utils_str_first_word: in.str=%s, out.str=%s", string, new_str );*/

	return( new_str );
}

/**
 * my_utils_str_funny_capitalized:
 * @string: the string to be dealt with.
 *
 * Returns: The funny capitalized @string, as a new string which should
 * be g_free() by the caller.
 */
gchar *
my_utils_str_funny_capitalized( const gchar *string )
{
	gchar **array, **it;
	GString *gstr;
	gchar *first, *first_case, *others, *others_case;

	gstr = NULL;

	if( string ){
		array = g_regex_split_simple( "[- \\.]", string, 0, 0 );
		it = array;
		gstr = g_string_new( "" );
		while( *it ){
			first = g_utf8_substring( *it, 0, 1 );
			first_case = g_utf8_strup( first, -1 );
			others = g_utf8_substring( *it, 1, my_strlen( *it ));
			others_case = g_utf8_strdown( others, -1 );
			g_string_append_printf( gstr, "%s%s", first_case, others_case );
			g_free( first );
			g_free( first_case );
			g_free( others );
			g_free( others_case );
			it++;
		}
		g_strfreev( array );
	}

	return( gstr ? g_string_free( gstr, FALSE ) : NULL );
}

/**
 * my_utils_str_remove_str_delim:
 * @string:
 * @fieldsep:
 * @strdelim:
 *
 * Returns a newly allocated string pointed without string delimiters.
 */
gchar *
my_utils_str_remove_str_delim( const gchar *cstr, gchar fieldsep, gchar strdelim )
{
	gchar *str1, *str2, *regexp, *str_delim_str;
	glong len;

	str2 = NULL;
	if( cstr ){
		str1 = NULL;
		str_delim_str = g_strdup_printf( "%c", strdelim );
		len = my_strlen( cstr );
		if( len ){
			if( strdelim && g_str_has_prefix( cstr, str_delim_str ) && g_str_has_suffix( cstr, str_delim_str )){
				str1 = g_utf8_substring( cstr, 1, len-1 );
			} else {
				str1 = g_strstrip( g_strdup( cstr ));
			}
			regexp = g_strdup_printf( "(\\\")|(\\\n)|(\\\r)|(\\%c)", fieldsep );
			str2 = my_utils_unquote_regexp( str1, regexp );
			if( !my_strlen( str2 )){
				g_free( str2 );
				str2 = NULL;
			}
		}
		if( 0 ){
			g_debug( "src='%s', temp='%s', out='%s'", cstr, str1, str2 );
		}
		g_free( str_delim_str );
		g_free( str1 );
	}

	return( str2 );
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
 * my_utils_str_remove_underline:
 * @string: source string.
 *
 * Returns: a newly allocated string as a copy of the source @string,
 * minus the present underlines '_'.
 *
 * The returned string should be g_free() by the caller.
 */
gchar *
my_utils_str_remove_underlines( const gchar *string )
{
	gchar **array, **iter;
	GString *result;

	array = g_strsplit( string, "_", -1 );
	result = g_string_new( "" );

	iter = array;
	while( *iter ){
		result = g_string_append( result, *iter );
		iter++;
	}
	g_strfreev( array );

	return( g_string_free( result, FALSE ));
}

/**
 * my_utils_str_replace:
 *
 * Replace 'old' string with 'new' in string, returning a newly
 * allocated string which should be g_free() by the caller.
 *
 * #my_utils_str_replace makes use of word boundary in a regex. It so
 * makes sure to only replace a whole word, and not a part of a bigger
 * (longer) one.
 */
gchar *
my_utils_str_replace( const gchar *string, const gchar *old, const gchar *new )
{
	gchar *regex_str, *new_str;
	GRegex *regex;

	new_str = NULL;

	if( string ){
		regex_str = g_strdup_printf( "\\b%s\\b", old );
		regex = g_regex_new( regex_str, 0, 0, NULL );
		new_str = g_regex_replace( regex, string, -1, 0, new, 0, NULL );
		g_regex_unref( regex );
		g_free( regex_str );
	}

	return( new_str );
}

/**
 * my_utils_str_from_uint_list:
 * @uint_list: a #GList of guint
 * @sep: the character separator.
 *
 * Returns: a @sep-separated list of unsigned integers, as a newly
 * allocated string which should be g_free() by the caller.
 */
gchar *
my_utils_str_from_uint_list( GList *uint_list, const gchar *sep )
{
	GString *str;
	GList *it;
	guint data;

	str = g_string_new( NULL );
	for( it=uint_list ; it ; it=it->next ){
		data = GPOINTER_TO_UINT( it->data );
		if( str->len > 0 ){
			g_string_append_printf( str, "%s", sep );
		}
		g_string_append_printf( str, "%u", data );
	}

	return( g_string_free( str, FALSE ));
}

/**
 * my_utils_str_to_uint_list:
 * @string: a @sep-separated list of unsigned integers.
 * @sep: the character separator.
 *
 * Returns: list of unsigned integers, as a newly allocated #GList which
 * should be g_list_free() by the caller.
 */
GList *
my_utils_str_to_uint_list( const gchar *string, const gchar *sep )
{
	gchar **array, **iter;
	GList *list;
	guint data;

	list = NULL;

	if( my_strlen( string )){
		array = g_strsplit( string, sep, -1 );
		list = NULL;
		iter = array;
		while( *iter ){
			data = atoi( *iter );
			list = g_list_prepend( list, GUINT_TO_POINTER( data ));
			iter++;
		}
		g_strfreev( array );
	}

	return( g_list_reverse( list ));
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
 * my_utils_container_attach_from_resource:
 * @container: the target container
 * @resource: the path of the bundled resource
 * @window: the name of the top window in the resource
 * @widget: the name of the top widget to be attached to the @container.
 *
 * Load the named resource @resource, load the named @window, extract
 * the named @widget from the @window, attaching this @widget to the
 * @container.
 *
 * Returns: the loaded @widget, or %NULL.
 */
GtkWidget *
my_utils_container_attach_from_resource( GtkContainer *container, const gchar *resource, const gchar *window, const gchar *widget )
{
	GtkWidget *toplevel, *top_widget;

	g_return_val_if_fail( container && GTK_IS_CONTAINER( container ), NULL );

	toplevel = my_utils_builder_load_from_resource( resource, window );
	g_return_val_if_fail( toplevel && GTK_IS_WINDOW( toplevel ), NULL );
	top_widget = my_utils_container_attach_from_window( container, GTK_WINDOW( toplevel ), widget );
	gtk_widget_destroy( toplevel );

	return( top_widget );
}

/**
 * my_utils_container_attach_from_ui:
 * @container: the target container
 * @ui: the path to the .xml file
 * @window: the name of the top window in the .xml file
 * @widget: the name of the widget to be attached to the @container.
 *
 * Read the named .xml file @ui, load the named @window, extract the
 * named @widget from the @window, attaching this @widget to the
 * @container.
 *
 * Returns: the loaded @widget, or %NULL.
 */
GtkWidget *
my_utils_container_attach_from_ui( GtkContainer *container, const gchar *ui, const gchar *window, const gchar *widget )
{
	GtkWidget *toplevel, *top_widget;

	g_return_val_if_fail( container && GTK_IS_CONTAINER( container ), NULL );

	toplevel = my_utils_builder_load_from_path( ui, window );
	g_return_val_if_fail( toplevel && GTK_IS_WINDOW( toplevel ), NULL );
	top_widget = my_utils_container_attach_from_window( container, GTK_WINDOW( toplevel ), widget );
	gtk_widget_destroy( toplevel );

	return( top_widget );
}

/**
 * my_utils_container_attach_from_window:
 * @container: the target container
 * @window: a #GtkWindow, most probably loaded from a .xml UI definition file
 * @widget: the name of the top widget inside of @window, to be attached
 *  to the @container.
 *
 * Extract the named @widget from the @window, attaching this @widget to the
 * @container.
 *
 * Returns: the loaded @widget, or %NULL.
 */
GtkWidget *
my_utils_container_attach_from_window( GtkContainer *container, GtkWindow *window, const gchar *widget )
{
	GtkWidget *top_widget;

	g_return_val_if_fail( container && GTK_IS_CONTAINER( container ), NULL );
	g_return_val_if_fail( window && GTK_IS_WINDOW( window ), NULL );

	top_widget = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), widget );
	g_return_val_if_fail( top_widget && GTK_IS_CONTAINER( top_widget ), NULL );

	g_object_ref( top_widget );
	gtk_container_remove( GTK_CONTAINER( window ), top_widget );
	gtk_container_add( container, top_widget );
	g_object_unref( top_widget );

	return( top_widget );
}

/**
 * my_utils_container_set_editable:
 * @container: the target container
 * @editable: whether the container may be edited.
 *
 * Set all widgets of @container editable.
 *
 * Note that gtk_container_foreach() is not recursive by itself.
 */
void
my_utils_container_set_editable( GtkContainer *container, gboolean editable )
{
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));
	gtk_container_foreach(
			container,
			( GtkCallback ) child_set_editable_cb,
			GUINT_TO_POINTER(( guint ) editable ));
}

static void
child_set_editable_cb( GtkWidget *widget, gpointer data )
{
	gboolean editable = ( gboolean ) GPOINTER_TO_UINT( data );

	/*g_debug( "child_set_editable_cb: %s", G_OBJECT_TYPE_NAME( widget ));*/

	if( GTK_IS_CONTAINER( widget ) &&
			!GTK_IS_BUTTON( widget ) && !GTK_IS_COMBO_BOX( widget ) && !GTK_IS_TEXT_VIEW( widget )){
		my_utils_container_set_editable( GTK_CONTAINER( widget ), editable );
	} else {
		my_utils_widget_set_editable( widget, editable );
	}
}

/**
 * my_utils_container_dump:
 *
 * Recursively dumps a container.
 */
void
my_utils_container_dump( GtkContainer *container )
{
	static const gchar *thisfn = "my_utils_container_dump";

	g_debug( "%s: container=%p (%s)",
			thisfn, ( void * ) container, G_OBJECT_TYPE_NAME( container ));

	my_utils_container_dump_rec( container, " " );
}

static void
my_utils_container_dump_rec( GtkContainer *container, const gchar *prefix )
{
	static const gchar *thisfn = "my_utils_container_dump";
	GList *children, *ic;
	GtkWidget *child;
	const gchar *child_name;
	gchar *new_prefix;

	children = gtk_container_get_children( container );

	for( ic = children ; ic ; ic = ic->next ){
		if( GTK_IS_WIDGET( ic->data )){
			child = GTK_WIDGET( ic->data );
			child_name = gtk_buildable_get_name( GTK_BUILDABLE( child ));
			g_debug( "%s:%s%s %s", thisfn, prefix, child_name, G_OBJECT_TYPE_NAME( child ));
			if( GTK_IS_CONTAINER( child )){
				new_prefix = g_strdup_printf( "%s  ", prefix );
				my_utils_container_dump_rec( GTK_CONTAINER( child ), new_prefix );
				g_free( new_prefix );
			}
		}
	}

	g_list_free( children );
}

/**
 * my_utils_container_notes_setup_full:
 * @container: the #GtkContainer which contains the widget.
 * @widget_name: the name of the #GtkTextView widget.
 * @content: the current 'notes' content.
 * @editable: whether the widget must be set editable.
 *
 * Setup the @content in the widget and the editability status.
 *
 * This function is mostly used through the
 * my_utils_container_notes_init() macro when the data members of the
 * caller are normalized:
 * - the widget is named 'pn-notes'
 * - the source object is stored with its canonical name.
 *
 * Returns: the #GtkTextView widget.
 */
GtkWidget *
my_utils_container_notes_setup_full(
		GtkContainer *container, const gchar *widget_name, const gchar *notes, gboolean editable )
{
	GtkWidget *view;

	g_return_val_if_fail( container && GTK_IS_CONTAINER( container ), NULL );

	view = my_utils_container_get_child_by_name( container, widget_name );
	g_return_val_if_fail( view && GTK_IS_TEXT_VIEW( view ), NULL );

	my_utils_container_notes_setup_ex( GTK_TEXT_VIEW( view ), notes, editable );

	return( view );
}

/**
 * my_utils_container_notes_setup_ex:
 * @textview: the #GtkTextView widget.
 * @content: the current 'notes' content.
 * @editable: whether the widget must be set editable.
 *
 * Setup the @content in the widget and the editability status.
 */
void
my_utils_container_notes_setup_ex( GtkTextView *textview, const gchar *notes, gboolean editable )
{
	GtkTextBuffer *buffer;
	gchar *str;

	g_return_if_fail( textview && GTK_IS_TEXT_VIEW( textview ));

	str = g_strdup( my_strlen( notes ) ? notes : "" );
	buffer = gtk_text_buffer_new( NULL );
	gtk_text_buffer_set_text( buffer, str, -1 );
	g_free( str );

	gtk_text_view_set_buffer( textview, buffer );
	g_object_unref( buffer );

	my_utils_widget_set_editable( GTK_WIDGET( textview ), editable );

	if( editable ){
		g_signal_connect( G_OBJECT( buffer ), "changed", G_CALLBACK( on_notes_changed ), NULL );
	}
}

/*
 * notes are described as varchar(4096) in DDL
 * and tables are defined with UTF-8 encoding.
 * we so can expect that the count of chars in a GtkTextBuffer is
 * compatible with the DBMS...
 *
 * The limit is 4096 (MAX_LENGTH), including the terminating null
 * character. So count must be less or equal to MAX_LENGTH-1.
 * Start offset is counted from zero, so must be 0 to MAX_LENGTH-1.
 *
 * The 'in' flag prevent infinite recursion. It is common to all managed
 * GtkTextBuffers, but this is not an issue as the user only works on
 * one at a time.
 */
static void
on_notes_changed( GtkTextBuffer *buffer, void *empty )
{
	static const gchar *thisfn = "my_utils_on_notes_changed";
	static const gint MAX_LENGTH = 4096;
	static gboolean in = FALSE;
	gint count;
	GtkTextIter start, end;

	/* prevent an infinite recursion */
	if( !in ){
		count = gtk_text_buffer_get_char_count( buffer );
		if( count >= MAX_LENGTH ){
			/*
			 * this code works, but emit the following Gtk-Warning:
			 *
			 * Invalid text buffer iterator: either the iterator is
			 * uninitialized, or the characters/pixbufs/widgets in the
			 * buffer have been modified since the iterator was created.
			 * You must use marks, character numbers, or line numbers to
			 * preserve a position across buffer modifications.
			 * You can apply tags and insert marks without invalidating
			 * your iterators, but any mutation that affects 'indexable'
			 * buffer contents (contents that can be referred to by character
			 * offset) will invalidate all outstanding iterators
			 */
			gtk_text_buffer_get_iter_at_offset( buffer, &start, MAX_LENGTH-1 );
			/*gtk_text_iter_backward_char( &start );*/
			/*gtk_text_buffer_get_iter_at_offset( buffer, &end, count );*/
			gtk_text_buffer_get_end_iter( buffer, &end );
			/*gtk_text_iter_backward_char( &end );*/
			in = TRUE;
			g_debug( "%s: count=%d, start=%d, end=%d",
					thisfn, count, gtk_text_iter_get_offset( &start ), gtk_text_iter_get_offset( &end ));
			gtk_text_buffer_delete( buffer, &start, &end );
			in = FALSE;
		}
	}
}

/**
 * my_utils_container_updstamp_setup_full:
 * @container: the #GtkContainer container
 * @label_name: the name of the #GtkLabel widget in the @container
 * @stamp: the timestamp to be displayed
 * @user: the username to be displayed.
 *
 * This function is mostly used through the
 * "my_utils_container_updstamp_init()" macro.
 */
void
my_utils_container_updstamp_setup_full( GtkContainer *container,
								const gchar *label_name, const myStampVal *stamp, const gchar *user )
{
	GtkWidget *label;
	gchar *str_stamp, *str;

	label = my_utils_container_get_child_by_name( container, label_name );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	str_stamp = stamp ? my_stamp_to_str( stamp, MY_STAMP_YYMDHMS ) : g_strdup( "" );

	if( my_strlen( str_stamp ) && my_strlen( user )){
		str = g_strdup_printf( "%s (%s)", str_stamp, user );
	} else {
		str = g_strdup( _( "-none-" ));
	}

	gtk_label_set_text( GTK_LABEL( label ), str );

	g_free( str );
	g_free( str_stamp );
}

/**
 * my_utils_menu_get_menu_model:
 * @menu: the input menu model.
 * @id: the identifier of a link or an intem.
 * @pos: [out][allow-none]: if set, a placeholder which receives the
 *  position of the item in its menu.
 *
 * Returns: the #GMenuModel
 */
GMenuModel *
my_utils_menu_get_menu_model( GMenuModel *menu, const gchar *id, gint *pos )
{
	gint mypos;
	GMenuModel *model;

	g_return_val_if_fail( menu && G_IS_MENU_MODEL( menu ), NULL );

	mypos = 0;
	model = menu_get_model_rec( menu, id, &mypos );
	if( pos ){
		*pos = mypos;
	}

	return( model );
}

#if 0
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ed060, i=0, att_name=id, value=dossier
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ed060, i=0, att_name=label, value=_Dossier
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ed060, i=0, link_name=submenu, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0x82ae30, i=0, att_name=id, value=dossier-10
MY-DEBUG: my_utils_menu_get_model_rec: model=0x82ae30, i=0, link_name=section, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8eded0, i=0, att_name=id, value=dossier-10-new
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8eded0, i=0, att_name=action, value=app.new
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8eded0, i=0, att_name=label, value=_New...
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8eded0, i=0, att_name=accel, value=<Control>n
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8eded0, i=1, att_name=id, value=dossier-10-open
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8eded0, i=1, att_name=action, value=app.open
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8eded0, i=1, att_name=label, value=_Open...
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8eded0, i=1, att_name=accel, value=<Control>o
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8eded0, i=2, att_name=id, value=dossier-10-properties
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8eded0, i=2, att_name=action, value=win.properties
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8eded0, i=2, att_name=label, value=_Properties...
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8eded0, i=2, att_name=accel, value=<Control>i
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8eded0, i=3, att_name=id, value=dossier-10-backup
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8eded0, i=3, att_name=action, value=win.backup
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8eded0, i=3, att_name=label, value=_Backup...
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8eded0, i=3, att_name=accel, value=<Control>b
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8eded0, i=4, att_name=id, value=dossier-10-close
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8eded0, i=4, att_name=action, value=win.close
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8eded0, i=4, att_name=label, value=_Close
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8eded0, i=4, att_name=accel, value=<Control>w
MY-DEBUG: my_utils_menu_get_model_rec: model=0x82ae30, i=1, att_name=id, value=dossier-20
MY-DEBUG: my_utils_menu_get_model_rec: model=0x82ae30, i=1, link_name=section, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8fc750, i=0, att_name=id, value=dossier-20-manage
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8fc750, i=0, att_name=action, value=app.manage
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8fc750, i=0, att_name=label, value=_Manage...
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8fc750, i=0, att_name=accel, value=<Control>m
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8fc750, i=1, att_name=id, value=dossier-20-restore
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8fc750, i=1, att_name=action, value=app.restore
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8fc750, i=1, att_name=label, value=_Restore...
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8fc750, i=1, att_name=accel, value=<Control>r
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8fc750, i=2, att_name=id, value=dossier-20-recover
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8fc750, i=2, att_name=action, value=app.recover
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8fc750, i=2, att_name=label, value=Re_cover...
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8fc750, i=2, att_name=accel, value=<Control>c
MY-DEBUG: my_utils_menu_get_model_rec: model=0x82ae30, i=2, att_name=id, value=dossier-30
MY-DEBUG: my_utils_menu_get_model_rec: model=0x82ae30, i=2, link_name=section, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8fe210, i=0, att_name=id, value=dossier-30-prefs
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8fe210, i=0, att_name=action, value=app.user_prefs
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8fe210, i=0, att_name=label, value=_User preferences...
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8fe210, i=0, att_name=accel, value=<Control>u
MY-DEBUG: my_utils_menu_get_model_rec: model=0x82ae30, i=3, att_name=id, value=dossier-40
MY-DEBUG: my_utils_menu_get_model_rec: model=0x82ae30, i=3, link_name=section, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0x902670, i=0, att_name=id, value=dossier-40-quit
MY-DEBUG: my_utils_menu_get_model_rec: model=0x902670, i=0, att_name=action, value=app.quit
MY-DEBUG: my_utils_menu_get_model_rec: model=0x902670, i=0, att_name=label, value=_Quit
MY-DEBUG: my_utils_menu_get_model_rec: model=0x902670, i=0, att_name=accel, value=<Control>q
MY-DEBUG: my_utils_menu_get_model_rec: model=0x82ae30, i=4, att_name=id, value=dossier-99
MY-DEBUG: my_utils_menu_get_model_rec: model=0x82ae30, i=4, link_name=section, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ed060, i=1, att_name=id, value=operations
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ed060, i=1, att_name=label, value=_Operations
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ed060, i=1, link_name=submenu, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0x9042a0, i=0, att_name=id, value=operations-10
MY-DEBUG: my_utils_menu_get_model_rec: model=0x9042a0, i=0, link_name=section, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ffd90, i=0, att_name=id, value=operations-10-guided
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ffd90, i=0, att_name=action, value=win.guided
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ffd90, i=0, att_name=label, value=_Guided input...
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ffd90, i=1, att_name=id, value=operations-10-entries
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ffd90, i=1, att_name=action, value=win.entries
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ffd90, i=1, att_name=label, value=_View entries...
MY-DEBUG: my_utils_menu_get_model_rec: model=0x9042a0, i=1, att_name=id, value=operations-20
MY-DEBUG: my_utils_menu_get_model_rec: model=0x9042a0, i=1, link_name=section, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0x905d00, i=0, att_name=id, value=operations-20-settlement
MY-DEBUG: my_utils_menu_get_model_rec: model=0x905d00, i=0, att_name=action, value=win.settlement
MY-DEBUG: my_utils_menu_get_model_rec: model=0x905d00, i=0, att_name=label, value=Account _settlement...
MY-DEBUG: my_utils_menu_get_model_rec: model=0x905d00, i=1, att_name=id, value=operations-20-concil
MY-DEBUG: my_utils_menu_get_model_rec: model=0x905d00, i=1, att_name=action, value=win.concil
MY-DEBUG: my_utils_menu_get_model_rec: model=0x905d00, i=1, att_name=label, value=Account _reconciliation...
MY-DEBUG: my_utils_menu_get_model_rec: model=0x9042a0, i=2, att_name=id, value=operations-30
MY-DEBUG: my_utils_menu_get_model_rec: model=0x9042a0, i=2, link_name=section, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0x904f50, i=0, att_name=id, value=operations-30-ledclosing
MY-DEBUG: my_utils_menu_get_model_rec: model=0x904f50, i=0, att_name=action, value=win.ledclosing
MY-DEBUG: my_utils_menu_get_model_rec: model=0x904f50, i=0, att_name=label, value=_Ledgers closing...
MY-DEBUG: my_utils_menu_get_model_rec: model=0x904f50, i=1, att_name=id, value=operations-30-perclosing
MY-DEBUG: my_utils_menu_get_model_rec: model=0x904f50, i=1, att_name=action, value=win.perclosing
MY-DEBUG: my_utils_menu_get_model_rec: model=0x904f50, i=1, att_name=label, value=_Period closing...
MY-DEBUG: my_utils_menu_get_model_rec: model=0x904f50, i=2, att_name=id, value=operations-30-execlosing
MY-DEBUG: my_utils_menu_get_model_rec: model=0x904f50, i=2, att_name=action, value=win.execlosing
MY-DEBUG: my_utils_menu_get_model_rec: model=0x904f50, i=2, att_name=label, value=E_xercice closing...
MY-DEBUG: my_utils_menu_get_model_rec: model=0x9042a0, i=3, att_name=id, value=operations-40
MY-DEBUG: my_utils_menu_get_model_rec: model=0x9042a0, i=3, link_name=section, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0x90a190, i=0, att_name=id, value=operations-40-import
MY-DEBUG: my_utils_menu_get_model_rec: model=0x90a190, i=0, att_name=action, value=win.import
MY-DEBUG: my_utils_menu_get_model_rec: model=0x90a190, i=0, att_name=label, value=_Import...
MY-DEBUG: my_utils_menu_get_model_rec: model=0x90a190, i=1, att_name=id, value=operations-40-export
MY-DEBUG: my_utils_menu_get_model_rec: model=0x90a190, i=1, att_name=action, value=win.export
MY-DEBUG: my_utils_menu_get_model_rec: model=0x90a190, i=1, att_name=label, value=_Export...
MY-DEBUG: my_utils_menu_get_model_rec: model=0x9042a0, i=4, att_name=id, value=operations-99
MY-DEBUG: my_utils_menu_get_model_rec: model=0x9042a0, i=4, link_name=section, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ed060, i=2, att_name=id, value=print
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ed060, i=2, att_name=label, value=_Printings
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ed060, i=2, link_name=submenu, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa39800, i=0, att_name=id, value=print-10
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa39800, i=0, link_name=section, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa39950, i=0, att_name=id, value=print-10-balances
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa39950, i=0, att_name=action, value=win.render-balances
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa39950, i=0, att_name=label, value=_Entries balance...
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa39950, i=1, att_name=id, value=print-10-books
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa39950, i=1, att_name=action, value=win.render-books
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa39950, i=1, att_name=label, value=_Accounts book...
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa39950, i=2, att_name=id, value=print-10-ledbook
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa39950, i=2, att_name=action, value=win.render-ledgers-book
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa39950, i=2, att_name=label, value=_Ledgers book...
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa39950, i=3, att_name=id, value=print-10-ledsummary
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa39950, i=3, att_name=action, value=win.render-ledgers-summary
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa39950, i=3, att_name=label, value=Le_dgers summary...
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa39950, i=4, att_name=id, value=print-10-reconcil
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa39950, i=4, att_name=action, value=win.render-reconcil
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa39950, i=4, att_name=label, value=_Reconciliation...
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa39800, i=1, att_name=id, value=print-99
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa39800, i=1, link_name=section, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ed060, i=3, att_name=id, value=ref
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ed060, i=3, att_name=label, value=_References
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ed060, i=3, link_name=submenu, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30800, i=0, att_name=id, value=ref-10
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30800, i=0, link_name=section, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=0, att_name=id, value=ref-10-accounts
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=0, att_name=action, value=win.accounts
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=0, att_name=label, value=Chart of _Accounts...
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=1, att_name=id, value=ref-10-ledgers
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=1, att_name=action, value=win.ledgers
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=1, att_name=label, value=_Ledgers...
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=2, att_name=id, value=ref-10-ope-templates
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=2, att_name=action, value=win.ope-templates
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=2, att_name=label, value=Operation templates...
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=3, att_name=id, value=ref-10-currencies
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=3, att_name=action, value=win.currencies
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=3, att_name=label, value=_Currencies...
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=4, att_name=id, value=ref-10-rates
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=4, att_name=action, value=win.rates
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=4, att_name=label, value=_Rates...
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=5, att_name=id, value=ref-10-paimeans
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=5, att_name=action, value=win.paimeans
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=5, att_name=label, value=_Means of paiement...
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=6, att_name=id, value=ref-10-classes
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=6, att_name=action, value=win.classes
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30950, i=6, att_name=label, value=Account c_lasses...
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30800, i=1, att_name=id, value=ref-20
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30800, i=1, link_name=section, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0x90ca10, i=0, att_name=id, value=ref-20-batfiles
MY-DEBUG: my_utils_menu_get_model_rec: model=0x90ca10, i=0, att_name=action, value=win.batfiles
MY-DEBUG: my_utils_menu_get_model_rec: model=0x90ca10, i=0, att_name=label, value=I_mported transaction files...
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30800, i=2, att_name=id, value=ref-99
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa30800, i=2, link_name=section, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ed060, i=4, att_name=id, value=misc
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ed060, i=4, att_name=label, value=_Miscellaneous
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ed060, i=4, link_name=submenu, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0x957c70, i=0, att_name=id, value=misc-10
MY-DEBUG: my_utils_menu_get_model_rec: model=0x957c70, i=0, link_name=section, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0x954400, i=0, att_name=id, value=misc-10-plugins
MY-DEBUG: my_utils_menu_get_model_rec: model=0x954400, i=0, att_name=action, value=app.plugin_manage
MY-DEBUG: my_utils_menu_get_model_rec: model=0x954400, i=0, att_name=label, value=_Plugins manager...
MY-DEBUG: my_utils_menu_get_model_rec: model=0x957c70, i=1, att_name=id, value=misc-20
MY-DEBUG: my_utils_menu_get_model_rec: model=0x957c70, i=1, link_name=section, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0x954460, i=0, att_name=id, value=misc-20-balances
MY-DEBUG: my_utils_menu_get_model_rec: model=0x954460, i=0, att_name=action, value=win.chkbal
MY-DEBUG: my_utils_menu_get_model_rec: model=0x954460, i=0, att_name=label, value=Check _balances...
MY-DEBUG: my_utils_menu_get_model_rec: model=0x954460, i=1, att_name=id, value=misc-20-integrity
MY-DEBUG: my_utils_menu_get_model_rec: model=0x954460, i=1, att_name=action, value=win.integrity
MY-DEBUG: my_utils_menu_get_model_rec: model=0x954460, i=1, att_name=label, value=Check DBMS _integrity...
MY-DEBUG: my_utils_menu_get_model_rec: model=0x957c70, i=2, att_name=id, value=misc-99
MY-DEBUG: my_utils_menu_get_model_rec: model=0x957c70, i=2, link_name=section, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ed060, i=5, att_name=id, value=help
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ed060, i=5, att_name=label, value=Help
MY-DEBUG: my_utils_menu_get_model_rec: model=0x8ed060, i=5, link_name=submenu, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa20ca0, i=0, att_name=id, value=help-10
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa20ca0, i=0, link_name=section, link_value=GMenu
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa50830, i=0, att_name=id, value=help-10-about
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa50830, i=0, att_name=action, value=app.about
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa50830, i=0, att_name=label, value=_About Openbook...
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa20ca0, i=1, att_name=id, value=help-99
MY-DEBUG: my_utils_menu_get_model_rec: model=0xa20ca0, i=1, link_name=section, link_value=GMenu
#endif

static GMenuModel *
menu_get_model_rec( GMenuModel *model, const gchar *id, gint *pos )
{
	static const gchar *thisfn = "my_utils_menu_get_model_rec";
	GMenuLinkIter *link_iter;
	GMenuAttributeIter *att_iter;
	gint i;
	const gchar *att_name, *att_value, *link_name;
	GMenuModel *lv, *ret;
	GVariant *att_variant;
	gboolean found;

	/* iterate through items and attributes to find the searched identifier */
	found = FALSE;
	for( i=0 ; i<g_menu_model_get_n_items( model ) ; ++i ){

		att_iter = g_menu_model_iterate_item_attributes( model, i );
		while( !found && g_menu_attribute_iter_get_next( att_iter, &att_name, &att_variant )){
			att_value = g_variant_get_string( att_variant, NULL );
			if( 0 ){
				g_debug( "%s: model=%p, i=%d, att_name=%s, att_value=%s",
						thisfn, model, i, att_name, att_value );
			}
			found = my_collate( att_name, "id" ) == 0 && my_collate( att_value, id ) == 0;
			g_variant_unref( att_variant );
		}
		g_object_unref( att_iter );

		if( found ){
			*pos = i;
			return( model );
		}

		ret = NULL;
		link_iter = g_menu_model_iterate_item_links( model, i );
		while( !ret && g_menu_link_iter_get_next( link_iter, &link_name, &lv )){
			if( 0 ){
				g_debug( "%s: model=%p, i=%d, link_name=%s, link_value=%s",
						thisfn, model, i, link_name, G_OBJECT_TYPE_NAME( lv ));
			}
			ret = menu_get_model_rec( lv, id, pos );
			g_object_unref( lv );
		}
		g_object_unref( link_iter );
		if( ret ){
			return( ret );
		}
	}

	return( NULL );
}

/**
 * my_utils_size_group_add_size_group:
 * @target: the target #GtkSizeGroup
 * @source: the source #GtkSizeGroup.
 *
 * Add @target the widget referenced by @source.
 */
void
my_utils_size_group_add_size_group( GtkSizeGroup *target, GtkSizeGroup *source )
{
	g_return_if_fail( target && GTK_IS_SIZE_GROUP( target ));
	g_return_if_fail( source && GTK_IS_SIZE_GROUP( source ));

	GSList *widgets = gtk_size_group_get_widgets( source );
	GSList *it;

	for( it=widgets ; it ; it=g_slist_next( it )){
		gtk_size_group_add_widget( target, GTK_WIDGET( it->data ));
	}
}

/**
 * my_utils_widget_get_toplevel:
 * @widget:
 *
 * Returns: the toplevel #GtkWindow parent of the @widget, or %NULL.
 */
GtkWindow *
my_utils_widget_get_toplevel( GtkWidget *widget )
{
	GtkWidget *parent;

	parent = NULL;
	if( widget ){
		parent = gtk_widget_get_toplevel( widget );
	}
	return( parent ? GTK_WINDOW( parent ) : NULL );
}

/**
 * my_utils_widget_set_editable:
 * @widget: the #GtkWdiget.
 * @editable: whether the @widget is editable or not.
 *
 * Try to set a visual indication of whether the @widget is editable or not.
 *
 * Having a GtkWidget should be enough, but we also deal with a GtkTreeViewColumn.
 * So the most-bottom common ancestor is just GObject (since GtkObject having been
 * deprecated in Gtk+-3.0)
 *
 * Note that using 'sensitivity' property is just a work-around because the
 * two things have distinct semantics:
 * - editable: whether we are allowed to modify the value (is not read-only)
 * - sensitive: whether the value is relevant (has a sense in this context)
 *
 * pwi 2017- 1-30: do not unsensitive a widget if editable is %TRUE
 * (because they default to be created as if editable=%TRUE)
 */
void
my_utils_widget_set_editable( GtkWidget *widget, gboolean editable )
{
	GList *columns, *it;
	GList *renderers, *irender;
	GtkWidget *editable_widget;

	g_return_if_fail( widget && GTK_IS_WIDGET( widget ));

	/*g_debug( "%s: GTK_IS_BUTTON=%s",
			G_OBJECT_TYPE_NAME( widget ), GTK_IS_BUTTON( widget ) ? "True":"False" );*/

	/* GtkComboBoxEntry is deprecated from Gtk+3
	 * see. http://git.gnome.org/browse/gtk+/commit/?id=9612c648176378bf237ad0e1a8c6c995b0ca7c61
	 * while 'has_entry' property exists since 2.24
	 */
	editable_widget = widget;
	if( GTK_IS_COMBO_BOX( widget ) && gtk_combo_box_get_has_entry( GTK_COMBO_BOX( widget ))){
		editable_widget = gtk_bin_get_child( GTK_BIN( widget ));
	}

	/* can focus */
	if( GTK_IS_WIDGET( widget )){
		g_object_set( G_OBJECT( widget ), "can-focus", editable, NULL );
	}
	if( editable_widget != widget && GTK_IS_WIDGET( editable_widget )){
		g_object_set( G_OBJECT( editable_widget ), "can-focus", editable, NULL );
	}

	/* set editable */
	if( GTK_IS_EDITABLE( widget )){
		gtk_editable_set_editable( GTK_EDITABLE( widget ), editable );
	}
	if( editable_widget != widget && GTK_IS_WIDGET( editable_widget )){
		gtk_editable_set_editable( GTK_EDITABLE( editable_widget ), editable );
	}

	/* others */
	if( GTK_IS_BUTTON( widget )){
		if( !editable ){
			gtk_widget_set_sensitive( widget, editable );
		}

	} else if( GTK_IS_COMBO_BOX( widget )){
		if( !editable ){
			gtk_combo_box_set_button_sensitivity(
					GTK_COMBO_BOX( widget ), editable ? GTK_SENSITIVITY_ON : GTK_SENSITIVITY_OFF );
		}

	} else if( GTK_IS_ENTRY( widget )){
		if( !editable ){
			gtk_widget_set_sensitive( widget, editable );
		}

	} else if( GTK_IS_FRAME( widget )){
		if( !editable ){
			gtk_widget_set_sensitive( widget, editable );
		}

	} else if( GTK_IS_TEXT_VIEW( widget )){
		gtk_text_view_set_editable( GTK_TEXT_VIEW( widget ), editable );
		if( !editable ){
			my_style_add( widget, "textviewinsensitive" );
		}

	} else if( GTK_IS_TREE_VIEW( widget )){
		columns = gtk_tree_view_get_columns( GTK_TREE_VIEW( widget ));
		for( it=columns ; it ; it=it->next ){
			renderers = gtk_cell_layout_get_cells( GTK_CELL_LAYOUT( GTK_TREE_VIEW_COLUMN( it->data )));
			for( irender = renderers ; irender ; irender = irender->next ){
				if( GTK_IS_CELL_RENDERER_TEXT( irender->data )){
					g_object_set( G_OBJECT( irender->data ), "editable", editable, "editable-set", TRUE, NULL );
				}
			}
			g_list_free( renderers );
		}
		g_list_free( columns );
	}
}

/**
 * my_utils_widget_set_margins:
 * @widget:
 * @top:
 * @bottom:
 * @left:
 * @right:
 *
 * Set the desired margins on the given @widget.
 */
void
my_utils_widget_set_margins( GtkWidget *widget, guint top, guint bottom, guint left, guint right )
{
	g_return_if_fail( widget && GTK_IS_WIDGET( widget ));

	gtk_widget_set_margin_top( widget, top );
	gtk_widget_set_margin_bottom( widget, bottom );
	my_utils_widget_set_margin_left( widget, left );
	my_utils_widget_set_margin_right( widget, right );
}

/**
 * my_utils_widget_set_margin_left:
 * @widget:
 * @left:
 *
 * Set the desired left/start margin on the given @widget.
 */
void
my_utils_widget_set_margin_left( GtkWidget *widget, guint left )
{
	g_return_if_fail( widget && GTK_IS_WIDGET( widget ));

#if GTK_MAJOR_VERSION > 3 || GTK_MINOR_VERSION >= 12
		gtk_widget_set_margin_start( widget, left );
#else
		gtk_widget_set_margin_left( widget, left );
#endif
}

/**
 * my_utils_widget_set_margin_right:
 * @widget:
 * @left:
 *
 * Set the desired right/end margin on the given @widget.
 */
void
my_utils_widget_set_margin_right( GtkWidget *widget, guint right )
{
	g_return_if_fail( widget && GTK_IS_WIDGET( widget ));

#if GTK_MAJOR_VERSION > 3 || GTK_MINOR_VERSION >= 12
		gtk_widget_set_margin_end( widget, right );
#else
		gtk_widget_set_margin_right( widget, right );
#endif
}

/**
 * my_utils_widget_set_xalign:
 * @widget:
 * @xalign:
 *
 * Set the desired horizontal alignment on the given @widget.
 */
void
my_utils_widget_set_xalign( GtkWidget *widget, gfloat xalign )
{
	g_return_if_fail( widget && GTK_IS_WIDGET( widget ));

#if GTK_MAJOR_VERSION > 3 || GTK_MINOR_VERSION >= 16
		g_object_set( widget, "xalign", xalign, NULL );
#else
		if( GTK_IS_MISC( widget )){
			gtk_misc_set_alignment( GTK_MISC( widget ), xalign, 0.5 );
		}
#endif
}

/**
 * my_utils_output_stream_new:
 * @filename: the UTF-8 encoded output filename
 */
gboolean
my_utils_output_stream_new( const gchar *uri, GFile **file, GOutputStream **stream )
{
	static const gchar *thisfn = "my_utils_output_stream_new";
	GError *error;
	gchar *sysfname;

	g_return_val_if_fail( my_strlen( uri ), FALSE );
	g_return_val_if_fail( file, FALSE );
	g_return_val_if_fail( stream, FALSE );

	error = NULL;
	sysfname = my_utils_filename_from_utf8( uri );
	if( !sysfname ){
		return( FALSE );
	}
	*file = g_file_new_for_uri( sysfname );
	g_free( sysfname );

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
 * my_utils_input_stream_new:
 * @filename: the UTF-8 encoded input filename
 */
gboolean
my_utils_input_stream_new( const gchar *filename, GFile **file, GInputStream **stream )
{
	static const gchar *thisfn = "my_utils_input_stream_new";
	GError *error;
	gchar *sysfname;

	g_return_val_if_fail( my_strlen( filename ), FALSE );
	g_return_val_if_fail( file, FALSE );
	g_return_val_if_fail( stream, FALSE );

	error = NULL;
	sysfname = my_utils_filename_from_utf8( filename );
	if( !sysfname ){
		return( FALSE );
	}
	*file = g_file_new_for_path( sysfname );
	g_free( sysfname );

	*stream = ( GInputStream * ) g_file_read( *file, NULL, &error );
	if( !*stream ){
		g_warning( "%s: g_file_read: %s", thisfn, error->message );
		g_error_free( error );
		g_object_unref( *file );
		*file = NULL;
		return( FALSE );
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
		g_warning( "%s: not a valid UTF-8 string: %s", thisfn, pango_layout_get_text( layout ));
	}
	g_free( strref );
}

/**
 * my_utils_window_position_get:
 * @settings: the #myISettings implementation which holds these settings.
 * @name: the prefix of the key in the settings; the actual key name
 *  will be "@name-pos".
 * @x: [out]: the x position.
 * @y: [out]: the y position.
 * @width: [out]: the width.
 * @height: [out]: the height.
 *
 * Set @x, @y, @width and @height.
 *
 * Returns: %TRUE if a position has been actually found in the settings
 * file.
 */
gboolean
my_utils_window_position_get( myISettings *settings, const gchar *name, gint *x, gint *y, gint *width, gint *height )
{
	gboolean set;

	set = position_get_geometry( settings, name, x, y, width, height );

	return( set );
}

/**
 * my_utils_window_position_restore:
 * @toplevel: a #GtkWindow whose size and position has to be restored
 *  from the settings.
 * @settings: the #myISettings implementation which holds these settings.
 * @name: the prefix of the key in the settings; the actual key name
 *  will be "@name-pos".
 *
 * Returns: %TRUE if size and position have been set, %FALSE else.
 */
gboolean
my_utils_window_position_restore( GtkWindow *toplevel, myISettings *settings, const gchar *name )
{
	gint x=0, y=0, width=0, height=0;
	gboolean set;

	set = position_get_geometry( settings, name, &x, &y, &width, &height );

	if( set ){
		gtk_window_move( toplevel, x, y );
		gtk_window_resize( toplevel, width, height );
	}

	return( set );
}

/**
 * my_utils_window_position_save:
 * @toplevel: a #GtkWindow whose size and position has to be saved to
 *  the settings.
 * @settings: the #myISettings implementation which holds these settings.
 * @name: the prefix of the key in the settings; the actual key name
 *  will be "@name-pos".
 *
 * Save size and position of @toplevel to @settings.
 */
void
my_utils_window_position_save( GtkWindow *toplevel, myISettings *settings, const gchar *name )
{
	gint x, y, width, height;

	gtk_window_get_position( toplevel, &x, &y );
	gtk_window_get_size( toplevel, &width, &height );
	position_save_geometry( settings, name, x, y, width, height );
}

/**
 * my_utils_window_position_save_pos_only:
 * @toplevel: a #GtkWindow whose size and position has to be saved to
 *  the settings.
 * @settings: the #myISettings implementation which holds these settings.
 * @name: the prefix of the key in the settings; the actual key name
 *  will be "@name-pos".
 *
 * Save only current position of @toplevel to @settings.
 * The currently saved size is kept.
 */
void
my_utils_window_position_save_pos_only( GtkWindow *toplevel, myISettings *settings, const gchar *name )
{
	gint x, y, width, height, prev_x, prev_y;

	position_get_geometry( settings, name, &prev_x, &prev_y, &width, &height );
	gtk_window_get_position( toplevel, &x, &y );
	position_save_geometry( settings, name, x, y, width, height );
}

/*
 * returns the int list which defines the window geometry, or %NULL if not found
 */
static gboolean
position_get_geometry( myISettings *settings, const gchar *name, gint *x, gint *y, gint *width, gint *height )
{
	static const gchar *thisfn = "my_utils_position_get_geometry";
	gchar *key;
	GList *list, *it;
	gboolean set;

	g_assert( x );
	g_assert( y );
	g_assert( width );
	g_assert( height );

	key = position_get_key( name );
	list = my_isettings_get_uint_list( settings, st_save_restore_group, key );
	g_free( key );

	set = ( g_list_length( list ) > 0 );

	if( set ){
		it = list;
		*x = GPOINTER_TO_UINT( it->data );

		it = it->next;
		*y = GPOINTER_TO_UINT( it->data );

		it = it->next;
		*width = GPOINTER_TO_UINT( it->data );

		it = it->next;
		*height = GPOINTER_TO_UINT( it->data );

	} else {
		*x = -1;
		*y = -1;
		*width = -1;
		*height = -1;
	}

	g_debug( "%s: list=%p (count=%d) x=%d, y=%d, width=%d, height=%d",
			thisfn, ( void * ) list, g_list_length( list ), *x, *y, *width, *height );

	my_isettings_free_uint_list( settings, list );

	return( set );
}

/*
 * returns the int list which defines the window geometry, or %NULL if not found
 */
static void
position_save_geometry( myISettings *settings, const gchar *name, gint x, gint y, gint width, gint height )
{
	static const gchar *thisfn = "my_utils_position_save_geometry";
	gchar *key, *str;

	g_debug( "%s: settings=%p, name=%s, x=%d, y=%d, width=%d, height=%d",
			thisfn, ( void * ) settings, name, x, y, width, height );

	key = position_get_key( name );
	str = g_strdup_printf( "%d;%d;%d;%d;", x, y, width, height );
	my_isettings_set_string( settings, st_save_restore_group, key, str );
	g_free( str );
	g_free( key );
}

/*
 * position_get_key:
 * @name: the prefix of the key in the settings; the actual key name
 *  will be "@name-pos".
 *
 * Returns: the actual key to be used, as a newly allocated string
 * which should be g_free() by the user.
 */
static gchar *
position_get_key( const gchar *name )
{
	gchar *key;

	key = g_strdup_printf( "%s-pos", name );

	return( key );
}

/**
 * my_utils_window_position_get_has_pos:
 * @settings: the #myISettings implementation which holds these settings.
 * @name: the prefix of the key in the settings; the actual key name
 *  will be "@name-pos".
 *
 * Returns: %TRUE if this @name already has a saved size-position.
 */
gboolean
my_utils_window_position_get_has_pos( myISettings *settings, const gchar *name )
{
	gboolean has_value;
	gchar *key;

	has_value = FALSE;

	key = position_get_key( name );
	has_value = my_isettings_has_key( settings, st_save_restore_group, key );
	g_free( key );

	return( has_value );
}

/**
 * my_utils_file_exists:
 * @filename: a file pathname
 *
 * Returns: %TRUE if the specified file exists, %FALSE else.
 *
 * This function doesn't distinguish between files and directories
 * (as in "all in file in Unix") - so if you really want a *file* then
 * rather take a glance at #my_utils_is_readable_file() function.
 *
 * The caller should be conscious and take care of the usual race
 * condition: anything may happen between this test and the actual
 * use of its result...
 */
gboolean
my_utils_file_exists( const gchar *filename )
{
	GFile *file;
	gboolean exists;
	gchar *sysfname;

	exists = FALSE;

	sysfname = my_utils_filename_from_utf8( filename );
	if( sysfname ){
		file = g_file_new_for_path( sysfname );
		exists = g_file_query_exists( file, NULL );
		g_object_unref( file );
	}
	g_free( sysfname );

	g_debug( "my_utils_file_exists: the file '%s' exists: %s", filename, exists ? "True":"False" );
	return( exists );
}

/**
 * my_utils_file_is_readable:
 * @filename: a file pathname
 *
 * Returns: %TRUE if the specified file exists, is a file and is readable,
 *  %FALSE else.
 *
 * The caller should be conscious and take care of the usual race
 * condition: anything may happen between this test and the actual
 * use of its result...
 */
gboolean
my_utils_file_is_readable( const gchar *filename )
{
	GFile *file;
	gboolean ok;
	gchar *sysfname;

	ok = FALSE;

	sysfname = my_utils_filename_from_utf8( filename );
	if( sysfname ){
		file = g_file_new_for_path( sysfname );
		ok = is_readable_gfile( file );
		g_object_unref( file );
	}
	g_free( sysfname );

	g_debug( "my_utils_file_is_readable: filename=%s, ok=%s", filename, ok ? "True":"False" );
	return( ok );
}

/**
 * my_utils_filename_from_utf8:
 */
gchar *
my_utils_filename_from_utf8( const gchar *filename )
{
	gchar *sysfname;
	GError *error;
	gchar *str;

	error = NULL;
	sysfname = g_filename_from_utf8( filename, -1, NULL, NULL, &error );
	if( !sysfname ){
		str = g_strdup_printf(
					_( "Unable to convert '%s' filename to filesystem encoding: %s" ),
					filename, error->message );
		my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );
		g_free( str );
		g_error_free( error );
	}

	return( sysfname );
}

/**
 * my_utils_uri_exists:
 * @uri: an uri pathname
 *
 * Returns: %TRUE if the specified uri exists, %FALSE else.
 *
 * This function doesn't distinguish between uris and directories
 * (as in "all is file in Unix") - so if you really want a *uri* then
 * rather take a glance at #my_utils_is_readable_uri() function.
 *
 * The caller should be conscious and take care of the usual race
 * condition: anything may happen between this test and the actual
 * use of its result...
 */
gboolean
my_utils_uri_exists( const gchar *uri )
{
	GFile *file;
	gboolean exists;
	gchar *sysfname;

	exists = FALSE;

	sysfname = my_utils_filename_from_utf8( uri );
	if( sysfname ){
		file = g_file_new_for_uri( sysfname );
		exists = g_file_query_exists( file, NULL );
		g_object_unref( file );
	}
	g_free( sysfname );

	g_debug( "my_utils_uri_exists: the uri '%s' exists: %s", uri, exists ? "True":"False" );

	return( exists );
}

/**
 * my_utils_uri_get_content:
 * @uri:
 * @from_codeset: [allow-none]: source codeset.
 * @errors: [allow-none][out]: error count.
 * @msgerr: [allow-none][out]: error message placeholder;
 *  if %NULL, error messages are displayed in a warning dialog box.
 *
 * Returns: the file content as a single, null-terminated, buffer of
 * UTF-8 chars, which should be g_free() by the caller.
 *
 * The function returns the unconverted buffer if the conversion cannot
 * be made.
 */
gchar *
my_utils_uri_get_content( const gchar *uri, const gchar *from_codeset, guint *errors, gchar **msgerr )
{
	GFile *gfile;
	gchar *sysfname, *content, *str, *temp, *buffer;
	GError *error;
	gsize read_length;
	glong loaded_len, computed_len, i;

	if( errors ){
		*errors = 0;
	}
	if( msgerr ){
		*msgerr = NULL;
	}

	sysfname = my_utils_filename_from_utf8( uri );
	if( !sysfname ){
		if( errors ){
			*errors += 1;
		}
		str = g_strdup_printf( _( "Unable to get a system filename for '%s' URI" ), uri );
		if( msgerr ){
			*msgerr = str;
		} else {
			my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );
			g_free( str );
		}
		return( NULL );
	}

	gfile = g_file_new_for_uri( sysfname );
	g_free( sysfname );

	error = NULL;
	content = NULL;
	if( !g_file_load_contents( gfile, NULL, &content, &read_length, NULL, &error )){
		if( errors ){
			*errors += 1;
		}
		str = g_strdup_printf( _( "Unable to load content from '%s' file: %s" ), uri, error->message );
		if( msgerr ){
			*msgerr = str;
		} else {
			my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );
			g_free( str );
		}
		g_error_free( error );
		g_free( content );
		content = NULL;
	}

	g_object_unref( gfile );

	/* BE VERY CAUTIOUS ABOUT THAT AS WE DO IGNORE WHICH IS THE LOADED CHARSET */
	/* if the returned length is greater that the computed one,
	 * it is probable that we are facing a badly-formatted content
	 * so remove '\00' chars */
	if( 0 ){
		loaded_len = ( glong ) read_length;
		computed_len = g_utf8_strlen( content, read_length );

		/*g_debug( "contents='%s'", contents );
		g_debug( "length=%d, strlen(-1)=%ld, strlen(length)=%ld",
				( gint ) length, g_utf8_strlen( contents, -1 ), g_utf8_strlen( contents, ( gint ) length ));*/

		if( loaded_len > computed_len ){
			for( i=computed_len ; content[i] == '\0' && i<loaded_len ; ++i ){
				/*g_debug( "i=%ld, contents[i]=%x", i, contents[i] );*/
				;
			}
			buffer = g_strdup_printf( "%s%s", content, content+i );
			g_free( content );
			content = buffer;
		}
	}

	/* convert to UTF-8 if needed */
	if( content && from_codeset && my_collate( from_codeset, "UTF-8" )){
		temp = g_convert( content, -1, "UTF-8", from_codeset, NULL, NULL, &error );
		if( !temp ){
			if( errors ){
				*errors += 1;
			}
			str = g_strdup_printf( _( "Unable to convert from %s to UTF-8 the '%s' file content: %s"),
					from_codeset, uri, error->message );
			if( msgerr ){
				*msgerr = str;
			} else {
				my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );
				g_free( str );
			}
		} else {
			g_free( content );
			content = temp;
		}
	}

	return( content );
}

/**
 * my_utils_uri_get_lines:
 * @uri: the URI of the input stream.
 * @from_codeset: [allow-none]: source codeset.
 * @errors: [allow-none][out]:
 * @msgerr: [allow-none][out]: error message placeholder;
 *  if %NULL, error messages are displayed in a warning dialog box.
 *
 * Returns: the file content as a #GSList list of UTF-8 null-terminated
 * strings, which should be  #g_slist_free_full( list, ( GDestroyNotify ) g_free )
 * by the caller.
 */
GSList *
my_utils_uri_get_lines( const gchar *uri, const gchar *from_codeset, guint *errors, gchar **msgerr )
{
	gchar *content;
	GSList *lines;

	/* load file content in memory + charmap conversion */
	if( msgerr ){
		*msgerr = NULL;
	}
	if( errors ){
		*errors = 0;
	}

	content = my_utils_uri_get_content( uri, from_codeset, errors, msgerr );

	if( errors && *errors ){
		g_free( content );
		return( NULL );
	}

	/* convert buffer content to list of lines */
	lines = content ? split_by_line( content ) : NULL;

	g_free( content );

	return( lines );
}

/*
 * split the content into a GSList of lines
 * taking into account the possible backslashed end-of-lines
 *
 * Note
 * ----
 * In some files, all lines but the last are 0x0a-terminated.
 * In some other files, really all lines, including the last one, are
 * line-feed terminated.
 * As a consequence the split-on-end-of-line below would may introduce
 * a last empty line in the second case due to the g_strsplit() behavior.
 * We get rid here of this last empty line.
 */
static GSList *
split_by_line( const gchar *content )
{
	GSList *eol_list;
	gchar **lines, **it_line;
	gchar *prev, *temp;
	guint numline;

	/* split on end-of-line
	 * then re-concatenate segments when end-of-line was backslashed */
	lines = g_strsplit( content, "\n", -1 );
	it_line = lines;
	eol_list = NULL;
	prev = NULL;
	numline = 0;

	while( *it_line ){
		if( prev ){
			temp = g_utf8_substring( prev, 0, my_strlen( prev )-1 );
			g_free( prev );
			prev = temp;
			temp = g_strconcat( prev, "\n", *it_line, NULL );
			g_free( prev );
			prev = temp;
		} else {
			prev = g_strdup( *it_line );
		}
		if( prev && !g_str_has_suffix( prev, "\\" )){
			numline += 1;
			eol_list = g_slist_prepend( eol_list, g_strdup( prev ));
			g_free( prev );
			prev = NULL;
		}
		it_line++;
	}

	g_strfreev( lines );

	/* remove possible empty last line(s) */
	while( eol_list && !my_strlen( eol_list->data )){
		eol_list = eol_list->next;
	}

	return( g_slist_reverse( eol_list ));
}

/**
 * my_utils_uri_is_dir:
 * @uri: an URI
 *
 * Returns: %TRUE if the specified URI is a directory.
 *
 * The caller should be conscious and take care of the usual race
 * condition: anything may happen between this test and the actual
 * use of its result...
 */
gboolean
my_utils_uri_is_dir( const gchar *uri )
{
	GFile *file;
	gboolean ok;
	gchar *sysfname;

	ok = FALSE;

	sysfname = my_utils_filename_from_utf8( uri );
	if( sysfname ){
		file = g_file_new_for_uri( sysfname );
		ok = is_dir( file );
		g_object_unref( file );
	}
	g_free( sysfname );

	g_debug( "my_utils_uri_is_dir: uri=%s, ok=%s", uri, ok ? "True":"False" );
	return( ok );
}

static gboolean
is_dir( GFile *file )
{
	gboolean ok;
	GFileType type;

	ok = FALSE;

	type = g_file_query_file_type( file, G_FILE_QUERY_INFO_NONE, NULL );
	ok = ( type == G_FILE_TYPE_DIRECTORY );

	return( ok );
}

/**
 * my_utils_uri_is_readable:
 * @uri: a file URI
 *
 * Returns: %TRUE if the specified file exists, is a file and is readable,
 *  %FALSE else.
 *
 * The caller should be conscious and take care of the usual race
 * condition: anything may happen between this test and the actual
 * use of its result...
 */
gboolean
my_utils_uri_is_readable( const gchar *uri )
{
	GFile *file;
	gboolean ok;
	gchar *sysfname;

	ok = FALSE;

	sysfname = my_utils_filename_from_utf8( uri );
	if( sysfname ){
		file = g_file_new_for_uri( sysfname );
		ok = is_readable_gfile( file );
		g_object_unref( file );
	}
	g_free( sysfname );

	g_debug( "my_utils_uri_is_readable: uri=%s, ok=%s", uri, ok ? "True":"False" );
	return( ok );
}

static gboolean
is_readable_gfile( GFile *file )
{
	gboolean ok;
	GFileInfo *info;
	GFileType type;

	ok = FALSE;

	info = g_file_query_info( file,
			G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_ACCESS_CAN_READ,
			G_FILE_QUERY_INFO_NONE, NULL, NULL );
	if( info ){
		ok = TRUE;
		type = g_file_info_get_attribute_uint32( info, G_FILE_ATTRIBUTE_STANDARD_TYPE );
		ok &= ( type == G_FILE_TYPE_REGULAR );
		ok &= g_file_info_get_attribute_boolean( info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ );
		g_object_unref( info );
	}

	return( ok );
}

/**
 * my_utils_uri_get_extension:
 * @uri: an URI.
 * @make_lower: whether to force the result to lowercase.
 *
 * Returns: the extension of the URI as a newly allocated string which
 * should be #g_free() by the caller (e.g. ".gz")
 */
gchar *
my_utils_uri_get_extension( const gchar *uri, gboolean make_lower )
{
	GRegex *regex;
	gchar *extension, *reverse;

	extension = NULL;

	if( my_strlen( uri )){
		reverse = g_utf8_strreverse( uri, -1 );
		regex = g_regex_new( "\\..*$", 0, 0, NULL );
		extension = g_regex_replace( regex, reverse, -1, 0, ".", 0, NULL );
		g_regex_unref( regex );
		g_free( reverse );
		reverse = g_utf8_strreverse( extension, -1 );
		g_free( extension );
		extension = reverse;

		if( make_lower ){
			reverse = g_utf8_strdown( extension, -1 );
			g_free( extension );
			extension = reverse;
		}
	}

	return( extension );
}

/**
 * my_utils_action_enable:
 * @map: the #GActionMap (main window or application)
 * @action: [allow-none]: a pointer to a #GSimpleAction pointer which may
 *  store the action to prevent the lookup next time the function is
 *  called
 * @name: [allow-none]: the action name, may be %NULL if @action is set
 * @enable: whether enable the menu item
 *
 * Enable the menu item.
 */
void
my_utils_action_enable( GActionMap *map, GSimpleAction **action, const gchar *name, gboolean enable )
{
	static const gchar *thisfn = "my_utils_action_enable";
	GAction *local_action;

	g_debug( "%s: map=%p, action=%p, name=%s, enable=%s",
			thisfn, ( void * ) map, ( void * ) action, name, enable ? "True":"False" );

	g_return_if_fail( map && G_IS_ACTION_MAP( map ));
	g_return_if_fail( action || my_strlen( name ));

	if( !action || !*action ){
		local_action = g_action_map_lookup_action( map, name );
		g_return_if_fail( local_action && G_IS_SIMPLE_ACTION( local_action ));
		if( action ){
			*action = G_SIMPLE_ACTION( local_action );
		}
	} else {
		local_action = G_ACTION( *action );
	}

	g_simple_action_set_enabled( G_SIMPLE_ACTION( local_action ), enable );
}
