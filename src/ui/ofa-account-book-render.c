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
	gchar             *settings_prefix;
	ofaHub            *hub;
	gchar             *from_account;
	gchar             *to_account;
	gboolean           all_accounts;
	gboolean           new_page;
	GDate              from_date;
	GDate              to_date;
	gint               count;					/* count of returned entries */

	/* print datas
	 */
	gdouble            render_width;
	gdouble            render_height;
	gdouble            page_margin;

	/* layout for account header line
	 */
	gdouble            body_accnumber_ltab;
	gdouble            body_acclabel_ltab;
	gdouble            body_acclabel_max_size;
	gdouble            body_acccurrency_rtab;

	/* layout for account footer line
	 */
	gdouble            body_acflabel_max_size;

	/* layout for entry line
	 */
	gdouble            body_dope_ltab;
	gdouble            body_deffect_ltab;
	gdouble            body_ledger_ltab;
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
	GList             *totals;			/* total of debit/credit per currency */
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

/* the columns of the body */
static const gint st_body_font_size      = 9;

/* the columns of the account header line */
#define st_acccurrency_width             (gdouble) 23/10*st_body_font_size

/* the columns of the entry line */
#define st_date_width                    (gdouble) 54/9*st_body_font_size
#define st_ledger_width                  (gdouble) 36/9*st_body_font_size
#define st_piece_width                   (gdouble) 64/9*st_body_font_size
#define st_settlement_width              (gdouble) 8/9*st_body_font_size
#define st_reconcil_width                (gdouble) 8/9*st_body_font_size
#define st_amount_width                  (gdouble) 90/9*st_body_font_size
#define st_sens_width                    (gdouble) 18/9*st_body_font_size
#define st_column_hspacing               (gdouble) 4

static GtkWidget         *page_v_get_top_focusable_widget( const ofaPage *page );
static void               paned_page_v_init_view( ofaPanedPage *page );
static GtkWidget         *render_page_v_get_args_widget( ofaRenderPage *page );
static const gchar       *render_page_v_get_paper_name( ofaRenderPage *page );
static GtkPageOrientation render_page_v_get_page_orientation( ofaRenderPage *page );
static void               render_page_v_get_print_settings( ofaRenderPage *page, GKeyFile **keyfile, gchar **group_name );
static GList             *render_page_v_get_dataset( ofaRenderPage *page );
static void               render_page_v_free_dataset( ofaRenderPage *page, GList *dataset );
static void               on_args_changed( ofaAccountBookArgs *bin, ofaAccountBookRender *page );
static void               irenderable_iface_init( ofaIRenderableInterface *iface );
static guint              irenderable_get_interface_version( const ofaIRenderable *instance );
static void               irenderable_reset_runtime( ofaIRenderable *instance );
static gboolean           irenderable_want_groups( const ofaIRenderable *instance );
static gboolean           irenderable_want_new_page( const ofaIRenderable *instance );
static void               irenderable_begin_render( ofaIRenderable *instance, gdouble render_width, gdouble render_height );
static gchar             *irenderable_get_dossier_name( const ofaIRenderable *instance );
static gchar             *irenderable_get_page_header_title( const ofaIRenderable *instance );
static gchar             *irenderable_get_page_header_subtitle( const ofaIRenderable *instance );
static void               irenderable_draw_page_header_columns( ofaIRenderable *instance, gint page_num );
static gboolean           irenderable_is_new_group( const ofaIRenderable *instance, GList *current, GList *prev );
static void               irenderable_draw_group_header( ofaIRenderable *instance, GList *current );
static void               irenderable_draw_group_top_report( ofaIRenderable *instance );
static void               draw_account_report( ofaAccountBookRender *self, gboolean with_solde );
static void               draw_account_solde_debit_credit( ofaAccountBookRender *self, gdouble y );
static void               irenderable_draw_line( ofaIRenderable *instance, GList *current );
static void               irenderable_draw_group_bottom_report( ofaIRenderable *instance );
static void               irenderable_draw_group_footer( ofaIRenderable *instance );
static void               irenderable_draw_bottom_summary( ofaIRenderable *instance );
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
	g_return_if_fail( instance && OFA_IS_ACCOUNT_BOOK_RENDER( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		write_settings( OFA_ACCOUNT_BOOK_RENDER( instance ));

		/* unref object members here */
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

	priv->hub = ofa_igetter_get_hub( OFA_IGETTER( page ));
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	on_args_changed( priv->args_bin, OFA_ACCOUNT_BOOK_RENDER( page ));

	read_settings( OFA_ACCOUNT_BOOK_RENDER( page ));
}

static GtkWidget *
render_page_v_get_args_widget( ofaRenderPage *page )
{
	ofaAccountBookRenderPrivate *priv;
	ofaAccountBookArgs *bin;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( page ));

	bin = ofa_account_book_args_new( OFA_IGETTER( page ), priv->settings_prefix );
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

	settings = ofa_hub_get_user_settings( priv->hub );
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
	GList *dataset;
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
						priv->hub,
						priv->all_accounts ? NULL : priv->from_account,
						priv->all_accounts ? NULL : priv->to_account,
						my_date_is_valid( &priv->from_date ) ? &priv->from_date : NULL,
						my_date_is_valid( &priv->to_date ) ? &priv->to_date : NULL );

	priv->count = g_list_length( dataset );

	return( dataset );
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
	iface->reset_runtime = irenderable_reset_runtime;
	iface->want_groups = irenderable_want_groups;
	iface->want_new_page = irenderable_want_new_page;
	iface->get_dossier_name = irenderable_get_dossier_name;
	iface->get_page_header_title = irenderable_get_page_header_title;
	iface->get_page_header_subtitle = irenderable_get_page_header_subtitle;
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
	ofaAccountBookRenderPrivate *priv;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

	ofs_currency_list_free( &priv->totals );

	g_free( priv->account_number );
	priv->account_number = NULL;
}

