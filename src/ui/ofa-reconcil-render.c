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

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-irenderable.h"
#include "ui/ofa-reconcil-bin.h"
#include "ui/ofa-reconcil-render.h"

/* private instance data
 */
struct _ofaReconcilRenderPrivate {

	ofaHub         *hub;
	ofaReconcilBin *args_bin;

	/* internals
	 */
	gchar          *account_number;
	ofoAccount     *account;
	const gchar    *currency;
	gint            digits;				/* decimal digits for the currency */
	GDate           date;
	gint            count;				/* count of entries in the dataset */

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
	gdouble         body_ref_ltab;
	gint            body_ref_max_size;			/* Pango units */
	gdouble         body_label_ltab;
	gint            body_label_max_size;		/* Pango units */
	gdouble         body_debit_rtab;
	gdouble         body_credit_rtab;
	gdouble         body_solde_rtab;

	/* runtime
	 */
	gint           line_num;			/* line number of the full report counted from 1 */
	GDate          global_deffect;
	gdouble        account_solde;
};

/*
 * Accounts balances print uses a portrait orientation
 */
#define THIS_PAGE_ORIENTATION            GTK_PAGE_ORIENTATION_LANDSCAPE
#define THIS_PAPER_NAME                  GTK_PAPER_NAME_A4

static const gchar *st_page_header_title = N_( "Account Reconciliation Summary" );

static const gchar *st_print_settings    = "RenderReconcilPrint";

/* these are parms which describe the page layout
 */

/* the columns of the body */
static const gchar  *st_body_font        = "Sans 7";
static const gchar  *st_line_number_font = "Sans 5";
static const gdouble st_body_vspace_rate = 0.3;

#define st_effect_width                  (gdouble) 54
#define st_ledger_width                  (gdouble) 36
#define st_ref_width                     (gdouble) 64
#define st_amount_width                  (gdouble) 90
#define st_column_hspacing               (gdouble) 4

#define COLOR_BLACK                      0,      0,      0
#define COLOR_DARK_CYAN                  0,      0.5156, 0.5156
#define COLOR_GRAY                       0.6,    0.6,    0.6

static void               page_init_view( ofaPage *page );
static GtkWidget         *page_get_top_focusable_widget( const ofaPage *page );
static GtkWidget         *render_page_get_args_widget( ofaRenderPage *page );
static const gchar       *render_page_get_paper_name( ofaRenderPage *page );
static GtkPageOrientation render_page_get_page_orientation( ofaRenderPage *page );
static void               render_page_get_print_settings( ofaRenderPage *page, GKeyFile **keyfile, gchar **group_name );
static GList             *render_page_get_dataset( ofaRenderPage *page );
static void               render_page_free_dataset( ofaRenderPage *page, GList *dataset );
static void               on_args_changed( ofaReconcilBin *bin, ofaReconcilRender *page );
static void               irenderable_iface_init( ofaIRenderableInterface *iface );
static guint              irenderable_get_interface_version( const ofaIRenderable *instance );
static gchar             *irenderable_get_body_font( ofaIRenderable *instance );
static gdouble            irenderable_get_body_vspace_rate( ofaIRenderable *instance );
static void               irenderable_reset_runtime( ofaIRenderable *instance );
static void               irenderable_begin_render( ofaIRenderable *instance, gdouble render_width, gdouble render_height );
static gchar             *irenderable_get_dossier_name( const ofaIRenderable *instance );
static gchar             *irenderable_get_page_header_title( const ofaIRenderable *instance );
static gchar             *irenderable_get_page_header_subtitle( const ofaIRenderable *instance );
static void               irenderable_draw_page_header_columns( ofaIRenderable *instance, gint page_num );
static void               irenderable_draw_top_summary( ofaIRenderable *instance );
static void               irenderable_draw_line( ofaIRenderable *instance, GList *current );
static void               irenderable_draw_bottom_summary( ofaIRenderable *instance );
static gchar             *account_solde_to_str( ofaReconcilRender *self, gdouble amount );

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

	g_free( priv->account_number );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_reconcil_render_parent_class )->finalize( instance );
}

static void
reconcil_render_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_RECONCIL_RENDER( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_reconcil_render_parent_class )->dispose( instance );
}

static void
ofa_reconcil_render_init( ofaReconcilRender *self )
{
	static const gchar *thisfn = "ofa_reconcil_render_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECONCIL_RENDER( self ));
}

