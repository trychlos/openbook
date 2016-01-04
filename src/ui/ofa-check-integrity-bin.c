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

#include "api/my-progress-bar.h"
#include "api/my-utils.h"
#include "api/ofo-account.h"
#include "api/ofo-bat.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-class.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofs-currency.h"

#include "ui/ofa-check-integrity-bin.h"

/* private instance data
 */
struct _ofaCheckIntegrityBinPrivate {
	gboolean       dispose_has_run;

	/* runtime data
	 */
	ofoDossier    *dossier;

	gulong         dossier_errs;
	gulong         bat_lines_errs;
	gulong         accounts_errs;
	gulong         entries_errs;
	gulong         ledgers_errs;
	gulong         ope_templates_errs;

	gulong         total_errs;

	/* UI
	 */
	GtkTextBuffer *text_buffer;
};

/* signals defined here
 */
enum {
	DONE = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_bin_xml          = PKGUIDIR "/ofa-check-integrity-bin.ui";

G_DEFINE_TYPE( ofaCheckIntegrityBin, ofa_check_integrity_bin, GTK_TYPE_BIN )

static void           setup_bin( ofaCheckIntegrityBin *self );
static gboolean       do_run( ofaCheckIntegrityBin *bin );
static void           check_dossier_run( ofaCheckIntegrityBin *bin );
static void           check_bat_lines_run( ofaCheckIntegrityBin *bin );
static void           check_accounts_run( ofaCheckIntegrityBin *bin );
static void           check_entries_run( ofaCheckIntegrityBin *bin );
static void           check_ledgers_run( ofaCheckIntegrityBin *bin );
static void           check_ope_templates_run( ofaCheckIntegrityBin *bin );
static void           set_check_status( ofaCheckIntegrityBin *bin, gulong errs, const gchar *w_name );
static myProgressBar *get_new_bar( ofaCheckIntegrityBin *bin, const gchar *w_name );
static void           set_bar_progression( myProgressBar *bar, gulong total, gulong current );
static void           set_checks_result( ofaCheckIntegrityBin *bin );
static void           add_message( ofaCheckIntegrityBin *bin, const gchar *text );

static void
check_integrity_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_check_integrity_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CHECK_INTEGRITY_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_check_integrity_bin_parent_class )->finalize( instance );
}

static void
check_integrity_bin_dispose( GObject *instance )
{
	ofaCheckIntegrityBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_CHECK_INTEGRITY_BIN( instance ));

	priv = OFA_CHECK_INTEGRITY_BIN( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_check_integrity_bin_parent_class )->dispose( instance );
}

static void
ofa_check_integrity_bin_init( ofaCheckIntegrityBin *self )
{
	static const gchar *thisfn = "ofa_check_integrity_bin_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_CHECK_INTEGRITY_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_CHECK_INTEGRITY_BIN, ofaCheckIntegrityBinPrivate );
}

static void
ofa_check_integrity_bin_class_init( ofaCheckIntegrityBinClass *klass )
{
	static const gchar *thisfn = "ofa_check_integrity_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = check_integrity_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = check_integrity_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaCheckIntegrityBinPrivate ));

	/**
	 * ofaCheckIntegrityBin::done:
	 *
	 * This signal is sent when the controls are finished.
	 *
	 * Arguments is whether they are OK or not.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaCheckIntegrityBin *bin,
	 * 						gulong              total_errs,
	 * 						gpointer            user_data );
	 */
	st_signals[ DONE ] = g_signal_new_class_handler(
				"ofa-done",
				OFA_TYPE_CHECK_INTEGRITY_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_ULONG );
}

/**
 * ofa_check_integrity_bin_new:
 */
ofaCheckIntegrityBin *
ofa_check_integrity_bin_new( void )
{
	ofaCheckIntegrityBin *self;

	self = g_object_new( OFA_TYPE_CHECK_INTEGRITY_BIN, NULL );

	setup_bin( self );

	return( self );
}

