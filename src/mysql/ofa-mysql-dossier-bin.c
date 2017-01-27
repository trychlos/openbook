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

#include "my/my-date.h"
#include "my/my-ibin.h"
#include "my/my-utils.h"

#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbprovider.h"

#include "ofa-mysql-connect.h"
#include "ofa-mysql-dossier-bin.h"

/* private instance data
 */
typedef struct {
	gboolean            dispose_has_run;

	/* setup
	 */
	ofaMysqlDBProvider *provider;
	gchar              *settings_prefix;
	guint               rule;

	/* runtime data
	 */
	gchar              *host;
	guint               port;
	gchar              *socket;
	ofaIDBDossierMeta  *dossier_meta;

	/* UI
	 */
	GtkSizeGroup       *group0;
	GtkWidget          *revealer;
}
	ofaMysqlDossierBinPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/mysql/ofa-mysql-dossier-bin.ui";

static void          setup_bin( ofaMysqlDossierBin *self );
static void          setup_dossier_meta( ofaMysqlDossierBin *self );
static void          on_host_changed( GtkEntry *entry, ofaMysqlDossierBin *self );
static void          on_port_changed( GtkEntry *entry, ofaMysqlDossierBin *self );
static void          on_socket_changed( GtkEntry *entry, ofaMysqlDossierBin *self );
static void          changed_composite( ofaMysqlDossierBin *self );
static void          ibin_iface_init( myIBinInterface *iface );
static guint         ibin_get_interface_version( void );
static GtkSizeGroup *ibin_get_size_group( const myIBin *instance, guint column );
static gboolean      ibin_is_valid( const myIBin *instance, gchar **msgerr );

G_DEFINE_TYPE_EXTENDED( ofaMysqlDossierBin, ofa_mysql_dossier_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaMysqlDossierBin )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IBIN, ibin_iface_init ))

static void
mysql_dossier_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_dossier_bin_finalize";
	ofaMysqlDossierBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MYSQL_DOSSIER_BIN( instance ));

	/* free data members here */
	priv = ofa_mysql_dossier_bin_get_instance_private( OFA_MYSQL_DOSSIER_BIN( instance ));

	g_free( priv->settings_prefix );
	g_free( priv->host );
	g_free( priv->socket );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_dossier_bin_parent_class )->finalize( instance );
}

static void
mysql_dossier_bin_dispose( GObject *instance )
{
	ofaMysqlDossierBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_DOSSIER_BIN( instance ));

	priv = ofa_mysql_dossier_bin_get_instance_private( OFA_MYSQL_DOSSIER_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_dossier_bin_parent_class )->dispose( instance );
}

static void
ofa_mysql_dossier_bin_init( ofaMysqlDossierBin *self )
{
	static const gchar *thisfn = "ofa_mysql_dossier_bin_instance_init";
	ofaMysqlDossierBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MYSQL_DOSSIER_BIN( self ));

	priv = ofa_mysql_dossier_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_mysql_dossier_bin_class_init( ofaMysqlDossierBinClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_dossier_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_dossier_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_dossier_bin_finalize;

	/**
	 * ofaMysqlDossierBin::ofa-changed:
	 *
	 * This signal is sent on the #ofaMysqlDossierBin when any of the
	 * underlying information is changed. This includes the host, socket
	 * and port values.
	 *
	 * There is no argument.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaMysqlDossierBin *bin,
	 * 						gpointer          user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_MYSQL_DOSSIER_BIN,
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
 * ofa_mysql_dossier_bin_new:
 * @provider: the #ofaIDBProvider instance.
 * @settings_prefix: the prefix of a user preference key.
 * @rule: the usage of this widget.
 *
 * Returns: a new #ofaMysqlDossierBin widget.
 */
ofaMysqlDossierBin *
ofa_mysql_dossier_bin_new( ofaMysqlDBProvider *provider, const gchar *settings_prefix, guint rule )
{
	static const gchar *thisfn = "ofa_mysql_dossier_bin_new";
	ofaMysqlDossierBin *bin;
	ofaMysqlDossierBinPrivate *priv;

	g_debug( "%s: provider=%p (%s), settings_prefix=%s, rule=%u",
			thisfn, ( void * ) provider, G_OBJECT_TYPE_NAME( provider ), settings_prefix, rule );

	g_return_val_if_fail( provider && OFA_IS_MYSQL_DBPROVIDER( provider ), NULL );

	bin = g_object_new( OFA_TYPE_MYSQL_DOSSIER_BIN, NULL );

	priv = ofa_mysql_dossier_bin_get_instance_private( bin );

	priv->provider = provider;

	g_free( priv->settings_prefix );
	priv->settings_prefix = g_strdup( settings_prefix );

	priv->rule = rule;

	setup_bin( bin );

	return( bin );
}

static void
setup_bin( ofaMysqlDossierBin *self )
{
	ofaMysqlDossierBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *entry, *label;

	priv = ofa_mysql_dossier_bin_get_instance_private( self );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "mdb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "mdb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mdb-host-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_host_changed ), self );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mdb-host-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mdb-port-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_port_changed ), self );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mdb-port-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mdb-socket-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_socket_changed ), self );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mdb-socket-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	priv->revealer = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mdb-revealer" );
	g_return_if_fail( priv->revealer && GTK_IS_REVEALER( priv->revealer ));

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
setup_dossier_meta( ofaMysqlDossierBin *self )
{

}

