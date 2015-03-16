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
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofs-currency.h"

#include "core/my-window-prot.h"

#include "ui/my-editable-date.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-iprintable.h"
#include "ui/ofa-ledger-treeview.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-pdf-ledgers.h"

/* private instance data
 */
struct _ofaPDFLedgersPrivate {

	gboolean           printed;

	/* UI
	 */
	GtkWidget         *alignment;
	ofaLedgerTreeview *ledgers_tview;		/* ledgers selection */
	GtkWidget         *all_ledgers_btn;

	GtkWidget         *from_date_entry;		/* date selection */
	GtkWidget         *to_date_entry;

	GtkWidget         *new_page_btn;

	/* internals
	 */
	gboolean           all_ledgers;
	GDate              from_date;
	GDate              to_date;
	gboolean           new_page;
	GList             *selected;			/* list of selected #ofoLedger */
	gint               count;				/* count of returned entries */

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
	gdouble            body_piece_ltab;
	gdouble            body_piece_max_size;
	gdouble            body_label_ltab;
	gint               body_label_max_size;		/* Pango units */
	gdouble            body_template_ltab;
	gdouble            body_settlement_ctab;
	gdouble            body_reconcil_ctab;
	gdouble            body_debit_rtab;
	gdouble            body_credit_rtab;
	gdouble            body_currency_rtab;

	/* for each ledger
	 */
	gchar             *ledger_mnemo;
	ofoLedger         *ledger_object;
	GList             *ledger_totals;	/* totals per currency for the ledger */

	/* total general
	 */
	GList             *report_totals;		/* all totals per currency */
};

static const gchar *st_ui_xml              = PKGUIDIR "/ofa-print-ledgers.ui";
static const gchar *st_ui_id               = "PrintLedgersDlg";

static const gchar *st_pref_uri            = "PDFLedgersURI";
static const gchar *st_pref_settings       = "PDFLedgersSettings";

static const gchar *st_def_fname           = "Ledgers.pdf";
static const gchar *st_page_header_title   = N_( "Ledgers Summary" );

/* these are parms which describe the page layout
 */
static const gint   st_default_font_size   = 9;
static const gint   st_default_orientation = GTK_PAGE_ORIENTATION_LANDSCAPE;

/* the space between columns headers */
#define st_page_header_columns_vspace      (gdouble) 2

/* the columns of the entry line
 * dope deff account piece label template set. rec. debit credit currency */
#define st_date_width                      (gdouble) 54/9*st_default_font_size
#define st_account_width                   (gdouble) 48/9*st_default_font_size
#define st_piece_width                     (gdouble) 64/9*st_default_font_size
#define st_template_width                  (gdouble) 44/9*st_default_font_size
#define st_settlement_width                (gdouble) 8/9*st_default_font_size
#define st_reconcil_width                  (gdouble) 8/9*st_default_font_size
#define st_amount_width                    (gdouble) 90/9*st_default_font_size
#define st_currency_width                  (gdouble) 23/10*st_default_font_size
#define st_column_hspacing                 (gdouble) 4

