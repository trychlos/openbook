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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <math.h>

#include "my/my-progress-bar.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
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
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;

	/* runtime data
	 */
	gboolean    display;

	gboolean    entries_ok;
	GList      *entries_current_rough_list;
	GList      *entries_current_val_list;
	GList      *entries_futur_rough_list;
	GList      *entries_futur_val_list;
	GList      *entries_list;
	gboolean    ledgers_ok;
	GList      *ledgers_current_rough_list;
	GList      *ledgers_current_val_list;
	GList      *ledgers_futur_rough_list;
	GList      *ledgers_futur_val_list;
	GList      *ledgers_list;
	gboolean    accounts_ok;
	GList      *accounts_current_rough_list;
	GList      *accounts_current_val_list;
	GList      *accounts_futur_rough_list;
	GList      *accounts_futur_val_list;
	GList      *accounts_list;

	gboolean    result;
}
	ofaCheckBalancesBinPrivate;

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

	ofs_currency_list_free( &priv->entries_current_rough_list );
	ofs_currency_list_free( &priv->entries_current_val_list );
	ofs_currency_list_free( &priv->entries_futur_rough_list );
	ofs_currency_list_free( &priv->entries_futur_val_list );
	ofs_currency_list_free( &priv->entries_list );
	ofs_currency_list_free( &priv->ledgers_current_rough_list );
	ofs_currency_list_free( &priv->ledgers_current_val_list );
	ofs_currency_list_free( &priv->ledgers_futur_rough_list );
	ofs_currency_list_free( &priv->ledgers_futur_val_list );
	ofs_currency_list_free( &priv->ledgers_list );
	ofs_currency_list_free( &priv->accounts_current_rough_list );
	ofs_currency_list_free( &priv->accounts_current_val_list );
	ofs_currency_list_free( &priv->accounts_futur_rough_list );
	ofs_currency_list_free( &priv->accounts_futur_val_list );
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
 * ofa_check_balances_bin_set_getter:
 */
