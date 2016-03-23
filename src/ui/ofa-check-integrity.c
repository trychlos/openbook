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

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-settings.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-check-integrity.h"
#include "ui/ofa-check-integrity-bin.h"

/* private instance data
 */
struct _ofaCheckIntegrityPrivate {
	gboolean              dispose_has_run;

	ofaCheckIntegrityBin *bin;

	/* UI
	 */
	GtkWidget            *close_btn;
};

static const gchar  *st_resource_ui     = "/org/trychlos/openbook/ui/ofa-check-integrity.ui";

static void   iwindow_iface_init( myIWindowInterface *iface );
static void   idialog_iface_init( myIDialogInterface *iface );
static void   idialog_init( myIDialog *instance );
static void   on_checks_done( ofaCheckIntegrityBin *bin, gboolean ok, ofaCheckIntegrity *self );

G_DEFINE_TYPE_EXTENDED( ofaCheckIntegrity, ofa_check_integrity, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaCheckIntegrity )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

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
	ofaCheckIntegrityPrivate *priv;

	g_return_if_fail( instance && OFA_IS_CHECK_INTEGRITY( instance ));

	priv = ofa_check_integrity_get_instance_private( OFA_CHECK_INTEGRITY( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_check_integrity_parent_class )->dispose( instance );
}

static void
ofa_check_integrity_init( ofaCheckIntegrity *self )
{
	static const gchar *thisfn = "ofa_check_integrity_init";
	ofaCheckIntegrityPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_check_integrity_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_check_integrity_class_init( ofaCheckIntegrityClass *klass )
{
	static const gchar *thisfn = "ofa_check_integrity_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = check_integrity_dispose;
	G_OBJECT_CLASS( klass )->finalize = check_integrity_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
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

	self = g_object_new( OFA_TYPE_CHECK_INTEGRITY, NULL );
	my_iwindow_set_main_window( MY_IWINDOW( self ), GTK_APPLICATION_WINDOW( main_window ));
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_check_integrity_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_check_integrity_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

static void
idialog_init( myIDialog *instance )
{
	ofaCheckIntegrityPrivate *priv;
	GtkApplicationWindow *main_window;
	ofaHub *hub;
	GtkWidget *parent;

	priv = ofa_check_integrity_get_instance_private( OFA_CHECK_INTEGRITY( instance ));

	priv->close_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-close" );
	g_return_if_fail( priv->close_btn && GTK_IS_BUTTON( priv->close_btn ));
	gtk_widget_set_sensitive( priv->close_btn, FALSE );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	/* have the class name as settings key */
	priv->bin = ofa_check_integrity_bin_new( G_OBJECT_TYPE_NAME( instance ));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->bin ));
	gtk_widget_show_all( GTK_WIDGET( parent ));

	g_signal_connect( priv->bin, "ofa-done", G_CALLBACK( on_checks_done ), instance );

	main_window = my_iwindow_get_main_window( MY_IWINDOW( instance ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	hub = ofa_main_window_get_hub( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	ofa_check_integrity_bin_set_hub( priv->bin, hub );
}

static void
on_checks_done( ofaCheckIntegrityBin *bin, gboolean ok, ofaCheckIntegrity *self )
{
	ofaCheckIntegrityPrivate *priv;

	priv = ofa_check_integrity_get_instance_private( self );

	gtk_widget_set_sensitive( priv->close_btn, TRUE );
}

/**
 * ofa_check_integrity_check:
 * @hub: the #ofaHub object of the application.
 *
 * Check the DBMS integrity without display.
 *
 * Returns: %TRUE if DBMS is safe, %FALSE else.
 */
gboolean
ofa_check_integrity_check( ofaHub *hub )
{
	static const gchar *thisfn = "ofa_check_integrity_check";
	gboolean ok;
	ofaCheckIntegrityBin *bin;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

	bin = ofa_check_integrity_bin_new( "ofaCheckIntegrity" );

	ofa_check_integrity_bin_set_display( bin, FALSE );
	ofa_check_integrity_bin_set_hub( bin, hub );

	ok = ofa_check_integrity_bin_get_status( bin );

	g_debug( "%s: ok=%s", thisfn, ok ? "True":"False" );

	gtk_widget_destroy( GTK_WIDGET( bin ));

	return( ok );
}
