/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include "my/my-progress-bar.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofs-currency.h"

#include "ui/ofa-balance-grid-bin.h"
#include "ui/ofa-check-balances-bin.h"

/* private instance data
 */
struct _ofaCheckBalancesBinPrivate {
	gboolean    dispose_has_run;

	/* runtime data
	 */
	ofaHub     *hub;
	gboolean    display;

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

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-check-balances-bin.ui";

static void               setup_bin( ofaCheckBalancesBin *self );
static gboolean           do_run( ofaCheckBalancesBin *self );
static void               check_entries_balance_run( ofaCheckBalancesBin *self );
static void               check_ledgers_balance_run( ofaCheckBalancesBin *self );
static void               check_accounts_balance_run( ofaCheckBalancesBin *self );
static gboolean           check_balances_per_currency( ofaCheckBalancesBin *self, GList *balances );
static void               set_balance_status( ofaCheckBalancesBin *self, gboolean ok, const gchar *w_name );
static myProgressBar     *get_new_bar( ofaCheckBalancesBin *self, const gchar *w_name );
static ofaBalanceGridBin *get_new_balance_grid_bin( ofaCheckBalancesBin *self, const gchar *w_name );
static void               set_bar_progression( ofaCheckBalancesBin *self, myProgressBar *bar, gulong total, gulong current );
static void               set_checks_result( ofaCheckBalancesBin *self );
static gboolean           cmp_lists( ofaCheckBalancesBin *self, GList *list_a, GList *list_b );

G_DEFINE_TYPE_EXTENDED( ofaCheckBalancesBin, ofa_check_balances_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaCheckBalancesBin ))

static void
check_balances_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_check_balances_bin_finalize";
	ofaCheckBalancesBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CHECK_BALANCES_BIN( instance ));

	/* free data members here */
	priv = ofa_check_balances_bin_get_instance_private( OFA_CHECK_BALANCES_BIN( instance ));

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

	priv = ofa_check_balances_bin_get_instance_private( OFA_CHECK_BALANCES_BIN( instance ));

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
	ofaCheckBalancesBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_CHECK_BALANCES_BIN( self ));

	priv = ofa_check_balances_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->display = TRUE;
}

static void
ofa_check_balances_bin_class_init( ofaCheckBalancesBinClass *klass )
{
	static const gchar *thisfn = "ofa_check_balances_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = check_balances_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = check_balances_bin_finalize;

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
	ofaCheckBalancesBin *bin;

	bin = g_object_new( OFA_TYPE_CHECK_BALANCES_BIN, NULL );

	setup_bin( bin );

	return( bin );
}

static void
setup_bin( ofaCheckBalancesBin *self )
{
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel;

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "cbb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

/**
 * ofa_check_balances_bin_set_display:
 * @bin: this #ofaCheckBalancesBin instance.
 * @display: whether the check should be displayed.
 */
void
ofa_check_balances_bin_set_display( ofaCheckBalancesBin *bin, gboolean display )
{
	ofaCheckBalancesBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_CHECK_BALANCES_BIN( bin ));

	priv = ofa_check_balances_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	priv->display = display;
}

/**
 * ofa_check_balances_bin_set_hub:
 */