static gboolean
irenderable_want_groups( const ofaIRenderable *instance )
{
	return( TRUE );
}

static gboolean
irenderable_want_new_page( const ofaIRenderable *instance )
{
	ofaAccountBookRenderPrivate *priv;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

	priv->new_page = ofa_account_book_args_get_new_page_per_account( priv->args_bin );

	return( priv->new_page );
}

/*
 * mainly here: compute the tab positions
 */
static void
irenderable_begin_render( ofaIRenderable *instance, gdouble render_width, gdouble render_height )
{
	static const gchar *thisfn = "ofa_account_book_render_irenderable_begin_render";
	ofaAccountBookRenderPrivate *priv;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

	g_debug( "%s: instance=%p, render_width=%lf, render_height=%lf",
			thisfn, ( void * ) instance, render_width, render_height );

	priv->render_width = render_width;
	priv->render_height = render_height;
	priv->page_margin = ofa_irenderable_get_page_margin( instance );

	/* entry line, starting from the left */
	priv->body_dope_ltab = priv->page_margin;
	priv->body_deffect_ltab = priv->body_dope_ltab + st_date_width + st_column_hspacing;
	priv->body_ledger_ltab = priv->body_deffect_ltab + st_date_width + st_column_hspacing;
	priv->body_piece_ltab = priv->body_ledger_ltab + st_ledger_width + st_column_hspacing;
	priv->body_label_ltab = priv->body_piece_ltab + st_piece_width + st_column_hspacing;

	/* entry line, starting from the right */
	priv->body_solde_sens_rtab = priv->render_width - priv->page_margin;
	priv->body_solde_rtab = priv->body_solde_sens_rtab - st_sens_width - st_column_hspacing/2;
	priv->body_credit_rtab = priv->body_solde_rtab - st_amount_width - st_column_hspacing;
	priv->body_debit_rtab = priv->body_credit_rtab - st_amount_width - st_column_hspacing;
	priv->body_reconcil_ctab = priv->body_debit_rtab - st_amount_width - st_column_hspacing - st_reconcil_width/2;
	priv->body_settlement_ctab = priv->body_reconcil_ctab - st_reconcil_width/2 - st_column_hspacing - st_settlement_width/2;

	/* account header, starting from the left
	 * computed here because aligned on (and so relying on) body effect date */
	priv->body_accnumber_ltab = priv->page_margin;
	priv->body_acclabel_ltab = priv->body_deffect_ltab;
	priv->body_acccurrency_rtab = priv->render_width - priv->page_margin;

	/* max size in Pango units */
	priv->body_acclabel_max_size = ( priv->body_acccurrency_rtab - st_acccurrency_width - st_column_hspacing - priv->body_acclabel_ltab )*PANGO_SCALE;
	priv->body_acflabel_max_size = ( priv->body_debit_rtab - st_amount_width - st_column_hspacing - priv->page_margin )*PANGO_SCALE;
	priv->body_piece_max_size = st_piece_width*PANGO_SCALE;
	priv->body_label_max_size = ( priv->body_settlement_ctab - st_column_hspacing - priv->body_label_ltab )*PANGO_SCALE;
}

static gchar *
irenderable_get_dossier_name( const ofaIRenderable *instance )
{
	ofaAccountBookRenderPrivate *priv;
	const ofaIDBConnect *connect;
	ofaIDBDossierMeta *meta;
	const gchar *dossier_name;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

	connect = ofa_hub_get_connect( priv->hub );
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

/*
 * Account from xxx to xxx - Date from xxx to xxx
 */
static gchar *
irenderable_get_page_header_subtitle( const ofaIRenderable *instance )
{
	ofaAccountBookRenderPrivate *priv;
	gchar *sfrom_date, *sto_date;
	GString *stitle;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

	/* recall of account and date selections in line 4 */
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
		sfrom_date = my_date_to_str( &priv->from_date, ofa_prefs_date_display( priv->hub ));
		sto_date = my_date_to_str( &priv->to_date, ofa_prefs_date_display( priv->hub ));
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
				ofo_entry_get_account( current_entry ),
				ofo_entry_get_account( prev_entry )) != 0 );
}

