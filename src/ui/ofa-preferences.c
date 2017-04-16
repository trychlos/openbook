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
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-date-combo.h"
#include "my/my-decimal-combo.h"
#include "my/my-ibin.h"
#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-extender-collection.h"
#include "api/ofa-igetter.h"
#include "api/ofa-iproperties.h"
#include "api/ofa-isignaler.h"
#include "api/ofa-prefs.h"

#include "core/ofa-open-prefs.h"
#include "core/ofa-open-prefs-bin.h"
#include "core/ofa-dossier-delete-prefs-bin.h"
#include "core/ofa-stream-format-bin.h"

#include "ui/ofa-preferences.h"

/* private instance data
 */
typedef struct {
	gboolean                  dispose_has_run;

	/* initialization
	 */
	ofaIGetter               *getter;
	GtkWindow                *parent;

	/* runtime
	 */
	GtkWindow                *actual_parent;

	/* UI - General
	 */
	GtkWidget                *book;			/* main notebook of the dialog */
	GtkWidget                *msg_label;
	GtkWidget                *ok_btn;

	/* when opening the preferences from the plugin manager
	 */
	ofaExtenderModule        *plugin;
	GtkWidget                *object_page;

	/* UI - User interface
	 */
	GSList                   *p1_startup_group;
	GSList                   *p1_open_group;
	GSList                   *p1_tabs_group;
	GtkWidget                *p1_pin_detach_btn;
	GSList                   *p1_close_group;
	GtkWidget                *p1_display_all_btn;
	GtkWidget                *p1_quit_on_escape_btn;
	GtkWidget                *p1_confirm_on_escape_btn;
	GtkWidget                *p1_confirm_on_cancel_btn;
	GtkWidget                *p1_confirm_altf4_btn;
	GtkWidget                *p1_confirm_quit_btn;

	/* UI - Dossier page
	 */
	ofaOpenPrefs             *open_prefs;
	ofaOpenPrefsBin          *prefs_bin;
	ofaDossierDeletePrefsBin *dd_prefs;

	/* UI - Account delete page
	 */
	GtkWidget                *p3_delete_children_btn;
	GtkWidget                *p3_settle_warns_btn;
	GtkWidget                *p3_settle_ctrl_btn;
	GtkWidget                *p3_reconciliate_warns_btn;
	GtkWidget                *p3_reconciliate_ctrl_btn;

	/* UI - Locales
	 */
	myDateCombo              *p4_display_combo;
	myDateCombo              *p4_check_combo;
	GtkWidget                *p4_date_over;
	myDecimalCombo           *p4_decimal_sep;
	GtkWidget                *p4_thousand_sep;
	GtkWidget                *p4_accept_dot;
	GtkWidget                *p4_accept_comma;

	/* Export settings
	 */
	ofaStreamFormatBin       *export_settings;
	GtkFileChooser           *p5_chooser;

	/* Import settings
	 */
	ofaStreamFormatBin       *import_settings;
}
	ofaPreferencesPrivate;

#define IPROPERTIES_PAGE                  "ofaIProperties"

#define PREFERENCES_MAINBOOK_STARTUP_MODE "ofaPreferences-mainbook-startup-mode"
#define PREFERENCES_MAINBOOK_OPEN_MODE    "ofaPreferences-mainbook-open-mode"
#define PREFERENCES_MAINBOOK_TABS_MODE    "ofaPreferences-mainbook-tabs-mode"
#define PREFERENCES_MAINBOOK_CLOSE_MODE   "ofaPreferences-mainbook-close-mode"

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-preferences.ui";

typedef gboolean ( *pfnPlugin )( ofaPreferences *, gchar **msgerr, ofaIProperties * );

