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
#include <stdarg.h>

#include "my/my-idialog.h"
#include "my/my-igridlist.h"
#include "my/my-iwindow.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-account-editable.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-preferences.h"
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-dossier.h"
#include "api/ofo-paimean.h"

#include "core/ofa-paimean-properties.h"

/* private instance data
 */
typedef struct {
	gboolean       dispose_has_run;

	/* initialization
	 */
	ofaIGetter    *getter;
	GtkWindow     *parent;
	ofoPaimean    *paimean;

	/* runtime
	 */
	ofaHub        *hub;
	gboolean       is_writable;
	gboolean       is_new;

	/* UI
	 */
	GtkWidget     *code_entry;
	GtkWidget     *label_entry;
	GtkWidget     *account_entry;
	GtkWidget     *account_label;
	GtkWidget     *ok_btn;
	GtkWidget     *msg_label;
}
	ofaPaimeanPropertiesPrivate;

static const gchar *st_style_error      = "labelerror";
static const gchar *st_style_warning    = "labelwarning";
static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-paimean-properties.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static gchar   *iwindow_get_identifier( const myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     init_dialog( ofaPaimeanProperties *self );
static void     init_properties( ofaPaimeanProperties *self );
static void     set_properties( ofaPaimeanProperties *self );
static void     on_code_changed( GtkEntry *entry, ofaPaimeanProperties *self );
static void     on_label_changed( GtkEntry *entry, ofaPaimeanProperties *self );
static void     on_account_changed( GtkEntry *entry, ofaPaimeanProperties *self );
static void     check_for_enable_dlg( ofaPaimeanProperties *self );
static gboolean is_dialog_validable( ofaPaimeanProperties *self );
static gboolean do_update( ofaPaimeanProperties *self, gchar **msgerr );
static void     set_msgerr( ofaPaimeanProperties *self, const gchar *msg, const gchar *style );

G_DEFINE_TYPE_EXTENDED( ofaPaimeanProperties, ofa_paimean_properties, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaPaimeanProperties )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
paimean_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_paimean_properties_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PAIMEAN_PROPERTIES( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_paimean_properties_parent_class )->finalize( instance );
}

static void
paimean_properties_dispose( GObject *instance )
{
	ofaPaimeanPropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_PAIMEAN_PROPERTIES( instance ));

	priv = ofa_paimean_properties_get_instance_private( OFA_PAIMEAN_PROPERTIES( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_paimean_properties_parent_class )->dispose( instance );
}

