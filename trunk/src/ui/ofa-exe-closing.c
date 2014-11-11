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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofo-dossier.h"

#include "core/my-window-prot.h"

#include "ui/my-editable-date.h"
#include "ui/ofa-exe-closing.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaExeClosingPrivate {

	gboolean  success;
};

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-exe-closing.ui";
static const gchar  *st_ui_id  = "ExeClosingDlg";

G_DEFINE_TYPE( ofaExeClosing, ofa_exe_closing, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      check_for_enable_dlg( ofaExeClosing *self );
static gboolean  is_dialog_validable( ofaExeClosing *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_close( ofaExeClosing *self );
static void      do_end_close( ofaExeClosing *self );

static void
exe_closing_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_exe_closing_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EXE_CLOSING( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exe_closing_parent_class )->finalize( instance );
}

static void
exe_closing_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_EXE_CLOSING( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exe_closing_parent_class )->dispose( instance );
}

static void
ofa_exe_closing_init( ofaExeClosing *self )
{
	static const gchar *thisfn = "ofa_exe_closing_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXE_CLOSING( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_EXE_CLOSING, ofaExeClosingPrivate );
}

static void
ofa_exe_closing_class_init( ofaExeClosingClass *klass )
{
	static const gchar *thisfn = "ofa_exe_closing_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = exe_closing_dispose;
	G_OBJECT_CLASS( klass )->finalize = exe_closing_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaExeClosingPrivate ));
}

/**
 * ofa_exe_closing_run:
 * @main: the main window of the application.
 *
 * Run an intermediate closing on selected ledgers
 */
gboolean
ofa_exe_closing_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_exe_closing_run";
	ofaExeClosing *self;
	gboolean done;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new(
					OFA_TYPE_EXE_CLOSING,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	my_dialog_run_dialog( MY_DIALOG( self ));

	done = self->priv->success;

	g_object_unref( self );

	return( done );
}

static void
v_init_dialog( myDialog *dialog )
{
	/*ofaExeClosingPrivate *priv;*/
	GtkContainer *container;

	/*priv = OFA_EXE_CLOSING( dialog )->priv;*/

	container = ( GtkContainer * ) my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	check_for_enable_dlg( OFA_EXE_CLOSING( dialog ));
}

static void
check_for_enable_dlg( ofaExeClosing *self )
{
	is_dialog_validable( self );
}

static gboolean
is_dialog_validable( ofaExeClosing *self )
{
	ofaExeClosingPrivate *priv;
	gboolean ok;

	priv = self->priv;

	ok = priv->success;

	return( ok );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_close( OFA_EXE_CLOSING( dialog )));
}

static gboolean
do_close( ofaExeClosing *self )
{
	/*ofaExeClosingPrivate *priv;*/
	gboolean ok;

	/*priv = self->priv;*/

	ok = is_dialog_validable( self );

	do_end_close( self );

	return( ok );
}

static void
do_end_close( ofaExeClosing *self )
{
}
