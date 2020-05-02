/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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
#include "api/ofa-prefs.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofs-currency.h"

#include "core/ofa-iconcil.h"

#include "ui/ofa-iaccount-filter.h"
#include "ui/ofa-ledger-book-args.h"
#include "ui/ofa-ledger-book-render.h"

/* private instance data
 */
typedef struct {

	ofaLedgerBookArgs *args_bin;

	/* runtime
	 */
	ofaIGetter        *getter;
	gchar             *settings_prefix;
	GList             *selected;				/* list of selected #ofoLedger */
	gboolean           all_ledgers;
	gboolean           new_page;
	gboolean           with_summary;
	gboolean           only_summary;
	GDate              from_date;
	GDate              to_date;
	gint               count;					/* count of returned entries */

	/* print datas
	 */
	gdouble            render_width;
	gdouble            render_height;
	gdouble            page_margin;

	/* layout for ledger header line
	 */
	gdouble            group_h_ledcode_ltab;
	gdouble            group_h_ledlabel_ltab;
	gdouble            group_h_ledlabel_max_size;

	/* layout for entry line
	 */
	gdouble            body_dope_ltab;
	gdouble            body_deffect_ltab;
	gdouble            body_account_ltab;
	gdouble            body_account_max_size;
	gdouble            body_piece_ltab;
	gdouble            body_piece_max_size;
	gdouble            body_label_ltab;
	gint               body_label_max_size;		/* Pango units */
	gdouble            body_template_ltab;
	gdouble            body_template_max_size;
	gdouble            body_settlement_ctab;
	gdouble            body_reconcil_ctab;
	gdouble            body_debit_rtab;
	gdouble            body_credit_rtab;
	gdouble            body_currency_rtab;
	gdouble            amount_width;

	/* for each ledger
	 */
	gchar             *ledger_mnemo;
	ofoLedger         *ledger_object;
	GList             *ledger_totals;		/* totals per currency for the ledger */

	/* total general
	 */
	GList             *report_totals;		/* all totals per currency */
	GList             *ledgers_summary;
}
	ofaLedgerBookRenderPrivate;

/* a structure to hold the totals per ledger
 * used to build the global summary
 */
typedef struct {
	ofoLedger *ledger;
	GList     *totals;
}
	sLedger;

/*
 * Accounts balances print uses a portrait orientation
 */
#define THIS_PAGE_ORIENTATION            GTK_PAGE_ORIENTATION_LANDSCAPE
#define THIS_PAPER_NAME                  GTK_PAPER_NAME_A4

static const gchar *st_page_header_title = N_( "General Ledgers Book" );

/* these are parms which describe the page layout
 */
static const gchar *st_title2_font       = "Sans Bold 8";

static GtkWidget         *page_v_get_top_focusable_widget( const ofaPage *page );
static void               paned_page_v_init_view( ofaPanedPage *page );
static GtkWidget         *render_page_v_get_args_widget( ofaRenderPage *page );
static const gchar       *render_page_v_get_paper_name( ofaRenderPage *page );
static GtkPageOrientation render_page_v_get_page_orientation( ofaRenderPage *page );
static void               render_page_v_get_print_settings( ofaRenderPage *page, GKeyFile **keyfile, gchar **group_name );
static GList             *render_page_v_get_dataset( ofaRenderPage *page );
static void               render_page_v_free_dataset( ofaRenderPage *page, GList *dataset );
static void               on_args_changed( ofaLedgerBookArgs *bin, ofaLedgerBookRender *page );
static void               irenderable_iface_init( ofaIRenderableInterface *iface );
static guint              irenderable_get_interface_version( void );
static void               irenderable_begin_render( ofaIRenderable *instance );
static gchar             *irenderable_get_dossier_label( const ofaIRenderable *instance );
static void               irenderable_draw_page_header_title( ofaIRenderable *instance );
static void               irenderable_draw_header_column_names( ofaIRenderable *instance );
static gboolean           irenderable_is_new_group( const ofaIRenderable *instance, GList *prev, GList *line, ofeIRenderableBreak *sep );
static void               irenderable_draw_group_header( ofaIRenderable *instance );
static void               irenderable_draw_top_report( ofaIRenderable *instance );
static void               irenderable_draw_line( ofaIRenderable *instance );
static void               irenderable_draw_bottom_report( ofaIRenderable *instance );
static void               irenderable_draw_group_footer( ofaIRenderable *instance );
static void               append_ledger_to_summary( ofaLedgerBookRender *self );
static void               irenderable_draw_last_summary( ofaIRenderable *instance );
static void               draw_ledgers_summary( ofaIRenderable *instance );
static void               irenderable_clear_runtime_data( ofaIRenderable *instance );
static void               clear_ledger_data( ofaLedgerBookRender *self );
static void               draw_ledger_totals( ofaIRenderable *instance );
static void               free_ledgers( GList **ledgers );
static void               free_ledger( sLedger *ledger );
static void               read_settings( ofaLedgerBookRender *self );
static void               write_settings( ofaLedgerBookRender *self );

