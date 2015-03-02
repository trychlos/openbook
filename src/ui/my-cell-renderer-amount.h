/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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
 */

#ifndef __MY_CELL_RENDERER_AMOUNT_H__
#define __MY_CELL_RENDERER_AMOUNT_H__

/**
 * SECTION: my_cell_renderer_amount
 * @short_description: #my_cell_renderer_amount set of functions
 * @include: ui/my-cell-renderer-amount.h
 *
 * This class lets the user enter amounts in cell renderers.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

void my_cell_renderer_amount_init( GtkCellRenderer *renderer );

G_END_DECLS

#endif /* __MY_CELL_RENDERER_AMOUNT_H__ */
