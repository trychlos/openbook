/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "core/my-window-prot.h"

#include "ui/my-editable-date.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-iprintable.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-pdf-books.h"

/* private instance data
 */
struct _ofaPDFBooksPrivate {

	gboolean       printed;

	/* UI
	 */
	GtkWidget     *from_account_etiq;	/* account selection */
	GtkWidget     *from_account_entry;
	GtkWidget     *from_account_btn;
	GtkWidget     *from_account_label;
	GtkWidget     *to_account_etiq;
	GtkWidget     *to_account_entry;
	GtkWidget     *to_account_btn;
	GtkWidget     *to_account_label;
	GtkWidget     *all_accounts_btn;

	GtkWidget     *from_date_entry;		/* date selection */
	GtkWidget     *to_date_entry;

	GtkWidget     *new_page_btn;		/* whether have a new page per account */

	/* internals
	 */
	gchar         *from_account;
	gchar         *to_account;
	gboolean       all_accounts;
	GDate          from_date;
	GDate          to_date;
	gboolean       new_page;
	gint           count;				/* count of returned entries */

	/* layout for account header line
	 */
	gdouble        page_width;
	gdouble        page_margin;
	gdouble        body_accnumber_ltab;
	gdouble        body_acclabel_ltab;
	gdouble        body_acclabel_max_size;
	gdouble        body_acccurrency_rtab;

	/* layout for account footer line
	 */
	gdouble        body_acflabel_max_size;

	/* layout for entry line
	 */
	gdouble        body_dope_ltab;
	gdouble        body_deffect_ltab;
	gdouble        body_ledger_ltab;
	gdouble        body_piece_ltab;
	gdouble        body_piece_max_size;
	gdouble        body_label_ltab;
	gint           body_label_max_size;		/* Pango units */
	gdouble        body_settlement_ctab;
	gdouble        body_reconcil_ctab;
	gdouble        body_debit_rtab;
	gdouble        body_credit_rtab;
	gdouble        body_solde_rtab;
	gdouble        body_solde_sens_rtab;

	/* for each account
	 */
	gchar         *account_number;
	ofxAmount      account_debit;
	ofxAmount      account_credit;
	ofoAccount    *account_object;
	gchar         *currency_code;
	gint           currency_digits;

	/* total general
	 */
	GList         *totals;				/* list of totals per currency */
};

typedef struct {
	gchar    *currency;
	ofxAmount debit;
	ofxAmount credit;
}
	sCurrency;

static const gchar *st_ui_xml              = PKGUIDIR "/ofa-print-books.ui";
static const gchar *st_ui_id               = "PrintBooksDlg";

static const gchar *st_pref_fname          = "PDFBooksFilename";
static const gchar *st_pref_from_account   = "PDFBooksFromAccount";
static const gchar *st_pref_to_account     = "PDFBooksToAccount";
static const gchar *st_pref_all_accounts   = "PDFBooksAllAccounts";
static const gchar *st_pref_from_date      = "PDFBooksFromDate";
static const gchar *st_pref_to_date        = "PDFBooksToDate";
static const gchar *st_pref_new_page       = "PDFBooksNewPage";

static const gchar *st_def_fname           = "GeneralBooks";
static const gchar *st_page_header_title   = N_( "General Books Summary" );

/* these are parms which describe the page layout
 */
static const gint   st_default_font_size   = 9;
static const gint   st_default_orientation = GTK_PAGE_ORIENTATION_LANDSCAPE;

/* Apart from each page header, we have three types of lines:
 * - account header, when the account changes
 *   Number Label Currency
 * - entry line, inside of an account
 *   DOpe DEffect Ledger Piece Label Settlement Debit Credit Solde
 * - total for the account
 *   label Debit Credit Solde with DB/CR indicator
 * - total general at the end of the summary
 */
/* the columns of the account header line */
/*#define st_accnumber_width                 (gdouble) 60/9*st_default_font_size*/
#define st_acccurrency_width               (gdouble) 23/10*st_default_font_size

/* the columns of the entry line */
#define st_date_width                      (gdouble) 54/9*st_default_font_size
#define st_ledger_width                    (gdouble) 36/9*st_default_font_size
#define st_piece_width                     (gdouble) 64/9*st_default_font_size
#define st_settlement_width                (gdouble) 8/9*st_default_font_size
#define st_reconcil_width                  (gdouble) 8/9*st_default_font_size
#define st_amount_width                    (gdouble) 90/9*st_default_font_size
#define st_sens_width                      (gdouble) 18/9*st_default_font_size
#define st_column_hspacing                 (gdouble) 4
/*
(openbook:29799): OFA-DEBUG: '99/99/9999   ' width=61
(openbook:29799): OFA-DEBUG: 'XXXXXX   ' width=46   -> 107
(openbook:29799): OFA-DEBUG: 'XXXXXXXXXX    ' width=71 ->
(openbook:29799): OFA-DEBUG: 'XXXXXXXXXX' width=62
(openbook:29799): OFA-DEBUG: 'XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX   ' width=441
(openbook:29799): OFA-DEBUG: '   99 999 999,99' width=75
1 space ~ 3px
70 chars = 432 => 1'X' ~ 6.17 px
For 72dpi resolution, we have ~2.835 dots/mm
*/

