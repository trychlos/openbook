/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
 *
 * Open Freelance Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Freelance Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Freelance Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "my/my-utils.h"

#include "core/ofa-open-prefs-bin.h"

/* private instance data
 */
struct _ofaOpenPrefsBinPrivate {
	gboolean      dispose_has_run;

	/* UI
	 */
	GtkWidget    *notes_btn;
	GtkWidget    *nonempty_btn;
	GtkWidget    *properties_btn;
	GtkWidget    *balances_btn;
	GtkWidget    *integrity_btn;
};

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-open-prefs-bin.ui";

static void     setup_bin( ofaOpenPrefsBin *self );
static void     on_display_notes_toggled( GtkToggleButton *button, ofaOpenPrefsBin *self );

G_DEFINE_TYPE_EXTENDED( ofaOpenPrefsBin, ofa_open_prefs_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaOpenPrefsBin ))

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
 */
ofaOpenPrefsBin *
ofa_open_prefs_bin_new( void )
{
	ofaOpenPrefsBin *bin;

	bin = g_object_new( OFA_TYPE_OPEN_PREFS_BIN, NULL );

	setup_bin( bin );

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
	priv->nonempty_btn = btn;

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-properties" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	priv->properties_btn = btn;

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-balance" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	priv->balances_btn = btn;

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-integrity" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	priv->integrity_btn = btn;

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

/**
 * ofa_open_prefs_bin_get_data:
 * @bin: this #ofaOpenPrefsBin instance.
 * @display_notes: [out][allow-none]: placeholder for the returned boolean.
 * @when_non_empty: [out][allow-none]: placeholder for the returned boolean.
 * @display_properties: [out][allow-none]: placeholder for the returned boolean.
 * @check_balances: [out][allow-none]: placeholder for the returned boolean.
 * @check_integrity: [out][allow-none]: placeholder for the returned boolean.
 *
 * Set the output values.
 */
void
ofa_open_prefs_bin_get_data( const ofaOpenPrefsBin *bin,
		gboolean *display_notes, gboolean *when_non_empty, gboolean *display_properties,  gboolean *check_balances,
		gboolean *check_integrity )
{
	ofaOpenPrefsBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_OPEN_PREFS_BIN( bin ));

	priv = ofa_open_prefs_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	if( display_notes ){
		*display_notes = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->notes_btn ));
	}
	if( when_non_empty ){
		*when_non_empty = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->nonempty_btn ));
	}
	if( display_properties ){
		*display_properties = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->properties_btn ));
	}
	if( check_balances ){
		*check_balances = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->balances_btn ));
	}
	if( check_integrity ){
		*check_integrity = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->integrity_btn ));
	}
}

/**
 * ofa_open_prefs_bin_set_data:
 * @bin: this #ofaOpenPrefsBin instance.
 * @display_notes: whether to display notes.
 * @when_non_empty: whether to display notes only when non empty.
 * @display_properties: whether to display properties.
 * @check_balances: whether to check balances.
 * @check_integrity: whether to check DBMS integrity.
 *
 * Set the data.
 */
void
ofa_open_prefs_bin_set_data( ofaOpenPrefsBin *bin,
		gboolean display_notes, gboolean when_non_empty, gboolean display_properties, gboolean check_balances,
		gboolean check_integrity )
{
	ofaOpenPrefsBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_OPEN_PREFS_BIN( bin ));

	priv = ofa_open_prefs_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->notes_btn ), display_notes );
	on_display_notes_toggled( GTK_TOGGLE_BUTTON( priv->notes_btn ), bin );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->nonempty_btn ), when_non_empty );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->properties_btn ), display_properties );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->balances_btn ), check_balances );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->integrity_btn ), check_integrity );
}

static void
on_display_notes_toggled( GtkToggleButton *button, ofaOpenPrefsBin *self )
{
	ofaOpenPrefsBinPrivate *priv;

	priv = ofa_open_prefs_bin_get_instance_private( self );

	gtk_widget_set_sensitive( priv->nonempty_btn, gtk_toggle_button_get_active( button ));
}
