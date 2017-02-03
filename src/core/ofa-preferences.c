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
#include "api/ofa-preferences.h"

#include "core/ofa-open-prefs.h"
#include "core/ofa-open-prefs-bin.h"
#include "core/ofa-dossier-delete-prefs-bin.h"
#include "core/ofa-stream-format-bin.h"

/* private instance data
 */
typedef struct {
	gboolean                  dispose_has_run;

	/* initialization
	 */
	ofaIGetter               *getter;
	GtkWindow                *parent;

	/* UI - General
	 */
	GtkWidget                *book;			/* main notebook of the dialog */
	GtkWidget                *msg_label;
	GtkWidget                *ok_btn;

	/* when opening the preferences from the plugin manager
	 */
	ofaExtenderModule        *plugin;
	GtkWidget                *object_page;

	/* UI - Quitting
	 */
	GtkCheckButton           *confirm_on_escape_btn;

	/* UI - Dossier page
	 */
	ofaOpenPrefs             *open_prefs;
	ofaOpenPrefsBin          *prefs_bin;
	ofaDossierDeletePrefsBin *dd_prefs;

	/* UI - Account delete page
	 */

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

#define SETTINGS_AMOUNT                               "UserAmount"
#define SETTINGS_DATE                                 "UserDate"
#define SETTINGS_REORDER                              "R"
#define SETTINGS_DETACH                               "D"

#define IPROPERTIES_PAGE                              "ofaIProperties"

/* a cache for some often used preferences
 */
static gboolean     st_date_prefs_set                 = FALSE;
static myDateFormat st_date_display                   = 0;
static myDateFormat st_date_check                     = 0;
static myDateFormat st_date_overwrite                 = FALSE;
static gboolean     st_amount_prefs_set               = FALSE;
static gchar       *st_amount_decimal                 = NULL;
static gchar       *st_amount_thousand                = NULL;
static gboolean     st_amount_accept_dot              = FALSE;
static gboolean     st_amount_accept_comma            = FALSE;

static const gchar *st_dnd_main_tabs                  = "ofaPreferences-DndMainTabs";
static const gchar *st_assistant_quit_on_escape       = "AssistantQuitOnEscape";
static const gchar *st_assistant_confirm_on_escape    = "AssistantConfirmOnEscape";
static const gchar *st_assistant_confirm_on_cancel    = "AssistantConfirmOnCancel";
static const gchar *st_appli_confirm_on_quit          = "ApplicationConfirmOnQuit";
static const gchar *st_appli_confirm_on_altf4         = "ApplicationConfirmOnAltF4";
static const gchar *st_account_delete_root_with_child = "AssistantConfirmOnCancel";
static const gchar *st_export_default_folder          = "ExportDefaultFolder";

static const gchar *st_resource_ui                    = "/org/trychlos/openbook/core/ofa-preferences.ui";

typedef gboolean ( *pfnPlugin )( ofaPreferences *, gchar **msgerr, ofaIProperties * );

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     init_user_interface_page( ofaPreferences *self );
static void     init_dossier_page( ofaPreferences *self );
static void     init_account_page( ofaPreferences *self );
static void     init_locales_page( ofaPreferences *self );
static void     init_locale_date( ofaPreferences *self, myDateCombo **wcombo, const gchar *label, const gchar *parent, myDateFormat ivalue );
static void     init_locale_sep( ofaPreferences *self, GtkWidget **wentry, const gchar *label, const gchar *wname, const gchar *svalue );
static void     init_export_page( ofaPreferences *self );
static void     init_import_page( ofaPreferences *self );
static gboolean enumerate_prefs_plugins( ofaPreferences *self, gchar **msgerr, pfnPlugin pfn );
static gboolean init_plugin_page( ofaPreferences *self, gchar **msgerr, ofaIProperties *plugin );
//static void     activate_first_page( ofaPreferences *self );
static void     on_quit_on_escape_toggled( GtkToggleButton *button, ofaPreferences *self );
static void     on_display_date_changed( GtkComboBox *box, ofaPreferences *self );
static void     on_check_date_changed( GtkComboBox *box, ofaPreferences *self );
static void     on_date_overwrite_toggled( GtkToggleButton *toggle, ofaPreferences *self );
static void     on_date_changed( ofaPreferences *self, GtkComboBox *box, const gchar *sample_name );
static void     on_accept_dot_toggled( GtkToggleButton *toggle, ofaPreferences *self );
static void     on_accept_comma_toggled( GtkToggleButton *toggle, ofaPreferences *self );
static void     check_for_activable_dlg( ofaPreferences *self );
static void     on_ok_clicked( ofaPreferences *self );
static gboolean do_update_user_interface_page( ofaPreferences *self, gchar **msgerr );
static gchar   *dnd_main_tabs_read_settings( ofaIGetter *getter );
static void     dnd_main_tabs_write_settings( ofaPreferences *self, GtkWidget *reorder_btn );
static gboolean is_willing_to_quit( void );
static gboolean do_update_dossier_page( ofaPreferences *self, gchar **msgerr );
static gboolean do_update_account_page( ofaPreferences *self, gchar **msgerr );
static gboolean do_update_locales_page( ofaPreferences *self, gchar **msgerr );
/*static void     error_decimal_sep( ofaPreferences *self );*/
static void     setup_date_formats( ofaIGetter *getter );
static void     setup_amount_formats( ofaIGetter *getter );
static gboolean do_update_export_page( ofaPreferences *self, gchar **msgerr );
static gboolean do_update_import_page( ofaPreferences *self, gchar **msgerr );
static gboolean update_prefs_plugin( ofaPreferences *self, gchar **msgerr );
static void     set_message( ofaPreferences *self, const gchar *message );

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

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
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

	my_iwindow_set_parent( instance, priv->parent );

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
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
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

static void
init_user_interface_page( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	GtkWidget *button;
	gboolean bvalue;

	priv = ofa_preferences_get_instance_private( self );

	/* reorder vs. detach man tabs */
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-dnd-reorder" );
	g_return_if_fail( button && GTK_IS_RADIO_BUTTON( button ));
	bvalue = ofa_prefs_dnd_reorder( priv->getter );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-dnd-detach" );
	g_return_if_fail( button && GTK_IS_RADIO_BUTTON( button ));
	bvalue = ofa_prefs_dnd_detach( priv->getter );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );

	/* priv->confirm_on_escape_btn is set before acting on
	 *  quit-on-escape button as triggered signal use the variable */
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-confirm-on-escape" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_assistant_confirm_on_escape( priv->getter );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
	priv->confirm_on_escape_btn = GTK_CHECK_BUTTON( button );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-quit-on-escape" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( button, "toggled", G_CALLBACK( on_quit_on_escape_toggled ), self );
	bvalue = ofa_prefs_assistant_quit_on_escape( priv->getter );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
	on_quit_on_escape_toggled( GTK_TOGGLE_BUTTON( button ), self );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-confirm-on-cancel" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_assistant_confirm_on_cancel( priv->getter );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-confirm-altf4" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_appli_confirm_on_altf4( priv->getter );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-confirm-quit" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_appli_confirm_on_quit( priv->getter );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
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

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-delete-with-child" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_account_delete_root_with_children( priv->getter );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
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
			ofa_prefs_date_display( priv->getter ));
	g_signal_connect( priv->p4_display_combo, "changed", G_CALLBACK( on_display_date_changed ), self );
	on_display_date_changed( GTK_COMBO_BOX( priv->p4_display_combo ), self );

	init_locale_date( self,
			&priv->p4_check_combo,
			"p4-check-label",  "p4-check-parent",
			ofa_prefs_date_check( priv->getter ));
	g_signal_connect( priv->p4_check_combo, "changed", G_CALLBACK( on_check_date_changed ), self );
	on_check_date_changed( GTK_COMBO_BOX( priv->p4_check_combo ), self );

	check = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-date-over" );
	g_return_if_fail( check && GTK_IS_CHECK_BUTTON( check ));
	priv->p4_date_over = check;
	g_signal_connect( check, "toggled", G_CALLBACK( on_date_overwrite_toggled ), self );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check ), ofa_prefs_date_overwrite( priv->getter ));
	on_date_overwrite_toggled( GTK_TOGGLE_BUTTON( check ), self );

	/* decimal display */
	priv->p4_decimal_sep = my_decimal_combo_new();

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-decimal-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p4_decimal_sep ));
	my_decimal_combo_set_selected( priv->p4_decimal_sep, ofa_prefs_amount_decimal_sep( priv->getter ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-decimal-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->p4_decimal_sep ));

	/* accept dot decimal separator */
	check = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-accept-dot" );
	g_return_if_fail( check && GTK_IS_CHECK_BUTTON( check ));
	priv->p4_accept_dot = check;
	g_signal_connect( check, "toggled", G_CALLBACK( on_accept_dot_toggled ), self );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check ), ofa_prefs_amount_accept_dot( priv->getter ));
	on_accept_dot_toggled( GTK_TOGGLE_BUTTON( check ), self );

	/* accept comma decimal separator */
	check = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-accept-comma" );
	g_return_if_fail( check && GTK_IS_CHECK_BUTTON( check ));
	priv->p4_accept_comma = check;
	g_signal_connect( check, "toggled", G_CALLBACK( on_accept_comma_toggled ), self );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check ), ofa_prefs_amount_accept_comma( priv->getter ));
	on_accept_comma_toggled( GTK_TOGGLE_BUTTON( check ), self );

	/* thousand separator */
	init_locale_sep( self,
			&priv->p4_thousand_sep,
			"p4-thousand-label", "p4-thousand-sep",
			ofa_prefs_amount_thousand_sep( priv->getter ));
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
	gchar *str;
	GtkSizeGroup *group, *group_bin;
	ofaStreamFormat *format;

	priv = ofa_preferences_get_instance_private( self );

	group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	target = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-export-parent" );
	g_return_if_fail( target && GTK_IS_CONTAINER( target ));

	format = ofa_stream_format_new( priv->getter, NULL, OFA_SFMODE_EXPORT );
	priv->export_settings = ofa_stream_format_bin_new( format );
	g_object_unref( format );
	gtk_container_add( GTK_CONTAINER( target ), GTK_WIDGET( priv->export_settings ));
	if(( group_bin = my_ibin_get_size_group( MY_IBIN( priv->export_settings ), 0 ))){
		my_utils_size_group_add_size_group( group, group_bin );
	}
	ofa_stream_format_bin_set_name_sensitive( priv->export_settings, FALSE );
	ofa_stream_format_bin_set_mode_sensitive( priv->export_settings, FALSE );

	priv->p5_chooser = GTK_FILE_CHOOSER( my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p52-folder" ));
	str = ofa_prefs_export_default_folder( priv->getter );
	if( my_strlen( str )){
		gtk_file_chooser_set_current_folder_uri( priv->p5_chooser, str );
	}
	g_free( str );

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
	ofaStreamFormat *settings;

	priv = ofa_preferences_get_instance_private( self );

	target = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p6-import-parent" );
	g_return_if_fail( target && GTK_IS_CONTAINER( target ));

	settings = ofa_stream_format_new( priv->getter, NULL, OFA_SFMODE_IMPORT );
	priv->import_settings = ofa_stream_format_bin_new( settings );
	g_object_unref( settings );
	gtk_container_add( GTK_CONTAINER( target ), GTK_WIDGET( priv->import_settings ));

	ofa_stream_format_bin_set_name_sensitive( priv->import_settings, FALSE );
	ofa_stream_format_bin_set_mode_sensitive( priv->import_settings, FALSE );
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

	ofa_extender_collection_free_types( list );

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
on_quit_on_escape_toggled( GtkToggleButton *button, ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;

	priv = ofa_preferences_get_instance_private( self );

	gtk_widget_set_sensitive(
			GTK_WIDGET( priv->confirm_on_escape_btn ),
			gtk_toggle_button_get_active( button ));
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
	gboolean ok;
	gchar *msgerr;

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

	my_iwindow_close( MY_IWINDOW( self ));
}

static gboolean
do_update_user_interface_page( ofaPreferences *self, gchar **msgerr )
{
	ofaPreferencesPrivate *priv;
	myISettings *settings;
	GtkWidget *button;

	priv = ofa_preferences_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-dnd-reorder" );
	g_return_val_if_fail( button && GTK_IS_RADIO_BUTTON( button ), FALSE );
	dnd_main_tabs_write_settings( self, button );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-quit-on-escape" );
	g_return_val_if_fail( button && GTK_IS_CHECK_BUTTON( button ), FALSE );
	my_isettings_set_boolean(
			settings, HUB_USER_SETTINGS_GROUP,
			st_assistant_quit_on_escape,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( button )));

	my_isettings_set_boolean(
			settings, HUB_USER_SETTINGS_GROUP,
			st_assistant_confirm_on_escape,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->confirm_on_escape_btn )));

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-confirm-on-cancel" );
	g_return_val_if_fail( button && GTK_IS_CHECK_BUTTON( button ), FALSE );
	my_isettings_set_boolean(
			settings, HUB_USER_SETTINGS_GROUP,
			st_assistant_confirm_on_cancel,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( button )));

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-confirm-altf4" );
	g_return_val_if_fail( button && GTK_IS_CHECK_BUTTON( button ), FALSE );
	my_isettings_set_boolean(
			settings, HUB_USER_SETTINGS_GROUP,
			st_appli_confirm_on_altf4,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( button )));

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-confirm-quit" );
	g_return_val_if_fail( button && GTK_IS_CHECK_BUTTON( button ), FALSE );
	my_isettings_set_boolean(
			settings, HUB_USER_SETTINGS_GROUP,
			st_appli_confirm_on_quit,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( button )));

	return( TRUE );
}

