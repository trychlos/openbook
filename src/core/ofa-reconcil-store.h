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

#ifndef __OFA_RECONCIL_STORE_H__
#define __OFA_RECONCIL_STORE_H__

/**
 * SECTION: reconcil_store
 * @title: ofaReconcilStore
 * @short_description: The ReconcilStore class description
 * @include: core/ofa-reconcil-store.h
 *
 * The #ofaReconcilStore derives from #ofaTreeStore. It stores entries
 * and BAT lines which are proposed to bank reconciliation.
 *
 * The #ofaReconcilStore takes advantage of the dossier signaling
 * system to maintain itself up to date.
 *
 * When inserting an entry:
 *
 * - if the entry is conciliated,
 *     if a row at level 0 is member of the same conciliation group,
 *       entry_insert_with_remediation of this parent
 *     else
 *       entry insert at level 0 (which will load the entry part of
 *       the conciliation group)
 *
 * - if the entry is not conciliated,
 *     if a row at level 0 has a compatible amount,
 *       entry_insert_with_remediation of this parent
 *     else
 *       entry insert at level 0
 *
 * When inserting a BAT line:
 *
 * - if the BAT line is conciliated,
 *     if a row at level 0 is member of the same conciliation group,
 *       bat_insert_with_remediation of this parent
 *     else
 *       bat insert at level 0 (which will load the BAT line part of
 *       the conciliation group)
 *
 * - if the BAT line is not conciliated,
 *     if a row at level 0 has a compatible amount,
 *       bat_insert_with_remediation of this parent
 *     else
 *       bat insert at level 0
 */

#include <gtk/gtk.h>

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofa-tree-store.h"

#include "core/ofa-iconcil.h"

G_BEGIN_DECLS

#define OFA_TYPE_RECONCIL_STORE                ( ofa_reconcil_store_get_type())
#define OFA_RECONCIL_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RECONCIL_STORE, ofaReconcilStore ))
#define OFA_RECONCIL_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RECONCIL_STORE, ofaReconcilStoreClass ))
#define OFA_IS_RECONCIL_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RECONCIL_STORE ))
#define OFA_IS_RECONCIL_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RECONCIL_STORE ))
#define OFA_RECONCIL_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RECONCIL_STORE, ofaReconcilStoreClass ))

typedef struct {
	/*< public members >*/
	ofaTreeStore      parent;
}
	ofaReconcilStore;

/**
 * ofaReconcilStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaTreeStoreClass parent;
}
	ofaReconcilStoreClass;

/**
 * The columns stored in the subjacent #ofaTreeStore.
 *                                ofoEntry               ofoBatLine             Type    Displayable
 *                                ---------------------  ---------------------  ------  -----------
 * @RECONCIL_COL_DOPE           : operation date         operation date         String      Yes
 * @RECONCIL_COL_DEFFECT        : effect date            effect date            String      Yes
 * @RECONCIL_COL_LABEL          : entry label            line label             String      Yes
 * @RECONCIL_COL_REF            : piece reference        line reference         String      Yes
 * @RECONCIL_COL_CURRENCY       : currency               currency               String      Yes
 * @RECONCIL_COL_LEDGER         : ledger                                        String      Yes
 * @RECONCIL_COL_OPE_TEMPLATE   : operation template                            String      Yes
 * @RECONCIL_COL_ACCOUNT        : account                                       String      Yes
 * @RECONCIL_COL_DEBIT          : debit                  debit                  String      Yes
 * @RECONCIL_COL_CREDIT         : credit                 credit                 String      Yes
 * @RECONCIL_COL_OPE_NUMBER     : operation number                              String      Yes
 * @RECONCIL_COL_STLMT_NUMBER   : settlement number                             String      Yes
 * @RECONCIL_COL_STLMT_USER     : settlement user                               String      Yes
 * @RECONCIL_COL_STLMT_STAMP    : settlement timestamp                          String      Yes
 * @RECONCIL_COL_ENT_NUMBER     : entry number           line number            String      Yes
 * @RECONCIL_COL_ENT_NUMBER_I   : entry number           line number            Int          No
 * @RECONCIL_COL_UPD_USER       : last update user       import user            String      Yes
 * @RECONCIL_COL_UPD_STAMP      : last update timestamp  import timestamp       String      Yes
 * @RECONCIL_COL_STATUS         : status                                        String      Yes
 * @RECONCIL_COL_STATUS_I       : status                                        Int          No
 * @RECONCIL_COL_RULE           : rule                                          String      Yes
 * @RECONCIL_COL_RULE_I         : rule                                          Int          No
 * @RECONCIL_COL_TIERS          : tiers                                         String      Yes
 * @RECONCIL_COL_CONCIL_NUMBER  : reconciliation number  reconciliation number  String      Yes
 * @RECONCIL_COL_CONCIL_NUMBER_I: reconciliation number  reconciliation number  Int          No
 * @RECONCIL_COL_CONCIL_DATE    : reconciliation date    reconciliation date    String      Yes
 * @RECONCIL_COL_CONCIL_TYPE    : reconciliation type    reconciliation type    String      Yes
 * @RECONCIL_COL_OBJECT         : #ofoEntry object       ofobatLine             GObject      No
 * @RECONCIL_COL_IPERIOD        : period                                        String      Yes
 * @RECONCIL_COL_IPERIOD_I      : period                                        Int          No
 */
