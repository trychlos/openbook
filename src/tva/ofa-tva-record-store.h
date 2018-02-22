/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#include "api/ofa-igetter-def.h"
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
 *                                                                               Type     Displayable
 *                                                                               -------  -----------
 * @TVA_RECORD_COL_MNEMO             : form mnemonic identifier                  String       Yes
 * @TVA_RECORD_COL_END               : declaration end                           String       Yes
 * @TVA_RECORD_COL_HAS_CORRESPONDENCE: has correspondence                        String       Yes
 * @TVA_RECORD_COL_CRE_USER          : creation user                             String       Yes
 * @TVA_RECORD_COL_CRE_STAMP         : creation timestamp                        String       Yes
 * @TVA_RECORD_COL_LABEL             : form label                                String       Yes
 * @TVA_RECORD_COL_CORRESPONDENCE    : correspondence                            String       Yes
 * @TVA_RECORD_COL_CORRESPONDENCE_PNG: correspondence indicator                  Pixbuf       Yes
 * @TVA_RECORD_COL_BEGIN             : declaration beginning                     String       Yes
 * @TVA_RECORD_COL_NOTES             : notes                                     String       Yes
 * @TVA_RECORD_COL_NOTES_PNG         : notes indicator                           Pixbuf       Yes
 * @TVA_RECORD_COL_UPD_USER          : last update user                          String       Yes
 * @TVA_RECORD_COL_UPD_STAMP         : last update timestamp                     String       Yes
 * @TVA_RECORD_COL_DOPE              : accounting operation date                 String       Yes
 * @TVA_RECORD_COL_OPE_USER          : operation user                            String       Yes
 * @TVA_RECORD_COL_OPE_STAMP         : operation timestamp                       String       Yes
 * @TVA_RECORD_COL_STATUS            : the validation status of the declaration  String       Yes
 * @TVA_RECORD_COL_STATUS_I          : the validation status of the declaration  Int           No
 * @TVA_RECORD_COL_STA_CLOSING       : date of closing at validation             String       Yes
 * @TVA_RECORD_COL_STA_USER          : user who last changes the status          String       Yes
 * @TVA_RECORD_COL_STA_STAMP         : stamp of last status change               String       Yes
 * @TVA_RECORD_COL_OBJECT            : #ofoTVARecord object                      GObject       No
 */
enum {
	TVA_RECORD_COL_MNEMO = 0,
	TVA_RECORD_COL_END,
	TVA_RECORD_COL_HAS_CORRESPONDENCE,
	TVA_RECORD_COL_CRE_USER,
	TVA_RECORD_COL_CRE_STAMP,
	TVA_RECORD_COL_LABEL,
	TVA_RECORD_COL_CORRESPONDENCE,
	TVA_RECORD_COL_CORRESPONDENCE_PNG,
	TVA_RECORD_COL_BEGIN,
	TVA_RECORD_COL_NOTES,
	TVA_RECORD_COL_NOTES_PNG,
	TVA_RECORD_COL_UPD_USER,
	TVA_RECORD_COL_UPD_STAMP,
	TVA_RECORD_COL_DOPE,
	TVA_RECORD_COL_OPE_USER,
	TVA_RECORD_COL_OPE_STAMP,
	TVA_RECORD_COL_STATUS,
	TVA_RECORD_COL_STATUS_I,
	TVA_RECORD_COL_STA_CLOSING,
	TVA_RECORD_COL_STA_USER,
	TVA_RECORD_COL_STA_STAMP,
	TVA_RECORD_COL_OBJECT,
	TVA_RECORD_N_COLUMNS
};

GType              ofa_tva_record_store_get_type( void );

ofaTVARecordStore *ofa_tva_record_store_new     ( ofaIGetter *getter );

G_END_DECLS

#endif /* __OFA_TVA_RECORD_STORE_H__ */
