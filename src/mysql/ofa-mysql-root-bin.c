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

#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbprovider.h"

#include "mysql/ofa-mysql-dossier-meta.h"
#include "mysql/ofa-mysql-root-bin.h"

/* private instance data
 */
typedef struct {
	gboolean            dispose_has_run;

	/* initialization
	 */
	ofaMysqlDBProvider *provider;
	guint               rule;

	/* UI
	 */
	GtkWidget          *account_entry;
	GtkWidget          *password_entry;
	GtkWidget          *msg;
	GtkSizeGroup       *group0;
	GtkWidget          *revealer;

	/* runtime data
	 */
	ofaIDBDossierMeta  *dossier_meta;
	gchar              *account;
	gboolean            remember;
	gchar              *password;
}
	ofaMysqlRootBinPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/mysql/ofa-mysql-root-bin.ui";

static void     setup_bin( ofaMysqlRootBin *bin );
static void     on_account_changed( GtkEditable *entry, ofaMysqlRootBin *self );
static void     on_remember_toggled( GtkToggleButton *button, ofaMysqlRootBin *self );
static void     on_password_changed( GtkEditable *entry, ofaMysqlRootBin *self );
static void     changed_composite( ofaMysqlRootBin *self );
static gboolean is_valid( ofaMysqlRootBin *self, gchar **str );

G_DEFINE_TYPE_EXTENDED( ofaMysqlRootBin, ofa_mysql_root_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaMysqlRootBin ))

static void
mysql_root_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_root_bin_finalize";
	ofaMysqlRootBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MYSQL_ROOT_BIN( instance ));

	/* free data members here */
	priv = ofa_mysql_root_bin_get_instance_private( OFA_MYSQL_ROOT_BIN( instance ));

	g_free( priv->account );
	g_free( priv->password );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_root_bin_parent_class )->finalize( instance );
}

static void
mysql_root_bin_dispose( GObject *instance )
{
	ofaMysqlRootBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_ROOT_BIN( instance ));

	priv = ofa_mysql_root_bin_get_instance_private( OFA_MYSQL_ROOT_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_root_bin_parent_class )->dispose( instance );
}

static void
ofa_mysql_root_bin_init( ofaMysqlRootBin *self )
{
	static const gchar *thisfn = "ofa_mysql_root_bin_instance_init";
	ofaMysqlRootBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MYSQL_ROOT_BIN( self ));

	priv = ofa_mysql_root_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->account = NULL;
	priv->password = NULL;
}

static void
ofa_mysql_root_bin_class_init( ofaMysqlRootBinClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_root_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_root_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_root_bin_finalize;

	/**
	 * ofaMysqlRootBin::changed:
	 *
	 * This signal is sent on the #ofaMysqlRootBin when the account or
	 * the password are changed.
	 *
	 * There is no argument.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaMysqlRootBin *bin,
	 * 						gpointer       user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_MYSQL_ROOT_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0,
				G_TYPE_NONE );
}

/**
 * ofa_mysql_root_bin_new:
 * @provider: the #ofaMysqlDBProvider instance.
 * @rule: the usage of this widget.
 *
 * Returns: a new #ofaMysqlRootBin widget.
 */
ofaMysqlRootBin *
ofa_mysql_root_bin_new( ofaMysqlDBProvider *provider, guint rule )
{
	static const gchar *thisfn = "ofa_mysql_root_bin_new";
	ofaMysqlRootBin *bin;
	ofaMysqlRootBinPrivate *priv;

	g_debug( "%s: provider=%p (%s), rule=%u",
			thisfn, ( void * ) provider, G_OBJECT_TYPE_NAME( provider ), rule );

	g_return_val_if_fail( provider && OFA_IS_MYSQL_DBPROVIDER( provider ), NULL );

	bin = g_object_new( OFA_TYPE_MYSQL_ROOT_BIN, NULL );

	priv = ofa_mysql_root_bin_get_instance_private( bin );

	priv->provider = provider;
	priv->rule = rule;

	setup_bin( bin );

	return( bin );
}

static void
setup_bin( ofaMysqlRootBin *self )
{
	ofaMysqlRootBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *label, *btn;

	priv = ofa_mysql_root_bin_get_instance_private( self );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "mrb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "mrb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	/* connect to the 'changed' signal of the entry */
	priv->account_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mrb-account-entry" );
	g_return_if_fail( priv->account_entry && GTK_IS_ENTRY( priv->account_entry ));
	g_signal_connect( priv->account_entry, "changed", G_CALLBACK( on_account_changed ), self );
	/* setup the mnemonic widget on the label */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mrb-account-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->account_entry );

	/* remember the root account */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mrb-remember-btn" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	g_signal_connect( btn, "toggled", G_CALLBACK( on_remember_toggled ), self );

	/* connect to the 'changed' signal of the entry */
	priv->password_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mrb-password-entry" );
	g_return_if_fail( priv->password_entry && GTK_IS_ENTRY( priv->password_entry ));
	g_signal_connect( priv->password_entry, "changed", G_CALLBACK( on_password_changed ), self );
	/* setup the mnemonic widget on the label */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mrb-password-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->password_entry );

	priv->revealer = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mrb-revealer" );
	g_return_if_fail( priv->revealer && GTK_IS_REVEALER( priv->revealer ));

	/* message */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mrb-msg" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_style_add( label, "labeliinfo" );
	priv->msg = label;

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