/**
 * ofa_prefs_dnd_reorder:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if the user can reorder the main tabs.
 */
gboolean
ofa_prefs_dnd_reorder( ofaIGetter *getter )
{
	gchar *str;
	gboolean reorder;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	str = dnd_main_tabs_read_settings( getter );
	reorder = ( my_collate( str, SETTINGS_REORDER ) == 0 );
	g_free( str );

	return( reorder );
}

/**
 * ofa_prefs_dnd_detach:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if the user can detach the main tabs.
 */
gboolean
ofa_prefs_dnd_detach( ofaIGetter *getter )
{
	gchar *str;
	gboolean detach;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	str = dnd_main_tabs_read_settings( getter );
	detach = ( my_collate( str, SETTINGS_DETACH ) == 0 );
	g_free( str );

	return( detach );
}

/*
 * DndMainTabs settings: R(reorder) | D(detach);
 * Defaults is Reorder
 */
static gchar *
dnd_main_tabs_read_settings( ofaIGetter *getter )
{
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;
	gchar *str;

	settings = ofa_igetter_get_user_settings( getter );

	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, st_dnd_main_tabs );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_collate( cstr, SETTINGS_REORDER ) != 0 && my_collate( cstr, SETTINGS_DETACH ) != 0 ){
		str = g_strdup( SETTINGS_REORDER );
	} else {
		str = g_strdup( cstr );
	}

	return( str );
}

