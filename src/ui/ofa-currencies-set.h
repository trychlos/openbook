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

#ifndef __OFA_CURRENCIES_SET_H__
#define __OFA_CURRENCIES_SET_H__

/**
 * SECTION: ofa_currencies_set
 * @short_description: #ofaCurrenciesSet class definition.
 * @include: ui/ofa-currencies-set.h
 *
 * Display the list of known currencies, letting the user edit it.
 *
 * The display treeview is sorted in a the ascending currency code
 * order with insensitive case.
 */

#include "core/ofa-main-page-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_CURRENCIES_SET                ( ofa_currencies_set_get_type())
#define OFA_CURRENCIES_SET( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_CURRENCIES_SET, ofaCurrenciesSet ))
#define OFA_CURRENCIES_SET_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_CURRENCIES_SET, ofaCurrenciesSetClass ))
#define OFA_IS_CURRENCIES_SET( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_CURRENCIES_SET ))
#define OFA_IS_CURRENCIES_SET_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_CURRENCIES_SET ))
#define OFA_CURRENCIES_SET_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_CURRENCIES_SET, ofaCurrenciesSetClass ))

typedef struct _ofaCurrenciesSetPrivate        ofaCurrenciesSetPrivate;

typedef struct {
	/*< private >*/
	ofaMainPage              parent;
	ofaCurrenciesSetPrivate *private;
}
	ofaCurrenciesSet;

typedef struct {
	/*< private >*/
	ofaMainPageClass parent;
}
	ofaCurrenciesSetClass;

GType ofa_currencies_set_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_CURRENCIES_SET_H__ */
