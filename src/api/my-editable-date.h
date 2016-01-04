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

#ifndef __OPENBOOK_API_MY_EDITABLE_DATE_H__
#define __OPENBOOK_API_MY_EDITABLE_DATE_H__

/**
 * SECTION: my_editable_date
 * @short_description: #my_editable_date set of functons
 * @include: openbook/my-editable-date.h
 *
 * This class lets the user enter dates in entries.
 *
 * An editable date may be entered with the following format:
 * - several formats available
 *
 * An editable date is rendered with the following format:
 * - several formats available
 */

#include <gtk/gtk.h>

#include "api/my-date.h"

G_BEGIN_DECLS

void         my_editable_date_init            ( GtkEditable *editable );

void         my_editable_date_set_format      ( GtkEditable *editable, myDateFormat format );

void         my_editable_date_set_date        ( GtkEditable *editable, const GDate *date );

void         my_editable_date_set_label       ( GtkEditable *editable, GtkWidget *label, myDateFormat format );

void         my_editable_date_set_mandatory   ( GtkEditable *editable, gboolean mandatory );

const GDate *my_editable_date_get_date        ( GtkEditable *editable, gboolean *valid );

gboolean     my_editable_date_is_empty        ( GtkEditable *editable );

G_END_DECLS

#endif /* __OPENBOOK_API_MY_EDITABLE_DATE_H__ */
