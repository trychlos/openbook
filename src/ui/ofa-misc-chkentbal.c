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
#include "api/ofo-entry.h"

#include "ui/ofa-misc-chkentbal.h"

typedef struct {
	gchar    *currency;
	ofxAmount debit;
	ofxAmount credit;
}
	sBalance;

static void      impute_balance( GList **balances, const ofoEntry *entry, const gchar *currency, ofaBalancesGrid *grid );
static sBalance *get_balance_for_currency( GList **list, const gchar *currency );
static gboolean  check_balances( GList *balances );
static void      free_balance( sBalance *sbal );
static void      free_balances( GList *balances );

/**
 * ofa_misc_chkentbal_run:
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
ofa_misc_chkentbal_run( ofoDossier *dossier, myProgressBar *bar, ofaBalancesGrid *grid )
{
	GList *entries, *it;
	const GDate *dbegin, *dend;
	gint count, i;
	gdouble progress;
	gchar *text;
	ofoEntry *entry;
	const gchar *cstr;
	GList *balances;
	gboolean ok;

	balances = NULL;
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
		impute_balance( &balances, entry, cstr, grid );

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

	ok = check_balances( balances );

	free_balances( balances );

	return( ok );
}

static void
impute_balance( GList **balances, const ofoEntry *entry, const gchar *currency, ofaBalancesGrid *grid )
{
	sBalance *sbal;

	sbal = get_balance_for_currency( balances, currency );
	sbal->debit += ofo_entry_get_debit( entry );
	sbal->credit += ofo_entry_get_credit( entry );

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
