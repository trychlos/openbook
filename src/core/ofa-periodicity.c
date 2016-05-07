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

#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-periodicity.h"

typedef struct {
	const gchar *code;
	const gchar *label;
}
	sLabels;

typedef struct {
	const gchar *code;
	GDateWeekday weekday;
}
	sWeekdays;

/* periodicity labels are listed here in display order
 */
static const sLabels st_labels[] = {
		{ PER_NEVER,   N_( "Never (disabled)" ) },
		{ PER_WEEKLY,  N_( "Weekly" ) },
		{ PER_MONTHLY, N_( "Monthly" ) },
		{ 0 }
};

/* weekly detail labels
 */
static const sLabels st_weekly_labels[] = {
		{ PER_WEEK_MONDAY,    N_( "Monday" )},
		{ PER_WEEK_TUESDAY,   N_( "Tuesday" )},
		{ PER_WEEK_WEDNESDAY, N_( "Wednesday" )},
		{ PER_WEEK_THURSDAY,  N_( "Thursday" )},
		{ PER_WEEK_FRIDAY,    N_( "Friday" )},
		{ PER_WEEK_SATURDAY,  N_( "Saturday" )},
		{ PER_WEEK_SUNDAY,    N_( "Sunday" )},
		{ 0 }
};

static const sWeekdays st_weekly_days[] = {
		{ PER_WEEK_MONDAY,    G_DATE_MONDAY },
		{ PER_WEEK_TUESDAY,   G_DATE_TUESDAY },
		{ PER_WEEK_WEDNESDAY, G_DATE_WEDNESDAY },
		{ PER_WEEK_THURSDAY,  G_DATE_THURSDAY },
		{ PER_WEEK_FRIDAY,    G_DATE_FRIDAY },
		{ PER_WEEK_SATURDAY,  G_DATE_SATURDAY },
		{ PER_WEEK_SUNDAY,    G_DATE_SUNDAY },
		{ 0 }
};

/* monthly detail labels
 *
 * Note that code is subject to atoi() interpretation.
 */
static const sLabels st_monthly_labels[] = {
		{ "1",  N_( " 1" ) },
		{ "2",  N_( " 2" ) },
		{ "3",  N_( " 3" ) },
		{ "4",  N_( " 4" ) },
		{ "5",  N_( " 5" ) },
		{ "6",  N_( " 6" ) },
		{ "7",  N_( " 7" ) },
		{ "8",  N_( " 8" ) },
		{ "9",  N_( " 9" ) },
		{ "10", N_( "10" ) },
		{ "11", N_( "11" ) },
		{ "12", N_( "12" ) },
		{ "13", N_( "13" ) },
		{ "14", N_( "14" ) },
		{ "15", N_( "15" ) },
		{ "16", N_( "16" ) },
		{ "17", N_( "17" ) },
		{ "18", N_( "18" ) },
		{ "19", N_( "19" ) },
		{ "20", N_( "20" ) },
		{ "21", N_( "21" ) },
		{ "22", N_( "22" ) },
		{ "23", N_( "23" ) },
		{ "24", N_( "24" ) },
		{ "25", N_( "25" ) },
		{ "26", N_( "26" ) },
		{ "27", N_( "27" ) },
		{ "28", N_( "28" ) },
		{ "29", N_( "29" ) },
		{ "30", N_( "30" ) },
		{ "31", N_( "31" ) },
		{ 0 }
};

static const sLabels *get_labels_for_periodicity( const gchar *periodicity );
static gint           get_weekday( const gchar *detail );

/**
 * ofa_periodicity_get_label:
 * @periodicity: the unlocalized periodicity code.
 *
 * Returns: the corresponding label.
 */
const gchar *
ofa_periodicity_get_label( const gchar *periodicity )
{
	gint i;

	if( my_strlen( periodicity )){
		for( i=0 ; st_labels[i].code ; ++i ){
			if( !my_collate( st_labels[i].code, periodicity )){
				return( gettext( st_labels[i].label ));
			}
		}
	}

	return( NULL );
}

