/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-irenderable.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "ui/ofa-account-balance-args.h"
#include "ui/ofa-account-balance-render.h"

/* private instance data
 */
typedef struct {

	ofaAccountBalanceArgs *args_bin;

	/* runtime
	 */
	ofaIGetter            *getter;
	gchar                 *settings_prefix;
	GDate                  from_date;
	GDate                  to_date;
	gint                   count;						/* count of accounts */
	GList                 *accounts;

	/* print datas
	 */
	gdouble                render_width;
	gdouble                render_height;
	gdouble                page_margin;
	gdouble                amount_width;
	gdouble                body_number_ltab;
	gint                   body_number_max_size;		/* Pango units */
	gdouble                body_label_ltab;
	gint                   body_label_max_size;			/* Pango units */
	gdouble                body_begin_solde_rtab;
	gdouble                body_begin_sens_ltab;
	gdouble                body_debit_period_rtab;
	gdouble                body_credit_period_rtab;
	gdouble                body_end_solde_rtab;
	gdouble                body_end_sens_ltab;
	gdouble                body_currency_ltab;

	/* general total by currency
	 */
	GList                 *gen_totals;
}
	ofaAccountBalanceRenderPrivate;

/* A structure to hold the sum of all accounts
 */
typedef struct {
	ofoCurrency *currency;
	ofxAmount    begin_solde;
	ofxAmount    debits;
	ofxAmount    credits;
	ofxAmount    end_solde;
}
	sCurrency;

/* A structure to hold the line for an account
 */
typedef struct {
	ofoAccount *account;
	sCurrency  *scur;
}
	sAccount;

/*
 * Accounts balances print uses a portrait orientation
 */
#define THIS_PAGE_ORIENTATION            GTK_PAGE_ORIENTATION_PORTRAIT
#define THIS_PAPER_NAME                  GTK_PAPER_NAME_A4

static const gchar *st_page_header_title_accounts = N_( "Accounts Balance Summary" );

/* these are parms which describe the page layout
 */

static const gchar *st_title2_font       = "Sans Bold 8";
static const gchar *st_summary_font      = "Sans Bold 6";

static GtkWidget         *page_v_get_top_focusable_widget( const ofaPage *page );
static void               paned_page_v_init_view( ofaPanedPage *page );
static GtkWidget         *render_page_v_get_args_widget( ofaRenderPage *page );
static const gchar       *render_page_v_get_paper_name( ofaRenderPage *page );
static GtkPageOrientation render_page_v_get_page_orientation( ofaRenderPage *page );
static void               render_page_v_get_print_settings( ofaRenderPage *page, GKeyFile **keyfile, gchar **group_name );
static GList             *render_page_v_get_dataset( ofaRenderPage *page );
static void               compute_accounts_balance( ofaAccountBalanceRender *self );
static gint               cmp_entries( ofoEntry *a, ofoEntry *b );
static void               render_page_v_free_dataset( ofaRenderPage *page, GList *dataset );
static void               on_args_changed( ofaAccountBalanceArgs *bin, ofaAccountBalanceRender *page );
static void               irenderable_iface_init( ofaIRenderableInterface *iface );
static guint              irenderable_get_interface_version( const ofaIRenderable *instance );
static void               irenderable_begin_render( ofaIRenderable *instance );
static gchar             *irenderable_get_dossier_label( const ofaIRenderable *instance );
static void               irenderable_draw_page_header_title( ofaIRenderable *instance );
static void               irenderable_draw_header_column_names( ofaIRenderable *instance );
static void               irenderable_draw_line( ofaIRenderable *instance );
static void               irenderable_draw_last_summary( ofaIRenderable *instance );
static const gchar       *irenderable_get_summary_font( const ofaIRenderable *instance, guint page_num );
static void               irenderable_clear_runtime_data( ofaIRenderable *instance );
static void               draw_account_balance( ofaIRenderable *instance, GList *list, gdouble top, const gchar *title );
static void               draw_amounts( ofaIRenderable *instance, sCurrency *scur );
static void               add_by_currency( ofaAccountBalanceRender *self, GList **list, sCurrency *scur );
static gint               cmp_currencies( const sCurrency *a, const sCurrency *b );
static void               free_currencies( GList **list );
static sAccount          *find_account( ofaAccountBalanceRender *self, ofoAccount *account );
static gint               cmp_accounts( const sAccount *a, const sAccount *b );
static void               free_accounts( GList **list );
static void               free_account( sAccount *sacc );
static void               read_settings( ofaAccountBalanceRender *self );
static void               write_settings( ofaAccountBalanceRender *self );

