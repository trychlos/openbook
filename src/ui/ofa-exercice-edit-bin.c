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

#include "my/my-ibin.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-editor.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"

#include "ui/ofa-admin-credentials-bin.h"
#include "ui/ofa-dossier-actions-bin.h"
#include "ui/ofa-exercice-edit-bin.h"
#include "ui/ofa-exercice-meta-bin.h"

/* private instance data
 */
typedef struct {
	gboolean                dispose_has_run;

	/* initialization
	 */
	ofaHub                 *hub;
	gchar                  *settings_prefix;
	guint                   rule;
	gboolean                with_admin;
	gboolean                with_open;

	/* UI
	 */
	GtkSizeGroup           *group0;
	GtkSizeGroup           *group1;
	ofaExerciceMetaBin     *exercice_meta_bin;
	GtkWidget              *exercice_editor_parent;
	ofaIDBExerciceEditor   *exercice_editor_bin;
	ofaAdminCredentialsBin *admin_bin;
	ofaDossierActionsBin   *actions_bin;
	GtkWidget              *msg_label;

	/* runtime
	 */
	ofaIDBProvider         *provider;
	ofaIDBDossierMeta      *dossier_meta;

	/* result
	 */
	ofaIDBExerciceMeta     *exercice_meta;		/* the newly created ofaIDBExerciceMeta */
}
	ofaExerciceEditBinPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-exercice-edit-bin.ui";

static void          setup_bin( ofaExerciceEditBin *self );
static void          on_exercice_editor_changed( ofaIDBExerciceEditor *editor, ofaExerciceEditBin *self );
static void          on_exercice_meta_changed( ofaExerciceMetaBin *bin, ofaExerciceEditBin *self );
static void          on_exercice_editor_changed( ofaIDBExerciceEditor *editor, ofaExerciceEditBin *self );
static void          on_admin_credentials_changed( ofaAdminCredentialsBin *bin, const gchar *account, const gchar *password, ofaExerciceEditBin *self );
static void          on_actions_bin_changed( ofaDossierActionsBin *bin, ofaExerciceEditBin *self );
static void          changed_composite( ofaExerciceEditBin *self );
static void          ibin_iface_init( myIBinInterface *iface );
static guint         ibin_get_interface_version( void );
static GtkSizeGroup *ibin_get_size_group( const myIBin *instance, guint column );
static gboolean      ibin_is_valid( const myIBin *instance, gchar **msgerr );

G_DEFINE_TYPE_EXTENDED( ofaExerciceEditBin, ofa_exercice_edit_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaExerciceEditBin )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IBIN, ibin_iface_init ))

static void
exercice_edit_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_exercice_edit_bin_finalize";
	ofaExerciceEditBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EXERCICE_EDIT_BIN( instance ));

	/* free data members here */
	priv = ofa_exercice_edit_bin_get_instance_private( OFA_EXERCICE_EDIT_BIN( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exercice_edit_bin_parent_class )->finalize( instance );
}

static void
exercice_edit_bin_dispose( GObject *instance )
{
	ofaExerciceEditBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_EXERCICE_EDIT_BIN( instance ));

	priv = ofa_exercice_edit_bin_get_instance_private( OFA_EXERCICE_EDIT_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
		g_clear_object( &priv->group1 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exercice_edit_bin_parent_class )->dispose( instance );
}

static void
ofa_exercice_edit_bin_init( ofaExerciceEditBin *self )
{
	static const gchar *thisfn = "ofa_exercice_edit_bin_instance_init";
	ofaExerciceEditBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXERCICE_EDIT_BIN( self ));

	priv = ofa_exercice_edit_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->with_admin = TRUE;
	priv->with_open = TRUE;
	priv->group0 = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );
	priv->group1 = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );
	priv->dossier_meta = NULL;
	priv->exercice_meta = NULL;
}

