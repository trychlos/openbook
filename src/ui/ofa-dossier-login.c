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

#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofa-idbms.h"
#include "api/ofa-settings.h"

#include "ui/ofa-dossier-login.h"
#include "ui/ofa-user-credentials-bin.h"

/* private instance data
 */
struct _ofaDossierLoginPrivate {

	/* initialization
	 */
	ofaMainWindow *main_window;
	const gchar   *label;				/* dossier name */
	const gchar   *dbname;				/* database */
	gchar         *account;
	gchar         *password;

	/* returned value */
	gboolean       ok;

	/* UI
	 */
	GtkWidget     *ok_btn;
};

static const gchar  *st_ui_xml          = PKGUIDIR "/ofa-dossier-login.ui";
static const gchar  *st_ui_id           = "DossierLoginDlg";

G_DEFINE_TYPE( ofaDossierLogin, ofa_dossier_login, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_user_credentials_changed( ofaUserCredentialsBin *credentials, const gchar *account, const gchar *password, ofaDossierLogin *bin );
static void      check_for_enable_dlg( ofaDossierLogin *bin );
static gboolean  is_dialog_validable( ofaDossierLogin *bin );
static gboolean  v_quit_on_ok( myDialog *dialog );

static void
dossier_login_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_login_finalize";
	ofaDossierLoginPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_LOGIN( instance ));

	/* free data members here */
	priv = OFA_DOSSIER_LOGIN( instance )->priv;
	g_free( priv->account );
	g_free( priv->password );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_login_parent_class )->finalize( instance );
}

static void
dossier_login_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_DOSSIER_LOGIN( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_login_parent_class )->dispose( instance );
}

static void
ofa_dossier_login_init( ofaDossierLogin *self )
{
	static const gchar *thisfn = "ofa_dossier_login_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_LOGIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_DOSSIER_LOGIN, ofaDossierLoginPrivate );

	self->priv->ok = FALSE;
}

static void
ofa_dossier_login_class_init( ofaDossierLoginClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_login_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_login_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_login_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaDossierLoginPrivate ));
}

/**
 * ofa_dossier_login_run:
 * @main_window: the main window of the application.
 * @dname: the name of the dossier to be opened
 * @dbname: the name of the database to be opened
 * @account: placeholder for the user's account
 * @password: placeholder for the user's password.
 *
 * Get the user credentials to open the specified @dname dossier.
 */
void
ofa_dossier_login_run( ofaMainWindow *main_window,
		const gchar *dname, const gchar *dbname, gchar **account, gchar **password )
{
	static const gchar *thisfn = "ofa_dossier_login_run";
	ofaDossierLogin *self;

	g_debug( "%s: main_window=%p, dname=%s, dbname=%s, account=%p, password=%p",
				thisfn, ( void * ) main_window, dname, dbname, ( void * ) account, ( void * ) password );

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	g_return_if_fail( my_strlen( dname ));
	g_return_if_fail( my_strlen( dbname ));
	g_return_if_fail( account && password );

	self = g_object_new(
					OFA_TYPE_DOSSIER_LOGIN,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	self->priv->main_window = main_window;
	self->priv->label = dname;
	self->priv->dbname = dbname;
	self->priv->account = g_strdup( *account );
	self->priv->password = g_strdup( *password );

	my_dialog_run_dialog( MY_DIALOG( self ));

	if( self->priv->ok ){
		g_free( *account );
		*account = g_strdup( self->priv->account );
		g_free( *password );
		*password = g_strdup( self->priv->password );
	}

	g_object_unref( self );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaDossierLoginPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *parent, *label;
	ofaUserCredentialsBin *user_credentials;
	gchar *msg;

	priv = OFA_DOSSIER_LOGIN( dialog )->priv;

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( toplevel && GTK_IS_WINDOW( toplevel ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	msg = g_strdup_printf(
				_( "In order to connect to '%s' dossier and its '%s' database, "
					"please enter below a user account and password." ),
					priv->label, priv->dbname );
	gtk_label_set_text( GTK_LABEL( label ), msg );
	g_free( msg );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "dl-user-parent" );
	g_return_if_fail( parent && GTK_IS_WIDGET( parent ));
	user_credentials = ofa_user_credentials_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( user_credentials ));
	g_signal_connect( user_credentials, "ofa-changed", G_CALLBACK( on_user_credentials_changed ), dialog );

	if( priv->account ){
		ofa_user_credentials_bin_set_account( user_credentials, priv->account );
	}
	if( priv->password ){
		ofa_user_credentials_bin_set_password( user_credentials, priv->password );
	}

	check_for_enable_dlg( OFA_DOSSIER_LOGIN( dialog ));
}

static void
on_user_credentials_changed( ofaUserCredentialsBin *credentials, const gchar *account, const gchar *password, ofaDossierLogin *bin )
{
	ofaDossierLoginPrivate *priv;

	priv = bin->priv;

	g_free( priv->account );
	priv->account = g_strdup( account );

	g_free( priv->password );
	priv->password = g_strdup( password );

	check_for_enable_dlg( bin );
}

static void
check_for_enable_dlg( ofaDossierLogin *bin )
{
	ofaDossierLoginPrivate *priv;
	gboolean ok;

	ok = is_dialog_validable( bin );

	priv = bin->priv;

	gtk_widget_set_sensitive( priv->ok_btn, ok );
}

static gboolean
is_dialog_validable( ofaDossierLogin *bin )
{
	ofaDossierLoginPrivate *priv;
	gboolean ok;

	priv = bin->priv;

	ok = my_strlen( priv->account ) && my_strlen( priv->password );

	return( ok );
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
v_quit_on_ok( myDialog *dialog )
{
	ofaDossierLoginPrivate *priv;

	priv = OFA_DOSSIER_LOGIN( dialog )->priv;

	priv->ok = TRUE;

	return( priv->ok );
}
