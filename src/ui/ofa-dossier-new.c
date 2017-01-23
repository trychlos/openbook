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
#include "api/ofa-idbdossier-editor.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-igetter.h"

#include "ui/ofa-dossier-edit-bin.h"
#include "ui/ofa-dossier-new.h"

/* private instance data
 */
typedef struct {
	gboolean           dispose_has_run;

	/* initialization
	 */
	ofaIGetter        *getter;
	GtkWindow         *parent;
		/* when run as modal */
	gboolean           allow_open;
	gchar            **dossier_name;

	/* runtime
	 */
	gchar             *settings_prefix;
	ofaHub            *hub;
	gboolean           dossier_created;

	/* UI
	 */
	ofaDossierEditBin *edit_bin;
	GtkWidget         *ok_btn;
	GtkWidget         *msg_label;
}
	ofaDossierNewPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-dossier-new.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     on_edit_bin_changed( ofaDossierEditBin *bin, ofaDossierNew *self );
static void     check_for_enable_dlg( ofaDossierNew *self );
static gboolean idialog_quit_on_ok( myIDialog *instance );
static gboolean create_confirmed( const ofaDossierNew *self );
static void     set_message( ofaDossierNew *self, const gchar *message );
static void     read_settings( ofaDossierNew *self );
static void     write_settings( ofaDossierNew *self );

G_DEFINE_TYPE_EXTENDED( ofaDossierNew, ofa_dossier_new, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaDossierNew )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
dossier_new_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_new_finalize";
	ofaDossierNewPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW( instance ));

	/* free data members here */
	priv = ofa_dossier_new_get_instance_private( OFA_DOSSIER_NEW( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_new_parent_class )->finalize( instance );
}

static void
dossier_new_dispose( GObject *instance )
{
	ofaDossierNewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW( instance ));

	priv = ofa_dossier_new_get_instance_private( OFA_DOSSIER_NEW( instance ));

	if( !priv->dispose_has_run ){

		write_settings( OFA_DOSSIER_NEW( instance ));

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_new_parent_class )->dispose( instance );
}

static void
ofa_dossier_new_init( ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_init";
	ofaDossierNewPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_NEW( self ));

	priv = ofa_dossier_new_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->allow_open = TRUE;
	priv->dossier_created = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_dossier_new_class_init( ofaDossierNewClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_new_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_new_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_new_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_dossier_new_run:
 * @getter: a #ofaIGetter instance.
 * @parent: the parent window.
 *
 * Run the DossierNew non-modal dialog.
 */
void
ofa_dossier_new_run( ofaIGetter *getter, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_dossier_new_run";
	ofaDossierNew *self;
	ofaDossierNewPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p",
			thisfn, ( void * ) getter, ( void * ) parent );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_DOSSIER_NEW, NULL );

	priv = ofa_dossier_new_get_instance_private( self );

	priv->getter = ofa_igetter_get_permanent_getter( getter );
	priv->parent = parent;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/**
 * ofa_dossier_new_run_modal:
 * @getter: a #ofaIGetter instance.
 * @parent: the parent window.
 * @allow_open: whether this dialog should be allowed to open the newly
 *  created dossier (if any).
 * @dossier_name: [out][allow-none]: a placeholder for the name of the
 *  newly created dossier.
 *
 * Run the DossierNew as a modal dialog.
 *
 * Returns: %TRUE if a dossier has actually been created, %FALSE on
 * cancel.
 */
gboolean
ofa_dossier_new_run_modal( ofaIGetter *getter, GtkWindow *parent, gboolean allow_open, gchar **dossier_name )
{
	static const gchar *thisfn = "ofa_dossier_new_run_modal";
	ofaDossierNew *self;
	ofaDossierNewPrivate *priv;
	gboolean dossier_created;

	g_debug( "%s: getter=%p, parent=%p, allow_open=%s, dossier_name=%p",
			thisfn, ( void * ) getter, ( void * ) parent, allow_open ? "True":"False", ( void * ) dossier_name );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );
	g_return_val_if_fail( !parent || GTK_IS_WINDOW( parent ), FALSE );

	self = g_object_new( OFA_TYPE_DOSSIER_NEW, NULL );

	priv = ofa_dossier_new_get_instance_private( self );

	priv->getter = ofa_igetter_get_permanent_getter( getter );
	priv->parent = parent;
	priv->allow_open = allow_open;
	priv->dossier_name = dossier_name;

	dossier_created = FALSE;
	if( priv->dossier_name ){
		*priv->dossier_name = NULL;
	}

	if( my_idialog_run( MY_IDIALOG( self )) == GTK_RESPONSE_OK ){
		g_debug( "%s: dossier_created=%s", thisfn, priv->dossier_created ? "True":"False" );
		dossier_created = priv->dossier_created;
		my_iwindow_close( MY_IWINDOW( self ));
	}

	return( dossier_created );
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_new_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_dossier_new_iwindow_init";
	ofaDossierNewPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_new_get_instance_private( OFA_DOSSIER_NEW( instance ));

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
	static const gchar *thisfn = "ofa_dossier_new_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
	iface->quit_on_ok = idialog_quit_on_ok;
}