static void
ofa_reconcil_render_class_init( ofaReconcilRenderClass *klass )
{
	static const gchar *thisfn = "ofa_reconcil_render_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = reconcil_render_dispose;
	G_OBJECT_CLASS( klass )->finalize = reconcil_render_finalize;

	OFA_PAGE_CLASS( klass )->init_view = page_init_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = page_get_top_focusable_widget;

	OFA_RENDER_PAGE_CLASS( klass )->get_args_widget = render_page_get_args_widget;
	OFA_RENDER_PAGE_CLASS( klass )->get_paper_name = render_page_get_paper_name;
	OFA_RENDER_PAGE_CLASS( klass )->get_page_orientation = render_page_get_page_orientation;
	OFA_RENDER_PAGE_CLASS( klass )->get_print_settings = render_page_get_print_settings;
	OFA_RENDER_PAGE_CLASS( klass )->get_dataset = render_page_get_dataset;
	OFA_RENDER_PAGE_CLASS( klass )->free_dataset = render_page_free_dataset;
}

static void
page_init_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_reconcil_render_page_init_view";
	ofaReconcilRenderPrivate *priv;

	OFA_PAGE_CLASS( ofa_reconcil_render_parent_class )->init_view( page );

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( page ));

	on_args_changed( priv->args_bin, OFA_RECONCIL_RENDER( page ));

	priv->hub = ofa_page_get_hub( page );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));
}

static GtkWidget *
page_get_top_focusable_widget( const ofaPage *page )
{
	return( NULL );
}

static GtkWidget *
render_page_get_args_widget( ofaRenderPage *page )
{
	ofaReconcilRenderPrivate *priv;
	ofaReconcilBin *bin;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( page ));

	bin = ofa_reconcil_bin_new( ofa_page_get_main_window( OFA_PAGE( page )));
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
	ofaReconcilRenderPrivate *priv;
	GList *dataset;
	ofoCurrency *currency;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( page ));

	g_free( priv->account_number );
	priv->account_number = g_strdup( ofa_reconcil_bin_get_account( priv->args_bin ));
	/*g_debug( "irenderable_get_dataset: account_number=%s", priv->account_number );*/
	g_return_val_if_fail( my_strlen( priv->account_number ), NULL );

	priv->account = ofo_account_get_by_number( priv->hub, priv->account_number );
	g_return_val_if_fail( priv->account && OFO_IS_ACCOUNT( priv->account ), NULL );

	priv->currency = ofo_account_get_currency( priv->account );
	currency = ofo_currency_get_by_code( priv->hub, priv->currency );
	g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), NULL );

	priv->digits = ofo_currency_get_digits( currency );

	my_date_set_from_date( &priv->date, ofa_reconcil_bin_get_date( priv->args_bin ));

	dataset = ofo_entry_get_dataset_for_print_reconcil(
					priv->hub,
					ofo_account_get_number( priv->account ),
					&priv->date );

	priv->count = g_list_length( dataset );

	return( dataset );
}

static void
render_page_free_dataset( ofaRenderPage *page, GList *dataset )
{
	g_list_free_full( dataset, ( GDestroyNotify ) g_object_unref );
}

/*
 * ofaReconcilBin "ofa-changed" handler
 */
static void
on_args_changed( ofaReconcilBin *bin, ofaReconcilRender *page )
{
	gboolean valid;
	gchar *message;

	valid = ofa_reconcil_bin_is_valid( bin, &message );
	ofa_render_page_set_args_changed( OFA_RENDER_PAGE( page ), valid, message );
	g_free( message );
}

/**
 * ofa_reconcil_render_set_account:
 * @page:
 * @account_number:
 *
 * pwi 2014- 4-18: I do prefer expose this api and just redirect to
 * ofaReconcilBin rather than exposing an get_reconcil_bin() which
 * would return the composite widget - Say that ofaReconcilRender page
 * doesn't expose its internal composition
 */
void
ofa_reconcil_render_set_account( ofaReconcilRender *page, const gchar *account_number )
{
	ofaReconcilRenderPrivate *priv;

	g_return_if_fail( page && OFA_IS_RECONCIL_RENDER( page ));

	g_return_if_fail( !OFA_PAGE( page )->prot->dispose_has_run );

	priv = ofa_reconcil_render_get_instance_private( page );

	ofa_reconcil_bin_set_account( priv->args_bin, account_number );
	ofa_render_page_clear_drawing_area( OFA_RENDER_PAGE( page ));
}

static void
irenderable_iface_init( ofaIRenderableInterface *iface )
{
	static const gchar *thisfn = "ofa_reconcil_render_irenderable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = irenderable_get_interface_version;
	iface->get_body_font = irenderable_get_body_font;
	iface->get_body_vspace_rate = irenderable_get_body_vspace_rate;
	iface->begin_render = irenderable_begin_render;
	iface->reset_runtime = irenderable_reset_runtime;
	iface->get_dossier_name = irenderable_get_dossier_name;
	iface->get_page_header_title = irenderable_get_page_header_title;
	iface->get_page_header_subtitle = irenderable_get_page_header_subtitle;
	iface->draw_page_header_columns = irenderable_draw_page_header_columns;
	iface->draw_top_summary = irenderable_draw_top_summary;
	iface->draw_line = irenderable_draw_line;
	iface->draw_bottom_summary = irenderable_draw_bottom_summary;
}