void
ofa_check_balances_bin_set_hub( ofaCheckBalancesBin *bin, ofaHub *hub )
{
	ofaCheckBalancesBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_CHECK_BALANCES_BIN( bin ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_check_balances_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	priv->hub = hub;

	if( priv->display ){
		g_idle_add(( GSourceFunc ) do_run, bin );
	} else {
		do_run( bin );
	}
}

static gboolean
do_run( ofaCheckBalancesBin *self )
{
	ofaCheckBalancesBinPrivate *priv;

	priv = ofa_check_balances_bin_get_instance_private( self );

	check_entries_balance_run( self );
	check_ledgers_balance_run( self );
	check_accounts_balance_run( self );

	set_checks_result( self );

	g_signal_emit_by_name( self, "ofa-done", priv->result );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * Check that the entries of the current exercice are well balanced.
 * If beginning or ending dates of the exercice are not set, then
 * all found entries are checked.
 *
 * All entries (validated, rough and future, but not deleted) starting
 * with the beginning date of the exercice are considered.
 */
static void
check_entries_balance_run( ofaCheckBalancesBin *self )
{
	ofaCheckBalancesBinPrivate *priv;
	ofoDossier *dossier;
	myProgressBar *bar;
	ofaBalanceGridBin *grid;
	GList *entries, *it;
	const GDate *dbegin;
	gulong count, i;
	ofoEntry *entry;
	const gchar *currency;
	ofsCurrency *sbal;

	priv = ofa_check_balances_bin_get_instance_private( self );

	bar = get_new_bar( self, "p4-entry-parent" );
	grid = get_new_balance_grid_bin( self, "p4-entry-bals" );

	if( priv->display ){
		gtk_widget_show_all( GTK_WIDGET( self ) );
	}

	dossier = ofa_hub_get_dossier( priv->hub );

	priv->entries_list = NULL;
	dbegin = ofo_dossier_get_exe_begin( dossier );
	entries = ofo_entry_get_dataset_for_print_general_books( priv->hub, NULL, NULL, dbegin, NULL );
	count = g_list_length( entries );

	for( i=1, it=entries ; it && count ; ++i, it=it->next ){
		/* just slow a bit, else it is too fast */
		/*g_usleep( 0.005*G_USEC_PER_SEC );*/

		entry = OFO_ENTRY( it->data );
		currency = ofo_entry_get_currency( entry );

		sbal = ofs_currency_add_by_code(
					&priv->entries_list, priv->hub, currency,
					ofo_entry_get_debit( entry ), ofo_entry_get_credit( entry ));

		if( priv->display ){
			g_signal_emit_by_name( grid, "ofa-update", currency, sbal->debit, sbal->credit );
		}
		set_bar_progression( self, bar, count, i );
	}

	ofo_entry_free_dataset( entries );

	priv->entries_ok = check_balances_per_currency( self, priv->entries_list );
	set_balance_status( self, priv->entries_ok, "p4-entry-ok" );
}

/*
 * Check that the ledgers of the current exercice are well balanced.
 * If beginning or ending dates of the exercice are not set, then
 * all found ledgers are checked.
 *
 * All entries (validated, rough and future, but not deleted) starting
 * with the beginning date of the exercice are considered.
 */
static void
check_ledgers_balance_run( ofaCheckBalancesBin *self )
{
	ofaCheckBalancesBinPrivate *priv;
	myProgressBar *bar;
	ofaBalanceGridBin *grid;
	GList *ledgers, *it;
	GList *currencies, *ic;
	gulong count, i;
	ofoLedger *ledger;
	ofsCurrency *sbal;
	const gchar *currency;

	priv = ofa_check_balances_bin_get_instance_private( self );

	bar = get_new_bar( self, "p4-ledger-parent" );
	grid = get_new_balance_grid_bin( self, "p4-ledger-bals" );

	if( priv->display ){
		gtk_widget_show_all( GTK_WIDGET( self ));
	}

	priv->ledgers_list = NULL;
	ledgers = ofo_ledger_get_dataset( priv->hub );
	count = g_list_length( ledgers );

	for( i=1, it=ledgers ; it && count ; ++i, it=it->next ){
		/*g_usleep( 0.005*G_USEC_PER_SEC );*/

		ledger = OFO_LEDGER( it->data );
		currencies = ofo_ledger_get_currencies( ledger );

		for( ic=currencies ; ic ; ic=ic->next ){
			currency = ( const gchar * ) ic->data;

			sbal = ofs_currency_add_by_code(
						&priv->ledgers_list, priv->hub, currency,
						ofo_ledger_get_val_debit( ledger, currency )
							+ ofo_ledger_get_rough_debit( ledger, currency )
							+ ofo_ledger_get_futur_debit( ledger, currency ),
						ofo_ledger_get_val_credit( ledger, currency )
							+ ofo_ledger_get_rough_credit( ledger, currency )
							+ ofo_ledger_get_futur_credit( ledger, currency ));

			if( priv->display ){
				g_signal_emit_by_name( grid, "ofa-update", currency, sbal->debit, sbal->credit );
			}
		}

		g_list_free( currencies );
		set_bar_progression( self, bar, count, i );
	}

	priv->ledgers_ok = check_balances_per_currency( self, priv->ledgers_list );
	set_balance_status( self, priv->ledgers_ok, "p4-ledger-ok" );
}

/*
 * 3/ check that accounts are balanced per currency
 *
 * Validated, rough and future balances are considered.
 */
static void
check_accounts_balance_run( ofaCheckBalancesBin *self )
{
	ofaCheckBalancesBinPrivate *priv;
	myProgressBar *bar;
	ofaBalanceGridBin *grid;
	GList *accounts, *it;
	gulong count, i;
	ofoAccount *account;
	const gchar *currency;
	ofsCurrency *sbal;

	priv = ofa_check_balances_bin_get_instance_private( self );

	bar = get_new_bar( self, "p4-account-parent" );
	grid = get_new_balance_grid_bin( self, "p4-account-bals" );

	if( priv->display ){
		gtk_widget_show_all( GTK_WIDGET( self ));
	}

	priv->accounts_list = NULL;
	accounts = ofo_account_get_dataset( priv->hub );
	count = g_list_length( accounts );

	for( i=1, it=accounts ; it && count ; ++i, it=it->next ){
		/*g_usleep( 0.005*G_USEC_PER_SEC );*/

		account = OFO_ACCOUNT( it->data );
		if( !ofo_account_is_root( account )){
			currency = ofo_account_get_currency( account );

			sbal = ofs_currency_add_by_code(
						&priv->accounts_list, priv->hub, currency,
						ofo_account_get_val_debit( account )
							+ ofo_account_get_rough_debit( account )
							+ ofo_account_get_futur_debit( account ),
						ofo_account_get_val_credit( account )
							+ ofo_account_get_rough_credit( account )
							+ ofo_account_get_futur_credit( account ));

			if( priv->display ){
				g_signal_emit_by_name( grid, "ofa-update", currency, sbal->debit, sbal->credit );
			}
		}

		set_bar_progression( self, bar, count, i );
	}

	priv->accounts_ok = check_balances_per_currency( self, priv->accounts_list );
	set_balance_status( self, priv->accounts_ok, "p4-account-ok" );
}

static gboolean
check_balances_per_currency( ofaCheckBalancesBin *self, GList *balances )
{
	static const gchar *thisfn = "ofa_check_balances_bin_check_balances_per_currency";
	gboolean ok;
	GList *it;
	ofsCurrency *sbal;

	ok = TRUE;

	for( it=balances ; it ; it=it->next ){
		sbal = ( ofsCurrency * ) it->data;
		g_debug( "%s: currency=%s, debit=%lf, credit=%lf",
				thisfn, ofo_currency_get_code( sbal->currency ), sbal->debit, sbal->credit );
		ok &= ofs_currency_is_balanced( sbal );
	}

	return( ok );
}

/*
 * display OK/NOT OK for a single balance check
 */
static void
set_balance_status( ofaCheckBalancesBin *self, gboolean ok, const gchar *w_name )
{
	ofaCheckBalancesBinPrivate *priv;
	GtkWidget *label;

	priv = ofa_check_balances_bin_get_instance_private( self );

	if( priv->display ){
		label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), w_name );
		g_return_if_fail( label && GTK_IS_LABEL( label ));

		gtk_label_set_text( GTK_LABEL( label ), ok ? _( "OK" ) : _( "NOT OK" ));
		my_utils_widget_set_style( label, ok ? "labelinfo" : "labelerror" );
	}
}

