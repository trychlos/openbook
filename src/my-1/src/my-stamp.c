/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

static GTimeVal *set_from_str_yymdhms( GTimeVal *timeval, const gchar *str );
static GTimeVal *set_from_str_dmyyhm( GTimeVal *timeval, const gchar *str );

/**
 * my_stamp_set_now:
 * @timeval: [out]: the destination of the timestamp
 *
 * Set the provided #GTimeVal to the current timestamp.
 *
 * Returns: @timeval.
 */
GTimeVal *
my_stamp_set_now( GTimeVal *timeval )
{
	GDateTime *dt;

	dt = g_date_time_new_now_local();
	g_date_time_to_timeval( dt, timeval );
	g_date_time_unref( dt );

	return( timeval );
}

/**
 * my_stamp_set_from_sql:
 * @timeval: a pointer to a GTimeVal structure
 * @str: [allow-none]:
 *
 * SQL timestamp is returned as a string '2014-05-24 20:05:46'
 */
GTimeVal *
my_stamp_set_from_sql( GTimeVal *timeval, const gchar *str )
{
	return( set_from_str_yymdhms( timeval, str ));
}

/**
 * my_stamp_set_from_str:
 * @timeval: a pointer to a GTimeVal structure
 * @str: [allow-none]: the string to be parsed
 * @format: the expected #myStampFormat.
 *
 * Parse the @str to the @timeval timestamp structure.
 */
GTimeVal *
my_stamp_set_from_str( GTimeVal *timeval, const gchar *str, myStampFormat format )
{
	static const gchar *thisfn = "my_stamp_set_from_str";

	switch( format ){
		case MY_STAMP_YYMDHMS:
			set_from_str_yymdhms( timeval, str );
			break;
		case MY_STAMP_DMYYHM:
			set_from_str_dmyyhm( timeval, str );
			break;
		default:
			g_warning( "%s: unknown or invalid format: %u", thisfn, format );
	}

	return( timeval );
}

/*
 * SQL timestamp is returned as a string '2014-05-24 20:05:46'
 */
static GTimeVal *
set_from_str_yymdhms( GTimeVal *timeval, const gchar *str )
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

/*
 * The string is expected to be 'dd/mm/yyyy hh:mi'
 */
static GTimeVal *
set_from_str_dmyyhm( GTimeVal *timeval, const gchar *str )
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

	timeval->tv_sec = mktime( &broken );
	timeval->tv_usec = 0;

	return( timeval );
}

/**
 * my_stamp_set_from_stamp:
 * @timeval: a pointer to a GTimeVal structure
 * @orig: [allow-none]:
 *
 * Returns a pointer to the destination @timeval.
 */
GTimeVal *
my_stamp_set_from_stamp( GTimeVal *timeval, const GTimeVal *orig )
{
	if( orig ){
		memcpy( timeval, orig, sizeof( GTimeVal ));
	} else {
		memset( timeval, '\0', sizeof( GTimeVal ));
	}

	return( timeval );
}

/**
 * my_stamp_to_str:
 */
gchar *
my_stamp_to_str( const GTimeVal *stamp, myStampFormat format )
{
	GDateTime *dt;
	gchar *str;

	str = NULL;

	if( stamp ){
		dt = g_date_time_new_from_timeval_local( stamp );
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
			}
			g_date_time_unref( dt );
		}
	}

	return( str );
}
