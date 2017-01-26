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
#include <stdlib.h>

#include "my/my-utils.h"

#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-editor.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-editor.h"
#include "api/ofa-idbprovider.h"

#include "mysql/ofa-mysql-connect.h"
#include "mysql/ofa-mysql-dossier-bin.h"
#include "mysql/ofa-mysql-dossier-editor.h"
#include "mysql/ofa-mysql-root-bin.h"

/* private instance data
 */
typedef struct {
	gboolean            dispose_has_run;

	/* initialization
	 */
	ofaIDBProvider     *provider;
	gchar              *settings_prefix;
	guint               rule;
	gboolean            with_su;

	/* UI
	 */
	GtkSizeGroup       *group0;
	ofaMysqlDossierBin *dossier_bin;
	ofaMysqlRootBin    *root_bin;

	/* runtime
	 */
	ofaMysqlConnect    *connect;
}
	ofaMysqlDossierEditorPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/mysql/ofa-mysql-dossier-editor.ui";

static void           setup_bin( ofaMysqlDossierEditor *self );
static void           on_dossier_bin_changed( ofaMysqlDossierBin *bin, ofaMysqlDossierEditor *self );
static void           on_root_bin_changed( ofaMysqlRootBin *bin, ofaMysqlDossierEditor *self );
static void           changed_composite( ofaMysqlDossierEditor *self );
static gboolean       check_root_connection( ofaMysqlDossierEditor *self, gchar **msgerr );
static void           idbdossier_editor_iface_init( ofaIDBDossierEditorInterface *iface );
static guint          idbdossier_editor_get_interface_version( void );
static GtkSizeGroup  *idbdossier_editor_get_size_group( const ofaIDBDossierEditor *instance, guint column );
static gboolean       idbdossier_editor_is_valid( const ofaIDBDossierEditor *instance, gchar **message );
static ofaIDBConnect *idbdossier_editor_get_valid_connect( const ofaIDBDossierEditor *instance, ofaIDBDossierMeta *dossier_meta );

G_DEFINE_TYPE_EXTENDED( ofaMysqlDossierEditor, ofa_mysql_dossier_editor, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaMysqlDossierEditor )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDBDOSSIER_EDITOR, idbdossier_editor_iface_init ))

static void
mysql_dossier_editor_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_dossier_editor_finalize";
	ofaMysqlDossierEditorPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MYSQL_DOSSIER_EDITOR( instance ));

	/* free data members here */
	priv = ofa_mysql_dossier_editor_get_instance_private( OFA_MYSQL_DOSSIER_EDITOR( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_dossier_editor_parent_class )->finalize( instance );
}

static void
mysql_dossier_editor_dispose( GObject *instance )
{
	ofaMysqlDossierEditorPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_DOSSIER_EDITOR( instance ));

	priv = ofa_mysql_dossier_editor_get_instance_private( OFA_MYSQL_DOSSIER_EDITOR( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
		g_clear_object( &priv->connect );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_dossier_editor_parent_class )->dispose( instance );
}

static void
ofa_mysql_dossier_editor_init( ofaMysqlDossierEditor *self )
{
	static const gchar *thisfn = "ofa_mysql_dossier_editor_instance_init";
	ofaMysqlDossierEditorPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MYSQL_DOSSIER_EDITOR( self ));

	priv = ofa_mysql_dossier_editor_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->with_su = TRUE;
	priv->connect = ofa_mysql_connect_new();
}

static void
ofa_mysql_dossier_editor_class_init( ofaMysqlDossierEditorClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_dossier_editor_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_dossier_editor_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_dossier_editor_finalize;
}

/**
 * ofa_mysql_dossier_editor_new:
 * @provider: the #ofaIDBProvider provider.
 * @settings_prefix: the prefix of a user preference key.
 * @rule: the usage of the widget.
 * @with_su: whether this editor should display the super-user widget.
 *
 * Returns: a new #ofaMysqlDossierEditor widget.
 *
 * The dossier editor for MySQL embeds both:
 * - the instance informations
 * - the root credentials.
 */
ofaMysqlDossierEditor *
ofa_mysql_dossier_editor_new( ofaIDBProvider *provider, const gchar *settings_prefix, guint rule, gboolean with_su )
{
	ofaMysqlDossierEditor *bin;
	ofaMysqlDossierEditorPrivate *priv;

	g_return_val_if_fail( provider && OFA_IS_IDBPROVIDER( provider ), NULL );

	bin = g_object_new( OFA_TYPE_MYSQL_DOSSIER_EDITOR, NULL );

	priv = ofa_mysql_dossier_editor_get_instance_private( bin );

	priv->provider = provider;

	g_free( priv->settings_prefix );
	priv->settings_prefix = g_strdup( settings_prefix );

	priv->rule = rule;
	priv->with_su = with_su;

	setup_bin( bin );

	return( bin );
}

