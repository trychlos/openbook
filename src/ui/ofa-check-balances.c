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

#include "ui/ofa-check-balances.h"
#include "ui/ofa-check-balances-bin.h"

/* private instance data
 */
struct _ofaCheckBalancesPrivate {
	gboolean             dispose_has_run;

	ofaCheckBalancesBin *bin;

	/* UI
	 */
	GtkWidget           *close_btn;
};

static const gchar  *st_resource_ui     = "/org/trychlos/openbook/ui/ofa-check-balances.ui";

static void  iwindow_iface_init( myIWindowInterface *iface );
static void  idialog_iface_init( myIDialogInterface *iface );
static void  idialog_init( myIDialog *instance );
static void  on_checks_done( ofaCheckBalancesBin *bin, gboolean ok, ofaCheckBalances *self );

G_DEFINE_TYPE_EXTENDED( ofaCheckBalances, ofa_check_balances, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaCheckBalances )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
check_balances_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_check_balances_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CHECK_BALANCES( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_check_balances_parent_class )->finalize( instance );
}

static void
check_balances_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_CHECK_BALANCES( instance ));
	ofaCheckBalancesPrivate *priv;

	priv = ofa_check_balances_get_instance_private( OFA_CHECK_BALANCES( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_check_balances_parent_class )->dispose( instance );
}

static void
ofa_check_balances_init( ofaCheckBalances *self )
{
	static const gchar *thisfn = "ofa_check_balances_init";
	ofaCheckBalancesPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_check_balances_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_check_balances_class_init( ofaCheckBalancesClass *klass )
{
	static const gchar *thisfn = "ofa_check_balances_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = check_balances_dispose;
	G_OBJECT_CLASS( klass )->finalize = check_balances_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_check_balances_run:
 * @main: the main window of the application.
 *
 * Update the properties of an account
 */
void
ofa_check_balances_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_check_balances_run";
	ofaCheckBalances *self;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new( OFA_TYPE_CHECK_BALANCES, NULL );
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
	static const gchar *thisfn = "ofa_check_balances_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_check_balances_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

static void
idialog_init( myIDialog *instance )
{
	ofaCheckBalancesPrivate *priv;
	GtkApplicationWindow *main_window;
	ofaHub *hub;
	GtkWidget *parent;

	priv = ofa_check_balances_get_instance_private( OFA_CHECK_BALANCES( instance ));

	priv->close_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-close" );
	g_return_if_fail( priv->close_btn && GTK_IS_BUTTON( priv->close_btn ));
	gtk_widget_set_sensitive( priv->close_btn, FALSE );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->bin = ofa_check_balances_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->bin ));
	gtk_widget_show_all( GTK_WIDGET( parent ));

	g_signal_connect( priv->bin, "ofa-done", G_CALLBACK( on_checks_done ), instance );

	main_window = my_iwindow_get_main_window( MY_IWINDOW( instance ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	hub = ofa_main_window_get_hub( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	ofa_check_balances_bin_set_hub( priv->bin, hub );
}

static void
on_checks_done( ofaCheckBalancesBin *bin, gboolean ok, ofaCheckBalances *self )
{
	ofaCheckBalancesPrivate *priv;

	priv = ofa_check_balances_get_instance_private( self );

	gtk_widget_set_sensitive( priv->close_btn, TRUE );
}

/**
 * ofa_check_balances_check:
 * @hub: the #ofaHub object of the application.
 *
 * Check the balances without display.
 *
 * Returns: %TRUE if balances are well balanced, %FALSE else.
 */
gboolean
ofa_check_balances_check( ofaHub *hub )
{
	static const gchar *thisfn = "ofa_check_balances_check";
	gboolean ok;
	ofaCheckBalancesBin *bin;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

	bin = ofa_check_balances_bin_new();

	ofa_check_balances_bin_set_display( bin, FALSE );
	ofa_check_balances_bin_set_hub( bin, hub );

	ok = ofa_check_balances_bin_get_status( bin );

	g_debug( "%s: ok=%s", thisfn, ok ? "True":"False" );

	gtk_widget_destroy( GTK_WIDGET( bin ));

	return( ok );
}
