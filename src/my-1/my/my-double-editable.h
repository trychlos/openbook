/*
 * Open Freelance Accounting
 * A double-editable accounting application for freelances.
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

#ifndef __MY_API_MY_DOUBLE_EDITABLE_H__
#define __MY_API_MY_DOUBLE_EDITABLE_H__

/**
 * SECTION: my_editable_amount
 * @short_description: #my_editable_amount set of functons
 * @include: my/my-editable-amount.h
 *
 * This class lets the user enter amounts in entries.
 *
 * An editable amount may be entered with the following format:
 * - any digit(s)
 * - maybe a decimal dot
 * - any digit(s)
 * TODO: accept a leading sign
 * TODO: accept other decimal separators than the dot
 *
 * An editable amount is rendered with the following format:
 * - locale representation of a double
 * - with at least one digit before the decimal separator
 * - with the specified count of decimal digits
 * TODO: accept a different display format
 */

#include <gtk/gtk.h>

#include <my/my-double.h>

G_BEGIN_DECLS

void    my_double_editable_init            ( GtkEditable *editable );

void    my_double_editable_init_ex         ( GtkEditable *editable,
												gunichar thousand_sep,
												gunichar decimal_sep,
												gboolean accept_dot,
												gboolean accept_comma,
												gint decimals );

void    my_double_editable_set_decimals    ( GtkEditable *editable,
												gint decimals );

void    my_double_editable_set_thousand_sep( GtkEditable *editable,
												gunichar thousand_sep );

void    my_double_editable_set_decimal_sep ( GtkEditable *editable,
												gunichar decimal_sep );

void    my_double_editable_set_accept_dot  ( GtkEditable *editable,
												gboolean accept_dot );

void    my_double_editable_set_accept_comma( GtkEditable *editable,
												gboolean accept_comma );

gdouble my_double_editable_get_amount      ( GtkEditable *editable );

void    my_double_editable_set_amount      ( GtkEditable *editable,
												gdouble amount );

gchar  *my_double_editable_get_string      ( GtkEditable *editable );

void    my_double_editable_set_string      ( GtkEditable *editable,
												const gchar *string );

void    my_double_editable_set_changed_cb  ( GtkEditable *editable,
												GCallback cb,
												void *user_data );

G_END_DECLS

#endif /* __MY_API_MY_DOUBLE_EDITABLE_H__ */
