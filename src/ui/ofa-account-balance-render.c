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
#include "api/ofa-icontext.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iexporter.h"
#include "api/ofa-igetter.h"
#include "api/ofa-irenderable.h"
#include "api/ofa-isignaler.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-prefs.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "core/ofa-account-balance.h"

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
	ofaAccountBalance     *account_balance;
	guint                  count;

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

	/* actions
	 */
	GSimpleAction         *export_action;
}
	ofaAccountBalanceRenderPrivate;

/*
 * Accounts balances print uses a portrait orientation
 */
#define THIS_PAGE_ORIENTATION            GTK_PAGE_ORIENTATION_PORTRAIT
#define THIS_PAPER_NAME                  GTK_PAPER_NAME_A4

static const gchar *st_page_header_title_accounts = N_( "Accounts Balance Summary" );

/* see same labels in core/ofa-account-balance.c */
static const gchar *st_header_account             = N_( "Account" );
static const gchar *st_header_label               = N_( "Label" );
static const gchar *st_header_solde_at            = N_( "Solde at" );
static const gchar *st_header_total_debits        = N_( "Total debits" );
static const gchar *st_header_total_credits       = N_( "Total credits" );

/* these are parms which describe the page layout
 */

static const gchar *st_title2_font                = "Sans Bold 8";
static const gchar *st_summary_font               = "Sans Bold 6";

static GtkWidget         *page_v_get_top_focusable_widget( const ofaPage *page );
static void               paned_page_v_setup_view( ofaPanedPage *page, GtkPaned *paned );
static void               paned_page_v_init_view( ofaPanedPage *page );
static GtkWidget         *render_page_v_get_args_widget( ofaRenderPage *page );
static const gchar       *render_page_v_get_paper_name( ofaRenderPage *page );
static GtkPageOrientation render_page_v_get_page_orientation( ofaRenderPage *page );
static void               render_page_v_get_print_settings( ofaRenderPage *page, GKeyFile **keyfile, gchar **group_name );
static GList             *render_page_v_get_dataset( ofaRenderPage *page );
static void               render_page_v_free_dataset( ofaRenderPage *page, GList *dataset );
static void               on_args_changed( ofaAccountBalanceArgs *bin, ofaAccountBalanceRender *page );
static void               irenderable_iface_init( ofaIRenderableInterface *iface );
static guint              irenderable_get_interface_version( void );
static void               irenderable_begin_render( ofaIRenderable *instance );
static gchar             *irenderable_get_dossier_label( const ofaIRenderable *instance );
static void               irenderable_draw_page_header_title( ofaIRenderable *instance );
static void               irenderable_draw_header_column_names( ofaIRenderable *instance );
static void               irenderable_draw_line( ofaIRenderable *instance );
static void               irenderable_draw_last_summary( ofaIRenderable *instance );
static const gchar       *irenderable_get_summary_font( const ofaIRenderable *instance, guint page_num );
static void               irenderable_clear_runtime_data( ofaIRenderable *instance );
static void               draw_account_balance( ofaIRenderable *instance, GList *list, gdouble top, const gchar *title );
static void               draw_amounts( ofaIRenderable *instance, ofsAccountBalanceCurrency *scur );
static void               irenderable_end_render( ofaIRenderable *instance );
static void               action_on_export_activated( GSimpleAction *action, GVariant *empty, ofaAccountBalanceRender *self );
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

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_balance_render_parent_class )->finalize( instance );
}