static void
ofa_exercice_edit_bin_class_init( ofaExerciceEditBinClass *klass )
{
	static const gchar *thisfn = "ofa_exercice_edit_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = exercice_edit_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = exercice_edit_bin_finalize;

	/**
	 * ofaExerciceEditBin::ofa-changed:
	 *
	 * This signal is sent on the #ofaExerciceEditBin when any of the
	 * underlying information is changed. This includes the dossier
	 * name, the DBMS provider, the connection informations and the
	 * DBMS root credentials
	 *
	 * There is no argument.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaExerciceEditBin *bin,
	 * 						gpointer         user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_EXERCICE_EDIT_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0,
				G_TYPE_NONE );
}

/**
 * ofa_exercice_edit_bin_new:
 * @hub: the #ofaHub object of the application.
 * @settings_prefix: the prefix of the key in user settings.
 * @rule: the usage of this widget.
 * @with_admin: whether we should display the AdminCredentials widget.
 * @with_open: whether we should display the DossierActions widget.
 *
 * Returns: a newly defined composite widget.
 */
ofaExerciceEditBin *
ofa_exercice_edit_bin_new( ofaHub *hub, const gchar *settings_prefix, guint rule, gboolean with_admin, gboolean with_open )
{
	static const gchar *thisfn = "ofa_exercice_edit_bin_new";
	ofaExerciceEditBin *bin;
	ofaExerciceEditBinPrivate *priv;

	g_debug( "%s: hub=%p, settings_prefix=%s, rule=%u, with_admin=%s, with_open=%s",
			thisfn, ( void * ) hub, settings_prefix, rule, with_admin ? "True":"False", with_open ? "True":"False" );

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( my_strlen( settings_prefix ), NULL );

	bin = g_object_new( OFA_TYPE_EXERCICE_EDIT_BIN, NULL );

	priv = ofa_exercice_edit_bin_get_instance_private( bin );

	priv->hub = hub;
	priv->rule = rule;
	priv->with_admin = with_admin;
	priv->with_open = with_open;

	g_free( priv->settings_prefix );
	priv->settings_prefix = g_strdup( settings_prefix );

	setup_bin( bin );

	return( bin );
}

static void
setup_bin( ofaExerciceEditBin *self )
{
	ofaExerciceEditBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *parent;
	GtkSizeGroup *group_bin;

	priv = ofa_exercice_edit_bin_get_instance_private( self );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "eeb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	/* exercice meta datas */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "eeb-exercice-meta-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->exercice_meta_bin = ofa_exercice_meta_bin_new( priv->hub, priv->settings_prefix, priv->rule );
	g_signal_connect( priv->exercice_meta_bin, "ofa-changed", G_CALLBACK( on_exercice_meta_changed ), self );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->exercice_meta_bin ));
	if(( group_bin = my_ibin_get_size_group( MY_IBIN( priv->exercice_meta_bin ), 0 ))){
		my_utils_size_group_add_size_group( priv->group0, group_bin );
	}

	/* exercice dbeditor */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "eeb-exercice-editor-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->exercice_editor_parent = parent;

	/* administrative credentials */
	if( priv->with_admin ){
		parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "eeb-admin-parent" );
		g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
		priv->admin_bin = ofa_admin_credentials_bin_new( priv->hub, priv->settings_prefix, priv->rule );
		g_signal_connect( priv->admin_bin, "ofa-changed", G_CALLBACK( on_admin_credentials_changed ), self );
		gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->admin_bin ));
		if(( group_bin = my_ibin_get_size_group( MY_IBIN( priv->admin_bin ), 0 ))){
			my_utils_size_group_add_size_group( priv->group0, group_bin );
		}
	}

	/* actions on creation */
	if( priv->with_open ){
		parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "eeb-actions-parent" );
		g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
		priv->actions_bin = ofa_dossier_actions_bin_new( priv->hub, priv->settings_prefix, priv->rule );
		g_signal_connect( priv->actions_bin, "ofa-changed", G_CALLBACK( on_actions_bin_changed ), self );
		gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->actions_bin ));
	}

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
on_exercice_meta_changed( ofaExerciceMetaBin *bin, ofaExerciceEditBin *self )
{
	changed_composite( self );
}

/**
 * ofa_exercice_edit_bin_set_provider:
 * @bin: this #ofaExerciceEditBin instance.
 * @provider: [allow-none]: the #ofaIDBProvider to be attached to.
 *
 * Set the #ofaIDBProvider, initializing the specific part of the
 * exercice editor.
 */
