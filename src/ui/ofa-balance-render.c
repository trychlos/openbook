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

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-class.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofs-account-balance.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-iaccount-filter.h"
#include "ui/ofa-idate-filter.h"
#include "ui/ofa-irenderable.h"
#include "ui/ofa-balance-bin.h"
#include "ui/ofa-balance-render.h"

/* private instance data
 */
struct _ofaBalanceRenderPrivate {

	ofoDossier    *dossier;
	ofaBalanceBin *args_bin;

	/* internals
	 */
	gchar         *from_account;
	gchar         *to_account;
	gboolean       all_accounts;
	gboolean       per_class;
	gboolean       new_page;
	GDate          from_date;
	GDate          to_date;
	gboolean       accounts_balance;
	GList         *totals;
	gint           count;				/* count of returned entries */

	/* print datas
	 */
	gdouble        render_width;
	gdouble        render_height;
	gdouble        page_margin;
	gdouble        amount_width;
	gdouble        body_number_ltab;
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
	GList         *subtotals;			/* subtotals per currency for this class */
};

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

static const gchar *st_print_settings    = "RenderBalancesPrint";

/* these are parms which describe the page layout
 */

/* the columns of the body */
static const gint st_body_font_size      = 8;
#define st_number_width                  (gdouble) 50/9*st_body_font_size
#define st_currency_width                (gdouble) 23/9*st_body_font_size
#define st_column_spacing                (gdouble) 4

#define COLOR_WHITE                      1,      1,      1

static ofaRenderPageClass *ofa_balance_render_parent_class = NULL;

static GType              register_type( void );
static void               balance_render_finalize( GObject *instance );
static void               balance_render_dispose( GObject *instance );
static void               balance_render_instance_init( ofaBalanceRender *self );
static void               balance_render_class_init( ofaBalanceRenderClass *klass );
static void               page_init_view( ofaPage *page );
static GtkWidget         *page_get_top_focusable_widget( const ofaPage *page );
static GtkWidget         *render_page_get_args_widget( ofaRenderPage *page );
static const gchar       *render_page_get_paper_name( ofaRenderPage *page );
static GtkPageOrientation render_page_get_page_orientation( ofaRenderPage *page );
static void               render_page_get_print_settings( ofaRenderPage *page, GKeyFile **keyfile, gchar **group_name );
static GList             *render_page_get_dataset( ofaRenderPage *page );
static void               render_page_free_dataset( ofaRenderPage *page, GList *dataset );
static void               on_args_changed( ofaBalanceBin *bin, ofaBalanceRender *page );
static void               irenderable_iface_init( ofaIRenderableInterface *iface );
static guint              irenderable_get_interface_version( const ofaIRenderable *instance );
static void               irenderable_reset_runtime( ofaIRenderable *instance );
static gboolean           irenderable_want_groups( const ofaIRenderable *instance );
static gboolean           irenderable_want_new_page( const ofaIRenderable *instance );
static void               irenderable_begin_render( ofaIRenderable *instance, gdouble render_width, gdouble render_height );
static gchar             *irenderable_get_dossier_name( const ofaIRenderable *instance );
static gchar             *irenderable_get_page_header_title( const ofaIRenderable *instance );
static gchar             *irenderable_get_page_header_subtitle( const ofaIRenderable *instance );
static void               irenderable_draw_page_header_notes( ofaIRenderable *instance, gint page_num );
static void               irenderable_draw_page_header_columns( ofaIRenderable *instance, gint page_num );
static gboolean           irenderable_is_new_group( const ofaIRenderable *instance, GList *current, GList *prev );
static void               irenderable_draw_group_header( ofaIRenderable *instance, GList *current );
static void               irenderable_draw_group_top_report( ofaIRenderable *instance );
static void               irenderable_draw_line( ofaIRenderable *instance, GList *current );
static void               irenderable_draw_group_bottom_report( ofaIRenderable *instance );
static void               irenderable_draw_group_footer( ofaIRenderable *instance );
static void               irenderable_draw_bottom_summary( ofaIRenderable *instance );
static void               draw_subtotals_balance( ofaIRenderable *instance, const gchar *title );
static void               draw_account_balance( ofaIRenderable *instance, GList *list, gdouble top, const gchar *title );
static GList             *add_account_balance( ofaBalanceRender *self, GList *list, const gchar *currency, gdouble solde, ofsAccountBalance *sbal );
static gint               cmp_currencies( const sCurrency *a, const sCurrency *b );

