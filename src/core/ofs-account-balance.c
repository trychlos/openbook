/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>

#include "api/ofo-account.h"
#include "api/ofs-account-balance.h"

static gint account_balance_cmp( const ofsAccountBalance *a, const ofsAccountBalance *b );
static void account_balance_free( ofsAccountBalance *balance );

/**
 * ofs_account_balance_list_add
 */
void
ofs_account_balance_list_add( GList **list, const ofoAccount *account )
{
	ofsAccountBalance *balance;

	balance = g_new0( ofsAccountBalance, 1 );
	balance->account = g_strdup( ofo_account_get_number( account ));
	balance->debit = 0;
	balance->credit = 0;
	balance->currency = g_strdup( ofo_account_get_currency( account ));

	*list = g_list_insert_sorted( *list, balance, ( GCompareFunc ) account_balance_cmp );
}

static gint
account_balance_cmp( const ofsAccountBalance *a, const ofsAccountBalance *b )
{
	return( g_utf8_collate( a->account, b->account ));
}

/**
 * ofs_account_balance_list_find:
 */
gboolean
ofs_account_balance_list_find( const GList *dataset, const gchar *number )
{
	const GList *it;
	ofsAccountBalance *balance;
	gint cmp;

	for( it=dataset ; it ; it=it->next ){
		balance = ( ofsAccountBalance * ) it->data;
		cmp = g_utf8_collate( number, balance->account );
		if( cmp == 0 ){
			return( TRUE );
		}
		if( cmp < 0 ){
			return( FALSE );
		}
	}

	return( FALSE );
}

/**
 * ofs_account_balance_list_free:
 */
void
ofs_account_balance_list_free( GList **list )
{
	if( *list ){
		g_list_free_full( *list, ( GDestroyNotify ) account_balance_free );
		*list = NULL;
	}
}

static void
account_balance_free( ofsAccountBalance *balance )
{
	g_free( balance->account );
	g_free( balance->currency );
	g_free( balance );
}
