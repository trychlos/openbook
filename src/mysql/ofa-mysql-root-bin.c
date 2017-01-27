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

#include "my/my-ibin.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-idbsuperuser.h"

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
	GtkWidget          *remember_btn;
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

static const gchar *st_settings_prefix  = "mysql-su";
static const gchar *st_resource_ui      = "/org/trychlos/openbook/mysql/ofa-mysql-root-bin.ui";

static void          setup_bin( ofaMysqlRootBin *bin );
static void          on_account_changed( GtkEditable *entry, ofaMysqlRootBin *self );
static void          on_remember_toggled( GtkToggleButton *button, ofaMysqlRootBin *self );
static void          on_password_changed( GtkEditable *entry, ofaMysqlRootBin *self );
static void          changed_composite( ofaMysqlRootBin *self );
static gboolean      is_valid( ofaMysqlRootBin *self, gchar **str );
static void          read_settings( ofaMysqlRootBin *self );
static void          write_settings( ofaMysqlRootBin *self );
static void          ibin_iface_init( myIBinInterface *iface );
static guint         ibin_get_interface_version( void );
static GtkSizeGroup *ibin_get_size_group( const myIBin *instance, guint column );
static gboolean      ibin_is_valid( const myIBin *instance, gchar **msgerr );
static void          idbsuperuser_iface_init( ofaIDBSuperuserInterface *iface );
static GtkSizeGroup *idbsuperuser_get_size_group( const ofaIDBSuperuser *instance, guint column );
static void          idbsuperuser_set_dossier_meta( ofaIDBSuperuser *instance, ofaIDBDossierMeta *dossier_meta );
static gboolean      idbsuperuser_is_valid( const ofaIDBSuperuser *instance, gchar **message );
static void          idbsuperuser_set_valid( ofaIDBSuperuser *instance, gboolean valid );
static void          idbsuperuser_set_credentials_from_connect( ofaIDBSuperuser *instance, ofaIDBConnect *connect );

G_DEFINE_TYPE_EXTENDED( ofaMysqlRootBin, ofa_mysql_root_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaMysqlRootBin )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IBIN, ibin_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDBSUPERUSER, idbsuperuser_iface_init ))

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

		write_settings( OFA_MYSQL_ROOT_BIN( instance ));

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
}

/**
 * ofa_mysql_root_bin_new_ex:
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
	priv->remember_btn = btn;

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
 * ofa_mysql_root_bin_set_dossier_meta:
 * @bin: this #ofaMysqlRootBin instance.
 * @dossier_meta: [allow-none]: the #ofaIDBDossierMeta object which
 *  holds dossier meta datas.
 *
 * When set, this let the composite widget validate the account and the
 * password against the actual MYSQL which manages this dossier.
 * Else, we only check if account and password are set.
 *
 * Settings are read as soon as (and every time that) this @dossier_meta
 * is set.
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

	read_settings( bin );
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
 * If something has changed, then default to be invalid
 */
static void
changed_composite( ofaMysqlRootBin *self )
{
	ofa_mysql_root_bin_set_valid( self, FALSE );

	g_signal_emit_by_name( self, "ofa-changed" );
}

/*
 * We only check here for the presence of an account
 * as we don't know if it really has a password (though it should for sure)
 * But we force the exigence of a password.
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

	if( ok ){
		if( !my_strlen( priv->password )){
			ok = FALSE;
			*str = g_strdup( _( "DBMS root password is not set" ));
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

#if 0
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
#endif

/*
 * settings are: remember_root_account(b); root_account(s);
 *
 * They are written in the dossier settings for the specifically set
 * dossier. So no settings are read while the #ofa_mysql_root_bin_set_dossier_meta()
 * function has not been called.
 */
static void
read_settings( ofaMysqlRootBin *self )
{
	ofaMysqlRootBinPrivate *priv;
	myISettings *settings;
	const gchar *group, *cstr;
	GList *strlist, *it;
	gboolean remember;

	priv = ofa_mysql_root_bin_get_instance_private( self );

	if( priv->dossier_meta ){
		settings = ofa_idbdossier_meta_get_settings_iface( priv->dossier_meta );
		group = ofa_idbdossier_meta_get_settings_group( priv->dossier_meta );

		if( settings && group ){
			strlist = my_isettings_get_string_list( settings, group, st_settings_prefix );
			remember = FALSE;

			it = strlist;
			cstr = it ? ( const gchar * ) it->data : NULL;
			if( my_strlen( cstr )){
				remember = my_utils_boolean_from_str( cstr );
				if( priv->remember_btn ){
					gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->remember_btn ), remember );
				}
			}

			it = it ? it->next : NULL;
			cstr = it ? ( const gchar * ) it->data : NULL;
			if( my_strlen( cstr ) && remember ){
				gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), cstr );
			}

			my_isettings_free_string_list( settings, strlist );
		}
	}
}

