/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbdossier-editor.h"
#include "api/ofa-idbexercice-editor.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-igetter.h"

#include "ui/ofa-admin-credentials-bin.h"
#include "ui/ofa-dossier-actions-bin.h"
#include "ui/ofa-dossier-edit-bin.h"
#include "ui/ofa-dossier-meta-bin.h"
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

	/* UI
	 */
	GtkSizeGroup           *group0;
	GtkSizeGroup           *group1;
	ofaDossierMetaBin      *dossier_meta_bin;
	GtkWidget              *dossier_editor_parent;
	ofaIDBDossierEditor    *dossier_editor_bin;
	ofaExerciceMetaBin     *exercice_meta_bin;
	GtkWidget              *exercice_editor_parent;
	ofaIDBExerciceEditor   *exercice_editor_bin;
	ofaAdminCredentialsBin *admin_bin;
	ofaDossierActionsBin   *actions_bin;
	GtkWidget              *msg_label;

	/* runtime
	 */
	ofaIDBProvider         *provider;
}
	ofaDossierEditBinPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-dossier-edit-bin.ui";

static void setup_bin( ofaDossierEditBin *self );
static void on_dossier_meta_changed( ofaDossierMetaBin *bin, ofaDossierEditBin *self );
static void on_dossier_editor_changed( ofaIDBDossierEditor *editor, ofaDossierEditBin *self );
static void on_exercice_meta_changed( ofaExerciceMetaBin *bin, ofaDossierEditBin *self );
static void on_exercice_editor_changed( ofaIDBExerciceEditor *editor, ofaDossierEditBin *self );
static void on_admin_credentials_changed( ofaAdminCredentialsBin *bin, const gchar *account, const gchar *password, ofaDossierEditBin *self );
static void on_actions_bin_changed( ofaDossierActionsBin *bin, ofaDossierEditBin *self );
static void changed_composite( ofaDossierEditBin *self );

G_DEFINE_TYPE_EXTENDED( ofaDossierEditBin, ofa_dossier_edit_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaDossierEditBin ))

static void
dossier_edit_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_edit_bin_finalize";
	ofaDossierEditBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_EDIT_BIN( instance ));

	/* free data members here */
	priv = ofa_dossier_edit_bin_get_instance_private( OFA_DOSSIER_EDIT_BIN( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_edit_bin_parent_class )->finalize( instance );
}

static void
dossier_edit_bin_dispose( GObject *instance )
{
	ofaDossierEditBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_EDIT_BIN( instance ));

	priv = ofa_dossier_edit_bin_get_instance_private( OFA_DOSSIER_EDIT_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
		g_clear_object( &priv->group1 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_edit_bin_parent_class )->dispose( instance );
}

static void
ofa_dossier_edit_bin_init( ofaDossierEditBin *self )
{
	static const gchar *thisfn = "ofa_dossier_edit_bin_instance_init";
	ofaDossierEditBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_EDIT_BIN( self ));

	priv = ofa_dossier_edit_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->provider = NULL;
	priv->group0 = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );
	priv->group1 = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );
}