/*
 * the dialog is composed with:
 *
 * - DossierEditBin composite widget
 *   which includes dossier name, provider selection, connection
 *   informations and dbms root credentials
 *
 * - toggle buttons for actions on opening
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_dossier_new_idialog_init";
	ofaDossierNewPrivate *priv;
	GtkWidget *parent, *label;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_new_get_instance_private( OFA_DOSSIER_NEW( instance ));

	/* create the composite widget and attach it to the dialog */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "edit-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->edit_bin = ofa_dossier_edit_bin_new( priv->hub, priv->settings_prefix, HUB_RULE_DOSSIER_NEW, priv->allow_open );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->edit_bin ));
	g_signal_connect( priv->edit_bin, "ofa-changed", G_CALLBACK( on_edit_bin_changed ), instance );

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "dn-msg" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_style_add( label, "labelerror" );
	priv->msg_label = label;

	read_settings( OFA_DOSSIER_NEW( instance ));

	check_for_enable_dlg( OFA_DOSSIER_NEW( instance ));
}

static void
on_edit_bin_changed( ofaDossierEditBin *bin, ofaDossierNew *self )
{
	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	gboolean ok;
	gchar *message;

	priv = ofa_dossier_new_get_instance_private( self );

	message = NULL;
	ok = ofa_dossier_edit_bin_is_valid( priv->edit_bin, &message );
	set_message( self, message );
	g_free( message );

	if( priv->ok_btn ){
		gtk_widget_set_sensitive( priv->ok_btn, ok );
	}
}

/*
 * Create the database
 * and register the new dossier in dossier settings
 *
 * Returns %TRUE if we accept to terminate this dialog
 */
static gboolean
idialog_quit_on_ok( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_dossier_new_idialog_quit_on_ok";
	ofaDossierNewPrivate *priv;
	gboolean ret, open, apply_actions;
	ofaIDBDossierMeta *dossier_meta;
	ofaIDBExerciceMeta *exercice_meta;
	ofaIDBConnect *connect;
	ofaIDBDossierEditor *dossier_editor;
	gchar *msgerr, *adm_account, *adm_password;
	ofaDossierCollection *collection;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_new_get_instance_private( OFA_DOSSIER_NEW( instance ));

	ret = FALSE;
	open = FALSE;
	msgerr = NULL;

	/* ask for user confirmation */
	if( !create_confirmed( OFA_DOSSIER_NEW( instance ))){
		return( FALSE );
	}

	/* register the new dossier in dossier settings */
	if( !ofa_dossier_edit_bin_apply( priv->edit_bin )){
		my_utils_msg_dialog( GTK_WINDOW( instance ), GTK_MESSAGE_ERROR,
				_( "Unable to register the new dossier in settings" ));
		return( FALSE );
	}

	/* create the new dossier */
	dossier_meta = ofa_dossier_edit_bin_get_dossier_meta( priv->edit_bin );
	dossier_editor = ofa_dossier_edit_bin_get_dossier_editor( priv->edit_bin );
	connect = ofa_idbdossier_editor_get_valid_connect( dossier_editor, dossier_meta );
	ofa_dossier_edit_bin_get_admin_credentials( priv->edit_bin, &adm_account, &adm_password );

	if( !ofa_idbconnect_period_new( connect, adm_account, adm_password, &msgerr )){
		collection = ofa_hub_get_dossier_collection( priv->hub );
		ofa_dossier_collection_delete_period( collection, connect, NULL, TRUE, NULL );
		if( !my_strlen( msgerr )){
			msgerr = g_strdup( _( "Unable to create the new dossier" ));
		}
		my_utils_msg_dialog( GTK_WINDOW( instance ), GTK_MESSAGE_ERROR, msgerr );
		g_free( msgerr );
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
				my_utils_msg_dialog( GTK_WINDOW( instance ), GTK_MESSAGE_ERROR,
						_( "Unable to connect to newly created dossier" ));
				g_object_unref( connect );
			} else {
				apply_actions = ofa_dossier_edit_bin_get_apply_actions( priv->edit_bin );
				if( ofa_hub_dossier_open( priv->hub, priv->parent, connect, apply_actions, FALSE )){
					ofa_hub_dossier_remediate_settings( priv->hub );
					ret = TRUE;
				} else {
					my_utils_msg_dialog( GTK_WINDOW( instance ), GTK_MESSAGE_ERROR,
							_( "Unable to open the newly created dossier" ));
					g_object_unref( connect );
				}
			}
		}
	}

	g_free( adm_password );
	g_free( adm_account );

	return( ret );
}

static gboolean
create_confirmed( const ofaDossierNew *self )
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

static void
set_message( ofaDossierNew *self, const gchar *message )
{
	ofaDossierNewPrivate *priv;

	priv = ofa_dossier_new_get_instance_private( self );

	if( priv->msg_label ){
		gtk_label_set_text( GTK_LABEL( priv->msg_label ), my_strlen( message ) ? message : "" );
	}
}

/*
 * settings are: <none>
 */
static void
read_settings( ofaDossierNew *self )
{
}

static void
write_settings( ofaDossierNew *self )
{
}
