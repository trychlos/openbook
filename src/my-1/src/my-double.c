/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "my/my-double.h"
#include "my/my-utils.h"

/* we have to deal with:
 * - from locale to prefs display (this is needed so that we do not
 *   have to try to insert spaces between thousand digits)
 * - from prefs display to brut editable
 */
static gunichar     st_locale_thousand_sep   = '\0';
static gunichar     st_locale_decimal_sep    = '\0';
static GRegex      *st_locale_thousand_regex = NULL;
static GRegex      *st_locale_decimal_regex  = NULL;
static GRegex      *st_inter_regex           = NULL;
static const gchar *st_inter                 = "|";

static void   double_set_locale( void );
static gchar *double_decorate( const gchar *text, gunichar thousand_sep, gunichar decimal_sep );

/**
 * my_double_is_zero:
 * @value:
 * @decimal_digits:
 *
 * Returns: %TRUE if @value is zero with respect to @decimal_digits.
 */
gboolean
my_double_is_zero( gdouble value, gint decimal_digits )
{
	gdouble precision;
	gboolean zero;

	precision = 1.0 / exp10( decimal_digits );
	zero = ( fabs( value ) < precision );

	return( zero );
}

/**
 * my_double_undecorate:
 * @text:
 * @thousand_sep:
 * @decimal_sep:
 *
 * Remove from the given string all decoration added for the display
 * of a double, returning so a 'brut' double string:
 * - without any thousand separator,
 * - with a dot as the decimal sep.
 *
 * This is a "prefs to brut editable" transformation, suitable for
 * #g_strtod() interpretation.
 *
 * Returns: a newly allocated string which should be g_free() by the
 * caller.
 */
gchar *
my_double_undecorate( const gchar *text, gunichar thousand_sep, gunichar decimal_sep )
{
	gchar *dest1, *dest2;
	GRegex *regex;
	gchar *str;

	dest1 = g_strdup( text );

	/* remove the specified thousand separator */
	if( thousand_sep != '\0' ){
		str = thousand_sep == '.' ? g_strdup( "\\." ) : g_strdup_printf( "%c", thousand_sep );
		regex = g_regex_new( str, 0, 0, NULL );
		dest2 = g_regex_replace_literal( regex, dest1, -1, 0, "", 0, NULL );
		g_free( dest1 );
		dest1 = dest2;
		g_regex_unref( regex );
		g_free( str );
	}

	/* replace the specified decimal separator with a dot '.' */
	if( decimal_sep != '.' ){
		str = g_strdup_printf( "%c", decimal_sep );
		regex = g_regex_new( str, 0, 0, NULL );
		dest2 = g_regex_replace_literal( regex, dest1, -1, 0, ".", 0, NULL );
		g_free( dest1 );
		dest1 = dest2;
		g_regex_unref( regex );
		g_free( str );
	}

	return( dest1 );
}

/*
 * when run for the first time, evaluate the thousand separator and the
 * decimal separator for the current locale
 *
 * they are those which will be outputed by printf(),
 * and that g_ascii_strtod() is able to sucessfully parse.
 */
static void
double_set_locale( void )
{
	static const gchar *thisfn = "my_double_set_locale";
	gchar *str, *p, *srev;

	if( !st_locale_thousand_sep ){
		str = g_strdup_printf( "%'.1lf", 1000.0 );
		p = g_utf8_next_char( str );
		st_locale_thousand_sep = g_utf8_get_char( p );
		srev = g_utf8_strreverse( str, -1 );
		p = g_utf8_next_char( srev );
		st_locale_decimal_sep = g_utf8_get_char( p );

		g_debug( "%s: locale_thousand_sep='%c', locale_decimal_sep='%c'",
						thisfn,
						st_locale_thousand_sep, st_locale_decimal_sep );

		str = g_strdup_printf( "\\%c", st_locale_thousand_sep );
		st_locale_thousand_regex = g_regex_new( str, 0, 0, NULL );
		g_free( str );

		str = g_strdup_printf( "\\%c", st_locale_decimal_sep );
		st_locale_decimal_regex = g_regex_new( str, 0, 0, NULL );
		g_free( str );

		str = g_strdup_printf( "\\%s", st_inter );
		st_inter_regex = g_regex_new( str, 0, 0, NULL );
		g_free( str );
	}
}

/**
 * my_double_set_from_csv:
 *
 * Returns a double from the imported field
 */
gdouble
my_double_set_from_csv( const gchar *sql_string, gchar decimal_sep )
{
	gchar *str;
	gint i;
	gdouble amount;

	if( !my_strlen( sql_string )){
		return( 0.0 );
	}

	str = g_strdup( sql_string );

	if( decimal_sep != '.' ){
		for( i=0 ; str[i] ; ++i ){
			if( str[i] == decimal_sep ){
				str[i] = '.';
				break;
			}
		}
	}

	amount = my_double_set_from_sql( str );

	g_free( str );

	return( amount );
}

/**
 * my_double_set_from_sql:
 *
 * Returns a double from the specified SQL-stringified decimal
 *
 * The input string is not supposed to be localized, nor decorated.
 */
gdouble
my_double_set_from_sql( const gchar *sql_string )
{
	return( my_double_set_from_sql_ex( sql_string, 5 ));
}

