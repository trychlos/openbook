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

#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-date-combo.h"
#include "my/my-decimal-combo.h"
#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-extender-collection.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-iproperties.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"

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

	/* UI - General
	 */
	GtkWidget                *book;			/* main notebook of the dialog */
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
	ofaOpenPrefsBin          *prefs_bin;
	ofaDossierDeletePrefsBin *dd_prefs;

	/* UI - Account delete page
	 */

	/* UI - Locales
	 */
	myDateCombo              *p4_display_combo;
	myDateCombo              *p4_check_combo;
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

#define IPROPERTIES_PAGE                              "ofaIProperties"

/* a cache for some often used preferences
 */
static gboolean     st_date_prefs_set                 = FALSE;
static myDateFormat st_date_display                   = 0;
static myDateFormat st_date_check                     = 0;
static gboolean     st_amount_prefs_set               = FALSE;
static gchar       *st_amount_decimal                 = NULL;
static gchar       *st_amount_thousand                = NULL;
static gboolean     st_amount_accept_dot              = FALSE;
static gboolean     st_amount_accept_comma            = FALSE;

static const gchar *st_assistant_quit_on_escape       = "AssistantQuitOnEscape";
static const gchar *st_assistant_confirm_on_escape    = "AssistantConfirmOnEscape";
static const gchar *st_assistant_confirm_on_cancel    = "AssistantConfirmOnCancel";
static const gchar *st_appli_confirm_on_quit          = "ApplicationConfirmOnQuit";
static const gchar *st_appli_confirm_on_altf4         = "ApplicationConfirmOnAltF4";
static const gchar *st_dossier_open_notes             = "DossierOpenNotes";
static const gchar *st_dossier_open_notes_if_empty    = "DossierOpenNotesIfNonEmpty";
static const gchar *st_dossier_open_properties        = "DossierOpenProperties";
static const gchar *st_dossier_open_balance           = "DossierOpenBalance";
static const gchar *st_dossier_open_integrity         = "DossierOpenIntegrity";
static const gchar *st_account_delete_root_with_child = "AssistantConfirmOnCancel";

static const gchar *st_resource_ui                    = "/org/trychlos/openbook/core/ofa-preferences.ui";

typedef gboolean ( *pfnPlugin )( ofaPreferences *, gchar **msgerr, ofaIProperties * );

static void           iwindow_iface_init( myIWindowInterface *iface );
static void           idialog_iface_init( myIDialogInterface *iface );
static void           idialog_init( myIDialog *instance );
static void           init_quitting_page( ofaPreferences *self );
static void           init_dossier_page( ofaPreferences *self );
static void           init_account_page( ofaPreferences *self );
static void           init_locales_page( ofaPreferences *self );
static void           init_locale_date( ofaPreferences *self, myDateCombo **wcombo, const gchar *label, const gchar *parent, myDateFormat ivalue );
static void           init_locale_sep( ofaPreferences *self, GtkWidget **wentry, const gchar *label, const gchar *wname, const gchar *svalue );
static void           init_export_page( ofaPreferences *self );
static void           init_import_page( ofaPreferences *self );
static gboolean       enumerate_prefs_plugins( ofaPreferences *self, gchar **msgerr, pfnPlugin pfn );
static gboolean       init_plugin_page( ofaPreferences *self, gchar **msgerr, ofaIProperties *plugin );
//static void           activate_first_page( ofaPreferences *self );
static void           on_quit_on_escape_toggled( GtkToggleButton *button, ofaPreferences *self );
static void           on_display_date_changed( GtkComboBox *box, ofaPreferences *self );
static void           on_check_date_changed( GtkComboBox *box, ofaPreferences *self );
static void           on_date_changed( ofaPreferences *self, GtkComboBox *box, const gchar *sample_name );
static void           on_accept_dot_toggled( GtkToggleButton *toggle, ofaPreferences *self );
static void           on_accept_comma_toggled( GtkToggleButton *toggle, ofaPreferences *self );
static void           check_for_activable_dlg( ofaPreferences *self );
static gboolean       do_update( ofaPreferences *self, gchar **msgerr );
static gboolean       do_update_quitting_page( ofaPreferences *self, gchar **msgerr );
static gboolean       is_willing_to_quit( void );
static gboolean       do_update_dossier_page( ofaPreferences *self, gchar **msgerr );
static gboolean       do_update_account_page( ofaPreferences *self, gchar **msgerr );
static gboolean       do_update_locales_page( ofaPreferences *self, gchar **msgerr );
/*static void       error_decimal_sep( ofaPreferences *self );*/
static void           setup_date_formats( void );
static void           setup_amount_formats( void );
static gboolean       do_update_export_page( ofaPreferences *self, gchar **msgerr );
static gboolean       do_update_import_page( ofaPreferences *self, gchar **msgerr );
static gboolean       update_prefs_plugin( ofaPreferences *self, gchar **msgerr );

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
 * @parent: [alllow-none]: the #GtkWindow parent.
 * @plugin: [allow-null]: the #ofaExtenderModule object for which the
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
	my_iwindow_set_parent( MY_IWINDOW( self ), parent );
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

	priv = ofa_preferences_get_instance_private( self );

	priv->getter = getter;
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

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_preferences_get_instance_private( OFA_PREFERENCES( instance ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	my_idialog_click_to_update( instance, priv->ok_btn, ( myIDialogUpdateCb ) do_update );

	priv->book = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "notebook" );
	g_return_if_fail( priv->book && GTK_IS_NOTEBOOK( priv->book ));

	init_quitting_page( OFA_PREFERENCES( instance ));
	init_dossier_page( OFA_PREFERENCES( instance ));
	init_account_page( OFA_PREFERENCES( instance ));
	init_locales_page( OFA_PREFERENCES( instance ));
	init_export_page( OFA_PREFERENCES( instance ));
	init_import_page( OFA_PREFERENCES( instance ));
	enumerate_prefs_plugins( OFA_PREFERENCES( instance ), NULL, init_plugin_page );

	gtk_widget_show_all( GTK_WIDGET( instance ));
}

