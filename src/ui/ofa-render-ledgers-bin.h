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
 */

#ifndef __OFA_RENDER_LEDGERS_BIN_H__
#define __OFA_RENDER_LEDGERS_BIN_H__

/**
 * SECTION: ofa_render_ledgers_bin
 * @short_description: #ofaRenderLedgersBin class definition.
 * @include: ui/ofa-render-ledgers-bin.h
 *
 * Display a frame with let the user select the parameters needed to
 * print the entries ledgers between two effect dates.
 */

#include "ui/ofa-idates-filter.h"
#include "ui/ofa-ledger-treeview.h"
#include "ui/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_RENDER_LEDGERS_BIN                ( ofa_render_ledgers_bin_get_type())
#define OFA_RENDER_LEDGERS_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RENDER_LEDGERS_BIN, ofaRenderLedgersBin ))
#define OFA_RENDER_LEDGERS_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RENDER_LEDGERS_BIN, ofaRenderLedgersBinClass ))
#define OFA_IS_RENDER_LEDGERS_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RENDER_LEDGERS_BIN ))
#define OFA_IS_RENDER_LEDGERS_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RENDER_LEDGERS_BIN ))
#define OFA_RENDER_LEDGERS_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RENDER_LEDGERS_BIN, ofaRenderLedgersBinClass ))

typedef struct _ofaRenderLedgersBinPrivate         ofaRenderLedgersBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                      parent;

	/*< private members >*/
	ofaRenderLedgersBinPrivate *priv;
}
	ofaRenderLedgersBin;

typedef struct {
	/*< public members >*/
	GtkBinClass                 parent;
}
	ofaRenderLedgersBinClass;

GType                ofa_render_ledgers_bin_get_type  ( void ) G_GNUC_CONST;

ofaRenderLedgersBin *ofa_render_ledgers_bin_new       ( ofaMainWindow *main_window );

gboolean             ofa_render_ledgers_bin_is_valid  ( ofaRenderLedgersBin *bin,
																		gchar **message );

ofaLedgerTreeview   *ofa_render_ledgers_bin_get_treeview
                                                      ( const ofaRenderLedgersBin *bin );

gboolean             ofa_render_ledgers_bin_get_all_ledgers
                                                      ( const ofaRenderLedgersBin *bin );

gboolean             ofa_render_ledgers_bin_get_new_page_per_ledger
                                                      ( const ofaRenderLedgersBin *bin );

ofaIDatesFilter     *ofa_render_ledgers_bin_get_dates_filter
                                                      ( const ofaRenderLedgersBin *bin );

G_END_DECLS

#endif /* __OFA_RENDER_LEDGERS_BIN_H__ */
