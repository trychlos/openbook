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
#include "api/ofo-ledger.h"
#include "api/ofs-currency.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-iaccount-filter.h"
#include "ui/ofa-irenderable.h"
#include "ui/ofa-ledger-summary-bin.h"
#include "ui/ofa-ledger-summary-render.h"

/* private instance data
 */
struct _ofaLedgerSummaryRenderPrivate {

	ofaHub              *hub;
	ofaLedgerSummaryBin *args_bin;

	/* internals
	 */
	GList               *ledgers;
	GDate                from_date;
	GDate                to_date;
	gint                 count;					/* count of returned entries */

	/* print datas
	 */
	gdouble              render_width;
	gdouble              render_height;
	gdouble              page_margin;

	/* layout for ledger header line
	 */
	gdouble              body_ledcode_ltab;
	gdouble              body_ledlabel_ltab;
	gdouble              body_ledlabel_max_size;
	gdouble              body_debit_rtab;
	gdouble              body_credit_rtab;
	gdouble              body_currency_rtab;

	/* for each ledger
	 */
	gchar               *ledger_mnemo;
	ofoLedger           *ledger_object;
	GList               *ledger_totals;		/* totals per currency for the ledger */

	/* total general
	 */
	GList               *report_totals;		/* all totals per currency */
};

#define THIS_PAGE_ORIENTATION            GTK_PAGE_ORIENTATION_LANDSCAPE
#define THIS_PAPER_NAME                  GTK_PAPER_NAME_A4

static const gchar *st_page_header_title = N_( "General Ledgers Summary" );

static const gchar *st_print_settings    = "RenderLedgersSummaryPrint";

/* these are parms which describe the page layout
 */

/* the columns of the body */
static const gint st_body_font_size      = 9;

#define st_mnemo_width                   (gdouble) 54/9*st_body_font_size
#define st_amount_width                  (gdouble) 90/9*st_body_font_size
#define st_currency_width                (gdouble) 23/10*st_body_font_size
#define st_column_hspacing               (gdouble) 4

static ofaRenderPageClass *ofa_ledger_summary_render_parent_class = NULL;

static GType              register_type( void );
static void               ledgers_book_render_finalize( GObject *instance );
static void               ledgers_book_render_dispose( GObject *instance );
static void               ledgers_book_render_instance_init( ofaLedgerSummaryRender *self );
static void               ledgers_book_render_class_init( ofaLedgerSummaryRenderClass *klass );
static void               page_init_view( ofaPage *page );
static GtkWidget         *page_get_top_focusable_widget( const ofaPage *page );
static GtkWidget         *render_page_get_args_widget( ofaRenderPage *page );
static const gchar       *render_page_get_paper_name( ofaRenderPage *page );
static GtkPageOrientation render_page_get_page_orientation( ofaRenderPage *page );
static void               render_page_get_print_settings( ofaRenderPage *page, GKeyFile **keyfile, gchar **group_name );
static GList             *render_page_get_dataset( ofaRenderPage *page );
static void               render_page_free_dataset( ofaRenderPage *page, GList *dataset );
static void               on_args_changed( ofaLedgerSummaryBin *bin, ofaLedgerSummaryRender *page );
static void               irenderable_iface_init( ofaIRenderableInterface *iface );
static guint              irenderable_get_interface_version( const ofaIRenderable *instance );
static void               irenderable_reset_runtime( ofaIRenderable *instance );
static gboolean           irenderable_want_groups( const ofaIRenderable *instance );
static void               irenderable_begin_render( ofaIRenderable *instance, gdouble render_width, gdouble render_height );
static gchar             *irenderable_get_dossier_name( const ofaIRenderable *instance );
static gchar             *irenderable_get_page_header_title( const ofaIRenderable *instance );
static void               irenderable_draw_page_header_columns( ofaIRenderable *instance, gint page_num );
static gboolean           irenderable_is_new_group( const ofaIRenderable *instance, GList *current, GList *prev );
static void               irenderable_draw_group_header( ofaIRenderable *instance, GList *current );
static void               irenderable_draw_line( ofaIRenderable *instance, GList *current );
static void               irenderable_draw_group_footer( ofaIRenderable *instance );
static void               irenderable_draw_bottom_summary( ofaIRenderable *instance );
static void               draw_ledger_totals( ofaIRenderable *instance );
static void               free_currency( ofsCurrency *total_per_currency );

