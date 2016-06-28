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

#include "my/my-utils.h"

#include "ui/ofa-user-credentials-bin.h"

/* private instance data
 */
typedef struct {
	gboolean      dispose_has_run;

	/* UI
	 */
	GtkSizeGroup *group0;
	GtkWidget    *account_entry;
	GtkWidget    *password_entry;

	/* runtime data
	 */
	gchar        *account;
	gchar        *password;
}
	ofaUserCredentialsBinPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-user-credentials-bin.ui";

static void     setup_bin( ofaUserCredentialsBin *self );
static void     on_account_changed( GtkEditable *entry, ofaUserCredentialsBin *self );
static void     on_password_changed( GtkEditable *entry, ofaUserCredentialsBin *self );
static void     changed_composite( ofaUserCredentialsBin *self );
static gboolean is_valid_composite( ofaUserCredentialsBin *self );

G_DEFINE_TYPE_EXTENDED( ofaUserCredentialsBin, ofa_user_credentials_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaUserCredentialsBin ))

static void
user_credentials_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_user_credentials_bin_finalize";
	ofaUserCredentialsBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_USER_CREDENTIALS_BIN( instance ));

	/* free data members here */
	priv = ofa_user_credentials_bin_get_instance_private( OFA_USER_CREDENTIALS_BIN( instance ));

	g_free( priv->account );
	g_free( priv->password );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_user_credentials_bin_parent_class )->finalize( instance );
}

static void
user_credentials_bin_dispose( GObject *instance )
{
	ofaUserCredentialsBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_USER_CREDENTIALS_BIN( instance ));

	priv = ofa_user_credentials_bin_get_instance_private( OFA_USER_CREDENTIALS_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_user_credentials_bin_parent_class )->dispose( instance );
}

static void
ofa_user_credentials_bin_init( ofaUserCredentialsBin *self )
{
	static const gchar *thisfn = "ofa_user_credentials_bin_init";
	ofaUserCredentialsBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_USER_CREDENTIALS_BIN( self ));

	priv = ofa_user_credentials_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_user_credentials_bin_class_init( ofaUserCredentialsBinClass *klass )
{
	static const gchar *thisfn = "ofa_user_credentials_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = user_credentials_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = user_credentials_bin_finalize;

	/**
	 * ofaUserCredentialsBin::changed:
	 *
	 * This signal is sent on the #ofaUserCredentialsBin when one of
	 * the entry fields (account, password or second password) is changed.
	 *
	 * Arguments are account and password.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaUserCredentialsBin *self,
	 * 						const gchar         *account,
	 * 						const gchar         *password,
	 * 						gpointer             user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_USER_CREDENTIALS_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_STRING, G_TYPE_STRING );
}

/**
 * ofa_user_credentials_bin_new:
 */
ofaUserCredentialsBin *
ofa_user_credentials_bin_new( void )
{
	ofaUserCredentialsBin *self;

	self = g_object_new( OFA_TYPE_USER_CREDENTIALS_BIN, NULL );

	setup_bin( self );

	return( self );
}

static void
setup_bin( ofaUserCredentialsBin *self )
{
	ofaUserCredentialsBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *entry, *label;

	priv = ofa_user_credentials_bin_get_instance_private( self );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "ucb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "ucb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ucb-account-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect(
			G_OBJECT( entry ), "changed", G_CALLBACK( on_account_changed ), self );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ucb-account-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );
	priv->account_entry = entry;

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ucb-password-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect(
			G_OBJECT( entry ), "changed", G_CALLBACK( on_password_changed ), self );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ucb-password-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );
	priv->password_entry = entry;

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

/**
 * ofa_user_credentials_bin_get_size_group:
 * @bin: this #ofaUserCredentialsBin instance.
 * @column: the desired column number, counted from zero.
 *
 * Returns: the #GtkSizeGroup used to horizontally align the @column.
 *
 * As this is a composite widget, it is probable that we will want align
 * it with other composites or widgets in a dialog box. Having a size
 * group prevents us to have to determine the longest label, which
 * should be computed dynamically as this may depend of the translation.
 *
 * Here, the .xml UI definition defines a dedicated GtkSizeGroup that
 * we have just to return as is.
 */
