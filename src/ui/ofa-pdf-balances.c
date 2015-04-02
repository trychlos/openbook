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
#include "api/my-window-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-class.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofs-account-balance.h"

#include "ui/my-editable-date.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-date-filter-bin.h"
#include "ui/ofa-iprintable.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-pdf-balances.h"

/* private instance data
 */
struct _ofaPDFBalancesPrivate {

	gboolean          printed;

	/* UI
	 */
	GtkWidget        *from_account_etiq;	/* account selection */
	GtkWidget        *from_account_entry;
	GtkWidget        *from_account_btn;
	GtkWidget        *from_account_label;
	GtkWidget        *to_account_etiq;
	GtkWidget        *to_account_entry;
	GtkWidget        *to_account_btn;
	GtkWidget        *to_account_label;

	ofaDateFilterBin *dates_filter;

	GtkWidget        *per_class_btn;	/* subtotal per class */
	GtkWidget        *new_page_btn;

	GtkWidget        *msg_label;
	GtkWidget        *btn_ok;

	/* internals
	 */
	gchar            *from_account;
	gchar            *to_account;
	gboolean          all_accounts;
	gboolean          per_class;
	gboolean          new_page;
	GDate             from_date;
	GDate             to_date;
	GList            *totals;
	gint              count;			/* count of returned entries */

	/* print datas
	 */
	gdouble           page_margin;
	gdouble           amount_width;
	gdouble           body_number_ltab;
	gdouble           body_label_ltab;
	gint              body_label_max_size;		/* Pango units */
	gdouble           body_debit_period_rtab;
	gdouble           body_credit_period_rtab;
	gdouble           body_debit_solde_rtab;
	gdouble           body_credit_solde_rtab;
	gdouble           body_currency_rtab;

	/* subtotal per class
	 */
	gint              class_num;
	ofoClass         *class_object;
	GList            *subtotals;		/* subtotals per currency for this class */
};

typedef struct {
	gchar  *currency;
	gdouble period_d;
	gdouble period_c;
	gdouble solde_d;
	gdouble solde_c;
}
	sCurrency;

static const gchar *st_ui_xml            = PKGUIDIR "/ofa-print-balances.ui";
static const gchar *st_ui_id             = "PrintBalancesDlg";

static const gchar *st_pref_uri          = "PDFBalancesURI";
static const gchar *st_pref_settings     = "PDFBalancesSettings";
static const gchar *st_pref_dates        = "PDFBalancesDates";

static const gchar *st_def_fname         = "Balances.pdf";
static const gchar *st_page_header_title = N_( "Entries Balance Summary" );

/* these are parms which describe the page layout
 */
/* the space between columns headers */
#define st_page_header_columns_vspace    (gdouble) 2

/* the columns of the body */
#define st_number_width                  (gdouble) 50/9*body_font_size
#define st_currency_width                (gdouble) 23/9*body_font_size
#define st_column_spacing                (gdouble) 4

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