void
ofa_check_balances_bin_set_getter( ofaCheckBalancesBin *bin, ofaIGetter *getter )
{
	ofaCheckBalancesBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_CHECK_BALANCES_BIN( bin ));
	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	priv = ofa_check_balances_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	priv->getter = getter;

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
	guint count, i;
	ofoEntry *entry;
	const gchar *currency;
	ofsCurrency *sbal;
	ofaHub *hub;
	ofeEntryStatus status;
	ofeEntryPeriod period;
	ofxAmount debit, credit;

	priv = ofa_check_balances_bin_get_instance_private( self );

	priv->entries_current_rough_list = NULL;
	priv->entries_current_val_list = NULL;
	priv->entries_futur_rough_list = NULL;
	priv->entries_futur_val_list = NULL;
	priv->entries_list = NULL;

	bar = get_new_bar( self, "p4-entry-parent" );
	grid = get_new_balance_grid_bin( self, "p4-entry-bals" );

	if( priv->display ){
		gtk_widget_show_all( GTK_WIDGET( self ) );
	}

	hub = ofa_igetter_get_hub( priv->getter );
	dossier = ofa_hub_get_dossier( hub );
	dbegin = ofo_dossier_get_exe_begin( dossier );

	entries = ofo_entry_get_dataset( priv->getter );
	count = g_list_length( entries );

	if( count == 0 ){
		set_bar_progression( self, bar, 0, 0 );

	} else {
		for( i=1, it=entries ; it && count ; ++i, it=it->next ){
			entry = OFO_ENTRY( it->data );

			if( my_date_compare_ex( ofo_entry_get_deffect( entry ), dbegin, TRUE ) >= 0 ){
				period = ofo_entry_get_period( entry );
				status = ofo_entry_get_status( entry );
				currency = ofo_entry_get_currency( entry );
				debit = ofo_entry_get_debit( entry );
				credit = ofo_entry_get_credit( entry );

				switch( period ){
					case ENT_PERIOD_CURRENT:
						switch( status ){
							case ENT_STATUS_ROUGH:
								sbal = ofs_currency_add_by_code(
											&priv->entries_current_rough_list,
											priv->getter, currency, debit, credit );
								if( priv->display ){
									ofa_balance_grid_bin_set_currency( grid,
											BALANCEGRID_CURRENT_ROUGH, sbal );
								}
								break;
							case ENT_STATUS_VALIDATED:
								sbal = ofs_currency_add_by_code(
											&priv->entries_current_val_list,
											priv->getter, currency, debit, credit );
								if( priv->display ){
									ofa_balance_grid_bin_set_currency( grid,
											BALANCEGRID_CURRENT_VALIDATED, sbal );
								}
								break;
							default:
								break;
						}
						break;
					case ENT_PERIOD_FUTURE:
						switch( status ){
							case ENT_STATUS_ROUGH:
								sbal = ofs_currency_add_by_code(
											&priv->entries_futur_rough_list,
											priv->getter, currency, debit, credit );
								if( priv->display ){
									ofa_balance_grid_bin_set_currency( grid,
											BALANCEGRID_FUTUR_ROUGH, sbal );
								}
								break;
							case ENT_STATUS_VALIDATED:
								sbal = ofs_currency_add_by_code(
											&priv->entries_futur_val_list,
											priv->getter, currency, debit, credit );
								if( priv->display ){
									ofa_balance_grid_bin_set_currency( grid,
											BALANCEGRID_FUTUR_VALIDATED, sbal );
								}
								break;
							default:
								break;
						}
						break;
					default:
						break;
				}

				if( status != ENT_STATUS_DELETED ){
					sbal = ofs_currency_add_by_code(
								&priv->entries_list, priv->getter, currency, debit, credit );
					if( priv->display ){
						ofa_balance_grid_bin_set_currency( grid, BALANCEGRID_TOTAL, sbal );
					}
				}
			}

			set_bar_progression( self, bar, count, i );
		}
	}

	priv->entries_ok =
			check_balances_per_currency( self, priv->entries_current_rough_list ) &&
			check_balances_per_currency( self, priv->entries_current_val_list ) &&
			check_balances_per_currency( self, priv->entries_futur_rough_list ) &&
			check_balances_per_currency( self, priv->entries_futur_val_list ) &&
			check_balances_per_currency( self, priv->entries_list );

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
	guint count, i;
	ofoLedger *ledger;
	ofsCurrency *sbal;
	const gchar *currency;
	ofxAmount tot_debit, tot_credit, debit, credit;

	priv = ofa_check_balances_bin_get_instance_private( self );

	priv->ledgers_current_rough_list = NULL;
	priv->ledgers_current_val_list = NULL;
	priv->ledgers_futur_rough_list = NULL;
	priv->ledgers_futur_val_list = NULL;
	priv->ledgers_list = NULL;

	bar = get_new_bar( self, "p4-ledger-parent" );
	grid = get_new_balance_grid_bin( self, "p4-ledger-bals" );

	if( priv->display ){
		gtk_widget_show_all( GTK_WIDGET( self ));
	}

	ledgers = ofo_ledger_get_dataset( priv->getter );
	count = g_list_length( ledgers );

	if( count == 0 ){
		set_bar_progression( self, bar, 0, 0 );

	} else {
		for( i=1, it=ledgers ; it && count ; ++i, it=it->next ){
			ledger = OFO_LEDGER( it->data );
			currencies = ofo_ledger_currency_get_list( ledger );

			for( ic=currencies ; ic ; ic=ic->next ){
				currency = ( const gchar * ) ic->data;
				tot_debit = 0;
				tot_credit = 0;

				debit = ofo_ledger_get_current_rough_debit( ledger, currency );
				credit = ofo_ledger_get_current_rough_credit( ledger, currency );
				sbal = ofs_currency_add_by_code(
							&priv->ledgers_current_rough_list, priv->getter, currency, debit, credit );
				if( priv->display ){
					ofa_balance_grid_bin_set_currency( grid, BALANCEGRID_CURRENT_ROUGH, sbal );
				}
				tot_debit += debit;
				tot_credit += credit;

				debit = ofo_ledger_get_current_val_debit( ledger, currency );
				credit = ofo_ledger_get_current_val_credit( ledger, currency );
				sbal = ofs_currency_add_by_code(
							&priv->ledgers_current_val_list, priv->getter, currency, debit, credit );
				if( priv->display ){
					ofa_balance_grid_bin_set_currency( grid, BALANCEGRID_CURRENT_VALIDATED, sbal );
				}
				tot_debit += debit;
				tot_credit += credit;

				debit = ofo_ledger_get_futur_rough_debit( ledger, currency );
				credit = ofo_ledger_get_futur_rough_credit( ledger, currency );
				sbal = ofs_currency_add_by_code(
							&priv->ledgers_futur_rough_list, priv->getter, currency, debit, credit );
				if( priv->display ){
					ofa_balance_grid_bin_set_currency( grid, BALANCEGRID_FUTUR_ROUGH, sbal );
				}
				tot_debit += debit;
				tot_credit += credit;

				debit = ofo_ledger_get_futur_val_debit( ledger, currency );
				credit = ofo_ledger_get_futur_val_credit( ledger, currency );
				sbal = ofs_currency_add_by_code(
							&priv->ledgers_futur_val_list, priv->getter, currency, debit, credit );
				if( priv->display ){
					ofa_balance_grid_bin_set_currency( grid, BALANCEGRID_FUTUR_VALIDATED, sbal );
				}
				tot_debit += debit;
				tot_credit += credit;

				sbal = ofs_currency_add_by_code(
							&priv->ledgers_list, priv->getter, currency, tot_debit, tot_credit );
				if( priv->display ){
					ofa_balance_grid_bin_set_currency( grid, BALANCEGRID_TOTAL, sbal );
				}
			}

			g_list_free( currencies );
			set_bar_progression( self, bar, count, i );
		}
	}

	priv->ledgers_ok =
			check_balances_per_currency( self, priv->ledgers_current_rough_list ) &&
			check_balances_per_currency( self, priv->ledgers_current_val_list ) &&
			check_balances_per_currency( self, priv->ledgers_futur_rough_list ) &&
			check_balances_per_currency( self, priv->ledgers_futur_val_list ) &&
			check_balances_per_currency( self, priv->ledgers_list );

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
	guint count, i;
	ofoAccount *account;
	const gchar *currency;
	ofsCurrency *sbal;
	ofxAmount tot_debit, tot_credit, debit, credit;

	priv = ofa_check_balances_bin_get_instance_private( self );

	priv->accounts_current_rough_list = NULL;
	priv->accounts_current_val_list = NULL;
	priv->accounts_futur_rough_list = NULL;
	priv->accounts_futur_val_list = NULL;
	priv->accounts_list = NULL;

	bar = get_new_bar( self, "p4-account-parent" );
	grid = get_new_balance_grid_bin( self, "p4-account-bals" );

	if( priv->display ){
		gtk_widget_show_all( GTK_WIDGET( self ));
	}

	accounts = ofo_account_get_dataset( priv->getter );
	count = g_list_length( accounts );

	if( count == 0 ){
		set_bar_progression( self, bar, 0, 0 );

	} else {
		for( i=1, it=accounts ; it && count ; ++i, it=it->next ){
			account = OFO_ACCOUNT( it->data );
			tot_debit = 0;
			tot_credit = 0;

			if( !ofo_account_is_root( account )){
				currency = ofo_account_get_currency( account );

				debit = ofo_account_get_current_rough_debit( account );
				credit = ofo_account_get_current_rough_credit( account );
				sbal = ofs_currency_add_by_code(
							&priv->accounts_current_rough_list, priv->getter, currency, debit, credit );
				if( priv->display ){
					ofa_balance_grid_bin_set_currency( grid, BALANCEGRID_CURRENT_ROUGH, sbal );
				}
				tot_debit += debit;
				tot_credit += credit;

				debit = ofo_account_get_current_val_debit( account );
				credit = ofo_account_get_current_val_credit( account );
				sbal = ofs_currency_add_by_code(
							&priv->accounts_current_val_list, priv->getter, currency, debit, credit );
				if( priv->display ){
					ofa_balance_grid_bin_set_currency( grid, BALANCEGRID_CURRENT_VALIDATED, sbal );
				}
				tot_debit += debit;
				tot_credit += credit;

				debit = ofo_account_get_futur_rough_debit( account );
				credit = ofo_account_get_futur_rough_credit( account );
				sbal = ofs_currency_add_by_code(
							&priv->accounts_futur_rough_list, priv->getter, currency, debit, credit );
				if( priv->display ){
					ofa_balance_grid_bin_set_currency( grid, BALANCEGRID_FUTUR_ROUGH, sbal );
				}
				tot_debit += debit;
				tot_credit += credit;

				debit = ofo_account_get_futur_val_debit( account );
				credit = ofo_account_get_futur_val_credit( account );
				sbal = ofs_currency_add_by_code(
							&priv->accounts_futur_val_list, priv->getter, currency, debit, credit );
				if( priv->display ){
					ofa_balance_grid_bin_set_currency( grid, BALANCEGRID_FUTUR_VALIDATED, sbal );
				}
				tot_debit += debit;
				tot_credit += credit;

				sbal = ofs_currency_add_by_code(
							&priv->accounts_list, priv->getter, currency, tot_debit, tot_credit );
				if( priv->display ){
					ofa_balance_grid_bin_set_currency( grid, BALANCEGRID_TOTAL, sbal );
				}
			}

			set_bar_progression( self, bar, count, i );
		}
	}

	priv->accounts_ok =
			check_balances_per_currency( self, priv->accounts_current_rough_list ) &&
			check_balances_per_currency( self, priv->accounts_current_val_list ) &&
			check_balances_per_currency( self, priv->accounts_futur_rough_list ) &&
			check_balances_per_currency( self, priv->accounts_futur_val_list ) &&
			check_balances_per_currency( self, priv->accounts_list );

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
		my_style_add( label, ok ? "labelinfo" : "labelerror" );
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

		grid = ofa_balance_grid_bin_new( priv->getter );
		gtk_widget_set_margin_start( GTK_WIDGET( grid ), 8 );
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
		if( total ){
			progress = ( gdouble ) current / ( gdouble ) total;
			g_signal_emit_by_name( bar, "my-double", progress );
		}
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
	gboolean cmpres;

	priv = ofa_check_balances_bin_get_instance_private( self );

	cmpres = TRUE;
	priv->result = priv->entries_ok && priv->ledgers_ok && priv->accounts_ok;

	if( priv->result &&
		( priv->entries_list || priv->ledgers_list || priv->accounts_list )){

		g_return_if_fail( priv->entries_list );
		g_return_if_fail( priv->ledgers_list );
		g_return_if_fail( priv->accounts_list );

		cmpres &= cmp_lists( self, priv->entries_list, priv->ledgers_list );
		cmpres &= cmp_lists( self, priv->entries_list, priv->accounts_list );
	}

	if( priv->display ){
		label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-label-end" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));

		if( !priv->entries_list && !priv->ledgers_list && !priv->accounts_list ){
			gtk_label_set_text( GTK_LABEL( label ),
					_( "Your books appear empty." ));
			my_style_add( label, "labelinfo" );

		} else if( priv->result && cmpres ){
			gtk_label_set_text( GTK_LABEL( label ),
					_( "Your books are rightly balanced. Good !" ));
			my_style_add( label, "labelinfo" );

		} else if( !priv->result ){
			gtk_label_set_text( GTK_LABEL( label ),
					_( "We have detected losses of balance in your books.\n"
						"In this current state, we will be unable to close this "
						"exercice until you fix your balances." ));
			my_style_add( label, "labelerror" );

		} else {
			gtk_label_set_text( GTK_LABEL( label ),
					_( "Though each book is individually balanced, it appears "
						"that some distorsion has happened among them.\n"
						"In this current state, we will be unable to close this "
						"exercice until you fix your balances." ));
			my_style_add( label, "labelerror" );
		}
	}

	priv->result &= cmpres;
}

static gboolean
cmp_lists( ofaCheckBalancesBin *self, GList *list_a, GList *list_b )
{
	GList *it;
	ofsCurrency *sbal_a, *sbal_b;

	if( 0 ){
		g_debug( "list_a" );
		ofs_currency_list_dump( list_a );
		g_debug( "list_b" );
		ofs_currency_list_dump( list_b );
	}

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
ofa_check_balances_bin_get_status( ofaCheckBalancesBin *bin )
{
	ofaCheckBalancesBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_CHECK_BALANCES_BIN( bin ), FALSE );

	priv = ofa_check_balances_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->result );
}
