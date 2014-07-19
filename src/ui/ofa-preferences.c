/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>

#include "core/my-utils.h"
#include "core/ofa-settings.h"

#include "ui/my-window-prot.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-preferences.h"

/* private instance data
 */
struct _ofaPreferencesPrivate {

	/* whether the dialog has been validated
	 */
	gboolean        updated;

	/* UI - Assistant page
	 */
	GtkCheckButton *confirm_on_escape_btn;
};

static const gchar *st_assistant_quit_on_escape    = "AssistantQuitOnEscape";
static const gchar *st_assistant_confirm_on_escape = "AssistantConfirmOnEscape";
static const gchar *st_assistant_confirm_on_cancel = "AssistantConfirmOnCancel";

static const gchar *st_ui_xml = PKGUIDIR "/ofa-preferences.ui";
static const gchar *st_ui_id  = "PreferencesDlg";

G_DEFINE_TYPE( ofaPreferences, ofa_preferences, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      init_assistant_page( ofaPreferences *self );
static void      on_quit_on_escape_toggled( GtkToggleButton *button, ofaPreferences *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_update( ofaPreferences *self );
static void      do_update_assistant_page( ofaPreferences *self );

static void
preferences_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_preferences_finalize";
	ofaPreferencesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PREFERENCES( instance ));

	priv = OFA_PREFERENCES( instance )->private;

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_preferences_parent_class )->finalize( instance );
}

static void
preferences_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_PREFERENCES( instance ));

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_preferences_parent_class )->dispose( instance );
}

static void
ofa_preferences_init( ofaPreferences *self )
{
	static const gchar *thisfn = "ofa_preferences_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PREFERENCES( self ));

	self->private = g_new0( ofaPreferencesPrivate, 1 );
}

static void
ofa_preferences_class_init( ofaPreferencesClass *klass )
{
	static const gchar *thisfn = "ofa_preferences_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = preferences_dispose;
	G_OBJECT_CLASS( klass )->finalize = preferences_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;
}

/**
 * ofa_preferences_run:
 * @main: the main window of the application.
 *
 * Update the properties of an dossier
 */
gboolean
ofa_preferences_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_preferences_run";
	ofaPreferences *self;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new(
				OFA_TYPE_PREFERENCES,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	my_dialog_run_dialog( MY_DIALOG( self ));

	updated = self->private->updated;

	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( myDialog *dialog )
{
	init_assistant_page( OFA_PREFERENCES( dialog ));
}

static void
init_assistant_page( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	GtkContainer *container;
	GtkWidget *button;

	priv = self->private;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));

	/* priv->confirm_on_escape_btn is set before acting on
	 *  quit-on-escape button as triggered signal use the variable */
	button = my_utils_container_get_child_by_name( container, "p1-confirm-on-escape" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON( button ), ofa_prefs_assistant_confirm_on_escape());
	priv->confirm_on_escape_btn = GTK_CHECK_BUTTON( button );

	button = my_utils_container_get_child_by_name( container, "p1-quit-on-escape" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "toggled", G_CALLBACK( on_quit_on_escape_toggled ), self );
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON( button ), ofa_prefs_assistant_quit_on_escape());

	button = my_utils_container_get_child_by_name( container, "p1-confirm-on-cancel" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON( button ), ofa_prefs_assistant_confirm_on_cancel());
}

static void
on_quit_on_escape_toggled( GtkToggleButton *button, ofaPreferences *self )
{
	gtk_widget_set_sensitive(
			GTK_WIDGET( self->private->confirm_on_escape_btn ),
			gtk_toggle_button_get_active( button ));
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_update( OFA_PREFERENCES( dialog )));
}

static gboolean
do_update( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;

	priv = self->private;

	do_update_assistant_page( self );

	priv->updated = TRUE;

	return( priv->updated );
}

static void
do_update_assistant_page( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	GtkContainer *container;
	GtkWidget *button;

	priv = self->private;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));

	button = my_utils_container_get_child_by_name( container, "p1-quit-on-escape" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	ofa_settings_set_boolean(
			st_assistant_quit_on_escape,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( button )));

	ofa_settings_set_boolean(
			st_assistant_confirm_on_escape,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->confirm_on_escape_btn )));

	button = my_utils_container_get_child_by_name( container, "p1-confirm-on-cancel" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	ofa_settings_set_boolean(
			st_assistant_confirm_on_cancel,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( button )));
}

/**
 * ofa_prefs_assistant_quit_on_escape:
 */
gboolean
ofa_prefs_assistant_quit_on_escape( void )
{
	return( ofa_settings_get_boolean( st_assistant_quit_on_escape ));
}

/**
 * ofa_prefs_assistant_confirm_on_escape:
 */
gboolean
ofa_prefs_assistant_confirm_on_escape( void )
{
	return( ofa_settings_get_boolean( st_assistant_confirm_on_escape ));
}

/**
 * ofa_prefs_assistant_confirm_on_cancel:
 */
gboolean
ofa_prefs_assistant_confirm_on_cancel( void )
{
	return( ofa_settings_get_boolean( st_assistant_confirm_on_cancel ));
}
