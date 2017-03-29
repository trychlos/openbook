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
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-igetter.h"
#include "api/ofa-irenderable.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofs-currency.h"

#include "core/ofa-iconcil.h"

#include "ui/ofa-iaccount-filter.h"
#include "ui/ofa-account-book-args.h"
#include "ui/ofa-account-book-render.h"

/* private instance data
 */
typedef struct {

	ofaAccountBookArgs *args_bin;

	/* runtime
	 */
	ofaIGetter        *getter;
	gchar             *settings_prefix;
	gchar             *from_account;
	gchar             *to_account;
	gboolean           all_accounts;
	GDate              from_date;
	GDate              to_date;
	gboolean           account_new_page;
	gboolean           class_new_page;
	gboolean           class_subtotal;
	guint              sort_ind;
	gint               count;					/* count of returned entries */

	/* print datas
	 */
	gdouble            render_width;
	gdouble            render_height;
	gdouble            page_margin;

	/* layout for account header line
	 */
	gdouble            acc_number_ltab;
	gdouble            acc_label_ltab;
	gdouble            acc_label_max_size;
	gdouble            acc_currency_ltab;

	/* layout for account footer line
	 */
	gdouble            acc_footer_max_size;

	/* layout for general balance
	 */
	gdouble            gen_balance_rtab;

	/* layout for entry line
	 */
	gdouble            body_dope_ltab;
	gdouble            body_deffect_ltab;
	gdouble            body_ledger_ltab;
	gdouble            body_ledger_max_size;
	gdouble            body_piece_ltab;
	gdouble            body_piece_max_size;
	gdouble            body_label_ltab;
	gint               body_label_max_size;		/* Pango units */
	gdouble            body_settlement_ctab;
	gdouble            body_reconcil_ctab;
	gdouble            body_debit_rtab;
	gdouble            body_credit_rtab;
	gdouble            body_solde_rtab;
	gdouble            body_solde_sens_rtab;

	/* for each account
	 */
	gchar             *account_number;
	ofxAmount          account_debit;
	ofxAmount          account_credit;
	ofoAccount        *account_object;
	gchar             *currency_code;
	gint               currency_digits;
	ofoCurrency       *currency_object;

	/* general summary
	 */
	GList             *class_totals;		/* total of debit/credit per currency for the current class*/
	GList             *gen_totals;			/* general total of debit/credit per currency */
}
	ofaAccountBookRenderPrivate;

/*
 * Accounts balances print uses a portrait orientation
 */
#define THIS_PAGE_ORIENTATION            GTK_PAGE_ORIENTATION_LANDSCAPE
#define THIS_PAPER_NAME                  GTK_PAPER_NAME_A4

static const gchar *st_page_header_title = N_( "General Books Summary" );

/* these are parms which describe the page layout
 */
static const gchar *st_title2_font       = "Sans Bold 8";
static const gchar *st_group_font        = "Sans Bold 6";
static const gchar *st_subtotal_font     = "Sans 6";
static const gchar *st_report_font       = "Sans 6";

static GtkWidget         *page_v_get_top_focusable_widget( const ofaPage *page );
static void               paned_page_v_init_view( ofaPanedPage *page );
static GtkWidget         *render_page_v_get_args_widget( ofaRenderPage *page );
static const gchar       *render_page_v_get_paper_name( ofaRenderPage *page );
static GtkPageOrientation render_page_v_get_page_orientation( ofaRenderPage *page );
static void               render_page_v_get_print_settings( ofaRenderPage *page, GKeyFile **keyfile, gchar **group_name );
static GList             *render_page_v_get_dataset( ofaRenderPage *page );
static gint               entry_cmp( ofoEntry *a, ofoEntry *b, ofaAccountBookRender *self );
static void               render_page_v_free_dataset( ofaRenderPage *page, GList *dataset );
static void               on_args_changed( ofaAccountBookArgs *bin, ofaAccountBookRender *page );
static void               irenderable_iface_init( ofaIRenderableInterface *iface );
static guint              irenderable_get_interface_version( const ofaIRenderable *instance );
static void               irenderable_begin_render( ofaIRenderable *instance );
static gchar             *irenderable_get_dossier_label( const ofaIRenderable *instance );
static void               irenderable_draw_page_header_title( ofaIRenderable *instance );
static void               irenderable_draw_header_column_names( ofaIRenderable *instance );
static gboolean           irenderable_is_new_group( const ofaIRenderable *instance, GList *prev, GList *line, ofeIRenderableBreak *sep );
static gboolean           is_new_class( const ofaIRenderable *instance, GList *prev, GList *line );
static void               irenderable_draw_group_header( ofaIRenderable *instance );
static void               irenderable_draw_top_report( ofaIRenderable *instance );
static void               irenderable_draw_line( ofaIRenderable *instance );
static void               irenderable_draw_bottom_report( ofaIRenderable *instance );
static void               irenderable_draw_group_footer( ofaIRenderable *instance );
static void               draw_group_footer( ofaIRenderable *instance );
static void               irenderable_draw_last_summary( ofaIRenderable *instance );
static void               irenderable_clear_runtime_data( ofaIRenderable *instance );
static void               clear_account_data( ofaAccountBookRender *self );
static void               draw_account_report( ofaAccountBookRender *self, gboolean with_solde );
static void               draw_account_solde_debit_credit( ofaAccountBookRender *self, gdouble y );
static void               draw_currencies_balance( ofaIRenderable *instance, const gchar *label, GList *currencies, gboolean bottom );
static void               read_settings( ofaAccountBookRender *self );
static void               write_settings( ofaAccountBookRender *self );