static void
free_currency( sCurrency *total_per_currency )
{
	g_free( total_per_currency->currency );
	g_free( total_per_currency );
}

GType
ofa_balance_render_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofo_balance_render_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaBalanceRenderClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) balance_render_class_init,
		NULL,
		NULL,
		sizeof( ofaBalanceRender ),
		0,
		( GInstanceInitFunc ) balance_render_instance_init
	};

	static const GInterfaceInfo irenderable_iface_info = {
		( GInterfaceInitFunc ) irenderable_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFA_TYPE_RENDER_PAGE, "ofaBalanceRender", &info, 0 );

	g_type_add_interface_static( type, OFA_TYPE_IRENDERABLE, &irenderable_iface_info );

	return( type );
}

static void
balance_render_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_balance_render_finalize";
	ofaBalanceRenderPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BALANCE_RENDER( instance ));

	/* free data members here */
	priv = OFA_BALANCE_RENDER( instance )->priv;

	g_free( priv->from_account );
	g_free( priv->to_account );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_balance_render_parent_class )->finalize( instance );
}

static void
balance_render_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_BALANCE_RENDER( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_balance_render_parent_class )->dispose( instance );
}

static void
balance_render_instance_init( ofaBalanceRender *self )
{
	static const gchar *thisfn = "ofa_balance_render_instance_init";

	g_return_if_fail( OFA_IS_BALANCE_RENDER( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_BALANCE_RENDER, ofaBalanceRenderPrivate );
}

static void
balance_render_class_init( ofaBalanceRenderClass *klass )
{
	static const gchar *thisfn = "ofa_balance_render_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	ofa_balance_render_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = balance_render_dispose;
	G_OBJECT_CLASS( klass )->finalize = balance_render_finalize;

	OFA_PAGE_CLASS( klass )->init_view = page_init_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = page_get_top_focusable_widget;

	OFA_RENDER_PAGE_CLASS( klass )->get_args_widget = render_page_get_args_widget;
	OFA_RENDER_PAGE_CLASS( klass )->get_paper_name = render_page_get_paper_name;
	OFA_RENDER_PAGE_CLASS( klass )->get_page_orientation = render_page_get_page_orientation;
	OFA_RENDER_PAGE_CLASS( klass )->get_print_settings = render_page_get_print_settings;
	OFA_RENDER_PAGE_CLASS( klass )->get_dataset = render_page_get_dataset;
	OFA_RENDER_PAGE_CLASS( klass )->free_dataset = render_page_free_dataset;

	g_type_class_add_private( klass, sizeof( ofaBalanceRenderPrivate ));
}

static void
page_init_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_balance_render_page_init_view";
	ofaBalanceRenderPrivate *priv;

	OFA_PAGE_CLASS( ofa_balance_render_parent_class )->init_view( page );

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = OFA_BALANCE_RENDER( page )->priv;
	on_args_changed( priv->args_bin, OFA_BALANCE_RENDER( page ));

	priv->dossier = ofa_page_get_dossier( page );
}

static GtkWidget *
page_get_top_focusable_widget( const ofaPage *page )
{
	return( NULL );
}

static GtkWidget *
render_page_get_args_widget( ofaRenderPage *page )
{
	ofaBalanceRenderPrivate *priv;
	ofaBalanceBin *bin;

	priv = OFA_BALANCE_RENDER( page )->priv;

	bin = ofa_balance_bin_new( ofa_page_get_main_window( OFA_PAGE( page )));
	g_signal_connect( G_OBJECT( bin ), "ofa-changed", G_CALLBACK( on_args_changed ), page );
	priv->args_bin = bin;

	return( GTK_WIDGET( bin ));
}