static void
setup_bin( ofaCheckIntegrityBin *bin )
{
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel;

	builder = gtk_builder_new_from_file( st_bin_xml );

	object = gtk_builder_get_object( builder, "cib-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( bin ), GTK_WINDOW( toplevel ), "top" );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

/**
 * ofa_check_integrity_bin_set_dossier:
 */
void
ofa_check_integrity_bin_set_dossier( ofaCheckIntegrityBin *bin, ofoDossier *dossier )
{
	ofaCheckIntegrityBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_CHECK_INTEGRITY_BIN( bin ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		priv->dossier = dossier;

		g_idle_add(( GSourceFunc ) do_run, bin );
	}
}

static gboolean
do_run( ofaCheckIntegrityBin *bin )
{
	ofaCheckIntegrityBinPrivate *priv;

	priv = bin->priv;

	check_dossier_run( bin );
	check_bat_lines_run( bin );
	check_accounts_run( bin );
	check_entries_run( bin );
	check_ledgers_run( bin );
	check_ope_templates_run( bin );

	set_checks_result( bin );

	g_signal_emit_by_name( bin, "ofa-done", priv->total_errs );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * Check that all references from dossier exist.
 */
static void
check_dossier_run( ofaCheckIntegrityBin *bin )
{
	ofaCheckIntegrityBinPrivate *priv;
	myProgressBar *bar;
	gulong count, i;
	const gchar *cur_code, *for_ope, *sld_ope, *acc_number;
	GSList *currencies, *it;
	ofoCurrency *cur_obj;
	ofoOpeTemplate *ope_obj;
	ofoAccount *acc_obj;
	gchar *str;

	bar = get_new_bar( bin, "dossier-parent" );
	gtk_widget_show_all( GTK_WIDGET( bin ) );

	priv = bin->priv;
	priv->dossier_errs = 0;
	currencies = ofo_dossier_get_currencies( priv->dossier );
	count = 3+g_slist_length( currencies );
	i = 0;

	/* check for default currency */
	cur_code = ofo_dossier_get_default_currency( priv->dossier );
	if( !my_strlen( cur_code )){
		add_message( bin, _( "Dossier has no default currency" ));
		priv->dossier_errs += 1;
	} else {
		cur_obj = ofo_currency_get_by_code( priv->dossier, cur_code );
		if( !cur_obj || !OFO_IS_CURRENCY( cur_obj )){
			str = g_strdup_printf(
					_( "Dossier default currency '%s' doesn't exist" ), cur_code );
			add_message( bin, str );
			g_free( str );
			priv->dossier_errs += 1;
		}
	}
	set_bar_progression( bar, count, ++i );

	/* check for forward and solde operation templates */
	for_ope = ofo_dossier_get_forward_ope( priv->dossier );
	if( !my_strlen( for_ope )){
		add_message( bin, _( "Dossier has no forward operation template" ));
		priv->dossier_errs += 1;
	} else {
		ope_obj = ofo_ope_template_get_by_mnemo( priv->dossier, for_ope );
		if( !ope_obj || !OFO_IS_OPE_TEMPLATE( ope_obj )){
			str = g_strdup_printf(
					_( "Dossier forward operation template '%s' doesn't exist" ), for_ope );
			add_message( bin, str );
			g_free( str );
			priv->dossier_errs += 1;
		}
	}
	set_bar_progression( bar, count, ++i );

	sld_ope = ofo_dossier_get_sld_ope( priv->dossier );
	if( !my_strlen( sld_ope )){
		add_message( bin, _( "Dossier has no solde operation template" ));
		priv->dossier_errs += 1;
	} else {
		ope_obj = ofo_ope_template_get_by_mnemo( priv->dossier, sld_ope );
		if( !ope_obj || !OFO_IS_OPE_TEMPLATE( ope_obj )){
			str = g_strdup_printf(
					_( "Dossier solde operation template '%s' doesn't exist" ), sld_ope );
			add_message( bin, str );
			g_free( str );
			priv->dossier_errs += 1;
		}
	}
	set_bar_progression( bar, count, ++i );

	/* check solde accounts per currency */

	for( it=currencies ; it ; it=it->next ){
		cur_code = ( const gchar * ) it->data;
		if( !my_strlen( cur_code )){
			add_message( bin, _( "Dossier solde account has no currency" ));
			priv->dossier_errs += 1;
		} else {
			cur_obj = ofo_currency_get_by_code( priv->dossier, cur_code );
			if( !cur_obj || !OFO_IS_CURRENCY( cur_obj )){
				str = g_strdup_printf(
						_( "Dossier solde account currency '%s' doesn't exist" ), cur_code );
				add_message( bin, str );
				g_free( str );
				priv->dossier_errs += 1;
			}
			acc_number = ofo_dossier_get_sld_account( priv->dossier, cur_code );
			if( !my_strlen( acc_number )){
				str = g_strdup_printf(
						_( "Dossier solde account for currency '%s' is empty" ), cur_code );
				add_message( bin, str );
				g_free( str );
				priv->dossier_errs += 1;
			} else {
				acc_obj = ofo_account_get_by_number( priv->dossier, acc_number );
				if( !acc_obj || !OFO_IS_ACCOUNT( acc_obj )){
					str = g_strdup_printf(
							_( "Dossier solde account '%s' for currency '%s' doesn't exist" ),
							acc_number, cur_code );
					add_message( bin, str );
					g_free( str );
					priv->dossier_errs += 1;
				}
			}
		}
		set_bar_progression( bar, count, ++i );
	}

	ofo_dossier_free_currencies( currencies );

	set_check_status( bin, priv->dossier_errs, "dossier-result" );
}

/*
 * Check that BAT and BAT lines are OK
 */
static void
check_bat_lines_run( ofaCheckIntegrityBin *bin )
{
	ofaCheckIntegrityBinPrivate *priv;
	myProgressBar *bar;
	gulong count, i;
	GList *bats, *it, *lines, *itl;
	ofoBat *bat;
	ofoBatLine *line;
	const gchar *cur_code;
	ofoCurrency *cur_obj;
	gchar *str;
	ofxCounter id, idline;

	bar = get_new_bar( bin, "bat-parent" );
	gtk_widget_show_all( GTK_WIDGET( bin ));

	priv = bin->priv;
	priv->bat_lines_errs = 0;
	bats = ofo_bat_get_dataset( priv->dossier );
	count = g_list_length( bats );

	if( count == 0 ){
		set_bar_progression( bar, 0, 0 );
	}

	for( i=1, it=bats ; it ; ++i, it=it->next ){
		bat = OFO_BAT( it->data );
		id = ofo_bat_get_id( bat );

		/* it is ok for a BAT file to not have a currency set */
		cur_code = ofo_bat_get_currency( bat );
		if( my_strlen( cur_code )){
			cur_obj = ofo_currency_get_by_code( priv->dossier, cur_code );
			if( !cur_obj || !OFO_IS_CURRENCY( cur_obj )){
				str = g_strdup_printf(
						_( "BAT file %lu currency '%s' doesn't exist" ), id, cur_code );
				add_message( bin, str );
				g_free( str );
				priv->bat_lines_errs += 1;
			}
		}

		lines = ofo_bat_line_get_dataset( priv->dossier, id );

		for( itl=lines ; itl ; itl=itl->next ){
			line = OFO_BAT_LINE( itl->data );
			idline = ofo_bat_line_get_line_id( line );

			/* it is ok for a BAT line to not have a currency */
			cur_code = ofo_bat_line_get_currency( line );
			if( my_strlen( cur_code )){
				cur_obj = ofo_currency_get_by_code( priv->dossier, cur_code );
				if( !cur_obj || !OFO_IS_CURRENCY( cur_obj )){
					str = g_strdup_printf(
							_( "BAT line %lu (from BAT file %lu) currency '%s' doesn't exist" ),
							idline, id, cur_code );
					add_message( bin, str );
					g_free( str );
					priv->bat_lines_errs += 1;
				}
			}

			/* do not check the entry number of the line */
		}

		ofo_bat_line_free_dataset( lines );

		set_bar_progression( bar, count, i );
	}

	set_check_status( bin, priv->bat_lines_errs, "bat-result" );
}

/*
 * 3/ check that accounts are balanced per currency
 */
static void
check_accounts_run( ofaCheckIntegrityBin *bin )
{
	ofaCheckIntegrityBinPrivate *priv;
	myProgressBar *bar;
	GList *accounts, *it;
	gulong count, i;
	ofoAccount *account;
	gint cla_num;
	const gchar *acc_num, *cur_code;
	ofoClass *cla_obj;
	gchar *str;
	ofoCurrency *cur_obj;

	bar = get_new_bar( bin, "account-parent" );
	gtk_widget_show_all( GTK_WIDGET( bin ));

	priv = bin->priv;
	priv->accounts_errs = 0;
	accounts = ofo_account_get_dataset( priv->dossier );
	count = g_list_length( accounts );

	for( i=1, it=accounts ; it && count ; ++i, it=it->next ){
		account = OFO_ACCOUNT( it->data );
		acc_num = ofo_account_get_number( account );

		cla_num = ofo_account_get_class( account );
		cla_obj = ofo_class_get_by_number( priv->dossier, cla_num );
		if( !cla_obj || !OFO_IS_CLASS( cla_obj )){
			str = g_strdup_printf(
					_( "Class %d doesn't exist for account %s" ), cla_num, acc_num );
			add_message( bin, str );
			g_free( str );
			priv->accounts_errs += 1;
		}

		if( !ofo_account_is_root( account )){
			cur_code = ofo_account_get_currency( account );
			if( !my_strlen( cur_code )){
				str = g_strdup_printf( _( "Account %s doesn't have a currency" ), acc_num );
				add_message( bin, str );
				g_free( str );
				priv->accounts_errs += 1;
			} else {
				cur_obj = ofo_currency_get_by_code( priv->dossier, cur_code );
				if( !cur_obj || !OFO_IS_CURRENCY( cur_obj )){
					str = g_strdup_printf(
							_( "Account %s currency '%s' doesn't exist" ), acc_num, cur_code );
					add_message( bin, str );
					g_free( str );
					priv->accounts_errs += 1;
				}
			}
		}

		set_bar_progression( bar, count, i );
	}

	set_check_status( bin, priv->accounts_errs, "account-result" );
}

/*
 * check for entries integrity
 */
static void
check_entries_run( ofaCheckIntegrityBin *bin )
{
	ofaCheckIntegrityBinPrivate *priv;
	myProgressBar *bar;
	GList *entries, *it;
	gulong count, i;
	ofoEntry *entry;
	gchar *str;
	ofxCounter number;
	const gchar *acc_number, *cur_code, *led_mnemo, *ope_mnemo;
	ofoAccount *acc_obj;
	ofoCurrency *cur_obj;
	ofoLedger *led_obj;
	ofoOpeTemplate *ope_obj;

	bar = get_new_bar( bin, "entry-parent" );
	gtk_widget_show_all( GTK_WIDGET( bin ));

	priv = bin->priv;
	priv->entries_errs = 0;
	entries = ofo_entry_get_dataset_by_account( priv->dossier, NULL );
	count = g_list_length( entries );

	for( i=1, it=entries ; it && count ; ++i, it=it->next ){
		entry = OFO_ENTRY( it->data );
		number = ofo_entry_get_number( entry );

		acc_number = ofo_entry_get_account( entry );
		if( !my_strlen( acc_number )){
			str = g_strdup_printf(
					_( "Entry %lu doesn't have account" ), number );
			add_message( bin, str );
			g_free( str );
			priv->entries_errs += 1;
		} else {
			acc_obj = ofo_account_get_by_number( priv->dossier, acc_number );
			if( !acc_obj || !OFO_IS_ACCOUNT( acc_obj )){
				str = g_strdup_printf(
						_( "Entry %lu has account %s which doesn't exist" ), number, acc_number );
				add_message( bin, str );
				g_free( str );
				priv->entries_errs += 1;
			}
		}

		cur_code = ofo_entry_get_currency( entry );
		if( !my_strlen( cur_code )){
			str = g_strdup_printf( _( "Entry %lu doesn't have a currency" ), number );
			add_message( bin, str );
			g_free( str );
			priv->entries_errs += 1;
		} else {
			cur_obj = ofo_currency_get_by_code( priv->dossier, cur_code );
			if( !cur_obj || !OFO_IS_CURRENCY( cur_obj )){
				str = g_strdup_printf(
						_( "Entry %lu has currency '%s' which doesn't exist" ), number, cur_code );
				add_message( bin, str );
				g_free( str );
				priv->entries_errs += 1;
			}
		}

		led_mnemo = ofo_entry_get_ledger( entry );
		if( !my_strlen( led_mnemo )){
			str = g_strdup_printf( _( "Entry %lu doesn't have a ledger" ), number );
			add_message( bin, str );
			g_free( str );
			priv->entries_errs += 1;
		} else {
			led_obj = ofo_ledger_get_by_mnemo( priv->dossier, led_mnemo );
			if( !led_obj || !OFO_IS_LEDGER( led_obj )){
				str = g_strdup_printf(
						_( "Entry %lu has ledger '%s' which doesn't exist" ), number, led_mnemo );
				add_message( bin, str );
				g_free( str );
				priv->entries_errs += 1;
			}
		}

		/* ope template is not mandatory */
		ope_mnemo = ofo_entry_get_ope_template( entry );
		if( my_strlen( ope_mnemo )){
			ope_obj = ofo_ope_template_get_by_mnemo( priv->dossier, ope_mnemo );
			if( !ope_obj || !OFO_IS_OPE_TEMPLATE( ope_obj )){
				str = g_strdup_printf(
						_( "Entry %lu has operation template '%s' which doesn't exist" ), number, ope_mnemo );
				add_message( bin, str );
				g_free( str );
				priv->entries_errs += 1;
			}
		}

		set_bar_progression( bar, count, i );
	}

	set_check_status( bin, priv->entries_errs, "entry-result" );
}

/*
 * check for ledgers integrity
 */
static void
check_ledgers_run( ofaCheckIntegrityBin *bin )
{
	ofaCheckIntegrityBinPrivate *priv;
	myProgressBar *bar;
	GList *ledgers, *it;
	GList *currencies, *itc;
	gulong count, i;
	ofoLedger *ledger;
	gchar *str;
	const gchar *mnemo, *cur_code;
	ofoCurrency *cur_obj;

	bar = get_new_bar( bin, "ledger-parent" );
	gtk_widget_show_all( GTK_WIDGET( bin ));

	priv = bin->priv;
	priv->ledgers_errs = 0;
	ledgers = ofo_ledger_get_dataset( priv->dossier );
	count = g_list_length( ledgers );

	for( i=1, it=ledgers ; it && count ; ++i, it=it->next ){
		ledger = OFO_LEDGER( it->data );
		mnemo = ofo_ledger_get_mnemo( ledger );
		currencies = ofo_ledger_get_currencies( ledger );
		for( itc=currencies ; itc ; itc=itc->next ){
			cur_code = ( const gchar * ) itc->data;
			if( !my_strlen( cur_code )){
				str = g_strdup_printf( _( "Ledger %s has an empty currency" ), mnemo );
				add_message( bin, str );
				g_free( str );
				priv->ledgers_errs += 1;
			} else {
				cur_obj = ofo_currency_get_by_code( priv->dossier, cur_code );
				if( !cur_obj || !OFO_IS_CURRENCY( cur_obj )){
					str = g_strdup_printf(
							_( "Ledger %s has currency '%s' which doesn't exist" ), mnemo, cur_code );
					add_message( bin, str );
					g_free( str );
					priv->ledgers_errs += 1;
				}
			}
		}
		g_list_free( currencies );

		set_bar_progression( bar, count, i );
	}

	set_check_status( bin, priv->ledgers_errs, "ledger-result" );
}

/*
 * check for ope_templates integrity
 */
static void
check_ope_templates_run( ofaCheckIntegrityBin *bin )
{
	ofaCheckIntegrityBinPrivate *priv;
	myProgressBar *bar;
	GList *ope_templates, *it;
	gulong count, i;
	ofoOpeTemplate *ope_template;
	gchar *str;
	const gchar *mnemo, *led_mnemo, *acc_number;
	ofoLedger *led_obj;
	gint nbdets, idet;

	bar = get_new_bar( bin, "ope-parent" );
	gtk_widget_show_all( GTK_WIDGET( bin ));

	priv = bin->priv;
	priv->ope_templates_errs = 0;
	ope_templates = ofo_ope_template_get_dataset( priv->dossier );
	count = g_list_length( ope_templates );

	for( i=1, it=ope_templates ; it && count ; ++i, it=it->next ){
		ope_template = OFO_OPE_TEMPLATE( it->data );
		mnemo = ofo_ope_template_get_mnemo( ope_template );

		/* ledger is optional here */
		led_mnemo = ofo_ope_template_get_ledger( ope_template );
		if( my_strlen( led_mnemo )){
			led_obj = ofo_ledger_get_by_mnemo( priv->dossier, led_mnemo );
			if( !led_obj || !OFO_IS_LEDGER( led_obj )){
				str = g_strdup_printf(
						_( "Operation template %s has ledger '%s' which doesn't exist" ), mnemo, led_mnemo );
				add_message( bin, str );
				g_free( str );
				priv->entries_errs += 1;
			}
		}

		nbdets = ofo_ope_template_get_detail_count( ope_template );
		for( idet=0 ; idet<nbdets ; ++idet ){
			/* cannot check for account without first identifying formulas */
			acc_number = ofo_ope_template_get_detail_account( ope_template, idet );
			if( !my_strlen( acc_number )){

			}
		}

		set_bar_progression( bar, count, i );
	}

	set_check_status( bin, priv->ope_templates_errs, "ope-result" );
}

/*
 * display OK/NOT OK for a check
 */
static void
set_check_status( ofaCheckIntegrityBin *bin, gulong errs, const gchar *w_name )
{
	GtkWidget *label;
	gchar *str;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), w_name );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	if( errs == 0 ){
		gtk_label_set_text( GTK_LABEL( label ), _( "OK" ));

	} else {
		str = g_strdup_printf( _( "%lu error(s)" ), errs );
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );
	}

	my_utils_widget_set_style( label, errs == 0 ? "labelinfo" : "labelerror" );
}

