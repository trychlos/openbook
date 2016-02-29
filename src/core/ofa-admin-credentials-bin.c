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

#include "api/my-utils.h"

#include "core/ofa-admin-credentials-bin.h"

/* private instance data
 */
struct _ofaAdminCredentialsBinPrivate {
	gboolean      dispose_has_run;

	/* UI
	 */
	GtkSizeGroup *group0;
	GtkWidget    *account_entry;
	GtkWidget    *password_entry;
	GtkWidget    *bis_entry;

	/* runtime data
	 */
	gchar        *account;
	gchar        *password;
	gchar        *bis;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-admin-credentials-bin.ui";

static void     setup_bin( ofaAdminCredentialsBin *bin );
static void     on_account_changed( GtkEditable *entry, ofaAdminCredentialsBin *self );
static void     on_password_changed( GtkEditable *entry, ofaAdminCredentialsBin *self );
static void     on_bis_changed( GtkEditable *entry, ofaAdminCredentialsBin *self );
static void     changed_composite( ofaAdminCredentialsBin *self );
static gboolean is_valid_composite( const ofaAdminCredentialsBin *self );

G_DEFINE_TYPE_EXTENDED( ofaAdminCredentialsBin, ofa_admin_credentials_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaAdminCredentialsBin ))

static void
admin_credentials_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_admin_credentials_bin_finalize";
	ofaAdminCredentialsBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ADMIN_CREDENTIALS_BIN( instance ));

	/* free data members here */
	priv = ofa_admin_credentials_bin_get_instance_private( OFA_ADMIN_CREDENTIALS_BIN( instance ));

	g_free( priv->account );
	g_free( priv->password );
	g_free( priv->bis );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_admin_credentials_bin_parent_class )->finalize( instance );
}

static void
admin_credentials_bin_dispose( GObject *instance )
{
	ofaAdminCredentialsBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ADMIN_CREDENTIALS_BIN( instance ));

	priv = ofa_admin_credentials_bin_get_instance_private( OFA_ADMIN_CREDENTIALS_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_admin_credentials_bin_parent_class )->dispose( instance );
}

