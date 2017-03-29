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
#include "api/ofo-class.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofs-account-balance.h"

#include "ui/ofa-iaccount-filter.h"
#include "ui/ofa-balance-args.h"
#include "ui/ofa-balance-render.h"

/* private instance data
 */
typedef struct {

	ofaBalanceArgs *args_bin;

	/* runtime
	 */
	ofaIGetter    *getter;
	gchar         *settings_prefix;
	gchar         *from_account;
	gchar         *to_account;
	gboolean       all_accounts;
	GDate          from_date;
	GDate          to_date;
	gboolean       accounts_balance;
	gboolean       class_subtotal;
	gboolean       class_break;
	gint           count;				/* count of returned entries */

	/* print datas
	 */
	gdouble        render_width;
	gdouble        render_height;
	gdouble        page_margin;
	gdouble        amount_width;
	gdouble        body_number_ltab;
	gint           body_number_max_size;		/* Pango units */
	gdouble        body_label_ltab;
	gint           body_label_max_size;			/* Pango units */
	gdouble        body_debit_period_rtab;
	gdouble        body_credit_period_rtab;
	gdouble        body_debit_solde_rtab;
	gdouble        body_credit_solde_rtab;
	gdouble        body_currency_ltab;

	/* subtotal per class
	 */
	gint           class_num;
	ofoClass      *class_object;
	GList         *class_totals;			/* class_totals per currency for this class */
	GList         *gen_totals;
}
	ofaBalanceRenderPrivate;

typedef struct {
	gchar  *currency;
	gdouble period_d;
	gdouble period_c;
	gdouble solde_d;
	gdouble solde_c;
}
	sCurrency;

/*
 * Accounts balances print uses a portrait orientation
 */
#define THIS_PAGE_ORIENTATION            GTK_PAGE_ORIENTATION_PORTRAIT
#define THIS_PAPER_NAME                  GTK_PAPER_NAME_A4

static const gchar *st_page_header_title_entries  = N_( "Entries Balance Summary" );
static const gchar *st_page_header_title_accounts = N_( "Accounts Balance Summary" );

/* these are parms which describe the page layout
 */

static const gchar *st_title2_font       = "Sans Bold 8";
static const gchar *st_summary_font      = "Sans Bold 6";
static const gchar *st_notes_font        = "Sans Italic 5";

#define COLOR_BLACK                      0,      0,      0
#define COLOR_WHITE                      1,      1,      1

static GtkWidget         *page_v_get_top_focusable_widget( const ofaPage *page );
static void               paned_page_v_init_view( ofaPanedPage *page );
static GtkWidget         *render_page_v_get_args_widget( ofaRenderPage *page );
static const gchar       *render_page_v_get_paper_name( ofaRenderPage *page );
static GtkPageOrientation render_page_v_get_page_orientation( ofaRenderPage *page );
static void               render_page_v_get_print_settings( ofaRenderPage *page, GKeyFile **keyfile, gchar **group_name );
static GList             *render_page_v_get_dataset( ofaRenderPage *page );
static void               render_page_v_free_dataset( ofaRenderPage *page, GList *dataset );
static void               on_args_changed( ofaBalanceArgs *bin, ofaBalanceRender *page );
static void               irenderable_iface_init( ofaIRenderableInterface *iface );
static guint              irenderable_get_interface_version( const ofaIRenderable *instance );
static void               irenderable_begin_render( ofaIRenderable *instance );
static gchar             *irenderable_get_dossier_label( const ofaIRenderable *instance );
static void               irenderable_draw_page_header_title( ofaIRenderable *instance );
static void               irenderable_draw_page_header_notes( ofaIRenderable *instance );
static void               irenderable_draw_header_column_names( ofaIRenderable *instance );
static gboolean           irenderable_is_new_group( const ofaIRenderable *instance, GList *prev, GList *line, ofeIRenderableBreak *sep );
static void               irenderable_draw_group_header( ofaIRenderable *instance );
static void               irenderable_draw_top_report( ofaIRenderable *instance );
static void               irenderable_draw_line( ofaIRenderable *instance );
static void               irenderable_draw_bottom_report( ofaIRenderable *instance );
static void               irenderable_draw_group_footer( ofaIRenderable *instance );
static void               irenderable_draw_last_summary( ofaIRenderable *instance );
static const gchar       *irenderable_get_summary_font( const ofaIRenderable *instance, guint page_num );
static void               irenderable_clear_runtime_data( ofaIRenderable *instance );
static void               draw_subtotals_balance( ofaIRenderable *instance, const gchar *title );
static void               draw_account_balance( ofaIRenderable *instance, GList *list, gdouble top, const gchar *title );
static GList             *add_account_balance( ofaBalanceRender *self, GList *list, const gchar *currency, gdouble solde, ofsAccountBalance *sbal );
static gint               cmp_currencies( const sCurrency *a, const sCurrency *b );
static void               free_currencies( GList **list );
static void               free_currency( sCurrency *scur );
static void               read_settings( ofaBalanceRender *self );
static void               write_settings( ofaBalanceRender *self );

