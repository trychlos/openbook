/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofa-ipreferences.h"
#include "api/ofa-preferences.h"
#include "api/ofa-plugin.h"
#include "api/ofa-settings.h"

#include "core/my-date-combo.h"
#include "core/my-decimal-combo.h"
#include "core/ofa-dossier-delete-prefs-bin.h"
#include "core/ofa-file-format-bin.h"

/* private instance data
 */
struct _ofaPreferencesPrivate {

	GtkWidget                *book;			/* main notebook of the dialog */
	GtkWidget                *btn_ok;

	/* whether the dialog has been validated
	 */
	gboolean                  updated;

	/* when opening the preferences from the plugin manager
	 */
	ofaPlugin                *plugin;
	GtkWidget                *object_page;

	/* UI - Quitting
	 */
	GtkCheckButton           *confirm_on_escape_btn;

	/* UI - Dossier delete page
	 */
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
	ofaFileFormatBin         *export_settings;
	GtkFileChooser           *p5_chooser;

	/* Import settings
	 */
	ofaFileFormatBin         *import_settings;

	/* UI - Plugin pages
	 */
	GList                    *plugs;
};

#define SETTINGS_AMOUNT                    "UserAmount"
#define SETTINGS_DATE                      "UserDate"

/* a cache for some often used preferences
 */
static gboolean     st_date_prefs_set      = FALSE;
static myDateFormat st_date_display        = 0;
static myDateFormat st_date_check          = 0;
static gboolean     st_amount_prefs_set    = FALSE;
static gchar       *st_amount_decimal      = NULL;
static gchar       *st_amount_thousand     = NULL;
static gboolean     st_amount_accept_dot   = FALSE;
static gboolean     st_amount_accept_comma = FALSE;

static const gchar *st_assistant_quit_on_escape       = "AssistantQuitOnEscape";
static const gchar *st_assistant_confirm_on_escape    = "AssistantConfirmOnEscape";
static const gchar *st_assistant_confirm_on_cancel    = "AssistantConfirmOnCancel";
static const gchar *st_appli_confirm_on_quit          = "ApplicationConfirmOnQuit";
static const gchar *st_appli_confirm_on_altf4         = "ApplicationConfirmOnAltF4";
static const gchar *st_account_delete_root_with_child = "AssistantConfirmOnCancel";

static const gchar *st_ui_xml                         = PKGUIDIR "/ofa-preferences.ui";
static const gchar *st_ui_id                          = "PreferencesDlg";

typedef struct {
	ofaIPreferences *object;
	GtkWidget       *page;
}
	sPagePlugin;

typedef void ( *pfnPlugin )( ofaPreferences *, ofaIPreferences * );

G_DEFINE_TYPE( ofaPreferences, ofa_preferences, MY_TYPE_DIALOG )

static void       v_init_dialog( myDialog *dialog );
static void       init_quitting_page( ofaPreferences *self, GtkContainer *toplevel );
static void       init_dossier_delete_page( ofaPreferences *self, GtkContainer *toplevel );
static void       init_account_page( ofaPreferences *self, GtkContainer *toplevel );
static void       init_locales_page( ofaPreferences *self, GtkContainer *toplevel );
/*static void       get_locales( void );*/
static void       init_locale_date( ofaPreferences *self, GtkContainer *toplevel, myDateCombo **wcombo, const gchar *label, const gchar *parent, myDateFormat ivalue );
static void       init_locale_sep( ofaPreferences *self, GtkContainer *toplevel, GtkWidget **wentry, const gchar *label, const gchar *wname, const gchar *svalue );
static void       init_export_page( ofaPreferences *self, GtkContainer *toplevel );
static void       init_import_page( ofaPreferences *self, GtkContainer *toplevel );
static void       enumerate_prefs_plugins( ofaPreferences *self, pfnPlugin pfn );
static void       init_plugin_page( ofaPreferences *self, ofaIPreferences *plugin );
static void       activate_first_page( ofaPreferences *self );
static void       on_quit_on_escape_toggled( GtkToggleButton *button, ofaPreferences *self );
static void       on_display_date_changed( GtkComboBox *box, ofaPreferences *self );
static void       on_check_date_changed( GtkComboBox *box, ofaPreferences *self );
static void       on_date_changed( ofaPreferences *self, GtkComboBox *box, const gchar *sample_name );
static void       on_accept_dot_toggled( GtkToggleButton *toggle, ofaPreferences *self );
static void       on_accept_comma_toggled( GtkToggleButton *toggle, ofaPreferences *self );
static void       check_for_activable_dlg( ofaPreferences *self );
static gboolean   v_quit_on_ok( myDialog *dialog );
static gboolean   do_update( ofaPreferences *self );
static void       do_update_quitting_page( ofaPreferences *self );
static void       do_update_account_page( ofaPreferences *self );
static gboolean   do_update_locales_page( ofaPreferences *self );
/*static void       error_decimal_sep( ofaPreferences *self );*/
static void       setup_date_formats( void );
static void       setup_amount_formats( void );
static void       do_update_export_page( ofaPreferences *self );
static void       do_update_import_page( ofaPreferences *self );
static void       update_prefs_plugin( ofaPreferences *self, ofaIPreferences *plugin );
static GtkWidget *find_prefs_plugin( ofaPreferences *self, ofaIPreferences *plugin );

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

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = OFA_PREFERENCES( instance )->priv;

		g_list_free_full( priv->plugs, ( GDestroyNotify ) g_free );
		priv->plugs = NULL;
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

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_PREFERENCES, ofaPreferencesPrivate );
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

	g_type_class_add_private( klass, sizeof( ofaPreferencesPrivate ));
}