G_DEFINE_TYPE_EXTENDED( ofaAccountBalanceRender, ofa_account_balance_render, OFA_TYPE_RENDER_PAGE, 0,
		G_ADD_PRIVATE( ofaAccountBalanceRender )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IRENDERABLE, irenderable_iface_init ))

static void
account_balance_render_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_balance_render_finalize";
	ofaAccountBalanceRenderPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_BALANCE_RENDER( instance ));

	/* free data members here */
	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( instance ));

	g_free( priv->settings_prefix );

	free_accounts( &priv->accounts );
	free_currencies( &priv->gen_totals );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_balance_render_parent_class )->finalize( instance );
}

static void
account_balance_render_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_ACCOUNT_BALANCE_RENDER( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		write_settings( OFA_ACCOUNT_BALANCE_RENDER( instance ));

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_balance_render_parent_class )->dispose( instance );
}

static void
ofa_account_balance_render_init( ofaAccountBalanceRender *self )
{
	static const gchar *thisfn = "ofa_account_balance_render_init";
	ofaAccountBalanceRenderPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ACCOUNT_BALANCE_RENDER( self ));

	priv = ofa_account_balance_render_get_instance_private( self );

	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_account_balance_render_class_init( ofaAccountBalanceRenderClass *klass )
{
	static const gchar *thisfn = "ofa_account_balance_render_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_balance_render_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_balance_render_finalize;

	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = page_v_get_top_focusable_widget;

	OFA_PANED_PAGE_CLASS( klass )->init_view = paned_page_v_init_view;

	OFA_RENDER_PAGE_CLASS( klass )->get_args_widget = render_page_v_get_args_widget;
	OFA_RENDER_PAGE_CLASS( klass )->get_paper_name = render_page_v_get_paper_name;
	OFA_RENDER_PAGE_CLASS( klass )->get_page_orientation = render_page_v_get_page_orientation;
	OFA_RENDER_PAGE_CLASS( klass )->get_print_settings = render_page_v_get_print_settings;
	OFA_RENDER_PAGE_CLASS( klass )->get_dataset = render_page_v_get_dataset;
	OFA_RENDER_PAGE_CLASS( klass )->free_dataset = render_page_v_free_dataset;
}

static GtkWidget *
page_v_get_top_focusable_widget( const ofaPage *page )
{
	return( NULL );
}

static void
paned_page_v_init_view( ofaPanedPage *page )
{
	static const gchar *thisfn = "ofa_account_balance_render_paned_page_v_init_view";
	ofaAccountBalanceRenderPrivate *priv;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( page ));

	on_args_changed( priv->args_bin, OFA_ACCOUNT_BALANCE_RENDER( page ));

	read_settings( OFA_ACCOUNT_BALANCE_RENDER( page ));
}

static GtkWidget *
render_page_v_get_args_widget( ofaRenderPage *page )
{
	ofaAccountBalanceRenderPrivate *priv;
	ofaAccountBalanceArgs *bin;

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( page ));

	priv->getter = ofa_page_get_getter( OFA_PAGE( page ));

	bin = ofa_account_balance_args_new( priv->getter, priv->settings_prefix );
	g_signal_connect( bin, "ofa-changed", G_CALLBACK( on_args_changed ), page );
	priv->args_bin = bin;

	return( GTK_WIDGET( bin ));
}

static const gchar *
render_page_v_get_paper_name( ofaRenderPage *page )
{
	return( THIS_PAPER_NAME );
}

static GtkPageOrientation
render_page_v_get_page_orientation( ofaRenderPage *page )
{
	return( THIS_PAGE_ORIENTATION );
}

