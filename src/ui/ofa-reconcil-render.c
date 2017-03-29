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
#include "api/ofo-base.h"
#include "api/ofo-bat.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "ui/ofa-reconcil-args.h"
#include "ui/ofa-reconcil-render.h"

/* private instance data
 */
typedef struct {

	ofaReconcilArgs *args_bin;

	/* initialization
	 */
	gchar          *settings_prefix;
	ofaIGetter     *getter;

	/* from ofaReconcilArgs
	 */
	gchar          *account_number;
	ofoAccount     *account;
	ofoCurrency    *currency;
	GDate           arg_date;

	/* computed account solde at the closest date
	 */
	ofxAmount       account_solde;
	GDate           account_deffect;

	/* print datas
	 */
	gdouble         render_width;
	gdouble         render_height;
	gdouble         page_margin;

	/* layout for entry line
	 */
	gdouble         body_count_rtab;
	gdouble         body_effect_ltab;
	gdouble         body_ledger_ltab;
	gint            body_ledger_max_size;		/* Pango units */
	gdouble         body_ref_ltab;
	gint            body_ref_max_size;			/* Pango units */
	gdouble         body_label_ltab;
	gint            body_label_max_size;		/* Pango units */
	gdouble         body_debit_rtab;
	gdouble         body_credit_rtab;
	gdouble         body_solde_rtab;

	/* runtime
	 */
	gboolean        body_entry;
	guint           line_num;				/* entry line number of the full report counted from 1 */
	guint           batline_num;			/* bat line number of the full report counted from 1 */
	ofxAmount       current_solde;
	GDate           current_date;
	ofxAmount       solde_debit;
	ofxAmount       solde_credit;
}
	ofaReconcilRenderPrivate;

/*
 * Accounts balances print uses a portrait orientation
 */
#define THIS_PAGE_ORIENTATION            GTK_PAGE_ORIENTATION_LANDSCAPE
#define THIS_PAPER_NAME                  GTK_PAPER_NAME_A4

static const gchar *st_page_header_title = N_( "Account Reconciliation Summary" );

/*
 * these are parms which describe the page layout
 */

/* the columns of the body */
static const gchar  *st_title2_font         = "Sans Bold 8";
static const gchar  *st_summary0_font       = "Sans Bold 7";
static const gchar  *st_summary1_font       = "Sans 7";
static const gchar  *st_bat_header_font     = "Sans Italic 7";
static const gchar  *st_body_entry_font     = "Sans 6";
static const gchar  *st_body_batline_font   = "Sans Italic 6";
static const gchar  *st_last_summary_font   = "Sans Italic 6";
static const gchar  *st_line_number_font    = "Sans 5";

#define COLOR_BLACK                      0,      0,      0			/* last summary text */
#define COLOR_DARK_GRAY                  0.251,  0.251,  0.251		/* bat lines (special font, special color) */
#define COLOR_GRAY                       0.6,    0.6,    0.6		/* line number */
#define COLOR_MAROON                     0.4,    0.2,    0			/* comparison of soldes */

static GtkWidget         *page_v_get_top_focusable_widget( const ofaPage *page );
static void               paned_page_v_init_view( ofaPanedPage *page );
static GtkWidget         *render_page_v_get_args_widget( ofaRenderPage *page );
static const gchar       *render_page_v_get_paper_name( ofaRenderPage *page );
static GtkPageOrientation render_page_v_get_page_orientation( ofaRenderPage *page );
static void               render_page_v_get_print_settings( ofaRenderPage *page, GKeyFile **keyfile, gchar **group_name );
static GList             *render_page_v_get_dataset( ofaRenderPage *page );
static void               render_page_v_free_dataset( ofaRenderPage *page, GList *dataset );
static void               on_args_changed( ofaReconcilArgs *bin, ofaReconcilRender *page );
static void               irenderable_iface_init( ofaIRenderableInterface *iface );
static guint              irenderable_get_interface_version( const ofaIRenderable *instance );
static void               irenderable_begin_render( ofaIRenderable *instance );
static gchar             *irenderable_get_dossier_label( const ofaIRenderable *instance );
static void               irenderable_draw_page_header_title( ofaIRenderable *instance );
static void               irenderable_draw_header_column_names( ofaIRenderable *instance );
static void               irenderable_draw_top_summary( ofaIRenderable *instance );
static gboolean           irenderable_is_new_group( const ofaIRenderable *instance, GList *prev, GList *line, ofeIRenderableBreak *sep );
static void               irenderable_draw_group_header( ofaIRenderable *instance );
static void               irenderable_draw_line( ofaIRenderable *instance );
static void               draw_line_entry( ofaIRenderable *instance, ofoEntry *entry );
static void               draw_line_bat( ofaIRenderable *instance, ofoBatLine *batline );
static void               irenderable_draw_bottom_report( ofaIRenderable *instance );
static void               draw_top_bottom_summary( ofaIRenderable *instance, const GDate *date, const gchar *font );
static void               irenderable_draw_last_summary( ofaIRenderable *instance );
static const gchar       *irenderable_get_summary_font( const ofaIRenderable *instance, guint page_num );
static const gchar       *irenderable_get_body_font( const ofaIRenderable *instance );
static void               irenderable_clear_runtime_data( ofaIRenderable *instance );
static void               draw_line_num( ofaIRenderable *instance, guint line_num );
static void               draw_bat_title( ofaIRenderable *instance );
static gchar             *account_solde_to_str( ofaReconcilRender *self, gdouble amount );
static void               read_settings( ofaReconcilRender *self );
static void               write_settings( ofaReconcilRender *self );