static void
ofa_admin_credentials_bin_init( ofaAdminCredentialsBin *self )
{
	static const gchar *thisfn = "ofa_admin_credentials_bin_instance_init";
	ofaAdminCredentialsBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ADMIN_CREDENTIALS_BIN( self ));

	priv = ofa_admin_credentials_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_admin_credentials_bin_class_init( ofaAdminCredentialsBinClass *klass )
{
	static const gchar *thisfn = "ofa_admin_credentials_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = admin_credentials_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = admin_credentials_bin_finalize;

	/**
	 * ofaAdminCredentialsBin::changed:
	 *
	 * This signal is sent on the #ofaAdminCredentialsBin when one of
	 * the three entry fields (account, password or second password) is
	 * changed.
	 *
	 * Arguments are current account and password.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAdminCredentialsBin *bin,
	 * 						const gchar    *account,
	 * 						const gchar    *password,
	 * 						gpointer        user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_ADMIN_CREDENTIALS_BIN,
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
 * ofa_admin_credentials_bin_new:
 */
ofaAdminCredentialsBin *
ofa_admin_credentials_bin_new( void )
{
	ofaAdminCredentialsBin *bin;

	bin = g_object_new( OFA_TYPE_ADMIN_CREDENTIALS_BIN, NULL );

	setup_bin( bin );

	return( bin );
}

static void
setup_bin( ofaAdminCredentialsBin *bin )
{
	ofaAdminCredentialsBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *label;

	priv = ofa_admin_credentials_bin_get_instance_private( bin );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "acb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "acb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( bin ), GTK_WINDOW( toplevel ), "top" );

	priv->account_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "acb-account-entry" );
	g_return_if_fail( priv->account_entry && GTK_IS_ENTRY( priv->account_entry ));
	g_signal_connect( priv->account_entry, "changed", G_CALLBACK( on_account_changed ), bin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "acb-account-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->account_entry );

	priv->password_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "acb-password-entry" );
	g_return_if_fail( priv->password_entry && GTK_IS_ENTRY( priv->password_entry ));
	g_signal_connect( priv->password_entry, "changed", G_CALLBACK( on_password_changed ), bin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "acb-password-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->password_entry );

	priv->bis_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "acb-passbis-entry" );
	g_return_if_fail( priv->bis_entry && GTK_IS_ENTRY( priv->bis_entry ));
	g_signal_connect( priv->bis_entry, "changed", G_CALLBACK( on_bis_changed ), bin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "acb-passbis-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->bis_entry );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

/**
 * ofa_admin_credentials_bin_get_size_group:
 * @bin: this #ofaAdminCredentialsBin instance.
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
ofa_admin_credentials_bin_get_size_group( const ofaAdminCredentialsBin *bin, guint column )
{
	static const gchar *thisfn = "ofa_admin_credentials_bin_get_size_group";
	ofaAdminCredentialsBinPrivate *priv;

	g_debug( "%s: bin=%p, column=%u", thisfn, ( void * ) bin, column );

	g_return_val_if_fail( bin && OFA_IS_ADMIN_CREDENTIALS_BIN( bin ), NULL );

	priv = ofa_admin_credentials_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}

	return( NULL );
}

/**
 * ofa_admin_credentials_bin_grab_focus:
 * @bin: this #ofaAdminCredentialsBin instance.
 *
 * Set the focus.
 */
void
ofa_admin_credentials_bin_grab_focus( const ofaAdminCredentialsBin *bin )
{
	static const gchar *thisfn = "ofa_admin_credentials_bin_grab_focus";
	ofaAdminCredentialsBinPrivate *priv;

	g_debug( "%s: bin=%p", thisfn, ( void * ) bin );

	g_return_if_fail( bin && OFA_IS_ADMIN_CREDENTIALS_BIN( bin ));

	priv = ofa_admin_credentials_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	if( my_strlen( priv->account )){
		if( my_strlen( priv->password )){
			if( my_strlen( priv->bis )){
				gtk_widget_grab_focus( priv->account_entry );
			} else {
				gtk_widget_grab_focus( priv->bis_entry );
			}
		} else {
			gtk_widget_grab_focus( priv->password_entry );
		}
	} else {
		gtk_widget_grab_focus( priv->account_entry );
	}
}

static void
on_account_changed( GtkEditable *entry, ofaAdminCredentialsBin *self )
{
	ofaAdminCredentialsBinPrivate *priv;

	priv = ofa_admin_credentials_bin_get_instance_private( self );

	g_free( priv->account );
	priv->account = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	changed_composite( self );
}

static void
on_password_changed( GtkEditable *entry, ofaAdminCredentialsBin *self )
{
	ofaAdminCredentialsBinPrivate *priv;

	priv = ofa_admin_credentials_bin_get_instance_private( self );

	g_free( priv->password );
	priv->password = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	changed_composite( self );
}

static void
on_bis_changed( GtkEditable *entry, ofaAdminCredentialsBin *self )
{
	ofaAdminCredentialsBinPrivate *priv;

	priv = ofa_admin_credentials_bin_get_instance_private( self );

	g_free( priv->bis );
	priv->bis = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	changed_composite( self );
}

/*
 * check that all fields are set, and that the two passwords are equal
 */
static void
changed_composite( ofaAdminCredentialsBin *self )
{
	ofaAdminCredentialsBinPrivate *priv;

	priv = ofa_admin_credentials_bin_get_instance_private( self );

	g_signal_emit_by_name( self, "ofa-changed", priv->account, priv->password );
}

/**
 * ofa_admin_credentials_bin_is_valid:
 * @bin:
 * @error_message: [allow-none]: set to the error message as a newly
 *  allocated string which should be g_free() by the caller.
 *
 * Returns: %TRUE if the composite widget is valid: both account and
 * password are set, password is repeated twice and are equal.
 */
gboolean
ofa_admin_credentials_bin_is_valid( const ofaAdminCredentialsBin *bin, gchar **error_message )
{
	ofaAdminCredentialsBinPrivate *priv;
	gboolean is_valid;

	g_return_val_if_fail( bin && OFA_IS_ADMIN_CREDENTIALS_BIN( bin ), FALSE );

	priv = ofa_admin_credentials_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	is_valid = is_valid_composite( bin );

	if( error_message ){
		*error_message = is_valid ?
				NULL :
				g_strdup( _( "Dossier administrative credentials are not valid" ));
	}

	return( is_valid );
}

static gboolean
is_valid_composite( const ofaAdminCredentialsBin *bin )
{
	ofaAdminCredentialsBinPrivate *priv;
	gboolean ok;

	priv = ofa_admin_credentials_bin_get_instance_private( bin );

	ok = my_strlen( priv->account ) &&
			my_strlen( priv->password ) &&
			my_strlen( priv->bis ) &&
			!g_utf8_collate( priv->password, priv->bis );

	return( ok );
}
