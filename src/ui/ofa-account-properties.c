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
#include "api/my-utils.h"
#include "api/ofo-base.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"

#include "core/my-window-prot.h"

#include "ui/ofa-account-properties.h"
#include "ui/ofa-currency-combo.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaAccountPropertiesPrivate {

	/* internals
	 */
	ofoAccount     *account;
	gboolean        is_new;
	gboolean        updated;
	gboolean        number_ok;

	/* UI
	 */
	GtkEntry       *w_number;
	GtkRadioButton *w_root;
	GtkRadioButton *w_detail;

	/* account data
	 */
	gchar          *number;
	gchar          *label;
	gchar          *currency;
	gchar          *type;
	gchar          *upd_user;
	GTimeVal        upd_stamp;
	gint            deb_entry;
	myDate         *deb_date;
	gdouble         deb_amount;
	gint            cre_entry;
	myDate         *cre_date;
	gdouble         cre_amount;
	gint            day_deb_entry;
	myDate         *day_deb_date;
	gdouble         day_deb_amount;
	gint            day_cre_entry;
	myDate         *day_cre_date;
	gdouble         day_cre_amount;
};

typedef gdouble        ( *fnGetDouble )( const ofoAccount * );
typedef gint           ( *fnGetInt )   ( const ofoAccount * );
typedef const myDate * ( *fnGetDate )  ( const ofoAccount * );

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-account-properties.ui";
static const gchar  *st_ui_id  = "AccountPropertiesDlg";

G_DEFINE_TYPE( ofaAccountProperties, ofa_account_properties, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      set_amount( ofaAccountProperties *self, gdouble *amount, fnGetDouble fn, const gchar *wname );
static void      set_entry_number( ofaAccountProperties *self, gint *num, fnGetInt fn, const gchar *wname );
static void      set_entry_date( ofaAccountProperties *self, myDate *date, fnGetDate fn, const gchar *wname );
static void      on_number_changed( GtkEntry *entry, ofaAccountProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaAccountProperties *self );
static void      on_currency_changed( const gchar *code, ofaAccountProperties *self );
static void      on_root_toggled( GtkRadioButton *btn, ofaAccountProperties *self );
static void      on_detail_toggled( GtkRadioButton *btn, ofaAccountProperties *self );
static void      on_type_toggled( GtkRadioButton *btn, ofaAccountProperties *self, const gchar *type );
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

	priv = OFA_ACCOUNT_PROPERTIES( instance )->private;

	/* free data members here */
	g_free( priv->number );
	g_free( priv->label );
	g_free( priv->currency );
	g_free( priv->type );
	g_free( priv->upd_user );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_properties_parent_class )->finalize( instance );
}

static void
account_properties_dispose( GObject *instance )
{
	ofaAccountPropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_PROPERTIES( instance ));

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

		priv = OFA_ACCOUNT_PROPERTIES( instance )->private;

		/* unref object members here */
		g_object_unref( priv->deb_date );
		g_object_unref( priv->cre_date );
		g_object_unref( priv->day_deb_date );
		g_object_unref( priv->day_cre_date );
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

	self->private = g_new0( ofaAccountPropertiesPrivate, 1 );

	self->private->account = NULL;
	self->private->is_new = FALSE;
	self->private->updated = FALSE;

	self->private->deb_date = my_date_new();
	self->private->cre_date = my_date_new();
	self->private->day_deb_date = my_date_new();
	self->private->day_cre_date = my_date_new();
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
}

/**
 * ofa_account_properties_run:
 * @main: the main window of the application.
 *
 * Update the properties of an account
 */