static const gchar *
render_page_get_paper_name( ofaRenderPage *page )
{
	return( THIS_PAPER_NAME );
}

static GtkPageOrientation
render_page_get_page_orientation( ofaRenderPage *page )
{
	return( THIS_PAGE_ORIENTATION );
}

static void
render_page_get_print_settings( ofaRenderPage *page, GKeyFile **keyfile, gchar **group_name )
{
	myISettings *settings;

	settings = ofa_settings_get_settings( SETTINGS_TARGET_USER );
	*keyfile = my_isettings_get_keyfile( settings );
	*group_name = g_strdup( st_print_settings );
}

static GList *
render_page_get_dataset( ofaRenderPage *page )
{
	ofaBalanceRenderPrivate *priv;
	GList *dataset;
	ofaIAccountFilter *account_filter;
	ofaIDateFilter *date_filter;

	priv = OFA_BALANCE_RENDER( page )->priv;

	g_free( priv->from_account );
	g_free( priv->to_account );
	account_filter = ofa_balance_bin_get_account_filter( priv->args_bin );
	priv->from_account = g_strdup( ofa_iaccount_filter_get_account( account_filter, IACCOUNT_FILTER_FROM ));
	priv->to_account = g_strdup( ofa_iaccount_filter_get_account( account_filter, IACCOUNT_FILTER_TO ));
	priv->all_accounts = ofa_iaccount_filter_get_all_accounts( account_filter );

	priv->accounts_balance = ofa_balance_bin_get_accounts_balance( priv->args_bin );

	date_filter = ofa_balance_bin_get_date_filter( priv->args_bin );
	my_date_set_from_date( &priv->from_date, ofa_idate_filter_get_date( date_filter, IDATE_FILTER_FROM ));
	my_date_set_from_date( &priv->to_date, ofa_idate_filter_get_date( date_filter, IDATE_FILTER_TO ));

	dataset = ofo_entry_get_dataset_for_print_balance(
						priv->dossier,
						priv->all_accounts ? NULL : priv->from_account,
						priv->all_accounts ? NULL : priv->to_account,
						my_date_is_valid( &priv->from_date ) ? &priv->from_date : NULL,
						my_date_is_valid( &priv->to_date ) ? &priv->to_date : NULL );

	priv->count = g_list_length( dataset );

	return( dataset );
}

static void
render_page_free_dataset( ofaRenderPage *page, GList *dataset )
{
	ofs_account_balance_list_free( &dataset );
}

/*
 * ofaBalanceBin "ofa-changed" handler
 */
static void
on_args_changed( ofaBalanceBin *bin, ofaBalanceRender *page )
{
	gboolean valid;
	gchar *message;

	valid = ofa_balance_bin_is_valid( bin, &message );
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
	iface->reset_runtime = irenderable_reset_runtime;
	iface->want_groups = irenderable_want_groups;
	iface->want_new_page = irenderable_want_new_page;
	iface->get_dossier_name = irenderable_get_dossier_name;
	iface->get_page_header_title = irenderable_get_page_header_title;
	iface->get_page_header_subtitle = irenderable_get_page_header_subtitle;
	iface->draw_page_header_notes = irenderable_draw_page_header_notes;
	iface->draw_page_header_columns = irenderable_draw_page_header_columns;
	iface->is_new_group = irenderable_is_new_group;
	iface->draw_group_header = irenderable_draw_group_header;
	iface->draw_group_top_report = irenderable_draw_group_top_report;
	iface->draw_line = irenderable_draw_line;
	iface->draw_group_bottom_report = irenderable_draw_group_bottom_report;
	iface->draw_group_footer = irenderable_draw_group_footer;
	iface->draw_bottom_summary = irenderable_draw_bottom_summary;
}

static guint
irenderable_get_interface_version( const ofaIRenderable *instance )
{
	return( 1 );
}

static void
irenderable_reset_runtime( ofaIRenderable *instance )
{
	ofaBalanceRenderPrivate *priv;

	priv = OFA_BALANCE_RENDER( instance )->priv;

	g_list_free_full( priv->totals, ( GDestroyNotify ) free_currency );
	priv->totals = NULL;
}

