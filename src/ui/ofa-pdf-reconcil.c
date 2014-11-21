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
#include "ui/ofa-pdf-reconcil.h"

/* private instance data
 */
struct _ofaPDFReconcilPrivate {

	gboolean       printed;

	/* internals
	 */
	ofoAccount    *account;
	const gchar   *currency;
	gint           digits;					/* decimal digits for the currency */
	GDate          date;
	gint           count;					/* count of entries in the dataset */

	/* UI
	 */
	GtkWidget     *account_entry;			/* account selection */
	GtkWidget     *account_label;
	GtkWidget     *date_entry;				/* date selection */
	GtkWidget     *date_label;

	/* other datas
	 */
	gdouble        page_margin;
	gdouble        body_count_rtab;
	gdouble        body_effect_ltab;
	gdouble        body_ledger_ltab;
	gdouble        body_ref_ltab;
	gint           body_ref_max_size;		/* Pango units */
	gdouble        body_label_ltab;
	gint           body_label_max_size;		/* Pango units */
	gdouble        body_debit_rtab;
	gdouble        body_credit_rtab;
	gdouble        body_solde_rtab;

	/* runtime
	 */
	gint           line_num;				/* line number of the full report counted from 1 */
	gdouble        account_solde;
};

static const gchar *st_ui_xml              = PKGUIDIR "/ofa-print-reconcil.ui";
static const gchar *st_ui_id               = "PrintReconciliationDlg";

static const gchar *st_pref_fname          = "PDFReconciliationFilename";
static const gchar *st_pref_account        = "PDFReconciliationAccount";
static const gchar *st_pref_date           = "PDFReconciliationDate";

static const gchar *st_def_fname           = "Reconciliation";
static const gchar *st_page_header_title   = N_( "Account Reconciliation Summary" );

/* these are parms which describe the page layout
 */
static const gint   st_default_font_size   = 9;
static const gint   st_default_orientation = GTK_PAGE_ORIENTATION_LANDSCAPE;

/* the columns of the body */
#define st_effect_width                    (gdouble) 54/9*body_font_size
#define st_ledger_width                    (gdouble) 36/9*body_font_size
#define st_ref_width                       (gdouble) 64/9*body_font_size
#define st_amount_width                    (gdouble) 90/9*body_font_size
#define st_column_spacing                  (gdouble) 4
/*
(openbook:29799): OFA-DEBUG: '99/99/9999   ' width=61
(openbook:29799): OFA-DEBUG: 'XXXXXX   ' width=46   -> 107
(openbook:29799): OFA-DEBUG: 'XXXXXXXXXX    ' width=71 ->
(openbook:29799): OFA-DEBUG: 'XXXXXXXXXX' width=62
(openbook:29799): OFA-DEBUG: 'XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX   ' width=441
(openbook:29799): OFA-DEBUG: '   99 999 999,99' width=75
1 space ~ 3px
70 chars = 432 => 1'X' ~ 6.17 px
*/

#define COLOR_BLACK                 0,      0,      0
#define COLOR_DARK_CYAN             0,      0.5156, 0.5156
#define COLOR_GRAY                  0.6,    0.6,    0.6

