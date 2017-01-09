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

#ifndef __MY_API_MY_DATE_EDITABLE_H__
#define __MY_API_MY_DATE_EDITABLE_H__

/**
 * SECTION: my_editable_date
 * @short_description: #my_editable_date set of functons
 * @include: my/my-editable-date.h
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

#include <my/my-date.h>

G_BEGIN_DECLS

void         my_date_editable_init         ( GtkEditable *editable );

void         my_date_editable_set_format   ( GtkEditable *editable,
													myDateFormat format );

void         my_date_editable_set_date     ( GtkEditable *editable,
													const GDate *date );

void         my_date_editable_set_label    ( GtkEditable *editable,
													GtkWidget *label,
													myDateFormat format );

void         my_date_editable_set_mandatory( GtkEditable *editable,
													gboolean mandatory );

void         my_date_editable_set_overwrite( GtkEditable *editable,
													gboolean overwrite );

const GDate *my_date_editable_get_date     ( GtkEditable *editable,
													gboolean *valid );

gboolean     my_date_editable_is_empty     ( GtkEditable *editable );

G_END_DECLS

#endif /* __MY_API_MY_DATE_EDITABLE_H__ */
