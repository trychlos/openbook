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

#include "api/ofa-hub-def.h"
#include "api/ofa-list-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_RECURRENT_MODEL_STORE                ( ofa_recurrent_model_store_get_type())
#define OFA_RECURRENT_MODEL_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RECURRENT_MODEL_STORE, ofaRecurrentModelStore ))
#define OFA_RECURRENT_MODEL_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RECURRENT_MODEL_STORE, ofaRecurrentModelStoreClass ))
#define OFA_IS_RECURRENT_MODEL_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RECURRENT_MODEL_STORE ))
#define OFA_IS_RECURRENT_MODEL_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RECURRENT_MODEL_STORE ))
#define OFA_RECURRENT_MODEL_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RECURRENT_MODEL_STORE, ofaRecurrentModelStoreClass ))

typedef struct _ofaRecurrentModelStorePrivate         ofaRecurrentModelStorePrivate;

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
 */
enum {
	REC_MODEL_COL_MNEMO = 0,
	REC_MODEL_COL_LABEL,
	REC_MODEL_COL_OPE_TEMPLATE,
	REC_MODEL_COL_PERIODICITY,
	REC_MODEL_COL_PERIODICITY_DETAIL,
	REC_MODEL_COL_NOTES,
	REC_MODEL_COL_UPD_USER,
	REC_MODEL_COL_UPD_STAMP,
	REC_MODEL_COL_OBJECT,
	REC_N_COLUMNS
};

/**
 * ofaCurrencyColumns:
 * The columns displayed in the views.
 */
typedef enum {
	REC_MODEL_DISP_MNEMO              = 1 << 0,
	REC_MODEL_DISP_LABEL              = 1 << 1,
	REC_MODEL_DISP_OPE_TEMPLATE       = 1 << 2,
	REC_MODEL_DISP_PERIODICITY        = 1 << 3,
	REC_MODEL_DISP_PERIODICITY_DETAIL = 1 << 4,
	REC_MODEL_DISP_NOTES              = 1 << 5,
	REC_MODEL_DISP_UPD_USER           = 1 << 6,
	REC_MODEL_DISP_UPD_STAMP          = 1 << 7
}
	ofeRecurrentModelColumns;

GType                   ofa_recurrent_model_store_get_type( void );

ofaRecurrentModelStore *ofa_recurrent_model_store_new     ( ofaHub *hub );

G_END_DECLS

#endif /* __OFA_RECURRENT_MODEL_STORE_H__ */