static void
on_host_changed( GtkEntry *entry, ofaMysqlDossierBin *self )
{
	ofaMysqlDossierBinPrivate *priv;

	priv = ofa_mysql_dossier_bin_get_instance_private( self );

	g_free( priv->host );
	priv->host = g_strdup( gtk_entry_get_text( entry ));

	changed_composite( self );
}

static void
on_port_changed( GtkEntry *entry, ofaMysqlDossierBin *self )
{
	ofaMysqlDossierBinPrivate *priv;
	const gchar *port;

	priv = ofa_mysql_dossier_bin_get_instance_private( self );

	port = gtk_entry_get_text( entry );
	if( my_strlen( port )){
		priv->port = ( guint ) atoi( port );
	} else {
		priv->port = 0;
	}

	changed_composite( self );
}

static void
on_socket_changed( GtkEntry *entry, ofaMysqlDossierBin *self )
{
	ofaMysqlDossierBinPrivate *priv;

	priv = ofa_mysql_dossier_bin_get_instance_private( self );

	g_free( priv->socket );
	priv->socket = g_strdup( gtk_entry_get_text( entry ));

	changed_composite( self );
}

static void
changed_composite( ofaMysqlDossierBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_mysql_dossier_bin_get_host:
 * @bin: this #ofaMysqlDossierBin instance.
 *
 * Returns: the DBMS host.
 *
 * The returned string is owned by the @bin, and should not be
 * released by the caller.
 */
const gchar *
ofa_mysql_dossier_bin_get_host( ofaMysqlDossierBin *bin )
{
	ofaMysqlDossierBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_MYSQL_DOSSIER_BIN( bin ), NULL );

	priv = ofa_mysql_dossier_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->host );
}

/**
 * ofa_mysql_dossier_bin_get_port:
 * @bin: this #ofaMysqlDossierBin instance.
 *
 * Returns: the DBMS listening port.
 */
guint
ofa_mysql_dossier_bin_get_port( ofaMysqlDossierBin *bin )
{
	ofaMysqlDossierBinPrivate *priv;

g_return_val_if_fail( bin && OFA_IS_MYSQL_DOSSIER_BIN( bin ), 0 );

	priv = ofa_mysql_dossier_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	return( priv->port );
}

/**
 * ofa_mysql_dossier_bin_get_socket:
 * @bin: this #ofaMysqlDossierBin instance.
 *
 * Returns: the DBMS listening socket.
 *
 * The returned string is owned by the @bin, and should not be
 * released by the caller.
 */
const gchar *
ofa_mysql_dossier_bin_get_socket( ofaMysqlDossierBin *bin )
{
	ofaMysqlDossierBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_MYSQL_DOSSIER_BIN( bin ), NULL );

	priv = ofa_mysql_dossier_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->socket );
}

/**
 * ofa_mysql_dossier_bin_set_dossier_meta:
 * @bin: this #ofaMysqlDossierBin instance.
 * @dossier_meta: the #ofaIDBDossierMeta object.
 *
 * Set the dossier meta datas.
 */
void
ofa_mysql_dossier_bin_set_dossier_meta( ofaMysqlDossierBin *bin, ofaIDBDossierMeta *dossier_meta )
{
	ofaMysqlDossierBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_MYSQL_DOSSIER_BIN( bin ));

	priv = ofa_mysql_dossier_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	priv->dossier_meta = dossier_meta;

	setup_dossier_meta( bin );
}

/*
 * myIBin interface management
 */
static void
ibin_iface_init( myIBinInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_dossier_bin_ibin_iface_init";

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
	static const gchar *thisfn = "ofa_mysql_dossier_bin_ibin_get_size_group";
	ofaMysqlDossierBinPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_DOSSIER_BIN( instance ), NULL );

	priv = ofa_mysql_dossier_bin_get_instance_private( OFA_MYSQL_DOSSIER_BIN( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}

	g_warning( "%s: invalid column=%u", thisfn, column );

	return( NULL );
}

/*
 * All informations are optional: the widget is always valid.
 */
gboolean
ibin_is_valid( const myIBin *instance, gchar **msgerr )
{
	ofaMysqlDossierBinPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_DOSSIER_BIN( instance ), FALSE );

	priv = ofa_mysql_dossier_bin_get_instance_private( OFA_MYSQL_DOSSIER_BIN( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	if( msgerr ){
		*msgerr = NULL;
	}

	return( TRUE );
}
