/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
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
#include "ofa-mysql-connect-enter-piece.h"

/*
 * this structure is attached to the GtkContainer parent of the grid
 * (i.e. the container from the DossierNewDlg dialog box)
 */
typedef struct {
	ofaIDbms     *module;

	GtkContainer *parent;
	GtkSizeGroup *group;
	GtkWidget    *message;

	mysqlInfos    sInfos;
}
	sPrivate;

#define IDBMS_DATA                      "mysql-IDBMS-data"

static const gchar *st_newui_xml        = PROVIDER_DATADIR "/ofa-mysql-connect-enter-piece.ui";
static const gchar *st_newui_mysql      = "MySQLConnectEnterPiece";

static void       on_parent_finalized( sPrivate *priv, GObject *finalized_parent );
static GtkWidget *set_parent( const ofaIDbms *instance, sPrivate *priv, GtkContainer *parent );
static void       setup_dialog( const ofaIDbms *instance, sPrivate *priv );
static void       on_host_changed( GtkEntry *entry, sPrivate *priv );
static void       on_port_changed( GtkEntry *entry, sPrivate *priv );
static void       on_socket_changed( GtkEntry *entry, sPrivate *priv );
static void       on_database_changed( GtkEntry *entry, sPrivate *priv );
static void       set_message( const sPrivate *priv, const gchar *msg );

/*
 * @parent: the GtkContainer in the DossierNewDlg dialog box which will
 *  contain the provider properties grid
 */
void
ofa_mysql_connect_enter_piece_attach_to( ofaIDbms *instance, GtkContainer *parent, GtkSizeGroup *group )
{
	static const gchar *thisfn = "ofa_mysql_connect_enter_piece_attach_to";
	sPrivate *priv;

	priv = g_new0( sPrivate, 1 );
	g_object_set_data( G_OBJECT( parent ), IDBMS_DATA, priv );
	g_debug( "%s: priv->sInfos=%p", thisfn, ( void * ) &priv->sInfos );

	priv->module = instance;
	priv->group = group;

	set_parent( instance, priv, parent );
	g_object_weak_ref( G_OBJECT( priv->parent ), ( GWeakNotify ) on_parent_finalized, priv );

	setup_dialog( instance, priv );
}

static void
on_parent_finalized( sPrivate *priv, GObject *finalized_parent )
{
	static const gchar *thisfn = "ofa_mysql_connect_enter_piece_on_parent_finalized";

	g_debug( "%s: priv=%p, finalized_parent=%p",
			thisfn, ( void * ) priv, ( void * ) finalized_parent );

	ofa_mysql_free_connect_infos( &priv->sInfos );

	g_free( priv );
}

static GtkWidget *
set_parent( const ofaIDbms *instance, sPrivate *priv, GtkContainer *parent )
{
	GtkWidget *window;
	GtkWidget *frame;

	/* attach our sgdb provider grid */
	window = my_utils_builder_load_from_path( st_newui_xml, st_newui_mysql );
	g_return_val_if_fail( window && GTK_IS_WINDOW( window ), NULL );

	frame = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "top-frame" );
	g_return_val_if_fail( frame && GTK_IS_FRAME( frame ), NULL );
	gtk_widget_reparent( frame, GTK_WIDGET( parent ));

	priv->parent = GTK_CONTAINER( frame );

	return( frame );
}

static void
setup_dialog( const ofaIDbms *instance, sPrivate *priv )
{
	GtkWidget *label, *entry;

	if( priv->group ){
		label = my_utils_container_get_child_by_name( priv->parent, "pl-host" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_size_group_add_widget( priv->group, label );

		label = my_utils_container_get_child_by_name( priv->parent, "pl-port" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_size_group_add_widget( priv->group, label );

		label = my_utils_container_get_child_by_name( priv->parent, "pl-socket" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_size_group_add_widget( priv->group, label );

		label = my_utils_container_get_child_by_name( priv->parent, "pl-database" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_size_group_add_widget( priv->group, label );
	}

	entry = my_utils_container_get_child_by_name( priv->parent, "p2-host" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_host_changed ), priv );

	entry = my_utils_container_get_child_by_name( priv->parent, "p2-port" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_port_changed ), priv );

	entry = my_utils_container_get_child_by_name( priv->parent, "p2-socket" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_socket_changed ), priv );

	entry = my_utils_container_get_child_by_name( priv->parent, "p2-database" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_database_changed ), priv );

	priv->message = my_utils_container_get_child_by_name( priv->parent, "pm-message" );
	g_return_if_fail( priv->message && GTK_IS_LABEL( priv->message ));
}