gboolean
ofa_account_properties_run( ofaMainWindow *main_window, ofoAccount *account )
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
				MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	self->private->account = account;

	my_dialog_run_dialog( MY_DIALOG( self ));

	updated = self->private->updated;
	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( myDialog *dialog )
{
	static const gchar *thisfn = "ofa_account_properties_v_init_dialog";
	ofaAccountProperties *self;
	ofaAccountPropertiesPrivate *priv;
	gchar *title;
	const gchar *acc_number;
	GtkEntry *entry;
	ofaCurrencyComboParms parms;
	GtkContainer *container;

	self = OFA_ACCOUNT_PROPERTIES( dialog );
	priv = self->private;

	container = ( GtkContainer * ) my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	acc_number = ofo_account_get_number( priv->account );
	if( !acc_number ){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new account" ));
	} else {
		title = g_strdup_printf( _( "Updating account %s" ), acc_number );
	}
	gtk_window_set_title( GTK_WINDOW( container ), title );

	priv->number = g_strdup( acc_number );
	priv->w_number = GTK_ENTRY( my_utils_container_get_child_by_name( container, "p1-number" ));
	if( priv->number ){
		gtk_entry_set_text( priv->w_number, priv->number );
	}
	g_signal_connect(
			G_OBJECT( priv->w_number ), "changed", G_CALLBACK( on_number_changed ), dialog );

	priv->label = g_strdup( ofo_account_get_label( priv->account ));
	entry = GTK_ENTRY( my_utils_container_get_child_by_name( container, "p1-label" ));
	if( priv->label ){
		gtk_entry_set_text( entry, priv->label );
	}
	g_signal_connect(
			G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), dialog );

	priv->currency = g_strdup( ofo_account_get_currency( priv->account ));

	parms.container = container;
	parms.dossier = MY_WINDOW( dialog )->protected->dossier;
	parms.combo_name = "p1-currency";
	parms.label_name = NULL;
	parms.disp_code = TRUE;
	parms.disp_label = TRUE;
	parms.pfnSelected = ( ofaCurrencyComboCb ) on_currency_changed;
	parms.user_data = self;
	parms.initial_code = priv->currency;

	ofa_currency_combo_new( &parms );

	priv->type = g_strdup( ofo_account_get_type_account( priv->account ));

	priv->w_root = GTK_RADIO_BUTTON( my_utils_container_get_child_by_name( container, "p1-root-account" ));
	priv->w_detail = GTK_RADIO_BUTTON( my_utils_container_get_child_by_name( container, "p1-detail-account" ));
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->w_root ), FALSE );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->w_detail ), FALSE );

	if( priv->type && g_utf8_strlen( priv->type, -1 )){
		if( ofo_account_is_root( priv->account )){
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->w_root ), TRUE );
		} else if( !g_utf8_collate( priv->type, "D" )){
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->w_detail ), TRUE );
		} else {
			g_warning( "%s: account has type %s", thisfn, priv->type );
		}
	}
	g_signal_connect(
			G_OBJECT( priv->w_root ), "toggled", G_CALLBACK( on_root_toggled ), dialog );
	g_signal_connect(
			G_OBJECT( priv->w_detail ), "toggled", G_CALLBACK( on_detail_toggled ), dialog );
	if( !priv->type || !g_utf8_strlen( priv->type, -1 )){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->w_detail ), TRUE );
	}

	set_amount( self, &priv->deb_amount, ofo_account_get_deb_amount, "p2-deb-amount" );
	set_entry_number( self, &priv->deb_entry, ofo_account_get_deb_entry, "p2-deb-entry" );
	set_entry_date( self, priv->deb_date, ofo_account_get_deb_date, "p2-deb-date" );

	set_amount( self, &priv->cre_amount, ofo_account_get_cre_amount, "p2-cre-amount" );
	set_entry_number( self, &priv->cre_entry, ofo_account_get_cre_entry, "p2-cre-entry" );
	set_entry_date( self, priv->cre_date, ofo_account_get_cre_date, "p2-cre-date" );

	set_amount( self, &priv->day_deb_amount, ofo_account_get_day_deb_amount, "p2-day-deb-amount" );
	set_entry_number( self, &priv->day_deb_entry, ofo_account_get_day_deb_entry, "p2-day-deb-entry" );
	set_entry_date( self, priv->day_deb_date, ofo_account_get_day_deb_date, "p2-day-deb-date" );

	set_amount( self, &priv->day_cre_amount, ofo_account_get_day_cre_amount, "p2-day-cre-amount" );
	set_entry_number( self, &priv->day_cre_entry, ofo_account_get_day_cre_entry, "p2-day-cre-entry" );
	set_entry_date( self, priv->day_cre_date, ofo_account_get_day_cre_date, "p2-day-cre-date" );

	my_utils_init_notes_ex( container, account );
	my_utils_init_upd_user_stamp_ex( container, account );

	check_for_enable_dlg( OFA_ACCOUNT_PROPERTIES( dialog ));
}

static void
set_amount( ofaAccountProperties *self, gdouble *amount, fnGetDouble fn, const gchar *wname )
{
	GtkLabel *label;
	gchar *str;

	*amount = ( *fn )( self->private->account );
	label = GTK_LABEL( my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), wname ));
	str = g_strdup_printf( "%'.2lf €", *amount );
	gtk_label_set_text( label, str );
	g_free( str );
}

static void
set_entry_number( ofaAccountProperties *self, gint *num, fnGetInt fn, const gchar *wname )
{
	GtkLabel *label;
	gchar *str;

	*num = ( *fn )( self->private->account );
	label = GTK_LABEL( my_utils_container_get_child_by_name(
				GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), wname ));
	if( *num ){
		str = g_strdup_printf( "%u", *num );
	} else {
		str = g_strdup( "" );
	}
	gtk_label_set_text( label, str );
	g_free( str );
}