#define COLOR_BLACK                 0,      0,      0
#define COLOR_WHITE                 1,      1,      1
#define COLOR_DARK_RED              0.5,    0,      0
#define COLOR_DARK_CYAN             0,      0.5156, 0.5156
#define COLOR_LIGHT_GRAY            0.9375, 0.9375, 0.9375

static void     iprintable_iface_init( ofaIPrintableInterface *iface );
static guint    iprintable_get_interface_version( const ofaIPrintable *instance );
static void     v_init_dialog( myDialog *dialog );
static void     init_account_selection( ofaPDFBooks *self );
static void     init_date_selection( ofaPDFBooks *self );
static void     on_from_account_changed( GtkEntry *entry, ofaPDFBooks *self );
static void     on_from_account_select( GtkButton *button, ofaPDFBooks *self );
static void     on_to_account_changed( GtkEntry *entry, ofaPDFBooks *self );
static void     on_to_account_select( GtkButton *button, ofaPDFBooks *self );
static void     on_account_changed( GtkEntry *entry, ofaPDFBooks *self, GtkWidget *label );
static void     on_account_select( GtkButton *button, ofaPDFBooks *self, GtkWidget *entry );
static void     on_all_accounts_toggled( GtkToggleButton *button, ofaPDFBooks *self );
static gboolean v_quit_on_ok( myDialog *dialog );
static gboolean do_apply( ofaPDFBooks *self );
static GList   *iprintable_get_dataset( const ofaIPrintable *instance );
static void     iprintable_free_dataset( GList *elements );
static void     iprintable_reset_runtime( ofaIPrintable *instance );
static void     iprintable_on_begin_print( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static gchar   *iprintable_get_page_header_title( const ofaIPrintable *instance );
static gchar   *iprintable_get_page_header_subtitle( const ofaIPrintable *instance );
static void     iprintable_draw_page_header_columns( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static gboolean iprintable_is_new_group( const ofaIPrintable *instance, GList *current, GList *prev );
static void     iprintable_draw_group_header( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, GList *current );
static void     iprintable_draw_group_top_report( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static void     draw_account_report( ofaPDFBooks *self, GtkPrintOperation *operation, GtkPrintContext *context, gboolean with_solde );
static void     draw_account_solde_debit_credit( ofaPDFBooks *self, GtkPrintContext *context, gdouble y );
static void     iprintable_draw_line( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, GList *current );
static void     iprintable_draw_group_bottom_report( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static void     iprintable_draw_group_footer( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static void     add_account_balance( ofaPDFBooks *self, gboolean is_drawing );
static gint     cmp_currencies( const sCurrency *a, const sCurrency *b );
static void     iprintable_draw_bottom_summary( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );

G_DEFINE_TYPE_EXTENDED( ofaPDFBooks, ofa_pdf_books, OFA_TYPE_PDF_DIALOG, 0, \
		G_IMPLEMENT_INTERFACE (OFA_TYPE_IPRINTABLE, iprintable_iface_init ));

static void
free_currency( sCurrency *total_per_currency )
{
	g_free( total_per_currency->currency );
	g_free( total_per_currency );
}

static void
pdf_books_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_pdf_books_finalize";
	ofaPDFBooksPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PDF_BOOKS( instance ));

	/* free data members here */
	priv = OFA_PDF_BOOKS( instance )->priv;
	g_free( priv->from_account );
	g_free( priv->to_account );
	g_free( priv->account_number );
	g_free( priv->currency_code );
	g_list_free_full( priv->totals, ( GDestroyNotify ) free_currency );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_pdf_books_parent_class )->finalize( instance );
}

static void
pdf_books_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_PDF_BOOKS( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_pdf_books_parent_class )->dispose( instance );
}

static void
ofa_pdf_books_init( ofaPDFBooks *self )
{
	static const gchar *thisfn = "ofa_pdf_books_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PDF_BOOKS( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_PDF_BOOKS, ofaPDFBooksPrivate );
	my_date_clear( &self->priv->from_date );
	my_date_clear( &self->priv->to_date );
	self->priv->account_number = NULL;
}

static void
ofa_pdf_books_class_init( ofaPDFBooksClass *klass )
{
	static const gchar *thisfn = "ofa_pdf_books_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = pdf_books_dispose;
	G_OBJECT_CLASS( klass )->finalize = pdf_books_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaPDFBooksPrivate ));
}