static void     iprintable_iface_init( ofaIPrintableInterface *iface );
static guint    iprintable_get_interface_version( const ofaIPrintable *instance );
static void     v_init_dialog( myDialog *dialog );
static void     init_ledgers_selection( ofaPDFLedgers *self );
static void     init_date_selection( ofaPDFLedgers *self );
static void     init_others( ofaPDFLedgers *self );
static void     on_ledgers_activated( ofaLedgerTreeview *view, GList *selected, ofaPDFLedgers *self );
static void     on_all_ledgers_toggled( GtkToggleButton *button, ofaPDFLedgers *self );
static void     on_new_page_toggled( GtkToggleButton *button, ofaPDFLedgers *self );
static gboolean v_quit_on_ok( myDialog *dialog );
static gboolean do_apply( ofaPDFLedgers *self );
static GList   *iprintable_get_dataset( const ofaIPrintable *instance );
static void     iprintable_free_dataset( GList *elements );
static void     iprintable_reset_runtime( ofaIPrintable *instance );
static void     iprintable_on_begin_print( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static gchar   *iprintable_get_page_header_title( const ofaIPrintable *instance );
static gchar   *iprintable_get_page_header_subtitle( const ofaIPrintable *instance );
static void     iprintable_draw_page_header_columns( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num );
static gboolean iprintable_is_new_group( const ofaIPrintable *instance, GList *current, GList *prev );
static void     iprintable_draw_group_header( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, GList *current );
static void     iprintable_draw_group_top_report( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static void     iprintable_draw_line( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, GList *current );
static void     iprintable_draw_group_bottom_report( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static void     iprintable_draw_group_footer( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static void     iprintable_draw_bottom_summary( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static void     draw_ledger_totals( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static void     get_settings( ofaPDFLedgers *self );
static void     set_settings( ofaPDFLedgers *self );

G_DEFINE_TYPE_EXTENDED( ofaPDFLedgers, ofa_pdf_ledgers, OFA_TYPE_PDF_DIALOG, 0, \
		G_IMPLEMENT_INTERFACE (OFA_TYPE_IPRINTABLE, iprintable_iface_init ));

static void
free_currency( ofsCurrency *total_per_currency )
{
	g_free( total_per_currency->currency );
	g_free( total_per_currency );
}

static void
pdf_ledgers_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_pdf_ledgers_finalize";
	ofaPDFLedgersPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PDF_LEDGERS( instance ));

	/* free data members here */
	priv = OFA_PDF_LEDGERS( instance )->priv;

	ofs_currency_list_free( &priv->ledger_totals );
	ofs_currency_list_free( &priv->report_totals );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_pdf_ledgers_parent_class )->finalize( instance );
}

static void
pdf_ledgers_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_PDF_LEDGERS( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_pdf_ledgers_parent_class )->dispose( instance );
}

static void
ofa_pdf_ledgers_init( ofaPDFLedgers *self )
{
	static const gchar *thisfn = "ofa_pdf_ledgers_instance_init";
	ofaPDFLedgersPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PDF_LEDGERS( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_PDF_LEDGERS, ofaPDFLedgersPrivate );
	priv = self->priv;

	priv->printed = FALSE;
	my_date_clear( &priv->from_date );
	my_date_clear( &priv->to_date );
}

static void
ofa_pdf_ledgers_class_init( ofaPDFLedgersClass *klass )
{
	static const gchar *thisfn = "ofa_pdf_ledgers_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = pdf_ledgers_dispose;
	G_OBJECT_CLASS( klass )->finalize = pdf_ledgers_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaPDFLedgersPrivate ));
}

static void
iprintable_iface_init( ofaIPrintableInterface *iface )
{
	static const gchar *thisfn = "ofa_pdf_ledgers_iprintable_iface_init";

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
 * ofa_pdf_ledgers_run:
 * @main: the main window of the application.
 *
 * Print the accounts balance
 */
gboolean
ofa_pdf_ledgers_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_pdf_ledgers_run";
	ofaPDFLedgers *self;
	gboolean printed;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new(
				OFA_TYPE_PDF_LEDGERS,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				PDF_PROP_DEF_NAME,   st_def_fname,
				PDF_PROP_PREF_NAME,  st_pref_uri,
				NULL );

	my_dialog_run_dialog( MY_DIALOG( self ));

	printed = self->priv->printed;
	g_object_unref( self );

	return( printed );
}

static void
v_init_dialog( myDialog *dialog )
{
	get_settings( OFA_PDF_LEDGERS( dialog ));
	init_ledgers_selection( OFA_PDF_LEDGERS( dialog ));
	init_date_selection( OFA_PDF_LEDGERS( dialog ));
	init_others( OFA_PDF_LEDGERS( dialog ));
}

static void
init_ledgers_selection( ofaPDFLedgers *self )
{
	ofaPDFLedgersPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *widget;

	priv = self->priv;
	toplevel = my_window_get_toplevel( MY_WINDOW( self ));

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "p1-alignment" );
	g_return_if_fail( widget && GTK_IS_ALIGNMENT( widget ));
	priv->alignment = widget;

	priv->ledgers_tview = ofa_ledger_treeview_new();
	gtk_container_add( GTK_CONTAINER( widget ), GTK_WIDGET( priv->ledgers_tview ));
	ofa_ledger_treeview_set_columns( priv->ledgers_tview,
			LEDGER_DISP_MNEMO | LEDGER_DISP_LABEL | LEDGER_DISP_LAST_ENTRY | LEDGER_DISP_LAST_CLOSE );
	ofa_ledger_treeview_set_main_window( priv->ledgers_tview, MY_WINDOW( self )->prot->main_window );
	ofa_ledger_treeview_set_selection_mode( priv->ledgers_tview, GTK_SELECTION_MULTIPLE );

	g_signal_connect(
			G_OBJECT( priv->ledgers_tview ), "ofa-activated", G_CALLBACK( on_ledgers_activated ), self );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "p1-all-ledgers" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_all_ledgers_toggled ), self );
	priv->all_ledgers_btn = widget;
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), priv->all_ledgers );
	on_all_ledgers_toggled( GTK_TOGGLE_BUTTON( widget ), self );
}