G_DEFINE_TYPE_EXTENDED( ofaReconcilRender, ofa_reconcil_render, OFA_TYPE_RENDER_PAGE, 0,
		G_ADD_PRIVATE( ofaReconcilRender )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IRENDERABLE, irenderable_iface_init ))

static void
reconcil_render_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_reconcil_render_finalize";
	ofaReconcilRenderPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECONCIL_RENDER( instance ));

	/* free data members here */
	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	g_free( priv->settings_prefix );
	g_free( priv->account_number );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_reconcil_render_parent_class )->finalize( instance );
}

static void
reconcil_render_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_RECONCIL_RENDER( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		write_settings( OFA_RECONCIL_RENDER( instance ));

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_reconcil_render_parent_class )->dispose( instance );
}

static void
ofa_reconcil_render_init( ofaReconcilRender *self )
{
	static const gchar *thisfn = "ofa_reconcil_render_init";
	ofaReconcilRenderPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECONCIL_RENDER( self ));

	priv = ofa_reconcil_render_get_instance_private( self );

	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_reconcil_render_class_init( ofaReconcilRenderClass *klass )
{
	static const gchar *thisfn = "ofa_reconcil_render_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = reconcil_render_dispose;
	G_OBJECT_CLASS( klass )->finalize = reconcil_render_finalize;

	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = page_v_get_top_focusable_widget;

	OFA_PANED_PAGE_CLASS( klass )->init_view = paned_page_v_init_view;

	OFA_RENDER_PAGE_CLASS( klass )->get_args_widget = render_page_v_get_args_widget;
	OFA_RENDER_PAGE_CLASS( klass )->get_paper_name = render_page_v_get_paper_name;
	OFA_RENDER_PAGE_CLASS( klass )->get_page_orientation = render_page_v_get_page_orientation;
	OFA_RENDER_PAGE_CLASS( klass )->get_print_settings = render_page_v_get_print_settings;
	OFA_RENDER_PAGE_CLASS( klass )->get_dataset = render_page_v_get_dataset;
	OFA_RENDER_PAGE_CLASS( klass )->free_dataset = render_page_v_free_dataset;
}

static void
paned_page_v_init_view( ofaPanedPage *page )
{
	static const gchar *thisfn = "ofa_reconcil_render_paned_page_v_init_view";
	ofaReconcilRenderPrivate *priv;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( page ));

	on_args_changed( priv->args_bin, OFA_RECONCIL_RENDER( page ));

	read_settings( OFA_RECONCIL_RENDER( page ));
}

static GtkWidget *
page_v_get_top_focusable_widget( const ofaPage *page )
{
	return( NULL );
}

static GtkWidget *
render_page_v_get_args_widget( ofaRenderPage *page )
{
	ofaReconcilRenderPrivate *priv;
	ofaReconcilArgs *bin;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( page ));

	priv->getter = ofa_page_get_getter( OFA_PAGE( page ));

	bin = ofa_reconcil_args_new( priv->getter, priv->settings_prefix );
	g_signal_connect( G_OBJECT( bin ), "ofa-changed", G_CALLBACK( on_args_changed ), page );
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
	ofaReconcilRenderPrivate *priv;
	myISettings *settings;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( page ));

	settings = ofa_igetter_get_user_settings( priv->getter );
	*keyfile = my_isettings_get_keyfile( settings );
	*group_name = g_strdup_printf( "%s-print", priv->settings_prefix );
}