static void
ofa_paimean_properties_init( ofaPaimeanProperties *self )
{
	static const gchar *thisfn = "ofa_paimean_properties_init";
	ofaPaimeanPropertiesPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( OFA_IS_PAIMEAN_PROPERTIES( self ));

	priv = ofa_paimean_properties_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->is_new = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_paimean_properties_class_init( ofaPaimeanPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_paimean_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = paimean_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = paimean_properties_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_paimean_properties_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 * @paimean: the #ofoPaimean to be displayed/updated.
 *
 * Update the properties of a paimean.
 */
void
ofa_paimean_properties_run( ofaIGetter *getter, GtkWindow *parent, ofoPaimean *paimean )
{
	static const gchar *thisfn = "ofa_paimean_properties_run";
	ofaPaimeanProperties *self;
	ofaPaimeanPropertiesPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p, paimean=%p",
			thisfn, ( void * ) getter, ( void * ) parent, ( void * ) paimean );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_PAIMEAN_PROPERTIES, NULL );

	priv = ofa_paimean_properties_get_instance_private( self );

	priv->getter = ofa_igetter_get_permanent_getter( getter );
	priv->parent = parent;
	priv->paimean = paimean;

	/* run modal or non-modal depending of the parent */
	my_idialog_run_maybe_modal( MY_IDIALOG( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_paimean_properties_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
	iface->get_identifier = iwindow_get_identifier;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_paimean_properties_iwindow_init";
	ofaPaimeanPropertiesPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_paimean_properties_get_instance_private( OFA_PAIMEAN_PROPERTIES( instance ));

	my_iwindow_set_parent( instance, priv->parent );

	priv->hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	my_iwindow_set_settings( instance, ofa_hub_get_user_settings( priv->hub ));
}

/*
 * identifier is built with class name and paimean code
 */
static gchar *
iwindow_get_identifier( const myIWindow *instance )
{
	ofaPaimeanPropertiesPrivate *priv;
	gchar *id;

	priv = ofa_paimean_properties_get_instance_private( OFA_PAIMEAN_PROPERTIES( instance ));

	id = g_strdup_printf( "%s-%s",
				G_OBJECT_TYPE_NAME( instance ),
				ofo_paimean_get_code( priv->paimean ));

	return( id );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_paimean_properties_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

/*
 * this dialog is subject to 'is_writable' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * paimean
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_paimean_properties_idialog_init";
	ofaPaimeanPropertiesPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_paimean_properties_get_instance_private( OFA_PAIMEAN_PROPERTIES( instance ));

	init_dialog( OFA_PAIMEAN_PROPERTIES( instance ));
	init_properties( OFA_PAIMEAN_PROPERTIES( instance ));
	set_properties( OFA_PAIMEAN_PROPERTIES( instance ));

	my_utils_container_set_editable( GTK_CONTAINER( instance ), priv->is_writable );

	/* if not the current exercice, then only have a 'Close' button */
	if( !priv->is_writable ){
		my_idialog_set_close_button( instance );
		priv->ok_btn = NULL;
	}

	check_for_enable_dlg( OFA_PAIMEAN_PROPERTIES( instance ));
}

static void
init_dialog( ofaPaimeanProperties *self )
{
	ofaPaimeanPropertiesPrivate *priv;
	const gchar *code;
	gchar *title;

	priv = ofa_paimean_properties_get_instance_private( self );

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	my_idialog_click_to_update( MY_IDIALOG( self ), priv->ok_btn, ( myIDialogUpdateCb ) do_update );

	priv->is_writable = ofa_hub_dossier_is_writable( priv->hub );

	code = ofo_paimean_get_code( priv->paimean );
	if( !code ){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new mean of paiement" ));
	} else {
		title = g_strdup_printf( _( "Updating « %s » mean of paiement" ), code );
	}
	gtk_window_set_title( GTK_WINDOW( self ), title );

	my_utils_container_notes_init( self, paimean );
	my_utils_container_updstamp_init( self, paimean );
}

static void
init_properties( ofaPaimeanProperties *self )
{
	ofaPaimeanPropertiesPrivate *priv;
	GtkWidget *prompt, *entry, *label;

	priv = ofa_paimean_properties_get_instance_private( self );

	/* code */
	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-code-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-code-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );
	g_signal_connect( entry, "changed", G_CALLBACK( on_code_changed ), self );
	priv->code_entry = entry;

	/* label */
	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );
	g_signal_connect( entry, "changed", G_CALLBACK( on_label_changed ), self );
	priv->label_entry = entry;

	/* account */
	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-account-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-account-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-account-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );
	ofa_account_editable_init( GTK_EDITABLE( entry ), priv->getter, ACCOUNT_ALLOW_ALL );
	g_signal_connect( entry, "changed", G_CALLBACK( on_account_changed ), self );
	priv->account_entry = entry;
	priv->account_label = label;
}

static void
set_properties( ofaPaimeanProperties *self )
{
	ofaPaimeanPropertiesPrivate *priv;
	const gchar *cstr;

	priv = ofa_paimean_properties_get_instance_private( self );

	/* code */
	cstr = ofo_paimean_get_code( priv->paimean );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->code_entry ), cstr );
	}

	/* label */
	cstr = ofo_paimean_get_label( priv->paimean );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->label_entry ), cstr );
	}

	/* account */
	cstr = ofo_paimean_get_account( priv->paimean );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), cstr );
	}
}

static void
on_code_changed( GtkEntry *entry, ofaPaimeanProperties *self )
{
	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaPaimeanProperties *self )
{
	check_for_enable_dlg( self );
}

static void
on_account_changed( GtkEntry *entry, ofaPaimeanProperties *self )
{
	ofaPaimeanPropertiesPrivate *priv;
	const gchar *number, *label;
	ofoAccount *account;

	priv = ofa_paimean_properties_get_instance_private( self );

	number = gtk_entry_get_text( entry );
	account = my_strlen( number ) ? ofo_account_get_by_number( priv->hub, number ) : NULL;
	label = account ? ofo_account_get_label( account ) : "";
	gtk_label_set_text( GTK_LABEL( priv->account_label ), label );

	check_for_enable_dlg( self );
}