G_DEFINE_TYPE_EXTENDED( ofaLedgerBookRender, ofa_ledger_book_render, OFA_TYPE_RENDER_PAGE, 0,
		G_ADD_PRIVATE( ofaLedgerBookRender )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IRENDERABLE, irenderable_iface_init ))

static void
ledger_book_render_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledger_book_render_finalize";
	ofaLedgerBookRenderPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGER_BOOK_RENDER( instance ));

	/* free data members here */
	priv = ofa_ledger_book_render_get_instance_private( OFA_LEDGER_BOOK_RENDER( instance ));

	g_free( priv->settings_prefix );

	ofs_currency_list_free( &priv->ledger_totals );
	ofs_currency_list_free( &priv->report_totals );
	free_ledgers( &priv->ledgers_summary );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_book_render_parent_class )->finalize( instance );
}

static void
ledger_book_render_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_LEDGER_BOOK_RENDER( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		write_settings( OFA_LEDGER_BOOK_RENDER( instance ));

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_book_render_parent_class )->dispose( instance );
}

static void
ofa_ledger_book_render_init( ofaLedgerBookRender *self )
{
	static const gchar *thisfn = "ofa_ledger_book_render_init";
	ofaLedgerBookRenderPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_LEDGER_BOOK_RENDER( self ));

	priv = ofa_ledger_book_render_get_instance_private( self );

	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_ledger_book_render_class_init( ofaLedgerBookRenderClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_book_render_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledger_book_render_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledger_book_render_finalize;

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
	static const gchar *thisfn = "ofa_ledger_book_render_paned_page_v_init_view";
	ofaLedgerBookRenderPrivate *priv;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_ledger_book_render_get_instance_private( OFA_LEDGER_BOOK_RENDER( page ));

	on_args_changed( priv->args_bin, OFA_LEDGER_BOOK_RENDER( page ));

	read_settings( OFA_LEDGER_BOOK_RENDER( page ));
}

static GtkWidget *
render_page_v_get_args_widget( ofaRenderPage *page )
{
	ofaLedgerBookRenderPrivate *priv;
	ofaLedgerBookArgs *bin;

	priv = ofa_ledger_book_render_get_instance_private( OFA_LEDGER_BOOK_RENDER( page ));

	priv->getter = ofa_page_get_getter( OFA_PAGE( page ));

	bin = ofa_ledger_book_args_new( priv->getter, priv->settings_prefix );
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
	ofaLedgerBookRenderPrivate *priv;
	myISettings *settings;

	priv = ofa_ledger_book_render_get_instance_private( OFA_LEDGER_BOOK_RENDER( page ));

	settings = ofa_igetter_get_user_settings( priv->getter );
	*keyfile = my_isettings_get_keyfile( settings );
	*group_name = g_strdup_printf( "%s-print", priv->settings_prefix );
}