static void     iprintable_iface_init( ofaIPrintableInterface *iface );
static guint    iprintable_get_interface_version( const ofaIPrintable *instance );
static void     v_init_dialog( myDialog *dialog );
static void     init_account_selection( ofaPDFReconcil *self );
static void     init_date_selection( ofaPDFReconcil *self );
static void     on_account_changed( GtkEntry *entry, ofaPDFReconcil *self );
static void     on_account_select( GtkButton *button, ofaPDFReconcil *self );
static gboolean v_quit_on_ok( myDialog *dialog );
static gboolean do_apply( ofaPDFReconcil *self );
static void     widget_error( ofaPDFReconcil *self, const gchar *msg );
static GList   *iprintable_get_dataset( const ofaIPrintable *instance );
static void     iprintable_reset_runtime( ofaIPrintable *instance );
static void     iprintable_free_dataset( GList *dataset );
static void     iprintable_on_begin_print( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static gchar   *iprintable_get_page_header_title( const ofaIPrintable *instance );
static gchar   *iprintable_get_page_header_subtitle( const ofaIPrintable *instance );
static void     iprintable_draw_page_header_columns( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static void     iprintable_draw_top_summary( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static void     iprintable_draw_line( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, GList *current );
static void     iprintable_draw_bottom_summary( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static gchar   *account_solde_to_str( ofaPDFReconcil *self, gdouble amount );

G_DEFINE_TYPE_EXTENDED( ofaPDFReconcil, ofa_pdf_reconcil, OFA_TYPE_PDF_DIALOG, 0, \
		G_IMPLEMENT_INTERFACE (OFA_TYPE_IPRINTABLE, iprintable_iface_init ));

static void
pdf_reconcil_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_pdf_reconcil_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PDF_RECONCIL( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_pdf_reconcil_parent_class )->finalize( instance );
}

static void
pdf_reconcil_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_PDF_RECONCIL( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_pdf_reconcil_parent_class )->dispose( instance );
}

static void
ofa_pdf_reconcil_init( ofaPDFReconcil *self )
{
	static const gchar *thisfn = "ofa_pdf_reconcil_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PDF_RECONCIL( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_PDF_RECONCIL, ofaPDFReconcilPrivate );

	my_date_clear( &self->priv->date );
}

static void
ofa_pdf_reconcil_class_init( ofaPDFReconcilClass *klass )
{
	static const gchar *thisfn = "ofa_pdf_reconcil_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = pdf_reconcil_dispose;
	G_OBJECT_CLASS( klass )->finalize = pdf_reconcil_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaPDFReconcilPrivate ));
}

static void
iprintable_iface_init( ofaIPrintableInterface *iface )
{
	static const gchar *thisfn = "ofa_print_balance_iprintable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iprintable_get_interface_version;
	iface->get_dataset = iprintable_get_dataset;
	iface->reset_runtime = iprintable_reset_runtime;
	iface->free_dataset = iprintable_free_dataset;
	iface->on_begin_print = iprintable_on_begin_print;
	iface->get_page_header_title = iprintable_get_page_header_title;
	iface->get_page_header_subtitle = iprintable_get_page_header_subtitle;
	iface->draw_page_header_columns = iprintable_draw_page_header_columns;
	iface->draw_top_summary = iprintable_draw_top_summary;
	iface->draw_line = iprintable_draw_line;
	iface->draw_bottom_summary = iprintable_draw_bottom_summary;
}

static guint
iprintable_get_interface_version( const ofaIPrintable *instance )
{
	return( 1 );
}

/**
 * ofa_pdf_reconcil_run:
 * @main: the main window of the application.
 *
 * Print the reconciliation summary
 */
gboolean
ofa_pdf_reconcil_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_pdf_reconcil_run";
	ofaPDFReconcil *self;
	gboolean printed;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new(
			OFA_TYPE_PDF_RECONCIL,
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
	init_account_selection( OFA_PDF_RECONCIL( dialog ));
	init_date_selection( OFA_PDF_RECONCIL( dialog ));
}

static void
init_account_selection( ofaPDFReconcil *self )
{
	ofaPDFReconcilPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *widget;
	gchar *text;

	priv = self->priv;
	toplevel = my_window_get_toplevel( MY_WINDOW( self ));

	/* init account label before set a value in account entry */
	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "account-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->account_label = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "account-entry" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	priv->account_entry = widget;
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_account_changed ), self );
	text = ofa_settings_get_string( st_pref_account );
	if( text && g_utf8_strlen( text, -1 )){
		gtk_entry_set_text( GTK_ENTRY( widget ), text );
	}
	g_free( text );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "account-select" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_account_select ), self );
}