static void       iwindow_iface_init( myIWindowInterface *iface );
static void       iwindow_init( myIWindow *instance );
static void       idialog_iface_init( myIDialogInterface *iface );
static void       idialog_init( myIDialog *instance );
static GtkWidget *radio_button_init( ofaPreferences *self, const gchar *group_name, guint current_value, const gchar *btn_name, guint btn_value, GCallback cb, GSList **group );
static gint       radio_button_group_get_value( ofaPreferences *self, const gchar *group_name, GSList *group );
static void       init_user_interface_page( ofaPreferences *self );
static void       init_dossier_page( ofaPreferences *self );
static void       init_account_page( ofaPreferences *self );
static void       init_locales_page( ofaPreferences *self );
static void       init_locale_date( ofaPreferences *self, myDateCombo **wcombo, const gchar *label, const gchar *parent, myDateFormat ivalue );
static void       init_locale_sep( ofaPreferences *self, GtkWidget **wentry, const gchar *label, const gchar *wname, const gchar *svalue );
static void       init_export_page( ofaPreferences *self );
static void       init_import_page( ofaPreferences *self );
static gboolean   enumerate_prefs_plugins( ofaPreferences *self, gchar **msgerr, pfnPlugin pfn );
static gboolean   init_plugin_page( ofaPreferences *self, gchar **msgerr, ofaIProperties *plugin );
//static void       activate_first_page( ofaPreferences *self );
static void       on_mainbook_startup_toggled( GtkToggleButton *button, ofaPreferences *self );
static void       on_mainbook_open_toggled( GtkToggleButton *button, ofaPreferences *self );
static void       on_mainbook_tabs_toggled( GtkToggleButton *button, ofaPreferences *self );
static void       on_mainbook_close_toggled( GtkToggleButton *button, ofaPreferences *self );
static void       on_display_all_toggled( GtkToggleButton *button, ofaPreferences *self );
static void       on_quit_on_escape_toggled( GtkToggleButton *button, ofaPreferences *self );
static void       on_settle_warns_toggled( GtkToggleButton *button, ofaPreferences *self );
static void       on_reconciliate_warns_toggled( GtkToggleButton *button, ofaPreferences *self );
static void       on_display_date_changed( GtkComboBox *box, ofaPreferences *self );
static void       on_check_date_changed( GtkComboBox *box, ofaPreferences *self );
static void       on_date_overwrite_toggled( GtkToggleButton *toggle, ofaPreferences *self );
static void       on_date_changed( ofaPreferences *self, GtkComboBox *box, const gchar *sample_name );
static void       on_accept_dot_toggled( GtkToggleButton *toggle, ofaPreferences *self );
static void       on_accept_comma_toggled( GtkToggleButton *toggle, ofaPreferences *self );
static void       check_for_activable_dlg( ofaPreferences *self );
static void       on_ok_clicked( ofaPreferences *self );
//static gboolean   user_confirm_restart( ofaPreferences *self );
static gboolean   do_update_user_interface_page( ofaPreferences *self, gchar **msgerr );
static gboolean   do_update_dossier_page( ofaPreferences *self, gchar **msgerr );
static gboolean   do_update_account_page( ofaPreferences *self, gchar **msgerr );
static gboolean   do_update_locales_page( ofaPreferences *self, gchar **msgerr );
/*static void       error_decimal_sep( ofaPreferences *self );*/
static gboolean   do_update_export_page( ofaPreferences *self, gchar **msgerr );
static gboolean   do_update_import_page( ofaPreferences *self, gchar **msgerr );
static gboolean   update_prefs_plugin( ofaPreferences *self, gchar **msgerr );
static void       set_message( ofaPreferences *self, const gchar *message );

G_DEFINE_TYPE_EXTENDED( ofaPreferences, ofa_preferences, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaPreferences )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
preferences_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_preferences_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PREFERENCES( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_preferences_parent_class )->finalize( instance );
}

static void
preferences_dispose( GObject *instance )
{
	ofaPreferencesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_PREFERENCES( instance ));

	priv = ofa_preferences_get_instance_private( OFA_PREFERENCES( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->open_prefs );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_preferences_parent_class )->dispose( instance );
}

static void
ofa_preferences_init( ofaPreferences *self )
{
	static const gchar *thisfn = "ofa_preferences_init";
	ofaPreferencesPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PREFERENCES( self ));

	priv = ofa_preferences_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_preferences_class_init( ofaPreferencesClass *klass )
{
	static const gchar *thisfn = "ofa_preferences_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = preferences_dispose;
	G_OBJECT_CLASS( klass )->finalize = preferences_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_preferences_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 * @plugin: [allow-none]: the #ofaExtenderModule object for which the
 *  properties are to be displayed.
 *
 * Update the properties of a dossier.
 */
void
ofa_preferences_run( ofaIGetter *getter, GtkWindow *parent, ofaExtenderModule *plugin )
{
	static const gchar *thisfn = "ofa_preferences_run";
	ofaPreferences *self;
	ofaPreferencesPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p, plugin=%p",
			thisfn, ( void * ) getter, ( void * ) parent, ( void * ) plugin );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));
	g_return_if_fail( !plugin || OFA_IS_EXTENDER_MODULE( plugin ));

	self = g_object_new( OFA_TYPE_PREFERENCES, NULL );

	priv = ofa_preferences_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
	priv->plugin = plugin;
	priv->object_page = NULL;

	/* run modal or non-modal depending of the parent */
	my_idialog_run_maybe_modal( MY_IDIALOG( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_preferences_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_preferences_iwindow_init";
	ofaPreferencesPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_preferences_get_instance_private( OFA_PREFERENCES( instance ));

	priv->actual_parent = priv->parent ? priv->parent : GTK_WINDOW( ofa_igetter_get_main_window( priv->getter ));
	my_iwindow_set_parent( instance, priv->actual_parent );

	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_preferences_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

/*
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_preferences_idialog_init";
	ofaPreferencesPrivate *priv;
	GtkWidget *btn;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_preferences_get_instance_private( OFA_PREFERENCES( instance ));

	/* validate the settings on OK + always terminates */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect_swapped( btn, "clicked", G_CALLBACK( on_ok_clicked ), instance );
	priv->ok_btn = btn;

	priv->msg_label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "message" );
	g_return_if_fail( priv->msg_label && GTK_IS_LABEL( priv->msg_label ));
	my_style_add( priv->msg_label, "labelerror" );

	priv->book = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "notebook" );
	g_return_if_fail( priv->book && GTK_IS_NOTEBOOK( priv->book ));

	init_user_interface_page( OFA_PREFERENCES( instance ));
	init_dossier_page( OFA_PREFERENCES( instance ));
	init_account_page( OFA_PREFERENCES( instance ));
	init_locales_page( OFA_PREFERENCES( instance ));
	init_export_page( OFA_PREFERENCES( instance ));
	init_import_page( OFA_PREFERENCES( instance ));
	enumerate_prefs_plugins( OFA_PREFERENCES( instance ), NULL, init_plugin_page );

	check_for_activable_dlg( OFA_PREFERENCES( instance ));
	gtk_widget_show_all( GTK_WIDGET( instance ));
}