static void
iprintable_iface_init( ofaIPrintableInterface *iface )
{
	static const gchar *thisfn = "ofa_print_balance_iprintable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iprintable_get_interface_version;
	iface->get_dataset = iprintable_get_dataset;
	iface->free_dataset = iprintable_free_dataset;
	iface->reset_runtime = iprintable_reset_runtime;
	iface->on_begin_print = iprintable_on_begin_print;
	iface->get_page_header_title = iprintable_get_page_header_title;
	iface->get_page_header_subtitle = iprintable_get_page_header_subtitle;
	iface->draw_page_header_columns = iprintable_draw_page_header_columns;
	iface->is_new_group = iprintable_is_new_group;
	iface->draw_group_header = iprintable_draw_group_header;
	iface->draw_group_top_report = iprintable_draw_group_top_report;
	iface->draw_line = iprintable_draw_line;
	iface->draw_group_bottom_report = iprintable_draw_group_bottom_report;
	iface->draw_group_footer = iprintable_draw_group_footer;
	iface->draw_bottom_summary = iprintable_draw_bottom_summary;
}

static guint
iprintable_get_interface_version( const ofaIPrintable *instance )
{
	return( 1 );
}

/**
 * ofa_pdf_books_run:
 * @main: the main window of the application.
 *
 * Print the accounts balance
 */
gboolean
ofa_pdf_books_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_pdf_books_run";
	ofaPDFBooks *self;
	gboolean printed;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new(
			OFA_TYPE_PDF_BOOKS,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				PDF_PROP_DEF_NAME,   st_def_fname,
				PDF_PROP_PREF_NAME,  st_pref_fname,
				NULL );

	my_dialog_run_dialog( MY_DIALOG( self ));

	printed = self->priv->printed;
	g_object_unref( self );

	return( printed );
}

static void
v_init_dialog( myDialog *dialog )
{
	init_account_selection( OFA_PDF_BOOKS( dialog ));
	init_date_selection( OFA_PDF_BOOKS( dialog ));
}

static void
init_account_selection( ofaPDFBooks *self )
{
	ofaPDFBooksPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *widget;
	gchar *text;
	gboolean bvalue;

	priv = self->priv;
	toplevel = my_window_get_toplevel( MY_WINDOW( self ));

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "from-account-etiq" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->from_account_etiq = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "from-account-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->from_account_label = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "from-account-entry" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_from_account_changed ), self );
	text = ofa_settings_get_string( st_pref_from_account );
	if( text && g_utf8_strlen( text, -1 )){
		gtk_entry_set_text( GTK_ENTRY( widget ), text );
	}
	g_free( text );
	priv->from_account_entry = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "from-account-select" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_from_account_select ), self );
	priv->from_account_btn = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "to-account-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->to_account_label = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "to-account-etiq" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->to_account_etiq = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "to-account-entry" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_to_account_changed ), self );
	text = ofa_settings_get_string( st_pref_to_account );
	if( text && g_utf8_strlen( text, -1 )){
		gtk_entry_set_text( GTK_ENTRY( widget ), text );
	}
	g_free( text );
	priv->to_account_entry = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "to-account-select" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_to_account_select ), self );
	priv->to_account_btn = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "all-accounts" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_all_accounts_toggled ), self );
	bvalue = ofa_settings_get_boolean( st_pref_all_accounts );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), !bvalue );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), bvalue );
	priv->all_accounts_btn = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "p3-one-page" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	bvalue = ofa_settings_get_boolean( st_pref_new_page );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), !bvalue );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), bvalue );
	priv->new_page_btn = widget;
}

static void
init_date_selection( ofaPDFBooks *self )
{
	ofaPDFBooksPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *widget;
	gchar *text;
	GDate date;

	priv = self->priv;
	toplevel = my_window_get_toplevel( MY_WINDOW( self ));

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "from-date-entry" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	my_editable_date_init( GTK_EDITABLE( widget ));
	my_editable_date_set_format( GTK_EDITABLE( widget ), MY_DATE_DMYY );
	my_editable_date_set_mandatory( GTK_EDITABLE( widget ), FALSE );
	priv->from_date_entry = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "from-date-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	my_editable_date_set_label( GTK_EDITABLE( priv->from_date_entry ), widget, MY_DATE_DMMM );
	text = ofa_settings_get_string( st_pref_from_date );
	if( text && g_utf8_strlen( text, -1 )){
		my_date_set_from_sql( &date, text );
		my_editable_date_set_date( GTK_EDITABLE( priv->from_date_entry ), &date );
	}
	g_free( text );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "to-date-entry" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	my_editable_date_init( GTK_EDITABLE( widget ));
	my_editable_date_set_format( GTK_EDITABLE( widget ), MY_DATE_DMYY );
	my_editable_date_set_mandatory( GTK_EDITABLE( widget ), FALSE );
	priv->to_date_entry = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "to-date-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	my_editable_date_set_label( GTK_EDITABLE( priv->to_date_entry ), widget, MY_DATE_DMMM );
	text = ofa_settings_get_string( st_pref_to_date );
	if( text && g_utf8_strlen( text, -1 )){
		my_date_set_from_sql( &date, text );
		my_editable_date_set_date( GTK_EDITABLE( priv->to_date_entry ), &date );
	}
	g_free( text );
}

