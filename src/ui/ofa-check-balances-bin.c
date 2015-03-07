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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <math.h>

#include "api/my-utils.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofs-currency.h"

#include "ui/my-progress-bar.h"
#include "ui/ofa-balances-grid.h"
#include "ui/ofa-check-balances-bin.h"

/* private instance data
 */
struct _ofaCheckBalancesBinPrivate {
	gboolean    dispose_has_run;

	/* runtime data
	 */
	ofoDossier *dossier;

	gboolean    entries_ok;
	GList      *entries_list;
	gboolean    ledgers_ok;
	GList      *ledgers_list;
	gboolean    accounts_ok;
	GList      *accounts_list;

	gboolean    result;
};

/* signals defined here
 */
enum {
	DONE = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-check-balances-bin.ui";
static const gchar *st_ui_id            = "CheckBalancesBin";

G_DEFINE_TYPE( ofaCheckBalancesBin, ofa_check_balances_bin, GTK_TYPE_BIN )

static void             load_dialog( ofaCheckBalancesBin *self );
static gboolean         do_run( ofaCheckBalancesBin *bin );
static void             check_entries_balance_run( ofaCheckBalancesBin *bin );
static void             check_ledgers_balance_run( ofaCheckBalancesBin *bin );
static void             check_accounts_balance_run( ofaCheckBalancesBin *bin );
static ofsCurrency     *get_balance_for_currency( ofaCheckBalancesBin *bin, GList **list, const gchar *currency );
static gboolean         check_balances_per_currency( ofaCheckBalancesBin *bin, GList *balances );
static void             set_balance_status( ofaCheckBalancesBin *bin, gboolean ok, const gchar *w_name );
static myProgressBar   *get_new_bar( ofaCheckBalancesBin *bin, const gchar *w_name );
static ofaBalancesGrid *get_new_balances_grid( ofaCheckBalancesBin *bin, const gchar *w_name );
static void             set_bar_progression( myProgressBar *bar, gulong total, gulong current );
static void             set_checks_result( ofaCheckBalancesBin *bin );
static gboolean         cmp_lists( ofaCheckBalancesBin *bin, GList *list_a, GList *list_b );

static void
check_balances_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_check_balances_bin_finalize";
	ofaCheckBalancesBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CHECK_BALANCES_BIN( instance ));

	/* free data members here */
	priv = OFA_CHECK_BALANCES_BIN( instance )->priv;

	ofs_currency_list_free( &priv->entries_list );
	ofs_currency_list_free( &priv->ledgers_list );
	ofs_currency_list_free( &priv->accounts_list );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_check_balances_bin_parent_class )->finalize( instance );
}

static void
check_balances_bin_dispose( GObject *instance )
{
	ofaCheckBalancesBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_CHECK_BALANCES_BIN( instance ));

	priv = OFA_CHECK_BALANCES_BIN( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_check_balances_bin_parent_class )->dispose( instance );
}

static void
ofa_check_balances_bin_init( ofaCheckBalancesBin *self )
{
	static const gchar *thisfn = "ofa_check_balances_bin_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_CHECK_BALANCES_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_CHECK_BALANCES_BIN, ofaCheckBalancesBinPrivate );
}

static void
ofa_check_balances_bin_class_init( ofaCheckBalancesBinClass *klass )
{
	static const gchar *thisfn = "ofa_check_balances_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = check_balances_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = check_balances_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaCheckBalancesBinPrivate ));

	/**
	 * ofaCheckBalancesBin::done:
	 *
	 * This signal is sent when the controls are finished.
	 *
	 * Arguments is whether they are OK or not.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaCheckBalancesBin *bin,
	 * 						gboolean           ok,
	 * 						gpointer           user_data );
	 */
	st_signals[ DONE ] = g_signal_new_class_handler(
				"ofa-done",
				OFA_TYPE_CHECK_BALANCES_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_BOOLEAN );
}

/**
 * ofa_check_balances_bin_new:
 */
ofaCheckBalancesBin *
ofa_check_balances_bin_new( void )
{
	ofaCheckBalancesBin *self;

	self = g_object_new( OFA_TYPE_CHECK_BALANCES_BIN, NULL );

	load_dialog( self );

	return( self );
}

static void
load_dialog( ofaCheckBalancesBin *bin )
{
	GtkWidget *window;
	GtkWidget *top_widget;

	window = my_utils_builder_load_from_path( st_ui_xml, st_ui_id );
	g_return_if_fail( window && GTK_IS_WINDOW( window ));

	top_widget = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "check-top" );
	g_return_if_fail( top_widget && GTK_IS_CONTAINER( top_widget ));

	gtk_widget_reparent( top_widget, GTK_WIDGET( bin ));
}

