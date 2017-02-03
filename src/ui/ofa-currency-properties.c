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
#include <stdlib.h>

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-currency-properties.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
typedef struct {
	gboolean     dispose_has_run;

	/* initialization
	 */
	ofaIGetter  *getter;
	GtkWindow   *parent;
	ofoCurrency *currency;

	/* runtime
	 */
	gboolean     is_writable;
	gboolean     is_new;

	/* data
	 */
	gchar       *code;
	gchar       *label;
	gchar       *symbol;
	gint         digits;

	/* UI
	 */
	GtkWidget   *ok_btn;
	GtkWidget   *msg_label;
}
	ofaCurrencyPropertiesPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-currency-properties.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static gchar   *iwindow_get_identifier( const myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     on_code_changed( GtkEntry *entry, ofaCurrencyProperties *self );
static void     on_label_changed( GtkEntry *entry, ofaCurrencyProperties *self );
static void     on_symbol_changed( GtkEntry *entry, ofaCurrencyProperties *self );
static void     on_digits_changed( GtkEntry *entry, ofaCurrencyProperties *self );
static void     check_for_enable_dlg( ofaCurrencyProperties *self );
static gboolean is_dialog_validable( ofaCurrencyProperties *self );
static void     on_ok_clicked( ofaCurrencyProperties *self );
static gboolean do_update( ofaCurrencyProperties *self, gchar **msgerr );
static void     set_msgerr( ofaCurrencyProperties *self, const gchar *msg );

G_DEFINE_TYPE_EXTENDED( ofaCurrencyProperties, ofa_currency_properties, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaCurrencyProperties )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
currency_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_currency_properties_finalize";
	ofaCurrencyPropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CURRENCY_PROPERTIES( instance ));

	/* free data members here */
	priv = ofa_currency_properties_get_instance_private( OFA_CURRENCY_PROPERTIES( instance ));

	g_free( priv->code );
	g_free( priv->label );
	g_free( priv->symbol );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currency_properties_parent_class )->finalize( instance );
}

static void
currency_properties_dispose( GObject *instance )
{
	ofaCurrencyPropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_CURRENCY_PROPERTIES( instance ));

	priv = ofa_currency_properties_get_instance_private( OFA_CURRENCY_PROPERTIES( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currency_properties_parent_class )->dispose( instance );
}

static void
ofa_currency_properties_init( ofaCurrencyProperties *self )
{
	static const gchar *thisfn = "ofa_currency_properties_init";
	ofaCurrencyPropertiesPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_CURRENCY_PROPERTIES( self ));

	priv = ofa_currency_properties_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->is_new = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_currency_properties_class_init( ofaCurrencyPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_currency_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = currency_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = currency_properties_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_currency_properties_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 * @currency: the #ofoCurrency to be displayed/updated.
 *
 * Update the properties of an currency
 */
void
ofa_currency_properties_run( ofaIGetter *getter, GtkWindow *parent, ofoCurrency *currency )
{
	static const gchar *thisfn = "ofa_currency_properties_run";
	ofaCurrencyProperties *self;
	ofaCurrencyPropertiesPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p, currency=%p",
			thisfn, ( void * ) getter, ( void * ) parent, ( void * ) currency );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_CURRENCY_PROPERTIES, NULL );

	priv = ofa_currency_properties_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
	priv->currency = currency;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_currency_properties_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
	iface->get_identifier = iwindow_get_identifier;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_currency_properties_iwindow_init";
	ofaCurrencyPropertiesPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_currency_properties_get_instance_private( OFA_CURRENCY_PROPERTIES( instance ));

	my_iwindow_set_parent( instance, priv->parent );

	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));
}

/*
 * identifier is built with class name and currency iso 3a code
 */
static gchar *
iwindow_get_identifier( const myIWindow *instance )
{
	ofaCurrencyPropertiesPrivate *priv;
	gchar *id;

	priv = ofa_currency_properties_get_instance_private( OFA_CURRENCY_PROPERTIES( instance ));

	id = g_strdup_printf( "%s-%s",
				G_OBJECT_TYPE_NAME( instance ),
				ofo_currency_get_code( priv->currency ));

	return( id );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_currency_properties_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

/*
 * this dialog is subject to 'is_writable' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * currency
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_currency_properties_idialog_init";
	ofaCurrencyPropertiesPrivate *priv;
	ofaHub *hub;
	gchar *title;
	const gchar *code;
	GtkEntry *entry;
	GtkWidget *label, *btn;
	gchar *str;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_currency_properties_get_instance_private( OFA_CURRENCY_PROPERTIES( instance ));

	/* update properties on OK + always terminates */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect_swapped( btn, "clicked", G_CALLBACK( on_ok_clicked ), instance );
	priv->ok_btn = btn;

	hub = ofa_igetter_get_hub( priv->getter );
	priv->is_writable = ofa_hub_is_writable_dossier( hub );

	code = ofo_currency_get_code( priv->currency );
	if( !code ){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new currency" ));
	} else {
		title = g_strdup_printf( _( "Updating « %s » currency" ), code );
	}
	gtk_window_set_title( GTK_WINDOW( instance ), title );

	/* iso 3a code */
	priv->code = g_strdup( code );
	entry = GTK_ENTRY(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( instance ), "p1-code-entry" ));
	if( priv->code ){
		gtk_entry_set_text( entry, priv->code );
	}
	g_signal_connect( entry, "changed", G_CALLBACK( on_code_changed ), instance );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-code-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));

	/* label */
	priv->label = g_strdup( ofo_currency_get_label( priv->currency ));
	entry = GTK_ENTRY(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( instance ), "p1-label-entry" ));
	if( priv->label ){
		gtk_entry_set_text( entry, priv->label );
	}
	g_signal_connect( entry, "changed", G_CALLBACK( on_label_changed ), instance );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-label-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));

	/* symbol */
	priv->symbol = g_strdup( ofo_currency_get_symbol( priv->currency ));
	entry = GTK_ENTRY(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( instance ), "p1-symbol-entry" ));
	if( priv->symbol ){
		gtk_entry_set_text( entry, priv->symbol );
	}
	g_signal_connect( entry, "changed", G_CALLBACK( on_symbol_changed ), instance );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-symbol-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));

	/* digits */
	priv->digits = ofo_currency_get_digits( priv->currency );
	entry = GTK_ENTRY(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( instance ), "p1-digits-entry" ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_digits_changed ), instance );
	str = g_strdup_printf( "%d", priv->digits );
	gtk_entry_set_text( entry, str );
	g_free( str );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-digits-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));

	my_utils_container_notes_init( instance, currency );
	my_utils_container_updstamp_init( instance, currency );
	my_utils_container_set_editable( GTK_CONTAINER( instance ), priv->is_writable );

	/* if not the current exercice, then only have a 'Close' button */
	if( !priv->is_writable ){
		my_idialog_set_close_button( instance );
		priv->ok_btn = NULL;
	}

	check_for_enable_dlg( OFA_CURRENCY_PROPERTIES( instance ));
}

