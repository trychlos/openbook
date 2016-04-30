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

#ifndef __OFA_TVA_RECORD_STORE_H__
#define __OFA_TVA_RECORD_STORE_H__

/**
 * SECTION: tva_record_store
 * @title: ofaTVARecordStore
 * @short_description: The TVARecordStore class definition
 * @include: tva/ofa-tva-record-store.h
 *
 * The #ofaTVARecordStore derived from #ofaListStore, which itself
 * derives from #GtkListStore. It is populated with all the TVA recorded
 * declarations defined on the dossier on first call, and stay then alive
 * until the dossier is closed.
 *
 * Once more time: there is only one #ofaTVARecordStore while the dossier
 * is opened. All the views are built on this store, using ad-hoc filter
 * models when needed.
 *
 * The #ofaTVARecordStore takes advantage of the dossier signaling
 * system to maintain itself up to date.
 */

#include "api/ofa-hub-def.h"
#include "api/ofa-list-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_TVA_RECORD_STORE                ( ofa_tva_record_store_get_type())
#define OFA_TVA_RECORD_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_TVA_RECORD_STORE, ofaTVARecordStore ))
#define OFA_TVA_RECORD_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_TVA_RECORD_STORE, ofaTVARecordStoreClass ))
#define OFA_IS_TVA_RECORD_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_TVA_RECORD_STORE ))
#define OFA_IS_TVA_RECORD_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_TVA_RECORD_STORE ))
#define OFA_TVA_RECORD_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_TVA_RECORD_STORE, ofaTVARecordStoreClass ))

typedef struct {
	/*< public members >*/
	ofaListStore      parent;
}
	ofaTVARecordStore;

/**
 * ofaTVARecordStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaListStoreClass parent;
}
	ofaTVARecordStoreClass;

/**
 * The columns stored in the subjacent #GtkListStore.
 */
enum {
	TVA_RECORD_COL_MNEMO = 0,
	TVA_RECORD_COL_LABEL,
	TVA_RECORD_COL_IS_VALIDATED,
	TVA_RECORD_COL_BEGIN,
	TVA_RECORD_COL_END,
	TVA_RECORD_COL_DOPE,
	TVA_RECORD_COL_OBJECT,
	TVA_RECORD_N_COLUMNS
};

GType              ofa_tva_record_store_get_type( void );

ofaTVARecordStore *ofa_tva_record_store_new     ( ofaHub *hub );

G_END_DECLS

#endif /* __OFA_TVA_RECORD_STORE_H__ */
