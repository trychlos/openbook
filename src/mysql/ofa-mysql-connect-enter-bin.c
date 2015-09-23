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

/*
 * this structure is attached to the returned GtkWidget
 */
typedef struct {
	ofaIDbms   *module;
	mysqlInfos  sInfos;
}
	sPrivate;

#define IDBMS_DATA                      "mysql-IDBMS-data"

static const gchar *st_newui_xml        = PROVIDER_DATADIR "/ofa-mysql-connect-enter-bin.ui";
static const gchar *st_newui_mysql      = "MySQLConnectEnterBin";

static void       on_widget_finalized( sPrivate *priv, GObject *finalized_widget );
static GtkWidget *setup_widget( const ofaIDbms *instance, sPrivate *priv, GtkSizeGroup *group );
static void       on_host_changed( GtkEntry *entry, sPrivate *priv );
static void       on_port_changed( GtkEntry *entry, sPrivate *priv );
static void       on_socket_changed( GtkEntry *entry, sPrivate *priv );
static void       on_database_changed( GtkEntry *entry, sPrivate *priv );

/*
 * @parent: the GtkContainer in the DossierNewDlg dialog box which will
 *  contain the provider properties grid
 */
GtkWidget *
ofa_mysql_connect_enter_bin_new( ofaIDbms *instance, GtkSizeGroup *group )
{
	static const gchar *thisfn = "ofa_mysql_connect_enter_bin_new";
	GtkWidget *widget;
	sPrivate *priv;

	priv = g_new0( sPrivate, 1 );
	priv->module = instance;

	widget = setup_widget( instance, priv, group );

	g_object_set_data( G_OBJECT( widget ), IDBMS_DATA, priv );
	g_object_weak_ref( G_OBJECT( widget ), ( GWeakNotify ) on_widget_finalized, priv );

	g_debug( "%s: widget=%p (%s)", thisfn, ( void * ) widget, G_OBJECT_TYPE_NAME( widget ));

	return( widget );
}

static void
on_widget_finalized( sPrivate *priv, GObject *finalized_widget )
{
	static const gchar *thisfn = "ofa_mysql_connect_enter_bin_on_widget_finalized";

	g_debug( "%s: priv=%p, finalized_widget=%p",
			thisfn, ( void * ) priv, ( void * ) finalized_widget );

	ofa_mysql_free_connect_infos( &priv->sInfos );

	g_free( priv );
}

static GtkWidget *
setup_widget( const ofaIDbms *instance, sPrivate *priv, GtkSizeGroup *group )
{
	GtkWidget *top, *widget;
	GtkWidget *label, *entry;

	/* attach our sgdb provider grid */
	widget = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	top = my_utils_container_attach_from_ui( GTK_CONTAINER( widget ), st_newui_xml, st_newui_mysql, "top" );
	g_return_val_if_fail( top && GTK_IS_CONTAINER( top ), NULL );

	if( group ){
		label = my_utils_container_get_child_by_name( GTK_CONTAINER( top ), "pl-host" );
		g_return_val_if_fail( label && GTK_IS_LABEL( label ), NULL );
		gtk_size_group_add_widget( group, label );

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( top ), "pl-port" );
		g_return_val_if_fail( label && GTK_IS_LABEL( label ), NULL );
		gtk_size_group_add_widget( group, label );

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( top ), "pl-socket" );
		g_return_val_if_fail( label && GTK_IS_LABEL( label ), NULL );
		gtk_size_group_add_widget( group, label );

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( top ), "pl-database" );
		g_return_val_if_fail( label && GTK_IS_LABEL( label ), NULL );
		gtk_size_group_add_widget( group, label );
	}

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( top ), "p2-host" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), NULL );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_host_changed ), priv );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( top ), "p2-port" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), NULL );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_port_changed ), priv );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( top ), "p2-socket" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), NULL );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_socket_changed ), priv );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( top ), "p2-database" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), NULL );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_database_changed ), priv );

	return( widget );
}

static void
on_host_changed( GtkEntry *entry, sPrivate *priv )
{
	g_free( priv->sInfos.host );
	priv->sInfos.host = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( priv->module, "dbms-changed", &priv->sInfos );
}

static void
on_port_changed( GtkEntry *entry, sPrivate *priv )
{
	const gchar *port;

	port = gtk_entry_get_text( entry );
	if( my_strlen( port )){
		priv->sInfos.port = atoi( port );
	} else {
		priv->sInfos.port = 0;
	}

	g_signal_emit_by_name( priv->module, "dbms-changed", &priv->sInfos );
}

static void
on_socket_changed( GtkEntry *entry, sPrivate *priv )
{
	g_free( priv->sInfos.socket );
	priv->sInfos.socket = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( priv->module, "dbms-changed", &priv->sInfos );
}

static void
on_database_changed( GtkEntry *entry, sPrivate *priv )
{
	g_free( priv->sInfos.dbname );
	priv->sInfos.dbname = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( priv->module, "dbms-changed", &priv->sInfos );
}

/*
 * check the entered connection informations
 * as we do not have any credentials, we can only check here whether
 * a database name is set
 */
gboolean
ofa_mysql_connect_enter_bin_is_valid( const ofaIDbms *instance, GtkWidget *piece, gchar **message )
{
	sPrivate *priv;
	gboolean ok;

	g_return_val_if_fail( instance && OFA_IS_IDBMS( instance ), FALSE );
	g_return_val_if_fail( piece && GTK_IS_WIDGET( piece ), FALSE );

	priv = ( sPrivate * ) g_object_get_data( G_OBJECT( piece ), IDBMS_DATA );
	g_return_val_if_fail( priv, FALSE );

	ok = my_strlen( priv->sInfos.dbname ) > 0;

	if( message ){
		*message = ok ? NULL : g_strdup( _( "Database name is not set" ));
	}

	return( ok );
}

/*
 * returns the database name
 */
gchar *
ofa_mysql_connect_enter_bin_get_database( const ofaIDbms *instance, GtkWidget *piece )
{
	sPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_IDBMS( instance ), NULL );
	g_return_val_if_fail( piece && GTK_IS_WIDGET( piece ), NULL );

	priv = ( sPrivate * ) g_object_get_data( G_OBJECT( piece ), IDBMS_DATA );
	g_return_val_if_fail( priv, NULL );

	return( g_strdup( priv->sInfos.dbname ));
}

/*
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
