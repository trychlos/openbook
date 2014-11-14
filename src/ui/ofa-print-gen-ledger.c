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

#include "ui/my-editable-date.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-print.h"
#include "ui/ofa-print-gen-ledger.h"

/* private instance data
 */
struct _ofaPrintGenLedgerPrivate {
	gboolean       dispose_has_run;

	/* initialization data
	 */
	ofaMainWindow *main_window;

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
	GList         *entries;

	/* print datas
	 */
	gdouble        page_width;
	gdouble        page_height;
	gdouble        max_y;
	gint           pages_count;
	PangoLayout   *layout;
	gdouble        last_y;
	GList         *last_printed;
	gboolean       general_summary_printed;

	/* layout for account header line
	 */
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
	gdouble        body_settlement_ltab;
	gdouble        body_debit_rtab;
	gdouble        body_credit_rtab;
	gdouble        body_solde_rtab;
	gdouble        body_solde_sens_rtab;

	/* when the account changes
	 */
	gchar         *prev_account;
	ofxAmount      prev_debit;
	ofxAmount      prev_credit;
	ofoAccount    *prev_accobj;
	gboolean       prev_header_printed;
	gboolean       prev_footer_printed;
	ofoCurrency   *prev_currency;
	gint           prev_digits;

	/* total general
	 */
	GList         *total;				/* list of totals per currency */

};

typedef struct {
	gchar    *currency;
	ofxAmount debit;
	ofxAmount credit;
}
	sCurrency;

static const gchar  *st_ui_xml              = PKGUIDIR "/ofa-print-gen-ledger.piece.ui";

static const gchar  *st_pref_from_account   = "PrintGenLedgerFromAccount";
static const gchar  *st_pref_to_account     = "PrintGenLedgerToAccount";
static const gchar  *st_pref_all_accounts   = "PrintGenLedgerAllAccounts";
static const gchar  *st_pref_from_date      = "PrintGenLedgerFromDate";
static const gchar  *st_pref_to_date        = "PrintGenLedgerToDate";
static const gchar  *st_pref_new_page       = "PrintGenLedgerNewPage";

/* These are parms which describe the page layout
 *
 * Page setup: A4 portrait
 *   Unit=none
 *   context_width =559,275591 pixels -> 559.2 pix.
 *   context_height=783,569764 pixels -> 783.5 pix.
 *   DPI x,y = 72,72
 *
 *   Hard margins left=0,0, top=0,0, right=0,0, bottom=0,0
 *   thus these are outside of the print context.
 *
 * Font sizes
 * We make use of the following font size:
 * - standard body font size bfs (e.g. bfs=9)
 *   rather choose bfs=9 for landscape prints, bfs=8 for portrait
 *
 * In order to take into account ascending and descending letters, we
 * have to reserve about 1/4 of the font size above and below each line.
 * So the spacing between lines is about 1/2 bfs.
 */
/* makes use of the same font family for all fields */
static const gchar  *st_font_family                    = "Sans";
static const gint    st_body_font_size                 = 9;
/* a small vertical space between the column headers line and the first line of the summary */
#define              st_title_cols_header_vspacing       st_body_font_size*.5
/* the vertical space between body lines */
#define              st_body_line_vspacing               st_body_font_size*.5
/* the vertical space an account final balance and the next account header */
#define              st_account_vspacing                 st_body_font_size*.5
/* as we use white-on-cyan column headers, we keep a 2px left and right margins */
static const gint    st_page_margin                    = 2;

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
#define st_accnumber_width                             60/9*st_body_font_size
#define st_acccurrency_width                           23/10*st_body_font_size

/* the columns of the entry line */
#define st_date_width                                  54/9*st_body_font_size
#define st_ledger_width                                36/9*st_body_font_size
#define st_piece_width                                 64/9*st_body_font_size
#define st_settlement_width                            20/9*st_body_font_size
#define st_amount_width                                90/9*st_body_font_size
#define st_sens_width                                  18/9*st_body_font_size
#define st_column_hspacing                             4

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

G_DEFINE_TYPE( ofaPrintGenLedger, ofa_print_gen_ledger, G_TYPE_OBJECT )

static gboolean     print_gen_ledger_operate( ofaPrintGenLedger *self );
static GObject     *on_create_custom_widget( GtkPrintOperation *operation, ofaPrintGenLedger *self );
static void         on_from_account_changed( GtkEntry *entry, ofaPrintGenLedger *self );
static void         on_from_account_select( GtkButton *button, ofaPrintGenLedger *self );
static void         on_to_account_changed( GtkEntry *entry, ofaPrintGenLedger *self );
static void         on_to_account_select( GtkButton *button, ofaPrintGenLedger *self );
static void         on_account_changed( GtkEntry *entry, ofaPrintGenLedger *self, GtkWidget *label );
static void         on_account_select( GtkButton *button, ofaPrintGenLedger *self, GtkWidget *entry );
static void         on_all_accounts_toggled( GtkToggleButton *button, ofaPrintGenLedger *self );
static void         on_custom_widget_apply( GtkPrintOperation *operation, GtkWidget *widget, ofaPrintGenLedger *self );
static void         on_begin_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaPrintGenLedger *self );
static void         begin_print_build_body_layout( ofaPrintGenLedger *self, GtkPrintContext *context );
static gboolean     on_paginate( GtkPrintOperation *operation, GtkPrintContext *context, ofaPrintGenLedger *self );
static void         on_draw_page( GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, ofaPrintGenLedger *self );
static gboolean     draw_page( ofaPrintGenLedger *self, GtkPrintContext *context, gboolean draw, gint page_num );
static void         draw_page_header( ofaPrintGenLedger *self, GtkPrintContext *context, gboolean draw, gint page_num, gboolean is_last );
static gboolean     draw_account_header( ofaPrintGenLedger *self, GtkPrintContext *context, gboolean draw, gint line_num );
static gdouble      account_header_height( void );
static void         draw_account_top_report( ofaPrintGenLedger *self, GtkPrintContext *context, gboolean draw );
static void         draw_account_report( ofaPrintGenLedger *self, GtkPrintContext *context, gboolean draw, gboolean with_solde );
static gdouble      account_top_report_height( void );
static void         draw_account_bottom_report( ofaPrintGenLedger *self, GtkPrintContext *context, gboolean draw, GList *next );
static gdouble      account_bottom_report_height( void );
static void         draw_account_balance( ofaPrintGenLedger *self, GtkPrintContext *context, gboolean draw );
static gdouble      account_balance_height( void );
static void         add_account_balance( ofaPrintGenLedger *self );
static gint         cmp_currencies( const sCurrency *a, const sCurrency *b );
static void         draw_account_solde_debit_credit( ofaPrintGenLedger *self, GtkPrintContext *context, gdouble y );
static gboolean     draw_line( ofaPrintGenLedger *self, GtkPrintContext *context, gboolean draw, gint page_num, gint line_num, GList *line, GList *next );
static gboolean     draw_general_summary( ofaPrintGenLedger *self, GtkPrintContext *context, gboolean draw );
static gdouble      general_summary_height( ofaPrintGenLedger *self );
static gboolean     is_new_account( ofaPrintGenLedger *self, ofoEntry *entry );
static void         setup_new_account( ofaPrintGenLedger *self, ofoEntry *entry );
static void         on_end_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaPrintGenLedger *self );