static void
init_date_selection( ofaPDFLedgers *self )
{
	ofaPDFLedgersPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *widget;

	priv = self->priv;
	toplevel = my_window_get_toplevel( MY_WINDOW( self ));

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "from-date-entry" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	my_editable_date_init( GTK_EDITABLE( widget ));
	my_editable_date_set_format( GTK_EDITABLE( widget ), ofa_prefs_date_display());
	my_editable_date_set_mandatory( GTK_EDITABLE( widget ), FALSE );
	priv->from_date_entry = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "from-date-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	my_editable_date_set_label( GTK_EDITABLE( priv->from_date_entry ), widget, ofa_prefs_date_check());
	if( my_date_is_valid( &priv->from_date )){
		my_editable_date_set_date( GTK_EDITABLE( priv->from_date_entry ), &priv->from_date );
	}

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "to-date-entry" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	my_editable_date_init( GTK_EDITABLE( widget ));
	my_editable_date_set_format( GTK_EDITABLE( widget ), ofa_prefs_date_display());
	my_editable_date_set_mandatory( GTK_EDITABLE( widget ), FALSE );
	priv->to_date_entry = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "to-date-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	my_editable_date_set_label( GTK_EDITABLE( priv->to_date_entry ), widget, ofa_prefs_date_check());
	if( my_date_is_valid( &priv->to_date )){
		my_editable_date_set_date( GTK_EDITABLE( priv->to_date_entry ), &priv->to_date );
	}
}

static void
init_others( ofaPDFLedgers *self )
{
	ofaPDFLedgersPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *widget;

	priv = self->priv;
	toplevel = my_window_get_toplevel( MY_WINDOW( self ));

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "p3-new-page" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_new_page_toggled ), self );
	priv->new_page_btn = widget;
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), priv->new_page );
	on_new_page_toggled( GTK_TOGGLE_BUTTON( widget ), self );
}

static void
on_ledgers_activated( ofaLedgerTreeview *view, GList *selected, ofaPDFLedgers *self )
{
	gtk_dialog_response( GTK_DIALOG( self ), GTK_RESPONSE_OK );
}

static void
on_all_ledgers_toggled( GtkToggleButton *button, ofaPDFLedgers *self )
{
	ofaPDFLedgersPrivate *priv;
	gboolean bvalue;

	priv = self->priv;
	bvalue = gtk_toggle_button_get_active( button );

	gtk_widget_set_sensitive( priv->alignment, !bvalue );

	priv->all_ledgers = bvalue;
}

static void
on_new_page_toggled( GtkToggleButton *button, ofaPDFLedgers *self )
{
	ofaPDFLedgersPrivate *priv;

	priv = self->priv;

	priv->new_page = gtk_toggle_button_get_active( button );
}

/*
 * #GtkPrintOperation only export to PDF addressed by filename (not URI)
 * so first convert
 */
static gboolean
v_quit_on_ok( myDialog *dialog )
{
	gboolean ok;
	gchar *fname;

	/* chain up to the parent class */
	ok = MY_DIALOG_CLASS( ofa_pdf_ledgers_parent_class )->quit_on_ok( dialog );

	if( ok ){
		ok &= do_apply( OFA_PDF_LEDGERS( dialog ));
	}

	if( ok ){
		ofa_iprintable_set_paper_orientation( OFA_IPRINTABLE( dialog ), st_default_orientation );
		ofa_iprintable_set_default_font_size( OFA_IPRINTABLE( dialog ), st_default_font_size );

		fname = ofa_pdf_dialog_get_filename( OFA_PDF_DIALOG( dialog ));
		ok &= ofa_iprintable_print_to_pdf( OFA_IPRINTABLE( dialog ), fname );
		g_free( fname );
	}

	return( ok );
}

/*
 * save parameters (all fields are optional, but at least one ledger
 * should be selected)
 */