static void
on_from_account_changed( GtkEntry *entry, ofaPDFBooks *self )
{
	on_account_changed( entry, self, self->priv->from_account_label );
}

static void
on_from_account_select( GtkButton *button, ofaPDFBooks *self )
{
	on_account_select( button, self, self->priv->from_account_entry );
}

static void
on_to_account_changed( GtkEntry *entry, ofaPDFBooks *self )
{
	on_account_changed( entry, self, self->priv->to_account_label );
}

static void
on_to_account_select( GtkButton *button, ofaPDFBooks *self )
{
	on_account_select( button, self, self->priv->to_account_entry );
}

static void
on_account_changed( GtkEntry *entry, ofaPDFBooks *self, GtkWidget *label )
{
	const gchar *str;
	ofoAccount *account;

	str = gtk_entry_get_text( entry );
	account = ofo_account_get_by_number( MY_WINDOW( self )->prot->dossier, str );
	if( account ){
		gtk_label_set_text( GTK_LABEL( label ), ofo_account_get_label( account ));
	} else {
		gtk_label_set_text( GTK_LABEL( label ), "" );
	}
}

static void
on_account_select( GtkButton *button, ofaPDFBooks *self, GtkWidget *entry )
{
	gchar *number;

	number = ofa_account_select_run(
					MY_WINDOW( self )->prot->main_window,
					gtk_entry_get_text( GTK_ENTRY( entry )));
	if( number ){
		gtk_entry_set_text( GTK_ENTRY( entry ), number );
		g_free( number );
	}
}

static void
on_all_accounts_toggled( GtkToggleButton *button, ofaPDFBooks *self )
{
	ofaPDFBooksPrivate *priv;
	gboolean bvalue;

	priv = self->priv;
	bvalue = gtk_toggle_button_get_active( button );

	gtk_widget_set_sensitive( priv->from_account_etiq, !bvalue );
	gtk_widget_set_sensitive( priv->from_account_entry, !bvalue );
	gtk_widget_set_sensitive( priv->from_account_btn, !bvalue );
	gtk_widget_set_sensitive( priv->from_account_label, !bvalue );

	gtk_widget_set_sensitive( priv->to_account_etiq, !bvalue );
	gtk_widget_set_sensitive( priv->to_account_entry, !bvalue );
	gtk_widget_set_sensitive( priv->to_account_btn, !bvalue );
	gtk_widget_set_sensitive( priv->to_account_label, !bvalue );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	gboolean ok;

	/* chain up to the parent class */
	ok = MY_DIALOG_CLASS( ofa_pdf_books_parent_class )->quit_on_ok( dialog );

	if( ok ){
		ok &= do_apply( OFA_PDF_BOOKS( dialog ));
	}

	if( ok ){
		ofa_iprintable_set_paper_orientation( OFA_IPRINTABLE( dialog ), st_default_orientation );
		ofa_iprintable_set_default_font_size( OFA_IPRINTABLE( dialog ), st_default_font_size );

		ok &= ofa_iprintable_print_to_pdf(
					OFA_IPRINTABLE( dialog ),
					ofa_pdf_dialog_get_filename( OFA_PDF_DIALOG( dialog )));
	}

	return( ok );
}

static gboolean
do_apply( ofaPDFBooks *self )
{
	static const gchar *thisfn = "ofa_pdf_books_do_apply";
	ofaPDFBooksPrivate *priv;
	gboolean all_accounts;
	gchar *text;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = self->priv;

	all_accounts = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->all_accounts_btn ));
	ofa_settings_set_boolean( st_pref_all_accounts, all_accounts );

	/* preferences are only saved if they have been useful */
	if( !all_accounts ){
		priv->from_account = g_strdup( gtk_entry_get_text( GTK_ENTRY( priv->from_account_entry )));
		ofa_settings_set_string( st_pref_from_account, priv->from_account );

		priv->to_account = g_strdup( gtk_entry_get_text( GTK_ENTRY( priv->to_account_entry )));
		ofa_settings_set_string( st_pref_to_account, priv->to_account );
	}

	my_date_set_from_date( &priv->from_date,
			my_editable_date_get_date( GTK_EDITABLE( priv->from_date_entry ), NULL ));
	text = my_date_to_str( &priv->from_date, MY_DATE_SQL );
	ofa_settings_set_string( st_pref_from_date, text );
	g_free( text );

	my_date_set_from_date( &priv->to_date,
			my_editable_date_get_date( GTK_EDITABLE( priv->to_date_entry ), NULL ));
	text = my_date_to_str( &priv->to_date, MY_DATE_SQL );
	ofa_settings_set_string( st_pref_to_date, text );
	g_free( text );

	priv->new_page = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->new_page_btn ));
	ofa_settings_set_boolean( st_pref_new_page, priv->new_page );
	ofa_iprintable_set_group_on_new_page( OFA_IPRINTABLE( self ), priv->new_page );

	return( TRUE );
}

