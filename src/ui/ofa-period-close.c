/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-isignaler.h"
#include "api/ofa-prefs.h"
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
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;
	GtkWindow  *parent;

	/* runtime
	 */
	gchar      *settings_prefix;
	GtkWindow  *actual_parent;
	GDate       prev_closing;
	GDate       closing;

	/* UI
	 */
	GtkWidget  *closing_date;
	GtkWidget  *accounts_btn;
	GtkWidget  *ledgers_btn;
	GtkWidget  *do_close_btn;
	GtkWidget  *message_label;
}
	ofaPeriodClosePrivate;

static const gchar  *st_resource_ui     = "/org/trychlos/openbook/ui/ofa-period-close.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     setup_date( ofaPeriodClose *self );
static void     setup_others( ofaPeriodClose *self );
static void     on_date_changed( GtkEditable *entry, ofaPeriodClose *self );
static void     check_for_enable_dlg( ofaPeriodClose *self );
static gboolean is_dialog_validable( ofaPeriodClose *self );
static void     on_ok_clicked( ofaPeriodClose *self );
static void     do_ok( ofaPeriodClose *self );
static gboolean do_close( ofaPeriodClose *self );
static void     read_settings( ofaPeriodClose *self );
static void     write_settings( ofaPeriodClose *self );

G_DEFINE_TYPE_EXTENDED( ofaPeriodClose, ofa_period_close, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaPeriodClose )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
period_close_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_period_close_finalize";
	ofaPeriodClosePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PERIOD_CLOSE( instance ));

	/* free data members here */
	priv = ofa_period_close_get_instance_private( OFA_PERIOD_CLOSE( instance ));

	g_free( priv->settings_prefix );

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

		write_settings( OFA_PERIOD_CLOSE( instance ));

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
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));

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

	priv = ofa_period_close_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;

	/* run modal or non-modal depending of the parent */
	my_idialog_run_maybe_modal( MY_IDIALOG( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_period_close_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_period_close_iwindow_init";
	ofaPeriodClosePrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_period_close_get_instance_private( OFA_PERIOD_CLOSE( instance ));

	priv->actual_parent = priv->parent ? priv->parent : GTK_WINDOW( ofa_igetter_get_main_window( priv->getter ));
	my_iwindow_set_parent( instance, priv->actual_parent );

	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));
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
	ofaPeriodClosePrivate *priv;
	GtkWidget *btn;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_period_close_get_instance_private( OFA_PERIOD_CLOSE( instance ));

	/* close ledgers on OK + change Cancel button to Close */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect_swapped( btn, "clicked", G_CALLBACK( on_ok_clicked ), instance );
	priv->do_close_btn = btn;

	setup_date( OFA_PERIOD_CLOSE( instance ));
	setup_others( OFA_PERIOD_CLOSE( instance ));

	read_settings( OFA_PERIOD_CLOSE( instance ));
}

