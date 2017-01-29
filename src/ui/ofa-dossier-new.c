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

#include "my/my-ibin.h"
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

#include "ui/ofa-admin-credentials-bin.h"
#include "ui/ofa-dossier-actions-bin.h"
#include "ui/ofa-dossier-edit-bin.h"
#include "ui/ofa-dossier-new.h"
#include "ui/ofa-exercice-edit-bin.h"

/* private instance data
 */
typedef struct {
	gboolean                dispose_has_run;

	/* initialization
	 */
	ofaIGetter             *getter;
	GtkWindow              *parent;
		/* when run as modal
		 * (the caller is waiting for the result) */
	guint                   rule;
	gboolean                with_su;
	gboolean                with_admin;
	gboolean                with_confirm;
	gboolean                with_actions;
	ofaIDBDossierMeta     **dossier_meta;

	/* runtime
	 */
	gchar                  *settings_prefix;
	ofaHub                 *hub;
	gboolean                dossier_created;

	/* UI
	 */
	ofaDossierEditBin      *dossier_bin;
	ofaExerciceEditBin     *exercice_bin;
	ofaAdminCredentialsBin *admin_bin;
	ofaDossierActionsBin   *actions_bin;
	GtkWidget              *ok_btn;
	GtkWidget              *msg_label;
}
	ofaDossierNewPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-dossier-new.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static gchar   *iwindow_get_key_prefix( const myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     on_dossier_bin_changed( ofaDossierEditBin *bin, ofaDossierNew *self );
static void     on_exercice_bin_changed( ofaExerciceEditBin *bin, ofaDossierNew *self );
static void     on_admin_bin_changed( ofaAdminCredentialsBin *bin, const gchar *account, const gchar *password, ofaDossierNew *self );
static void     on_actions_bin_changed( ofaDossierActionsBin *bin, ofaDossierNew *self );
static void     check_for_enable_dlg( ofaDossierNew *self );
static void     on_ok_clicked( ofaDossierNew *self );
static gboolean do_create( ofaDossierNew *self );
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
	priv->rule = HUB_RULE_DOSSIER_NEW;
	priv->with_su = TRUE;
	priv->with_admin = TRUE;
	priv->with_confirm = TRUE;
	priv->with_actions = TRUE;
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
 * @settings_prefix: [allow-none]: the prefix of the key in user settings;
 *  if %NULL, then rely on this class name;
 *  when set, then this class automatically adds its name as a suffix.
 * @with_su: whether this dialog must display the super-user widget.
 * @with_admin: whether this dialog must display the AdminCredentials widget.
 * @with_confirm: whether we request a user confirmation.
 * @with_actions: whether this dialog must display the DossierActions widget.
 * @dossier_meta: [out][allow-none]: a placeholder for the newly created
 *  dossier.
 *
 * Run the DossierNew as a modal dialog.
 *
 * Returns: %TRUE if a dossier has actually been created, %FALSE on
 * cancel.
 */
gboolean
ofa_dossier_new_run_modal( ofaIGetter *getter, GtkWindow *parent, const gchar *settings_prefix, guint rule,
								gboolean with_su, gboolean with_admin, gboolean with_confirm, gboolean with_actions, ofaIDBDossierMeta **dossier_meta )
{
	static const gchar *thisfn = "ofa_dossier_new_run_modal";
	ofaDossierNew *self;
	ofaDossierNewPrivate *priv;
	gboolean dossier_created;
	gchar *str;

	g_debug( "%s: getter=%p, parent=%p, settings_prefix=%s, rule=%u, "
			"	with_su=%s, with_admin=%s, with_confirm=%s, with_actions=%s, dossier_meta=%p",
			thisfn, ( void * ) getter, ( void * ) parent, settings_prefix, rule,
			with_su ? "True":"False", with_admin ? "True":"False", with_confirm ? "True":"False",
			with_actions ? "True":"False", ( void * ) dossier_meta );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );
	g_return_val_if_fail( !parent || GTK_IS_WINDOW( parent ), FALSE );

	self = g_object_new( OFA_TYPE_DOSSIER_NEW, NULL );

	priv = ofa_dossier_new_get_instance_private( self );

	priv->getter = ofa_igetter_get_permanent_getter( getter );
	priv->parent = parent;
	priv->rule = rule;
	priv->with_su = with_su;
	priv->with_admin = with_admin;
	priv->with_confirm = with_confirm;
	priv->with_actions = with_actions;
	priv->dossier_meta = dossier_meta;

	if( my_strlen( settings_prefix )){
		str = priv->settings_prefix;
		priv->settings_prefix = g_strdup_printf( "%s-%s", settings_prefix, str );
		g_free( str );
	}

	dossier_created = FALSE;

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
	iface->get_key_prefix = iwindow_get_key_prefix;
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

static gchar *
iwindow_get_key_prefix( const myIWindow *instance )
{
	ofaDossierNewPrivate *priv;

	priv = ofa_dossier_new_get_instance_private( OFA_DOSSIER_NEW( instance ));

	return( g_strdup( priv->settings_prefix ));
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
	GtkWidget *btn, *parent, *label;
	GtkSizeGroup *group, *group_bin;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_new_get_instance_private( OFA_DOSSIER_NEW( instance ));

	group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	/* we do not know at this time if are going to run as modal or non-modal
	 * so only option is to wait until OK button is clicked */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect_swapped( btn, "clicked", G_CALLBACK( on_ok_clicked ), instance );
	priv->ok_btn = btn;

	/* dossier edition */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "dossier-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->dossier_bin = ofa_dossier_edit_bin_new(
			priv->hub, priv->settings_prefix, HUB_RULE_DOSSIER_NEW, priv->with_su );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->dossier_bin ));
	g_signal_connect( priv->dossier_bin, "ofa-changed", G_CALLBACK( on_dossier_bin_changed ), instance );
	if(( group_bin = my_ibin_get_size_group( MY_IBIN( priv->dossier_bin ), 0 ))){
		my_utils_size_group_add_size_group( group, group_bin );
	}

	/* exercice edition */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "exercice-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->exercice_bin = ofa_exercice_edit_bin_new( priv->hub, priv->settings_prefix, HUB_RULE_DOSSIER_NEW );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->exercice_bin ));
	g_signal_connect( priv->exercice_bin, "ofa-changed", G_CALLBACK( on_exercice_bin_changed ), instance );
	if(( group_bin = my_ibin_get_size_group( MY_IBIN( priv->exercice_bin ), 0 ))){
		my_utils_size_group_add_size_group( group, group_bin );
	}

	/* admin credentials */
	if( priv->with_admin ){
		parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "admin-parent" );
		g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
		priv->admin_bin = ofa_admin_credentials_bin_new( priv->hub, priv->settings_prefix, HUB_RULE_DOSSIER_NEW );
		gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->admin_bin ));
		g_signal_connect( priv->admin_bin, "ofa-changed", G_CALLBACK( on_admin_bin_changed ), instance );
		if(( group_bin = my_ibin_get_size_group( MY_IBIN( priv->admin_bin ), 0 ))){
			my_utils_size_group_add_size_group( group, group_bin );
		}
	}

	/* dossier actions on open */
	if( priv->with_actions ){
		parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "actions-parent" );
		g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
		priv->actions_bin = ofa_dossier_actions_bin_new( priv->hub, priv->settings_prefix, HUB_RULE_DOSSIER_NEW );
		gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->actions_bin ));
		g_signal_connect( priv->actions_bin, "ofa-changed", G_CALLBACK( on_actions_bin_changed ), instance );
	}

	/* message */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "dn-msg" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_style_add( label, "labelerror" );
	priv->msg_label = label;

	g_object_unref( group );

	read_settings( OFA_DOSSIER_NEW( instance ));

	on_dossier_bin_changed( priv->dossier_bin, OFA_DOSSIER_NEW( instance ));
}

