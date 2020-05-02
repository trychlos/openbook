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

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "my/my-stamp.h"

struct _myStampVal {
	guint64 sec;
	guint64 usec;
};

static myStampVal *set_from_str_yymdhms( myStampVal *timeval, const gchar *str );
static myStampVal *set_from_str_dmyyhm( myStampVal *timeval, const gchar *str );

/**
 * my_stamp_new:
 *
 * Allocates a new #myStampVal.
 *
 * Returns: a new #myStampVal which should be my_stamp_free() by the caller.
 */
myStampVal *
my_stamp_new()
{
	return( g_new0( struct _myStampVal, 1 ));
}

/**
 * my_stamp_new_now:
 *
 * Allocates a new #myStampVal, setting it to the current timestamp.
 *
 * Returns: a new #myStampVal which should be my_stamp_free() by the caller.
 */
myStampVal *
my_stamp_new_now()
{
	return( my_stamp_set_now( my_stamp_new()));
}

/**
 * my_stamp_new_from_sql:
 * @str: [allow-none]: a SQL timestamp string as 'YYYY-MM-DD HH:MI:SS'.
 *
 * Allocates a new #myStampVal, setting it to the @str timestamp.
 *
 * Returns: a new #myStampVal which should be my_stamp_free() by the caller.
 */
myStampVal *
my_stamp_new_from_sql( const gchar *str )
{
	return( my_stamp_set_from_sql( my_stamp_new(), str ));
}

/**
 * my_stamp_new_from_stamp:
 * @stamp: [allow-none]: a @myStampVal value to be copied.
 *
 * Allocates a new #myStampVal, setting it to the @stamp timestamp.
 *
 * Returns: a new #myStampVal which should be my_stamp_free() by the caller.
 */
myStampVal *
my_stamp_new_from_stamp( const myStampVal *stamp )
{
	return( my_stamp_set_from_stamp( my_stamp_new(), stamp ));
}

/**
 * my_stamp_new_from_str:
 * @str: [allow-none]: the string to be parsed
 * @format: the expected #myStampFormat.
 *
 * Allocates a new #myStampVal, setting it to the @str parsed timestamp.
 *
 * Returns: a new #myStampVal which should be my_stamp_free() by the caller.
 */
myStampVal *
my_stamp_new_from_str( const gchar *str, myStampFormat format )
{
	return( my_stamp_set_from_str( my_stamp_new(), str, format ));
}

/**
 * my_stamp_set_now:
 * @stamp: [out]: the destination of the timestamp
 *
 * Set the provided #myStampVal to the current timestamp.
 *
 * Returns: @stamp.
 */
myStampVal *
my_stamp_set_now( myStampVal *stamp )
{
	GDateTime *dt;

	g_return_val_if_fail( stamp, NULL );

	dt = g_date_time_new_now_local();
	stamp->sec = g_date_time_to_unix( dt );
	stamp->usec = g_date_time_get_microsecond( dt );
	g_date_time_unref( dt );

	return( stamp );
}

/**
 * my_stamp_compare:
 * @a: [allow-none]: a #myStampVal.
 * @b: [allow-none]: another #myStampVal.
 *
 * Returns: -1 if @a < @b, +1 if @a > @b, 0 if they are equal.
 */
gint
my_stamp_compare( const myStampVal *a, const myStampVal *b )
{
	if( a ){
		if( b ){
			return( a->sec < b->sec ? -1 : ( a->sec > b->sec ? 1 : ( a->usec < b->usec ? -1 : ( a->usec > b->usec ? 1 : 0 ))));
		} else {
			return( 1 );
		}
	} else if( b ){
		return( -1 );
	}

	return( 0 );
}

/**
 * my_stamp_diff_us:
 * @a: [allow-none]: a #myStampVal.
 * @b: [allow-none]: another #myStampVal.
 *
 * Returns: the difference in micro-seconds between @a and @b, as @a - @b.
 */
gint64
my_stamp_diff_us( const myStampVal *a, const myStampVal *b )
{
	gint64 diff = 0;

	if( a ){
		if( b ){
			diff = G_USEC_PER_SEC * ( a->sec - b->sec );
			diff += a->usec - b->usec;
		} else {
			diff = G_USEC_PER_SEC * a->sec;
			diff += a->usec;
		}
	} else if( b ){
		diff = G_USEC_PER_SEC * b->sec;
		diff += b->usec;
		diff *= -1;
	}

	return( diff );
}

/**
 * my_stamp_get_seconds:
 * @stamp: [allow-none]: a #myStampVal.
 *
 * Returns: the seconds since EPOCH time.
 */
time_t
my_stamp_get_seconds( const myStampVal *stamp )
{
	GDateTime *dt;
	time_t seconds = 0;

	dt = g_date_time_new_from_unix_local( stamp->sec );
	if( dt ){
		seconds = ( time_t ) g_date_time_to_unix( dt );
		g_date_time_unref( dt );
	}

	return( seconds );
}

