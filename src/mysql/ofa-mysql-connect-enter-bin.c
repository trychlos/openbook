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
#include <stdlib.h>

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-idbms.h"
#include "api/ofa-settings.h"

#include "ofa-mysql.h"
#include "ofa-mysql-idbms.h"
#include "ofa-mysql-connect-enter-bin.h"

/* private instance data
 */
struct _ofaMySQLConnectEnterBinPrivate {
	gboolean      dispose_has_run;

	/* runtime data
	 */
	ofaIDbms     *instance;
	mysqlInfos    sInfos;

	/* UI
	 */
	GtkSizeGroup *group0;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_bin_xml          = PROVIDER_DATADIR "/ofa-mysql-connect-enter-bin.ui";

G_DEFINE_TYPE( ofaMySQLConnectEnterBin, ofa_mysql_connect_enter_bin, GTK_TYPE_BIN )

static void setup_bin( ofaMySQLConnectEnterBin *bin );
static void on_host_changed( GtkEntry *entry, ofaMySQLConnectEnterBin *bin );
static void on_port_changed( GtkEntry *entry, ofaMySQLConnectEnterBin *bin );
static void on_socket_changed( GtkEntry *entry, ofaMySQLConnectEnterBin *bin );
static void on_database_changed( GtkEntry *entry, ofaMySQLConnectEnterBin *bin );

static void
mysql_connect_enter_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_connect_enter_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MYSQL_CONNECT_ENTER_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_connect_enter_bin_parent_class )->finalize( instance );
}

static void
mysql_connect_enter_bin_dispose( GObject *instance )
{
	ofaMySQLConnectEnterBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_CONNECT_ENTER_BIN( instance ));

	priv = OFA_MYSQL_CONNECT_ENTER_BIN( instance )->priv;

	if( !priv->dispose_has_run ){

		/* unref object members here */
		g_clear_object( &priv->group0 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_connect_enter_bin_parent_class )->dispose( instance );
}

static void
ofa_mysql_connect_enter_bin_init( ofaMySQLConnectEnterBin *self )
{
	static const gchar *thisfn = "ofa_mysql_connect_enter_bin_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MYSQL_CONNECT_ENTER_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
							self, OFA_TYPE_MYSQL_CONNECT_ENTER_BIN, ofaMySQLConnectEnterBinPrivate );
}

static void
ofa_mysql_connect_enter_bin_class_init( ofaMySQLConnectEnterBinClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_connect_enter_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_connect_enter_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_connect_enter_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaMySQLConnectEnterBinPrivate ));

	/**
	 * ofaMySQLConnectEnterBin::changed:
	 *
	 * This signal is sent on the #ofaMySQLConnectEnterBin widget when
	 * any of the content changes.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaMySQLConnectEnterBin *bin,
	 * 						gpointer               user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_MYSQL_CONNECT_ENTER_BIN,
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
 * ofa_mysql_connect_enter_bin_new:
 * @instance: the provider instance
 *
 * Returns: a new #ofaMySQLConnectEnterBin instance as a #GtkWidget.
 */
GtkWidget *
ofa_mysql_connect_enter_bin_new( ofaIDbms *instance )
{
	ofaMySQLConnectEnterBin *bin;

	g_return_val_if_fail( instance && OFA_IS_IDBMS( instance ), NULL );

	bin = g_object_new( OFA_TYPE_MYSQL_CONNECT_ENTER_BIN, NULL );

	bin->priv->instance = instance;

	setup_bin( bin );

	g_signal_emit_by_name( instance, "dbms-changed", &bin->priv->sInfos );

	return( GTK_WIDGET( bin ));
}

static void
setup_bin( ofaMySQLConnectEnterBin *bin )
{
	ofaMySQLConnectEnterBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *entry, *label;

	priv = bin->priv;
	builder = gtk_builder_new_from_file( st_bin_xml );

	object = gtk_builder_get_object( builder, "mceb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "mceb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( bin ), GTK_WINDOW( toplevel ), "top" );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-host" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_host_changed ), bin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "pl-host" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-port" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_port_changed ), bin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "pl-port" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-socket" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_socket_changed ), bin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "pl-socket" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-database" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_database_changed ), bin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "pl-database" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
on_host_changed( GtkEntry *entry, ofaMySQLConnectEnterBin *bin )
{
	ofaMySQLConnectEnterBinPrivate *priv;

	priv = bin->priv;
	g_free( priv->sInfos.host );
	priv->sInfos.host = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( priv->instance, "dbms-changed", &priv->sInfos );
	g_signal_emit_by_name( bin, "ofa-changed" );
}

static void
on_port_changed( GtkEntry *entry, ofaMySQLConnectEnterBin *bin )
{
	ofaMySQLConnectEnterBinPrivate *priv;
	const gchar *port;

	priv = bin->priv;
	port = gtk_entry_get_text( entry );
	if( my_strlen( port )){
		priv->sInfos.port = atoi( port );
	} else {
		priv->sInfos.port = 0;
	}

	g_signal_emit_by_name( priv->instance, "dbms-changed", &priv->sInfos );
	g_signal_emit_by_name( bin, "ofa-changed" );
}