static void
render_page_v_get_print_settings( ofaRenderPage *page, GKeyFile **keyfile, gchar **group_name )
{
	ofaAccountBalanceRenderPrivate *priv;
	myISettings *settings;

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( page ));

	settings = ofa_igetter_get_user_settings( priv->getter );
	*keyfile = my_isettings_get_keyfile( settings );
	*group_name = g_strdup_printf( "%s-print", priv->settings_prefix );
}

static GList *
render_page_v_get_dataset( ofaRenderPage *page )
{
	ofaAccountBalanceRenderPrivate *priv;
	GList *dataset, *it, *details;
	ofaIDateFilter *date_filter;
	ofoAccount *account;

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( page ));

	date_filter = ofa_account_balance_args_get_date_filter( priv->args_bin );
	my_date_set_from_date( &priv->from_date, ofa_idate_filter_get_date( date_filter, IDATE_FILTER_FROM ));
	my_date_set_from_date( &priv->to_date, ofa_idate_filter_get_date( date_filter, IDATE_FILTER_TO ));

	/* compute the entries balances by account */
	compute_accounts_balance( OFA_ACCOUNT_BALANCE_RENDER( page ));

	/* get only the detail accounts */
	dataset = ofo_account_get_dataset( priv->getter );
	details = NULL;

	for( it=dataset ; it ; it=it->next ){
		account = ( ofoAccount * ) it->data;
		g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );
		if( !ofo_account_is_root( account )){
			details = g_list_prepend( details, account );
		}
	}
	priv->count = g_list_length( details );

	return( g_list_reverse( details ));
}

/*
 * Compute the accounts balances once, before rendering
 */
static void
compute_accounts_balance( ofaAccountBalanceRender *self )
{
	static const gchar *thisfn = "ofa_account_balance_render_compute_accounts_balance";
	ofaAccountBalanceRenderPrivate *priv;
	ofaHub *hub;
	ofoDossier *dossier;
	const GDate *exe_begin, *deffect;
	GList *entries, *sorted, *it;
	ofoEntry *entry;
	ofeEntryStatus status;
	ofeEntryRule rule;
	const gchar *acc_number;
	gchar *prev_number;
	sAccount *sacc;
	ofoAccount *account;
	gint cmp;
	ofxAmount debit, credit;
	gboolean is_begin;

	priv = ofa_account_balance_render_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );
	dossier = ofa_hub_get_dossier( hub );
	exe_begin = ofo_dossier_get_exe_begin( dossier );

	/* whether the from_date is the beginning of the exercice */
	is_begin = my_date_is_valid( exe_begin ) && my_date_compare( &priv->from_date, exe_begin ) == 0;

	/* get all entries (once) sorted by account, effect_date */
	entries = ofo_entry_get_dataset( priv->getter );
	sorted = g_list_sort( entries, ( GCompareFunc ) cmp_entries );
	prev_number = NULL;
	priv->accounts = NULL;
	sacc = NULL;

	for( it=sorted ; it ; it=it->next ){
		entry = ( ofoEntry * ) it->data;
		g_return_if_fail( OFO_IS_ENTRY( entry ));
		status = ofo_entry_get_status( entry );
		if( status == ENT_STATUS_PAST || status == ENT_STATUS_DELETED ){
			continue;
		}
		/* on new account, initialize a new structure */
		acc_number = ofo_entry_get_account( entry );
		if( my_collate( acc_number, prev_number ) != 0 ){
			g_free( prev_number );
			prev_number = g_strdup( acc_number );
			account = ofo_account_get_by_number( priv->getter, acc_number );
			g_return_if_fail( account && OFO_IS_ACCOUNT( account ) && !ofo_account_is_root( account ));
			sacc = find_account( self, account );
		}
		g_return_if_fail( sacc != NULL );
		/* do not consider the effect date after the to_date */
		deffect = ofo_entry_get_deffect( entry );
		if( my_date_compare( &priv->to_date, deffect ) < 0 ){
			continue;
		}
		/* if from_date is the beginning of the exercice, then begin_solde
		 * must take into account the forward entries at this date
		 * else begin_solde stops at the day-1 */
		rule = ofo_entry_get_rule( entry );
		debit = ofo_entry_get_debit( entry );
		credit = ofo_entry_get_credit( entry );
		if( is_begin ){
			cmp = my_date_compare( deffect, &priv->from_date );
			if( cmp < 0 ){
				g_warning(
						"%s: have entry number %lu before the beginning of the exercice, but it is not 'past'",
						thisfn, ofo_entry_get_number( entry ));
				debit = 0.0;
				credit = 0.0;
			} else if( cmp == 0 && rule == ENT_RULE_FORWARD ){
				sacc->scur->begin_solde += credit - debit;
			} else {
				sacc->scur->debits += debit;
				sacc->scur->credits += credit;
			}
		} else {
			cmp = my_date_compare( deffect, &priv->from_date );
			if( cmp < 0 ){
				sacc->scur->begin_solde += credit - debit;

			} else {
				sacc->scur->debits += debit;
				sacc->scur->credits += credit;
			}
		}
		sacc->scur->end_solde += credit - debit;
	}

	g_free( prev_number );
}

