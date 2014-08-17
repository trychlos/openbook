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

#include "api/my-utils.h"
#include "api/ofa-idbms.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"
#include "api/ofo-sgbd.h"

#include "ui/my-window-prot.h"
#include "ui/ofa-dossier-delete.h"
#include "ui/ofa-dossier-delete-prefs.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaDossierDeletePrivate {

	/* input parameters
	 */
	const gchar           *label;
	const gchar           *provider;
	const gchar           *host;
	const gchar           *dbname;

	/* entered data
	 */
	gchar                 *p2_account;
	gchar                 *p2_password;

	/* output data
	 */
	gboolean               deleted;

	/* UI
	 */
	gboolean               connect_ok;
	GtkLabel              *p2_msg;
	ofaDossierDeletePrefs *prefs;
	GtkWidget             *btn_ok;
};

static const gchar  *st_ui_xml           = PKGUIDIR "/ofa-dossier-delete.ui";
static const gchar  *st_ui_id            = "DossierDeleteDlg";

static const gchar  *st_delete_prefs_xml = PKGUIDIR "/ofa-dossier-delete-prefs.piece.ui";
static const gchar  *st_delete_prefs_ui  = "DossierDeleteWindow";

/* keep the dbserver admin password */
static       gchar  *st_passwd           = NULL;

G_DEFINE_TYPE( ofaDossierDelete, ofa_dossier_delete, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      init_delete_prefs( ofaDossierDelete *self );
static void      on_account_changed( GtkEntry *entry, ofaDossierDelete *self );
static void      on_password_changed( GtkEntry *entry, ofaDossierDelete *self );
static void      check_for_dbserver_connection( ofaDossierDelete *self );
static void      check_for_enable_dlg( ofaDossierDelete *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_delete_dossier( ofaDossierDelete *self );

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
	g_free( priv->p2_account );
	g_free( priv->p2_password );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_delete_parent_class )->finalize( instance );
}

static void
dossier_delete_dispose( GObject *instance )
{
	ofaDossierDeletePrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_DELETE( instance ));

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

		priv = OFA_DOSSIER_DELETE( instance )->private;

		/* unref object members here */
		g_clear_object( &priv->prefs );
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
gboolean
ofa_dossier_delete_run( ofaMainWindow *main_window, const gchar *label,
							const gchar *provider, const gchar *host, const gchar *dbname )
{
	static const gchar *thisfn = "ofa_dossier_delete_run";
	ofaDossierDelete *self;
	ofaDossierDeletePrivate *priv;
	gboolean deleted;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, label=%s, provider=%s, host=%s, dbname=%s",
			thisfn, main_window, label, provider, host, dbname );

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
	priv->dbname = dbname;

	priv->deleted = FALSE;

	my_dialog_run_dialog( MY_DIALOG( self ));

	deleted = priv->deleted;

	g_object_unref( self );

	return( deleted );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaDossierDeletePrivate *priv;
	GtkContainer *container;
	GtkWidget *label, *entry;
	gchar *msg, *svalue;

	container = ( GtkContainer * ) my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	priv = OFA_DOSSIER_DELETE( dialog )->private;
	priv->connect_ok = FALSE;

	label = my_utils_container_get_child_by_name( container, "message" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	msg = g_strdup_printf( _(
			"You are about to delete the '%s' dossier.\n"
			"Please provide below the connection informations "
			"for the DBserver administrative account." ),
				priv->label );
	gtk_label_set_text( GTK_LABEL( label ), msg );
	g_free( msg );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( container ), "p1-provider" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), priv->provider );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( container ), "p1-host" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), priv->host );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( container ), "p1-dbname" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), priv->dbname );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( container ), "p2-account" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_account_changed ), dialog );
	svalue = ofa_settings_get_string( "DBServerLoginDlg-admin-account" );
	if( svalue ){
		gtk_entry_set_text( GTK_ENTRY( entry ), svalue );
		g_free( svalue );
	}

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( container ), "p2-password" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_password_changed ), dialog );
	if( st_passwd ){
		gtk_entry_set_text( GTK_ENTRY( entry ), st_passwd );
	}

	init_delete_prefs( OFA_DOSSIER_DELETE( dialog ));
}

