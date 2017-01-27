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

#include "my/my-utils.h"

#include "api/ofa-dossier-collection.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-igetter.h"

#include "ui/ofa-dossier-treeview.h"
#include "ui/ofa-dossier-new.h"
#include "ui/ofa-exercice-treeview.h"
#include "ui/ofa-exercice-new.h"
#include "ui/ofa-target-chooser-bin.h"

/* private instance data
 */
typedef struct {
	gboolean             dispose_has_run;

	/* initialization
	 */
	ofaIGetter          *getter;
	gchar               *settings_prefix;

	/* runtime data
	 */
	ofaHub              *hub;
	ofaIDBDossierMeta   *dossier_meta;
	ofaIDBExerciceMeta  *exercice_meta;
	gulong               collection_handler;
	gboolean             block_dossier;
	gboolean             block_exercice;
	GList               *new_list;

	/* UI
	 */
	ofaDossierTreeview  *dossier_tview;
	GtkWidget           *dossier_new_btn;
	ofaExerciceTreeview *exercice_tview;
	GtkWidget           *exercice_new_btn;
}
	ofaTargetChooserBinPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-target-chooser-bin.ui";

static void     setup_bin( ofaTargetChooserBin *self );
static void     dossier_on_selection_changed( ofaDossierTreeview *treeview, ofaIDBDossierMeta *meta, ofaIDBExerciceMeta *empty, ofaTargetChooserBin *self );
static void     dossier_on_new( GtkButton *button, ofaTargetChooserBin *self );
static void     exercice_set_sensitive( ofaTargetChooserBin *self );
static void     exercice_set_sensitive_widget( GtkWidget *widget, ofaTargetChooserBin *self );
static void     exercice_on_selection_changed( ofaExerciceTreeview *treeview, ofaIDBExerciceMeta *meta, ofaTargetChooserBin *self );
static void     period_on_new( GtkButton *button, ofaTargetChooserBin *self );
static void     set_collection_handler( ofaTargetChooserBin *self );
static void     remove_collection_handler( ofaTargetChooserBin *self );
static void     on_collection_changed( ofaDossierCollection *collection, guint count, ofaTargetChooserBin *self );
static void     set_new_object( ofaTargetChooserBin *self, GObject *object );
static gboolean get_is_new_object( ofaTargetChooserBin *self, GObject *object );

G_DEFINE_TYPE_EXTENDED( ofaTargetChooserBin, ofa_target_chooser_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaTargetChooserBin ))

static void
target_chooser_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_target_chooser_bin_finalize";
	ofaTargetChooserBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TARGET_CHOOSER_BIN( instance ));

	/* free data members here */
	priv = ofa_target_chooser_bin_get_instance_private( OFA_TARGET_CHOOSER_BIN( instance ));

	g_free( priv->settings_prefix );
	g_list_free( priv->new_list );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_target_chooser_bin_parent_class )->finalize( instance );
}

static void
target_chooser_bin_dispose( GObject *instance )
{
	ofaTargetChooserBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_TARGET_CHOOSER_BIN( instance ));

	priv = ofa_target_chooser_bin_get_instance_private( OFA_TARGET_CHOOSER_BIN( instance ));

	if( !priv->dispose_has_run ){

		remove_collection_handler( OFA_TARGET_CHOOSER_BIN( instance ));

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_target_chooser_bin_parent_class )->dispose( instance );
}

static void
ofa_target_chooser_bin_init( ofaTargetChooserBin *self )
{
	static const gchar *thisfn = "ofa_target_chooser_bin_init";
	ofaTargetChooserBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_TARGET_CHOOSER_BIN( self ));

	priv = ofa_target_chooser_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->block_dossier = FALSE;
	priv->block_exercice = FALSE;
}

