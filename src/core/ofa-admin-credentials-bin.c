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

#include "api/my-utils.h"

#include "core/ofa-admin-credentials-bin.h"

/* private instance data
 */
struct _ofaAdminCredentialsBinPrivate {
	gboolean      dispose_has_run;

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

static const gchar *st_bin_xml          = PKGUIDIR "/ofa-admin-credentials-bin.ui";
static const gchar *st_bin_id           = "AdminCredentialsBin";

G_DEFINE_TYPE( ofaAdminCredentialsBin, ofa_admin_credentials_bin, GTK_TYPE_BIN )

static void     setup_composite( ofaAdminCredentialsBin *bin );
static void     on_account_changed( GtkEditable *entry, ofaAdminCredentialsBin *self );
static void     on_password_changed( GtkEditable *entry, ofaAdminCredentialsBin *self );
static void     on_bis_changed( GtkEditable *entry, ofaAdminCredentialsBin *self );
static void     changed_composite( ofaAdminCredentialsBin *self );
static gboolean is_valid_composite( const ofaAdminCredentialsBin *self );

static void
admin_credentials_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_admin_credentials_bin_finalize";
	ofaAdminCredentialsBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ADMIN_CREDENTIALS_BIN( instance ));

	/* free data members here */
	priv = OFA_ADMIN_CREDENTIALS_BIN( instance )->priv;

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

	priv = OFA_ADMIN_CREDENTIALS_BIN( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_admin_credentials_bin_parent_class )->dispose( instance );
}

static void
ofa_admin_credentials_bin_init( ofaAdminCredentialsBin *self )
{
	static const gchar *thisfn = "ofa_admin_credentials_bin_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ADMIN_CREDENTIALS_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
							self, OFA_TYPE_ADMIN_CREDENTIALS_BIN, ofaAdminCredentialsBinPrivate );
}

static void
ofa_admin_credentials_bin_class_init( ofaAdminCredentialsBinClass *klass )
{
	static const gchar *thisfn = "ofa_admin_credentials_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = admin_credentials_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = admin_credentials_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaAdminCredentialsBinPrivate ));

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

	setup_composite( bin );

	return( bin );
}

static void
setup_composite( ofaAdminCredentialsBin *bin )
{
	GtkWidget *top_widget, *entry, *label;

	top_widget = my_utils_container_attach_from_ui( GTK_CONTAINER( bin ), st_bin_xml, st_bin_id, "top" );
	g_return_if_fail( top_widget && GTK_IS_CONTAINER( top_widget ));

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "adm-account" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect(
			G_OBJECT( entry ), "changed", G_CALLBACK( on_account_changed ), bin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "adm-label11" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "adm-password" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect(
			G_OBJECT( entry ), "changed", G_CALLBACK( on_password_changed ), bin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "adm-label12" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "adm-passbis" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect(
			G_OBJECT( entry ), "changed", G_CALLBACK( on_bis_changed ), bin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "adm-label13" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );
}

static void
on_account_changed( GtkEditable *entry, ofaAdminCredentialsBin *self )
{
	ofaAdminCredentialsBinPrivate *priv;

	priv = self->priv;

	g_free( priv->account );
	priv->account = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	changed_composite( self );
}

static void
on_password_changed( GtkEditable *entry, ofaAdminCredentialsBin *self )
{
	ofaAdminCredentialsBinPrivate *priv;

	priv = self->priv;

	g_free( priv->password );
	priv->password = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	changed_composite( self );
}

static void
on_bis_changed( GtkEditable *entry, ofaAdminCredentialsBin *self )
{
	ofaAdminCredentialsBinPrivate *priv;

	priv = self->priv;

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

	priv = self->priv;

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

	priv = bin->priv;
	is_valid = FALSE;

	if( !priv->dispose_has_run ){

		is_valid = is_valid_composite( bin );

		if( error_message ){
			*error_message = is_valid ?
					NULL :
					g_strdup( _( "Dossier administrative credentials are not valid" ));
		}
	}

	return( is_valid );
}

static gboolean
is_valid_composite( const ofaAdminCredentialsBin *bin )
{
	ofaAdminCredentialsBinPrivate *priv;
	gboolean ok;

	priv = bin->priv;

	ok = my_strlen( priv->account ) &&
			my_strlen( priv->password ) &&
			my_strlen( priv->bis ) &&
			!g_utf8_collate( priv->password, priv->bis );

	return( ok );
}
