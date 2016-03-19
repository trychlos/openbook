/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include "my/my-date.h"
#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-base.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "core/ofa-account-properties.h"
#include "core/ofa-currency-combo.h"
#include "core/ofa-main-window.h"

/* private instance data
 */
struct _ofaAccountPropertiesPrivate {
	gboolean             dispose_has_run;

	/* initialization
	 */
	ofoAccount          *account;

	/* runtime data
	 */
	ofaHub              *hub;
	ofoDossier          *dossier;
	gboolean             is_current;
	gboolean             is_new;
	gboolean             number_ok;
	gboolean             has_entries;
	gboolean             balances_displayed;

	/* UI
	 */
	GtkWidget           *number_entry;
	GtkWidget           *label_entry;
	GtkWidget           *closed_btn;
	GtkWidget           *type_frame;
	GtkWidget           *root_btn;
	GtkWidget           *detail_btn;
	GtkWidget           *p1_exe_frame;
	GtkWidget           *settleable_btn;
	GtkWidget           *reconciliable_btn;
	GtkWidget           *forward_btn;
	GtkWidget           *currency_etiq;
	GtkWidget           *currency_parent;
	GtkWidget           *currency_combo;
	GtkWidget           *msg_label;
	GtkWidget           *ok_btn;

	/* account data
	 */
	gchar               *number;
	gchar               *label;
	gchar               *currency;
	ofoCurrency         *cur_object;
	gint                 cur_digits;
	const gchar         *cur_symbol;
	gboolean             root;
	gchar               *upd_user;
	GTimeVal             upd_stamp;
};

typedef gdouble       ( *fnGetDouble )( const ofoAccount * );
typedef gint          ( *fnGetInt )   ( const ofoAccount * );
typedef const GDate * ( *fnGetDate )  ( const ofoAccount * );

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-account-properties.ui";

static void      iwindow_iface_init( myIWindowInterface *iface );
static gchar    *iwindow_get_identifier( const myIWindow *instance );
static void      idialog_iface_init( myIDialogInterface *iface );
static void      idialog_init( myIDialog *instance );
static void      init_ui( ofaAccountProperties *dialog );
static void      remove_balances_page( ofaAccountProperties *self );
static void      init_balances_page( ofaAccountProperties *self );
static void      set_amount( ofaAccountProperties *self, gdouble amount, const gchar *wname, const gchar *wname_cur );
static void      on_number_changed( GtkEntry *entry, ofaAccountProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaAccountProperties *self );
static void      on_currency_changed( ofaCurrencyCombo *combo, const gchar *code, ofaAccountProperties *self );
static void      on_root_toggled( GtkRadioButton *btn, ofaAccountProperties *self );
static void      on_detail_toggled( GtkRadioButton *btn, ofaAccountProperties *self );
static void      on_type_toggled( GtkRadioButton *btn, ofaAccountProperties *self, gboolean root );
static void      check_for_enable_dlg( ofaAccountProperties *self );
static gboolean  is_dialog_validable( ofaAccountProperties *self );
static gboolean  do_update( ofaAccountProperties *self, gchar **msgerr );
static void      set_msgerr( ofaAccountProperties *self, const gchar *msg );

G_DEFINE_TYPE_EXTENDED( ofaAccountProperties, ofa_account_properties, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaAccountProperties )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
account_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_properties_finalize";
	ofaAccountPropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_PROPERTIES( instance ));

	/* free data members here */
	priv = ofa_account_properties_get_instance_private( OFA_ACCOUNT_PROPERTIES( instance ));
	g_free( priv->number );
	g_free( priv->label );
	g_free( priv->currency );
	g_free( priv->upd_user );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_properties_parent_class )->finalize( instance );
}

static void
account_properties_dispose( GObject *instance )
{
	ofaAccountPropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_PROPERTIES( instance ));

	priv = ofa_account_properties_get_instance_private( OFA_ACCOUNT_PROPERTIES( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_properties_parent_class )->dispose( instance );
}