static void
init_date_selection( ofaPDFReconcil *self )
{
	ofaPDFReconcilPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *widget;
	gchar *text;

	priv = self->priv;
	toplevel = my_window_get_toplevel( MY_WINDOW( self ));

	priv->date_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "date-entry" );
	my_editable_date_init( GTK_EDITABLE( priv->date_entry ));
	my_editable_date_set_format( GTK_EDITABLE( priv->date_entry ), MY_DATE_DMYY );
	text = ofa_settings_get_string( st_pref_date );
	my_date_set_from_sql( &priv->date, text );
	if( my_date_is_valid( &priv->date )){
		my_editable_date_set_date( GTK_EDITABLE( priv->date_entry ), &priv->date );
	}
	g_free( text );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "date-label" );
	my_editable_date_set_label( GTK_EDITABLE( priv->date_entry ), widget, MY_DATE_DMMM );
}

static void
on_account_changed( GtkEntry *entry, ofaPDFReconcil *self )
{
	ofaPDFReconcilPrivate *priv;
	const gchar *str;
	ofoCurrency *currency;

	priv = self->priv;
	str = gtk_entry_get_text( entry );
	priv->account = ofo_account_get_by_number( MY_WINDOW( self )->prot->dossier, str );

	if( priv->account ){
		gtk_label_set_text(
				GTK_LABEL( priv->account_label), ofo_account_get_label( priv->account ));
		priv->currency = ofo_account_get_currency( priv->account );
		/* currency code may be empty for root accounts */
		if( priv->currency && g_utf8_strlen( priv->currency, -1 )){
			currency = ofo_currency_get_by_code( MY_WINDOW( self )->prot->dossier, priv->currency );
			if( currency && OFO_IS_CURRENCY( currency )){
				priv->digits = ofo_currency_get_digits( currency );
			}
		}

	} else {
		gtk_label_set_text( GTK_LABEL( priv->account_label ), "" );
	}
}

static void
on_account_select( GtkButton *button, ofaPDFReconcil *self )
{
	ofaPDFReconcilPrivate *priv;
	gchar *number;

	priv = self->priv;
	number = ofa_account_select_run(
						MY_WINDOW( self )->prot->main_window,
						gtk_entry_get_text( GTK_ENTRY( priv->account_entry )));
	if( number ){
		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), number );
		g_free( number );
	}
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	gboolean ok;

	/* chain up to the parent class */
	ok = MY_DIALOG_CLASS( ofa_pdf_reconcil_parent_class )->quit_on_ok( dialog );

	if( ok ){
		ok &= do_apply( OFA_PDF_RECONCIL( dialog ));
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
do_apply( ofaPDFReconcil *self )
{
	ofaPDFReconcilPrivate *priv;
	gchar *sdate;

	priv = self->priv;

	if( !priv->account || !OFO_IS_ACCOUNT( priv->account )){
		widget_error( self, _( "Invalid account" ));
		return( FALSE );
	}
	ofa_settings_set_string( st_pref_account, ofo_account_get_number( priv->account ));

	my_date_set_from_date( &priv->date,
			my_editable_date_get_date( GTK_EDITABLE( priv->date_entry ), NULL ));

	if( !my_date_is_valid( &priv->date )){
		widget_error( self, _( "Invalid reconciliation date" ));
		return( FALSE );
	}

	sdate = my_date_to_str( &priv->date, MY_DATE_SQL );
	ofa_settings_set_string( st_pref_date, sdate );
	g_free( sdate );

	return( TRUE );
}

static void
widget_error( ofaPDFReconcil *self, const gchar *msg )
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(
						GTK_WINDOW( MY_WINDOW( self )->prot->main_window ),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_CLOSE,
						"%s", msg );

	gtk_dialog_run( GTK_DIALOG( dialog ));
	gtk_widget_show( dialog );
}

