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
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "ui/my-editable-date.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-print.h"
#include "ui/ofa-print-balance.h"

/* private instance data
 */
struct _ofaPrintBalancePrivate {
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

	/* internals
	 */
	gchar         *from_account;
	gchar         *to_account;
	gboolean       all_accounts;
	GDate          from_date;
	GDate          to_date;
	GList         *balances;
	GList         *totals;

	/* print datas
	 */
	gdouble        page_width;
	gdouble        page_height;
	gint           pages_count;
	PangoLayout   *layout;
	gdouble        amount_width;
	gdouble        body_number_ltab;
	gdouble        body_label_ltab;
	gint           body_label_max_size;		/* Pango units */
	gdouble        body_debit_period_rtab;
	gdouble        body_credit_period_rtab;
	gdouble        body_debit_solde_rtab;
	gdouble        body_credit_solde_rtab;
	gdouble        body_currency_rtab;
	gdouble        last_y;
	gint           last_entry;
};

typedef struct {
	gchar  *currency;
	gdouble period_d;
	gdouble period_c;
	gdouble solde_d;
	gdouble solde_c;
}
	sCurrency;

static const gchar  *st_ui_xml              = PKGUIDIR "/ofa-print-balance.piece.ui";

static const gchar  *st_pref_from_account   = "PrintBalanceFromAccount";
static const gchar  *st_pref_to_account     = "PrintBalanceToAccount";
static const gchar  *st_pref_all_accounts   = "PrintBalanceAllAccounts";
static const gchar  *st_pref_from_date      = "PrintBalanceFromDate";
static const gchar  *st_pref_to_date        = "PrintBalanceToDate";

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
 * This summary manages:
 * - a first page:
 *   with the dossier header/footer for the first page
 *   with the summary header/footer for the first page
 * - standard pages:
 *   with the dossier header/footer for a standard page
 *   with the summary header/footer for a standard page
 * - a last page:
 *   with the dossier header/footer for the last page
 *   with the summary header/footer for the last page
 *
 * Font sizes
 * We make use of the following font size:
 * - standard body font size bfs (e.g. bfs=9)
 *   rather choose bfs=9 for landscape prints, bfs=8 for portrait
 *
 * In order to take into account ascending and descending letters, we
 * have to reserve about 1/4 of the font size above and below each line.
 * So the spacing between lines is about 1/2 bfs.
 *
 * We have to pre-count the number of pages of the summary - but how ?
 */
/* makes use of the same font family for all fields */
static const gchar  *st_font_family                    = "Sans";
static const gint    st_body_font_size                 = 8; /*9;*/
/* the space between body lines */
#define              st_body_line_spacing                st_body_font_size*.5
#define              st_col_header_spacing               2
/* as we use white-on-cyan column headers, we keep a 2px left and right margins */
static const gint    st_page_margin                    = 2;

/* the columns of the body */
#define st_number_width                                (gdouble) 60/9*st_body_font_size
#define st_amount_width                                (gdouble) 90/9*st_body_font_size
#define st_currency_width                              (gdouble) 20/9*st_body_font_size
#define st_column_spacing                              (gdouble) 4

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
#define COLOR_WHITE                 1,      1,      1
#define COLOR_DARK_RED              0.5,    0,      0
#define COLOR_DARK_CYAN             0,      0.5156, 0.5156
#define COLOR_LIGHT_GRAY            0.9375, 0.9375, 0.9375

G_DEFINE_TYPE( ofaPrintBalance, ofa_print_balance, G_TYPE_OBJECT )