static void     iprintable_iface_init( ofaIPrintableInterface *iface );
static guint    iprintable_get_interface_version( const ofaIPrintable *instance );
static void     v_init_dialog( myDialog *dialog );
static void     init_account_selection( ofaPDFBalances *self );
static void     init_date_selection( ofaPDFBalances *self );
static void     init_others( ofaPDFBalances *self );
static void     on_from_account_changed( GtkEntry *entry, ofaPDFBalances *self );
static void     on_from_account_select( GtkButton *button, ofaPDFBalances *self );
static void     on_to_account_changed( GtkEntry *entry, ofaPDFBalances *self );
static void     on_to_account_select( GtkButton *button, ofaPDFBalances *self );
static void     on_account_changed( ofaPDFBalances *self, GtkEntry *entry, GtkWidget *label, gchar **dest );
static void     on_account_select( GtkButton *button, ofaPDFBalances *self, GtkWidget *entry );
static void     on_all_accounts_toggled( GtkToggleButton *button, ofaPDFBalances *self );
static void     on_per_class_toggled( GtkToggleButton *button, ofaPDFBalances *self );
static void     on_new_page_toggled( GtkToggleButton *button, ofaPDFBalances *self );
static void     on_date_filter_changed( ofaDateFilterBin *bin, gint who, gboolean empty, gboolean valid, ofaPDFBalances *self );
static void     check_for_validable_dlg( ofaPDFBalances *self );
static gboolean v_quit_on_ok( myDialog *dialog );
static gboolean do_apply( ofaPDFBalances *self );
static GList   *iprintable_get_dataset( const ofaIPrintable *instance );
static void     iprintable_free_dataset( GList *elements );
static void     iprintable_reset_runtime( ofaIPrintable *instance );
static void     iprintable_on_begin_print( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static gchar   *iprintable_get_page_header_title( const ofaIPrintable *instance );
static gchar   *iprintable_get_page_header_subtitle( const ofaIPrintable *instance );
static void     iprintable_draw_page_header_notes( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num );
static void     iprintable_draw_page_header_columns( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num );
static gboolean iprintable_is_new_group( const ofaIPrintable *instance, GList *current, GList *prev );
static void     iprintable_draw_group_header( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, GList *current );
static void     iprintable_draw_group_top_report( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static void     iprintable_draw_line( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, GList *current );
static void     iprintable_draw_group_bottom_report( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static void     iprintable_draw_group_footer( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static void     iprintable_draw_bottom_summary( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static void     draw_subtotals_balance( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, const gchar *title );
static void     draw_account_balance( ofaIPrintable *instance, GtkPrintContext *context, GList *list, gdouble top, const gchar *title );
static GList   *add_account_balance( ofaPDFBalances *self, GList *list, const gchar *currency, gdouble solde, ofsAccountBalance *sbal );
static gint     cmp_currencies( const sCurrency *a, const sCurrency *b );
static void     get_settings( ofaPDFBalances *self );
static void     set_settings( ofaPDFBalances *self );

G_DEFINE_TYPE_EXTENDED( ofaPDFBalances, ofa_pdf_balances, OFA_TYPE_PDF_DIALOG, 0, \
		G_IMPLEMENT_INTERFACE (OFA_TYPE_IPRINTABLE, iprintable_iface_init ));

static void
free_currency( sCurrency *total_per_currency )
{
	g_free( total_per_currency->currency );
	g_free( total_per_currency );
}

static void
pdf_balances_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_pdf_balances_finalize";
	ofaPDFBalancesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PDF_BALANCES( instance ));

	/* free data members here */
	priv = OFA_PDF_BALANCES( instance )->priv;
	g_free( priv->from_account );
	g_free( priv->to_account );
	g_list_free_full( priv->totals, ( GDestroyNotify ) free_currency );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_pdf_balances_parent_class )->finalize( instance );
}

static void
pdf_balances_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_PDF_BALANCES( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_pdf_balances_parent_class )->dispose( instance );
}

static void
ofa_pdf_balances_init( ofaPDFBalances *self )
{
	static const gchar *thisfn = "ofa_pdf_balances_instance_init";
	ofaPDFBalancesPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PDF_BALANCES( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_PDF_BALANCES, ofaPDFBalancesPrivate );
	priv = self->priv;

	priv->printed = FALSE;
	priv->per_class = FALSE;
}

static void
ofa_pdf_balances_class_init( ofaPDFBalancesClass *klass )
{
	static const gchar *thisfn = "ofa_pdf_balances_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = pdf_balances_dispose;
	G_OBJECT_CLASS( klass )->finalize = pdf_balances_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaPDFBalancesPrivate ));
}

static void
iprintable_iface_init( ofaIPrintableInterface *iface )
{
	static const gchar *thisfn = "ofa_pdf_balances_iprintable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iprintable_get_interface_version;
	iface->get_dataset = iprintable_get_dataset;
	iface->free_dataset = iprintable_free_dataset;
	iface->reset_runtime = iprintable_reset_runtime;
	iface->on_begin_print = iprintable_on_begin_print;
	iface->get_page_header_title = iprintable_get_page_header_title;
	iface->get_page_header_subtitle = iprintable_get_page_header_subtitle;
	iface->draw_page_header_notes = iprintable_draw_page_header_notes;
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
 * ofa_pdf_balances_run:
 * @main: the main window of the application.
 *
 * Print the accounts balance
 */
gboolean
ofa_pdf_balances_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_pdf_balances_run";
	ofaPDFBalances *self;
	gboolean printed;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new(
				OFA_TYPE_PDF_BALANCES,
				MY_PROP_MAIN_WINDOW, main_window,
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
	get_settings( OFA_PDF_BALANCES( dialog ));

	init_account_selection( OFA_PDF_BALANCES( dialog ));
	init_date_selection( OFA_PDF_BALANCES( dialog ));
	init_others( OFA_PDF_BALANCES( dialog ));
}

static void
init_account_selection( ofaPDFBalances *self )
{
	ofaPDFBalancesPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *widget;

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
	priv->from_account_entry = widget;
	if( priv->from_account ){
		gtk_entry_set_text( GTK_ENTRY( widget ), priv->from_account );
	}

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
	priv->to_account_entry = widget;
	if( priv->to_account ){
		gtk_entry_set_text( GTK_ENTRY( widget ), priv->to_account );
	}

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "to-account-select" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_to_account_select ), self );
	priv->to_account_btn = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "all-accounts" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_all_accounts_toggled ), self );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), priv->all_accounts );
	on_all_accounts_toggled( GTK_TOGGLE_BUTTON( widget ), self );
}

