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

#include "api/ofa-hub.h"
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
												ofaHub *hub,
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

void         ofs_currency_list_dump    ( GList *list );

void         ofs_currency_list_free    ( GList **list );

G_END_DECLS

#endif /* __OPENBOOK_API_OFS_CURRENCY_H__ */
