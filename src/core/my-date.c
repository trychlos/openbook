/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

#include "api/my-date.h"
#include "api/my-utils.h"

typedef struct {
	gint         code;
	const gchar *label;
}
	sDateFormat;

static const sDateFormat st_date_format[] = {
		{ MY_DATE_DMMM, N_( "D MMM YYYY" )},
		{ MY_DATE_MMYY, N_( "MMM YYYY" )},
		{ MY_DATE_DMYY, N_( "DD/MM/YYYY" )},
		{ MY_DATE_SQL,  N_( "YYYY-MM-DD" )},
		{ MY_DATE_YYMD, N_( "YYYYMMDD" )},
		{ 0 }
};

static gboolean parse_ddmmyyyy_string( GDate *date, const gchar *string, gint *year );
static gboolean parse_yyyymmdd_string( GDate *date, const gchar *string );

/**
 * my_date_clear:
 * @date: [in-out]: a not-null pointer to a GDate structure
 *
 * Clear the given @date.
 */
void
my_date_clear( GDate *date )
{
	g_date_clear( date, 1 );
}

/**
 * my_date_is_valid:
 * @date: the #GDate to be evaluated
 *
 * Returns: %TRUE if the date is valid.
 */
gboolean
my_date_is_valid( const GDate *date )
{
	return( date && g_date_valid( date ));
}

/**
 * my_date_compare:
 * @a: the first #GDate to be compared
 * @b: the second #GDate to be compared to @a
 *
 * Compare two valid dates, returning -1, 0 or 1 if @a less than, equal
 * or greater than @b.
 *
 * Returns: -1, 0 or 1.
 *
 * It is to the caller to make sure that @a and @b are two valid dates,
 * because it is the only one to have an idea of what must be done is
 * such a case...
 * So all calls to #my_date_compare() must be protected by a previous
 * #my_date_is_valid() call.
 */
gint
my_date_compare( const GDate *a, const GDate *b )
{
	if( !my_date_is_valid( a )){
		g_return_val_if_reached( 0 );
	}
	if( !my_date_is_valid( b )){
		g_return_val_if_reached( 0 );
	}

	return( g_date_compare( a, b ));
}

/**
 * my_date_compare_ex:
 * @a: the first #GDate to be compared
 * @b: the second #GDate to be compared to @a
 * @clear_is_past_infinite: if %TRUE, then any cleared or invalid date
 *  is considered as a past infinite value, and set as lesser than
 *  anything, but another past infinite value.
 *  Else, a cleared or invalid value is considered infinite in the
 *  future.
 *
 * Compare the two dates, returning -1, 0 or 1 if @a less than, equal or
 * greater than @b.
 * A cleared or invalid date is considered infinite.
 *
 * Returns: -1, 0 or 1.
 */
gint
my_date_compare_ex( const GDate *a, const GDate *b, gboolean clear_is_past_infinite )
{
	gboolean a_ok, b_ok;

	a_ok = my_date_is_valid( a );
	b_ok = my_date_is_valid( b );

	if( !a_ok ){
		if( b_ok ){
			return( clear_is_past_infinite ? -1 : 1 );
		} else {
			return( 0 );
		}
	} else if( !b_ok ){
		return( clear_is_past_infinite ? 1 : -1 );
	}

	return( g_date_compare( a, b ));
}

/**
 * my_date_compare_by_str:
 *
 * Compare two strings which are supposed to represent dates
 */
gint
my_date_compare_by_str( const gchar *sda, const gchar *sdb, myDateFormat format )
{
	GDate da, db;

	if( !sda || !g_utf8_strlen( sda, -1 )){
		if( !sdb || !g_utf8_strlen( sdb, -1 )){
			/* the two dates are both empty */
			return( 0 );
		}
		/* a is empty while b is set */
		return( -1 );
	} else if( !sdb || !g_utf8_strlen( sdb, -1 )){
		/* a is set while b is empty */
		return( 1 );
	}

	/* both a and b are set */
	my_date_set_from_str( &da, sda, format );
	my_date_set_from_str( &db, sdb, format );

	return( my_date_compare_ex( &da, &db, TRUE ));
}

/**
 * my_date_set_now:
 * @date: [out]: a not-null pointer to the destination GDate structure
 *
 * Initialize the given @date with the current date.
 *
 * Returns: @date, in order to be able to chain the functions.
 */
GDate *
my_date_set_now( GDate *date )
{
	GDateTime *dt;
	gint year, month, day;

	g_return_val_if_fail( date, NULL );

	dt = g_date_time_new_now_local();
	if( dt ){
		g_date_time_get_ymd( dt, &year, &month, &day );
		g_date_time_unref( dt );
		g_date_set_dmy( date, day, month, year );
	}

	return( date );
}