static gboolean     print_balance_operate( ofaPrintBalance *self );
static GObject     *on_create_custom_widget( GtkPrintOperation *operation, ofaPrintBalance *self );
static void         on_from_account_changed( GtkEntry *entry, ofaPrintBalance *self );
static void         on_from_account_select( GtkButton *button, ofaPrintBalance *self );
static void         on_to_account_changed( GtkEntry *entry, ofaPrintBalance *self );
static void         on_to_account_select( GtkButton *button, ofaPrintBalance *self );
static void         on_account_changed( GtkEntry *entry, ofaPrintBalance *self, GtkWidget *label );
static void         on_account_select( GtkButton *button, ofaPrintBalance *self, GtkWidget *entry );
static void         on_all_accounts_toggled( GtkToggleButton *button, ofaPrintBalance *self );
static void         on_custom_widget_apply( GtkPrintOperation *operation, GtkWidget *widget, ofaPrintBalance *self );
static void         on_begin_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaPrintBalance *self );
static void         begin_print_build_body_layout( ofaPrintBalance *self, GtkPrintContext *context );
static void         on_draw_page( GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, ofaPrintBalance *self );
static void         draw_page_header( ofaPrintBalance *self, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, gboolean is_last );
static void         draw_page_line( ofaPrintBalance *self, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, gint line_num, ofsAccountBalance *sbal );
static void         add_account_balance( ofaPrintBalance *self, const gchar *currency, gdouble solde, ofsAccountBalance *sbal );
static gint         cmp_currencies( const sCurrency *a, const sCurrency *b );
static void         draw_page_end_summary( ofaPrintBalance *self, GtkPrintContext *context );
static void         on_end_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaPrintBalance *self );

static void
free_currency( sCurrency *total_per_currency )
{
	g_free( total_per_currency->currency );
	g_free( total_per_currency );
}

static void
print_balance_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_print_balance_finalize";
	ofaPrintBalancePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PRINT_BALANCE( instance ));

	/* free data members here */
	priv = OFA_PRINT_BALANCE( instance )->priv;
	g_free( priv->from_account );
	g_free( priv->to_account );
	g_list_free_full( priv->totals, ( GDestroyNotify ) free_currency );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_print_balance_parent_class )->finalize( instance );
}

static void
print_balance_dispose( GObject *instance )
{
	ofaPrintBalancePrivate *priv;

	g_return_if_fail( instance && OFA_IS_PRINT_BALANCE( instance ));

	priv = OFA_PRINT_BALANCE( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		if( priv->balances ){
			ofo_account_free_balances( priv->balances );
			priv->balances = NULL;
		}
		if( priv->layout ){
			g_clear_object( &priv->layout );
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_print_balance_parent_class )->dispose( instance );
}

static void
ofa_print_balance_init( ofaPrintBalance *self )
{
	static const gchar *thisfn = "ofa_print_balance_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PRINT_BALANCE( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_PRINT_BALANCE, ofaPrintBalancePrivate );
	self->priv->dispose_has_run = FALSE;
	my_date_clear( &self->priv->from_date );
	my_date_clear( &self->priv->to_date );
	self->priv->balances = NULL;
	self->priv->last_entry = -1;
}

static void
ofa_print_balance_class_init( ofaPrintBalanceClass *klass )
{
	static const gchar *thisfn = "ofa_print_balance_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = print_balance_dispose;
	G_OBJECT_CLASS( klass )->finalize = print_balance_finalize;

	g_type_class_add_private( klass, sizeof( ofaPrintBalancePrivate ));
}

/**
 * ofa_print_balance_run:
 * @main: the main window of the application.
 *
 * Print the accounts balance
 */
gboolean
ofa_print_balance_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_print_balance_run";
	ofaPrintBalance *self;
	gboolean printed;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new( OFA_TYPE_PRINT_BALANCE, NULL );
	self->priv->main_window = main_window;
	printed = print_balance_operate( self );
	g_object_unref( self );

	return( printed );
}

/*
 * print_balance_operate:
 *
 * run the GtkPrintOperation operation
 * returns %TRUE if the print has been successful
 */