/**
 * ofa_preferences_run:
 * @main: the main window of the application.
 * @plugin: [allow-null]: the #ofaPlugin object for which the properties
 *  are to be displayed.
 *
 * Update the properties of an dossier
 */
gboolean
ofa_preferences_run( GtkApplicationWindow *main_window, ofaPlugin *plugin )
{
	static const gchar *thisfn = "ofa_preferences_run";
	ofaPreferences *self;
	ofaPreferencesPrivate *priv;
	gboolean updated;

	g_return_val_if_fail( main_window && GTK_IS_APPLICATION_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new(
				OFA_TYPE_PREFERENCES,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	priv = self->priv;

	priv->plugin = plugin;
	priv->object_page = NULL;

	my_dialog_init_dialog( MY_DIALOG( self ));
	activate_first_page( self );
	my_dialog_run_dialog( MY_DIALOG( self ));

	updated = self->priv->updated;

	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaPreferencesPrivate *priv;
	GtkWindow *toplevel;

	priv = OFA_PREFERENCES( dialog )->priv;

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( toplevel && GTK_IS_WINDOW( toplevel ));

	priv->book = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "notebook" );
	g_return_if_fail( priv->book && GTK_IS_NOTEBOOK( priv->book ));

	init_quitting_page( OFA_PREFERENCES( dialog ), GTK_CONTAINER( toplevel ));
	init_dossier_delete_page( OFA_PREFERENCES( dialog ), GTK_CONTAINER( toplevel ));
	init_account_page( OFA_PREFERENCES( dialog ), GTK_CONTAINER( toplevel ));
	init_locales_page( OFA_PREFERENCES( dialog ), GTK_CONTAINER( toplevel ));
	init_export_page( OFA_PREFERENCES( dialog ), GTK_CONTAINER( toplevel ));
	init_import_page( OFA_PREFERENCES( dialog ), GTK_CONTAINER( toplevel ));
	enumerate_prefs_plugins( OFA_PREFERENCES( dialog ), init_plugin_page );
}

static void
init_quitting_page( ofaPreferences *self, GtkContainer *toplevel )
{
	ofaPreferencesPrivate *priv;
	GtkWidget *button;
	gboolean bvalue;

	priv = self->priv;

	/* priv->confirm_on_escape_btn is set before acting on
	 *  quit-on-escape button as triggered signal use the variable */
	button = my_utils_container_get_child_by_name( toplevel, "p1-confirm-on-escape" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_assistant_confirm_on_escape();
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), !bvalue );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
	priv->confirm_on_escape_btn = GTK_CHECK_BUTTON( button );

	button = my_utils_container_get_child_by_name( toplevel, "p1-quit-on-escape" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "toggled", G_CALLBACK( on_quit_on_escape_toggled ), self );
	bvalue = ofa_prefs_assistant_quit_on_escape();
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), !bvalue );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );

	button = my_utils_container_get_child_by_name( toplevel, "p1-confirm-on-cancel" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_assistant_confirm_on_cancel();
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), !bvalue );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );

	button = my_utils_container_get_child_by_name( toplevel, "p1-confirm-altf4" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_appli_confirm_on_altf4();
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );

	button = my_utils_container_get_child_by_name( toplevel, "p1-confirm-quit" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_appli_confirm_on_quit();
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
}