/**
 * my_stamp_get_usecs:
 * @stamp: [allow-none]: a #myStampVal.
 *
 * Returns: the micro-seconds.
 */
gulong
my_stamp_get_usecs( const myStampVal *stamp )
{
	GDateTime *dt;
	gulong usecs = 0;

	dt = g_date_time_new_from_unix_local( stamp->sec );
	if( dt ){
		usecs = ( gulong ) g_date_time_get_microsecond( dt );
		g_date_time_unref( dt );
	}

	return( usecs );
}

/**
 * my_stamp_set_from_sql:
 * @stamp: [out]: a pointer to a #myStampVal structure.
 * @str: [allow-none]: a SQL timestamp string as 'YYYY-MM-DD HH:MI:SS'.
 *
 * SQL timestamp is returned as a string '2014-05-24 20:05:46'
 *
 * Returns: @stamp.
 */
myStampVal *
my_stamp_set_from_sql( myStampVal *stamp, const gchar *str )
{
	return( set_from_str_yymdhms( stamp, str ));
}

/**
 * my_stamp_set_from_stamp:
 * @stamp: (out]: a pointer to a myStampVal structure
 * @orig: [allow-none]: the #myStampVal to be copied from.
 *
 * Returns: @stamp.
 */
myStampVal *
my_stamp_set_from_stamp( myStampVal *stamp, const myStampVal *orig )
{
	g_return_val_if_fail( stamp, NULL );

	if( orig ){
		memcpy( stamp, orig, sizeof( myStampVal ));
	} else {
		memset( stamp, '\0', sizeof( myStampVal ));
	}

	return( stamp );
}

/**
 * my_stamp_set_from_str:
 * @stamp: [out]: a pointer to a #myStampVal structure
 * @str: [allow-none]: the string to be parsed
 * @format: the expected #myStampFormat.
 *
 * Parse the @str to the @timeval timestamp structure.
 *
 * Returns: @stamp.
 */
myStampVal *
my_stamp_set_from_str( myStampVal *stamp, const gchar *str, myStampFormat format )
{
	static const gchar *thisfn = "my_stamp_set_from_str";

	switch( format ){
		case MY_STAMP_YYMDHMS:
			set_from_str_yymdhms( stamp, str );
			break;
		case MY_STAMP_DMYYHM:
			set_from_str_dmyyhm( stamp, str );
			break;
		default:
			g_warning( "%s: unknown or invalid format: %u", thisfn, format );
	}

	return( stamp );
}

/*
 * SQL timestamp is returned as a string '2014-05-24 20:05:46'
 */
static myStampVal *
set_from_str_yymdhms( myStampVal *timeval, const gchar *str )
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

	timeval->sec = mktime( &broken );
	timeval->usec = 0;

	return( timeval );
}

/*
 * The string is expected to be 'dd/mm/yyyy hh:mi'
 */
static myStampVal *
set_from_str_dmyyhm( myStampVal *timeval, const gchar *str )
{
	gint y, m, d, H, M, S;
	struct tm broken;

	S = 0;
	sscanf( str, "%d/%d/%d %d:%d", &d, &m, &y, &H, &M );

	memset( &broken, '\0', sizeof( broken ));
	broken.tm_year = y - 1900;
	broken.tm_mon = m-1;	/* 0 to 11 */
	broken.tm_mday = d;
	broken.tm_hour = H;
	broken.tm_min = M;
	broken.tm_sec = S;
	broken.tm_isdst = -1;

	timeval->sec = mktime( &broken );
	timeval->usec = 0;

	return( timeval );
}

/**
 * my_stamp_to_str:
 * @stamp: a #myStampVal timestamp.
 * @format: a #myStampFormat string format.
 *
 * Returns: a newly allocated string which should be g_free() by the caller,
 * or %NULL if @stamp was NULL or invalid.
 */
gchar *
my_stamp_to_str( const myStampVal *stamp, myStampFormat format )
{
	GDateTime *dt;
	gchar *str;

	str = NULL;

	if( stamp ){
		dt = g_date_time_new_from_unix_local( stamp->sec );
		if( dt ){
			switch( format ){
				/* this is SQL format */
				case MY_STAMP_YYMDHMS:
					str = g_date_time_format( dt, "%Y-%m-%d %H:%M:%S" );
					break;

				/* this is a display format */
				case MY_STAMP_DMYYHM:
					str = g_date_time_format( dt, "%d/%m/%Y %H:%M" );
					break;

				/* this is the format for FEC export */
				case MY_STAMP_YYMD:
					str = g_date_time_format( dt, "%Y%m%d" );
					break;
			}
			g_date_time_unref( dt );
		}
	}

	return( str );
}

/**
 * my_stamp_free:
 * @stamp: [allow-none]: a #myStampVal structure.
 *
 * Release the @stamp allocation.
 */
void
my_stamp_free( myStampVal *stamp )
{
	if( stamp ){
		g_free( stamp );
	}
}
