/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_RECURRENT_RUN_STORE_H__
#define __OFA_RECURRENT_RUN_STORE_H__

/**
 * SECTION: recurrent_run_store
 * @title: ofaRecurrentRunStore
 * @short_description: The RecurrentRunStore class definition
 * @include: recurrent/ofa-recurrent-form-store.h
 *
 * The #ofaRecurrentRunStore derived from #ofaListStore, which itself
 * derives from #GtkListStore. It is used under two distinct forms:
 *
 * - first, it may populated with all the generated recurrent operations
 *   from the DBMS, on first call; in this mode, the store auto-updates
 *   itself automatically, and will stay alive until the dossier is
 *   closed;
 *
 * - second, it may be populated with a list provided by the caller;
 *   in this mode, the store is cleared and released when the caller
 *   itself ends up.
 */

#include "api/ofa-igetter-def.h"
#include "api/ofa-list-store.h"

#include "recurrent/ofo-recurrent-run.h"

G_BEGIN_DECLS

#define OFA_TYPE_RECURRENT_RUN_STORE                ( ofa_recurrent_run_store_get_type())
#define OFA_RECURRENT_RUN_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RECURRENT_RUN_STORE, ofaRecurrentRunStore ))
#define OFA_RECURRENT_RUN_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RECURRENT_RUN_STORE, ofaRecurrentRunStoreClass ))
#define OFA_IS_RECURRENT_RUN_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RECURRENT_RUN_STORE ))
#define OFA_IS_RECURRENT_RUN_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RECURRENT_RUN_STORE ))
#define OFA_RECURRENT_RUN_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RECURRENT_RUN_STORE, ofaRecurrentRunStoreClass ))

typedef struct {
	/*< public members >*/
	ofaListStore      parent;
}
	ofaRecurrentRunStore;

/**
 * ofaRecurrentRunStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaListStoreClass parent;
}
	ofaRecurrentRunStoreClass;

/**
 * The columns stored in the subjacent #GtkListStore.
 *                                                           Type     Displayable
 *                                                           -------  -----------
 * @REC_RUN_COL_MNEMO         : mnemonic identifier          String       Yes
 * @REC_RUN_COL_NUMSEQ        : sequence number              String       Yes
 * @REC_RUN_COL_NUMSEQ_INT    : sequence number              Int           No
 * @REC_RUN_COL_LABEL         : label                        String       Yes
 * @REC_RUN_COL_DATE          : operation date               String       Yes
 * @REC_RUN_COL_STATUS        : operation status             String       Yes
 * @REC_RUN_COL_STATUS_I      : operation status             Int           No
 * @REC_RUN_COL_AMOUNT1       : amount 1                     String       Yes
 * @REC_RUN_COL_AMOUNT2       : amount 2                     String       Yes
 * @REC_RUN_COL_AMOUNT3       : amount 3                     String       Yes
 * @REC_RUN_COL_OPE_TEMPLATE  : operation template           String       Yes
 * @REC_RUN_COL_PERIOD_ID     : period key                   String       Yes
 * @REC_RUN_COL_PERIOD_N      : period every                 String       Yes
 * @REC_RUN_COL_PERIOD_DET    : period details               String       Yes
 * @REC_RUN_COL_END           : model end date               String       Yes
 * @REC_RUN_COL_CRE_USER      : creation user                String       Yes
 * @REC_RUN_COL_CRE_STAMP     : creation timestamp           String       Yes
 * @REC_RUN_COL_STA_USER      : status user                  String       Yes
 * @REC_RUN_COL_STA_STAMP     : status timestamp             String       Yes
 * @REC_RUN_COL_EDI_USER      : amounts edition user         String       Yes
 * @REC_RUN_COL_EDI_STAMP     : amounts edition timestamp    String       Yes
 * @REC_RUN_COL_OBJECT        : #ofoRecurrentRun object      GObject       No
 * @REC_RUN_COL_MODEL         : #ofoRecurrentModel object    GObject       No
 */
enum {
	REC_RUN_COL_MNEMO = 0,
	REC_RUN_COL_NUMSEQ,
	REC_RUN_COL_NUMSEQ_INT,
	REC_RUN_COL_LABEL,
	REC_RUN_COL_DATE,
	REC_RUN_COL_STATUS,
	REC_RUN_COL_STATUS_I,
	REC_RUN_COL_AMOUNT1,
	REC_RUN_COL_AMOUNT2,
	REC_RUN_COL_AMOUNT3,
	REC_RUN_COL_OPE_TEMPLATE,
	REC_RUN_COL_PERIOD_ID,
	REC_RUN_COL_PERIOD_N,
	REC_RUN_COL_PERIOD_DET,
	REC_RUN_COL_END,
	REC_RUN_COL_CRE_USER,
	REC_RUN_COL_CRE_STAMP,
	REC_RUN_COL_STA_USER,
	REC_RUN_COL_STA_STAMP,
	REC_RUN_COL_EDI_USER,
	REC_RUN_COL_EDI_STAMP,
	REC_RUN_COL_OBJECT,
	REC_RUN_COL_MODEL,
	REC_RUN_N_COLUMNS
};

/* work mode
 */
enum {
	REC_MODE_FROM_DBMS = 0,
	REC_MODE_FROM_LIST
};

GType                 ofa_recurrent_run_store_get_type     ( void );

ofaRecurrentRunStore *ofa_recurrent_run_store_new          ( ofaIGetter *getter,
																	gint mode );

void                  ofa_recurrent_run_store_set_from_list( ofaRecurrentRunStore *store,
																	GList *dataset );

gboolean              ofa_recurrent_run_store_get_iter     ( ofaRecurrentRunStore *store,
																	ofoRecurrentRun *run,
																	GtkTreeIter *iter );

G_END_DECLS

#endif /* __OFA_RECURRENT_RUN_STORE_H__ */