static void
ofa_target_chooser_bin_class_init( ofaTargetChooserBinClass *klass )
{
	static const gchar *thisfn = "ofa_target_chooser_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = target_chooser_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = target_chooser_bin_finalize;

	/**
	 * ofaTargetChooserBin::ofa-changed:
	 *
	 * This signal is sent on the #ofaTargetChooserBin when any of the
	 * underlying information is changed.
	 *
	 * Arguments are currently selected dossier and exercices.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaTargetChooserBin *bin,
	 * 						ofaDossierMeta    *dossier_meta;
	 * 						ofaExerciceMeta   *exercice_meta,
	 * 						gpointer           user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_TARGET_CHOOSER_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_POINTER, G_TYPE_POINTER );
}

/**
 * ofa_target_chooser_bin_new:
 * @getter: an #ofaIGetter.
 * @settings_prefix: the prefix of the user preferences in settings.
 *
 * Returns: a new #ofaTargetChooserBin instance.
 */
ofaTargetChooserBin *
ofa_target_chooser_bin_new( ofaIGetter *getter, const gchar *settings_prefix )
{
	ofaTargetChooserBin *bin;
	ofaTargetChooserBinPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	bin = g_object_new( OFA_TYPE_TARGET_CHOOSER_BIN, NULL );

	priv = ofa_target_chooser_bin_get_instance_private( bin );

	priv->getter = ofa_igetter_get_permanent_getter( getter );

	g_free( priv->settings_prefix );
	priv->settings_prefix = g_strdup( settings_prefix );

	setup_bin( bin );
	exercice_set_sensitive( bin );

	return( bin );
}

static void
setup_bin( ofaTargetChooserBin *self )
{
	ofaTargetChooserBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *parent, *btn;
	gchar *settings_prefix;

	priv = ofa_target_chooser_bin_get_instance_private( self );

	priv->hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "TargetChooserWindow" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-dossier-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->dossier_tview = ofa_dossier_treeview_new( priv->hub );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->dossier_tview ));
	settings_prefix = g_strdup_printf( "%s-%s", priv->settings_prefix, G_OBJECT_TYPE_NAME( priv->dossier_tview ));
	ofa_dossier_treeview_set_settings_key( priv->dossier_tview, settings_prefix );
	g_free( settings_prefix );
	ofa_dossier_treeview_setup_columns( priv->dossier_tview );
	ofa_dossier_treeview_set_show_all( priv->dossier_tview, FALSE );
	ofa_dossier_treeview_setup_store( priv->dossier_tview );
	g_signal_connect( priv->dossier_tview, "ofa-doschanged", G_CALLBACK( dossier_on_selection_changed ), self );

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-new-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect( btn, "clicked", G_CALLBACK( dossier_on_new ), self );
	priv->dossier_new_btn = btn;

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p2-period-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	settings_prefix = g_strdup_printf( "%s-%s", priv->settings_prefix, g_type_name( OFA_TYPE_EXERCICE_TREEVIEW ));
	priv->exercice_tview = ofa_exercice_treeview_new( priv->hub, settings_prefix );
	g_free( settings_prefix );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->exercice_tview ));
	g_signal_connect( priv->exercice_tview, "ofa-exechanged", G_CALLBACK( exercice_on_selection_changed ), self );

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p2-new-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect( btn, "clicked", G_CALLBACK( period_on_new ), self );
	priv->exercice_new_btn = btn;

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
dossier_on_selection_changed( ofaDossierTreeview *treeview, ofaIDBDossierMeta *meta, ofaIDBExerciceMeta *empty, ofaTargetChooserBin *self )
{
	ofaTargetChooserBinPrivate *priv;

	priv = ofa_target_chooser_bin_get_instance_private( self );

	if( !priv->block_dossier ){

		priv->dossier_meta = meta;
		priv->exercice_meta = NULL;

		ofa_exercice_treeview_set_dossier( priv->exercice_tview, meta );

		exercice_set_sensitive( self );

		g_signal_emit_by_name( self, "ofa-changed", priv->dossier_meta, priv->exercice_meta );
	}
}

static void
dossier_on_new( GtkButton *button, ofaTargetChooserBin *self )
{
	ofaTargetChooserBinPrivate *priv;
	GtkWindow *toplevel;
	gchar *settings_prefix;

	priv = ofa_target_chooser_bin_get_instance_private( self );

	set_collection_handler( self );
	ofa_dossier_treeview_set_selected( priv->dossier_tview, NULL );
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	priv->block_dossier = TRUE;
	settings_prefix = g_strdup_printf( "%s-ofaDossierNew", priv->settings_prefix );

	if( ofa_dossier_new_run_modal(
			priv->getter, toplevel, settings_prefix, FALSE, FALSE, FALSE, FALSE, &priv->dossier_meta )){

		set_new_object( self, G_OBJECT( priv->dossier_meta ));

		/* as a new dossier has been created, the dossier settings are
		 * updated, the dossier store is cleared and reloaded; we so lose
		 * the selection on the dossier treeview
		 * => just rely on the collection handler to reset the selection
		 */

	} else {
		priv->block_dossier = FALSE;
	}

	g_free( settings_prefix );
}

