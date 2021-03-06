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

#ifndef __OFA_BAT_STORE_H__
#define __OFA_BAT_STORE_H__

/**
 * SECTION: bat_store
 * @title: ofaBatStore
 * @short_description: The BatStore class definition
 * @include: core/ofa-bat-store.h
 *
 * The #ofaBatStore derived from #ofaListStore, which itself
 * derives from #GtkListStore. It is populated with all the BAT files
 * imported in the dossier on first call, and stay then alive until the
 * dossier is closed.
 *
 * Once more time: there is only one #ofaBatStore while the dossier
 * is opened. All the views are built on this store, using ad-hoc filter
 * models when needed.
 *
 * The #ofaBatStore takes advantage of the dossier signaling system to
 * maintain itself up to date.
 */

#include "api/ofa-igetter-def.h"
#include "api/ofa-list-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_BAT_STORE                ( ofa_bat_store_get_type())
#define OFA_BAT_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BAT_STORE, ofaBatStore ))
#define OFA_BAT_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BAT_STORE, ofaBatStoreClass ))
#define OFA_IS_BAT_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BAT_STORE ))
#define OFA_IS_BAT_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BAT_STORE ))
#define OFA_BAT_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BAT_STORE, ofaBatStoreClass ))

typedef struct {
	/*< public members >*/
	ofaListStore      parent;
}
	ofaBatStore;

/**
 * ofaBatStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaListStoreClass parent;
}
	ofaBatStoreClass;

/**
 * The columns stored in the subjacent #GtkListStore.
 *                                                                          Displayable
 *                                                                 Type     in treeview
 *                                                                 -------  -----------
 * @BAT_COL_ID             : identifier                            String       Yes
 * @BAT_COL_URI            : source uri                            String       Yes
 * @BAT_COL_FORMAT         : format                                String       Yes
 * @BAT_COL_BEGIN          : begin date                            String       Yes
 * @BAT_COL_END            : end date                              String       Yes
 * @BAT_COL_RIB            : rib                                   String       Yes
 * @BAT_COL_CURRENCY       : currency iso 3a                       String       Yes
 * @BAT_COL_BEGIN_SOLDE    : beginning solde                       String       Yes
 * @BAT_COL_BEGIN_SOLDE_SET: whether begin solde is set            Bool          No
 * @BAT_COL_END_SOLDE      : ending solde                          String       Yes
 * @BAT_COL_END_SOLDE_SET  : whether end solde is set              Bool          No
 * @BAT_COL_CRE_USER       : creation user                         String       Yes
 * @BAT_COL_CRE_STAMP      : creation timestamp                    String       Yes
 * @BAT_COL_NOTES          : notes                                 String       Yes
 * @BAT_COL_NOTES_PNG      : notes indicator                       Pixbuf       Yes
 * @BAT_COL_UPD_USER       : last update user                      String       Yes
 * @BAT_COL_UPD_STAMP      : last update timestamp                 String       Yes
 * @BAT_COL_ACCOUNT        : Openbook account                      String       Yes
 * @BAT_COL_ACC_USER       : account reconciliation user           String       Yes
 * @BAT_COL_ACC_STAMP      : account reconciliation timestamp      String       Yes
 * @BAT_COL_COUNT          : total lines count                     String       Yes
 * @BAT_COL_UNUSED         : unused lines count                    String       Yes
 * @BAT_COL_OBJECT         : #ofoBat object                        GObject       No
 */
enum {
	BAT_COL_ID = 0,
	BAT_COL_URI,
	BAT_COL_FORMAT,
	BAT_COL_BEGIN,
	BAT_COL_END,
	BAT_COL_RIB,
	BAT_COL_CURRENCY,
	BAT_COL_BEGIN_SOLDE,
	BAT_COL_BEGIN_SOLDE_SET,
	BAT_COL_END_SOLDE,
	BAT_COL_END_SOLDE_SET,
	BAT_COL_CRE_USER,
	BAT_COL_CRE_STAMP,
	BAT_COL_NOTES,
	BAT_COL_NOTES_PNG,
	BAT_COL_UPD_USER,
	BAT_COL_UPD_STAMP,
	BAT_COL_ACCOUNT,
	BAT_COL_ACC_USER,
	BAT_COL_ACC_STAMP,
	BAT_COL_COUNT,
	BAT_COL_UNUSED,
	BAT_COL_OBJECT,
	BAT_N_COLUMNS
};

GType        ofa_bat_store_get_type( void );

ofaBatStore *ofa_bat_store_new     ( ofaIGetter *getter );

G_END_DECLS

#endif /* __OFA_BAT_STORE_H__ */
