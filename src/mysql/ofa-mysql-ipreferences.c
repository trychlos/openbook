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
#include "ofa-mysql-ipreferences.h"

/*
 * this structure is attached to the GtkContainer parent (the one which
 * is returned by do_init() method)
 */
typedef struct {
	const ofaIPreferences *module;
}
	sPrivate;

#define PREFS_DATA                "ofa-mysql-prefs-data"

static const gchar *st_ui_xml   = PROVIDER_DATADIR "/ofa-mysql-prefs.piece.ui";
static const gchar *st_ui_mysql = "MySQLPrefsWindow";

static guint      ipreferences_get_interface_version( const ofaIPreferences *instance );
static GtkWidget *ipreferences_do_init ( const ofaIPreferences *instance, GtkNotebook *book );
static void       on_container_weak_notify( sPrivate *priv, GObject *was_the_container );
static GtkWidget *window_set_parent( const ofaIPreferences *instance, GtkNotebook *book );
static void       page_init_backup( const ofaIPreferences *instance, GtkContainer *page, sPrivate *priv );
static gboolean   ipreferences_do_check( const ofaIPreferences *instance, GtkWidget *page );
static void       ipreferences_do_apply( const ofaIPreferences *instance, GtkWidget *page );

void
ofa_mysql_ipreferences_iface_init( ofaIPreferencesInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_ipreferences_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = ipreferences_get_interface_version;
	iface->do_init = ipreferences_do_init;
	iface->do_check = ipreferences_do_check;
	iface->do_apply = ipreferences_do_apply;
}

static guint
ipreferences_get_interface_version( const ofaIPreferences *instance )
{
	return( 1 );
}

/*
 * @book: the GtkNotebook in the Preferences dialog box which will
 *  contain our own preferences grid
 */
static GtkWidget *
ipreferences_do_init( const ofaIPreferences *instance, GtkNotebook *book )
{
	sPrivate *priv;
	GtkWidget *page;

	priv = g_new0( sPrivate, 1 );
	g_object_weak_ref( G_OBJECT( book ), ( GWeakNotify ) on_container_weak_notify, priv );

	page = window_set_parent( instance, book );

	if( page ){
		priv->module = instance;
		g_object_set_data( G_OBJECT( page ), PREFS_DATA, priv );
		page_init_backup( instance, GTK_CONTAINER( page ), priv );
	}

	return( page );
}

static void
on_container_weak_notify( sPrivate *priv, GObject *was_the_container )
{
	static const gchar *thisfn = "ofa_mysql_prefs_on_container_weak_notify";

	g_debug( "%s: priv=%p, the_container_was=%p",
			thisfn, ( void * ) priv, ( void * ) was_the_container );

	g_free( priv );
}

static GtkWidget *
window_set_parent( const ofaIPreferences *instance, GtkNotebook *book )
{
	GtkWidget *window;
	GtkWidget *label;
	GtkWidget *grid, *alignment;

	/* attach our sgdb provider grid */
	window = my_utils_builder_load_from_path( st_ui_xml, st_ui_mysql );
	g_return_val_if_fail( window && GTK_IS_WINDOW( window ), NULL );

	grid = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "top-grid" );
	g_return_val_if_fail( grid && GTK_IS_GRID( grid ), NULL );

	alignment = gtk_alignment_new( 0.5, 0.5, 1, 1 );
	gtk_alignment_set_padding( GTK_ALIGNMENT( alignment ), 4, 4, 4, 4 );
	gtk_widget_reparent( grid, alignment );

	label = gtk_label_new( ofa_mysql_idbms_get_provider_name( NULL ));
	gtk_notebook_append_page( book, alignment, label );


	return( alignment );
}

/*
 * @parent: the provider grid
 */
static void
page_init_backup( const ofaIPreferences *instance, GtkContainer *page, sPrivate *priv )
{
	GtkWidget *entry;
	gchar *cmdline;

	entry = my_utils_container_get_child_by_name( page, "backup" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	cmdline = ofa_settings_get_string_ex( SETTINGS_TARGET_USER, PREFS_GROUP, PREFS_BACKUP_CMDLINE );
	if( my_strlen( cmdline )){
		gtk_entry_set_text( GTK_ENTRY( entry ), cmdline );
	} else {
		gtk_entry_set_text( GTK_ENTRY( entry ), ofa_mysql_get_def_backup_cmd( OFA_IDBMS( instance )));
	}
	g_free( cmdline );

	entry = my_utils_container_get_child_by_name( page, "restore" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	cmdline = ofa_settings_get_string_ex( SETTINGS_TARGET_USER, PREFS_GROUP, PREFS_RESTORE_CMDLINE );
	if( my_strlen( cmdline )){
		gtk_entry_set_text( GTK_ENTRY( entry ), cmdline );
	} else {
		gtk_entry_set_text( GTK_ENTRY( entry ), ofa_mysql_get_def_restore_cmd( OFA_IDBMS( instance )));
	}
	g_free( cmdline );
}

static gboolean
ipreferences_do_check( const ofaIPreferences *instance, GtkWidget *page )
{
	return( TRUE );
}

static void
ipreferences_do_apply( const ofaIPreferences *instance, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_mysql_ipreferences_do_apply";
	sPrivate *priv;
	GtkWidget *entry;

	priv = ( sPrivate * ) g_object_get_data( G_OBJECT( page ), PREFS_DATA );

	g_debug( "%s: instance=%p, page=%p, priv=%p",
			thisfn, ( void * ) instance, ( void * ) page, ( void * ) priv );

	g_return_if_fail( priv );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "backup" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	ofa_settings_set_string_ex(
			SETTINGS_TARGET_USER, PREFS_GROUP, PREFS_BACKUP_CMDLINE,
			gtk_entry_get_text( GTK_ENTRY( entry )));

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "restore" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	ofa_settings_set_string_ex(
			SETTINGS_TARGET_USER, PREFS_GROUP, PREFS_RESTORE_CMDLINE,
			gtk_entry_get_text( GTK_ENTRY( entry )));
}
