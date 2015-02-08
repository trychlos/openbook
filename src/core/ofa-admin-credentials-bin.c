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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "api/my-utils.h"

#include "core/ofa-admin-credentials-bin.h"

/* private instance data
 */
struct _ofaAdminCredentialsBinPrivate {
	gboolean      dispose_has_run;

	/* UI
	 */
	GtkWidget    *msg_label;

	/* runtime data
	 */
	gchar        *account;
	gchar        *password;
	gchar        *bis;
	gboolean      ok;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_bin_xml          = PKGUIDIR "/ofa-admin-credentials-bin.ui";
static const gchar *st_bin_id           = "AdminCredentialsBin";

G_DEFINE_TYPE( ofaAdminCredentialsBin, ofa_admin_credentials_bin, GTK_TYPE_BIN )

static void setup_dialog( ofaAdminCredentialsBin *bin );
static void on_account_changed( GtkEditable *entry, ofaAdminCredentialsBin *self );
static void on_password_changed( GtkEditable *entry, ofaAdminCredentialsBin *self );
static void on_bis_changed( GtkEditable *entry, ofaAdminCredentialsBin *self );
static void check_for_enable_dlg( ofaAdminCredentialsBin *self );

static void
admin_credentials_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_admin_credentials_bin_finalize";
	ofaAdminCredentialsBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ADMIN_CREDENTIALS_BIN( instance ));

	/* free data members here */
	priv = OFA_ADMIN_CREDENTIALS_BIN( instance )->priv;

	g_free( priv->account );
	g_free( priv->password );
	g_free( priv->bis );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_admin_credentials_bin_parent_class )->finalize( instance );
}

static void
admin_credentials_bin_dispose( GObject *instance )
{
	ofaAdminCredentialsBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ADMIN_CREDENTIALS_BIN( instance ));

	priv = OFA_ADMIN_CREDENTIALS_BIN( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_admin_credentials_bin_parent_class )->dispose( instance );
}

static void
ofa_admin_credentials_bin_init( ofaAdminCredentialsBin *self )
{
	static const gchar *thisfn = "ofa_admin_credentials_bin_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ADMIN_CREDENTIALS_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
							self, OFA_TYPE_ADMIN_CREDENTIALS_BIN, ofaAdminCredentialsBinPrivate );
}

static void
ofa_admin_credentials_bin_class_init( ofaAdminCredentialsBinClass *klass )
{
	static const gchar *thisfn = "ofa_admin_credentials_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = admin_credentials_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = admin_credentials_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaAdminCredentialsBinPrivate ));

	/**
	 * ofaAdminCredentialsBin::changed:
	 *
	 * This signal is sent on the #ofaAdminCredentialsBin when the
	 * account or the password are changed.
	 *
	 * Arguments are account and password.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAdminCredentialsBin *bin,
	 * 						const gchar    *account,
	 * 						const gchar    *password,
	 * 						gpointer        user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_ADMIN_CREDENTIALS_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_STRING, G_TYPE_STRING );
}

/**
 * ofa_admin_credentials_bin_new:
 */
ofaAdminCredentialsBin *
ofa_admin_credentials_bin_new( void )
{
	ofaAdminCredentialsBin *bin;

	bin = g_object_new( OFA_TYPE_ADMIN_CREDENTIALS_BIN, NULL );

	setup_dialog( bin );

	return( bin );
}

static void
setup_dialog( ofaAdminCredentialsBin *bin )
{
	ofaAdminCredentialsBinPrivate *priv;
	GtkWidget *window, *top_widget, *entry;

	priv =bin->priv;

	window = my_utils_builder_load_from_path( st_bin_xml, st_bin_id );
	g_return_if_fail( window && GTK_IS_CONTAINER( window ));

	top_widget = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "adm-top" );
	g_return_if_fail( top_widget && GTK_IS_CONTAINER( top_widget ));

	gtk_widget_reparent( top_widget, GTK_WIDGET( bin ));

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "adm-account" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect(
			G_OBJECT( entry ), "changed", G_CALLBACK( on_account_changed ), bin );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "adm-password" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_password_changed ), bin );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "adm-bis" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_bis_changed ), bin );

	priv->msg_label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "adm-msg" );
	g_return_if_fail( priv->msg_label && GTK_IS_LABEL( priv->msg_label ));

	check_for_enable_dlg( bin );

	gtk_widget_show_all( GTK_WIDGET( bin ));
}

static void
on_account_changed( GtkEditable *entry, ofaAdminCredentialsBin *self )
{
	ofaAdminCredentialsBinPrivate *priv;

	priv = self->priv;

	g_free( priv->account );
	priv->account = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	check_for_enable_dlg( self );
}

static void
on_password_changed( GtkEditable *entry, ofaAdminCredentialsBin *self )
{
	ofaAdminCredentialsBinPrivate *priv;

	priv = self->priv;

	g_free( priv->password );
	priv->password = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	check_for_enable_dlg( self );
}

static void
on_bis_changed( GtkEditable *entry, ofaAdminCredentialsBin *self )
{
	ofaAdminCredentialsBinPrivate *priv;

	priv = self->priv;

	g_free( priv->bis );
	priv->bis = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	check_for_enable_dlg( self );
}

/*
 * test the DBMS root connection by trying to connect with empty dbname
 */
static void
check_for_enable_dlg( ofaAdminCredentialsBin *self )
{
	ofaAdminCredentialsBinPrivate *priv;
	GdkRGBA color;

	priv = self->priv;
	priv->ok = FALSE;

	if( priv->msg_label ){
		gtk_label_set_text( GTK_LABEL( priv->msg_label ), "" );

		priv->ok = priv->account && g_utf8_strlen( priv->account, -1 ) &&
					priv->password && g_utf8_strlen( priv->password, -1 ) &&
					priv->bis && g_utf8_strlen( priv->bis, -1 ) &&
					!g_utf8_collate( priv->password, priv->bis );

		if( !priv->ok ){
			gtk_label_set_text( GTK_LABEL( priv->msg_label ),
					_( "Dossier administrative credentials are not valid" ));
			gdk_rgba_parse( &color, "#ff0000" );
			gtk_widget_override_color(
					GTK_WIDGET( priv->msg_label ), GTK_STATE_FLAG_NORMAL, &color );
		}
	}

	g_signal_emit_by_name( self, "changed", priv->account, priv->password );
}

/**
 * ofa_admin_credentials_bin_is_valid:
 */
gboolean
ofa_admin_credentials_bin_is_valid( const ofaAdminCredentialsBin *bin )
{
	ofaAdminCredentialsBinPrivate *priv;

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		return( priv->ok );
	}

	return( FALSE );
}