static void
ofa_account_properties_init( ofaAccountProperties *self )
{
	static const gchar *thisfn = "ofa_account_properties_init";
	ofaAccountPropertiesPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ACCOUNT_PROPERTIES( self ));

	priv = ofa_account_properties_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->account = NULL;
	priv->is_new = FALSE;
	priv->balances_displayed = TRUE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_account_properties_class_init( ofaAccountPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_account_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_properties_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_account_properties_run:
 * @main_window: the main window of the application.
 * @parent: [allow-none]: the #GtkWindow parent of this dialog.
 * @account: the account.
 *
 * Update the properties of an account.
 */
void
ofa_account_properties_run( ofaMainWindow *main_window, GtkWindow *parent, ofoAccount *account )
{
	static const gchar *thisfn = "ofa_account_properties_run";
	ofaAccountProperties *self;
	ofaAccountPropertiesPrivate *priv;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	g_return_if_fail( account && OFO_IS_ACCOUNT( account ));

	g_debug( "%s: main_window=%p, parent=%p, account=%p",
			thisfn, ( void * ) main_window, ( void * ) parent, ( void * ) account );

	self = g_object_new( OFA_TYPE_ACCOUNT_PROPERTIES, NULL );
	my_iwindow_set_main_window( MY_IWINDOW( self ), GTK_APPLICATION_WINDOW( main_window ));
	my_iwindow_set_parent( MY_IWINDOW( self ), parent );
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

	priv = ofa_account_properties_get_instance_private( self );
	priv->account = account;

	/* run modal or non-modal depending of the parent */
	my_idialog_run_maybe_modal( MY_IDIALOG( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_account_properties_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_identifier = iwindow_get_identifier;
}

/*
 * identifier is built with class name and template mnemo
 */
static gchar *
iwindow_get_identifier( const myIWindow *instance )
{
	ofaAccountPropertiesPrivate *priv;
	gchar *id;

	priv = ofa_account_properties_get_instance_private( OFA_ACCOUNT_PROPERTIES( instance ));

	id = g_strdup_printf( "%s-%s",
				G_OBJECT_TYPE_NAME( instance ),
				ofo_account_get_number( priv->account ));

	return( id );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_account_properties_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

/*
 * this dialog is subject to 'is_current' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * account
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_account_properties_idialog_init";
	ofaAccountPropertiesPrivate *priv;
	GtkApplicationWindow *main_window;
	gchar *title;
	const gchar *acc_number;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_account_properties_get_instance_private( OFA_ACCOUNT_PROPERTIES( instance ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	my_idialog_click_to_update( instance, priv->ok_btn, ( myIDialogUpdateCb ) do_update );

	main_window = my_iwindow_get_main_window( MY_IWINDOW( instance ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv->hub = ofa_main_window_get_hub( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	/* dialog title */
	acc_number = ofo_account_get_number( priv->account );
	if( !acc_number ){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new account" ));
	} else {
		priv->is_new = FALSE;
		title = g_strdup_printf( _( "Updating account %s" ), acc_number );
	}
	gtk_window_set_title( GTK_WINDOW( instance ), title );

	/* dossier */
	priv->dossier = ofa_hub_get_dossier( priv->hub );
	g_return_if_fail( priv->dossier && OFO_IS_DOSSIER( priv->dossier ));
	priv->is_current = ofo_dossier_is_current( priv->dossier );

	/* account */
	priv->has_entries = ofo_entry_use_account( priv->hub, acc_number );
	g_debug( "%s: has_entries=%s", thisfn, priv->has_entries ? "True":"False" );
	priv->number = g_strdup( acc_number );
	priv->label = g_strdup( ofo_account_get_label( priv->account ));
	priv->root = ofo_account_is_root( priv->account );
	priv->currency = g_strdup( ofo_account_get_currency( priv->account ));

	init_ui( OFA_ACCOUNT_PROPERTIES( instance ));

	/* account number
	 * set read-only if not empty though we would be able to remediate
	 * to all impacted records  */
	if( priv->number ){
		gtk_entry_set_text( GTK_ENTRY( priv->number_entry ), priv->number );
	}

	/* account label */
	if( priv->label ){
		gtk_entry_set_text( GTK_ENTRY( priv->label_entry ), priv->label );
	}

	/* whether the account is closed */
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON( priv->closed_btn ), ofo_account_is_closed( priv->account ));

	/* type of account */
	if( priv->root ){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->root_btn ), TRUE );
		on_root_toggled( GTK_RADIO_BUTTON( priv->root_btn ), OFA_ACCOUNT_PROPERTIES( instance ));
	} else {
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->detail_btn ), TRUE );
		on_detail_toggled( GTK_RADIO_BUTTON( priv->detail_btn ), OFA_ACCOUNT_PROPERTIES( instance ));
	}

	/* when closing exercice */
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON( priv->settleable_btn ), ofo_account_is_settleable( priv->account ));

	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON( priv->reconciliable_btn ), ofo_account_is_reconciliable( priv->account ));

	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON( priv->forward_btn ), ofo_account_is_forwardable( priv->account ));

	/* account currency
	 * set read-only if has entries */
	if( my_strlen( priv->currency )){
		ofa_currency_combo_set_selected(
				OFA_CURRENCY_COMBO( priv->currency_combo ), priv->currency );
	}

	if( ofo_account_is_root( priv->account )){
		remove_balances_page( OFA_ACCOUNT_PROPERTIES( instance ));
	} else {
		init_balances_page( OFA_ACCOUNT_PROPERTIES( instance ));
	}

	my_utils_container_notes_init( GTK_CONTAINER( instance ), account );
	my_utils_container_updstamp_init( GTK_CONTAINER( instance ), account );
	my_utils_container_set_editable( GTK_CONTAINER( instance ), priv->is_current );

	/* setup fields editability, depending of
	 * - whether the dossier is current
	 * - whether account has entries or is empty
	 */
	my_utils_widget_set_editable( priv->number_entry, priv->is_current && !priv->has_entries );
	my_utils_widget_set_editable( priv->root_btn, priv->is_current && !priv->has_entries );
	my_utils_widget_set_editable( priv->detail_btn, priv->is_current && !priv->has_entries );
	my_utils_widget_set_editable( priv->currency_combo, priv->is_current && !priv->has_entries );

	if( !priv->is_current ){
		my_idialog_set_close_button( instance );
		priv->ok_btn = NULL;
	}

	gtk_widget_show_all( GTK_WIDGET( instance ));
}

