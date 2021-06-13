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

#ifndef __MY_API_MY_PERIOD_H__
#define __MY_API_MY_PERIOD_H__

/* @title: myPeriod
 * @short_description: The myPeriod Class Definition
 * @include: my/my-period.h
 *
 * The #myPeriod class provides some useful methods to define and
 * manage periodic events.
 *
 * - Daily:
 *   may be every <n> days;
 *     n <= 0 is invalid
 *     n > 0 means new_date = prev_date + n
 *     n mini = 1, which means every day
 *     default = 1
 *   configuration as <daily> + <n>
 *
 * - Weekly:
 *   may be every <n> weeks (zero to 7 times by week)
 *   may select 0 to all between monday, tuesday, wednesday, thursday, friday, saturday, sunday
 *     new_week = week_of_prev_date + n
 *     during this week (monday to sunday), we will generate one record
 *     foreach selected day of the week
 *   configuration as <weekly> + <n> + <list_of_days_of_week>
 *
 * - Monthly:
 *   may be every <n> months (zero to 31 times by month)
 *   may add a comma-separated list of numbers 0..31
 *     new_month = month_of_prev_date + n
 *     during this month (1 to 31), we will generate one record
 *     foreach selected day of the month
 *   configuration as <monthly> + <n> + <list_of_days_of_month>
 *
 * - Yearly:
 *   may be every <n> years (zero to 365 times by month)
 *   may add a comma-separated list of numbers 0..365
 *     new_year = year_of_prev_date + n
 *     during this year (1 to 365), we will generate one record
 *     foreach quantieme of the list
 *   configuration as <yearly> + <n> + <list_of_quantieme>
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define MY_TYPE_PERIOD                ( my_period_get_type())
#define MY_PERIOD( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_PERIOD, myPeriod ))
#define MY_PERIOD_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_PERIOD, myPeriodClass ))
#define MY_IS_PERIOD( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_PERIOD ))
#define MY_IS_PERIOD_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_PERIOD ))
#define MY_PERIOD_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_PERIOD, myPeriodClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	myPeriod;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	myPeriodClass;

/**
 * myPeriodKey:
 *
 * This is an invariant which identifies the periodicity.
 *
 * This cannot be configurable as the #my_period_enum_between() method
 * must know how to deal with each periodicity.
 */
typedef enum {
	MY_PERIOD_UNSET = 1,
	MY_PERIOD_DAILY,
	MY_PERIOD_WEEKLY,
	MY_PERIOD_MONTHLY,
	MY_PERIOD_YEARLY
}
	myPeriodKey;

/**
 * myPeriodEnumKeyCb:
 *
 * The #my_period_enum_key() callback.
 *
 * Enumerates all known and managed periodicity identifiers.
 */
typedef void (*myPeriodEnumKeyCb)       ( myPeriodKey key,
												void *user_data );

/**
 * myPeriodEnumBetweenCb:
 *
 * The #my_period_enum_between() callback.
 *
 * Enumerates all dates compatible with the configured periodicity, and
 * included in the provided interval.
 */
typedef void (*myPeriodEnumBetweenCb)   ( const GDate *date,
												void *user_data );

/**
 * myPeriodEnumDetailsCb:
 *
 * The #my_period_enum_details() callback.
 *
 * Enumerates available details for the given periodicity.
 */
typedef void (*myPeriodEnumDetailsCb)   ( guint idn,
												const gchar *ids,
												const gchar *abr,
												const gchar *label,
												void *user_data );

GType        my_period_get_type         ( void ) G_GNUC_CONST;

myPeriod    *my_period_new              ( void );

myPeriod    *my_period_new_with_data    ( const gchar *key,
												guint every,
												const gchar *details );

myPeriodKey  my_period_get_key          ( myPeriod *period );
guint        my_period_get_every        ( myPeriod *period );
GList       *my_period_get_details      ( myPeriod *period );
gchar       *my_period_get_details_str_i( myPeriod *period );
gchar       *my_period_get_details_str_s( myPeriod *period );

void         my_period_set_key          ( myPeriod *period, myPeriodKey key );
void         my_period_set_every        ( myPeriod *period, guint every );
void         my_period_set_details      ( myPeriod *period, const gchar *details );
void         my_period_details_add      ( myPeriod *period, guint det );
void         my_period_details_remove   ( myPeriod *period, guint det );

gboolean     my_period_is_empty         ( myPeriod *period );
gboolean     my_period_is_valid         ( myPeriod *period, gchar **msgerr );

void         my_period_enum_key         ( myPeriodEnumKeyCb cb,
												void *user_data );

void         my_period_enum_between     ( myPeriod *period,
												const GDate *last,
												const GDate *enum_begin,
												const GDate *enum_end,
												myPeriodEnumBetweenCb cb,
												void *user_data );

void         my_period_enum_details     ( myPeriodKey key,
												myPeriodEnumDetailsCb cb,
												void *user_data );

myPeriodKey  my_period_key_from_dbms    ( const gchar *dbms );

const gchar *my_period_key_get_dbms     ( myPeriodKey key );
const gchar *my_period_key_get_abr      ( myPeriodKey key );
const gchar *my_period_key_get_label    ( myPeriodKey key );

G_END_DECLS

#endif /* __MY_API_MY_PERIOD_H__ */