/*
 * sort the entries by account and effect_date
 */
static gint
cmp_entries( ofoEntry *a, ofoEntry *b )
{
	gint cmp;
	const gchar *acc_a, *acc_b;
	const GDate *effect_a, *effect_b;

	acc_a = ofo_entry_get_account( a );
	acc_b = ofo_entry_get_account( b );
	cmp = my_collate( acc_a, acc_b );

	if( cmp == 0 ){
		effect_a = ofo_entry_get_deffect( a );
		effect_b = ofo_entry_get_deffect( b );
		cmp = my_date_compare( effect_a, effect_b );
	}

	return( cmp );
}

static void
render_page_v_free_dataset( ofaRenderPage *page, GList *dataset )
{
	ofaAccountBalanceRenderPrivate *priv;

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( page ));

	/* the filtered list of accounts */
	g_list_free( dataset );

	free_accounts( &priv->accounts );
}

/*
 * ofaAccountBalanceArgs "ofa-changed" handler
 */
static void
on_args_changed( ofaAccountBalanceArgs *bin, ofaAccountBalanceRender *page )
{
	gboolean valid;
	gchar *message;

	valid = ofa_account_balance_args_is_valid( bin, &message );
	ofa_render_page_set_args_changed( OFA_RENDER_PAGE( page ), valid, message );
	g_free( message );
}

static void
irenderable_iface_init( ofaIRenderableInterface *iface )
{
	static const gchar *thisfn = "ofa_account_balance_render_irenderable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = irenderable_get_interface_version;
	iface->begin_render = irenderable_begin_render;
	iface->get_dossier_label = irenderable_get_dossier_label;
	iface->draw_page_header_title = irenderable_draw_page_header_title;
	iface->draw_header_column_names = irenderable_draw_header_column_names;
	iface->draw_line = irenderable_draw_line;
	iface->draw_last_summary = irenderable_draw_last_summary;
	iface->get_summary_font = irenderable_get_summary_font;
	iface->clear_runtime_data = irenderable_clear_runtime_data;
}

static guint
irenderable_get_interface_version( const ofaIRenderable *instance )
{
	return( 1 );
}

