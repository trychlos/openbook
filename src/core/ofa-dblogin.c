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

#include "core/my-window-prot.h"
#include "core/ofa-dblogin.h"

/* private instance data
 */
struct _ofaDBLoginPrivate {

	const gchar *label;
	GtkWidget   *btn_ok;
	gchar       *account;
	gchar       *password;
	gboolean     ok;
};

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-dblogin.ui";
static const gchar  *st_ui_id  = "DBLoginDlg";

G_DEFINE_TYPE( ofaDBLogin, ofa_dblogin, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_account_changed( GtkEditable *entry, ofaDBLogin *self );
static void      on_password_changed( GtkEditable *entry, ofaDBLogin *self );
static void      check_for_enable_dlg( ofaDBLogin *self );
static gboolean  is_dialog_validable( ofaDBLogin *self );
static gboolean  v_quit_on_ok( myDialog *dialog );

static void
dblogin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dblogin_finalize";
	ofaDBLoginPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DBLOGIN( instance ));

	/* free data members here */
	priv = OFA_DBLOGIN( instance )->priv;
	g_free( priv->account );
	g_free( priv->password );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dblogin_parent_class )->finalize( instance );
}

static void
dblogin_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_DBLOGIN( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dblogin_parent_class )->dispose( instance );
}

static void
ofa_dblogin_init( ofaDBLogin *self )
{
	static const gchar *thisfn = "ofa_dblogin_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DBLOGIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_DBLOGIN, ofaDBLoginPrivate );
	self->priv->ok = FALSE;
}

static void
ofa_dblogin_class_init( ofaDBLoginClass *klass )
{
	static const gchar *thisfn = "ofa_dblogin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dblogin_dispose;
	G_OBJECT_CLASS( klass )->finalize = dblogin_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaDBLoginPrivate ));
}

/**
 * ofa_dblogin_run:
 * @main: the main window of the application.
 *
 * Update the properties of an journal
 */
gboolean
ofa_dblogin_run( const gchar *label, gchar **account, gchar **password )
{
	static const gchar *thisfn = "ofa_dblogin_run";
	ofaDBLogin *self;
	gboolean ok;

	g_debug( "%s: account=%p, password=%p",
				thisfn, ( void * ) account, ( void * ) password );

	g_return_val_if_fail( account && password, FALSE );

	self = g_object_new(
					OFA_TYPE_DBLOGIN,
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	self->priv->label = label;

	my_dialog_run_dialog( MY_DIALOG( self ));

	ok = self->priv->ok;
	*account = g_strdup( self->priv->account );
	*password = g_strdup( self->priv->password );

	g_object_unref( self );

	return( ok );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaDBLoginPrivate *priv;
	GtkContainer *container;
	GtkWidget *entry, *label, *alignment;
	gchar *msg;

	priv = OFA_DBLOGIN( dialog )->priv;

	container = ( GtkContainer * ) my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	label = my_utils_container_get_child_by_name( container, "label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	msg = g_strdup_printf(
				_( "In order to conduct administrative tasks on '%s' dossier, "
					"please enter below DBMS administrator account and password." ),
						priv->label );
	gtk_label_set_text( GTK_LABEL( label ), msg );
	g_free( msg );

	alignment = my_utils_container_get_child_by_name( container, "provider-alignment" );
	g_return_if_fail( alignment && GTK_IS_ALIGNMENT( alignment ));
	ofa_idbms_display_connect_infos( alignment, priv->label );

	entry = my_utils_container_get_child_by_name( container, "account" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_account_changed ), dialog );

	entry = my_utils_container_get_child_by_name( container, "password" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_password_changed ), dialog );

	check_for_enable_dlg( OFA_DBLOGIN( dialog ));
}

static void
on_account_changed( GtkEditable *entry, ofaDBLogin *self )
{
	g_free( self->priv->account );
	self->priv->account = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	check_for_enable_dlg( self );
}

static void
on_password_changed( GtkEditable *entry, ofaDBLogin *self )
{
	g_free( self->priv->password );
	self->priv->password = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaDBLogin *self )
{
	ofaDBLoginPrivate *priv;
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
is_dialog_validable( ofaDBLogin *self )
{
	ofaDBLoginPrivate *priv;
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
	ofaDBLoginPrivate *priv;

	priv = OFA_DBLOGIN( dialog )->priv;

	priv->ok = TRUE;

	return( priv->ok );
}