GType
ofa_ledger_summary_render_get_type( void )
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
	static const gchar *thisfn = "ofo_ledger_summary_render_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaLedgerSummaryRenderClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) ledgers_book_render_class_init,
		NULL,
		NULL,
		sizeof( ofaLedgerSummaryRender ),
		0,
		( GInstanceInitFunc ) ledgers_book_render_instance_init
	};

	static const GInterfaceInfo irenderable_iface_info = {
		( GInterfaceInitFunc ) irenderable_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFA_TYPE_RENDER_PAGE, "ofaLedgerSummaryRender", &info, 0 );

	g_type_add_interface_static( type, OFA_TYPE_IRENDERABLE, &irenderable_iface_info );

	return( type );
}

static void
ledgers_book_render_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledger_summary_render_finalize";
	ofaLedgerSummaryRenderPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGER_SUMMARY_RENDER( instance ));

	/* free data members here */
	priv = OFA_LEDGER_SUMMARY_RENDER( instance )->priv;

	ofs_currency_list_free( &priv->ledger_totals );
	ofs_currency_list_free( &priv->report_totals );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_summary_render_parent_class )->finalize( instance );
}

static void
ledgers_book_render_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_LEDGER_SUMMARY_RENDER( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_summary_render_parent_class )->dispose( instance );
}

static void
ledgers_book_render_instance_init( ofaLedgerSummaryRender *self )
{
	static const gchar *thisfn = "ofa_ledger_summary_render_instance_init";

	g_return_if_fail( OFA_IS_LEDGER_SUMMARY_RENDER( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_LEDGER_SUMMARY_RENDER, ofaLedgerSummaryRenderPrivate );
}

static void
ledgers_book_render_class_init( ofaLedgerSummaryRenderClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_summary_render_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	ofa_ledger_summary_render_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = ledgers_book_render_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledgers_book_render_finalize;

	OFA_PAGE_CLASS( klass )->init_view = page_init_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = page_get_top_focusable_widget;

	OFA_RENDER_PAGE_CLASS( klass )->get_args_widget = render_page_get_args_widget;
	OFA_RENDER_PAGE_CLASS( klass )->get_paper_name = render_page_get_paper_name;
	OFA_RENDER_PAGE_CLASS( klass )->get_page_orientation = render_page_get_page_orientation;
	OFA_RENDER_PAGE_CLASS( klass )->get_print_settings = render_page_get_print_settings;
	OFA_RENDER_PAGE_CLASS( klass )->get_dataset = render_page_get_dataset;
	OFA_RENDER_PAGE_CLASS( klass )->free_dataset = render_page_free_dataset;

	g_type_class_add_private( klass, sizeof( ofaLedgerSummaryRenderPrivate ));
}

static void
page_init_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_ledger_summary_render_page_init_view";
	ofaLedgerSummaryRenderPrivate *priv;

	OFA_PAGE_CLASS( ofa_ledger_summary_render_parent_class )->init_view( page );

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = OFA_LEDGER_SUMMARY_RENDER( page )->priv;
	on_args_changed( priv->args_bin, OFA_LEDGER_SUMMARY_RENDER( page ));

	priv->hub = ofa_page_get_hub( page );
}

static GtkWidget *
page_get_top_focusable_widget( const ofaPage *page )
{
	return( NULL );
}

static GtkWidget *
render_page_get_args_widget( ofaRenderPage *page )
{
	ofaLedgerSummaryRenderPrivate *priv;
	ofaLedgerSummaryBin *bin;

	priv = OFA_LEDGER_SUMMARY_RENDER( page )->priv;

	bin = ofa_ledger_summary_bin_new( ofa_page_get_main_window( OFA_PAGE( page )));
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
	ofaLedgerSummaryRenderPrivate *priv;
	GList *it;
	GSList *mnemos;
	GList *dataset;
	ofaIDateFilter *date_filter;

	priv = OFA_LEDGER_SUMMARY_RENDER( page )->priv;

	priv->ledgers = ofo_ledger_get_dataset( priv->hub );

	for( mnemos=NULL, it=priv->ledgers ; it ; it=it->next ){
		mnemos = g_slist_prepend( mnemos, g_strdup( ofo_ledger_get_mnemo( OFO_LEDGER( it->data ))));
	}

	date_filter = ofa_ledger_summary_bin_get_date_filter( priv->args_bin );
	my_date_set_from_date( &priv->from_date, ofa_idate_filter_get_date( date_filter, IDATE_FILTER_FROM ));
	my_date_set_from_date( &priv->to_date, ofa_idate_filter_get_date( date_filter, IDATE_FILTER_TO ));

	dataset = ofo_entry_get_dataset_for_print_ledgers(
						priv->hub, mnemos,
						my_date_is_valid( &priv->from_date ) ? &priv->from_date : NULL,
						my_date_is_valid( &priv->to_date ) ? &priv->to_date : NULL );

	priv->count = g_list_length( dataset );
	g_slist_free_full( mnemos, ( GDestroyNotify ) g_free );

	return( dataset );
}

