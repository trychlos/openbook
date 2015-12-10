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
#include "api/my-settings.h"
#include "api/my-utils.h"
#include "api/ofa-idbeditor.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-ifile-period.h"

#include "ofa-mysql.h"
#include "ofa-mysql-connect.h"
#include "ofa-mysql-editor-enter.h"
#include "ofa-mysql-idbprovider.h"

/* private instance data
 */
struct _ofaMySQLEditorEnterPrivate {
	gboolean        dispose_has_run;

	/* setup
	 */
	ofaIDBMeta     *meta;
	ofaIFilePeriod *period;

	/* runtime data
	 */
	gchar          *host;
	gchar          *socket;
	guint           port;
	gchar          *database;

	/* UI
	 */
	GtkSizeGroup   *group0;
};

static const gchar *st_bin_xml          = PROVIDER_DATADIR "/ofa-mysql-editor-enter.ui";

static void           idbeditor_iface_init( ofaIDBEditorInterface *iface );
static guint          idbeditor_get_interface_version( const ofaIDBEditor *instance );
static GtkSizeGroup  *idbeditor_get_size_group( const ofaIDBEditor *instance, guint column );
static gboolean       idbeditor_get_valid( const ofaIDBEditor *instance, gchar **message );
static void           setup_bin( ofaMySQLEditorEnter *bin );
static void           on_host_changed( GtkEntry *entry, ofaMySQLEditorEnter *bin );
static void           on_port_changed( GtkEntry *entry, ofaMySQLEditorEnter *bin );
static void           on_socket_changed( GtkEntry *entry, ofaMySQLEditorEnter *bin );
static void           on_database_changed( GtkEntry *entry, ofaMySQLEditorEnter *bin );

G_DEFINE_TYPE_EXTENDED( ofaMySQLEditorEnter, ofa_mysql_editor_enter, GTK_TYPE_BIN, 0, \
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDBEDITOR, idbeditor_iface_init ));

static void
mysql_editor_enter_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_editor_enter_finalize";
	ofaMySQLEditorEnterPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MYSQL_EDITOR_ENTER( instance ));

	/* free data members here */
	priv = OFA_MYSQL_EDITOR_ENTER( instance )->priv;

	g_free( priv->host );
	g_free( priv->socket );
	g_free( priv->database );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_editor_enter_parent_class )->finalize( instance );
}

static void
mysql_editor_enter_dispose( GObject *instance )
{
	ofaMySQLEditorEnterPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_EDITOR_ENTER( instance ));

	priv = OFA_MYSQL_EDITOR_ENTER( instance )->priv;

	if( !priv->dispose_has_run ){

		/* unref object members here */
		g_clear_object( &priv->group0 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_editor_enter_parent_class )->dispose( instance );
}

static void
ofa_mysql_editor_enter_init( ofaMySQLEditorEnter *self )
{
	static const gchar *thisfn = "ofa_mysql_editor_enter_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MYSQL_EDITOR_ENTER( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
							self, OFA_TYPE_MYSQL_EDITOR_ENTER, ofaMySQLEditorEnterPrivate );
}

static void
ofa_mysql_editor_enter_class_init( ofaMySQLEditorEnterClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_editor_enter_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_editor_enter_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_editor_enter_finalize;

	g_type_class_add_private( klass, sizeof( ofaMySQLEditorEnterPrivate ));
}

/*
 * ofaIDBEditor interface management
 */
static void
idbeditor_iface_init( ofaIDBEditorInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_editor_enter_idbeditor_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idbeditor_get_interface_version;
	iface->get_size_group = idbeditor_get_size_group;
	iface->get_valid = idbeditor_get_valid;
}

static guint
idbeditor_get_interface_version( const ofaIDBEditor *instance )
{
	return( 1 );
}

static GtkSizeGroup *
idbeditor_get_size_group( const ofaIDBEditor *instance, guint column )
{
	static const gchar *thisfn = "ofa_mysql_editor_enter_get_size_group";
	ofaMySQLEditorEnterPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_EDITOR_ENTER( instance ), NULL );

	priv = OFA_MYSQL_EDITOR_ENTER( instance )->priv;

	if( !priv->dispose_has_run ){

		if( column == 0 ){
			return( priv->group0 );
		}
		g_warning( "%s: column=%u", thisfn, column );
		return( NULL );
	}

	g_return_val_if_reached( NULL );
}

/*
 * all the informations, but the database, are optional
 */
static gboolean
idbeditor_get_valid( const ofaIDBEditor *instance, gchar **message )
{
	ofaMySQLEditorEnterPrivate *priv;
	gboolean ok;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_EDITOR_ENTER( instance ), FALSE );

	priv = OFA_MYSQL_EDITOR_ENTER( instance )->priv;

	if( !priv->dispose_has_run ){

		ok = my_strlen( priv->database ) > 0;

		if( message ){
			*message = ok ? NULL : g_strdup( _( "Database name is not set" ));
		}

		return( ok );
	}

	g_return_val_if_reached( FALSE );
}