/**
 * my_date_set_from_date:
 * @date: [out]: a not-null pointer to the destination GDate structure
 * @orig: [in][allow-none]: the #GDate to be copied
 *
 * Set the @date to the give @orig one.
 * The dest @date is set invalid if the @orig one is itself invalid.
 *
 * Returns: @date, in order to be able to chain the functions.
 */
GDate *
my_date_set_from_date( GDate *date, const GDate *orig )
{
	g_return_val_if_fail( date, NULL );

	g_date_clear( date, 1 );

	if( my_date_is_valid( orig )){
		memcpy( date, orig, sizeof( GDate ));
	}

	return( date );
}

/**
 * my_date_set_from_sql:
 * @date: [out]: a not-null pointer to the destination GDate structure
 * @sql_string: [in]: a SQL string 'yyyy-mm-dd', or NULL;
 *  the SQL string may be zero '0000-00-00' or a valid date.
 *
 * Parse a SQL string, putting the result in @date.
 * The dest @date is set invalid if the @sql_string doesn't evaluate to
 * a valid date.
 * NB: parsing a 'yyyy-mm-dd' is not locale-sensitive.
 *
 * Returns: @date, in order to be able to chain the functions.
 */
GDate *
my_date_set_from_sql( GDate *date, const gchar *sql_string )
{
	g_return_val_if_fail( date, NULL );

	g_date_clear( date, 1 );

	if( sql_string &&
			g_utf8_strlen( sql_string, -1 ) &&
			g_utf8_collate( sql_string, "0000-00-00" )){

		g_date_set_parse( date, sql_string );
	}

	return( date );
}

/**
 * my_date_set_from_str:
 * @date: [out]: a not-null pointer to the destination GDate structure
 * @fmt_string: [in][allow-none]: the string to be parsed
 * @format: the expected format of the string
 *
 * Parse a string which should represent a date into a #GDate structure.
 * The dest @date is set invalid if the @sql_string doesn't evaluate to
 * a valid date.
 *
 * Returns: @date, in order to be able to chain the functions.
 */
GDate *
my_date_set_from_str( GDate *date, const gchar *fmt_string, myDateFormat format )
{
	return( my_date_set_from_str_ex( date, fmt_string, format, NULL ));
}

/*
 * Returns TRUE if the string parses to a valid 'dd/mm/yyyy' date
 * even if the provider default @year is to be used for that
 */
static gboolean
parse_ddmmyyyy_string( GDate *date, const gchar *string, gint *year )
{
	static const gchar *thisfn = "my_date_parse_ddmmyyyy_string";
	gboolean valid;
	gchar **tokens, **iter;
	gint dd, mm, yy;

	dd = mm = yy = 0;
	valid = FALSE;
	my_date_clear( date );

	if( my_strlen( string )){
		tokens = g_strsplit( string, "/", -1 );
		iter = tokens;
		if( my_strlen( *iter )){
			dd = atoi( *iter );
			iter++;
			if( my_strlen( *iter )){
				mm = atoi( *iter );
				iter++;
				if( my_strlen( *iter )){
					yy = atoi( *iter );
				} else if( year && *year > 0 ){
					g_debug( "%s: setting yy to %u", thisfn, *year );
					yy = *year;
				}
			}
		}
		g_strfreev( tokens );
	}

	if( g_date_valid_dmy( dd, mm, yy )){
		g_date_set_dmy( date, dd, mm, yy );
		valid = TRUE;
		if( year ){
			g_debug( "%s: setting year to %u", thisfn, yy );
			*year = yy;
		}
	}

	return( valid );
}

/*
 * Returns TRUE if the string parses to a valid 'yyyymmdd' date
 */
static gboolean
parse_yyyymmdd_string( GDate *date, const gchar *string )
{
	gboolean valid;
	gint dd, mm, yy;
	gchar part[5];
	const gchar *src;

	dd = mm = yy = 0;
	valid = FALSE;
	my_date_clear( date );

	if( string && g_utf8_strlen( string, -1 )){
		src = string;
		memset( part, '\0', sizeof( part ));
		g_utf8_strncpy( part, src, 4 );
		yy = atoi( part );
		src = g_utf8_find_next_char( src, NULL );
		src = g_utf8_find_next_char( src, NULL );
		src = g_utf8_find_next_char( src, NULL );
		src = g_utf8_find_next_char( src, NULL );
		memset( part, '\0', sizeof( part ));
		g_utf8_strncpy( part, src, 2 );
		mm = atoi( part );
		src = g_utf8_find_next_char( src, NULL );
		src = g_utf8_find_next_char( src, NULL );
		memset( part, '\0', sizeof( part ));
		g_utf8_strncpy( part, src, 2 );
		dd = atoi( part );
	}

	if( g_date_valid_dmy( dd, mm, yy )){
		g_date_set_dmy( date, dd, mm, yy );
		valid = TRUE;
	}

	return( valid );
}

