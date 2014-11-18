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

#include "core/my-window-prot.h"

#include "ui/my-editable-date.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-iprintable.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-print-balance.h"

/* private instance data
 */
struct _ofaPrintBalancePrivate {

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

	/* internals
	 */
	gchar         *from_account;
	gchar         *to_account;
	gboolean       all_accounts;
	GDate          from_date;
	GDate          to_date;

	GList         *totals;

	/* print datas
	 */
	gdouble        page_margin;
	gdouble        amount_width;
	gdouble        body_number_ltab;
	gdouble        body_label_ltab;
	gint           body_label_max_size;		/* Pango units */
	gdouble        body_debit_period_rtab;
	gdouble        body_credit_period_rtab;
	gdouble        body_debit_solde_rtab;
	gdouble        body_credit_solde_rtab;
	gdouble        body_currency_rtab;
};

typedef struct {
	gchar  *currency;
	gdouble period_d;
	gdouble period_c;
	gdouble solde_d;
	gdouble solde_c;
}
	sCurrency;

static const gchar *st_ui_xml            = PKGUIDIR "/ofa-print-balance.ui";
static const gchar *st_ui_id             = "AccountsBalanceDlg";

static const gchar *st_pref_fname        = "PrintBalanceFilename";
static const gchar *st_pref_from_account = "PrintBalanceFromAccount";
static const gchar *st_pref_to_account   = "PrintBalanceToAccount";
static const gchar *st_pref_all_accounts = "PrintBalanceAllAccounts";
static const gchar *st_pref_from_date    = "PrintBalanceFromDate";
static const gchar *st_pref_to_date      = "PrintBalanceToDate";

static const gchar *st_def_fname         = "AccountsBalance";
static const gchar *st_page_header_title = N_( "Accounts Balance Summary" );

/* makes use of the same font family for all fields */
/* the space between body lines */
#define              st_page_header_columns_vspace     (gdouble) 2

/* the columns of the body */
#define st_number_width                                (gdouble) 50/9*body_font_size
#define st_currency_width                              (gdouble) 23/9*body_font_size
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