static void
init_date_selection( ofaPDFBalances *self )
{
	ofaPDFBalancesPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *parent, *label;
	ofaDateFilterBin *bin;

	priv = self->priv;
	toplevel = my_window_get_toplevel( MY_WINDOW( self ));

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "date-filter" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	bin = ofa_date_filter_bin_new( st_pref_dates );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( bin ));

	label = ofa_date_filter_bin_get_frame_label( bin );
	gtk_label_set_text( GTK_LABEL( label ), _( "Effect date selection" ));

	g_signal_connect( G_OBJECT( bin ), "ofa-changed", G_CALLBACK( on_date_filter_changed ), self );

	priv->dates_filter = bin;
}

static void
init_others( ofaPDFBalances *self )
{
	ofaPDFBalancesPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *widget, *label, *button;
	GdkRGBA color;

	priv = self->priv;
	toplevel = my_window_get_toplevel( MY_WINDOW( self ));

	/* setup the new_page btn before the per_class one in order to be
	 * safely updated when setting the later preference */
	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "p3-new-page" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_new_page_toggled ), self );
	priv->new_page_btn = widget;

	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), priv->new_page );
	on_new_page_toggled( GTK_TOGGLE_BUTTON( widget ), self );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "p3-per-class" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_per_class_toggled ), self );
	priv->per_class_btn = widget;

	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), priv->per_class );
	on_per_class_toggled( GTK_TOGGLE_BUTTON( widget ), self );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "btn-ok" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	priv->btn_ok = button;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "message" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gdk_rgba_parse( &color, "#ff0000" );
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
	priv->msg_label = label;
}

static void
on_from_account_changed( GtkEntry *entry, ofaPDFBalances *self )
{
	on_account_changed( self, entry, self->priv->from_account_label, &self->priv->from_account );
}

static void
on_from_account_select( GtkButton *button, ofaPDFBalances *self )
{
	on_account_select( button, self, self->priv->from_account_entry );
}

static void
on_to_account_changed( GtkEntry *entry, ofaPDFBalances *self )
{
	on_account_changed( self, entry, self->priv->to_account_label, &self->priv->to_account );
}

static void
on_to_account_select( GtkButton *button, ofaPDFBalances *self )
{
	on_account_select( button, self, self->priv->to_account_entry );
}

static void
on_account_changed( ofaPDFBalances *self, GtkEntry *entry, GtkWidget *label, gchar **dest )
{
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;
	const gchar *cstr;
	ofoAccount *account;

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	cstr = gtk_entry_get_text( entry );
	account = ofo_account_get_by_number( dossier, cstr );
	if( account ){
		gtk_label_set_text( GTK_LABEL( label ), ofo_account_get_label( account ));
	} else {
		gtk_label_set_text( GTK_LABEL( label ), "" );
	}

	g_free( *dest );
	*dest = g_strdup( cstr );
}

static void
on_account_select( GtkButton *button, ofaPDFBalances *self, GtkWidget *entry )
{
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;
	gchar *number;

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	number = ofa_account_select_run(
						OFA_MAIN_WINDOW( main_window ),
						gtk_entry_get_text( GTK_ENTRY( entry )),
						ACCOUNT_ALLOW_DETAIL );
	if( number ){
		gtk_entry_set_text( GTK_ENTRY( entry ), number );
		g_free( number );
	}
}

static void
on_all_accounts_toggled( GtkToggleButton *button, ofaPDFBalances *self )
{
	ofaPDFBalancesPrivate *priv;
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

	priv->all_accounts = bvalue;
	g_debug( "on_all_accounts_toggled: settings all_accounts=%s", priv->all_accounts ? "True":"False" );
}

