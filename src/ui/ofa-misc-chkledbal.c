/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/ofa-boxed.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"

#include "ui/ofa-misc-chkledbal.h"

typedef struct {
	gchar    *currency;
	ofxAmount debit;
	ofxAmount credit;
}
	sBalance;

static void      impute_balance( GList **balances, const ofoLedger *ledger, const gchar *currency, ofaBalancesGrid *grid );
static sBalance *get_balance_for_currency( GList **list, const gchar *currency );
static gboolean  check_balances( GList *balances );
static void      free_balance( sBalance *sbal );
static void      free_balances( GList *balances );

/**
 * ofa_misc_chkledbal_run:
 * @dossier: the currently opened #ofoDossier dossier.
 * @bar: [allow-none]:  #myProgressBar to let the user see the
 *  progression.
 *
 * Check that the ledgers of the current exercice are well balanced.
 * If beginning or ending dates of the exercice are not set, then
 * all found ledgers are checked.
 *
 * All entries (validated or rough) between the beginning and ending
 * dates are considered.
 *
 * Returns: %TRUE if the entries are well balanced, %FALSE else.
 */
gboolean
ofa_misc_chkledbal_run( ofoDossier *dossier, myProgressBar *bar, ofaBalancesGrid *grid )
{
	GList *ledgers, *it;
	GList *currencies, *ic;
	gint count, i;
	gdouble progress;
	gchar *text;
	ofoLedger *ledger;
	GList *balances;
	gboolean ok;

	balances = NULL;
	ledgers = ofo_ledger_get_dataset( dossier );
	count = g_list_length( ledgers );

	for( i=1, it=ledgers ; it && count ; ++i, it=it->next ){
		/* a small delay so that user actually see the progression
		 * else it is too fast and we just see the end */
		if( bar ){
			g_usleep( 0.01*G_USEC_PER_SEC );
		}

		ledger = OFO_LEDGER( it->data );
		currencies = ofo_ledger_get_currencies( ledger );
		for( ic=currencies ; ic ; ic=ic->next ){
			impute_balance( &balances, ledger, ( const gchar * ) ic->data, grid );
		}
		g_list_free( currencies );

		/* set the progression */
		if( bar ){
			progress = ( gdouble ) i / ( gdouble ) count;
			g_signal_emit_by_name( bar, "double", progress );
			text = g_strdup_printf( "%d/%d", i+1, count );
			g_signal_emit_by_name( bar, "text", text );
			g_free( text );
		}
	}

	ok = check_balances( balances );

	free_balances( balances );

	return( ok );
}

static void
impute_balance( GList **balances, const ofoLedger *ledger, const gchar *currency, ofaBalancesGrid *grid )
{
	sBalance *sbal;

	sbal = get_balance_for_currency( balances, currency );
	sbal->debit +=
			ofo_ledger_get_clo_deb( ledger, currency )
			+ ofo_ledger_get_deb( ledger, currency );
	sbal->credit +=
			ofo_ledger_get_clo_cre( ledger, currency )
			+ ofo_ledger_get_cre( ledger, currency );

	g_signal_emit_by_name( grid, "update", currency, sbal->debit, sbal->credit );
}

static sBalance *
get_balance_for_currency( GList **list, const gchar *currency )
{
	GList *it;
	sBalance *sbal;
	gboolean found;

	found = FALSE;

	for( it=*list ; it ; it=it->next ){
		sbal = ( sBalance * ) it->data;
		if( !g_utf8_collate( sbal->currency, currency )){
			found = TRUE;
			break;
		}
	}

	if( !found ){
		sbal = g_new0( sBalance, 1 );
		sbal->currency = g_strdup( currency );
		*list = g_list_prepend( *list, sbal );
	}

	return( sbal );
}

static gboolean
check_balances( GList *balances )
{
	gboolean ok;
	GList *it;
	sBalance *sbal;

	ok = TRUE;
	for( it=balances ; it ; it=it->next ){
		sbal = ( sBalance * ) it->data;
		ok &= ( sbal->debit == sbal->credit );
	}

	return( ok );
}

static void
free_balance( sBalance *sbal )
{
	g_free( sbal->currency );
	g_free( sbal );
}

static void
free_balances( GList *balances )
{
	g_list_free_full( balances, ( GDestroyNotify ) free_balance );
}