static void
dnd_main_tabs_write_settings( ofaPreferences *self, GtkWidget *reorder_btn )
{
	ofaPreferencesPrivate *priv;
	myISettings *settings;
	gchar *str;

	priv = ofa_preferences_get_instance_private( self );

	str = g_strdup_printf( "%s;",
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( reorder_btn )) ? SETTINGS_REORDER : SETTINGS_DETACH );

	settings = ofa_igetter_get_user_settings( priv->getter );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, st_dnd_main_tabs, str );

	g_free( str );
}

/**
 * ofa_prefs_assistant_quit_on_escape:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if assistant can be quit on Escape key.
 */
gboolean
ofa_prefs_assistant_quit_on_escape( ofaIGetter *getter )
{
	myISettings *settings;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	settings = ofa_igetter_get_user_settings( getter );

	return( my_isettings_get_boolean( settings, HUB_USER_SETTINGS_GROUP, st_assistant_quit_on_escape ));
}

/**
 * ofa_prefs_assistant_confirm_on_escape:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if confirmation is required when quitting an assistant
 * on Escape key.
 */
gboolean
ofa_prefs_assistant_confirm_on_escape( ofaIGetter *getter )
{
	myISettings *settings;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	settings = ofa_igetter_get_user_settings( getter );

	return( my_isettings_get_boolean( settings, HUB_USER_SETTINGS_GROUP, st_assistant_confirm_on_escape ));
}

