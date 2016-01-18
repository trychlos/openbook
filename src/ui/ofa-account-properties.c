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

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofa-hub.h"
#include "api/ofo-base.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-account-properties.h"
#include "ui/ofa-currency-combo.h"

/* private instance data
 */
struct _ofaAccountPropertiesPrivate {

	/* initialization
	 */
	const ofaMainWindow *main_window;
	ofaHub              *hub;
	ofoAccount          *account;

	/* runtime data
	 */
	ofoDossier          *dossier;
	gboolean             is_current;
	gboolean             is_new;
	gboolean             updated;
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
	GtkWidget           *btn_ok;

	/* account data
	 */
	gchar               *number;
	gchar               *label;
	gchar               *currency;
	gint                 cur_digits;
	const gchar         *cur_symbol;
	gboolean             root;
	gchar               *upd_user;
	GTimeVal             upd_stamp;
};

typedef gdouble       ( *fnGetDouble )( const ofoAccount * );
typedef gint          ( *fnGetInt )   ( const ofoAccount * );
typedef const GDate * ( *fnGetDate )  ( const ofoAccount * );

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-account-properties.ui";
static const gchar  *st_ui_id  = "AccountPropertiesDlg";

G_DEFINE_TYPE( ofaAccountProperties, ofa_account_properties, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      init_ui( ofaAccountProperties *dialog, GtkContainer *container );
static void      remove_balances_page( ofaAccountProperties *self, GtkContainer *container );
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
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_update( ofaAccountProperties *self );

static void
account_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_properties_finalize";
	ofaAccountPropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_PROPERTIES( instance ));

	/* free data members here */
	priv = OFA_ACCOUNT_PROPERTIES( instance )->priv;
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
	g_return_if_fail( instance && OFA_IS_ACCOUNT_PROPERTIES( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_properties_parent_class )->dispose( instance );
}

static void
ofa_account_properties_init( ofaAccountProperties *self )
{
	static const gchar *thisfn = "ofa_account_properties_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_ACCOUNT_PROPERTIES, ofaAccountPropertiesPrivate );

	self->priv->account = NULL;
	self->priv->is_new = FALSE;
	self->priv->updated = FALSE;
	self->priv->balances_displayed = TRUE;
}

static void
ofa_account_properties_class_init( ofaAccountPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_account_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_properties_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaAccountPropertiesPrivate ));
}

/**
 * ofa_account_properties_run:
 * @main_window: the main window of the application.
 * @account: the account.
 *
 * Update the properties of an account
 */
gboolean
ofa_account_properties_run( const ofaMainWindow *main_window, ofoAccount *account )
{
	static const gchar *thisfn = "ofa_account_properties_run";
	ofaAccountProperties *self;
	gboolean updated;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );
	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );

	g_debug( "%s: main_window=%p, account=%p",
			thisfn, ( void * ) main_window, ( void * ) account );

	self = g_object_new(
				OFA_TYPE_ACCOUNT_PROPERTIES,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	self->priv->main_window = main_window;
	self->priv->account = account;

	my_dialog_run_dialog( MY_DIALOG( self ));

	updated = self->priv->updated;
	g_object_unref( self );

	return( updated );
}

/*
 * this dialog is subject to 'is_current' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * account
 */
static void
v_init_dialog( myDialog *dialog )
{
	static const gchar *thisfn = "ofa_account_properties_v_init_dialog";
	ofaAccountProperties *self;
	ofaAccountPropertiesPrivate *priv;
	gchar *title;
	const gchar *acc_number;
	GtkWindow *toplevel;

	self = OFA_ACCOUNT_PROPERTIES( dialog );
	priv = self->priv;

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( toplevel && GTK_IS_WINDOW( toplevel ));

	priv->hub = ofa_main_window_get_hub( priv->main_window );
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
	gtk_window_set_title( toplevel, title );

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

	init_ui( self, GTK_CONTAINER( toplevel ));

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
		on_root_toggled( GTK_RADIO_BUTTON( priv->root_btn ), self );
	} else {
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->detail_btn ), TRUE );
		on_detail_toggled( GTK_RADIO_BUTTON( priv->detail_btn ), self );
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
		remove_balances_page( self, GTK_CONTAINER( toplevel ));
	} else {
		init_balances_page( self );
	}

	my_utils_container_notes_init( GTK_CONTAINER( toplevel ), account );
	my_utils_container_updstamp_init( GTK_CONTAINER( toplevel ), account );
	my_utils_container_set_editable( GTK_CONTAINER( toplevel ), priv->is_current );

	/* setup fields editability, depending of
	 * - whether the dossier is current
	 * - whether account has entries or is empty
	 */
	my_utils_widget_set_editable( priv->number_entry, priv->is_current && !priv->has_entries );
	my_utils_widget_set_editable( priv->root_btn, priv->is_current && !priv->has_entries );
	my_utils_widget_set_editable( priv->detail_btn, priv->is_current && !priv->has_entries );
	my_utils_widget_set_editable( priv->currency_combo, priv->is_current && !priv->has_entries );

	if( !priv->is_current ){
		priv->btn_ok = my_dialog_set_readonly_buttons( dialog );
	}
}

