/*
 * Open Firm Ledgering
 * A double-entry ledgering application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Ledgering is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Ledgering is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Ledgering; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifndef __OPENBOOK_API_OFS_LEDGER_BALANCE_H__
#define __OPENBOOK_API_OFS_LEDGER_BALANCE_H__

/**
 * SECTION: ofs_ledger_balance
 * @short_description: #ofsLedgerBalance structure definition.
 * @include: openbook/ofs-ledger-balance.h
 *
 * This structure holds the balance per currency for a ledger.
 */

#include "api/ofo-ledger-def.h"

G_BEGIN_DECLS

/**
 * ofsLedgerBalance
 */
typedef struct {
	gchar     *ledger;
	gchar     *currency;
	ofxAmount  debit;
	ofxAmount  credit;
}
	ofsLedgerBalance;

ofsLedgerBalance *ofs_ledger_balance_find_currency( GList *list,
															const gchar *ledger,
															const gchar *currency );

void              ofs_ledger_balance_list_free    ( GList **list );

G_END_DECLS

#endif /* __OPENBOOK_API_OFS_LEDGER_BALANCE_H__ */
