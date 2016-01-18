/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofa-hub.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-check-integrity.h"
#include "ui/ofa-check-integrity-bin.h"

/* private instance data
 */
struct _ofaCheckIntegrityPrivate {

	ofaMainWindow        *main_window;
	ofaCheckIntegrityBin *bin;

	/* UI
	 */
	GtkWidget            *close_btn;
};

static const gchar  *st_ui_xml          = PKGUIDIR "/ofa-check-integrity.ui";
static const gchar  *st_ui_id           = "CheckIntegrityDlg";

G_DEFINE_TYPE( ofaCheckIntegrity, ofa_check_integrity, MY_TYPE_DIALOG )

static void v_init_dialog( myDialog *dialog );
static void on_checks_done( ofaCheckIntegrityBin *bin, gboolean ok, ofaCheckIntegrity *self );

static void
check_integrity_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_check_integrity_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CHECK_INTEGRITY( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_check_integrity_parent_class )->finalize( instance );
}

static void
check_integrity_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_CHECK_INTEGRITY( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_check_integrity_parent_class )->dispose( instance );
}

static void
ofa_check_integrity_init( ofaCheckIntegrity *self )
{
	static const gchar *thisfn = "ofa_check_integrity_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_CHECK_INTEGRITY, ofaCheckIntegrityPrivate );
}

static void
ofa_check_integrity_class_init( ofaCheckIntegrityClass *klass )
{
	static const gchar *thisfn = "ofa_check_integrity_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = check_integrity_dispose;
	G_OBJECT_CLASS( klass )->finalize = check_integrity_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;

	g_type_class_add_private( klass, sizeof( ofaCheckIntegrityPrivate ));
}

/**
 * ofa_check_integrity_run:
 * @main: the main window of the application.
 *
 * Update the properties of an account
 */
void
ofa_check_integrity_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_check_integrity_run";
	ofaCheckIntegrity *self;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new(
				OFA_TYPE_CHECK_INTEGRITY,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	self->priv->main_window = main_window;

	my_dialog_run_dialog( MY_DIALOG( self ));

	g_object_unref( self );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaCheckIntegrityPrivate *priv;
	ofaHub *hub;
	GtkWindow *toplevel;
	GtkWidget *parent;

	priv = OFA_CHECK_INTEGRITY( dialog )->priv;

	hub = ofa_main_window_get_hub( priv->main_window );
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( toplevel && GTK_IS_WINDOW( toplevel ));

	priv->close_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "btn-ok" );
	g_return_if_fail( priv->close_btn && GTK_IS_BUTTON( priv->close_btn ));
	gtk_widget_set_sensitive( priv->close_btn, FALSE );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->bin = ofa_check_integrity_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->bin ));
	gtk_widget_show_all( GTK_WIDGET( parent ));

	g_signal_connect( priv->bin, "ofa-done", G_CALLBACK( on_checks_done ), dialog );

	ofa_check_integrity_bin_set_hub( priv->bin, hub );

	g_debug( "ofa_check_integrity_v_init_dialog: returning..." );
}

static void
on_checks_done( ofaCheckIntegrityBin *bin, gboolean ok, ofaCheckIntegrity *self )
{
	ofaCheckIntegrityPrivate *priv;

	priv = self->priv;

	gtk_widget_set_sensitive( priv->close_btn, TRUE );
}