static gboolean
irenderable_want_groups( const ofaIRenderable *instance )
{
	ofaBalanceRenderPrivate *priv;

	priv = OFA_BALANCE_RENDER( instance )->priv;
	priv->per_class = ofa_balance_bin_get_subtotal_per_class( priv->args_bin );

	return( priv->per_class );
}

static gboolean
irenderable_want_new_page( const ofaIRenderable *instance )
{
	ofaBalanceRenderPrivate *priv;

	priv = OFA_BALANCE_RENDER( instance )->priv;
	priv->new_page = ofa_balance_bin_get_new_page_per_class( priv->args_bin );

	return( priv->new_page );
}

static void
irenderable_begin_render( ofaIRenderable *instance, gdouble render_width, gdouble render_height )
{
	static const gchar *thisfn = "ofa_balance_render_irenderable_begin_render";
	ofaBalanceRenderPrivate *priv;

	/*gint entries_count, other_count, last_count;
	gdouble footer_height, balance_height, last_height, header_height;*/

	g_debug( "%s: instance=%p, render_width=%lf, render_height=%lf",
			thisfn, ( void * ) instance, render_width, render_height );

	priv = OFA_BALANCE_RENDER( instance )->priv;

	priv->render_width = render_width;
	priv->render_height = render_height;
	priv->page_margin = ofa_irenderable_get_page_margin( instance );

	/* starting from the left : body_number_ltab on the left margin */
	priv->body_number_ltab = priv->page_margin;
	priv->body_label_ltab = priv->body_number_ltab+ st_number_width + st_column_spacing;

	/* computing the width of amounts so that the four columns will
	 * take half of the page width
	 * margin+number+col+label+col+amount+col+amount+col+amount+col+amount+col+currency+margin */
	priv->amount_width = (priv->render_width/2 - priv->page_margin)/4 - st_column_spacing;
	g_debug( "%s: amount_width=%lf", thisfn, priv->amount_width );
	/* amount_width=65 instead of 80 */
	priv->amount_width = 75;

	/* starting from the right */
	priv->body_currency_ltab = priv->render_width - priv->page_margin - st_currency_width;
	priv->body_credit_solde_rtab = priv->body_currency_ltab - st_column_spacing;
	priv->body_debit_solde_rtab = priv->body_credit_solde_rtab - priv->amount_width - st_column_spacing;
	priv->body_credit_period_rtab = priv->body_debit_solde_rtab - priv->amount_width - st_column_spacing;
	priv->body_debit_period_rtab = priv->body_credit_period_rtab - priv->amount_width - st_column_spacing;

	/* max size in Pango units */
	priv->body_label_max_size = ( priv->body_debit_period_rtab - priv->amount_width - st_column_spacing - priv->body_label_ltab )*PANGO_SCALE;
}

/*
 * Accounts Balance
 */
static gchar *
irenderable_get_dossier_name( const ofaIRenderable *instance )
{
	ofaBalanceRenderPrivate *priv;
	const ofaIDBConnect *connect;
	ofaIDBMeta *meta;
	gchar *dossier_name;

	priv = OFA_BALANCE_RENDER( instance )->priv;
	connect = ofo_dossier_get_connect( priv->dossier );
	meta = ofa_idbconnect_get_meta( connect );
	dossier_name = ofa_idbmeta_get_dossier_name( meta );
	g_object_unref( meta );

	return( dossier_name );
}

static gchar *
irenderable_get_page_header_title( const ofaIRenderable *instance )
{
	ofaBalanceRenderPrivate *priv;

	priv = OFA_BALANCE_RENDER( instance )->priv;

	return(
			g_strdup( priv->accounts_balance ?
					st_page_header_title_accounts : st_page_header_title_entries ));
}

/*
 * From account xxx to account xxx - From date xxx to date xxx
 * Accounts balance doesn't specify the beginning date as this one
 * is mandatorily at the beginning of the exercice
 */