static void
init_quitting_page( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	GtkWidget *button;
	gboolean bvalue;

	priv = ofa_preferences_get_instance_private( self );

	/* priv->confirm_on_escape_btn is set before acting on
	 *  quit-on-escape button as triggered signal use the variable */
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-confirm-on-escape" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_assistant_confirm_on_escape();
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
	priv->confirm_on_escape_btn = GTK_CHECK_BUTTON( button );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-quit-on-escape" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "toggled", G_CALLBACK( on_quit_on_escape_toggled ), self );
	bvalue = ofa_prefs_assistant_quit_on_escape();
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
	on_quit_on_escape_toggled( GTK_TOGGLE_BUTTON( button ), self );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-confirm-on-cancel" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_assistant_confirm_on_cancel();
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-confirm-altf4" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_appli_confirm_on_altf4();
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-confirm-quit" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_appli_confirm_on_quit();
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
}

static void
init_dossier_page( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	GtkWidget *parent;

	priv = ofa_preferences_get_instance_private( self );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "prefs-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->prefs_bin = ofa_open_prefs_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->prefs_bin ));

	ofa_open_prefs_bin_set_data( priv->prefs_bin,
			ofa_prefs_dossier_open_notes(),
			ofa_prefs_dossier_open_notes_if_empty(),
			ofa_prefs_dossier_open_properties(),
			ofa_prefs_dossier_open_balance(),
			ofa_prefs_dossier_open_integrity());

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "dossier-delete-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->dd_prefs = ofa_dossier_delete_prefs_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->dd_prefs ));
}

