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

#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbprovider.h"

#include "core/ofa-dbms-root-bin.h"

/* private instance data
 */
typedef struct {
	gboolean           dispose_has_run;

	/* initialization
	 */
	ofaHub            *hub;

	/* UI
	 */
	GtkWidget         *account_entry;
	GtkWidget         *password_entry;
	GtkWidget         *msg_label;
	GtkSizeGroup      *group0;

	/* runtime data
	 */
	ofaIDBDossierMeta *meta;
	gchar             *account;
	gchar             *password;
}
	ofaDBMSRootBinPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-dbms-root-bin.ui";

static void     setup_bin( ofaDBMSRootBin *self );
static void     on_account_changed( GtkEditable *entry, ofaDBMSRootBin *self );
static void     on_password_changed( GtkEditable *entry, ofaDBMSRootBin *self );
static void     changed_composite( ofaDBMSRootBin *self );
static gboolean is_valid_composite( ofaDBMSRootBin *self );

G_DEFINE_TYPE_EXTENDED( ofaDBMSRootBin, ofa_dbms_root_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaDBMSRootBin ))

static void
dbms_root_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dbms_root_bin_finalize";
	ofaDBMSRootBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DBMS_ROOT_BIN( instance ));

	/* free data members here */
	priv = ofa_dbms_root_bin_get_instance_private( OFA_DBMS_ROOT_BIN( instance ));

	g_free( priv->account );
	g_free( priv->password );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dbms_root_bin_parent_class )->finalize( instance );
}

static void
dbms_root_bin_dispose( GObject *instance )
{
	ofaDBMSRootBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DBMS_ROOT_BIN( instance ));

	priv = ofa_dbms_root_bin_get_instance_private( OFA_DBMS_ROOT_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
		g_clear_object( &priv->meta );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dbms_root_bin_parent_class )->dispose( instance );
}

static void
ofa_dbms_root_bin_init( ofaDBMSRootBin *self )
{
	static const gchar *thisfn = "ofa_dbms_root_bin_instance_init";
	ofaDBMSRootBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DBMS_ROOT_BIN( self ));

	priv = ofa_dbms_root_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->account = NULL;
	priv->password = NULL;
}

static void
ofa_dbms_root_bin_class_init( ofaDBMSRootBinClass *klass )
{
	static const gchar *thisfn = "ofa_dbms_root_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dbms_root_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = dbms_root_bin_finalize;

	/**
	 * ofaDBMSRootBin::changed:
	 *
	 * This signal is sent on the #ofaDBMSRootBin when the account or
	 * the password are changed.
	 *
	 * Arguments are new account and password.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDBMSRootBin *bin,
	 * 						const gchar    *account,
	 * 						const gchar    *password,
	 * 						gpointer        user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_DBMS_ROOT_BIN,
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
 * ofa_dbms_root_bin_new:
 * @hub: the #ofaHub object of the application.
 *
 * Returns: a new #ofaDBMSRootBin instance.
 */
ofaDBMSRootBin *
ofa_dbms_root_bin_new( ofaHub *hub )
{
	ofaDBMSRootBin *bin;
	ofaDBMSRootBinPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	bin = g_object_new( OFA_TYPE_DBMS_ROOT_BIN, NULL );

	priv = ofa_dbms_root_bin_get_instance_private( bin );

	priv->hub = hub;

	setup_bin( bin );

	return( bin );
}

static void
setup_bin( ofaDBMSRootBin *self )
{
	ofaDBMSRootBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *label;

	priv = ofa_dbms_root_bin_get_instance_private( self );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "drb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "drb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	/* connect to the 'changed' signal of the entry */
	priv->account_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "drb-account-entry" );
	g_return_if_fail( priv->account_entry && GTK_IS_ENTRY( priv->account_entry ));
	g_signal_connect(
			G_OBJECT( priv->account_entry ), "changed", G_CALLBACK( on_account_changed ), self );
	/* setup the mnemonic widget on the label */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "drb-account-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->account_entry );

	/* connect to the 'changed' signal of the entry */
	priv->password_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "drb-password-entry" );
	g_return_if_fail( priv->password_entry && GTK_IS_ENTRY( priv->password_entry ));
	g_signal_connect(
			G_OBJECT( priv->password_entry ), "changed", G_CALLBACK( on_password_changed ), self );
	/* setup the mnemonic widget on the label */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "drb-password-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->password_entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "drb-message" );
	my_style_add( label, "labelinfo" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->msg_label = label;

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

/**
 * ofa_dbms_root_bin_get_size_group:
 * @bin: this #ofaDBMSRootBin instance.
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
ofa_dbms_root_bin_get_size_group( ofaDBMSRootBin *bin, guint column )
{
	static const gchar *thisfn = "ofa_dbms_root_bin_get_size_group";
	ofaDBMSRootBinPrivate *priv;

	g_debug( "%s: bin=%p, column=%u", thisfn, ( void * ) bin, column );

	g_return_val_if_fail( bin && OFA_IS_DBMS_ROOT_BIN( bin ), NULL );

	priv = ofa_dbms_root_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}

	return( NULL );
}

/**
 * ofa_dbms_root_bin_set_meta:
 * @bin: this #ofaDBMSRootBin instance.
 * @meta: the #ofaIDBDossierMeta object which holds meta dossier datas.
 *
 * When set, this let the composite widget validate the account and the
 * password against the actual DBMS which manages this dossier.
 * Else, we only check if account and password are set.
 *
 * The composite widget takes a reference on the provided @meta object.
 * This reference will be released on widget destroy.
 */
void
ofa_dbms_root_bin_set_meta( ofaDBMSRootBin *bin, ofaIDBDossierMeta *meta )
{
	ofaDBMSRootBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_DBMS_ROOT_BIN( bin ));
	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));

	priv = ofa_dbms_root_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	g_clear_object( &priv->meta );
	priv->meta = g_object_ref( meta );
}