/*
 * @group_name: the key string to g_object_set_data() the @btn_value
 * @current_value: the current_value for the data for this group
 * @btn_name: the name of the widget in the UI XML description
 * @btn_value: the integer to be associated with this button
 * @cb: [allow-none]: the handler to be connected.
 * @group_var: [out][allow-none]: placeholder for the buttons group GList;
 *  should be only set for the first button of a group
 */
static GtkWidget *
radio_button_init( ofaPreferences *self, const gchar *group_name, guint current_value,
											const gchar *btn_name, guint btn_value, GCallback cb, GSList **group )
{
	GtkWidget *button;

	g_debug( "radio_button_init: group_name=%s, current_value=%u, btn_name=%s, btn_value=%u",
			group_name, current_value, btn_name, btn_value );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), btn_name );
	g_return_val_if_fail( button && GTK_IS_RADIO_BUTTON( button ), NULL );
	g_object_set_data( G_OBJECT( button ), group_name, GUINT_TO_POINTER( btn_value ));
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), current_value == btn_value );

	if( cb ){
		typedef void ( *localCb )( GtkToggleButton *, void * );
		localCb local_cb = ( localCb ) cb;
		g_signal_connect( button, "toggled", cb, self );
		local_cb( GTK_TOGGLE_BUTTON( button ), self );
	}

	if( group ){
		*group = gtk_radio_button_get_group( GTK_RADIO_BUTTON( button ));
	}

	return( button );
}

static gint
radio_button_group_get_value( ofaPreferences *self, const gchar *group_name, GSList *group )
{
	GSList *it;
	void *btn;
	gboolean active;
	gint value;

	for( it=group ; it ; it=it->next ){
		btn = it->data;
		g_return_val_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ), -1 );
		active = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( btn ));
		value = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( btn ), group_name ));
		g_debug( "radio_button_group_get_value: group=%s, btn=%p, value=%d, active=%s",
				group_name, btn, value, active ? "True":"False" );
		if( active ){
			return( value );
		}
	}

	return( -1 );
}

/*
 * Radio button management
 * (E): the current value as an integer
 * Foreach button:
 *   set_active if current value is the one for this button
 *   set_data with the int value associated with the button
 *   if first button, then identify the buttons group
 *   (so that we can explore the buttons group to know which is the selected final value)
 * +
 * if the UI is dynamic regarding these radio buttons
 * Foreach button:
 *
 */
static void
init_user_interface_page( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	GtkWidget *button;
	gboolean bvalue;
	guint mode;

	priv = ofa_preferences_get_instance_private( self );

	/* startup mode */
	mode = ofa_prefs_mainbook_get_startup_mode( priv->getter );
	radio_button_init( self,
			PREFERENCES_MAINBOOK_STARTUP_MODE, mode,
			"p1-start-normal-btn", MAINBOOK_STARTNORMAL, G_CALLBACK( on_mainbook_startup_toggled ), &priv->p1_startup_group );
	radio_button_init( self,
			PREFERENCES_MAINBOOK_STARTUP_MODE, mode,
			"p1-start-mini-btn", MAINBOOK_STARTMINI, G_CALLBACK( on_mainbook_startup_toggled ), NULL );

	/* display mode when a dossier is opened */
	mode = ofa_prefs_mainbook_get_open_mode( priv->getter );
	radio_button_init( self,
			PREFERENCES_MAINBOOK_OPEN_MODE, mode,
			"p1-open-keep-btn", MAINBOOK_OPENKEEP, G_CALLBACK( on_mainbook_open_toggled ), &priv->p1_open_group );
	radio_button_init( self,
			PREFERENCES_MAINBOOK_OPEN_MODE, mode,
			"p1-open-normal-btn", MAINBOOK_OPENNATURAL, G_CALLBACK( on_mainbook_open_toggled ), NULL );

	/* display mode after a dossier has been closed */
	mode = ofa_prefs_mainbook_get_close_mode( priv->getter );
	radio_button_init( self,
			PREFERENCES_MAINBOOK_CLOSE_MODE, mode,
			"p1-close-keep-btn", MAINBOOK_CLOSEKEEP, G_CALLBACK( on_mainbook_close_toggled ), &priv->p1_close_group );
	radio_button_init( self,
			PREFERENCES_MAINBOOK_CLOSE_MODE, mode,
			"p1-close-start-btn", MAINBOOK_CLOSERESET, G_CALLBACK( on_mainbook_close_toggled ), NULL );

	/* tabs have a detach pin
	 * must be setup before tabs management below because of the tabs handler */
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-pin-detach" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_mainbook_get_with_detach_pin( priv->getter );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
	priv->p1_pin_detach_btn = button;

	/* tab management in case of a normal display */
	mode = ofa_prefs_mainbook_get_tabs_mode( priv->getter );
	radio_button_init( self,
			PREFERENCES_MAINBOOK_TABS_MODE, mode,
			"p1-dnd-reorder-btn", MAINBOOK_TABREORDER, G_CALLBACK( on_mainbook_tabs_toggled ), &priv->p1_tabs_group );
	radio_button_init( self,
			PREFERENCES_MAINBOOK_TABS_MODE, mode,
			"p1-dnd-detach-btn", MAINBOOK_TABDETACH, G_CALLBACK( on_mainbook_tabs_toggled ), NULL );

	/* check integrity display messages */
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-disp-all" );
	g_return_if_fail( button && GTK_IS_RADIO_BUTTON( button ));
	g_signal_connect( button, "toggled", G_CALLBACK( on_display_all_toggled ), self );
	bvalue = ofa_prefs_check_integrity_get_display_all( priv->getter );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
	priv->p1_display_all_btn = button;

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-disp-errs" );
	g_return_if_fail( button && GTK_IS_RADIO_BUTTON( button ));
	g_signal_connect( button, "toggled", G_CALLBACK( on_display_all_toggled ), self );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), !bvalue );
	on_display_all_toggled( GTK_TOGGLE_BUTTON( button ), self );

	/* quitting an assistant */
	/* priv->confirm_on_escape_btn is set before acting on
	 *  quit-on-escape button as triggered signal use the variable */
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-confirm-on-escape" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_assistant_confirm_on_escape( priv->getter );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
	priv->p1_confirm_on_escape_btn = button;

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-quit-on-escape" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( button, "toggled", G_CALLBACK( on_quit_on_escape_toggled ), self );
	bvalue = ofa_prefs_assistant_quit_on_escape( priv->getter );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
	on_quit_on_escape_toggled( GTK_TOGGLE_BUTTON( button ), self );
	priv->p1_quit_on_escape_btn = button;

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-confirm-on-cancel" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_assistant_confirm_on_cancel( priv->getter );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
	priv->p1_confirm_on_cancel_btn = button;

	/* quitting the application */
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-confirm-altf4" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_appli_confirm_on_altf4( priv->getter );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
	priv->p1_confirm_altf4_btn = button;

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-confirm-quit" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_appli_confirm_on_quit( priv->getter );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
	priv->p1_confirm_quit_btn = button;
}