/*
 * static initialization, i.e. which does not depend of current status
 */
static void
init_ui( ofaAccountProperties *dialog )
{
	ofaAccountPropertiesPrivate *priv;
	ofaCurrencyCombo *combo;
	GtkWidget *label;

	priv = ofa_account_properties_get_instance_private( dialog );

	/* account number */
	priv->number_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "p1-number" );
	g_return_if_fail( priv->number_entry && GTK_IS_ENTRY( priv->number_entry ));
	g_signal_connect(
			G_OBJECT( priv->number_entry ), "changed", G_CALLBACK( on_number_changed ), dialog );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "p1-account-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->number_entry ));

	/* account label */
	priv->label_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "p1-label" );
	g_return_if_fail( priv->label_entry && GTK_IS_ENTRY( priv->label_entry ));
	g_signal_connect(
			G_OBJECT( priv->label_entry ), "changed", G_CALLBACK( on_label_changed ), dialog );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "p1-label-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->label_entry ));

	/* closed account */
	priv->closed_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "p1-closed" );

	/* account type */
	priv->type_frame = my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "p1-type-frame" );

	priv->root_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "p1-root-account" );
	g_return_if_fail( priv->root_btn && GTK_IS_RADIO_BUTTON( priv->root_btn ));
	g_signal_connect(
			G_OBJECT( priv->root_btn ), "toggled", G_CALLBACK( on_root_toggled ), dialog );

	priv->detail_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "p1-detail-account" );
	g_return_if_fail( priv->detail_btn && GTK_IS_RADIO_BUTTON( priv->detail_btn ));
	g_signal_connect(
			G_OBJECT( priv->detail_btn ), "toggled", G_CALLBACK( on_detail_toggled ), dialog );

	/* account behavior when closing exercice */
	priv->p1_exe_frame = my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "p1-exe-frame" );

	priv->settleable_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "p1-settleable" );
	g_return_if_fail( priv->settleable_btn && GTK_IS_TOGGLE_BUTTON( priv->settleable_btn ));

	priv->reconciliable_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "p1-reconciliable" );
	g_return_if_fail( priv->reconciliable_btn && GTK_IS_TOGGLE_BUTTON( priv->reconciliable_btn ));

	priv->forward_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "p1-forward" );
	g_return_if_fail( priv->forward_btn && GTK_IS_TOGGLE_BUTTON( priv->forward_btn ));

	/* currency */
	combo = ofa_currency_combo_new();
	priv->currency_parent = my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "p1-currency-parent" );
	g_return_if_fail( priv->currency_parent && GTK_IS_CONTAINER( priv->currency_parent ));
	gtk_container_add( GTK_CONTAINER( priv->currency_parent ), GTK_WIDGET( combo ));
	ofa_currency_combo_set_columns( combo, CURRENCY_DISP_CODE );
	ofa_currency_combo_set_hub( combo, priv->hub );
	g_signal_connect( G_OBJECT( combo ), "ofa-changed", G_CALLBACK( on_currency_changed ), dialog );
	priv->currency_etiq = my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "p1-currency-label" );
	g_return_if_fail( priv->currency_etiq && GTK_IS_LABEL( priv->currency_etiq ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( priv->currency_etiq ), GTK_WIDGET( combo ));
	priv->currency_combo = GTK_WIDGET( combo );
}