static GList *
iprintable_get_dataset( const ofaIPrintable *instance )
{
	ofaPDFReconcilPrivate *priv;
	GList *dataset;

	priv = OFA_PDF_RECONCIL( instance )->priv;

	dataset = ofo_entry_get_dataset_for_print_reconcil(
					MY_WINDOW( instance )->prot->dossier,
					ofo_account_get_number( priv->account ),
					&priv->date );

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
	ofaPDFReconcilPrivate *priv;

	priv = OFA_PDF_RECONCIL( instance )->priv;

	priv->line_num = 0;
	priv->account_solde = 0;
}

/*
 * mainly here: compute the tab positions
 */
static void
iprintable_on_begin_print( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	static const gchar *thisfn = "ofa_pdf_reconcil_iprintable_on_begin_print";
	ofaPDFReconcilPrivate *priv;
	gdouble page_width;
	gint body_font_size;
	gchar *str;
	gint digits;

	g_debug( "%s: instance=%p, operation=%p, context=%p",
			thisfn, ( void * ) instance, ( void * ) operation, ( void * ) context );

	priv = OFA_PDF_RECONCIL( instance )->priv;

	priv->page_margin = ofa_iprintable_get_page_margin( instance );
	body_font_size = ofa_iprintable_get_default_font_size( instance );
	page_width = gtk_print_context_get_width( context );

	/* keep the leftest column to display a line number */
	str = g_strdup_printf( "%d", priv->count );
	digits = g_utf8_strlen( str, -1 );
	g_free( str );
	priv->body_count_rtab = priv->page_margin + (1+digits) * 6*7/9;

	/* starting from the left : body_effect_ltab on the left margin */
	priv->body_effect_ltab = priv->body_count_rtab + st_column_spacing;
	priv->body_ledger_ltab = priv->body_effect_ltab+ st_effect_width + st_column_spacing;
	priv->body_ref_ltab = priv->body_ledger_ltab + st_ledger_width + st_column_spacing;
	priv->body_label_ltab = priv->body_ref_ltab + st_ref_width + st_column_spacing;

	/* starting from the right */
	priv->body_solde_rtab = page_width - priv->page_margin;
	priv->body_credit_rtab = priv->body_solde_rtab - st_amount_width - st_column_spacing;
	priv->body_debit_rtab = priv->body_credit_rtab - st_amount_width - st_column_spacing;

	/* max size in Pango units */
	priv->body_ref_max_size = st_ref_width*PANGO_SCALE;
	priv->body_label_max_size = ( priv->body_debit_rtab - st_amount_width - st_column_spacing - priv->body_label_ltab )*PANGO_SCALE;
}

static gchar *
iprintable_get_page_header_title( const ofaIPrintable *instance )
{
	return( g_strdup( st_page_header_title ));
}

/*
 * Account xxx - xxx
 */
static gchar *
iprintable_get_page_header_subtitle( const ofaIPrintable *instance )
{
	ofaPDFReconcilPrivate *priv;
	gchar *str;

	priv = OFA_PDF_RECONCIL( instance )->priv;

	/* account number and label in line 4 */
	str = g_strdup_printf(
				_( "Account %s - %s" ),
				ofo_account_get_number( priv->account ),
				ofo_account_get_label( priv->account ));

	return( str );
}

static void
iprintable_draw_page_header_columns( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	ofaPDFReconcilPrivate *priv;
	gboolean is_drawing;
	gdouble y, vspace;

	priv = OFA_PDF_RECONCIL( instance )->priv;

	is_drawing = ( operation ?
			gtk_print_operation_get_status( operation ) == GTK_PRINT_STATUS_GENERATING_DATA :
			FALSE );

	y = ofa_iprintable_get_last_y( instance );
	vspace = ofa_iprintable_get_current_line_vspace( instance );
	y+= vspace;

	if( is_drawing ){
		ofa_iprintable_set_text( instance, context,
				priv->body_effect_ltab, y, _( "Effect date" ), PANGO_ALIGN_LEFT );

		ofa_iprintable_set_text( instance, context,
				priv->body_ledger_ltab, y, _( "Ledger" ), PANGO_ALIGN_LEFT );

		ofa_iprintable_set_text( instance, context,
				priv->body_ref_ltab, y, _( "Piece" ), PANGO_ALIGN_LEFT );

		ofa_iprintable_set_text( instance, context,
				priv->body_label_ltab, y, _( "Label" ), PANGO_ALIGN_LEFT );

		ofa_iprintable_set_text( instance, context,
				priv->body_debit_rtab, y, _( "Debit" ), PANGO_ALIGN_RIGHT );

		ofa_iprintable_set_text( instance, context,
				priv->body_credit_rtab, y, _( "Credit" ), PANGO_ALIGN_RIGHT );

		ofa_iprintable_set_text( instance, context,
				priv->body_solde_rtab, y, _( "Solde" ), PANGO_ALIGN_RIGHT );
	}

	/* this set the 'y' height just after the column headers */
	y += ofa_iprintable_get_current_line_height( instance );
	ofa_iprintable_set_last_y( instance, y );
}

