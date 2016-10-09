/*
 * Open Firm Ledgering
 * A double-entry ledgering application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include <math.h>

#include <my/my-utils.h>

#include "api/ofo-ledger.h"
#include "api/ofs-ledger-balance.h"

static gint ledger_balance_cmp( const ofsLedgerBalance *a, const ofsLedgerBalance *b );
static void ledger_balance_free( ofsLedgerBalance *balance );

/**
 * ofs_ledger_balance_list_add:
 * @list: [in][out]: the list of balances.
 * @ledger: the #ofoLedger balance.
 *
 * Add to the @list an empty #ofsLedgerBalance structure for each
 * registered currency.
 */
void
ofs_ledger_balance_list_add( GList **list, ofoLedger *ledger )
{
	ofsLedgerBalance *balance;
	GList *currencies, *it;
	const gchar *mnemo, *currency;

	mnemo = ofo_ledger_get_mnemo( ledger );
	currencies = ofo_ledger_get_currencies( ledger );

	for( it=currencies ; it ; it=it->next ){
		currency = ( const gchar * ) it->data;
		balance = g_new0( ofsLedgerBalance, 1 );
		balance->ledger = g_strdup( mnemo );
		balance->currency = g_strdup( currency );
		balance->debit = 0;
		balance->credit = 0;
		*list = g_list_insert_sorted( *list, balance, ( GCompareFunc ) ledger_balance_cmp );
	}

	g_list_free( currencies );
}

static gint
ledger_balance_cmp( const ofsLedgerBalance *a, const ofsLedgerBalance *b )
{
	gint cmp;

	cmp = my_collate( a->ledger, b->ledger );
	if( cmp == 0 ){
		cmp = my_collate( a->currency, b->currency );
	}

	return( cmp );
}

/**
 * ofs_ledger_balance_list_find::
 * @list: a list of #ofsLedgerBalance structures.
 * @mnemo: a ledger identifier.
 * @currency: [allow-none]: a currency identifier.
 *
 * Returns: %TRUE if the specified @mnemo[+@currency] is already
 * registered in the @list.
 */
gboolean
ofs_ledger_balance_list_find( const GList *dataset, const gchar *mnemo, const gchar *currency )
{
	const GList *it;
	ofsLedgerBalance *balance;
	gint cmp;

	g_return_val_if_fail( my_strlen( mnemo ), FALSE );

	for( it=dataset ; it ; it=it->next ){
		balance = ( ofsLedgerBalance * ) it->data;
		cmp = my_collate( mnemo, balance->ledger );
		if( cmp == 0 ){
			if( !my_strlen( currency ) || !my_collate( currency, balance->currency )){
				return( TRUE );
			}
		}
		if( cmp < 0 ){
			return( FALSE );
		}
	}

	return( FALSE );
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