static void
init_dossier_page( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	GtkWidget *parent;
	myISettings *settings;

	priv = ofa_preferences_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	priv->open_prefs = ofa_open_prefs_new( settings, HUB_USER_SETTINGS_GROUP, OPEN_PREFS_USER_KEY );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "prefs-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->prefs_bin = ofa_open_prefs_bin_new( priv->open_prefs );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->prefs_bin ));

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "dossier-delete-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->dd_prefs = ofa_dossier_delete_prefs_bin_new( priv->getter );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->dd_prefs ));
}

static void
init_account_page( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	GtkWidget *button;
	gboolean bvalue;

	priv = ofa_preferences_get_instance_private( self );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-delete-children" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_account_get_delete_with_children( priv->getter );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
	priv->p3_delete_children_btn = button;

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-settle-ctrl" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_account_settle_warns_unless_ctrl( priv->getter );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
	priv->p3_settle_ctrl_btn = button;

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-settle-warns" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( button, "toggled", G_CALLBACK( on_settle_warns_toggled ), self );
	bvalue = ofa_prefs_account_settle_warns_if_unbalanced( priv->getter );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
	on_settle_warns_toggled( GTK_TOGGLE_BUTTON( button ), self );
	priv->p3_settle_warns_btn = button;

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-reconciliate-ctrl" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_account_reconcil_warns_unless_ctrl( priv->getter );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
	priv->p3_reconciliate_ctrl_btn = button;

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-reconciliate-warns" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( button, "toggled", G_CALLBACK( on_reconciliate_warns_toggled ), self );
	bvalue = ofa_prefs_account_reconcil_warns_if_unbalanced( priv->getter );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
	on_reconciliate_warns_toggled( GTK_TOGGLE_BUTTON( button ), self );
	priv->p3_reconciliate_warns_btn = button;
}

static void
init_locales_page( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	GtkWidget *parent, *label, *check;

	priv = ofa_preferences_get_instance_private( self );

	/*get_locales();*/

	init_locale_date( self,
			&priv->p4_display_combo,
			"p4-display-label", "p4-display-parent",
			ofa_prefs_date_get_display_format( priv->getter ));
	g_signal_connect( priv->p4_display_combo, "changed", G_CALLBACK( on_display_date_changed ), self );
	on_display_date_changed( GTK_COMBO_BOX( priv->p4_display_combo ), self );

	init_locale_date( self,
			&priv->p4_check_combo,
			"p4-check-label",  "p4-check-parent",
			ofa_prefs_date_get_check_format( priv->getter ));
	g_signal_connect( priv->p4_check_combo, "changed", G_CALLBACK( on_check_date_changed ), self );
	on_check_date_changed( GTK_COMBO_BOX( priv->p4_check_combo ), self );

	check = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-date-over" );
	g_return_if_fail( check && GTK_IS_CHECK_BUTTON( check ));
	priv->p4_date_over = check;
	g_signal_connect( check, "toggled", G_CALLBACK( on_date_overwrite_toggled ), self );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check ), ofa_prefs_date_get_overwrite( priv->getter ));
	on_date_overwrite_toggled( GTK_TOGGLE_BUTTON( check ), self );

	/* decimal display */
	priv->p4_decimal_sep = my_decimal_combo_new();

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-decimal-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p4_decimal_sep ));
	my_decimal_combo_set_selected( priv->p4_decimal_sep, ofa_prefs_amount_get_decimal_sep( priv->getter ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-decimal-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->p4_decimal_sep ));

	/* accept dot decimal separator */
	check = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-accept-dot" );
	g_return_if_fail( check && GTK_IS_CHECK_BUTTON( check ));
	priv->p4_accept_dot = check;
	g_signal_connect( check, "toggled", G_CALLBACK( on_accept_dot_toggled ), self );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check ), ofa_prefs_amount_get_accept_dot( priv->getter ));
	on_accept_dot_toggled( GTK_TOGGLE_BUTTON( check ), self );

	/* accept comma decimal separator */
	check = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-accept-comma" );
	g_return_if_fail( check && GTK_IS_CHECK_BUTTON( check ));
	priv->p4_accept_comma = check;
	g_signal_connect( check, "toggled", G_CALLBACK( on_accept_comma_toggled ), self );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check ), ofa_prefs_amount_get_accept_comma( priv->getter ));
	on_accept_comma_toggled( GTK_TOGGLE_BUTTON( check ), self );

	/* thousand separator */
	init_locale_sep( self,
			&priv->p4_thousand_sep,
			"p4-thousand-label", "p4-thousand-sep",
			ofa_prefs_amount_get_thousand_sep( priv->getter ));
}