static void
free_currency( sCurrency *total_per_currency )
{
	g_free( total_per_currency->currency );
	g_free( total_per_currency );
}

static void
print_gen_ledger_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_print_gen_ledger_finalize";
	ofaPrintGenLedgerPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PRINT_GEN_LEDGER( instance ));

	/* free data members here */
	priv = OFA_PRINT_GEN_LEDGER( instance )->priv;
	g_free( priv->from_account );
	g_free( priv->to_account );
	g_free( priv->prev_account );
	g_list_free_full( priv->total, ( GDestroyNotify ) free_currency );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_print_gen_ledger_parent_class )->finalize( instance );
}

static void
print_gen_ledger_dispose( GObject *instance )
{
	ofaPrintGenLedgerPrivate *priv;

	g_return_if_fail( instance && OFA_IS_PRINT_GEN_LEDGER( instance ));

	priv = OFA_PRINT_GEN_LEDGER( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		if( priv->entries ){
			g_list_free_full( priv->entries, ( GDestroyNotify ) g_object_unref );
			priv->entries = NULL;
		}
		if( priv->layout ){
			g_clear_object( &priv->layout );
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_print_gen_ledger_parent_class )->dispose( instance );
}

static void
ofa_print_gen_ledger_init( ofaPrintGenLedger *self )
{
	static const gchar *thisfn = "ofa_print_gen_ledger_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PRINT_GEN_LEDGER( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_PRINT_GEN_LEDGER, ofaPrintGenLedgerPrivate );
	self->priv->dispose_has_run = FALSE;
	my_date_clear( &self->priv->from_date );
	my_date_clear( &self->priv->to_date );
	self->priv->entries = NULL;
	self->priv->general_summary_printed = FALSE;
	self->priv->last_printed = NULL;
	self->priv->prev_account = NULL;
	self->priv->prev_header_printed = FALSE;
}

static void
ofa_print_gen_ledger_class_init( ofaPrintGenLedgerClass *klass )
{
	static const gchar *thisfn = "ofa_print_gen_ledger_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = print_gen_ledger_dispose;
	G_OBJECT_CLASS( klass )->finalize = print_gen_ledger_finalize;

	g_type_class_add_private( klass, sizeof( ofaPrintGenLedgerPrivate ));
}

/**
 * ofa_print_gen_ledger_run:
 * @main: the main window of the application.
 *
 * Print the accounts balance
 */
gboolean
ofa_print_gen_ledger_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_print_gen_ledger_run";
	ofaPrintGenLedger *self;
	gboolean printed;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new( OFA_TYPE_PRINT_GEN_LEDGER, NULL );
	self->priv->main_window = main_window;
	printed = print_gen_ledger_operate( self );
	g_object_unref( self );

	return( printed );
}

/*
 * print_gen_ledger_operate:
 *
 * run the GtkPrintOperation operation
 * returns %TRUE if the print has been successful
 */
static gboolean
print_gen_ledger_operate( ofaPrintGenLedger *self )
{
	ofaPrintGenLedgerPrivate *priv;
	gboolean printed;
	GtkPrintOperation *print;
	GtkPrintOperationResult res;
	GtkWidget *dialog;
	gchar *str;
	GError *error;
	GtkPageSetup *psetup;
	GtkPaperSize *psize;

	priv = self->priv;
	printed = FALSE;
	error = NULL;
	print = gtk_print_operation_new ();

	/* Sets up the transformation for the cairo context obtained from
	 * GtkPrintContext in such a way that distances are measured in mm */
	/*gtk_print_operation_set_unit( print, GTK_UNIT_MM );
	 * setting the unit in mm makes the GtkPrintContext gives its width
	 * and height in mm (althouh the doc says in pix)
	 * so: width=197,3, height=276,4 mm for a A4
	 * but 197,3 mm / 25,4 mm per inch * 72 dots per inch = 547 dots, not 559 pix
	 *     276,4                                          = 782  */

	/* unit_none gives width=559,2, height=783,5 */
	gtk_print_operation_set_unit( print, GTK_UNIT_NONE );
	gtk_print_operation_set_custom_tab_label( print, _( "General Ledger Summary" ));

	g_signal_connect( print, "create-custom-widget", G_CALLBACK( on_create_custom_widget ), self );
	g_signal_connect( print, "custom-widget-apply", G_CALLBACK( on_custom_widget_apply ), self );
	g_signal_connect( print, "begin-print", G_CALLBACK( on_begin_print ), self );
	g_signal_connect( print, "paginate", G_CALLBACK( on_paginate ), self );
	g_signal_connect( print, "draw-page", G_CALLBACK( on_draw_page ), self );
	g_signal_connect( print, "end-print", G_CALLBACK( on_end_print ), self );

	psize = gtk_paper_size_new( GTK_PAPER_NAME_A4 );
	psetup = gtk_page_setup_new();
	gtk_page_setup_set_paper_size( psetup, psize );
	gtk_paper_size_free( psize );
	gtk_page_setup_set_orientation( psetup, GTK_PAGE_ORIENTATION_LANDSCAPE );
	gtk_print_operation_set_default_page_setup( print, psetup );
	g_object_unref( psetup );

	res = gtk_print_operation_run(
					print,
					GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
	                GTK_WINDOW( priv->main_window ),
	                &error );

	if( res == GTK_PRINT_OPERATION_RESULT_ERROR ){

		str = g_strdup_printf( _( "Error while printing document:\n%s" ), error->message );
		dialog = gtk_message_dialog_new(
							GTK_WINDOW( priv->main_window ),
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_MESSAGE_ERROR,
							GTK_BUTTONS_CLOSE,
							"%s", str );
		g_free( str );
		g_signal_connect( dialog, "response", G_CALLBACK( gtk_widget_destroy ), NULL );
		gtk_widget_show( dialog );
		g_error_free( error );

	} else if( res == GTK_PRINT_OPERATION_RESULT_APPLY ){

		if( priv->pages_count <= 1 ){
			str = g_strdup_printf(
					_( "The General Ledger has been successfully printed\n"
							"(%u printed page)" ), priv->pages_count );
		} else {
			str = g_strdup_printf(
					_( "The General Ledger has been successfully printed\n"
							"(%u printed pages)" ), priv->pages_count );
		}
		dialog = gtk_message_dialog_new(
							GTK_WINDOW( priv->main_window ),
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_MESSAGE_INFO,
							GTK_BUTTONS_CLOSE,
							"%s", str );
		g_free( str );
		g_signal_connect( dialog, "response", G_CALLBACK( gtk_widget_destroy ), NULL );
		gtk_widget_show( dialog );

		printed = TRUE;
	}

	g_object_unref( print );

	return( printed );
}