static gboolean
do_apply( ofaPDFLedgers *self )
{
	ofaPDFLedgersPrivate *priv;
	GList *list, *it;
	ofoLedger *ledger;

	priv = self->priv;

	if( priv->all_ledgers ){
		priv->selected = ofo_ledger_get_dataset( MY_WINDOW( self )->prot->dossier );
	} else {
		list = ofa_ledger_treeview_get_selected( priv->ledgers_tview );
		priv->selected = NULL;
		for( it=list ; it ; it=it->next ){
			ledger = ofo_ledger_get_by_mnemo( MY_WINDOW( self )->prot->dossier, ( const gchar * ) it->data );
			g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
			priv->selected = g_list_append( priv->selected, ledger );
		}
		ofa_ledger_treeview_free_selected( list );
	}

	if( !g_list_length( priv->selected )){
		my_utils_dialog_error( _( "Empty ledgers selection: unable to continue" ));
		return( FALSE );
	}

	my_date_set_from_date( &priv->from_date,
			my_editable_date_get_date( GTK_EDITABLE( priv->from_date_entry ), NULL ));

	my_date_set_from_date( &priv->to_date,
			my_editable_date_get_date( GTK_EDITABLE( priv->to_date_entry ), NULL ));

	set_settings( self );

	ofa_iprintable_set_group_on_new_page( OFA_IPRINTABLE( self ), priv->new_page );

	return( TRUE );
}

static GList *
iprintable_get_dataset( const ofaIPrintable *instance )
{
	ofaPDFLedgersPrivate *priv;
	GSList *mnemos;
	GList *it;
	GList *dataset;

	priv = OFA_PDF_LEDGERS( instance )->priv;

	/* build a list of requested ledger mnemos */
	for( mnemos=NULL, it=priv->selected ; it ; it=it->next ){
		mnemos = g_slist_prepend( mnemos, g_strdup( ofo_ledger_get_mnemo( OFO_LEDGER( it->data ))));
	}

	dataset = ofo_entry_get_dataset_for_print_ledgers(
							MY_WINDOW( instance )->prot->dossier,
							mnemos, &priv->from_date, &priv->to_date );

	priv->count = g_list_length( dataset );
	g_debug( "ofa_pdf_ledgers_iprintable_get_dataset: count=%d", priv->count );

	g_slist_free_full( mnemos, ( GDestroyNotify ) g_free );

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
	ofaPDFLedgersPrivate *priv;

	priv = OFA_PDF_LEDGERS( instance )->priv;

	ofs_currency_list_free( &priv->report_totals );

	g_free( priv->ledger_mnemo );
	priv->ledger_mnemo = NULL;
}

/*
 * mainly here: compute the tab positions
 */
static void
iprintable_on_begin_print( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	ofaPDFLedgersPrivate *priv;
	gdouble page_width, page_margin;

	priv = OFA_PDF_LEDGERS( instance )->priv;

	page_width = gtk_print_context_get_width( context );
	page_margin = ofa_iprintable_get_page_margin( instance );

	/* entry line, starting from the left */
	priv->body_dope_ltab = page_margin;
	priv->body_deffect_ltab = priv->body_dope_ltab + st_date_width + st_column_hspacing;
	priv->body_account_ltab = priv->body_deffect_ltab + st_date_width + st_column_hspacing;
	priv->body_piece_ltab = priv->body_account_ltab + st_account_width + st_column_hspacing;
	priv->body_label_ltab = priv->body_piece_ltab + st_piece_width + st_column_hspacing;

	/* entry line, starting from the right */
	priv->body_currency_rtab = page_width - page_margin;
	priv->body_credit_rtab = priv->body_currency_rtab - st_currency_width - st_column_hspacing;
	priv->body_debit_rtab = priv->body_credit_rtab - st_amount_width - st_column_hspacing;
	priv->body_reconcil_ctab = priv->body_debit_rtab - st_amount_width - st_column_hspacing - st_reconcil_width/2;
	priv->body_settlement_ctab = priv->body_reconcil_ctab - st_reconcil_width/2 - st_column_hspacing - st_settlement_width/2;
	priv->body_template_ltab = priv->body_settlement_ctab + st_settlement_width/2 - st_column_hspacing - st_template_width;

	/* account header, starting from the left
	 * computed here because aligned on (and so relying on) body effect date */
	priv->group_h_ledcode_ltab = page_margin;
	priv->group_h_ledlabel_ltab = priv->body_deffect_ltab;

	/* max size in Pango units */
	priv->group_h_ledlabel_max_size = ( page_width - page_margin - priv->group_h_ledlabel_ltab )*PANGO_SCALE;
	priv->body_piece_max_size = st_piece_width*PANGO_SCALE;
	priv->body_label_max_size = ( priv->body_template_ltab - st_column_hspacing - priv->body_label_ltab )*PANGO_SCALE;
}