static void
setup_date( ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	GtkWidget *label;
	gchar *str;
	ofaHub *hub;
	ofoDossier *dossier;

	priv = ofa_period_close_get_instance_private( self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-last-closing" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	hub = ofa_igetter_get_hub( priv->getter );
	dossier = ofa_hub_get_dossier( hub );

	my_date_set_from_date( &priv->prev_closing, ofo_dossier_get_last_closing_date( dossier ));
	if( my_date_is_valid( &priv->prev_closing )){
		str = my_date_to_str( &priv->prev_closing, ofa_prefs_date_get_display_format( priv->getter ));
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
	my_date_editable_set_entry_format( GTK_EDITABLE( priv->closing_date ), ofa_prefs_date_get_display_format( priv->getter ));
	my_date_editable_set_label_format( GTK_EDITABLE( priv->closing_date ), label, ofa_prefs_date_get_check_format( priv->getter ));
	my_date_editable_set_overwrite( GTK_EDITABLE( priv->closing_date ), ofa_prefs_date_get_overwrite( priv->getter ));

	g_signal_connect( priv->closing_date, "changed", G_CALLBACK( on_date_changed ), self );
}

static void
setup_others( ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	GtkWidget *label;

	priv = ofa_period_close_get_instance_private( self );

	priv->ledgers_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p2-ledgers" );
	g_return_if_fail( priv->ledgers_btn && GTK_IS_CHECK_BUTTON( priv->ledgers_btn ));

	priv->accounts_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p2-accounts" );
	g_return_if_fail( priv->accounts_btn && GTK_IS_CHECK_BUTTON( priv->accounts_btn ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-message" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->message_label = label;
	my_style_add( label, "labelerror" );
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
on_ok_clicked( ofaPeriodClose *self )
{
	do_ok( self );

	/* does not close the window here */
}

static void
do_ok( ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	GtkWidget *close_btn;
	GtkWindow *toplevel;

	priv = ofa_period_close_get_instance_private( self );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));

	/* check balances and dbms integrity */
	if( !ofa_check_balances_check( priv->getter )){
		my_utils_msg_dialog( toplevel, GTK_MESSAGE_WARNING,
				_( "We have detected losses of balance in your books.\n\n"
					"In this current state, we will be unable to close the "
					"period until you fix your balances." ));

	} else if( !ofa_check_integrity_check( priv->getter )){
		my_utils_msg_dialog( toplevel, GTK_MESSAGE_WARNING,
				_( "Integrity check of the DBMS has failed.\\"
					"In this current state, we will be unable to close the "
					"period until you fix the errors." ));

	} else if( do_close( self )){
		gtk_widget_set_sensitive( priv->closing_date, FALSE );
		gtk_widget_set_sensitive( priv->ledgers_btn, FALSE );
		gtk_widget_set_sensitive( priv->accounts_btn, FALSE );
		gtk_widget_set_sensitive( GTK_WIDGET( priv->do_close_btn ), FALSE );
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
	guint count, length, total;
	gchar *text;
	GtkWidget *dialog, *button, *content, *grid, *label;
	myProgressBar *bar;
	gdouble progress;
	ofaISignaler *signaler;

	priv = ofa_period_close_get_instance_private( self );

	signaler = ofa_igetter_get_signaler( priv->getter );
	g_signal_emit_by_name( signaler, SIGNALER_DOSSIER_PERIOD_CLOSING, SIGNALER_CLOSING_INTERMEDIATE, &priv->closing );

	/* close all ledgers, archiving their balance if asked for
	 */
	do_archive = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->ledgers_btn ));
	ofa_ledger_close_do_close_all( priv->getter, GTK_WINDOW( self ), &priv->closing, do_archive );

	/* as all ledgers have been closed, it is possible to also archive
	 * accounts balances (if asked for)
	 */
	do_archive = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->accounts_btn ));

	if( do_archive ){
		/* create the dialog */
		dialog = gtk_dialog_new_with_buttons(
						_( "Archiving account balances" ),
						GTK_WINDOW( self ),
						GTK_DIALOG_MODAL,
						_( "_Close" ), GTK_RESPONSE_OK,
						NULL );
		gtk_container_set_border_width( GTK_CONTAINER( dialog ), 4 );
		button = gtk_dialog_get_widget_for_response( GTK_DIALOG( dialog ), GTK_RESPONSE_OK );
		g_return_val_if_fail( button && GTK_IS_BUTTON( button ), FALSE );
		gtk_widget_set_sensitive( button, FALSE );
		content = gtk_dialog_get_content_area( GTK_DIALOG( dialog ));
		g_return_val_if_fail( content && GTK_IS_CONTAINER( content ), FALSE );
		grid = gtk_grid_new();
		my_utils_widget_set_margins( grid, 2, 8, 8, 8 );
		gtk_container_add( GTK_CONTAINER( content ), grid );
		bar = my_progress_bar_new();
		gtk_grid_attach( GTK_GRID( grid ), GTK_WIDGET( bar ), 0, 0, 1, 1 );
		gtk_grid_set_row_spacing( GTK_GRID( grid ), 4 );
		gtk_widget_show_all( dialog );

		/* run */
		count = 0;
		total = 0;
		dataset = ofo_account_get_dataset( priv->getter );
		length = g_list_length( dataset );
		for( it=dataset ; it ; it=it->next ){
			if( !ofo_account_is_root( OFO_ACCOUNT( it->data ))){
				count += 1;
				ofo_account_archive_balances( OFO_ACCOUNT( it->data ), &priv->closing );
			}
			total += 1;
			text = g_strdup_printf( "%u/%u", total, length );
			g_signal_emit_by_name( bar, "my-text", text );
			g_free( text );
			progress = ( gdouble ) total / ( gdouble ) length;
			g_signal_emit_by_name( bar, "my-double", progress );
		}
		text = g_strdup_printf( _( "%u detail accounts (on %u total count) successfully archived." ), count, length );
		label = gtk_label_new( text );
		gtk_grid_attach( GTK_GRID( grid ), label, 0, 1, 1, 1 );
		g_free( text );
		gtk_widget_show_all( dialog );

		gtk_widget_set_sensitive( button, TRUE );
		gtk_dialog_run( GTK_DIALOG( dialog ));
		gtk_widget_destroy( dialog );
	}

	hub = ofa_igetter_get_hub( priv->getter );
	dossier = ofa_hub_get_dossier( hub );
	ofo_dossier_set_last_closing_date( dossier, &priv->closing );
	ofo_dossier_update( dossier );

	g_signal_emit_by_name( signaler, SIGNALER_DOSSIER_PERIOD_CLOSED, SIGNALER_CLOSING_INTERMEDIATE, &priv->closing );

	return( TRUE );
}

/*
 * settings: a string list:
 * archive_accounts; archive_ledgers;
 */
static void
read_settings( ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;
	gchar *key;

	priv = ofa_period_close_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, key );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON( priv->accounts_btn ), my_utils_boolean_from_str( cstr ));
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON( priv->ledgers_btn ), my_utils_boolean_from_str( cstr ));
	}

	my_isettings_free_string_list( settings, strlist );
	g_free( key );
}

static void
write_settings( ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	myISettings *settings;
	gchar *str, *key;

	priv = ofa_period_close_get_instance_private( self );

	str = g_strdup_printf( "%s;%s;",
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->accounts_btn )) ? "True":"False",
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->ledgers_btn )) ? "True":"False" );

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( str );
	g_free( key );
}