static void
init_dossier_delete_page( ofaPreferences *self, GtkContainer *toplevel )
{
	ofaPreferencesPrivate *priv;
	GtkWidget *parent;

	priv = self->priv;
	parent = my_utils_container_get_child_by_name( toplevel, "dossier-delete-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->dd_prefs = ofa_dossier_delete_prefs_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->dd_prefs ));
}

static void
init_account_page( ofaPreferences *self, GtkContainer *toplevel )
{
	GtkWidget *button;
	gboolean bvalue;

	button = my_utils_container_get_child_by_name( toplevel, "p4-delete-with-child" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_account_delete_root_with_children();
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
}

static void
init_locales_page( ofaPreferences *self, GtkContainer *toplevel )
{
	ofaPreferencesPrivate *priv;
	GtkWidget *parent, *label, *check;

	priv = self->priv;

	/*get_locales();*/

	init_locale_date( self, toplevel,
			&priv->p4_display_combo, "p4-display-label", "p4-display-parent", ofa_prefs_date_display());
	g_signal_connect( priv->p4_display_combo, "changed", G_CALLBACK( on_display_date_changed ), self );
	on_display_date_changed( GTK_COMBO_BOX( priv->p4_display_combo ), self );

	init_locale_date( self, toplevel,
			&priv->p4_check_combo,   "p4-check-label",  "p4-check-parent",   ofa_prefs_date_check());
	g_signal_connect( priv->p4_check_combo, "changed", G_CALLBACK( on_check_date_changed ), self );
	on_check_date_changed( GTK_COMBO_BOX( priv->p4_check_combo ), self );

	/* decimal display */
	priv->p4_decimal_sep = my_decimal_combo_new();

	parent = my_utils_container_get_child_by_name( toplevel, "p4-decimal-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p4_decimal_sep ));
	my_decimal_combo_set_selected( priv->p4_decimal_sep, ofa_prefs_amount_decimal_sep());

	label = my_utils_container_get_child_by_name( toplevel, "p4-decimal-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->p4_decimal_sep ));

	/* accept dot decimal separator */
	check = my_utils_container_get_child_by_name( toplevel, "p4-accept-dot" );
	g_return_if_fail( check && GTK_IS_CHECK_BUTTON( check ));
	priv->p4_accept_dot = check;
	g_signal_connect( check, "toggled", G_CALLBACK( on_accept_dot_toggled ), self );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check ), ofa_prefs_amount_accept_dot());
	on_accept_dot_toggled( GTK_TOGGLE_BUTTON( check ), self );

	/* accept comma decimal separator */
	check = my_utils_container_get_child_by_name( toplevel, "p4-accept-comma" );
	g_return_if_fail( check && GTK_IS_CHECK_BUTTON( check ));
	priv->p4_accept_comma = check;
	g_signal_connect( check, "toggled", G_CALLBACK( on_accept_comma_toggled ), self );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check ), ofa_prefs_amount_accept_comma());
	on_accept_comma_toggled( GTK_TOGGLE_BUTTON( check ), self );

	/* thousand separator */
	init_locale_sep( self, toplevel,
			&priv->p4_thousand_sep, "l-thousand", "p4-thousand-sep", ofa_prefs_amount_thousand_sep());
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
init_locale_date( ofaPreferences *self, GtkContainer *toplevel, myDateCombo **wcombo, const gchar *label_name, const gchar *parent, myDateFormat ivalue )
{
	GtkWidget *parent_widget, *label;

	parent_widget = my_utils_container_get_child_by_name( toplevel, parent );
	g_return_if_fail( parent_widget && GTK_IS_CONTAINER( parent_widget ));

	*wcombo = my_date_combo_new();
	gtk_container_add( GTK_CONTAINER( parent_widget ), GTK_WIDGET( *wcombo ));
	my_date_combo_set_selected( *wcombo, ivalue );

	label = my_utils_container_get_child_by_name( toplevel, label_name );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( *wcombo ));
}

static void
init_locale_sep( ofaPreferences *self, GtkContainer *toplevel, GtkWidget **wentry, const gchar *label_name, const gchar *wname, const gchar *svalue )
{
	GtkWidget *label;

	*wentry = my_utils_container_get_child_by_name( toplevel, wname );
	g_return_if_fail( *wentry && GTK_IS_ENTRY( *wentry ));

	gtk_entry_set_text( GTK_ENTRY( *wentry ), svalue );

	label = my_utils_container_get_child_by_name( toplevel, label_name );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( *wentry ));
}

