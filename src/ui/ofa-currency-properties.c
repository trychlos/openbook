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
#include <stdlib.h>

#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-currency-properties.h"

/* private instance data
 */
struct _ofaCurrencyPropertiesPrivate {

	/* internals
	 */
	ofoCurrency   *currency;
	gboolean       is_new;
	gboolean       updated;

	/* data
	 */
	gchar         *code;
	gchar         *label;
	gchar         *symbol;
	gint           digits;
};

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-currency-properties.ui";
static const gchar  *st_ui_id  = "CurrencyPropertiesDlg";

G_DEFINE_TYPE( ofaCurrencyProperties, ofa_currency_properties, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_code_changed( GtkEntry *entry, ofaCurrencyProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaCurrencyProperties *self );
static void      on_symbol_changed( GtkEntry *entry, ofaCurrencyProperties *self );
static void      on_digits_changed( GtkEntry *entry, ofaCurrencyProperties *self );
static void      check_for_enable_dlg( ofaCurrencyProperties *self );
static gboolean  is_dialog_validable( ofaCurrencyProperties *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_update( ofaCurrencyProperties *self );

static void
currency_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_currency_properties_finalize";
	ofaCurrencyPropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CURRENCY_PROPERTIES( instance ));

	/* free data members here */
	priv = OFA_CURRENCY_PROPERTIES( instance )->priv;
	g_free( priv->code );
	g_free( priv->label );
	g_free( priv->symbol );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currency_properties_parent_class )->finalize( instance );
}

static void
currency_properties_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_CURRENCY_PROPERTIES( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currency_properties_parent_class )->dispose( instance );
}

static void
ofa_currency_properties_init( ofaCurrencyProperties *self )
{
	static const gchar *thisfn = "ofa_currency_properties_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_CURRENCY_PROPERTIES( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_CURRENCY_PROPERTIES, ofaCurrencyPropertiesPrivate );
	self->priv->is_new = FALSE;
	self->priv->updated = FALSE;
}

static void
ofa_currency_properties_class_init( ofaCurrencyPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_currency_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = currency_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = currency_properties_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaCurrencyPropertiesPrivate ));
}

/**
 * ofa_currency_properties_run:
 * @main_window: the #ofaMainWindow main window of the application.
 * @currency: the #ofoCurrency to be displayed/updated.
 *
 * Update the properties of an currency
 */
gboolean
ofa_currency_properties_run( const ofaMainWindow *main_window, ofoCurrency *currency )
{
	static const gchar *thisfn = "ofa_currency_properties_run";
	ofaCurrencyProperties *self;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, currency=%p",
			thisfn, ( void * ) main_window, ( void * ) currency );

	self = g_object_new(
				OFA_TYPE_CURRENCY_PROPERTIES,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	self->priv->currency = currency;

	my_dialog_run_dialog( MY_DIALOG( self ));

	updated = self->priv->updated;
	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaCurrencyPropertiesPrivate *priv;
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;
	gchar *title;
	const gchar *code;
	GtkEntry *entry;
	GtkWidget *label;
	gchar *str;
	GtkWindow *toplevel;
	gboolean is_current;

	priv = OFA_CURRENCY_PROPERTIES( dialog )->priv;
	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));

	main_window = my_window_get_main_window( MY_WINDOW( dialog ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	code = ofo_currency_get_code( priv->currency );
	if( !code ){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new currency" ));
	} else {
		title = g_strdup_printf( _( "Updating « %s » currency" ), code );
	}
	gtk_window_set_title( toplevel, title );

	is_current = ofo_dossier_is_current( dossier );

	/* iso 3a code */
	priv->code = g_strdup( code );
	entry = GTK_ENTRY(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( toplevel ), "p1-code-entry" ));
	if( priv->code ){
		gtk_entry_set_text( entry, priv->code );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_code_changed ), dialog );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "p1-code-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));

	/* label */
	priv->label = g_strdup( ofo_currency_get_label( priv->currency ));
	entry = GTK_ENTRY(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( toplevel ), "p1-label-entry" ));
	if( priv->label ){
		gtk_entry_set_text( entry, priv->label );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), dialog );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "p1-label-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));

	/* symbol */
	priv->symbol = g_strdup( ofo_currency_get_symbol( priv->currency ));
	entry = GTK_ENTRY(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( toplevel ), "p1-symbol-entry" ));
	if( priv->symbol ){
		gtk_entry_set_text( entry, priv->symbol );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_symbol_changed ), dialog );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "p1-symbol-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));

	/* digits */
	priv->digits = ofo_currency_get_digits( priv->currency );
	entry = GTK_ENTRY(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( toplevel ), "p1-digits-entry" ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_digits_changed ), dialog );
	str = g_strdup_printf( "%d", priv->digits );
	gtk_entry_set_text( entry, str );
	g_free( str );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "p1-digits-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));

	my_utils_container_notes_init( toplevel, currency );
	my_utils_container_updstamp_init( toplevel, currency );
	my_utils_container_set_editable( GTK_CONTAINER( toplevel ), is_current );

	/* if not the current exercice, then only have a 'Close' button */
	if( !is_current ){
		my_dialog_set_readonly_buttons( dialog );
	}

	check_for_enable_dlg( OFA_CURRENCY_PROPERTIES( dialog ));
}

static void
on_code_changed( GtkEntry *entry, ofaCurrencyProperties *self )
{
	g_free( self->priv->code );
	self->priv->code = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaCurrencyProperties *self )
{
	g_free( self->priv->label );
	self->priv->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_symbol_changed( GtkEntry *entry, ofaCurrencyProperties *self )
{
	g_free( self->priv->symbol );
	self->priv->symbol = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_digits_changed( GtkEntry *entry, ofaCurrencyProperties *self )
{
	self->priv->digits = atoi( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaCurrencyProperties *self )
{
	GtkWidget *button;

	button = my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "btn-ok" );

	gtk_widget_set_sensitive( button, is_dialog_validable( self ));
}

static gboolean
is_dialog_validable( ofaCurrencyProperties *self )
{
	ofaCurrencyPropertiesPrivate *priv;
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;
	gboolean ok;
	ofoCurrency *exists;

	priv = self->priv;

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	ok = ofo_currency_is_valid( priv->code, priv->label, priv->symbol, priv->digits );
	if( ok ){
		exists = ofo_currency_get_by_code( dossier, priv->code );
		ok &= !exists ||
				( !priv->is_new && !g_utf8_collate( priv->code, ofo_currency_get_code( priv->currency )));
	}

	return( ok );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_update( OFA_CURRENCY_PROPERTIES( dialog )));
}

static gboolean
do_update( ofaCurrencyProperties *self )
{
	ofaCurrencyPropertiesPrivate *priv;
	GtkApplicationWindow *main_window;
	gchar *prev_code;
	ofoDossier *dossier;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = self->priv;

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	prev_code = g_strdup( ofo_currency_get_code( priv->currency ));

	ofo_currency_set_code( priv->currency, priv->code );
	ofo_currency_set_label( priv->currency, priv->label );
	ofo_currency_set_symbol( priv->currency, priv->symbol );
	ofo_currency_set_digits( priv->currency, priv->digits );
	my_utils_container_notes_get( my_window_get_toplevel( MY_WINDOW( self )), currency );

	if( priv->is_new ){
		priv->updated =
				ofo_currency_insert( priv->currency, dossier );
	} else {
		priv->updated =
				ofo_currency_update( priv->currency, dossier, prev_code );
	}

	g_free( prev_code );

	return( priv->updated );
}
