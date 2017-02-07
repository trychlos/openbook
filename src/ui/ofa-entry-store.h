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

#ifndef __OFA_ENTRY_STORE_H__
#define __OFA_ENTRY_STORE_H__

/**
 * SECTION: entry_store
 * @title: ofaEntryStore
 * @short_description: The EntryStore class description
 * @include: core/ofa-entry-store.h
 *
 * The #ofaEntryStore derives from #ofaListStore.
 *
 * As other stores of the application, only one store exists, which is
 * loaded on demand.
 *
 * The #ofaEntryStore takes advantage of the dossier signaling
 * system to maintain itself up to date.
 */

#include <gtk/gtk.h>

#include "api/ofa-list-store.h"
#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ENTRY_STORE                ( ofa_entry_store_get_type())
#define OFA_ENTRY_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ENTRY_STORE, ofaEntryStore ))
#define OFA_ENTRY_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ENTRY_STORE, ofaEntryStoreClass ))
#define OFA_IS_ENTRY_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ENTRY_STORE ))
#define OFA_IS_ENTRY_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ENTRY_STORE ))
#define OFA_ENTRY_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ENTRY_STORE, ofaEntryStoreClass ))

typedef struct {
	/*< public members >*/
	ofaListStore      parent;
}
	ofaEntryStore;

/**
 * ofaEntryStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaListStoreClass parent;
}
	ofaEntryStoreClass;

/**
 * The columns stored in the subjacent #ofaListStore.
 *                                                             Type     Displayable
 *                                                             -------  -----------
 * @ENTRY_COL_DOPE          : operation date                    String      Yes
 * @ENTRY_COL_DEFFECT       : effect date                       String      Yes
 * @ENTRY_COL_LABEL         : entry label                       String      Yes
 * @ENTRY_COL_REF           : piece reference                   String      Yes
 * @ENTRY_COL_CURRENCY      : currency                          String      Yes
 * @ENTRY_COL_LEDGER        : ledger                            String      Yes
 * @ENTRY_COL_OPE_TEMPLATE  : operation template                String      Yes
 * @ENTRY_COL_ACCOUNT       : account                           String      Yes
 * @ENTRY_COL_DEBIT         : debit                             String      Yes
 * @ENTRY_COL_CREDIT        : credit                            String      Yes
 * @ENTRY_COL_OPE_NUMBER    : operation number                  String      Yes
 * @ENTRY_COL_STLMT_NUMBER  : settlement number                 String      Yes
 * @ENTRY_COL_STLMT_USER    : settlement user                   String      Yes
 * @ENTRY_COL_STLMT_STAMP   : settlement timestamp              String      Yes
 * @ENTRY_COL_ENT_NUMBER    : entry number                      String      Yes
 * @ENTRY_COL_ENT_NUMBER_I  : entry number                      Int          No
 * @ENTRY_COL_UPD_USER      : last update user                  String      Yes
 * @ENTRY_COL_UPD_STAMP     : last update timestamp             String      Yes
 * @ENTRY_COL_CONCIL_NUMBER : reconciliation number             String      Yes
 * @ENTRY_COL_CONCIL_DATE   : reconciliation date               String      Yes
 * @ENTRY_COL_STATUS        : status                            String      Yes
 * @ENTRY_COL_STATUS_I      : status                            Int          No
 * @ENTRY_COL_OBJECT        : #ofoEntry object                  GObject      No
 * @ENTRY_COL_MSGERR        : error message                     String       No
 * @ENTRY_COL_MSGWARN       : warning message                   String       No
 * @ENTRY_COL_DOPE_SET      : whether operation date is set     Bool         No
 * @ENTRY_COL_DEFFECT_SET   : whether effect date is set        Bool         No
 * @ENTRY_COL_CURRENCY_SET  : whether currency is set           Bool         No
 * @ENTRY_COL_RULE_I        : rule indicator                    Int          No
 * @ENTRY_COL_RULE          : rule localized string             String      Yes
 * @ENTRY_COL_NOTES         : notes                             String      Yes
 */
enum {
	ENTRY_COL_DOPE = 0,
	ENTRY_COL_DEFFECT,
	ENTRY_COL_LABEL,
	ENTRY_COL_REF,
	ENTRY_COL_CURRENCY,
	ENTRY_COL_LEDGER,
	ENTRY_COL_OPE_TEMPLATE,
	ENTRY_COL_ACCOUNT,
	ENTRY_COL_DEBIT,
	ENTRY_COL_CREDIT,
	ENTRY_COL_OPE_NUMBER,
	ENTRY_COL_STLMT_NUMBER,
	ENTRY_COL_STLMT_USER,
	ENTRY_COL_STLMT_STAMP,
	ENTRY_COL_ENT_NUMBER,
	ENTRY_COL_ENT_NUMBER_I,
	ENTRY_COL_UPD_USER,
	ENTRY_COL_UPD_STAMP,
	ENTRY_COL_CONCIL_NUMBER,
	ENTRY_COL_CONCIL_DATE,
	ENTRY_COL_STATUS,
	ENTRY_COL_STATUS_I,
	ENTRY_COL_OBJECT,
	ENTRY_COL_MSGERR,
	ENTRY_COL_MSGWARN,
	ENTRY_COL_DOPE_SET,
	ENTRY_COL_DEFFECT_SET,
	ENTRY_COL_CURRENCY_SET,
	ENTRY_COL_RULE_I,
	ENTRY_COL_RULE,
	ENTRY_COL_NOTES,
	ENTRY_N_COLUMNS
};

GType          ofa_entry_store_get_type( void );

ofaEntryStore *ofa_entry_store_new     ( ofaIGetter *getter );

G_END_DECLS

#endif /* __OFA_ENTRY_STORE_H__ */
