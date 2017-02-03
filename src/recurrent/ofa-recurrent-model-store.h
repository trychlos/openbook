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

#ifndef __OFA_RECURRENT_MODEL_STORE_H__
#define __OFA_RECURRENT_MODEL_STORE_H__

/**
 * SECTION: recurrent_model_store
 * @title: ofaRecurrentModelStore
 * @short_description: The RecurrentModelStore class definition
 * @include: recurrent/ofa-recurrent-form-store.h
 *
 * The #ofaRecurrentModelStore derived from #ofaListStore, which itself
 * derives from #GtkListStore. It is populated with all the models
 * defined on the dossier on first call, and stay then alive until the
 * dossier is closed.
 *
 * Once more time: there is only one #ofaRecurrentModelStore while the dossier
 * is opened. All the views are built on this store, using ad-hoc filter
 * models when needed.
 *
 * The #ofaRecurrentModelStore takes advantage of the hub signaling
 * system to maintain itself up to date.
 */

#include "api/ofa-igetter-def.h"
#include "api/ofa-list-store.h"

#include "recurrent/ofo-recurrent-model.h"

G_BEGIN_DECLS

#define OFA_TYPE_RECURRENT_MODEL_STORE                ( ofa_recurrent_model_store_get_type())
#define OFA_RECURRENT_MODEL_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RECURRENT_MODEL_STORE, ofaRecurrentModelStore ))
#define OFA_RECURRENT_MODEL_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RECURRENT_MODEL_STORE, ofaRecurrentModelStoreClass ))
#define OFA_IS_RECURRENT_MODEL_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RECURRENT_MODEL_STORE ))
#define OFA_IS_RECURRENT_MODEL_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RECURRENT_MODEL_STORE ))
#define OFA_RECURRENT_MODEL_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RECURRENT_MODEL_STORE, ofaRecurrentModelStoreClass ))

typedef struct {
	/*< public members >*/
	ofaListStore      parent;
}
	ofaRecurrentModelStore;

/**
 * ofaRecurrentModelStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaListStoreClass parent;
}
	ofaRecurrentModelStoreClass;

/**
 * The columns stored in the subjacent #GtkListStore.
 *                                                                  Type     Displayable
 *                                                                  -------  -----------
 * @REC_MODEL_COL_MNEMO             : mnemonic identifier           String       Yes
 * @REC_MODEL_COL_LABEL             : label                         String       Yes
 * @REC_MODEL_COL_OPE_TEMPLATE      : operation template            String       Yes
 * @REC_MODEL_COL_PERIODICITY       : periodicity                   String       Yes
 * @REC_MODEL_COL_PERIODICITY_DETAIL: periodicity detail            String       Yes
 * @REC_MODEL_COL_PERIOD_DETAIL_I   : periodicity detail            Long          No
 * @REC_MODEL_COL_DEF_AMOUNT1       : amount 1                      String       Yes
 * @REC_MODEL_COL_DEF_AMOUNT2       : amount 2                      String       Yes
 * @REC_MODEL_COL_DEF_AMOUNT3       : amount 3                      String       Yes
 * @REC_MODEL_COL_ENABLED           : whether the model is enabled  String       Yes
 * @REC_MODEL_COL_ENABLED_B         : whether the model is enabled  Bool          No
 * @REC_MODEL_COL_NOTES             : notes                         String       Yes
 * @REC_MODEL_COL_NOTES_PNG         : notes indicator               Pixbuf       Yes
 * @REC_MODEL_COL_UPD_USER          : last update user              String       Yes
 * @REC_MODEL_COL_UPD_STAMP         : last update timestamp         String       Yes
 * @REC_MODEL_COL_OBJECT            : #ofoRecurrentModel object     GObject       No
 */
enum {
	REC_MODEL_COL_MNEMO = 0,
	REC_MODEL_COL_LABEL,
	REC_MODEL_COL_OPE_TEMPLATE,
	REC_MODEL_COL_PERIODICITY,
	REC_MODEL_COL_PERIODICITY_DETAIL,
	REC_MODEL_COL_PERIOD_DETAIL_I,
	REC_MODEL_COL_DEF_AMOUNT1,
	REC_MODEL_COL_DEF_AMOUNT2,
	REC_MODEL_COL_DEF_AMOUNT3,
	REC_MODEL_COL_ENABLED,
	REC_MODEL_COL_ENABLED_B,
	REC_MODEL_COL_NOTES,
	REC_MODEL_COL_NOTES_PNG,
	REC_MODEL_COL_UPD_USER,
	REC_MODEL_COL_UPD_STAMP,
	REC_MODEL_COL_OBJECT,
	REC_MODEL_N_COLUMNS
};

GType                   ofa_recurrent_model_store_get_type( void );

ofaRecurrentModelStore *ofa_recurrent_model_store_new     ( ofaIGetter *getter );

gboolean                ofa_recurrent_model_store_get_iter( ofaRecurrentModelStore *store,
																ofoRecurrentModel *model,
																GtkTreeIter *iter );

G_END_DECLS

#endif /* __OFA_RECURRENT_MODEL_STORE_H__ */
