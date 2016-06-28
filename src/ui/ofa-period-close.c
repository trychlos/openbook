/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "my/my-date-editable.h"
#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-progress-bar.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"

#include "ui/ofa-check-balances.h"
#include "ui/ofa-check-integrity.h"
#include "ui/ofa-ledger-close.h"
#include "ui/ofa-period-close.h"

/* private instance data
 */
typedef struct {
	gboolean            dispose_has_run;

	ofaIGetter         *getter;
	GDate               prev_closing;
	GDate               closing;

	/* UI
	 */
	GtkWidget          *closing_date;
	GtkWidget          *accounts_btn;
	GtkWidget          *do_close_btn;
	GtkWidget          *message_label;
}
	ofaPeriodClosePrivate;

static const gchar  *st_resource_ui     = "/org/trychlos/openbook/ui/ofa-period-close.ui";
static const gchar  *st_settings        = "PeriodClose";

static void      iwindow_iface_init( myIWindowInterface *iface );
static void      idialog_iface_init( myIDialogInterface *iface );
static void      idialog_init( myIDialog *instance );
static void      setup_date( ofaPeriodClose *self );
static void      setup_others( ofaPeriodClose *self );
static void      on_date_changed( GtkEditable *entry, ofaPeriodClose *self );
static void      check_for_enable_dlg( ofaPeriodClose *self );
static gboolean  is_dialog_validable( ofaPeriodClose *self );
static void      on_ok_clicked( GtkButton *button, ofaPeriodClose *self );
static gboolean  do_close( ofaPeriodClose *self );
static void      load_settings( ofaPeriodClose *self );
static void      set_settings( ofaPeriodClose *self );

G_DEFINE_TYPE_EXTENDED( ofaPeriodClose, ofa_period_close, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaPeriodClose )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
period_close_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_period_close_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PERIOD_CLOSE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_period_close_parent_class )->finalize( instance );
}

static void
period_close_dispose( GObject *instance )
{
	ofaPeriodClosePrivate *priv;

	g_return_if_fail( instance && OFA_IS_PERIOD_CLOSE( instance ));

	priv = ofa_period_close_get_instance_private( OFA_PERIOD_CLOSE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_period_close_parent_class )->dispose( instance );
}

