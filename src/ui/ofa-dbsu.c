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

#include <glib/gi18n.h>

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-idbprovider.h"
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
	gchar           *settings_prefix;
	ofaIDBProvider  *provider;
	guint            rule;

	/* runtime
	 */
	ofaIDBSuperuser *su_bin;

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
static gchar   *iwindow_get_key_prefix( const myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     on_su_bin_changed( ofaIDBSuperuser *bin, ofaDbsu *self );
static void     check_for_enable_dlg( ofaDbsu *self );
static gboolean idialog_quit_on_ok( myIDialog *instance );
static void     set_message( ofaDbsu *self, const gchar *message );
static void     read_settings( ofaDbsu *self );
static void     write_settings( ofaDbsu *self );

G_DEFINE_TYPE_EXTENDED( ofaDbsu, ofa_dbsu, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaDbsu )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
dbsu_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dbsu_finalize";
	ofaDbsuPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DBSU( instance ));

	/* free data members here */
	priv = ofa_dbsu_get_instance_private( OFA_DBSU( instance ));

	g_free( priv->settings_prefix );

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

		write_settings( OFA_DBSU( instance ));

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
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));

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
 * ofa_dbsu_run:
 * @getter: a #ofaIGetter instance.
 * @parent: the parent window.
 * @settings_prefix: the prefix of the keys in user settings.
 * @provider: the #ofaIDBProvider DBMS provider.
 * @rule: the function of the credentials.
 * @su_bin: [out]: a placeholder for a new reference to a
 *  #ofaIDBSuperuser allocated by the @provider; this reference should
 *  be released by the caller.
 *
 * Run the Dbsu as a modal dialog.
 *
 * Returns: %TRUE if the user has confirmed the dialog, %FALSE else.
 *
 * The returned @su_bin reference is only set if the user has confirmed
 * the dialog, and the @provider needs super-user credentials to do the
 * tasks described by @rule.
 *
 * If the @provider does not need super-user credentials for these tasks,
 * then we return %TRUE without even show the dialog.
 */
gboolean
ofa_dbsu_run( ofaIGetter *getter, GtkWindow *parent, const gchar *settings_prefix,
					ofaIDBProvider *provider, guint rule, ofaIDBSuperuser **su_bin )
{
	static const gchar *thisfn = "ofa_dbsu_run";
	ofaDbsu *self;
	ofaDbsuPrivate *priv;
	gboolean ok;

	g_debug( "%s: getter=%p, parent=%p, settings_prefix=%s, provider=%p, rule=%u, su_bin=%p",
			thisfn, ( void * ) getter, ( void * ) parent, settings_prefix,
			( void * ) provider, rule, ( void * ) su_bin );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );
	g_return_val_if_fail( !parent || GTK_IS_WINDOW( parent ), FALSE );
	g_return_val_if_fail( provider && OFA_IS_IDBPROVIDER( provider ), FALSE );
	g_return_val_if_fail( su_bin, FALSE );

	self = g_object_new( OFA_TYPE_DBSU, NULL );

	priv = ofa_dbsu_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
	priv->provider = provider;
	priv->rule = rule;

	g_free( priv->settings_prefix );
	priv->settings_prefix = g_strdup( settings_prefix );

	ok = FALSE;
	*su_bin = NULL;

	priv->su_bin = ofa_idbprovider_new_superuser_bin( provider, rule );
	if( priv->su_bin ){
		if( my_idialog_run( MY_IDIALOG( self )) == GTK_RESPONSE_OK ){
			ok = TRUE;
			*su_bin = g_object_ref( priv->su_bin );
			gtk_container_remove( GTK_CONTAINER( priv->su_parent ), GTK_WIDGET( priv->su_bin ));
			my_iwindow_close( MY_IWINDOW( self ));
		}
	} else {
		ok = TRUE;
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
	iface->get_key_prefix = iwindow_get_key_prefix;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_dbsu_iwindow_init";
	ofaDbsuPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dbsu_get_instance_private( OFA_DBSU( instance ));

	my_iwindow_set_parent( instance, priv->parent );

	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));
}

static gchar *
iwindow_get_key_prefix( const myIWindow *instance )
{
	ofaDbsuPrivate *priv;

	priv = ofa_dbsu_get_instance_private( OFA_DBSU( instance ));

	return( g_strdup( priv->settings_prefix ));
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
	g_signal_connect( priv->su_bin, "ofa-changed", G_CALLBACK( on_su_bin_changed ), instance );
	priv->su_parent = parent;

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "dn-msg" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_style_add( label, "labelerror" );
	priv->msg_label = label;

	read_settings( OFA_DBSU( instance ));

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

/*
 * settings are: <none>
 */
static void
read_settings( ofaDbsu *self )
{
}

static void
write_settings( ofaDbsu *self )
{
}