static GList *
render_page_v_get_dataset( ofaRenderPage *page )
{
	static const gchar *thisfn = "ofa_reconcil_render_render_page_v_get_dataset";
	ofaReconcilRenderPrivate *priv;
	GList *dataset, *batlist;
	const gchar *cur_code;
	gchar *str;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( page ));

	g_free( priv->account_number );
	priv->account_number = g_strdup( ofa_reconcil_args_get_account( priv->args_bin ));
	g_return_val_if_fail( my_strlen( priv->account_number ), NULL );

	priv->account = ofo_account_get_by_number( priv->getter, priv->account_number );
	g_return_val_if_fail( priv->account && OFO_IS_ACCOUNT( priv->account ), NULL );

	cur_code = ofo_account_get_currency( priv->account );
	g_return_val_if_fail( my_strlen( cur_code ), NULL );

	priv->currency = ofo_currency_get_by_code( priv->getter, cur_code );
	g_return_val_if_fail( priv->currency && OFO_IS_CURRENCY( priv->currency ), NULL );

	my_date_set_from_date( &priv->arg_date, ofa_reconcil_args_get_date( priv->args_bin ));

	dataset = ofo_entry_get_dataset_for_print_reconcil(
					priv->getter,
					priv->account_number,
					&priv->arg_date );

	batlist = ofo_bat_line_get_dataset_for_print_reconcil(
					priv->getter,
					priv->account_number );

	if( batlist && g_list_length( batlist ) > 0 ){
		dataset = g_list_concat( dataset, batlist );
	}

	/* compute the solde of the account from the last archived balance
	 * up to the requested date - return the actual greated effect date
	 */
	priv->account_solde = ofo_account_get_solde_at_date( priv->account, &priv->arg_date, &priv->account_deffect );
	str = my_date_to_str( &priv->account_deffect, MY_DATE_SQL );
	g_debug( "%s: account_solde=%lf, deffect=%s", thisfn, priv->account_solde, str );
	g_free( str );

	return( dataset );
}

static void
render_page_v_free_dataset( ofaRenderPage *page, GList *dataset )
{
	ofaReconcilRenderPrivate *priv;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( page ));

	g_free( priv->account_number );
	priv->account_number = NULL;
	priv->account = NULL;
	priv->currency = NULL;
	my_date_clear( &priv->arg_date );

	g_list_free_full( dataset, ( GDestroyNotify ) g_object_unref );
}

/*
 * ofaReconcilArgs "ofa-changed" handler
 */
static void
on_args_changed( ofaReconcilArgs *bin, ofaReconcilRender *page )
{
	gboolean valid;
	gchar *message;

	valid = ofa_reconcil_args_is_valid( bin, &message );
	ofa_render_page_set_args_changed( OFA_RENDER_PAGE( page ), valid, message );
	g_free( message );
}

/**
 * ofa_reconcil_render_set_account:
 * @page:
 * @account_number:
 *
 * pwi 2014- 4-18: I do prefer expose this api and just redirect to
 * ofaReconcilArgs rather than exposing an get_reconcil_args() which
 * would return the composite widget (say that ofaReconcilRender page
 * doesn't expose its internal composition).
 */
void
ofa_reconcil_render_set_account( ofaReconcilRender *page, const gchar *account_number )
{
	ofaReconcilRenderPrivate *priv;

	g_return_if_fail( page && OFA_IS_RECONCIL_RENDER( page ));

	g_return_if_fail( !OFA_PAGE( page )->prot->dispose_has_run );

	priv = ofa_reconcil_render_get_instance_private( page );

	ofa_reconcil_args_set_account( priv->args_bin, account_number );
}

static void
irenderable_iface_init( ofaIRenderableInterface *iface )
{
	static const gchar *thisfn = "ofa_reconcil_render_irenderable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = irenderable_get_interface_version;
	iface->begin_render = irenderable_begin_render;
	iface->get_dossier_label = irenderable_get_dossier_label;
	iface->draw_page_header_title = irenderable_draw_page_header_title;
	iface->draw_header_column_names = irenderable_draw_header_column_names;
	iface->draw_top_summary = irenderable_draw_top_summary;
	iface->is_new_group = irenderable_is_new_group;
	iface->draw_group_header = irenderable_draw_group_header;
	iface->draw_line = irenderable_draw_line;
	iface->draw_bottom_report = irenderable_draw_bottom_report;
	iface->draw_last_summary = irenderable_draw_last_summary;
	iface->get_summary_font = irenderable_get_summary_font;
	iface->get_body_font = irenderable_get_body_font;
	iface->clear_runtime_data = irenderable_clear_runtime_data;
}

