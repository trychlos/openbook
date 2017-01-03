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

#include "my/my-date.h"
#include "my/my-date-editable.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-preferences.h"

#include "ui/ofa-application.h"
#include "ui/ofa-exercice-meta-bin.h"

/* private instance data
 */
typedef struct {
	gboolean            dispose_has_run;

	/* initialization
	 */
	ofaHub             *hub;
	gchar              *settings_prefix;
	guint               rule;

	/* UI
	 */
	GtkSizeGroup       *group0;

	/* runtime data
	 */
	GDate               begin;
	GDate               end;
	gboolean            is_current;

	/* on apply
	 */
	ofaIDBExerciceMeta *exercice_meta;
}
	ofaExerciceMetaBinPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-exercice-meta-bin.ui";

static void setup_bin( ofaExerciceMetaBin *self );
static void on_begin_changed( GtkEditable *editable, ofaExerciceMetaBin *self );
static void on_end_changed( GtkEditable *editable, ofaExerciceMetaBin *self );
static void on_current_toggled( GtkToggleButton *button, ofaExerciceMetaBin *self );
static void changed_composite( ofaExerciceMetaBin *self );

G_DEFINE_TYPE_EXTENDED( ofaExerciceMetaBin, ofa_exercice_meta_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaExerciceMetaBin ))

static void
exercice_meta_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_exercice_meta_bin_finalize";
	ofaExerciceMetaBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EXERCICE_META_BIN( instance ));

	/* free data members here */
	priv = ofa_exercice_meta_bin_get_instance_private( OFA_EXERCICE_META_BIN( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exercice_meta_bin_parent_class )->finalize( instance );
}

static void
exercice_meta_bin_dispose( GObject *instance )
{
	ofaExerciceMetaBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_EXERCICE_META_BIN( instance ));

	priv = ofa_exercice_meta_bin_get_instance_private( OFA_EXERCICE_META_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exercice_meta_bin_parent_class )->dispose( instance );
}

static void
ofa_exercice_meta_bin_init( ofaExerciceMetaBin *self )
{
	static const gchar *thisfn = "ofa_exercice_meta_bin_instance_init";
	ofaExerciceMetaBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXERCICE_META_BIN( self ));

	priv = ofa_exercice_meta_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	my_date_clear( &priv->begin );
	my_date_clear( &priv->end );
}

static void
ofa_exercice_meta_bin_class_init( ofaExerciceMetaBinClass *klass )
{
	static const gchar *thisfn = "ofa_exercice_meta_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = exercice_meta_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = exercice_meta_bin_finalize;

	/**
	 * ofaExerciceMetaBin::ofa-changed:
	 *
	 * This signal is sent on the #ofaExerciceMetaBin when any of the
	 * underlying information is changed. This includes the exercice
	 * name and the DBMS provider.
	 *
	 * There is no argument.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaExerciceMetaBin *bin,
	 * 						gpointer          user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_EXERCICE_META_BIN,
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
 * ofa_exercice_meta_bin_new:
 * @hub: the #ofaHub object of the application.
 * @settings_prefix: the prefix of the key in user settings.
 * @rule: the usage of this widget.
 *
 * Returns: a newly defined composite widget which aggregates exercice
 * meta datas: name and DBMS provider.
 */
ofaExerciceMetaBin *
ofa_exercice_meta_bin_new( ofaHub *hub, const gchar *settings_prefix, guint rule )
{
	static const gchar *thisfn = "ofa_exercice_meta_bin_new";
	ofaExerciceMetaBin *self;
	ofaExerciceMetaBinPrivate *priv;

	g_debug( "%s: hub=%p, settings_prefix=%s, guint=%u",
			thisfn, ( void * ) hub, settings_prefix, rule );

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( my_strlen( settings_prefix ), NULL );

	self = g_object_new( OFA_TYPE_EXERCICE_META_BIN, NULL );

	priv = ofa_exercice_meta_bin_get_instance_private( self );

	priv->hub = hub;
	priv->rule = rule;

	g_free( priv->settings_prefix );
	priv->settings_prefix = g_strdup( settings_prefix );

	setup_bin( self );

	return( self );
}

static void
setup_bin( ofaExerciceMetaBin *self )
{
	ofaExerciceMetaBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *entry, *label, *btn;

	priv = ofa_exercice_meta_bin_get_instance_private( self );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "emb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "emb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	/* beginning date */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "emb-begin-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_begin_changed ), self );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "emb-begin-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_editable_set_format( GTK_EDITABLE( entry ), ofa_prefs_date_display( priv->hub ));
	my_date_editable_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check( priv->hub ));
	my_date_editable_set_date( GTK_EDITABLE( entry ), &priv->begin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "emb-begin-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* ending date */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "emb-end-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_end_changed ), self );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "emb-end-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_editable_set_format( GTK_EDITABLE( entry ), ofa_prefs_date_display( priv->hub ));
	my_date_editable_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check( priv->hub ));
	my_date_editable_set_date( GTK_EDITABLE( entry ), &priv->end );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "emb-end-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* current flag
	 * depending of the specified rule, the flag may be initially set */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "emb-current-btn" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	g_signal_connect( btn, "toggled", G_CALLBACK( on_current_toggled ), self );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( btn ), FALSE );

	switch( priv->rule ){

		/* when defining a new dossier, the new exercice is current
		 * and this is mandatory */
		case HUB_RULE_DOSSIER_NEW:
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( btn ), TRUE );
			gtk_widget_set_sensitive( btn, FALSE );
			break;
	}

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