static void
irenderable_begin_render( ofaIRenderable *instance )
{
	static const gchar *thisfn = "ofa_account_balance_render_irenderable_begin_render";
	ofaAccountBalanceRenderPrivate *priv;
	gdouble spacing, account_width, amount_width, sens_width, cur_width;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( instance ));

	priv->render_width = ofa_irenderable_get_render_width( instance );
	priv->render_height = ofa_irenderable_get_render_height( instance );
	priv->page_margin = ofa_irenderable_get_page_margin( instance );
	spacing = ofa_irenderable_get_columns_spacing( instance );

	/* account width should use the body font */
	ofa_irenderable_set_font( instance, ofa_irenderable_get_body_font( instance ));
	account_width = ofa_irenderable_get_text_width( instance, "XXXXXXXX" );

	/* the width of the amounts should use the last summary font */
	ofa_irenderable_set_font( instance, ofa_irenderable_get_summary_font( instance, 0 ));
	sens_width = ofa_irenderable_get_text_width( instance, "XX" );
	cur_width = ofa_irenderable_get_text_width( instance, "XXX" );
	amount_width = ofa_irenderable_get_text_width( instance, "99,999,999.99" );
	priv->amount_width = amount_width;

	/* starting from the left : body_number_ltab on the left margin */
	priv->body_number_ltab = priv->page_margin;
	priv->body_label_ltab = priv->body_number_ltab + account_width + spacing;

	/* starting from the right */
	priv->body_currency_ltab = priv->render_width - priv->page_margin - cur_width;
	priv->body_end_sens_ltab = priv->body_currency_ltab - spacing - sens_width;
	priv->body_end_solde_rtab = priv->body_end_sens_ltab - spacing;
	priv->body_credit_period_rtab = priv->body_end_solde_rtab - amount_width - spacing;
	priv->body_debit_period_rtab = priv->body_credit_period_rtab - amount_width - spacing;
	priv->body_begin_sens_ltab = priv->body_debit_period_rtab - amount_width - spacing - sens_width;
	priv->body_begin_solde_rtab = priv->body_begin_sens_ltab - spacing;

	/* max size in Pango units */
	priv->body_number_max_size = account_width*PANGO_SCALE;
	priv->body_label_max_size = ( priv->body_begin_solde_rtab - amount_width - spacing - priv->body_label_ltab )*PANGO_SCALE;
}

/*
 * Accounts Balance
 */
static gchar *
irenderable_get_dossier_label( const ofaIRenderable *instance )
{
	ofaAccountBalanceRenderPrivate *priv;
	ofaHub *hub;
	ofoDossier *dossier;
	const gchar *label;

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( instance ));

	hub = ofa_igetter_get_hub( priv->getter );
	dossier = ofa_hub_get_dossier( hub );
	label = ofo_dossier_get_label( dossier );

	return( g_strdup( label ));
}

/*
 * Title is two lines
 */
static void
irenderable_draw_page_header_title( ofaIRenderable *instance )
{
	ofaAccountBalanceRenderPrivate *priv;
	gdouble y, r, g, b, height;
	GString *stitle;
	gchar *sfrom_date, *sto_date;

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( instance ));

	ofa_irenderable_get_title_color( instance, &r, &g, &b );
	ofa_irenderable_set_color( instance, r, g, b );
	y = ofa_irenderable_get_last_y( instance );

	/* line 1 general books summary */
	ofa_irenderable_set_font( instance,
			ofa_irenderable_get_title_font( instance, ofa_irenderable_get_current_page_num( instance )));
	height = ofa_irenderable_set_text( instance, priv->render_width/2, y,
			st_page_header_title_accounts, PANGO_ALIGN_CENTER );
	y += height;

	/* line 2 - From date xxx to date xxx */
	stitle = g_string_new( "" );

	sfrom_date = my_date_to_str( &priv->from_date, ofa_prefs_date_display( priv->getter ));
	sto_date = my_date_to_str( &priv->to_date, ofa_prefs_date_display( priv->getter ));
	if( my_date_is_valid( &priv->from_date )){
		g_string_append_printf( stitle, _( "From %s" ), sfrom_date );
		if( my_date_is_valid( &priv->to_date )){
			g_string_append_printf( stitle, _( " to %s" ), sto_date );
		}
	} else {
		g_string_append_printf( stitle, _( "Up to %s" ), sto_date );
	}
	g_free( sto_date );
	g_free( sfrom_date );

	ofa_irenderable_set_font( instance, st_title2_font );
	height = ofa_irenderable_set_text( instance, priv->render_width/2, y, stitle->str, PANGO_ALIGN_CENTER );
	g_string_free( stitle, TRUE );
	y += height;

	ofa_irenderable_set_last_y( instance, y );
}