/**
 * ofa_check_balances_bin_set_dossier:
 */
void
ofa_check_balances_bin_set_dossier( ofaCheckBalancesBin *bin, ofoDossier *dossier )
{
	ofaCheckBalancesBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_CHECK_BALANCES_BIN( bin ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		priv->dossier = dossier;

		g_idle_add(( GSourceFunc ) do_run, bin );
	}
}

static gboolean
do_run( ofaCheckBalancesBin *bin )
{
	ofaCheckBalancesBinPrivate *priv;

	priv = bin->priv;

	check_entries_balance_run( bin );
	check_ledgers_balance_run( bin );
	check_accounts_balance_run( bin );

	set_checks_result( bin );

	g_signal_emit_by_name( bin, "ofa-done", priv->result );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * Check that the entries of the current exercice are well balanced.
 * If beginning or ending dates of the exercice are not set, then
 * all found entries are checked.
 *
 * All entries (validated or rough, but not deleted) between the
 * beginning and ending dates are considered.
 */
static void
check_entries_balance_run( ofaCheckBalancesBin *bin )
{
	ofaCheckBalancesBinPrivate *priv;
	myProgressBar *bar;
	ofaBalancesGrid *grid;
	GList *entries, *it;
	const GDate *dbegin, *dend;
	gulong count, i;
	ofoEntry *entry;
	const gchar *currency;
	ofsCurrency *sbal;

	bar = get_new_bar( bin, "p4-entry-parent" );
	grid = get_new_balances_grid( bin, "p4-entry-bals" );
	gtk_widget_show_all( GTK_WIDGET( bin ) );

	priv = bin->priv;
	priv->entries_list = NULL;
	dbegin = ofo_dossier_get_exe_begin( priv->dossier );
	dend = ofo_dossier_get_exe_end( priv->dossier );
	entries = ofo_entry_get_dataset_for_print_general_books( priv->dossier, NULL, NULL, dbegin, dend );
	count = g_list_length( entries );

	for( i=1, it=entries ; it && count ; ++i, it=it->next ){
		/* just slow a bit, else it is too fast */
		/*g_usleep( 0.005*G_USEC_PER_SEC );*/

		entry = OFO_ENTRY( it->data );
		currency = ofo_entry_get_currency( entry );
		sbal = get_balance_for_currency( bin, &priv->entries_list, currency );

		sbal->ldebit += ofs_currency_amount_to_long( sbal, ofo_entry_get_debit( entry ));
		sbal->lcredit += ofs_currency_amount_to_long( sbal, ofo_entry_get_credit( entry ));

		ofs_currency_update_amounts( sbal );
		g_signal_emit_by_name( grid, "ofa-update", currency, sbal->debit, sbal->credit );
		set_bar_progression( bar, count, i );
	}

	ofo_entry_free_dataset( entries );

	priv->entries_ok = check_balances_per_currency( bin, priv->entries_list );
	set_balance_status( bin, priv->entries_ok, "p4-entry-ok" );
}

/*
 * Check that the ledgers of the current exercice are well balanced.
 * If beginning or ending dates of the exercice are not set, then
 * all found ledgers are checked.
 *
 * All entries (validated or rough, but not deleted) between the
 * beginning and ending dates are considered.
 */
static void
check_ledgers_balance_run( ofaCheckBalancesBin *bin )
{
	ofaCheckBalancesBinPrivate *priv;
	myProgressBar *bar;
	ofaBalancesGrid *grid;
	GList *ledgers, *it;
	GList *currencies, *ic;
	gulong count, i;
	ofoLedger *ledger;
	ofsCurrency *sbal;
	const gchar *currency;

	bar = get_new_bar( bin, "p4-ledger-parent" );
	grid = get_new_balances_grid( bin, "p4-ledger-bals" );
	gtk_widget_show_all( GTK_WIDGET( bin ));

	priv = bin->priv;
	priv->ledgers_list = NULL;
	ledgers = ofo_ledger_get_dataset( priv->dossier );
	count = g_list_length( ledgers );

	for( i=1, it=ledgers ; it && count ; ++i, it=it->next ){
		/*g_usleep( 0.005*G_USEC_PER_SEC );*/

		ledger = OFO_LEDGER( it->data );
		currencies = ofo_ledger_get_currencies( ledger );

		for( ic=currencies ; ic ; ic=ic->next ){
			currency = ( const gchar * ) ic->data;
			sbal = get_balance_for_currency( bin, &priv->ledgers_list, currency );

			sbal->ldebit +=
					ofs_currency_amount_to_long( sbal, ofo_ledger_get_val_debit( ledger, currency ))
					+ ofs_currency_amount_to_long( sbal, ofo_ledger_get_rough_debit( ledger, currency ));
			sbal->lcredit +=
					ofs_currency_amount_to_long( sbal, ofo_ledger_get_val_credit( ledger, currency ))
					+ ofs_currency_amount_to_long( sbal, ofo_ledger_get_rough_credit( ledger, currency ));

			ofs_currency_update_amounts( sbal );
			g_signal_emit_by_name( grid, "ofa-update", currency, sbal->debit, sbal->credit );
		}
		g_list_free( currencies );

		set_bar_progression( bar, count, i );
	}

	priv->ledgers_ok = check_balances_per_currency( bin, priv->ledgers_list );
	set_balance_status( bin, priv->ledgers_ok, "p4-ledger-ok" );
}

/*
 * 3/ check that accounts are balanced per currency
 */
static void
check_accounts_balance_run( ofaCheckBalancesBin *bin )
{
	ofaCheckBalancesBinPrivate *priv;
	myProgressBar *bar;
	ofaBalancesGrid *grid;
	GList *accounts, *it;
	gulong count, i;
	ofoAccount *account;
	const gchar *currency;
	ofsCurrency *sbal;

	bar = get_new_bar( bin, "p4-account-parent" );
	grid = get_new_balances_grid( bin, "p4-account-bals" );
	gtk_widget_show_all( GTK_WIDGET( bin ));

	priv = bin->priv;
	priv->accounts_list = NULL;
	accounts = ofo_account_get_dataset( priv->dossier );
	count = g_list_length( accounts );

	for( i=1, it=accounts ; it && count ; ++i, it=it->next ){
		/*g_usleep( 0.005*G_USEC_PER_SEC );*/

		account = OFO_ACCOUNT( it->data );
		if( !ofo_account_is_root( account )){
			currency = ofo_account_get_currency( account );
			sbal = get_balance_for_currency( bin, &priv->accounts_list, currency );

			sbal->ldebit +=
					ofs_currency_amount_to_long( sbal, ofo_account_get_val_debit( account ))
					+ ofs_currency_amount_to_long( sbal, ofo_account_get_rough_debit( account ));
			sbal->lcredit +=
					ofs_currency_amount_to_long( sbal, ofo_account_get_val_credit( account ))
					+ ofs_currency_amount_to_long( sbal, ofo_account_get_rough_credit( account ));

			ofs_currency_update_amounts( sbal );
			g_signal_emit_by_name( grid, "ofa-update", currency, sbal->debit, sbal->credit );
		}

		set_bar_progression( bar, count, i );
	}

	priv->accounts_ok = check_balances_per_currency( bin, priv->accounts_list );
	set_balance_status( bin, priv->accounts_ok, "p4-account-ok" );
}

static ofsCurrency *
get_balance_for_currency( ofaCheckBalancesBin *bin, GList **list, const gchar *currency )
{
	ofaCheckBalancesBinPrivate *priv;
	GList *it;
	ofsCurrency *sbal;
	ofoCurrency *cur_object;
	gboolean found;

	priv = bin->priv;
	found = FALSE;

	for( it=*list ; it ; it=it->next ){
		sbal = ( ofsCurrency * ) it->data;
		if( !g_utf8_collate( sbal->currency, currency )){
			found = TRUE;
			break;
		}
	}

	if( !found ){
		sbal = g_new0( ofsCurrency, 1 );
		sbal->currency = g_strdup( currency );
		cur_object = ofo_currency_get_by_code( priv->dossier, currency );
		g_return_val_if_fail( cur_object && OFO_IS_CURRENCY( cur_object ), NULL );
		sbal->digits = ofo_currency_get_digits( cur_object );
		*list = g_list_prepend( *list, sbal );
	}

	return( sbal );
}

static gboolean
check_balances_per_currency( ofaCheckBalancesBin *bin, GList *balances )
{
	gboolean ok;
	GList *it;
	ofsCurrency *sbal;

	ok = TRUE;

	for( it=balances ; it ; it=it->next ){
		sbal = ( ofsCurrency * ) it->data;
		g_debug( "check_balances_per_currency: debit=%lf, credit=%lf", sbal->debit, sbal->credit );
		ok &= ( sbal->debit == sbal->credit );
	}

	return( ok );
}

/*
 * display OK/NOT OK for a single balance check
 */
static void
set_balance_status( ofaCheckBalancesBin *bin, gboolean ok, const gchar *w_name )
{
	GtkWidget *label;
	GdkRGBA color;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), w_name );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	gdk_rgba_parse( &color, ok ? "#000000" : "#ff0000" );

	if( ok ){
		gtk_label_set_text( GTK_LABEL( label ), _( "OK" ));

	} else {
		gtk_label_set_text( GTK_LABEL( label ), _( "NOT OK" ));
	}

	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
}