void
ofa_exercice_edit_bin_set_provider( ofaExerciceEditBin *bin, ofaIDBProvider *provider )
{
	ofaExerciceEditBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_EXERCICE_EDIT_BIN( bin ));
	g_return_if_fail( !provider || OFA_IS_IDBPROVIDER( provider ));

	priv = ofa_exercice_edit_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	if( provider != priv->provider ){
		if( priv->exercice_editor_bin ){
			gtk_container_remove( GTK_CONTAINER( priv->exercice_editor_parent ), GTK_WIDGET( priv->exercice_editor_bin ));
			priv->provider = NULL;
		}
		if( provider ){
			priv->provider = provider;
			priv->exercice_editor_bin = ofa_idbprovider_new_exercice_editor( provider, priv->settings_prefix, priv->rule );
			gtk_container_add( GTK_CONTAINER( priv->exercice_editor_parent ), GTK_WIDGET( priv->exercice_editor_bin ));
			g_signal_connect( priv->exercice_editor_bin, "ofa-changed", G_CALLBACK( on_exercice_editor_changed ), bin );
			my_utils_size_group_add_size_group( priv->group1, ofa_idbexercice_editor_get_size_group( priv->exercice_editor_bin, 0 ));
		}
		gtk_widget_show_all( GTK_WIDGET( bin ));
	}
}

/**
 * ofa_exercice_edit_bin_set_dossier_meta:
 * @bin: this #ofaExerciceEditBin instance.
 * @dossier_meta: [allow-none]: the #ofaIDBDossierMeta to be attached to.
 *
 * Set the #ofaIDBDossierMeta dossier, initializing the specific part
 * of the exercice editor.
 */
void
ofa_exercice_edit_bin_set_dossier_meta( ofaExerciceEditBin *bin, ofaIDBDossierMeta *dossier_meta )
{
	ofaExerciceEditBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_EXERCICE_EDIT_BIN( bin ));
	g_return_if_fail( !dossier_meta || OFA_IS_IDBDOSSIER_META( dossier_meta ));

	priv = ofa_exercice_edit_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	priv->dossier_meta = dossier_meta;
	ofa_exercice_meta_bin_set_dossier_meta( priv->exercice_meta_bin, dossier_meta );
	ofa_exercice_edit_bin_set_provider( bin, ofa_idbdossier_meta_get_provider( dossier_meta ));
}

static void
on_exercice_editor_changed( ofaIDBExerciceEditor *editor, ofaExerciceEditBin *self )
{
	changed_composite( self );
}

static void
on_admin_credentials_changed( ofaAdminCredentialsBin *bin, const gchar *account, const gchar *password, ofaExerciceEditBin *self )
{
	changed_composite( self );
}

static void
on_actions_bin_changed( ofaDossierActionsBin *bin, ofaExerciceEditBin *self )
{
	changed_composite( self );
}

static void
changed_composite( ofaExerciceEditBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_exercice_edit_bin_apply:
 * @bin: this #ofaExerciceEditBin instance.
 *
 * Returns: a new #ofaIDBExerciceMeta object attached to the dossier.
 *
 * This is an error to not have set a dossier at apply time (because we
 * cannot actually define an exercice if we do not know for which dossier).
 */
ofaIDBExerciceMeta *
ofa_exercice_edit_bin_apply( ofaExerciceEditBin *bin )
{
	static const gchar *thisfn = "ofa_exercice_edit_bin_apply";
	ofaExerciceEditBinPrivate *priv;
	ofaIDBExerciceMeta *exercice_meta;
	const gchar *account;

	g_debug( "%s: bin=%p", thisfn, ( void * ) bin );

	g_return_val_if_fail( bin && OFA_IS_EXERCICE_EDIT_BIN( bin ), NULL );

	priv = ofa_exercice_edit_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );
	g_return_val_if_fail( priv->dossier_meta && OFA_IS_IDBDOSSIER_META( priv->dossier_meta ), NULL );

	exercice_meta = ofa_exercice_meta_bin_apply( priv->exercice_meta_bin );
	ofa_idbexercice_meta_set_from_editor( exercice_meta, priv->exercice_editor_bin );

	if( priv->admin_bin ){
		account = ofa_admin_credentials_bin_get_remembered_account( priv->admin_bin );
		ofa_idbexercice_meta_set_remembered_account( exercice_meta, account );
	}

	return( exercice_meta );
}