/**
 * ofa_prefs_assistant_confirm_on_cancel:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if confirmation is required when quitting an assistant
 * on Cancel key.
 */
gboolean
ofa_prefs_assistant_confirm_on_cancel( ofaIGetter *getter )
{
	myISettings *settings;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	settings = ofa_igetter_get_user_settings( getter );

	return( my_isettings_get_boolean( settings, HUB_USER_SETTINGS_GROUP, st_assistant_confirm_on_cancel ));
}

/**
 * ofa_prefs_assistant_is_willing_to_quit:
 * @getter: a #ofaIGetter instance.
 * @keyval: the hit key.
 *
 * Returns: %TRUE if the assistant can quit.
 */
gboolean
ofa_prefs_assistant_is_willing_to_quit( ofaIGetter *getter, guint keyval )
{
	static const gchar *thisfn = "ofa_prefs_assistant_is_willing_to_quit";
	gboolean ok_escape, ok_cancel;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	ok_escape = ( keyval == GDK_KEY_Escape &&
					ofa_prefs_assistant_quit_on_escape( getter ) &&
					(!ofa_prefs_assistant_confirm_on_escape( getter ) || is_willing_to_quit()));
	g_debug( "%s: ok_escape=%s", thisfn, ok_escape ? "True":"False" );

	ok_cancel = ( keyval == GDK_KEY_Cancel &&
					(!ofa_prefs_assistant_confirm_on_cancel( getter ) || is_willing_to_quit()));
	g_debug( "%s: ok_cancel=%s", thisfn, ok_cancel ? "True":"False" );

	return( ok_escape || ok_cancel );
}