static void
irenderable_draw_header_column_names( ofaIRenderable *instance )
{
	ofaAccountBalanceRenderPrivate *priv;
	static gdouble st_vspace_rate = 0.5;
	gdouble text_height, y, vspace, half;
	gchar *sdate;

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( instance ));

	y = ofa_irenderable_get_last_y( instance );
	text_height = ofa_irenderable_get_text_height( instance );
	half = text_height / 2.0;
	vspace = text_height * st_vspace_rate;
	y+= vspace;

	ofa_irenderable_set_text( instance,
			priv->body_number_ltab, y+half,
			_( "Account" ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			priv->body_label_ltab, y+half,
			_( "Label" ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			priv->body_begin_sens_ltab, y,
			_( "Solde au" ), PANGO_ALIGN_RIGHT );

	sdate = my_date_to_str( &priv->from_date, ofa_prefs_date_display( priv->getter ));
	ofa_irenderable_set_text( instance,
			priv->body_begin_sens_ltab, y+text_height,
			sdate, PANGO_ALIGN_RIGHT );
	g_free( sdate );

	ofa_irenderable_set_text( instance,
			priv->body_debit_period_rtab, y+half,
			_( "Total debits" ), PANGO_ALIGN_RIGHT );

	ofa_irenderable_set_text( instance,
			priv->body_credit_period_rtab, y+half,
			_( "Total credits" ), PANGO_ALIGN_RIGHT );

	ofa_irenderable_set_text( instance,
			priv->body_end_sens_ltab, y,
			_( "Solde au" ), PANGO_ALIGN_RIGHT );

	sdate = my_date_to_str( &priv->to_date, ofa_prefs_date_display( priv->getter ));
	ofa_irenderable_set_text( instance,
			priv->body_end_sens_ltab, y+text_height,
			sdate, PANGO_ALIGN_RIGHT );
	g_free( sdate );

	/* this set the 'y' height just after the column headers */
	y += 2.0*text_height + vspace;
	ofa_irenderable_set_last_y( instance, y );
}

/*
 * The rendering is account-driven.
 * For each account:
 * - get the balance at the beginning of the period
 * - adds all the entries from the period
 *
 * When requesting the balance between D1 and D2, we expect have
 * - the beginning solde at D1 0:00h
 * - all the entries from D1 to D2 (included)
 * - the resulting solde
 *
 * From our point of view, this is the same than requesting the solde at D1-1.
 */
static void
irenderable_draw_line( ofaIRenderable *instance )
{
	ofaAccountBalanceRenderPrivate *priv;
	GList *line;
	ofoAccount *account;
	sAccount *sacc;
	gdouble y;

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( instance ));

	line = ofa_irenderable_get_current_line( instance );
	if( line ){

		/* get the data properties */
		account = ( ofoAccount * ) line->data;
		g_return_if_fail( OFO_IS_ACCOUNT( account ));

		sacc = find_account( OFA_ACCOUNT_BALANCE_RENDER( instance ), account );

		/* render the line */
		y = ofa_irenderable_get_last_y( instance );

		ofa_irenderable_ellipsize_text( instance,
				priv->body_number_ltab, y,
				ofo_account_get_number( account ), priv->body_number_max_size );

		ofa_irenderable_ellipsize_text( instance,
				priv->body_label_ltab, y,
				ofo_account_get_label( account ), priv->body_label_max_size );

		draw_amounts( instance, sacc->scur );

		add_by_currency(
				OFA_ACCOUNT_BALANCE_RENDER( instance ),
				&priv->gen_totals, sacc->scur );
	}
}

/*
 * draw on the bottom of the last page the summary with one line per
 * currency
 */
static void
irenderable_draw_last_summary( ofaIRenderable *instance )
{
	ofaAccountBalanceRenderPrivate *priv;
	static const gdouble st_vspace_rate = 0.25;
	gdouble bottom, top, vspace, req_height, height, last_y;

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( instance ));

	if( !priv->count ){
		ofa_irenderable_draw_no_data( instance );
		return;
	}

	/* bottom of the rectangle */
	bottom = ofa_irenderable_get_max_y( instance );
	last_y = ofa_irenderable_get_last_y( instance );

	/* top of the rectangle */
	height = ofa_irenderable_get_text_height( instance );
	vspace = height * st_vspace_rate;
	req_height = g_list_length( priv->gen_totals ) * height
			+ ( 1+g_list_length( priv->gen_totals )) * vspace;
	top = bottom - req_height;

	ofa_irenderable_draw_rect( instance, 0, top, -1, req_height );
	top += vspace;

	draw_account_balance( instance, priv->gen_totals, top, _( "General balance : " ));

	ofa_irenderable_set_last_y( instance, last_y + req_height );
}