static void
account_balance_render_dispose( GObject *instance )
{
	ofaAccountBalanceRenderPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_BALANCE_RENDER( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		write_settings( OFA_ACCOUNT_BALANCE_RENDER( instance ));

		/* unref object members here */
		priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( instance ));

		g_clear_object( &priv->account_balance );
		g_clear_object( &priv->export_action );
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

	OFA_PANED_PAGE_CLASS( klass )->setup_view = paned_page_v_setup_view;
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
paned_page_v_setup_view( ofaPanedPage *page, GtkPaned *paned )
{
	static const gchar *thisfn = "ofa_account_balance_render_paned_page_v_setup_view";
	ofaAccountBalanceRenderPrivate *priv;
	ofaIContext *render_context;
	GMenu *menu;

	g_debug( "%s: entering page=%p", thisfn, ( void * ) page );

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( page ));

	priv->getter = ofa_page_get_getter( OFA_PAGE( page ));

	/* call the parent class which defines the drawing area */
	if( OFA_PANED_PAGE_CLASS( ofa_account_balance_render_parent_class )->setup_view ){
		OFA_PANED_PAGE_CLASS( ofa_account_balance_render_parent_class )->setup_view( page, paned );
	}

	g_debug( "%s: continuing page=%p", thisfn, ( void * ) page );

	/* adds a contextual menu to the drawing area */
	priv->export_action = g_simple_action_new( "export", NULL );
	g_signal_connect( priv->export_action, "activate", G_CALLBACK( action_on_export_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->export_action ),
			_( "Export the render content..." ));
	g_simple_action_set_enabled( priv->export_action, FALSE );

	render_context = ofa_render_page_get_icontext( OFA_RENDER_PAGE( page ));
	menu = ofa_iactionable_get_menu( OFA_IACTIONABLE( page ), priv->settings_prefix );
	ofa_icontext_set_menu( render_context, OFA_IACTIONABLE( page ), menu );
}

static void
paned_page_v_init_view( ofaPanedPage *page )
{
	static const gchar *thisfn = "ofa_account_balance_render_paned_page_v_init_view";
	ofaAccountBalanceRenderPrivate *priv;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( page ));

	priv->account_balance = ofa_account_balance_new( priv->getter );

	read_settings( OFA_ACCOUNT_BALANCE_RENDER( page ));

	on_args_changed( priv->args_bin, OFA_ACCOUNT_BALANCE_RENDER( page ));
}

static GtkWidget *
render_page_v_get_args_widget( ofaRenderPage *page )
{
	ofaAccountBalanceRenderPrivate *priv;
	ofaAccountBalanceArgs *bin;

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( page ));

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
	ofaIDateFilter *date_filter;
	GList *accounts;

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( page ));

	date_filter = ofa_account_balance_args_get_date_filter( priv->args_bin );
	my_date_set_from_date( &priv->from_date, ofa_idate_filter_get_date( date_filter, IDATE_FILTER_FROM ));
	my_date_set_from_date( &priv->to_date, ofa_idate_filter_get_date( date_filter, IDATE_FILTER_TO ));

	accounts = ofa_account_balance_compute( priv->account_balance, &priv->from_date, &priv->to_date );
	priv->count = g_list_length( accounts );

	return( accounts );
}

static void
render_page_v_free_dataset( ofaRenderPage *page, GList *dataset )
{
	ofaAccountBalanceRenderPrivate *priv;

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( page ));

	ofa_account_balance_clear( priv->account_balance );

	g_simple_action_set_enabled( priv->export_action, FALSE );
}

/*
 * ofaAccountBalanceArgs "ofa-changed" handler
 */
static void
on_args_changed( ofaAccountBalanceArgs *bin, ofaAccountBalanceRender *page )
{
	ofaAccountBalanceRenderPrivate *priv;
	gboolean valid;
	gchar *message;

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( page ));

	valid = ofa_account_balance_args_is_valid( bin, &message );
	ofa_render_page_set_args_changed( OFA_RENDER_PAGE( page ), valid, message );
	g_free( message );

	g_simple_action_set_enabled( priv->export_action, FALSE );
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
	iface->end_render = irenderable_end_render;
}

