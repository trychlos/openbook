/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
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
#include "api/ofa-igetter.h"

#include "ui/ofa-check-integrity.h"
#include "ui/ofa-check-integrity-bin.h"

/* private instance data
 */
typedef struct {
	gboolean              dispose_has_run;

	/* initialization (when running with UI)
	 */
	ofaIGetter           *getter;
	GtkWindow            *parent;

	/* runtime
	 */
	ofaHub               *hub;

	/* UI
	 */
	ofaCheckIntegrityBin *bin;
	GtkWidget            *close_btn;
}
	ofaCheckIntegrityPrivate;

static const gchar  *st_resource_ui     = "/org/trychlos/openbook/ui/ofa-check-integrity.ui";

static void iwindow_iface_init( myIWindowInterface *iface );
static void iwindow_init( myIWindow *instance );
static void idialog_iface_init( myIDialogInterface *iface );
static void idialog_init( myIDialog *instance );
static void on_checks_done( ofaCheckIntegrityBin *bin, gboolean ok, ofaCheckIntegrity *self );

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
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 *
 * Check the DBMS integrity.
 */
void
ofa_check_integrity_run( ofaIGetter *getter, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_check_integrity_run";
	ofaCheckIntegrity *self;
	ofaCheckIntegrityPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p", thisfn, ( void * ) getter, ( void * ) parent );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_CHECK_INTEGRITY, NULL );

	priv = ofa_check_integrity_get_instance_private( self );

	priv->getter = ofa_igetter_get_permanent_getter( getter );
	priv->parent = parent;

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

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_check_integrity_iwindow_init";
	ofaCheckIntegrityPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_check_integrity_get_instance_private( OFA_CHECK_INTEGRITY( instance ));

	my_iwindow_set_parent( instance, priv->parent );

	priv->hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	my_iwindow_set_geometry_settings( instance, ofa_hub_get_user_settings( priv->hub ));
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
	GtkWidget *parent;

	priv = ofa_check_integrity_get_instance_private( OFA_CHECK_INTEGRITY( instance ));

	priv->close_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-close" );
	g_return_if_fail( priv->close_btn && GTK_IS_BUTTON( priv->close_btn ));
	gtk_widget_set_sensitive( priv->close_btn, FALSE );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	/* have the class name as settings key */
	priv->bin = ofa_check_integrity_bin_new( priv->hub, G_OBJECT_TYPE_NAME( instance ));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->bin ));
	gtk_widget_show_all( GTK_WIDGET( parent ));

	g_signal_connect( priv->bin, "ofa-done", G_CALLBACK( on_checks_done ), instance );

	ofa_check_integrity_bin_check( priv->bin );
}

static void
on_checks_done( ofaCheckIntegrityBin *bin, gboolean ok, ofaCheckIntegrity *self )
{
	ofaCheckIntegrityPrivate *priv;

	priv = ofa_check_integrity_get_instance_private( self );

	gtk_widget_set_sensitive( priv->close_btn, TRUE );
	gtk_widget_grab_focus( priv->close_btn );
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

	bin = ofa_check_integrity_bin_new( hub, "ofaCheckIntegrity" );
	ofa_check_integrity_bin_set_display( bin, FALSE );
	ofa_check_integrity_bin_check( bin );
	ok = ofa_check_integrity_bin_get_status( bin );

	g_debug( "%s: ok=%s", thisfn, ok ? "True":"False" );

	gtk_widget_destroy( GTK_WIDGET( bin ));

	return( ok );
}