/*
 * no need to display a balance page for root accounts
 */
static void
remove_balances_page( ofaAccountProperties *self )
{
	ofaAccountPropertiesPrivate *priv;
	GtkWidget *book, *page_w;
	gint page_n;

	priv = ofa_account_properties_get_instance_private( self );

	book = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "properties-book" );
	g_return_if_fail( book && GTK_IS_NOTEBOOK( book ));

	page_w = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "balance-grid" );
	page_n = gtk_notebook_page_num( GTK_NOTEBOOK( book ), page_w );
	gtk_notebook_remove_page( GTK_NOTEBOOK( book ), page_n );

	priv->balances_displayed = FALSE;
}

static void
init_balances_page( ofaAccountProperties *self )
{
	ofaAccountPropertiesPrivate *priv;

	priv = ofa_account_properties_get_instance_private( self );

	set_amount( self,
			ofo_account_get_open_debit( priv->account ),
			"p2-open-debit", "p2-open-debit-cur" );
	set_amount( self,
			ofo_account_get_open_credit( priv->account ),
			"p2-open-credit", "p2-open-credit-cur" );

	set_amount( self,
			ofo_account_get_val_debit( priv->account ),
			"p2-val-debit", "p2-val-debit-cur" );
	set_amount( self,
			ofo_account_get_val_credit( priv->account ),
			"p2-val-credit", "p2-val-credit-cur" );

	set_amount( self,
			ofo_account_get_rough_debit( priv->account ),
			"p2-rough-debit", "p2-rough-debit-cur" );
	set_amount( self,
			ofo_account_get_rough_credit( priv->account ),
			"p2-rough-credit", "p2-rough-credit-cur" );

	set_amount( self,
			ofo_account_get_futur_debit( priv->account ),
			"p2-futur-debit", "p2-fut-debit-cur" );
	set_amount( self,
			ofo_account_get_futur_credit( priv->account ),
			"p2-futur-credit", "p2-fut-credit-cur" );
}

static void
set_amount( ofaAccountProperties *self, gdouble amount, const gchar *wname, const gchar *wname_cur )
{
	ofaAccountPropertiesPrivate *priv;
	GtkLabel *label;
	gchar *str;

	priv = ofa_account_properties_get_instance_private( self );

	label = GTK_LABEL( my_utils_container_get_child_by_name( GTK_CONTAINER( self ), wname ));
	str = ofa_amount_to_str( amount, priv->cur_object );
	gtk_label_set_text( label, str );
	g_free( str );

	label = GTK_LABEL( my_utils_container_get_child_by_name( GTK_CONTAINER( self ), wname_cur ));
	gtk_label_set_text( label, priv->cur_symbol );
}

static void
on_number_changed( GtkEntry *entry, ofaAccountProperties *self )
{
	ofaAccountPropertiesPrivate *priv;

	priv = ofa_account_properties_get_instance_private( self );

	g_free( priv->number );
	priv->number = g_strdup( gtk_entry_get_text( entry ));
	priv->number_ok = FALSE;

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaAccountProperties *self )
{
	ofaAccountPropertiesPrivate *priv;

	priv = ofa_account_properties_get_instance_private( self );

	g_free( priv->label );
	priv->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

/*
 * ofaCurrencyCombo signal
 */
static void
on_currency_changed( ofaCurrencyCombo *combo, const gchar *code, ofaAccountProperties *self )
{
	ofaAccountPropertiesPrivate *priv;
	const gchar *iso3a;

	priv = ofa_account_properties_get_instance_private( self );

	g_free( priv->currency );
	priv->currency = g_strdup( code );

	priv->cur_object = ofo_currency_get_by_code( priv->hub, code );

	if( !priv->cur_object || !OFO_IS_CURRENCY( priv->cur_object )){
		iso3a = ofo_dossier_get_default_currency( priv->dossier );
		priv->cur_object = ofo_currency_get_by_code( priv->hub, iso3a );
	}

	priv->cur_digits = 2;
	priv->cur_symbol = NULL;
	if( priv->cur_object && OFO_IS_CURRENCY( priv->cur_object )){
		priv->cur_digits = ofo_currency_get_digits( priv->cur_object );
		priv->cur_symbol = ofo_currency_get_symbol( priv->cur_object );
	}

	if( priv->balances_displayed ){
		init_balances_page( self );
	}

	check_for_enable_dlg( self );
}

static void
on_root_toggled( GtkRadioButton *btn, ofaAccountProperties *self )
{
	on_type_toggled( btn, self, TRUE );
}

static void
on_detail_toggled( GtkRadioButton *btn, ofaAccountProperties *self )
{
	on_type_toggled( btn, self, FALSE );
}

static void
on_type_toggled( GtkRadioButton *btn, ofaAccountProperties *self, gboolean root )
{
	static const gchar *thisfn = "ofa_account_properties_on_type_toggled";
	ofaAccountPropertiesPrivate *priv;

	priv = ofa_account_properties_get_instance_private( self );

	if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( btn ))){
		g_debug( "%s: setting root account to %s", thisfn, root ? "Y":"N" );
		priv->root = root;
	}

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaAccountProperties *self )
{
	ofaAccountPropertiesPrivate *priv;
	gboolean ok_enabled;

	priv = ofa_account_properties_get_instance_private( self );

	if( priv->is_current ){

		gtk_widget_set_sensitive( priv->type_frame, !priv->has_entries );
		/*g_debug( "setting type frame to %s", priv->has_entries ? "False":"True" );*/

		gtk_widget_set_sensitive( priv->p1_exe_frame, !priv->root );

		gtk_widget_set_sensitive( priv->currency_etiq, !priv->root && !priv->has_entries );
		gtk_widget_set_sensitive( GTK_WIDGET( priv->currency_parent ), !priv->root && !priv->has_entries );

		ok_enabled = is_dialog_validable( self );

		gtk_widget_set_sensitive( priv->ok_btn, ok_enabled );
	}
}