/*
 * add a tab to the print notebook
 *
 * - set label before changing the entry, so that the callback doesn't
 *   fail
 */
static GObject*
on_create_custom_widget( GtkPrintOperation *operation, ofaPrintGenLedger *self )
{
	static const gchar *thisfn = "ofa_print_gen_ledger_on_create_custom_widget";
	ofaPrintGenLedgerPrivate *priv;
	GtkWidget *box, *frame, *widget;
	gchar *text;
	gboolean bvalue;
	GDate date;

	g_debug( "%s: operation=%p, self=%p",
			thisfn, ( void * ) operation, ( void * ) self );

	priv = self->priv;

	box = my_utils_builder_load_from_path( st_ui_xml, "box-balance" );
	frame = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "frame-balance" );
	g_object_ref( frame );
	gtk_container_remove( GTK_CONTAINER( box ), frame );
	g_object_unref( box );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "from-account-etiq" );
	g_return_val_if_fail( widget && GTK_IS_LABEL( widget ), NULL );
	priv->from_account_etiq = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "from-account-label" );
	g_return_val_if_fail( widget && GTK_IS_LABEL( widget ), NULL );
	priv->from_account_label = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "from-account-entry" );
	g_return_val_if_fail( widget && GTK_IS_ENTRY( widget ), NULL );
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_from_account_changed ), self );
	text = ofa_settings_get_string( st_pref_from_account );
	if( text && g_utf8_strlen( text, -1 )){
		gtk_entry_set_text( GTK_ENTRY( widget ), text );
	}
	g_free( text );
	priv->from_account_entry = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "from-account-select" );
	g_return_val_if_fail( widget && GTK_IS_BUTTON( widget ), NULL );
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_from_account_select ), self );
	priv->from_account_btn = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "to-account-label" );
	g_return_val_if_fail( widget && GTK_IS_LABEL( widget ), NULL );
	priv->to_account_label = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "to-account-etiq" );
	g_return_val_if_fail( widget && GTK_IS_LABEL( widget ), NULL );
	priv->to_account_etiq = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "to-account-entry" );
	g_return_val_if_fail( widget && GTK_IS_ENTRY( widget ), NULL );
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_to_account_changed ), self );
	text = ofa_settings_get_string( st_pref_to_account );
	if( text && g_utf8_strlen( text, -1 )){
		gtk_entry_set_text( GTK_ENTRY( widget ), text );
	}
	g_free( text );
	priv->to_account_entry = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "to-account-select" );
	g_return_val_if_fail( widget && GTK_IS_BUTTON( widget ), NULL );
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_to_account_select ), self );
	priv->to_account_btn = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "all-accounts" );
	g_return_val_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ), NULL );
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_all_accounts_toggled ), self );
	bvalue = ofa_settings_get_boolean( st_pref_all_accounts );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), !bvalue );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), bvalue );
	priv->all_accounts_btn = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "from-date-entry" );
	g_return_val_if_fail( widget && GTK_IS_ENTRY( widget ), NULL );
	my_editable_date_init( GTK_EDITABLE( widget ));
	my_editable_date_set_format( GTK_EDITABLE( widget ), MY_DATE_DMYY );
	my_editable_date_set_mandatory( GTK_EDITABLE( widget ), FALSE );
	priv->from_date_entry = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "from-date-label" );
	g_return_val_if_fail( widget && GTK_IS_LABEL( widget ), NULL );
	my_editable_date_set_label( GTK_EDITABLE( priv->from_date_entry ), widget, MY_DATE_DMMM );
	text = ofa_settings_get_string( st_pref_from_date );
	if( text && g_utf8_strlen( text, -1 )){
		my_date_set_from_sql( &date, text );
		my_editable_date_set_date( GTK_EDITABLE( priv->from_date_entry ), &date );
	}
	g_free( text );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "to-date-entry" );
	g_return_val_if_fail( widget && GTK_IS_ENTRY( widget ), NULL );
	my_editable_date_init( GTK_EDITABLE( widget ));
	my_editable_date_set_format( GTK_EDITABLE( widget ), MY_DATE_DMYY );
	my_editable_date_set_mandatory( GTK_EDITABLE( widget ), FALSE );
	priv->to_date_entry = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "to-date-label" );
	g_return_val_if_fail( widget && GTK_IS_LABEL( widget ), NULL );
	my_editable_date_set_label( GTK_EDITABLE( priv->to_date_entry ), widget, MY_DATE_DMMM );
	text = ofa_settings_get_string( st_pref_to_date );
	if( text && g_utf8_strlen( text, -1 )){
		my_date_set_from_sql( &date, text );
		my_editable_date_set_date( GTK_EDITABLE( priv->to_date_entry ), &date );
	}
	g_free( text );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "p3-one-page" );
	g_return_val_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ), NULL );
	bvalue = ofa_settings_get_boolean( st_pref_new_page );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), !bvalue );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), bvalue );
	priv->new_page_btn = widget;

	return( G_OBJECT( frame ));
}

static void
on_from_account_changed( GtkEntry *entry, ofaPrintGenLedger *self )
{
	on_account_changed( entry, self, self->priv->from_account_label );
}

static void
on_from_account_select( GtkButton *button, ofaPrintGenLedger *self )
{
	on_account_select( button, self, self->priv->from_account_entry );
}

