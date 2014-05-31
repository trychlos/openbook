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

#include "ui/my-utils.h"
#include "ui/ofa-base-dialog-prot.h"
#include "ui/ofa-devise-properties.h"
#include "ui/ofa-main-window.h"
#include "ui/ofo-devise.h"
#include "ui/ofo-dossier.h"

/* private instance data
 */
struct _ofaDevisePropertiesPrivate {

	/* internals
	 */
	ofoDevise     *devise;
	gboolean       is_new;
	gboolean       updated;

	/* data
	 */
	gchar         *code;
	gchar         *label;
	gchar         *symbol;
};

static const gchar  *st_ui_xml       = PKGUIDIR "/ofa-devise-properties.ui";
static const gchar  *st_ui_id        = "DevisePropertiesDlg";

G_DEFINE_TYPE( ofaDeviseProperties, ofa_devise_properties, OFA_TYPE_BASE_DIALOG )

static void      v_init_dialog( ofaBaseDialog *dialog );
static void      on_code_changed( GtkEntry *entry, ofaDeviseProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaDeviseProperties *self );
static void      on_symbol_changed( GtkEntry *entry, ofaDeviseProperties *self );
static void      check_for_enable_dlg( ofaDeviseProperties *self );
static gboolean  is_dialog_validable( ofaDeviseProperties *self );
static gboolean  v_quit_on_ok( ofaBaseDialog *dialog );
static gboolean  do_update( ofaDeviseProperties *self );

static void
devise_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_devise_properties_finalize";
	ofaDevisePropertiesPrivate *priv;

	g_return_if_fail( OFA_IS_DEVISE_PROPERTIES( instance ));

	priv = OFA_DEVISE_PROPERTIES( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free members here */
	g_free( priv->code );
	g_free( priv->label );
	g_free( priv->symbol );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_devise_properties_parent_class )->finalize( instance );
}

static void
devise_properties_dispose( GObject *instance )
{
	g_return_if_fail( OFA_IS_DEVISE_PROPERTIES( instance ));

	if( !OFA_BASE_DIALOG( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_devise_properties_parent_class )->dispose( instance );
}

static void
ofa_devise_properties_init( ofaDeviseProperties *self )
{
	static const gchar *thisfn = "ofa_devise_properties_init";

	g_return_if_fail( OFA_IS_DEVISE_PROPERTIES( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaDevisePropertiesPrivate, 1 );

	self->private->is_new = FALSE;
	self->private->updated = FALSE;
}

static void
ofa_devise_properties_class_init( ofaDevisePropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_devise_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = devise_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = devise_properties_finalize;

	OFA_BASE_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	OFA_BASE_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;
}

/**
 * ofa_devise_properties_run:
 * @main: the main window of the application.
 *
 * Update the properties of an devise
 */
gboolean
ofa_devise_properties_run( ofaMainWindow *main_window, ofoDevise *devise )
{
	static const gchar *thisfn = "ofa_devise_properties_run";
	ofaDeviseProperties *self;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, devise=%p",
			thisfn, ( void * ) main_window, ( void * ) devise );

	self = g_object_new(
				OFA_TYPE_DEVISE_PROPERTIES,
				OFA_PROP_MAIN_WINDOW, main_window,
				OFA_PROP_DIALOG_XML,  st_ui_xml,
				OFA_PROP_DIALOG_NAME, st_ui_id,
				NULL );

	self->private->devise = devise;

	ofa_base_dialog_run_dialog( OFA_BASE_DIALOG( self ));

	updated = self->private->updated;
	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( ofaBaseDialog *dialog )
{
	ofaDevisePropertiesPrivate *priv;
	gchar *title;
	const gchar *code;
	GtkEntry *entry;

	priv = OFA_DEVISE_PROPERTIES( dialog )->private;

	code = ofo_devise_get_code( priv->devise );
	if( !code ){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new devise" ));
	} else {
		title = g_strdup_printf( _( "Updating « %s » devise" ), code );
	}
	gtk_window_set_title( GTK_WINDOW( dialog->prot->dialog ), title );

	priv->code = g_strdup( code );
	entry = GTK_ENTRY(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( dialog->prot->dialog ), "p1-code" ));
	if( priv->code ){
		gtk_entry_set_text( entry, priv->code );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_code_changed ), dialog );

	priv->label = g_strdup( ofo_devise_get_label( priv->devise ));
	entry = GTK_ENTRY(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( dialog->prot->dialog ), "p1-label" ));
	if( priv->label ){
		gtk_entry_set_text( entry, priv->label );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), dialog );

	priv->symbol = g_strdup( ofo_devise_get_symbol( priv->devise ));
	entry = GTK_ENTRY(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( dialog->prot->dialog ), "p1-symbol" ));
	if( priv->symbol ){
		gtk_entry_set_text( entry, priv->symbol );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_symbol_changed ), dialog );

	my_utils_init_notes_ex( dialog->prot->dialog, devise );
	my_utils_init_maj_user_stamp_ex( dialog->prot->dialog, devise );

	check_for_enable_dlg( OFA_DEVISE_PROPERTIES( dialog ));
}

static void
on_code_changed( GtkEntry *entry, ofaDeviseProperties *self )
{
	g_free( self->private->code );
	self->private->code = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaDeviseProperties *self )
{
	g_free( self->private->label );
	self->private->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_symbol_changed( GtkEntry *entry, ofaDeviseProperties *self )
{
	g_free( self->private->symbol );
	self->private->symbol = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaDeviseProperties *self )
{
	GtkWidget *button;

	button = my_utils_container_get_child_by_name(
					GTK_CONTAINER( OFA_BASE_DIALOG( self )->prot->dialog ), "btn-ok" );

	gtk_widget_set_sensitive( button, is_dialog_validable( self ));
}

static gboolean
is_dialog_validable( ofaDeviseProperties *self )
{
	ofaDevisePropertiesPrivate *priv;
	gboolean ok;
	ofoDevise *exists;

	priv = self->private;

	ok = ofo_devise_is_valid( priv->code, priv->label, priv->symbol );
	if( ok ){
		exists = ofo_devise_get_by_code(
				ofa_base_dialog_get_dossier( OFA_BASE_DIALOG( self )), priv->code );
		ok &= !exists ||
				( ofo_devise_get_id( exists ) == ofo_devise_get_id( priv->devise ));
	}

	return( ok );
}

static gboolean
v_quit_on_ok( ofaBaseDialog *dialog )
{
	return( do_update( OFA_DEVISE_PROPERTIES( dialog )));
}

static gboolean
do_update( ofaDeviseProperties *self )
{
	ofaDevisePropertiesPrivate *priv;
	ofoDossier *dossier;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = self->private;
	dossier = ofa_base_dialog_get_dossier( OFA_BASE_DIALOG( self ));

	ofo_devise_set_code( priv->devise, priv->code );
	ofo_devise_set_label( priv->devise, priv->label );
	ofo_devise_set_symbol( priv->devise, priv->symbol );
	my_utils_getback_notes_ex( OFA_BASE_DIALOG( self )->prot->dialog, devise );

	if( priv->is_new ){
		priv->updated =
				ofo_devise_insert( priv->devise, dossier );
	} else {
		priv->updated =
				ofo_devise_update( priv->devise, dossier );
	}

	return( priv->updated );
}