#if 0
static void
get_locales( void )
{
	static const gchar *thisfn = "ofa_preferences_get_locales";
	gchar *stdout, *stderr;
	gint exit_status;
	GError *error;
	gchar **lines, **iter;

	stdout = NULL;
	stderr = NULL;
	error = NULL;

	if( !g_spawn_command_line_sync( "locale -a", &stdout, &stderr, &exit_status, &error )){
		g_warning( "%s: %s", thisfn, error->message );
		g_error_free( error );

	} else if( my_strlen( stderr )){
		g_warning( "%s: stderr='%s'", thisfn, stderr );
		g_free( stderr );

	} else {
		/*g_debug( "%s: stdout='%s'", thisfn, stdout );*/
		lines = g_strsplit( stdout, "\n", -1 );
		g_free( stdout );
		iter = lines;
		while( *iter ){
			if( my_strlen( *iter )){
				g_debug( "%s: iter='%s'", thisfn, *iter );
			}
			iter++;
		}
		g_strfreev( lines );
	}
}
#endif

static void
init_locale_date( ofaPreferences *self, myDateCombo **wcombo, const gchar *label_name, const gchar *parent, myDateFormat ivalue )
{
	GtkWidget *parent_widget, *label;

	parent_widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), parent );
	g_return_if_fail( parent_widget && GTK_IS_CONTAINER( parent_widget ));

	*wcombo = my_date_combo_new();
	gtk_container_add( GTK_CONTAINER( parent_widget ), GTK_WIDGET( *wcombo ));
	my_date_combo_set_selected( *wcombo, ivalue );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), label_name );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( *wcombo ));
}

static void
init_locale_sep( ofaPreferences *self, GtkWidget **wentry, const gchar *label_name, const gchar *wname, const gchar *svalue )
{
	GtkWidget *label;

	*wentry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), wname );
	g_return_if_fail( *wentry && GTK_IS_ENTRY( *wentry ));

	gtk_entry_set_text( GTK_ENTRY( *wentry ), svalue );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), label_name );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( *wentry ));
}

static void
init_export_page( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	GtkWidget *target, *label;
	GtkSizeGroup *group, *group_bin;
	ofaStreamFormat *stformat;
	const gchar *cstr;

	priv = ofa_preferences_get_instance_private( self );

	group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	target = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-export-parent" );
	g_return_if_fail( target && GTK_IS_CONTAINER( target ));

	stformat = ofa_stream_format_new( priv->getter, NULL, OFA_SFMODE_EXPORT );
	ofa_stream_format_set_field_updatable( stformat, OFA_SFHAS_NAME, FALSE );
	ofa_stream_format_set_field_updatable( stformat, OFA_SFHAS_MODE, FALSE );
	priv->export_settings = ofa_stream_format_bin_new( stformat );
	g_object_unref( stformat );
	gtk_container_add( GTK_CONTAINER( target ), GTK_WIDGET( priv->export_settings ));
	if(( group_bin = my_ibin_get_size_group( MY_IBIN( priv->export_settings ), 0 ))){
		my_utils_size_group_add_size_group( group, group_bin );
	}

	priv->p5_chooser = GTK_FILE_CHOOSER( my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p52-folder" ));
	cstr = ofa_prefs_export_get_default_folder( priv->getter );
	if( my_strlen( cstr )){
		gtk_file_chooser_set_current_folder_uri( priv->p5_chooser, cstr );
	}

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p52-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->p5_chooser ));
	gtk_size_group_add_widget( group, label );

	g_object_unref( group );
}

static void
init_import_page( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	GtkWidget *target;
	ofaStreamFormat *stformat;

	priv = ofa_preferences_get_instance_private( self );

	target = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p6-import-parent" );
	g_return_if_fail( target && GTK_IS_CONTAINER( target ));

	stformat = ofa_stream_format_new( priv->getter, NULL, OFA_SFMODE_IMPORT );
	ofa_stream_format_set_field_updatable( stformat, OFA_SFHAS_NAME, FALSE );
	ofa_stream_format_set_field_updatable( stformat, OFA_SFHAS_MODE, FALSE );
	priv->import_settings = ofa_stream_format_bin_new( stformat );
	g_object_unref( stformat );
	gtk_container_add( GTK_CONTAINER( target ), GTK_WIDGET( priv->import_settings ));
}

