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
#include <glib/gprintf.h>
#include <stdlib.h>

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-editor.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-igetter.h"

#include "ui/ofa-exercice-edit-bin.h"
#include "ui/ofa-exercice-new.h"

/* private instance data
 */
typedef struct {
	gboolean             dispose_has_run;

	/* initialization
	 */
	ofaIGetter          *getter;
	GtkWindow           *parent;
	ofaIDBDossierMeta   *dossier_meta;
		/* when run as modal */
	gboolean             with_admin;
	gboolean             with_open;
	ofaIDBExerciceMeta **exercice_meta;

	/* runtime
	 */
	gchar               *settings_prefix;
	ofaHub              *hub;
	gboolean             exercice_created;

	/* UI
	 */
	ofaExerciceEditBin  *edit_bin;
	GtkWidget           *ok_btn;
	GtkWidget           *msg_label;
}
	ofaExerciceNewPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-exercice-new.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static gchar   *iwindow_get_key_prefix( const myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     on_edit_bin_changed( ofaExerciceEditBin *bin, ofaExerciceNew *self );
static void     check_for_enable_dlg( ofaExerciceNew *self );
static gboolean idialog_quit_on_ok( myIDialog *instance );
static void     set_message( ofaExerciceNew *self, const gchar *message );
static void     read_settings( ofaExerciceNew *self );
static void     write_settings( ofaExerciceNew *self );

G_DEFINE_TYPE_EXTENDED( ofaExerciceNew, ofa_exercice_new, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaExerciceNew )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
exercice_new_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_exercice_new_finalize";
	ofaExerciceNewPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EXERCICE_NEW( instance ));

	/* free data members here */
	priv = ofa_exercice_new_get_instance_private( OFA_EXERCICE_NEW( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exercice_new_parent_class )->finalize( instance );
}

static void
exercice_new_dispose( GObject *instance )
{
	ofaExerciceNewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_EXERCICE_NEW( instance ));

	priv = ofa_exercice_new_get_instance_private( OFA_EXERCICE_NEW( instance ));

	if( !priv->dispose_has_run ){

		write_settings( OFA_EXERCICE_NEW( instance ));

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exercice_new_parent_class )->dispose( instance );
}

static void
ofa_exercice_new_init( ofaExerciceNew *self )
{
	static const gchar *thisfn = "ofa_exercice_new_init";
	ofaExerciceNewPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXERCICE_NEW( self ));

	priv = ofa_exercice_new_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->with_admin = TRUE;
	priv->with_open = TRUE;
	priv->exercice_created = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_exercice_new_class_init( ofaExerciceNewClass *klass )
{
	static const gchar *thisfn = "ofa_exercice_new_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = exercice_new_dispose;
	G_OBJECT_CLASS( klass )->finalize = exercice_new_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_exercice_new_run_modal:
 * @getter: a #ofaIGetter instance.
 * @parent: the parent window.
 * @settings_prefix: the prefix of the keys in user settings.
 * @dossier_meta: the #ofaIDBDossierMeta to be attached to.
 * @with_admin: whether this dialog must display the admin credentials.
 * @with_open: whether this dialog must display the actions on open.
 * @exercice_meta: [out][allow-none]: a placeholder for the newly created
 *  #ofaIDBExerciceMeta.
 *
 * Run the ExerciceNew as a modal dialog.
 *
 * Returns: %TRUE if an exercice has actually been created, %FALSE on
 * cancel.
 *
 * This modality is typically used from #ofaRestoreAssistant, as the
 * assistant needs the result of this function to be able to continue.
 * in this mode, we do not ask (here) from admin credentials nor actions
 * on open.
 */
gboolean
ofa_exercice_new_run_modal( ofaIGetter *getter, GtkWindow *parent, const gchar *settings_prefix,
								ofaIDBDossierMeta *dossier_meta, gboolean with_admin, gboolean with_open,
								ofaIDBExerciceMeta **exercice_meta )
{
	static const gchar *thisfn = "ofa_exercice_new_run_modal";
	ofaExerciceNew *self;
	ofaExerciceNewPrivate *priv;
	gboolean exercice_created;

	g_debug( "%s: getter=%p, parent=%p, settings_prefix=%s, dossier_meta=%p, with_admin=%s, with_open=%s, exercice_meta=%p",
			thisfn, ( void * ) getter, ( void * ) parent, settings_prefix, ( void * ) dossier_meta,
			with_admin ? "True":"False", with_open ? "True":"False",
			( void * ) exercice_meta );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );
	g_return_val_if_fail( !parent || GTK_IS_WINDOW( parent ), FALSE );
	g_return_val_if_fail( dossier_meta && OFA_IS_IDBDOSSIER_META( dossier_meta ), FALSE );

	self = g_object_new( OFA_TYPE_EXERCICE_NEW, NULL );

	priv = ofa_exercice_new_get_instance_private( self );

	priv->getter = ofa_igetter_get_permanent_getter( getter );
	priv->parent = parent;
	priv->dossier_meta = dossier_meta;
	priv->with_admin = with_admin;
	priv->with_open = with_open;
	priv->exercice_meta = exercice_meta;

	g_free( priv->settings_prefix );
	priv->settings_prefix = g_strdup( settings_prefix );

	exercice_created = FALSE;

	if( my_idialog_run( MY_IDIALOG( self )) == GTK_RESPONSE_OK ){
		g_debug( "%s: exercice_created=%s", thisfn, priv->exercice_created ? "True":"False" );
		exercice_created = priv->exercice_created;
		my_iwindow_close( MY_IWINDOW( self ));
	}

	return( exercice_created );
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_exercice_new_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
	iface->get_key_prefix = iwindow_get_key_prefix;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_exercice_new_iwindow_init";
	ofaExerciceNewPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_exercice_new_get_instance_private( OFA_EXERCICE_NEW( instance ));

	my_iwindow_set_parent( instance, priv->parent );

	priv->hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	my_iwindow_set_geometry_settings( instance, ofa_hub_get_user_settings( priv->hub ));
}