static void
init_export_page( ofaPreferences *self, GtkContainer *toplevel )
{
	ofaPreferencesPrivate *priv;
	GtkWidget *target, *label;
	gchar *str;
	GtkSizeGroup *group;
	ofaFileFormat *settings;

	priv = self->priv;
	group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	target = my_utils_container_get_child_by_name( toplevel, "p5-export-parent" );
	g_return_if_fail( target && GTK_IS_CONTAINER( target ));

	settings = ofa_file_format_new( SETTINGS_EXPORT_SETTINGS );
	priv->export_settings = ofa_file_format_bin_new( settings );
	g_object_unref( settings );
	gtk_container_add( GTK_CONTAINER( target ), GTK_WIDGET( priv->export_settings ));
	my_utils_size_group_add_size_group(
			group, ofa_file_format_bin_get_size_group( priv->export_settings, 0 ));

	priv->p5_chooser = GTK_FILE_CHOOSER( my_utils_container_get_child_by_name( toplevel, "p52-folder" ));
	str = ofa_settings_get_string( SETTINGS_EXPORT_FOLDER );
	if( my_strlen( str )){
		gtk_file_chooser_set_current_folder_uri( priv->p5_chooser, str );
	}
	g_free( str );

	label = my_utils_container_get_child_by_name( toplevel, "p52-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->p5_chooser ));
	gtk_size_group_add_widget( group, label );

	g_object_unref( group );
}

static void
init_import_page( ofaPreferences *self, GtkContainer *toplevel )
{
	ofaPreferencesPrivate *priv;
	GtkWidget *target;
	ofaFileFormat *settings;

	priv = self->priv;

	target = my_utils_container_get_child_by_name( toplevel, "p6-import-parent" );
	g_return_if_fail( target && GTK_IS_CONTAINER( target ));

	settings = ofa_file_format_new( SETTINGS_IMPORT_SETTINGS );
	priv->import_settings = ofa_file_format_bin_new( settings );
	g_object_unref( settings );
	gtk_container_add( GTK_CONTAINER( target ), GTK_WIDGET( priv->import_settings ));
}

static void
enumerate_prefs_plugins( ofaPreferences *self, pfnPlugin pfn )
{
	GList *list, *it;

	list = ofa_plugin_get_extensions_for_type( OFA_TYPE_IPREFERENCES );

	for( it=list ; it ; it=it->next ){
		( *pfn )( self, OFA_IPREFERENCES( it->data ));
	}

	ofa_plugin_free_extensions( list );
}

/*
 * @instance: an object maintained by a plugin, which implements our
 *  IPreferences interface.
 *
 * add a page to the notebook for each object type which implements the
 * ofaIPreferences interface
 */
static void
init_plugin_page( ofaPreferences *self, ofaIPreferences *instance )
{
	ofaPreferencesPrivate *priv;
	GtkWidget *page, *wlabel;
	sPagePlugin *splug;
	gchar *label;

	priv = self->priv;

	page = ofa_ipreferences_do_init( instance, &label );
	my_utils_widget_set_margin( page, 4, 4, 4, 4 );
	wlabel = gtk_label_new( label );
	g_free( label );
	gtk_notebook_append_page( GTK_NOTEBOOK( priv->book ), page, wlabel );

	splug = g_new0( sPagePlugin, 1 );
	splug->object = instance;
	splug->page = page;

	priv->plugs = g_list_append( priv->plugs, splug );

	/* try to identify if the plugin which implements this object is the
	 * one which has been required */
	if( priv->plugin && !priv->object_page ){
		if( ofa_plugin_has_object( priv->plugin, G_OBJECT( instance ))){
			priv->object_page = page;
		}
	}
}

/*
 * activate the first page of the preferences notebook
 */
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

static void
on_quit_on_escape_toggled( GtkToggleButton *button, ofaPreferences *self )
{
	gtk_widget_set_sensitive(
			GTK_WIDGET( self->priv->confirm_on_escape_btn ),
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
	GtkContainer *container;
	GtkWidget *label;

	format = my_date_combo_get_selected( MY_DATE_COMBO( box ));
	if( !date ){
		date = g_date_new_dmy( 31, 8, 2015 );
	}
	str = my_date_to_str( date, format );
	str2 = g_strdup_printf( "<i>%s</i>", str );

	container = ( GtkContainer * ) my_window_get_toplevel( MY_WINDOW( self ));
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	label = my_utils_container_get_child_by_name( container, sample_name );
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

	priv = self->priv;
	activable = TRUE;

	if( !priv->p4_accept_dot || !priv->p4_accept_comma ){
		activable = FALSE;
	} else {
		accept_dot = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p4_accept_dot ));
		accept_comma = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p4_accept_comma ));
		activable &= ( accept_dot || accept_comma );
	}

	if( !priv->btn_ok ){
		priv->btn_ok = my_utils_container_get_child_by_name(
				GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "btn-ok" );
		g_return_if_fail( priv->btn_ok && GTK_IS_BUTTON( priv->btn_ok ));
	}

	gtk_widget_set_sensitive( priv->btn_ok, activable );
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
	gboolean ok;

	priv = self->priv;

	do_update_quitting_page( self );
	ofa_dossier_delete_prefs_bin_set_settings( priv->dd_prefs );
	do_update_account_page( self );
	ok = do_update_locales_page( self );
	do_update_export_page( self );
	do_update_import_page( self );
	enumerate_prefs_plugins( self, update_prefs_plugin );

	priv->updated = ok;

	return( priv->updated );
}