static void
on_to_account_changed( GtkEntry *entry, ofaPrintGenLedger *self )
{
	on_account_changed( entry, self, self->priv->to_account_label );
}

static void
on_to_account_select( GtkButton *button, ofaPrintGenLedger *self )
{
	on_account_select( button, self, self->priv->to_account_entry );
}

static void
on_account_changed( GtkEntry *entry, ofaPrintGenLedger *self, GtkWidget *label )
{
	ofaPrintGenLedgerPrivate *priv;
	const gchar *str;
	ofoAccount *account;

	priv = self->priv;
	str = gtk_entry_get_text( entry );
	account = ofo_account_get_by_number( ofa_main_window_get_dossier( priv->main_window ), str );
	if( account ){
		gtk_label_set_text( GTK_LABEL( label ), ofo_account_get_label( account ));
	} else {
		gtk_label_set_text( GTK_LABEL( label ), "" );
	}
}

static void
on_account_select( GtkButton *button, ofaPrintGenLedger *self, GtkWidget *entry )
{
	ofaPrintGenLedgerPrivate *priv;
	gchar *number;

	priv = self->priv;
	number = ofa_account_select_run(
						priv->main_window,
						gtk_entry_get_text( GTK_ENTRY( entry )));
	if( number ){
		gtk_entry_set_text( GTK_ENTRY( entry ), number );
		g_free( number );
	}
}

static void
on_all_accounts_toggled( GtkToggleButton *button, ofaPrintGenLedger *self )
{
	ofaPrintGenLedgerPrivate *priv;
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

/*
 * get the content of the added tab
 * all fields are optional
 * then load the entries
 */
static void
on_custom_widget_apply( GtkPrintOperation *operation, GtkWidget *widget, ofaPrintGenLedger *self )
{
	ofaPrintGenLedgerPrivate *priv;
	gboolean all_accounts;
	gchar *text;

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

	priv->entries = ofo_entry_get_dataset_for_print_gen_ledger(
							ofa_main_window_get_dossier( priv->main_window ),
							priv->from_account, priv->to_account,
							&priv->from_date, &priv->to_date );

	priv->new_page = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->new_page_btn ));
	ofa_settings_set_boolean( st_pref_new_page, priv->new_page );
}

static void
on_begin_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaPrintGenLedger *self )
{
	static const gchar *thisfn = "ofa_print_gen_ledger_on_begin_print";
	ofaPrintGenLedgerPrivate *priv;
	gdouble header_height, footer_height;

	/*gint entries_count, other_count, last_count;
	gdouble footer_height, balance_height, last_height, header_height;*/

	g_debug( "%s: operation=%p, context=%p, self=%p",
			thisfn, ( void * ) operation, ( void * ) context, ( void * ) self );

	priv = self->priv;
	priv->page_width = gtk_print_context_get_width( context );
	priv->page_height = gtk_print_context_get_height( context );

	header_height =
			ofa_print_header_dossier_get_height( 1 )
			+ ofa_print_header_title_get_height( 1 )
			+ ofa_print_header_subtitle_get_height( 1 )
			+ 2*st_body_font_size						/* column headers */
			+ st_title_cols_header_vspacing;

	footer_height =
			ofa_print_footer_get_height( 1, FALSE );

	priv->max_y = priv->page_height - footer_height;

	g_debug( "%s: page_width=%.5lf, page_height=%.5lf, "
			"header_height=%.5lf, footer_height=%.5lf, max_y=%.5lf",
			thisfn,
			priv->page_width, priv->page_height,
			header_height, footer_height, priv->max_y );

	priv->layout = gtk_print_context_create_pango_layout( context );
	/* context_width=559, pango_layout_width=572416 */
	begin_print_build_body_layout( self, context );
}

static void
begin_print_build_body_layout( ofaPrintGenLedger *self, GtkPrintContext *context )
{
	ofaPrintGenLedgerPrivate *priv;

	priv = self->priv;

	/* account header, starting from the left */
	priv->body_accnumber_ltab = st_page_margin;
	priv->body_acclabel_ltab = priv->body_accnumber_ltab + st_accnumber_width + st_column_hspacing;
	priv->body_acccurrency_rtab = priv->page_width - st_page_margin;

	/* entry line, starting from the left */
	priv->body_dope_ltab = st_page_margin;
	priv->body_deffect_ltab = priv->body_dope_ltab + st_date_width + st_column_hspacing;
	priv->body_ledger_ltab = priv->body_deffect_ltab + st_date_width + st_column_hspacing;
	priv->body_piece_ltab = priv->body_ledger_ltab + st_ledger_width + st_column_hspacing;
	priv->body_label_ltab = priv->body_piece_ltab + st_piece_width + st_column_hspacing;

	/* entry line, starting from the right */
	priv->body_solde_sens_rtab = priv->page_width - st_page_margin;
	priv->body_solde_rtab = priv->body_solde_sens_rtab - st_sens_width - st_column_hspacing/2;
	priv->body_credit_rtab = priv->body_solde_rtab - st_amount_width - st_column_hspacing;
	priv->body_debit_rtab = priv->body_credit_rtab - st_amount_width - st_column_hspacing;
	priv->body_settlement_ltab = priv->body_debit_rtab - st_amount_width - st_column_hspacing - st_settlement_width;

	/* max size in Pango units */
	priv->body_acclabel_max_size = ( priv->body_acccurrency_rtab - st_acccurrency_width - st_column_hspacing - priv->body_acclabel_ltab )*PANGO_SCALE;
	priv->body_acflabel_max_size = ( priv->body_debit_rtab - st_amount_width - st_column_hspacing - st_page_margin )*PANGO_SCALE;
	priv->body_piece_max_size = st_piece_width*PANGO_SCALE;
	priv->body_label_max_size = ( priv->body_settlement_ltab - st_column_hspacing - priv->body_label_ltab )*PANGO_SCALE;
}

/*
 * Emitted after the “begin-print” signal, but before the actual
 * rendering starts. It keeps getting emitted until a connected signal
 * handler returns TRUE.
 * The ::paginate signal is intended to be used for paginating a
 * document in small chunks, to avoid blocking the user interface for a
 * long time. The signal handler should update the number of pages using
 * gtk_print_operation_set_n_pages(), and return TRUE if the document
 * has been completely paginated.
 *
 * -> use it to compute the count of pages to be printed
 *    we have computed in on_begin_print() the available height per page
 *    iterate through the entries to simulate the printing
 */