/**
 * ofa_exercice_meta_bin_get_size_group:
 * @bin: this #ofaExerciceMetaBin instance.
 * @column: the desire column.
 *
 * Returns: the #GtkSizeGroup which handles the desired @column.
 */
GtkSizeGroup *
ofa_exercice_meta_bin_get_size_group( ofaExerciceMetaBin *bin, guint column )
{
	static const gchar *thisfn = "ofa_exercice_meta_bin_get_size_group";
	ofaExerciceMetaBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_EXERCICE_META_BIN( bin ), NULL );

	priv = ofa_exercice_meta_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}

	g_warning( "%s: unmanaged column=%u", thisfn, column );
	return( NULL );
}

static void
on_begin_changed( GtkEditable *editable, ofaExerciceMetaBin *self )
{
	ofaExerciceMetaBinPrivate *priv;

	priv = ofa_exercice_meta_bin_get_instance_private( self );

	my_date_set_from_date( &priv->begin, my_date_editable_get_date( editable, NULL ));

	changed_composite( self );
}

static void
on_end_changed( GtkEditable *editable, ofaExerciceMetaBin *self )
{
	ofaExerciceMetaBinPrivate *priv;

	priv = ofa_exercice_meta_bin_get_instance_private( self );

	my_date_set_from_date( &priv->end, my_date_editable_get_date( editable, NULL ));

	changed_composite( self );
}

static void
on_current_toggled( GtkToggleButton *button, ofaExerciceMetaBin *self )
{
	ofaExerciceMetaBinPrivate *priv;

	priv = ofa_exercice_meta_bin_get_instance_private( self );

	priv->is_current = gtk_toggle_button_get_active( button );

	changed_composite( self );
}

static void
changed_composite( ofaExerciceMetaBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_exercice_meta_bin_is_valid:
 * @bin: this #ofaExerciceMetaBin instance.
 * @error_message: [allow-none]: the error message to be displayed.
 *
 * The dialog is always valid.
 */
gboolean
ofa_exercice_meta_bin_is_valid( ofaExerciceMetaBin *bin, gchar **error_message )
{
	return( TRUE );
}

/**
 * ofa_exercice_meta_bin_apply:
 * @bin: this #ofaExerciceMetaBin instance.
 * @dossier_meta: the #ofaIDBDossierMeta dossier.
 *
 * Returns: %TRUE.
 */
gboolean
ofa_exercice_meta_bin_apply( ofaExerciceMetaBin *bin, ofaIDBDossierMeta *dossier_meta )
{
	static const gchar *thisfn = "ofa_exercice_meta_bin_apply";
	ofaExerciceMetaBinPrivate *priv;

	g_debug( "%s: bin=%p, dossier_meta=%p", thisfn, ( void * ) bin, ( void * ) dossier_meta );

	g_return_val_if_fail( bin && OFA_IS_EXERCICE_META_BIN( bin ), FALSE );
	g_return_val_if_fail( dossier_meta && OFA_IS_IDBDOSSIER_META( dossier_meta ), FALSE );

	priv = ofa_exercice_meta_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	switch( priv->rule ){
		case HUB_RULE_DOSSIER_NEW:
			priv->exercice_meta = ofa_idbdossier_meta_new_exercice_meta( dossier_meta );
			ofa_idbexercice_meta_set_begin_date( priv->exercice_meta, &priv->begin );
			ofa_idbexercice_meta_set_end_date( priv->exercice_meta, &priv->end );
			ofa_idbexercice_meta_set_current( priv->exercice_meta, priv->is_current );
			break;
	}

	return( TRUE );
}

/**
 * ofa_exercice_meta_bin_get_exercice_meta:
 * @bin: this #ofaExerciceMetaBin instance.
 *
 * Returns: the #ofaIDBExerciceMeta instance allocated on apply.
 */
ofaIDBExerciceMeta *
ofa_exercice_meta_bin_get_exercice_meta( ofaExerciceMetaBin *bin )
{
	ofaExerciceMetaBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_EXERCICE_META_BIN( bin ), NULL );

	priv = ofa_exercice_meta_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->exercice_meta );
}

/**
 * ofa_exercice_meta_bin_get_begin_date:
 * @bin: this #ofaExerciceMetaBin instance.
 *
 * Returns: the beginning date.
 */
const GDate *
ofa_exercice_meta_bin_get_begin_date( ofaExerciceMetaBin *bin )
{
	ofaExerciceMetaBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_EXERCICE_META_BIN( bin ), NULL );

	priv = ofa_exercice_meta_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( &priv->begin );
}

/**
 * ofa_exercice_meta_bin_get_end_date:
 * @bin: this #ofaExerciceMetaBin instance.
 *
 * Returns: the ending date.
 */
const GDate *
ofa_exercice_meta_bin_get_end_date( ofaExerciceMetaBin *bin )
{
	ofaExerciceMetaBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_EXERCICE_META_BIN( bin ), NULL );

	priv = ofa_exercice_meta_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( &priv->end );
}

/**
 * ofa_exercice_meta_bin_get_is_current:
 * @bin: this #ofaExerciceMetaBin instance.
 *
 * Returns: %TRUE if the exercice is current.
 */
gboolean
ofa_exercice_meta_bin_get_is_current( ofaExerciceMetaBin *bin )
{
	ofaExerciceMetaBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_EXERCICE_META_BIN( bin ), FALSE );

	priv = ofa_exercice_meta_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->is_current );
}