static void         iprintable_iface_init( ofaIPrintableInterface *iface );
static guint        iprintable_get_interface_version( const ofaIPrintable *instance );
static void         v_init_dialog( myDialog *dialog );
static void         init_filechooser( ofaPrintBalance *self );
static void         init_account_selection( ofaPrintBalance *self );
static void         init_date_selection( ofaPrintBalance *self );
static void         on_from_account_changed( GtkEntry *entry, ofaPrintBalance *self );
static void         on_from_account_select( GtkButton *button, ofaPrintBalance *self );
static void         on_to_account_changed( GtkEntry *entry, ofaPrintBalance *self );
static void         on_to_account_select( GtkButton *button, ofaPrintBalance *self );
static void         on_account_changed( GtkEntry *entry, ofaPrintBalance *self, GtkWidget *label );
static void         on_account_select( GtkButton *button, ofaPrintBalance *self, GtkWidget *entry );
static void         on_all_accounts_toggled( GtkToggleButton *button, ofaPrintBalance *self );
static gboolean     v_quit_on_ok( myDialog *dialog );
static gboolean     do_apply( ofaPrintBalance *self );
static GList       *iprintable_get_dataset( const ofaIPrintable *instance );
static void         iprintable_on_begin_print( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static gchar       *iprintable_get_page_header_title( const ofaIPrintable *instance );
static gchar       *iprintable_get_page_header_subtitle( const ofaIPrintable *instance );
static void         iprintable_draw_page_header_columns( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static void         iprintable_draw_line( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, GList *current );
static gboolean     iprintable_draw_bottom_summary( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static void         add_account_balance( ofaPrintBalance *self, const gchar *currency, gdouble solde, ofsAccountBalance *sbal );
static gint         cmp_currencies( const sCurrency *a, const sCurrency *b );

G_DEFINE_TYPE_EXTENDED( ofaPrintBalance, ofa_print_balance, MY_TYPE_DIALOG, 0, \
		G_IMPLEMENT_INTERFACE (OFA_TYPE_IPRINTABLE, iprintable_iface_init ));

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
	g_return_if_fail( instance && OFA_IS_PRINT_BALANCE( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_print_balance_parent_class )->dispose( instance );
}

static void
ofa_print_balance_init( ofaPrintBalance *self )
{
	static const gchar *thisfn = "ofa_print_balance_instance_init";
	ofaPrintBalancePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PRINT_BALANCE( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_PRINT_BALANCE, ofaPrintBalancePrivate );
	priv = self->priv;

	priv->printed = FALSE;
	my_date_clear( &priv->from_date );
	my_date_clear( &priv->to_date );
}

static void
ofa_print_balance_class_init( ofaPrintBalanceClass *klass )
{
	static const gchar *thisfn = "ofa_print_balance_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = print_balance_dispose;
	G_OBJECT_CLASS( klass )->finalize = print_balance_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaPrintBalancePrivate ));
}

static void
iprintable_iface_init( ofaIPrintableInterface *iface )
{
	static const gchar *thisfn = "ofa_print_balance_iprintable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iprintable_get_interface_version;
	iface->get_dataset = iprintable_get_dataset;
	iface->on_begin_print = iprintable_on_begin_print;
	iface->get_page_header_title = iprintable_get_page_header_title;
	iface->get_page_header_subtitle = iprintable_get_page_header_subtitle;
	iface->draw_page_header_columns = iprintable_draw_page_header_columns;
	iface->draw_line = iprintable_draw_line;
	iface->draw_bottom_summary = iprintable_draw_bottom_summary;
}

static guint
iprintable_get_interface_version( const ofaIPrintable *instance )
{
	return( 1 );
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

	self = g_object_new(
				OFA_TYPE_PRINT_BALANCE,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	my_dialog_run_dialog( MY_DIALOG( self ));

	printed = self->priv->printed;
	g_object_unref( self );

	return( printed );
}

static void
v_init_dialog( myDialog *dialog )
{
	init_filechooser( OFA_PRINT_BALANCE( dialog ));
	init_account_selection( OFA_PRINT_BALANCE( dialog ));
	init_date_selection( OFA_PRINT_BALANCE( dialog ));
}

static void
init_filechooser( ofaPrintBalance *self )
{
	gchar *text;

	ofa_iprintable_init_dialog( OFA_IPRINTABLE( self ));

	ofa_iprintable_set_default_filename( OFA_IPRINTABLE( self ), st_def_fname );

	text = ofa_settings_get_string( st_pref_fname );
	ofa_iprintable_set_last_filename( OFA_IPRINTABLE( self ), text );
	g_free( text );
}

static void
init_account_selection( ofaPrintBalance *self )
{
	ofaPrintBalancePrivate *priv;
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
}

static void
init_date_selection( ofaPrintBalance *self )
{
	ofaPrintBalancePrivate *priv;
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
	const gchar *str;
	ofoAccount *account;

	str = gtk_entry_get_text( entry );
	account = ofo_account_get_by_number(
					ofa_main_window_get_dossier( MY_WINDOW( self )->prot->main_window ), str );
	if( account ){
		gtk_label_set_text( GTK_LABEL( label ), ofo_account_get_label( account ));
	} else {
		gtk_label_set_text( GTK_LABEL( label ), "" );
	}
}

static void
on_account_select( GtkButton *button, ofaPrintBalance *self, GtkWidget *entry )
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
 * all fields are optional
 */
static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_apply( OFA_PRINT_BALANCE( dialog )) &&
				ofa_iprintable_apply( OFA_IPRINTABLE( dialog )));
}

/*
 * save parameters (all fields are optional)
 * then load the entries
 */
static gboolean
do_apply( ofaPrintBalance *self )
{
	ofaPrintBalancePrivate *priv;
	gboolean all_accounts;
	const gchar *filename;
	gchar *text;

	priv = self->priv;

	/* the export filename is the only mandatory argument */
	filename = ofa_iprintable_get_filename( OFA_IPRINTABLE( self ));
	if( !filename || !g_utf8_strlen( filename, -1 )){
		return( FALSE );
	}
	ofa_settings_set_string( st_pref_fname, filename );

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

	return( TRUE );
}

static GList *
iprintable_get_dataset( const ofaIPrintable *instance )
{
	ofaPrintBalancePrivate *priv;

	priv = OFA_PRINT_BALANCE( instance )->priv;

	return( ofo_entry_get_dataset_for_print_balance(
						ofa_main_window_get_dossier( MY_WINDOW( instance )->prot->main_window ),
						priv->from_account, priv->to_account,
						&priv->from_date, &priv->to_date ));
}