static gboolean
on_paginate( GtkPrintOperation *operation, GtkPrintContext *context, ofaPrintGenLedger *self )
{
	static const gchar *thisfn = "ofa_print_gen_ledger_on_paginate";
	ofaPrintGenLedgerPrivate *priv;
	gint page_num;

	g_debug( "%s: operation=%p, context=%p, self=%p",
			thisfn, ( void * ) operation, ( void * ) context, ( void * ) self );

	priv = self->priv;
	page_num = 0;

	while( draw_page( self, context, FALSE, page_num )){
		page_num += 1;
	}

	/* page_num is counted from zero, so add 1 for count */
	priv->pages_count = page_num+1;
	gtk_print_operation_set_n_pages( operation, priv->pages_count );

	priv->general_summary_printed = FALSE;
	g_free( priv->prev_account );
	priv->prev_account = NULL;
	priv->last_printed = NULL;

	return( TRUE );
}

/*
 * this handler is triggered once for each printed page
 */
static void
on_draw_page( GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, ofaPrintGenLedger *self )
{
	static const gchar *thisfn = "ofa_print_gen_ledger_on_draw_page";

	g_debug( "%s: operation=%p, context=%p, page_num=%d, self=%p",
			thisfn, ( void * ) operation, ( void * ) context, page_num, ( void * ) self );

	draw_page( self, context, TRUE, page_num );
}

/*
 * Used when paginating first, then for actually drawing
 *
 * Returns: %TRUE while there is still page(s) to be printed,
 * %FALSE at the end
 *
 * The returned value is only used while paginating.
 */
static gboolean
draw_page( ofaPrintGenLedger *self, GtkPrintContext *context, gboolean draw, gint page_num )
{
	static const gchar *thisfn = "ofa_print_gen_ledger_draw_page";
	ofaPrintGenLedgerPrivate *priv;
	gboolean is_last;
	gint count;
	GList *line, *next;

	g_debug( "%s: self=%p, context=%p, draw=%s, page_num=%d",
			thisfn, ( void * ) self, ( void * ) context, draw ? "True":"False", page_num );

	priv = self->priv;
	is_last = draw ? ( page_num == priv->pages_count-1 ) : FALSE;

	draw_page_header( self, context, draw, page_num, is_last );

	/* get the next line to be printed */
	line = priv->last_printed ? g_list_next( priv->last_printed ) : priv->entries;
	for( count=0 ; line ; count+=1 ){
		next = g_list_next( line );
		if( !draw_line( self, context, draw, page_num, count, line, next )){
			break;
		}
		line = next;
		priv->last_printed = line;
	}

	/* end of the last page */
	if( !line ){
		draw_account_balance( self, context, draw );
		is_last = draw_general_summary( self, context, draw );
	}

	if( draw ){
		ofa_print_footer_render( context, priv->layout, page_num, priv->pages_count );
	}

	return( !is_last );
}

static void
draw_page_header( ofaPrintGenLedger *self, GtkPrintContext *context, gboolean draw, gint page_num, gboolean is_last )
{
	ofaPrintGenLedgerPrivate *priv;
	cairo_t *cr;
	gchar *str, *sfrom_date, *sto_date;
	gdouble y, height;
	GString *stitle;

	priv = self->priv;
	y = 0;
	cr = gtk_print_context_get_cairo_context( context );

	/* dossier header */
	if( draw ){
		ofa_print_header_dossier_render(
				context, priv->layout, page_num, y,
				ofa_main_window_get_dossier( priv->main_window ));
	}
	y += ofa_print_header_dossier_get_height( page_num );

	/* print summary title in line 3 */
	if( draw ){
		ofa_print_header_title_render(
				context, priv->layout, page_num, y,
				_( "General Ledger Summary"));
	}
	y+= ofa_print_header_title_get_height( page_num );

	/* recall of account and date selections in line 4 */
	if( draw ){
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
		ofa_print_header_subtitle_render(
				context, priv->layout, page_num, y, stitle->str );
		g_string_free( stitle, TRUE );
	}
	y+= ofa_print_header_subtitle_get_height( page_num );

	/* column headers
	 * draw a rectangle for one columns header line with spacings as:
	 * spacing(bfs/2) + line(bfs) + spacing(bfs/2) */
	if( draw ){
		ofa_print_header_title_set_color( context, priv->layout );
		height = 2*st_body_font_size;
		cairo_rectangle( cr, 0, y, gtk_print_context_get_width( context ), height );
		cairo_fill( cr );

		/* columns title are white on same dark cyan background */
		str = g_strdup_printf( "%s Bold %d", st_font_family, st_body_font_size );
		ofa_print_set_font( context, priv->layout, str );
		g_free( str );
	}
	y += 0.5*st_body_font_size;
	y -= 1;
	if( draw ){
		cairo_set_source_rgb( cr, COLOR_WHITE );

		ofa_print_set_text( context, priv->layout,
				priv->body_dope_ltab, y,
				_( "Operation" ), PANGO_ALIGN_LEFT );

		ofa_print_set_text( context, priv->layout,
				priv->body_deffect_ltab, y,
				_( "Effect" ), PANGO_ALIGN_LEFT );

		ofa_print_set_text( context, priv->layout,
				priv->body_ledger_ltab, y,
				_( "Ledger" ), PANGO_ALIGN_LEFT );

		ofa_print_set_text( context, priv->layout,
				priv->body_piece_ltab, y,
				_( "Piece" ), PANGO_ALIGN_LEFT );

		ofa_print_set_text( context, priv->layout,
				priv->body_label_ltab, y,
				_( "Label" ), PANGO_ALIGN_LEFT );

		ofa_print_set_text( context, priv->layout,
				priv->body_settlement_ltab, y,
				_( "Sett." ), PANGO_ALIGN_LEFT );

		ofa_print_set_text( context, priv->layout,
				priv->body_debit_rtab, y,
				_( "Debit" ), PANGO_ALIGN_RIGHT );

		ofa_print_set_text( context, priv->layout,
				priv->body_credit_rtab, y,
				_( "Credit" ), PANGO_ALIGN_RIGHT );

		ofa_print_set_text( context, priv->layout,
				priv->page_width-st_page_margin, y,
				_( "Solde" ), PANGO_ALIGN_RIGHT );
	}
	y += 1;
	y += 1.5*st_body_font_size;
	y += st_title_cols_header_vspacing;
	priv->last_y = y;
}

