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

#ifndef __OFA_LEDGER_SUMMARY_RENDER_H__
#define __OFA_LEDGER_SUMMARY_RENDER_H__

/**
 * SECTION: ofa_ledger_summary_render
 * @short_description: #ofaLedgerSummaryRender class definition.
 * @include: ui/ofa-ledger-summary-render.h
 *
 * The class which manages the rendering (preview/print) of ledgers
 * summary.
 */

#include "ui/ofa-render-page.h"

G_BEGIN_DECLS

#define OFA_TYPE_LEDGER_SUMMARY_RENDER                ( ofa_ledger_summary_render_get_type())
#define OFA_LEDGER_SUMMARY_RENDER( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_LEDGER_SUMMARY_RENDER, ofaLedgerSummaryRender ))
#define OFA_LEDGER_SUMMARY_RENDER_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_LEDGER_SUMMARY_RENDER, ofaLedgerSummaryRenderClass ))
#define OFA_IS_LEDGER_SUMMARY_RENDER( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_LEDGER_SUMMARY_RENDER ))
#define OFA_IS_LEDGER_SUMMARY_RENDER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_LEDGER_SUMMARY_RENDER ))
#define OFA_LEDGER_SUMMARY_RENDER_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_LEDGER_SUMMARY_RENDER, ofaLedgerSummaryRenderClass ))

typedef struct _ofaLedgerSummaryRenderPrivate         ofaLedgerSummaryRenderPrivate;

typedef struct {
	/*< public members >*/
	ofaRenderPage                  parent;

	/*< private members >*/
	ofaLedgerSummaryRenderPrivate *priv;
}
	ofaLedgerSummaryRender;

typedef struct {
	/*< public members >*/
	ofaRenderPageClass             parent;
}
	ofaLedgerSummaryRenderClass;

GType ofa_ledger_summary_render_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_LEDGER_SUMMARY_RENDER_H__ */