enum {
	RECONCIL_COL_DOPE = 0,
	RECONCIL_COL_DEFFECT,
	RECONCIL_COL_LABEL,
	RECONCIL_COL_REF,
	RECONCIL_COL_CURRENCY,
	RECONCIL_COL_LEDGER,
	RECONCIL_COL_OPE_TEMPLATE,
	RECONCIL_COL_ACCOUNT,
	RECONCIL_COL_DEBIT,
	RECONCIL_COL_CREDIT,
	RECONCIL_COL_OPE_NUMBER,
	RECONCIL_COL_STLMT_NUMBER,
	RECONCIL_COL_STLMT_USER,
	RECONCIL_COL_STLMT_STAMP,
	RECONCIL_COL_ENT_NUMBER,
	RECONCIL_COL_ENT_NUMBER_I,
	RECONCIL_COL_UPD_USER,
	RECONCIL_COL_UPD_STAMP,
	RECONCIL_COL_STATUS,
	RECONCIL_COL_STATUS_I,
	RECONCIL_COL_RULE,
	RECONCIL_COL_RULE_I,
	RECONCIL_COL_TIERS,
	RECONCIL_COL_CONCIL_NUMBER,
	RECONCIL_COL_CONCIL_NUMBER_I,
	RECONCIL_COL_CONCIL_DATE,
	RECONCIL_COL_CONCIL_TYPE,
	RECONCIL_COL_OBJECT,
	RECONCIL_COL_IPERIOD,
	RECONCIL_COL_IPERIOD_I,
	RECONCIL_N_COLUMNS
};

GType             ofa_reconcil_store_get_type         ( void );

ofaReconcilStore *ofa_reconcil_store_new              ( ofaIGetter *getter );

ofxCounter        ofa_reconcil_store_load_by_account  ( ofaReconcilStore *store,
															const gchar *account_id );

ofxCounter        ofa_reconcil_store_load_by_bat      ( ofaReconcilStore *store,
															ofxCounter bat_id );

ofxCounter        ofa_reconcil_store_load_by_concil   ( ofaReconcilStore *store,
															ofxCounter concil_id );

void              ofa_reconcil_store_insert_row       ( ofaReconcilStore *store,
															ofaIConcil *iconcil,
															GtkTreeIter *parent_iter,
															GtkTreeIter *iter );

void              ofa_reconcil_store_insert_level_zero( ofaReconcilStore *store,
															ofaIConcil *iconcil,
															GtkTreeIter *iter );

void              ofa_reconcil_store_set_concil_data  ( ofaReconcilStore *store,
															ofxCounter concil_id,
															const GDate *concil_date,
															GtkTreeIter *iter );

G_END_DECLS

#endif /* __OFA_RECONCIL_STORE_H__ */