static myProgressBar *
get_new_bar( ofaCheckBalancesBin *bin, const gchar *w_name )
{
	GtkWidget *parent;
	myProgressBar *bar;

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), w_name );
	g_return_val_if_fail( parent && GTK_IS_CONTAINER( parent ), FALSE );

	bar = my_progress_bar_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( bar ));

	return( bar );
}

static ofaBalancesGrid *
get_new_balances_grid( ofaCheckBalancesBin *bin, const gchar *w_name )
{
	GtkWidget *parent;
	ofaBalancesGrid *grid;

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), w_name );
	g_return_val_if_fail( parent && GTK_IS_CONTAINER( parent ), FALSE );

	grid = ofa_balances_grid_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( grid ));

	return( grid );
}

static void
set_bar_progression( myProgressBar *bar, gulong total, gulong current )
{
	gdouble progress;
	gchar *text;

	progress = ( gdouble ) current / ( gdouble ) total;
	g_signal_emit_by_name( bar, "ofa-double", progress );

	text = g_strdup_printf( "%lu/%lu", current, total );
	g_signal_emit_by_name( bar, "ofa-text", text );
	g_free( text );
}

/*
 * after the end of individual checks (entries, ledgers, accounts)
 * check that the balances are the sames
 */
static void
set_checks_result( ofaCheckBalancesBin *bin )
{
	ofaCheckBalancesBinPrivate *priv;
	GtkWidget *label;

	priv = bin->priv;

	priv->result = priv->entries_ok && priv->ledgers_ok && priv->accounts_ok;

	if( !priv->result ){
		my_utils_dialog_error(
				_( "We have detected losses of balance in your books.\n\n"
					"In this current state, we are unable to close this exercice\n"
					"until you fix your balances." ));

	} else {
		priv->result &= cmp_lists( bin, priv->entries_list, priv->ledgers_list );
		priv->result &= cmp_lists( bin, priv->entries_list, priv->accounts_list );

		label = my_utils_container_get_child_by_name(
						GTK_CONTAINER( bin ), "p4-label-end" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));

		if( priv->result ){
			gtk_label_set_text( GTK_LABEL( label ),
					_( "Your books are rightly balanced. Good !" ));

		} else {
			gtk_label_set_text( GTK_LABEL( label ),
					_( "\nThough each book is individually balanced, it appears "
						"that some distorsion has happened among them.\n"
						"In this current state, we are unable to close this exercice "
						"until you fix your balances." ));
		}
	}
}

