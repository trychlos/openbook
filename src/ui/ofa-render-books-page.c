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
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofs-currency.h"

#include "core/ofa-iconcil.h"

#include "ui/ofa-iaccounts-filter.h"
#include "ui/ofa-idates-filter.h"
#include "ui/ofa-irenderable.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-page.h"
#include "ui/ofa-page-prot.h"
#include "ui/ofa-render-books-bin.h"
#include "ui/ofa-render-books-page.h"

/* private instance data
 */
struct _ofaRenderBooksPagePrivate {

	ofaRenderBooksBin *args_bin;

	/* internals
	 */
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

	/* general summary
	 */
	GList             *totals;			/* total of debit/credit per currency */
};

/*
 * Accounts balances print uses a portrait orientation
 */
#define THIS_PAGE_ORIENTATION            GTK_PAGE_ORIENTATION_LANDSCAPE
#define THIS_PAPER_NAME                  GTK_PAPER_NAME_A4

static const gchar *st_page_header_title = N_( "General Books Summary" );

static const gchar *st_print_settings    = "RenderBooksPrint";

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

static ofaRenderPageClass *ofa_render_books_page_parent_class = NULL;

static GType              register_type( void );
static void               render_books_page_finalize( GObject *instance );
static void               render_books_page_dispose( GObject *instance );
static void               render_books_page_instance_init( ofaRenderBooksPage *self );
static void               render_books_page_class_init( ofaRenderBooksPageClass *klass );
static void               v_init_view( ofaPage *page );
static GtkWidget         *v_get_top_focusable_widget( const ofaPage *page );
static GtkWidget         *v_get_args_widget( ofaRenderPage *page );
static const gchar       *v_get_paper_name( ofaRenderPage *page );
static GtkPageOrientation v_get_page_orientation( ofaRenderPage *page );
static void               v_get_print_settings( ofaRenderPage *page, GKeyFile **keyfile, gchar **group_name );
static void               on_args_changed( ofaRenderBooksBin *bin, ofaRenderBooksPage *page );
static void               irenderable_iface_init( ofaIRenderableInterface *iface );
static guint              irenderable_get_interface_version( const ofaIRenderable *instance );
static GList             *irenderable_get_dataset( ofaIRenderable *instance );
static void               irenderable_free_dataset( ofaIRenderable *instance, GList *elements );
static void               irenderable_reset_runtime( ofaIRenderable *instance );
static gboolean           irenderable_want_groups( const ofaIRenderable *instance );
static gboolean           irenderable_want_new_page( const ofaIRenderable *instance );
static void               irenderable_begin_render( ofaIRenderable *instance, gdouble render_width, gdouble render_height );
static const gchar       *irenderable_get_dossier_name( const ofaIRenderable *instance );
static gchar             *irenderable_get_page_header_title( const ofaIRenderable *instance );
static gchar             *irenderable_get_page_header_subtitle( const ofaIRenderable *instance );
static void               irenderable_draw_page_header_columns( ofaIRenderable *instance, gint page_num );
static gboolean           irenderable_is_new_group( const ofaIRenderable *instance, GList *current, GList *prev );
static void               irenderable_draw_group_header( ofaIRenderable *instance, GList *current );
static void               irenderable_draw_group_top_report( ofaIRenderable *instance );
static void               draw_account_report( ofaRenderBooksPage *self, gboolean with_solde );
static void               draw_account_solde_debit_credit( ofaRenderBooksPage *self, gdouble y );
static void               irenderable_draw_line( ofaIRenderable *instance, GList *current );
static void               irenderable_draw_group_bottom_report( ofaIRenderable *instance );
static void               irenderable_draw_group_footer( ofaIRenderable *instance );
static void               irenderable_draw_bottom_summary( ofaIRenderable *instance );

GType
ofa_render_books_page_get_type( void )
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
	static const gchar *thisfn = "ofo_render_books_page_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaRenderBooksPageClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) render_books_page_class_init,
		NULL,
		NULL,
		sizeof( ofaRenderBooksPage ),
		0,
		( GInstanceInitFunc ) render_books_page_instance_init
	};

	static const GInterfaceInfo irenderable_iface_info = {
		( GInterfaceInitFunc ) irenderable_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFA_TYPE_RENDER_PAGE, "ofaRenderBooksPage", &info, 0 );

	g_type_add_interface_static( type, OFA_TYPE_IRENDERABLE, &irenderable_iface_info );

	return( type );
}

static void
render_books_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_render_books_page_finalize";
	ofaRenderBooksPagePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RENDER_BOOKS_PAGE( instance ));

	/* free data members here */
	priv = OFA_RENDER_BOOKS_PAGE( instance )->priv;

	g_free( priv->from_account );
	g_free( priv->to_account );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_render_books_page_parent_class )->finalize( instance );
}

