/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include "my/my-isettings.h"
#include "my/my-utils.h"

#include "api/ofa-idbprovider.h"
#include "api/ofa-iprefs-page.h"

#include "ofa-mysql.h"
#include "ofa-mysql-idbprovider.h"
#include "ofa-mysql-iprefs-provider.h"
#include "ofa-mysql-prefs-bin.h"
#include "ofa-mysql-user-prefs.h"

/* private instance data
 */
struct _ofaMySQLPrefsBinPrivate {
	gboolean                 dispose_has_run;

	/* initialization
	 */
	myISettings             *settings;

	/* UI
	 */
	GtkSizeGroup            *group0;

	/* runtime data
	 */
	const ofaIPrefsProvider *instance;
	gchar                   *backup_cmdline;
	gchar                   *restore_cmdline;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/mysql/ofa-mysql-prefs-bin.ui";

static void     iprefs_page_iface_init( ofaIPrefsPageInterface *iface );
static guint    iprefs_page_get_interface_version( const ofaIPrefsPage *instance );
static gboolean iprefs_page_init( const ofaIPrefsPage *instance, myISettings *settings, gchar **label, gchar **msgerr );
static gboolean iprefs_page_get_valid( const ofaIPrefsPage *instance, gchar **msgerr );
static gboolean iprefs_page_apply( const ofaIPrefsPage *instance, gchar **msgerr );
static void     setup_bin( ofaMySQLPrefsBin *bin );
static void     on_backup_changed( GtkEntry *entry, ofaMySQLPrefsBin *bin );
static void     on_restore_changed( GtkEntry *entry, ofaMySQLPrefsBin *bin );

G_DEFINE_TYPE_EXTENDED( ofaMySQLPrefsBin, ofa_mysql_prefs_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaMySQLPrefsBin )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IPREFS_PAGE, iprefs_page_iface_init ))

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
	g_free( priv->restore_cmdline );

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
		g_clear_object( &priv->settings );
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

/*
 * ofaIPrefsPage interface management
 */
static void
iprefs_page_iface_init( ofaIPrefsPageInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_page_iprefs_page_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iprefs_page_get_interface_version;
	iface->init = iprefs_page_init;
	iface->get_valid = iprefs_page_get_valid;
	iface->apply = iprefs_page_apply;
}

static guint
iprefs_page_get_interface_version( const ofaIPrefsPage *instance )
{
	return( 1 );
}

static gboolean
iprefs_page_init( const ofaIPrefsPage *instance, myISettings *settings, gchar **label, gchar **msgerr )
{
	ofaMySQLPrefsBinPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_PREFS_BIN( instance ), FALSE );

	priv = ofa_mysql_prefs_bin_get_instance_private( OFA_MYSQL_PREFS_BIN( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	priv->settings = g_object_ref( settings );

	setup_bin( OFA_MYSQL_PREFS_BIN( instance ));

	if( label ){
		*label = g_strdup( ofa_mysql_idbprovider_get_provider_name());
	}

	return( TRUE );
}

static gboolean
iprefs_page_get_valid( const ofaIPrefsPage *instance, gchar **message )
{
	ofaMySQLPrefsBinPrivate *priv;
	gboolean ok;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_PREFS_BIN( instance ), FALSE );

	priv = ofa_mysql_prefs_bin_get_instance_private( OFA_MYSQL_PREFS_BIN( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	ok = TRUE;
	g_warning( "ofa_mysql_prefs_bin_iprefs_page_get_valid: to be written" );

	if( message ){
		*message = NULL;
	}

	return( ok );
}

static gboolean
iprefs_page_apply( const ofaIPrefsPage *instance, gchar **msgerr )
{
	ofaMySQLPrefsBinPrivate *priv;
	GtkWidget *entry;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_PREFS_BIN( instance ), FALSE );

	priv = ofa_mysql_prefs_bin_get_instance_private( OFA_MYSQL_PREFS_BIN( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "mpb-backup-entry" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	ofa_mysql_user_prefs_set_backup_command( gtk_entry_get_text( GTK_ENTRY( entry )));

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "mpb-restore-entry" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	ofa_mysql_user_prefs_set_restore_command( gtk_entry_get_text( GTK_ENTRY( entry )));

	return( TRUE );
}

/**
 * ofa_mysql_prefs_bin_new:
 *
 * Returns: a new #ofaMySQLPrefsBin instance as a #GtkWidget.
 */
GtkWidget *
ofa_mysql_prefs_bin_new( void )
{
	ofaMySQLPrefsBin *bin;

	bin = g_object_new( OFA_TYPE_MYSQL_PREFS_BIN, NULL );

	return( GTK_WIDGET( bin ));
}

static void
setup_bin( ofaMySQLPrefsBin *bin )
{
	ofaMySQLPrefsBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *entry, *label;
	gchar *cmdline;

	priv = ofa_mysql_prefs_bin_get_instance_private( bin );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "mpb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "mpb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( bin ), GTK_WINDOW( toplevel ), "top" );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "mpb-backup-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_backup_changed ), bin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "mpb-backup-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );
	cmdline = ofa_mysql_user_prefs_get_backup_command();
	gtk_entry_set_text( GTK_ENTRY( entry ), cmdline );
	g_free( cmdline );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "mpb-restore-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_restore_changed ), bin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "mpb-restore-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );
	cmdline = ofa_mysql_user_prefs_get_restore_command();
	gtk_entry_set_text( GTK_ENTRY( entry ), cmdline );
	g_free( cmdline );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
on_backup_changed( GtkEntry *entry, ofaMySQLPrefsBin *bin )
{
	ofaMySQLPrefsBinPrivate *priv;

	priv = ofa_mysql_prefs_bin_get_instance_private( bin );

	g_free( priv->backup_cmdline );
	priv->backup_cmdline = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( bin, "ofa-changed" );
}

static void
on_restore_changed( GtkEntry *entry, ofaMySQLPrefsBin *bin )
{
	ofaMySQLPrefsBinPrivate *priv;

	priv = ofa_mysql_prefs_bin_get_instance_private( bin );

	g_free( priv->restore_cmdline );
	priv->restore_cmdline = g_strdup( gtk_entry_get_text( entry ));

	g_signal_emit_by_name( bin, "ofa-changed" );
}