G_DEFINE_TYPE_EXTENDED( ofaAccountBookRender, ofa_account_book_render, OFA_TYPE_RENDER_PAGE, 0,
		G_ADD_PRIVATE( ofaAccountBookRender )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IRENDERABLE, irenderable_iface_init ))

static void
account_book_render_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_book_render_finalize";
	ofaAccountBookRenderPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_BOOK_RENDER( instance ));

	/* free data members here */
	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

	g_free( priv->settings_prefix );
	g_free( priv->from_account );
	g_free( priv->to_account );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_book_render_parent_class )->finalize( instance );
}

static void
account_book_render_dispose( GObject *instance )
{
	ofaAccountBookRenderPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_BOOK_RENDER( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		write_settings( OFA_ACCOUNT_BOOK_RENDER( instance ));

		/* unref object members here */
		priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

		ofs_currency_list_free( &priv->gen_totals );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_book_render_parent_class )->dispose( instance );
}

static void
ofa_account_book_render_init( ofaAccountBookRender *self )
{
	static const gchar *thisfn = "ofa_account_book_render_init";
	ofaAccountBookRenderPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ACCOUNT_BOOK_RENDER( self ));

	priv = ofa_account_book_render_get_instance_private( self );

	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_account_book_render_class_init( ofaAccountBookRenderClass *klass )
{
	static const gchar *thisfn = "ofa_account_book_render_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_book_render_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_book_render_finalize;

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
	static const gchar *thisfn = "ofa_account_book_render_paned_page_v_init_view";
	ofaAccountBookRenderPrivate *priv;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( page ));

	on_args_changed( priv->args_bin, OFA_ACCOUNT_BOOK_RENDER( page ));

	read_settings( OFA_ACCOUNT_BOOK_RENDER( page ));
}

static GtkWidget *
render_page_v_get_args_widget( ofaRenderPage *page )
{
	ofaAccountBookRenderPrivate *priv;
	ofaAccountBookArgs *bin;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( page ));

	priv->getter = ofa_page_get_getter( OFA_PAGE( page ));

	bin = ofa_account_book_args_new( priv->getter, priv->settings_prefix );
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
	ofaAccountBookRenderPrivate *priv;
	myISettings *settings;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( page ));

	settings = ofa_igetter_get_user_settings( priv->getter );
	*keyfile = my_isettings_get_keyfile( settings );
	*group_name = g_strdup_printf( "%s-print", priv->settings_prefix );
}

/*
 * ofaAccountBookArgs "ofa-changed" handler
 */
static void
on_args_changed( ofaAccountBookArgs *bin, ofaAccountBookRender *page )
{
	gboolean valid;
	gchar *message;

	valid = ofa_account_book_args_is_valid( bin, &message );
	ofa_render_page_set_args_changed( OFA_RENDER_PAGE( page ), valid, message );
	g_free( message );
}

static GList *
render_page_v_get_dataset( ofaRenderPage *page )
{
	ofaAccountBookRenderPrivate *priv;
	GList *dataset, *sorted, *it;
	ofaIAccountFilter *account_filter;
	ofaIDateFilter *date_filter;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( page ));

	g_free( priv->from_account );
	g_free( priv->to_account );
	account_filter = ofa_account_book_args_get_account_filter( priv->args_bin );
	priv->from_account = g_strdup( ofa_iaccount_filter_get_account( account_filter, IACCOUNT_FILTER_FROM ));
	priv->to_account = g_strdup( ofa_iaccount_filter_get_account( account_filter, IACCOUNT_FILTER_TO ));
	priv->all_accounts = ofa_iaccount_filter_get_all_accounts( account_filter );

	date_filter = ofa_account_book_args_get_date_filter( priv->args_bin );
	my_date_set_from_date( &priv->from_date, ofa_idate_filter_get_date( date_filter, IDATE_FILTER_FROM ));
	my_date_set_from_date( &priv->to_date, ofa_idate_filter_get_date( date_filter, IDATE_FILTER_TO ));

	dataset = ofo_entry_get_dataset_for_print_by_account(
						priv->getter,
						priv->all_accounts ? NULL : priv->from_account,
						priv->all_accounts ? NULL : priv->to_account,
						my_date_is_valid( &priv->from_date ) ? &priv->from_date : NULL,
						my_date_is_valid( &priv->to_date ) ? &priv->to_date : NULL );

	priv->count = g_list_length( dataset );

	priv->account_new_page = ofa_account_book_args_get_new_page_per_account( priv->args_bin );
	priv->class_new_page = ofa_account_book_args_get_new_page_per_class( priv->args_bin );
	priv->class_subtotal = ofa_account_book_args_get_subtotal_per_class( priv->args_bin );

	priv->sort_ind = ofa_account_book_args_get_sort_ind( priv->args_bin );
	sorted = NULL;
	for( it=dataset ; it ; it=it->next ){
		sorted = g_list_insert_sorted_with_data( sorted, it->data, ( GCompareDataFunc ) entry_cmp, page );
	}
	g_list_free( dataset );

	return( sorted );
}