static gchar *
irenderable_get_page_header_subtitle( const ofaIRenderable *instance )
{
	ofaBalanceRenderPrivate *priv;
	GString *stitle;
	gchar *sfrom_date, *sto_date;

	priv = OFA_BALANCE_RENDER( instance )->priv;
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
			sto_date = my_date_to_str( &priv->to_date, ofa_prefs_date_display());
			g_string_append_printf( stitle, _( "As of %s" ), sto_date );
			g_free( sto_date );
		} else {
			stitle = g_string_append( stitle, _( "As of today" ));
		}

	} else if( !my_date_is_valid( &priv->from_date ) && !my_date_is_valid( &priv->to_date )){
		stitle = g_string_append( stitle, "All effect dates" );

	} else {
		sfrom_date = my_date_to_str( &priv->from_date, ofa_prefs_date_display());
		sto_date = my_date_to_str( &priv->to_date, ofa_prefs_date_display());
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

	return( g_string_free( stitle, FALSE ));
}

static void
irenderable_draw_page_header_notes( ofaIRenderable *instance, gint page_num )
{
	ofaBalanceRenderPrivate *priv;
	static const gdouble st_vspace_rate_after = 0.5;
	gdouble y;

	if( page_num == 0 ){
		priv = OFA_BALANCE_RENDER( instance )->priv;
		if( !priv->accounts_balance ){

			y = ofa_irenderable_get_last_y( instance );
			y += ofa_irenderable_set_wrapped_text( instance,
						priv->page_margin, y,
						(priv->render_width-priv->page_margin)*PANGO_SCALE,
						_( "Please note that this entries balance summary only "
							"displays the balance of the entries whose effect "
							"date is between the above date limits.\n"
							"As such, it is not intended nor expected to reflect "
							"the balance of the accounts at the end of the period." ),
						PANGO_ALIGN_LEFT );

			y += ofa_irenderable_get_text_height( instance ) * st_vspace_rate_after;
			ofa_irenderable_set_last_y( instance, y );
		}
	}
}

static void
irenderable_draw_page_header_columns( ofaIRenderable *instance, gint page_num )
{
	ofaBalanceRenderPrivate *priv;
	static gdouble st_vspace_rate_before = 0.25;
	static gdouble st_vspace_rate_after = 0.25;
	gdouble text_height, y, height, yh, hline;
	cairo_t *context;

	priv = OFA_BALANCE_RENDER( instance )->priv;

	y = ofa_irenderable_get_last_y( instance );
	text_height = ofa_irenderable_get_text_height( instance );
	context = ofa_irenderable_get_context( instance );

	/* draw three vertical white lines to visually separate the amounts */
	cairo_set_source_rgb( context, COLOR_WHITE );
	cairo_set_line_width( context, 0.5 );
	height = ofa_irenderable_get_page_header_columns_height( instance );

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
			priv->body_debit_solde_rtab, y, _( "Solde balance" ), PANGO_ALIGN_CENTER );

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
 * test if the current entry account is on the same class than the
 * previous one
 */
static gboolean
irenderable_is_new_group( const ofaIRenderable *instance, GList *current, GList *prev )
{
	ofaBalanceRenderPrivate *priv;
	ofsAccountBalance *current_sbal, *prev_sbal;
	gint current_class, prev_class;

	g_return_val_if_fail( current, FALSE );

	priv = OFA_BALANCE_RENDER( instance )->priv;

	if( priv->per_class ){

		if( !prev ){
			return( TRUE );
		}

		current_sbal = ( ofsAccountBalance * ) current->data;
		current_class = ofo_account_get_class_from_number( current_sbal->account );

		prev_sbal = ( ofsAccountBalance * ) prev->data;
		prev_class = ofo_account_get_class_from_number( prev_sbal->account );

		return( current_class != prev_class );
	}

	return( FALSE );
}

/*
 * draw account header
 * Class x - xxx
 */