/**
 * my_date_set_from_str_ex:
 * @date: [out]: a not-null pointer to the destination GDate structure
 * @fmt_string: [in][allow-none]: the string to be parsed
 * @format: the expected format of the string
 * @year: [in][out][allow-none]: if set, may be used as a default year if it is
 *  missing from the @fmt_string - On output, is set with the year of
 *  the @date if it is valid.
 *
 * Parse a string which should represent a date into a #GDate structure.
 * The dest @date is set invalid if the @sql_string doesn't evaluate to
 * a valid date.
 *
 * Returns: @date, in order to be able to chain the functions.
 */
GDate *
my_date_set_from_str_ex( GDate *date, const gchar *fmt_string, myDateFormat format, gint *year )
{
	static const gchar *thisfn = "my_date_set_from_str_ex";

	g_return_val_if_fail( date, NULL);

	switch( format ){

		case MY_DATE_DMYY:
			if( !parse_ddmmyyyy_string( date, fmt_string, year )){
				my_date_clear( date );
			}
			break;

		case MY_DATE_SQL:
			my_date_set_from_sql( date, fmt_string );
			break;

		case MY_DATE_YYMD:
			if( !parse_yyyymmdd_string( date, fmt_string )){
				my_date_clear( date );
			}
			break;

		default:
			g_warning( "%s: unhandled format code %u", thisfn, format );
			break;
	}

	return( date );
}

/**
 * my_date_set_from_stamp:
 * @date: [out]: a not-null pointer to the destination GDate structure.
 * @stamp: a #GTimeVal timestamp, may be invalid.
 *
 * Returns: @date, in order to be able to chain the functions.
 */
GDate *
my_date_set_from_stamp( GDate *date, const GTimeVal *stamp )
{
	GDateTime *dt;

	g_return_val_if_fail( date, NULL);

	my_date_clear( date );
	if( stamp && stamp->tv_sec ){
		dt = g_date_time_new_from_timeval_local( stamp );
		if( dt ){
			g_date_set_day( date, g_date_time_get_day_of_month( dt ));
			g_date_set_month( date, g_date_time_get_month( dt ));
			g_date_set_year( date, g_date_time_get_year( dt ));
			g_date_time_unref( dt );
		}
	}

	return( date );
}

/**
 * my_date_to_str:
 * @date: [in]: the GDate to be represented as a string
 * @format: [in]: the required format.
 *
 * Returns: a newly allocated string which represents the given @date,
 * with the required @format.
 * If the given @date is invalid, then an empty string is returned.
 *
 * The returned string should be g_free() by the caller.
 */
gchar *
my_date_to_str( const GDate *date, myDateFormat format )
{
	static const gchar *thisfn = "my_date_to_str";
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

	if( my_date_is_valid( date )){
		switch( format ){

			/* d mmm yyyy - display as a label */
			case MY_DATE_DMMM:
				g_free( str );
				str = g_strdup_printf( "%d %s %4.4d",
						g_date_get_day( date ),
						gettext( st_month[g_date_get_month( date )-1] ),
						g_date_get_year( date ));
				break;

			/* mmm yyyy - display as a label with first uppercase */
			case MY_DATE_MMYY:
				g_free( str );
				str = g_strdup_printf( "%s %4.4d",
						gettext( st_month[g_date_get_month( date )-1] ),
						g_date_get_year( date ));
				str[0] = g_ascii_toupper( str[0] );
				break;

			/* dd/mm/yyyy - display for entry */
			case MY_DATE_DMYY:
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

			/* yyyymmdd - for filenames */
			case MY_DATE_YYMD:
				g_free( str );
				str = g_strdup_printf( "%4.4d%2.2d%2.2d",
						g_date_get_year( date ),
						g_date_get_month( date ),
						g_date_get_day( date ));
				break;

			default:
				g_warning( "%s: unhandled format %u", thisfn, format );
				break;
		}
	}

	return( str );
}

/**
 * my_date_get_format_str:
 *
 * Returns: a localized string which describes the specified format.
 */
const gchar *
my_date_get_format_str( myDateFormat format )
{
	static const gchar *thisfn = "my_date_get_format_str";
	gint i;

	for( i=0 ; st_date_format[i].code ; ++i ){
		if( st_date_format[i].code == format ){
			return( gettext( st_date_format[i].label ));
		}
	}

	g_warning( "%s: unknown date format: %d", thisfn, format );
	return( NULL );
}
