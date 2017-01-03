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

#include "ui/ofa-dossier-actions-bin.h"

/* private instance data
 */
typedef struct {
	gboolean      dispose_has_run;

	/* initialization
	 */
	ofaHub       *hub;
	gchar        *settings_prefix;
	guint         rule;

	/* UI
	 */
	GtkSizeGroup *group0;
	GtkWidget    *open_btn;
	GtkWidget    *standard_btn;

	/* runtime
	 */
	gboolean      do_open;
}
	ofaDossierActionsBinPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-dossier-actions-bin.ui";

static void setup_bin( ofaDossierActionsBin *self );
static void on_open_toggled( GtkToggleButton *button, ofaDossierActionsBin *self );
static void changed_composite( ofaDossierActionsBin *self );
static void read_settings( ofaDossierActionsBin *self );
static void write_settings( ofaDossierActionsBin *self );

G_DEFINE_TYPE_EXTENDED( ofaDossierActionsBin, ofa_dossier_actions_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaDossierActionsBin ))

static void
dossier_actions_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_actions_bin_finalize";
	ofaDossierActionsBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_ACTIONS_BIN( instance ));

	/* free data members here */
	priv = ofa_dossier_actions_bin_get_instance_private( OFA_DOSSIER_ACTIONS_BIN( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_actions_bin_parent_class )->finalize( instance );
}

static void
dossier_actions_bin_dispose( GObject *instance )
{
	ofaDossierActionsBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_ACTIONS_BIN( instance ));

	priv = ofa_dossier_actions_bin_get_instance_private( OFA_DOSSIER_ACTIONS_BIN( instance ));

	if( !priv->dispose_has_run ){

		write_settings( OFA_DOSSIER_ACTIONS_BIN( instance ));

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_actions_bin_parent_class )->dispose( instance );
}

static void
ofa_dossier_actions_bin_init( ofaDossierActionsBin *self )
{
	static const gchar *thisfn = "ofa_dossier_actions_bin_instance_init";
	ofaDossierActionsBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_ACTIONS_BIN( self ));

	priv = ofa_dossier_actions_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_dossier_actions_bin_class_init( ofaDossierActionsBinClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_actions_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_actions_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_actions_bin_finalize;

	/**
	 * ofaDossierActionsBin::ofa-changed:
	 *
	 * This signal is sent on the #ofaDossierActionsBin when any of the
	 * underlying information is changed.
	 *
	 * There is no argument.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDossierActionsBin *bin,
	 * 						gpointer            user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_DOSSIER_ACTIONS_BIN,
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
 * ofa_dossier_actions_bin_new:
 * @hub: the #ofaHub object of the application.
 * @settings_prefix: the prefix of the key in user settings.
 * @rule: the usage of this widget.
 *
 * Returns: a newly defined composite widget which aggregates dossier
 * name, DBMS provider, connection informations and root credentials.
 */
ofaDossierActionsBin *
ofa_dossier_actions_bin_new( ofaHub *hub, const gchar *settings_prefix, guint rule )
{
	static const gchar *thisfn = "ofa_dossier_actions_bin_new";
	ofaDossierActionsBin *bin;
	ofaDossierActionsBinPrivate *priv;

	g_debug( "%s: hub=%p, settings_prefix=%s, guint=%u",
			thisfn, ( void * ) hub, settings_prefix, rule );

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( my_strlen( settings_prefix ), NULL );

	bin = g_object_new( OFA_TYPE_DOSSIER_ACTIONS_BIN, NULL );

	priv = ofa_dossier_actions_bin_get_instance_private( bin );

	priv->hub = hub;
	priv->rule = rule;

	g_free( priv->settings_prefix );
	priv->settings_prefix = g_strdup( settings_prefix );

	setup_bin( bin );
	read_settings( bin );

	return( bin );
}

static void
setup_bin( ofaDossierActionsBin *self )
{
	ofaDossierActionsBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *btn;

	priv = ofa_dossier_actions_bin_get_instance_private( self );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "dab-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "dab-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	/* apply standard actions */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "dab-apply-btn" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	priv->standard_btn = btn;

	/* open the dossier */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "dab-open-btn" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	g_signal_connect( btn, "toggled", G_CALLBACK( on_open_toggled ), self );
	priv->open_btn = btn;
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( btn ), FALSE );
	on_open_toggled( GTK_TOGGLE_BUTTON( btn ), self );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

/**
 * ofa_dossier_actions_bin_get_size_group:
 * @bin: this #ofaDossierActionsBin instance.
 * @column: the desired column.
 *
 * Returns: the #GtkSizeGroup which handles the desired @column.
 */
GtkSizeGroup *
ofa_dossier_actions_bin_get_size_group( ofaDossierActionsBin *bin, guint column )
{
	static const gchar *thisfn = "ofa_dossier_actions_bin_get_size_group";
	ofaDossierActionsBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_DOSSIER_ACTIONS_BIN( bin ), NULL );

	priv = ofa_dossier_actions_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}

	g_warning( "%s: unmanaged column=%u", thisfn, column );
	return( NULL );
}

static void
on_open_toggled( GtkToggleButton *button, ofaDossierActionsBin *self )
{
	ofaDossierActionsBinPrivate *priv;

	priv = ofa_dossier_actions_bin_get_instance_private( self );

	priv->do_open = gtk_toggle_button_get_active( button );

	gtk_widget_set_sensitive( priv->standard_btn, priv->do_open );

	changed_composite( self );
}

static void
changed_composite( ofaDossierActionsBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_dossier_actions_bin_is_valid:
 * @bin: this #ofaDossierActionsBin instance.
 * @error_message: [allow-none]: the error message to be displayed.
 *
 * The dialog is always valid.
 */
gboolean
ofa_dossier_actions_bin_is_valid( ofaDossierActionsBin *bin, gchar **error_message )
{
	return( TRUE );
}

/**
 * ofa_dossier_actions_bin_get_open_on_create:
 * @bin: this #ofaDossierActionsBin instance.
 *
 * Returns: %TRUE if the dossier should be opened after creation.
 */
gboolean
ofa_dossier_actions_bin_get_open_on_create( ofaDossierActionsBin *bin )
{
	ofaDossierActionsBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_DOSSIER_ACTIONS_BIN( bin ), FALSE );

	priv = ofa_dossier_actions_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->do_open );
}

/*
 * settings are: "open_on_creation(b); apply_standard_actions(b);"
 */
static void
read_settings( ofaDossierActionsBin *self )
{
	ofaDossierActionsBinPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;
	gchar *key;

	priv = ofa_dossier_actions_bin_get_instance_private( self );

	settings = ofa_hub_get_user_settings( priv->hub );
	key = g_strdup_printf( "%s-dossier-actions", priv->settings_prefix );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, key );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->open_btn ), my_utils_boolean_from_str( cstr ));
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->standard_btn ), my_utils_boolean_from_str( cstr ));
	}

	my_isettings_free_string_list( settings, strlist );
	g_free( key );
}

static void
write_settings( ofaDossierActionsBin *self )
{
	ofaDossierActionsBinPrivate *priv;
	myISettings *settings;
	gchar *key, *str;

	priv = ofa_dossier_actions_bin_get_instance_private( self );

	str = g_strdup_printf( "%s;%s;",
				gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->open_btn )) ? "True":"False",
				gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->standard_btn )) ? "True":"False" );

	settings = ofa_hub_get_user_settings( priv->hub );
	key = g_strdup_printf( "%s-dossier-actions", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( key );
	g_free( str );
}