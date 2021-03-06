/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#include <stdlib.h>
#include <string.h>

#include "my/my-utils.h"

#include "api/ofa-backup-header.h"
#include "api/ofa-igetter.h"

#include "mysql/ofa-mysql-prefs-bin.h"
#include "mysql/ofa-mysql-user-prefs.h"

/* private instance data
 */
typedef struct {
	gboolean      dispose_has_run;

	/* initialization
	 */
	ofaIGetter   *getter;

	/* UI
	 */
	GtkSizeGroup *group0;

	/* runtime data
	 */
	gchar        *backup_cmdline;
	gchar        *restore_gz;
	gchar        *restore_zip;
}
	ofaMySQLPrefsBinPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/mysql/ofa-mysql-prefs-bin.ui";

static void     setup_bin( ofaMySQLPrefsBin *self );
static void     on_backup_changed( GtkEntry *entry, ofaMySQLPrefsBin *self );
static void     on_restore_gz_changed( GtkEntry *entry, ofaMySQLPrefsBin *self );
static void     on_restore_zip_changed( GtkEntry *entry, ofaMySQLPrefsBin *self );
static gboolean is_valid( ofaMySQLPrefsBin *self, gchar **message );
static void     do_apply( ofaMySQLPrefsBin *self );

G_DEFINE_TYPE_EXTENDED( ofaMySQLPrefsBin, ofa_mysql_prefs_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaMySQLPrefsBin ))

static void
mysql_prefs_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_prefs_bin_finalize";
	ofaMySQLPrefsBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MYSQL_PREFS_BIN( instance ));

	/* free data members here */
	priv = ofa_mysql_prefs_bin_get_instance_private( OFA_MYSQL_PREFS_BIN( instance ));

	g_free( priv->backup_cmdline );
	g_free( priv->restore_gz );
	g_free( priv->restore_zip );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_prefs_bin_parent_class )->finalize( instance );
}

static void
mysql_prefs_bin_dispose( GObject *instance )
{
	ofaMySQLPrefsBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_PREFS_BIN( instance ));

	priv = ofa_mysql_prefs_bin_get_instance_private( OFA_MYSQL_PREFS_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_prefs_bin_parent_class )->dispose( instance );
}

static void
ofa_mysql_prefs_bin_init( ofaMySQLPrefsBin *self )
{
	static const gchar *thisfn = "ofa_mysql_prefs_bin_instance_init";
	ofaMySQLPrefsBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MYSQL_PREFS_BIN( self ));

	priv = ofa_mysql_prefs_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_mysql_prefs_bin_class_init( ofaMySQLPrefsBinClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_prefs_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_prefs_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_prefs_bin_finalize;

	/**
	 * ofaMySQLPrefsBin::changed:
	 *
	 * This signal is sent on the #ofaMySQLPrefsBin widget when
	 * any of the content changes.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaMySQLPrefsBin *self,
	 * 						gpointer               user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_MYSQL_PREFS_BIN,
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
 * ofa_mysql_prefs_bin_new:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: a new #ofaMySQLPrefsBin instance as a #GtkWidget.
 */
GtkWidget *
ofa_mysql_prefs_bin_new( ofaIGetter *getter )
{
	ofaMySQLPrefsBin *bin;
	ofaMySQLPrefsBinPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	bin = g_object_new( OFA_TYPE_MYSQL_PREFS_BIN, NULL );

	priv = ofa_mysql_prefs_bin_get_instance_private( bin );

	priv->getter = getter;

	setup_bin( bin );

	return( GTK_WIDGET( bin ));
}

static void
setup_bin( ofaMySQLPrefsBin *self )
{
	ofaMySQLPrefsBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *entry, *label;
	gchar *cmdline;

	priv = ofa_mysql_prefs_bin_get_instance_private( self );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "mpb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "mpb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mpb-backup-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_backup_changed ), self );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mpb-backup-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );
	cmdline = ofa_mysql_user_prefs_get_backup_command( priv->getter );
	gtk_entry_set_text( GTK_ENTRY( entry ), cmdline );
	g_free( cmdline );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mpb-restore-gz-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_restore_gz_changed ), self );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mpb-restore-gz-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );
	cmdline = ofa_mysql_user_prefs_get_restore_command( priv->getter, OFA_BACKUP_HEADER_GZ );
	gtk_entry_set_text( GTK_ENTRY( entry ), cmdline );
	g_free( cmdline );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mpb-restore-zip-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_restore_zip_changed ), self );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mpb-restore-zip-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );
	cmdline = ofa_mysql_user_prefs_get_restore_command( priv->getter, OFA_BACKUP_HEADER_ZIP );
	gtk_entry_set_text( GTK_ENTRY( entry ), cmdline );
	g_free( cmdline );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
on_backup_changed( GtkEntry *entry, ofaMySQLPrefsBin *self )
{
	ofaMySQLPrefsBinPrivate *priv;

	priv = ofa_mysql_prefs_bin_get_instance_private( self );

	g_free( priv->backup_cmdline );
	priv->backup_cmdline = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_restore_gz_changed( GtkEntry *entry, ofaMySQLPrefsBin *self )
{
	ofaMySQLPrefsBinPrivate *priv;

	priv = ofa_mysql_prefs_bin_get_instance_private( self );

	g_free( priv->restore_gz );
	priv->restore_gz = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_restore_zip_changed( GtkEntry *entry, ofaMySQLPrefsBin *self )
{
	ofaMySQLPrefsBinPrivate *priv;

	priv = ofa_mysql_prefs_bin_get_instance_private( self );

	g_free( priv->restore_zip );
	priv->restore_zip = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_mysql_prefs_bin_is_valid:
 * @bin: this #ofaMySQLPrefsBin instance.
 * @message: [allow-none][out]: error message placeholder.
 *
 * Returns: %TRUE if the @bin instance is valid.
 */
gboolean
ofa_mysql_prefs_bin_is_valid( ofaMySQLPrefsBin *bin, gchar **message )
{
	ofaMySQLPrefsBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_MYSQL_PREFS_BIN( bin ), FALSE );

	priv = ofa_mysql_prefs_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( is_valid( bin, message ));
}

static gboolean
is_valid( ofaMySQLPrefsBin *self, gchar **message )
{
	if( message ){
		*message = NULL;
	}

	/* this dialog is always valid, as MySQL provides nonetheless
	 * suitable default values */

	return( TRUE );
}

/**
 * ofa_mysql_prefs_bin_apply:
 * @bin: this #ofaMySQLPrefsBin instance.
 *
 * Apply the change, written them to user settings.
 */
void
ofa_mysql_prefs_bin_apply( ofaMySQLPrefsBin *bin )
{
	ofaMySQLPrefsBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_MYSQL_PREFS_BIN( bin ));

	priv = ofa_mysql_prefs_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	return( do_apply( bin ));
}

static void
do_apply( ofaMySQLPrefsBin *self )
{
	ofaMySQLPrefsBinPrivate *priv;

	priv = ofa_mysql_prefs_bin_get_instance_private( self );

	ofa_mysql_user_prefs_set_backup_command( priv->getter, priv->backup_cmdline );
	ofa_mysql_user_prefs_set_restore_command( priv->getter, OFA_BACKUP_HEADER_GZ, priv->restore_gz );
	ofa_mysql_user_prefs_set_restore_command( priv->getter, OFA_BACKUP_HEADER_ZIP, priv->restore_zip );
}