G_DEFINE_TYPE_EXTENDED( ofaBalanceRender, ofa_balance_render, OFA_TYPE_RENDER_PAGE, 0,
		G_ADD_PRIVATE( ofaBalanceRender )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IRENDERABLE, irenderable_iface_init ))

static void
balance_render_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_balance_render_finalize";
	ofaBalanceRenderPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BALANCE_RENDER( instance ));

	/* free data members here */
	priv = ofa_balance_render_get_instance_private( OFA_BALANCE_RENDER( instance ));

	g_free( priv->settings_prefix );
	g_free( priv->from_account );
	g_free( priv->to_account );

	free_currencies( &priv->class_totals );
	free_currencies( &priv->gen_totals );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_balance_render_parent_class )->finalize( instance );
}

static void
balance_render_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_BALANCE_RENDER( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		write_settings( OFA_BALANCE_RENDER( instance ));

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_balance_render_parent_class )->dispose( instance );
}

static void
ofa_balance_render_init( ofaBalanceRender *self )
{
	static const gchar *thisfn = "ofa_balance_render_init";
	ofaBalanceRenderPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BALANCE_RENDER( self ));

	priv = ofa_balance_render_get_instance_private( self );

	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_balance_render_class_init( ofaBalanceRenderClass *klass )
{
	static const gchar *thisfn = "ofa_balance_render_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = balance_render_dispose;
	G_OBJECT_CLASS( klass )->finalize = balance_render_finalize;

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
	static const gchar *thisfn = "ofa_balance_render_paned_page_v_init_view";
	ofaBalanceRenderPrivate *priv;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_balance_render_get_instance_private( OFA_BALANCE_RENDER( page ));

	on_args_changed( priv->args_bin, OFA_BALANCE_RENDER( page ));

	read_settings( OFA_BALANCE_RENDER( page ));
}

static GtkWidget *
render_page_v_get_args_widget( ofaRenderPage *page )
{
	ofaBalanceRenderPrivate *priv;
	ofaBalanceArgs *bin;

	priv = ofa_balance_render_get_instance_private( OFA_BALANCE_RENDER( page ));

	priv->getter = ofa_page_get_getter( OFA_PAGE( page ));

	bin = ofa_balance_args_new( priv->getter, priv->settings_prefix );
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
	ofaBalanceRenderPrivate *priv;
	myISettings *settings;

	priv = ofa_balance_render_get_instance_private( OFA_BALANCE_RENDER( page ));

	settings = ofa_igetter_get_user_settings( priv->getter );
	*keyfile = my_isettings_get_keyfile( settings );
	*group_name = g_strdup_printf( "%s-print", priv->settings_prefix );
}

