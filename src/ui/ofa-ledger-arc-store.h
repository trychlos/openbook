/*
 * Open Firm Ledgering
 * A double-entry ledgering application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Ledgering is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Ledgering is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Ledgering; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifndef __OFA_LEDGER_ARC_STORE_H__
#define __OFA_LEDGER_ARC_STORE_H__

/**
 * SECTION: ledger_arc_store
 * @title: ofaLedgerArcStore
 * @short_description: The LedgerArcStore class definition
 * @include: core/ofa-ledger-arc-store.h
 *
 * The #ofaLedgerArcStore derived from #ofaListStore, which itself
 * derives from #GtkListStore. It is populated with the archived soldes
 * of a ledger.
 *
 * The #ofaLedgerArcStore is managed with #ofaLedgerProperties.
 */

#include "api/ofa-hub-def.h"
#include "api/ofa-list-store.h"
#include "api/ofo-ledger-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_LEDGER_ARC_STORE                ( ofa_ledger_arc_store_get_type())
#define OFA_LEDGER_ARC_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_LEDGER_ARC_STORE, ofaLedgerArcStore ))
#define OFA_LEDGER_ARC_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_LEDGER_ARC_STORE, ofaLedgerArcStoreClass ))
#define OFA_IS_LEDGER_ARC_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_LEDGER_ARC_STORE ))
#define OFA_IS_LEDGER_ARC_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_LEDGER_ARC_STORE ))
#define OFA_LEDGER_ARC_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_LEDGER_ARC_STORE, ofaLedgerArcStoreClass ))

typedef struct {
	/*< public members >*/
	ofaListStore      parent;
}
	ofaLedgerArcStore;

/**
 * ofaLedgerArcStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaListStoreClass parent;
}
	ofaLedgerArcStoreClass;

/**
 * The columns stored in the subjacent #GtkListStore.
 *                                                           Type     Displayable
 *                                                           -------  -----------
 * @LEDGER_ARC_COL_DATE      : archive date                  String       Yes
 * @LEDGER_ARC_COL_ISO       : currency iso code             String       Yes
 * @LEDGER_ARC_COL_DEBIT     : debit                         String       Yes
 * @LEDGER_ARC_COL_SYMBOL1   : currency symbol               String       Yes
 * @LEDGER_ARC_COL_CREDIT    : credit                        String       Yes
 * @LEDGER_ARC_COL_SYMBOL2   : currency symbol               String       Yes
 * @LEDGER_ARC_COL_LEDGER    : ledger object                 GObject       No
 * @LEDGER_ARC_COL_CURRENCY  : currency object               GObject       No
 */
enum {
	LEDGER_ARC_COL_DATE = 0,
	LEDGER_ARC_COL_ISO,
	LEDGER_ARC_COL_DEBIT,
	LEDGER_ARC_COL_SYMBOL1,
	LEDGER_ARC_COL_CREDIT,
	LEDGER_ARC_COL_SYMBOL2,
	LEDGER_ARC_COL_LEDGER,
	LEDGER_ARC_COL_CURRENCY,
	LEDGER_ARC_N_COLUMNS
};

GType              ofa_ledger_arc_store_get_type( void );

ofaLedgerArcStore *ofa_ledger_arc_store_new     ( ofaHub *hub,
														ofoLedger *ledger );

G_END_DECLS

#endif /* __OFA_LEDGER_ARC_STORE_H__ */