static void
iprintable_on_begin_print( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	static const gchar *thisfn = "ofa_print_balance_iprintable_on_begin_print";
	ofaPrintBalancePrivate *priv;
	gdouble page_width;
	gint body_font_size;

	/*gint entries_count, other_count, last_count;
	gdouble footer_height, balance_height, last_height, header_height;*/

	g_debug( "%s: instance=%p, operation=%p, context=%p",
			thisfn, ( void * ) instance, ( void * ) operation, ( void * ) context );

	priv = OFA_PRINT_BALANCE( instance )->priv;
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
	ofaPrintBalancePrivate *priv;
	GString *stitle;
	gchar *sfrom_date, *sto_date;

	priv = OFA_PRINT_BALANCE( instance )->priv;
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
	ofaPrintBalancePrivate *priv;
	gboolean is_drawing;
	cairo_t *cr;
	gdouble y, height, yh, hline, vspace;
	gint bfs;

	priv = OFA_PRINT_BALANCE( instance )->priv;

	y = ofa_iprintable_get_last_y( instance );
	bfs = ofa_iprintable_get_current_font_size( instance );
	vspace = ofa_iprintable_get_current_line_vspace( instance );

	is_drawing = ( operation ?
			gtk_print_operation_get_status( operation ) == GTK_PRINT_STATUS_GENERATING_DATA :
			FALSE );

	if( is_drawing ){
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

	if( is_drawing ){
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
	}

	y += hline + st_page_header_columns_vspace;

	if( is_drawing ){
		ofa_iprintable_set_text( instance, context,
				priv->body_debit_period_rtab, y, _( "Debit" ), PANGO_ALIGN_RIGHT );

		ofa_iprintable_set_text( instance, context,
				priv->body_credit_period_rtab, y, _( "Credit" ), PANGO_ALIGN_RIGHT );

		ofa_iprintable_set_text( instance, context,
				priv->body_debit_solde_rtab, y, _( "Debit" ), PANGO_ALIGN_RIGHT );

		ofa_iprintable_set_text( instance, context,
				priv->body_credit_solde_rtab, y, _( "Credit" ), PANGO_ALIGN_RIGHT );
	}

	y += hline;

	ofa_iprintable_set_last_y( instance, y );
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
	ofaPrintBalancePrivate *priv;
	gdouble y;
	gboolean is_drawing;
	ofsAccountBalance *sbal;
	ofoAccount *account;
	gchar *str;
	gdouble solde;

	priv = OFA_PRINT_BALANCE( instance )->priv;

	y = ofa_iprintable_get_last_y( instance );

	is_drawing = ( operation ?
			gtk_print_operation_get_status( operation ) == GTK_PRINT_STATUS_GENERATING_DATA :
			FALSE );

	sbal = ( ofsAccountBalance * ) current->data;

	account = ofo_account_get_by_number(
					ofa_main_window_get_dossier( MY_WINDOW( instance )->prot->main_window ),
					sbal->account );

	solde = 0;

	if( is_drawing ){
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
		}

		if( solde > 0 ){
			str = my_double_to_str( solde );
			ofa_iprintable_set_text( instance, context,
					priv->body_credit_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
		}

		ofa_iprintable_set_text( instance, context,
				priv->body_currency_rtab, y, sbal->currency, PANGO_ALIGN_RIGHT );
	}

	add_account_balance( OFA_PRINT_BALANCE( instance ), sbal->currency, solde, sbal );
}

/*
 * draw on the bottom of the last page the summary with one line per
 * currency
 */
static gboolean
iprintable_draw_bottom_summary( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	ofaPrintBalancePrivate *priv;
	gboolean is_drawing, first;
	gdouble bottom, top, vspace, req_height, last_y;
	gint bfs;
	GList *it;
	gchar *str;
	sCurrency *scur;

	priv = OFA_PRINT_BALANCE( instance )->priv;

	is_drawing = ( operation ?
			gtk_print_operation_get_status( operation ) == GTK_PRINT_STATUS_GENERATING_DATA :
			FALSE );

	/* bottom of the rectangle */
	bottom = ofa_iprintable_get_max_y( instance );

	/* top of the rectangle */
	bfs = ofa_iprintable_get_current_font_size( instance );
	vspace = ofa_iprintable_get_current_line_vspace( instance );
	req_height = vspace + g_list_length( priv->totals )*(bfs+vspace );
	top = bottom - req_height;

	last_y = ofa_iprintable_get_last_y( instance );
	if( last_y + req_height > bottom ){
		return( FALSE );
	}

	if( is_drawing ){
		ofa_iprintable_draw_rect( instance, context, 0, top, -1, req_height );
	}

	top += vspace;

	for( it=priv->totals, first=TRUE ; it ; it=it->next ){
		if( first ){
			ofa_iprintable_set_text( instance, context,
						priv->body_debit_period_rtab-priv->amount_width, top,
						_( "General balance : " ), PANGO_ALIGN_RIGHT );
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

		top += bfs + vspace;
	}

	return( TRUE );
}

static void
add_account_balance( ofaPrintBalance *self,
							const gchar *currency, gdouble solde, ofsAccountBalance *sbal )
{
	static const gchar *thisfn = "ofa_print_balance_add_account_balance";
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
		g_debug( "%s: adding %s currency", thisfn, scur->currency );
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