/*
 * draw account header, taking care of having a new page if asked for
 *
 * on a page's bottom, we must have at least:
 * - the header
 * - a line
 * - the bottom of the page, or the account footer
 *
 * more, if line_num > 0, we draw a line between the previous account
 * and this new one
 */
static gboolean
draw_account_header( ofaPrintGenLedger *self, GtkPrintContext *context, gboolean draw, gint line_num )
{
	ofaPrintGenLedgerPrivate *priv;
	cairo_t *cr;
	gchar *str;
	gdouble y, req_height;
	const gchar *cur_mnemo;

	priv = self->priv;

	/* if we must begin the account on a new page, then just return */
	if( priv->new_page && line_num > 0 ){
		return( FALSE );
	}

	/* compute the requested height */
	req_height =
			account_header_height()
			+ st_body_font_size+st_body_line_vspacing
			+ MAX( account_bottom_report_height(), account_balance_height());

	if( line_num > 0 ){
		req_height += st_body_line_vspacing;
	}

	if( priv->last_y + req_height > priv->max_y ){
		return( FALSE );
	}

	/* OK, we have the place, so draw the account header */
	y = priv->last_y;
	cr = gtk_print_context_get_cairo_context( context );

	if( line_num > 0 ){
		if( draw ){
			cairo_set_source_rgb( cr, COLOR_DARK_CYAN );
			cairo_set_line_width( cr, 1 );
			cairo_move_to( cr, 0, y );
			cairo_line_to( cr, priv->page_width, y );
			cairo_stroke( cr );
		}
		y += st_body_line_vspacing;
	}

	/* setup the account properties */
	priv->prev_accobj = ofo_account_get_by_number(
							ofa_main_window_get_dossier( priv->main_window ), priv->prev_account );
	g_return_val_if_fail( priv->prev_accobj && OFO_IS_ACCOUNT( priv->prev_accobj ), TRUE );

	priv->prev_debit = 0;
	priv->prev_credit = 0;

	cur_mnemo = ofo_account_get_currency( priv->prev_accobj );
	priv->prev_currency = ofo_currency_get_by_code(
							ofa_main_window_get_dossier( priv->main_window ), cur_mnemo );
	g_return_val_if_fail( priv->prev_currency && OFO_IS_CURRENCY( priv->prev_currency ), TRUE );

	priv->prev_digits = ofo_currency_get_digits( priv->prev_currency );
	priv->prev_header_printed = TRUE;
	priv->prev_footer_printed = FALSE;

	/* display the account header */
	if( draw ){
		ofa_print_header_title_set_color( context, priv->layout );
		str = g_strdup_printf( "%s Bold %d", st_font_family, st_body_font_size );
		ofa_print_set_font( context, priv->layout, str );
		g_free( str );

		/* account number */
		ofa_print_set_text( context, priv->layout,
				priv->body_accnumber_ltab, y
				, ofo_account_get_number( priv->prev_accobj ), PANGO_ALIGN_LEFT );

		/* account label */
		pango_layout_set_text( priv->layout, ofo_account_get_label( priv->prev_accobj ), -1 );
		my_utils_pango_layout_ellipsize( priv->layout, priv->body_acclabel_max_size );
		cairo_move_to( cr, priv->body_acclabel_ltab, y );
		pango_cairo_update_layout( cr, priv->layout );
		pango_cairo_show_layout( cr, priv->layout );

		/* account currency */
		ofa_print_set_text( context, priv->layout,
				priv->body_acccurrency_rtab, y,
				ofo_account_get_currency( priv->prev_accobj ), PANGO_ALIGN_RIGHT );
	}
	y += account_header_height();
	priv->last_y = y;

	return( TRUE );
}

static gdouble
account_header_height( void )
{
	return( st_body_font_size + st_body_line_vspacing );
}

static void
draw_account_top_report( ofaPrintGenLedger *self, GtkPrintContext *context, gboolean draw )
{
	draw_account_report( self, context, draw, TRUE );
}