static gint
entry_cmp( ofoEntry *a, ofoEntry *b, ofaAccountBookRender *self )
{
	ofaAccountBookRenderPrivate *priv;
	gint cmp;
	ofxCounter numa, numb;

	priv = ofa_account_book_render_get_instance_private( self );

	/* first sort by account number */
	cmp = my_collate( ofo_entry_get_account( a ), ofo_entry_get_account( b ));

	/* inside of an account, default is to sort by operation date
	 * when the dates are equal, sort with the other date
	 * last sort by entry number
	 * (so that the sort is stable!)
	 */
	if( cmp == 0 ){
		numa = ofo_entry_get_number( a );
		numb = ofo_entry_get_number( b );

		if( priv->sort_ind == ARG_SORT_DEFFECT ){
			cmp = my_date_compare( ofo_entry_get_deffect( a ), ofo_entry_get_deffect( b ));
			if( cmp == 0 ){
				cmp = my_date_compare( ofo_entry_get_dope( a ), ofo_entry_get_dope( b ));
				if( cmp == 0 ){
					cmp = ( numa < numb ? -1 : ( numa > numb ? 1 : 0 ));
				}
			}

		} else {
			cmp = my_date_compare( ofo_entry_get_dope( a ), ofo_entry_get_dope( b ));
			if( cmp == 0 ){
				cmp = my_date_compare( ofo_entry_get_deffect( a ), ofo_entry_get_deffect( b ));
				if( cmp == 0 ){
					cmp = ( numa < numb ? -1 : ( numa > numb ? 1 : 0 ));
				}
			}
		}
	}

	return( cmp );
}

static void
render_page_v_free_dataset( ofaRenderPage *page, GList *dataset )
{
	g_list_free_full( dataset, ( GDestroyNotify ) g_object_unref );
}

static void
irenderable_iface_init( ofaIRenderableInterface *iface )
{
	static const gchar *thisfn = "ofa_account_book_render_irenderable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = irenderable_get_interface_version;
	iface->begin_render = irenderable_begin_render;
	iface->get_dossier_label = irenderable_get_dossier_label;
	iface->draw_page_header_title = irenderable_draw_page_header_title;
	iface->draw_header_column_names = irenderable_draw_header_column_names;
	iface->is_new_group = irenderable_is_new_group;
	iface->draw_group_header = irenderable_draw_group_header;
	iface->draw_top_report = irenderable_draw_top_report;
	iface->draw_line = irenderable_draw_line;
	iface->draw_bottom_report = irenderable_draw_bottom_report;
	iface->draw_group_footer = irenderable_draw_group_footer;
	iface->draw_last_summary = irenderable_draw_last_summary;
	iface->clear_runtime_data = irenderable_clear_runtime_data;
}

static guint
irenderable_get_interface_version( const ofaIRenderable *instance )
{
	return( 1 );
}

/*
 * mainly here: compute the tab positions
 */