static void
iprintable_draw_top_summary( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	ofaPDFReconcilPrivate *priv;
	gboolean is_drawing;
	const GDate *date;
	gchar *str, *str_solde, *sdate;
	gdouble y/*, vspace*/;

	priv = OFA_PDF_RECONCIL( instance )->priv;

	is_drawing = ( operation ?
			gtk_print_operation_get_status( operation ) == GTK_PRINT_STATUS_GENERATING_DATA :
			FALSE );

	y = ofa_iprintable_get_last_y( instance );
	/*vspace = ofa_iprintable_get_current_line_vspace( instance );
	y += vspace;*/

	if( is_drawing ){
		date = ofo_account_get_global_deffect( priv->account );
		if( !my_date_is_valid( date )){
			date = ( const GDate * ) &priv->date;
		}
		sdate = my_date_to_str( date, MY_DATE_DMYY );
		priv->account_solde = ofo_account_get_global_solde( priv->account );
		str_solde = account_solde_to_str( OFA_PDF_RECONCIL( instance ), priv->account_solde );
		str = g_strdup_printf( _( "Account solde on %s is %s" ), sdate, str_solde );
		g_free( sdate );
		g_free( str_solde );

		ofa_iprintable_set_text( instance, context,
				priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );

		g_free( str );
	}

	y += ofa_iprintable_get_current_line_height( instance );
	ofa_iprintable_set_last_y( instance, y );
}

/*
 * num_line is counted from 0 in the page
 * line number is last_entry + count + 2
 *
 * (printable)width(A4)=559
 * date  journal  piece    label      debit   credit   solde
 * 10    6        max(10)  max(80)      15d      15d     15d
 */