static gboolean
is_dialog_validable( ofaAccountProperties *self )
{
	ofaAccountPropertiesPrivate *priv;
	gboolean ok;
	ofoAccount *exists;
	const gchar *prev;
	gchar *msgerr;

	priv = ofa_account_properties_get_instance_private( self );

	msgerr = NULL;

	ok = ofo_account_is_valid_data( priv->number, priv->label, priv->currency, priv->root, &msgerr );

	/* intrinsec validity is ok
	 * the number may have been modified ; the new number is acceptable
	 * if it doesn't exist yet, or has not been modified
	 * => we are refusing a new number which already exists and is for
	 *    another account
	 */
	if( ok && !priv->number_ok ){

		exists = ofo_account_get_by_number( priv->hub, priv->number );
		if( exists ){
			prev = ofo_account_get_number( priv->account );
			priv->number_ok = prev && g_utf8_collate( prev, priv->number ) == 0;
		} else {
			priv->number_ok = TRUE;
		}
		if( !priv->number_ok ){
			msgerr = g_strdup( _( "Account already exists" ));
		}
		ok &= priv->number_ok;
	}

	set_msgerr( self, msgerr );
	g_free( msgerr );

	return( ok );
}

static gboolean
do_update( ofaAccountProperties *self, gchar **msgerr )
{
	ofaAccountPropertiesPrivate *priv;
	gchar *prev_number;
	gboolean ok;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = ofa_account_properties_get_instance_private( self );
	prev_number = g_strdup( ofo_account_get_number( priv->account ));

	ofo_account_set_number( priv->account, priv->number );
	ofo_account_set_label( priv->account, priv->label );
	ofo_account_set_closed( priv->account,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->closed_btn )));
	ofo_account_set_root( priv->account, priv->root );
	ofo_account_set_settleable( priv->account,
			priv->root ? FALSE : gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->settleable_btn )));
	ofo_account_set_reconciliable( priv->account,
			priv->root ? FALSE : gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->reconciliable_btn )));
	ofo_account_set_forwardable( priv->account,
			priv->root ? FALSE : gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->forward_btn )));
	ofo_account_set_currency( priv->account, priv->root ? NULL : priv->currency );
	my_utils_container_notes_get( self, account );

	if( priv->is_new ){
		ok = ofo_account_insert( priv->account, priv->hub );
		if( !ok ){
			*msgerr = g_strdup( _( "Unable to create this new account" ));
		}
	} else {
		ok = ofo_account_update( priv->account, prev_number );
		if( !ok ){
			*msgerr = g_strdup( _( "Unable to update the account" ));
		}
	}

	g_free( prev_number );

	return( ok );
}

static void
set_msgerr( ofaAccountProperties *self, const gchar *msg )
{
	ofaAccountPropertiesPrivate *priv;
	GtkWidget *label;

	priv = ofa_account_properties_get_instance_private( self );

	if( !priv->msg_label ){
		label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "px-msgerr" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_utils_widget_set_style( label, "labelerror" );
		priv->msg_label = label;
	}

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}