/*
 * static initialization, i.e. which does not depend of current status
 */
static void
init_ui( ofaAccountProperties *dialog, GtkContainer *container )
{
	ofaAccountPropertiesPrivate *priv;
	ofaCurrencyCombo *combo;
	GtkWidget *label;

	priv = dialog->priv;

	/* account number */
	priv->number_entry = my_utils_container_get_child_by_name( container, "p1-number" );
	g_return_if_fail( priv->number_entry && GTK_IS_ENTRY( priv->number_entry ));
	g_signal_connect(
			G_OBJECT( priv->number_entry ), "changed", G_CALLBACK( on_number_changed ), dialog );
	label = my_utils_container_get_child_by_name( container, "p1-account-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->number_entry ));

	/* account label */
	priv->label_entry = my_utils_container_get_child_by_name( container, "p1-label" );
	g_return_if_fail( priv->label_entry && GTK_IS_ENTRY( priv->label_entry ));
	g_signal_connect(
			G_OBJECT( priv->label_entry ), "changed", G_CALLBACK( on_label_changed ), dialog );
	label = my_utils_container_get_child_by_name( container, "p1-label-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->label_entry ));

	/* closed account */
	priv->closed_btn = my_utils_container_get_child_by_name( container, "p1-closed" );

	/* account type */
	priv->type_frame = my_utils_container_get_child_by_name( container, "p1-type-frame" );

	priv->root_btn = my_utils_container_get_child_by_name( container, "p1-root-account" );
	g_return_if_fail( priv->root_btn && GTK_IS_RADIO_BUTTON( priv->root_btn ));
	g_signal_connect(
			G_OBJECT( priv->root_btn ), "toggled", G_CALLBACK( on_root_toggled ), dialog );

	priv->detail_btn = my_utils_container_get_child_by_name( container, "p1-detail-account" );
	g_return_if_fail( priv->detail_btn && GTK_IS_RADIO_BUTTON( priv->detail_btn ));
	g_signal_connect(
			G_OBJECT( priv->detail_btn ), "toggled", G_CALLBACK( on_detail_toggled ), dialog );

	/* account behavior when closing exercice */
	priv->p1_exe_frame = my_utils_container_get_child_by_name( container, "p1-exe-frame" );

	priv->settleable_btn = my_utils_container_get_child_by_name( container, "p1-settleable" );
	g_return_if_fail( priv->settleable_btn && GTK_IS_TOGGLE_BUTTON( priv->settleable_btn ));

	priv->reconciliable_btn = my_utils_container_get_child_by_name( container, "p1-reconciliable" );
	g_return_if_fail( priv->reconciliable_btn && GTK_IS_TOGGLE_BUTTON( priv->reconciliable_btn ));

	priv->forward_btn = my_utils_container_get_child_by_name( container, "p1-forward" );
	g_return_if_fail( priv->forward_btn && GTK_IS_TOGGLE_BUTTON( priv->forward_btn ));

	/* currency */
	combo = ofa_currency_combo_new();
	priv->currency_parent = my_utils_container_get_child_by_name( container, "p1-currency-parent" );
	g_return_if_fail( priv->currency_parent && GTK_IS_CONTAINER( priv->currency_parent ));
	gtk_container_add( GTK_CONTAINER( priv->currency_parent ), GTK_WIDGET( combo ));
	ofa_currency_combo_set_columns( combo, CURRENCY_DISP_CODE );
	ofa_currency_combo_set_hub( combo, priv->hub );
	g_signal_connect( G_OBJECT( combo ), "ofa-changed", G_CALLBACK( on_currency_changed ), dialog );
	priv->currency_etiq = my_utils_container_get_child_by_name( container, "p1-currency-label" );
	g_return_if_fail( priv->currency_etiq && GTK_IS_LABEL( priv->currency_etiq ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( priv->currency_etiq ), GTK_WIDGET( combo ));
	priv->currency_combo = GTK_WIDGET( combo );

	/* buttons */
	priv->btn_ok = my_utils_container_get_child_by_name( container, "btn-ok" );
}

/*
 * no need to display a balance page for root accounts
 */
static void
remove_balances_page( ofaAccountProperties *self, GtkContainer *container )
{
	GtkWidget *book, *page_w;
	gint page_n;

	book = my_utils_container_get_child_by_name( container, "properties-book" );
	g_return_if_fail( book && GTK_IS_NOTEBOOK( book ));

	page_w = my_utils_container_get_child_by_name( container, "balance-grid" );
	page_n = gtk_notebook_page_num( GTK_NOTEBOOK( book ), page_w );
	gtk_notebook_remove_page( GTK_NOTEBOOK( book ), page_n );

	self->priv->balances_displayed = FALSE;
}

static void
init_balances_page( ofaAccountProperties *self )
{
	ofaAccountPropertiesPrivate *priv;

	priv = self->priv;

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

	priv = self->priv;

	label = GTK_LABEL( my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), wname ));
	str = my_double_to_str_ex( amount, priv->cur_digits );
	gtk_label_set_text( label, str );
	g_free( str );

	label = GTK_LABEL( my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), wname_cur ));
	gtk_label_set_text( label, priv->cur_symbol );
}