static void
iprintable_draw_line( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, GList *current )
{
	ofaPDFReconcilPrivate *priv;
	gboolean is_drawing;
	gdouble y;
	gchar *str;
	ofoEntry *entry;
	const gchar *cstr;
	gdouble amount;

	priv = OFA_PDF_RECONCIL( instance )->priv;

	is_drawing = ( operation ?
			gtk_print_operation_get_status( operation ) == GTK_PRINT_STATUS_GENERATING_DATA :
			FALSE );

	y = ofa_iprintable_get_last_y( instance );
	entry = OFO_ENTRY( current->data );

	if( is_drawing ){
		str = my_date_to_str( ofo_entry_get_deffect( entry ), MY_DATE_DMYY );
		ofa_iprintable_set_text( instance, context,
				priv->body_effect_ltab, y, str, PANGO_ALIGN_LEFT );
		g_free( str );

		ofa_iprintable_set_text( instance, context,
				priv->body_ledger_ltab, y, ofo_entry_get_ledger( entry ), PANGO_ALIGN_LEFT );

		cstr = ofo_entry_get_ref( entry );
		if( cstr && g_utf8_strlen( cstr, -1 )){
			ofa_iprintable_ellipsize_text( instance, context,
					priv->body_ref_ltab, y, cstr, priv->body_ref_max_size );
		}

		ofa_iprintable_ellipsize_text( instance, context,
				priv->body_label_ltab, y, ofo_entry_get_label( entry ), priv->body_label_max_size );

		amount = ofo_entry_get_debit( entry );
		if( amount ){
			str = my_double_to_str( amount );
			ofa_iprintable_set_text( instance, context,
					priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
			g_free( str );
			priv->account_solde -= amount;
		}

		amount = ofo_entry_get_credit( entry );
		if( amount ){
			str = my_double_to_str( amount );
			ofa_iprintable_set_text( instance, context,
					priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
			g_free( str );
			priv->account_solde += amount;
		}

		/* current solde */
		ofa_iprintable_set_color( instance, context, COLOR_DARK_CYAN );
		str = my_double_to_str( priv->account_solde );
		ofa_iprintable_set_text( instance, context,
				priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		/* display the line number starting from 1 */
		ofa_iprintable_set_color( instance, context, COLOR_GRAY );
		ofa_iprintable_set_font( instance, "", 7 );

		str = g_strdup_printf( "%d", ++priv->line_num );
		ofa_iprintable_set_text( instance, context, priv->body_count_rtab, y+1, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		/*g_debug( "ofa_pdf_reconcil_draw_line: quitting line_num=%d", priv->line_num );*/
	}

	/* restore the default font size so that the line height computing is rigth */
	ofa_iprintable_set_font( instance, "", ofa_iprintable_get_default_font_size( instance ));
}

static void
iprintable_draw_bottom_summary( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	ofaPDFReconcilPrivate *priv;
	gboolean is_drawing;
	gdouble y;
	const GDate *date;
	gchar *str, *sdate, *str_amount;

	priv = OFA_PDF_RECONCIL( instance )->priv;

	is_drawing = ( operation ?
			gtk_print_operation_get_status( operation ) == GTK_PRINT_STATUS_GENERATING_DATA :
			FALSE );

	y = ofa_iprintable_get_last_y( instance );

	date = ofo_account_get_global_deffect( priv->account );
	if( !my_date_is_valid( date ) || my_date_compare( date, &priv->date ) < 0 ){
		date = ( const GDate * ) &priv->date;
	}
	sdate = my_date_to_str( date, MY_DATE_DMYY );

	str_amount = account_solde_to_str( OFA_PDF_RECONCIL( instance ), priv->account_solde );
	str = g_strdup_printf( _( "Reconciliated account solde on %s is %s" ), sdate, str_amount );
	g_free( sdate );
	g_free( str_amount );

	if( is_drawing ){
		ofa_iprintable_set_text( instance, context,
				priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
	}

	g_free( str );

	y += ofa_iprintable_get_current_line_height( instance );

	if( is_drawing ){
		ofa_iprintable_set_color( instance, context, COLOR_BLACK );
		ofa_iprintable_set_font( instance, "", ofa_iprintable_get_default_font_size( instance ));

		ofa_iprintable_set_wrapped_text( instance, context,
					priv->page_margin, y,
					(gtk_print_context_get_width( context )-priv->page_margin)*PANGO_SCALE,
					_( "This reconciliated solde "
						"should be the same, though inversed, "
						"that the one of the account extraction sent by your bank.\n"
						"If this is not the case, then you have most probably "
						"forgotten to reconciliate "
						"some of the above entries, or some other entries have been recorded "
						"by your bank, are present in your account extraction, but are not "
						"found in your books." ), PANGO_ALIGN_LEFT );
	}

	y += 3*ofa_iprintable_get_current_line_height( instance );
	ofa_iprintable_set_last_y( instance, y );
}

static gchar *
account_solde_to_str( ofaPDFReconcil *self, gdouble amount )
{
	ofaPDFReconcilPrivate *priv;
	gchar *str, *str_amount;

	priv = self->priv;

	str_amount = my_double_to_str_ex( amount, priv->digits );
	str = g_strdup_printf( "%s %s", str_amount, priv->currency );
	g_free( str_amount );

	return( str );
}