static gboolean
print_balance_operate( ofaPrintBalance *self )
{
	ofaPrintBalancePrivate *priv;
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
	gtk_print_operation_set_custom_tab_label( print, _( "Accounts Balance Summary" ));

	g_signal_connect( print, "create-custom-widget", G_CALLBACK( on_create_custom_widget ), self );
	g_signal_connect( print, "custom-widget-apply", G_CALLBACK( on_custom_widget_apply ), self );
	g_signal_connect( print, "begin-print", G_CALLBACK( on_begin_print ), self );
	g_signal_connect( print, "draw-page", G_CALLBACK( on_draw_page ), self );
	g_signal_connect( print, "end-print", G_CALLBACK( on_end_print ), self );

	psize = gtk_paper_size_new( GTK_PAPER_NAME_A4 );
	psetup = gtk_page_setup_new();
	gtk_page_setup_set_paper_size( psetup, psize );
	gtk_paper_size_free( psize );
	gtk_page_setup_set_orientation( psetup, GTK_PAGE_ORIENTATION_PORTRAIT );
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
					_( "The Accounts Balance has been successfully printed\n"
							"(%u printed page)" ), priv->pages_count );
		} else {
			str = g_strdup_printf(
					_( "The Accounts Balance has been successfully printed\n"
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
on_create_custom_widget( GtkPrintOperation *operation, ofaPrintBalance *self )
{
	static const gchar *thisfn = "ofa_print_balance_on_create_custom_widget";
	ofaPrintBalancePrivate *priv;
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

	return( G_OBJECT( frame ));
}

static void
on_from_account_changed( GtkEntry *entry, ofaPrintBalance *self )
{
	on_account_changed( entry, self, self->priv->from_account_label );
}

static void
on_from_account_select( GtkButton *button, ofaPrintBalance *self )
{
	on_account_select( button, self, self->priv->from_account_entry );
}

static void
on_to_account_changed( GtkEntry *entry, ofaPrintBalance *self )
{
	on_account_changed( entry, self, self->priv->to_account_label );
}

static void
on_to_account_select( GtkButton *button, ofaPrintBalance *self )
{
	on_account_select( button, self, self->priv->to_account_entry );
}

static void
on_account_changed( GtkEntry *entry, ofaPrintBalance *self, GtkWidget *label )
{
	ofaPrintBalancePrivate *priv;
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
on_account_select( GtkButton *button, ofaPrintBalance *self, GtkWidget *entry )
{
	ofaPrintBalancePrivate *priv;
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
on_all_accounts_toggled( GtkToggleButton *button, ofaPrintBalance *self )
{
	ofaPrintBalancePrivate *priv;
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
on_custom_widget_apply( GtkPrintOperation *operation, GtkWidget *widget, ofaPrintBalance *self )
{
	ofaPrintBalancePrivate *priv;
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

	priv->balances = ofo_entry_get_dataset_for_print_balance(
							ofa_main_window_get_dossier( priv->main_window ),
							priv->from_account, priv->to_account,
							&priv->from_date, &priv->to_date );
}

static void
on_begin_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaPrintBalance *self )
{
	static const gchar *thisfn = "ofa_print_balance_on_begin_print";
	ofaPrintBalancePrivate *priv;
	gint entries_count, lines;
	gdouble context_width, context_height;
	gdouble header_height, footer_height, summary_height, line_height;
	gdouble avail, need;

	/*gint entries_count, other_count, last_count;
	gdouble footer_height, balance_height, last_height, header_height;*/

	g_debug( "%s: operation=%p, context=%p, self=%p",
			thisfn, ( void * ) operation, ( void * ) context, ( void * ) self );

	priv = self->priv;

	context_width = gtk_print_context_get_width( context );
	context_height = gtk_print_context_get_height( context );
	priv->page_width = context_width - 2*st_page_margin;
	priv->page_height = context_height;

	g_debug( "%s; context_width=%lf, context_height=%lf, page_width=%lf, page_height=%lf",
			thisfn,
			context_width, context_height,
			priv->page_width, priv->page_height );

	header_height =
			ofa_print_header_dossier_get_height( 1 )
			+ ofa_print_header_title_get_height( 1 )
			+ ofa_print_header_subtitle_get_height( 1 )
			+ 3*st_body_font_size						/* column headers */
			+ st_body_line_spacing/2;

	footer_height =
			ofa_print_footer_get_height( 1, FALSE );

	summary_height =
			st_body_font_size
			+ st_body_line_spacing;

	line_height = st_body_font_size + st_body_line_spacing;

	entries_count = g_list_length( priv->balances );
	g_debug( "%s: lines_count=%u", thisfn, entries_count );

	avail = priv->page_height - header_height -footer_height;
	need = ( entries_count * line_height ) + summary_height;
	priv->pages_count = 1;

	if( need > avail ){
		while( TRUE ){
			priv->pages_count += 1;
			lines = avail / line_height;	/* lines of standard page */
			entries_count -= lines;			/* left lines to be printed */
			need = ( entries_count * line_height ) + summary_height;
			if( need <= avail ){
				break;
			}
		}
	}
	gtk_print_operation_set_n_pages( operation, priv->pages_count );

	priv->layout = gtk_print_context_create_pango_layout( context );
	/* context_width=559, pango_layout_width=572416 */
	begin_print_build_body_layout( self, context );
}

static void
begin_print_build_body_layout( ofaPrintBalance *self, GtkPrintContext *context )
{
	ofaPrintBalancePrivate *priv;

	priv = self->priv;

	/* starting from the left : body_number_ltab on the left margin */
	priv->body_number_ltab = st_page_margin;
	priv->body_label_ltab = priv->body_number_ltab+ st_number_width + st_column_spacing;

	/* starting from the right */
	priv->amount_width = (priv->page_width/2 - st_currency_width)/4 - st_column_spacing;
	/* g_debug( "begin_print_build_body_layout: st_amount_widht=%d, amount_width=%lf", st_amount_width, amount_width );
	st_amount_widht=80, amount_width=61,409449 */

	priv->body_currency_rtab = priv->page_width + st_page_margin;
	priv->body_credit_solde_rtab = priv->body_currency_rtab - st_currency_width - st_column_spacing;
	priv->body_debit_solde_rtab = priv->body_credit_solde_rtab - priv->amount_width - st_column_spacing;
	priv->body_credit_period_rtab = priv->body_debit_solde_rtab - priv->amount_width - st_column_spacing;
	priv->body_debit_period_rtab = priv->body_credit_period_rtab - priv->amount_width - st_column_spacing;

	/* max size in Pango units */
	priv->body_label_max_size = ( priv->body_debit_period_rtab - priv->amount_width - st_column_spacing - priv->body_label_ltab )*PANGO_SCALE;
}

/*
 * this handler is triggered once for each printed page
 */
static void
on_draw_page( GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, ofaPrintBalance *self )
{
	static const gchar *thisfn = "ofa_print_balance_on_draw_page";
	ofaPrintBalancePrivate *priv;
	gboolean is_last;
	gint count;
	GList *ent;
	gdouble max_y;

	g_debug( "%s: operation=%p, context=%p, page_num=%d, self=%p",
			thisfn, ( void * ) operation, ( void * ) context, page_num, ( void * ) self );

	priv = self->priv;

	is_last = ( page_num == priv->pages_count-1 );

	draw_page_header( self, operation, context, page_num, is_last );
	priv->last_y += st_body_line_spacing/2;

	max_y = priv->page_height
				- ofa_print_footer_get_height( 1, FALSE );

	for( count=0, ent=g_list_nth( priv->balances, priv->last_entry+1 ) ;
								ent && priv->last_y<max_y ; ent=ent->next, count++ ){

		draw_page_line( self, operation, context, page_num, count, ( ofsAccountBalance * )( ent->data ));
	}
	priv->last_entry += count;

	/* end of the last page */
	if( is_last ){
		draw_page_end_summary( self, context );
	}

	ofa_print_footer_render( context, priv->layout, page_num, priv->pages_count );
}

static void
draw_page_header( ofaPrintBalance *self, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, gboolean is_last )
{
	ofaPrintBalancePrivate *priv;
	cairo_t *cr;
	gchar *str, *sfrom_date, *sto_date;
	gdouble y, height, yh;
	GString *stitle;

	priv = self->priv;
	y = 0;

	/* dossier header */
	ofa_print_header_dossier_render(
			context, priv->layout, page_num, y,
			ofa_main_window_get_dossier( priv->main_window ));
	y += ofa_print_header_dossier_get_height( page_num );

	/* print summary title in line 3 */
	ofa_print_header_title_render(
			context, priv->layout, page_num, y,
			_( "Accounts Balance Summary"));
	y+= ofa_print_header_title_get_height( page_num );

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
	ofa_print_header_subtitle_render(
			context, priv->layout, page_num, y, stitle->str );
	g_string_free( stitle, TRUE );
	y+= ofa_print_header_subtitle_get_height( page_num );

	/* column headers
	 * draw a rectangle for two lines with spacings as:
	 * spacing(bfs/2) + line(bfs) + spacing(?) + line(bfs) + spacing(bfs/2) */
	ofa_print_header_title_set_color( context, priv->layout );
	cr = gtk_print_context_get_cairo_context( context );
	height = 3*st_body_font_size+st_col_header_spacing;
	cairo_rectangle( cr, 0, y, gtk_print_context_get_width( context ), height );
	cairo_fill( cr );

	/* draw three vertical white lines to visually separate the amounts */
	cairo_set_source_rgb( cr, COLOR_WHITE );
	cairo_set_line_width( cr, 0.5 );

	cairo_move_to( cr, priv->body_debit_period_rtab-priv->amount_width, y );
	cairo_line_to( cr, priv->body_debit_period_rtab-priv->amount_width, y+height );
	cairo_stroke( cr );

	cairo_move_to( cr, priv->body_credit_period_rtab+st_page_margin, y );
	cairo_line_to( cr, priv->body_credit_period_rtab+st_page_margin, y+height );
	cairo_stroke( cr );

	cairo_move_to( cr, priv->body_credit_solde_rtab+st_page_margin, y );
	cairo_line_to( cr, priv->body_credit_solde_rtab+st_page_margin, y+height );
	cairo_stroke( cr );

	yh = y+(height/2);
	cairo_move_to( cr, priv->body_debit_period_rtab-priv->amount_width, yh );
	cairo_line_to( cr, priv->body_credit_solde_rtab+st_page_margin, yh );
	cairo_stroke( cr );

	str = g_strdup_printf( "%s Bold %d", st_font_family, st_body_font_size-1 );
	ofa_print_set_font( context, priv->layout, str );
	g_free( str );

	/* columns title are white on same dark cyan background */
	cairo_set_source_rgb( cr, COLOR_WHITE );
	y += 0.5*st_body_font_size;

	ofa_print_set_text( context, priv->layout,
			priv->body_number_ltab, y+(st_body_font_size+st_col_header_spacing)/2,
			_( "Account" ), PANGO_ALIGN_LEFT );

	ofa_print_set_text( context, priv->layout,
			priv->body_label_ltab, y+(st_body_font_size+st_col_header_spacing)/2,
			_( "Label" ), PANGO_ALIGN_LEFT );

	ofa_print_set_text( context, priv->layout,
			priv->body_debit_period_rtab,
			y-1, _( "Period balance" ), PANGO_ALIGN_CENTER );

	ofa_print_set_text( context, priv->layout,
			priv->body_debit_solde_rtab,
			y-1, _( "Solde balance" ), PANGO_ALIGN_CENTER );

	y += st_body_font_size + st_col_header_spacing;

	ofa_print_set_text( context, priv->layout,
			priv->body_debit_period_rtab, y+1, _( "Debit" ), PANGO_ALIGN_RIGHT );

	ofa_print_set_text( context, priv->layout,
			priv->body_credit_period_rtab, y+1, _( "Credit" ), PANGO_ALIGN_RIGHT );

	ofa_print_set_text( context, priv->layout,
			priv->body_debit_solde_rtab, y+1, _( "Debit" ), PANGO_ALIGN_RIGHT );

	ofa_print_set_text( context, priv->layout,
			priv->body_credit_solde_rtab, y+1, _( "Credit" ), PANGO_ALIGN_RIGHT );

	y += 1.5*st_body_font_size;

	priv->last_y = y;

	/*g_debug( "draw_header: final y=%lf", y ); */
	/* w_header: final y=138,500000
(openbook:27358): OFA-DEBUG: ofa_print_balance_on_draw_page: operation=0x7a9430, context=0xb55e80, self=0x8afd00
(openbook:27358): OFA-DEBUG: draw_header: final y=116,00000 */
}

/*
 * num_line is counted from 0 in the page
 *
 * (printable)width(A4)=559
 * date  journal  piece    label      debit   credit   solde
 * 10    6        max(10)  max(80)      15d      15d     15d
 */
static void
draw_page_line( ofaPrintBalance *self, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, gint line_num, ofsAccountBalance *sbal )
{
	ofaPrintBalancePrivate *priv;
	gchar *str;
	cairo_t *cr;
	gdouble y;
	ofoAccount *account;
	gdouble solde;
	const gchar *currency;

	priv = self->priv;

	y = priv->last_y;

	cr = gtk_print_context_get_cairo_context( context );

	/* we are using a unique font to draw the lines */
	str = g_strdup_printf( "%s %d", st_font_family, st_body_font_size );
	ofa_print_set_font( context, priv->layout, str );
	g_free( str );

	/* have a rubber every other line */
	if( line_num % 2 ){
		ofa_print_rubber( context, priv->layout,
				y-0.5*st_body_line_spacing, st_body_font_size+st_body_line_spacing );
	}

	cairo_set_source_rgb( cr, COLOR_BLACK );

	/* 0 is not really the edge of the sheet, but includes the printer margin */
	/* y is in context units (pixels) */

	ofa_print_set_text( context, priv->layout,
			priv->body_number_ltab, y, sbal->account, PANGO_ALIGN_LEFT );

	account = ofo_account_get_by_number(
					ofa_main_window_get_dossier( priv->main_window ), sbal->account );

	pango_layout_set_text( priv->layout, ofo_account_get_label( account ), -1 );
	my_utils_pango_layout_ellipsize( priv->layout, priv->body_label_max_size );
	cairo_move_to( cr, priv->body_label_ltab, y );
	pango_cairo_update_layout( cr, priv->layout );
	pango_cairo_show_layout( cr, priv->layout );

	solde = 0;

	if( sbal->debit ){
		str = my_double_to_str( sbal->debit );
		ofa_print_set_text( context, priv->layout,
				priv->body_debit_period_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		solde -= sbal->debit;
	}

	if( sbal->credit ){
		str = my_double_to_str( sbal->credit );
		ofa_print_set_text( context, priv->layout,
				priv->body_credit_period_rtab, y, str, PANGO_ALIGN_RIGHT );
		solde += sbal->credit;
	}

	if( solde < 0 ){
		str = my_double_to_str( -1*solde );
		ofa_print_set_text( context, priv->layout,
				priv->body_debit_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
	}

	if( solde > 0 ){
		str = my_double_to_str( solde );
		ofa_print_set_text( context, priv->layout,
				priv->body_credit_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
	}

	currency = ofo_account_get_currency( account );
	ofa_print_set_text( context, priv->layout,
			priv->body_currency_rtab, y, currency, PANGO_ALIGN_RIGHT );

	add_account_balance( self, currency, solde, sbal );

	y += st_body_font_size + st_body_line_spacing;
	priv->last_y = y;
}

static void
add_account_balance( ofaPrintBalance *self,
							const gchar *currency, gdouble solde, ofsAccountBalance *sbal )
{
	ofaPrintBalancePrivate *priv;
	GList *it;
	sCurrency *scur;
	gboolean found;

	priv = self->priv;

	for( it=priv->totals, found=FALSE ; it ; it=it->next ){
		scur = ( sCurrency * ) it->data;
		if( !g_utf8_collate( scur->currency, currency )){
			found = TRUE;
			break;
		}
	}

	if( !found ){
		scur = g_new0( sCurrency, 1 );
		scur->currency = g_strdup( currency );
		priv->totals = g_list_insert_sorted( priv->totals, scur, ( GCompareFunc ) cmp_currencies );
		scur->period_d = 0;
		scur->period_c = 0;
		scur->solde_d = 0;
		scur->solde_c = 0;
	}

	scur->period_d += sbal->debit;
	scur->period_c += sbal->credit;
	if( solde < 0 ){
		scur->solde_d += -1*solde;
	} else if( solde > 0 ){
		scur->solde_c += solde;
	}
}

static gint
cmp_currencies( const sCurrency *a, const sCurrency *b )
{
	return( g_utf8_collate( a->currency, b->currency ));
}

static void
draw_page_end_summary( ofaPrintBalance *self, GtkPrintContext *context )
{
	ofaPrintBalancePrivate *priv;
	cairo_t *cr;
	gdouble req_height, y, top, bottom, width;
	gchar *str;
	GList *it;
	gboolean first;
	sCurrency *scur;

	priv = self->priv;

	cr = gtk_print_context_get_cairo_context( context );
	cairo_set_source_rgb( cr, COLOR_DARK_CYAN );

	/* bottom of the rectangle */
	bottom = priv->page_height - ofa_print_footer_get_height( 1, FALSE );
	/* top of the rectangle */
	req_height = st_body_line_spacing
			+ g_list_length( priv->totals )*(st_body_font_size+1+st_body_line_spacing );
	top = bottom - req_height;

	width = gtk_print_context_get_width( context );
	cairo_set_line_width( cr, 0.5 );

	cairo_move_to( cr, 0, top );
	cairo_line_to( cr, width, top );
	cairo_stroke( cr );

	cairo_move_to( cr, 0, bottom );
	cairo_line_to( cr, width, bottom );
	cairo_stroke( cr );

	cairo_move_to( cr, 0, top );
	cairo_line_to( cr, 0, bottom );
	cairo_stroke( cr );

	cairo_move_to( cr, width, top );
	cairo_line_to( cr, width, bottom );
	cairo_stroke( cr );

	str = g_strdup_printf( "%s Bold %d", st_font_family, st_body_font_size );
	ofa_print_set_font( context, priv->layout, str );
	g_free( str );

	y = top + st_body_line_spacing;

	for( it=priv->totals, first=TRUE ; it ; it=it->next ){

		if( first ){
			ofa_print_set_text( context, priv->layout,
					priv->body_debit_period_rtab-st_amount_width, y,
					_( "General balance : " ), PANGO_ALIGN_RIGHT );
			first = FALSE;
		}

		scur = ( sCurrency * ) it->data;

		str = my_double_to_str( scur->period_d );
		ofa_print_set_text( context, priv->layout,
				priv->body_debit_period_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = my_double_to_str( scur->period_c );
		ofa_print_set_text( context, priv->layout,
				priv->body_credit_period_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = my_double_to_str( scur->solde_d );
		ofa_print_set_text( context, priv->layout,
				priv->body_debit_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = my_double_to_str( scur->solde_c );
		ofa_print_set_text( context, priv->layout,
				priv->body_credit_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		ofa_print_set_text( context, priv->layout,
				priv->body_currency_rtab, y, scur->currency, PANGO_ALIGN_RIGHT );

		y += st_body_font_size+1+st_body_line_spacing;
	}
}

static void
on_end_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaPrintBalance *self )
{
	static const gchar *thisfn = "ofa_print_balance_on_end_print";

	g_debug( "%s: operation=%p, context=%p, self=%p",
			thisfn, ( void * ) operation, ( void * ) context, ( void * ) self );
}