/*
 * are we able to validate this #ofoPaimean ?
 */
static void
check_for_enable_dlg( ofaPaimeanProperties *self )
{
	ofaPaimeanPropertiesPrivate *priv;

	priv = ofa_paimean_properties_get_instance_private( self );

	if( priv->is_writable ){
		gtk_widget_set_sensitive( priv->ok_btn, is_dialog_validable( self ));
	}
}

static gboolean
is_dialog_validable( ofaPaimeanProperties *self )
{
	ofaPaimeanPropertiesPrivate *priv;
	const gchar *code, *number;
	gchar *msgerr;
	gboolean ok;
	ofoPaimean *exists;
	ofoAccount *account;

	priv = ofa_paimean_properties_get_instance_private( self );

	code = gtk_entry_get_text( GTK_ENTRY( priv->code_entry ));

	ok = ofo_paimean_is_valid_data( code, &msgerr );

	if( ok ){
		exists = ofo_paimean_get_by_code( priv->hub, code );
		ok &= !exists ||
				( !priv->is_new && !my_collate( code, ofo_paimean_get_code( priv->paimean )));
		if( !ok ){
			msgerr = g_strdup( _( "Mean of paiement already exists" ));
		}
	}

	set_msgerr( self, msgerr, st_style_error );
	g_free( msgerr );

	/* if not error, check if the account exists (and is a detail account)
	 * but does not prevent to save */
	if( ok ){
		number = gtk_entry_get_text( GTK_ENTRY( priv->account_entry ));
		account = my_strlen( number ) ? ofo_account_get_by_number( priv->hub, number ) : NULL;
		if( !account ){
			msgerr = g_strdup( _( "Account does not exist" ));

		} else if( ofo_account_is_closed( account )){
			msgerr = g_strdup_printf( _( "Account %s is closed" ), number );

		} else if( ofo_account_is_root( account )){
			msgerr = g_strdup_printf( _( "Account %s is a root account" ), number );
		}
		if( msgerr ){
			set_msgerr( self, msgerr, st_style_warning );
			g_free( msgerr );
		}
	}

	return( ok );
}

/*
 * either creating a new paimean (prev_code is empty)
 * or updating an existing one, and prev_code may have been modified
 * Please note that a record is uniquely identified by the code
 */
static gboolean
do_update( ofaPaimeanProperties *self, gchar **msgerr )
{
	ofaPaimeanPropertiesPrivate *priv;
	gchar *prev_code;
	gboolean ok;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = ofa_paimean_properties_get_instance_private( self );

	prev_code = g_strdup( ofo_paimean_get_code( priv->paimean ));

	ofo_paimean_set_code( priv->paimean, gtk_entry_get_text( GTK_ENTRY( priv->code_entry )));
	ofo_paimean_set_label( priv->paimean, gtk_entry_get_text( GTK_ENTRY( priv->label_entry )));
	ofo_paimean_set_account( priv->paimean, gtk_entry_get_text( GTK_ENTRY( priv->account_entry )));
	my_utils_container_notes_get( self, paimean );

	if( priv->is_new ){
		ok = ofo_paimean_insert( priv->paimean, priv->hub );
		if( !ok ){
			*msgerr = g_strdup( _( "Unable to create this new mean of paiement" ));
		}
	} else {
		ok = ofo_paimean_update( priv->paimean, prev_code );
		if( !ok ){
			*msgerr = g_strdup( _( "Unable to update the mean of paiement" ));
		}
	}

	g_free( prev_code );

	return( ok );
}

static void
set_msgerr( ofaPaimeanProperties *self, const gchar *msg, const gchar *style )
{
	ofaPaimeanPropertiesPrivate *priv;
	GtkWidget *label;

	priv = ofa_paimean_properties_get_instance_private( self );

	if( !priv->msg_label ){
		label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "px-msgerr" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		priv->msg_label = label;
	}

	my_style_remove( priv->msg_label, st_style_error );
	my_style_remove( priv->msg_label, st_style_warning );

	my_style_add( priv->msg_label, style );
	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}