static GList *
render_page_v_get_dataset( ofaRenderPage *page )
{
	ofaBalanceRenderPrivate *priv;
	GList *dataset;
	ofaIAccountFilter *account_filter;
	ofaIDateFilter *date_filter;

	priv = ofa_balance_render_get_instance_private( OFA_BALANCE_RENDER( page ));

	g_free( priv->from_account );
	g_free( priv->to_account );
	account_filter = ofa_balance_args_get_account_filter( priv->args_bin );
	priv->from_account = g_strdup( ofa_iaccount_filter_get_account( account_filter, IACCOUNT_FILTER_FROM ));
	priv->to_account = g_strdup( ofa_iaccount_filter_get_account( account_filter, IACCOUNT_FILTER_TO ));
	priv->all_accounts = ofa_iaccount_filter_get_all_accounts( account_filter );

	priv->accounts_balance = ofa_balance_args_get_accounts_balance( priv->args_bin );

	date_filter = ofa_balance_args_get_date_filter( priv->args_bin );
	my_date_set_from_date( &priv->from_date, ofa_idate_filter_get_date( date_filter, IDATE_FILTER_FROM ));
	my_date_set_from_date( &priv->to_date, ofa_idate_filter_get_date( date_filter, IDATE_FILTER_TO ));

	dataset = ofo_entry_get_dataset_account_balance(
						priv->getter,
						priv->all_accounts ? NULL : priv->from_account,
						priv->all_accounts ? NULL : priv->to_account,
						my_date_is_valid( &priv->from_date ) ? &priv->from_date : NULL,
						my_date_is_valid( &priv->to_date ) ? &priv->to_date : NULL );

	priv->count = g_list_length( dataset );

	priv->class_subtotal = ofa_balance_args_get_subtotal_per_class( priv->args_bin );
	priv->class_break = ofa_balance_args_get_new_page_per_class( priv->args_bin );

	return( dataset );
}

static void
render_page_v_free_dataset( ofaRenderPage *page, GList *dataset )
{
	ofs_account_balance_list_free( &dataset );
}

/*
 * ofaBalanceArgs "ofa-changed" handler
 */
static void
on_args_changed( ofaBalanceArgs *bin, ofaBalanceRender *page )
{
	gboolean valid;
	gchar *message;

	valid = ofa_balance_args_is_valid( bin, &message );
	ofa_render_page_set_args_changed( OFA_RENDER_PAGE( page ), valid, message );
	g_free( message );
}

static void
irenderable_iface_init( ofaIRenderableInterface *iface )
{
	static const gchar *thisfn = "ofa_balance_render_irenderable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = irenderable_get_interface_version;
	iface->begin_render = irenderable_begin_render;
	iface->get_dossier_label = irenderable_get_dossier_label;
	iface->draw_page_header_title = irenderable_draw_page_header_title;
	iface->draw_page_header_notes = irenderable_draw_page_header_notes;
	iface->draw_header_column_names = irenderable_draw_header_column_names;
	iface->is_new_group = irenderable_is_new_group;
	iface->draw_group_header = irenderable_draw_group_header;
	iface->draw_top_report = irenderable_draw_top_report;
	iface->draw_line = irenderable_draw_line;
	iface->draw_bottom_report = irenderable_draw_bottom_report;
	iface->draw_group_footer = irenderable_draw_group_footer;
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
	static const gchar *thisfn = "ofa_balance_render_irenderable_begin_render";
	ofaBalanceRenderPrivate *priv;
	gdouble spacing, account_width, amount_width, cur_width;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_balance_render_get_instance_private( OFA_BALANCE_RENDER( instance ));

	priv->render_width = ofa_irenderable_get_render_width( instance );
	priv->render_height = ofa_irenderable_get_render_height( instance );
	priv->page_margin = ofa_irenderable_get_page_margin( instance );
	spacing = ofa_irenderable_get_columns_spacing( instance );

	/* account width should use the group font */
	ofa_irenderable_set_font( instance, ofa_irenderable_get_group_font( instance, 0 ));
	account_width = ofa_irenderable_get_text_width( instance, "XXXXXXXX" );

	/* the width of the amounts should use the last summary font */
	ofa_irenderable_set_font( instance, ofa_irenderable_get_summary_font( instance, 0 ));
	cur_width = ofa_irenderable_get_text_width( instance, "XXX" );
	amount_width = ofa_irenderable_get_text_width( instance, "99,999,999.99" );
	priv->amount_width = amount_width;

	/* starting from the left : body_number_ltab on the left margin */
	priv->body_number_ltab = priv->page_margin;
	priv->body_label_ltab = priv->body_number_ltab + account_width + spacing;

	/* starting from the right */
	priv->body_currency_ltab = priv->render_width - priv->page_margin - cur_width;
	priv->body_credit_solde_rtab = priv->body_currency_ltab - spacing;
	priv->body_debit_solde_rtab = priv->body_credit_solde_rtab - amount_width - spacing;
	priv->body_credit_period_rtab = priv->body_debit_solde_rtab - amount_width - spacing;
	priv->body_debit_period_rtab = priv->body_credit_period_rtab - amount_width - spacing;

	/* max size in Pango units */
	priv->body_number_max_size = ( priv->body_label_ltab - spacing - priv->body_number_ltab )*PANGO_SCALE;
	priv->body_label_max_size = ( priv->body_debit_period_rtab - amount_width - spacing - priv->body_label_ltab )*PANGO_SCALE;
}

