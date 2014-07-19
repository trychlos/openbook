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

#include "api/ofo-dossier.h"
#include "api/ofo-sgbd.h"

#include "core/my-utils.h"
#include "core/ofa-settings.h"

#include "ui/my-window-prot.h"
#include "ui/ofa-dossier-delete.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaDossierDeletePrivate {

	/* input parameters
	 */
	const gchar    *label;
	const gchar    *provider;
	const gchar    *host;
	gint            port;
	const gchar    *socket;
	const gchar    *account;
	const gchar    *password;
	const gchar    *dbname;
	const gchar    *admin_account;

	/* data
	 */
	gint            db_mode;
	gboolean        drop_account;
	gboolean        keep_asking;

	/* UI
	 */
	GtkRadioButton *p2_db_drop;
	GtkRadioButton *p2_db_leave;
};

/* what to do with the database ?
 */
enum {
	DB_DROP = 1,
	DB_LEAVE_AS_IS
};

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-dossier-delete.ui";
static const gchar  *st_ui_id  = "DossierDeleteDlg";

G_DEFINE_TYPE( ofaDossierDelete, ofa_dossier_delete, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_db_mode_toggled( GtkToggleButton *btn, ofaDossierDelete *self );
static void      on_account_toggled( GtkToggleButton *btn, ofaDossierDelete *self );
static void      on_keep_asking_toggled( GtkToggleButton *btn, ofaDossierDelete *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_delete_dossier( ofaDossierDelete *self );
static void      drop_database( ofaDossierDelete *self );
static void      drop_user( ofaDossierDelete *self );

static void
dossier_delete_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_delete_finalize";
	ofaDossierDeletePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_DELETE( instance ));

	priv = OFA_DOSSIER_DELETE( instance )->private;

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_delete_parent_class )->finalize( instance );
}

static void
dossier_delete_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_DOSSIER_DELETE( instance ));

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_delete_parent_class )->dispose( instance );
}

static void
ofa_dossier_delete_init( ofaDossierDelete *self )
{
	static const gchar *thisfn = "ofa_dossier_delete_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_DELETE( self ));

	self->private = g_new0( ofaDossierDeletePrivate, 1 );
}

static void
ofa_dossier_delete_class_init( ofaDossierDeleteClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_delete_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_delete_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_delete_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;
}

/**
 * ofa_dossier_delete_run:
 * @main: the main window of the application.
 *
 * Run the selection dialog to delete a dossier
 */
void
ofa_dossier_delete_run( ofaMainWindow *main_window, const gchar *label,
		const gchar *provider,
		const gchar *host, gint port, const gchar *socket, const gchar *account, const gchar *password,
		const gchar *dbname,
		const gchar *dbaccount )
{
	static const gchar *thisfn = "ofa_dossier_delete_run";
	ofaDossierDelete *self;
	ofaDossierDeletePrivate *priv;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p, label=%s, provider=%s, host=%s, port=%u, socket=%s, account=%s, password=%s, dbname=%s, admin_account=%s",
			thisfn, main_window, label, provider, host, port, socket, account, password, dbname, dbaccount );

	self = g_object_new(
				OFA_TYPE_DOSSIER_DELETE,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	priv = self->private;

	priv->label = label;
	priv->provider = provider;
	priv->host = host;
	priv->port = port;
	priv->socket = socket;
	priv->account = account;
	priv->password = password;
	priv->dbname = dbname;
	priv->admin_account = dbaccount;

	my_dialog_run_dialog( MY_DIALOG( self ));

	g_object_unref( self );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaDossierDeletePrivate *priv;
	GtkContainer *container;
	GtkWidget *label, *radio, *check;
	gint ivalue;
	gboolean bvalue;
	gchar *msg;

	container = ( GtkContainer * ) my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	priv = OFA_DOSSIER_DELETE( dialog )->private;

	label = my_utils_container_get_child_by_name( container, "p1-msg" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	msg = g_strdup_printf( _( "About to delete the '%s' dossier :" ), priv->label );
	gtk_label_set_text( GTK_LABEL( label ), msg );
	g_free( msg );

	radio = my_utils_container_get_child_by_name( container, "p2-db-drop" );
	g_return_if_fail( radio && GTK_IS_RADIO_BUTTON( radio ));
	g_signal_connect( G_OBJECT( radio ), "toggled", G_CALLBACK( on_db_mode_toggled ), dialog );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( radio ), FALSE );
	priv->p2_db_drop = GTK_RADIO_BUTTON( radio );

	radio = my_utils_container_get_child_by_name( container, "p2-db-leave" );
	g_return_if_fail( radio && GTK_IS_RADIO_BUTTON( radio ));
	g_signal_connect( G_OBJECT( radio ), "toggled", G_CALLBACK( on_db_mode_toggled ), dialog );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( radio ), FALSE );
	priv->p2_db_leave = GTK_RADIO_BUTTON( radio );

	ivalue = ofa_settings_get_uint( "DossierDeleteDlg-dbmode" );
	if( ivalue == DB_DROP ){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->p2_db_drop ), TRUE );
	} else if( ivalue == DB_LEAVE_AS_IS ){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->p2_db_leave ), TRUE );
	}

	check = my_utils_container_get_child_by_name( container, "p3-account-drop" );
	g_return_if_fail( check && GTK_IS_CHECK_BUTTON( check ));
	g_signal_connect( G_OBJECT( check ), "toggled", G_CALLBACK( on_account_toggled ), dialog );

	bvalue = ofa_settings_get_boolean( "DossierDeleteDlg-drop_account" );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check ), !bvalue );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check ), bvalue );

	check = my_utils_container_get_child_by_name( container, "p1-keep-asking" );
	g_return_if_fail( check && GTK_IS_CHECK_BUTTON( check ));
	g_signal_connect( G_OBJECT( check ), "toggled", G_CALLBACK( on_keep_asking_toggled ), dialog );

	bvalue = ofa_settings_get_boolean( "DossierDeleteDlg-keep_asking" );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check ), !bvalue );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check ), bvalue );
}

