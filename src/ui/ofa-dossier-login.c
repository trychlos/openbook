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
#include "api/ofa-idbms.h"
#include "api/ofa-settings.h"

#include "core/my-window-prot.h"

#include "ui/ofa-dossier-login.h"

/* private instance data
 */
struct _ofaDossierLoginPrivate {
	const gchar *label;
	GtkWidget   *btn_ok;
	gchar       *account;
	gchar       *password;
	gboolean     ok;
};

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-dossier-login.ui";
static const gchar  *st_ui_id  = "DossierLoginDlg";

G_DEFINE_TYPE( ofaDossierLogin, ofa_dossier_login, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_account_changed( GtkEditable *entry, ofaDossierLogin *self );
static void      on_password_changed( GtkEditable *entry, ofaDossierLogin *self );
static void      check_for_enable_dlg( ofaDossierLogin *self );
static gboolean  is_dialog_validable( ofaDossierLogin *self );
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
 * @main: the main window of the application.
 *
 * Update the properties of an journal
 */
void
ofa_dossier_login_run( const ofaMainWindow *main_window, const gchar *dname, gchar **account, gchar **password )
{
	static const gchar *thisfn = "ofa_dossier_login_run";
	ofaDossierLogin *self;

	g_debug( "%s: main_window=%p, account=%p, password=%p",
				thisfn, ( void * ) main_window, ( void * ) account, ( void * ) password );

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	g_return_if_fail( account && password );

	self = g_object_new(
					OFA_TYPE_DOSSIER_LOGIN,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	self->priv->label = dname;
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
	GtkContainer *container;
	GtkWidget *entry, *label;
	gchar *msg;

	priv = OFA_DOSSIER_LOGIN( dialog )->priv;

	container = ( GtkContainer * ) my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	label = my_utils_container_get_child_by_name( container, "label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	msg = g_strdup_printf(
				_( "In order to connect to '%s' dossier, please enter below "
					"a user account and password." ), priv->label );
	gtk_label_set_text( GTK_LABEL( label ), msg );
	g_free( msg );

	entry = my_utils_container_get_child_by_name( container, "account" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_account_changed ), dialog );
	if( priv->account ){
		gtk_entry_set_text( GTK_ENTRY( entry ), priv->account );
	}

	entry = my_utils_container_get_child_by_name( container, "password" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_password_changed ), dialog );
	if( priv->password ){
		gtk_entry_set_text( GTK_ENTRY( entry ), priv->password );
	}

	check_for_enable_dlg( OFA_DOSSIER_LOGIN( dialog ));
}

static void
on_account_changed( GtkEditable *entry, ofaDossierLogin *self )
{
	g_free( self->priv->account );
	self->priv->account = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	check_for_enable_dlg( self );
}

static void
on_password_changed( GtkEditable *entry, ofaDossierLogin *self )
{
	g_free( self->priv->password );
	self->priv->password = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaDossierLogin *self )
{
	ofaDossierLoginPrivate *priv;
	gboolean ok;

	ok = is_dialog_validable( self );

	priv = self->priv;

	if( !priv->btn_ok ){
		priv->btn_ok = my_utils_container_get_child_by_name(
							GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "btn-ok" );
		g_return_if_fail( priv->btn_ok && GTK_IS_BUTTON( priv->btn_ok ));
	}

	gtk_widget_set_sensitive( self->priv->btn_ok, ok );
}

static gboolean
is_dialog_validable( ofaDossierLogin *self )
{
	ofaDossierLoginPrivate *priv;
	gboolean ok;

	priv = self->priv;

	ok = priv->account && g_utf8_strlen( priv->account, -1 ) &&
			priv->password && g_utf8_strlen( priv->password, -1 );

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
