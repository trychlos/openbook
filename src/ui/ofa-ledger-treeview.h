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

#ifndef __OFA_LEDGER_TREEVIEW_H__
#define __OFA_LEDGER_TREEVIEW_H__

/**
 * SECTION: ofa_ledger_treeview
 * @short_description: #ofaLedgerTreeview class definition.
 * @include: ui/ofa-ledger-treeview.h
 *
 * Manage a treeview with the list of the ledgers.
 *
 * The class provides the following signals, which are proxyed from
 * #ofaTVBin base class.
 *    +------------------+--------------+
 *    | Signal           | Ledger list  |
 *    |                  | may be %NULL |
 *    +------------------+--------------+
 *    | ofa-ledchanged   |      Yes     |
 *    | ofa-ledactivated |       No     |
 *    | ofa-leddelete    |       No     |
 *    +------------------+--------------+
 *
 * As the treeview may allow multiple selection, both signals provide
 * a list of path of selected rows. It is up to the user of this class
 * to decide whether an action may apply or not on a multiple selection.
 */

#include "api/ofa-hub-def.h"
#include "api/ofa-tvbin.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_LEDGER_TREEVIEW                ( ofa_ledger_treeview_get_type())
#define OFA_LEDGER_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_LEDGER_TREEVIEW, ofaLedgerTreeview ))
#define OFA_LEDGER_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_LEDGER_TREEVIEW, ofaLedgerTreeviewClass ))
#define OFA_IS_LEDGER_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_LEDGER_TREEVIEW ))
#define OFA_IS_LEDGER_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_LEDGER_TREEVIEW ))
#define OFA_LEDGER_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_LEDGER_TREEVIEW, ofaLedgerTreeviewClass ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaLedgerTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaLedgerTreeviewClass;

GType              ofa_ledger_treeview_get_type         ( void ) G_GNUC_CONST;

ofaLedgerTreeview *ofa_ledger_treeview_new              ( ofaHub *hub );

void               ofa_ledger_treeview_set_settings_key ( ofaLedgerTreeview *view,
																const gchar *key );

void               ofa_ledger_treeview_setup_columns    ( ofaLedgerTreeview *view );

void               ofa_ledger_treeview_setup_store      ( ofaLedgerTreeview *view );

GList             *ofa_ledger_treeview_get_selected     ( ofaLedgerTreeview *view );

#define            ofa_ledger_treeview_free_selected(L) g_list_free_full(( L ), ( GDestroyNotify ) g_object_unref )

void               ofa_ledger_treeview_set_selected     ( ofaLedgerTreeview *view,
																const gchar *ledger );

G_END_DECLS

#endif /* __OFA_LEDGER_TREEVIEW_H__ */