static void
setup_bin( ofaMysqlDossierEditor *self )
{
	ofaMysqlDossierEditorPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *parent;

	priv = ofa_mysql_dossier_editor_get_instance_private( self );

	priv->group0 = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "mde-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mde-dossier-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->dossier_bin = ofa_mysql_dossier_bin_new( OFA_MYSQL_DBPROVIDER( priv->provider ), priv->settings_prefix, priv->rule );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->dossier_bin ));
	g_signal_connect( priv->dossier_bin, "ofa-changed", G_CALLBACK( on_dossier_bin_changed ), self );
	my_utils_size_group_add_size_group( priv->group0, ofa_mysql_dossier_bin_get_size_group( priv->dossier_bin, 0 ));

	if( priv->with_su ){
		parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mde-root-parent" );
		g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
		priv->root_bin = ofa_mysql_root_bin_new( OFA_MYSQL_DBPROVIDER( priv->provider ), priv->rule );
		gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->root_bin ));
		g_signal_connect( priv->root_bin, "ofa-changed", G_CALLBACK( on_root_bin_changed ), self );
		my_utils_size_group_add_size_group( priv->group0, ofa_mysql_root_bin_get_size_group( priv->root_bin, 0 ));
	}

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
on_dossier_bin_changed( ofaMysqlDossierBin *bin, ofaMysqlDossierEditor *self )
{
	changed_composite( self );
}

static void
on_root_bin_changed( ofaMysqlRootBin *bin, ofaMysqlDossierEditor *self )
{
	changed_composite( self );
}

static void
changed_composite( ofaMysqlDossierEditor *self )
{
	ofaMysqlDossierEditorPrivate *priv;

	priv = ofa_mysql_dossier_editor_get_instance_private( self );

	ofa_mysql_connect_close( priv->connect );

	g_signal_emit_by_name( self, "ofa-changed" );
}

/*
 * Try a connection with root credentials at server level
 *
 * It happens that the MariaDB instance accepts all connections which
 * have an unknown user and no password, showing only 'test' database.
 * So we only check here if password is set.
 *
 * This requires that a superuser widget was allowed at initialization.
 */
static gboolean
check_root_connection( ofaMysqlDossierEditor *self, gchar **msgerr )
{
	ofaMysqlDossierEditorPrivate *priv;
	gboolean ok;
	const gchar *password;

	priv = ofa_mysql_dossier_editor_get_instance_private( self );

	password = ofa_mysql_root_bin_get_password( priv->root_bin );

	if( my_strlen( password )){
		ok = ofa_mysql_connect_open_with_details( priv->connect,
					ofa_mysql_dossier_bin_get_host( priv->dossier_bin ),
					ofa_mysql_dossier_bin_get_port( priv->dossier_bin ),
					ofa_mysql_dossier_bin_get_socket( priv->dossier_bin ),
					NULL,													// database
					ofa_mysql_root_bin_get_account( priv->root_bin ),
					password );
	} else {
		ok = FALSE;
	}

	ofa_mysql_root_bin_set_valid( priv->root_bin, ok );

	if( !ok ){
		*msgerr = g_strdup( _( "DBMS root credentials are not valid" ));
	}

	return( ok );
}

/**
 * ofa_mysql_dossier_editor_get_host:
 * @editor: this #ofaMysqlDossierEditor instance.
 *
 * Returns: the DBMS host.
 *
 * The returned string is owned by the @editor, and should not be
 * released by the caller.
 */
const gchar *
ofa_mysql_dossier_editor_get_host( ofaMysqlDossierEditor *editor )
{
	ofaMysqlDossierEditorPrivate *priv;

	g_return_val_if_fail( editor && OFA_IS_MYSQL_DOSSIER_EDITOR( editor ), NULL );

	priv = ofa_mysql_dossier_editor_get_instance_private( editor );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( ofa_mysql_dossier_bin_get_host( priv->dossier_bin ));
}

/**
 * ofa_mysql_dossier_editor_get_port:
 * @editor: this #ofaMysqlDossierEditor instance.
 *
 * Returns: the DBMS listening port.
 */
guint
ofa_mysql_dossier_editor_get_port( ofaMysqlDossierEditor *editor )
{
	ofaMysqlDossierEditorPrivate *priv;

	g_return_val_if_fail( editor && OFA_IS_MYSQL_DOSSIER_EDITOR( editor ), 0 );

	priv = ofa_mysql_dossier_editor_get_instance_private( editor );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	return( ofa_mysql_dossier_bin_get_port( priv->dossier_bin ));
}