static void
irenderable_draw_group_header( ofaIRenderable *instance, GList *current )
{
	ofaBalanceRenderPrivate *priv;
	static const gdouble st_vspace_rate = 0.4;
	ofsAccountBalance *sbal;
	gdouble height, y;
	gchar *str;

	priv = OFA_BALANCE_RENDER( instance )->priv;

	y = ofa_irenderable_get_last_y( instance );

	/* setup the class properties */
	sbal = ( ofsAccountBalance * ) current->data;
	priv->class_num = ofo_account_get_class_from_number( sbal->account );

	priv->class_object = ofo_class_get_by_number( priv->dossier, priv->class_num );

	g_list_free_full( priv->subtotals, ( GDestroyNotify ) free_currency );
	priv->subtotals = NULL;

	/* display the class header */
	/* label */
	str = g_strdup_printf(
				_( "Class %u - %s" ),
				priv->class_num, ofo_class_get_label( priv->class_object ));
	height = ofa_irenderable_set_text( instance, priv->page_margin, y, str, PANGO_ALIGN_LEFT );
	g_free( str );

	y += height * ( 1+st_vspace_rate );
	ofa_irenderable_set_last_y( instance, y );
}

static void
irenderable_draw_group_top_report( ofaIRenderable *instance )
{
	draw_subtotals_balance( instance, _( "Top class report : " ));
}

/*
 * num_line is counted from 0 in the page
 *
 * (printable2)width(A4)=559
 * date  journal  piece    label      debit   credit   solde
 * 10    6        max(10)  max(80)      15d      15d     15d
 */