static void
irenderable_begin_render( ofaIRenderable *instance )
{
	static const gchar *thisfn = "ofa_account_book_render_irenderable_begin_render";
	ofaAccountBookRenderPrivate *priv;
	gdouble date_width, ledger_width, piece_width, amount_width, sens_width, char_width;
	gdouble spacing;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv->render_width = ofa_irenderable_get_render_width( instance );
	priv->render_height = ofa_irenderable_get_render_height( instance );
	priv->page_margin = ofa_irenderable_get_page_margin( instance );

	/* compute the width of the columns with the body font */
	ofa_irenderable_set_font( instance, ofa_irenderable_get_body_font( instance ));
	date_width = ofa_irenderable_get_text_width( instance, "9999-99-99-" );
	ledger_width = ofa_irenderable_get_text_width( instance, "XXXXXXXX" );
	piece_width = ofa_irenderable_get_text_width( instance, "XX 99999999" );
	char_width = ofa_irenderable_get_text_width( instance, "X" );

	/* the width of the currency code should use the group font */
	ofa_irenderable_set_font( instance, ofa_irenderable_get_group_font( instance, 0 ));
	sens_width = ofa_irenderable_get_text_width( instance, "XX" );

	/* the width of the amounts should use the last summary font */
	ofa_irenderable_set_font( instance, ofa_irenderable_get_summary_font( instance, 0 ));
	amount_width = ofa_irenderable_get_text_width( instance, "9,999,999,999.99" );

	spacing = ofa_irenderable_get_columns_spacing( instance );

	/* entry line, starting from the left */
	priv->body_dope_ltab = priv->page_margin;
	priv->body_deffect_ltab = priv->body_dope_ltab + date_width + spacing;
	priv->body_ledger_ltab = priv->body_deffect_ltab + date_width + spacing;
	priv->body_piece_ltab = priv->body_ledger_ltab + ledger_width + spacing;
	priv->body_label_ltab = priv->body_piece_ltab + piece_width + spacing;

	/* entry line, starting from the right */
	priv->body_solde_sens_rtab = priv->render_width - priv->page_margin;
	priv->body_solde_rtab = priv->body_solde_sens_rtab - sens_width - spacing / 2.0;
	priv->body_credit_rtab = priv->body_solde_rtab - amount_width - spacing;
	priv->body_debit_rtab = priv->body_credit_rtab - amount_width - spacing;
	priv->body_reconcil_ctab = priv->body_debit_rtab - amount_width - spacing - char_width / 2.0;
	priv->body_settlement_ctab = priv->body_reconcil_ctab - spacing - char_width;

	/* account header, starting from the left
	 * computed here because aligned on (and so relying on) body effect date */
	priv->acc_number_ltab = priv->page_margin;
	priv->acc_label_ltab = priv->body_deffect_ltab;

	/* account footer and last summary have a currency code
	 * left align on settlement indicator */
	priv->acc_currency_ltab = priv->body_settlement_ctab - char_width/2.0;

	/* general balance */
	priv->gen_balance_rtab = priv->acc_currency_ltab - spacing;

	/* max size in Pango units */
	priv->acc_label_max_size = ( priv->acc_currency_ltab - spacing - priv->acc_label_ltab )*PANGO_SCALE;
	priv->acc_footer_max_size = ( priv->acc_currency_ltab - spacing - priv->page_margin )*PANGO_SCALE;
	priv->body_ledger_max_size = ledger_width*PANGO_SCALE;
	priv->body_piece_max_size = piece_width*PANGO_SCALE;
	priv->body_label_max_size = ( priv->body_settlement_ctab - spacing - priv->body_label_ltab )*PANGO_SCALE;
}

static gchar *
irenderable_get_dossier_label( const ofaIRenderable *instance )
{
	ofaAccountBookRenderPrivate *priv;
	ofaHub *hub;
	ofoDossier *dossier;
	const gchar *label;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

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
	ofaAccountBookRenderPrivate *priv;
	gchar *sfrom_date, *sto_date;
	GString *stitle;
	gdouble y, r, g, b, height;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

	ofa_irenderable_get_title_color( instance, &r, &g, &b );
	ofa_irenderable_set_color( instance, r, g, b );
	y = ofa_irenderable_get_last_y( instance );

	/* line 1 general books summary */
	ofa_irenderable_set_font( instance,
			ofa_irenderable_get_title_font( instance, ofa_irenderable_get_current_page_num( instance )));
	height = ofa_irenderable_set_text( instance, priv->render_width/2, y, st_page_header_title, PANGO_ALIGN_CENTER );
	y += height;

	/* line 2 - Account from xxx to xxx - Date from xxx to xxx
	 * recall of account and date selections in line 4
	 */
	stitle = g_string_new( "" );
	if( priv->all_accounts ||
			( !my_strlen( priv->from_account ) && !my_strlen( priv->to_account ))){
		g_string_printf( stitle, _( "All accounts" ));
	} else {
		if( my_strlen( priv->from_account )){
			g_string_printf( stitle, _( "From account %s" ), priv->from_account );
			if( my_strlen( priv->to_account )){
				g_string_append_printf( stitle, " to account %s", priv->to_account );
			}
		} else {
			g_string_printf( stitle, _( "Up to account %s" ), priv->to_account );
		}
	}

	stitle = g_string_append( stitle, " - " );

	if( !my_date_is_valid( &priv->from_date ) && !my_date_is_valid( &priv->to_date )){
		stitle = g_string_append( stitle, "All effect dates" );
	} else {
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
	}

	ofa_irenderable_set_font( instance, st_title2_font );
	height = ofa_irenderable_set_text( instance, priv->render_width/2, y, stitle->str, PANGO_ALIGN_CENTER );
	g_string_free( stitle, TRUE );
	y += height;

	ofa_irenderable_set_last_y( instance, y );
}

