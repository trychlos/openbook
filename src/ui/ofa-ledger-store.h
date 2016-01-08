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

#ifndef __OFA_LEDGER_STORE_H__
#define __OFA_LEDGER_STORE_H__

/**
 * SECTION: ledger_store
 * @title: ofaLedgerStore
 * @short_description: The LedgerStore class
 * @include: ui/ofa-ledger-store.h
 *
 * The #ofaLedgerStore derived from #ofaListStore, which itself
 * derives from #GtkListStore. It is populated with all the ledgers
 * of the dossier on first call, and stay then alive until the dossier
 * is closed.
 *
 * Once more time: there is only one #ofaLedgerStore while the dossier
 * is opened. All the views are built on this store, using ad-hoc filter
 * models when needed.
 *
 * The #ofaLedgerStore takes advantage of the dossier signaling
 * system to maintain itself up to date.
 */

#include "api/ofa-hub-def.h"
#include "api/ofa-list-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_LEDGER_STORE                ( ofa_ledger_store_get_type())
#define OFA_LEDGER_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_LEDGER_STORE, ofaLedgerStore ))
#define OFA_LEDGER_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_LEDGER_STORE, ofaLedgerStoreClass ))
#define OFA_IS_LEDGER_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_LEDGER_STORE ))
#define OFA_IS_LEDGER_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_LEDGER_STORE ))
#define OFA_LEDGER_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_LEDGER_STORE, ofaLedgerStoreClass ))

typedef struct _ofaLedgerStorePrivate        ofaLedgerStorePrivate;

typedef struct {
	/*< public members >*/
	ofaListStore           parent;

	/*< private members >*/
	ofaLedgerStorePrivate *priv;
}
	ofaLedgerStore;

/**
 * ofaLedgerStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaListStoreClass      parent;
}
	ofaLedgerStoreClass;

/**
 * The columns stored in the subjacent #GtkListStore.
 */
enum {
	LEDGER_COL_MNEMO = 0,
	LEDGER_COL_LABEL,
	LEDGER_COL_LAST_ENTRY,
	LEDGER_COL_LAST_CLOSE,
	LEDGER_COL_NOTES,
	LEDGER_COL_UPD_USER,
	LEDGER_COL_UPD_STAMP,
	LEDGER_COL_OBJECT,
	LEDGER_N_COLUMNS
};

/**
 * ofaLedgerColumns:
 * The columns displayed in the ComboBox or the Treeview.
 */
typedef enum {
	LEDGER_DISP_MNEMO      = 1 << 0,
	LEDGER_DISP_LABEL      = 1 << 1,
	LEDGER_DISP_LAST_ENTRY = 1 << 2,
	LEDGER_DISP_LAST_CLOSE = 1 << 3,
	LEDGER_DISP_NOTES      = 1 << 4,
	LEDGER_DISP_UPD_USER   = 1 << 5,
	LEDGER_DISP_UPD_STAMP  = 1 << 6,
}
	ofaLedgerColumns;

GType           ofa_ledger_store_get_type( void );

ofaLedgerStore *ofa_ledger_store_new     ( ofaHub *hub );

G_END_DECLS

#endif /* __OFA_LEDGER_STORE_H__ */
