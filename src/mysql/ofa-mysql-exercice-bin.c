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
#include "my/my-utils.h"

#include "api/ofa-idbexercice-editor.h"
#include "api/ofa-idbprovider.h"

#include "ofa-mysql-connect.h"
#include "ofa-mysql-exercice-bin.h"

/* private instance data
 */
typedef struct {
	gboolean      dispose_has_run;

	/* initialization
	 */
	gchar        *settings_prefix;
	guint         rule;

	/* runtime
	 */
	gchar        *database;

	/* UI
	 */
	GtkSizeGroup *group0;
}
	ofaMysqlExerciceBinPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/mysql/ofa-mysql-exercice-bin.ui";

static void setup_bin( ofaMysqlExerciceBin *self );
static void on_database_insert_text( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position, ofaMysqlExerciceBin *self );
static void on_database_changed( GtkEntry *entry, ofaMysqlExerciceBin *self );

G_DEFINE_TYPE_EXTENDED( ofaMysqlExerciceBin, ofa_mysql_exercice_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaMysqlExerciceBin ))

static void
mysql_exercice_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_exercice_bin_finalize";
	ofaMysqlExerciceBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MYSQL_EXERCICE_BIN( instance ));

	/* free data members here */
	priv = ofa_mysql_exercice_bin_get_instance_private( OFA_MYSQL_EXERCICE_BIN( instance ));

	g_free( priv->settings_prefix );
	g_free( priv->database );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_exercice_bin_parent_class )->finalize( instance );
}

static void
mysql_exercice_bin_dispose( GObject *instance )
{
	ofaMysqlExerciceBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_EXERCICE_BIN( instance ));

	priv = ofa_mysql_exercice_bin_get_instance_private( OFA_MYSQL_EXERCICE_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_exercice_bin_parent_class )->dispose( instance );
}

static void
ofa_mysql_exercice_bin_init( ofaMysqlExerciceBin *self )
{
	static const gchar *thisfn = "ofa_mysql_exercice_bin_instance_init";
	ofaMysqlExerciceBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MYSQL_EXERCICE_BIN( self ));

	priv = ofa_mysql_exercice_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_mysql_exercice_bin_class_init( ofaMysqlExerciceBinClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_exercice_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_exercice_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_exercice_bin_finalize;

	/**
	 * ofaMysqlExerciceBin::ofa-changed:
	 *
	 * This signal is sent on the #ofaMysqlExerciceBin when any of the
	 * underlying information is changed.
	 *
	 * There is no argument.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaMysqlExerciceBin *bin,
	 * 						gpointer           user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_MYSQL_EXERCICE_BIN,
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
 * ofa_mysql_exercice_bin_new:
 * @settings_prefix: the prefix of a user preference key.
 * @rule: the usage of the widget.
 *
 * Returns: a new #ofaMysqlExerciceBin widget.
 */
ofaMysqlExerciceBin *
ofa_mysql_exercice_bin_new( const gchar *settings_prefix, guint rule )
{
	ofaMysqlExerciceBin *bin;
	ofaMysqlExerciceBinPrivate *priv;

	bin = g_object_new( OFA_TYPE_MYSQL_EXERCICE_BIN, NULL );

	priv = ofa_mysql_exercice_bin_get_instance_private( bin );

	g_free( priv->settings_prefix );
	priv->settings_prefix = g_strdup( settings_prefix );

	priv->rule = rule;

	setup_bin( bin );

	return( bin );
}

static void
setup_bin( ofaMysqlExerciceBin *self )
{
	ofaMysqlExerciceBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *entry, *label;

	priv = ofa_mysql_exercice_bin_get_instance_private( self );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "meb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "meb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "meb-database-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "insert-text", G_CALLBACK( on_database_insert_text ), self );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_database_changed ), self );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "meb-database-prompt" );
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
on_database_insert_text( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position, ofaMysqlExerciceBin *self )
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
on_database_changed( GtkEntry *entry, ofaMysqlExerciceBin *self )
{
	ofaMysqlExerciceBinPrivate *priv;

	priv = ofa_mysql_exercice_bin_get_instance_private( self );

	g_free( priv->database );
	priv->database = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_mysql_exercice_bin_get_size_group:
 * @bin: this #ofaMysqlExerciceBin instance.
 * @column: the desired column.
 *
 * Returns: the #GtkSizeGroup for the @column if it exists.
 */
GtkSizeGroup *
ofa_mysql_exercice_bin_get_size_group( ofaMysqlExerciceBin *bin, guint column )
{
	static const gchar *thisfn = "ofa_mysql_exercice_bin_get_size_group";
	ofaMysqlExerciceBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_MYSQL_EXERCICE_BIN( bin ), NULL );

	priv = ofa_mysql_exercice_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}

	g_warning( "%s: column=%u", thisfn, column );
	return( NULL );
}

/**
 * ofa_mysql_exercice_bin_is_valid:
 * @bin: this #ofaMysqlExerciceBin instance.
 * @¢message: [out][allow-none]: a placeholder for an error message.
 *
 * Returns: %TRUE if the widget is valid.
 */
gboolean
ofa_mysql_exercice_bin_is_valid( ofaMysqlExerciceBin *bin, gchar **message )
{
	ofaMysqlExerciceBinPrivate *priv;
	gboolean ok;

	g_return_val_if_fail( bin && OFA_IS_MYSQL_EXERCICE_BIN( bin ), FALSE );

	priv = ofa_mysql_exercice_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	ok = TRUE;
	if( message ){
		*message = NULL;
	}

	if( !my_strlen( priv->database )){
		ok = FALSE;
		if( message ){
			*message = g_strdup( _( "The database name is empty" ));
		}
	}

	return( ok );
}

/**
 * ofa_mysql_exercice_bin_apply:
 * @bin: this #ofaMysqlExerciceBin instance.
 *
 * Returns: %TRUE if the widget has been applied (whatever this may mean
 * here).
 */
gboolean
ofa_mysql_exercice_bin_apply( ofaMysqlExerciceBin *bin )
{
	ofaMysqlExerciceBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_MYSQL_EXERCICE_BIN( bin ), FALSE );

	priv = ofa_mysql_exercice_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( TRUE );
}

/**
 * ofa_mysql_exercice_bin_get_database:
 * @exercice_bin: this #ofaMysqlExerciceBin instance.
 *
 * Returns: the DBMS database.
 *
 * The returned string is owned by the @exercice_bin, and should not be
 * released by the caller.
 */
const gchar *
ofa_mysql_exercice_bin_get_database( ofaMysqlExerciceBin *exercice_bin )
{
ofaMysqlExerciceBinPrivate *priv;

	g_return_val_if_fail( exercice_bin && OFA_IS_MYSQL_EXERCICE_BIN( exercice_bin ), NULL );

	priv = ofa_mysql_exercice_bin_get_instance_private( exercice_bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->database );
}