static void
on_socket_changed( GtkEntry *entry, ofaMySQLConnectEnterBin *bin )
{
	ofaMySQLConnectEnterBinPrivate *priv;

	priv = bin->priv;
	g_free( priv->sInfos.socket );
	priv->sInfos.socket = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( priv->instance, "dbms-changed", &priv->sInfos );
	g_signal_emit_by_name( bin, "ofa-changed" );
}

static void
on_database_changed( GtkEntry *entry, ofaMySQLConnectEnterBin *bin )
{
	ofaMySQLConnectEnterBinPrivate *priv;

	priv = bin->priv;
	g_free( priv->sInfos.dbname );
	priv->sInfos.dbname = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( priv->instance, "dbms-changed", &priv->sInfos );
	g_signal_emit_by_name( bin, "ofa-changed" );
}

/**
 * ofa_mysql_connect_enter_bin_get_size_group:
 * @instance: the provider instance
 * @bin: this #ofaMySQLConnectEnterBin composite widget
 * @column: the desired column.
 *
 * Returns: the #GtkSizeGroup for the specified @column.
 */
GtkSizeGroup *
ofa_mysql_connect_enter_bin_get_size_group( const ofaIDbms *instance, GtkWidget *bin, guint column )
{
	ofaMySQLConnectEnterBinPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_IDBMS( instance ), NULL );
	g_return_val_if_fail( bin && OFA_IS_MYSQL_CONNECT_ENTER_BIN( bin ), NULL );

	priv = OFA_MYSQL_CONNECT_ENTER_BIN( bin )->priv;

	if( !priv->dispose_has_run ){
		if( column == 0 ){
			return( priv->group0 );
		}
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofa_mysql_connect_enter_bin_is_valid:
 * @instance:
 * @bin:
 * @message: [allow-none]:
 *
 * check the entered connection informations
 * as we do not have any credentials, we can only check here whether
 * a database name is set
 */
gboolean
ofa_mysql_connect_enter_bin_is_valid( const ofaIDbms *instance, GtkWidget *bin, gchar **message )
{
	ofaMySQLConnectEnterBinPrivate *priv;
	gboolean ok;

	g_return_val_if_fail( instance && OFA_IS_IDBMS( instance ), FALSE );
	g_return_val_if_fail( bin && OFA_IS_MYSQL_CONNECT_ENTER_BIN( bin ), FALSE );

	priv = OFA_MYSQL_CONNECT_ENTER_BIN( bin )->priv;

	if( !priv->dispose_has_run ){

		ok = my_strlen( priv->sInfos.dbname ) > 0;

		if( message ){
			*message = ok ? NULL : g_strdup( _( "Database name is not set" ));
		}

		return( ok );
	}

	g_return_val_if_reached( FALSE );
}

/**
 * ofa_mysql_connect_enter_bin_get_database:
 * @instance:
 * @bin:
 *
 * Returns: the database name as a newly allocated string which should
 * be g_free() by the caller.
 */
gchar *
ofa_mysql_connect_enter_bin_get_database( const ofaIDbms *instance, GtkWidget *bin )
{
	ofaMySQLConnectEnterBinPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_IDBMS( instance ), NULL );
	g_return_val_if_fail( bin && OFA_IS_MYSQL_CONNECT_ENTER_BIN( bin ), FALSE );

	priv = OFA_MYSQL_CONNECT_ENTER_BIN( bin )->priv;

	if( !priv->dispose_has_run ){
		return( g_strdup( priv->sInfos.dbname ));
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofa_mysql_connect_enter_bin_apply:
 * @instance:
 * @dname:
 * @infos:
 *
 * Record the newly defined dossier in settings
 */
gboolean
ofa_mysql_connect_enter_bin_apply( const ofaIDbms *instance, const gchar *dname, void *infos )
{
	mysqlInfos *sInfos;
	gboolean ok;

	g_return_val_if_fail( instance && OFA_IS_IDBMS( instance ), FALSE );
	g_return_val_if_fail( my_strlen( dname ), FALSE );

	sInfos = ( mysqlInfos * ) infos;

	ok = ofa_settings_create_dossier(
						dname,
						SETTINGS_DBMS_PROVIDER, SETTINGS_TYPE_STRING, ofa_mysql_idbms_get_provider_name( instance ),
						SETTINGS_HOST,          SETTINGS_TYPE_STRING, sInfos->host,
						SETTINGS_PORT,          SETTINGS_TYPE_INT,    sInfos->port,
						SETTINGS_SOCKET,        SETTINGS_TYPE_STRING, sInfos->socket,
						SETTINGS_DBMS_DATABASE, SETTINGS_TYPE_STRING, sInfos->dbname,
						NULL );

	return( ok );
}
