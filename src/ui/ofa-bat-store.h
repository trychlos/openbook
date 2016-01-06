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

#ifndef __OFA_BAT_STORE_H__
#define __OFA_BAT_STORE_H__

/**
 * SECTION: bat_store
 * @title: ofaBatStore
 * @short_description: The BatStore class definition
 * @include: ui/ofa-bat-store.h
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

#include "api/ofa-hub-def.h"
#include "api/ofa-list-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_BAT_STORE                ( ofa_bat_store_get_type())
#define OFA_BAT_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BAT_STORE, ofaBatStore ))
#define OFA_BAT_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BAT_STORE, ofaBatStoreClass ))
#define OFA_IS_BAT_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BAT_STORE ))
#define OFA_IS_BAT_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BAT_STORE ))
#define OFA_BAT_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BAT_STORE, ofaBatStoreClass ))

typedef struct _ofaBatStorePrivate        ofaBatStorePrivate;

typedef struct {
	/*< public members >*/
	ofaListStore        parent;

	/*< private members >*/
	ofaBatStorePrivate *priv;
}
	ofaBatStore;

/**
 * ofaBatStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaListStoreClass   parent;
}
	ofaBatStoreClass;

/**
 * The columns stored in the subjacent #GtkListStore.
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
	BAT_COL_NOTES,
	BAT_COL_COUNT,
	BAT_COL_UPD_USER,
	BAT_COL_UPD_STAMP,
	BAT_COL_OBJECT,
	BAT_N_COLUMNS
};

GType        ofa_bat_store_get_type( void );

ofaBatStore *ofa_bat_store_new     ( ofaHub *hub );

G_END_DECLS

#endif /* __OFA_BAT_STORE_H__ */