static void
ofa_dossier_edit_bin_class_init( ofaDossierEditBinClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_edit_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_edit_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_edit_bin_finalize;

	/**
	 * ofaDossierEditBin::ofa-changed:
	 *
	 * This signal is sent on the #ofaDossierEditBin when any of the
	 * underlying information is changed. This includes the dossier
	 * name, the DBMS provider, the connection informations and the
	 * DBMS root credentials
	 *
	 * There is no argument.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDossierEditBin *bin,
	 * 						gpointer         user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_DOSSIER_EDIT_BIN,
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
 * ofa_dossier_edit_bin_new:
 * @hub: the #ofaHub object of the application.
 * @settings_prefix: the prefix of the key in user settings.
 * @rule: the usage of this widget.
 *
 * Returns: a newly defined composite widget.
 */
ofaDossierEditBin *
ofa_dossier_edit_bin_new( ofaHub *hub, const gchar *settings_prefix, guint rule )
{
	static const gchar *thisfn = "ofa_dossier_edit_bin_new";
	ofaDossierEditBin *bin;
	ofaDossierEditBinPrivate *priv;

	g_debug( "%s: hub=%p, settings_prefix=%s, guint=%u",
			thisfn, ( void * ) hub, settings_prefix, rule );

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( my_strlen( settings_prefix ), NULL );

	bin = g_object_new( OFA_TYPE_DOSSIER_EDIT_BIN, NULL );

	priv = ofa_dossier_edit_bin_get_instance_private( bin );

	priv->hub = hub;
	priv->rule = rule;

	g_free( priv->settings_prefix );
	priv->settings_prefix = g_strdup( settings_prefix );

	setup_bin( bin );
	on_dossier_meta_changed( priv->dossier_meta_bin, bin );

	return( bin );
}

static void
setup_bin( ofaDossierEditBin *self )
{
	ofaDossierEditBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *parent;

	priv = ofa_dossier_edit_bin_get_instance_private( self );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "deb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	/* dossier meta datas */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "deb-dossier-meta-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->dossier_meta_bin = ofa_dossier_meta_bin_new( priv->hub, priv->settings_prefix, priv->rule );
	g_signal_connect( priv->dossier_meta_bin, "ofa-changed", G_CALLBACK( on_dossier_meta_changed ), self );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->dossier_meta_bin ));
	my_utils_size_group_add_size_group( priv->group0, ofa_dossier_meta_bin_get_size_group( priv->dossier_meta_bin, 0 ));

	/* dossier dbeditor */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "deb-dossier-editor-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->dossier_editor_parent = parent;

	/* exercice meta datas */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "deb-exercice-meta-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->exercice_meta_bin = ofa_exercice_meta_bin_new( priv->hub, priv->settings_prefix, priv->rule );
	g_signal_connect( priv->exercice_meta_bin, "ofa-changed", G_CALLBACK( on_exercice_meta_changed ), self );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->exercice_meta_bin ));
	my_utils_size_group_add_size_group( priv->group0, ofa_exercice_meta_bin_get_size_group( priv->exercice_meta_bin, 0 ));

	/* exercice dbeditor */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "deb-exercice-editor-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->exercice_editor_parent = parent;

	/* administrative credentials */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "deb-admin-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->admin_bin = ofa_admin_credentials_bin_new();
	g_signal_connect( priv->admin_bin, "ofa-changed", G_CALLBACK( on_admin_credentials_changed ), self );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->admin_bin ));
	my_utils_size_group_add_size_group( priv->group0, ofa_admin_credentials_bin_get_size_group( priv->admin_bin, 0 ));

	/* actions on creation */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "deb-actions-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->actions_bin = ofa_dossier_actions_bin_new( priv->hub, priv->settings_prefix, priv->rule );
	g_signal_connect( priv->actions_bin, "ofa-changed", G_CALLBACK( on_actions_bin_changed ), self );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->actions_bin ));
	//my_utils_size_group_add_size_group( priv->group1, ofa_dossier_actions_bin_get_size_group( priv->actions_bin, 0 ));

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
on_dossier_meta_changed( ofaDossierMetaBin *bin, ofaDossierEditBin *self )
{
	ofaDossierEditBinPrivate *priv;
	ofaIDBProvider *provider;

	priv = ofa_dossier_edit_bin_get_instance_private( self );

	provider = ofa_dossier_meta_bin_get_provider( priv->dossier_meta_bin );

	if( provider != priv->provider ){
		priv->provider = provider;

		/* dossier editor */
		if( priv->dossier_editor_bin ){
			gtk_container_remove( GTK_CONTAINER( priv->dossier_editor_parent ), GTK_WIDGET( priv->dossier_editor_bin ));
		}
		priv->dossier_editor_bin = ofa_idbprovider_new_dossier_editor( provider, priv->rule );
		gtk_container_add( GTK_CONTAINER( priv->dossier_editor_parent ), GTK_WIDGET( priv->dossier_editor_bin ));
		g_signal_connect( priv->dossier_editor_bin, "ofa-changed", G_CALLBACK( on_dossier_editor_changed ), self );
		my_utils_size_group_add_size_group( priv->group1, ofa_idbdossier_editor_get_size_group( priv->dossier_editor_bin, 0 ));

		/* exercice editor */
		if( priv->exercice_editor_bin ){
			gtk_container_remove( GTK_CONTAINER( priv->exercice_editor_parent ), GTK_WIDGET( priv->exercice_editor_bin ));
		}
		priv->exercice_editor_bin = ofa_idbprovider_new_exercice_editor( provider, priv->rule, priv->dossier_editor_bin );
		gtk_container_add( GTK_CONTAINER( priv->exercice_editor_parent ), GTK_WIDGET( priv->exercice_editor_bin ));
		g_signal_connect( priv->exercice_editor_bin, "ofa-changed", G_CALLBACK( on_exercice_editor_changed ), self );
		my_utils_size_group_add_size_group( priv->group1, ofa_idbexercice_editor_get_size_group( priv->exercice_editor_bin, 0 ));

		gtk_widget_show_all( GTK_WIDGET( self ));
	}

	changed_composite( self );
}