static gboolean
enumerate_prefs_plugins( ofaPreferences *self, gchar **msgerr, pfnPlugin pfn )
{
	ofaPreferencesPrivate *priv;
	ofaExtenderCollection *extenders;
	GList *list, *it;
	gboolean ok;

	priv = ofa_preferences_get_instance_private( self );

	ok = TRUE;
	extenders = ofa_igetter_get_extender_collection( priv->getter );
	list = ofa_extender_collection_get_for_type( extenders, OFA_TYPE_IPROPERTIES );

	for( it=list ; it ; it=it->next ){
		ok &= ( *pfn )( self, msgerr, OFA_IPROPERTIES( it->data ));
	}

	g_list_free( list );

	return( ok );
}

/*
 * @instance: an object maintained by a plugin, which implements our
 *  IProperties interface.
 *
 * Add a page to the notebook for each object of the list.
 * Flags the page as being managed by an ofaIProperties instance.
 */
static gboolean
init_plugin_page( ofaPreferences *self, gchar **msgerr, ofaIProperties *instance )
{
	static const gchar *thisfn = "ofa_preferences_init_plugin_page";
	ofaPreferencesPrivate *priv;
	GtkWidget *page, *wlabel;
	gchar *label;
	gboolean ok;

	g_debug( "%s: self=%p, msgerr=%p, instance=%p (%s)",
			thisfn, ( void * ) self, ( void * ) msgerr, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	priv = ofa_preferences_get_instance_private( self );

	ok = FALSE;
	page = ofa_iproperties_init( instance, priv->getter );
	label = ofa_iproperties_get_title( instance );

	if( page && my_strlen( label )){
		g_object_set_data( G_OBJECT( page ), IPROPERTIES_PAGE, IPROPERTIES_PAGE );

		my_utils_widget_set_margins( GTK_WIDGET( page ), 4, 4, 4, 4 );

		wlabel = gtk_label_new( label );
		g_free( label );

		gtk_notebook_append_page( GTK_NOTEBOOK( priv->book ), GTK_WIDGET( page ), wlabel );

		/* try to identify if the plugin which implements this object is the
		 * one which has been required */
		if( priv->plugin && !priv->object_page ){
			if( ofa_extender_module_has_object( priv->plugin, G_OBJECT( instance ))){
				priv->object_page = GTK_WIDGET( page );
			}
		}
		ok = TRUE;
	}

	return( ok );
}

/*
 * activate the first page of the preferences notebook
 */
#if 0
static void
activate_first_page( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	gint page_num;

	priv = self->priv;

	if( priv->object_page ){
		page_num = gtk_notebook_page_num( GTK_NOTEBOOK( priv->book ), priv->object_page );
		if( page_num >= 0 ){
			gtk_notebook_set_current_page( GTK_NOTEBOOK( priv->book ), page_num );
		}
	}
}
#endif

static void
on_mainbook_startup_toggled( GtkToggleButton *button, ofaPreferences *self )
{
}

static void
on_mainbook_open_toggled( GtkToggleButton *button, ofaPreferences *self )
{
}

static void
on_mainbook_tabs_toggled( GtkToggleButton *button, ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	guint mode;
	gboolean active;

	priv = ofa_preferences_get_instance_private( self );

	mode = GPOINTER_TO_UINT( g_object_get_data( G_OBJECT( button ), PREFERENCES_MAINBOOK_TABS_MODE ));
	active = gtk_toggle_button_get_active( button );

	gtk_widget_set_sensitive(
			priv->p1_pin_detach_btn,
			( active && mode == MAINBOOK_TABREORDER ) || ( !active && mode != MAINBOOK_TABREORDER ) );
}

static void
on_mainbook_close_toggled( GtkToggleButton *button, ofaPreferences *self )
{
}

static void
on_display_all_toggled( GtkToggleButton *button, ofaPreferences *self )
{
}

static void
on_quit_on_escape_toggled( GtkToggleButton *button, ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;

	priv = ofa_preferences_get_instance_private( self );

	gtk_widget_set_sensitive(
			priv->p1_confirm_on_escape_btn, gtk_toggle_button_get_active( button ));
}

static void
on_settle_warns_toggled( GtkToggleButton *button, ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;

	priv = ofa_preferences_get_instance_private( self );

	gtk_widget_set_sensitive( priv->p3_settle_ctrl_btn, gtk_toggle_button_get_active( button ));
}

static void
on_reconciliate_warns_toggled( GtkToggleButton *button, ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;

	priv = ofa_preferences_get_instance_private( self );

	gtk_widget_set_sensitive( priv->p3_reconciliate_ctrl_btn, gtk_toggle_button_get_active( button ));
}

static void
on_display_date_changed( GtkComboBox *box, ofaPreferences *self )
{
	on_date_changed( self, box, "p4-display-sample" );
}

static void
on_check_date_changed( GtkComboBox *box, ofaPreferences *self )
{
	on_date_changed( self, box, "p4-check-sample" );
}

static void
on_date_changed( ofaPreferences *self, GtkComboBox *box, const gchar *sample_name )
{
	gint format;
	static GDate *date = NULL;
	gchar *str, *str2;
	GtkWidget *label;

	format = my_date_combo_get_selected( MY_DATE_COMBO( box ));
	if( !date ){
		date = g_date_new_dmy( 31, 8, 2015 );
	}
	str = my_date_to_str( date, format );
	str2 = g_strdup_printf( "<i>%s</i>", str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), sample_name );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_markup( GTK_LABEL( label ), str2 );

	g_free( str2 );
	g_free( str );
}