static gchar *
iprintable_get_page_header_title( const ofaIPrintable *instance )
{
	return( g_strdup( st_page_header_title ));
}

/*
 * Ledgers xxx, xxx, xxx - Date from xxx to xxx
 */
static gchar *
iprintable_get_page_header_subtitle( const ofaIPrintable *instance )
{
	ofaPDFLedgersPrivate *priv;
	gchar *sfrom_date, *sto_date;
	GString *stitle;
	GList *it;
	gboolean first;

	priv = OFA_PDF_LEDGERS( instance )->priv;

	/* recall of ledgers and date selections in line 4 */
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
iprintable_draw_page_header_columns( ofaIPrintable *instance,
		GtkPrintOperation *operation, GtkPrintContext *context, gint page_num )
{
	ofaPDFLedgersPrivate *priv;
	gdouble y, vspace;

	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	priv = OFA_PDF_LEDGERS( instance )->priv;

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
			priv->body_account_ltab, y,
			_( "Account" ), PANGO_ALIGN_LEFT );

	ofa_iprintable_set_text( instance, context,
			priv->body_piece_ltab, y,
			_( "Piece" ), PANGO_ALIGN_LEFT );

	ofa_iprintable_set_text( instance, context,
			priv->body_label_ltab, y,
			_( "Label" ), PANGO_ALIGN_LEFT );

	ofa_iprintable_set_text( instance, context,
			priv->body_template_ltab, y,
			_( "Tmpl." ), PANGO_ALIGN_LEFT );

	ofa_iprintable_set_text( instance, context,
			(priv->body_settlement_ctab+priv->body_reconcil_ctab)/2, y,
			_( "Set./Rec." ), PANGO_ALIGN_CENTER );

	ofa_iprintable_set_text( instance, context,
			priv->body_debit_rtab, y,
			_( "Debit" ), PANGO_ALIGN_RIGHT );

	ofa_iprintable_set_text( instance, context,
			priv->body_credit_rtab, y,
			_( "Credit" ), PANGO_ALIGN_RIGHT );

	/* no header for currency */

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
				ofo_entry_get_ledger( current_entry ), ofo_entry_get_ledger( prev_entry )) != 0 );
}

/*
 * draw account header
 */
static void
iprintable_draw_group_header( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, GList *current )
{
	ofaPDFLedgersPrivate *priv;
	gdouble y;

	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	priv = OFA_PDF_LEDGERS( instance )->priv;

	y = ofa_iprintable_get_last_y( instance );

	/* setup the account properties */
	g_free( priv->ledger_mnemo );
	priv->ledger_mnemo = g_strdup( ofo_entry_get_ledger( OFO_ENTRY( current->data )));

	priv->ledger_object = ofo_ledger_get_by_mnemo(
							MY_WINDOW( instance )->prot->dossier,
							priv->ledger_mnemo );
	g_return_if_fail( priv->ledger_object && OFO_IS_LEDGER( priv->ledger_object ));

	g_list_free_full( priv->ledger_totals, ( GDestroyNotify ) free_currency );
	priv->ledger_totals = NULL;

	/* display the ledger header */
	/* ledger mnemo */
	ofa_iprintable_set_text( instance, context,
			priv->group_h_ledcode_ltab, y,
			priv->ledger_mnemo, PANGO_ALIGN_LEFT );

	/* ledger label */
	ofa_iprintable_ellipsize_text( instance, context,
			priv->group_h_ledlabel_ltab, y,
			ofo_ledger_get_label( priv->ledger_object ), priv->group_h_ledlabel_max_size );

	y += ofa_iprintable_get_current_line_height( instance );
	ofa_iprintable_set_last_y( instance, y );
}

static void
iprintable_draw_group_top_report( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	draw_ledger_totals( instance, operation, context );
}