static guint
irenderable_get_interface_version( void )
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

	sfrom_date = my_date_to_str( &priv->from_date, ofa_prefs_date_get_display_format( priv->getter ));
	sto_date = my_date_to_str( &priv->to_date, ofa_prefs_date_get_display_format( priv->getter ));
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
			gettext( st_header_account ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			priv->body_label_ltab, y+half,
			gettext( st_header_label ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			priv->body_begin_sens_ltab, y,
			gettext( st_header_solde_at ), PANGO_ALIGN_RIGHT );

	sdate = my_date_to_str( &priv->from_date, ofa_prefs_date_get_display_format( priv->getter ));
	ofa_irenderable_set_text( instance,
			priv->body_begin_sens_ltab, y+text_height,
			sdate, PANGO_ALIGN_RIGHT );
	g_free( sdate );

	ofa_irenderable_set_text( instance,
			priv->body_debit_period_rtab, y+half,
			gettext( st_header_total_debits ), PANGO_ALIGN_RIGHT );

	ofa_irenderable_set_text( instance,
			priv->body_credit_period_rtab, y+half,
			gettext( st_header_total_credits ), PANGO_ALIGN_RIGHT );

	ofa_irenderable_set_text( instance,
			priv->body_end_sens_ltab, y,
			st_header_solde_at, PANGO_ALIGN_RIGHT );

	sdate = my_date_to_str( &priv->to_date, ofa_prefs_date_get_display_format( priv->getter ));
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
	ofsAccountBalanceAccount *sacc;
	gdouble y;

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( instance ));

	line = ofa_irenderable_get_current_line( instance );
	if( line ){

		/* get the data properties */
		sacc = ( ofsAccountBalanceAccount * ) line->data;

		/* render the line */
		y = ofa_irenderable_get_last_y( instance );

		ofa_irenderable_ellipsize_text( instance,
				priv->body_number_ltab, y,
				ofo_account_get_number( sacc->account ), priv->body_number_max_size );

		ofa_irenderable_ellipsize_text( instance,
				priv->body_label_ltab, y,
				ofo_account_get_label( sacc->account ), priv->body_label_max_size );

		draw_amounts( instance, sacc->scur );
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
	GList *totals;

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( instance ));

	if( !priv->count ){
		ofa_irenderable_draw_no_data( instance );
		return;
	}

	totals = ofa_account_balance_get_totals( priv->account_balance );

	/* bottom of the rectangle */
	bottom = ofa_irenderable_get_max_y( instance );
	last_y = ofa_irenderable_get_last_y( instance );

	/* top of the rectangle */
	height = ofa_irenderable_get_text_height( instance );
	vspace = height * st_vspace_rate;
	req_height = g_list_length( totals ) * height
			+ ( 1+g_list_length( totals )) * vspace;
	top = bottom - req_height;

	ofa_irenderable_draw_rect( instance, 0, top, -1, req_height );
	top += vspace;

	draw_account_balance( instance, totals, top, _( "General balance : " ));

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
}

static void
draw_account_balance( ofaIRenderable *instance,
							GList *list, gdouble top, const gchar *title )
{
	ofaAccountBalanceRenderPrivate *priv;
	GList *it;
	gboolean first;
	ofsAccountBalanceCurrency *scur;
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

		scur = ( ofsAccountBalanceCurrency * ) it->data;
		g_return_if_fail( scur && OFO_IS_CURRENCY( scur->currency ));

		draw_amounts( instance, scur );

		top += height;
	}
}

static void
draw_amounts( ofaIRenderable *instance, ofsAccountBalanceCurrency *scur )
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
irenderable_end_render( ofaIRenderable *instance )
{
	static const gchar *thisfn = "ofa_account_balance_render_irenderable_end_render";
	ofaAccountBalanceRenderPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_account_balance_render_get_instance_private( OFA_ACCOUNT_BALANCE_RENDER( instance ));

	g_simple_action_set_enabled( priv->export_action, TRUE );
}

/*
 * export the content of the render area
 */
static void
action_on_export_activated( GSimpleAction *action, GVariant *empty, ofaAccountBalanceRender *self )
{
	ofaAccountBalanceRenderPrivate *priv;
	ofaISignaler *signaler;

	priv = ofa_account_balance_render_get_instance_private( self );

	signaler = ofa_igetter_get_signaler( priv->getter );

	g_signal_emit_by_name( signaler, SIGNALER_EXPORT_ASSISTANT_RUN, OFA_IEXPORTABLE( priv->account_balance ), TRUE );
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
