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

#ifndef __OPENBOOK_API_MY_DOUBLE_H__
#define __OPENBOOK_API_MY_DOUBLE_H__

/**
 * SECTION: my_amount
 * @short_description: Miscellaneous utilities for gdouble management
 * @include: openbook/my-double.h
 */

#include <glib.h>

G_BEGIN_DECLS

gchar     *my_double_undecorate     ( const gchar *decorated );

gdouble    my_double_set_from_csv   ( const gchar *csv_string, gchar decimal_sep );
gdouble    my_double_set_from_sql   ( const gchar *sql_string );
gdouble    my_double_set_from_sql_ex( const gchar *sql_string, gint digits );

gdouble    my_double_set_from_str   ( const gchar *string );

gchar     *my_bigint_to_str         ( glong value );

gchar     *my_double_to_sql         ( gdouble value );
gchar     *my_double_to_sql_ex      ( gdouble value, gint decimals );
gchar     *my_double_to_str         ( gdouble value );
gchar     *my_double_to_str_ex      ( gdouble value, gint decimals );

G_END_DECLS

#endif /* __OPENBOOK_API_MY_DOUBLE_H__ */