static void
render_books_page_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_RENDER_BOOKS_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_render_books_page_parent_class )->dispose( instance );
}

static void
render_books_page_instance_init( ofaRenderBooksPage *self )
{
	static const gchar *thisfn = "ofa_render_books_page_instance_init";

	g_return_if_fail( OFA_IS_RENDER_BOOKS_PAGE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_RENDER_BOOKS_PAGE, ofaRenderBooksPagePrivate );
}

static void
render_books_page_class_init( ofaRenderBooksPageClass *klass )
{
	static const gchar *thisfn = "ofa_render_books_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	ofa_render_books_page_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = render_books_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = render_books_page_finalize;

	OFA_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;

	OFA_RENDER_PAGE_CLASS( klass )->get_args_widget = v_get_args_widget;
	OFA_RENDER_PAGE_CLASS( klass )->get_paper_name = v_get_paper_name;
	OFA_RENDER_PAGE_CLASS( klass )->get_page_orientation = v_get_page_orientation;
	OFA_RENDER_PAGE_CLASS( klass )->get_print_settings = v_get_print_settings;

	g_type_class_add_private( klass, sizeof( ofaRenderBooksPagePrivate ));
}

static void
v_init_view( ofaPage *page )
{
	ofaRenderBooksPagePrivate *priv;

	OFA_PAGE_CLASS( ofa_render_books_page_parent_class )->init_view( page );

	priv = OFA_RENDER_BOOKS_PAGE( page )->priv;
	on_args_changed( priv->args_bin, OFA_RENDER_BOOKS_PAGE( page ));
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	return( NULL );
}

static GtkWidget *
v_get_args_widget( ofaRenderPage *page )
{
	ofaRenderBooksPagePrivate *priv;
	ofaRenderBooksBin *bin;

	priv = OFA_RENDER_BOOKS_PAGE( page )->priv;

	bin = ofa_render_books_bin_new( ofa_page_get_main_window( OFA_PAGE( page )));
	g_signal_connect( G_OBJECT( bin ), "ofa-changed", G_CALLBACK( on_args_changed ), page );
	priv->args_bin = bin;

	return( GTK_WIDGET( bin ));
}

static const gchar *
v_get_paper_name( ofaRenderPage *page )
{
	return( THIS_PAPER_NAME );
}

static GtkPageOrientation
v_get_page_orientation( ofaRenderPage *page )
{
	return( THIS_PAGE_ORIENTATION );
}

static void
v_get_print_settings( ofaRenderPage *page, GKeyFile **keyfile, gchar **group_name )
{
	*keyfile = ofa_settings_get_actual_keyfile( SETTINGS_TARGET_USER );
	*group_name = g_strdup( st_print_settings );
}

/*
 * ofaRenderBooksBin "ofa-changed" handler
 */
static void
on_args_changed( ofaRenderBooksBin *bin, ofaRenderBooksPage *page )
{
	gboolean valid;
	gchar *message;

	valid = ofa_render_books_bin_is_valid( bin, &message );
	ofa_render_page_set_args_valid( OFA_RENDER_PAGE( page ), valid, message );
	g_free( message );
}

static void
irenderable_iface_init( ofaIRenderableInterface *iface )
{
	static const gchar *thisfn = "ofa_render_books_page_irenderable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = irenderable_get_interface_version;
	iface->get_dataset = irenderable_get_dataset;
	iface->begin_render = irenderable_begin_render;
	iface->free_dataset = irenderable_free_dataset;
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

static GList *
irenderable_get_dataset( ofaIRenderable *instance )
{
	ofaRenderBooksPagePrivate *priv;
	ofaMainWindow *main_window;
	ofoDossier *dossier;
	GList *dataset;
	ofaIAccountsFilter *accounts_filter;
	ofaIDatesFilter *dates_filter;

	priv = OFA_RENDER_BOOKS_PAGE( instance )->priv;

	main_window = ofa_page_get_main_window( OFA_PAGE( instance ));
	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), NULL );
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	g_free( priv->from_account );
	g_free( priv->to_account );
	accounts_filter = ofa_render_books_bin_get_accounts_filter( priv->args_bin );
	priv->from_account = g_strdup( ofa_iaccounts_filter_get_account( accounts_filter, IACCOUNTS_FILTER_FROM ));
	priv->to_account = g_strdup( ofa_iaccounts_filter_get_account( accounts_filter, IACCOUNTS_FILTER_TO ));
	priv->all_accounts = ofa_iaccounts_filter_get_all_accounts( accounts_filter );

	priv->new_page = ofa_render_books_bin_get_new_page_per_account( priv->args_bin );

	dates_filter = ofa_render_books_bin_get_dates_filter( priv->args_bin );
	my_date_set_from_date( &priv->from_date, ofa_idates_filter_get_date( dates_filter, IDATES_FILTER_FROM ));
	my_date_set_from_date( &priv->to_date, ofa_idates_filter_get_date( dates_filter, IDATES_FILTER_TO ));

	dataset = ofo_entry_get_dataset_for_print_general_books(
						dossier,
						priv->all_accounts ? NULL : priv->from_account,
						priv->all_accounts ? NULL : priv->to_account,
						my_date_is_valid( &priv->from_date ) ? &priv->from_date : NULL,
						my_date_is_valid( &priv->to_date ) ? &priv->to_date : NULL );

	priv->count = g_list_length( dataset );

	return( dataset );
}

