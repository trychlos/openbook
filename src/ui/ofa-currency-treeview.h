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

#ifndef __OFA_CURRENCY_TREEVIEW_H__
#define __OFA_CURRENCY_TREEVIEW_H__

/**
 * SECTION: ofa_currency_treeview
 * @short_description: #ofaCurrencyTreeview class definition.
 * @include: ui/ofa-currency-treeview.h
 *
 * Manage a treeview with the list of the currencies.
 *
 * The class provides the following signals, which are proxyed from
 * #ofaTVBin base class.
 *    +------------------+--------------+
 *    | Signal           | Currency may |
 *    |                  | be %NULL     |
 *    +------------------+--------------+
 *    | ofa-curchanged   |      Yes     |
 *    | ofa-curactivated |       No     |
 *    | ofa-curdelete    |       No     |
 *    +------------------+--------------+
 */

#include "api/ofa-igetter-def.h"
#include "api/ofa-tvbin.h"
#include "api/ofo-currency-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_CURRENCY_TREEVIEW                ( ofa_currency_treeview_get_type())
#define OFA_CURRENCY_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_CURRENCY_TREEVIEW, ofaCurrencyTreeview ))
#define OFA_CURRENCY_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CURRENCY_CAST( klass, OFA_TYPE_CURRENCY_TREEVIEW, ofaCurrencyTreeviewCurrency ))
#define OFA_IS_CURRENCY_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_CURRENCY_TREEVIEW ))
#define OFA_IS_CURRENCY_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CURRENCY_TYPE(( klass ), OFA_TYPE_CURRENCY_TREEVIEW ))
#define OFA_CURRENCY_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_CURRENCY_TREEVIEW, ofaCurrencyTreeviewCurrency ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaCurrencyTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaCurrencyTreeviewClass;

GType                ofa_currency_treeview_get_type     ( void ) G_GNUC_CONST;

ofaCurrencyTreeview *ofa_currency_treeview_new          ( ofaIGetter *getter,
																const gchar *settings_prefix );

void                 ofa_currency_treeview_setup_store  ( ofaCurrencyTreeview *view );

ofoCurrency         *ofa_currency_treeview_get_selected ( ofaCurrencyTreeview *view );

G_END_DECLS

#endif /* __OFA_CURRENCY_TREEVIEW_H__ */
