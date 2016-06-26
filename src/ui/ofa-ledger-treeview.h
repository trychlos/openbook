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

#ifndef __OFA_LEDGER_TREEVIEW_H__
#define __OFA_LEDGER_TREEVIEW_H__

/**
 * SECTION: ofa_ledger_treeview
 * @short_description: #ofaLedgerTreeview class definition.
 * @include: ui/ofa-ledger-treeview.h
 *
 * A convenience class to display ledgers treeview based on a liststore
 * (use cases: ofaLedgersSet main page, ofaIntClosing dialog)
 *
 * In the provided parent container, this class will create a GtkTreeView
 * embedded in a GtkScrolledWindow.
 */

#include "api/ofo-dossier-def.h"

#include "core/ofa-ledger-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_LEDGER_TREEVIEW                ( ofa_ledger_treeview_get_type())
#define OFA_LEDGER_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_LEDGER_TREEVIEW, ofaLedgerTreeview ))
#define OFA_LEDGER_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_LEDGER_TREEVIEW, ofaLedgerTreeviewClass ))
#define OFA_IS_LEDGER_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_LEDGER_TREEVIEW ))
#define OFA_IS_LEDGER_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_LEDGER_TREEVIEW ))
#define OFA_LEDGER_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_LEDGER_TREEVIEW, ofaLedgerTreeviewClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaLedgerTreeview;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaLedgerTreeviewClass;

GType              ofa_ledger_treeview_get_type          ( void ) G_GNUC_CONST;

ofaLedgerTreeview *ofa_ledger_treeview_new               ( void );

void               ofa_ledger_treeview_set_columns       ( ofaLedgerTreeview *view,
																ofaLedgerColumns columns );

void               ofa_ledger_treeview_set_hub           ( ofaLedgerTreeview *view,
																ofaHub *hub );

void               ofa_ledger_treeview_set_selection_mode( ofaLedgerTreeview *view,
																GtkSelectionMode mode );

void               ofa_ledger_treeview_set_hexpand       ( ofaLedgerTreeview *view,
																gboolean hexpand );

GtkTreeSelection  *ofa_ledger_treeview_get_selection     ( ofaLedgerTreeview *view );

GList             *ofa_ledger_treeview_get_selected      ( ofaLedgerTreeview *view );

#define            ofa_ledger_treeview_free_selected(L)  g_list_free_full(( L ), ( GDestroyNotify ) g_free )

void               ofa_ledger_treeview_set_selected      ( ofaLedgerTreeview *view,
																	const gchar *ledger );

GtkWidget         *ofa_ledger_treeview_get_treeview      ( ofaLedgerTreeview *view );

G_END_DECLS

#endif /* __OFA_LEDGER_TREEVIEW_H__ */
