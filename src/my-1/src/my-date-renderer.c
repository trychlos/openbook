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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "my/my-date-editable.h"
#include "my/my-date-renderer.h"

static void on_editing_started( GtkCellRenderer *renderer, GtkCellEditable *editable, gchar *path, gpointer user_data );

/**
 * my_date_renderer_init:
 * @renderer: this #GtkCellRendererText object
 *
 * Initialise a cell renderer which is created to enter adate.
 *  Is thought to be called once at cell renderer creation.
 */
void
my_date_renderer_init( GtkCellRenderer *renderer )
{
	static const gchar *thisfn = "my_date_renderer_init";

	g_debug( "%s: renderer=%p (%s)",
			thisfn, ( void * ) renderer, G_OBJECT_TYPE_NAME( renderer ));

	g_return_if_fail( renderer && GTK_IS_CELL_RENDERER_TEXT( renderer ));

	g_signal_connect(
			G_OBJECT( renderer ),
			"editing-started",
			G_CALLBACK( on_editing_started ),
			NULL );
}

/*
 * note that while the cell renderer is unique for all the column, the
 * editable is for itself specifique to each row
 */
static void
on_editing_started( GtkCellRenderer *renderer, GtkCellEditable *editable, gchar *path, gpointer user_data )
{
	my_date_editable_init( GTK_EDITABLE( editable ));
}
