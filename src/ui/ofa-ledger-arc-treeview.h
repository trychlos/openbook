/*
 * Open Firm LedgerArcing
 * A double-entry ledger_arcing application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
 *
 * Open Firm LedgerArcing is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm LedgerArcing is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm LedgerArcing; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifndef __OFA_LEDGER_ARC_TREEVIEW_H__
#define __OFA_LEDGER_ARC_TREEVIEW_H__

/**
 * SECTION: ofa_ledger_arc_treeview
 * @short_description: #ofaLedgerArcTreeview class definition.
 * @include: ui/ofa-ledger-arc-treeview.h
 *
 * Manage a treeview with a sorted list of archived soldes.
 *
 * The class does not manage the selection.
 */

#include "api/ofa-hub-def.h"
#include "api/ofa-tvbin.h"
#include "api/ofo-ledger-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_LEDGER_ARC_TREEVIEW                ( ofa_ledger_arc_treeview_get_type())
#define OFA_LEDGER_ARC_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_LEDGER_ARC_TREEVIEW, ofaLedgerArcTreeview ))
#define OFA_LEDGER_ARC_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_LEDGER_ARC_TREEVIEW, ofaLedgerArcTreeviewClass ))
#define OFA_IS_LEDGER_ARC_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_LEDGER_ARC_TREEVIEW ))
#define OFA_IS_LEDGER_ARC_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_LEDGER_ARC_TREEVIEW ))
#define OFA_LEDGER_ARC_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_LEDGER_ARC_TREEVIEW, ofaLedgerArcTreeviewClass ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaLedgerArcTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaLedgerArcTreeviewClass;

GType                 ofa_ledger_arc_treeview_get_type( void ) G_GNUC_CONST;

ofaLedgerArcTreeview *ofa_ledger_arc_treeview_new     ( ofaHub *hub,
															ofoLedger *ledger );

G_END_DECLS

#endif /* __OFA_LEDGER_ARC_TREEVIEW_H__ */