static void
exercice_set_sensitive( ofaTargetChooserBin *self )
{
	ofaTargetChooserBinPrivate *priv;

	priv = ofa_target_chooser_bin_get_instance_private( self );

	gtk_container_foreach( GTK_CONTAINER( priv->exercice_tview ), ( GtkCallback ) exercice_set_sensitive_widget, self );
	exercice_set_sensitive_widget( priv->exercice_new_btn, self );
}

static void
exercice_set_sensitive_widget( GtkWidget *widget, ofaTargetChooserBin *self )
{
	ofaTargetChooserBinPrivate *priv;
	gboolean have_dossier;

	priv = ofa_target_chooser_bin_get_instance_private( self );

	have_dossier = priv->dossier_meta && OFA_IS_IDBDOSSIER_META( priv->dossier_meta );
	gtk_widget_set_sensitive( widget, have_dossier );
}

static void
exercice_on_selection_changed( ofaExerciceTreeview *treeview, ofaIDBExerciceMeta *meta, ofaTargetChooserBin *self )
{
	ofaTargetChooserBinPrivate *priv;

	priv = ofa_target_chooser_bin_get_instance_private( self );

	if( !priv->block_exercice ){

		priv->exercice_meta = meta;

		g_signal_emit_by_name( self, "ofa-changed", priv->dossier_meta, priv->exercice_meta );
	}
}

static void
period_on_new( GtkButton *button, ofaTargetChooserBin *self )
{
	ofaTargetChooserBinPrivate *priv;
	GtkWindow *toplevel;
	gchar *settings_prefix;

	priv = ofa_target_chooser_bin_get_instance_private( self );

	set_collection_handler( self );
	ofa_exercice_treeview_set_selected( priv->exercice_tview, NULL );
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	priv->block_dossier = TRUE;
	priv->block_exercice = TRUE;
	settings_prefix = g_strdup_printf( "%s-ofaExerciceNew", priv->settings_prefix );

	if( ofa_exercice_new_run_modal( priv->getter, toplevel, settings_prefix, priv->dossier_meta, FALSE, FALSE, &priv->exercice_meta )){

		set_new_object( self, G_OBJECT( priv->exercice_meta ));

		/* as a new exercice has been created, the dossier settings are
		 * updated, the dossier store is cleared and reloaded; we so lose
		 * the selection on the dossier treeview, and thus are unabled
		 * to select the newly created exercice in exercice treeview
		 * => just rely on the collection handler to reset the selections
		 */

	} else {
		priv->block_dossier = FALSE;
		priv->block_exercice = FALSE;
	}

	g_free( settings_prefix );
}

#if 0
/**
 * ofa_target_chooser_bin_get_selected:
 * @view: this #ofaTargetChooserBin instance.
 * @meta: [out]: the placeholder for the selected #ofaIDBDossierMeta object.
 * @period: [out]: the placeholder for the selected #ofaIDBExerciceMeta object.
 *
 * Returns: %TRUE if there is a selection.
 */
gboolean
ofa_target_chooser_bin_get_selected( ofaTargetChooserBin *bin, ofaIDBDossierMeta **meta, ofaIDBExerciceMeta **period )
{
	ofaTargetChooserBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_TARGET_CHOOSER_BIN( bin ), FALSE );
	g_return_val_if_fail( meta, FALSE );
	g_return_val_if_fail( period, FALSE );

	priv = ofa_target_chooser_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	*meta = priv->dossier_meta;
	ofa_exercice_treeview_get_selected( priv->exercice_tview, period );

	return( *meta && *period );
}
#endif

/*
 * Before updating the dossier collection, set a handler to reset the
 * current selection after a dossier store reload
 *
 * The handler is set when we need id, and removed on dispose().
 */
static void
set_collection_handler( ofaTargetChooserBin *self )
{
	ofaTargetChooserBinPrivate *priv;
	ofaDossierCollection *collection;

	priv = ofa_target_chooser_bin_get_instance_private( self );

	if( !priv->collection_handler ){
		collection = ofa_hub_get_dossier_collection( priv->hub );
		priv->collection_handler = g_signal_connect( collection, "changed", G_CALLBACK( on_collection_changed ), self );
	}
}

static void
remove_collection_handler( ofaTargetChooserBin *self )
{
	ofaTargetChooserBinPrivate *priv;
	ofaDossierCollection *collection;

	priv = ofa_target_chooser_bin_get_instance_private( self );

	if( priv->collection_handler ){
		collection = ofa_hub_get_dossier_collection( priv->hub );
		g_signal_handler_disconnect( collection, priv->collection_handler );
	}
}