static void
on_per_class_toggled( GtkToggleButton *button, ofaPDFBalances *self )
{
	ofaPDFBalancesPrivate *priv;
	gboolean bvalue;

	priv = self->priv;

	bvalue = gtk_toggle_button_get_active( button );
	gtk_widget_set_sensitive( priv->new_page_btn, bvalue );

	priv->per_class = bvalue;
}

static void
on_new_page_toggled( GtkToggleButton *button, ofaPDFBalances *self )
{
	ofaPDFBalancesPrivate *priv;

	priv = self->priv;

	priv->new_page = gtk_toggle_button_get_active( button );
}

static void
on_date_filter_changed( ofaDateFilterBin *bin, gint who, gboolean empty, gboolean valid, ofaPDFBalances *self )
{
	check_for_validable_dlg( self );
}

/*
 * valid whatever be the accounts
 * valid if dates are empty or valid
 */
static void
check_for_validable_dlg( ofaPDFBalances *self )
{
	ofaPDFBalancesPrivate *priv;
	gboolean valid;

	priv = self->priv;
	valid = FALSE;

	if( priv->msg_label ){
		gtk_label_set_text( GTK_LABEL( priv->msg_label ), "" );

		valid = ( ofa_date_filter_bin_is_from_empty( priv->dates_filter ) ||
						ofa_date_filter_bin_is_from_valid( priv->dates_filter )) &&
				( ofa_date_filter_bin_is_to_empty( priv->dates_filter ) ||
						ofa_date_filter_bin_is_to_valid( priv->dates_filter ));

		if( !valid ){
			gtk_label_set_text( GTK_LABEL( priv->msg_label ), _( "Invalid effect dates selection" ));
		}
	}

	if( priv->btn_ok ){
		gtk_widget_set_sensitive( priv->btn_ok, valid );
	}
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
	ok = MY_DIALOG_CLASS( ofa_pdf_balances_parent_class )->quit_on_ok( dialog );

	if( ok ){
		ok &= do_apply( OFA_PDF_BALANCES( dialog ));
	}

	if( ok ){
		fname = ofa_pdf_dialog_get_filename( OFA_PDF_DIALOG( dialog ));
		ok &= ofa_iprintable_print_to_pdf( OFA_IPRINTABLE( dialog ), fname );
		g_free( fname );
	}

	return( ok );
}

/*
 * save parameters (all fields are optional)
 * then load the entries
 */
static gboolean
do_apply( ofaPDFBalances *self )
{
	ofaPDFBalancesPrivate *priv;

	priv = self->priv;

	set_settings( self );

	my_date_set_from_date( &priv->from_date, ofa_date_filter_bin_get_from( priv->dates_filter ));
	my_date_set_from_date( &priv->to_date, ofa_date_filter_bin_get_to( priv->dates_filter ));

	ofa_iprintable_set_group_on_new_page( OFA_IPRINTABLE( self ), priv->new_page );

	return( TRUE );
}

static GList *
iprintable_get_dataset( const ofaIPrintable *instance )
{
	ofaPDFBalancesPrivate *priv;
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;
	GList *dataset;

	priv = OFA_PDF_BALANCES( instance )->priv;

	main_window = my_window_get_main_window( MY_WINDOW( instance ));
	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), NULL );
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	dataset = ofo_entry_get_dataset_for_print_balance(
						dossier,
						priv->all_accounts ? NULL : priv->from_account,
						priv->all_accounts ? NULL : priv->to_account,
						my_date_is_valid( &priv->from_date ) ? &priv->from_date : NULL,
						my_date_is_valid( &priv->to_date ) ? &priv->to_date : NULL );

	priv->count = g_list_length( dataset );

	return( dataset );
}

static void
iprintable_free_dataset( GList *elements )
{
	ofs_account_balance_list_free( &elements );
}

static void
iprintable_reset_runtime( ofaIPrintable *instance )
{
	ofaPDFBalancesPrivate *priv;

	priv = OFA_PDF_BALANCES( instance )->priv;

	g_list_free_full( priv->totals, ( GDestroyNotify ) free_currency );
	priv->totals = NULL;
}

