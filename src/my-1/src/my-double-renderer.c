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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "my/my-double-editable.h"
#include "my/my-double-renderer.h"

/*
 * data attached to each GtkCellRenderer
 */
typedef struct {
	gunichar thousand_sep;
	gunichar decimal_sep;
	gboolean accept_dot;
	gboolean accept_comma;
	guint    decimals;
}
	sRenderer;

#define DEFAULT_DECIMALS                 2

#define DOUBLE_RENDERER_DATA            "my-double-renderer-data"

static void       on_editing_started( GtkCellRenderer *renderer, GtkCellEditable *editable, gchar *path, sRenderer *sdata );
static sRenderer *get_renderer_data( GtkCellRenderer *renderer );
static void       on_renderer_finalized( sRenderer *sdata, GObject *finalized_renderer );

/**
 * my_double_renderer_init:
 * @renderer: this #GtkCellRendererText object.
 * @thousand_sep:
 * @decimal_sep:
 * @accept_dot:
 * @accept_comma:
 * @decimals: if -1, then get default decimals count (2).
 *
 * Initialize a cell renderer which is created to enter an amount or
 * a rate. Is thought to be called once at cell renderer creation.
 */
void
my_double_renderer_init( GtkCellRenderer *renderer,
		gunichar thousand_sep, gunichar decimal_sep, gboolean accept_dot, gboolean accept_comma, gint decimals )
{
	static const gchar *thisfn = "my_double_renderer_init";
	sRenderer *sdata;

	g_debug( "%s: renderer=%p (%s)",
			thisfn, ( void * ) renderer, G_OBJECT_TYPE_NAME( renderer ));

	g_return_if_fail( renderer && GTK_IS_CELL_RENDERER_TEXT( renderer ));

	sdata = get_renderer_data( renderer );

	sdata->thousand_sep = thousand_sep;
	sdata->decimal_sep = decimal_sep;
	sdata->accept_dot = accept_dot;
	sdata->accept_comma = accept_comma;
	if( decimals >= 0 ){
		sdata->decimals = decimals;
	}

	g_signal_connect( renderer, "editing-started", G_CALLBACK( on_editing_started ), sdata );

	gtk_cell_renderer_set_alignment( renderer, 1.0, 0.5 );
}

/*
 * note that while the cell renderer is unique for all the column, the
 * editable is for itself specifique to each row
 */
static void
on_editing_started( GtkCellRenderer *renderer, GtkCellEditable *editable, gchar *path, sRenderer *sdata )
{
	my_double_editable_init_ex( GTK_EDITABLE( editable ),
			sdata->thousand_sep, sdata->decimal_sep, sdata->accept_dot, sdata->accept_comma, sdata->decimals );
}

static sRenderer *
get_renderer_data( GtkCellRenderer *renderer )
{
	sRenderer *sdata;

	sdata = ( sRenderer * ) g_object_get_data( G_OBJECT( renderer ), DOUBLE_RENDERER_DATA );

	if( !sdata ){
		sdata = g_new0( sRenderer, 1 );
		sdata->decimals = DEFAULT_DECIMALS;
		g_object_set_data( G_OBJECT( renderer ), DOUBLE_RENDERER_DATA, sdata );
		g_object_weak_ref( G_OBJECT( renderer ), ( GWeakNotify ) on_renderer_finalized, sdata );
	}

	return( sdata );
}

static void
on_renderer_finalized( sRenderer *sdata, GObject *finalized_renderer )
{
	static const gchar *thisfn = "my_double_renderer_on_renderer_finalized";

	g_debug( "%s: sdata=%p, finalized_renderer=%p (%s)",
			thisfn, ( void * ) sdata, ( void * ) finalized_renderer, G_OBJECT_TYPE_NAME( finalized_renderer ));

	g_free( sdata );
}