static void
write_settings( ofaMysqlRootBin *self )
{
	ofaMysqlRootBinPrivate *priv;
	myISettings *settings;
	const gchar *group;
	gchar *str;

	priv = ofa_mysql_root_bin_get_instance_private( self );

	if( priv->dossier_meta ){
		settings = ofa_idbdossier_meta_get_settings_iface( priv->dossier_meta );
		group = ofa_idbdossier_meta_get_settings_group( priv->dossier_meta );

		if( settings && group ){
			str = g_strdup_printf( "%s;%s;",
						priv->remember ? "True":"False",
						priv->remember ? ( priv->account ? priv->account : "" ) : "" );

			my_isettings_set_string( settings, group, st_settings_prefix, str );

			g_free( str );
		}
	}
}

/*
 * myIBin interface management
 */
static void
ibin_iface_init( myIBinInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_root_bin_ibin_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = ibin_get_interface_version;
	iface->get_size_group = ibin_get_size_group;
	iface->is_valid = ibin_is_valid;
}

static guint
ibin_get_interface_version( void )
{
	return( 1 );
}

static GtkSizeGroup *
ibin_get_size_group( const myIBin *instance, guint column )
{
	static const gchar *thisfn = "ofa_mysql_root_bin_ibin_get_size_group";
	ofaMysqlRootBinPrivate *priv;

	g_debug( "%s: instance=%p, column=%u", thisfn, ( void * ) instance, column );

	g_return_val_if_fail( instance && OFA_IS_MYSQL_ROOT_BIN( instance ), NULL );

	priv = ofa_mysql_root_bin_get_instance_private( OFA_MYSQL_ROOT_BIN( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}

	g_warning( "%s: invalid column=%u", thisfn, column );

	return( NULL );
}

/*
 * ibin_is_valid:
 * @bin: this #ofaMysqlRootBin instance.
 * @msgerr: [allow-none]: set to the error message as a newly
 *  allocated string which should be g_free() by the caller.
 *
 * Returns: %TRUE if both account and password are set.
 *
 * If a #ofaIDBDossierMeta is set, then check a connection against the
 * DBMS server (at DBMS server level).
 */
gboolean
ibin_is_valid( const myIBin *instance, gchar **msgerr )
{
	ofaMysqlRootBinPrivate *priv;
	gboolean ok;
	gchar *str;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_ROOT_BIN( instance ), FALSE );

	priv = ofa_mysql_root_bin_get_instance_private( OFA_MYSQL_ROOT_BIN( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	ok = is_valid( OFA_MYSQL_ROOT_BIN( instance ), &str );

	if( msgerr ){
		*msgerr = ok ? NULL : g_strdup( str );
	}

	g_free( str );

	return( ok );
}

/*
 * #ofaIDBSuperuser interface setup
 */
static void
idbsuperuser_iface_init( ofaIDBSuperuserInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_root_bin_idbsuperuser_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_size_group = idbsuperuser_get_size_group;
	iface->set_dossier_meta = idbsuperuser_set_dossier_meta;
	iface->is_valid = idbsuperuser_is_valid;
	iface->set_valid = idbsuperuser_set_valid;
	iface->set_credentials_from_connect = idbsuperuser_set_credentials_from_connect;
}

static GtkSizeGroup *
idbsuperuser_get_size_group( const ofaIDBSuperuser *instance, guint column )
{
	return( my_ibin_get_size_group( MY_IBIN( instance ), column ));
}

static void
idbsuperuser_set_dossier_meta( ofaIDBSuperuser *instance, ofaIDBDossierMeta *dossier_meta )
{
	ofa_mysql_root_bin_set_dossier_meta( OFA_MYSQL_ROOT_BIN( instance ), dossier_meta );
}

static gboolean
idbsuperuser_is_valid( const ofaIDBSuperuser *instance, gchar **msgerr )
{
	return( my_ibin_is_valid( MY_IBIN( instance ), msgerr ));
}

static void
idbsuperuser_set_valid( ofaIDBSuperuser *instance, gboolean valid )
{
	ofa_mysql_root_bin_set_valid( OFA_MYSQL_ROOT_BIN( instance ), valid );
}

static void
idbsuperuser_set_credentials_from_connect( ofaIDBSuperuser *instance, ofaIDBConnect *connect )
{
	ofaMysqlRootBinPrivate *priv;
	const gchar *cstr;

	priv = ofa_mysql_root_bin_get_instance_private( OFA_MYSQL_ROOT_BIN( instance ));

	cstr = ofa_idbconnect_get_account( connect );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), cstr );
	}
	cstr = ofa_idbconnect_get_password( connect );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->password_entry ), cstr );
	}
}
