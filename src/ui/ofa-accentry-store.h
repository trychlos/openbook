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

#ifndef __OFA_ACCENTRY_STORE_H__
#define __OFA_ACCENTRY_STORE_H__

/**
 * SECTION: accentry_store
 * @title: ofaAccentryStore
 * @short_description: The AccentryStore class description
 * @include: ui/ofa-accentry-store.h
 *
 * The #ofaAccentryStore derives from #ofaTreeStore. It stores accounts
 * on level 0 and child entries related to the parent account.
 *
 * Other filters are left to the treeview.
 *
 * The #ofaAccentryStore takes advantage of the dossier signaling
 * system to maintain itself up to date.
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"
#include "api/ofa-tree-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCENTRY_STORE                ( ofa_accentry_store_get_type())
#define OFA_ACCENTRY_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCENTRY_STORE, ofaAccentryStore ))
#define OFA_ACCENTRY_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCENTRY_STORE, ofaAccentryStoreClass ))
#define OFA_IS_ACCENTRY_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCENTRY_STORE ))
#define OFA_IS_ACCENTRY_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCENTRY_STORE ))
#define OFA_ACCENTRY_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCENTRY_STORE, ofaAccentryStoreClass ))

typedef struct {
	/*< public members >*/
	ofaTreeStore      parent;
}
	ofaAccentryStore;

/**
 * ofaAccentryStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaTreeStoreClass parent;
}
	ofaAccentryStoreClass;

/**
 * The columns stored in the subjacent #ofaTreeStore.
 *                                                                    Type     Displayable
 *                                                                    -------  -----------
 * -- data common both to ofoAccount and ofoEntry
 * @ACCENTRY_COL_ACCOUNT             : account                        String       Yes
 * @ACCENTRY_COL_LABEL               : label                          String       Yes
 * @ACCENTRY_COL_CURRENCY            : currency                       String       Yes
 * @ACCENTRY_COL_UPD_USER            : last update user               String       Yes
 * @ACCENTRY_COL_UPD_STAMP           : last update timestamp          String       Yes
 * -- ofoAccount specific
 * @ACCENTRY_COL_SETTLEABLE          : account is settleable          String       Yes
 * @ACCENTRY_COL_KEEP_UNSETTLED      : account keeps unsettled        String       Yes
 * @ACCENTRY_COL_RECONCILIABLE       : account is reconciliable       String       Yes
 * @ACCENTRY_COL_KEEP_UNRECONCILIATED: account keeps unreconciliated  String       Yes
 * -- ofoEntry specific
 * @ACCENTRY_COL_DOPE                : entry operation date           String       Yes
 * @ACCENTRY_COL_DEFFECT             : entry effect date              String       Yes
 * @ACCENTRY_COL_REF                 : entry piece reference          String       Yes
 * @ACCENTRY_COL_LEDGER              : entry ledger                   String       Yes
 * @ACCENTRY_COL_OPE_TEMPLATE        : entry operation template       String       Yes
 * @ACCENTRY_COL_DEBIT               : entry debit                    String       Yes
 * @ACCENTRY_COL_CREDIT              : entry credit                   String       Yes
 * @ACCENTRY_COL_OPE_NUMBER          : entry operation number         String       Yes
 * @ACCENTRY_COL_ENT_NUMBER          : entry number                   String       Yes
 * @ACCENTRY_COL_ENT_NUMBER_I        : entry number                   Integer       No
 * @ACCENTRY_COL_STATUS              : entry status                   String       Yes
 * @ACCENTRY_COL_OBJECT              : #ofoEntry/ofoAccount object    GObject       No
 */
enum {
	ACCENTRY_COL_ACCOUNT = 0,
	ACCENTRY_COL_LABEL,
	ACCENTRY_COL_CURRENCY,
	ACCENTRY_COL_UPD_USER,
	ACCENTRY_COL_UPD_STAMP,
	ACCENTRY_COL_SETTLEABLE,
	ACCENTRY_COL_KEEP_UNSETTLED,
	ACCENTRY_COL_RECONCILIABLE,
	ACCENTRY_COL_KEEP_UNRECONCILIATED,
	ACCENTRY_COL_DOPE,
	ACCENTRY_COL_DEFFECT,
	ACCENTRY_COL_REF,
	ACCENTRY_COL_LEDGER,
	ACCENTRY_COL_OPE_TEMPLATE,
	ACCENTRY_COL_DEBIT,
	ACCENTRY_COL_CREDIT,
	ACCENTRY_COL_OPE_NUMBER,
	ACCENTRY_COL_ENT_NUMBER,
	ACCENTRY_COL_ENT_NUMBER_I,
	ACCENTRY_COL_STATUS,
	ACCENTRY_COL_OBJECT,
	ACCENTRY_N_COLUMNS
};

GType             ofa_accentry_store_get_type( void );

ofaAccentryStore *ofa_accentry_store_new     ( ofaIGetter *getter );

G_END_DECLS

#endif /* __OFA_ACCENTRY_STORE_H__ */