static GList *
iprintable_get_dataset( const ofaIPrintable *instance )
{
	ofaPDFBooksPrivate *priv;
	GList *dataset;

	priv = OFA_PDF_BOOKS( instance )->priv;

	dataset = ofo_entry_get_dataset_for_print_general_books(
							MY_WINDOW( instance )->prot->dossier,
							priv->from_account, priv->to_account,
							&priv->from_date, &priv->to_date );

	priv->count = g_list_length( dataset );

	return( dataset );
}

static void
iprintable_free_dataset( GList *elements )
{
	g_list_free_full( elements, ( GDestroyNotify ) g_object_unref );
}

static void
iprintable_reset_runtime( ofaIPrintable *instance )
{
	ofaPDFBooksPrivate *priv;

	priv = OFA_PDF_BOOKS( instance )->priv;

	g_list_free_full( priv->totals, ( GDestroyNotify ) free_currency );
	priv->totals = NULL;

	g_free( priv->account_number );
	priv->account_number = NULL;
}

/*
 * mainly here: compute the tab positions
 */
static void
iprintable_on_begin_print( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	ofaPDFBooksPrivate *priv;

	priv = OFA_PDF_BOOKS( instance )->priv;

	priv->page_width = gtk_print_context_get_width( context );
	priv->page_margin = ofa_iprintable_get_page_margin( instance );

	/* entry line, starting from the left */
	priv->body_dope_ltab = priv->page_margin;
	priv->body_deffect_ltab = priv->body_dope_ltab + st_date_width + st_column_hspacing;
	priv->body_ledger_ltab = priv->body_deffect_ltab + st_date_width + st_column_hspacing;
	priv->body_piece_ltab = priv->body_ledger_ltab + st_ledger_width + st_column_hspacing;
	priv->body_label_ltab = priv->body_piece_ltab + st_piece_width + st_column_hspacing;

	/* entry line, starting from the right */
	priv->body_solde_sens_rtab = priv->page_width - priv->page_margin;
	priv->body_solde_rtab = priv->body_solde_sens_rtab - st_sens_width - st_column_hspacing/2;
	priv->body_credit_rtab = priv->body_solde_rtab - st_amount_width - st_column_hspacing;
	priv->body_debit_rtab = priv->body_credit_rtab - st_amount_width - st_column_hspacing;
	priv->body_reconcil_ctab = priv->body_debit_rtab - st_amount_width - st_column_hspacing - st_reconcil_width/2;
	priv->body_settlement_ctab = priv->body_reconcil_ctab - st_reconcil_width/2 - st_column_hspacing - st_settlement_width/2;

	/* account header, starting from the left
	 * computed here because aligned on (and so relying on) body effect date */
	priv->body_accnumber_ltab = priv->page_margin;
	priv->body_acclabel_ltab = priv->body_deffect_ltab;
	priv->body_acccurrency_rtab = priv->page_width - priv->page_margin;

	/* max size in Pango units */
	priv->body_acclabel_max_size = ( priv->body_acccurrency_rtab - st_acccurrency_width - st_column_hspacing - priv->body_acclabel_ltab )*PANGO_SCALE;
	priv->body_acflabel_max_size = ( priv->body_debit_rtab - st_amount_width - st_column_hspacing - priv->page_margin )*PANGO_SCALE;
	priv->body_piece_max_size = st_piece_width*PANGO_SCALE;
	priv->body_label_max_size = ( priv->body_settlement_ctab - st_column_hspacing - priv->body_label_ltab )*PANGO_SCALE;
}

static gchar *
iprintable_get_page_header_title( const ofaIPrintable *instance )
{
	return( g_strdup( st_page_header_title ));
}

/*
 * Account from xxx to xxx - Date from xxx to xxx
 */