static void
on_date_overwrite_toggled( GtkToggleButton *toggle, ofaPreferences *self )
{
	check_for_activable_dlg( self );
}

static void
on_accept_dot_toggled( GtkToggleButton *toggle, ofaPreferences *self )
{
	check_for_activable_dlg( self );
}

static void
on_accept_comma_toggled( GtkToggleButton *toggle, ofaPreferences *self )
{
	check_for_activable_dlg( self );
}

/*
 * refuse to validate the dialog if:
 * - the user doesn't accept dot decimal separator, nor comma
 * - or export or import pages are not valid
 */
static void
check_for_activable_dlg( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	gboolean accept_dot, accept_comma;
	gboolean activable;
	gchar *msg, *msgerr;

	priv = ofa_preferences_get_instance_private( self );

	set_message( self, "" );
	msg = NULL;

	activable = my_ibin_is_valid( MY_IBIN( priv->prefs_bin ), &msg );

	if( !priv->p4_accept_dot || !priv->p4_accept_comma ){
		activable = FALSE;
	} else {
		accept_dot = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p4_accept_dot ));
		accept_comma = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p4_accept_comma ));
		activable &= ( accept_dot || accept_comma );
	}

	if( !activable ){
		set_message( self, _( "Language must accept either dot or comma decimal separator" ));

	} else if( priv->export_settings && !my_ibin_is_valid( MY_IBIN( priv->export_settings ), &msg )){
		msgerr = g_strdup_printf( _( "Export settings: %s" ), msg );
		set_message( self, msgerr );
		g_free( msg );
		g_free( msgerr );
		activable = FALSE;

	} else if( priv->import_settings && !my_ibin_is_valid( MY_IBIN( priv->import_settings ), &msg )){
		msgerr = g_strdup_printf( _( "Import settings: %s" ), msg );
		set_message( self, msgerr );
		g_free( msg );
		g_free( msgerr );
		activable = FALSE;
	}

	gtk_widget_set_sensitive( priv->ok_btn, activable );
}

static void
on_ok_clicked( ofaPreferences *self )
{
	static const gchar *thisfn = "ofa_preferences_do_update";
	//ofaPreferencesPrivate *priv;
	gboolean ok;
	gchar *msgerr;

	//priv = ofa_preferences_get_instance_private( self );

	msgerr = NULL;

	ok = do_update_user_interface_page( self, &msgerr ) &&
			do_update_dossier_page( self, &msgerr ) &&
			do_update_account_page( self, &msgerr ) &&
			do_update_locales_page( self, &msgerr ) &&
			do_update_export_page( self, &msgerr ) &&
			do_update_import_page( self, &msgerr ) &&
			update_prefs_plugin( self, &msgerr );

	g_debug( "%s: ok=%s", thisfn, ok ? "True":"False" );

	if( !ok && msgerr ){
		my_utils_msg_dialog( GTK_WINDOW( self ), GTK_MESSAGE_WARNING, msgerr );
		g_free( msgerr );
	}

#if 0
	ofaISignaler *signaler;
	if( priv->p1_dirty_ui ){
		if( user_confirm_restart( self )){
			my_iwindow_set_allow_close( MY_IWINDOW( self ), FALSE );
			signaler = ofa_igetter_get_signaler( priv->getter );
			g_signal_emit_by_name( signaler, SIGNALER_UI_RESTART );
			my_iwindow_set_allow_close( MY_IWINDOW( self ), TRUE );
		}
	}
#endif

	my_iwindow_close( MY_IWINDOW( self ));
}

#if 0
static gboolean
user_confirm_restart( ofaPreferences *self )
{
	GtkWidget *dialog;
	gint response;

	dialog = gtk_message_dialog_new(
			my_utils_widget_get_toplevel( GTK_WIDGET( self )),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE, "%s",
			_( "Some of the preferences you have updated require a restart "
				"of the application to be taken into account.\n"
				"Do you want to restart now ?" ));

	gtk_dialog_add_buttons( GTK_DIALOG( dialog ),
			_( "_No, I will restart later" ), GTK_RESPONSE_CANCEL,
			_( "_Yes, restart now" ), GTK_RESPONSE_OK,
			NULL );

	response = gtk_dialog_run( GTK_DIALOG( dialog ));

	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
}
#endif