static void
irenderable_free_dataset( ofaIRenderable *instance, GList *elements )
{
	g_list_free_full( elements, ( GDestroyNotify ) g_object_unref );
}

static void
irenderable_reset_runtime( ofaIRenderable *instance )
{
	ofaRenderBooksPagePrivate *priv;

	priv = OFA_RENDER_BOOKS_PAGE( instance )->priv;

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
	ofaRenderBooksPagePrivate *priv;

	priv = OFA_RENDER_BOOKS_PAGE( instance )->priv;

	return( priv->new_page );
}

/*
 * mainly here: compute the tab positions
 */
static void
irenderable_begin_render( ofaIRenderable *instance, gdouble render_width, gdouble render_height )
{
	static const gchar *thisfn = "ofa_render_books_page_irenderable_begin_render";
	ofaRenderBooksPagePrivate *priv;

	priv = OFA_RENDER_BOOKS_PAGE( instance )->priv;

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

static const gchar *
irenderable_get_dossier_name( const ofaIRenderable *instance )
{
	ofaMainWindow *main_window;
	ofoDossier *dossier;

	main_window = ofa_page_get_main_window( OFA_PAGE( instance ));
	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), NULL );
	dossier = ofa_main_window_get_dossier( main_window );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	return( ofo_dossier_get_name( dossier ));
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
	ofaRenderBooksPagePrivate *priv;
	gchar *sfrom_date, *sto_date;
	GString *stitle;

	priv = OFA_RENDER_BOOKS_PAGE( instance )->priv;

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
	ofaRenderBooksPagePrivate *priv;
	static gdouble st_vspace_rate = 0.5;
	gdouble y, text_height, vspace;

	priv = OFA_RENDER_BOOKS_PAGE( instance )->priv;

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
	ofaRenderBooksPagePrivate *priv;
	static const gdouble st_vspace_rate = 0.4;
	ofaMainWindow *main_window;
	ofoDossier *dossier;
	gdouble y, height;
	ofoCurrency *currency;

	main_window = ofa_page_get_main_window( OFA_PAGE( instance ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	priv = OFA_RENDER_BOOKS_PAGE( instance )->priv;

	y = ofa_irenderable_get_last_y( instance );

	/* setup the account properties */
	g_free( priv->account_number );
	priv->account_number = g_strdup( ofo_entry_get_account( OFO_ENTRY( current->data )));

	priv->account_debit = 0;
	priv->account_credit = 0;

	priv->account_object = ofo_account_get_by_number( dossier, priv->account_number );
	g_return_if_fail( priv->account_object && OFO_IS_ACCOUNT( priv->account_object ));

	priv->currency_code = g_strdup( ofo_account_get_currency( priv->account_object ));

	currency = ofo_currency_get_by_code( dossier, priv->currency_code );
	g_return_if_fail( currency && OFO_IS_CURRENCY( currency ));

	priv->currency_digits = ofo_currency_get_digits( currency );

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
	draw_account_report( OFA_RENDER_BOOKS_PAGE( instance ), TRUE );
}

/*
 * draw total of debit and credits for this account
 * current balance is not printed on the bottom report (because already
 * appears on the immediate previous line)
 */
static void
draw_account_report( ofaRenderBooksPage *self, gboolean with_solde )
{
	ofaRenderBooksPagePrivate *priv;
	static const gdouble st_vspace_rate = 0.4;
	gchar *str;
	gdouble y, height;

	priv = self->priv;

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
		str = my_double_to_str_ex( priv->account_debit, priv->currency_digits );
		ofa_irenderable_set_text( OFA_IRENDERABLE( self ),
				priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = my_double_to_str_ex( priv->account_credit, priv->currency_digits );
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
draw_account_solde_debit_credit( ofaRenderBooksPage *self, gdouble y )
{
	ofaRenderBooksPagePrivate *priv;
	ofxAmount amount;
	gchar *str;

	priv = self->priv;

	/* current account balance
	 * if current balance is zero, then also print it */
	amount = priv->account_credit - priv->account_debit;
	if( amount >= 0 ){
		str = my_double_to_str_ex( amount, priv->currency_digits );
		ofa_irenderable_set_text( OFA_IRENDERABLE( self ),
				priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		ofa_irenderable_set_text( OFA_IRENDERABLE( self ),
				priv->body_solde_sens_rtab, y, _( "CR" ), PANGO_ALIGN_RIGHT );

	} else {
		str = my_double_to_str_ex( -1*amount, priv->currency_digits );
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
	ofaRenderBooksPagePrivate *priv;
	ofaMainWindow *main_window;
	ofoDossier *dossier;
	ofoEntry *entry;
	const gchar *cstr;
	gchar *str;
	gdouble y;
	ofxAmount amount;
	ofoConcil *concil;

	main_window = ofa_page_get_main_window( OFA_PAGE( instance ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	priv = OFA_RENDER_BOOKS_PAGE( instance )->priv;

	y = ofa_irenderable_get_last_y( instance );
	entry = OFO_ENTRY( current->data );

	/* operation date */
	str = my_date_to_str( ofo_entry_get_dope( entry ), ofa_prefs_date_display());
	ofa_irenderable_set_text( instance,
			priv->body_dope_ltab, y, str, PANGO_ALIGN_LEFT );
	g_free( str );

	/* effect date */
	str = my_date_to_str( ofo_entry_get_deffect( entry ), ofa_prefs_date_display());
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
	concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ), dossier );
	if( concil ){
		ofa_irenderable_set_text( instance,
				priv->body_reconcil_ctab, y, _( "R" ), PANGO_ALIGN_CENTER );
	}

	/* debit */
	amount = ofo_entry_get_debit( entry );
	if( amount ){
		str = my_double_to_str_ex( amount, priv->currency_digits );
		ofa_irenderable_set_text( instance,
				priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		priv->account_debit += amount;
	}

	/* credit */
	amount = ofo_entry_get_credit( entry );
	if( amount ){
		str = my_double_to_str_ex( amount, priv->currency_digits );
		ofa_irenderable_set_text( instance,
				priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		priv->account_credit += amount;
	}

	/* current account solde */
	draw_account_solde_debit_credit( OFA_RENDER_BOOKS_PAGE( instance ), y );
}

static void
irenderable_draw_group_bottom_report( ofaIRenderable *instance )
{
	draw_account_report( OFA_RENDER_BOOKS_PAGE( instance ), FALSE );
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
	ofaRenderBooksPagePrivate *priv;
	static const gdouble st_vspace_rate = 0.4;
	gchar *str;
	gdouble y, height;
	gboolean is_paginating;

	priv = OFA_RENDER_BOOKS_PAGE( instance )->priv;

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
		str = my_double_to_str_ex( priv->account_debit, priv->currency_digits );
		ofa_irenderable_set_text( instance,
				priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		/* solde credit */
		str = my_double_to_str_ex( priv->account_credit, priv->currency_digits );
		ofa_irenderable_set_text( instance,
				priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		/* current account solde */
		draw_account_solde_debit_credit( OFA_RENDER_BOOKS_PAGE( instance ), y );

		/* add the account balance to the total per currency */
		is_paginating = ofa_irenderable_is_paginating( instance );
		ofs_currency_add_currency( &priv->totals,
				priv->currency_code,
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
	ofaRenderBooksPagePrivate *priv;
	static const gdouble st_vspace_rate = 0.25;
	ofaMainWindow *main_window;
	ofoDossier *dossier;
	gdouble bottom, vspace, req_height, height, top;
	gchar *str;
	GList *it;
	ofsCurrency *scur;
	gboolean first;
	ofoCurrency *currency;
	gint digits;

	main_window = ofa_page_get_main_window( OFA_PAGE( instance ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	priv = OFA_RENDER_BOOKS_PAGE( instance )->priv;

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
		currency = ofo_currency_get_by_code( dossier, scur->currency );
		g_return_if_fail( currency && OFO_IS_CURRENCY( currency ));
		digits = ofo_currency_get_digits( currency );

		if( first ){
			ofa_irenderable_set_text( instance,
					priv->body_debit_rtab-st_amount_width, top,
					_( "General balance : " ), PANGO_ALIGN_RIGHT );
			first = FALSE;
		}

		str = my_double_to_str_ex( scur->debit, digits );
		ofa_irenderable_set_text( instance,
				priv->body_debit_rtab, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = my_double_to_str_ex( scur->credit, digits );
		ofa_irenderable_set_text( instance,
				priv->body_credit_rtab, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		ofa_irenderable_set_text( instance,
				priv->body_solde_sens_rtab, top, scur->currency, PANGO_ALIGN_RIGHT );

		top += height+vspace;
	}

	ofa_irenderable_set_last_y( instance, ofa_irenderable_get_last_y( instance ) + req_height );
}
