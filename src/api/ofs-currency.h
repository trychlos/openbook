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

#ifndef __OPENBOOK_API_OFS_CURRENCY_H__
#define __OPENBOOK_API_OFS_CURRENCY_H__

/**
 * SECTION: ofs_currency
 * @short_description: #ofsCurrency structure definition.
 * @include: openbook/ofs-currency.h
 *
 * This structure is used when computing balances per currency.
 *
 * Two modes may be used:
 *
 * 1/ directly computing with floating point (gdouble) amounts: this
 * may be slighty faster (as less code is run), but this is at the
 * risk of some rounding errors
 *
 * 2/ computing after conversion to integers
 * (#ofs_currency_amount_to_long() function), and checking the balances
 * after floating point updates (#ofs_currency_update_amounts() function).
 */

#include <glib.h>

#include "api/ofa-igetter-def.h"
#include "api/ofo-currency-def.h"

G_BEGIN_DECLS

/**
 * ofsCurrency:
 */
typedef struct {
	ofoCurrency *currency;
	gdouble      debit;
	gdouble      credit;
}
	ofsCurrency;

ofsCurrency *ofs_currency_add_by_code  ( GList **list,
												ofaIGetter *getter,
												const gchar *currency,
												gdouble debit,
												gdouble credit );

ofsCurrency *ofs_currency_add_by_object( GList **list,
												ofoCurrency *currency,
												gdouble debit,
												gdouble credit );

ofsCurrency *ofs_currency_get_by_code  ( GList *list,
												const gchar *currency );

gboolean     ofs_currency_is_balanced  ( const ofsCurrency *currency );

gboolean     ofs_currency_is_zero      ( const ofsCurrency *currency );

gint         ofs_currency_cmp          ( const ofsCurrency *a, const ofsCurrency *b );

void         ofs_currency_list_dump    ( GList *list );

GList       *ofs_currency_list_copy    ( GList *list );

void         ofs_currency_list_free    ( GList **list );

G_END_DECLS

#endif /* __OPENBOOK_API_OFS_CURRENCY_H__ */