static void
set_entry_date( ofaAccountProperties *self, myDate *date, fnGetDate fn, const gchar *wname )
{
	GtkLabel *label;
	gchar *str;

	g_debug( "ofa_account_properties_set_entry_date: wname=%s", wname );
	my_date_set_from_date( date, ( *fn )( self->private->account ));
	label = GTK_LABEL( my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), wname ));
	str = my_date_to_str( date, MY_DATE_DMMM );
	gtk_label_set_text( label, str );
	g_free( str );
}

static void
on_number_changed( GtkEntry *entry, ofaAccountProperties *self )
{
	g_free( self->private->number );
	self->private->number = g_strdup( gtk_entry_get_text( entry ));
	self->private->number_ok = FALSE;

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaAccountProperties *self )
{
	g_free( self->private->label );
	self->private->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

/*
 * ofaCurrencyComboCb
 */
static void
on_currency_changed( const gchar *code, ofaAccountProperties *self )
{
	g_free( self->private->currency );
	self->private->currency = g_strdup( code );

	check_for_enable_dlg( self );
}

static void
on_root_toggled( GtkRadioButton *btn, ofaAccountProperties *self )
{
	on_type_toggled( btn, self, "R" );
}

static void
on_detail_toggled( GtkRadioButton *btn, ofaAccountProperties *self )
{
	on_type_toggled( btn, self, "D" );
}

static void
on_type_toggled( GtkRadioButton *btn, ofaAccountProperties *self, const gchar *type )
{
	static const gchar *thisfn = "ofa_account_properties_on_type_toggled";

	if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( btn ))){
		g_debug( "%s: setting account type to %s", thisfn, type );
		g_free( self->private->type );
		self->private->type = g_strdup( type );
	}

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaAccountProperties *self )
{
	ofaAccountPropertiesPrivate *priv;
	gboolean vierge;
	gboolean is_root;
	GtkComboBox *combo;
	GtkWidget *button;
	gboolean ok_enabled;

	priv = self->private;

	/* has this account already add some imputation ? */
	vierge = !priv->deb_amount && !priv->cre_amount && !priv->day_deb_amount && !priv->day_cre_amount;

	gtk_widget_set_sensitive( GTK_WIDGET( priv->w_number ), vierge );

	is_root = ( priv->type && !g_utf8_collate( priv->type, "R" ));

	if( priv->w_root && priv->w_detail ){
		if( priv->type && !g_utf8_collate( priv->type, "D" )){
			gtk_widget_set_sensitive( GTK_WIDGET( priv->w_root ), vierge );
			gtk_widget_set_sensitive( GTK_WIDGET( priv->w_detail ), vierge );
		} else {
			gtk_widget_set_sensitive( GTK_WIDGET( priv->w_root ), TRUE );
			gtk_widget_set_sensitive( GTK_WIDGET( priv->w_detail ), TRUE );
		}
	}

	combo = GTK_COMBO_BOX(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "p1-currency" ));
	if( combo ){
		gtk_widget_set_sensitive( GTK_WIDGET( combo ), vierge && !is_root );
	}

	ok_enabled = is_dialog_validable( self );
	button = my_utils_container_get_child_by_name(
							GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "btn-ok" );
	if( button ){
		gtk_widget_set_sensitive( button, ok_enabled );
	}
}

static gboolean
is_dialog_validable( ofaAccountProperties *self )
{
	gboolean ok;
	ofaAccountPropertiesPrivate *priv;
	ofoDossier *dossier;
	ofoAccount *exists;
	const gchar *prev;

	priv = self->private;

	ok = ofo_account_is_valid_data( priv->number, priv->label, priv->currency, priv->type );

	/* intrinsec validity is ok
	 * the number may have been modified ; the new number is acceptable
	 * if it doesn't exist yet, or has not been modified
	 * => we are refusing a new number which already exists and is for
	 *    another account
	 */
	if( ok && !self->private->number_ok ){

		dossier = MY_WINDOW( self )->protected->dossier;
		exists = ofo_account_get_by_number( dossier, self->private->number );
		prev = ofo_account_get_number( priv->account );
		self->private->number_ok = !exists || !g_utf8_collate( prev, priv->number );
		ok &= self->private->number_ok;
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

	priv = self->private;
	prev_number = g_strdup( ofo_account_get_number( self->private->account ));

	ofo_account_set_number( priv->account, priv->number );
	ofo_account_set_label( priv->account, priv->label );
	ofo_account_set_type( priv->account, priv->type );
	ofo_account_set_currency( priv->account, priv->currency );
	my_utils_getback_notes_ex( my_window_get_toplevel( MY_WINDOW( self )), account );

	if( priv->is_new ){
		priv->updated =
				ofo_account_insert( priv->account );
	} else {
		priv->updated =
				ofo_account_update( priv->account, prev_number );
	}

	g_free( prev_number );

	return( priv->updated );
}
