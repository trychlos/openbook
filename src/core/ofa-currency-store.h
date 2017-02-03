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

#ifndef __OFA_CURRENCY_STORE_H__
#define __OFA_CURRENCY_STORE_H__

/**
 * SECTION: currency_store
 * @title: ofaCurrencyStore
 * @short_description: The CurrencyStore class definition
 * @include: core/ofa-currency-store.h
 *
 * The #ofaCurrencyStore derived from #ofaListStore, which itself
 * derives from #GtkListStore. It is populated with all the currencies
 * of the dossier on first call, and stay then alive until the dossier
 * is closed.
 *
 * Once more time: there is only one #ofaCurrencyStore while the dossier
 * is opened. All the views are built on this store, using ad-hoc filter
 * models when needed.
 *
 * The #ofaCurrencyStore takes advantage of the dossier signaling
 * system to maintain itself up to date.
 */

#include "api/ofa-igetter-def.h"
#include "api/ofa-list-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_CURRENCY_STORE                ( ofa_currency_store_get_type())
#define OFA_CURRENCY_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_CURRENCY_STORE, ofaCurrencyStore ))
#define OFA_CURRENCY_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_CURRENCY_STORE, ofaCurrencyStoreClass ))
#define OFA_IS_CURRENCY_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_CURRENCY_STORE ))
#define OFA_IS_CURRENCY_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_CURRENCY_STORE ))
#define OFA_CURRENCY_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_CURRENCY_STORE, ofaCurrencyStoreClass ))

typedef struct {
	/*< public members >*/
	ofaListStore      parent;
}
	ofaCurrencyStore;

/**
 * ofaCurrencyStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaListStoreClass parent;
}
	ofaCurrencyStoreClass;

/**
 * The columns stored in the subjacent #GtkListStore.
 *                                                             Type     Displayable
 *                                                             -------  -----------
 * @CURRENCY_COL_CODE      : identifier                        String       Yes
 * @CURRENCY_COL_LABEL     : label                             String       Yes
 * @CURRENCY_COL_SYMBOL    : symbol                            String       Yes
 * @CURRENCY_COL_DIGITS    : decimal digits count              String       Yes
 * @CURRENCY_COL_NOTES     : notes                             String       Yes
 * @CURRENCY_COL_NOTES_PNG : notes indicator                   Pixbuf       Yes
 * @CURRENCY_COL_UPD_USER  : last update user                  String       Yes
 * @CURRENCY_COL_UPD_STAMP : last update timestamp             String       Yes
 * @CURRENCY_COL_OBJECT    : #ofoCurrency object               GObject       No
 */
enum {
	CURRENCY_COL_CODE = 0,
	CURRENCY_COL_LABEL,
	CURRENCY_COL_SYMBOL,
	CURRENCY_COL_DIGITS,
	CURRENCY_COL_NOTES,
	CURRENCY_COL_NOTES_PNG,
	CURRENCY_COL_UPD_USER,
	CURRENCY_COL_UPD_STAMP,
	CURRENCY_COL_OBJECT,
	CURRENCY_N_COLUMNS
};

GType             ofa_currency_store_get_type( void );

ofaCurrencyStore *ofa_currency_store_new     ( ofaIGetter *getter );

G_END_DECLS

#endif /* __OFA_CURRENCY_STORE_H__ */
