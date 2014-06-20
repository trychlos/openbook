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

#include "api/ofo-account.h"

#include "core/my-utils.h"

#include "ui/my-window-prot.h"
#include "ui/ofa-account-notebook.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaAccountSelectPrivate {

	/* runtime
	 */
	ofaAccountNotebook *child;

	/* returned value
	 */
	gchar              *account_number;
};

static const gchar      *st_ui_xml = PKGUIDIR "/ofa-account-select.ui";
static const gchar      *st_ui_id  = "AccountSelectDlg";

static ofaAccountSelect *st_this   = NULL;

G_DEFINE_TYPE( ofaAccountSelect, ofa_account_select, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_account_activated( const gchar *number, ofaAccountSelect *self );
static void      check_for_enable_dlg( ofaAccountSelect *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_select( ofaAccountSelect *self );

static void
account_select_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_select_finalize";
	ofaAccountSelectPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_SELECT( instance ));

	priv = OFA_ACCOUNT_SELECT( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	g_free( priv->account_number );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_select_parent_class )->finalize( instance );
}

static void
account_select_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_ACCOUNT_SELECT( instance ));

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

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

	self->private = g_new0( ofaAccountSelectPrivate, 1 );
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
}

static void
on_main_window_finalized( gpointer is_null, gpointer this_was_the_dialog )
{
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
ofa_account_select_run( ofaMainWindow *main_window, const gchar *asked_number )
{
	static const gchar *thisfn = "ofa_account_select_run";

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), NULL );

	g_debug( "%s: main_window=%p, asked_number=%s",
			thisfn, ( void * ) main_window, asked_number );

	if( !st_this ){

		st_this = g_object_new(
				OFA_TYPE_ACCOUNT_SELECT,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

		my_dialog_init_dialog( MY_DIALOG( st_this ));

		/* setup a weak reference on the main window to auto-unref */
		g_object_weak_ref( G_OBJECT( main_window ), ( GWeakNotify ) on_main_window_finalized, NULL );
	}

	g_free( st_this->private->account_number );
	st_this->private->account_number = NULL;

	ofa_account_notebook_set_selected( st_this->private->child, asked_number );
	check_for_enable_dlg( st_this );

	my_dialog_run_dialog( MY_DIALOG( st_this ));

	gtk_widget_hide( GTK_WIDGET( my_window_get_toplevel( MY_WINDOW( st_this ))));

	return( g_strdup( st_this->private->account_number ));
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaAccountSelectPrivate *priv;
	GtkWidget *box;
	ofaAccountNotebookParms parms;

	priv = OFA_ACCOUNT_SELECT( dialog )->private;

	box = my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( st_this ))),
					"top-box" );
	g_return_if_fail( box && GTK_IS_BOX( box ));

	parms.main_window = MY_WINDOW( dialog )->protected->main_window;
	parms.parent = GTK_CONTAINER( box );
	parms.pfnSelected = NULL;
	parms.pfnActivated = ( ofaAccountNotebookCb ) on_account_activated;
	parms.pfnViewEntries = NULL;
	parms.user_data = dialog;

	priv->child = ofa_account_notebook_new( &parms );

	ofa_account_notebook_init_view( st_this->private->child, NULL );
}

static void
on_account_activated( const gchar *number, ofaAccountSelect *self )
{
	gtk_dialog_response(
			GTK_DIALOG( my_window_get_toplevel( MY_WINDOW( self ))),
			GTK_RESPONSE_OK );
}

static void
check_for_enable_dlg( ofaAccountSelect *self )
{
	ofoAccount *account;
	GtkWidget *btn;

	account = ofa_account_notebook_get_selected( self->private->child );

	btn = my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( st_this ))),
					"btn-ok" );

	gtk_widget_set_sensitive( btn, account && OFO_IS_ACCOUNT( account ));
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_select( OFA_ACCOUNT_SELECT( dialog )));
}

static gboolean
do_select( ofaAccountSelect *self )
{
	ofoAccount *account;

	account = ofa_account_notebook_get_selected( self->private->child );
	if( account ){
		self->private->account_number = g_strdup( ofo_account_get_number( account ));
	}

	return( TRUE );
}