static void
on_host_changed( GtkEntry *entry, sPrivate *priv )
{
	g_free( priv->sInfos.host );
	priv->sInfos.host = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( priv->module, "changed", &priv->sInfos );
}

static void
on_port_changed( GtkEntry *entry, sPrivate *priv )
{
	const gchar *port;

	port = gtk_entry_get_text( entry );
	if( port && g_utf8_strlen( port, -1 )){
		priv->sInfos.port = atoi( port );
	} else {
		priv->sInfos.port = 0;
	}

	g_signal_emit_by_name( priv->module, "changed", &priv->sInfos );
}

static void
on_socket_changed( GtkEntry *entry, sPrivate *priv )
{
	g_free( priv->sInfos.socket );
	priv->sInfos.socket = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( priv->module, "changed", &priv->sInfos );
}

static void
on_database_changed( GtkEntry *entry, sPrivate *priv )
{
	g_free( priv->sInfos.dbname );
	priv->sInfos.dbname = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( priv->module, "changed", &priv->sInfos );
}

/*
 * check the entered connection informations
 * as we do not have any credentials, we can only check here whether
 * a database name is set
 */
gboolean
ofa_mysql_connect_enter_piece_is_valid( const ofaIDbms *instance, GtkContainer *parent )
{
	sPrivate *priv;
	gboolean ok;

	g_return_val_if_fail( instance && OFA_IS_IDBMS( instance ), FALSE );
	g_return_val_if_fail( parent && GTK_IS_CONTAINER( parent ), FALSE );

	priv = ( sPrivate * ) g_object_get_data( G_OBJECT( parent ), IDBMS_DATA );
	g_return_val_if_fail( priv, FALSE );

	set_message( priv, "" );
	ok = FALSE;

	if( !priv->sInfos.dbname || !g_utf8_strlen( priv->sInfos.dbname, -1 )){
		set_message( priv, _( "Database name is not set" ));

	} else {
		ok = TRUE;
	}

	return( ok );
}

/*
 * returns the database name
 */
gchar *
ofa_mysql_connect_enter_piece_get_database( const ofaIDbms *instance, GtkContainer *parent )
{
	sPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_IDBMS( instance ), NULL );
	g_return_val_if_fail( parent && GTK_IS_CONTAINER( parent ), NULL );

	priv = ( sPrivate * ) g_object_get_data( G_OBJECT( parent ), IDBMS_DATA );
	g_return_val_if_fail( priv, NULL );

	return( g_strdup( priv->sInfos.dbname ));
}

/*
 * Record the newly defined dossier in settings
 */
gboolean
ofa_mysql_connect_enter_piece_apply( const ofaIDbms *instance, const gchar *dname, void *infos )
{
	mysqlInfos *sInfos;
	gboolean ok;
	gchar *str;

	g_return_val_if_fail( instance && OFA_IS_IDBMS( instance ), FALSE );
	g_return_val_if_fail( dname && g_utf8_strlen( dname, -1 ), FALSE );

	sInfos = ( mysqlInfos * ) infos;

	str = g_strdup_printf( "%s;;;", sInfos->dbname );

	ok = ofa_settings_create_dossier(
						dname,
						SETTINGS_DBMS_PROVIDER, SETTINGS_TYPE_STRING, ofa_mysql_idbms_get_provider_name( instance ),
						SETTINGS_HOST,          SETTINGS_TYPE_STRING, sInfos->host,
						SETTINGS_PORT,          SETTINGS_TYPE_INT,    sInfos->port,
						SETTINGS_SOCKET,        SETTINGS_TYPE_STRING, sInfos->socket,
						SETTINGS_DATABASE,      SETTINGS_TYPE_STRING, str,
						NULL );

	g_free( str );

	return( ok );
}

static void
set_message( const sPrivate *priv, const gchar *msg )
{
	GdkRGBA color;

	if( priv->message ){
		gtk_label_set_text( GTK_LABEL( priv->message ), msg );
		gdk_rgba_parse( &color, "#ff0000" );
		gtk_widget_override_color(
				GTK_WIDGET( priv->message ), GTK_STATE_FLAG_NORMAL, &color );
	}
}