GtkSizeGroup *
ofa_user_credentials_bin_get_size_group( ofaUserCredentialsBin *bin, guint column )
{
	static const gchar *thisfn = "ofa_user_credentials_bin_get_size_group";
	ofaUserCredentialsBinPrivate *priv;

	g_debug( "%s: bin=%p, column=%u", thisfn, ( void * ) bin, column );

	g_return_val_if_fail( bin && OFA_IS_USER_CREDENTIALS_BIN( bin ), NULL );

	priv = ofa_user_credentials_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}

	g_warning( "%s: unknown column=%u", thisfn, column );
	return( NULL );
}

/**
 * ofa_user_credentials_bin_grab_focus:
 * @bin: this #ofaUserCredentialsBin instance.
 *
 * Set the focus on the account entry.
 * If this one is already set, then set the focus on the password entry.
 */
void
ofa_user_credentials_bin_grab_focus( ofaUserCredentialsBin *bin )
{
	static const gchar *thisfn = "ofa_user_credentials_bin_grab_focus";
	ofaUserCredentialsBinPrivate *priv;

	g_debug( "%s: bin=%p", thisfn, ( void * ) bin );

	g_return_if_fail( bin && OFA_IS_USER_CREDENTIALS_BIN( bin ));

	priv = ofa_user_credentials_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	gtk_widget_grab_focus(
			my_strlen( priv->account ) ? priv->password_entry : priv->account_entry );
}

static void
on_account_changed( GtkEditable *entry, ofaUserCredentialsBin *self )
{
	ofaUserCredentialsBinPrivate *priv;

	priv = ofa_user_credentials_bin_get_instance_private( self );

	g_free( priv->account );
	priv->account = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	changed_composite( self );
}

static void
on_password_changed( GtkEditable *entry, ofaUserCredentialsBin *self )
{
	ofaUserCredentialsBinPrivate *priv;

	priv = ofa_user_credentials_bin_get_instance_private( self );

	g_free( priv->password );
	priv->password = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	changed_composite( self );
}

/*
 * check that all fields are set, and that the two passwords are equal
 */
static void
changed_composite( ofaUserCredentialsBin *self )
{
	ofaUserCredentialsBinPrivate *priv;

	priv = ofa_user_credentials_bin_get_instance_private( self );

	g_signal_emit_by_name( self, "ofa-changed", priv->account, priv->password );
}

/**
 * ofa_user_credentials_bin_set_account:
 * @bin: this #ofaUserCredentialsBin instance.
 * @account: the account to be set.
 *
 * Set the account.
 */
void
ofa_user_credentials_bin_set_account( ofaUserCredentialsBin *bin, const gchar *account )
{
	ofaUserCredentialsBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_USER_CREDENTIALS_BIN( bin ));

	priv = ofa_user_credentials_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), account );
}

/**
 * ofa_user_credentials_bin_set_password:
 * @bin: this #ofaUserCredentialsBin instance.
 * @password: the password to be set.
 *
 * Set the password.
 */
void
ofa_user_credentials_bin_set_password( ofaUserCredentialsBin *bin, const gchar *password )
{
	ofaUserCredentialsBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_USER_CREDENTIALS_BIN( bin ));

	priv = ofa_user_credentials_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	gtk_entry_set_text( GTK_ENTRY( priv->password_entry ), password );
}

/**
 * ofa_user_credentials_bin_is_valid:
 * @bin: this #ofaUserCredentialsBin instance.
 * @msgerr: [allow-none]: set to the error message as a newly
 *  allocated string which should be g_free() by the caller.
 *
 * Returns: %TRUE if the widget is valid, i.e. if both account and
 * password are set.
 */
gboolean
ofa_user_credentials_bin_is_valid( ofaUserCredentialsBin *bin, gchar **msgerr )
{
	ofaUserCredentialsBinPrivate *priv;
	gboolean is_valid;

	g_return_val_if_fail( bin && OFA_IS_USER_CREDENTIALS_BIN( bin ), FALSE );

	priv = ofa_user_credentials_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	is_valid = is_valid_composite( bin );

	if( msgerr ){
		*msgerr = is_valid ?
				NULL :
				g_strdup( _( "User credentials are not valid" ));
	}

	return( is_valid );
}

static gboolean
is_valid_composite( ofaUserCredentialsBin *self )
{
	ofaUserCredentialsBinPrivate *priv;
	gboolean ok;

	priv = ofa_user_credentials_bin_get_instance_private( self );

	ok = my_strlen( priv->account ) &&
			my_strlen( priv->password );

	return( ok );
}