static void
on_dossier_editor_changed( ofaIDBDossierEditor *editor, ofaDossierEditBin *self )
{
	changed_composite( self );
}

static void
on_exercice_meta_changed( ofaExerciceMetaBin *bin, ofaDossierEditBin *self )
{
	changed_composite( self );
}

static void
on_exercice_editor_changed( ofaIDBExerciceEditor *editor, ofaDossierEditBin *self )
{
	changed_composite( self );
}

static void
on_admin_credentials_changed( ofaAdminCredentialsBin *bin, const gchar *account, const gchar *password, ofaDossierEditBin *self )
{
	changed_composite( self );
}

static void
on_actions_bin_changed( ofaDossierActionsBin *bin, ofaDossierEditBin *self )
{
	changed_composite( self );
}

static void
changed_composite( ofaDossierEditBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_dossier_edit_bin_is_valid:
 * @bin: this #ofaDossierEditBin instance.
 * @message: [out][allow-none]: a placeholder for the output error message.
 *
 * Returns: %TRUE if the dialog is valid.
 */
gboolean
ofa_dossier_edit_bin_is_valid( ofaDossierEditBin *bin, gchar **message )
{
	ofaDossierEditBinPrivate *priv;
	gboolean ok = TRUE;

	g_return_val_if_fail( bin && OFA_IS_DOSSIER_EDIT_BIN( bin ), FALSE );

	priv = ofa_dossier_edit_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	if( ok ){
		ok = ofa_dossier_meta_bin_is_valid( priv->dossier_meta_bin, message );
	}
	if( ok ){
		ok = priv->dossier_editor_bin ? ofa_idbdossier_editor_is_valid( priv->dossier_editor_bin, message ) : TRUE;
	}
	if( ok ){
		ok = ofa_exercice_meta_bin_is_valid( priv->exercice_meta_bin, message );
	}
	if( ok ){
		ok = priv->exercice_editor_bin ? ofa_idbexercice_editor_is_valid( priv->exercice_editor_bin, message ) : TRUE;
	}
	if( ok ){
		ok = ofa_admin_credentials_bin_is_valid( priv->admin_bin, message );
	}
	if( ok ){
		ok = ofa_dossier_actions_bin_is_valid( priv->actions_bin, message );
	}

	return( ok );
}

/**
 * ofa_dossier_edit_bin_apply:
 *
 * Define the dossier in user settings.
 *
 * Returns: %TRUE if the new dossier has been successfully registered.
 */
gboolean
ofa_dossier_edit_bin_apply( ofaDossierEditBin *bin )
{
	ofaDossierEditBinPrivate *priv;
	gboolean ok = TRUE;

	g_return_val_if_fail( bin && OFA_IS_DOSSIER_EDIT_BIN( bin ), FALSE );

	priv = ofa_dossier_edit_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	if( ok ){
		ok = ofa_dossier_meta_bin_apply( priv->dossier_meta_bin );
	}
	if( ok ){
		ok = priv->dossier_editor_bin ? ofa_idbdossier_editor_apply( priv->dossier_editor_bin ) : TRUE;
	}
	if( ok ){
		ok = ofa_exercice_meta_bin_apply( priv->exercice_meta_bin );
	}
	if( ok ){
		ok = priv->exercice_editor_bin ? ofa_idbexercice_editor_apply( priv->exercice_editor_bin ) : TRUE;
	}
	if( ok ){
		ok = ofa_dossier_actions_bin_apply( priv->actions_bin );
	}

	return( ok );
}