static gboolean
cmp_lists( ofaCheckBalancesBin *bin, GList *list_a, GList *list_b )
{
	GList *it;
	ofsCurrency *sbal_a, *sbal_b;

	/* check that all 'a' records are found and same in list_b */
	for( it=list_a ; it ; it=it->next ){
		sbal_a = ( ofsCurrency * ) it->data;
		sbal_b = get_balance_for_currency( bin, &list_b, sbal_a->currency );
		if( sbal_a->debit != sbal_b->debit || sbal_a->credit != sbal_b->credit ){
			return( FALSE );
		}
	}

	/* check that all 'b' records are found and same in list_a */
	for( it=list_b ; it ; it=it->next ){
		sbal_b = ( ofsCurrency * ) it->data;
		sbal_a = get_balance_for_currency( bin, &list_a, sbal_b->currency );
		if( sbal_b->debit != sbal_a->debit || sbal_b->credit != sbal_a->credit ){
			return( FALSE );
		}
	}

	return( TRUE );
}

/**
 * ofa_check_balances_bin_get_status:
 */
gboolean
ofa_check_balances_bin_get_status( const ofaCheckBalancesBin *bin )
{
	ofaCheckBalancesBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_CHECK_BALANCES_BIN( bin ), FALSE );

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		return( priv->result );
	}

	return( FALSE );
}