/**
 * ofa_mysql_dossier_editor_get_socket:
 * @editor: this #ofaMysqlDossierEditor instance.
 *
 * Returns: the DBMS listening socket.
 *
 * The returned string is owned by the @editor, and should not be
 * released by the caller.
 */
const gchar *
ofa_mysql_dossier_editor_get_socket( ofaMysqlDossierEditor *editor )
{
	ofaMysqlDossierEditorPrivate *priv;

	g_return_val_if_fail( editor && OFA_IS_MYSQL_DOSSIER_EDITOR( editor ), NULL );

	priv = ofa_mysql_dossier_editor_get_instance_private( editor );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( ofa_mysql_dossier_bin_get_socket( priv->dossier_bin ));
}

/**
 * ofa_mysql_dossier_editor_get_remembered_account:
 * @editor: this #ofaMysqlDossierEditor instance.
 *
 * Returns: the DBMS remembered root account.
 *
 * The returned string is owned by the @editor, and should not be
 * released by the caller.
 */
const gchar *
ofa_mysql_dossier_editor_get_remembered_account( ofaMysqlDossierEditor *editor )
{
	ofaMysqlDossierEditorPrivate *priv;

	g_return_val_if_fail( editor && OFA_IS_MYSQL_DOSSIER_EDITOR( editor ), NULL );

	priv = ofa_mysql_dossier_editor_get_instance_private( editor );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->root_bin ? ofa_mysql_root_bin_get_remembered_account( priv->root_bin ) : NULL );
}

/*
 * ofaIDBDossierEditor interface management
 */
static void
idbdossier_editor_iface_init( ofaIDBDossierEditorInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_dossier_editor_idbdossier_editor_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idbdossier_editor_get_interface_version;
	iface->get_size_group = idbdossier_editor_get_size_group;
	iface->is_valid = idbdossier_editor_is_valid;
	iface->get_valid_connect = idbdossier_editor_get_valid_connect;
}

static guint
idbdossier_editor_get_interface_version( void )
{
	return( 1 );
}

static GtkSizeGroup *
idbdossier_editor_get_size_group( const ofaIDBDossierEditor *instance, guint column )
{
	static const gchar *thisfn = "ofa_mysql_dossier_editor_get_size_group";
	ofaMysqlDossierEditorPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_DOSSIER_EDITOR( instance ), NULL );

	priv = ofa_mysql_dossier_editor_get_instance_private( OFA_MYSQL_DOSSIER_EDITOR( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}

	g_warning( "%s: column=%u", thisfn, column );
	return( NULL );
}

/*
 * All the informations are optional.
 * When all pieces are valid, then we can check the connection itself.
 */
static gboolean
idbdossier_editor_is_valid( const ofaIDBDossierEditor *instance, gchar **message )
{
	ofaMysqlDossierEditorPrivate *priv;
	gboolean ok;

	priv = ofa_mysql_dossier_editor_get_instance_private( OFA_MYSQL_DOSSIER_EDITOR( instance ));

	ok = ofa_mysql_dossier_bin_is_valid( priv->dossier_bin, message );

	if( ok && priv->root_bin ){
		ok &= ofa_mysql_root_bin_is_valid( priv->root_bin, message ) &&
				check_root_connection( OFA_MYSQL_DOSSIER_EDITOR( instance ), message );
	}

	return( ok );
}

/*
 * There is no valid connection if the superuser widget was not allowed
 * at initialization time.
 */
static ofaIDBConnect *
idbdossier_editor_get_valid_connect( const ofaIDBDossierEditor *instance, ofaIDBDossierMeta *dossier_meta )
{
	ofaMysqlDossierEditorPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_DOSSIER_EDITOR( instance ), NULL );
	g_return_val_if_fail( idbdossier_editor_is_valid( instance, NULL ), NULL );

	priv = ofa_mysql_dossier_editor_get_instance_private( OFA_MYSQL_DOSSIER_EDITOR( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( priv->root_bin ){
		ofa_idbconnect_set_dossier_meta(
				OFA_IDBCONNECT( priv->connect ), dossier_meta );

		ofa_idbconnect_set_account(
				OFA_IDBCONNECT( priv->connect ),
				ofa_mysql_root_bin_get_account( priv->root_bin ), ofa_mysql_root_bin_get_password( priv->root_bin ));
	}

	return( priv->root_bin ? OFA_IDBCONNECT( priv->connect ) : NULL );
}
