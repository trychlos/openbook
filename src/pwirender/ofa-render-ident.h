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

#ifndef __OFA_RENDER_IDENT_H__
#define __OFA_RENDER_IDENT_H__

/**
 * SECTION: ofa_render_ident
 * @short_description: #ofaRenderIdent class definition.
 * @include: render/ofa-render-ident.h
 *
 * The class which provides identification to the dynamic plugin.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_RENDER_IDENT                ( ofa_render_ident_get_type())
#define OFA_RENDER_IDENT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RENDER_IDENT, ofaRenderIdent ))
#define OFA_RENDER_IDENT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RENDER_IDENT, ofaRenderIdentClass ))
#define OFA_IS_RENDER_IDENT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RENDER_IDENT ))
#define OFA_IS_RENDER_IDENT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RENDER_IDENT ))
#define OFA_RENDER_IDENT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RENDER_IDENT, ofaRenderIdentClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaRenderIdent;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaRenderIdentClass;

GType ofa_render_ident_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_RENDER_IDENT_H__ */
