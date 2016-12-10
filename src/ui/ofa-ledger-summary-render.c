/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofs-currency.h"

#include "ui/ofa-iaccount-filter.h"
#include "ui/ofa-ledger-summary-bin.h"
#include "ui/ofa-ledger-summary-render.h"

/* private instance data
 */
typedef struct {

	ofaLedgerSummaryBin *args_bin;

	/* internals
	 */
	GDate                from_date;
	GDate                to_date;
	gulong               count;					/* total count of entries */

	/* print datas
	 */
	gdouble              render_width;
	gdouble              render_height;
	gdouble              page_margin;

	/* layout for ledger line
	 */
	gdouble              body_ledcode_ltab;
	gdouble              body_ledlabel_ltab;
	gdouble              body_ledlabel_max_size;
	gdouble              body_debit_rtab;
	gdouble              body_credit_rtab;
	gdouble              body_currency_rtab;

	/* total general
	 */
	GList               *report_totals;		/* all totals per currency */
}
	ofaLedgerSummaryRenderPrivate;

#define THIS_PAGE_ORIENTATION            GTK_PAGE_ORIENTATION_LANDSCAPE
#define THIS_PAPER_NAME                  GTK_PAPER_NAME_A4

static const gchar *st_page_header_title = N_( "General Ledgers Summary" );

static const gchar *st_page_settings     = "ofaLedgerSummaryRender-settings";
static const gchar *st_print_settings    = "ofaLedgerSummaryRender-print";

/* these are parms which describe the page layout
 */

/* the columns of the body */
static const gint st_body_font_size      = 9;

#define st_mnemo_width                   (gdouble) 54/9*st_body_font_size
#define st_amount_width                  (gdouble) 90/9*st_body_font_size
#define st_currency_width                (gdouble) 23/10*st_body_font_size
#define st_column_hspacing               (gdouble) 4

static GtkWidget         *page_v_get_top_focusable_widget( const ofaPage *page );
static void               paned_page_v_init_view( ofaPanedPage *page );
static GtkWidget         *render_page_v_get_args_widget( ofaRenderPage *page );
static const gchar       *render_page_v_get_paper_name( ofaRenderPage *page );
static GtkPageOrientation render_page_v_get_page_orientation( ofaRenderPage *page );
static void               render_page_v_get_print_settings( ofaRenderPage *page, GKeyFile **keyfile, gchar **group_name );
static GList             *render_page_v_get_dataset( ofaRenderPage *page );
static void               on_args_changed( ofaLedgerSummaryBin *bin, ofaLedgerSummaryRender *page );
static void               irenderable_iface_init( ofaIRenderableInterface *iface );
static guint              irenderable_get_interface_version( const ofaIRenderable *instance );
static void               irenderable_reset_runtime( ofaIRenderable *instance );
static void               irenderable_begin_render( ofaIRenderable *instance, gdouble render_width, gdouble render_height );
static gchar             *irenderable_get_dossier_name( const ofaIRenderable *instance );
static gchar             *irenderable_get_page_header_title( const ofaIRenderable *instance );
static gchar             *irenderable_get_page_header_subtitle( const ofaIRenderable *instance );
static void               irenderable_draw_page_header_columns( ofaIRenderable *instance, gint page_num );
static void               irenderable_draw_line( ofaIRenderable *instance, GList *current );
static void               irenderable_draw_bottom_summary( ofaIRenderable *instance );
static void               get_settings( ofaLedgerSummaryRender *self );
static void               set_settings( ofaLedgerSummaryRender *self );

G_DEFINE_TYPE_EXTENDED( ofaLedgerSummaryRender, ofa_ledger_summary_render, OFA_TYPE_RENDER_PAGE, 0,
		G_ADD_PRIVATE( ofaLedgerSummaryRender )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IRENDERABLE, irenderable_iface_init ))

static void
ledger_summary_render_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledger_summary_render_finalize";
	ofaLedgerSummaryRenderPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGER_SUMMARY_RENDER( instance ));

	/* free data members here */
	priv = ofa_ledger_summary_render_get_instance_private( OFA_LEDGER_SUMMARY_RENDER( instance ));

	ofs_currency_list_free( &priv->report_totals );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_summary_render_parent_class )->finalize( instance );
}