static void
init_delete_prefs( ofaDossierDelete *self )
{
	ofaDossierDeletePrivate *priv;
	GtkContainer *container;
	GtkWidget *window;
	GtkWidget *grid, *parent;

	priv = self->private;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));

	window = my_utils_builder_load_from_path( st_delete_prefs_xml, st_delete_prefs_ui );
	g_return_if_fail( window && GTK_IS_WINDOW( window ));
	grid = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "grid-container" );
	g_return_if_fail( grid && GTK_IS_GRID( grid ));

	parent = my_utils_container_get_child_by_name( container, "alignment3-parent" );
	gtk_widget_reparent( grid, parent );

	priv->prefs = ofa_dossier_delete_prefs_new();
	ofa_dossier_delete_prefs_init_dialog( priv->prefs, container );
}

static void
on_account_changed( GtkEntry *entry, ofaDossierDelete *self )
{
	ofaDossierDeletePrivate *priv;
	const gchar *account;

	priv = self->private;

	account = gtk_entry_get_text( entry );
	g_free( priv->p2_account );
	priv->p2_account = g_strdup( account );

	priv->connect_ok = FALSE;
	check_for_dbserver_connection( self );
	check_for_enable_dlg( self );
}

static void
on_password_changed( GtkEntry *entry, ofaDossierDelete *self )
{
	ofaDossierDeletePrivate *priv;
	const gchar *password;

	priv = self->private;

	password = gtk_entry_get_text( entry );
	g_free( priv->p2_password );
	priv->p2_password = g_strdup( password );

	priv->connect_ok = FALSE;
	check_for_dbserver_connection( self );
	check_for_enable_dlg( self );
}

static void
check_for_dbserver_connection( ofaDossierDelete *self )
{
	ofaDossierDeletePrivate *priv;
	ofoSgbd *sgbd;
	const gchar *msg;
	GdkRGBA color;

	priv = self->private;

	if( !priv->connect_ok ){

		/* test a connexion without the database */
		sgbd = ofo_sgbd_new( priv->label );
		if( ofo_sgbd_connect_ex( sgbd, "mysql", priv->p2_account, priv->p2_password, FALSE )){
			priv->connect_ok = TRUE;
		}
		g_object_unref( sgbd );

		msg = priv->connect_ok ?
				_( "DB server connection is OK" ) : _( "Unable to connect to DB server" );

		if( !priv->p2_msg ){
			priv->p2_msg = ( GtkLabel * )
									my_utils_container_get_child_by_name(
											GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))),
											"p2-msg" );
			g_return_if_fail( priv->p2_msg && GTK_IS_LABEL( priv->p2_msg ));
		}

		gtk_label_set_text( priv->p2_msg, msg );
		if( gdk_rgba_parse( &color, priv->connect_ok ? "#000000" : "#FF0000" )){
			gtk_widget_override_color( GTK_WIDGET( priv->p2_msg ), GTK_STATE_FLAG_NORMAL, &color );
		}
	}
}

static void
check_for_enable_dlg( ofaDossierDelete *self )
{
	ofaDossierDeletePrivate *priv;
	gboolean enabled;

	priv = self->private;
	enabled = priv->connect_ok;

	if( !priv->btn_ok ){
		priv->btn_ok = my_utils_container_get_child_by_name(
										GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))),
										"btn-ok" );
		g_return_if_fail( priv->btn_ok && GTK_IS_BUTTON( priv->btn_ok ));
	}

	gtk_widget_set_sensitive( priv->btn_ok, enabled );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	ofaDossierDeletePrivate *priv;

	priv = OFA_DOSSIER_DELETE( dialog )->private;

	priv->deleted = do_delete_dossier( OFA_DOSSIER_DELETE( dialog ));

	ofa_dossier_delete_prefs_set_settings( priv->prefs );

	return( TRUE );
}

static gboolean
do_delete_dossier( ofaDossierDelete *self )
{
	static const gchar *thisfn = "ofa_dossier_delete_do_delete_dossier";
	ofaDossierDeletePrivate *priv;
	gint db_mode;
	gboolean drop_db, drop_accounts;
	ofaIDbms *module;

	priv = self->private;

	module = ofa_idbms_get_provider_by_name( priv->provider );
	if( !module ){
		return( FALSE );
	}

	db_mode = ofa_dossier_delete_prefs_get_db_mode( priv->prefs );
	g_debug( "%s: db_mode=%u", thisfn, db_mode );
	drop_db = ( db_mode == DBMODE_REINIT );

	drop_accounts = ofa_dossier_delete_prefs_get_account_mode( priv->prefs );

	ofa_idbms_delete_dossier(
			module,
			priv->label, priv->p2_account, priv->p2_password,
			drop_db, drop_accounts, FALSE );

	g_object_unref( module );

	return( TRUE );
}