static void
irenderable_draw_line( ofaIRenderable *instance, GList *current )
{
	ofaBalanceRenderPrivate *priv;
	gdouble y;
	ofsAccountBalance *sbal;
	ofoAccount *account;
	gchar *str;
	gdouble solde;

	priv = OFA_BALANCE_RENDER( instance )->priv;

	y = ofa_irenderable_get_last_y( instance );
	/*g_debug( "irenderable_draw_line: y=%lf, current=%p", y, ( void * ) current );*/

	sbal = ( ofsAccountBalance * ) current->data;
	g_return_if_fail( my_strlen( sbal->account ));
	account = ofo_account_get_by_number( priv->dossier, sbal->account );
	/*g_debug( "irenderable_draw_line: account=%s %s", sbal->account, ofo_account_get_label( account ));*/

	solde = 0;

	ofa_irenderable_set_text( instance,
			priv->body_number_ltab, y, sbal->account, PANGO_ALIGN_LEFT );

	ofa_irenderable_ellipsize_text( instance,
			priv->body_label_ltab, y,
			ofo_account_get_label( account ), priv->body_label_max_size );

	if( sbal->debit ){
		str = my_double_to_str( sbal->debit );
		ofa_irenderable_set_text( instance,
				priv->body_debit_period_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		solde -= sbal->debit;
	}

	if( sbal->credit ){
		str = my_double_to_str( sbal->credit );
		ofa_irenderable_set_text( instance,
				priv->body_credit_period_rtab, y, str, PANGO_ALIGN_RIGHT );
		solde += sbal->credit;
	}

	if( solde < 0 ){
		str = my_double_to_str( -1*solde );
		ofa_irenderable_set_text( instance,
				priv->body_debit_solde_rtab, y, str, PANGO_ALIGN_RIGHT );

	} else {
		str = my_double_to_str( solde );
		ofa_irenderable_set_text( instance,
				priv->body_credit_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
	}

	ofa_irenderable_set_text( instance,
			priv->body_currency_ltab, y, sbal->currency, PANGO_ALIGN_LEFT );

	if( !ofa_irenderable_is_paginating( instance )){
		priv->subtotals = add_account_balance(
							OFA_BALANCE_RENDER( instance ),
							priv->subtotals, sbal->currency, solde, sbal );
		priv->totals = add_account_balance(
							OFA_BALANCE_RENDER( instance ),
							priv->totals, sbal->currency, solde, sbal );
	}
}

static void
irenderable_draw_group_bottom_report( ofaIRenderable *instance )
{
	draw_subtotals_balance( instance, _( "Bottom class report : " ));
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

	priv = OFA_BALANCE_RENDER( instance )->priv;

	str = g_strdup_printf( _( "Class %d entries balance : "), priv->class_num );
	draw_subtotals_balance( instance, str );
	g_free( str );
}

/*
 * draw on the bottom of the last page the summary with one line per
 * currency
 */
static void
irenderable_draw_bottom_summary( ofaIRenderable *instance )
{
	ofaBalanceRenderPrivate *priv;
	static const gdouble st_vspace_rate = 0.25;
	gdouble bottom, top, vspace, req_height, height;

	priv = OFA_BALANCE_RENDER( instance )->priv;

	if( !priv->count ){
		ofa_irenderable_draw_no_data( instance );
		return;
	}

	/* bottom of the rectangle */
	bottom = ofa_irenderable_get_max_y( instance );

	/* top of the rectangle */
	height = ofa_irenderable_get_text_height( instance );
	vspace = height * st_vspace_rate;
	req_height = g_list_length( priv->totals ) * height
			+ ( 1+g_list_length( priv->totals )) * vspace;
	top = bottom - req_height;

	ofa_irenderable_draw_rect( instance, 0, top, -1, req_height );
	top += vspace;

	draw_account_balance( instance, priv->totals, top, _( "General balance : " ));

	ofa_irenderable_set_last_y( instance, ofa_irenderable_get_last_y( instance ) + req_height );
}

static void
draw_subtotals_balance( ofaIRenderable *instance, const gchar *title )
{
	ofaBalanceRenderPrivate *priv;
	static const gdouble st_vspace_rate = 0.5;
	gdouble vspace, req_height, last_y, height;

	priv = OFA_BALANCE_RENDER( instance )->priv;

	/* top of the rectangle */
	height = ofa_irenderable_get_text_height( instance );
	vspace = height * ( 1+st_vspace_rate );
	req_height = g_list_length( priv->subtotals ) * vspace;
	last_y = ofa_irenderable_get_last_y( instance );

	draw_account_balance( instance, priv->subtotals, last_y, title );

	ofa_irenderable_set_last_y( instance, last_y+req_height );
}

static void
draw_account_balance( ofaIRenderable *instance,
							GList *list, gdouble top, const gchar *title )
{
	ofaBalanceRenderPrivate *priv;
	static const gdouble st_vspace_rate = 0.5;
	GList *it;
	gboolean first;
	sCurrency *scur;
	gchar *str;
	gdouble height;

	priv = OFA_BALANCE_RENDER( instance )->priv;
	height = 0;

	for( it=list, first=TRUE ; it ; it=it->next ){
		if( first ){
			height = ofa_irenderable_set_text( instance,
						priv->body_debit_period_rtab-priv->amount_width, top,
						title, PANGO_ALIGN_RIGHT );
			first = FALSE;
			}

		scur = ( sCurrency * ) it->data;

		str = my_double_to_str( scur->period_d );
		ofa_irenderable_set_text( instance,
				priv->body_debit_period_rtab, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = my_double_to_str( scur->period_c );
		ofa_irenderable_set_text( instance,
				priv->body_credit_period_rtab, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = my_double_to_str( scur->solde_d );
		ofa_irenderable_set_text( instance,
				priv->body_debit_solde_rtab, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = my_double_to_str( scur->solde_c );
		ofa_irenderable_set_text( instance,
				priv->body_credit_solde_rtab, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		ofa_irenderable_set_text( instance,
				priv->body_currency_ltab, top, scur->currency, PANGO_ALIGN_LEFT );

		top += height * ( 1+st_vspace_rate );
	}
}

static GList *
add_account_balance( ofaBalanceRender *self, GList *list,
							const gchar *currency, gdouble solde, ofsAccountBalance *sbal )
{
	static const gchar *thisfn = "ofa_balance_render_add_account_balance";
	GList *it;
	sCurrency *scur;
	gboolean found;

	for( it=list, found=FALSE ; it ; it=it->next ){
		scur = ( sCurrency * ) it->data;
		if( !g_utf8_collate( scur->currency, currency )){
			found = TRUE;
			break;
		}
	}

	if( !found ){
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
	return( g_utf8_collate( a->currency, b->currency ));
}