/*
 * draw account header
 */
static void
irenderable_draw_group_header( ofaIRenderable *instance, GList *current )
{
	ofaAccountBookRenderPrivate *priv;
	static const gdouble st_vspace_rate = 0.4;
	gdouble y, height;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

	y = ofa_irenderable_get_last_y( instance );

	/* setup the account properties */
	g_free( priv->account_number );
	priv->account_number = g_strdup( ofo_entry_get_account( OFO_ENTRY( current->data )));

	priv->account_debit = 0;
	priv->account_credit = 0;

	priv->account_object = ofo_account_get_by_number( priv->hub, priv->account_number );
	g_return_if_fail( priv->account_object && OFO_IS_ACCOUNT( priv->account_object ));

	priv->currency_code = g_strdup( ofo_account_get_currency( priv->account_object ));

	priv->currency_object = ofo_currency_get_by_code( priv->hub, priv->currency_code );
	g_return_if_fail( priv->currency_object && OFO_IS_CURRENCY( priv->currency_object ));

	priv->currency_digits = ofo_currency_get_digits( priv->currency_object );

	/* display the account header */
	/* account number */
	height = ofa_irenderable_set_text( instance,
			priv->body_accnumber_ltab, y,
			ofo_account_get_number( priv->account_object ), PANGO_ALIGN_LEFT );

	/* account label */
	ofa_irenderable_ellipsize_text( instance,
			priv->body_acclabel_ltab, y,
			ofo_account_get_label( priv->account_object ), priv->body_acclabel_max_size );

	/* account currency */
	ofa_irenderable_set_text( instance,
			priv->body_acccurrency_rtab, y,
			ofo_account_get_currency( priv->account_object ), PANGO_ALIGN_RIGHT );

	y += height * ( 1+st_vspace_rate );
	ofa_irenderable_set_last_y( instance, y );
}

