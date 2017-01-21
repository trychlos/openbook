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
#include "api/ofa-idbexercice-editor.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"
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
	ofaIDBProvider      *provider;
		/* when run as modal */
	gboolean             allow_open;
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
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     on_edit_bin_changed( ofaExerciceEditBin *bin, ofaExerciceNew *self );
static void     check_for_enable_dlg( ofaExerciceNew *self );
static guint    do_create( ofaExerciceNew *self, gchar **msgerr );
//static gboolean create_confirmed( const ofaExerciceNew *self );
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
	priv->allow_open = TRUE;
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
 * @provider: the #ofaIDBProvider to be attached to.
 * @allow_open: whether this dialog should be allowed to open the newly
 *  created dossier (if any).
 * @exercice_meta: [out][allow-none]: a placeholder for the newly created
 *  #ofaIDBExerciceMeta.
 *
 * Run the ExerciceNew as a modal dialog.
 *
 * Returns: %TRUE if an exercice has actually been created, %FALSE on
 * cancel.
 */
gboolean
ofa_exercice_new_run_modal( ofaIGetter *getter, GtkWindow *parent, ofaIDBProvider *provider,
									gboolean allow_open, ofaIDBExerciceMeta **exercice_meta )
{
	static const gchar *thisfn = "ofa_exercice_new_run_modal";
	ofaExerciceNew *self;
	ofaExerciceNewPrivate *priv;
	gboolean exercice_created;

	g_debug( "%s: getter=%p, parent=%p, provider=%p, allow_open=%s, exercice_meta=%p",
			thisfn, ( void * ) getter, ( void * ) parent, ( void * ) provider,
			allow_open ? "True":"False", ( void * ) exercice_meta );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );
	g_return_val_if_fail( !parent || GTK_IS_WINDOW( parent ), FALSE );
	g_return_val_if_fail( provider && OFA_IS_IDBPROVIDER( provider ), FALSE );

	self = g_object_new( OFA_TYPE_EXERCICE_NEW, NULL );

	priv = ofa_exercice_new_get_instance_private( self );

	priv->getter = ofa_igetter_get_permanent_getter( getter );
	priv->parent = parent;
	priv->provider = provider;
	priv->allow_open = allow_open;
	priv->exercice_meta = exercice_meta;

	my_idialog_run( MY_IDIALOG( self ));

	exercice_created = priv->exercice_created;

	g_object_unref( self );

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

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_exercice_new_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
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
	priv->edit_bin = ofa_exercice_edit_bin_new( priv->hub, priv->settings_prefix, HUB_RULE_EXERCICE_NEW, priv->allow_open );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->edit_bin ));
	ofa_exercice_edit_bin_set_provider( priv->edit_bin, priv->provider );
	g_signal_connect( priv->edit_bin, "ofa-changed", G_CALLBACK( on_edit_bin_changed ), instance );

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	my_idialog_click_to_update_ex( instance, priv->ok_btn, ( myIDialogUpdateExCb ) do_create );

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
 * Create the database
 * and register the new dossier in dossier settings
 */
static guint
do_create( ofaExerciceNew *self, gchar **msgerr )
{
#if 0
	static const gchar *thisfn = "ofa_exercice_new_do_create";
	ofaExerciceNewPrivate *priv;
	gboolean open, apply_actions;
	ofaIDBDossierMeta *dossier_meta;
	ofaIDBExerciceMeta *exercice_meta;
	ofaIDBConnect *connect;
	ofaIDBDossierEditor *dossier_editor;
	gchar *adm_account, *adm_password;
	ofaDossierCollection *collection;
	guint ret;

	g_debug( "%s: self=%p, msgerr=%p", thisfn, ( void * ) self, ( void * ) msgerr );

	priv = ofa_exercice_new_get_instance_private( self );

	ret = IDIALOG_UPDATE_OK;
	open = FALSE;

	/* ask for user confirmation */
	if( !create_confirmed( self )){
		return( IDIALOG_UPDATE_REDO );
	}

	/* register the new dossier in dossier settings */
	if( !ofa_dossier_edit_bin_apply( priv->edit_bin )){
		*msgerr = g_strdup( _( "Unable to register the new dossier in settings" ));
		return( IDIALOG_UPDATE_ERROR );
	}

	/* create the new dossier */
	dossier_meta = ofa_dossier_edit_bin_get_dossier_meta( priv->edit_bin );
	dossier_editor = ofa_dossier_edit_bin_get_dossier_editor( priv->edit_bin );
	connect = ofa_idbdossier_editor_get_valid_connect( dossier_editor, dossier_meta );
	ofa_dossier_edit_bin_get_admin_credentials( priv->edit_bin, &adm_account, &adm_password );

	if( !ofa_idbconnect_period_new( connect, adm_account, adm_password, msgerr )){
		collection = ofa_hub_get_dossier_collection( priv->hub );
		ofa_dossier_collection_delete_period( collection, connect, NULL, TRUE, NULL );
		if( !my_strlen( *msgerr )){
			*msgerr = g_strdup( _( "Unable to create the new dossier" ));
		}
		g_object_unref( dossier_meta );

	/* open the newly created dossier if asked for */
	} else {
		priv->dossier_created = TRUE;
		if( priv->dossier_name ){
			*( priv->dossier_name ) = g_strdup( ofa_idbdossier_meta_get_dossier_name( dossier_meta ));
		}
		open = ofa_dossier_edit_bin_get_open_on_create( priv->edit_bin );
		if( open ){
			exercice_meta = ofa_idbdossier_meta_get_current_period( dossier_meta );
			connect = ofa_idbdossier_meta_new_connect( dossier_meta, exercice_meta );
			if( !ofa_idbconnect_open_with_account( connect, adm_account, adm_password )){
				*msgerr = g_strdup( _( "Unable to connect to newly created dossier" ));
				ret = IDIALOG_UPDATE_ERROR;
				g_object_unref( connect );
			} else {
				apply_actions = ofa_dossier_edit_bin_get_apply_actions( priv->edit_bin );
				if( ofa_hub_dossier_open( priv->hub, priv->parent, connect, apply_actions, FALSE )){
					ofa_hub_dossier_remediate_settings( priv->hub );
				} else {
					*msgerr = g_strdup( _( "Unable to open the newly created dossier" ));
					ret = IDIALOG_UPDATE_ERROR;
					g_object_unref( connect );
				}
			}
		}
	}

	g_free( adm_password );
	g_free( adm_account );

	return( ret );
#endif
	return( IDIALOG_UPDATE_OK );
}

#if 0
static gboolean
create_confirmed( const ofaExerciceNew *self )
{
	gboolean ok;
	gchar *str;

	str = g_strdup_printf(
				_( "The create operation will drop and fully reset the target database.\n"
					"This may not be what you actually want !\n"
					"Are you sure you want to create into this database ?" ));

	ok = my_utils_dialog_question( str, _( "C_reate" ));

	g_free( str );

	return( ok );
}
#endif

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
