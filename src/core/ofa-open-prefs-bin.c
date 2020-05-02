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

#include "core/ofa-open-prefs-bin.h"

/* private instance data
 */
typedef struct {
	gboolean      dispose_has_run;

	/* initialization
	 */
	ofaOpenPrefs *prefs;

	/* UI
	 */
	GtkWidget    *notes_btn;
	GtkWidget    *nonempty_btn;
	GtkWidget    *properties_btn;
	GtkWidget    *balances_btn;
	GtkWidget    *integrity_btn;
}
	ofaOpenPrefsBinPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-open-prefs-bin.ui";

static void  setup_bin( ofaOpenPrefsBin *self );
static void  setup_data( ofaOpenPrefsBin *self );
static void  on_display_notes_toggled( GtkToggleButton *button, ofaOpenPrefsBin *self );
static void  on_non_empty_notes_toggled( GtkToggleButton *button, ofaOpenPrefsBin *self );
static void  on_display_properties_toggled( GtkToggleButton *button, ofaOpenPrefsBin *self );
static void  on_check_balances_toggled( GtkToggleButton *button, ofaOpenPrefsBin *self );
static void  on_check_integrity_toggled( GtkToggleButton *button, ofaOpenPrefsBin *self );
static void  on_bin_changed( ofaOpenPrefsBin *self );
static void  ibin_iface_init( myIBinInterface *iface );
static guint ibin_get_interface_version( void );
static void  ibin_apply( myIBin *instance );

G_DEFINE_TYPE_EXTENDED( ofaOpenPrefsBin, ofa_open_prefs_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaOpenPrefsBin )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IBIN, ibin_iface_init ))

static void
open_prefs_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_open_prefs_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_OPEN_PREFS_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_open_prefs_bin_parent_class )->finalize( instance );
}

static void
open_prefs_bin_dispose( GObject *instance )
{
	ofaOpenPrefsBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_OPEN_PREFS_BIN( instance ));

	priv = ofa_open_prefs_bin_get_instance_private( OFA_OPEN_PREFS_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->prefs );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_open_prefs_bin_parent_class )->dispose( instance );
}

static void
ofa_open_prefs_bin_init( ofaOpenPrefsBin *self )
{
	static const gchar *thisfn = "ofa_open_prefs_bin_instance_init";
	ofaOpenPrefsBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_OPEN_PREFS_BIN( self ));

	priv = ofa_open_prefs_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_open_prefs_bin_class_init( ofaOpenPrefsBinClass *klass )
{
	static const gchar *thisfn = "ofa_open_prefs_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = open_prefs_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = open_prefs_bin_finalize;
}

/**
 * ofa_open_prefs_bin_new:
 * @prefs: a #ofaOpenPrefs object.
 *
 * Returns: a new #ofaOpenPrefsBin object.
 */
ofaOpenPrefsBin *
ofa_open_prefs_bin_new( ofaOpenPrefs *prefs )
{
	ofaOpenPrefsBin *bin;
	ofaOpenPrefsBinPrivate *priv;

	g_return_val_if_fail( prefs && OFA_IS_OPEN_PREFS( prefs ), NULL );

	bin = g_object_new( OFA_TYPE_OPEN_PREFS_BIN, NULL );

	priv = ofa_open_prefs_bin_get_instance_private( bin );

	priv->prefs = g_object_ref( prefs );

	setup_bin( bin );
	setup_data( bin );

	return( bin );
}