static void
init_account_page( ofaPreferences *self )
{
	GtkWidget *button;
	gboolean bvalue;

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-delete-with-child" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_account_delete_root_with_children();
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
			&priv->p4_display_combo, "p4-display-label", "p4-display-parent", ofa_prefs_date_display());
	g_signal_connect( priv->p4_display_combo, "changed", G_CALLBACK( on_display_date_changed ), self );
	on_display_date_changed( GTK_COMBO_BOX( priv->p4_display_combo ), self );

	init_locale_date( self,
			&priv->p4_check_combo,   "p4-check-label",  "p4-check-parent",   ofa_prefs_date_check());
	g_signal_connect( priv->p4_check_combo, "changed", G_CALLBACK( on_check_date_changed ), self );
	on_check_date_changed( GTK_COMBO_BOX( priv->p4_check_combo ), self );

	/* decimal display */
	priv->p4_decimal_sep = my_decimal_combo_new();

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-decimal-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p4_decimal_sep ));
	my_decimal_combo_set_selected( priv->p4_decimal_sep, ofa_prefs_amount_decimal_sep());

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-decimal-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->p4_decimal_sep ));

	/* accept dot decimal separator */
	check = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-accept-dot" );
	g_return_if_fail( check && GTK_IS_CHECK_BUTTON( check ));
	priv->p4_accept_dot = check;
	g_signal_connect( check, "toggled", G_CALLBACK( on_accept_dot_toggled ), self );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check ), ofa_prefs_amount_accept_dot());
	on_accept_dot_toggled( GTK_TOGGLE_BUTTON( check ), self );

	/* accept comma decimal separator */
	check = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-accept-comma" );
	g_return_if_fail( check && GTK_IS_CHECK_BUTTON( check ));
	priv->p4_accept_comma = check;
	g_signal_connect( check, "toggled", G_CALLBACK( on_accept_comma_toggled ), self );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check ), ofa_prefs_amount_accept_comma());
	on_accept_comma_toggled( GTK_TOGGLE_BUTTON( check ), self );

	/* thousand separator */
	init_locale_sep( self,
			&priv->p4_thousand_sep, "p4-thousand-label", "p4-thousand-sep", ofa_prefs_amount_thousand_sep());
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
	GtkWidget *target, *label, *entry, *combo;
	gchar *str;
	GtkSizeGroup *group;
	ofaStreamFormat *settings;

	priv = ofa_preferences_get_instance_private( self );

	group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	target = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-export-parent" );
	g_return_if_fail( target && GTK_IS_CONTAINER( target ));

	settings = ofa_stream_format_new( NULL, OFA_SFMODE_EXPORT );
	priv->export_settings = ofa_stream_format_bin_new( settings );
	g_object_unref( settings );
	gtk_container_add( GTK_CONTAINER( target ), GTK_WIDGET( priv->export_settings ));
	my_utils_size_group_add_size_group(
			group, ofa_stream_format_bin_get_size_group( priv->export_settings, 0 ));

	entry = ofa_stream_format_bin_get_name_entry( priv->export_settings );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	gtk_widget_set_sensitive( entry, FALSE );

	combo = ofa_stream_format_bin_get_mode_combo( priv->export_settings );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));
	gtk_widget_set_sensitive( combo, FALSE );

	priv->p5_chooser = GTK_FILE_CHOOSER( my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p52-folder" ));
	str = ofa_settings_user_get_string( SETTINGS_EXPORT_FOLDER );
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
	GtkWidget *target, *entry, *combo;
	ofaStreamFormat *settings;

	priv = ofa_preferences_get_instance_private( self );

	target = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p6-import-parent" );
	g_return_if_fail( target && GTK_IS_CONTAINER( target ));

	settings = ofa_stream_format_new( NULL, OFA_SFMODE_IMPORT );
	priv->import_settings = ofa_stream_format_bin_new( settings );
	g_object_unref( settings );
	gtk_container_add( GTK_CONTAINER( target ), GTK_WIDGET( priv->import_settings ));

	entry = ofa_stream_format_bin_get_name_entry( priv->import_settings );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	gtk_widget_set_sensitive( entry, FALSE );

	combo = ofa_stream_format_bin_get_mode_combo( priv->import_settings );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));
	gtk_widget_set_sensitive( combo, FALSE );
}

static gboolean
enumerate_prefs_plugins( ofaPreferences *self, gchar **msgerr, pfnPlugin pfn )
{
	ofaPreferencesPrivate *priv;
	ofaHub *hub;
	ofaExtenderCollection *extenders;
	GList *list, *it;
	gboolean ok;

	priv = ofa_preferences_get_instance_private( self );

	ok = TRUE;
	hub = ofa_igetter_get_hub( priv->getter );
	extenders = ofa_hub_get_extender_collection( hub );
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
	myISettings *settings;
	gboolean ok;

	g_debug( "%s: self=%p, msgerr=%p, instance=%p (%s)",
			thisfn, ( void * ) self, ( void * ) msgerr, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	priv = ofa_preferences_get_instance_private( self );
	settings = ofa_settings_get_settings( SETTINGS_TARGET_USER );

	ok = FALSE;
	page = ofa_iproperties_init( instance, settings );
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
 */
static void
check_for_activable_dlg( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	gboolean accept_dot, accept_comma;
	gboolean activable;

	priv = ofa_preferences_get_instance_private( self );
	activable = TRUE;

	if( !priv->p4_accept_dot || !priv->p4_accept_comma ){
		activable = FALSE;
	} else {
		accept_dot = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p4_accept_dot ));
		accept_comma = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p4_accept_comma ));
		activable &= ( accept_dot || accept_comma );
	}

	gtk_widget_set_sensitive( priv->ok_btn, activable );
}