static gchar *
iprintable_get_page_header_subtitle( const ofaIPrintable *instance )
{
	ofaPDFBooksPrivate *priv;
	gchar *sfrom_date, *sto_date;
	GString *stitle;

	priv = OFA_PDF_BOOKS( instance )->priv;

	/* recall of account and date selections in line 4 */
	stitle = g_string_new( "" );
	if( priv->all_accounts ||
			(( !priv->from_account || !g_utf8_strlen( priv->from_account, -1 )) &&
			 ( !priv->to_account || !g_utf8_strlen( priv->to_account, -1 )))){
		g_string_printf( stitle, _( "All accounts" ));
	} else {
		if( priv->from_account && g_utf8_strlen( priv->from_account, -1 )){
			g_string_printf( stitle, _( "From account %s" ), priv->from_account );
			if( priv->from_account && g_utf8_strlen( priv->from_account, -1 )){
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
		sfrom_date = my_date_to_str( &priv->from_date, MY_DATE_DMYY );
		sto_date = my_date_to_str( &priv->to_date, MY_DATE_DMYY );
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
iprintable_draw_page_header_columns( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	ofaPDFBooksPrivate *priv;
	gdouble y, vspace;

	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	priv = OFA_PDF_BOOKS( instance )->priv;

	y = ofa_iprintable_get_last_y( instance );
	vspace = ofa_iprintable_get_current_line_vspace( instance );
	y+= vspace;

	/* column headers */
	ofa_iprintable_set_text( instance, context,
			priv->body_dope_ltab, y,
			_( "Operation" ), PANGO_ALIGN_LEFT );

	ofa_iprintable_set_text( instance, context,
			priv->body_deffect_ltab, y,
			_( "Effect" ), PANGO_ALIGN_LEFT );

	ofa_iprintable_set_text( instance, context,
			priv->body_ledger_ltab, y,
			_( "Ledger" ), PANGO_ALIGN_LEFT );

	ofa_iprintable_set_text( instance, context,
			priv->body_piece_ltab, y,
			_( "Piece" ), PANGO_ALIGN_LEFT );

	ofa_iprintable_set_text( instance, context,
			priv->body_label_ltab, y,
			_( "Label" ), PANGO_ALIGN_LEFT );

	ofa_iprintable_set_text( instance, context,
			(priv->body_settlement_ctab+priv->body_reconcil_ctab)/2, y,
			_( "Set./Rec." ), PANGO_ALIGN_CENTER );

	ofa_iprintable_set_text( instance, context,
			priv->body_debit_rtab, y,
			_( "Debit" ), PANGO_ALIGN_RIGHT );

	ofa_iprintable_set_text( instance, context,
			priv->body_credit_rtab, y,
			_( "Credit" ), PANGO_ALIGN_RIGHT );

	ofa_iprintable_set_text( instance, context,
			priv->body_solde_sens_rtab, y,
			_( "Solde" ), PANGO_ALIGN_RIGHT );

	/* this set the 'y' height just after the column headers */
	y += ofa_iprintable_get_current_line_height( instance );
	ofa_iprintable_set_last_y( instance, y );
}

/*
 * just test if the current entry is on the same account than the
 * previous one
 */
static gboolean
iprintable_is_new_group( const ofaIPrintable *instance, GList *current, GList *prev )
{
	ofoEntry *current_entry, *prev_entry;

	g_return_val_if_fail( current, FALSE );

	if( !prev ){
		return( TRUE );
	}

	current_entry = OFO_ENTRY( current->data );
	prev_entry = OFO_ENTRY( prev->data );

	return( g_utf8_collate(
				ofo_entry_get_account( current_entry ), ofo_entry_get_account( prev_entry )) != 0 );
}

/*
 * draw account header
 */
static void
iprintable_draw_group_header( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, GList *current )
{
	ofaPDFBooksPrivate *priv;
	gdouble y;
	ofoCurrency *currency;

	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	priv = OFA_PDF_BOOKS( instance )->priv;

	y = ofa_iprintable_get_last_y( instance );

	/* setup the account properties */
	g_free( priv->account_number );
	priv->account_number = g_strdup( ofo_entry_get_account( OFO_ENTRY( current->data )));

	priv->account_debit = 0;
	priv->account_credit = 0;

	priv->account_object = ofo_account_get_by_number(
							MY_WINDOW( instance )->prot->dossier, priv->account_number );
	g_return_if_fail( priv->account_object && OFO_IS_ACCOUNT( priv->account_object ));

	priv->currency_code = g_strdup( ofo_account_get_currency( priv->account_object ));

	currency = ofo_currency_get_by_code(
							MY_WINDOW( instance )->prot->dossier, priv->currency_code );
	g_return_if_fail( currency && OFO_IS_CURRENCY( currency ));

	priv->currency_digits = ofo_currency_get_digits( currency );

	/* display the account header */
	/* account number */
	ofa_iprintable_set_text( instance, context,
			priv->body_accnumber_ltab, y,
			ofo_account_get_number( priv->account_object ), PANGO_ALIGN_LEFT );

	/* account label */
	ofa_iprintable_ellipsize_text( instance, context,
			priv->body_acclabel_ltab, y,
			ofo_account_get_label( priv->account_object ), priv->body_acclabel_max_size );

	/* account currency */
	ofa_iprintable_set_text( instance, context,
			priv->body_acccurrency_rtab, y,
			ofo_account_get_currency( priv->account_object ), PANGO_ALIGN_RIGHT );

	y += ofa_iprintable_get_current_line_height( instance );
	ofa_iprintable_set_last_y( instance, y );
}

static void
iprintable_draw_group_top_report( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	draw_account_report( OFA_PDF_BOOKS( instance ), operation, context, TRUE );
}

static void
draw_account_report( ofaPDFBooks *self, GtkPrintOperation *operation, GtkPrintContext *context, gboolean with_solde )
{
	ofaPDFBooksPrivate *priv;
	gchar *str;
	gdouble y;

	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	priv = self->priv;

	y = ofa_iprintable_get_last_y( OFA_IPRINTABLE( self ));

	/* account number */
	ofa_iprintable_set_text( OFA_IPRINTABLE( self ), context,
			priv->body_accnumber_ltab, y,
			ofo_account_get_number( priv->account_object ), PANGO_ALIGN_LEFT );

	/* account label */
	ofa_iprintable_ellipsize_text( OFA_IPRINTABLE( self ), context,
			priv->body_acclabel_ltab, y,
			ofo_account_get_label( priv->account_object ), priv->body_acclabel_max_size );

	/* current account balance */
	str = my_double_to_str_ex( priv->account_debit, priv->currency_digits );
	ofa_iprintable_set_text( OFA_IPRINTABLE( self ), context,
			priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	str = my_double_to_str_ex( priv->account_credit, priv->currency_digits );
	ofa_iprintable_set_text( OFA_IPRINTABLE( self ), context,
			priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	/* current account solde */
	if( with_solde ){
		draw_account_solde_debit_credit( self, context, y );
	}

	y += ofa_iprintable_get_current_line_height( OFA_IPRINTABLE( self ));
	ofa_iprintable_set_last_y( OFA_IPRINTABLE( self ), y );
}

/*
 * only run when is_drawing=TRUE
 */
static void
draw_account_solde_debit_credit( ofaPDFBooks *self, GtkPrintContext *context, gdouble y )
{
	ofaPDFBooksPrivate *priv;
	ofxAmount amount;
	gchar *str;

	priv = self->priv;

	/* current account balance */
	amount = priv->account_credit - priv->account_debit;
	if( amount ){
		if( amount > 0 ){
			str = my_double_to_str_ex( amount, priv->currency_digits );
			ofa_iprintable_set_text( OFA_IPRINTABLE( self ), context,
					priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
			g_free( str );
			ofa_iprintable_set_text( OFA_IPRINTABLE( self ), context,
					priv->body_solde_sens_rtab, y, _( "CR" ), PANGO_ALIGN_RIGHT );

		} else {
			str = my_double_to_str_ex( -1*amount, priv->currency_digits );
			ofa_iprintable_set_text( OFA_IPRINTABLE( self ), context,
					priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
			g_free( str );
			ofa_iprintable_set_text( OFA_IPRINTABLE( self ), context,
					priv->body_solde_sens_rtab, y, _( "DB" ), PANGO_ALIGN_RIGHT );
		}
	}
}

/*
 * (printable)width(A4)=559
 * date  journal  piece    label      debit   credit   solde
 * 10    6        max(10)  max(80)      15d      15d     15d
 *
 * Returns: %TRUE if we may continue to print on this page,
 * %FALSE when we have terminated the page.
 * If the page is terminated, this particular line may not have been
 * printed.
 */
static void
iprintable_draw_line( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, GList *current )
{
	ofaPDFBooksPrivate *priv;
	ofoEntry *entry;
	const gchar *cstr;
	gchar *str;
	gdouble y;
	ofxAmount amount;

	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	priv = OFA_PDF_BOOKS( instance )->priv;

	y = ofa_iprintable_get_last_y( instance );
	entry = OFO_ENTRY( current->data );

	/* operation date */
	str = my_date_to_str( ofo_entry_get_dope( entry ), MY_DATE_DMYY );
	ofa_iprintable_set_text( instance, context,
			priv->body_dope_ltab, y, str, PANGO_ALIGN_LEFT );
	g_free( str );

	/* effect date */
	str = my_date_to_str( ofo_entry_get_deffect( entry ), MY_DATE_DMYY );
	ofa_iprintable_set_text( instance, context,
			priv->body_deffect_ltab, y, str, PANGO_ALIGN_LEFT );
	g_free( str );

	/* ledger */
	ofa_iprintable_set_text( instance, context,
			priv->body_ledger_ltab, y, ofo_entry_get_ledger( entry ), PANGO_ALIGN_LEFT );

	/* piece */
	cstr = ofo_entry_get_ref( entry );
	if( cstr && g_utf8_strlen( cstr, -1 )){
		ofa_iprintable_ellipsize_text( instance, context,
				priv->body_piece_ltab, y,
				cstr, priv->body_piece_max_size );
	}

	/* label */
	ofa_iprintable_ellipsize_text( instance, context,
			priv->body_label_ltab, y,
			ofo_entry_get_label( entry ), priv->body_label_max_size );

	/* settlement ? */
	if( ofo_entry_get_settlement_number( entry ) > 0 ){
		ofa_iprintable_set_text( instance, context,
				priv->body_settlement_ctab, y, _( "S" ), PANGO_ALIGN_CENTER );
	}

	/* reconciliation */
	if( my_date_is_valid( ofo_entry_get_concil_dval( entry ))){
		ofa_iprintable_set_text( instance, context,
				priv->body_reconcil_ctab, y, _( "R" ), PANGO_ALIGN_CENTER );
	}

	/* debit */
	amount = ofo_entry_get_debit( entry );
	if( amount ){
		str = my_double_to_str_ex( amount, priv->currency_digits );
		ofa_iprintable_set_text( instance, context,
				priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		priv->account_debit += amount;
	}

	/* credit */
	amount = ofo_entry_get_credit( entry );
	if( amount ){
		str = my_double_to_str_ex( amount, priv->currency_digits );
		ofa_iprintable_set_text( instance, context,
				priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		priv->account_credit += amount;
	}

	/* current account solde */
	draw_account_solde_debit_credit( OFA_PDF_BOOKS( instance ), context, y );
}

static void
iprintable_draw_group_bottom_report( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	draw_account_report( OFA_PDF_BOOKS( instance ), operation, context, FALSE );
}

/*
 * This function is called many time with NULL arguments in order to
 * auto-detect the height of the group footer (in particular each time
 * the #ofa_iprintable_draw_line() function needs to know if there is
 * enough vertical space left to draw the current line) - so take care
 * of not updating the account balance when not drawing...
 */
static void
iprintable_draw_group_footer( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	ofaPDFBooksPrivate *priv;
	gchar *str;
	gdouble y;

	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	priv = OFA_PDF_BOOKS( instance )->priv;

	y = ofa_iprintable_get_last_y( instance );

	/* label */
	str = g_strdup_printf( _( "Balance for account %s - %s" ),
			priv->account_number,
			ofo_account_get_label( priv->account_object ));
	ofa_iprintable_ellipsize_text( instance, context,
			priv->page_margin, y, str, priv->body_acflabel_max_size );
	g_free( str );

	/* solde debit */
	str = my_double_to_str_ex( priv->account_debit, priv->currency_digits );
	ofa_iprintable_set_text( instance, context,
			priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	/* solde credit */
	str = my_double_to_str_ex( priv->account_credit, priv->currency_digits );
	ofa_iprintable_set_text( instance, context,
			priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	/* current account solde */
	draw_account_solde_debit_credit( OFA_PDF_BOOKS( instance ), context, y );

	add_account_balance( OFA_PDF_BOOKS( instance ), context != NULL );

	y += ofa_iprintable_get_current_line_height( instance );
	ofa_iprintable_set_last_y( instance, y );
}

/*
 * add the account balance to the total per currency,
 * adding a new currency record if needed
 */
static void
add_account_balance( ofaPDFBooks *self, gboolean is_drawing )
{
	ofaPDFBooksPrivate *priv;
	GList *it;
	sCurrency *scur;
	gboolean found;

	priv = self->priv;

	for( it=priv->totals, found=FALSE ; it ; it=it->next ){
		scur = ( sCurrency * ) it->data;
		if( !g_utf8_collate( scur->currency, priv->currency_code )){
			found = TRUE;
			break;
		}
	}

	if( !found ){
		scur = g_new0( sCurrency, 1 );
		scur->currency = g_strdup( priv->currency_code );
		priv->totals = g_list_insert_sorted( priv->totals, scur, ( GCompareFunc ) cmp_currencies );
		scur->debit = 0;
		scur->credit = 0;
	}

	scur->debit += is_drawing ? priv->account_debit : 0;
	scur->credit += is_drawing ? priv->account_credit : 0;

	/*g_debug( "add_account_balance: account=%s, debit=%lf, credit=%lf",
			priv->account_number, scur->debit, scur->credit );*/
}

static gint
cmp_currencies( const sCurrency *a, const sCurrency *b )
{
	return( g_utf8_collate( a->currency, b->currency ));
}

/*
 * print a line per found currency at the end of the printing
 */
static void
iprintable_draw_bottom_summary( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	ofaPDFBooksPrivate *priv;
	gdouble bottom, vspace, req_height, line_height, top, y;
	gchar *str;
	GList *it;
	sCurrency *scur;
	gboolean first;

	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	priv = OFA_PDF_BOOKS( instance )->priv;

	if( !priv->count ){
		ofa_iprintable_draw_no_data( instance, context );
		return;
	}

	/* bottom of the rectangle */
	bottom = ofa_iprintable_get_max_y( instance );

	/* top of the rectangle */
	vspace = ofa_iprintable_get_current_line_vspace( instance );
	line_height = ofa_iprintable_get_current_line_height( instance );
	req_height = vspace + g_list_length( priv->totals )*line_height;
	top = bottom - req_height;

	ofa_iprintable_draw_rect( instance, context, 0, top, -1, req_height );

	top += vspace;
	y = top;

	for( it=priv->totals, first=TRUE ; it ; it=it->next ){
		scur = ( sCurrency * ) it->data;

		if( first ){
			ofa_iprintable_set_text( instance, context,
					priv->body_debit_rtab-st_amount_width, y,
					_( "General balance : " ), PANGO_ALIGN_RIGHT );
			first = FALSE;
		}

		str = my_double_to_str( scur->debit );
		ofa_iprintable_set_text( instance, context,
				priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = my_double_to_str( scur->credit );
		ofa_iprintable_set_text( instance, context,
				priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		ofa_iprintable_set_text( instance, context,
				priv->body_solde_sens_rtab, y, scur->currency, PANGO_ALIGN_RIGHT );

		y += line_height;
	}

	ofa_iprintable_set_last_y( instance, ofa_iprintable_get_last_y( instance ) + req_height );
}