static void
irenderable_draw_group_top_report( ofaIRenderable *instance )
{
	draw_account_report( OFA_ACCOUNT_BOOK_RENDER( instance ), TRUE );
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
	static const gdouble st_vspace_rate = 0.4;
	gchar *str;
	gdouble y, height;

	priv = ofa_account_book_render_get_instance_private( self );

	y = ofa_irenderable_get_last_y( OFA_IRENDERABLE( self ));
	height = ofa_irenderable_get_text_height( OFA_IRENDERABLE( self ));

	if( priv->account_object ){
		/* account number */
		ofa_irenderable_set_text( OFA_IRENDERABLE( self ),
				priv->body_accnumber_ltab, y,
				ofo_account_get_number( priv->account_object ), PANGO_ALIGN_LEFT );

		/* account label */
		ofa_irenderable_ellipsize_text( OFA_IRENDERABLE( self ),
				priv->body_acclabel_ltab, y,
				ofo_account_get_label( priv->account_object ), priv->body_acclabel_max_size );

		/* current account balance */
		str = ofa_amount_to_str( priv->account_debit, priv->currency_object, priv->hub );
		ofa_irenderable_set_text( OFA_IRENDERABLE( self ),
				priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = ofa_amount_to_str( priv->account_credit, priv->currency_object, priv->hub );
		ofa_irenderable_set_text( OFA_IRENDERABLE( self ),
				priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		/* current account solde */
		if( with_solde ){
			draw_account_solde_debit_credit( self, y );
		}
	}

	y += height * ( 1+st_vspace_rate );
	ofa_irenderable_set_last_y( OFA_IRENDERABLE( self ), y );
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
		str = ofa_amount_to_str( amount, priv->currency_object, priv->hub );
		ofa_irenderable_set_text( OFA_IRENDERABLE( self ),
				priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		ofa_irenderable_set_text( OFA_IRENDERABLE( self ),
				priv->body_solde_sens_rtab, y, _( "CR" ), PANGO_ALIGN_RIGHT );

	} else {
		str = ofa_amount_to_str( -1*amount, priv->currency_object, priv->hub );
		ofa_irenderable_set_text( OFA_IRENDERABLE( self ),
				priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		ofa_irenderable_set_text( OFA_IRENDERABLE( self ),
				priv->body_solde_sens_rtab, y, _( "DB" ), PANGO_ALIGN_RIGHT );
	}
}

/*
 * each line update the account sum of debits and credits
 * the total of debits/credits for this currency is incremented in
 * group footer
 */
static void
irenderable_draw_line( ofaIRenderable *instance, GList *current )
{
	ofaAccountBookRenderPrivate *priv;
	ofoEntry *entry;
	const gchar *cstr;
	gchar *str;
	gdouble y;
	ofxAmount amount;
	ofoConcil *concil;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

	y = ofa_irenderable_get_last_y( instance );
	entry = OFO_ENTRY( current->data );

	/* operation date */
	str = my_date_to_str( ofo_entry_get_dope( entry ), ofa_prefs_date_display( priv->hub ));
	ofa_irenderable_set_text( instance,
			priv->body_dope_ltab, y, str, PANGO_ALIGN_LEFT );
	g_free( str );

	/* effect date */
	str = my_date_to_str( ofo_entry_get_deffect( entry ), ofa_prefs_date_display( priv->hub ));
	ofa_irenderable_set_text( instance,
			priv->body_deffect_ltab, y, str, PANGO_ALIGN_LEFT );
	g_free( str );

	/* ledger */
	ofa_irenderable_set_text( instance,
			priv->body_ledger_ltab, y, ofo_entry_get_ledger( entry ), PANGO_ALIGN_LEFT );

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
		str = ofa_amount_to_str( amount, priv->currency_object, priv->hub );
		ofa_irenderable_set_text( instance,
				priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		priv->account_debit += amount;
	}

	/* credit */
	amount = ofo_entry_get_credit( entry );
	if( amount ){
		str = ofa_amount_to_str( amount, priv->currency_object, priv->hub );
		ofa_irenderable_set_text( instance,
				priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		priv->account_credit += amount;
	}

	/* current account solde */
	draw_account_solde_debit_credit( OFA_ACCOUNT_BOOK_RENDER( instance ), y );
}

static void
irenderable_draw_group_bottom_report( ofaIRenderable *instance )
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
	ofaAccountBookRenderPrivate *priv;
	static const gdouble st_vspace_rate = 0.4;
	gchar *str;
	gdouble y, height;
	gboolean is_paginating;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

	y = ofa_irenderable_get_last_y( instance );
	height = 0;

	if( priv->account_number ){
		/* label */
		str = g_strdup_printf( _( "Balance for account %s - %s" ),
				priv->account_number,
				ofo_account_get_label( priv->account_object ));
		height = ofa_irenderable_ellipsize_text( instance,
				priv->page_margin, y, str, priv->body_acflabel_max_size );
		g_free( str );

		/* solde debit */
		str = ofa_amount_to_str( priv->account_debit, priv->currency_object, priv->hub );
		ofa_irenderable_set_text( instance,
				priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		/* solde credit */
		str = ofa_amount_to_str( priv->account_credit, priv->currency_object, priv->hub );
		ofa_irenderable_set_text( instance,
				priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		/* current account solde */
		draw_account_solde_debit_credit( OFA_ACCOUNT_BOOK_RENDER( instance ), y );

		/* add the account balance to the total per currency */
		is_paginating = ofa_irenderable_is_paginating( instance );
		ofs_currency_add_by_object( &priv->totals, priv->currency_object,
				is_paginating ? 0 : priv->account_debit,
				is_paginating ? 0 : priv->account_credit );
	}

	y += height * ( 1+st_vspace_rate );
	ofa_irenderable_set_last_y( instance, y );
}

/*
 * print a line per found currency at the end of the printing
 */
static void
irenderable_draw_bottom_summary( ofaIRenderable *instance )
{
	ofaAccountBookRenderPrivate *priv;
	static const gdouble st_vspace_rate = 0.25;
	gdouble bottom, vspace, req_height, height, top;
	gchar *str;
	GList *it;
	ofsCurrency *scur;
	gboolean first;

	priv = ofa_account_book_render_get_instance_private( OFA_ACCOUNT_BOOK_RENDER( instance ));

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

	for( it=priv->totals, first=TRUE ; it ; it=it->next ){
		scur = ( ofsCurrency * ) it->data;

		if( first ){
			ofa_irenderable_set_text( instance,
					priv->body_debit_rtab-st_amount_width, top,
					_( "General balance : " ), PANGO_ALIGN_RIGHT );
			first = FALSE;
		}

		str = ofa_amount_to_str( scur->debit, scur->currency, priv->hub );
		ofa_irenderable_set_text( instance,
				priv->body_debit_rtab, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = ofa_amount_to_str( scur->credit, scur->currency, priv->hub );
		ofa_irenderable_set_text( instance,
				priv->body_credit_rtab, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		ofa_irenderable_set_text( instance,
				priv->body_solde_sens_rtab, top, ofo_currency_get_code( scur->currency ), PANGO_ALIGN_RIGHT );

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

	settings = ofa_hub_get_user_settings( priv->hub );
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

	settings = ofa_hub_get_user_settings( priv->hub );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( key );
	g_free( str );
}