static void
on_dossier_bin_changed( ofaDossierEditBin *bin, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	ofaIDBProvider *provider;
	GtkSizeGroup *group, *group_bin;

	priv = ofa_dossier_new_get_instance_private( self );

	provider = ofa_dossier_edit_bin_get_provider( priv->dossier_bin );
	ofa_exercice_edit_bin_set_provider( priv->exercice_bin, provider );

	group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );
	if(( group_bin = my_ibin_get_size_group( MY_IBIN( priv->dossier_bin ), 1 ))){
		my_utils_size_group_add_size_group( group, group_bin );
	}
	if(( group_bin = my_ibin_get_size_group( MY_IBIN( priv->exercice_bin ), 1 ))){
		my_utils_size_group_add_size_group( group, group_bin );
	}
	g_object_unref( group );

	check_for_enable_dlg( self );
}

static void
on_exercice_bin_changed( ofaExerciceEditBin *bin, ofaDossierNew *self )
{
	check_for_enable_dlg( self );
}

static void
on_admin_bin_changed( ofaAdminCredentialsBin *bin, const gchar *account, const gchar *password, ofaDossierNew *self )
{
	check_for_enable_dlg( self );
}

static void
on_actions_bin_changed( ofaDossierActionsBin *bin, ofaDossierNew *self )
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

	ok = my_ibin_is_valid( MY_IBIN( priv->dossier_bin ), &message ) &&
			my_ibin_is_valid( MY_IBIN( priv->exercice_bin ), &message );

	if( ok && priv->admin_bin ){
		ok = my_ibin_is_valid( MY_IBIN( priv->admin_bin ), &message );
	}
	if( ok && priv->actions_bin ){
		ok = my_ibin_is_valid( MY_IBIN( priv->actions_bin ), &message );
	}

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
 * When running non-modal, close the window
 */