static guint
irenderable_get_interface_version( const ofaIRenderable *instance )
{
	return( 1 );
}

/*
 * mainly here: compute the tab positions
 * this is called after having loaded the dataset
 */
static void
irenderable_begin_render( ofaIRenderable *instance )
{
	static const gchar *thisfn = "ofa_reconcil_render_irenderable_begin_render";
	ofaReconcilRenderPrivate *priv;
	gchar *str;
	gdouble number_width, date_width, ledger_width, piece_width, amount_width, spacing;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv->render_width = ofa_irenderable_get_render_width( instance );
	priv->render_height = ofa_irenderable_get_render_height( instance );
	priv->page_margin = ofa_irenderable_get_page_margin( instance );

	/* compute the width of some columns */
	ofa_irenderable_set_font( instance, st_line_number_font );
	str = g_strdup_printf( "%d", g_list_length( ofa_irenderable_get_dataset( instance )));
	number_width = ofa_irenderable_get_text_width( instance, str );
	g_free( str );

	ofa_irenderable_set_font( instance, ofa_irenderable_get_body_font( instance ));
	date_width = ofa_irenderable_get_text_width( instance, "9999-99-99-" );
	ledger_width = ofa_irenderable_get_text_width( instance, "XXXXXXX" );
	piece_width = ofa_irenderable_get_text_width( instance, "XXX 99999999" );
	amount_width = ofa_irenderable_get_text_width( instance, "9,999,999,999.99" );

	spacing = ofa_irenderable_get_columns_spacing( instance );

	/* keep the leftest column to display a line number */
	priv->body_count_rtab = priv->page_margin + number_width;
	priv->body_effect_ltab = priv->body_count_rtab + spacing;
	priv->body_ledger_ltab = priv->body_effect_ltab+ date_width + spacing;
	priv->body_ref_ltab = priv->body_ledger_ltab + ledger_width + spacing;
	priv->body_label_ltab = priv->body_ref_ltab + piece_width + spacing;

	/* starting from the right */
	priv->body_solde_rtab = priv->render_width - priv->page_margin;
	priv->body_credit_rtab = priv->body_solde_rtab - amount_width - spacing;
	priv->body_debit_rtab = priv->body_credit_rtab - amount_width - spacing;

	/* max size in Pango units */
	priv->body_ledger_max_size = ledger_width*PANGO_SCALE;
	priv->body_ref_max_size = piece_width*PANGO_SCALE;
	priv->body_label_max_size = ( priv->body_debit_rtab - amount_width - spacing - priv->body_label_ltab )*PANGO_SCALE;
}

static gchar *
irenderable_get_dossier_label( const ofaIRenderable *instance )
{
	ofaReconcilRenderPrivate *priv;
	ofaHub *hub;
	ofoDossier *dossier;
	const gchar *label;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	hub = ofa_igetter_get_hub( priv->getter );
	dossier = ofa_hub_get_dossier( hub );
	label = ofo_dossier_get_label( dossier );

	return( g_strdup( label ));
}

/*
 * Title is three lines
 */
static void
irenderable_draw_page_header_title( ofaIRenderable *instance )
{
	ofaReconcilRenderPrivate *priv;
	gdouble r, g, b, y, height;
	gchar *str;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	ofa_irenderable_get_title_color( instance, &r, &g, &b );
	ofa_irenderable_set_color( instance, r, g, b );
	y = ofa_irenderable_get_last_y( instance );

	/* line 1 - Reconciliation summary */
	ofa_irenderable_set_font( instance,
			ofa_irenderable_get_title_font( instance, ofa_irenderable_get_current_page_num( instance )));
	height = ofa_irenderable_set_text( instance, priv->render_width/2, y, st_page_header_title, PANGO_ALIGN_CENTER );
	y += height;

	/* line 2 - account */
	ofa_irenderable_set_font( instance, st_title2_font );
	str = g_strdup_printf(
				_( "Account %s - %s" ),
				ofo_account_get_number( priv->account ),
				ofo_account_get_label( priv->account ));
	height = ofa_irenderable_set_text( instance, priv->render_width/2, y, str, PANGO_ALIGN_CENTER );
	g_free( str );
	y += height;

	/* line 3 - reconciliation date */
	str = my_date_to_str( &priv->arg_date, ofa_prefs_date_display( priv->getter ));
	height = ofa_irenderable_set_text( instance, priv->render_width/2, y, str, PANGO_ALIGN_CENTER );
	g_free( str );
	y += height;

	ofa_irenderable_set_last_y( instance, y );
}