static void
do_update_quitting_page( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	GtkContainer *container;
	GtkWidget *button;

	priv = self->priv;
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

	button = my_utils_container_get_child_by_name( container, "p1-confirm-altf4" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	ofa_settings_set_boolean(
			st_appli_confirm_on_altf4,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( button )));

	button = my_utils_container_get_child_by_name( container, "p1-confirm-quit" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	ofa_settings_set_boolean(
			st_appli_confirm_on_quit,
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

/**
 * ofa_prefs_appli_confirm_on_altf4:
 */
gboolean
ofa_prefs_appli_confirm_on_altf4( void )
{
	return( ofa_settings_get_boolean( st_appli_confirm_on_altf4 ));
}

/**
 * ofa_prefs_appli_confirm_on_quit:
 */
gboolean
ofa_prefs_appli_confirm_on_quit( void )
{
	return( ofa_settings_get_boolean( st_appli_confirm_on_quit ));
}

static void
do_update_account_page( ofaPreferences *self )
{
	GtkContainer *container;
	GtkWidget *button;

	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));

	button = my_utils_container_get_child_by_name( container, "p4-delete-with-child" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	ofa_settings_set_boolean(
			st_account_delete_root_with_child,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( button )));
}

/**
 * ofa_prefs_account_delete_root_with_child:
 */
gboolean
ofa_prefs_account_delete_root_with_children( void )
{
	return( ofa_settings_get_boolean( st_account_delete_root_with_child ));
}

static gboolean
do_update_locales_page( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	GList *list;
	const gchar *cstr;
	gchar *decimal_sep;
	gchar *str;

	priv = self->priv;

	list = g_list_append( NULL, GINT_TO_POINTER( my_date_combo_get_selected( priv->p4_display_combo )));
	list = g_list_append( list, GINT_TO_POINTER( my_date_combo_get_selected( priv->p4_check_combo )));
	ofa_settings_set_int_list( SETTINGS_DATE, list );
	ofa_settings_free_int_list( list );

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

	ofa_settings_set_string_list( SETTINGS_AMOUNT, list );
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

	list = ofa_settings_get_int_list( SETTINGS_DATE );
	if( list ){
		if( list->data ){
			st_date_display = GPOINTER_TO_INT( list->data );
		}
		it = list->next;
		if( it->data ){
			st_date_check = GPOINTER_TO_INT( it->data );
		}
	}
	ofa_settings_free_int_list( list );
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

	list = ofa_settings_get_string_list( SETTINGS_AMOUNT );
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

static void
do_update_export_page( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	gchar *text;

	priv = self->priv;

	ofa_file_format_bin_apply( priv->export_settings );

	text = gtk_file_chooser_get_current_folder_uri( priv->p5_chooser );
	if( my_strlen( text )){
		ofa_settings_set_string( SETTINGS_EXPORT_FOLDER, text );
	}
	g_free( text );
}

static void
do_update_import_page( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;

	priv = self->priv;

	ofa_file_format_bin_apply( priv->import_settings );
}

static void
update_prefs_plugin( ofaPreferences *self, ofaIPreferences *instance )
{
	GtkWidget *page;

	page = find_prefs_plugin( self, instance );
	g_return_if_fail( page && GTK_IS_WIDGET( page ));

	ofa_ipreferences_do_apply( instance, page );
}

static GtkWidget *
find_prefs_plugin( ofaPreferences *self, ofaIPreferences *instance )
{
	GList *it;
	sPagePlugin *splug;

	for( it=self->priv->plugs ; it ; it=it->next ){
		splug = ( sPagePlugin * ) it->data;
		if( splug->object == instance ){
			return( splug->page );
		}
	}

	return( NULL );
}