static void
on_ok_clicked( ofaDossierNew *self )
{
	do_create( self );

	if( !gtk_window_get_modal( GTK_WINDOW( self ))){
		my_iwindow_close( MY_IWINDOW( self ));
	}
}

static gboolean
do_create( ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	gboolean ret, open, apply_actions;
	ofaIDBDossierMeta *dossier_meta;
	ofaIDBExerciceMeta *exercice_meta;
	ofaIDBConnect *connect;
	ofaIDBDossierEditor *dossier_editor;
	gchar *msgerr, *adm_account, *adm_password;
	ofaDossierCollection *collection;

	priv = ofa_dossier_new_get_instance_private( self );

	ret = FALSE;
	open = FALSE;
	msgerr = NULL;
	adm_account = NULL;
	adm_password = NULL;

	/* ask for user confirmation */
	if( priv->with_confirm && !create_confirmed( self )){
		return( FALSE );
	}

	/* register the new dossier in dossier settings */
	dossier_meta = ofa_dossier_edit_bin_apply( priv->dossier_bin );
	if( dossier_meta ){
		ofa_exercice_edit_bin_set_dossier_meta( priv->exercice_bin, dossier_meta );
		exercice_meta = ofa_exercice_edit_bin_apply( priv->exercice_bin );
		if( exercice_meta ){
			ret = TRUE;
		}
	}
	if( !ret ){
		my_utils_msg_dialog( GTK_WINDOW( self ), GTK_MESSAGE_ERROR,
				_( "Unable to register the new dossier in settings" ));
		return( FALSE );
	}

	/* create the new dossier
	 * if superuser credentials were allowed at initialization */
	if( priv->with_su ){
		dossier_editor = ofa_dossier_edit_bin_get_dossier_editor( priv->dossier_bin );
		connect = ofa_idbdossier_editor_get_valid_connect( dossier_editor, dossier_meta );
		if( priv->with_admin ){
			ofa_admin_credentials_bin_get_credentials( priv->admin_bin, &adm_account, &adm_password );
		}
		ret = ofa_idbconnect_period_new( connect, adm_account, adm_password, &msgerr );

		if( !ret ){
			collection = ofa_hub_get_dossier_collection( priv->hub );
			ofa_dossier_collection_delete_period( collection, connect, NULL, TRUE, NULL );
			if( !my_strlen( msgerr )){
				msgerr = g_strdup( _( "Unable to create the new dossier" ));
			}
			my_utils_msg_dialog( GTK_WINDOW( self ), GTK_MESSAGE_ERROR, msgerr );
			g_free( msgerr );
			g_object_unref( dossier_meta );
		}
	}
	if( ret ){
		priv->dossier_created = TRUE;
		if( priv->dossier_meta ){
			*( priv->dossier_meta ) = dossier_meta;
		}
	}
/*
		if( priv->admin_bin ){
			account = ofa_admin_credentials_bin_get_remembered_account( priv->admin_bin );
			ofa_idbexercice_meta_set_remembered_account( exercice_meta, account );
		}
		*/

	/* open the newly created dossier if asked for */
	if( ret ){
		if( priv->with_actions ){
			open = ofa_dossier_actions_bin_get_open( priv->actions_bin );
			if( open ){
				exercice_meta = ofa_idbdossier_meta_get_current_period( dossier_meta );
				connect = ofa_idbdossier_meta_new_connect( dossier_meta, exercice_meta );
				if( !ofa_idbconnect_open_with_account( connect, adm_account, adm_password )){
					my_utils_msg_dialog( GTK_WINDOW( self ), GTK_MESSAGE_ERROR,
							_( "Unable to connect to newly created dossier" ));
					g_object_unref( connect );
				} else {
					apply_actions = ofa_dossier_actions_bin_get_apply( priv->actions_bin );
					if( ofa_hub_dossier_open( priv->hub, priv->parent, connect, apply_actions, FALSE )){
						ofa_hub_dossier_remediate_settings( priv->hub );
						ret = TRUE;
					} else {
						my_utils_msg_dialog( GTK_WINDOW( self ), GTK_MESSAGE_ERROR,
								_( "Unable to open the newly created dossier" ));
						g_object_unref( connect );
					}
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
	GtkWindow *toplevel;

	str = g_strdup_printf(
				_( "The create operation will drop and fully reset the target database.\n"
					"This may not be what you actually want !\n"
					"Are you sure you want to create into this database ?" ));

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ok = my_utils_dialog_question( toplevel, str, _( "C_reate" ));

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