static void
iprintable_draw_line( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, GList *current )
{
	ofaPDFLedgersPrivate *priv;
	ofoEntry *entry;
	const gchar *cstr, *code;
	gchar *str;
	gdouble y;
	ofxAmount debit, credit;
	ofoCurrency *currency;
	gint digits;

	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	priv = OFA_PDF_LEDGERS( instance )->priv;

	y = ofa_iprintable_get_last_y( instance );
	entry = OFO_ENTRY( current->data );

	/* get currency properties */
	code = ofo_entry_get_currency( entry );
	currency = ofo_currency_get_by_code( MY_WINDOW( instance )->prot->dossier, code );
	g_return_if_fail( currency && OFO_IS_CURRENCY( currency ));
	digits = ofo_currency_get_digits( currency );

	/* operation date */
	str = my_date_to_str( ofo_entry_get_dope( entry ), ofa_prefs_date_display());
	ofa_iprintable_set_text( instance, context,
			priv->body_dope_ltab, y, str, PANGO_ALIGN_LEFT );
	g_free( str );

	/* effect date */
	str = my_date_to_str( ofo_entry_get_deffect( entry ), ofa_prefs_date_display());
	ofa_iprintable_set_text( instance, context,
			priv->body_deffect_ltab, y, str, PANGO_ALIGN_LEFT );
	g_free( str );

	/* account */
	ofa_iprintable_set_text( instance, context,
			priv->body_account_ltab, y, ofo_entry_get_account( entry ), PANGO_ALIGN_LEFT );

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

	/* template */
	cstr = ofo_entry_get_ope_template( entry );
	if( cstr && g_utf8_strlen( cstr, -1 )){
		ofa_iprintable_set_text( instance, context,
				priv->body_template_ltab, y, cstr, PANGO_ALIGN_LEFT );
	}

	/* settlement ? */
	if( ofo_entry_get_settlement_number( entry ) > 0 ){
		ofa_iprintable_set_text( instance, context,
				priv->body_settlement_ctab, y, _( "S" ), PANGO_ALIGN_CENTER );
	}

	/* reconciliation ? */
	if( my_date_is_valid( ofo_entry_get_concil_dval( entry ))){
		ofa_iprintable_set_text( instance, context,
				priv->body_reconcil_ctab, y, _( "R" ), PANGO_ALIGN_CENTER );
	}

	/* debit */
	debit = ofo_entry_get_debit( entry );
	if( debit ){
		str = my_double_to_str_ex( debit, digits );
		ofa_iprintable_set_text( instance, context,
				priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
	}

	/* credit */
	credit = ofo_entry_get_credit( entry );
	if( credit ){
		str = my_double_to_str_ex( credit, digits );
		ofa_iprintable_set_text( instance, context,
				priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
	}

	/* currency */
	ofa_iprintable_set_text( instance, context,
			priv->body_currency_rtab, y, code, PANGO_ALIGN_RIGHT );

	ofs_currency_add_currency(
			&priv->ledger_totals, code, context ? debit : 0, context ? credit : 0 );
}

static void
iprintable_draw_group_bottom_report( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	draw_ledger_totals( instance, operation, context );
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
	ofaPDFLedgersPrivate *priv;
	GList *it;
	ofsCurrency *cur;

	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	draw_ledger_totals( instance, operation, context );

	priv = OFA_PDF_LEDGERS( instance )->priv;

	for( it=priv->ledger_totals ; it ; it=it->next ){
		cur = ( ofsCurrency * ) it->data;
		ofs_currency_add_currency(
				&priv->report_totals, cur->currency, context ? cur->debit : 0, context ? cur->credit : 0 );
	}
}

/*
 * print a line per found currency at the end of the printing
 */
static void
iprintable_draw_bottom_summary( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	ofaPDFLedgersPrivate *priv;
	gdouble bottom, vspace, req_height, line_height, top;
	gchar *str;
	GList *it;
	ofsCurrency *scur;
	gboolean first;
	ofoCurrency *currency;
	gint digits, shift;

	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	priv = OFA_PDF_LEDGERS( instance )->priv;

	if( !priv->count ){
		ofa_iprintable_draw_no_data( instance, context );
		return;
	}

	/* bottom of the rectangle */
	bottom = ofa_iprintable_get_max_y( instance );

	/* top of the rectangle */
	vspace = ofa_iprintable_get_current_line_vspace( instance );
	line_height = ofa_iprintable_get_current_line_height( instance );
	req_height = vspace + g_list_length( priv->report_totals )*line_height;
	top = bottom - req_height;

	ofa_iprintable_draw_rect( instance, context, 0, top, -1, req_height );
	top += vspace;
	shift = 4;

	for( it=priv->report_totals, first=TRUE ; it ; it=it->next ){
		scur = ( ofsCurrency * ) it->data;
		currency = ofo_currency_get_by_code( MY_WINDOW( instance )->prot->dossier, scur->currency );
		g_return_if_fail( currency && OFO_IS_CURRENCY( currency ));
		digits = ofo_currency_get_digits( currency );

		if( first ){
			ofa_iprintable_set_text( instance, context,
					priv->body_debit_rtab-st_amount_width-shift, top,
					_( "Ledgers general balance : " ), PANGO_ALIGN_RIGHT );
			first = FALSE;
		}

		str = my_double_to_str_ex( scur->debit, digits );
		ofa_iprintable_set_text( instance, context,
				priv->body_debit_rtab-shift, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = my_double_to_str_ex( scur->credit, digits );
		ofa_iprintable_set_text( instance, context,
				priv->body_credit_rtab-shift, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		ofa_iprintable_set_text( instance, context,
				priv->body_currency_rtab, top, scur->currency, PANGO_ALIGN_RIGHT );

		top += line_height;
	}

	ofa_iprintable_set_last_y( instance, ofa_iprintable_get_last_y( instance ) + req_height );
}

/*
 * draw the total for this ledger by currencies
 * update the last_y accordingly
 */
static void
draw_ledger_totals( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	ofaPDFLedgersPrivate *priv;
	gdouble y;
	gboolean first;
	gchar *str;
	GList *it;
	ofsCurrency *scur;
	ofoCurrency *currency;
	gint digits;

	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	priv = OFA_PDF_LEDGERS( instance )->priv;

	y = ofa_iprintable_get_last_y( instance );

	for( it=priv->ledger_totals, first=TRUE ; it ; it=it->next ){
		scur = ( ofsCurrency * ) it->data;
		currency = ofo_currency_get_by_code( MY_WINDOW( instance )->prot->dossier, scur->currency );
		g_return_if_fail( currency && OFO_IS_CURRENCY( currency ));
		digits = ofo_currency_get_digits( currency );

		if( first ){
			str = g_strdup_printf( _( "%s ledger balance : " ), priv->ledger_mnemo );
			ofa_iprintable_set_text( instance, context,
					priv->body_debit_rtab-st_amount_width, y,
					str, PANGO_ALIGN_RIGHT );
			g_free( str );
			first = FALSE;
		}

		str = my_double_to_str_ex( scur->debit, digits );
		ofa_iprintable_set_text( instance, context,
				priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = my_double_to_str_ex( scur->credit, digits );
		ofa_iprintable_set_text( instance, context,
				priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		ofa_iprintable_set_text( instance, context,
				priv->body_currency_rtab, y, scur->currency, PANGO_ALIGN_RIGHT );

		y += ofa_iprintable_get_current_line_height( instance );
	}

	ofa_iprintable_set_last_y( instance, y );
}

/*
 * settings are:
 * all_ledgers;from_date;to_date;new_page;
 */
static void
get_settings( ofaPDFLedgers *self )
{
	ofaPDFLedgersPrivate *priv;
	GList *slist, *it;
	const gchar *cstr;

	priv = self->priv;

	slist = ofa_settings_get_string_list( st_pref_settings );

	it = slist;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->all_ledgers = my_utils_boolean_from_str( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		my_date_set_from_str( &priv->from_date, cstr, MY_DATE_SQL );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		my_date_set_from_str( &priv->to_date, cstr, MY_DATE_SQL );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->new_page = my_utils_boolean_from_str( cstr );
	}

	ofa_settings_free_string_list( slist );
}

static void
set_settings( ofaPDFLedgers *self )
{
	ofaPDFLedgersPrivate *priv;
	gchar *str, *sfrom, *sto;

	priv = self->priv;

	sfrom = my_date_to_str( &priv->from_date, MY_DATE_SQL );
	sto = my_date_to_str( &priv->to_date, MY_DATE_SQL );

	str = g_strdup_printf( "%s;%s;%s;%s;",
			priv->all_ledgers ? "True":"False",
			sfrom, sto,
			priv->new_page ? "True":"False" );

	ofa_settings_set_string( st_pref_settings, str );

	g_free( str );
	g_free( sfrom );
	g_free( sto );
}