static void
on_code_changed( GtkEntry *entry, ofaCurrencyProperties *self )
{
	ofaCurrencyPropertiesPrivate *priv;

	priv = ofa_currency_properties_get_instance_private( self );

	g_free( priv->code );
	priv->code = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaCurrencyProperties *self )
{
	ofaCurrencyPropertiesPrivate *priv;

	priv = ofa_currency_properties_get_instance_private( self );

	g_free( priv->label );
	priv->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_symbol_changed( GtkEntry *entry, ofaCurrencyProperties *self )
{
	ofaCurrencyPropertiesPrivate *priv;

	priv = ofa_currency_properties_get_instance_private( self );

	g_free( priv->symbol );
	priv->symbol = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_digits_changed( GtkEntry *entry, ofaCurrencyProperties *self )
{
	ofaCurrencyPropertiesPrivate *priv;

	priv = ofa_currency_properties_get_instance_private( self );

	priv->digits = atoi( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaCurrencyProperties *self )
{
	ofaCurrencyPropertiesPrivate *priv;

	priv = ofa_currency_properties_get_instance_private( self );

	if( priv->is_writable ){
		gtk_widget_set_sensitive( priv->ok_btn, is_dialog_validable( self ));
	}
}

static gboolean
is_dialog_validable( ofaCurrencyProperties *self )
{
	ofaCurrencyPropertiesPrivate *priv;
	gboolean ok;
	ofoCurrency *exists;
	gchar *msgerr;

	priv = ofa_currency_properties_get_instance_private( self );

	msgerr = NULL;

	ok = ofo_currency_is_valid_data( priv->code, priv->label, priv->symbol, priv->digits, &msgerr );
	if( ok ){
		exists = ofo_currency_get_by_code( priv->getter, priv->code );
		ok = !exists ||
				( !priv->is_new && !g_utf8_collate( priv->code, ofo_currency_get_code( priv->currency )));
		if( !ok ){
			msgerr = g_strdup( _( "The currency already exists" ));
		}
	}

	set_msgerr( self, msgerr );
	g_free( msgerr );

	return( ok );
}

static void
on_ok_clicked( ofaCurrencyProperties *self )
{
	gchar *msgerr = NULL;

	do_update( self, &msgerr );

	if( my_strlen( msgerr )){
		my_utils_msg_dialog( GTK_WINDOW( self ), GTK_MESSAGE_WARNING, msgerr );
		g_free( msgerr );
	}

	my_iwindow_close( MY_IWINDOW( self ));
}

static gboolean
do_update( ofaCurrencyProperties *self, gchar **msgerr )
{
	ofaCurrencyPropertiesPrivate *priv;
	gchar *prev_code;
	gboolean ok;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = ofa_currency_properties_get_instance_private( self );

	prev_code = g_strdup( ofo_currency_get_code( priv->currency ));

	ofo_currency_set_code( priv->currency, priv->code );
	ofo_currency_set_label( priv->currency, priv->label );
	ofo_currency_set_symbol( priv->currency, priv->symbol );
	ofo_currency_set_digits( priv->currency, priv->digits );
	my_utils_container_notes_get( self, currency );

	if( priv->is_new ){
		ok = ofo_currency_insert( priv->currency );
		if( !ok ){
			*msgerr = g_strdup( _( "Unable to create this new currency" ));
		}
	} else {
		ok = ofo_currency_update( priv->currency, prev_code );
		if( !ok ){
			*msgerr = g_strdup( _( "Unable to update the currency" ));
		}
	}

	g_free( prev_code );

	return( ok );
}

static void
set_msgerr( ofaCurrencyProperties *self, const gchar *msg )
{
	ofaCurrencyPropertiesPrivate *priv;
	GtkWidget *label;

	priv = ofa_currency_properties_get_instance_private( self );

	if( !priv->msg_label ){
		label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "px-msgerr" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_style_add( label, "labelerror" );
		priv->msg_label = label;
	}

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}
