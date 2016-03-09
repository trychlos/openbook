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

#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <stdlib.h>

#include "api/my-idialog.h"
#include "api/my-iwindow.h"
#include "api/my-utils.h"
#include "api/ofa-idbmeta.h"
#include "api/ofo-dossier.h"

#include "core/ofa-dbms-root-bin.h"
#include "core/ofa-main-window.h"

#include "ui/ofa-dossier-new-bin.h"
#include "ui/ofa-dossier-new-mini.h"

/* private instance data
 */
struct _ofaDossierNewMiniPrivate {
	gboolean          dispose_has_run;

	/* UI
	 */
	ofaDossierNewBin *new_bin;
	GtkWidget        *ok_btn;
	GtkWidget        *msg_label;

	/* result
	 */
	ofaIDBMeta       *meta;
};

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-dossier-new-mini.ui";

static void      iwindow_iface_init( myIWindowInterface *iface );
static void      idialog_iface_init( myIDialogInterface *iface );
static void      idialog_init( myIDialog *instance );
static void      on_new_bin_changed( ofaDossierNewBin *bin, const gchar *dname, ofaIDBEditor *editor, ofaDossierNewMini *self );
static void      check_for_enable_dlg( ofaDossierNewMini *self );
static gboolean  is_validable( ofaDossierNewMini *self );
static gboolean  idialog_quit_on_ok( myIDialog *instance );
static void      set_message( ofaDossierNewMini *self, const gchar *message );

G_DEFINE_TYPE_EXTENDED( ofaDossierNewMini, ofa_dossier_new_mini, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaDossierNewMini )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
dossier_new_mini_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_new_mini_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW_MINI( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_new_mini_parent_class )->finalize( instance );
}

static void
dossier_new_mini_dispose( GObject *instance )
{
	ofaDossierNewMiniPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW_MINI( instance ));

	priv = ofa_dossier_new_mini_get_instance_private( OFA_DOSSIER_NEW_MINI( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		g_clear_object( &priv->meta );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_new_mini_parent_class )->dispose( instance );
}

static void
ofa_dossier_new_mini_init( ofaDossierNewMini *self )
{
	static const gchar *thisfn = "ofa_dossier_new_mini_init";
	ofaDossierNewMiniPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_NEW_MINI( self ));

	priv = ofa_dossier_new_mini_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_dossier_new_mini_class_init( ofaDossierNewMiniClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_new_mini_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_new_mini_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_new_mini_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_dossier_new_mini_run:
 * @main_window: the main window of the application.
 * @parent: [allow-none]: the window parent; defaults to the @main_window.
 * @meta: [out]: an #ofaIDBMeta object which defines the newly created
 *  dossier.
 *
 * Returns: %TRUE if a new dossier has been defined in the settings.
 */
gboolean
ofa_dossier_new_mini_run( ofaMainWindow *main_window, GtkWindow *parent, ofaIDBMeta **meta )
{
	static const gchar *thisfn = "ofa_dossier_new_mini_run";
	ofaDossierNewMini *self;
	ofaDossierNewMiniPrivate *priv;
	gboolean dossier_defined;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );
	g_return_val_if_fail( !parent || GTK_IS_WINDOW( parent ), FALSE );
	g_return_val_if_fail( meta, FALSE );

	g_debug( "%s: main_window=%p, parent=%p, meta=%p",
			thisfn, ( void * ) main_window, ( void * ) parent, ( void * ) meta );

	self = g_object_new( OFA_TYPE_DOSSIER_NEW_MINI, NULL );
	my_iwindow_set_main_window( MY_IWINDOW( self ), GTK_APPLICATION_WINDOW( main_window ));
	my_iwindow_set_parent( MY_IWINDOW( self ), parent );

	dossier_defined = FALSE;

	if( my_idialog_run( MY_IDIALOG( self )) == GTK_RESPONSE_OK ){
		priv = ofa_dossier_new_mini_get_instance_private( self );
		if( priv->meta ){
			dossier_defined = TRUE;
			*meta = g_object_ref( priv->meta );
		}
		my_iwindow_close( MY_IWINDOW( self ));
	}

	return( dossier_defined );
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_new_mini_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_new_mini_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
	iface->quit_on_ok = idialog_quit_on_ok;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_dossier_new_mini_idialog_init";
	ofaDossierNewMiniPrivate *priv;
	GtkApplicationWindow *main_window;
	GtkWidget *parent, *label;
	GtkSizeGroup *group;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_new_mini_get_instance_private( OFA_DOSSIER_NEW_MINI( instance ));

	main_window = my_iwindow_get_main_window( MY_IWINDOW( instance ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "new-bin-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->new_bin = ofa_dossier_new_bin_new( OFA_MAIN_WINDOW( main_window ));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->new_bin ));
	group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );
	my_utils_size_group_add_size_group(
			group, ofa_dossier_new_bin_get_size_group( priv->new_bin, 0 ));
	g_object_unref( group );

	g_signal_connect( priv->new_bin, "ofa-changed", G_CALLBACK( on_new_bin_changed ), instance );

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "err-message" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelerror" );
	priv->msg_label = label;

	check_for_enable_dlg( OFA_DOSSIER_NEW_MINI( instance ));
}

static void
on_new_bin_changed( ofaDossierNewBin *bin, const gchar *dname, ofaIDBEditor *editor, ofaDossierNewMini *self )
{
	static const gchar *thisfn = "ofa_dossier_new_mini_on_new_bin_changed";

	g_debug( "%s: bin=%p, editor=%p", thisfn, ( void * ) bin, ( void * ) editor );

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaDossierNewMini *self )
{
	ofaDossierNewMiniPrivate *priv;
	gboolean ok;

	priv = ofa_dossier_new_mini_get_instance_private( self );

	ok = is_validable( self );

	gtk_widget_set_sensitive( priv->ok_btn, ok );
}

static gboolean
is_validable( ofaDossierNewMini *self )
{
	ofaDossierNewMiniPrivate *priv;
	gboolean ok;
	gchar *str;

	priv = ofa_dossier_new_mini_get_instance_private( self );

	ok = ofa_dossier_new_bin_get_valid( priv->new_bin, &str );
	set_message( self, str );

	return( ok );
}

static gboolean
idialog_quit_on_ok( myIDialog *instance )
{
	ofaDossierNewMiniPrivate *priv;

	priv = ofa_dossier_new_mini_get_instance_private( OFA_DOSSIER_NEW_MINI( instance ));

	priv->meta = ofa_dossier_new_bin_apply( priv->new_bin );

	return( TRUE );
}

static void
set_message( ofaDossierNewMini *self, const gchar *message )
{
	ofaDossierNewMiniPrivate *priv;

	priv = ofa_dossier_new_mini_get_instance_private( self );

	if( priv->msg_label ){
		gtk_label_set_text( GTK_LABEL( priv->msg_label ), message );
	}
}
