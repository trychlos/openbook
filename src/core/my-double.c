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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "api/my-double.h"
#include "api/ofa-settings.h"

static gunichar st_double_thousand_sep   = '\0';
static gunichar st_double_decimal_sep    = '\0';
static GRegex  *st_double_thousand_regex = NULL;
static GRegex  *st_double_decimal_regex  = NULL;

static void double_set_locale( void );

/**
 * my_double_undecorate:
 *
 * Remove from the given string all decoration added for the display
 * of a double, returning so a 'brut' double string, without the locale
 * thousand separator and with a dot as the decimal point
 */
gchar *
my_double_undecorate( const gchar *text )
{
	gchar *dest1, *dest2;

	double_set_locale();

	/* remove locale thousand separator */
	dest1 = g_regex_replace_literal( st_double_thousand_regex, text, -1, 0, "", 0, NULL );

	/* replace locale decimal separator with a dot '.' */
	dest2 = g_regex_replace_literal( st_double_decimal_regex, dest1, -1, 0, ".", 0, NULL );

	g_free( dest1 );

	return( dest2 );
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

	if( !st_double_thousand_sep ){
		str = g_strdup_printf( "%'.1lf", 1000.0 );
		p = g_utf8_next_char( str );
		st_double_thousand_sep = g_utf8_get_char( p );
		srev = g_utf8_strreverse( str, -1 );
		p = g_utf8_next_char( srev );
		st_double_decimal_sep = g_utf8_get_char( p );

		g_debug( "%s: thousand_sep='%c', decimal_sep='%c'",
					thisfn, st_double_thousand_sep, st_double_decimal_sep );

		str = g_strdup_printf( "%c", st_double_thousand_sep );
		st_double_thousand_regex = g_regex_new( str, 0, 0, NULL );
		g_free( str );

		str = g_strdup_printf( "%c", st_double_decimal_sep );
		st_double_decimal_regex = g_regex_new( str, 0, 0, NULL );
		g_free( str );
	}
}

/**
 * my_double_set_from_sql:
 *
 * Returns a double from the specified SQl stringified decimal
 *
 * The input string is not supposed to be localized, nor decorated.
 */
gdouble
my_double_set_from_sql( const gchar *sql_string )
{
	if( !sql_string || !g_utf8_strlen( sql_string, -1 )){
		return( 0.0 );
	}

	return( g_ascii_strtod( sql_string, NULL ));
}

/**
 * my_double_set_from_str:
 *
 * In v1, we only target fr locale, so with space as thousand separator
 * and comma as decimal one on display -
 * When parsing a string - and because we want be able to re-parse a
 * string that we have previously displayed - we accept both
 */
gdouble
my_double_set_from_str( const gchar *string )
{
	gchar *text;
	gdouble d;

	if( string && g_utf8_strlen( string, -1 )){
		text = my_double_undecorate( string );
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
 * Prefer g_ascii_formatd() to g_ascii_dtostr() as the later doesn't
 * correctly rounds up the double (have a non-null twelth ou thirteenth
 * decimal digit)
 */
gchar *
my_double_to_sql( gdouble value )
{
	gchar amount[1+G_ASCII_DTOSTR_BUF_SIZE];
	gchar *text;

	/*g_ascii_dtostr( amount, G_ASCII_DTOSTR_BUF_SIZE, value );*/
	g_ascii_formatd( amount, G_ASCII_DTOSTR_BUF_SIZE, "%15.5f", value );
	g_strchug( amount );
	text = g_strdup( amount );

	return( text );
}

/**
 * my_double_to_str:
 * @value:
 *
 * Returns: a newly allocated string which represents the specified
 * value, decorated for display (with thousand and decimal separators).
 */
gchar *
my_double_to_str( gdouble value )
{
	return( my_double_to_str_ex( value, 2 ));
}

/**
 * my_double_to_str_ex:
 * @value:
 * @decimals:
 *
 * Returns: a newly allocated string which represents the specified
 * value, decorated for display (with thousand and decimal separators).
 */
gchar *
my_double_to_str_ex( gdouble value, gint decimals )
{
	gchar *text;

	text = g_strdup_printf( "%'.*lf", decimals, value);

	return( text );
}