static myProgressBar *
get_new_bar( ofaCheckBalancesBin *self, const gchar *w_name )
{
	ofaCheckBalancesBinPrivate *priv;
	GtkWidget *parent;
	myProgressBar *bar;

	priv = ofa_check_balances_bin_get_instance_private( self );

	bar = NULL;

	if( priv->display ){
		parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), w_name );
		g_return_val_if_fail( parent && GTK_IS_CONTAINER( parent ), FALSE );

		bar = my_progress_bar_new();
		gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( bar ));
	}

	return( bar );
}

static ofaBalanceGridBin *
get_new_balance_grid_bin( ofaCheckBalancesBin *self, const gchar *w_name )
{
	ofaCheckBalancesBinPrivate *priv;
	GtkWidget *parent;
	ofaBalanceGridBin *grid;

	priv = ofa_check_balances_bin_get_instance_private( self );

	grid = NULL;

	if( priv->display ){
		parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), w_name );
		g_return_val_if_fail( parent && GTK_IS_CONTAINER( parent ), FALSE );

		grid = ofa_balance_grid_bin_new();
		gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( grid ));
	}

	return( grid );
}

static void
set_bar_progression( ofaCheckBalancesBin *self, myProgressBar *bar, gulong total, gulong current )
{
	ofaCheckBalancesBinPrivate *priv;
	gdouble progress;
	gchar *text;

	priv = ofa_check_balances_bin_get_instance_private( self );

	if( priv->display ){
		progress = ( gdouble ) current / ( gdouble ) total;
		g_signal_emit_by_name( bar, "my-double", progress );

		text = g_strdup_printf( "%lu/%lu", current, total );
		g_signal_emit_by_name( bar, "my-text", text );
		g_free( text );
	}
}

