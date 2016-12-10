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

#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"

#include "ui/ofa-dossier-treeview.h"
#include "ui/ofa-exercice-treeview.h"
#include "ui/ofa-target-chooser-bin.h"

/* private instance data
 */
typedef struct {
	gboolean             dispose_has_run;

	/* runtime data
	 */
	gchar               *settings_prefix;
	ofaIDBDossierMeta   *meta;

	/* UI
	 */
	ofaDossierTreeview  *dossier_treeview;
	GtkWidget           *dossier_new_btn;
	ofaExerciceTreeview *period_treeview;
	GtkWidget           *period_new_btn;
}
	ofaTargetChooserBinPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-target-chooser-bin.ui";

static void setup_bin( ofaTargetChooserBin *self );
static void dossier_on_selection_changed( ofaDossierTreeview *treeview, ofaIDBDossierMeta *meta, ofaIDBExerciceMeta *empty, ofaTargetChooserBin *self );
static void dossier_on_new( GtkButton *button, ofaTargetChooserBin *self );
static void period_on_new( GtkButton *button, ofaTargetChooserBin *self );

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
}

static void
ofa_target_chooser_bin_class_init( ofaTargetChooserBinClass *klass )
{
	static const gchar *thisfn = "ofa_target_chooser_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = target_chooser_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = target_chooser_bin_finalize;
}

/**
 * ofa_target_chooser_bin_new:
 * @settings_prefix: the prefix of the user preferences in settings.
 *
 * Returns: a new #ofaTargetChooserBin instance.
 */
ofaTargetChooserBin *
ofa_target_chooser_bin_new( const gchar *settings_prefix )
{
	ofaTargetChooserBin *bin;
	ofaTargetChooserBinPrivate *priv;

	bin = g_object_new( OFA_TYPE_TARGET_CHOOSER_BIN, NULL );

	priv = ofa_target_chooser_bin_get_instance_private( bin );

	g_free( priv->settings_prefix );
	priv->settings_prefix = g_strdup( settings_prefix );

	setup_bin( bin );

	return( bin );
}

static void
setup_bin( ofaTargetChooserBin *self )
{
	ofaTargetChooserBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *parent, *btn;

	priv = ofa_target_chooser_bin_get_instance_private( self );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "TargetChooserWindow" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-dossier-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->dossier_treeview = ofa_dossier_treeview_new();
	ofa_dossier_treeview_set_settings_key( priv->dossier_treeview, priv->settings_prefix );
	ofa_dossier_treeview_setup_columns( priv->dossier_treeview );
	ofa_dossier_treeview_set_show_all( priv->dossier_treeview, FALSE );
	ofa_dossier_treeview_setup_store( priv->dossier_treeview );
	g_signal_connect( priv->dossier_treeview, "ofa-doschanged", G_CALLBACK( dossier_on_selection_changed ), self );

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-new-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect( btn, "clicked", G_CALLBACK( dossier_on_new ), self );
	priv->dossier_new_btn = btn;

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p2-period-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->period_treeview = ofa_exercice_treeview_new( priv->settings_prefix );

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p2-new-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect( btn, "clicked", G_CALLBACK( period_on_new ), self );
	priv->period_new_btn = btn;

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
dossier_on_selection_changed( ofaDossierTreeview *treeview, ofaIDBDossierMeta *meta, ofaIDBExerciceMeta *empty, ofaTargetChooserBin *self )
{
	ofaTargetChooserBinPrivate *priv;

	priv = ofa_target_chooser_bin_get_instance_private( self );

	priv->meta = meta;
	ofa_exercice_treeview_set_dossier( priv->period_treeview, meta );
}

static void
dossier_on_new( GtkButton *button, ofaTargetChooserBin *self )
{

}

static void
period_on_new( GtkButton *button, ofaTargetChooserBin *self )
{

}

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

	*meta = priv->meta;
	ofa_exercice_treeview_get_selected( priv->period_treeview, period );

	return( *meta && *period );
}
