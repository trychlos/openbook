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

#include "api/my-utils.h"
#include "api/ofo-account.h"

#include "core/my-window-prot.h"

#include "ui/ofa-account-select.h"
#include "ui/ofa-accounts-frame.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaAccountSelectPrivate {

	/* UI
	 */
	ofaAccountsFrame *accounts_frame;

	GtkWidget        *ok_btn;

	/* returned value
	 */
	gchar           *account_number;
};

static const gchar      *st_ui_xml      = PKGUIDIR "/ofa-account-select.ui";
static const gchar      *st_ui_id       = "AccountSelectDlg";

static ofaAccountSelect *st_this        = NULL;
static GtkWindow        *st_toplevel    = NULL;

G_DEFINE_TYPE( ofaAccountSelect, ofa_account_select, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_account_changed( ofaAccountsFrame *piece, const gchar *number, ofaAccountSelect *self );
static void      on_account_activated( ofaAccountsFrame *piece, const gchar *number, ofaAccountSelect *self );
static void      check_for_enable_dlg( ofaAccountSelect *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_select( ofaAccountSelect *self );

static void
account_select_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_select_finalize";
	ofaAccountSelectPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_SELECT( instance ));

	/* free data members here */
	priv = OFA_ACCOUNT_SELECT( instance )->priv;
	g_free( priv->account_number );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_select_parent_class )->finalize( instance );
}

static void
account_select_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_ACCOUNT_SELECT( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_select_parent_class )->dispose( instance );
}

static void
ofa_account_select_init( ofaAccountSelect *self )
{
	static const gchar *thisfn = "ofa_account_select_init";

	g_return_if_fail( self && OFA_IS_ACCOUNT_SELECT( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_ACCOUNT_SELECT, ofaAccountSelectPrivate );
}

static void
ofa_account_select_class_init( ofaAccountSelectClass *klass )
{
	static const gchar *thisfn = "ofa_account_select_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_select_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_select_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaAccountSelectPrivate ));
}

static void
on_dossier_finalized( gpointer is_null, gpointer finalized_dossier )
{
	static const gchar *thisfn = "ofa_account_select_on_dossier_finalized";

	g_debug( "%s: empty=%p, finalized_dossier=%p",
			thisfn, ( void * ) is_null, ( void * ) finalized_dossier );

	g_return_if_fail( st_this && OFA_IS_ACCOUNT_SELECT( st_this ));

	g_clear_object( &st_this );
}

/**
 * ofa_account_select_run:
 *
 * Returns the selected account number, as a newly allocated string
 * that must be g_free() by the caller
 */
gchar *
ofa_account_select_run( const ofaMainWindow *main_window, const gchar *asked_number )
{
	static const gchar *thisfn = "ofa_account_select_run";
	ofaAccountSelectPrivate *priv;
	ofoDossier *dossier;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), NULL );

	g_debug( "%s: main_window=%p, asked_number=%s",
			thisfn, ( void * ) main_window, asked_number );

	if( !st_this ){
		dossier = ofa_main_window_get_dossier( main_window );
		st_this = g_object_new(
				OFA_TYPE_ACCOUNT_SELECT,
				MY_PROP_MAIN_WINDOW,   main_window,
				MY_PROP_DOSSIER,       dossier,
				MY_PROP_WINDOW_XML,    st_ui_xml,
				MY_PROP_WINDOW_NAME,   st_ui_id,
				MY_PROP_SIZE_POSITION, FALSE,
				NULL );

		st_toplevel = my_window_get_toplevel( MY_WINDOW( st_this ));
		my_utils_window_restore_position( st_toplevel, st_ui_id );
		my_dialog_init_dialog( MY_DIALOG( st_this ));

		/* setup a weak reference on the dossier to auto-unref */
		g_object_weak_ref( G_OBJECT( dossier ), ( GWeakNotify ) on_dossier_finalized, NULL );
	}

	priv = st_this->priv;

	g_free( priv->account_number );
	priv->account_number = NULL;

	ofa_accounts_frame_set_selected( priv->accounts_frame, asked_number );
	check_for_enable_dlg( st_this );

	my_dialog_run_dialog( MY_DIALOG( st_this ));

	my_utils_window_save_position( st_toplevel, st_ui_id );
	gtk_widget_hide( GTK_WIDGET( st_toplevel ));

	return( g_strdup( priv->account_number ));
}

static void
v_init_dialog( myDialog *dialog )
{
	static const gchar *thisfn = "ofa_account_select_v_init_dialog";
	ofaAccountSelectPrivate *priv;
	GtkContainer *container;
	GtkWidget *parent;

	g_debug( "%s: dialog=%p", thisfn, ( void * ) dialog );

	priv = OFA_ACCOUNT_SELECT( dialog )->priv;

	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( st_this )));

	priv->ok_btn = my_utils_container_get_child_by_name( container, "btn-ok" );

	parent = my_utils_container_get_child_by_name( container, "piece-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->accounts_frame = ofa_accounts_frame_new();
	ofa_accounts_frame_attach_to( priv->accounts_frame, GTK_CONTAINER( parent ));
	ofa_accounts_frame_set_main_window( priv->accounts_frame, MY_WINDOW( dialog )->prot->main_window );
	ofa_accounts_frame_set_buttons( priv->accounts_frame, FALSE );

	g_signal_connect(
			G_OBJECT( priv->accounts_frame ), "changed", G_CALLBACK( on_account_changed ), dialog );
	g_signal_connect(
			G_OBJECT( priv->accounts_frame ), "activated", G_CALLBACK( on_account_activated ), dialog );
}

static void
on_account_changed( ofaAccountsFrame *piece, const gchar *number, ofaAccountSelect *self )
{
	check_for_enable_dlg( self );
}

static void
on_account_activated( ofaAccountsFrame *piece, const gchar *number, ofaAccountSelect *self )
{
	gtk_dialog_response(
			GTK_DIALOG( my_window_get_toplevel( MY_WINDOW( self ))),
			GTK_RESPONSE_OK );
}

static void
check_for_enable_dlg( ofaAccountSelect *self )
{
	ofaAccountSelectPrivate *priv;
	gchar *account;
	gboolean ok;

	priv = self->priv;

	account = ofa_accounts_frame_get_selected( priv->accounts_frame );
	ok = account && g_utf8_strlen( account, -1 );
	g_free( account );

	gtk_widget_set_sensitive( priv->ok_btn, ok );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_select( OFA_ACCOUNT_SELECT( dialog )));
}

static gboolean
do_select( ofaAccountSelect *self )
{
	ofaAccountSelectPrivate *priv;
	gchar *account;

	priv = self->priv;

	account = ofa_accounts_frame_get_selected( priv->accounts_frame );
	if( account && g_utf8_strlen( account, -1 )){
		priv->account_number = g_strdup( account );
	}
	g_free( account );

	return( TRUE );
}