static void
setup_bin( ofaOpenPrefsBin *self )
{
	ofaOpenPrefsBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *btn;

	priv = ofa_open_prefs_bin_get_instance_private( self );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "top-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top-widget" );

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-notes" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	g_signal_connect( btn, "toggled", G_CALLBACK( on_display_notes_toggled ), self );
	priv->notes_btn = btn;

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-nonempty" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	g_signal_connect( btn, "toggled", G_CALLBACK( on_non_empty_notes_toggled ), self );
	priv->nonempty_btn = btn;

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-properties" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	g_signal_connect( btn, "toggled", G_CALLBACK( on_display_properties_toggled ), self );
	priv->properties_btn = btn;

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-balance" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	g_signal_connect( btn, "toggled", G_CALLBACK( on_check_balances_toggled ), self );
	priv->balances_btn = btn;

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-integrity" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	g_signal_connect( btn, "toggled", G_CALLBACK( on_check_integrity_toggled ), self );
	priv->integrity_btn = btn;

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
setup_data( ofaOpenPrefsBin *self )
{
	ofaOpenPrefsBinPrivate *priv;
	gboolean active;

	priv = ofa_open_prefs_bin_get_instance_private( self );

	active = ofa_open_prefs_get_display_notes( priv->prefs );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->notes_btn ), TRUE );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->notes_btn ), active );

	active = ofa_open_prefs_get_non_empty_notes( priv->prefs );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->nonempty_btn ), active );

	active = ofa_open_prefs_get_display_properties( priv->prefs );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->properties_btn ), active );

	active = ofa_open_prefs_get_check_balances( priv->prefs );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->balances_btn ), active );

	active = ofa_open_prefs_get_check_integrity( priv->prefs );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->integrity_btn ), active );
}

static void
on_display_notes_toggled( GtkToggleButton *button, ofaOpenPrefsBin *self )
{
	ofaOpenPrefsBinPrivate *priv;
	gboolean active;

	priv = ofa_open_prefs_bin_get_instance_private( self );

	active = gtk_toggle_button_get_active( button );
	ofa_open_prefs_set_display_notes( priv->prefs, active );

	gtk_widget_set_sensitive( priv->nonempty_btn, active );

	on_bin_changed( self );
}

static void
on_non_empty_notes_toggled( GtkToggleButton *button, ofaOpenPrefsBin *self )
{
	ofaOpenPrefsBinPrivate *priv;
	gboolean active;

	priv = ofa_open_prefs_bin_get_instance_private( self );

	active = gtk_toggle_button_get_active( button );
	ofa_open_prefs_set_non_empty_notes( priv->prefs, active );

	on_bin_changed( self );
}

static void
on_display_properties_toggled( GtkToggleButton *button, ofaOpenPrefsBin *self )
{
	ofaOpenPrefsBinPrivate *priv;
	gboolean active;

	priv = ofa_open_prefs_bin_get_instance_private( self );

	active = gtk_toggle_button_get_active( button );
	ofa_open_prefs_set_display_properties( priv->prefs, active );

	on_bin_changed( self );
}

static void
on_check_balances_toggled( GtkToggleButton *button, ofaOpenPrefsBin *self )
{
	ofaOpenPrefsBinPrivate *priv;
	gboolean active;

	priv = ofa_open_prefs_bin_get_instance_private( self );

	active = gtk_toggle_button_get_active( button );
	ofa_open_prefs_set_check_balances( priv->prefs, active );

	on_bin_changed( self );
}

static void
on_check_integrity_toggled( GtkToggleButton *button, ofaOpenPrefsBin *self )
{
	ofaOpenPrefsBinPrivate *priv;
	gboolean active;

	priv = ofa_open_prefs_bin_get_instance_private( self );

	active = gtk_toggle_button_get_active( button );
	ofa_open_prefs_set_check_integrity( priv->prefs, active );

	on_bin_changed( self );
}

static void
on_bin_changed( ofaOpenPrefsBin *self )
{
	g_signal_emit_by_name( self, "my-ibin-changed" );
}

/*
 * myIBin interface management
 */
static void
ibin_iface_init( myIBinInterface *iface )
{
	static const gchar *thisfn = "ofa_open_prefs_bin_ibin_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = ibin_get_interface_version;
	iface->apply = ibin_apply;
}

static guint
ibin_get_interface_version( void )
{
	return( 1 );
}

void
ibin_apply( myIBin *instance )
{
	ofaOpenPrefsBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_OPEN_PREFS_BIN( instance ));

	priv = ofa_open_prefs_bin_get_instance_private( OFA_OPEN_PREFS_BIN( instance ));

	g_return_if_fail( !priv->dispose_has_run );

	ofa_open_prefs_apply_settings( priv->prefs );
}