static gchar *
iwindow_get_key_prefix( const myIWindow *instance )
{
	ofaExerciceNewPrivate *priv;

	priv = ofa_exercice_new_get_instance_private( OFA_EXERCICE_NEW( instance ));

	return( g_strdup( priv->settings_prefix ));
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_exercice_new_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
	iface->quit_on_ok = idialog_quit_on_ok;
}

/*
 * the dialog is composed with:
 *
 * - ExerciceEditBin composite widget
 *   which includes ExerciceMeta + provider-specific informations
 *
 * - toggle buttons for actions on opening
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_exercice_new_idialog_init";
	ofaExerciceNewPrivate *priv;
	GtkWidget *parent, *label;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_exercice_new_get_instance_private( OFA_EXERCICE_NEW( instance ));

	/* create the composite widget and attach it to the dialog */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "edit-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->edit_bin = ofa_exercice_edit_bin_new(
			priv->hub, priv->settings_prefix, HUB_RULE_EXERCICE_NEW, priv->with_admin, priv->with_open );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->edit_bin ));
	ofa_exercice_edit_bin_set_dossier_meta( priv->edit_bin, priv->dossier_meta );
	g_signal_connect( priv->edit_bin, "ofa-changed", G_CALLBACK( on_edit_bin_changed ), instance );

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "dn-msg" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_style_add( label, "labelerror" );
	priv->msg_label = label;

	read_settings( OFA_EXERCICE_NEW( instance ));

	check_for_enable_dlg( OFA_EXERCICE_NEW( instance ));
}

static void
on_edit_bin_changed( ofaExerciceEditBin *bin, ofaExerciceNew *self )
{
	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaExerciceNew *self )
{
	ofaExerciceNewPrivate *priv;
	gboolean ok;
	gchar *message;

	priv = ofa_exercice_new_get_instance_private( self );

	message = NULL;
	ok = ofa_exercice_edit_bin_is_valid( priv->edit_bin, &message );
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
	ofaExerciceNewPrivate *priv;
	ofaIDBExerciceMeta *meta;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_exercice_new_get_instance_private( OFA_EXERCICE_NEW( instance ));

	meta = ofa_exercice_edit_bin_apply( priv->edit_bin );

	priv->exercice_created = ( meta != NULL );

	if( priv->exercice_meta ){
		*priv->exercice_meta = meta;
	}

	return( TRUE );
}

static void
set_message( ofaExerciceNew *self, const gchar *message )
{
	ofaExerciceNewPrivate *priv;

	priv = ofa_exercice_new_get_instance_private( self );

	if( priv->msg_label ){
		gtk_label_set_text( GTK_LABEL( priv->msg_label ), my_strlen( message ) ? message : "" );
	}
}

/*
 * settings are: <none>
 */
static void
read_settings( ofaExerciceNew *self )
{
}

static void
write_settings( ofaExerciceNew *self )
{
}
