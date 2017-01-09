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

#include "my/my-utils.h"

#include "api/ofa-hub.h"

#include "ui/ofa-admin-credentials-bin.h"

/* private instance data
 */
typedef struct {
	gboolean      dispose_has_run;

	/* initialization
	 */
	ofaHub       *hub;
	gchar        *settings_prefix;

	/* UI
	 */
	GtkSizeGroup *group0;
	GtkWidget    *account_entry;
	GtkWidget    *remember_btn;
	GtkWidget    *password_entry;
	GtkWidget    *bis_entry;

	/* runtime
	 */
	gchar        *account;
	gchar        *password;
	gchar        *bis;
}
	ofaAdminCredentialsBinPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-admin-credentials-bin.ui";

static void     setup_bin( ofaAdminCredentialsBin *bin );
static void     on_account_changed( GtkEditable *entry, ofaAdminCredentialsBin *self );
static void     on_password_changed( GtkEditable *entry, ofaAdminCredentialsBin *self );
static void     on_bis_changed( GtkEditable *entry, ofaAdminCredentialsBin *self );
static void     changed_composite( ofaAdminCredentialsBin *self );
static gboolean is_valid_composite( ofaAdminCredentialsBin *self );
static void     read_settings( ofaAdminCredentialsBin *self );
static void     write_settings( ofaAdminCredentialsBin *self );

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

	g_free( priv->settings_prefix );
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

		write_settings( OFA_ADMIN_CREDENTIALS_BIN( instance ));

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
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_admin_credentials_bin_class_init( ofaAdminCredentialsBinClass *klass )
{
	static const gchar *thisfn = "ofa_admin_credentials_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = admin_credentials_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = admin_credentials_bin_finalize;

	/**
	 * ofaAdminCredentialsBin::ofa-changed:
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
 * @hub: the #ofaHub object of the application.
 * @settings_prefix: the prefix of the key in user settings.
 *
 * Returns: a new #ofaAdminCredentialsBin widget.
 */
ofaAdminCredentialsBin *
ofa_admin_credentials_bin_new( ofaHub *hub, const gchar *settings_prefix )
{
	ofaAdminCredentialsBin *bin;
	ofaAdminCredentialsBinPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	bin = g_object_new( OFA_TYPE_ADMIN_CREDENTIALS_BIN, NULL );

	priv = ofa_admin_credentials_bin_get_instance_private( bin );

	priv->hub = hub;

	g_free( priv->settings_prefix );
	priv->settings_prefix = g_strdup( settings_prefix );

	setup_bin( bin );
	read_settings( bin );

	return( bin );
}

static void
setup_bin( ofaAdminCredentialsBin *bin )
{
	ofaAdminCredentialsBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *label, *btn;

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

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "acb-remember-btn" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	priv->remember_btn = btn;

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
ofa_admin_credentials_bin_get_size_group( ofaAdminCredentialsBin *bin, guint column )
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
ofa_admin_credentials_bin_is_valid( ofaAdminCredentialsBin *bin, gchar **error_message )
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
is_valid_composite( ofaAdminCredentialsBin *bin )
{
	ofaAdminCredentialsBinPrivate *priv;
	gboolean ok;

	priv = ofa_admin_credentials_bin_get_instance_private( bin );

	ok = my_strlen( priv->account ) &&
			my_strlen( priv->password ) &&
			my_strlen( priv->bis ) &&
			!my_collate( priv->password, priv->bis );

	return( ok );
}

/**
 * ofa_admin_credentials_bin_get_remembered_account:
 * @bin: this #ofaAdminCredentialsBin instance.
 *
 * Returns: the administrative account which should be remembered, or %NULL.
 */
const gchar *
ofa_admin_credentials_bin_get_remembered_account( ofaAdminCredentialsBin *bin )
{
	ofaAdminCredentialsBinPrivate *priv;
	gboolean remember;

	g_return_val_if_fail( bin && OFA_IS_ADMIN_CREDENTIALS_BIN( bin ), NULL );

	priv = ofa_admin_credentials_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	remember = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->remember_btn ));

	return(( const gchar * )( remember ? priv->account : NULL ));
}

/**
 * ofa_admin_credentials_bin_get_credentials:
 * @bin: this #ofaAdminCredentialsBin instance.
 * @account: [out]: a placeholder for the account.
 * @password: [out]: a placeholder for the password.
 *
 * Set the provided placeholders to their respective value.
 */
void
ofa_admin_credentials_bin_get_credentials( ofaAdminCredentialsBin *bin, gchar **account, gchar **password )
{
	ofaAdminCredentialsBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_ADMIN_CREDENTIALS_BIN( bin ));
	g_return_if_fail( account );
	g_return_if_fail( password );

	priv = ofa_admin_credentials_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	*account = g_strdup( priv->account );
	*password = g_strdup( priv->password );
}

/*
 * settings are: remember_admin_account(b);
 */
static void
read_settings( ofaAdminCredentialsBin *self )
{
	ofaAdminCredentialsBinPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;
	gchar *key;

	priv = ofa_admin_credentials_bin_get_instance_private( self );

	settings = ofa_hub_get_user_settings( priv->hub );
	key = g_strdup_printf( "%s-admin-credentials", priv->settings_prefix );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, key );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->remember_btn ), my_utils_boolean_from_str( cstr ));
	}

	my_isettings_free_string_list( settings, strlist );
	g_free( key );
}

static void
write_settings( ofaAdminCredentialsBin *self )
{
	ofaAdminCredentialsBinPrivate *priv;
	myISettings *settings;
	gchar *key, *str;

	priv = ofa_admin_credentials_bin_get_instance_private( self );

	str = g_strdup_printf( "%s;",
				gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->remember_btn )) ? "True":"False" );

	settings = ofa_hub_get_user_settings( priv->hub );
	key = g_strdup_printf( "%s-admin-credentials", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( key );
	g_free( str );
}
