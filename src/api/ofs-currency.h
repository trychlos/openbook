/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#ifndef __OFS_CURRENCY_H__
#define __OFS_CURRENCY_H__

/**
 * SECTION: ofs_currency
 * @short_description: #ofsCurrency structure definition.
 * @include: api/ofs-currency.h
 *
 * This structure is used when computing balances per currency.
 */

#include <glib.h>

G_BEGIN_DECLS

/**
 * ofsCurrency:
 */
typedef struct {
	gchar  *currency;
	gdouble debit;
	gdouble credit;
}
	ofsCurrency;

void ofs_currency_add_currency( GList **list,
										const gchar *currency,
										gdouble debit,
										gdouble credit );

void ofs_currency_list_dump   ( GList *list );

void ofs_currency_list_free   ( GList **list );

G_END_DECLS

#endif /* __OFS_CURRENCY_H__ */