static GList *
render_page_v_get_dataset( ofaRenderPage *page )
{
	ofaLedgerBookRenderPrivate *priv;
	ofaLedgerTreeview *tview;
	GSList *mnemos;
	GList *list, *it;
	ofoLedger *ledger;
	GList *dataset;
	ofaIDateFilter *date_filter;

	priv = ofa_ledger_book_render_get_instance_private( OFA_LEDGER_BOOK_RENDER( page ));

	priv->all_ledgers = ofa_ledger_book_args_get_all_ledgers( priv->args_bin );
	tview = ofa_ledger_book_args_get_treeview( priv->args_bin );

	if( priv->all_ledgers ){
		priv->selected = ofo_ledger_get_dataset( priv->getter );
	} else {
		list = ofa_ledger_treeview_get_selected( tview );
		priv->selected = NULL;
		for( it=list ; it ; it=it->next ){
			ledger = ( ofoLedger * ) it->data;
			g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
			priv->selected = g_list_append( priv->selected, ledger );
		}
		ofa_ledger_treeview_free_selected( list );
	}
	for( mnemos=NULL, it=priv->selected ; it ; it=it->next ){
		mnemos = g_slist_prepend( mnemos, g_strdup( ofo_ledger_get_mnemo( OFO_LEDGER( it->data ))));
	}

	date_filter = ofa_ledger_book_args_get_date_filter( priv->args_bin );
	my_date_set_from_date( &priv->from_date, ofa_idate_filter_get_date( date_filter, IDATE_FILTER_FROM ));
	my_date_set_from_date( &priv->to_date, ofa_idate_filter_get_date( date_filter, IDATE_FILTER_TO ));

	dataset = ofo_entry_get_dataset_for_print_by_ledger(
						priv->getter, mnemos,
						my_date_is_valid( &priv->from_date ) ? &priv->from_date : NULL,
						my_date_is_valid( &priv->to_date ) ? &priv->to_date : NULL );

	priv->count = g_list_length( dataset );
	g_slist_free_full( mnemos, ( GDestroyNotify ) g_free );

	priv->new_page = ofa_ledger_book_args_get_new_page_per_ledger( priv->args_bin );
	priv->with_summary = ofa_ledger_book_args_get_with_summary( priv->args_bin );
	priv->only_summary = ofa_ledger_book_args_get_only_summary( priv->args_bin );

	return( dataset );
}

static void
render_page_v_free_dataset( ofaRenderPage *page, GList *dataset )
{
	g_list_free_full( dataset, ( GDestroyNotify ) g_object_unref );
}

/*
 * ofaLedgerBookArgs "ofa-changed" handler
 */
static void
on_args_changed( ofaLedgerBookArgs *bin, ofaLedgerBookRender *page )
{
	gboolean valid;
	gchar *message;

	valid = ofa_ledger_book_args_is_valid( bin, &message );
	ofa_render_page_set_args_changed( OFA_RENDER_PAGE( page ), valid, message );
	g_free( message );
}