/*
 * after the end of individual checks (entries, ledgers, accounts)
 * check that the balances are the sames
 */
static void
set_checks_result( ofaCheckBalancesBin *self )
{
	ofaCheckBalancesBinPrivate *priv;
	GtkWidget *label;

	priv = ofa_check_balances_bin_get_instance_private( self );

	priv->result = priv->entries_ok && priv->ledgers_ok && priv->accounts_ok;

	if( !priv->result ){
		if( priv->display ){
			my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING,
					_( "We have detected losses of balance in your books.\n\n"
						"In this current state, we will be unable to close this "
						"exercice until you fix your balances." ));
		}
	} else {
		priv->result &= cmp_lists( self, priv->entries_list, priv->ledgers_list );
		priv->result &= cmp_lists( self, priv->entries_list, priv->accounts_list );
	}

	if( priv->display ){
		label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-label-end" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));

		if( priv->result ){
			gtk_label_set_text( GTK_LABEL( label ),
					_( "Your books are rightly balanced. Good !" ));
			my_utils_widget_set_style( label, "labelinfo" );

		} else {
			gtk_label_set_text( GTK_LABEL( label ),
					_( "Though each book is individually balanced, it appears "
						"that some distorsion has happened among them.\n"
						"In this current state, we will be unable to close this "
						"exercice until you fix your balances." ));
			my_utils_widget_set_style( label, "labelerror" );
		}
	}
}

static gboolean
cmp_lists( ofaCheckBalancesBin *self, GList *list_a, GList *list_b )
{
	GList *it;
	ofsCurrency *sbal_a, *sbal_b;

	/* check that all 'a' records are found and same in list_b */
	for( it=list_a ; it ; it=it->next ){
		sbal_a = ( ofsCurrency * ) it->data;
		sbal_b = ofs_currency_get_by_code( list_b, ofo_currency_get_code( sbal_a->currency ));
		if( ofs_currency_cmp( sbal_a, sbal_b ) != 0 ){
			return( FALSE );
		}
	}

	/* check that all 'b' records are found and same in list_a */
	for( it=list_b ; it ; it=it->next ){
		sbal_b = ( ofsCurrency * ) it->data;
		sbal_a = ofs_currency_get_by_code( list_a, ofo_currency_get_code( sbal_b->currency ));
		if( ofs_currency_cmp( sbal_a, sbal_b ) != 0 ){
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

	priv = ofa_check_balances_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->result );
}