/*
 * Accounts Balance
 */
static gchar *
irenderable_get_dossier_label( const ofaIRenderable *instance )
{
	ofaBalanceRenderPrivate *priv;
	ofaHub *hub;
	ofoDossier *dossier;
	const gchar *label;

	priv = ofa_balance_render_get_instance_private( OFA_BALANCE_RENDER( instance ));

	hub = ofa_igetter_get_hub( priv->getter );
	dossier = ofa_hub_get_dossier( hub );
	label = ofo_dossier_get_label( dossier );

	return( g_strdup( label ));
}

/*
 * Title is two lines
 * Depending whether we have chosen accounts balances or entries balances
 */
static void
irenderable_draw_page_header_title( ofaIRenderable *instance )
{
	ofaBalanceRenderPrivate *priv;
	gdouble y, r, g, b, height;
	const gchar *cstr;
	GString *stitle;
	gchar *sfrom_date, *sto_date;

	priv = ofa_balance_render_get_instance_private( OFA_BALANCE_RENDER( instance ));

	ofa_irenderable_get_title_color( instance, &r, &g, &b );
	ofa_irenderable_set_color( instance, r, g, b );
	y = ofa_irenderable_get_last_y( instance );

	/* line 1 general books summary */
	ofa_irenderable_set_font( instance,
			ofa_irenderable_get_title_font( instance, ofa_irenderable_get_current_page_num( instance )));
	cstr = priv->accounts_balance ? st_page_header_title_accounts : st_page_header_title_entries;
	height = ofa_irenderable_set_text( instance, priv->render_width/2, y, cstr, PANGO_ALIGN_CENTER );
	y += height;

	/* line 2 - From account xxx to account xxx - From date xxx to date xxx
	 * Accounts balance doesn't specify the beginning date as this one
	 * is mandatorily at the beginning of the exercice */
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

	if( priv->accounts_balance ){
		if( my_date_is_valid( &priv->to_date )){
			sto_date = my_date_to_str( &priv->to_date, ofa_prefs_date_display( priv->getter ));
			g_string_append_printf( stitle, _( "As of %s" ), sto_date );
			g_free( sto_date );
		} else {
			stitle = g_string_append( stitle, _( "As of today" ));
		}

	} else if( !my_date_is_valid( &priv->from_date ) && !my_date_is_valid( &priv->to_date )){
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
irenderable_draw_page_header_notes( ofaIRenderable *instance )
{
	ofaBalanceRenderPrivate *priv;
	gdouble y, text_height;

	priv = ofa_balance_render_get_instance_private( OFA_BALANCE_RENDER( instance ));

	if( ofa_irenderable_get_current_page_num( instance ) == 0 ){
		if( !priv->accounts_balance ){

			y = ofa_irenderable_get_last_y( instance );

			ofa_irenderable_set_color( instance, COLOR_BLACK );
			ofa_irenderable_set_font( instance, st_notes_font );
			text_height = ofa_irenderable_get_text_height( instance );
			y += text_height/2.0;

			y += ofa_irenderable_set_wrapped_text( instance,
						priv->page_margin, y,
						(priv->render_width-2.0*priv->page_margin)*PANGO_SCALE,
						_( "Please note that this entries balance summary only "
							"displays the balance of the entries whose effect "
							"date is between the above date limits.\n"
							"As such, it is not intended nor expected to reflect "
							"the balance of the accounts at the end of the period." ),
						PANGO_ALIGN_LEFT );

			ofa_irenderable_set_last_y( instance, y+text_height/2.0 );
		}
	}
}

static void
irenderable_draw_header_column_names( ofaIRenderable *instance )
{
	ofaBalanceRenderPrivate *priv;
	static gdouble st_vspace_rate_before = 0.25;
	static gdouble st_vspace_rate_after = 0.25;
	gdouble text_height, y, height, yh, hline;
	cairo_t *context;

	priv = ofa_balance_render_get_instance_private( OFA_BALANCE_RENDER( instance ));

	y = ofa_irenderable_get_last_y( instance );
	text_height = ofa_irenderable_get_text_height( instance );
	context = ofa_irenderable_get_context( instance );

	/* draw three vertical white lines to visually separate the amounts */
	cairo_set_source_rgb( context, COLOR_WHITE );
	cairo_set_line_width( context, 0.5 );
	height = ofa_irenderable_get_header_columns_height( instance );

	cairo_move_to( context, priv->body_debit_period_rtab-priv->amount_width, y );
	cairo_line_to( context, priv->body_debit_period_rtab-priv->amount_width, y+height );
	cairo_stroke( context );

	cairo_move_to( context, priv->body_credit_period_rtab+priv->page_margin, y );
	cairo_line_to( context, priv->body_credit_period_rtab+priv->page_margin, y+height );
	cairo_stroke( context );

	cairo_move_to( context, priv->body_credit_solde_rtab+priv->page_margin, y );
	cairo_line_to( context, priv->body_credit_solde_rtab+priv->page_margin, y+height );
	cairo_stroke( context );

	yh = y+(height/2);
	cairo_move_to( context, priv->body_debit_period_rtab-priv->amount_width, yh );
	cairo_line_to( context, priv->body_credit_solde_rtab+priv->page_margin, yh );
	cairo_stroke( context );

	y += st_vspace_rate_before * text_height;
	hline = text_height * ( 1+st_vspace_rate_before+st_vspace_rate_after );

	ofa_irenderable_set_text( instance,
			priv->body_number_ltab, y+hline/2,
			_( "Account" ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			priv->body_label_ltab, y+hline/2,
			_( "Label" ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			priv->body_debit_period_rtab, y, _( "Period balance" ), PANGO_ALIGN_CENTER );

	ofa_irenderable_set_text( instance,
			priv->body_debit_solde_rtab, y, _( "Period solde" ), PANGO_ALIGN_CENTER );

	y += hline;

	ofa_irenderable_set_text( instance,
			priv->body_debit_period_rtab, y+1, _( "Debit" ), PANGO_ALIGN_RIGHT );

	ofa_irenderable_set_text( instance,
			priv->body_credit_period_rtab, y+1, _( "Credit" ), PANGO_ALIGN_RIGHT );

	ofa_irenderable_set_text( instance,
			priv->body_debit_solde_rtab, y+1, _( "Debit" ), PANGO_ALIGN_RIGHT );

	ofa_irenderable_set_text( instance,
			priv->body_credit_solde_rtab, y+1, _( "Credit" ), PANGO_ALIGN_RIGHT );

	y += hline;

	ofa_irenderable_set_last_y( instance, y );
}

/*
 * Have group break on account class number
 */
static gboolean
irenderable_is_new_group( const ofaIRenderable *instance, GList *prev, GList *line, ofeIRenderableBreak *sep )
{
	ofaBalanceRenderPrivate *priv;
	ofsAccountBalance *current_sbal, *prev_sbal;
	gint current_class, prev_class;

	priv = ofa_balance_render_get_instance_private( OFA_BALANCE_RENDER( instance ));

	*sep = IRENDERABLE_BREAK_NONE;

	if( !prev || !line ){
		return( TRUE );
	}

	if( priv->class_subtotal || priv->class_break ){

		current_sbal = ( ofsAccountBalance * ) line->data;
		current_class = ofo_account_get_class_from_number( current_sbal->account );

		prev_sbal = ( ofsAccountBalance * ) prev->data;
		prev_class = ofo_account_get_class_from_number( prev_sbal->account );

		if( priv->class_break ){
			*sep = IRENDERABLE_BREAK_NEW_PAGE;
		} else {
			*sep = IRENDERABLE_BREAK_SEP_LINE;
		}

		return( current_class != prev_class );
	}

	return( FALSE );
}

/*
 * draw account header
 * Class x - xxx
 */
static void
irenderable_draw_group_header( ofaIRenderable *instance )
{
	ofaBalanceRenderPrivate *priv;
	GList *line;
	ofsAccountBalance *sbal;
	gdouble height, y;
	gchar *str;

	priv = ofa_balance_render_get_instance_private( OFA_BALANCE_RENDER( instance ));

	if( priv->class_break || priv->class_subtotal ){

		line = ofa_irenderable_get_current_line( instance );
		if( line ){

			/* setup the class properties */
			sbal = ( ofsAccountBalance * ) line->data;
			priv->class_num = ofo_account_get_class_from_number( sbal->account );
			priv->class_object = ofo_class_get_by_number( priv->getter, priv->class_num );

			y = ofa_irenderable_get_last_y( instance );
			height = ofa_irenderable_get_line_height( instance );

			/* display the class header */
			/* label */
			str = g_strdup_printf(
						_( "Class %u - %s" ),
						priv->class_num, ofo_class_get_label( priv->class_object ));
			ofa_irenderable_set_text( instance, priv->page_margin, y, str, PANGO_ALIGN_LEFT );
			g_free( str );

			y += height;
			ofa_irenderable_set_last_y( instance, y );
		}
	}
}

static void
irenderable_draw_top_report( ofaIRenderable *instance )
{
	ofaBalanceRenderPrivate *priv;

	priv = ofa_balance_render_get_instance_private( OFA_BALANCE_RENDER( instance ));

	if( priv->class_subtotal ){
		draw_subtotals_balance( instance, _( "Top class report : " ));
	}
}

/*
 * num_line is counted from 0 in the page
 *
 * (printable2)width(A4)=559
 * date  journal  piece    label      debit   credit   solde
 * 10    6        max(10)  max(80)      15d      15d     15d
 */
static void
irenderable_draw_line( ofaIRenderable *instance )
{
	ofaBalanceRenderPrivate *priv;
	GList *line;
	gdouble y;
	ofsAccountBalance *sbal;
	ofoAccount *account;
	gchar *str;
	gdouble solde;
	const gchar *cur_code;
	ofoCurrency *cur_obj;

	priv = ofa_balance_render_get_instance_private( OFA_BALANCE_RENDER( instance ));

	line = ofa_irenderable_get_current_line( instance );
	if( line ){

		/* get the data properties */
		sbal = ( ofsAccountBalance * ) line->data;
		g_return_if_fail( my_strlen( sbal->account ));

		account = ofo_account_get_by_number( priv->getter, sbal->account );
		g_return_if_fail( account && OFO_IS_ACCOUNT( account ));
		/*g_debug( "irenderable_draw_line: account=%s %s", sbal->account, ofo_account_get_label( account ));*/

		cur_code = ofo_account_get_currency( account );
		g_return_if_fail( my_strlen( cur_code ));

		cur_obj = ofo_currency_get_by_code( priv->getter, cur_code );
		g_return_if_fail( cur_obj && OFO_IS_CURRENCY( cur_obj ));

		solde = 0;

		y = ofa_irenderable_get_last_y( instance );
		/*g_debug( "irenderable_draw_line: y=%lf, current=%p", y, ( void * ) current );*/

		ofa_irenderable_ellipsize_text( instance,
				priv->body_number_ltab, y, sbal->account, priv->body_number_max_size );

		ofa_irenderable_ellipsize_text( instance,
				priv->body_label_ltab, y,
				ofo_account_get_label( account ), priv->body_label_max_size );

		if( sbal->debit ){
			str = ofa_amount_to_str( sbal->debit, cur_obj, priv->getter );
			ofa_irenderable_set_text( instance,
					priv->body_debit_period_rtab, y, str, PANGO_ALIGN_RIGHT );
			g_free( str );
			solde -= sbal->debit;
		}

		if( sbal->credit ){
			str = ofa_amount_to_str( sbal->credit, cur_obj, priv->getter );
			ofa_irenderable_set_text( instance,
					priv->body_credit_period_rtab, y, str, PANGO_ALIGN_RIGHT );
			solde += sbal->credit;
		}

		if( solde < 0 ){
			str = ofa_amount_to_str( -1*solde, cur_obj, priv->getter );
			ofa_irenderable_set_text( instance,
					priv->body_debit_solde_rtab, y, str, PANGO_ALIGN_RIGHT );

		} else {
			str = ofa_amount_to_str( solde, cur_obj, priv->getter );
			ofa_irenderable_set_text( instance,
					priv->body_credit_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
		}

		ofa_irenderable_set_text( instance,
				priv->body_currency_ltab, y, sbal->currency, PANGO_ALIGN_LEFT );

		priv->class_totals = add_account_balance(
							OFA_BALANCE_RENDER( instance ),
							priv->class_totals, sbal->currency, solde, sbal );

		priv->gen_totals = add_account_balance(
							OFA_BALANCE_RENDER( instance ),
							priv->gen_totals, sbal->currency, solde, sbal );
	}
}

static void
irenderable_draw_bottom_report( ofaIRenderable *instance )
{
	ofaBalanceRenderPrivate *priv;

	priv = ofa_balance_render_get_instance_private( OFA_BALANCE_RENDER( instance ));

	if( priv->class_subtotal ){
		draw_subtotals_balance( instance, _( "Bottom class report : " ));
	}
}

/*
 * This function is called many time with NULL arguments in order to
 * auto-detect the height of the group footer (in particular each time
 * the #ofa_irenderable_draw_line() function needs to know if there is
 * enough vertical space left to draw the current line) - so take care
 * of not updating the account balance when not drawing...
 */
static void
irenderable_draw_group_footer( ofaIRenderable *instance )
{
	ofaBalanceRenderPrivate *priv;
	gchar *str;

	priv = ofa_balance_render_get_instance_private( OFA_BALANCE_RENDER( instance ));

	if( priv->class_subtotal ){
		str = g_strdup_printf( _( "Class %d entries balance : "), priv->class_num );
		draw_subtotals_balance( instance, str );
		g_free( str );
	}

	if( !ofa_irenderable_is_paginating( instance )){
		free_currencies( &priv->class_totals );
	}
}

/*
 * draw on the bottom of the last page the summary with one line per
 * currency
 */
static void
irenderable_draw_last_summary( ofaIRenderable *instance )
{
	ofaBalanceRenderPrivate *priv;
	static const gdouble st_vspace_rate = 0.25;
	gdouble bottom, top, vspace, req_height, height;

	priv = ofa_balance_render_get_instance_private( OFA_BALANCE_RENDER( instance ));

	if( !priv->count ){
		ofa_irenderable_draw_no_data( instance );
		return;
	}

	/* bottom of the rectangle */
	bottom = ofa_irenderable_get_max_y( instance );

	/* top of the rectangle */
	height = ofa_irenderable_get_text_height( instance );
	vspace = height * st_vspace_rate;
	req_height = g_list_length( priv->gen_totals ) * height
			+ ( 1+g_list_length( priv->gen_totals )) * vspace;
	top = bottom - req_height;

	ofa_irenderable_draw_rect( instance, 0, top, -1, req_height );
	top += vspace;

	draw_account_balance( instance, priv->gen_totals, top, _( "General balance : " ));

	ofa_irenderable_set_last_y( instance, ofa_irenderable_get_last_y( instance ) + req_height );
}

static const gchar *
irenderable_get_summary_font( const ofaIRenderable *instance, guint page_num )
{
	return( st_summary_font );
}

static void
irenderable_clear_runtime_data( ofaIRenderable *instance )
{
	ofaBalanceRenderPrivate *priv;

	priv = ofa_balance_render_get_instance_private( OFA_BALANCE_RENDER( instance ));

	free_currencies( &priv->class_totals );
	free_currencies( &priv->gen_totals );
}

static void
draw_subtotals_balance( ofaIRenderable *instance, const gchar *title )
{
	ofaBalanceRenderPrivate *priv;
	gdouble req_height, last_y, height;

	priv = ofa_balance_render_get_instance_private( OFA_BALANCE_RENDER( instance ));

	/* top of the rectangle */
	height = ofa_irenderable_get_line_height( instance );
	req_height = g_list_length( priv->class_totals ) * height;
	last_y = ofa_irenderable_get_last_y( instance );

	draw_account_balance( instance, priv->class_totals, last_y, title );

	ofa_irenderable_set_last_y( instance, last_y+req_height );
}

static void
draw_account_balance( ofaIRenderable *instance,
							GList *list, gdouble top, const gchar *title )
{
	ofaBalanceRenderPrivate *priv;
	GList *it;
	gboolean first;
	sCurrency *scur;
	gchar *str;
	gdouble height;
	ofoCurrency *cur_obj;

	priv = ofa_balance_render_get_instance_private( OFA_BALANCE_RENDER( instance ));

	height = ofa_irenderable_get_line_height( instance );

	for( it=list, first=TRUE ; it ; it=it->next ){
		if( first ){
			ofa_irenderable_set_text( instance,
						priv->body_debit_period_rtab-priv->amount_width, top,
						title, PANGO_ALIGN_RIGHT );
			first = FALSE;
			}

		scur = ( sCurrency * ) it->data;
		g_return_if_fail( scur && my_strlen( scur->currency ));

		cur_obj = ofo_currency_get_by_code( priv->getter, scur->currency );
		g_return_if_fail( cur_obj && OFO_IS_CURRENCY( cur_obj ));

		str = ofa_amount_to_str( scur->period_d, cur_obj, priv->getter );
		ofa_irenderable_set_text( instance,
				priv->body_debit_period_rtab, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = ofa_amount_to_str( scur->period_c, cur_obj, priv->getter );
		ofa_irenderable_set_text( instance,
				priv->body_credit_period_rtab, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = ofa_amount_to_str( scur->solde_d, cur_obj, priv->getter );
		ofa_irenderable_set_text( instance,
				priv->body_debit_solde_rtab, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = ofa_amount_to_str( scur->solde_c, cur_obj, priv->getter );
		ofa_irenderable_set_text( instance,
				priv->body_credit_solde_rtab, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		ofa_irenderable_set_text( instance,
				priv->body_currency_ltab, top, scur->currency, PANGO_ALIGN_LEFT );

		top += height;
	}
}

static GList *
add_account_balance( ofaBalanceRender *self, GList *list,
							const gchar *currency, gdouble solde, ofsAccountBalance *sbal )
{
	static const gchar *thisfn = "ofa_balance_render_add_account_balance";
	GList *it, *found;
	sCurrency *scur;

	found = NULL;
	for( it=list ; it ; it=it->next ){
		scur = ( sCurrency * ) it->data;
		if( !my_collate( scur->currency, currency )){
			found = it;
			break;
		}
	}

	if( found ){
		scur = ( sCurrency * ) found->data;

	} else {
		scur = g_new0( sCurrency, 1 );
		scur->currency = g_strdup( currency );
		g_debug( "%s: inserting new %s currency", thisfn, scur->currency );
		list = g_list_insert_sorted( list, scur, ( GCompareFunc ) cmp_currencies );
		scur->period_d = 0;
		scur->period_c = 0;
		scur->solde_d = 0;
		scur->solde_c = 0;
	}

	scur->period_d += sbal->debit;
	scur->period_c += sbal->credit;

	if( solde < 0 ){
		scur->solde_d += -1*solde;

	} else if( solde > 0 ){
		scur->solde_c += solde;
	}

	return( list );
}

static gint
cmp_currencies( const sCurrency *a, const sCurrency *b )
{
	return( my_collate( a->currency, b->currency ));
}

static void
free_currencies( GList **list )
{
	g_list_free_full( *list, ( GDestroyNotify ) free_currency );
	*list = NULL;
}

static void
free_currency( sCurrency *scur )
{
	g_free( scur->currency );
	g_free( scur );
}

/*
 * settings = paned_position;
 */
static void
read_settings( ofaBalanceRender *self )
{
	ofaBalanceRenderPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;
	GtkWidget *paned;
	gint pos;
	gchar *key;

	priv = ofa_balance_render_get_instance_private( self );

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
write_settings( ofaBalanceRender *self )
{
	ofaBalanceRenderPrivate *priv;
	myISettings *settings;
	GtkWidget *paned;
	gchar *str, *key;

	priv = ofa_balance_render_get_instance_private( self );

	paned = ofa_render_page_get_top_paned( OFA_RENDER_PAGE( self ));

	str = g_strdup_printf( "%d;",
			gtk_paned_get_position( GTK_PANED( paned )));

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( key );
	g_free( str );
}
