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
 */

#ifndef __OFS_ACCOUNT_BALANCE_H__
#define __OFS_ACCOUNT_BALANCE_H__

/**
 * SECTION: ofs_account_balance
 * @short_description: #ofsAccountBalance structure definition.
 * @include: api/ofs-account-balance.h
 *
 * This structure holds the balance for an account.
 */

#include "api/ofo-account-def.h"

G_BEGIN_DECLS

/**
 * ofsAccountBalance
 */
typedef struct {
	gchar     *account;
	ofxAmount  debit;
	ofxAmount  credit;
	gchar     *currency;
}
	ofsAccountBalance;

void     ofs_account_balance_list_add ( GList **list, const ofoAccount *account );

gboolean ofs_account_balance_list_find( const GList *list, const gchar *number );

void     ofs_account_balance_list_free( GList **list );

G_END_DECLS

#endif /* __OFS_ACCOUNT_BALANCE_H__ */