/**
 * ofa_periodicity_get_detail_label:
 * @periodicity: the unlocalized periodicity code.
 * @detail: the unlocalized detail code.
 *
 * Returns: the corresponding localized label as a newly allocated string
 * which should be g_free() by the caller.
 */
const gchar *
ofa_periodicity_get_detail_label( const gchar *periodicity, const gchar *detail )
{
	gint i;
	const sLabels *labels;

	labels = get_labels_for_periodicity( periodicity );

	if( labels ){
		for( i=0 ; labels[i].code ; ++i ){
			if( !my_collate( labels[i].code, detail )){
				return( gettext( labels[i].label ));
			}
		}
	}

	return( NULL );
}

/**
 * ofa_periodicity_enum:
 * @fn:
 * @user_data:
 *
 * Enumerate the known periodicities.
 */
void
ofa_periodicity_enum( PeriodicityEnumCb fn, void *user_data )
{
	gint i;

	for( i=0 ; st_labels[i].code ; ++i ){
		( *fn )( st_labels[i].code, st_labels[i].label, user_data );
	}
}

/**
 * ofa_periodicity_enum_detail:
 * @periodicity:
 * @fn:
 * @user_data:
 *
 * Enumerate the known detail for the @periodicity.
 */
void
ofa_periodicity_enum_detail( const gchar *periodicity, PeriodicityEnumCb fn, void *user_data )
{
	gint i;
	const sLabels *labels;

	labels = get_labels_for_periodicity( periodicity );

	if( labels ){
		for( i=0 ; labels[i].code ; ++i ){
			( *fn )( labels[i].code, labels[i].label, user_data );
		}
	}
}

static const sLabels *
get_labels_for_periodicity( const gchar *periodicity )
{
	const sLabels *labels;

	if( !my_collate( periodicity, PER_WEEKLY )){
		labels = st_weekly_labels;

	} else if( !my_collate( periodicity, PER_MONTHLY )){
		labels = st_monthly_labels;

	} else {
		labels = NULL;
	}

	return( labels );
}

/**
 * ofa_periodicity_enum_dates_between:
 * @periodicity: the periodicity code.
 * @detail: the periodicity detail code.
 * @begin: the beginning date.
 * @end: the ending date.
 * @cb: the user callback.
 * @user_data: a user data provided pointer.
 *
 * Enumerates all valid dates between @begin and @end included dates.
 */
void
ofa_periodicity_enum_dates_between( const gchar *periodicity, const gchar *detail,
										const GDate *begin, const GDate *end, PeriodicityDatesCb cb, void *user_data )
{
	GDate date;
	GDateDay date_day;
	GDateWeekday date_week;
	gint detail_day, detail_week;

	g_return_if_fail( my_date_is_valid( begin ));
	g_return_if_fail( my_date_is_valid( end ));

	my_date_set_from_date( &date, begin );
	detail_day = my_collate( periodicity, PER_MONTHLY ) ? G_DATE_BAD_DAY : atoi( detail );
	detail_week = my_collate( periodicity, PER_WEEKLY ) ? G_DATE_BAD_WEEKDAY : get_weekday( detail );

	while( my_date_compare( &date, end ) <= 0 ){
		if( !my_collate( periodicity, PER_MONTHLY )){
			date_day = g_date_get_day( &date );
			if( date_day == detail_day ){
				( *cb )( &date, user_data );
			}
		}
		if( !my_collate( periodicity, PER_WEEKLY )){
			date_week = g_date_get_weekday( &date );
			if( date_week == detail_week ){
				( *cb )( &date, user_data );
			}
		}
		g_date_add_days( &date, 1 );
	}
}

static gint
get_weekday( const gchar *detail )
{
	gint i;

	for( i=0 ; st_weekly_days[i].code ; ++i ){
		if( !my_collate( st_weekly_days[i].code, detail )){
			return( st_weekly_days[i].weekday );
		}
	}

	return( G_DATE_BAD_WEEKDAY );
}
