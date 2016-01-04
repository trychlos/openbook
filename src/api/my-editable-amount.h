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

#ifndef __OPENBOOK_API_MY_EDITABLE_AMOUNT_H__
#define __OPENBOOK_API_MY_EDITABLE_AMOUNT_H__

/**
 * SECTION: my_editable_amount
 * @short_description: #my_editable_amount set of functons
 * @include: openbook/my-editable-amount.h
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

G_BEGIN_DECLS

void    my_editable_amount_init          ( GtkEditable *editable );

void    my_editable_amount_init_ex       ( GtkEditable *editable,
													gint decimals );

void    my_editable_amount_set_decimals  ( GtkEditable *editable,
													gint decimals );

gdouble my_editable_amount_get_amount    ( GtkEditable *editable );

void    my_editable_amount_set_amount    ( GtkEditable *editable,
													gdouble amount );

gchar  *my_editable_amount_get_string    ( GtkEditable *editable );

void    my_editable_amount_set_string    ( GtkEditable *editable,
													const gchar *string );

void    my_editable_amount_set_changed_cb( GtkEditable *editable,
													GCallback cb,
													void *user_data );

G_END_DECLS

#endif /* __OPENBOOK_API_MY_EDITABLE_AMOUNT_H__ */
