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

	/* internals
	 */
	gchar          *settings_prefix;
	ofaIGetter     *getter;
	gchar          *account_number;
	ofoAccount     *account;
	ofoCurrency    *currency;
	GDate           date;
	gint            count;					/* count of entries in the dataset */

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
	gint            line_num;				/* entry line number of the full report counted from 1 */
	gint            batline_num;			/* bat line number of the full report counted from 1 */
	GDate           global_deffect;
	gdouble         solde_debit;
	gdouble         solde_credit;
	gdouble         account_solde;
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
static const gchar  *st_body_font           = "Sans 7";
static const gchar  *st_bat_header_font     = "Sans Italic 7";
static const gchar  *st_bat_line_font       = "Sans Italic 7";
static const gchar  *st_bottom_summary_font = "Sans Italic 6";
static const gchar  *st_line_number_font    = "Sans 5";
static const gdouble st_body_vspace_rate    = 0.3;

#define st_effect_width                  (gdouble) 54
#define st_ledger_width                  (gdouble) 36
#define st_ref_width                     (gdouble) 64
#define st_amount_width                  (gdouble) 90
#define st_column_hspacing               (gdouble) 4

#define COLOR_BLACK                      0,      0,      0
#define COLOR_DARK_CYAN                  0,      0.5156, 0.5156
#define COLOR_DARK_GRAY                  0.251,  0.251,  0.251
#define COLOR_GRAY                       0.6,    0.6,    0.6
#define COLOR_MAROON                     0.4,    0.2,    0

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
static gchar             *irenderable_get_body_font( ofaIRenderable *instance );
static gdouble            irenderable_get_body_vspace_rate( ofaIRenderable *instance );
static void               irenderable_reset_runtime( ofaIRenderable *instance );
static gboolean           irenderable_want_groups( const ofaIRenderable *instance );
static gboolean           irenderable_want_line_separation( const ofaIRenderable *instance );
static void               irenderable_begin_render( ofaIRenderable *instance, gdouble render_width, gdouble render_height );
static gchar             *irenderable_get_dossier_name( const ofaIRenderable *instance );
static gchar             *irenderable_get_page_header_title( const ofaIRenderable *instance );
static gchar             *irenderable_get_page_header_subtitle( const ofaIRenderable *instance );
static gchar             *irenderable_get_page_header_subtitle2( const ofaIRenderable *instance );
static void               irenderable_draw_page_header_columns( ofaIRenderable *instance, gint page_num );
static void               irenderable_draw_top_summary( ofaIRenderable *instance );
static gboolean           irenderable_is_new_group( const ofaIRenderable *instance, GList *current, GList *prev );
static void               irenderable_draw_group_header( ofaIRenderable *instance, GList *current );
static void               irenderable_draw_line( ofaIRenderable *instance, GList *current );
static void               draw_line_entry( ofaIRenderable *instance, ofoEntry *entry );
static void               draw_line_bat( ofaIRenderable *instance, ofoBatLine *batline );
static void               irenderable_draw_bottom_summary( ofaIRenderable *instance );
static gchar             *account_solde_to_str( ofaReconcilRender *self, gdouble amount );
static gchar             *get_render_date( ofaReconcilRender *self );
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

	priv->getter = ofa_page_get_getter( OFA_PAGE( page ));

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
	ofaReconcilRenderPrivate *priv;
	GList *dataset, *batlist;
	const gchar *cur_code;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( page ));

	g_free( priv->account_number );
	priv->account_number = g_strdup( ofa_reconcil_args_get_account( priv->args_bin ));
	/*g_debug( "irenderable_get_dataset: account_number=%s", priv->account_number );*/
	g_return_val_if_fail( my_strlen( priv->account_number ), NULL );

	priv->account = ofo_account_get_by_number( priv->getter, priv->account_number );
	g_return_val_if_fail( priv->account && OFO_IS_ACCOUNT( priv->account ), NULL );

	cur_code = ofo_account_get_currency( priv->account );
	g_return_val_if_fail( my_strlen( cur_code ), NULL );

	priv->currency = ofo_currency_get_by_code( priv->getter, cur_code );
	g_return_val_if_fail( priv->currency && OFO_IS_CURRENCY( priv->currency ), NULL );

	my_date_set_from_date( &priv->date, ofa_reconcil_args_get_date( priv->args_bin ));

	dataset = ofo_entry_get_dataset_for_print_reconcil(
					priv->getter,
					priv->account_number,
					&priv->date );

	batlist = ofo_bat_line_get_dataset_for_print_reconcil(
					priv->getter,
					priv->account_number );

	if( batlist && g_list_length( batlist ) > 0 ){
		dataset = g_list_concat( dataset, batlist );
	}

	priv->count = g_list_length( dataset );

	return( dataset );
}

