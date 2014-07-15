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

#include "api/ofo-dossier.h"
#include "api/ofo-sgbd.h"

#include "core/my-utils.h"
#include "core/ofa-settings.h"

#include "ui/my-window-prot.h"
#include "ui/ofa-dbserver-login.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaDBserverLoginPrivate {

	const gchar *name;

	const gchar *p1_provider;
	const gchar *p1_host;
	const gchar *p1_port;
	const gchar *p1_socket;
	const gchar *p1_dbname;

	gchar       *p2_account;
	gchar       *p2_password;

	gboolean     p3_remove_account;
};

static const gchar *st_ui_xml   = PKGUIDIR "/ofa-dbserver-login.ui";
static const gchar *st_ui_id    = "DBServerLoginDlg";

/* keep the dbserver admin password */
static       gchar *st_passwd   = NULL;

G_DEFINE_TYPE( ofaDBserverLogin, ofa_dbserver_login, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_account_changed( GtkEntry *entry, ofaDBserverLogin *self );
static void      on_password_changed( GtkEntry *entry, ofaDBserverLogin *self );
static void      on_remove_account_toggled( GtkToggleButton *button, ofaDBserverLogin *self );
static void      check_for_enable_dlg( ofaDBserverLogin *self );
static gboolean  v_quit_on_ok( myDialog *dialog );

static void
dbserver_login_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dbserver_login_finalize";
	ofaDBserverLoginPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DBSERVER_LOGIN( instance ));

	priv = OFA_DBSERVER_LOGIN( instance )->private;

	/* free data members here */
	g_free( priv->p2_account );
	g_free( priv->p2_password );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dbserver_login_parent_class )->finalize( instance );
}

static void
dbserver_login_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_DBSERVER_LOGIN( instance ));

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dbserver_login_parent_class )->dispose( instance );
}

static void
ofa_dbserver_login_init( ofaDBserverLogin *self )
{
	static const gchar *thisfn = "ofa_dbserver_login_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DBSERVER_LOGIN( self ));

	self->private = g_new0( ofaDBserverLoginPrivate, 1 );
}

static void
ofa_dbserver_login_class_init( ofaDBserverLoginClass *klass )
{
	static const gchar *thisfn = "ofa_dbserver_login_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dbserver_login_dispose;
	G_OBJECT_CLASS( klass )->finalize = dbserver_login_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;
}

/**
 * ofa_dbserver_login_run:
 * @main: the main window of the application.
 */
gboolean
ofa_dbserver_login_run( ofaMainWindow *main_window,
		const gchar *name, const gchar *provider,
		const gchar *host, const gchar *port, const gchar *socket, const gchar *dbname,
		gchar **account, gchar **password,
		gboolean *remove_account )
{
	static const gchar *thisfn = "ofa_dbserver_login_run";
	ofaDBserverLogin *self;
	gboolean ok;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p", thisfn, main_window );

	ok = FALSE;

	self = g_object_new( OFA_TYPE_DBSERVER_LOGIN,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	self->private->name = name;
	self->private->p1_provider = provider;
	self->private->p1_host = host;
	self->private->p1_port = port;
	self->private->p1_socket = socket;
	self->private->p1_dbname = dbname;

	if( my_dialog_run_dialog( MY_DIALOG( self )) == GTK_RESPONSE_OK ){

		*account = g_strdup( self->private->p2_account );
		*password = g_strdup( self->private->p2_password );
		*remove_account = self->private->p3_remove_account;

		ok = TRUE;
	}

	g_object_unref( self );

	return( ok );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaDBserverLoginPrivate *priv;
	GtkWindow *toplevel;
	gchar *msg;
	GtkWidget *widget;
	gboolean bvalue;
	gchar *svalue;

	priv = OFA_DBSERVER_LOGIN( dialog )->private;
	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "message" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	msg = g_strdup_printf( _(
			"You are about to delete the '%s' dossier.\n"
			"Please provide below the connection informations "
			"for the DBserver administrative account." ),
				priv->name );
	gtk_label_set_text( GTK_LABEL( widget ), msg );
	g_free( msg );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "p1-provider" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	gtk_label_set_text( GTK_LABEL( widget ), priv->p1_provider );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "p1-host" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	gtk_label_set_text( GTK_LABEL( widget ), priv->p1_host );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "p1-port" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	gtk_label_set_text( GTK_LABEL( widget ), priv->p1_port );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "p1-socket" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	gtk_label_set_text( GTK_LABEL( widget ), priv->p1_socket );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "p1-dbname" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	gtk_label_set_text( GTK_LABEL( widget ), priv->p1_dbname );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "p2-account" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_account_changed ), dialog );
	svalue = ofa_settings_get_string( "DBServerLoginDlg-admin-account" );
	if( svalue ){
		gtk_entry_set_text( GTK_ENTRY( widget ), svalue );
		g_free( svalue );
	}

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "p2-password" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_password_changed ), dialog );
	if( st_passwd ){
		gtk_entry_set_text( GTK_ENTRY( widget ), st_passwd );
	}

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "p3-account" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_remove_account_toggled ), dialog );
	/* force a signal to be triggered */
	bvalue = ofa_settings_get_boolean( "DBServerLoginDlg-remove-account" );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), !bvalue );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), bvalue );
}

static void
on_account_changed( GtkEntry *entry, ofaDBserverLogin *self )
{
	ofaDBserverLoginPrivate *priv;
	const gchar *account;

	priv = self->private;

	account = gtk_entry_get_text( entry );
	g_free( priv->p2_account );
	priv->p2_account = g_strdup( account );

	check_for_enable_dlg( self );
}

static void
on_password_changed( GtkEntry *entry, ofaDBserverLogin *self )
{
	ofaDBserverLoginPrivate *priv;
	const gchar *password;

	priv = self->private;

	password = gtk_entry_get_text( entry );
	g_free( priv->p2_password );
	priv->p2_password = g_strdup( password );

	check_for_enable_dlg( self );
}

static void
on_remove_account_toggled( GtkToggleButton *button, ofaDBserverLogin *self )
{
	self->private->p3_remove_account = gtk_toggle_button_get_active( button );
}

static void
check_for_enable_dlg( ofaDBserverLogin *self )
{
	ofaDBserverLoginPrivate *priv;
	gboolean enabled;
	GtkWidget *btn;

	priv = self->private;

	/* #288: enable dlg should not depend of connection check */
	enabled = priv->p2_account && g_utf8_strlen( priv->p2_account, -1 );

	btn = my_utils_container_get_child_by_name(
									GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))),
									"btn-ok" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));

	gtk_widget_set_sensitive( btn, enabled );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	ofaDBserverLoginPrivate *priv;
	ofoSgbd *sgbd;
	gboolean ok;
	gint port;

	priv = OFA_DBSERVER_LOGIN( dialog )->private;

	ofa_settings_set_string( "DBServerLoginDlg-admin-account", priv->p2_account );
	ofa_settings_set_boolean( "DBServerLoginDlg-remove-account", priv->p3_remove_account );

	sgbd = ofo_sgbd_new( priv->p1_provider );
	port = 0;

	if( priv->p1_port && g_utf8_strlen( priv->p1_port, -1 )){
		port = atoi( priv->p1_port );
	}

	if( ofo_sgbd_connect(
					sgbd,
					priv->p1_host,
					port,
					priv->p1_socket,
					NULL,
					priv->p2_account,
					priv->p2_password )){
		ok = TRUE;

		g_free( st_passwd );
		st_passwd = g_strdup( priv->p2_password );
	}

	g_object_unref( sgbd );

	return( ok );
}