static gboolean
do_update( ofaPreferences *self, gchar **msgerr )
{
	gboolean ok;

	ok = do_update_quitting_page( self, msgerr ) &&
			do_update_dossier_page( self, msgerr ) &&
			do_update_account_page( self, msgerr ) &&
			do_update_locales_page( self, msgerr ) &&
			do_update_export_page( self, msgerr ) &&
			do_update_import_page( self, msgerr ) &&
			update_prefs_plugin( self, msgerr );

	return( ok );
}

static gboolean
do_update_quitting_page( ofaPreferences *self, gchar **msgerr )
{
	ofaPreferencesPrivate *priv;
	GtkWidget *button;

	priv = ofa_preferences_get_instance_private( self );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-quit-on-escape" );
	g_return_val_if_fail( button && GTK_IS_CHECK_BUTTON( button ), FALSE );
	ofa_settings_user_set_boolean(
			st_assistant_quit_on_escape,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( button )));

	ofa_settings_user_set_boolean(
			st_assistant_confirm_on_escape,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->confirm_on_escape_btn )));

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-confirm-on-cancel" );
	g_return_val_if_fail( button && GTK_IS_CHECK_BUTTON( button ), FALSE );
	ofa_settings_user_set_boolean(
			st_assistant_confirm_on_cancel,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( button )));

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-confirm-altf4" );
	g_return_val_if_fail( button && GTK_IS_CHECK_BUTTON( button ), FALSE );
	ofa_settings_user_set_boolean(
			st_appli_confirm_on_altf4,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( button )));

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-confirm-quit" );
	g_return_val_if_fail( button && GTK_IS_CHECK_BUTTON( button ), FALSE );
	ofa_settings_user_set_boolean(
			st_appli_confirm_on_quit,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( button )));

	return( TRUE );
}

/**
 * ofa_prefs_assistant_quit_on_escape:
 */
gboolean
ofa_prefs_assistant_quit_on_escape( void )
{
	return( ofa_settings_user_get_boolean( st_assistant_quit_on_escape ));
}

/**
 * ofa_prefs_assistant_confirm_on_escape:
 */
gboolean
ofa_prefs_assistant_confirm_on_escape( void )
{
	return( ofa_settings_user_get_boolean( st_assistant_confirm_on_escape ));
}

/**
 * ofa_prefs_assistant_confirm_on_cancel:
 */
gboolean
ofa_prefs_assistant_confirm_on_cancel( void )
{
	return( ofa_settings_user_get_boolean( st_assistant_confirm_on_cancel ));
}

/**
 * ofa_prefs_assistant_is_willing_to_quit:
 */
gboolean
ofa_prefs_assistant_is_willing_to_quit( guint keyval )
{
	gboolean ok;

	ok = (( keyval == GDK_KEY_Escape &&
			ofa_prefs_assistant_quit_on_escape() && (!ofa_prefs_assistant_confirm_on_escape() || is_willing_to_quit())) ||
			( keyval == GDK_KEY_Cancel && (!ofa_prefs_assistant_confirm_on_cancel() || is_willing_to_quit())));

	return( ok );
}

static gboolean
is_willing_to_quit( void )
{
	gboolean ok;

	ok = my_utils_dialog_question(
			_( "Are you sure you want to quit this assistant ?" ),
			_( "_Quit" ));

	return( ok );
}

/**
 * ofa_prefs_appli_confirm_on_altf4:
 */
gboolean
ofa_prefs_appli_confirm_on_altf4( void )
{
	return( ofa_settings_user_get_boolean( st_appli_confirm_on_altf4 ));
}

/**
 * ofa_prefs_appli_confirm_on_quit:
 */
gboolean
ofa_prefs_appli_confirm_on_quit( void )
{
	return( ofa_settings_user_get_boolean( st_appli_confirm_on_quit ));
}

