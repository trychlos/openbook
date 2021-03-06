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

#include "my/my-ibin.h"
#include "my/my-utils.h"

#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-editor.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-igetter.h"

#include "ui/ofa-exercice-edit-bin.h"
#include "ui/ofa-exercice-meta-bin.h"

/* private instance data
 */
typedef struct {
	gboolean                dispose_has_run;

	/* initialization
	 */
	ofaIGetter             *getter;
	gchar                  *settings_prefix;
	guint                   rule;

	/* UI
	 */
	GtkSizeGroup           *group0;
	GtkSizeGroup           *group1;
	ofaExerciceMetaBin     *exercice_meta_bin;
	GtkWidget              *exercice_editor_parent;
	ofaIDBExerciceEditor   *exercice_editor_bin;
	GtkWidget              *msg_label;

	/* runtime
	 */
	ofaIDBProvider         *provider;
	ofaIDBDossierMeta      *dossier_meta;
}
	ofaExerciceEditBinPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-exercice-edit-bin.ui";

static void          setup_bin( ofaExerciceEditBin *self );
static void          on_exercice_editor_changed( myIBin *editor, ofaExerciceEditBin *self );
static void          on_exercice_meta_changed( myIBin *bin, ofaExerciceEditBin *self );
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
	priv->group0 = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );
	priv->group1 = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );
	priv->dossier_meta = NULL;
}

static void
ofa_exercice_edit_bin_class_init( ofaExerciceEditBinClass *klass )
{
	static const gchar *thisfn = "ofa_exercice_edit_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = exercice_edit_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = exercice_edit_bin_finalize;
}

/**
 * ofa_exercice_edit_bin_new:
 * @getter: a #ofaIGetter instance.
 * @settings_prefix: [allow-none]: the prefix of the key in user settings;
 *  if %NULL, then rely on this class name;
 *  when set, then this class automatically adds its name as a suffix.
 * @rule: the usage of this widget.
 *
 * Returns: a newly defined composite widget.
 */
ofaExerciceEditBin *
ofa_exercice_edit_bin_new( ofaIGetter *getter, const gchar *settings_prefix, guint rule )
{
	static const gchar *thisfn = "ofa_exercice_edit_bin_new";
	ofaExerciceEditBin *bin;
	ofaExerciceEditBinPrivate *priv;
	gchar *str;

	g_debug( "%s: getter=%p, settings_prefix=%s, rule=%u",
			thisfn, ( void * ) getter, settings_prefix, rule );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( settings_prefix ), NULL );

	bin = g_object_new( OFA_TYPE_EXERCICE_EDIT_BIN, NULL );

	priv = ofa_exercice_edit_bin_get_instance_private( bin );

	priv->getter = getter;
	priv->rule = rule;

	if( my_strlen( settings_prefix )){
		str = g_strdup_printf( "%s-%s", settings_prefix, priv->settings_prefix );
		g_free( priv->settings_prefix );
		priv->settings_prefix = str;
	}

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
	priv->exercice_meta_bin = ofa_exercice_meta_bin_new( priv->getter, priv->settings_prefix, priv->rule );
	g_signal_connect( priv->exercice_meta_bin, "my-ibin-changed", G_CALLBACK( on_exercice_meta_changed ), self );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->exercice_meta_bin ));
	if(( group_bin = my_ibin_get_size_group( MY_IBIN( priv->exercice_meta_bin ), 0 ))){
		my_utils_size_group_add_size_group( priv->group0, group_bin );
	}

	/* exercice dbeditor */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "eeb-exercice-editor-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->exercice_editor_parent = parent;

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
on_exercice_meta_changed( myIBin *bin, ofaExerciceEditBin *self )
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
			g_signal_connect( priv->exercice_editor_bin, "my-ibin-changed", G_CALLBACK( on_exercice_editor_changed ), bin );
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
on_exercice_editor_changed( myIBin *editor, ofaExerciceEditBin *self )
{
	changed_composite( self );
}

static void
changed_composite( ofaExerciceEditBin *self )
{
	g_signal_emit_by_name( self, "my-ibin-changed" );
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

	g_debug( "%s: bin=%p", thisfn, ( void * ) bin );

	g_return_val_if_fail( bin && OFA_IS_EXERCICE_EDIT_BIN( bin ), NULL );

	priv = ofa_exercice_edit_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );
	g_return_val_if_fail( priv->dossier_meta && OFA_IS_IDBDOSSIER_META( priv->dossier_meta ), NULL );

	exercice_meta = ofa_exercice_meta_bin_apply( priv->exercice_meta_bin );
	ofa_idbexercice_meta_set_from_editor( exercice_meta, priv->exercice_editor_bin );

	return( exercice_meta );
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

	return( ok );
}