static void
render_page_free_dataset( ofaRenderPage *page, GList *dataset )
{
	g_list_free_full( dataset, ( GDestroyNotify ) g_object_unref );
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
	iface->want_groups = irenderable_want_groups;
	iface->get_dossier_name = irenderable_get_dossier_name;
	iface->get_page_header_title = irenderable_get_page_header_title;
	iface->draw_page_header_columns = irenderable_draw_page_header_columns;
	iface->is_new_group = irenderable_is_new_group;
	iface->draw_group_header = irenderable_draw_group_header;
	iface->draw_line = irenderable_draw_line;
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
	ofaLedgerSummaryRenderPrivate *priv;

	priv = OFA_LEDGER_SUMMARY_RENDER( instance )->priv;

	ofs_currency_list_free( &priv->report_totals );

	g_free( priv->ledger_mnemo );
	priv->ledger_mnemo = NULL;
}

/*
 * yes we want group entries per ledger
 * but we won't print anything as part of group header, because we are
 * only interested here by group footer (ledger summary)
 */
static gboolean
irenderable_want_groups( const ofaIRenderable *instance )
{
	return( TRUE );
}

/*
 * mainly here: compute the tab positions
 */
static void
irenderable_begin_render( ofaIRenderable *instance, gdouble render_width, gdouble render_height )
{
	static const gchar *thisfn = "ofa_ledger_summary_render_irenderable_begin_render";
	ofaLedgerSummaryRenderPrivate *priv;

	priv = OFA_LEDGER_SUMMARY_RENDER( instance )->priv;

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
	ofaLedgerSummaryRenderPrivate *priv;
	const ofaIDBConnect *connect;
	ofaIDBMeta *meta;
	gchar *dossier_name;

	priv = OFA_LEDGER_SUMMARY_RENDER( instance )->priv;
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

static void
irenderable_draw_page_header_columns( ofaIRenderable *instance, gint page_num )
{
	ofaLedgerSummaryRenderPrivate *priv;
	static gdouble st_vspace_rate = 0.5;
	gdouble y, text_height, vspace;

	priv = OFA_LEDGER_SUMMARY_RENDER( instance )->priv;

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

/*
 * just test if the current entry is on the same ledger than the
 * previous one
 */
static gboolean
irenderable_is_new_group( const ofaIRenderable *instance, GList *current, GList *prev )
{
	ofoEntry *current_entry, *prev_entry;

	g_return_val_if_fail( current, FALSE );

	if( !prev ){
		return( TRUE );
	}

	current_entry = OFO_ENTRY( current->data );
	prev_entry = OFO_ENTRY( prev->data );

	return( g_utf8_collate(
				ofo_entry_get_ledger( current_entry ),
				ofo_entry_get_ledger( prev_entry )) != 0 );
}

/*
 * do not draw anything here
 */
static void
irenderable_draw_group_header( ofaIRenderable *instance, GList *current )
{
	ofaLedgerSummaryRenderPrivate *priv;

	priv = OFA_LEDGER_SUMMARY_RENDER( instance )->priv;

	/* setup the ledger properties */
	g_free( priv->ledger_mnemo );
	priv->ledger_mnemo = g_strdup( ofo_entry_get_ledger( OFO_ENTRY( current->data )));

	priv->ledger_object = ofo_ledger_get_by_mnemo( priv->hub, priv->ledger_mnemo );
	g_return_if_fail( priv->ledger_object && OFO_IS_LEDGER( priv->ledger_object ));

	g_list_free_full( priv->ledger_totals, ( GDestroyNotify ) free_currency );
	priv->ledger_totals = NULL;
}

/*
 *  do not draw anything here, but increment ledger totals
 */
static void
irenderable_draw_line( ofaIRenderable *instance, GList *current )
{
	ofaLedgerSummaryRenderPrivate *priv;
	ofoEntry *entry;
	const gchar *code;
	ofoCurrency *currency;
	ofxAmount debit, credit;
	gboolean is_paginating;

	priv = OFA_LEDGER_SUMMARY_RENDER( instance )->priv;
	entry = OFO_ENTRY( current->data );

	/* get currency properties */
	code = ofo_entry_get_currency( entry );
	currency = ofo_currency_get_by_code( priv->hub, code );
	g_return_if_fail( currency && OFO_IS_CURRENCY( currency ));

	/* debit */
	debit = ofo_entry_get_debit( entry );

	/* credit */
	credit = ofo_entry_get_credit( entry );

	is_paginating = ofa_irenderable_is_paginating( instance );
	ofs_currency_add_currency(
			&priv->ledger_totals, code,
			is_paginating ? 0 : debit, is_paginating ? 0 : credit );
}

/*
 * This function is called many times in order to auto-detect the
 * height of the group footer (in particular each time the
 * #ofa_irenderable_draw_line() function needs to know if there is
 * enough vertical space left to draw the current line)
 * so take care of:
 * - currency has yet to be defined even during pagination phase
 *   in order to be able to detect the heigth of the summary
 */
static void
irenderable_draw_group_footer( ofaIRenderable *instance )
{
	ofaLedgerSummaryRenderPrivate *priv;
	GList *it;
	ofsCurrency *cur;
	gboolean is_paginating;

	priv = OFA_LEDGER_SUMMARY_RENDER( instance )->priv;

	draw_ledger_totals( instance );

	is_paginating = ofa_irenderable_is_paginating( instance );
	for( it=priv->ledger_totals ; it ; it=it->next ){
		cur = ( ofsCurrency * ) it->data;
		ofs_currency_add_currency(
				&priv->report_totals, cur->currency,
				is_paginating ? 0 : cur->debit, is_paginating ? 0 : cur->credit );
	}
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
	ofoCurrency *currency;
	gint digits, shift;

	priv = OFA_LEDGER_SUMMARY_RENDER( instance )->priv;

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
		currency = ofo_currency_get_by_code( priv->hub, scur->currency );
		g_return_if_fail( currency && OFO_IS_CURRENCY( currency ));
		digits = ofo_currency_get_digits( currency );

		if( first ){
			ofa_irenderable_set_text( instance,
					priv->body_debit_rtab-st_amount_width-shift, top,
					_( "Ledgers general balance : " ), PANGO_ALIGN_RIGHT );
			first = FALSE;
		}

		str = my_double_to_str_ex( scur->debit, digits );
		ofa_irenderable_set_text( instance,
				priv->body_debit_rtab-shift, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = my_double_to_str_ex( scur->credit, digits );
		ofa_irenderable_set_text( instance,
				priv->body_credit_rtab-shift, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		ofa_irenderable_set_text( instance,
				priv->body_currency_rtab, top, scur->currency, PANGO_ALIGN_RIGHT );

		top += height+vspace;
	}

	ofa_irenderable_set_last_y( instance, ofa_irenderable_get_last_y( instance ) + req_height );
}

/*
 * draw the total for this ledger by currencies
 * update the last_y accordingly
 */
static void
draw_ledger_totals( ofaIRenderable *instance )
{
	ofaLedgerSummaryRenderPrivate *priv;
	static const gdouble st_vspace_rate = 0.4;
	gdouble y, height;
	gboolean first;
	gchar *str;
	GList *it;
	ofsCurrency *scur;
	ofoCurrency *currency;
	gint digits;

	priv = OFA_LEDGER_SUMMARY_RENDER( instance )->priv;

	y = ofa_irenderable_get_last_y( instance );
	height = 0;

	for( it=priv->ledger_totals, first=TRUE ; it ; it=it->next ){
		scur = ( ofsCurrency * ) it->data;
		currency = ofo_currency_get_by_code( priv->hub, scur->currency );
		g_return_if_fail( currency && OFO_IS_CURRENCY( currency ));
		digits = ofo_currency_get_digits( currency );

		if( first ){
			str = g_strdup_printf( _( "%s ledger balance : " ), priv->ledger_mnemo );
			height = ofa_irenderable_set_text( instance,
					priv->body_debit_rtab-st_amount_width, y,
					str, PANGO_ALIGN_RIGHT );
			g_free( str );
			first = FALSE;
		}

		str = my_double_to_str_ex( scur->debit, digits );
		ofa_irenderable_set_text( instance,
				priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = my_double_to_str_ex( scur->credit, digits );
		ofa_irenderable_set_text( instance,
				priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		ofa_irenderable_set_text( instance,
				priv->body_currency_rtab, y, scur->currency, PANGO_ALIGN_RIGHT );

		y += height * ( 1+st_vspace_rate );
	}

	ofa_irenderable_set_last_y( instance, y );
}

static void
free_currency( ofsCurrency *total_per_currency )
{
	g_free( total_per_currency->currency );
	g_free( total_per_currency );
}