static guint
irenderable_get_interface_version( const ofaIRenderable *instance )
{
	return( 1 );
}

static gchar *
irenderable_get_body_font( ofaIRenderable *instance )
{
	return( g_strdup( st_body_font ));
}

static gdouble
irenderable_get_body_vspace_rate( ofaIRenderable *instance )
{
	return( st_body_vspace_rate );
}

static void
irenderable_reset_runtime( ofaIRenderable *instance )
{
	ofaReconcilRenderPrivate *priv;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	priv->line_num = 0;
	priv->account_solde = 0;
}

/*
 * mainly here: compute the tab positions
 * this is called after having loaded the dataset
 */
static void
irenderable_begin_render( ofaIRenderable *instance, gdouble render_width, gdouble render_height )
{
	static const gchar *thisfn = "ofa_reconcil_render_irenderable_begin_render";
	ofaReconcilRenderPrivate *priv;
	gchar *str;
	gdouble number_width;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	g_debug( "%s: instance=%p, render_width=%lf, render_height=%lf",
			thisfn, ( void * ) instance, render_width, render_height );

	priv->render_width = render_width;
	priv->render_height = render_height;
	priv->page_margin = ofa_irenderable_get_page_margin( instance );

	/* keep the leftest column to display a line number */
	str = g_strdup_printf( "%d", priv->count );
	ofa_irenderable_set_font( instance, st_line_number_font );
	number_width = ofa_irenderable_get_text_width( instance, str );
	priv->body_count_rtab = priv->page_margin + number_width + st_column_hspacing;
	g_free( str );

	/* starting from the left : body_effect_ltab on the left margin */
	priv->body_effect_ltab = priv->body_count_rtab + st_column_hspacing;
	priv->body_ledger_ltab = priv->body_effect_ltab+ st_effect_width + st_column_hspacing;
	priv->body_ref_ltab = priv->body_ledger_ltab + st_ledger_width + st_column_hspacing;
	priv->body_label_ltab = priv->body_ref_ltab + st_ref_width + st_column_hspacing;

	/* starting from the right */
	priv->body_solde_rtab = priv->render_width - priv->page_margin;
	priv->body_credit_rtab = priv->body_solde_rtab - st_amount_width - st_column_hspacing;
	priv->body_debit_rtab = priv->body_credit_rtab - st_amount_width - st_column_hspacing;

	/* max size in Pango units */
	priv->body_ref_max_size = st_ref_width*PANGO_SCALE;
	priv->body_label_max_size = ( priv->body_debit_rtab - st_amount_width - st_column_hspacing - priv->body_label_ltab )*PANGO_SCALE;
}

static gchar *
irenderable_get_dossier_name( const ofaIRenderable *instance )
{
	ofaReconcilRenderPrivate *priv;
	const ofaIDBConnect *connect;
	ofaIDBMeta *meta;
	gchar *dossier_name;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	connect = ofa_hub_get_connect( priv->hub );
	meta = ofa_idbconnect_get_meta( connect );
	dossier_name = ofa_idbmeta_get_dossier_name( meta );
	g_object_unref( meta );

	return( dossier_name );
}

static gchar *
irenderable_get_page_header_title( const ofaIRenderable *instance )
{
	return( g_strdup( st_page_header_title ));
}

/*
 * Account xxxx
 */
static gchar *
irenderable_get_page_header_subtitle( const ofaIRenderable *instance )
{
	ofaReconcilRenderPrivate *priv;
	gchar *str;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	/* account number and label in line 4 */
	str = g_strdup_printf(
				_( "Account %s - %s" ),
				ofo_account_get_number( priv->account ),
				ofo_account_get_label( priv->account ));

	return( str );
}

static void
irenderable_draw_page_header_columns( ofaIRenderable *instance, gint page_num )
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

static void
irenderable_draw_top_summary( ofaIRenderable *instance )
{
	ofaReconcilRenderPrivate *priv;
	static const gdouble st_vspace_rate = 0.4;
	gchar *str, *str_solde, *sdate;
	gdouble y, height;
	GDate date;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	y = ofa_irenderable_get_last_y( instance );

	ofo_account_get_global_deffect( priv->account, &priv->global_deffect );
	my_date_set_from_date( &date, &priv->global_deffect );
	if( !my_date_is_valid( &date )){
		my_date_set_from_date( &date, &priv->date );
	}
	sdate = my_date_to_str( &date, ofa_prefs_date_display());

	priv->account_solde = ofo_account_get_global_solde( priv->account );
	str_solde = account_solde_to_str( OFA_RECONCIL_RENDER( instance ), priv->account_solde );

	str = g_strdup_printf( _( "Account solde on %s is %s" ), sdate, str_solde );

	g_free( sdate );
	g_free( str_solde );

	height = ofa_irenderable_set_text( instance,
			priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );

	g_free( str );

	y += height * ( 1+st_vspace_rate );
	ofa_irenderable_set_last_y( instance, y );
}