static gboolean
do_update_user_interface_page( ofaPreferences *self, gchar **msgerr )
{
	ofaPreferencesPrivate *priv;
	ofeMainbookStartup startup_mode;
	ofeMainbookOpen open_mode;
	ofeMainbookTabs tabs_mode;
	ofeMainbookClose close_mode;
	gboolean detach_pin, display_all, quit_on_escape, confirm_on_escape, confirm_on_cancel;
	gboolean confirm_altf4, confirm_quit;

	priv = ofa_preferences_get_instance_private( self );

	startup_mode = radio_button_group_get_value( self, PREFERENCES_MAINBOOK_STARTUP_MODE, priv->p1_startup_group );
	open_mode = radio_button_group_get_value( self, PREFERENCES_MAINBOOK_OPEN_MODE, priv->p1_open_group );
	tabs_mode = radio_button_group_get_value( self, PREFERENCES_MAINBOOK_TABS_MODE, priv->p1_tabs_group );
	close_mode = radio_button_group_get_value( self, PREFERENCES_MAINBOOK_CLOSE_MODE, priv->p1_close_group );

	detach_pin = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p1_pin_detach_btn ));

	ofa_prefs_mainbook_set_user_settings( priv->getter,
			startup_mode, open_mode, tabs_mode, detach_pin, close_mode );

	display_all = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p1_display_all_btn ));
	ofa_prefs_check_integrity_set_user_settings( priv->getter, display_all );

	quit_on_escape = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p1_quit_on_escape_btn ));
	confirm_on_escape = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p1_confirm_on_escape_btn ));
	confirm_on_cancel = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p1_confirm_on_cancel_btn ));
	ofa_prefs_assistant_set_user_settings( priv->getter, quit_on_escape, confirm_on_escape, confirm_on_cancel );

	confirm_altf4 = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p1_confirm_altf4_btn ));
	confirm_quit = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p1_confirm_quit_btn ));
	ofa_prefs_appli_set_user_settings( priv->getter, confirm_altf4, confirm_quit );

	return( TRUE );
}

static gboolean
do_update_dossier_page( ofaPreferences *self, gchar **msgerr )
{
	ofaPreferencesPrivate *priv;

	priv = ofa_preferences_get_instance_private( self );

	my_ibin_apply( MY_IBIN( priv->prefs_bin ));

	ofa_dossier_delete_prefs_bin_apply( priv->dd_prefs );

	return( TRUE );
}

/*
 * Settings are:
 * delete_children(b); settle_warns(b); settle_ctrl(b); reconciliate_warns(b); reconciliate_ctrl(b);
 */
static gboolean
do_update_account_page( ofaPreferences *self, gchar **msgerrr )
{
	ofaPreferencesPrivate *priv;
	gboolean delete_with_children, settle_warns, settle_ctrl, concil_warns, concil_ctrl;

	priv = ofa_preferences_get_instance_private( self );

	delete_with_children = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p3_delete_children_btn ));
	settle_warns = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p3_settle_warns_btn ));
	settle_ctrl = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p3_settle_ctrl_btn ));
	concil_warns = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p3_reconciliate_warns_btn ));
	concil_ctrl = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p3_reconciliate_ctrl_btn ));

	ofa_prefs_account_set_user_settings( priv->getter,
			delete_with_children, settle_warns, settle_ctrl, concil_warns, concil_ctrl );

	return( TRUE );
}

static gboolean
do_update_locales_page( ofaPreferences *self, gchar **msgerr )
{
	ofaPreferencesPrivate *priv;
	gchar *decimal_sep;

	priv = ofa_preferences_get_instance_private( self );

	ofa_prefs_date_set_user_settings( priv->getter,
			my_date_combo_get_selected( priv->p4_display_combo ),
			my_date_combo_get_selected( priv->p4_check_combo ),
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p4_date_over )));

	decimal_sep = my_decimal_combo_get_selected( priv->p4_decimal_sep );
	ofa_prefs_amount_set_user_settings( priv->getter,
			decimal_sep,
			gtk_entry_get_text( GTK_ENTRY( priv->p4_thousand_sep )),
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p4_accept_dot )),
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p4_accept_comma )));
	g_free( decimal_sep );

	return( TRUE );
}

static gboolean
do_update_export_page( ofaPreferences *self, gchar **msgerr )
{
	ofaPreferencesPrivate *priv;
	gchar *text;

	priv = ofa_preferences_get_instance_private( self );

	my_ibin_apply( MY_IBIN( priv->export_settings ));

	text = gtk_file_chooser_get_uri( priv->p5_chooser );
	if( my_strlen( text )){
		ofa_prefs_export_set_user_settings( priv->getter, text );
	}
	g_free( text );

	return( TRUE );
}

static gboolean
do_update_import_page( ofaPreferences *self, gchar **msgerr )
{
	ofaPreferencesPrivate *priv;

	priv = ofa_preferences_get_instance_private( self );

	my_ibin_apply( MY_IBIN( priv->import_settings ));

	return( TRUE );
}

static gboolean
update_prefs_plugin( ofaPreferences *self, gchar **msgerr )
{
	ofaPreferencesPrivate *priv;
	gint pages_count, i;
	GtkWidget *page;
	const gchar *cstr;

	priv = ofa_preferences_get_instance_private( self );

	pages_count = gtk_notebook_get_n_pages( GTK_NOTEBOOK( priv->book ));

	for( i=0 ; i<pages_count ; ++i ){
		page = gtk_notebook_get_nth_page( GTK_NOTEBOOK( priv->book ), i );
		cstr = g_object_get_data( G_OBJECT( page ), IPROPERTIES_PAGE );
		if( !my_collate( cstr, IPROPERTIES_PAGE )){
			ofa_iproperties_apply( page );
		}
	}

	return( TRUE );
}

static void
set_message( ofaPreferences *self, const gchar *message )
{
	ofaPreferencesPrivate *priv;

	priv = ofa_preferences_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), message ? message : "" );
}