/**
 * ofa_mysql_root_bin_get_size_group:
 * @bin: this #ofaMysqlRootBin instance.
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
ofa_mysql_root_bin_get_size_group( ofaMysqlRootBin *bin, guint column )
{
	static const gchar *thisfn = "ofa_mysql_root_bin_get_size_group";
	ofaMysqlRootBinPrivate *priv;

	g_debug( "%s: bin=%p, column=%u", thisfn, ( void * ) bin, column );

	g_return_val_if_fail( bin && OFA_IS_MYSQL_ROOT_BIN( bin ), NULL );

	priv = ofa_mysql_root_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}

	return( NULL );
}

static void
on_account_changed( GtkEditable *entry, ofaMysqlRootBin *self )
{
	ofaMysqlRootBinPrivate *priv;

	priv = ofa_mysql_root_bin_get_instance_private( self );

	g_free( priv->account );
	priv->account = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	changed_composite( self );
}

static void
on_remember_toggled( GtkToggleButton *button, ofaMysqlRootBin *self )
{
	ofaMysqlRootBinPrivate *priv;

	priv = ofa_mysql_root_bin_get_instance_private( self );

	priv->remember = gtk_toggle_button_get_active( button );

	changed_composite( self );
}

static void
on_password_changed( GtkEditable *entry, ofaMysqlRootBin *self )
{
	ofaMysqlRootBinPrivate *priv;

	priv = ofa_mysql_root_bin_get_instance_private( self );

	g_free( priv->password );
	priv->password = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	changed_composite( self );
}

/*
 * The signal 'ofa-changed' is intercepted by ofaMysqlDossierEditor::changed_composite()
 * which sets ofa_mysql_root_bin_set_valid() to FALSE.
 */
static void
changed_composite( ofaMysqlRootBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_mysql_root_bin_is_valid:
 * @bin: this #ofaMysqlRootBin instance.
 * @error_message: [allow-none]: set to the error message as a newly
 *  allocated string which should be g_free() by the caller.
 *
 * Returns: %TRUE if both account and password are set without actually
 * validating these credentials againt the MYSQL.
 */
gboolean
ofa_mysql_root_bin_is_valid( ofaMysqlRootBin *bin, gchar **error_message )
{
	ofaMysqlRootBinPrivate *priv;
	gboolean ok;
	gchar *str;

	g_return_val_if_fail( bin && OFA_IS_MYSQL_ROOT_BIN( bin ), FALSE );

	priv = ofa_mysql_root_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	ok = is_valid( bin, &str );

	if( error_message ){
		*error_message = ok ? NULL : g_strdup( str );
	}

	g_free( str );

	return( ok );
}

/*
 * we only check here for the presence of an account
 * as we don't know if it really has a password (though it should for sure)
 */
static gboolean
is_valid( ofaMysqlRootBin *self, gchar **str )
{
	ofaMysqlRootBinPrivate *priv;
	gboolean ok;

	priv = ofa_mysql_root_bin_get_instance_private( self );

	ok = TRUE;
	*str = NULL;

	if( ok ){
		if( !my_strlen( priv->account )){
			ok = FALSE;
			*str = g_strdup( _( "DBMS root account is not set" ));
		}
	}

	return( ok );
}

/**
 * ofa_mysql_root_bin_set_valid:
 * @bin: this #ofaMysqlRootBin instance.
 * @valid: whether this @bin is valid.
 *
 * Set an information message when the connection is OK.
 *
 * The message is cleared when the connection is not OK as error messages
 * are not handled by this widget.
 */
void
ofa_mysql_root_bin_set_valid( ofaMysqlRootBin *bin, gboolean valid )
{
	static const gchar *thisfn = "ofa_mysql_root_bin_set_valid";
	ofaMysqlRootBinPrivate *priv;

	g_debug( "%s: bin=%p, valid=%s", thisfn, ( void * ) bin, valid ? "True":"False" );

	g_return_if_fail( bin && OFA_IS_MYSQL_ROOT_BIN( bin ));

	priv = ofa_mysql_root_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	gtk_label_set_text( GTK_LABEL( priv->msg ), valid ? _( "DBMS root credentials are valid" ) : "" );
}

/**
 * ofa_mysql_root_bin_get_account:
 * @bin: this #ofaMysqlRootBin instance.
 *
 * Returns: the account.
 */
const gchar *
ofa_mysql_root_bin_get_account( ofaMysqlRootBin *bin )
{
	ofaMysqlRootBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_MYSQL_ROOT_BIN( bin ), NULL );

	priv = ofa_mysql_root_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->account );
}

