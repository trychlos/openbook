/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-idbsuperuser.h"
#include "api/ofa-igetter.h"

#include "ui/ofa-dbsu.h"

/* private instance data
 */
typedef struct {
	gboolean         dispose_has_run;

	/* initialization
	 */
	ofaIGetter      *getter;
	GtkWindow       *parent;
	ofaIDBSuperuser *su_bin;

	/* runtime
	 */
	GtkWindow       *actual_parent;

	/* UI
	 */
	GtkWidget       *su_parent;
	GtkWidget       *ok_btn;
	GtkWidget       *msg_label;
}
	ofaDbsuPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-dbsu.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     on_su_bin_changed( ofaIDBSuperuser *bin, ofaDbsu *self );
static void     check_for_enable_dlg( ofaDbsu *self );
static gboolean idialog_quit_on_ok( myIDialog *instance );
static void     set_message( ofaDbsu *self, const gchar *message );

G_DEFINE_TYPE_EXTENDED( ofaDbsu, ofa_dbsu, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaDbsu )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
dbsu_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dbsu_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DBSU( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dbsu_parent_class )->finalize( instance );
}

static void
dbsu_dispose( GObject *instance )
{
	ofaDbsuPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DBSU( instance ));

	priv = ofa_dbsu_get_instance_private( OFA_DBSU( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dbsu_parent_class )->dispose( instance );
}

static void
ofa_dbsu_init( ofaDbsu *self )
{
	static const gchar *thisfn = "ofa_dbsu_init";
	ofaDbsuPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DBSU( self ));

	priv = ofa_dbsu_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_dbsu_class_init( ofaDbsuClass *klass )
{
	static const gchar *thisfn = "ofa_dbsu_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dbsu_dispose;
	G_OBJECT_CLASS( klass )->finalize = dbsu_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_dbsu_run_modal:
 * @getter: a #ofaIGetter instance.
 * @parent: the parent window.
 * @su_bin: a #ofaIDBSuperuser object.
 *
 * Run the Dbsu as a modal dialog.
 *
 * Returns: %TRUE if the user has confirmed the dialog, %FALSE else.
 */
gboolean
ofa_dbsu_run_modal( ofaIGetter *getter, GtkWindow *parent, ofaIDBSuperuser *su_bin )
{
	static const gchar *thisfn = "ofa_dbsu_run_modal";
	ofaDbsu *self;
	ofaDbsuPrivate *priv;
	gboolean ok;

	g_debug( "%s: getter=%p, parent=%p, su_bin=%p",
			thisfn, ( void * ) getter, ( void * ) parent, ( void * ) su_bin );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );
	g_return_val_if_fail( !parent || GTK_IS_WINDOW( parent ), FALSE );
	g_return_val_if_fail( su_bin && OFA_IS_IDBSUPERUSER( su_bin ), FALSE );

	self = g_object_new( OFA_TYPE_DBSU, NULL );

	priv = ofa_dbsu_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
	priv->su_bin = su_bin;

	ok = FALSE;

	if( my_idialog_run( MY_IDIALOG( self )) == GTK_RESPONSE_OK ){
		ok = TRUE;
		gtk_container_remove( GTK_CONTAINER( priv->su_parent ), GTK_WIDGET( priv->su_bin ));
		my_iwindow_close( MY_IWINDOW( self ));
	}

	return( ok );
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_dbsu_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_dbsu_iwindow_init";
	ofaDbsuPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dbsu_get_instance_private( OFA_DBSU( instance ));

	priv->actual_parent = priv->parent ? priv->parent : GTK_WINDOW( ofa_igetter_get_main_window( priv->getter ));
	my_iwindow_set_parent( instance, priv->actual_parent );

	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_dbsu_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
	iface->quit_on_ok = idialog_quit_on_ok;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_dbsu_idialog_init";
	ofaDbsuPrivate *priv;
	GtkWidget *parent, *label;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dbsu_get_instance_private( OFA_DBSU( instance ));

	/*  the composite widget has been previously created */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "edit-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->su_bin ));
	g_signal_connect( priv->su_bin, "my-ibin-changed", G_CALLBACK( on_su_bin_changed ), instance );
	priv->su_parent = parent;

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "dn-msg" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_style_add( label, "labelerror" );
	priv->msg_label = label;

	check_for_enable_dlg( OFA_DBSU( instance ));
}

static void
on_su_bin_changed( ofaIDBSuperuser *bin, ofaDbsu *self )
{
	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaDbsu *self )
{
	ofaDbsuPrivate *priv;
	gboolean ok;
	gchar *message;

	priv = ofa_dbsu_get_instance_private( self );

	message = NULL;
	ok = ofa_idbsuperuser_is_valid( priv->su_bin, &message );
	set_message( self, message );
	g_free( message );

	if( priv->ok_btn ){
		gtk_widget_set_sensitive( priv->ok_btn, ok );
	}
}

/*
 * returns %TRUE if we accept to terminate the dialog
 * (whether a new #ofaIDBExerciceMeta has been actually created or not)
 */
static gboolean
idialog_quit_on_ok( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_exercice_idialog_quit_on_ok";
	ofaDbsuPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dbsu_get_instance_private( OFA_DBSU( instance ));

	return( ofa_idbsuperuser_is_valid( priv->su_bin, NULL ));
}

static void
set_message( ofaDbsu *self, const gchar *message )
{
	ofaDbsuPrivate *priv;

	priv = ofa_dbsu_get_instance_private( self );

	if( priv->msg_label ){
		gtk_label_set_text( GTK_LABEL( priv->msg_label ), my_strlen( message ) ? message : "" );
	}
}