/**
 * ofa_exercice_edit_bin_get_admin_credentials:
 * @bin: this #ofaExerciceEditBin instance.
 * @account: [out]: a placeholder for the account.
 * @password: [out]: a placeholder for the password.
 *
 * Set the provided placeholders to their respective value.
 */
void
ofa_exercice_edit_bin_get_admin_credentials( ofaExerciceEditBin *bin, gchar **account, gchar **password )
{
	ofaExerciceEditBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_EXERCICE_EDIT_BIN( bin ));
	g_return_if_fail( account );
	g_return_if_fail( password );

	priv = ofa_exercice_edit_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	if( account ){
		*account = NULL;
	}
	if( password ){
		*password = NULL;
	}
	if( priv->with_admin ){
		g_return_if_fail( priv->admin_bin && OFA_IS_ADMIN_CREDENTIALS_BIN( priv->admin_bin ));
		ofa_admin_credentials_bin_get_credentials( priv->admin_bin, account, password );
	}
}

/**
 * ofa_exercice_edit_bin_get_open_on_create:
 * @bin: this #ofaExerciceEditBin instance.
 *
 * Returns: %TRUE if the new dossier should be opened after creation.
 */
gboolean
ofa_exercice_edit_bin_get_open_on_create( ofaExerciceEditBin *bin )
{
	ofaExerciceEditBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_EXERCICE_EDIT_BIN( bin ), FALSE );

	priv = ofa_exercice_edit_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->actions_bin ? ofa_dossier_actions_bin_get_open( priv->actions_bin ) : FALSE );
}

/**
 * ofa_exercice_edit_bin_get_apply_actions:
 * @bin: this #ofaExerciceEditBin instance.
 *
 * Returns: %TRUE if the new dossier should be opened after creation.
 */
gboolean
ofa_exercice_edit_bin_get_apply_actions( ofaExerciceEditBin *bin )
{
	ofaExerciceEditBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_EXERCICE_EDIT_BIN( bin ), FALSE );

	priv = ofa_exercice_edit_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->actions_bin ? ofa_dossier_actions_bin_get_apply( priv->actions_bin ) : FALSE );
}

/*
 * myIBin interface management
 */
static void
ibin_iface_init( myIBinInterface *iface )
{
	static const gchar *thisfn = "ofa_exercice_edit_bin_ibin_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = ibin_get_interface_version;
	iface->get_size_group = ibin_get_size_group;
	iface->is_valid = ibin_is_valid;
}

static guint
ibin_get_interface_version( void )
{
	return( 1 );
}

static GtkSizeGroup *
ibin_get_size_group( const myIBin *instance, guint column )
{
	static const gchar *thisfn = "ofa_exercice_edit_bin_ibin_get_size_group";
	ofaExerciceEditBinPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_EXERCICE_EDIT_BIN( instance ), NULL );

	priv = ofa_exercice_edit_bin_get_instance_private( OFA_EXERCICE_EDIT_BIN( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );

	} else if( column == 1 ){
		return( priv->group1 );
	}

	g_warning( "%s: invalid column=%u", thisfn, column );

	return( NULL );
}

/*
 * Note that checks may be better if an #ofaIDBDossierMeta has been set.
 */
gboolean
ibin_is_valid( const myIBin *instance, gchar **msgerr )
{
	ofaExerciceEditBinPrivate *priv;
	gboolean ok = TRUE;

	g_return_val_if_fail( instance && OFA_IS_EXERCICE_EDIT_BIN( instance ), FALSE );

	priv = ofa_exercice_edit_bin_get_instance_private( OFA_EXERCICE_EDIT_BIN( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	if( ok ){
		ok = my_ibin_is_valid( MY_IBIN( priv->exercice_meta_bin ), msgerr );
	}
	if( ok ){
		ok = priv->exercice_editor_bin ? ofa_idbexercice_editor_is_valid( priv->exercice_editor_bin, msgerr ) : TRUE;
	}
	if( ok && priv->admin_bin ){
		ok = my_ibin_is_valid( MY_IBIN( priv->admin_bin ), msgerr );
	}
	if( ok && priv->actions_bin ){
		ok = my_ibin_is_valid( MY_IBIN( priv->actions_bin ), msgerr );
	}

	return( ok );
}
