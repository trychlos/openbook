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

#include <glib/gi18n.h>

#include "my/my-date.h"
#include "my/my-period.h"
#include "my/my-utils.h"

/* private instance data
 */
typedef struct {
	gboolean     dispose_has_run;

	/* runtime data
	 */
	myPeriodKey  key;
	guint        every;
	GList       *details;		/* a list of days which repeat in the period */
}
	myPeriodPrivate;

/* manage the periodicity indicator
 * - the identifier is from a public enum (easier for the code)
 * - a non-localized char stored in dbms
 * - a localized char (short string for treeviews)
 * - a localized label
 */
typedef struct {
	myPeriodKey  key;
	const gchar *dbms;
	const gchar *abr;
	const gchar *label;
}
	sPeriod;

static sPeriod st_period[] = {
		{ MY_PERIOD_UNSET,   "U", N_( "U" ), N_( "Unset" ) },
		{ MY_PERIOD_DAILY,   "D", N_( "D" ), N_( "Daily" ) },
		{ MY_PERIOD_WEEKLY,  "W", N_( "W" ), N_( "Weekly" ) },
		{ MY_PERIOD_MONTHLY, "M", N_( "M" ), N_( "Monthly" ) },
		{ MY_PERIOD_YEARLY,  "Y", N_( "Y" ), N_( "Yearly" ) },
		{ 0 },
};

/* associates the day of week we present with the day of week as provided
 * by the GLib library.
 * This is to be GLib-independant as far as possible.
 */
typedef struct {
	guint        int_weekday;
	guint        glib_weekday;
	const gchar *abr;
	const gchar *label;
}
	sDay;

static sDay st_day_of_week[] = {
		{ 1, G_DATE_MONDAY,    N_( "Mon" ), N_( "Monday" ) },
		{ 2, G_DATE_TUESDAY,   N_( "Tue" ), N_( "Tuesday" ) },
		{ 3, G_DATE_WEDNESDAY, N_( "Wed" ), N_( "Wednesday" ) },
		{ 4, G_DATE_THURSDAY,  N_( "Thu" ), N_( "Thursday" ) },
		{ 5, G_DATE_FRIDAY,    N_( "Fri" ), N_( "Friday" ) },
		{ 6, G_DATE_SATURDAY,  N_( "Sat" ), N_( "Saturday" ) },
		{ 7, G_DATE_SUNDAY,    N_( "Sun" ), N_( "Sunday" ) },
		{ 0 },
};

/* the separator */
static const gchar *st_sep = ",";

static gboolean     key_is_valid( myPeriodKey key );
static gint         details_sort_fn( gconstpointer ptra, gconstpointer ptrb );
static void         period_enum_daily( myPeriod *period, const GDate *last, const GDate *enum_begin, const GDate *enum_end, myPeriodEnumBetweenCb cb, void *user_data );
static void         period_enum_weekly( myPeriod *period, const GDate *last, const GDate *enum_begin, const GDate *enum_end, myPeriodEnumBetweenCb cb, void *user_data );
static void         period_enum_monthly( myPeriod *period, const GDate *last, const GDate *enum_begin, const GDate *enum_end, myPeriodEnumBetweenCb cb, void *user_data );
static void         period_enum_yearly( myPeriod *period, const GDate *last, const GDate *enum_begin, const GDate *enum_end, myPeriodEnumBetweenCb cb, void *user_data );
static guint        weekday_intern_to_glib( guint glib_weekday );
static const gchar *weekday_intern_to_abr( guint glib_weekday );

G_DEFINE_TYPE_EXTENDED( myPeriod, my_period, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( myPeriod ))

static void
settings_monitor_finalize( GObject *object )
{
	static const gchar *thisfn = "my_period_finalize";
	myPeriodPrivate *priv;

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_return_if_fail( object && MY_IS_PERIOD( object ));

	/* free data members here */
	priv = my_period_get_instance_private( MY_PERIOD( object ));

	g_list_free( priv->details );

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_period_parent_class )->finalize( object );
}

