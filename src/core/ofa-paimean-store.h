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

#ifndef __OFA_PAIMEAN_STORE_H__
#define __OFA_PAIMEAN_STORE_H__

/**
 * SECTION: paimean_store
 * @title: ofaPaimeanStore
 * @short_description: The PaimeanStore class definition
 * @include: core/ofa-paimean-store.h
 *
 * The #ofaPaimeanStore derived from #ofaListStore, which itself
 * derives from #GtkListStore. It is populated with all the paimeans
 * of the dossier on first call, and stay then alive until the dossier
 * is closed.
 *
 * There is only one #ofaPaimeanStore while the dossier is opened.
 * All the views are built on this store.
 *
 * The #ofaPaimeanStore takes advantage of the dossier signaling
 * system to maintain itself up to date.
 */

#include "api/ofa-igetter-def.h"
#include "api/ofa-list-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_PAIMEAN_STORE                ( ofa_paimean_store_get_type())
#define OFA_PAIMEAN_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PAIMEAN_STORE, ofaPaimeanStore ))
#define OFA_PAIMEAN_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PAIMEAN_STORE, ofaPaimeanStoreClass ))
#define OFA_IS_PAIMEAN_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PAIMEAN_STORE ))
#define OFA_IS_PAIMEAN_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PAIMEAN_STORE ))
#define OFA_PAIMEAN_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PAIMEAN_STORE, ofaPaimeanStoreClass ))

typedef struct {
	/*< public members >*/
	ofaListStore      parent;
}
	ofaPaimeanStore;

/**
 * ofaPaimeanStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaListStoreClass parent;
}
	ofaPaimeanStoreClass;

/**
 * The columns stored in the subjacent #GtkListStore.
 *                                                         Type     Displayable
 *                                                         -------  -----------
 * @PAM_COL_CODE       : mnemonic identifier               String       Yes
 * @PAM_COL_CRE_USER   : creation user                     String       Yes
 * @PAM_COL_CRE_STAMP  : creation timestamp                String       Yes
 * @PAM_COL_LABEL      : label                             String       Yes
 * @PAM_COL_ACCOUNT    : account                           String       Yes
 * @PAM_COL_NOTES      : notes                             String       Yes
 * @PAM_COL_NOTES_PNG  : notes indicator                   Pixbuf       Yes
 * @PAM_COL_UPD_USER   : last update user                  String       Yes
 * @PAM_COL_UPD_STAMP  : last update timestamp             String       Yes
 * @PAM_COL_OBJECT     : #ofoPaimean object                GObject       No
 */
enum {
	PAM_COL_CODE = 0,
	PAM_COL_CRE_USER,
	PAM_COL_CRE_STAMP,
	PAM_COL_LABEL,
	PAM_COL_ACCOUNT,
	PAM_COL_NOTES,
	PAM_COL_NOTES_PNG,
	PAM_COL_UPD_USER,
	PAM_COL_UPD_STAMP,
	PAM_COL_OBJECT,
	PAM_N_COLUMNS
};

GType            ofa_paimean_store_get_type( void );

ofaPaimeanStore *ofa_paimean_store_new     ( ofaIGetter *getter );

G_END_DECLS

#endif /* __OFA_PAIMEAN_STORE_H__ */