static void
iprintable_on_begin_print( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	static const gchar *thisfn = "ofa_pdf_balances_iprintable_on_begin_print";
	ofaPDFBalancesPrivate *priv;
	gdouble page_width;
	gint body_font_size;

	/*gint entries_count, other_count, last_count;
	gdouble footer_height, balance_height, last_height, header_height;*/

	g_debug( "%s: instance=%p, operation=%p, context=%p",
			thisfn, ( void * ) instance, ( void * ) operation, ( void * ) context );

	priv = OFA_PDF_BALANCES( instance )->priv;

	priv->page_margin = ofa_iprintable_get_page_margin( instance );
	body_font_size = ofa_iprintable_get_default_font_size( instance );
	page_width = gtk_print_context_get_width( context );

	/* starting from the left : body_number_ltab on the left margin */
	priv->body_number_ltab = priv->page_margin;
	priv->body_label_ltab = priv->body_number_ltab+ st_number_width + st_column_spacing;

	/* computing the width of amounts so that the four columns will
	 * take half of the page width
	 * margin+number+col+label+col+amount+col+amount+col+amount+col+amount+col+currency+margin */
	priv->amount_width = (page_width/2 - priv->page_margin)/4 - st_column_spacing;
	g_debug( "%s: amount_width=%lf", thisfn, priv->amount_width );
	/* amount_width=65 instead of 80 */
	priv->amount_width = 75;

	/* starting from the right */
	priv->body_currency_rtab = page_width - priv->page_margin;
	priv->body_credit_solde_rtab = priv->body_currency_rtab - st_currency_width - st_column_spacing;
	priv->body_debit_solde_rtab = priv->body_credit_solde_rtab - priv->amount_width - st_column_spacing;
	priv->body_credit_period_rtab = priv->body_debit_solde_rtab - priv->amount_width - st_column_spacing;
	priv->body_debit_period_rtab = priv->body_credit_period_rtab - priv->amount_width - st_column_spacing;

	/* max size in Pango units */
	priv->body_label_max_size = ( priv->body_debit_period_rtab - priv->amount_width - st_column_spacing - priv->body_label_ltab )*PANGO_SCALE;
}

/*
 * Accounts Balance
 */
static gchar *
iprintable_get_page_header_title( const ofaIPrintable *instance )
{
	return( g_strdup( st_page_header_title ));
}

/*
 * From account xxx to account xxx - From date xxx to date xxx
 */