static void
settings_monitor_dispose( GObject *object )
{
	myPeriodPrivate *priv;

	g_return_if_fail( object && MY_IS_PERIOD( object ));

	priv = my_period_get_instance_private( MY_PERIOD( object ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_period_parent_class )->dispose( object );
}

static void
my_period_init( myPeriod *self )
{
	static const gchar *thisfn = "my_period_init";
	myPeriodPrivate *priv;

	g_return_if_fail( self && MY_IS_PERIOD( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = my_period_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	priv->key = MY_PERIOD_UNSET;
	priv->every = 0;
	priv->details = NULL;
}

static void
my_period_class_init( myPeriodClass *klass )
{
	static const gchar *thisfn = "my_period_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = settings_monitor_dispose;
	G_OBJECT_CLASS( klass )->finalize = settings_monitor_finalize;
}

/**
 * my_period_new:
 *
 * Returns: a new #myPeriod object which should be g_object_unref()
 * by the caller.
 */
myPeriod *
my_period_new( void )
{
	myPeriod *period;

	period = g_object_new( MY_TYPE_PERIOD, NULL );

	return( period );
}

/**
 * my_period_new_with_data:
 * @key: the periodicity identifier read from the DBMS.
 * @every: the every count in days.
 * @details: a comma-separated list of days number.
 *
 * Returns: a new #myPeriod object which should be g_object_unref()
 * by the caller.
 */
myPeriod *
my_period_new_with_data( const gchar *key, guint every, const gchar *details )
{
	myPeriod *period;
	myPeriodPrivate *priv;

	period = my_period_new();

	priv = my_period_get_instance_private( period );

	priv->key = my_period_key_from_dbms( key );
	priv->every = every;
	priv->details = my_utils_str_to_uint_list( details, st_sep );

	return( period );
}

/**
 * my_period_get_key:
 * @period: this #myPeriod object.
 *
 * Returns: the identifier of the @period.
 */
myPeriodKey
my_period_get_key( myPeriod *period )
{
	myPeriodPrivate *priv;

	g_return_val_if_fail( period && MY_IS_PERIOD( period ), MY_PERIOD_UNSET );

	priv = my_period_get_instance_private( period );
	g_return_val_if_fail( !priv->dispose_has_run, MY_PERIOD_UNSET );

	return( priv->key );
}

/**
 * my_period_get_every:
 * @period: this #myPeriod object.
 *
 * Returns: the every count of the @period.
 */
guint
my_period_get_every( myPeriod *period )
{
	myPeriodPrivate *priv;

	g_return_val_if_fail( period && MY_IS_PERIOD( period ), 0 );

	priv = my_period_get_instance_private( period );
	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	return( priv->every );
}

/**
 * my_period_get_details:
 * @period: this #myPeriod object.
 *
 * Returns: the list of details.
 *
 * The returned #GList is owned by the @period object, and should not
 * be released by the caller.
 */
GList *
my_period_get_details( myPeriod *period )
{
	myPeriodPrivate *priv;

	g_return_val_if_fail( period && MY_IS_PERIOD( period ), NULL );

	priv = my_period_get_instance_private( period );
	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->details );
}

/**
 * my_period_get_details_str_i:
 * @period: this #myPeriod object.
 *
 * Returns: the details as a comma-separated list of (internal) integers.
 *
 * The returned string is a newly allocated one which should be g_free()
 * by the caller.
 */
gchar *
my_period_get_details_str_i( myPeriod *period )
{
	myPeriodPrivate *priv;
	GString *str;
	GList *it;

	g_return_val_if_fail( period && MY_IS_PERIOD( period ), NULL );

	priv = my_period_get_instance_private( period );
	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	str = g_string_new( "" );
	for( it=priv->details ; it ; it=it->next ){
		if( str->len ){
			g_string_append_printf( str, "%s", st_sep );
		}
		g_string_append_printf( str, "%u", GPOINTER_TO_UINT( it->data ));
	}

	return( g_string_free( str, FALSE ));
}

/**
 * my_period_get_details_str_s:
 * @period: this #myPeriod object.
 *
 * Returns: the details as a comma-separated list of labels.
 *
 * Labels are only set if the periodicity is 'weekly' as this is the
 * only case where the label makes sense.
 *
 * The returned string is a newly allocated one which should be g_free()
 * by the caller.
 */
gchar *
my_period_get_details_str_s( myPeriod *period )
{
	myPeriodPrivate *priv;
	GString *str;
	guint det_i;
	GList *it;

	g_return_val_if_fail( period && MY_IS_PERIOD( period ), NULL );

	priv = my_period_get_instance_private( period );
	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	str = g_string_new( "" );
	for( it=priv->details ; it ; it=it->next ){
		if( str->len ){
			g_string_append_printf( str, "%s", st_sep );
		}
		det_i = GPOINTER_TO_UINT( it->data );
		if( priv->key == MY_PERIOD_WEEKLY ){
			g_string_append_printf( str, "%s", weekday_intern_to_abr( det_i ));
		} else {
			g_string_append_printf( str, "%u", det_i );
		}
	}

	return( g_string_free( str, FALSE ));
}

/**
 * my_period_set_key:
 * @period: this #myPeriod object.
 * @key: the #myPeriodKey periodicity.
 *
 * Set the periodicity of the @period.
 */
void
my_period_set_key( myPeriod *period, myPeriodKey key )
{
	myPeriodPrivate *priv;

	g_return_if_fail( period && MY_IS_PERIOD( period ));
	g_return_if_fail( key_is_valid( key ));

	priv = my_period_get_instance_private( period );
	g_return_if_fail( !priv->dispose_has_run );

	priv->key = key;
}

static gboolean
key_is_valid( myPeriodKey key )
{
	guint i;

	for( i=0 ; st_period[i].key ; ++i ){
		if( st_period[i].key == key ){
			return( TRUE );
		}
	}

	return( FALSE );
}

/**
 * my_period_set_every:
 * @period: this #myPeriod object.
 * @every: the every count.
 *
 * Set the every count of the @period.
 *
 * Should be >= 1, but a zero value is accepted, making the #myPeriod
 * invalid (will not enumerate any date).
 */
void
my_period_set_every( myPeriod *period, guint every )
{
	myPeriodPrivate *priv;

	g_return_if_fail( period && MY_IS_PERIOD( period ));

	priv = my_period_get_instance_private( period );
	g_return_if_fail( !priv->dispose_has_run );

	priv->every = every;
}

/**
 * my_period_set_details:
 * @period: this #myPeriod object.
 * @details: the list of details as a comma-separated string.
 *
 * Set the list of the details of the @period.
 */
void
my_period_set_details( myPeriod *period, const gchar *details )
{
	myPeriodPrivate *priv;
	GList *uint_list;

	g_return_if_fail( period && MY_IS_PERIOD( period ));

	priv = my_period_get_instance_private( period );
	g_return_if_fail( !priv->dispose_has_run );

	uint_list = my_utils_str_to_uint_list( details, st_sep );
	g_list_free( priv->details );
	priv->details = uint_list;
}

/**
 * my_period_details_add:
 * @period: this #myPeriod object.
 * @det: the detail identifier to be added.
 *
 * Add the @det to the list of details.
 *
 * We take care here of keeping the list sorted in ascending order.
 */
void
my_period_details_add( myPeriod *period, guint det )
{
	myPeriodPrivate *priv;
	GList *it;
	gboolean found;
	guint n;

	g_return_if_fail( period && MY_IS_PERIOD( period ));

	priv = my_period_get_instance_private( period );
	g_return_if_fail( !priv->dispose_has_run );

	found = FALSE;
	for( it=priv->details ; it ; it=it->next ){
		n = GPOINTER_TO_UINT( it->data );
		if( n == det ){
			found = TRUE;
			break;
		}
	}

	if( !found ){
		priv->details = g_list_insert_sorted( priv->details, GUINT_TO_POINTER( det ), ( GCompareFunc ) details_sort_fn );
	}
}

static gint
details_sort_fn( gconstpointer ptra, gconstpointer ptrb )
{
	guint a, b;

	a = GPOINTER_TO_UINT( ptra );
	b = GPOINTER_TO_UINT( ptrb );

	return( a < b ? -1 : ( a > b ? 1 : 0 ));
}

/**
 * my_period_details_remove:
 * @period: this #myPeriod object.
 * @det: the detail identifier to be removed.
 *
 * Remove the @det from the list of details.
 */
void
my_period_details_remove( myPeriod *period, guint det )
{
	myPeriodPrivate *priv;

	g_return_if_fail( period && MY_IS_PERIOD( period ));

	priv = my_period_get_instance_private( period );
	g_return_if_fail( !priv->dispose_has_run );

	priv->details = g_list_remove( priv->details, GUINT_TO_POINTER( det ));
}

/**
 * my_period_enum_key:
 * @cb: the user callback.
 * @user_data: a user data provided pointer.
 *
 * Enumerates all known and managed periodicity identifiers.
 */
void
my_period_enum_key( myPeriodEnumKeyCb cb, void *user_data )
{
	guint i;

	for( i=0 ; st_period[i].key ; ++i ){
		( *cb )( st_period[i].key, user_data );
	}
}

/**
 * my_period_enum_between:
 * @period: this #myPeriod instance.
 * @last: [allow-none]: the last generation date.
 * @max_end: [allow-none]: the max generation date.
 * @enum_begin: the beginning date.
 * @enum_end: the ending date.
 * @cb: the user callback.
 * @user_data: a user data provided pointer.
 *
 * Enumerates all valid dates between @per_begin and @per_end included dates.
 * No date is generated after @end.
 */
void
my_period_enum_between( myPeriod *period, const GDate *last, const GDate *max_end,
							const GDate *enum_begin, const GDate *enum_end, myPeriodEnumBetweenCb cb, void *user_data )
{
	myPeriodPrivate *priv;
	myPeriodKey key;
	GDate date_end;

	g_return_if_fail( period && MY_IS_PERIOD( period ));

	priv = my_period_get_instance_private( period );
	g_return_if_fail( !priv->dispose_has_run );

	g_return_if_fail( my_date_is_valid( enum_begin ));
	g_return_if_fail( my_date_is_valid( enum_end ));
	g_return_if_fail( cb != NULL );

	/* last iteration date
	 * first of max_end and enum_end */
	if( !my_date_is_valid( max_end ) || my_date_compare( max_end, enum_end ) > 0 ){
		my_date_set_from_date( &date_end, enum_end );
	} else {
		my_date_set_from_date( &date_end, max_end );
	}

	key = my_period_get_key( period );

	switch( key ){
		case MY_PERIOD_DAILY:
			period_enum_daily( period, last, enum_begin, max_end, cb, user_data );
			break;
		case MY_PERIOD_WEEKLY:
			period_enum_weekly( period, last, enum_begin, max_end, cb, user_data );
			break;
		case MY_PERIOD_MONTHLY:
			period_enum_monthly( period, last, enum_begin, max_end, cb, user_data );
			break;
		case MY_PERIOD_YEARLY:
			period_enum_yearly( period, last, enum_begin, max_end, cb, user_data );
			break;
		default:
			break;
	}
}

static void
period_enum_daily( myPeriod *period, const GDate *last,
						const GDate *enum_begin, const GDate *enum_end, myPeriodEnumBetweenCb cb, void *user_data )
{
	GDate date;
	guint every;

	every = my_period_get_every( period );
	g_return_if_fail( every >= 1 );

	/* first iteration date:
	 * may be far earlier than enum_begin */
	if( my_date_is_valid( last )){
		my_date_set_from_date( &date, last );
		g_date_add_days( &date, every );

	} else {
		my_date_set_from_date( &date, enum_begin );
	}

	while( my_date_compare( &date, enum_end ) <= 0 ){
		( *cb )( &date, user_data );
		g_date_add_days( &date, every );
	}
}

/*
 * Example:
 * - every=2: every two weeks
 * - details=2,4: have tuesday+thursday
 *
 * Starting from <date>
 *   intern_week_day=1
 *   while <= 7
 *     while <= enum_end
 *       if >= enum_begin
 *         if tuesday or thursday, then call <cb>
 */
static void
period_enum_weekly( myPeriod *period, const GDate *last,
						const GDate *enum_begin, const GDate *enum_end, myPeriodEnumBetweenCb cb, void *user_data )
{
	GDate date, wmonday;
	guint every, wnum;
	GList *details, *it;
	GDateWeekday wday, wtarget;

	/* first iteration date:
	 * may be far earlier than enum_begin */
	if( my_date_is_valid( last )){
		my_date_set_from_date( &date, last );
		g_date_add_days( &date, 1 );

	} else {
		my_date_set_from_date( &date, enum_begin );
	}

	/* examine the beginning date,
	 * computing its week number (easy)
	 * computing the date of the monday of the begin of the week
	 * => wmonday is the first day of the 'last' week */
	wday = g_date_get_weekday( &date );
	my_date_set_from_date( &wmonday, &date );
	g_date_subtract_days( &wmonday, wday-G_DATE_MONDAY ); /* monday is our first day of week */

	every = my_period_get_every( period );
	g_return_if_fail( every >= 1 );

	details = my_period_get_details( period );

	while( my_date_compare( &date, enum_end ) <= 0 ){
		wday = g_date_get_weekday( &date );
		while( TRUE ){
			for( it=details ; it ; it=it->next ){
				wnum = GPOINTER_TO_UINT( it->data );
				wtarget = weekday_intern_to_glib( wnum );
				if( wtarget == wday &&
						my_date_compare( &date, enum_begin ) >= 0  &&
						my_date_compare( &date, enum_end ) <= 0 ){
					( *cb )( &date, user_data );
				}
			}
			if( wday == G_DATE_SUNDAY ){
				break;
			}
			g_date_add_days( &date, 1 );
			wday += 1;
		}
		g_date_add_days( &wmonday, 7*every );
		my_date_set_from_date( &date, &wmonday );
	}
}

static void
period_enum_monthly( myPeriod *period, const GDate *last, const GDate *enum_begin, const GDate *enum_end, myPeriodEnumBetweenCb cb, void *user_data )
{
	GDate date;
	guint every, dnum, ddate, month;
	GList *details, *it;

	/* first iteration date:
	 * may be far earlier than enum_begin */
	if( my_date_is_valid( last )){
		my_date_set_from_date( &date, last );
		g_date_add_days( &date, 1 );

	} else {
		my_date_set_from_date( &date, enum_begin );
	}

	every = my_period_get_every( period );
	g_return_if_fail( every >= 1 );

	details = my_period_get_details( period );

	while( my_date_compare( &date, enum_end ) <= 0 ){
		ddate = g_date_get_day( &date );
		while( TRUE ){
			for( it=details ; it ; it=it->next ){
				dnum = GPOINTER_TO_UINT( it->data );
				if( dnum == ddate &&
						my_date_compare( &date, enum_begin ) >= 0  &&
						my_date_compare( &date, enum_end ) <= 0 ){
					( *cb )( &date, user_data );
				}
			}
			g_date_add_days( &date, 1 );
			ddate = g_date_get_day( &date );
			if( ddate == 1 ){
				break;
			}
		}
		month = g_date_get_month( &date );
		month += every-1;
		g_date_set_month( &date, month );
	}
}

static void
period_enum_yearly( myPeriod *period, const GDate *last, const GDate *enum_begin, const GDate *enum_end, myPeriodEnumBetweenCb cb, void *user_data )
{
	GDate date;
	guint every, dnum, ddate, year;
	GList *details, *it;

	/* first iteration date:
	 * may be far earlier than enum_begin */
	if( my_date_is_valid( last )){
		my_date_set_from_date( &date, last );
		g_date_add_days( &date, 1 );

	} else {
		my_date_set_from_date( &date, enum_begin );
	}

	every = my_period_get_every( period );
	g_return_if_fail( every >= 1 );

	details = my_period_get_details( period );

	while( my_date_compare( &date, enum_end ) <= 0 ){
		ddate = g_date_get_day_of_year( &date );
		while( TRUE ){
			for( it=details ; it ; it=it->next ){
				dnum = GPOINTER_TO_UINT( it->data );
				if( dnum == ddate &&
						my_date_compare( &date, enum_begin ) >= 0  &&
						my_date_compare( &date, enum_end ) <= 0 ){
					( *cb )( &date, user_data );
				}
			}
			g_date_add_days( &date, 1 );
			ddate = g_date_get_day_of_year( &date );
			if( ddate == 1 ){
				break;
			}
		}
		year = g_date_get_year( &date );
		year += every-1;
		g_date_set_year( &date, year );
	}
}

/**
 * my_period_enum_details:
 * @key: a #myPeriodKey identifier.
 * @cb: the user callback.
 * @user_data: a user data provided pointer.
 *
 * Enumerates available details.
 */
void
my_period_enum_details( myPeriodKey key, myPeriodEnumDetailsCb cb, void *user_data )
{
	guint i;
	gchar *id_str;

	g_return_if_fail( key_is_valid( key ));
	g_return_if_fail( cb != NULL );

	switch( key ){
		case MY_PERIOD_WEEKLY:
			for( i=0 ; st_day_of_week[i].int_weekday ; ++i ){
				id_str = g_strdup_printf( "%u", st_day_of_week[i].int_weekday );
				( *cb )( st_day_of_week[i].int_weekday, id_str,
							gettext( st_day_of_week[i].abr ), gettext( st_day_of_week[i].label ),
							user_data );
				g_free( id_str );
			}
			break;
		case MY_PERIOD_MONTHLY:
			for( i=1 ; i<=31 ; ++i ){
				id_str = g_strdup_printf( "%u", i );
				( *cb )( i, id_str, id_str, id_str, user_data );
				g_free( id_str );
			}
			break;
		case MY_PERIOD_YEARLY:
			for( i=1 ; i<=365 ; ++i ){
				id_str = g_strdup_printf( "%u", i );
				( *cb )( i, id_str, id_str, id_str, user_data );
				g_free( id_str );
			}
			break;
		default:
			break;
	}
}

/*
 * Convert a internal day of week (1..7) to its GLib equivalent
 */
static guint
weekday_intern_to_glib( guint int_weekday )
{
	guint i;

	for( i=0 ; st_day_of_week[i].int_weekday ; ++i ){
		if( st_day_of_week[i].int_weekday == int_weekday ){
			return( st_day_of_week[i].glib_weekday );
		}
	}

	return( 0 );
}

/*
 * Convert a internal day of week (1..7) to its abbreviated label
 */
static const gchar *
weekday_intern_to_abr( guint int_weekday )
{
	guint i;

	for( i=0 ; st_day_of_week[i].int_weekday ; ++i ){
		if( st_day_of_week[i].int_weekday == int_weekday ){
			return( st_day_of_week[i].abr );
		}
	}

	return( "" );
}

/**
 * my_period_key_from_dbms:
 * @dbms: the periodicity identifier read from the DBMS.
 *
 * Returns: the #myPeriodKey corresponding to the @dbms identifier.
 */
myPeriodKey
my_period_key_from_dbms( const gchar *dbms )
{
	static const gchar *thisfn = "my_period_key_from_dbms";
	gint i;

	g_return_val_if_fail( my_strlen( dbms ), MY_PERIOD_UNSET );

	for( i=0 ; st_period[i].key ; ++i ){
		if( !my_collate( st_period[i].dbms, dbms )){
			return( st_period[i].key );
		}
	}

	g_warning( "%s: unknown or invalid dbms periodicity indicator: %s", thisfn, dbms );

	return( MY_PERIOD_UNSET );
}

/**
 * my_period_key_get_dbms:
 * @key: a periodicity identifier.
 *
 * Returns: the dbms string corresponding to the @period.
 */
const gchar *
my_period_key_get_dbms( myPeriodKey key )
{
	static const gchar *thisfn = "my_period_key_get_dbms";
	gint i;

	for( i=0 ; st_period[i].key ; ++i ){
		if( st_period[i].key == key ){
			return( st_period[i].dbms );
		}
	}

	g_warning( "%s: unknown or invalid periodicity identifier: %u", thisfn, key );

	return( NULL );
}

/**
 * my_period_key_get_abr:
 * @key: a periodicity identifier.
 *
 * Returns: the abbreviated localized string corresponding to the @period.
 */
const gchar *
my_period_key_get_abr( myPeriodKey key )
{
	static const gchar *thisfn = "my_period_key_get_abr";
	gint i;

	for( i=0 ; st_period[i].key ; ++i ){
		if( st_period[i].key == key ){
			return( gettext( st_period[i].abr ));
		}
	}

	g_warning( "%s: unknown or invalid periodicity identifier: %u", thisfn, key );

	return( "" );
}

/**
 * my_period_key_get_label:
 * @key: a periodicity identifier.
 *
 * Returns: the abbreviated localized label corresponding to the @period.
 */
const gchar *
my_period_key_get_label( myPeriodKey key )
{
	static const gchar *thisfn = "my_period_key_get_label";
	gint i;

	for( i=0 ; st_period[i].key ; ++i ){
		if( st_period[i].key == key ){
			return( gettext( st_period[i].label ));
		}
	}

	g_warning( "%s: unknown or invalid periodicity identifier: %u", thisfn, key );

	return( "" );
}
