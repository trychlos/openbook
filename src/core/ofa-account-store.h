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

#ifndef __OFA_ACCOUNT_STORE_H__
#define __OFA_ACCOUNT_STORE_H__

/**
 * SECTION: account_store
 * @title: ofaAccountStore
 * @short_description: The AccountStore class description
 * @include: ui/ofa-account-store.h
 *
 * The #ofaAccountStore derived from #ofaTreeStore, which itself
 * derives from #GtkTreeStore. It is populated with all the accounts
 * of the dossier on first call, and stay then alive until the dossier
 * is closed.
 *
 * Once more time: there is only one #ofaAccountStore while the dossier
 * is opened. All the views are built on this store, using ad-hoc filter
 * models when needed.
 *
 * The #ofaAccountStore takes advantage of the dossier signaling
 * system to maintain itself up to date.
 */

#include "api/ofa-igetter-def.h"
#include "api/ofa-tree-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNT_STORE                ( ofa_account_store_get_type())
#define OFA_ACCOUNT_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNT_STORE, ofaAccountStore ))
#define OFA_ACCOUNT_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNT_STORE, ofaAccountStoreClass ))
#define OFA_IS_ACCOUNT_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNT_STORE ))
#define OFA_IS_ACCOUNT_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNT_STORE ))
#define OFA_ACCOUNT_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNT_STORE, ofaAccountStoreClass ))

typedef struct {
	/*< public members >*/
	ofaTreeStore      parent;
}
	ofaAccountStore;

/**
 * ofaAccountStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaTreeStoreClass parent;
}
	ofaAccountStoreClass;

/**
 * The columns stored in the subjacent #GtkTreeStore.
 *
 * The column numbers are stored in user preferences.
 * New columns must be added at the end.
 *                                                                                      Displayable
 *                                                                             Type      in chart
 *                                                                             -------  -----------
 * @ACCOUNT_COL_NUMBER              : identifier                               String      Yes
 * @ACCOUNT_COL_LABEL               : label                                    String      Yes
 * @ACCOUNT_COL_CURRENCY            : currency identifier                      String      Yes
 * @ACCOUNT_COL_ROOT                : whether is root                          Bool         No
 * @ACCOUNT_COL_NOTES               : notes                                    String      Yes
 * @ACCOUNT_COL_NOTES_PNG           : notes indicator                          Pixbuf      Yes
 * @ACCOUNT_COL_UPD_USER            : last update user                         String      Yes
 * @ACCOUNT_COL_UPD_STAMP           : last update timestamp                    String      Yes
 * @ACCOUNT_COL_VAL_DEBIT           : validated debit                          String      Yes
 * @ACCOUNT_COL_VAL_CREDIT          : validated credit                         String      Yes
 * @ACCOUNT_COL_ROUGH_DEBIT         : rough debit                              String      Yes
 * @ACCOUNT_COL_ROUGH_CREDIT        : rough credit                             String      Yes
 * @ACCOUNT_COL_FUT_DEBIT           : future debit                             String      Yes
 * @ACCOUNT_COL_FUT_CREDIT          : future credit                            String      Yes
 * @ACCOUNT_COL_SETTLEABLE          : whether is settleable                    String      Yes
 * @ACCOUNT_COL_RECONCILIABLE       : whether is reconciliable                 String      Yes
 * @ACCOUNT_COL_FORWARDABLE         : whether is forwardable                   String      Yes
 * @ACCOUNT_COL_CLOSED              : whether is closed                        String      Yes
 * @ACCOUNT_COL_EXE_DEBIT           : current (validated+rough) debit          String      Yes
 * @ACCOUNT_COL_EXE_CREDIT          : current (validated+rough) credit         String      Yes
 * @ACCOUNT_COL_EXE_SOLDE           : current solde                            String      Yes
 * @ACCOUNT_COL_KEEP_UNSETTLED      : whether to keep unsettled entries        String      Yes
 * @ACCOUNT_COL_KEEP_UNRECONCILIATED: whether to keep unreconciliated entries  String      Yes
 * @ACCOUNT_COL_OBJECT              : #ofoAccount object                       GObject      No
 *
 * NB: accounts chart is not sortable (is always sorted by account number in the store).
 */
enum {
	ACCOUNT_COL_NUMBER = 0,
	ACCOUNT_COL_LABEL,
	ACCOUNT_COL_CURRENCY,
	ACCOUNT_COL_ROOT,
	ACCOUNT_COL_NOTES,
	ACCOUNT_COL_NOTES_PNG,
	ACCOUNT_COL_UPD_USER,
	ACCOUNT_COL_UPD_STAMP,
	ACCOUNT_COL_VAL_DEBIT,
	ACCOUNT_COL_VAL_CREDIT,
	ACCOUNT_COL_ROUGH_DEBIT,
	ACCOUNT_COL_ROUGH_CREDIT,
	ACCOUNT_COL_FUT_DEBIT,
	ACCOUNT_COL_FUT_CREDIT,
	ACCOUNT_COL_SETTLEABLE,
	ACCOUNT_COL_RECONCILIABLE,
	ACCOUNT_COL_FORWARDABLE,
	ACCOUNT_COL_CLOSED,
	ACCOUNT_COL_EXE_DEBIT,
	ACCOUNT_COL_EXE_CREDIT,
	ACCOUNT_COL_EXE_SOLDE,
	ACCOUNT_COL_KEEP_UNSETTLED,
	ACCOUNT_COL_KEEP_UNRECONCILIATED,
	ACCOUNT_COL_OBJECT,
	ACCOUNT_N_COLUMNS
};

GType            ofa_account_store_get_type     ( void );

ofaAccountStore *ofa_account_store_new          ( ofaIGetter *getter );

G_END_DECLS

#endif /* __OFA_ACCOUNT_STORE_H__ */