/**
 * ofa_mysql_root_bin_get_password:
 * @bin: this #ofaMysqlRootBin instance.
 *
 * Returns: the password.
 */
const gchar *
ofa_mysql_root_bin_get_password( ofaMysqlRootBin *bin )
{
	ofaMysqlRootBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_MYSQL_ROOT_BIN( bin ), NULL );

	priv = ofa_mysql_root_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->password );
}

/**
 * ofa_mysql_root_bin_get_remembered_account:
 * @bin: this #ofaMysqlRootBin instance.
 *
 * Returns: the account if the user has asked to remember it, %NULL else.
 */
const gchar *
ofa_mysql_root_bin_get_remembered_account( ofaMysqlRootBin *bin )
{
	ofaMysqlRootBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_MYSQL_ROOT_BIN( bin ), NULL );

	priv = ofa_mysql_root_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * )( priv->remember ? priv->account : NULL ));
}

/**
 * ofa_mysql_root_bin_get_credentials:
 * @bin: this #ofaMysqlRootBin instance.
 * @account: [out]: placeholder for the root account.
 * @password: [out]: placeholder for the password.
 *
 * Returns the credentials.
 */
void
ofa_mysql_root_bin_get_credentials( ofaMysqlRootBin *bin, gchar **account, gchar **password )
{
	ofaMysqlRootBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_MYSQL_ROOT_BIN( bin ));

	priv = ofa_mysql_root_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	*account = g_strdup( priv->account );
	*password = g_strdup( priv->password );
}

/**
 * ofa_mysql_root_bin_set_credentials:
 * @bin: this #ofaMysqlRootBin instance.
 * @account: the root account of the MYSQL.
 * @password: the password of the root account.
 *
 * Set the provider credentials.
 */
void
ofa_mysql_root_bin_set_credentials( ofaMysqlRootBin *bin, const gchar *account, const gchar *password )
{
	ofaMysqlRootBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_MYSQL_ROOT_BIN( bin ));

	priv = ofa_mysql_root_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), account );
	gtk_entry_set_text( GTK_ENTRY( priv->password_entry ), password );
}

/**
 * ofa_mysql_root_bin_set_dossier_meta:
 * @bin: this #ofaMysqlRootBin instance.
 * @dossier_meta: [allow-none]: the #ofaIDBDossierMeta object which
 *  holds dossier meta datas.
 *
 * When set, this let the composite widget validate the account and the
 * password against the actual MYSQL which manages this dossier.
 * Else, we only check if account and password are set.
 *
 * The composite widget takes a reference on the provided @meta object.
 * This reference will be released on widget destroy.
 */
void
ofa_mysql_root_bin_set_dossier_meta( ofaMysqlRootBin *bin, ofaIDBDossierMeta *dossier_meta )
{
	ofaMysqlRootBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_MYSQL_ROOT_BIN( bin ));
	g_return_if_fail( !dossier_meta || OFA_IS_IDBDOSSIER_META( dossier_meta ));

	priv = ofa_mysql_root_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	priv->dossier_meta = dossier_meta;
}
