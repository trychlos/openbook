/*
 * Open Firm Ledgering
 * A double-entry ledgering application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "my/my-utils.h"

#include "api/ofo-ledger.h"
#include "api/ofs-ledger-balance.h"

static void ledger_balance_free( ofsLedgerBalance *balance );

/**
 * ofs_ledger_balance_find_currency:
 * @list: a list of #ofsLedgerBalance.
 * @ledger: [allow-none]: a #ofoLedger identifier.
 * @currency: [allow-none]: a #ofoCurrency identifier.
 *
 * Returns: the #ofsLedgerBalance structure which holds this @ledger
 * (if specified), and this @currency (if specified).
 */
ofsLedgerBalance *
ofs_ledger_balance_find_currency( GList *list, const gchar *ledger, const gchar *currency )
{
	ofsLedgerBalance *sbal;
	GList *it;
	gboolean ok_ledger, ok_currency;

	for( it=list ; it ; it=it->next ){
		sbal = ( ofsLedgerBalance * ) it->data;
		ok_ledger = ( !my_strlen( ledger ) || !my_collate( ledger, sbal->ledger ));
		ok_currency = ( !my_strlen( currency ) || !my_collate( currency, sbal->currency ));
		if( ok_ledger && ok_currency ){
			return( sbal );
		}
	}

	return( NULL );
}

/**
 * ofs_ledger_balance_list_free:
 * @list: a list of #ofsLedgerBalance.
 *
 * Free the @list.
 */
void
ofs_ledger_balance_list_free( GList **list )
{
	if( *list ){
		g_list_free_full( *list, ( GDestroyNotify ) ledger_balance_free );
		*list = NULL;
	}
}

static void
ledger_balance_free( ofsLedgerBalance *balance )
{
	g_free( balance->ledger );
	g_free( balance->currency );
	g_free( balance );
}