static void
on_db_mode_toggled( GtkToggleButton *btn, ofaDossierDelete *self )
{
	ofaDossierDeletePrivate *priv;
	gboolean is_active;

	priv = self->private;
	is_active = gtk_toggle_button_get_active( btn );
	priv->db_mode = 0;

	if( is_active ){
		if( btn == ( GtkToggleButton * ) priv->p2_db_drop ){
			priv->db_mode = DB_DROP;
		} else if( btn == ( GtkToggleButton * ) priv->p2_db_leave ){
			priv->db_mode = DB_LEAVE_AS_IS;
		}
	}
}

static void
on_account_toggled( GtkToggleButton *btn, ofaDossierDelete *self )
{
	self->private->drop_account = gtk_toggle_button_get_active( btn );
}

static void
on_keep_asking_toggled( GtkToggleButton *btn, ofaDossierDelete *self )
{
	self->private->keep_asking = gtk_toggle_button_get_active( btn );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	ofaDossierDeletePrivate *priv;

	if( do_delete_dossier( OFA_DOSSIER_DELETE( dialog ))){

		priv = OFA_DOSSIER_DELETE( dialog )->private;

		ofa_settings_set_uint( "DossierDeleteDlg-dbmode", priv->db_mode );
		ofa_settings_set_boolean( "DossierDeleteDlg-drop_account", priv->drop_account );
		ofa_settings_set_boolean( "DossierDeleteDlg-keep_asking", priv->keep_asking );
	}

	return( TRUE );
}

/*
 * is called when the user click on the 'Open' button
 * return %TRUE if we can open a connection, %FALSE else
 */
static gboolean
do_delete_dossier( ofaDossierDelete *self )
{
	ofaDossierDeletePrivate *priv;

	priv = self->private;

	ofa_settings_remove_dossier( priv->label );

	if( priv->db_mode == DB_DROP ){
		drop_database( self );
	}

	if( priv->drop_account ){
		drop_user( self );
	}

	return( TRUE );
}

static void
drop_database( ofaDossierDelete *self )
{
	static const gchar *thisfn = "ofa_dossier_delete_drop_database";
	ofaDossierDeletePrivate *priv;
	ofoSgbd *sgbd;
	gchar *query;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = self->private;
	sgbd = ofo_sgbd_new( priv->provider );

	if( ofo_sgbd_connect( sgbd,
			priv->host,
			priv->port,
			priv->socket,
			"mysql",
			priv->account,
			priv->password )){

		query = g_strdup_printf( "DROP DATABASE %s", priv->dbname );
		ofo_sgbd_query_ignore( sgbd, query );
		g_free( query );
		g_object_unref( sgbd );
	}
}

static void
drop_user( ofaDossierDelete *self )
{
	static const gchar *thisfn = "ofa_dossier_delete_drop_user";
	ofaDossierDeletePrivate *priv;
	ofoSgbd *sgbd;
	gchar *query;
	gchar *hostname;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = self->private;
	sgbd = ofo_sgbd_new( priv->provider );

	if( ofo_sgbd_connect( sgbd,
			priv->host,
			priv->port,
			priv->socket,
			"mysql",
			priv->account,
			priv->password )){

		hostname = g_strdup( priv->host );
		if( !hostname || !g_utf8_strlen( hostname, -1 )){
			g_free( hostname );
			hostname = g_strdup( "localhost" );
		}

		query = g_strdup_printf( "delete from mysql.user where user='%s'", priv->admin_account );
		ofo_sgbd_query_ignore( sgbd, query );
		g_free( query );

		query = g_strdup_printf( "drop user '%s'@'%s'", priv->admin_account, hostname );
		ofo_sgbd_query_ignore( sgbd, query );
		g_free( query );

		ofo_sgbd_query_ignore( sgbd, "flush privileges" );

		g_object_unref( sgbd );
	}
}