static gboolean
is_willing_to_quit( void )
{
	gboolean ok;

	ok = my_utils_dialog_question(
			NULL,
			_( "Are you sure you want to quit this assistant ?" ),
			_( "_Quit" ));

	return( ok );
}

/**
 * ofa_prefs_appli_confirm_on_altf4:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if a confirmation is required when quitting the
 * application on Alt+F4 key.
 */
gboolean
ofa_prefs_appli_confirm_on_altf4( ofaIGetter *getter )
{
	myISettings *settings;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	settings = ofa_igetter_get_user_settings( getter );

	return( my_isettings_get_boolean( settings, HUB_USER_SETTINGS_GROUP, st_appli_confirm_on_altf4 ));
}

/**
 * ofa_prefs_appli_confirm_on_quit:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if a confirmation is required when quitting the
 * application.
 */
gboolean
ofa_prefs_appli_confirm_on_quit( ofaIGetter *getter )
{
	myISettings *settings;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	settings = ofa_igetter_get_user_settings( getter );

	return( my_isettings_get_boolean( settings, HUB_USER_SETTINGS_GROUP, st_appli_confirm_on_quit ));
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

static gboolean
do_update_account_page( ofaPreferences *self, gchar **msgerrr )
{
	ofaPreferencesPrivate *priv;
	myISettings *settings;
	GtkWidget *button;

	priv = ofa_preferences_get_instance_private( self );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-delete-with-child" );
	g_return_val_if_fail( button && GTK_IS_CHECK_BUTTON( button ), FALSE );

	settings = ofa_igetter_get_user_settings( priv->getter );
	my_isettings_set_boolean(
			settings, HUB_USER_SETTINGS_GROUP,
			st_account_delete_root_with_child,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( button )));

	return( TRUE );
}

/**
 * ofa_prefs_account_delete_root_with_child:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if deleting a root account also deletes its children.
 */
gboolean
ofa_prefs_account_delete_root_with_children( ofaIGetter *getter )
{
	myISettings *settings;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	settings = ofa_igetter_get_user_settings( getter );

	return( my_isettings_get_boolean( settings, HUB_USER_SETTINGS_GROUP, st_account_delete_root_with_child ));
}

static gboolean
do_update_locales_page( ofaPreferences *self, gchar **msgerr )
{
	ofaPreferencesPrivate *priv;
	myISettings *settings;
	GList *list;
	const gchar *cstr;
	gchar *decimal_sep;
	gchar *str;

	priv = ofa_preferences_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );

	str = g_strdup_printf( "%d;%d;%s;",
			my_date_combo_get_selected( priv->p4_display_combo ),
			my_date_combo_get_selected( priv->p4_check_combo ),
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p4_date_over )) ? "True":"False" );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, SETTINGS_DATE, str );
	g_free( str );

	decimal_sep = my_decimal_combo_get_selected( priv->p4_decimal_sep );
	list = g_list_append( NULL, decimal_sep );

	cstr = gtk_entry_get_text( GTK_ENTRY( priv->p4_thousand_sep ));
	list = g_list_append( list, g_strdup( cstr ));

	str = g_strdup_printf( "%s",
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p4_accept_dot )) ? "True" : "False" );
	list = g_list_append( list, str );

	str = g_strdup_printf( "%s",
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p4_accept_comma )) ? "True" : "False" );
	list = g_list_append( list, str );

	my_isettings_set_string_list( settings, HUB_USER_SETTINGS_GROUP, SETTINGS_AMOUNT, list );
	my_isettings_free_string_list( settings, list );

	/* reinitialize the cache */
	st_date_prefs_set = FALSE;
	st_amount_prefs_set = FALSE;

	return( TRUE );
}

/**
 * ofa_prefs_date_display:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the prefered format for displaying the dates
 */
myDateFormat
ofa_prefs_date_display( ofaIGetter *getter )
{
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), MY_DATE_YYMD );

	if( !st_date_prefs_set ){
		setup_date_formats( getter );
	}

	return( st_date_display );
}

/**
 * ofa_prefs_date_check:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the prefered format for visually checking the dates
 */