static gboolean
do_update_dossier_page( ofaPreferences *self, gchar **msgerr )
{
	ofaPreferencesPrivate *priv;
	gboolean notes, nonempty, props, bals, integ;

	priv = ofa_preferences_get_instance_private( self );

	ofa_open_prefs_bin_get_data( priv->prefs_bin, &notes, &nonempty, &props, &bals, &integ );

	ofa_settings_user_set_boolean( st_dossier_open_notes, notes );
	ofa_settings_user_set_boolean( st_dossier_open_notes_if_empty, nonempty );
	ofa_settings_user_set_boolean( st_dossier_open_properties, props );
	ofa_settings_user_set_boolean( st_dossier_open_balance, bals );
	ofa_settings_user_set_boolean( st_dossier_open_integrity, integ );

	ofa_dossier_delete_prefs_bin_set_settings( priv->dd_prefs );

	return( TRUE );
}

/**
 * ofa_prefs_dossier_open_notes:
 */
gboolean
ofa_prefs_dossier_open_notes( void )
{
	return( ofa_settings_user_get_boolean( st_dossier_open_notes ));
}

/**
 * ofa_prefs_dossier_open_notes_if_empty:
 */
gboolean
ofa_prefs_dossier_open_notes_if_empty( void )
{
	return( ofa_settings_user_get_boolean( st_dossier_open_notes_if_empty ));
}

/**
 * ofa_prefs_dossier_open_properties:
 */
gboolean
ofa_prefs_dossier_open_properties( void )
{
	return( ofa_settings_user_get_boolean( st_dossier_open_properties ));
}

/**
 * ofa_prefs_dossier_open_balance:
 */
gboolean
ofa_prefs_dossier_open_balance( void )
{
	return( ofa_settings_user_get_boolean( st_dossier_open_balance ));
}

/**
 * ofa_prefs_dossier_open_integrity:
 */
gboolean
ofa_prefs_dossier_open_integrity( void )
{
	return( ofa_settings_user_get_boolean( st_dossier_open_integrity ));
}

static gboolean
do_update_account_page( ofaPreferences *self, gchar **msgerrr )
{
	GtkWidget *button;

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-delete-with-child" );
	g_return_val_if_fail( button && GTK_IS_CHECK_BUTTON( button ), FALSE );
	ofa_settings_user_set_boolean(
			st_account_delete_root_with_child,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( button )));

	return( TRUE );
}

/**
 * ofa_prefs_account_delete_root_with_child:
 */
gboolean
ofa_prefs_account_delete_root_with_children( void )
{
	return( ofa_settings_user_get_boolean( st_account_delete_root_with_child ));
}

static gboolean
do_update_locales_page( ofaPreferences *self, gchar **msgerr )
{
	ofaPreferencesPrivate *priv;
	GList *list;
	const gchar *cstr;
	gchar *decimal_sep;
	gchar *str;

	priv = ofa_preferences_get_instance_private( self );

	list = g_list_append( NULL, GINT_TO_POINTER( my_date_combo_get_selected( priv->p4_display_combo )));
	list = g_list_append( list, GINT_TO_POINTER( my_date_combo_get_selected( priv->p4_check_combo )));
	ofa_settings_user_set_uint_list( SETTINGS_DATE, list );
	ofa_settings_free_uint_list( list );

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

	ofa_settings_user_set_string_list( SETTINGS_AMOUNT, list );
	ofa_settings_free_string_list( list );

	/* reinitialize the cache */
	st_date_prefs_set = FALSE;
	st_amount_prefs_set = FALSE;

	return( TRUE );
}

/**
 * ofa_prefs_date_display:
 *
 * Returns: the prefered format for displaying the dates
 */
myDateFormat
ofa_prefs_date_display( void )
{
	if( !st_date_prefs_set ){
		setup_date_formats();
	}

	return( st_date_display );
}

/**
 * ofa_prefs_date_check:
 *
 * Returns: the prefered format for visually checking the dates
 */
myDateFormat
ofa_prefs_date_check( void )
{
	if( !st_date_prefs_set ){
		setup_date_formats();
	}

	return( st_date_check );
}

/*
 * settings = display_format;check_format;
 */
static void
setup_date_formats( void )
{
	GList *list, *it;

	/* have a suitable default value */
	st_date_display = MY_DATE_DMYY;
	st_date_check = MY_DATE_DMMM;

	list = ofa_settings_user_get_uint_list( SETTINGS_DATE );
	if( list ){
		if( list->data ){
			st_date_display = GPOINTER_TO_INT( list->data );
		}
		it = list->next;
		if( it->data ){
			st_date_check = GPOINTER_TO_INT( it->data );
		}
	}
	ofa_settings_free_uint_list( list );
	st_date_prefs_set = TRUE;
}

