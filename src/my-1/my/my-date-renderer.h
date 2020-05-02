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

#ifndef __MY_API_MY_DATE_RENDERER_H__
#define __MY_API_MY_DATE_RENDERER_H__

/**
 * SECTION: my_cell_renderer_date
 * @short_description: #my_cell_renderer_date set of functions
 * @include: my/my-cell-renderer-date.h
 *
 * This class lets the user enter dates in cell renderers.
 */

#include <gtk/gtk.h>

#include <my/my-date.h>

G_BEGIN_DECLS

void my_date_renderer_init( GtkCellRenderer *renderer );

G_END_DECLS

#endif /* __MY_API_MY_DATE_RENDERER_H__ */