static void
irenderable_draw_header_column_names( ofaIRenderable *instance )
{
	ofaAccountBookRenderPrivate *priv;
	static gdouble st_vspace_rate = 0.5;
	gdouble y, text_height, vspace;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

	y = ofa_irenderable_get_last_y( instance );
	text_height = ofa_irenderable_get_text_height( instance );
	vspace = text_height * st_vspace_rate;
	y+= vspace;

	/* column headers */
	ofa_irenderable_set_text( instance,
			priv->body_dope_ltab, y,
			_( "Operation" ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			priv->body_deffect_ltab, y,
			_( "Effect" ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			priv->body_ledger_ltab, y,
			_( "Ledger" ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			priv->body_piece_ltab, y,
			_( "Piece" ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			priv->body_label_ltab, y,
			_( "Label" ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			(priv->body_settlement_ctab+priv->body_reconcil_ctab)/2, y,
			_( "Set./Rec." ), PANGO_ALIGN_CENTER );

	ofa_irenderable_set_text( instance,
			priv->body_debit_rtab, y,
			_( "Debit" ), PANGO_ALIGN_RIGHT );

	ofa_irenderable_set_text( instance,
			priv->body_credit_rtab, y,
			_( "Credit" ), PANGO_ALIGN_RIGHT );

	ofa_irenderable_set_text( instance,
			priv->body_solde_sens_rtab, y,
			_( "Entries solde" ), PANGO_ALIGN_RIGHT );

	/* this set the 'y' height just after the column headers */
	y += text_height * ( 1+st_vspace_rate );
	ofa_irenderable_set_last_y( instance, y );
}

/*
 * just test if the current entry is on the same account than the
 * previous one, or in the same class
 */
static gboolean
irenderable_is_new_group( const ofaIRenderable *instance, GList *prev, GList *line, ofeIRenderableBreak *sep )
{
	ofaAccountBookRenderPrivate *priv;
	ofoEntry *current_entry, *prev_entry;
	const gchar *acc_prev, *acc_line;
	gint cmp;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

	*sep = IRENDERABLE_BREAK_NONE;

	if( !prev || !line ){
		return( TRUE );
	}

	current_entry = OFO_ENTRY( line->data );
	acc_line = ofo_entry_get_account( current_entry );

	prev_entry = OFO_ENTRY( prev->data );
	acc_prev = ofo_entry_get_account( prev_entry );

	cmp = my_collate( acc_prev, acc_line );

	if( cmp != 0 ){

		if( priv->account_new_page ){
			*sep = IRENDERABLE_BREAK_NEW_PAGE;

		} else if( is_new_class( instance, prev, line )){
			if( priv->class_new_page ){
				*sep = IRENDERABLE_BREAK_NEW_PAGE;
			} else {
				*sep = IRENDERABLE_BREAK_BLANK_LINE;
			}

		} else {
			*sep = IRENDERABLE_BREAK_SEP_LINE;
		}
	}

	return( cmp != 0 );
}

static gboolean
is_new_class( const ofaIRenderable *instance, GList *prev, GList *line )
{
	const gchar *prev_account, *line_account;
	gint prev_class, line_class;

	if( !prev || !line ){
		return( TRUE );
	}

	prev_account = ofo_entry_get_account( OFO_ENTRY( prev->data ));
	prev_class = ofo_account_get_class_from_number( prev_account );

	line_account = ofo_entry_get_account( OFO_ENTRY( line->data ));
	line_class = ofo_account_get_class_from_number( line_account );

	return( prev_class != line_class );
}

/*
 * draw account header
 */
static void
irenderable_draw_group_header( ofaIRenderable *instance )
{
	ofaAccountBookRenderPrivate *priv;
	GList *line;
	gdouble y, height;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

	line = ofa_irenderable_get_current_line( instance );
	if( line ){

		/* setup the account properties */
		g_free( priv->account_number );
		priv->account_number = g_strdup( ofo_entry_get_account( OFO_ENTRY( line->data )));

		priv->account_debit = 0;
		priv->account_credit = 0;

		priv->account_object = ofo_account_get_by_number( priv->getter, priv->account_number );
		g_return_if_fail( priv->account_object && OFO_IS_ACCOUNT( priv->account_object ));

		g_free( priv->currency_code );
		priv->currency_code = g_strdup( ofo_account_get_currency( priv->account_object ));

		priv->currency_object = ofo_currency_get_by_code( priv->getter, priv->currency_code );
		g_return_if_fail( priv->currency_object && OFO_IS_CURRENCY( priv->currency_object ));

		priv->currency_digits = ofo_currency_get_digits( priv->currency_object );

		ofa_irenderable_set_font( instance, st_group_font );
		y = ofa_irenderable_get_last_y( instance );
		height = ofa_irenderable_get_line_height( instance );

		/* display the account header */
		/* account number */
		ofa_irenderable_set_text( instance,
				priv->acc_number_ltab, y,
				ofo_account_get_number( priv->account_object ), PANGO_ALIGN_LEFT );

		/* account label */
		ofa_irenderable_ellipsize_text( instance,
				priv->acc_label_ltab, y,
				ofo_account_get_label( priv->account_object ), priv->acc_label_max_size );

		/* account currency */
		ofa_irenderable_set_text( instance,
				priv->acc_currency_ltab, y,
				ofo_account_get_currency( priv->account_object ), PANGO_ALIGN_LEFT );

		y += height;
		ofa_irenderable_set_last_y( instance, y );
	}
}

/*
 * only draw a top report if no break between @prev and @line
 */
static void
irenderable_draw_top_report( ofaIRenderable *instance )
{
	ofeIRenderableBreak sep;
	GList *line;

	line = ofa_irenderable_get_current_line( instance );

	if( line && !irenderable_is_new_group( instance, line->prev, line, &sep )){
		draw_account_report( OFA_ACCOUNT_BOOK_RENDER( instance ), TRUE );
	}
}

/*
 * each line update the account sum of debits and credits
 * the total of debits/credits for this currency is incremented in
 * group footer
 */
static void
irenderable_draw_line( ofaIRenderable *instance )
{
	ofaAccountBookRenderPrivate *priv;
	GList *line;
	ofoEntry *entry;
	const gchar *cstr;
	gchar *str;
	gdouble y;
	ofxAmount amount;
	ofoConcil *concil;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

	line = ofa_irenderable_get_current_line( instance );
	if( line ){

		y = ofa_irenderable_get_last_y( instance );
		entry = OFO_ENTRY( line->data );

		/* operation date */
		str = my_date_to_str( ofo_entry_get_dope( entry ), ofa_prefs_date_display( priv->getter ));
		ofa_irenderable_set_text( instance,
				priv->body_dope_ltab, y, str, PANGO_ALIGN_LEFT );
		g_free( str );

		/* effect date */
		str = my_date_to_str( ofo_entry_get_deffect( entry ), ofa_prefs_date_display( priv->getter ));
		ofa_irenderable_set_text( instance,
				priv->body_deffect_ltab, y, str, PANGO_ALIGN_LEFT );
		g_free( str );

		/* ledger */
		ofa_irenderable_ellipsize_text( instance,
				priv->body_ledger_ltab, y, ofo_entry_get_ledger( entry ), priv->body_ledger_max_size );

		/* piece */
		cstr = ofo_entry_get_ref( entry );
		if( my_strlen( cstr )){
			ofa_irenderable_ellipsize_text( instance,
					priv->body_piece_ltab, y, cstr, priv->body_piece_max_size );
		}

		/* label */
		ofa_irenderable_ellipsize_text( instance,
				priv->body_label_ltab, y,
				ofo_entry_get_label( entry ), priv->body_label_max_size );

		/* settlement ? */
		if( ofo_entry_get_settlement_number( entry ) > 0 ){
			ofa_irenderable_set_text( instance,
					priv->body_settlement_ctab, y, _( "S" ), PANGO_ALIGN_CENTER );
		}

		/* reconciliation */
		concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ));
		if( concil ){
			ofa_irenderable_set_text( instance,
					priv->body_reconcil_ctab, y, _( "R" ), PANGO_ALIGN_CENTER );
		}

		/* debit */
		amount = ofo_entry_get_debit( entry );
		if( amount ){
			str = ofa_amount_to_str( amount, priv->currency_object, priv->getter );
			ofa_irenderable_set_text( instance,
					priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
			g_free( str );
			priv->account_debit += amount;
		}

		/* credit */
		amount = ofo_entry_get_credit( entry );
		if( amount ){
			str = ofa_amount_to_str( amount, priv->currency_object, priv->getter );
			ofa_irenderable_set_text( instance,
					priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
			g_free( str );
			priv->account_credit += amount;
		}

		/* current account solde */
		draw_account_solde_debit_credit( OFA_ACCOUNT_BOOK_RENDER( instance ), y );
	}
}

static void
irenderable_draw_bottom_report( ofaIRenderable *instance )
{
	draw_account_report( OFA_ACCOUNT_BOOK_RENDER( instance ), FALSE );
}

/*
 * This function is called many times in order to auto-detect the
 * height of the group footer (in particular each time the
 * #ofa_irenderable_draw_line() function needs to know if there is
 * enough vertical space left to draw the current line)
 * so take care of:
 * - no account has been yet identified on first call
 * - currency has yet to be defined even during pagination phase
 *   in order to be able to detect the heigtht of the summary
 */
static void
irenderable_draw_group_footer( ofaIRenderable *instance )
{
	static const gchar *thisfn = "ofa_account_book_render_irenderable_draw_group_footer";
	ofaAccountBookRenderPrivate *priv;
	GList *line;
	gboolean is_paginating;
	gchar *str;
	gdouble r, g, b;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

	if( 0 ){
		g_debug( "%s: account_number=%s, paginating=%s",
				thisfn, priv->account_number, ofa_irenderable_is_paginating( instance ) ? "True":"False" );
	}

	line = ofa_irenderable_get_current_line( instance );

	if( line && priv->account_number ){

		draw_group_footer( instance );

		/* add the account balance to the totals per currency */
		is_paginating = ofa_irenderable_is_paginating( instance );

		ofs_currency_add_by_object( &priv->class_totals, priv->currency_object,
				is_paginating ? 0 : priv->account_debit,
				is_paginating ? 0 : priv->account_credit );

		ofs_currency_add_by_object( &priv->gen_totals, priv->currency_object,
				is_paginating ? 0 : priv->account_debit,
				is_paginating ? 0 : priv->account_credit );

		/* print subtotal by class and currency if requested for
		 */
		if( priv->class_subtotal && is_new_class( instance, line, line->next )){
			ofa_irenderable_get_dossier_color( instance, &r, &g, &b );
			ofa_irenderable_set_color( instance, r, g, b );
			ofa_irenderable_set_font( instance, st_subtotal_font );
			str = g_strdup_printf( _( "Class %d balance : " ), ofo_account_get_class( priv->account_object ));
			draw_currencies_balance( instance, str, priv->class_totals, FALSE );
			g_free( str );

			if( !is_paginating ){
				ofs_currency_list_free( &priv->class_totals );
			}
		}

		if( !is_paginating ){
			clear_account_data( OFA_ACCOUNT_BOOK_RENDER( instance ));
		}
	}
}

static void
draw_group_footer( ofaIRenderable *instance )
{
	ofaAccountBookRenderPrivate *priv;
	gchar *str;
	gdouble y, height;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

	ofa_irenderable_set_font( instance, st_group_font );
	height = ofa_irenderable_get_line_height( instance );
	y = ofa_irenderable_get_last_y( instance );

	/* label */
	str = g_strdup_printf( _( "Balance for account %s - %s" ),
			priv->account_number,
			ofo_account_get_label( priv->account_object ));
	ofa_irenderable_ellipsize_text( instance,
			priv->page_margin, y, str, priv->acc_footer_max_size );
	g_free( str );

	/* currency */
	ofa_irenderable_set_text( instance,
			priv->acc_currency_ltab, y, priv->currency_code, PANGO_ALIGN_LEFT );

	/* solde debit */
	str = ofa_amount_to_str( priv->account_debit, priv->currency_object, priv->getter );
	ofa_irenderable_set_text( instance,
			priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	/* solde credit */
	str = ofa_amount_to_str( priv->account_credit, priv->currency_object, priv->getter );
	ofa_irenderable_set_text( instance,
			priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	/* current account solde */
	draw_account_solde_debit_credit( OFA_ACCOUNT_BOOK_RENDER( instance ), y );

	y += height;
	ofa_irenderable_set_last_y( instance, y );
}

/*
 * print a line per found currency at the end of the printing
 */
static void
irenderable_draw_last_summary( ofaIRenderable *instance )
{
	ofaAccountBookRenderPrivate *priv;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

	if( !priv->count ){
		ofa_irenderable_draw_no_data( instance );
		return;
	}

	draw_currencies_balance( instance, _( "General balance : " ), priv->gen_totals, TRUE );
}

static void
irenderable_clear_runtime_data( ofaIRenderable *instance )
{
	ofaAccountBookRenderPrivate *priv;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

	ofs_currency_list_free( &priv->class_totals );
	ofs_currency_list_free( &priv->gen_totals );

	clear_account_data( OFA_ACCOUNT_BOOK_RENDER( instance ));
}

static void
clear_account_data( ofaAccountBookRender *self )
{
	ofaAccountBookRenderPrivate *priv;

	priv = ofa_account_book_render_get_instance_private( self );

	g_free( priv->account_number );
	priv->account_number = NULL;
}

/*
 * draw total of debit and credits for this account
 * current balance is not printed on the bottom report (because already
 * appears on the immediate previous line)
 */
static void
draw_account_report( ofaAccountBookRender *self, gboolean with_solde )
{
	ofaAccountBookRenderPrivate *priv;
	gchar *str;
	gdouble y, height;

	priv = ofa_account_book_render_get_instance_private( self );

	if( priv->account_object ){

		ofa_irenderable_set_font( OFA_IRENDERABLE( self ), st_report_font );
		height = ofa_irenderable_get_line_height( OFA_IRENDERABLE( self ));
		y = ofa_irenderable_get_last_y( OFA_IRENDERABLE( self ));

		/* account number */
		ofa_irenderable_set_text( OFA_IRENDERABLE( self ),
				priv->acc_number_ltab, y,
				ofo_account_get_number( priv->account_object ), PANGO_ALIGN_LEFT );

		/* account label */
		ofa_irenderable_ellipsize_text( OFA_IRENDERABLE( self ),
				priv->acc_label_ltab, y,
				ofo_account_get_label( priv->account_object ), priv->acc_label_max_size );

		/* account currency */
		ofa_irenderable_set_text( OFA_IRENDERABLE( self ),
				priv->acc_currency_ltab, y,
				ofo_account_get_currency( priv->account_object ), PANGO_ALIGN_LEFT );

		/* current account balance */
		str = ofa_amount_to_str( priv->account_debit, priv->currency_object, priv->getter );
		ofa_irenderable_set_text( OFA_IRENDERABLE( self ),
				priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = ofa_amount_to_str( priv->account_credit, priv->currency_object, priv->getter );
		ofa_irenderable_set_text( OFA_IRENDERABLE( self ),
				priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		/* current account solde */
		if( with_solde ){
			draw_account_solde_debit_credit( self, y );
		}

		y += height;
		ofa_irenderable_set_last_y( OFA_IRENDERABLE( self ), y );
	}
}

static void
draw_account_solde_debit_credit( ofaAccountBookRender *self, gdouble y )
{
	ofaAccountBookRenderPrivate *priv;
	ofxAmount amount;
	gchar *str;

	priv = ofa_account_book_render_get_instance_private( self );

	/* current account balance
	 * if current balance is zero, then also print it */
	amount = priv->account_credit - priv->account_debit;
	if( amount >= 0 ){
		str = ofa_amount_to_str( amount, priv->currency_object, priv->getter );
		ofa_irenderable_set_text( OFA_IRENDERABLE( self ),
				priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		ofa_irenderable_set_text( OFA_IRENDERABLE( self ),
				priv->body_solde_sens_rtab, y, _( "CR" ), PANGO_ALIGN_RIGHT );

	} else {
		str = ofa_amount_to_str( -1*amount, priv->currency_object, priv->getter );
		ofa_irenderable_set_text( OFA_IRENDERABLE( self ),
				priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		ofa_irenderable_set_text( OFA_IRENDERABLE( self ),
				priv->body_solde_sens_rtab, y, _( "DB" ), PANGO_ALIGN_RIGHT );
	}
}

/*
 * Draw the list of currencies in the current font
 * at current y (bottom=False) or at the bottom of the page
 */
static void
draw_currencies_balance( ofaIRenderable *instance, const gchar *label, GList *currencies, gboolean bottom )
{
	ofaAccountBookRenderPrivate *priv;
	gboolean first;
	GList *it;
	ofsCurrency *scur;
	gdouble height, vspace, req_height, top;
	gchar *str;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

	/* top of the rectangle */
	height = ofa_irenderable_get_text_height( instance );
	vspace = height * ofa_irenderable_get_body_vspace_rate( instance );
	req_height = g_list_length( currencies ) * height
			+ ( 1+g_list_length( currencies )) * vspace;

	if( bottom ){
		bottom = ofa_irenderable_get_max_y( instance );
		top = bottom - req_height;
	} else {
		top = ofa_irenderable_get_last_y( instance );
	}

	ofa_irenderable_draw_rect( instance, 0, top, -1, req_height );
	top += vspace;

	for( it=currencies, first=TRUE ; it ; it=it->next ){
		scur = ( ofsCurrency * ) it->data;

		if( first ){
			ofa_irenderable_set_text( instance,
					priv->gen_balance_rtab, top,
					label, PANGO_ALIGN_RIGHT );
			first = FALSE;
		}

		ofa_irenderable_set_text( instance,
				priv->acc_currency_ltab, top,
				ofo_currency_get_code( scur->currency ), PANGO_ALIGN_LEFT );

		str = ofa_amount_to_str( scur->debit, scur->currency, priv->getter );
		ofa_irenderable_set_text( instance,
				priv->body_debit_rtab, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = ofa_amount_to_str( scur->credit, scur->currency, priv->getter );
		ofa_irenderable_set_text( instance,
				priv->body_credit_rtab, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		top += height+vspace;
	}

	ofa_irenderable_set_last_y( instance, ofa_irenderable_get_last_y( instance ) + req_height );
}

/*
 * settings = paned_position;
 */
static void
read_settings( ofaAccountBookRender *self )
{
	ofaAccountBookRenderPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;
	GtkWidget *paned;
	gint pos;
	gchar *key;

	priv = ofa_account_book_render_get_instance_private( self );

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
write_settings( ofaAccountBookRender *self )
{
	ofaAccountBookRenderPrivate *priv;
	myISettings *settings;
	GtkWidget *paned;
	gchar *str, *key;

	priv = ofa_account_book_render_get_instance_private( self );

	paned = ofa_render_page_get_top_paned( OFA_RENDER_PAGE( self ));

	str = g_strdup_printf( "%d;",
			gtk_paned_get_position( GTK_PANED( paned )));

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( key );
	g_free( str );
}
