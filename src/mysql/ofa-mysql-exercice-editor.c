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

#include "api/ofa-exercice-meta.h"
#include "api/ofa-idbexercice-editor.h"
#include "api/ofa-idbprovider.h"

#include "ofa-mysql-connect.h"
#include "ofa-mysql-exercice-editor.h"

/* private instance data
 */
typedef struct {
	gboolean         dispose_has_run;

	/* setup
	 */
	ofaIDBProvider  *provider;
	gboolean         editable;

	/* runtime data
	 */
	gchar           *database;

	/* UI
	 */
	GtkSizeGroup *group0;
}
	ofaMysqlExerciceEditorPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/mysql/ofa-mysql-exercice-editor.ui";

static void          idbexercice_editor_iface_init( ofaIDBExerciceEditorInterface *iface );
static guint         idbexercice_editor_get_interface_version( void );
static GtkSizeGroup *idbexercice_editor_get_size_group( const ofaIDBExerciceEditor *instance, guint column );
static gboolean      idbexercice_editor_is_valid( const ofaIDBExerciceEditor *instance, gchar **message );
static gboolean      idbexercice_editor_apply( const ofaIDBExerciceEditor *instance );
static void          setup_bin( ofaMysqlExerciceEditor *bin );
static void          on_database_insert_text( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position, ofaMysqlExerciceEditor *bin );
static void          on_database_changed( GtkEntry *entry, ofaMysqlExerciceEditor *bin );

G_DEFINE_TYPE_EXTENDED( ofaMysqlExerciceEditor, ofa_mysql_exercice_editor, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaMysqlExerciceEditor )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDBEXERCICE_EDITOR, idbexercice_editor_iface_init ))

static void
mysql_exercice_editor_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_exercice_editor_finalize";
	ofaMysqlExerciceEditorPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MYSQL_EXERCICE_EDITOR( instance ));

	/* free data members here */
	priv = ofa_mysql_exercice_editor_get_instance_private( OFA_MYSQL_EXERCICE_EDITOR( instance ));

	g_free( priv->database );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_exercice_editor_parent_class )->finalize( instance );
}

static void
mysql_exercice_editor_dispose( GObject *instance )
{
	ofaMysqlExerciceEditorPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_EXERCICE_EDITOR( instance ));

	priv = ofa_mysql_exercice_editor_get_instance_private( OFA_MYSQL_EXERCICE_EDITOR( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_exercice_editor_parent_class )->dispose( instance );
}

static void
ofa_mysql_exercice_editor_init( ofaMysqlExerciceEditor *self )
{
	static const gchar *thisfn = "ofa_mysql_exercice_editor_instance_init";
	ofaMysqlExerciceEditorPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MYSQL_EXERCICE_EDITOR( self ));

	priv = ofa_mysql_exercice_editor_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_mysql_exercice_editor_class_init( ofaMysqlExerciceEditorClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_exercice_editor_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_exercice_editor_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_exercice_editor_finalize;
}

/*
 * ofaIDBExerciceEditor interface management
 */
static void
idbexercice_editor_iface_init( ofaIDBExerciceEditorInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_exercice_editor_idbexercice_editor_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idbexercice_editor_get_interface_version;
	iface->get_size_group = idbexercice_editor_get_size_group;
	iface->is_valid = idbexercice_editor_is_valid;
	iface->apply = idbexercice_editor_apply;
}

static guint
idbexercice_editor_get_interface_version( void )
{
	return( 1 );
}

static GtkSizeGroup *
idbexercice_editor_get_size_group( const ofaIDBExerciceEditor *instance, guint column )
{
	static const gchar *thisfn = "ofa_mysql_exercice_editor_get_size_group";
	ofaMysqlExerciceEditorPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_EXERCICE_EDITOR( instance ), NULL );

	priv = ofa_mysql_exercice_editor_get_instance_private( OFA_MYSQL_EXERCICE_EDITOR( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}

	g_warning( "%s: column=%u", thisfn, column );
	return( NULL );
}

/*
 * the database is mandatory
 */
static gboolean
idbexercice_editor_is_valid( const ofaIDBExerciceEditor *instance, gchar **message )
{
	ofaMysqlExerciceEditorPrivate *priv;
	gboolean ok;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_EXERCICE_EDITOR( instance ), FALSE );

	priv = ofa_mysql_exercice_editor_get_instance_private( OFA_MYSQL_EXERCICE_EDITOR( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	ok = my_strlen( priv->database ) > 0;

	if( message ){
		*message = ok ? NULL : g_strdup( _( "Database name is not set" ));
	}

	return( ok );
}

static gboolean
idbexercice_editor_apply( const ofaIDBExerciceEditor *instance )
{
	return( FALSE );
}

/**
 * ofa_mysql_exercice_editor_new:
 * @provider: the #ofaIDBProvider instance.
 * @editable: whether the widget is editable.
 *
 * Returns: a new #ofaMysqlExerciceEditor widget.
 */
ofaMysqlExerciceEditor *
ofa_mysql_exercice_editor_new( ofaIDBProvider *provider, gboolean editable )
{
	ofaMysqlExerciceEditor *bin;
	ofaMysqlExerciceEditorPrivate *priv;

	bin = g_object_new( OFA_TYPE_MYSQL_EXERCICE_EDITOR, NULL );

	priv = ofa_mysql_exercice_editor_get_instance_private( bin );

	priv->provider = provider;
	priv->editable = editable;

	setup_bin( bin );

	return( bin );
}

static void
setup_bin( ofaMysqlExerciceEditor *bin )
{
	ofaMysqlExerciceEditorPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *entry, *label;

	priv = ofa_mysql_exercice_editor_get_instance_private( bin );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "mee-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "mee-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( bin ), GTK_WINDOW( toplevel ), "top" );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "mee-database-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "insert-text", G_CALLBACK( on_database_insert_text ), bin );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_database_changed ), bin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "mee-database-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

/*
 * keep the database name to the authorized set
 * see http://dev.mysql.com/doc/refman/5.7/en/identifiers.html
 * ASCII: [0-9,a-z,A-Z$_] (basic Latin letters, digits 0-9, dollar, underscore)
 */
static void
on_database_insert_text( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position, ofaMysqlExerciceEditor *bin )
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
on_database_changed( GtkEntry *entry, ofaMysqlExerciceEditor *bin )
{
	ofaMysqlExerciceEditorPrivate *priv;

	priv = ofa_mysql_exercice_editor_get_instance_private( bin );

	g_free( priv->database );
	priv->database = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( bin, "ofa-changed" );
}

/**
 * ofa_mysql_exercice_editor_get_database:
 * @exercice_editor: this #ofaMysqlExerciceEditor instance.
 *
 * Returns: the DBMS database.
 *
 * The returned string is owned by the @exercice_editor, and should not be
 * released by the caller.
 */
const gchar *
ofa_mysql_exercice_editor_get_database( ofaMysqlExerciceEditor *exercice_editor )
{
ofaMysqlExerciceEditorPrivate *priv;

	g_return_val_if_fail( exercice_editor && OFA_IS_MYSQL_EXERCICE_EDITOR( exercice_editor ), NULL );

	priv = ofa_mysql_exercice_editor_get_instance_private( exercice_editor );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->database );
}
