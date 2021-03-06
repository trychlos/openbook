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

#ifndef __MY_API_MY_DATE_H__
#define __MY_API_MY_DATE_H__

/**
 * SECTION: my_date
 * @short_description: Miscellaneous utilities for GDate management
 * @include: my/my-date.h
 */

#include <my/my-stamp.h>

G_BEGIN_DECLS

/**
 * myDateFormat:
 * ----------------+------------+--------------------------------------------+
 *                 |            |               usage                        |
 *                 | display as | display | entry | sql | filename | pdf-lcl |
 * ----------------+------------+---------+-------+-----+----------+---------+
 * @MY_DATE_DMMM   | d mmm yyyy |    X    |       |     |          |         |
 * @MY_DATE_MMYY   |  Mmm yyyy  |    X    |       |     |          |         |
 * @MY_DATE_DMYY   | dd/mm/yyyy |    X    |   X   |     |          |         |
 * @MY_DATE_SQL    | yyyy-mm-dd |         |       |  X  |          |         |
 * @MY_DATE_YYMD   |  yyyymmdd  |         |       |     |     X    |         |
 * @MY_DATE_DMYDOT | dd.mm.yyyy |         |       |     |          |    X    |
 * ----------------+------------+---------+-------+-----+----------+---------+
 *
 * MAINTAINER_NOTE: only add a new format at the end of the list as the
 * format number is stored as a user settings
 * + update my-date.c
 * + update my-editable-date.c
 */
typedef enum {
	MY_DATE_FIRST = 1,					/* formats must be greater than zero */
	MY_DATE_DMMM = MY_DATE_FIRST,
	MY_DATE_MMYY,
	MY_DATE_DMYY,
	MY_DATE_SQL,
	MY_DATE_YYMD,
	MY_DATE_DMYDOT,
	MY_DATE_LAST
}
	myDateFormat;

void         my_date_clear          ( GDate *date );

gboolean     my_date_is_valid       ( const GDate *date );

gint         my_date_compare        ( const GDate *a, const GDate *b );
gint         my_date_compare_ex     ( const GDate *a, const GDate *b, gboolean clear_is_past_infinite );
gint         my_date_compare_by_str ( const gchar *a, const gchar *b, myDateFormat format );

GDate       *my_date_set_now        ( GDate *date );
GDate       *my_date_set_from_date  ( GDate *date, const GDate *orig );
GDate       *my_date_set_from_sql   ( GDate *date, const gchar *string );
GDate       *my_date_set_from_str   ( GDate *date, const gchar *string, myDateFormat format );
GDate       *my_date_set_from_str_ex( GDate *date, const gchar *string, myDateFormat format, gint *year );
GDate       *my_date_set_from_stamp ( GDate *date, const myStampVal *stamp );

gchar       *my_date_to_str         ( const GDate *date, myDateFormat format );

const gchar *my_date_get_format_str ( myDateFormat format );

G_END_DECLS

#endif /* __MY_API_MY_DATE_H__ */
