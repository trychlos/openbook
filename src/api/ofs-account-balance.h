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

#ifndef __OPENBOOK_API_OFS_ACCOUNT_BALANCE_H__
#define __OPENBOOK_API_OFS_ACCOUNT_BALANCE_H__

/**
 * SECTION: ofs_account_balance
 * @short_description: #ofsAccountBalance0 structure definition.
 * @include: openbook/ofs-account-balance.h
 *
 * This structure holds the balance for an account.
 */

#include "api/ofa-box.h"
#include "api/ofo-account-def.h"
#include "api/ofo-currency-def.h"

G_BEGIN_DECLS

/**
 * ofsAccountBalance
 */
typedef struct {
	ofoAccount  *account;
	ofoCurrency *currency;
	ofxAmount    debit;
	ofxAmount    credit;
}
	ofsAccountBalance;

void     ofs_account_balance_list_free( GList **list );

G_END_DECLS

#endif /* __OPENBOOK_API_OFS_ACCOUNT_BALANCE_H__ */
