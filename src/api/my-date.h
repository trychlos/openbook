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

#ifndef __MY_DATE_H__
#define __MY_DATE_H__

/**
 * SECTION: my_date
 * @short_description: Miscellaneous utilities for GDate management
 * @include: ui/my-date.h
 */

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * myDateFormat:
 * --------------+------------+----------------------------------+
 *               |            |               usage              |
 *               | display as | display | entry | sql | filename |
 * --------------+------------+---------+-------+-----+----------+
 * @MY_DATE_DMMM | d mmm yyyy |    X    |       |     |          |
 * @MY_DATE_DMYY | dd/mm/yyyy |    X    |   X   |     |          |
 * @MY_DATE_SQL  | yyyy-mm-dd |         |       |  X  |          |
 * @MY_DATE_YYMD |  yyyymmdd  |         |       |     |     X    |
 * --------------+------------+---------+-------+-----+----------+
 */
typedef enum {
	MY_DATE_FIRST = 0,
	MY_DATE_DMMM,
	MY_DATE_DMYY,
	MY_DATE_SQL,
	MY_DATE_YYMD,
	MY_DATE_LAST
}
	myDateFormat;

void       my_date_clear         ( GDate *date );

gboolean   my_date_is_valid      ( const GDate *date );

gint       my_date_compare       ( const GDate *a, const GDate *b );
gint       my_date_compare_ex    ( const GDate *a, const GDate *b, gboolean clear_is_past_infinite );

GDate     *my_date_set_now       ( GDate *date );
GDate     *my_date_set_from_date ( GDate *date, const GDate *orig );
GDate     *my_date_set_from_sql  ( GDate *date, const gchar *sql_string );
GDate     *my_date_set_from_str  ( GDate *date, const gchar *fmt_string, myDateFormat format );

gchar     *my_date_to_str        ( const GDate *date, myDateFormat format );

G_END_DECLS

#endif /* __MY_DATE_H__ */
