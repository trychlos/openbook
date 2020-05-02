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

#ifndef __OFA_RENDER_AREA_H__
#define __OFA_RENDER_AREA_H__

/**
 * SECTION: ofa_render_area
 * @short_description: #ofaRenderArea class definition.
 * @include: core/ofa-render-area.h
 *
 * A #ofaRenderArea is just a #GtkDrawingArea inside of a #GtkScrolledWindow.
 * It implements the #ofaIContext interface.
 */

#include <cairo.h>
#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_RENDER_AREA                ( ofa_render_area_get_type())
#define OFA_RENDER_AREA( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RENDER_AREA, ofaRenderArea ))
#define OFA_RENDER_AREA_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RENDER_AREA, ofaRenderAreaClass ))
#define OFA_IS_RENDER_AREA( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RENDER_AREA ))
#define OFA_IS_RENDER_AREA_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RENDER_AREA ))
#define OFA_RENDER_AREA_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RENDER_AREA, ofaRenderAreaClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaRenderArea;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaRenderAreaClass;

GType          ofa_render_area_get_type        ( void ) G_GNUC_CONST;

ofaRenderArea *ofa_render_area_new             ( ofaIGetter *getter );

void           ofa_render_area_set_page_size   ( ofaRenderArea *area,
													gdouble width,
													gdouble height );

void           ofa_render_area_set_page_margins( ofaRenderArea *area,
													gdouble outside,
													gdouble between );

void           ofa_render_area_set_render_size ( ofaRenderArea *area,
													gdouble width,
													gdouble height );

void           ofa_render_area_clear           ( ofaRenderArea *area );

cairo_t       *ofa_render_area_new_context     ( ofaRenderArea *area );

void           ofa_render_area_append_page     ( ofaRenderArea *area,
													cairo_t *page );

void           ofa_render_area_queue_draw      ( ofaRenderArea *area );

G_END_DECLS

#endif /* __OFA_RENDER_AREA_H__ */