static gchar *
iprintable_get_page_header_subtitle( const ofaIPrintable *instance )
{
	ofaPDFBalancesPrivate *priv;
	GString *stitle;
	gchar *sfrom_date, *sto_date;

	priv = OFA_PDF_BALANCES( instance )->priv;
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
iprintable_draw_page_header_notes( ofaIPrintable *instance,
		GtkPrintOperation *operation, GtkPrintContext *context, gint page_num )
{
	ofaPDFBalancesPrivate *priv;
	gdouble y, width, line_height;

	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	if( page_num == 0 ){

		priv = OFA_PDF_BALANCES( instance )->priv;

		y = ofa_iprintable_get_last_y( instance );
		line_height = ofa_iprintable_get_current_line_height( instance );
		width = context ? gtk_print_context_get_width( context ) : 0;

		ofa_iprintable_set_wrapped_text( instance, context,
					priv->page_margin, y,
					(width-priv->page_margin)*PANGO_SCALE,
					_( "Please note that this entries balance printing only "
						"displays the balance of the entries whose effect "
						"date is between the above date limits.\n"
						"As such, it is not intended to reflect the balances "
						"of the accounts." ), PANGO_ALIGN_LEFT );

		y += 2*line_height;
		ofa_iprintable_set_last_y( instance, y );
	}
}

static void
iprintable_draw_page_header_columns( ofaIPrintable *instance,
		GtkPrintOperation *operation, GtkPrintContext *context, gint page_num )
{
	ofaPDFBalancesPrivate *priv;
	cairo_t *cr;
	gdouble y, height, yh, hline, vspace;
	gint bfs;

	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	priv = OFA_PDF_BALANCES( instance )->priv;

	y = ofa_iprintable_get_last_y( instance );
	bfs = ofa_iprintable_get_current_font_size( instance );
	vspace = ofa_iprintable_get_current_line_vspace( instance );

	if( context ){
		cr = gtk_print_context_get_cairo_context( context );

		/* draw three vertical white lines to visually separate the amounts */
		cairo_set_source_rgb( cr, COLOR_WHITE );
		cairo_set_line_width( cr, 0.5 );
		height = ofa_iprintable_get_page_header_columns_height( instance );

		cairo_move_to( cr, priv->body_debit_period_rtab-priv->amount_width, y );
		cairo_line_to( cr, priv->body_debit_period_rtab-priv->amount_width, y+height );
		cairo_stroke( cr );

		cairo_move_to( cr, priv->body_credit_period_rtab+priv->page_margin, y );
		cairo_line_to( cr, priv->body_credit_period_rtab+priv->page_margin, y+height );
		cairo_stroke( cr );

		cairo_move_to( cr, priv->body_credit_solde_rtab+priv->page_margin, y );
		cairo_line_to( cr, priv->body_credit_solde_rtab+priv->page_margin, y+height );
		cairo_stroke( cr );

		yh = y+(height/2);
		cairo_move_to( cr, priv->body_debit_period_rtab-priv->amount_width, yh );
		cairo_line_to( cr, priv->body_credit_solde_rtab+priv->page_margin, yh );
		cairo_stroke( cr );
	}

	y += vspace;
	hline = bfs + vspace;

	ofa_iprintable_set_text( instance, context,
			priv->body_number_ltab, y+(hline+st_page_header_columns_vspace)/2,
			_( "Account" ), PANGO_ALIGN_LEFT );

	ofa_iprintable_set_text( instance, context,
			priv->body_label_ltab, y+(hline+st_page_header_columns_vspace)/2,
			_( "Label" ), PANGO_ALIGN_LEFT );

	ofa_iprintable_set_text( instance, context,
			priv->body_debit_period_rtab, y-1, _( "Period balance" ), PANGO_ALIGN_CENTER );

	ofa_iprintable_set_text( instance, context,
			priv->body_debit_solde_rtab, y-1, _( "Solde balance" ), PANGO_ALIGN_CENTER );

	y += hline + st_page_header_columns_vspace;

	ofa_iprintable_set_text( instance, context,
			priv->body_debit_period_rtab, y, _( "Debit" ), PANGO_ALIGN_RIGHT );

	ofa_iprintable_set_text( instance, context,
			priv->body_credit_period_rtab, y, _( "Credit" ), PANGO_ALIGN_RIGHT );

	ofa_iprintable_set_text( instance, context,
			priv->body_debit_solde_rtab, y, _( "Debit" ), PANGO_ALIGN_RIGHT );

	ofa_iprintable_set_text( instance, context,
			priv->body_credit_solde_rtab, y, _( "Credit" ), PANGO_ALIGN_RIGHT );

	y += hline;

	ofa_iprintable_set_last_y( instance, y );
}

/*
 * test if the current entry account is on the same class than the
 * previous one
 */
static gboolean
iprintable_is_new_group( const ofaIPrintable *instance, GList *current, GList *prev )
{
	ofaPDFBalancesPrivate *priv;
	ofsAccountBalance *current_sbal, *prev_sbal;
	gint current_class, prev_class;

	g_return_val_if_fail( current, FALSE );

	priv = OFA_PDF_BALANCES( instance )->priv;

	if( priv->per_class ){

		if( !prev ){
			return( TRUE );
		}

		current_sbal = ( ofsAccountBalance * ) current->data;
		current_class = ofo_account_get_class_from_number( current_sbal->account );

		prev_sbal = ( ofsAccountBalance * ) prev->data;
		prev_class = ofo_account_get_class_from_number( prev_sbal->account );

		return( current_class != prev_class );
	}

	return( FALSE );
}

/*
 * draw account header
 * Class x - xxx
 */
static void
iprintable_draw_group_header( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, GList *current )
{
	ofaPDFBalancesPrivate *priv;
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;
	ofsAccountBalance *sbal;
	gdouble y;
	gchar *str;

	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	main_window = my_window_get_main_window( MY_WINDOW( instance ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	priv = OFA_PDF_BALANCES( instance )->priv;

	y = ofa_iprintable_get_last_y( instance );

	/* setup the class properties */
	sbal = ( ofsAccountBalance * ) current->data;
	priv->class_num = ofo_account_get_class_from_number( sbal->account );

	priv->class_object = ofo_class_get_by_number( dossier, priv->class_num );

	g_list_free_full( priv->subtotals, ( GDestroyNotify ) free_currency );
	priv->subtotals = NULL;

	/* display the class header */
	/* label */
	str = g_strdup_printf(
				_( "Class %u - %s" ),
				priv->class_num, ofo_class_get_label( priv->class_object ));
	ofa_iprintable_set_text( instance, context, priv->page_margin, y, str, PANGO_ALIGN_LEFT );
	g_free( str );

	y += ofa_iprintable_get_current_line_height( instance );
	ofa_iprintable_set_last_y( instance, y );
}

static void
iprintable_draw_group_top_report( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	draw_subtotals_balance( instance, operation, context, _( "Top class report : " ));
}

/*
 * num_line is counted from 0 in the page
 *
 * (printable)width(A4)=559
 * date  journal  piece    label      debit   credit   solde
 * 10    6        max(10)  max(80)      15d      15d     15d
 */
static void
iprintable_draw_line( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, GList *current )
{
	ofaPDFBalancesPrivate *priv;
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;
	gdouble y;
	ofsAccountBalance *sbal;
	ofoAccount *account;
	gchar *str;
	gdouble solde;

	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	main_window = my_window_get_main_window( MY_WINDOW( instance ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	priv = OFA_PDF_BALANCES( instance )->priv;

	y = ofa_iprintable_get_last_y( instance );

	sbal = ( ofsAccountBalance * ) current->data;

	account = ofo_account_get_by_number( dossier, sbal->account );

	solde = 0;

	ofa_iprintable_set_text( instance, context,
			priv->body_number_ltab, y, sbal->account, PANGO_ALIGN_LEFT );

	ofa_iprintable_ellipsize_text( instance, context,
			priv->body_label_ltab, y,
			ofo_account_get_label( account ), priv->body_label_max_size );

	if( sbal->debit ){
		str = my_double_to_str( sbal->debit );
		ofa_iprintable_set_text( instance, context,
				priv->body_debit_period_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		solde -= sbal->debit;
	}

	if( sbal->credit ){
		str = my_double_to_str( sbal->credit );
		ofa_iprintable_set_text( instance, context,
				priv->body_credit_period_rtab, y, str, PANGO_ALIGN_RIGHT );
		solde += sbal->credit;
	}

	if( solde < 0 ){
		str = my_double_to_str( -1*solde );
		ofa_iprintable_set_text( instance, context,
				priv->body_debit_solde_rtab, y, str, PANGO_ALIGN_RIGHT );

	} else {
		str = my_double_to_str( solde );
		ofa_iprintable_set_text( instance, context,
				priv->body_credit_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
	}

	ofa_iprintable_set_text( instance, context,
			priv->body_currency_rtab, y, sbal->currency, PANGO_ALIGN_RIGHT );

	priv->subtotals = add_account_balance(
						OFA_PDF_BALANCES( instance ), priv->subtotals, sbal->currency, solde, sbal );

	priv->totals = add_account_balance(
						OFA_PDF_BALANCES( instance ), priv->totals, sbal->currency, solde, sbal );
}

static void
iprintable_draw_group_bottom_report( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	draw_subtotals_balance( instance, operation, context, _( "Bottom class report : " ));
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
	ofaPDFBalancesPrivate *priv;
	gchar *str;

	priv = OFA_PDF_BALANCES( instance )->priv;

	str = g_strdup_printf( _( "Class %d entries balance : "), priv->class_num );
	draw_subtotals_balance( instance, operation, context, str );
	g_free( str );
}

/*
 * draw on the bottom of the last page the summary with one line per
 * currency
 */
static void
iprintable_draw_bottom_summary( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	ofaPDFBalancesPrivate *priv;
	gdouble bottom, top, vspace, req_height;
	gint bfs;

	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	priv = OFA_PDF_BALANCES( instance )->priv;

	if( !priv->count ){
		ofa_iprintable_draw_no_data( instance, context );
		return;
	}

	/* bottom of the rectangle */
	bottom = ofa_iprintable_get_max_y( instance );

	/* top of the rectangle */
	bfs = ofa_iprintable_get_current_font_size( instance );
	vspace = ofa_iprintable_get_current_line_vspace( instance );
	req_height = vspace + g_list_length( priv->totals )*(bfs+vspace );
	top = bottom - req_height;

	ofa_iprintable_draw_rect( instance, context, 0, top, -1, req_height );

	top += vspace;

	draw_account_balance( instance, context, priv->totals, top, _( "General balance : " ));

	ofa_iprintable_set_last_y( instance, ofa_iprintable_get_last_y( instance ) + req_height );
}

static void
draw_subtotals_balance( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, const gchar *title )
{
	ofaPDFBalancesPrivate *priv;
	gdouble vspace, req_height, last_y;
	gint bfs;

	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	priv = OFA_PDF_BALANCES( instance )->priv;

	/* top of the rectangle */
	bfs = ofa_iprintable_get_current_font_size( instance );
	vspace = ofa_iprintable_get_current_line_vspace( instance );
	req_height = g_list_length( priv->subtotals )*(bfs+vspace );
	last_y = ofa_iprintable_get_last_y( instance );

	draw_account_balance( instance, context, priv->subtotals, last_y, title );

	ofa_iprintable_set_last_y( instance, last_y+req_height );
}

static void
draw_account_balance( ofaIPrintable *instance, GtkPrintContext *context,
							GList *list, gdouble top, const gchar *title )
{
	ofaPDFBalancesPrivate *priv;
	GList *it;
	gboolean first;
	sCurrency *scur;
	gchar *str;

	priv = OFA_PDF_BALANCES( instance )->priv;

	for( it=list, first=TRUE ; it ; it=it->next ){
		if( first ){
			ofa_iprintable_set_text( instance, context,
						priv->body_debit_period_rtab-priv->amount_width, top,
						title, PANGO_ALIGN_RIGHT );
			first = FALSE;
			}

		scur = ( sCurrency * ) it->data;

		str = my_double_to_str( scur->period_d );
		ofa_iprintable_set_text( instance, context,
				priv->body_debit_period_rtab, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = my_double_to_str( scur->period_c );
		ofa_iprintable_set_text( instance, context,
				priv->body_credit_period_rtab, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = my_double_to_str( scur->solde_d );
		ofa_iprintable_set_text( instance, context,
				priv->body_debit_solde_rtab, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		str = my_double_to_str( scur->solde_c );
		ofa_iprintable_set_text( instance, context,
				priv->body_credit_solde_rtab, top, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		ofa_iprintable_set_text( instance, context,
				priv->body_currency_rtab, top, scur->currency, PANGO_ALIGN_RIGHT );

		top += ofa_iprintable_get_current_line_height( instance );
	}
}

static GList *
add_account_balance( ofaPDFBalances *self, GList *list,
							const gchar *currency, gdouble solde, ofsAccountBalance *sbal )
{
	static const gchar *thisfn = "ofa_pdf_balances_add_account_balance";
	GList *it;
	sCurrency *scur;
	gboolean found;

	for( it=list, found=FALSE ; it ; it=it->next ){
		scur = ( sCurrency * ) it->data;
		if( !g_utf8_collate( scur->currency, currency )){
			found = TRUE;
			break;
		}
	}

	if( !found ){
		scur = g_new0( sCurrency, 1 );
		scur->currency = g_strdup( currency );
		g_debug( "%s: inserting new %s currency", thisfn, scur->currency );
		list = g_list_insert_sorted( list, scur, ( GCompareFunc ) cmp_currencies );
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

	return( list );
}

static gint
cmp_currencies( const sCurrency *a, const sCurrency *b )
{
	return( g_utf8_collate( a->currency, b->currency ));
}

/*
 * settings are:
 * from_account;to_account;all_accounts;per_class;new_page;
 */
static void
get_settings( ofaPDFBalances *self )
{
	ofaPDFBalancesPrivate *priv;
	GList *slist, *it;
	const gchar *cstr;

	priv = self->priv;

	slist = ofa_settings_get_string_list( st_pref_settings );

	it = slist;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->from_account = g_strdup( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->to_account = g_strdup( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->all_accounts = my_utils_boolean_from_str( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->per_class = my_utils_boolean_from_str( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->new_page = my_utils_boolean_from_str( cstr );
	}

	ofa_settings_free_string_list( slist );
}

static void
set_settings( ofaPDFBalances *self )
{
	ofaPDFBalancesPrivate *priv;
	gchar *str;

	priv = self->priv;

	str = g_strdup_printf( "%s;%s;%s;%s;%s;",
			priv->from_account ? priv->from_account : "",
			priv->to_account ? priv->to_account : "",
			priv->all_accounts ? "True":"False",
			priv->per_class ? "True":"False",
			priv->new_page ? "True":"False" );

	ofa_settings_set_string( st_pref_settings, str );

	g_free( str );
}