static void
on_number_changed( GtkEntry *entry, ofaAccountProperties *self )
{
	g_free( self->priv->number );
	self->priv->number = g_strdup( gtk_entry_get_text( entry ));
	self->priv->number_ok = FALSE;

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaAccountProperties *self )
{
	g_free( self->priv->label );
	self->priv->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

/*
 * ofaCurrencyCombo signal
 */
static void
on_currency_changed( ofaCurrencyCombo *combo, const gchar *code, ofaAccountProperties *self )
{
	static const gchar *thisfn = "ofa_account_properties_on_currency_changed";
	ofaAccountPropertiesPrivate *priv;
	ofoCurrency *cur_obj;
	const gchar *iso3a;

	g_debug( "%s: combo=%p, code=%s, self=%p",
			thisfn, ( void * ) combo, code, ( void * ) self );

	priv = self->priv;

	g_free( priv->currency );
	priv->currency = g_strdup( code );

	cur_obj = ofo_currency_get_by_code( priv->hub, code );

	if( !cur_obj || !OFO_IS_CURRENCY( cur_obj )){
		iso3a = ofo_dossier_get_default_currency( priv->dossier );
		cur_obj = ofo_currency_get_by_code( priv->hub, iso3a );
	}
	priv->cur_digits = 2;
	priv->cur_symbol = NULL;
	if( cur_obj && OFO_IS_CURRENCY( cur_obj )){
		priv->cur_digits = ofo_currency_get_digits( cur_obj );
		priv->cur_symbol = ofo_currency_get_symbol( cur_obj );
	}

	if( priv->balances_displayed ){
		init_balances_page( self );
	}

	check_for_enable_dlg( self );
}

static void
on_root_toggled( GtkRadioButton *btn, ofaAccountProperties *self )
{
	/*g_debug( "on_root_toggled" );*/
	on_type_toggled( btn, self, TRUE );
}

static void
on_detail_toggled( GtkRadioButton *btn, ofaAccountProperties *self )
{
	/*g_debug( "on_detail_toggled" );*/
	on_type_toggled( btn, self, FALSE );
}

static void
on_type_toggled( GtkRadioButton *btn, ofaAccountProperties *self, gboolean root )
{
	static const gchar *thisfn = "ofa_account_properties_on_type_toggled";
	ofaAccountPropertiesPrivate *priv;

	priv = self->priv;

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

	priv = self->priv;

	if( !priv->is_current ){
		ok_enabled = TRUE;

	} else {
		gtk_widget_set_sensitive( priv->type_frame, !priv->has_entries );
		/*g_debug( "setting type frame to %s", priv->has_entries ? "False":"True" );*/

		gtk_widget_set_sensitive( priv->p1_exe_frame, !priv->root );

		gtk_widget_set_sensitive( priv->currency_etiq, !priv->root && !priv->has_entries );
		gtk_widget_set_sensitive( GTK_WIDGET( priv->currency_parent ), !priv->root && !priv->has_entries );

		ok_enabled = is_dialog_validable( self );
	}

	gtk_widget_set_sensitive( priv->btn_ok, ok_enabled );
}

static gboolean
is_dialog_validable( ofaAccountProperties *self )
{
	ofaAccountPropertiesPrivate *priv;
	gboolean ok;
	ofoAccount *exists;
	const gchar *prev;

	priv = self->priv;
	ok = ofo_account_is_valid_data( priv->number, priv->label, priv->currency, priv->root );

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
		ok &= priv->number_ok;
	}

	return( ok );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_update( OFA_ACCOUNT_PROPERTIES( dialog )));
}

static gboolean
do_update( ofaAccountProperties *self )
{
	ofaAccountPropertiesPrivate *priv;
	gchar *prev_number;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = self->priv;
	prev_number = g_strdup( ofo_account_get_number( self->priv->account ));

	ofo_account_set_number( priv->account, priv->number );
	ofo_account_set_label( priv->account, priv->label );
	ofo_account_set_root( priv->account, priv->root );
	ofo_account_set_settleable(
			priv->account, gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->settleable_btn )));
	ofo_account_set_reconciliable(
			priv->account, gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->reconciliable_btn )));
	ofo_account_set_forwardable(
			priv->account, gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->forward_btn )));
	ofo_account_set_closed(
			priv->account, gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->closed_btn )));
	ofo_account_set_currency( priv->account, priv->currency );
	my_utils_container_notes_get( my_window_get_toplevel( MY_WINDOW( self )), account );

	if( priv->is_new ){
		priv->updated = ofo_account_insert( priv->account, priv->hub );
	} else {
		priv->updated = ofo_account_update( priv->account, prev_number );
	}

	g_free( prev_number );

	return( priv->updated );
}
