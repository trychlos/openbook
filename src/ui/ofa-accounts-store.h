/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifndef __OFA_ACCOUNTS_STORE_H__
#define __OFA_ACCOUNTS_STORE_H__

/**
 * SECTION: accounts_store
 * @title: ofaAccountsStore
 * @short_description: The AccountsStore class description
 * @include: ui/ofa-accounts-store.h
 *
 * The #ofaAccountsStore derived from ofaStore, which itself derives
 * from GtkAccountsStore. It is populated with all the accounts of the
 * dossier on first call, and stay then alive until the dossier is
 * closed.
 *
 * Once more time: there is only one #ofaAccountsStore while the dossier
 * is opened. All the views are built on this store, using ad-hoc filter
 * models when needed.
 *
 * The #ofaAccountsStore takes advantage of the dossier signaling
 * system to maintain itself up to date.
 */

#include "api/ofo-dossier-def.h"

#include "ui/ofa-tree-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNTS_STORE                ( ofa_accounts_store_get_type())
#define OFA_ACCOUNTS_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNTS_STORE, ofaAccountsStore ))
#define OFA_ACCOUNTS_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNTS_STORE, ofaAccountsStoreClass ))
#define OFA_IS_ACCOUNTS_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNTS_STORE ))
#define OFA_IS_ACCOUNTS_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNTS_STORE ))
#define OFA_ACCOUNTS_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNTS_STORE, ofaAccountsStoreClass ))

typedef struct _ofaAccountsStorePrivate        ofaAccountsStorePrivate;

typedef struct {
	/*< public members >*/
	ofaTreeStore             parent;

	/*< private members >*/
	ofaAccountsStorePrivate *priv;
}
	ofaAccountsStore;

/**
 * ofaAccountsStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaTreeStoreClass        parent;
}
	ofaAccountsStoreClass;

/**
 * ofaAccountColumns:
 * The columns stored in the subjacent #GtkAccountsStore.
 */
typedef enum {
	ACCOUNT_COL_NUMBER = 0,
	ACCOUNT_COL_LABEL,
	ACCOUNT_COL_CURRENCY,
	ACCOUNT_COL_TYPE,
	ACCOUNT_COL_NOTES,
	ACCOUNT_COL_UPD_USER,
	ACCOUNT_COL_UPD_STAMP,
	ACCOUNT_COL_VAL_DEBIT,
	ACCOUNT_COL_VAL_CREDIT,
	ACCOUNT_COL_ROUGH_DEBIT,
	ACCOUNT_COL_ROUGH_CREDIT,
	ACCOUNT_COL_OPEN_DEBIT,
	ACCOUNT_COL_OPEN_CREDIT,
	ACCOUNT_COL_FUT_DEBIT,
	ACCOUNT_COL_FUT_CREDIT,
	ACCOUNT_COL_SETTLEABLE,
	ACCOUNT_COL_RECONCILIABLE,
	ACCOUNT_COL_FORWARD,
	ACCOUNT_COL_EXE_DEBIT,				/* exercice = validated+rough */
	ACCOUNT_COL_EXE_CREDIT,
	ACCOUNT_COL_OBJECT,
	ACCOUNT_N_COLUMNS
}
	ofaAccountColumns;

GType             ofa_accounts_store_get_type     ( void );

ofaAccountsStore *ofa_accounts_store_new          ( ofoDossier *dossier );

void              ofa_accounts_store_load_dataset ( ofaAccountsStore *store );

gboolean          ofa_accounts_store_get_by_number( ofaAccountsStore *store,
															const gchar *number,
															GtkTreeIter *iter );

G_END_DECLS

#endif /* __OFA_ACCOUNTS_STORE_H__ */