static void
irenderable_draw_line( ofaIRenderable *instance, GList *current )
{
	ofaReconcilRenderPrivate *priv;
	gdouble y;
	gchar *str;
	ofoEntry *entry;
	const gchar *cstr;
	gdouble amount;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	y = ofa_irenderable_get_last_y( instance );
	entry = OFO_ENTRY( current->data );

	str = my_date_to_str( ofo_entry_get_deffect( entry ), ofa_prefs_date_display());
	ofa_irenderable_set_text( instance,
			priv->body_effect_ltab, y, str, PANGO_ALIGN_LEFT );
	g_free( str );

	ofa_irenderable_set_text( instance,
			priv->body_ledger_ltab, y, ofo_entry_get_ledger( entry ), PANGO_ALIGN_LEFT );

	cstr = ofo_entry_get_ref( entry );
	if( my_strlen( cstr )){
		ofa_irenderable_ellipsize_text( instance,
				priv->body_ref_ltab, y, cstr, priv->body_ref_max_size );
	}

	ofa_irenderable_ellipsize_text( instance,
			priv->body_label_ltab, y, ofo_entry_get_label( entry ), priv->body_label_max_size );

	amount = ofo_entry_get_debit( entry );
	if( amount ){
		str = my_double_to_str( amount );
		ofa_irenderable_set_text( instance,
				priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		priv->account_solde += amount;
	}

	amount = ofo_entry_get_credit( entry );
	if( amount ){
		str = my_double_to_str( amount );
		ofa_irenderable_set_text( instance,
				priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		priv->account_solde -= amount;
	}

	/* current solde */
	ofa_irenderable_set_color( instance, COLOR_DARK_CYAN );
	str = my_double_to_str( priv->account_solde );
	ofa_irenderable_set_text( instance,
			priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	/* display the line number starting from 1 */
	ofa_irenderable_set_color( instance, COLOR_GRAY );
	ofa_irenderable_set_font( instance, st_line_number_font );
	str = g_strdup_printf( "%d", ++priv->line_num );
	ofa_irenderable_set_text( instance, priv->body_count_rtab, y+1.5, str, PANGO_ALIGN_RIGHT );
	g_free( str );
}

static void
irenderable_draw_bottom_summary( ofaIRenderable *instance )
{
	ofaReconcilRenderPrivate *priv;
	static const gdouble st_vspace_rate = 0.25;
	gdouble y, height;
	GDate date;
	gchar *str, *sdate, *str_amount, *font;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	g_return_if_fail( my_date_is_valid( &priv->date ));

	y = ofa_irenderable_get_last_y( instance );

	my_date_set_from_date( &date, &priv->global_deffect );
	if( !my_date_is_valid( &date ) || my_date_compare( &date, &priv->date ) < 0 ){
		my_date_set_from_date( &date, &priv->date );
	}
	sdate = my_date_to_str( &date, ofa_prefs_date_display());

	str_amount = account_solde_to_str( OFA_RECONCIL_RENDER( instance ), priv->account_solde );
	str = g_strdup_printf( _( "Reconciliated account solde on %s is %s" ), sdate, str_amount );
	g_free( sdate );
	g_free( str_amount );

	height = ofa_irenderable_set_text( instance,
			priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );

	g_free( str );

	y += height * ( 1+st_vspace_rate );

	ofa_irenderable_set_color( instance, COLOR_BLACK );
	font = irenderable_get_body_font( instance );
	ofa_irenderable_set_font( instance, font );
	g_free( font );

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

	y += height * ( 1+st_vspace_rate );
	ofa_irenderable_set_last_y( instance, y );
}

static gchar *
account_solde_to_str( ofaReconcilRender *self, gdouble amount )
{
	ofaReconcilRenderPrivate *priv;
	gchar *str, *str_amount;

	priv = ofa_reconcil_render_get_instance_private( self );

	str_amount = my_double_to_str_ex( amount, priv->digits );
	str = g_strdup_printf( "%s %s", str_amount, priv->currency );
	/*
	g_debug( "account_solde_to_str: digits=%d, currency=%s, amount=%lf, str=%s",
			priv->digits, priv->currency, amount, str );
			*/
	g_free( str_amount );

	return( str );
}