static myProgressBar *
get_new_bar( ofaCheckIntegrityBin *bin, const gchar *w_name )
{
	GtkWidget *parent;
	myProgressBar *bar;

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), w_name );
	g_return_val_if_fail( parent && GTK_IS_CONTAINER( parent ), FALSE );

	bar = my_progress_bar_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( bar ));

	return( bar );
}

static void
set_bar_progression( myProgressBar *bar, gulong total, gulong current )
{
	gdouble progress;
	gchar *text;

	if( total > 0 ){
		progress = ( gdouble ) current / ( gdouble ) total;
		g_signal_emit_by_name( bar, "ofa-double", progress );
	}
	text = g_strdup_printf( "%lu/%lu", current, total );
	g_signal_emit_by_name( bar, "ofa-text", text );
	g_free( text );
}

/*
 * after the end of individual checks (entries, ledgers, accounts)
 * check that the balances are the sames
 */
static void
set_checks_result( ofaCheckIntegrityBin *bin )
{
	ofaCheckIntegrityBinPrivate *priv;
	GtkWidget *label;
	gchar *str;

	priv = bin->priv;

	priv->total_errs =
			priv->dossier_errs
			+ priv->bat_lines_errs
			+ priv->accounts_errs
			+ priv->entries_errs
			+ priv->ledgers_errs
			+ priv->ope_templates_errs;

	if( priv->total_errs > 0 ){
		str = g_strdup_printf(
				_( "We have detected %lu integrity errors in the DBMS." ), priv->total_errs );
		my_utils_dialog_warning( str );
		g_free( str );

	} else {
		label = my_utils_container_get_child_by_name(
						GTK_CONTAINER( bin ), "p4-label-end" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));

		if( priv->total_errs == 0 ){
			gtk_label_set_text( GTK_LABEL( label ),
					_( "Your DBMS is right. Good !" ));
			my_utils_widget_set_style( label, "labelinfo" );

		} else {
			gtk_label_set_text( GTK_LABEL( label ),
					_( "Detected integrity errors have to be fixed." ));
			my_utils_widget_set_style( label, "labelerror" );
		}
	}
}

static void
add_message( ofaCheckIntegrityBin *bin, const gchar *text )
{
	ofaCheckIntegrityBinPrivate *priv;
	GtkWidget *view;
	GtkTextIter iter;
	gchar *str;

	priv = bin->priv;

	if( !priv->text_buffer ){
		view = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "textview" );
		g_return_if_fail( view && GTK_IS_TEXT_VIEW( view ));
		priv->text_buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( view ));
	}
	g_return_if_fail( priv->text_buffer && GTK_IS_TEXT_BUFFER( priv->text_buffer ));

	gtk_text_buffer_get_end_iter( priv->text_buffer, &iter );

	str = g_strdup_printf( "%s\n", text );
	gtk_text_buffer_insert( priv->text_buffer, &iter, str, -1 );
	g_free( str );
}

/**
 * ofa_check_integrity_bin_get_status:
 */
gboolean
ofa_check_integrity_bin_get_status( const ofaCheckIntegrityBin *bin )
{
	ofaCheckIntegrityBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_CHECK_INTEGRITY_BIN( bin ), FALSE );

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		return( priv->total_errs == 0 );
	}

	return( FALSE );
}