static void
render_page_v_free_dataset( ofaRenderPage *page, GList *dataset )
{
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

	ofa_reconcil_args_set_account( priv->args_bin, account_number );
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
	iface->want_groups = irenderable_want_groups;
	iface->want_line_separation = irenderable_want_line_separation;
	iface->get_dossier_name = irenderable_get_dossier_name;
	iface->get_page_header_title = irenderable_get_page_header_title;
	iface->get_page_header_subtitle = irenderable_get_page_header_subtitle;
	iface->get_page_header_subtitle2 = irenderable_get_page_header_subtitle2;
	iface->draw_page_header_columns = irenderable_draw_page_header_columns;
	iface->draw_top_summary = irenderable_draw_top_summary;
	iface->is_new_group = irenderable_is_new_group;
	iface->draw_group_header = irenderable_draw_group_header;
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
	priv->batline_num = 0;
	priv->account_solde = 0;
	priv->solde_debit = 0;
	priv->solde_credit = 0;
}

static gboolean
irenderable_want_groups( const ofaIRenderable *instance )
{
	return( TRUE );
}

static gboolean
irenderable_want_line_separation( const ofaIRenderable *instance )
{
	return( FALSE );
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
	ofaHub *hub;
	const ofaIDBConnect *connect;
	ofaIDBDossierMeta *dossier_meta;
	const gchar *dossier_name;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	hub = ofa_igetter_get_hub( priv->getter );
	connect = ofa_hub_get_connect( hub );
	dossier_meta = ofa_idbconnect_get_dossier_meta( connect );
	dossier_name = ofa_idbdossier_meta_get_dossier_name( dossier_meta );

	return( g_strdup( dossier_name ));
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

/*
 * Rendering date
 */
static gchar *
irenderable_get_page_header_subtitle2( const ofaIRenderable *instance )
{
	gchar *sdate;

	sdate = get_render_date( OFA_RECONCIL_RENDER( instance ));

	return( sdate );
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
	sdate = my_date_to_str( &date, ofa_prefs_date_display( priv->getter ));

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

/*
 * We are drawing bat lines as a new group: just test if this is the
 * first bat line
 */
static gboolean
irenderable_is_new_group( const ofaIRenderable *instance, GList *current, GList *prev )
{
	ofoBase *current_obj, *prev_obj;

	g_return_val_if_fail( current, FALSE );

	if( !prev ){
		return( FALSE );
	}

	current_obj = OFO_BASE( current->data );
	prev_obj = OFO_BASE( prev->data );

	return( OFO_IS_BAT_LINE( current_obj ) && OFO_IS_ENTRY( prev_obj ));
}

/*
 * draw a header before first bat line
 */
static void
irenderable_draw_group_header( ofaIRenderable *instance, GList *current )
{
	ofaReconcilRenderPrivate *priv;
	gdouble y;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	ofa_irenderable_set_color( instance, COLOR_DARK_CYAN );
	ofa_irenderable_set_font( instance, st_bat_header_font );

	y = ofa_irenderable_get_last_y( instance );

	ofa_irenderable_set_text(
			instance, priv->body_effect_ltab, y, _( "Unconciliated bank transactions "), PANGO_ALIGN_LEFT );

	y += ofa_irenderable_get_line_height( instance );;
	ofa_irenderable_set_last_y( instance, y );
}

static void
irenderable_draw_line( ofaIRenderable *instance, GList *current )
{
	if( OFO_IS_ENTRY( current->data )){
		draw_line_entry( instance, OFO_ENTRY( current->data ));

	} else if( OFO_IS_BAT_LINE( current->data )){
		draw_line_bat( instance, OFO_BAT_LINE( current->data ));
	}
}

static void
draw_line_entry( ofaIRenderable *instance, ofoEntry *entry )
{
	ofaReconcilRenderPrivate *priv;
	gdouble y;
	gchar *str;
	const gchar *cstr;
	gdouble amount;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	y = ofa_irenderable_get_last_y( instance );

	str = my_date_to_str( ofo_entry_get_deffect( entry ), ofa_prefs_date_display( priv->getter ));
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
		str = ofa_amount_to_str( amount, priv->currency, priv->getter );
		ofa_irenderable_set_text( instance,
				priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		priv->account_solde += amount;
		priv->solde_debit += amount;
	}

	amount = ofo_entry_get_credit( entry );
	if( amount ){
		str = ofa_amount_to_str( amount, priv->currency, priv->getter );
		ofa_irenderable_set_text( instance,
				priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		priv->account_solde -= amount;
		priv->solde_credit += amount;
	}

	/* current solde */
	ofa_irenderable_set_color( instance, COLOR_DARK_CYAN );
	str = ofa_amount_to_str( priv->account_solde, priv->currency, priv->getter );
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
draw_line_bat( ofaIRenderable *instance, ofoBatLine *batline )
{
	ofaReconcilRenderPrivate *priv;
	gdouble y;
	gchar *str;
	const gchar *cstr;
	gdouble amount;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	y = ofa_irenderable_get_last_y( instance );

	ofa_irenderable_set_color( instance, COLOR_DARK_GRAY );
	ofa_irenderable_set_font( instance, st_bat_line_font );

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
		priv->account_solde += amount;
		priv->solde_debit += amount;
	} else {
		str = ofa_amount_to_str( -amount, priv->currency, priv->getter );
		ofa_irenderable_set_text( instance,
				priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		priv->account_solde += amount;
		priv->solde_credit += amount;
	}

	/* current solde */
	ofa_irenderable_set_color( instance, COLOR_DARK_CYAN );
	str = ofa_amount_to_str( priv->account_solde, priv->currency, priv->getter );
	ofa_irenderable_set_text( instance,
			priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	/* display the line number starting from 1 */
	ofa_irenderable_set_color( instance, COLOR_GRAY );
	ofa_irenderable_set_font( instance, st_line_number_font );
	str = g_strdup_printf( "%d", ++priv->batline_num );
	ofa_irenderable_set_text( instance, priv->body_count_rtab, y+1.5, str, PANGO_ALIGN_RIGHT );
	g_free( str );
}

static void
irenderable_draw_bottom_summary( ofaIRenderable *instance )
{
	ofaReconcilRenderPrivate *priv;
	static const gdouble st_vspace_rate = 0.25;
	gdouble y, line_height, height;
	gchar *str, *sdate, *str_amount;
	ofoBat *bat;
	ofxAmount bat_solde, solde;
	const GDate *bat_end;
	const gchar *bat_currency;
	GString *bat_str;

	priv = ofa_reconcil_render_get_instance_private( OFA_RECONCIL_RENDER( instance ));

	g_return_if_fail( my_date_is_valid( &priv->date ));

	/* debit and credit totals
	 */
	y = ofa_irenderable_get_last_y( instance );
	line_height = ofa_irenderable_get_line_height( instance );

	ofa_irenderable_set_color( instance, COLOR_DARK_CYAN );
	ofa_irenderable_set_font( instance, st_body_font );

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

	ofa_irenderable_set_color( instance, COLOR_DARK_CYAN );
	ofa_irenderable_set_summary_font( instance );

	sdate = get_render_date( OFA_RECONCIL_RENDER( instance ));
	str_amount = account_solde_to_str( OFA_RECONCIL_RENDER( instance ), priv->account_solde );

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
	ofa_irenderable_set_font( instance, st_bottom_summary_font );

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
		ofa_irenderable_set_summary_font( instance );

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

		solde = priv->account_solde + bat_solde;

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

static gchar *
get_render_date( ofaReconcilRender *self )
{
	ofaReconcilRenderPrivate *priv;
	GDate date;
	gchar *sdate;

	priv = ofa_reconcil_render_get_instance_private( self );

	my_date_set_from_date( &date, &priv->global_deffect );
	if( !my_date_is_valid( &date ) || my_date_compare( &date, &priv->date ) < 0 ){
		my_date_set_from_date( &date, &priv->date );
	}
	sdate = my_date_to_str( &date, ofa_prefs_date_display( priv->getter ));

	return( sdate );
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