static void
on_account_changed( GtkEditable *entry, ofaDBMSRootBin *self )
{
	ofaDBMSRootBinPrivate *priv;

	priv = ofa_dbms_root_bin_get_instance_private( self );

	g_free( priv->account );
	priv->account = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	changed_composite( self );
}

static void
on_password_changed( GtkEditable *entry, ofaDBMSRootBin *self )
{
	ofaDBMSRootBinPrivate *priv;

	priv = ofa_dbms_root_bin_get_instance_private( self );

	g_free( priv->password );
	priv->password = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	changed_composite( self );
}

/*
 * test the DBMS root connection by trying to connect with empty dbname
 */
static void
changed_composite( ofaDBMSRootBin *self )
{
	ofaDBMSRootBinPrivate *priv;

	priv = ofa_dbms_root_bin_get_instance_private( self );

	g_signal_emit_by_name( self, "ofa-changed", priv->account, priv->password );
}

/**
 * ofa_dbms_root_bin_is_valid:
 * @bin: this #ofaDBMSRootBin instance.
 * @error_message: [allow-none]: set to the error message as a newly
 *  allocated string which should be g_free() by the caller.
 *
 * Returns: %TRUE if both account and password are set without actually
 * validating these credentials againt the DBMS.
 * If dossier is set and is registered in the settings, then check the
 * account and password credentials are validated gainst a successful
 * connection to the DBMS.
 *
 * If dossier name has been set, the returns code indicates a valid
 * connection against the DBMS. In this case, the DBMS status message
 * is automatically set.
 *
 * If dossier name has not been set, the returns code indicates that
 * both account and password credentials has been filled up by the user.
 * The DBMS status message has to be set by the caller.
 */
gboolean
ofa_dbms_root_bin_is_valid( ofaDBMSRootBin *bin, gchar **error_message )
{
	ofaDBMSRootBinPrivate *priv;
	gboolean ok;

	g_return_val_if_fail( bin && OFA_IS_DBMS_ROOT_BIN( bin ), FALSE );

	priv = ofa_dbms_root_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), "" );
	ok = is_valid_composite( bin );

	if( error_message ){
		*error_message = ok ?
				NULL :
				g_strdup( _( "DBMS root credentials are not valid" ));
	}

	return( ok );
}

static gboolean
is_valid_composite( ofaDBMSRootBin *self )
{
	ofaDBMSRootBinPrivate *priv;
	ofaIDBProvider *provider;
	ofaIDBConnect *connect;
	gboolean ok;

	priv = ofa_dbms_root_bin_get_instance_private( self );
	ok = FALSE;

	if( my_strlen( priv->account ) && my_strlen( priv->password )){
		ok = TRUE;

		/* this only works if the dossier is already registered */
		if( priv->meta ){
			ok = FALSE;
			provider = ofa_idbdossier_meta_get_provider( priv->meta );
			if( provider ){
				connect = ofa_idbprovider_new_connect( provider, priv->account, priv->password, priv->meta, NULL );
				ok = ( connect != NULL );
				g_clear_object( &connect );
			}
			ofa_dbms_root_bin_set_valid( self, ok );
		}
	}

	return( ok );
}

/**
 * ofa_dbms_root_bin_set_valid:
 * @bin: this #ofaDBMSRootBin instance.
 * @valid: %TRUE if the credentials has been validated, %FALSE else.
 *
 * This let us turn the 'connection ok' message on, which is useful
 * when checking for a connection which is not yet referenced in the
 * settings.
 */
void
ofa_dbms_root_bin_set_valid( ofaDBMSRootBin *bin, gboolean valid )
{
	ofaDBMSRootBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_DBMS_ROOT_BIN( bin ));

	priv = ofa_dbms_root_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	gtk_label_set_text(
			GTK_LABEL( priv->msg_label ), valid ? _( "DB server connection is OK" ) : "" );
}

/**
 * ofa_dbms_root_bin_get_credentials:
 *
 * Set the credentials.
 */
void
ofa_dbms_root_bin_get_credentials( ofaDBMSRootBin *bin, gchar **account, gchar **password )
{
	ofaDBMSRootBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_DBMS_ROOT_BIN( bin ));

	priv = ofa_dbms_root_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	*account = g_strdup( priv->account );
	*password = g_strdup( priv->password );
}

/**
 * ofa_dbms_root_bin_set_credentials:
 * @bin: this #ofaDBMSRootBin instance.
 * @account: the root account of the DBMS.
 * @password: the password of the root account.
 */
void
ofa_dbms_root_bin_set_credentials( ofaDBMSRootBin *bin, const gchar *account, const gchar *password )
{
	ofaDBMSRootBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_DBMS_ROOT_BIN( bin ));

	priv = ofa_dbms_root_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), account );
	gtk_entry_set_text( GTK_ENTRY( priv->password_entry ), password );
}