static void
on_collection_changed( ofaDossierCollection *collection, guint count, ofaTargetChooserBin *self )
{
	static const gchar *thisfn = "ofa_target_chooser_bin_on_collection_changed";
	ofaTargetChooserBinPrivate *priv;
	const gchar *dossier_name;
	ofaIDBDossierMeta *dossier_meta;

	priv = ofa_target_chooser_bin_get_instance_private( self );

	g_debug( "%s: dossier=%p, exercice=%p", thisfn, priv->dossier_meta, priv->exercice_meta );

	dossier_meta = priv->dossier_meta;

	priv->block_dossier = FALSE;
	priv->block_exercice = FALSE;

	dossier_name = dossier_meta ? ofa_idbdossier_meta_get_dossier_name( dossier_meta ) : NULL;
	ofa_dossier_treeview_set_selected( priv->dossier_tview, dossier_name );
}

/**
 * ofa_target_chooser_bin_is_new_dossier:
 * @bin: this #ofaTargetChooserBin instance.
 * @dossier_meta: the requested #ofaIDBDossierMeta object.
 *
 * Returns: %TRUE if the @dossier_meta has just been created.
 */
gboolean
ofa_target_chooser_bin_is_new_dossier( ofaTargetChooserBin *bin, ofaIDBDossierMeta *dossier_meta )
{
	ofaTargetChooserBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_TARGET_CHOOSER_BIN( bin ), FALSE );
	g_return_val_if_fail( dossier_meta && OFA_IS_IDBDOSSIER_META( dossier_meta ), FALSE );

	priv = ofa_target_chooser_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( get_is_new_object( bin, G_OBJECT( dossier_meta )));
}

/**
 * ofa_target_chooser_bin_is_new_dossier:
 * @bin: this #ofaTargetChooserBin instance.
 * @exercice_meta: the requested #ofaIDBExerciceMeta object.
 *
 * Returns: %TRUE if the @exercice_meta has just been created.
 */
gboolean
ofa_target_chooser_bin_is_new_exercice( ofaTargetChooserBin *bin, ofaIDBExerciceMeta *exercice_meta )
{
	ofaTargetChooserBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_TARGET_CHOOSER_BIN( bin ), FALSE );
	g_return_val_if_fail( exercice_meta && OFA_IS_IDBEXERCICE_META( exercice_meta ), FALSE );

	priv = ofa_target_chooser_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( get_is_new_object( bin, G_OBJECT( exercice_meta )));
}

static void
set_new_object( ofaTargetChooserBin *self, GObject *object )
{
	ofaTargetChooserBinPrivate *priv;

	priv = ofa_target_chooser_bin_get_instance_private( self );

	priv->new_list = g_list_prepend( priv->new_list, object );
}

static gboolean
get_is_new_object( ofaTargetChooserBin *self, GObject *object )
{
	ofaTargetChooserBinPrivate *priv;
	GList *find;

	priv = ofa_target_chooser_bin_get_instance_private( self );

	find = g_list_find( priv->new_list, object );

	return( find != NULL );
}

/**
 * ofa_target_chooser_bin_set_selected:
 * @bin: this #ofaTargetChooserBin instance.
 * @dossier_meta: [allow-none]: the requested #ofaIDBDossierMeta object.
 * @exercice_meta: [allow-none]: the requested #ofaIDBExerciceMeta object.
 *
 * Set the selection.
 */
void
ofa_target_chooser_bin_set_selected( ofaTargetChooserBin *bin,
										ofaIDBDossierMeta *dossier_meta, ofaIDBExerciceMeta *exercice_meta )
{
	static const gchar *thisfn = "ofa_target_chooser_bin_set_selected";
	ofaTargetChooserBinPrivate *priv;
	const gchar *dossier_name;

	g_debug( "%s: bin=%p, dossier_meta=%p, exercice_meta=%p",
			thisfn, ( void * ) bin, ( void * ) dossier_meta, ( void * ) exercice_meta );

	g_return_if_fail( bin && OFA_IS_TARGET_CHOOSER_BIN( bin ));
	g_return_if_fail( !dossier_meta || OFA_IS_IDBDOSSIER_META( dossier_meta ));
	g_return_if_fail( !exercice_meta || OFA_IS_IDBEXERCICE_META( exercice_meta ));

	priv = ofa_target_chooser_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	dossier_name = dossier_meta ? ofa_idbdossier_meta_get_dossier_name( dossier_meta ) : NULL;
	ofa_dossier_treeview_set_selected( priv->dossier_tview, dossier_name );
	ofa_exercice_treeview_set_selected( priv->exercice_tview, exercice_meta );
}