/**
 * ofa_mysql_editor_enter_new:
 *
 * Returns: a new #ofaIDBEditor container as a #GtkWidget.
 */
GtkWidget *
ofa_mysql_editor_enter_new( void )
{
	ofaMySQLEditorEnter *bin;

	bin = g_object_new( OFA_TYPE_MYSQL_EDITOR_ENTER, NULL );

	setup_bin( bin );

	return( GTK_WIDGET( bin ));
}

static void
setup_bin( ofaMySQLEditorEnter *bin )
{
	ofaMySQLEditorEnterPrivate *priv;
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

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-host-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_host_changed ), bin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-host-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-port-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_port_changed ), bin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-port-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-socket-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_socket_changed ), bin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-socket-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-database-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_database_changed ), bin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-database-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
on_host_changed( GtkEntry *entry, ofaMySQLEditorEnter *bin )
{
	ofaMySQLEditorEnterPrivate *priv;

	priv = bin->priv;
	g_free( priv->host );
	priv->host = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( bin, "ofa-changed" );
}

static void
on_port_changed( GtkEntry *entry, ofaMySQLEditorEnter *bin )
{
	ofaMySQLEditorEnterPrivate *priv;
	const gchar *port;

	priv = bin->priv;
	port = gtk_entry_get_text( entry );
	if( my_strlen( port )){
		priv->port = ( guint ) atoi( port );
	} else {
		priv->port = 0;
	}

	g_signal_emit_by_name( bin, "ofa-changed" );
}

static void
on_socket_changed( GtkEntry *entry, ofaMySQLEditorEnter *bin )
{
	ofaMySQLEditorEnterPrivate *priv;

	priv = bin->priv;
	g_free( priv->socket );
	priv->socket = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( bin, "ofa-changed" );
}

static void
on_database_changed( GtkEntry *entry, ofaMySQLEditorEnter *bin )
{
	ofaMySQLEditorEnterPrivate *priv;

	priv = bin->priv;
	g_free( priv->database );
	priv->database = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( bin, "ofa-changed" );
}

/**
 * ofa_mysql_editor_enter_get_host:
 * @editor: this #ofaMySQLEditorEnter instance.
 *
 * Returns: the DBMS host.
 *
 * The returned string is owned by the @editor, and should not be
 * released by the caller.
 */
const gchar *
ofa_mysql_editor_enter_get_host( const ofaMySQLEditorEnter *editor )
{
	ofaMySQLEditorEnterPrivate *priv;

	g_return_val_if_fail( editor && OFA_IS_MYSQL_EDITOR_ENTER( editor ), NULL );

	priv = editor->priv;

	if( !priv->dispose_has_run ){

		return(( const gchar * ) priv->host );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofa_mysql_editor_enter_get_socket:
 * @editor: this #ofaMySQLEditorEnter instance.
 *
 * Returns: the DBMS listening socket.
 *
 * The returned string is owned by the @editor, and should not be
 * released by the caller.
 */
const gchar *
ofa_mysql_editor_enter_get_socket( const ofaMySQLEditorEnter *editor )
{
	ofaMySQLEditorEnterPrivate *priv;

	g_return_val_if_fail( editor && OFA_IS_MYSQL_EDITOR_ENTER( editor ), NULL );

	priv = editor->priv;

	if( !priv->dispose_has_run ){

		return(( const gchar * ) priv->socket );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofa_mysql_editor_enter_get_port:
 * @editor: this #ofaMySQLEditorEnter instance.
 *
 * Returns: the DBMS listening port.
 */
guint
ofa_mysql_editor_enter_get_port( const ofaMySQLEditorEnter *editor )
{
	ofaMySQLEditorEnterPrivate *priv;

	g_return_val_if_fail( editor && OFA_IS_MYSQL_EDITOR_ENTER( editor ), 0 );

	priv = editor->priv;

	if( !priv->dispose_has_run ){

		return( priv->port );
	}

	g_return_val_if_reached( 0 );
}

/**
 * ofa_mysql_editor_enter_get_database:
 * @editor: this #ofaMySQLEditorEnter instance.
 *
 * Returns: the DBMS database.
 *
 * The returned string is owned by the @editor, and should not be
 * released by the caller.
 */
const gchar *
ofa_mysql_editor_enter_get_database( const ofaMySQLEditorEnter *editor )
{
	ofaMySQLEditorEnterPrivate *priv;

	g_return_val_if_fail( editor && OFA_IS_MYSQL_EDITOR_ENTER( editor ), NULL );

	priv = editor->priv;

	if( !priv->dispose_has_run ){

		return(( const gchar * ) priv->database );
	}

	g_return_val_if_reached( NULL );
}