/**
 * ofa_prefs_amount_decimal_sep:
 *
 * Returns: the prefered decimal separator (for display)
 *
 * The returned string should be g_free() by the caller.
 */
const gchar *
ofa_prefs_amount_decimal_sep( void )
{
	if( !st_amount_prefs_set ){
		setup_amount_formats();
	}

	return( st_amount_decimal );
}

/**
 * ofa_prefs_amount_thousand_sep:
 *
 * Returns: the prefered thousand separator (for display)
 *
 * The returned string should be g_free() by the caller.
 */
const gchar *
ofa_prefs_amount_thousand_sep( void )
{
	if( !st_amount_prefs_set ){
		setup_amount_formats();
	}

	return( st_amount_thousand );
}

/**
 * ofa_prefs_amount_accept_dot:
 *
 * Returns: whether the user accepts dot as a decimal separator
 */
gboolean
ofa_prefs_amount_accept_dot( void )
{
	if( !st_amount_prefs_set ){
		setup_amount_formats();
	}

	return( st_amount_accept_dot );
}

/**
 * ofa_prefs_amount_accept_comma:
 *
 * Returns: whether the user accepts comma as a decimal separator
 */
gboolean
ofa_prefs_amount_accept_comma( void )
{
	if( !st_amount_prefs_set ){
		setup_amount_formats();
	}

	return( st_amount_accept_comma );
}

/*
 * settings = decimal_char;thousand_char;accept_dot;accept_comma;
 */
static void
setup_amount_formats( void )
{
	GList *list, *it;
	const gchar *cstr;

	/* have a suitable default value (fr locale) */
	st_amount_decimal = g_strdup( "," );
	st_amount_thousand = g_strdup( " " );
	st_amount_accept_dot = TRUE;
	st_amount_accept_comma = TRUE;

	list = ofa_settings_user_get_string_list( SETTINGS_AMOUNT );
	if( list ){
		g_free( st_amount_decimal );
		st_amount_decimal = NULL;
		g_free( st_amount_thousand );
		st_amount_thousand = NULL;

		it = list;
		cstr = it ? ( const gchar * ) it->data : NULL;
		if( my_strlen( cstr )){
			st_amount_decimal = g_strdup( cstr );
		}

		it = it ? it->next : NULL;
		cstr = it ? ( const gchar * ) it->data : NULL;
		if( my_strlen( cstr )){
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
	}
	ofa_settings_free_string_list( list );
	st_amount_prefs_set = TRUE;
}

static gboolean
do_update_export_page( ofaPreferences *self, gchar **msgerr )
{
	ofaPreferencesPrivate *priv;
	gchar *text;

	priv = ofa_preferences_get_instance_private( self );

	ofa_stream_format_bin_apply( priv->export_settings );

	text = gtk_file_chooser_get_current_folder_uri( priv->p5_chooser );
	if( my_strlen( text )){
		ofa_settings_user_set_string( SETTINGS_EXPORT_FOLDER, text );
	}
	g_free( text );

	return( TRUE );
}

static gboolean
do_update_import_page( ofaPreferences *self, gchar **msgerr )
{
	ofaPreferencesPrivate *priv;

	priv = ofa_preferences_get_instance_private( self );

	ofa_stream_format_bin_apply( priv->import_settings );

	return( TRUE );
}

static gboolean
update_prefs_plugin( ofaPreferences *self, gchar **msgerr )
{
	ofaPreferencesPrivate *priv;
	gboolean ok;
	gint pages_count, i;
	GtkWidget *page;
	const gchar *cstr;

	priv = ofa_preferences_get_instance_private( self );

	ok = FALSE;
	pages_count = gtk_notebook_get_n_pages( GTK_NOTEBOOK( priv->book ));

	for( i=0 ; i<pages_count ; ++i ){
		page = gtk_notebook_get_nth_page( GTK_NOTEBOOK( priv->book ), i );
		cstr = g_object_get_data( G_OBJECT( page ), IPROPERTIES_PAGE );
		if( !my_collate( cstr, IPROPERTIES_PAGE )){
			ofa_iproperties_apply( page );
		}
	}

	return( ok );
}