static const gchar *
irenderable_get_summary_font( const ofaIRenderable *instance, guint page_num )
{
	return( st_summary_font );
}

static void
irenderable_clear_runtime_data( ofaIRenderable *instance )
{
	ofaAccountBalanceRenderPrivate *priv;

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( instance ));

	free_currencies( &priv->gen_totals );
}

static void
draw_account_balance( ofaIRenderable *instance,
							GList *list, gdouble top, const gchar *title )
{
	ofaAccountBalanceRenderPrivate *priv;
	GList *it;
	gboolean first;
	sCurrency *scur;
	gdouble height;

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( instance ));

	height = ofa_irenderable_get_line_height( instance );
	ofa_irenderable_set_last_y( instance, top );

	for( it=list, first=TRUE ; it ; it=it->next ){
		if( first ){
			ofa_irenderable_set_text( instance,
						priv->body_begin_solde_rtab-priv->amount_width, top,
						title, PANGO_ALIGN_RIGHT );
			first = FALSE;
			}

		scur = ( sCurrency * ) it->data;
		g_return_if_fail( scur && OFO_IS_CURRENCY( scur->currency ));

		draw_amounts( instance, scur );

		top += height;
	}
}

static void
draw_amounts( ofaIRenderable *instance, sCurrency *scur )
{
	ofaAccountBalanceRenderPrivate *priv;
	gdouble y;
	gchar *str;
	ofxAmount amount;
	const gchar *sens;

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( instance ));

	y = ofa_irenderable_get_last_y( instance );

	amount = scur->begin_solde;
	if( amount < 0 ){
		amount *= -1.0;
	}
	str = ofa_amount_to_str( amount, scur->currency, priv->getter );
	ofa_irenderable_set_text( instance,
			priv->body_begin_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	sens = amount > 0 ? ( scur->begin_solde > 0 ? _( "CR" ) : _( "DB" )) : "";
	ofa_irenderable_set_text( instance,
			priv->body_begin_sens_ltab, y, sens, PANGO_ALIGN_LEFT );

	str = ofa_amount_to_str( scur->debits, scur->currency, priv->getter );
	ofa_irenderable_set_text( instance,
			priv->body_debit_period_rtab, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	str = ofa_amount_to_str( scur->credits, scur->currency, priv->getter );
	ofa_irenderable_set_text( instance,
			priv->body_credit_period_rtab, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	amount = scur->end_solde;
	if( amount < 0 ){
		amount *= -1.0;
	}
	str = ofa_amount_to_str( amount, scur->currency, priv->getter );
	ofa_irenderable_set_text( instance,
			priv->body_end_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	sens = amount > 0 ? ( scur->end_solde > 0 ? _( "CR" ) : _( "DB" )) : "";
	ofa_irenderable_set_text( instance,
			priv->body_end_sens_ltab, y, sens, PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			priv->body_currency_ltab, y, ofo_currency_get_code( scur->currency ), PANGO_ALIGN_LEFT );
}

static void
add_by_currency( ofaAccountBalanceRender *self, GList **list, sCurrency *in_scur )
{
	GList *it;
	sCurrency *gen_scur;
	const gchar *cur_code;
	gboolean found;

	found = FALSE;
	cur_code = ofo_currency_get_code( in_scur->currency );

	for( it=*list ; it ; it=it->next ){
		gen_scur = ( sCurrency * ) it->data;
		if( !my_collate( cur_code, ofo_currency_get_code( gen_scur->currency ))){
			found = TRUE;
			break;
		}
	}

	if( !found ){
		gen_scur = g_new0( sCurrency, 1 );
		gen_scur->currency = in_scur->currency;
		*list = g_list_insert_sorted( *list, gen_scur, ( GCompareFunc ) cmp_currencies );
		gen_scur->begin_solde = 0.0;
		gen_scur->debits = 0.0;
		gen_scur->credits = 0.0;
		gen_scur->end_solde = 0.0;
	}

	g_return_if_fail( gen_scur != NULL );

	gen_scur->begin_solde += in_scur->begin_solde;
	gen_scur->debits += in_scur->debits;
	gen_scur->credits += in_scur->credits;
	gen_scur->end_solde += in_scur->end_solde;
}

static gint
cmp_currencies( const sCurrency *a, const sCurrency *b )
{
	return( my_collate( ofo_currency_get_code( a->currency ), ofo_currency_get_code( b->currency )));
}

static void
free_currencies( GList **list )
{
	g_list_free_full( *list, ( GDestroyNotify ) g_free );
	*list = NULL;
}

/*
 * Search for a sAccount structure which would have been initialized
 * from get_dataset().
 * If not found, allocates a new one, and inserts it in the list
 */
static sAccount *
find_account( ofaAccountBalanceRender *self, ofoAccount *account )
{
	ofaAccountBalanceRenderPrivate *priv;
	sAccount *sacc;
	GList *it;
	const gchar *cur_code;
	gboolean found;

	priv = ofa_account_balance_render_get_instance_private( self );

	found = FALSE;
	for( it=priv->accounts ; it ; it=it->next ){
		sacc = ( sAccount * ) it->data;
		if( sacc->account == account ){
			found = TRUE;
			break;
		}
	}

	if( !found ){
		sacc = g_new0( sAccount, 1 );
		sacc->account = account;
		sacc->scur = g_new0( sCurrency, 1 );
		cur_code = ofo_account_get_currency( account );
		g_return_val_if_fail( my_strlen( cur_code ), NULL );
		sacc->scur->currency = ofo_currency_get_by_code( priv->getter, cur_code );
		g_return_val_if_fail( sacc->scur->currency && OFO_IS_CURRENCY( sacc->scur->currency ), NULL );
		sacc->scur->begin_solde = 0.0;
		sacc->scur->debits = 0.0;
		sacc->scur->credits = 0.0;
		sacc->scur->end_solde = 0.0;
		priv->accounts = g_list_insert_sorted( priv->accounts, sacc, ( GCompareFunc ) cmp_accounts );
	}

	return( sacc );
}

static gint
cmp_accounts( const sAccount *a, const sAccount *b )
{
	return( my_collate( ofo_account_get_number( a->account ), ofo_account_get_number( b->account )));
}

static void
free_accounts( GList **list )
{
	g_list_free_full( *list, ( GDestroyNotify ) free_account );
	*list = NULL;
}

static void
free_account( sAccount *sacc )
{
	g_free( sacc->scur );
}

/*
 * settings = paned_position;
 */
static void
read_settings( ofaAccountBalanceRender *self )
{
	ofaAccountBalanceRenderPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;
	GtkWidget *paned;
	gint pos;
	gchar *key;

	priv = ofa_account_balance_render_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, key );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	pos = my_strlen( cstr ) ? atoi( cstr ) : 0;
	if( pos < 150 ){
		pos = 150;
	}
	paned = ofa_render_page_get_top_paned( OFA_RENDER_PAGE( self ));
	gtk_paned_set_position( GTK_PANED( paned ), pos );

	my_isettings_free_string_list( settings, strlist );
	g_free( key );
}

static void
write_settings( ofaAccountBalanceRender *self )
{
	ofaAccountBalanceRenderPrivate *priv;
	myISettings *settings;
	GtkWidget *paned;
	gchar *str, *key;

	priv = ofa_account_balance_render_get_instance_private( self );

	paned = ofa_render_page_get_top_paned( OFA_RENDER_PAGE( self ));

	str = g_strdup_printf( "%d;",
			gtk_paned_get_position( GTK_PANED( paned )));

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( key );
	g_free( str );
}
