/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_RECONCIL_RENDER_H__
#define __OFA_RECONCIL_RENDER_H__

/**
 * SECTION: ofa_reconcil_render
 * @short_description: #ofaReconcilRender class definition.
 * @include: core/ofa-reconcil-render.h
 *
 * The class which manages the rendering (preview/print) of reconciliations.
 *
 * We are trying to:
 * - get the solde of the account at the requested effect date
 * - get the most closest BAT file.
 *
 * ofaIRenderable group management:
 * - group of entry lines: no header, no footer
 * - group of bat lines: one title line, no footer
 *
 * ofaIRenderable page report management:
 * - top/bottom report: current solde of the account
 *
 * ofaIRenderable last summary:
 * - total of debits and credits of entry+bat lines
 * - last solde of the account
 * - somes notes
 * - comparison with bat solde
 */

#include "api/ofa-render-page.h"

G_BEGIN_DECLS

#define OFA_TYPE_RECONCIL_RENDER                ( ofa_reconcil_render_get_type())
#define OFA_RECONCIL_RENDER( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RECONCIL_RENDER, ofaReconcilRender ))
#define OFA_RECONCIL_RENDER_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RECONCIL_RENDER, ofaReconcilRenderClass ))
#define OFA_IS_RECONCIL_RENDER( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RECONCIL_RENDER ))
#define OFA_IS_RECONCIL_RENDER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RECONCIL_RENDER ))
#define OFA_RECONCIL_RENDER_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RECONCIL_RENDER, ofaReconcilRenderClass ))

typedef struct {
	/*< public members >*/
	ofaRenderPage      parent;
}
	ofaReconcilRender;

typedef struct {
	/*< public members >*/
	ofaRenderPageClass parent;
}
	ofaReconcilRenderClass;

GType ofa_reconcil_render_get_type   ( void ) G_GNUC_CONST;

void  ofa_reconcil_render_set_account( ofaReconcilRender *page,
												const gchar *account_number );

G_END_DECLS

#endif /* __OFA_RECONCIL_RENDER_H__ */
