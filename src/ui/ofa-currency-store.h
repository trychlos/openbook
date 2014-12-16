/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifndef __OFA_CURRENCY_STORE_H__
#define __OFA_CURRENCY_STORE_H__

/**
 * SECTION: currency_store
 * @title: ofaCurrencyStore
 * @short_description: The CurrencyStore Interface
 * @include: ui/ofa-currency-istore.h
 *
 * The #ofaCurrencyStore interface manages the subjacent GtkListStore
 * of currency views.
 */

#include "api/ofo-dossier-def.h"

#include "ui/ofa-list-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_CURRENCY_STORE                ( ofa_currency_store_get_type())
#define OFA_CURRENCY_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_CURRENCY_STORE, ofaCurrencyStore ))
#define OFA_CURRENCY_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_CURRENCY_STORE, ofaCurrencyStoreClass ))
#define OFA_IS_CURRENCY_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_CURRENCY_STORE ))
#define OFA_IS_CURRENCY_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_CURRENCY_STORE ))
#define OFA_CURRENCY_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_CURRENCY_STORE, ofaCurrencyStoreClass ))

typedef struct _ofaCurrencyStorePrivate        ofaCurrencyStorePrivate;

typedef struct {
	/*< public members >*/
	ofaListStore             parent;

	/*< private members >*/
	ofaCurrencyStorePrivate *priv;
}
	ofaCurrencyStore;

/**
 * ofaCurrencyStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaListStoreClass        parent;
}
	ofaCurrencyStoreClass;

/**
 * The columns stored in the subjacent #GtkListStore.
 */
enum {
	CURRENCY_COL_CODE = 0,
	CURRENCY_COL_LABEL,
	CURRENCY_COL_SYMBOL,
	CURRENCY_COL_DIGITS,
	CURRENCY_COL_NOTES,
	CURRENCY_COL_UPD_USER,
	CURRENCY_COL_UPD_STAMP,
	CURRENCY_COL_OBJECT,
	CURRENCY_N_COLUMNS
};

GType             ofa_currency_store_get_type( void );

ofaCurrencyStore *ofa_currency_store_new     ( ofoDossier *dossier );

G_END_DECLS

#endif /* __OFA_CURRENCY_STORE_H__ */