myDateFormat
ofa_prefs_date_check( ofaIGetter *getter )
{
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), MY_DATE_YYMD );

	if( !st_date_prefs_set ){
		setup_date_formats( getter );
	}

	return( st_date_check );
}

/**
 * ofa_prefs_date_overwrite:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: whether the edition should start in overwrite mode.
 */
gboolean
ofa_prefs_date_overwrite( ofaIGetter *getter )
{
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	if( !st_date_prefs_set ){
		setup_date_formats( getter );
	}

	return( st_date_overwrite );
}

/*
 * settings = display_format(i); check_format(i); overwrite(b);
 */
static void
setup_date_formats( ofaIGetter *getter )
{
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;

	settings = ofa_igetter_get_user_settings( getter );

	/* have a suitable default value */
	st_date_display = MY_DATE_DMYY;
	st_date_check = MY_DATE_DMMM;
	st_date_overwrite = FALSE;

	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, SETTINGS_DATE );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		st_date_display = atoi( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		st_date_check = atoi( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		st_date_overwrite = my_utils_boolean_from_str( cstr );
	}

	my_isettings_free_string_list( settings, strlist );
	st_date_prefs_set = TRUE;
}

/**
 * ofa_prefs_amount_decimal_sep:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the prefered decimal separator (for display)
 *
 * The returned string should be g_free() by the caller.
 */
const gchar *
ofa_prefs_amount_decimal_sep( ofaIGetter *getter )
{
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	if( !st_amount_prefs_set ){
		setup_amount_formats( getter );
	}

	return( st_amount_decimal );
}

/**
 * ofa_prefs_amount_thousand_sep:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the prefered thousand separator (for display)
 *
 * The returned string should be g_free() by the caller.
 */
const gchar *
ofa_prefs_amount_thousand_sep( ofaIGetter *getter )
{
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	if( !st_amount_prefs_set ){
		setup_amount_formats( getter );
	}

	return( st_amount_thousand );
}

/**
 * ofa_prefs_amount_accept_dot:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: whether the user accepts dot as a decimal separator
 */
gboolean
ofa_prefs_amount_accept_dot( ofaIGetter *getter )
{
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	if( !st_amount_prefs_set ){
		setup_amount_formats( getter );
	}

	return( st_amount_accept_dot );
}

/**
 * ofa_prefs_amount_accept_comma:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: whether the user accepts comma as a decimal separator
 */
gboolean
ofa_prefs_amount_accept_comma( ofaIGetter *getter )
{
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	if( !st_amount_prefs_set ){
		setup_amount_formats( getter );
	}

	return( st_amount_accept_comma );
}

/*
 * settings = decimal_char;thousand_char;accept_dot;accept_comma;
 */
static void
setup_amount_formats( ofaIGetter *getter )
{
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;

	/* have a suitable default value (fr locale) */
	st_amount_decimal = g_strdup( "," );
	st_amount_thousand = g_strdup( " " );
	st_amount_accept_dot = TRUE;
	st_amount_accept_comma = TRUE;

	settings = ofa_igetter_get_user_settings( getter );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, SETTINGS_AMOUNT );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		g_free( st_amount_decimal );
		st_amount_decimal = g_strdup( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		g_free( st_amount_thousand );
		st_amount_thousand = g_strdup( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		st_amount_accept_dot = my_utils_boolean_from_str( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		st_amount_accept_comma = my_utils_boolean_from_str( cstr );
	}

	my_isettings_free_string_list( settings, strlist );
	st_amount_prefs_set = TRUE;
}

static gboolean
do_update_export_page( ofaPreferences *self, gchar **msgerr )
{
	ofaPreferencesPrivate *priv;
	myISettings *settings;
	gchar *text;

	priv = ofa_preferences_get_instance_private( self );

	my_ibin_apply( MY_IBIN( priv->export_settings ));

	text = gtk_file_chooser_get_uri( priv->p5_chooser );
	if( my_strlen( text )){
		settings = ofa_igetter_get_user_settings( priv->getter );
		my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, st_export_default_folder, text );
	}
	g_free( text );

	return( TRUE );
}

/**
 * ofa_prefs_export_default_folder:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the default export folder as a newly allocated string which
 * should be #g_free() by the caller.
 */
gchar *
ofa_prefs_export_default_folder( ofaIGetter *getter )
{
	myISettings *settings;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	settings = ofa_igetter_get_user_settings( getter );

	return( my_isettings_get_string( settings, HUB_USER_SETTINGS_GROUP, st_export_default_folder ));
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
