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
#include <string.h>

#include "api/my-utils.h"
#include "api/ofa-settings.h"

#include "ofa-mysql.h"
#include "ofa-mysql-backup.h"
#include "ofa-mysql-idbms.h"
#include "ofa-mysql-prefs-bin.h"

/* private instance data
 */
struct _ofaMySQLPrefsBinPrivate {
	gboolean               dispose_has_run;

	/* UI
	 */
	GtkSizeGroup          *group0;

	/* runtime data
	 */
	const ofaIPreferences *instance;
	gchar                 *backup_cmdline;
	gchar                 *restore_cmdline;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_bin_xml          = PROVIDER_DATADIR "/ofa-mysql-prefs-bin.ui";

G_DEFINE_TYPE( ofaMySQLPrefsBin, ofa_mysql_prefs_bin, GTK_TYPE_BIN )

static void setup_composite( ofaMySQLPrefsBin *bin );
static void on_backup_changed( GtkEntry *entry, ofaMySQLPrefsBin *bin );
static void on_restore_changed( GtkEntry *entry, ofaMySQLPrefsBin *bin );

static void
mysql_prefs_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_prefs_bin_finalize";
	ofaMySQLPrefsBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MYSQL_PREFS_BIN( instance ));

	/* free data members here */
	priv = OFA_MYSQL_PREFS_BIN( instance )->priv;

	g_free( priv->backup_cmdline );
	g_free( priv->restore_cmdline );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_prefs_bin_parent_class )->finalize( instance );
}

static void
mysql_prefs_bin_dispose( GObject *instance )
{
	ofaMySQLPrefsBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_PREFS_BIN( instance ));

	priv = OFA_MYSQL_PREFS_BIN( instance )->priv;

	if( !priv->dispose_has_run ){

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

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MYSQL_PREFS_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
							self, OFA_TYPE_MYSQL_PREFS_BIN, ofaMySQLPrefsBinPrivate );
}

static void
ofa_mysql_prefs_bin_class_init( ofaMySQLPrefsBinClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_prefs_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_prefs_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_prefs_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaMySQLPrefsBinPrivate ));

	/**
	 * ofaMySQLPrefsBin::changed:
	 *
	 * This signal is sent on the #ofaMySQLPrefsBin widget when
	 * any of the content changes.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaMySQLPrefsBin *bin,
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
 * @instance: the provider instance
 * @label: the label to be set on the notebook page.
 *
 * Returns: a new #ofaMySQLPrefsBin instance as a #GtkWidget.
 */
GtkWidget *
ofa_mysql_prefs_bin_new( const ofaIPreferences *instance, gchar **label )
{
	ofaMySQLPrefsBin *bin;

	g_return_val_if_fail( instance && OFA_IS_IPREFERENCES( instance ), NULL );

	bin = g_object_new( OFA_TYPE_MYSQL_PREFS_BIN, NULL );

	bin->priv->instance = instance;
	setup_composite( bin );

	if( label ){
		*label = g_strdup( ofa_mysql_idbms_get_provider_name( OFA_IDBMS( instance )));
	}

	return( GTK_WIDGET( bin ));
}

static void
setup_composite( ofaMySQLPrefsBin *bin )
{
	ofaMySQLPrefsBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *entry;
	gchar *cmdline;

	priv = bin->priv;
	builder = gtk_builder_new_from_file( st_bin_xml );

	object = gtk_builder_get_object( builder, "mpb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "mpb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( bin ), GTK_WINDOW( toplevel ), "top" );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "backup" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_backup_changed ), bin );
	cmdline = ofa_settings_get_string_ex( SETTINGS_TARGET_USER, PREFS_GROUP, PREFS_BACKUP_CMDLINE );
	if( my_strlen( cmdline )){
		gtk_entry_set_text( GTK_ENTRY( entry ), cmdline );
	} else {
		gtk_entry_set_text( GTK_ENTRY( entry ), ofa_mysql_get_def_backup_cmd( OFA_IDBMS( priv->instance )));
	}
	g_free( cmdline );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "restore" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_restore_changed ), bin );
	cmdline = ofa_settings_get_string_ex( SETTINGS_TARGET_USER, PREFS_GROUP, PREFS_RESTORE_CMDLINE );
	if( my_strlen( cmdline )){
		gtk_entry_set_text( GTK_ENTRY( entry ), cmdline );
	} else {
		gtk_entry_set_text( GTK_ENTRY( entry ), ofa_mysql_get_def_restore_cmd( OFA_IDBMS( priv->instance )));
	}
	g_free( cmdline );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
on_backup_changed( GtkEntry *entry, ofaMySQLPrefsBin *bin )
{
	ofaMySQLPrefsBinPrivate *priv;

	priv = bin->priv;
	g_free( priv->backup_cmdline );
	priv->backup_cmdline = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( bin, "ofa-changed" );
}

static void
on_restore_changed( GtkEntry *entry, ofaMySQLPrefsBin *bin )
{
	ofaMySQLPrefsBinPrivate *priv;

	priv = bin->priv;
	g_free( priv->restore_cmdline );
	priv->restore_cmdline = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( bin, "ofa-changed" );
}

/**
 * ofa_mysql_prefs_bin_is_valid:
 * @instance: the provider instance
 * @bin: this #ofaMySQLPrefsBin instance.
 * @message: [allow-none]:
 *
 * Returns: %TRUE if the widget is valid, %FALSE else.
 */
gboolean
ofa_mysql_prefs_bin_is_valid( const ofaIPreferences *instance, GtkWidget *bin, gchar **message )
{
	ofaMySQLPrefsBinPrivate *priv;
	gboolean ok;

	g_return_val_if_fail( instance && OFA_IS_IPREFERENCES( instance ), FALSE );
	g_return_val_if_fail( bin && OFA_IS_MYSQL_PREFS_BIN( bin ), FALSE );

	priv = OFA_MYSQL_PREFS_BIN( bin )->priv;

	if( !priv->dispose_has_run ){

		ok = TRUE;
		g_warning( "ofa_mysql_prefs_bin_is_valid: to be written" );

		if( message ){
			*message = NULL;
		}

		return( ok );
	}

	g_return_val_if_reached( FALSE );
}

/**
 * ofa_mysql_prefs_bin_apply:
 * @instance: the provider instance
 * @bin: this #ofaMySQLPrefsBin instance.
 *
 * Writes the data into the user's settings.
 */
void
ofa_mysql_prefs_bin_apply( const ofaIPreferences *instance, GtkWidget *bin )
{
	ofaMySQLPrefsBinPrivate *priv;
	GtkWidget *entry;

	g_return_if_fail( instance && OFA_IS_IPREFERENCES( instance ));
	g_return_if_fail( bin && OFA_IS_MYSQL_PREFS_BIN( bin ));

	priv = OFA_MYSQL_PREFS_BIN( bin )->priv;

	if( !priv->dispose_has_run ){

		entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "backup" );
		g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
		ofa_settings_set_string_ex(
				SETTINGS_TARGET_USER, PREFS_GROUP, PREFS_BACKUP_CMDLINE,
				gtk_entry_get_text( GTK_ENTRY( entry )));

		entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "restore" );
		g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
		ofa_settings_set_string_ex(
				SETTINGS_TARGET_USER, PREFS_GROUP, PREFS_RESTORE_CMDLINE,
				gtk_entry_get_text( GTK_ENTRY( entry )));

	}
}
