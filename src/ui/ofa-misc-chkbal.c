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
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"

#include "ui/ofa-misc-chkbal.h"

typedef struct {
	gchar    *currency;
	ofxAmount debit;
	ofxAmount credit;
}
	sBalance;

static void      impute_acc_balance( GList **balances, const ofoAccount *account, const gchar *currency, ofaBalancesGrid *grid );
static void      impute_ent_balance( GList **balances, const ofoEntry *entry, const gchar *currency, ofaBalancesGrid *grid );
static void      impute_led_balance( GList **balances, const ofoLedger *ledger, const gchar *currency, ofaBalancesGrid *grid );
static sBalance *get_balance_for_currency( GList **list, const gchar *currency );
static gboolean  check_balances( GList *balances );
static gboolean  cmp_lists( GList *list_a, GList *list_b );
static void      free_balance( sBalance *sbal );

/**
 * ofa_misc_chkbalacc_run:
 * @dossier: the currently opened #ofoDossier dossier.
 * @bar: [allow-none]:  #myProgressBar to let the user see the
 *  progression.
 *
 * Check that the accounts are well balanced.
 *
 * Returns: %TRUE if the accounts are well balanced, %FALSE else.
 */
gboolean
ofa_misc_chkbalacc_run( ofoDossier *dossier, GList **balances, myProgressBar *bar, ofaBalancesGrid *grid )
{
	GList *accounts, *it;
	gint count, i;
	gdouble progress;
	gchar *text;
	ofoAccount *account;
	const gchar *cstr;
	gboolean ok;

	*balances = NULL;
	accounts = ofo_account_get_dataset( dossier );
	count = g_list_length( accounts );

	for( i=1, it=accounts ; it && count ; ++i, it=it->next ){
		/* a small delay so that user actually see the progression
		 * else it is too fast and we just see the end */
		if( bar ){
			g_usleep( 0.01*G_USEC_PER_SEC );
		}

		account = OFO_ACCOUNT( it->data );
		if( !ofo_account_is_root( account )){
			cstr = ofo_account_get_currency( account );
			impute_acc_balance( balances, account, cstr, grid );
		}

		/* set the progression */
		if( bar ){
			progress = ( gdouble ) i / ( gdouble ) count;
			g_signal_emit_by_name( bar, "double", progress );
			text = g_strdup_printf( "%d/%d", i+1, count );
			g_signal_emit_by_name( bar, "text", text );
			g_free( text );
		}
	}

	ok = check_balances( *balances );

	return( ok );
}

static void
impute_acc_balance( GList **balances, const ofoAccount *account, const gchar *currency, ofaBalancesGrid *grid )
{
	sBalance *sbal;

	sbal = get_balance_for_currency( balances, currency );
	sbal->debit +=
			ofo_account_get_val_debit( account )
			+ ofo_account_get_rough_debit( account );
	sbal->credit +=
			ofo_account_get_val_credit( account )
			+ ofo_account_get_rough_credit( account );

	g_signal_emit_by_name( grid, "update", currency, sbal->debit, sbal->credit );
}

/**
 * ofa_misc_chkbalent_run:
 * @dossier: the currently opened #ofoDossier dossier.
 * @bar: [allow-none]:  #myProgressBar to let the user see the
 *  progression.
 *
 * Check that the entries of the current exercice are well balanced.
 * If beginning or ending dates of the exercice are not set, then
 * all found entries are checked.
 *
 * All entries (validated or rough) between the beginning and ending
 * dates are considered.
 *
 * Returns: %TRUE if the entries are well balanced, %FALSE else.
 */