static void
draw_account_report( ofaPrintGenLedger *self, GtkPrintContext *context, gboolean draw, gboolean with_solde )
{
	ofaPrintGenLedgerPrivate *priv;
	cairo_t *cr;
	gchar *str;
	gdouble y;

	priv = self->priv;
	y = priv->last_y;
	cr = gtk_print_context_get_cairo_context( context );

	if( draw ){
		ofa_print_header_title_set_color( context, priv->layout );
		str = g_strdup_printf( "%s %d", st_font_family, st_body_font_size );
		ofa_print_set_font( context, priv->layout, str );
		g_free( str );

		/* account number */
		ofa_print_set_text( context, priv->layout,
				priv->body_accnumber_ltab, y, ofo_account_get_number( priv->prev_accobj ), PANGO_ALIGN_LEFT );

		/* account label */
		pango_layout_set_text( priv->layout, ofo_account_get_label( priv->prev_accobj ), -1 );
		my_utils_pango_layout_ellipsize( priv->layout, priv->body_acclabel_max_size );
		cairo_move_to( cr, priv->body_acclabel_ltab, y );
		pango_cairo_update_layout( cr, priv->layout );
		pango_cairo_show_layout( cr, priv->layout );

		/* current account balance */
		str = my_double_to_str_ex( priv->prev_debit, priv->prev_digits );
		ofa_print_set_text( context, priv->layout,
				priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = my_double_to_str_ex( priv->prev_credit, priv->prev_digits );
		ofa_print_set_text( context, priv->layout,
				priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		/* current account solde */
		if( with_solde ){
			draw_account_solde_debit_credit( self, context, y );
		}
	}
	y += account_top_report_height();
	priv->last_y = y;
}

static gdouble
account_top_report_height( void )
{
	return( st_body_line_vspacing + st_body_font_size );
}

/*
 * draw the end of page report unless the @next line will be on another
 * account - is so, then go to account balance report
 */
static void
draw_account_bottom_report( ofaPrintGenLedger *self, GtkPrintContext *context, gboolean draw, GList *next )
{
	if( !next || is_new_account( self, OFO_ENTRY( next->data ))){
		draw_account_balance( self, context, draw );
	} else {
		draw_account_report( self, context, draw, FALSE );
	}
}

static gdouble
account_bottom_report_height( void )
{
	return( account_top_report_height());
}

static void
draw_account_balance( ofaPrintGenLedger *self, GtkPrintContext *context, gboolean draw )
{
	ofaPrintGenLedgerPrivate *priv;
	cairo_t *cr;
	gchar *str;
	gdouble y;

	priv = self->priv;
	y = priv->last_y;
	cr = gtk_print_context_get_cairo_context( context );

	if( draw ){
		ofa_print_header_title_set_color( context, priv->layout );
		str = g_strdup_printf( "%s Bold %d", st_font_family, st_body_font_size );
		ofa_print_set_font( context, priv->layout, str );
		g_free( str );

		/* label */
		str = g_strdup_printf( _( "Balance for account %s - %s" ),
				priv->prev_account,
				ofo_account_get_label( priv->prev_accobj ));
		pango_layout_set_text( priv->layout, str, -1 );
		my_utils_pango_layout_ellipsize( priv->layout, priv->body_acflabel_max_size );
		cairo_move_to( cr, st_page_margin, y );
		pango_cairo_update_layout( cr, priv->layout );
		pango_cairo_show_layout( cr, priv->layout );

		/* solde debit */
		str = my_double_to_str_ex( priv->prev_debit, priv->prev_digits );
		ofa_print_set_text( context, priv->layout,
				priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		/* solde debit */
		str = my_double_to_str_ex( priv->prev_credit, priv->prev_digits );
		ofa_print_set_text( context, priv->layout,
				priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		/* current account solde */
		draw_account_solde_debit_credit( self, context, y );
	}

	priv->prev_footer_printed = TRUE;
	add_account_balance( self );

	y += account_balance_height();
	priv->last_y = y;
}

static gdouble
account_balance_height( void )
{
	return( st_body_font_size + st_body_line_vspacing );
}

/*
 * add the account balance to the total per currency,
 * adding a new currency record if needed
 */
static void
add_account_balance( ofaPrintGenLedger *self )
{
	ofaPrintGenLedgerPrivate *priv;
	GList *it;
	const gchar *currency;
	sCurrency *scur;
	gboolean found;

	priv = self->priv;
	currency = ofo_account_get_currency( priv->prev_accobj );

	for( it=priv->total, found=FALSE ; it ; it=it->next ){
		scur = ( sCurrency * ) it->data;
		if( !g_utf8_collate( scur->currency, currency )){
			found = TRUE;
			break;
		}
	}

	if( !found ){
		scur = g_new0( sCurrency, 1 );
		scur->currency = g_strdup( currency );
		priv->total = g_list_insert_sorted( priv->total, scur, ( GCompareFunc ) cmp_currencies );
		scur->debit = 0;
		scur->credit = 0;
	}

	scur->debit += priv->prev_debit;
	scur->credit += priv->prev_credit;
}

static gint
cmp_currencies( const sCurrency *a, const sCurrency *b )
{
	return( g_utf8_collate( a->currency, b->currency ));
}

static void
draw_account_solde_debit_credit( ofaPrintGenLedger *self, GtkPrintContext *context, gdouble y )
{
	ofaPrintGenLedgerPrivate *priv;
	ofxAmount amount;
	gchar *str;

	priv = self->priv;

	/* current account balance */
	amount = priv->prev_credit - priv->prev_debit;
	if( amount ){
		if( amount > 0 ){
			str = my_double_to_str_ex( amount, priv->prev_digits );
			ofa_print_set_text( context, priv->layout,
					priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
			g_free( str );
			ofa_print_set_text( context, priv->layout,
					priv->body_solde_sens_rtab, y, _( "CR" ), PANGO_ALIGN_RIGHT );
		} else {
			str = my_double_to_str_ex( -1*amount, priv->prev_digits );
			ofa_print_set_text( context, priv->layout,
					priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
			g_free( str );
			ofa_print_set_text( context, priv->layout,
					priv->body_solde_sens_rtab, y, _( "DB" ), PANGO_ALIGN_RIGHT );
		}
	}
}

/*
 * @num_line: line number in the page, counted from 0
 * @line: the line candidate to be printed
 * @next: the next line after this one, may be NULL if we are at the
 *  end of the list
 *
 * (printable)width(A4)=559
 * date  journal  piece    label      debit   credit   solde
 * 10    6        max(10)  max(80)      15d      15d     15d
 *
 * Returns: %TRUE if we may continue to print on this page,
 * %FALSE when we have terminated the page.
 * If the page is terminated, this particular line may not have been
 * printed.
 */
static gboolean
draw_line( ofaPrintGenLedger *self, GtkPrintContext *context, gboolean draw, gint page_num, gint line_num, GList *line, GList *next )
{
	ofaPrintGenLedgerPrivate *priv;
	ofoEntry *entry;
	const gchar *cstr;
	gchar *str;
	cairo_t *cr;
	gdouble req_height, y;
	ofxAmount amount;
	ofxCounter counter;

	priv = self->priv;
	entry = OFO_ENTRY( line->data );

	/* does the account change ? */
	if( is_new_account( self, entry ) || !priv->prev_header_printed ){
		if( priv->prev_account && !priv->prev_footer_printed ){
			draw_account_balance( self, context, draw );
		}
		setup_new_account( self, entry );
		if( !draw_account_header( self, context, draw, line_num )){
			return( FALSE );
		}

	} else if( line_num == 0 ){
		draw_account_top_report( self, context, draw );
	}

	/* only print the line if we also have the vertical space to print
	 * the end-of-page account report */
	req_height =
			st_body_font_size+st_body_line_vspacing
			+ MAX( account_bottom_report_height(), account_balance_height());

	if( priv->last_y + req_height > priv->max_y ){
		draw_account_bottom_report( self, context, draw, next );
		return( FALSE );
	}

	/* last, draw the line ! */
	/* we are using a unique font to draw the lines */
	y = priv->last_y;
	if( draw ){
		cr = gtk_print_context_get_cairo_context( context );

		str = g_strdup_printf( "%s %d", st_font_family, st_body_font_size );
		ofa_print_set_font( context, priv->layout, str );
		g_free( str );

		/* have a rubber every other line */
		if( line_num % 2 ){
			ofa_print_rubber( context, priv->layout,
					y-0.5*st_body_line_vspacing, st_body_font_size+st_body_line_vspacing );
		}

		cairo_set_source_rgb( cr, COLOR_BLACK );

		/* 0 is not really the edge of the sheet, but includes the printer margin */
		/* y is in context units (pixels) */

		/* operation date */
		str = my_date_to_str( ofo_entry_get_dope( entry ), MY_DATE_DMYY );
		ofa_print_set_text( context, priv->layout,
				priv->body_dope_ltab, y, str, PANGO_ALIGN_LEFT );
		g_free( str );

		/* effect date */
		str = my_date_to_str( ofo_entry_get_deffect( entry ), MY_DATE_DMYY );
		ofa_print_set_text( context, priv->layout,
				priv->body_deffect_ltab, y, str, PANGO_ALIGN_LEFT );
		g_free( str );

		/* ledger */
		ofa_print_set_text( context, priv->layout,
				priv->body_ledger_ltab, y, ofo_entry_get_ledger( entry ), PANGO_ALIGN_LEFT );

		/* piece */
		cstr = ofo_entry_get_ref( entry );
		if( cstr && g_utf8_strlen( cstr, -1 )){
			pango_layout_set_text( priv->layout, ofo_entry_get_ref( entry ), -1 );
			my_utils_pango_layout_ellipsize( priv->layout, priv->body_piece_max_size );
			cairo_move_to( cr, priv->body_piece_ltab, y );
			pango_cairo_update_layout( cr, priv->layout );
			pango_cairo_show_layout( cr, priv->layout );
		}

		/* label */
		pango_layout_set_text( priv->layout, ofo_entry_get_label( entry ), -1 );
		my_utils_pango_layout_ellipsize( priv->layout, priv->body_label_max_size );
		cairo_move_to( cr, priv->body_label_ltab, y );
		pango_cairo_update_layout( cr, priv->layout );
		pango_cairo_show_layout( cr, priv->layout );

		/* settlement number */
		counter = ofo_entry_get_settlement_number( entry );
		if( counter ){
			str = g_strdup_printf( "%ld", counter );
			ofa_print_set_text( context, priv->layout,
					priv->body_settlement_ltab, y, str, PANGO_ALIGN_LEFT );
			g_free( str );
		}

		/* debit */
		amount = ofo_entry_get_debit( entry );
		if( amount ){
			str = my_double_to_str_ex( amount, priv->prev_digits );
			ofa_print_set_text( context, priv->layout,
					priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
			g_free( str );
			priv->prev_debit += amount;
		}

		/* credit */
		amount = ofo_entry_get_credit( entry );
		if( amount ){
			str = my_double_to_str_ex( amount, priv->prev_digits );
			ofa_print_set_text( context, priv->layout,
					priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
			g_free( str );
			priv->prev_credit += amount;
		}

		/* current account solde */
		draw_account_solde_debit_credit( self, context, y );
	}

	y += st_body_font_size + st_body_line_vspacing;
	priv->last_y = y;

	return( TRUE );
}

/*
 * print a line per found currency at the end of the printing
 */
static gboolean
draw_general_summary( ofaPrintGenLedger *self, GtkPrintContext *context, gboolean draw )
{
	ofaPrintGenLedgerPrivate *priv;
	cairo_t *cr;
	gdouble req_height, height, top, width, y;
	gchar *str;
	GList *it;
	sCurrency *scur;
	gboolean first;

	priv = self->priv;

	/* make sur we have enough place to draw general summary */
	req_height = general_summary_height( self )
			+ st_body_line_vspacing;
	if( priv->last_y + req_height > priv->max_y ){
		return( FALSE );
	}

	/* actually print */
	cr = gtk_print_context_get_cairo_context( context );
	cairo_set_source_rgb( cr, COLOR_DARK_CYAN );

	/* top of the rectangle */
	height = general_summary_height( self );
	top = priv->max_y - height;

	if( draw ){
		width = gtk_print_context_get_width( context );
		cairo_set_line_width( cr, 0.5 );

		cairo_move_to( cr, 0, top );
		cairo_line_to( cr, width, top );
		cairo_stroke( cr );

		cairo_move_to( cr, 0, priv->max_y );
		cairo_line_to( cr, width, priv->max_y );
		cairo_stroke( cr );

		cairo_move_to( cr, 0, top );
		cairo_line_to( cr, 0, priv->max_y );
		cairo_stroke( cr );

		cairo_move_to( cr, width, top );
		cairo_line_to( cr, width, priv->max_y );
		cairo_stroke( cr );
	}

	y = top + st_body_line_vspacing;

	if( draw ){
		str = g_strdup_printf( "%s Bold %d", st_font_family, st_body_font_size+1 );
		ofa_print_set_font( context, priv->layout, str );
		g_free( str );

		for( it=priv->total, first=TRUE ; it ; it=it->next ){
			scur = ( sCurrency * ) it->data;

			if( first ){
				ofa_print_set_text( context, priv->layout,
						priv->body_debit_rtab-st_amount_width, y,
						_( "General balance : " ), PANGO_ALIGN_RIGHT );
				first = FALSE;
			}

			str = my_double_to_str( scur->debit );
			ofa_print_set_text( context, priv->layout,
					priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
			g_free( str );

			str = my_double_to_str( scur->credit );
			ofa_print_set_text( context, priv->layout,
					priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
			g_free( str );

			ofa_print_set_text( context, priv->layout,
					width-st_page_margin, y, scur->currency, PANGO_ALIGN_RIGHT );

			y += st_body_font_size+1+st_body_line_vspacing;
		}
	}

	return( TRUE );
}

/*
 * one summary line per currency
 */
static gdouble
general_summary_height( ofaPrintGenLedger *self )
{
	return( st_body_line_vspacing
			+ g_list_length( self->priv->total )*(st_body_font_size+1+st_body_line_vspacing ));
}

/*
 * just test if the current entry is on the same account than the
 * previous one
 */
static gboolean
is_new_account( ofaPrintGenLedger *self, ofoEntry *entry )
{
	ofaPrintGenLedgerPrivate *priv;

	priv = self->priv;
	return( !priv->prev_account ||
				g_utf8_collate( priv->prev_account, ofo_entry_get_account( entry )) != 0 );
}

static void
setup_new_account( ofaPrintGenLedger *self, ofoEntry *entry )
{
	ofaPrintGenLedgerPrivate *priv;

	priv = self->priv;

	g_free( priv->prev_account );
	priv->prev_account = g_strdup( ofo_entry_get_account( entry ));
	priv->prev_header_printed = FALSE;
}

static void
on_end_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaPrintGenLedger *self )
{
	static const gchar *thisfn = "ofa_print_gen_ledger_on_end_print";

	g_debug( "%s: operation=%p, context=%p, self=%p",
			thisfn, ( void * ) operation, ( void * ) context, ( void * ) self );
}