static void
ledger_summary_render_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_LEDGER_SUMMARY_RENDER( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		set_settings( OFA_LEDGER_SUMMARY_RENDER( instance ));

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_summary_render_parent_class )->dispose( instance );
}

static void
ofa_ledger_summary_render_init( ofaLedgerSummaryRender *self )
{
	static const gchar *thisfn = "ofa_ledger_summary_render_init";

	g_return_if_fail( OFA_IS_LEDGER_SUMMARY_RENDER( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_ledger_summary_render_class_init( ofaLedgerSummaryRenderClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_summary_render_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledger_summary_render_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledger_summary_render_finalize;

	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = page_v_get_top_focusable_widget;

	OFA_PANED_PAGE_CLASS( klass )->init_view = paned_page_v_init_view;

	OFA_RENDER_PAGE_CLASS( klass )->get_args_widget = render_page_v_get_args_widget;
	OFA_RENDER_PAGE_CLASS( klass )->get_paper_name = render_page_v_get_paper_name;
	OFA_RENDER_PAGE_CLASS( klass )->get_page_orientation = render_page_v_get_page_orientation;
	OFA_RENDER_PAGE_CLASS( klass )->get_print_settings = render_page_v_get_print_settings;
	OFA_RENDER_PAGE_CLASS( klass )->get_dataset = render_page_v_get_dataset;
}

static void
paned_page_v_init_view( ofaPanedPage *page )
{
	static const gchar *thisfn = "ofa_ledger_summary_render_paned_page_v_init_view";
	ofaLedgerSummaryRenderPrivate *priv;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_ledger_summary_render_get_instance_private( OFA_LEDGER_SUMMARY_RENDER( page ));

	on_args_changed( priv->args_bin, OFA_LEDGER_SUMMARY_RENDER( page ));
	get_settings( OFA_LEDGER_SUMMARY_RENDER( page ));
}

static GtkWidget *
page_v_get_top_focusable_widget( const ofaPage *page )
{
	return( NULL );
}

static GtkWidget *
render_page_v_get_args_widget( ofaRenderPage *page )
{
	ofaLedgerSummaryRenderPrivate *priv;
	ofaLedgerSummaryBin *bin;

	priv = ofa_ledger_summary_render_get_instance_private( OFA_LEDGER_SUMMARY_RENDER( page ));

	bin = ofa_ledger_summary_bin_new( OFA_IGETTER( page ));
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
	myISettings *settings;

	settings = ofa_settings_get_settings( SETTINGS_TARGET_USER );
	*keyfile = my_isettings_get_keyfile( settings );
	*group_name = g_strdup( st_print_settings );
}

static GList *
render_page_v_get_dataset( ofaRenderPage *page )
{
	ofaLedgerSummaryRenderPrivate *priv;
	GList *dataset;
	ofaIDateFilter *date_filter;
	ofaHub *hub;

	priv = ofa_ledger_summary_render_get_instance_private( OFA_LEDGER_SUMMARY_RENDER( page ));

	hub = ofa_igetter_get_hub( OFA_IGETTER( page ));

	date_filter = ofa_ledger_summary_bin_get_date_filter( priv->args_bin );
	my_date_set_from_date( &priv->from_date, ofa_idate_filter_get_date( date_filter, IDATE_FILTER_FROM ));
	my_date_set_from_date( &priv->to_date, ofa_idate_filter_get_date( date_filter, IDATE_FILTER_TO ));

	dataset = ofo_ledger_get_dataset( hub );

	return( dataset );
}

/*
 * ofaLedgerSummaryBin "ofa-changed" handler
 */
static void
on_args_changed( ofaLedgerSummaryBin *bin, ofaLedgerSummaryRender *page )
{
	gboolean valid;
	gchar *message;

	valid = ofa_ledger_summary_bin_is_valid( bin, &message );
	ofa_render_page_set_args_changed( OFA_RENDER_PAGE( page ), valid, message );
	g_free( message );
}

static void
irenderable_iface_init( ofaIRenderableInterface *iface )
{
	static const gchar *thisfn = "ofa_ledger_summary_render_irenderable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = irenderable_get_interface_version;
	iface->begin_render = irenderable_begin_render;
	iface->reset_runtime = irenderable_reset_runtime;
	iface->get_dossier_name = irenderable_get_dossier_name;
	iface->get_page_header_title = irenderable_get_page_header_title;
	iface->get_page_header_subtitle = irenderable_get_page_header_subtitle;
	iface->draw_page_header_columns = irenderable_draw_page_header_columns;
	iface->draw_line = irenderable_draw_line;
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
	ofaLedgerSummaryRenderPrivate *priv;

	priv = ofa_ledger_summary_render_get_instance_private( OFA_LEDGER_SUMMARY_RENDER( instance ));

	priv->count = 0;
	ofs_currency_list_free( &priv->report_totals );
}

/*
 * mainly here: compute the tab positions
 */
static void
irenderable_begin_render( ofaIRenderable *instance, gdouble render_width, gdouble render_height )
{
	static const gchar *thisfn = "ofa_ledger_summary_render_irenderable_begin_render";
	ofaLedgerSummaryRenderPrivate *priv;

	priv = ofa_ledger_summary_render_get_instance_private( OFA_LEDGER_SUMMARY_RENDER( instance ));

	g_debug( "%s: instance=%p, render_width=%lf, render_height=%lf",
			thisfn, ( void * ) instance, render_width, render_height );

	priv->render_width = render_width;
	priv->render_height = render_height;
	priv->page_margin = ofa_irenderable_get_page_margin( instance );

	/* starting from the left */
	priv->body_ledcode_ltab = priv->page_margin;
	priv->body_ledlabel_ltab = priv->body_ledcode_ltab + st_mnemo_width + st_column_hspacing;

	/* starting from the right */
	priv->body_currency_rtab = priv->render_width - priv->page_margin;
	priv->body_credit_rtab = priv->body_currency_rtab - st_currency_width - st_column_hspacing;
	priv->body_debit_rtab = priv->body_credit_rtab - st_amount_width - st_column_hspacing;

	/* max size in Pango units */
	priv->body_ledlabel_max_size = ( priv->body_debit_rtab - st_amount_width - st_column_hspacing )*PANGO_SCALE;
}

static gchar *
irenderable_get_dossier_name( const ofaIRenderable *instance )
{
	ofaHub *hub;
	const ofaIDBConnect *connect;
	ofaIDBDossierMeta *meta;
	const gchar *dossier_name;

	hub = ofa_igetter_get_hub( OFA_IGETTER( instance ));
	connect = ofa_hub_get_connect( hub );
	meta = ofa_idbconnect_get_dossier_meta( connect );
	dossier_name = ofa_idbdossier_meta_get_dossier_name( meta );
	g_object_unref( meta );

	return( g_strdup( dossier_name ));
}

static gchar *
irenderable_get_page_header_title( const ofaIRenderable *instance )
{
	return( g_strdup( st_page_header_title ));
}

static gchar *
irenderable_get_page_header_subtitle( const ofaIRenderable *instance )
{
	ofaLedgerSummaryRenderPrivate *priv;
	gchar *sfrom_date, *sto_date;
	GString *stitle;

	priv = ofa_ledger_summary_render_get_instance_private( OFA_LEDGER_SUMMARY_RENDER( instance ));

	/* recall of date selections in line 4 */
	stitle = g_string_new( "" );

	if( !my_date_is_valid( &priv->from_date ) && !my_date_is_valid( &priv->to_date )){
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
irenderable_draw_page_header_columns( ofaIRenderable *instance, gint page_num )
{
	ofaLedgerSummaryRenderPrivate *priv;
	static gdouble st_vspace_rate = 0.5;
	gdouble y, text_height, vspace;

	priv = ofa_ledger_summary_render_get_instance_private( OFA_LEDGER_SUMMARY_RENDER( instance ));

	y = ofa_irenderable_get_last_y( instance );
	text_height = ofa_irenderable_get_text_height( instance );
	vspace = text_height * st_vspace_rate;
	y+= vspace;

	/* column headers */
	ofa_irenderable_set_text( instance,
			priv->body_ledcode_ltab, y,
			_( "Mnemo" ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			priv->body_ledlabel_ltab, y,
			_( "Label" ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			priv->body_debit_rtab, y,
			_( "Debit" ), PANGO_ALIGN_RIGHT );

	ofa_irenderable_set_text( instance,
			priv->body_credit_rtab, y,
			_( "Credit" ), PANGO_ALIGN_RIGHT );

	/* no header for currency */

	/* this set the 'y' height just after the column headers */
	y += text_height * ( 1+st_vspace_rate );
	ofa_irenderable_set_last_y( instance, y );
}

static void
irenderable_draw_line( ofaIRenderable *instance, GList *current )
{
	ofaLedgerSummaryRenderPrivate *priv;
	ofoLedger *ledger;
	GSList *mnemos_list;
	GList *entries, *it, *ledger_currencies;
	const gchar *cledmnemo, *ccuriso;
	ofoEntry *entry;
	ofsCurrency *scur;
	ofxAmount debit, credit;
	gboolean is_paginating, is_empty;
	gdouble y, line_height;
	gboolean first;
	gchar *str;
	ofaHub *hub;

	priv = ofa_ledger_summary_render_get_instance_private( OFA_LEDGER_SUMMARY_RENDER( instance ));

	hub = ofa_igetter_get_hub( OFA_IGETTER( instance ));
	is_paginating = ofa_irenderable_is_paginating( instance );

	/* take ledger properties */
	ledger = OFO_LEDGER( current->data );
	ledger_currencies = NULL;
	cledmnemo = ofo_ledger_get_mnemo( ledger );
	mnemos_list = g_slist_prepend( NULL, ( gpointer ) cledmnemo );

	/* take the entries for this ledger */
	entries = ofo_entry_get_dataset_for_print_by_ledger(
						hub, mnemos_list,
						my_date_is_valid( &priv->from_date ) ? &priv->from_date : NULL,
						my_date_is_valid( &priv->to_date ) ? &priv->to_date : NULL );

	g_slist_free( mnemos_list );
	is_empty = ( g_list_length( entries ) == 0 );

	/* compute the balance per currencies */
	for( it=entries ; it ; it=it->next ){
		entry = OFO_ENTRY( it->data );

		ccuriso = ofo_entry_get_currency( entry );
		debit = ofo_entry_get_debit( entry );
		credit = ofo_entry_get_credit( entry );

		ofs_currency_add_by_code(
				&ledger_currencies, hub, ccuriso,
				is_paginating ? 0 : debit, is_paginating ? 0 : credit );

		priv->count += 1;
	}

	ofo_entry_free_dataset( entries );

	/* draw a line per currency */
	y = ofa_irenderable_get_last_y( instance );
	line_height = ofa_irenderable_get_line_height( instance );
	first = TRUE;

	if( is_empty ){
		ofa_irenderable_set_text( instance,
				priv->body_ledcode_ltab, y, cledmnemo, PANGO_ALIGN_LEFT );
		ofa_irenderable_set_text( instance,
				priv->body_ledlabel_ltab, y, ofo_ledger_get_label( ledger ), PANGO_ALIGN_LEFT );
		/* Aligning on the decimal point would be a pain.
		 * Would it be worth !? */
		ofa_irenderable_set_text( instance,
				priv->body_debit_rtab, y, "0", PANGO_ALIGN_RIGHT );
		ofa_irenderable_set_text( instance,
				priv->body_credit_rtab, y, "0", PANGO_ALIGN_RIGHT );
	} else {
		for( it=ledger_currencies ; it ; it=it->next ){
			scur = ( ofsCurrency * ) it->data;

			if( first ){
				ofa_irenderable_set_text( instance,
						priv->body_ledcode_ltab, y, cledmnemo, PANGO_ALIGN_LEFT );
				ofa_irenderable_set_text( instance,
						priv->body_ledlabel_ltab, y, ofo_ledger_get_label( ledger ), PANGO_ALIGN_LEFT );
			}

			str = ofa_amount_to_str( scur->debit, scur->currency );
			ofa_irenderable_set_text( instance,
					priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
			g_free( str );

			str = ofa_amount_to_str( scur->credit, scur->currency );
			ofa_irenderable_set_text( instance,
					priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
			g_free( str );

			ofa_irenderable_set_text( instance,
					priv->body_currency_rtab, y, ofo_currency_get_code( scur->currency ), PANGO_ALIGN_RIGHT );

			ofs_currency_add_by_object(
					&priv->report_totals, scur->currency,
					is_paginating ? 0 : scur->debit, is_paginating ? 0 : scur->credit );

			first = FALSE;
			y += line_height;
		}
	}
	ofs_currency_list_free( &ledger_currencies );

	y -= line_height;
	ofa_irenderable_set_last_y( instance, y );
}

/*
 * print a line per found currency at the end of the printing
 */
static void
irenderable_draw_bottom_summary( ofaIRenderable *instance )
{
	ofaLedgerSummaryRenderPrivate *priv;
	static const gdouble st_vspace_rate = 0.25;
	gdouble bottom, vspace, req_height, height, top;
	gchar *str;
	GList *it;
	ofsCurrency *scur;
	gboolean first;
	gint shift;

	priv = ofa_ledger_summary_render_get_instance_private( OFA_LEDGER_SUMMARY_RENDER( instance ));

	if( !priv->count ){
		ofa_irenderable_draw_no_data( instance );
		return;
	}

	/* bottom of the rectangle */
	bottom = ofa_irenderable_get_max_y( instance );

	/* top of the rectangle */
	height = ofa_irenderable_get_text_height( instance );
	vspace = height * st_vspace_rate;
	req_height = g_list_length( priv->report_totals ) * height
			+ ( 1+g_list_length( priv->report_totals )) * vspace;
	top = bottom - req_height;

	ofa_irenderable_draw_rect( instance, 0, top, -1, req_height );
	top += vspace;
	shift = 4;

	for( it=priv->report_totals, first=TRUE ; it ; it=it->next ){
		scur = ( ofsCurrency * ) it->data;

		if( first ){
			ofa_irenderable_set_text( instance,
					priv->body_debit_rtab-st_amount_width-shift, top,
					_( "Ledgers general balance : " ), PANGO_ALIGN_RIGHT );
			first = FALSE;
		}

		str = ofa_amount_to_str( scur->debit, scur->currency );
		ofa_irenderable_set_text( instance,
				priv->body_debit_rtab-shift, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = ofa_amount_to_str( scur->credit, scur->currency );
		ofa_irenderable_set_text( instance,
				priv->body_credit_rtab-shift, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		ofa_irenderable_set_text( instance,
				priv->body_currency_rtab, top, ofo_currency_get_code( scur->currency ), PANGO_ALIGN_RIGHT );

		top += height+vspace;
	}

	ofa_irenderable_set_last_y( instance, ofa_irenderable_get_last_y( instance ) + req_height );
}

/*
 * settings = paned_position;
 */
static void
get_settings( ofaLedgerSummaryRender *self )
{
	GList *slist, *it;
	const gchar *cstr;
	GtkWidget *paned;
	gint pos;

	slist = ofa_settings_user_get_string_list( st_page_settings );
	if( slist ){
		it = slist ? slist : NULL;
		cstr = it ? it->data : NULL;
		pos = cstr ? atoi( cstr ) : 0;
		if( pos <= 10 ){
			pos = 150;
		}
		paned = ofa_render_page_get_top_paned( OFA_RENDER_PAGE( self ));
		gtk_paned_set_position( GTK_PANED( paned ), pos );

		ofa_settings_free_string_list( slist );
	}
}

static void
set_settings( ofaLedgerSummaryRender *self )
{
	GtkWidget *paned;
	gint pos;
	gchar *str;

	paned = ofa_render_page_get_top_paned( OFA_RENDER_PAGE( self ));
	pos = gtk_paned_get_position( GTK_PANED( paned ));

	str = g_strdup_printf( "%d;", pos );

	ofa_settings_user_set_string( st_page_settings, str );

	g_free( str );
}