static void
irenderable_iface_init( ofaIRenderableInterface *iface )
{
	static const gchar *thisfn = "ofa_ledger_book_render_irenderable_iface_init";

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
irenderable_get_interface_version( void )
{
	return( 1 );
}

/*
 * mainly here: compute the tab positions
 */
static void
irenderable_begin_render( ofaIRenderable *instance )
{
	static const gchar *thisfn = "ofa_ledger_book_render_irenderable_begin_render";
	ofaLedgerBookRenderPrivate *priv;
	gdouble spacing, date_width, account_width, piece_width, template_width, char_width, cur_width, amount_width;

	priv = ofa_ledger_book_render_get_instance_private( OFA_LEDGER_BOOK_RENDER( instance ));

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv->render_width = ofa_irenderable_get_render_width( instance );
	priv->render_height = ofa_irenderable_get_render_height( instance );
	priv->page_margin = ofa_irenderable_get_page_margin( instance );

	spacing = ofa_irenderable_get_columns_spacing( instance );

	/* compute the width of the columns with the body font */
	ofa_irenderable_set_font( instance, ofa_irenderable_get_body_font( instance ));
	date_width = ofa_irenderable_get_text_width( instance, "9999-99-99-" );
	account_width = ofa_irenderable_get_text_width( instance, "XXXXXXXX" );
	piece_width = ofa_irenderable_get_text_width( instance, "XX 99999999" );
	char_width = ofa_irenderable_get_text_width( instance, "X" );
	template_width = ofa_irenderable_get_text_width( instance, "XXXXXXXXXXXX" );

	/* the width of the amounts should use the last summary font */
	ofa_irenderable_set_font( instance, ofa_irenderable_get_summary_font( instance, 0 ));
	cur_width = ofa_irenderable_get_text_width( instance, "XXX" );
	amount_width = ofa_irenderable_get_text_width( instance, "9,999,999,999.99" );
	priv->amount_width = amount_width;

	/* entry line, starting from the left */
	priv->body_dope_ltab = priv->page_margin;
	priv->body_deffect_ltab = priv->body_dope_ltab + date_width + spacing;
	priv->body_account_ltab = priv->body_deffect_ltab + date_width + spacing;
	priv->body_piece_ltab = priv->body_account_ltab + account_width + spacing;
	priv->body_label_ltab = priv->body_piece_ltab + piece_width + spacing;

	/* entry line, starting from the right */
	priv->body_currency_rtab = priv->render_width - priv->page_margin;
	priv->body_credit_rtab = priv->body_currency_rtab - cur_width - spacing;
	priv->body_debit_rtab = priv->body_credit_rtab - amount_width - spacing;
	priv->body_reconcil_ctab = priv->body_debit_rtab - amount_width - spacing - char_width/2.0;
	priv->body_settlement_ctab = priv->body_reconcil_ctab - char_width/2.0 - spacing - char_width/2.0;
	priv->body_template_ltab = priv->body_settlement_ctab + char_width/2.0 - spacing - template_width;

	/* account header, starting from the left
	 * computed here because aligned on (and so relying on) body effect date */
	priv->group_h_ledcode_ltab = priv->page_margin;
	priv->group_h_ledlabel_ltab = priv->body_deffect_ltab;

	/* max size in Pango units */
	priv->group_h_ledlabel_max_size = ( priv->render_width - priv->page_margin - priv->group_h_ledlabel_ltab )*PANGO_SCALE;
	priv->body_account_max_size = account_width*PANGO_SCALE;
	priv->body_piece_max_size = piece_width*PANGO_SCALE;
	priv->body_label_max_size = ( priv->body_template_ltab - spacing - priv->body_label_ltab )*PANGO_SCALE;
	priv->body_template_max_size = template_width*PANGO_SCALE;

	/* only summary ? */
	ofa_irenderable_set_line_mode( instance, priv->only_summary ? IRENDERABLE_MODE_NOPRINT : IRENDERABLE_MODE_NORMAL );
}

static gchar *
irenderable_get_dossier_label( const ofaIRenderable *instance )
{
	ofaLedgerBookRenderPrivate *priv;
	ofaHub *hub;
	ofoDossier *dossier;
	const gchar *label;

	priv = ofa_ledger_book_render_get_instance_private( OFA_LEDGER_BOOK_RENDER( instance ));

	hub = ofa_igetter_get_hub( priv->getter );
	dossier = ofa_hub_get_dossier( hub );
	label = ofo_dossier_get_label( dossier );

	return( g_strdup( label ));
}

static void
irenderable_draw_page_header_title( ofaIRenderable *instance )
{
	ofaLedgerBookRenderPrivate *priv;
	gdouble r, g, b, y, height;
	gchar *sfrom_date, *sto_date;
	GString *stitle;
	GList *it;
	gboolean first;

	priv = ofa_ledger_book_render_get_instance_private( OFA_LEDGER_BOOK_RENDER( instance ));

	ofa_irenderable_get_title_color( instance, &r, &g, &b );
	ofa_irenderable_set_color( instance, r, g, b );
	y = ofa_irenderable_get_last_y( instance );

	/* line 1 - ledgers book summary */
	ofa_irenderable_set_font( instance,
			ofa_irenderable_get_title_font( instance, ofa_irenderable_get_current_page_num( instance )));
	height = ofa_irenderable_set_text( instance, priv->render_width/2, y, st_page_header_title, PANGO_ALIGN_CENTER );
	y += height;

	/* line 2 - Account from xxx to xxx - Date from xxx to xxx *
	 * recall of ledgers and date selections in line 4 */
	stitle = g_string_new( "" );
	if( priv->all_ledgers ){
		g_string_printf( stitle, _( "All ledgers" ));
	} else {
		g_string_printf( stitle, _( "Ledgers " ));
		for( it=priv->selected, first=TRUE ; it ; it=it->next ){
			if( !first ){
				stitle = g_string_append( stitle, ", " );
			}
			g_string_append_printf( stitle, "%s", ofo_ledger_get_mnemo( OFO_LEDGER( it->data )));
			first = FALSE;
		}
	}

	stitle = g_string_append( stitle, " - " );

	if( !my_date_is_valid( &priv->from_date ) && !my_date_is_valid( &priv->to_date )){
		stitle = g_string_append( stitle, "All effect dates" );
	} else {
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
	ofaLedgerBookRenderPrivate *priv;
	static gdouble st_vspace_rate = 0.5;
	gdouble y, text_height, vspace;

	priv = ofa_ledger_book_render_get_instance_private( OFA_LEDGER_BOOK_RENDER( instance ));

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
			priv->body_account_ltab, y,
			_( "Account" ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			priv->body_piece_ltab, y,
			_( "Piece" ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			priv->body_label_ltab, y,
			_( "Label" ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			priv->body_template_ltab, y,
			_( "Tmpl." ), PANGO_ALIGN_LEFT );

	ofa_irenderable_set_text( instance,
			(priv->body_settlement_ctab+priv->body_reconcil_ctab)/2, y,
			_( "Set./Rec." ), PANGO_ALIGN_CENTER );

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
irenderable_is_new_group( const ofaIRenderable *instance, GList *prev, GList *line, ofeIRenderableBreak *sep )
{
	ofaLedgerBookRenderPrivate *priv;
	ofoEntry *current_entry, *prev_entry;
	gint cmp;

	priv = ofa_ledger_book_render_get_instance_private( OFA_LEDGER_BOOK_RENDER( instance ));

	*sep = IRENDERABLE_BREAK_NONE;

	if( !prev || !line ){
		return( TRUE );
	}

	current_entry = OFO_ENTRY( line->data );
	prev_entry = OFO_ENTRY( prev->data );

	cmp = my_collate( ofo_entry_get_ledger( current_entry ), ofo_entry_get_ledger( prev_entry ));

	if( cmp != 0 ){
		if( !priv->only_summary ){
			if( priv->new_page ){
				*sep = IRENDERABLE_BREAK_NEW_PAGE;

			} else {
				*sep = IRENDERABLE_BREAK_SEP_LINE;
			}
		}
	}

	return( cmp != 0 );
}

/*
 * draw account header
 */
static void
irenderable_draw_group_header( ofaIRenderable *instance )
{
	ofaLedgerBookRenderPrivate *priv;
	GList *line;
	gdouble y, height;

	priv = ofa_ledger_book_render_get_instance_private( OFA_LEDGER_BOOK_RENDER( instance ));

	line = ofa_irenderable_get_current_line( instance );
	if( line ){

		/* setup the ledger properties */
		g_free( priv->ledger_mnemo );
		priv->ledger_mnemo = g_strdup( ofo_entry_get_ledger( OFO_ENTRY( line->data )));

		priv->ledger_object = ofo_ledger_get_by_mnemo( priv->getter, priv->ledger_mnemo );
		g_return_if_fail( priv->ledger_object && OFO_IS_LEDGER( priv->ledger_object ));

		ofs_currency_list_free( &priv->ledger_totals );
		priv->ledger_totals = NULL;

		if( !priv->only_summary ){
			y = ofa_irenderable_get_last_y( instance );
			height = ofa_irenderable_get_line_height( instance );

			/* display the ledger header */
			/* ledger mnemo */
			ofa_irenderable_set_text( instance,
					priv->group_h_ledcode_ltab, y,
					priv->ledger_mnemo, PANGO_ALIGN_LEFT );

			/* ledger label */
			ofa_irenderable_ellipsize_text( instance,
					priv->group_h_ledlabel_ltab, y,
					ofo_ledger_get_label( priv->ledger_object ), priv->group_h_ledlabel_max_size );

			y += height;
			ofa_irenderable_set_last_y( instance, y );
		}
	}
}

static void
irenderable_draw_top_report( ofaIRenderable *instance )
{
	ofaLedgerBookRenderPrivate *priv;

	priv = ofa_ledger_book_render_get_instance_private( OFA_LEDGER_BOOK_RENDER( instance ));

	if( my_strlen( priv->ledger_mnemo ) && !priv->only_summary ){
		draw_ledger_totals( instance );
	}
}

static void
irenderable_draw_line( ofaIRenderable *instance )
{
	ofaLedgerBookRenderPrivate *priv;
	GList *line;
	ofoEntry *entry;
	const gchar *cstr, *code;
	gchar *str;
	gdouble y;
	ofxAmount debit, credit;
	ofoCurrency *currency;
	ofoConcil *concil;

	priv = ofa_ledger_book_render_get_instance_private( OFA_LEDGER_BOOK_RENDER( instance ));

	line = ofa_irenderable_get_current_line( instance );
	if( line ){

		entry = OFO_ENTRY( line->data );
		debit = ofo_entry_get_debit( entry );
		credit = ofo_entry_get_credit( entry );

		/* get currency properties */
		code = ofo_entry_get_currency( entry );
		currency = ofo_currency_get_by_code( priv->getter, code );
		g_return_if_fail( currency && OFO_IS_CURRENCY( currency ));

		if( !priv->only_summary ){
			y = ofa_irenderable_get_last_y( instance );

			/* operation date */
			str = my_date_to_str( ofo_entry_get_dope( entry ), ofa_prefs_date_get_display_format( priv->getter ));
			ofa_irenderable_set_text( instance,
					priv->body_dope_ltab, y, str, PANGO_ALIGN_LEFT );
			g_free( str );

			/* effect date */
			str = my_date_to_str( ofo_entry_get_deffect( entry ), ofa_prefs_date_get_display_format( priv->getter ));
			ofa_irenderable_set_text( instance,
					priv->body_deffect_ltab, y, str, PANGO_ALIGN_LEFT );
			g_free( str );

			/* account */
			ofa_irenderable_ellipsize_text( instance,
					priv->body_account_ltab, y,
					ofo_entry_get_account( entry ), priv->body_account_max_size );

			/* piece */
			cstr = ofo_entry_get_ref( entry );
			if( my_strlen( cstr )){
				ofa_irenderable_ellipsize_text( instance,
						priv->body_piece_ltab, y,
						cstr, priv->body_piece_max_size );
			}

			/* label */
			ofa_irenderable_ellipsize_text( instance,
					priv->body_label_ltab, y,
					ofo_entry_get_label( entry ), priv->body_label_max_size );

			/* template */
			cstr = ofo_entry_get_ope_template( entry );
			if( my_strlen( cstr )){
				ofa_irenderable_ellipsize_text( instance,
						priv->body_template_ltab, y, cstr, priv->body_template_max_size );
			}

			/* settlement ? */
			if( ofo_entry_get_settlement_number( entry ) > 0 ){
				ofa_irenderable_set_text( instance,
						priv->body_settlement_ctab, y, _( "S" ), PANGO_ALIGN_CENTER );
			}

			/* reconciliation ? */
			concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ));
			if( concil ){
				ofa_irenderable_set_text( instance,
						priv->body_reconcil_ctab, y, _( "R" ), PANGO_ALIGN_CENTER );
			}

			/* debit */
			if( debit ){
				str = ofa_amount_to_str( debit, currency, priv->getter );
				ofa_irenderable_set_text( instance,
						priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
				g_free( str );
			}

			/* credit */
			if( credit ){
				str = ofa_amount_to_str( credit, currency, priv->getter );
				ofa_irenderable_set_text( instance,
						priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
				g_free( str );
			}

			/* currency */
			ofa_irenderable_set_text( instance,
					priv->body_currency_rtab, y, code, PANGO_ALIGN_RIGHT );
		}

		ofs_currency_add_by_code(
				&priv->ledger_totals, priv->getter, code, debit, credit );
	}
}

static void
irenderable_draw_bottom_report( ofaIRenderable *instance )
{
	ofaLedgerBookRenderPrivate *priv;

	priv = ofa_ledger_book_render_get_instance_private( OFA_LEDGER_BOOK_RENDER( instance ));

	if( my_strlen( priv->ledger_mnemo ) && !priv->only_summary ){
		draw_ledger_totals( instance );
	}
}

/*
 * This function is called many times in order to auto-detect the
 * height of the group footer (in particular each time the
 * #ofa_irenderable_draw_line() function needs to know if there is
 * enough vertical space left to draw the current line)
 * so take care of:
 * - currency has yet to be defined even during pagination phase
 *   in order to be able to detect the heigtht of the summary
 */
static void
irenderable_draw_group_footer( ofaIRenderable *instance )
{
	ofaLedgerBookRenderPrivate *priv;
	GList *it;
	ofsCurrency *cur;
	gboolean is_paginating;

	priv = ofa_ledger_book_render_get_instance_private( OFA_LEDGER_BOOK_RENDER( instance ));

	if( !priv->only_summary ){
		draw_ledger_totals( instance );
	}

	is_paginating = ofa_irenderable_is_paginating( instance );

	for( it=priv->ledger_totals ; it ; it=it->next ){
		cur = ( ofsCurrency * ) it->data;
		ofs_currency_add_by_object(
				&priv->report_totals, cur->currency,
				is_paginating ? 0 : cur->debit, is_paginating ? 0 : cur->credit );
	}

	append_ledger_to_summary( OFA_LEDGER_BOOK_RENDER( instance ));

	if( !is_paginating ){
		clear_ledger_data( OFA_LEDGER_BOOK_RENDER( instance ));
	}
}

/*
 * From draw_group_footer():
 * Add the current ledger totals by currency to the summary list
 * removing the previously added if exist
 *
 * Rationale:
 * We need to add the ledgers and their currencies to the summary
 * during pagination in order to get the right height; but we do not
 * control the number of times the draw_group_footer() is called.
 * We are just only sure that the last one is the good one.
 */
static void
append_ledger_to_summary( ofaLedgerBookRender *self )
{
	ofaLedgerBookRenderPrivate *priv;
	sLedger *sledg;
	GList *it;

	priv = ofa_ledger_book_render_get_instance_private( self );

	if( priv->ledger_object ){
		g_return_if_fail( OFO_IS_LEDGER( priv->ledger_object ));

		/* removing the previously added */
		for( it=priv->ledgers_summary ; it ; it=it->next ){
			sledg = ( sLedger * ) it->data;
			if( sledg->ledger == priv->ledger_object ){
				priv->ledgers_summary = g_list_remove( priv->ledgers_summary, sledg );
				free_ledger( sledg );
				break;
			}
		}

		/* then add this new one */
		sledg = g_new0( sLedger, 1 );
		sledg->ledger = priv->ledger_object;
		sledg->totals = ofs_currency_list_copy( priv->ledger_totals );
		priv->ledgers_summary = g_list_append( priv->ledgers_summary, sledg );
	}
}

/*
 * print a line per found currency at the end of the printing
 */
static void
irenderable_draw_last_summary( ofaIRenderable *instance )
{
	ofaLedgerBookRenderPrivate *priv;
	static const gdouble st_vspace_rate = 0.25;
	gdouble bottom, vspace, req_height, height, top;
	gchar *str;
	GList *it;
	ofsCurrency *scur;
	gboolean first;
	gint shift;

	priv = ofa_ledger_book_render_get_instance_private( OFA_LEDGER_BOOK_RENDER( instance ));

	if( !priv->count ){
		ofa_irenderable_draw_no_data( instance );
		return;
	}

	if( priv->with_summary ){
		draw_ledgers_summary( instance );
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
					priv->body_debit_rtab-priv->amount_width-shift, top,
					_( "Ledgers general balance : " ), PANGO_ALIGN_RIGHT );
			first = FALSE;
		}

		str = ofa_amount_to_str( scur->debit, scur->currency, priv->getter );
		ofa_irenderable_set_text( instance,
				priv->body_debit_rtab-shift, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = ofa_amount_to_str( scur->credit, scur->currency, priv->getter );
		ofa_irenderable_set_text( instance,
				priv->body_credit_rtab-shift, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		ofa_irenderable_set_text( instance,
				priv->body_currency_rtab, top, ofo_currency_get_code( scur->currency ), PANGO_ALIGN_RIGHT );

		top += height+vspace;
	}

	ofa_irenderable_set_last_y( instance, ofa_irenderable_get_last_y( instance ) + req_height );
}

static void
draw_ledgers_summary( ofaIRenderable *instance )
{
	ofaLedgerBookRenderPrivate *priv;
	gdouble y, r, g, b, height;
	GList *itl, *itc;
	sLedger *sled;
	gboolean first;
	ofsCurrency *scur;
	gchar *str;

	priv = ofa_ledger_book_render_get_instance_private( OFA_LEDGER_BOOK_RENDER( instance ));

	ofa_irenderable_get_group_color( instance, &r, &g, &b );
	ofa_irenderable_set_color( instance, r, g, b );
	ofa_irenderable_set_font( instance, ofa_irenderable_get_group_font( instance, 0 ));
	height = ofa_irenderable_get_line_height( instance );

	y = ofa_irenderable_get_last_y( instance );
	y += height;

	ofa_irenderable_set_text( instance, priv->page_margin, y, _( "Ledgers summary" ), PANGO_ALIGN_LEFT );
	y += height;

	ofa_irenderable_set_font( instance, ofa_irenderable_get_report_font( instance, 0 ));
	height = ofa_irenderable_get_line_height( instance );

	for( itl=priv->ledgers_summary ; itl ; itl=itl->next ){
		sled = ( sLedger * ) itl->data;
		first = TRUE;

		for( itc=sled->totals ; itc ; itc=itc->next ){
			scur = ( ofsCurrency * ) itc->data;

			if( first ){
				ofa_irenderable_set_text( instance,
								priv->group_h_ledcode_ltab, y,
								ofo_ledger_get_mnemo( sled->ledger ), PANGO_ALIGN_LEFT );
				ofa_irenderable_ellipsize_text( instance,
								priv->group_h_ledlabel_ltab, y,
								ofo_ledger_get_label( sled->ledger ), priv->group_h_ledlabel_max_size );
				ofa_irenderable_set_text( instance,
								priv->body_currency_rtab, y,
								ofo_currency_get_code( scur->currency ), PANGO_ALIGN_RIGHT );
				first = FALSE;
			}

			str = ofa_amount_to_str( scur->debit, scur->currency, priv->getter );
			ofa_irenderable_set_text( instance,
					priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
			g_free( str );

			str = ofa_amount_to_str( scur->credit, scur->currency, priv->getter );
			ofa_irenderable_set_text( instance,
					priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
			g_free( str );

			y += height;
		}
	}

	ofa_irenderable_set_last_y( instance, y );

	ofa_irenderable_set_font( instance, ofa_irenderable_get_summary_font( instance, 0 ));
}

static void
irenderable_clear_runtime_data( ofaIRenderable *instance )
{
	ofaLedgerBookRenderPrivate *priv;

	priv = ofa_ledger_book_render_get_instance_private( OFA_LEDGER_BOOK_RENDER( instance ));

	ofs_currency_list_free( &priv->report_totals );
	free_ledgers( &priv->ledgers_summary );

	clear_ledger_data( OFA_LEDGER_BOOK_RENDER( instance ));
}

static void
clear_ledger_data( ofaLedgerBookRender *self )
{
	ofaLedgerBookRenderPrivate *priv;

	priv = ofa_ledger_book_render_get_instance_private( self );

	g_free( priv->ledger_mnemo );
	priv->ledger_mnemo = NULL;

	ofs_currency_list_free( &priv->ledger_totals );
}

/*
 * draw the total for this ledger by currencies
 * update the last_y accordingly
 */
static void
draw_ledger_totals( ofaIRenderable *instance )
{
	ofaLedgerBookRenderPrivate *priv;
	static const gdouble st_vspace_rate = 0.4;
	gdouble y, height;
	gboolean first;
	gchar *str;
	GList *it;
	ofsCurrency *scur;

	priv = ofa_ledger_book_render_get_instance_private( OFA_LEDGER_BOOK_RENDER( instance ));

	y = ofa_irenderable_get_last_y( instance );
	height = 0;

	for( it=priv->ledger_totals, first=TRUE ; it ; it=it->next ){
		scur = ( ofsCurrency * ) it->data;

		if( first ){
			str = g_strdup_printf( _( "%s ledger balance : " ), priv->ledger_mnemo );
			height = ofa_irenderable_set_text( instance,
					priv->body_debit_rtab-priv->amount_width, y,
					str, PANGO_ALIGN_RIGHT );
			g_free( str );
			first = FALSE;
		}

		str = ofa_amount_to_str( scur->debit, scur->currency, priv->getter );
		ofa_irenderable_set_text( instance,
				priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = ofa_amount_to_str( scur->credit, scur->currency, priv->getter );
		ofa_irenderable_set_text( instance,
				priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		ofa_irenderable_set_text( instance,
				priv->body_currency_rtab, y, ofo_currency_get_code( scur->currency ), PANGO_ALIGN_RIGHT );

		y += height * ( 1+st_vspace_rate );
	}

	ofa_irenderable_set_last_y( instance, y );
}

static void
free_ledgers( GList **ledgers )
{
	g_list_free_full( *ledgers, ( GDestroyNotify ) free_ledger );
	*ledgers = NULL;
}

static void
free_ledger( sLedger *ledger )
{
	ofs_currency_list_free( &ledger->totals );
	g_free( ledger );
}

/*
 * settings = paned_position;
 */
static void
read_settings( ofaLedgerBookRender *self )
{
	ofaLedgerBookRenderPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;
	GtkWidget *paned;
	gint pos;
	gchar *key;

	priv = ofa_ledger_book_render_get_instance_private( self );

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
write_settings( ofaLedgerBookRender *self )
{
	ofaLedgerBookRenderPrivate *priv;
	myISettings *settings;
	GtkWidget *paned;
	gchar *str, *key;

	priv = ofa_ledger_book_render_get_instance_private( self );

	paned = ofa_render_page_get_top_paned( OFA_RENDER_PAGE( self ));

	str = g_strdup_printf( "%d;",
			gtk_paned_get_position( GTK_PANED( paned )));

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( str );
	g_free( key );
}
