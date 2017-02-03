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

#ifndef __OFA_RATE_STORE_H__
#define __OFA_RATE_STORE_H__

/**
 * SECTION: rate_store
 * @title: ofaRateStore
 * @short_description: The RateStore class definition
 * @include: core/ofa-rate-store.h
 *
 * The #ofaRateStore derived from #ofaListStore, which itself
 * derives from #GtkListStore. It is populated with all the rates
 * of the dossier on first call, and stay then alive until the dossier
 * is closed.
 *
 * Once more time: there is only one #ofaRateStore while the dossier
 * is opened. All the views are built on this store, using ad-hoc filter
 * models when needed.
 *
 * The #ofaRateStore takes advantage of the dossier signaling
 * system to maintain itself up to date.
 */

#include "api/ofa-igetter-def.h"
#include "api/ofa-list-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_RATE_STORE                ( ofa_rate_store_get_type())
#define OFA_RATE_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RATE_STORE, ofaRateStore ))
#define OFA_RATE_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RATE_STORE, ofaRateStoreClass ))
#define OFA_IS_RATE_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RATE_STORE ))
#define OFA_IS_RATE_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RATE_STORE ))
#define OFA_RATE_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RATE_STORE, ofaRateStoreClass ))

typedef struct {
	/*< public members >*/
	ofaListStore      parent;
}
	ofaRateStore;

/**
 * ofaRateStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaListStoreClass parent;
}
	ofaRateStoreClass;

/**
 * The columns stored in the subjacent #GtkListStore.
 *                                                         Type     Displayable
 *                                                         -------  -----------
 * @RATE_COL_MNEMO     : mnemonic identifier               String       Yes
 * @RATE_COL_LABEL     : label                             String       Yes
 * @RATE_COL_NOTES     : notes                             String       Yes
 * @RATE_COL_NOTES_PNG : notes indicator                   Pixbuf       Yes
 * @RATE_COL_UPD_USER  : last update user                  String       Yes
 * @RATE_COL_UPD_STAMP : last update timestamp             String       Yes
 * @RATE_COL_OBJECT    : #ofoRate object                   GObject       No
 */
enum {
	RATE_COL_MNEMO = 0,
	RATE_COL_LABEL,
	RATE_COL_NOTES,
	RATE_COL_NOTES_PNG,
	RATE_COL_UPD_USER,
	RATE_COL_UPD_STAMP,
	RATE_COL_OBJECT,
	RATE_N_COLUMNS
};

GType         ofa_rate_store_get_type( void );

ofaRateStore *ofa_rate_store_new     ( ofaIGetter *getter );

G_END_DECLS

#endif /* __OFA_RATE_STORE_H__ */