gboolean
ofa_misc_chkbalent_run( ofoDossier *dossier, GList **balances, myProgressBar *bar, ofaBalancesGrid *grid )
{
	GList *entries, *it;
	const GDate *dbegin, *dend;
	gint count, i;
	gdouble progress;
	gchar *text;
	ofoEntry *entry;
	const gchar *cstr;
	gboolean ok;

	*balances = NULL;
	dbegin = ofo_dossier_get_exe_begin( dossier );
	dend = ofo_dossier_get_exe_end( dossier );
	entries = ofo_entry_get_dataset_for_print_general_books( dossier, NULL, NULL, dbegin, dend );
	count = g_list_length( entries );

	for( i=1, it=entries ; it && count ; ++i, it=it->next ){
		/* a small delay so that user actually see the progression
		 * else it is too fast and we just see the end */
		if( bar ){
			g_usleep( 0.01*G_USEC_PER_SEC );
		}

		entry = OFO_ENTRY( it->data );
		cstr = ofo_entry_get_currency( entry );
		impute_ent_balance( balances, entry, cstr, grid );

		/* set the progression */
		if( bar ){
			progress = ( gdouble ) i / ( gdouble ) count;
			g_signal_emit_by_name( bar, "double", progress );
			text = g_strdup_printf( "%d/%d", i+1, count );
			g_signal_emit_by_name( bar, "text", text );
			g_free( text );
		}
	}

	ofo_entry_free_dataset( entries );

	ok = check_balances( *balances );

	return( ok );
}

static void
impute_ent_balance( GList **balances, const ofoEntry *entry, const gchar *currency, ofaBalancesGrid *grid )
{
	sBalance *sbal;

	sbal = get_balance_for_currency( balances, currency );
	sbal->debit += ofo_entry_get_debit( entry );
	sbal->credit += ofo_entry_get_credit( entry );

	g_signal_emit_by_name( grid, "update", currency, sbal->debit, sbal->credit );
}

/**
 * ofa_misc_chkballed_run:
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
ofa_misc_chkballed_run( ofoDossier *dossier, GList **balances, myProgressBar *bar, ofaBalancesGrid *grid )
{
	GList *ledgers, *it;
	GList *currencies, *ic;
	gint count, i;
	gdouble progress;
	gchar *text;
	ofoLedger *ledger;
	gboolean ok;

	*balances = NULL;
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
			impute_led_balance( balances, ledger, ( const gchar * ) ic->data, grid );
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

	ok = check_balances( *balances );

	return( ok );
}

static void
impute_led_balance( GList **balances, const ofoLedger *ledger, const gchar *currency, ofaBalancesGrid *grid )
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

/**
 * ofa_misc_chkbalsame_run:
 *
 * Check that the balances are the same.
 */
gboolean
ofa_misc_chkbalsame_run( GList *entries_list, GList *ledgers_list, GList *accounts_list )
{
	gboolean ok;

	ok = cmp_lists( entries_list, ledgers_list );
	ok &= cmp_lists( entries_list, accounts_list );

	return( ok );
}

static gboolean
cmp_lists( GList *list_a, GList *list_b )
{
	GList *it;
	sBalance *sbal_a, *sbal_b;

	/* check that all 'a' records are found and same in list_b */
	for( it=list_a ; it ; it=it->next ){
		sbal_a = ( sBalance * ) it->data;
		sbal_b = get_balance_for_currency( &list_b, sbal_a->currency );
		if( sbal_a->debit != sbal_b->debit || sbal_a->credit != sbal_b->credit ){
			return( FALSE );
		}
	}

	/* check that all 'b' records are found and same in list_a */
	for( it=list_b ; it ; it=it->next ){
		sbal_b = ( sBalance * ) it->data;
		sbal_a = get_balance_for_currency( &list_a, sbal_b->currency );
		if( sbal_b->debit != sbal_a->debit || sbal_b->credit != sbal_a->credit ){
			return( FALSE );
		}
	}

	return( TRUE );
}

static void
free_balance( sBalance *sbal )
{
	g_free( sbal->currency );
	g_free( sbal );
}

/**
 * ofa_misc_chkbal_free:
 */
void
ofa_misc_chkbal_free( GList *balances )
{
	g_list_free_full( balances, ( GDestroyNotify ) free_balance );
}
