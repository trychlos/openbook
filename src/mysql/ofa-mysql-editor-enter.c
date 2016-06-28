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
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-idbeditor.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-idbperiod.h"

#include "ofa-mysql-connect.h"
#include "ofa-mysql-editor-enter.h"

/* private instance data
 */
typedef struct {
	gboolean      dispose_has_run;

	/* setup
	 */
	ofaIDBMeta   *meta;
	ofaIDBPeriod *period;

	/* runtime data
	 */
	gchar        *host;
	gchar        *socket;
	guint         port;
	gchar        *database;

	/* UI
	 */
	GtkSizeGroup *group0;
}
	ofaMySQLEditorEnterPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/mysql/ofa-mysql-editor-enter.ui";

static void          idbeditor_iface_init( ofaIDBEditorInterface *iface );
static guint         idbeditor_get_interface_version( void );
static GtkSizeGroup *idbeditor_get_size_group( const ofaIDBEditor *instance, guint column );
static gboolean      idbeditor_get_valid( const ofaIDBEditor *instance, gchar **message );
static void          setup_bin( ofaMySQLEditorEnter *bin );
static void          on_host_changed( GtkEntry *entry, ofaMySQLEditorEnter *bin );
static void          on_port_changed( GtkEntry *entry, ofaMySQLEditorEnter *bin );
static void          on_socket_changed( GtkEntry *entry, ofaMySQLEditorEnter *bin );
static void          on_database_insert_text( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position, ofaMySQLEditorEnter *bin );
static void          on_database_changed( GtkEntry *entry, ofaMySQLEditorEnter *bin );

G_DEFINE_TYPE_EXTENDED( ofaMySQLEditorEnter, ofa_mysql_editor_enter, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaMySQLEditorEnter )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDBEDITOR, idbeditor_iface_init ))

static void
mysql_editor_enter_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_editor_enter_finalize";
	ofaMySQLEditorEnterPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MYSQL_EDITOR_ENTER( instance ));

	/* free data members here */
	priv = ofa_mysql_editor_enter_get_instance_private( OFA_MYSQL_EDITOR_ENTER( instance ));

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

	priv = ofa_mysql_editor_enter_get_instance_private( OFA_MYSQL_EDITOR_ENTER( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

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
	ofaMySQLEditorEnterPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MYSQL_EDITOR_ENTER( self ));

	priv = ofa_mysql_editor_enter_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_mysql_editor_enter_class_init( ofaMySQLEditorEnterClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_editor_enter_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_editor_enter_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_editor_enter_finalize;
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
idbeditor_get_interface_version( void )
{
	return( 1 );
}

static GtkSizeGroup *
idbeditor_get_size_group( const ofaIDBEditor *instance, guint column )
{
	static const gchar *thisfn = "ofa_mysql_editor_enter_get_size_group";
	ofaMySQLEditorEnterPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_EDITOR_ENTER( instance ), NULL );

	priv = ofa_mysql_editor_enter_get_instance_private( OFA_MYSQL_EDITOR_ENTER( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}

	g_warning( "%s: column=%u", thisfn, column );
	return( NULL );
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

	priv = ofa_mysql_editor_enter_get_instance_private( OFA_MYSQL_EDITOR_ENTER( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	ok = my_strlen( priv->database ) > 0;

	if( message ){
		*message = ok ? NULL : g_strdup( _( "Database name is not set" ));
	}

	return( ok );
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

	priv = ofa_mysql_editor_enter_get_instance_private( bin );

	builder = gtk_builder_new_from_resource( st_resource_ui );

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
	g_signal_connect( G_OBJECT( entry ), "insert-text", G_CALLBACK( on_database_insert_text ), bin );
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

	priv = ofa_mysql_editor_enter_get_instance_private( bin );

	g_free( priv->host );
	priv->host = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( bin, "ofa-changed" );
}

static void
on_port_changed( GtkEntry *entry, ofaMySQLEditorEnter *bin )
{
	ofaMySQLEditorEnterPrivate *priv;
	const gchar *port;

	priv = ofa_mysql_editor_enter_get_instance_private( bin );

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

	priv = ofa_mysql_editor_enter_get_instance_private( bin );

	g_free( priv->socket );
	priv->socket = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( bin, "ofa-changed" );
}

/*
 * keep the database name to the authorized set
 * see http://dev.mysql.com/doc/refman/5.7/en/identifiers.html
 * ASCII: [0-9,a-z,A-Z$_] (basic Latin letters, digits 0-9, dollar, underscore)
 */
static void
on_database_insert_text( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position, ofaMySQLEditorEnter *bin )
{
	GRegex *regex;
	gchar *replaced;

	regex = g_regex_new( "[A-Za-z0-9$_]+", 0, 0, NULL );
	replaced = g_regex_replace( regex, new_text, -1, 0, "", 0, NULL );
	g_regex_unref( regex );

	if( my_strlen( replaced )){
		g_signal_stop_emission_by_name( editable, "insert-text" );
	}

	g_free( replaced );
}

static void
on_database_changed( GtkEntry *entry, ofaMySQLEditorEnter *bin )
{
	ofaMySQLEditorEnterPrivate *priv;

	priv = ofa_mysql_editor_enter_get_instance_private( bin );

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
ofa_mysql_editor_enter_get_host( ofaMySQLEditorEnter *editor )
{
	ofaMySQLEditorEnterPrivate *priv;

	g_return_val_if_fail( editor && OFA_IS_MYSQL_EDITOR_ENTER( editor ), NULL );

	priv = ofa_mysql_editor_enter_get_instance_private( editor );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->host );
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
ofa_mysql_editor_enter_get_socket( ofaMySQLEditorEnter *editor )
{
	ofaMySQLEditorEnterPrivate *priv;

	g_return_val_if_fail( editor && OFA_IS_MYSQL_EDITOR_ENTER( editor ), NULL );

	priv = ofa_mysql_editor_enter_get_instance_private( editor );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->socket );
}

/**
 * ofa_mysql_editor_enter_get_port:
 * @editor: this #ofaMySQLEditorEnter instance.
 *
 * Returns: the DBMS listening port.
 */
guint
ofa_mysql_editor_enter_get_port( ofaMySQLEditorEnter *editor )
{
	ofaMySQLEditorEnterPrivate *priv;

	g_return_val_if_fail( editor && OFA_IS_MYSQL_EDITOR_ENTER( editor ), 0 );

	priv = ofa_mysql_editor_enter_get_instance_private( editor );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	return( priv->port );
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
ofa_mysql_editor_enter_get_database( ofaMySQLEditorEnter *editor )
{
	ofaMySQLEditorEnterPrivate *priv;

	g_return_val_if_fail( editor && OFA_IS_MYSQL_EDITOR_ENTER( editor ), NULL );

	priv = ofa_mysql_editor_enter_get_instance_private( editor );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->database );
}