static void
irenderable_draw_header_column_names( ofaIRenderable *instance )
{
	ofaReconcilRenderPrivate *priv;
	static gdouble st_vspace_rate = 0.5;
	gdouble y, text_height, vspace;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	y = ofa_irenderable_get_last_y( instance );
	text_height = ofa_irenderable_get_text_height( instance );
	vspace = text_height * st_vspace_rate;
	y+= vspace;

	/* column headers */
	ofa_irenderable_set_text( instance,
			priv->body_effect_ltab, y, _( "Effect date" ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			priv->body_ledger_ltab, y, _( "Ledger" ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			priv->body_ref_ltab, y, _( "Piece" ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			priv->body_label_ltab, y, _( "Label" ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			priv->body_debit_rtab, y, _( "Debit" ), PANGO_ALIGN_RIGHT );

	ofa_irenderable_set_text( instance,
			priv->body_credit_rtab, y, _( "Credit" ), PANGO_ALIGN_RIGHT );

	ofa_irenderable_set_text( instance,
			priv->body_solde_rtab, y, _( "Solde" ), PANGO_ALIGN_RIGHT );

	/* this set the 'y' height just after the column headers */
	y += text_height * ( 1+st_vspace_rate );
	ofa_irenderable_set_last_y( instance, y );
}

/*
 * top summary: display the account of the solde at the found date
 * if no entry has been found (thus zero), use the arg date
 */
static void
irenderable_draw_top_summary( ofaIRenderable *instance )
{
	ofaReconcilRenderPrivate *priv;
	const GDate *date;
	const gchar *font;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	if( ofa_irenderable_get_current_page_num( instance ) == 0 ){
		date = priv->account_solde ? &priv->account_deffect : &priv->arg_date;
		font = st_summary0_font;

	} else {
		date = &priv->current_date;
		font = st_summary1_font;
	}

	draw_top_bottom_summary( instance, date, font );
}

/*
 * Have a group line if @line is a batline
 * and @prev was an entry or null
 *
 * We do not manage here the first entry.
 */
static gboolean
irenderable_is_new_group( const ofaIRenderable *instance, GList *prev, GList *line, ofeIRenderableBreak *sep )
{
	gboolean new_group;

	new_group = !prev || !line || ( OFO_IS_ENTRY( prev->data ) && OFO_IS_BAT_LINE( line->data ));
	*sep = IRENDERABLE_BREAK_NONE;

	return( new_group );
}

static void
irenderable_draw_group_header( ofaIRenderable *instance )
{
	GList *line;

	line = ofa_irenderable_get_current_line( instance );

	/* have a group line if current is a batline
	 * and prev was an entry or null */
	if( line && OFO_IS_BAT_LINE( line->data )){
		draw_bat_title( instance );
	}
}

static void
irenderable_draw_line( ofaIRenderable *instance )
{
	GList *line;

	line = ofa_irenderable_get_current_line( instance );
	if( line ){
		if( OFO_IS_ENTRY( line->data )){
			draw_line_entry( instance, OFO_ENTRY( line->data ));

		} else if( OFO_IS_BAT_LINE( line->data )){
			draw_line_bat( instance, OFO_BAT_LINE( line->data ));
		}
	}
}

static void
draw_line_entry( ofaIRenderable *instance, ofoEntry *entry )
{
	ofaReconcilRenderPrivate *priv;
	gdouble y;
	gchar *str;
	const gchar *cstr;
	ofxAmount amount;
	gdouble r, g, b;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	y = ofa_irenderable_get_last_y( instance );

	str = my_date_to_str( ofo_entry_get_deffect( entry ), ofa_prefs_date_display( priv->getter ));
	ofa_irenderable_set_text( instance,
			priv->body_effect_ltab, y, str, PANGO_ALIGN_LEFT );
	g_free( str );

	ofa_irenderable_ellipsize_text( instance,
			priv->body_ledger_ltab, y, ofo_entry_get_ledger( entry ), priv->body_ledger_max_size );

	cstr = ofo_entry_get_ref( entry );
	if( my_strlen( cstr )){
		ofa_irenderable_ellipsize_text( instance,
				priv->body_ref_ltab, y, cstr, priv->body_ref_max_size );
	}

	ofa_irenderable_ellipsize_text( instance,
			priv->body_label_ltab, y, ofo_entry_get_label( entry ), priv->body_label_max_size );

	amount = ofo_entry_get_debit( entry );
	if( amount ){
		str = ofa_amount_to_str( amount, priv->currency, priv->getter );
		ofa_irenderable_set_text( instance,
				priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		priv->current_solde += amount;
		priv->solde_debit += amount;
	}

	amount = ofo_entry_get_credit( entry );
	if( amount ){
		str = ofa_amount_to_str( amount, priv->currency, priv->getter );
		ofa_irenderable_set_text( instance,
				priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		priv->current_solde -= amount;
		priv->solde_credit += amount;
	}

	/* current solde */
	ofa_irenderable_get_summary_color( instance, &r, &g, &b );
	ofa_irenderable_set_color( instance, r, g, b );
	str = ofa_amount_to_str( priv->current_solde, priv->currency, priv->getter );
	ofa_irenderable_set_text( instance,
			priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	/* display the line number starting from 1 */
	draw_line_num( instance, ++priv->line_num );
}

static void
draw_line_bat( ofaIRenderable *instance, ofoBatLine *batline )
{
	ofaReconcilRenderPrivate *priv;
	gdouble y;
	gchar *str;
	const gchar *cstr;
	ofxAmount amount;
	gdouble r, g, b;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	y = ofa_irenderable_get_last_y( instance );

	priv->body_entry = FALSE;
	ofa_irenderable_set_color( instance, COLOR_DARK_GRAY );
	ofa_irenderable_set_font( instance, ofa_irenderable_get_body_font( instance ));

	str = my_date_to_str( ofo_bat_line_get_deffect( batline ), ofa_prefs_date_display( priv->getter ));
	ofa_irenderable_set_text( instance, priv->body_effect_ltab, y, str, PANGO_ALIGN_LEFT );
	g_free( str );

	cstr = ofo_bat_line_get_ref( batline );
	if( my_strlen( cstr )){
		ofa_irenderable_ellipsize_text( instance, priv->body_ref_ltab, y, cstr, priv->body_ref_max_size );
	}

	ofa_irenderable_ellipsize_text( instance,
			priv->body_label_ltab, y, ofo_bat_line_get_label( batline ), priv->body_label_max_size );

	amount = -1 * ofo_bat_line_get_amount( batline );
	if( amount > 0 ){
		str = ofa_amount_to_str( amount, priv->currency, priv->getter );
		ofa_irenderable_set_text( instance,
				priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		priv->current_solde += amount;
		priv->solde_debit += amount;
	} else {
		str = ofa_amount_to_str( -amount, priv->currency, priv->getter );
		ofa_irenderable_set_text( instance,
				priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		priv->current_solde += amount;
		priv->solde_credit += amount;
	}

	/* current solde */
	ofa_irenderable_get_summary_color( instance, &r, &g, &b );
	ofa_irenderable_set_color( instance, r, g, b );
	str = ofa_amount_to_str( priv->current_solde, priv->currency, priv->getter );
	ofa_irenderable_set_text( instance,
			priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	/* display the line number starting from 1 */
	draw_line_num( instance, ++priv->batline_num );
}

/*
 * bottom report at the end of a page
 */
static void
irenderable_draw_bottom_report( ofaIRenderable *instance )
{
	ofaReconcilRenderPrivate *priv;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	draw_top_bottom_summary( instance, &priv->current_date, st_summary1_font );
}

/*
 * Draw the account solde as a report, either at the top or at the end
 * of each page.
 *
 * The font is selected depending of we are writing the first top summary,
 * or the top summary for other pages, or a bottom report
 */
static void
draw_top_bottom_summary( ofaIRenderable *instance, const GDate *date, const gchar *font )
{
	ofaReconcilRenderPrivate *priv;
	static const gdouble st_vspace_after = 0.25;
	gchar *str, *str_solde, *sdate;
	gdouble y, height;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	ofa_irenderable_set_font( instance, font );

	sdate = my_date_to_str( date, ofa_prefs_date_display( priv->getter ));
	str_solde = account_solde_to_str( OFA_RECONCIL_RENDER( instance ), priv->current_solde );
	str = g_strdup_printf( _( "Account solde on %s is %s" ), sdate, str_solde );

	y = ofa_irenderable_get_last_y( instance );
	height = ofa_irenderable_set_text( instance, priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
	y += height * ( 1+st_vspace_after );
	ofa_irenderable_set_last_y( instance, y );

	g_free( sdate );
	g_free( str_solde );
	g_free( str );

}

static void
irenderable_draw_last_summary( ofaIRenderable *instance )
{
	ofaReconcilRenderPrivate *priv;
	static const gdouble st_vspace_rate = 0.25;
	gdouble y, line_height, height, r, g, b;
	gchar *str, *sdate, *str_amount;
	ofoBat *bat;
	ofxAmount bat_solde, solde;
	const GDate *bat_end;
	const gchar *bat_currency;
	GString *bat_str;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	g_return_if_fail( my_date_is_valid( &priv->arg_date ));
	priv->body_entry = TRUE;

	/* debit and credit totals
	 */
	y = ofa_irenderable_get_last_y( instance );
	line_height = ofa_irenderable_get_line_height( instance );

	ofa_irenderable_get_summary_color( instance, &r, &g, &b );
	ofa_irenderable_set_color( instance, r, g, b );
	ofa_irenderable_set_font( instance, ofa_irenderable_get_body_font( instance ));

	str = ofa_amount_to_str( priv->solde_debit, priv->currency, priv->getter );
	ofa_irenderable_set_text( instance,
			priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	str = ofa_amount_to_str( priv->solde_credit, priv->currency, priv->getter );
	ofa_irenderable_set_text( instance,
			priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	/* reconciliated account solde
	 */
	y += line_height * ( 1+st_vspace_rate );

	ofa_irenderable_set_font( instance, ofa_irenderable_get_summary_font( instance, 0 ));

	sdate = my_date_to_str( &priv->account_deffect, ofa_prefs_date_display( priv->getter ));
	str_amount = account_solde_to_str( OFA_RECONCIL_RENDER( instance ), priv->current_solde );

	str = g_strdup_printf( _( "Reconciliated account solde on %s is %s" ), sdate, str_amount );

	g_free( sdate );
	g_free( str_amount );

	ofa_irenderable_set_text( instance,
			priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );

	g_free( str );

	/* caution
	 */
	y += line_height * ( 1+st_vspace_rate );

	ofa_irenderable_set_color( instance, COLOR_BLACK );
	ofa_irenderable_set_font( instance, st_last_summary_font );

	height = ofa_irenderable_set_wrapped_text( instance,
				priv->page_margin, y,
				(priv->render_width-priv->page_margin)*PANGO_SCALE,
				_( "This reconciliated solde "
					"should be the same, though inversed, "
					"that the one of the account extraction sent by your bank.\n"
					"If this is not the case, then you have most probably "
					"forgotten to reconciliate "
					"some of the above entries, or some other entries have been recorded "
					"by your bank, are present in your account extraction, but are not "
					"found in your books." ), PANGO_ALIGN_LEFT );

	/* BAT solde
	 */
	bat = ofo_bat_get_most_recent_for_account( priv->getter, priv->account_number );
	if( bat ){
		ofa_irenderable_set_font( instance, ofa_irenderable_get_summary_font( instance, 0 ));

		bat_solde = ofo_bat_get_end_solde( bat );
		bat_end = ofo_bat_get_end_date( bat );
		bat_currency = ofo_bat_get_currency( bat );

		bat_str = g_string_new( "" );

		sdate = my_date_to_str( bat_end, ofa_prefs_date_display( priv->getter ));
		str_amount = ofa_amount_to_str( bat_solde, priv->currency, priv->getter );
		g_string_append_printf( bat_str, _( "Bank solde on %s is %s" ), sdate, str_amount );
		g_free( str_amount );
		g_free( sdate );

		if( bat_currency ){
			g_string_append_printf( bat_str, " %s", bat_currency );
		}

		bat_str = g_string_append( bat_str, ": " );

		solde = priv->current_solde + bat_solde;

		if( ofa_amount_is_zero( solde, priv->currency )){
			bat_str = g_string_append( bat_str, "OK" );
		} else {
			str_amount = ofa_amount_to_str( solde, priv->currency, priv->getter );
			g_string_append_printf( bat_str, "diff=%s", str_amount );
			g_free( str_amount );
		}

		y += height + line_height * st_vspace_rate;

		ofa_irenderable_set_color( instance, COLOR_MAROON );
		ofa_irenderable_set_text( instance, priv->body_solde_rtab, y, bat_str->str, PANGO_ALIGN_RIGHT );

		g_string_free( bat_str, TRUE );
	}

	y += line_height * ( 1+st_vspace_rate );
	ofa_irenderable_set_last_y( instance, y );
}

static const gchar *
irenderable_get_summary_font( const ofaIRenderable *instance, guint page_num )
{
	return( page_num == 0 ? st_summary0_font : st_summary1_font );
}

/*
 * here, we manage two body fonts, for the entries and for the batlines.
 */
static const gchar *
irenderable_get_body_font( const ofaIRenderable *instance )
{
	ofaReconcilRenderPrivate *priv;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	return( priv->body_entry ? st_body_entry_font : st_body_batline_font );
}

static void
irenderable_clear_runtime_data( ofaIRenderable *instance )
{
	ofaReconcilRenderPrivate *priv;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	priv->body_entry = TRUE;
	priv->line_num = 0;
	priv->batline_num = 0;
	priv->current_solde = priv->account_solde;
	priv->solde_debit = 0;
	priv->solde_credit = 0;
}

/*
 * draw the line number of the leftest column
 */
static void
draw_line_num( ofaIRenderable *instance, guint line_num )
{
	ofaReconcilRenderPrivate *priv;
	gchar *str;
	gdouble y;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	ofa_irenderable_set_color( instance, COLOR_GRAY );
	ofa_irenderable_set_font( instance, st_line_number_font );

	str = g_strdup_printf( "%d", line_num );

	y = ofa_irenderable_get_last_y( instance );
	ofa_irenderable_set_text( instance, priv->body_count_rtab, y+1, str, PANGO_ALIGN_RIGHT );

	g_free( str );

	y += ofa_irenderable_get_line_height( instance );
	ofa_irenderable_set_last_y( instance, y );
}

/*
 * draw a header before first bat line
 */
static void
draw_bat_title( ofaIRenderable *instance )
{
	ofaReconcilRenderPrivate *priv;
	gdouble y, r, g, b;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	ofa_irenderable_get_summary_color( instance, &r, &g, &b );
	ofa_irenderable_set_color( instance, r, g, b );
	ofa_irenderable_set_font( instance, st_bat_header_font );

	y = ofa_irenderable_get_last_y( instance );

	ofa_irenderable_set_text(
			instance, priv->body_effect_ltab, y, _( "Unconciliated bank transactions "), PANGO_ALIGN_LEFT );

	y += ofa_irenderable_get_line_height( instance );

	ofa_irenderable_set_last_y( instance, y );
}

static gchar *
account_solde_to_str( ofaReconcilRender *self, gdouble amount )
{
	ofaReconcilRenderPrivate *priv;
	gchar *str, *str_amount;

	priv = ofa_reconcil_render_get_instance_private( self );

	str_amount = ofa_amount_to_str( amount, priv->currency, priv->getter );
	str = g_strdup_printf( "%s %s", str_amount, ofo_currency_get_code( priv->currency ));
	/*
	g_debug( "account_solde_to_str: digits=%d, currency=%s, amount=%lf, str=%s",
			priv->digits, priv->currency, amount, str );
			*/
	g_free( str_amount );

	return( str );
}

/*
 * settings = paned_position;
 */
static void
read_settings( ofaReconcilRender *self )
{
	ofaReconcilRenderPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;
	GtkWidget *paned;
	gint pos;
	gchar *key;

	priv = ofa_reconcil_render_get_instance_private( self );

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
write_settings( ofaReconcilRender *self )
{
	ofaReconcilRenderPrivate *priv;
	myISettings *settings;
	GtkWidget *paned;
	gchar *str, *key;

	priv = ofa_reconcil_render_get_instance_private( self );

	paned = ofa_render_page_get_top_paned( OFA_RENDER_PAGE( self ));

	str = g_strdup_printf( "%d;",
			gtk_paned_get_position( GTK_PANED( paned )));

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( key );
	g_free( str );
}