/**
 * my_double_set_from_sql_ex:
 *
 * Returns a double from the specified SQL-stringified decimal
 *
 * The input string is not supposed to be localized, nor decorated.
 */
gdouble
my_double_set_from_sql_ex( const gchar *sql_string, gint digits )
{
	gdouble amount;
	gdouble precision;

	if( !my_strlen( sql_string )){
		return( 0.0 );
	}

	precision = exp10( digits );
	amount = g_ascii_strtod( sql_string, NULL );
	amount = round( amount*precision ) / precision;

	return( amount );
}

/**
 * my_double_set_from_str:
 * @string:
 * @thousand_sep:
 * @decimal_sep:
 *
 * Parse the @string which is expected to be a decorated double.
 */
gdouble
my_double_set_from_str( const gchar *string, gunichar thousand_sep, gunichar decimal_sep )
{
	gchar *text;
	gdouble d;

	if( my_strlen( string )){
		text = my_double_undecorate( string, thousand_sep, decimal_sep );
		d = g_strtod( text, NULL );
		g_free( text );

	} else {
		d = 0.0;
	}

	return( d );
}

/**
 * my_double_to_sql:
 *
 * Returns: a newly allocated string which represents the specified
 * value, suitable for an SQL insertion.
 *
 * Decimal separator is a dot '.' (not locale-dependant, not user-prefs dependant);
 * No thousand separator.
 */
gchar *
my_double_to_sql( gdouble value )
{
	gchar amount[1+G_ASCII_DTOSTR_BUF_SIZE];
	gchar *text;

	g_ascii_dtostr( amount, G_ASCII_DTOSTR_BUF_SIZE, value );
	g_strchug( amount );
	text = g_strdup( amount );

	return( text );
}

/**
 * my_double_to_sql_ex:
 * @value:
 * @decimals:
 *
 * Returns: a newly allocated string which represents the specified
 * value, suitable for an SQL insertion.
 *
 * Decimal separator is a dot '.' (not locale-dependant, not user-prefs dependant);
 * No thousand separator.
 */
gchar *
my_double_to_sql_ex( gdouble value, gint decimals )
{
	gchar amount_str[1+G_ASCII_DTOSTR_BUF_SIZE];
	gchar *temp_str, *text;
	gdouble temp_double;
	glong temp_long;

	if( value >= 0 ){
		temp_double = value*exp10( decimals ) + 0.5;
	} else {
		temp_double = value*exp10( decimals ) - 0.5;
	}
	temp_long = ( glong ) temp_double;
	g_ascii_dtostr( amount_str, G_ASCII_DTOSTR_BUF_SIZE, temp_long );
	g_strchug( amount_str );

	if( value ){
		temp_str = g_strreverse( g_strdup( amount_str ));
		temp_str[decimals] = '\0';
		amount_str[my_strlen( amount_str )-decimals] = '\0';
		text = g_strconcat( amount_str, ".", g_strreverse( temp_str ), NULL );
	} else {
		text = g_strdup( amount_str );
	}

	return( text );
}

/**
 * my_bigint_to_str:
 * @value:
 * @thousand_sep:
 *
 * Returns: a newly allocated string which represents the specified
 * value, decorated for display (with thousand separator).
 */
gchar *
my_bigint_to_str( glong value, gunichar thousand_sep )
{
	gchar *text, *deco;

	text = g_strdup_printf( "%'ld", value);
	deco = double_decorate( text, thousand_sep, '\0' );

	g_free( text );

	return( deco );
}

/**
 * my_double_to_str:
 * @value:
 * @thousand_sep:
 * @decimal_sep:
 * @decimal_digits:
 *
 * Returns: a newly allocated string which represents the specified
 * value, decorated for display (with thousand and decimal separators).
 */
gchar *
my_double_to_str( gdouble value, gunichar thousand_sep, gunichar decimal_sep, gint decimal_digits )
{
	gchar *text, *deco;

	text = g_strdup_printf( "%'.*lf", decimal_digits, value);
	deco = double_decorate( text, thousand_sep, decimal_sep );

	g_free( text );

	return( deco );
}

/*
 * double_decorate:
 *
 * This a "locale to prefs" transformation.
 */
static gchar *
double_decorate( const gchar *text, gunichar thousand_sep, gunichar decimal_sep )
{
	gchar *dest1, *dest2, *dest3;
	gchar *str;

	double_set_locale();

	/* change locale thousand separator to inter */
	dest1 = g_regex_replace_literal( st_locale_thousand_regex, text, -1, 0, st_inter, 0, NULL );

	/* change locale decimal separator with the provided one */
	if( decimal_sep ){
		str = g_strdup_printf( "%c", decimal_sep );
		dest2 = g_regex_replace_literal( st_locale_decimal_regex, dest1, -1, 0, str, 0, NULL );
		g_free( str );
	} else {
		dest2 = g_strdup( dest1 );
	}

	/* change inter to prefs thousand separator */
	if( thousand_sep ){
		str = g_strdup_printf( "%c", thousand_sep );
		dest3 = g_regex_replace_literal( st_inter_regex, dest2, -1, 0, str, 0, NULL );
		g_free( str );
	} else {
		dest3 = g_strdup( dest2 );
	}

	g_free( dest1 );
	g_free( dest2 );

	return( dest3 );
}
