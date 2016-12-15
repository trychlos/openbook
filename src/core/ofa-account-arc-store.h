/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_ACCOUNT_ARC_STORE_H__
#define __OFA_ACCOUNT_ARC_STORE_H__

/**
 * SECTION: account_arc_store
 * @title: ofaAccountArcStore
 * @short_description: The AccountArcStore class definition
 * @include: core/ofa-account-arc-store.h
 *
 * The #ofaAccountArcStore derived from #ofaListStore, which itself
 * derives from #GtkListStore. It is populated with the archived soldes
 * of an account.
 *
 * The #ofaAccountArcStore is managed with #ofaAccountProperties.
 */

#include "api/ofa-hub-def.h"
#include "api/ofa-list-store.h"
#include "api/ofo-account-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNT_ARC_STORE                ( ofa_account_arc_store_get_type())
#define OFA_ACCOUNT_ARC_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNT_ARC_STORE, ofaAccountArcStore ))
#define OFA_ACCOUNT_ARC_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNT_ARC_STORE, ofaAccountArcStoreClass ))
#define OFA_IS_ACCOUNT_ARC_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNT_ARC_STORE ))
#define OFA_IS_ACCOUNT_ARC_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNT_ARC_STORE ))
#define OFA_ACCOUNT_ARC_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNT_ARC_STORE, ofaAccountArcStoreClass ))

typedef struct {
	/*< public members >*/
	ofaListStore      parent;
}
	ofaAccountArcStore;

/**
 * ofaAccountArcStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaListStoreClass parent;
}
	ofaAccountArcStoreClass;

/**
 * The columns stored in the subjacent #GtkListStore.
 *                                                             Type     Displayable
 *                                                             -------  -----------
 * @ACCOUNT_ARC_COL_DATE      : archive date                   String       Yes
 * @ACCOUNT_ARC_COL_DEBIT     : debit                          String       Yes
 * @ACCOUNT_ARC_COL_SYMBOL1   : currency symbol                String       Yes
 * @ACCOUNT_ARC_COL_CREDIT    : credit                         String       Yes
 * @ACCOUNT_ARC_COL_SYMBOL2   : currency symbol                String       Yes
 * @ACCOUNT_ARC_COL_ACCOUNT   : account object                 GObject       No
 * @ACCOUNT_ARC_COL_CURRENCY  : currency object                GObject       No
 */
enum {
	ACCOUNT_ARC_COL_DATE = 0,
	ACCOUNT_ARC_COL_DEBIT,
	ACCOUNT_ARC_COL_SYMBOL1,
	ACCOUNT_ARC_COL_CREDIT,
	ACCOUNT_ARC_COL_SYMBOL2,
	ACCOUNT_ARC_COL_ACCOUNT,
	ACCOUNT_ARC_COL_CURRENCY,
	ACCOUNT_ARC_N_COLUMNS
};

GType               ofa_account_arc_store_get_type( void );

ofaAccountArcStore *ofa_account_arc_store_new     ( ofaHub *hub,
														ofoAccount *account );

G_END_DECLS

#endif /* __OFA_ACCOUNT_ARC_STORE_H__ */