static void
ofa_period_close_init( ofaPeriodClose *self )
{
	static const gchar *thisfn = "ofa_period_close_init";
	ofaPeriodClosePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PERIOD_CLOSE( self ));

	priv = ofa_period_close_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	my_date_clear( &priv->closing );

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_period_close_class_init( ofaPeriodCloseClass *klass )
{
	static const gchar *thisfn = "ofa_period_close_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = period_close_dispose;
	G_OBJECT_CLASS( klass )->finalize = period_close_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_period_close_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 *
 * Run an intermediate closing on selected ledgers
 */
void
ofa_period_close_run( ofaIGetter *getter, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_period_close_run";
	ofaPeriodClose *self;
	ofaPeriodClosePrivate *priv;

	g_debug( "%s: getter=%p, parent=%p", thisfn, ( void * ) getter, ( void * ) parent );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_PERIOD_CLOSE, NULL );
	my_iwindow_set_parent( MY_IWINDOW( self ), parent );
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

	priv = ofa_period_close_get_instance_private( self );

	priv->getter = getter;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_period_close_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_period_close_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

/*
 * first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_period_close_idialog_init";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	setup_date( OFA_PERIOD_CLOSE( instance ));
	setup_others( OFA_PERIOD_CLOSE( instance ));

	load_settings( OFA_PERIOD_CLOSE( instance ));
}

static void
setup_date( ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	GtkWidget *label;
	gchar *str;
	ofoDossier *dossier;
	ofaHub *hub;

	priv = ofa_period_close_get_instance_private( self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-last-closing" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	hub = ofa_igetter_get_hub( priv->getter );
	dossier = ofa_hub_get_dossier( hub );
	my_date_set_from_date( &priv->prev_closing, ofo_dossier_get_last_closing_date( dossier ));
	if( my_date_is_valid( &priv->prev_closing )){
		str = my_date_to_str( &priv->prev_closing, ofa_prefs_date_display());
	} else {
		str = g_strdup( "" );
	}
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	priv->closing_date = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-date" );
	g_return_if_fail( priv->closing_date && GTK_IS_ENTRY( priv->closing_date ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->closing_date );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( priv->closing_date ));
	my_date_editable_set_format( GTK_EDITABLE( priv->closing_date ), ofa_prefs_date_display());
	my_date_editable_set_label( GTK_EDITABLE( priv->closing_date ), label, ofa_prefs_date_check());
	my_date_editable_set_overwrite( GTK_EDITABLE( priv->closing_date ), ofa_prefs_date_overwrite());

	g_signal_connect( priv->closing_date, "changed", G_CALLBACK( on_date_changed ), self );
}

static void
setup_others( ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	GtkWidget *btn, *label;

	priv = ofa_period_close_get_instance_private( self );

	priv->accounts_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p2-accounts" );
	g_return_if_fail( priv->accounts_btn && GTK_IS_CHECK_BUTTON( priv->accounts_btn ));

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "btn-ok" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect( btn, "clicked", G_CALLBACK( on_ok_clicked ), self );
	priv->do_close_btn = btn;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-message" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->message_label = label;
	my_utils_widget_set_style( label, "labelerror" );
}

static void
on_date_changed( GtkEditable *entry, ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;

	priv = ofa_period_close_get_instance_private( self );

	my_date_set_from_date( &priv->closing,
			my_date_editable_get_date( GTK_EDITABLE( priv->closing_date ), NULL ));

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	gboolean ok;

	priv = ofa_period_close_get_instance_private( self );

	ok = is_dialog_validable( self );

	gtk_widget_set_sensitive( priv->do_close_btn, ok );

	if( ok ){
		set_settings( self );
	}
}

/*
 * the closing date is valid:
 * - if it is itself valid
 * - greater or equal to the begin of the exercice (if set)
 * - stricly lesser than the end of the exercice (if set)
 */
static gboolean
is_dialog_validable( ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	ofaHub *hub;
	ofoDossier *dossier;
	gboolean ok;
	const GDate *exe_begin, *exe_end;

	priv = ofa_period_close_get_instance_private( self );

	ok = FALSE;
	gtk_label_set_text( GTK_LABEL( priv->message_label ), "" );
	hub = ofa_igetter_get_hub( priv->getter );
	dossier = ofa_hub_get_dossier( hub );

	/* do we have a intrinsically valid proposed closing date
	 * + compare it to the limits of the exercice */
	if( !my_date_is_valid( &priv->closing )){
		gtk_label_set_text( GTK_LABEL( priv->message_label ), _( "Invalid closing date" ));

	} else {
		exe_begin = ofo_dossier_get_exe_begin( dossier );
		if( my_date_is_valid( exe_begin ) && my_date_compare( &priv->closing, exe_begin ) <= 0 ){
			gtk_label_set_text( GTK_LABEL( priv->message_label ),
					_( "Closing date must be greater to the beginning of exercice" ));

		} else {
			exe_end = ofo_dossier_get_exe_end( dossier );
			if( my_date_is_valid( exe_end ) && my_date_compare( &priv->closing, exe_end ) >= 0 ){
				gtk_label_set_text( GTK_LABEL( priv->message_label ),
						_( "Closing date must be lesser than the end of exercice" ));

			} else if( my_date_is_valid( &priv->prev_closing ) && my_date_compare( &priv->prev_closing, &priv->closing ) >= 0 ){
					gtk_label_set_text( GTK_LABEL( priv->message_label ),
							_( "Closing date must be greater than the previous one" ));

			} else {
				ok = TRUE;
			}
		}
	}

	return( ok );
}

static void
on_ok_clicked( GtkButton *button, ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	ofaHub *hub;
	GtkWidget *close_btn;
	GtkWindow *toplevel;

	priv = ofa_period_close_get_instance_private( self );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	hub = ofa_igetter_get_hub( priv->getter );

	/* check balances and dbms integrity */
	if( !ofa_check_balances_check( hub )){
		my_utils_msg_dialog( toplevel, GTK_MESSAGE_WARNING,
				_( "We have detected losses of balance in your books.\n\n"
					"In this current state, we will be unable to close the "
					"period until you fix your balances." ));

	} else if( !ofa_check_integrity_check( hub )){
		my_utils_msg_dialog( toplevel, GTK_MESSAGE_WARNING,
				_( "Integrity check of the DBMS has failed.\\"
					"In this current state, we will be unable to close the "
					"period until you fix the errors." ));

	} else if( do_close( self )){
		gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE );

		close_btn = gtk_dialog_get_widget_for_response( GTK_DIALOG( self ), GTK_RESPONSE_CANCEL );
		g_return_if_fail( close_btn && GTK_IS_BUTTON( close_btn ));
		gtk_button_set_label( GTK_BUTTON( close_btn ), _( "_Close" ));
		gtk_button_set_use_underline( GTK_BUTTON( close_btn ), TRUE );
	}
}

static gboolean
do_close( ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	ofaHub *hub;
	ofoDossier *dossier;
	gboolean do_archive;
	GList *dataset, *it;
	guint count;
	gchar *msg;

	priv = ofa_period_close_get_instance_private( self );

	ofa_ledger_close_do_close_all( priv->getter, GTK_WINDOW( self ), &priv->closing );

	hub = ofa_igetter_get_hub( priv->getter );
	dossier = ofa_hub_get_dossier( hub );
	ofo_dossier_set_last_closing_date( dossier, &priv->closing );
	ofo_dossier_update( dossier );

	do_archive = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->accounts_btn ));
	if( do_archive ){
		count = 0;
		dataset = ofo_account_get_dataset( hub );
		for( it=dataset ; it ; it=it->next ){
			if( !ofo_account_is_root( OFO_ACCOUNT( it->data ))){
				count += 1;
				ofo_account_archive_balances( OFO_ACCOUNT( it->data ), &priv->closing );
			}
		}
		msg = g_strdup_printf( _( "%u accounts successfully archived" ), count );
		my_iwindow_msg_dialog( MY_IWINDOW( self ), GTK_MESSAGE_INFO, msg );
		g_free( msg );
	}

	return( TRUE );
}

/*
 * settings: a string list:
 * save_accounts;
 */
static void
load_settings( ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	GList *list, *it;
	const gchar *cstr;

	priv = ofa_period_close_get_instance_private( self );

	list = ofa_settings_user_get_string_list( st_settings );

	it = list;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON( priv->accounts_btn ), my_utils_boolean_from_str( cstr ));
	}

	ofa_settings_free_string_list( list );
}

static void
set_settings( ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	gchar *str;

	priv = ofa_period_close_get_instance_private( self );

	str = g_strdup_printf( "%s;",
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->accounts_btn )) ? "True":"False" );

	ofa_settings_user_set_string( st_settings, str );

	g_free( str );
}
