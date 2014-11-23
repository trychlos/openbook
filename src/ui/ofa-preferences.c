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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-ipreferences.h"
#include "api/ofa-settings.h"

#include "core/my-window-prot.h"
#include "core/ofa-plugin.h"

#include "ui/ofa-dossier-delete-prefs.h"
#include "ui/ofa-export-settings-prefs.h"
#include "ui/ofa-main-window.h"
#include "core/ofa-preferences.h"

/* private instance data
 */
struct _ofaPreferencesPrivate {

	GtkWidget              *book;			/* main notebook of the dialog */

	/* whether the dialog has been validated
	 */
	gboolean                updated;

	/* when opening the preferences from the plugin manager
	 */
	ofaPlugin              *plugin;
	GtkWidget              *object_page;

	/* UI - Quitting
	 */
	GtkCheckButton         *confirm_on_escape_btn;

	/* UI - Dossier delete page
	 */
	ofaDossierDeletePrefs  *dd_prefs;

	/* UI - Account delete page
	 */

	/* UI - Locales
	 */
	gchar                  *p3_decimal_sep;
	gboolean                p3_accept_dot;
	gint                    p3_date_enter;
	gint                    p3_date_display;
	gint                    p3_date_check;

	/* Export settings
	 */
	ofaExportSettingsPrefs *export_settings;

	/* UI - Plugin pages
	 */
	GList                  *plugs;
};

static const gchar *st_assistant_quit_on_escape       = "AssistantQuitOnEscape";
static const gchar *st_assistant_confirm_on_escape    = "AssistantConfirmOnEscape";
static const gchar *st_assistant_confirm_on_cancel    = "AssistantConfirmOnCancel";
static const gchar *st_appli_confirm_on_quit          = "ApplicationConfirmOnQuit";
static const gchar *st_appli_confirm_on_altf4         = "ApplicationConfirmOnAltF4";
static const gchar *st_account_delete_root_with_child = "AssistantConfirmOnCancel";

static const gchar *st_ui_xml                         = PKGUIDIR "/ofa-preferences.ui";
static const gchar *st_ui_id                          = "PreferencesDlg";
static const gchar *st_delete_dossier_xml             = PKGUIDIR "/ofa-dossier-delete-prefs.piece.ui";
static const gchar *st_delete_dossier_id              = "DossierDeleteWindow";

/*
enum {
	DATE_COL_CODE = 0,
	DATE_COL_LABEL,
	DATE_N_COLUMNS
};

typedef struct {
	gint         code;
	const gchar *label;
}
	FmtDate;

static const FmtDate st_fmt_date[] = {
		{ MY_DATE_DMMM, N_( "D MMM YYYY" )},
		{ MY_DATE_DMYY, N_( "DD/MM/YYYY" )},
		{ MY_DATE_SQL,  N_( "YYYY-MM-DD" )},
		{ 0 }
};
*/

#define DATA_DATE                           "ofa-data-date"
#define DATA_DECIMAL                        "ofa-data-decimal"
#define DATA_SEPARATOR                      "ofa-data-separator"

typedef struct {
	ofaIPreferences *object;
	GtkWidget       *page;
}
	sPagePlugin;

typedef void ( *pfnPlugin )( ofaPreferences *, ofaIPreferences * );

G_DEFINE_TYPE( ofaPreferences, ofa_preferences, MY_TYPE_DIALOG )

static void       v_init_dialog( myDialog *dialog );
static void       init_quitting_page( ofaPreferences *self );
static void       init_dossier_delete_page( ofaPreferences *self );
static void       init_account_page( ofaPreferences *self );
static void       init_locales_page( ofaPreferences *self );
static void       get_locales( void );
/*static void       init_locale_date( ofaPreferences *self, const gchar *wname, const gchar *pref, gint *pdata, gint def_value );
static void       init_locale_decimal( ofaPreferences *self, const gchar *wname, GSList *slist, const gchar *sep, gboolean *pdata );
static gint       find_str( const gchar *a, const gchar *b );
static void       init_locale_sep( ofaPreferences *self, const gchar *wname, const gchar *pref, gchar **pdata, const gchar *def_value );*/
static void       init_export_page( ofaPreferences *self );
static void       enumerate_prefs_plugins( ofaPreferences *self, pfnPlugin pfn );
static void       init_plugin_page( ofaPreferences *self, ofaIPreferences *plugin );
static void       activate_first_page( ofaPreferences *self );
static void       on_quit_on_escape_toggled( GtkToggleButton *button, ofaPreferences *self );
/*static void       on_format_date_changed( GtkComboBox *combo, ofaPreferences *self );
static void       on_decimal_toggled( GtkToggleButton *check, ofaPreferences *self );
static void       on_sep_changed( GtkCellEditable *editable, ofaPreferences *self );*/
static gboolean   v_quit_on_ok( myDialog *dialog );
static gboolean   do_update( ofaPreferences *self );
static void       do_update_quitting_page( ofaPreferences *self );
static void       do_update_account_page( ofaPreferences *self );
static void       do_update_locales_page( ofaPreferences *self );
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

		g_clear_object( &priv->dd_prefs );
		g_clear_object( &priv->export_settings );

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
ofa_preferences_run( ofaMainWindow *main_window, ofaPlugin *plugin )
{
	static const gchar *thisfn = "ofa_preferences_run";
	ofaPreferences *self;
	ofaPreferencesPrivate *priv;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

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
	priv->book =
			my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "notebook" );

	init_quitting_page( OFA_PREFERENCES( dialog ));
	init_dossier_delete_page( OFA_PREFERENCES( dialog ));
	init_account_page( OFA_PREFERENCES( dialog ));
	init_locales_page( OFA_PREFERENCES( dialog ));
	init_export_page( OFA_PREFERENCES( dialog ));
	enumerate_prefs_plugins( OFA_PREFERENCES( dialog ), init_plugin_page );
}

static void
init_quitting_page( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	GtkContainer *container;
	GtkWidget *button;
	gboolean bvalue;

	priv = self->priv;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));

	/* priv->confirm_on_escape_btn is set before acting on
	 *  quit-on-escape button as triggered signal use the variable */
	button = my_utils_container_get_child_by_name( container, "p1-confirm-on-escape" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_assistant_confirm_on_escape();
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), !bvalue );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
	priv->confirm_on_escape_btn = GTK_CHECK_BUTTON( button );

	button = my_utils_container_get_child_by_name( container, "p1-quit-on-escape" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "toggled", G_CALLBACK( on_quit_on_escape_toggled ), self );
	bvalue = ofa_prefs_assistant_quit_on_escape();
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), !bvalue );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );

	button = my_utils_container_get_child_by_name( container, "p1-confirm-on-cancel" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_assistant_confirm_on_cancel();
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), !bvalue );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );

	button = my_utils_container_get_child_by_name( container, "p1-confirm-altf4" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_appli_confirm_on_altf4();
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );

	button = my_utils_container_get_child_by_name( container, "p1-confirm-quit" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_appli_confirm_on_quit();
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
}

static void
init_dossier_delete_page( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	GtkContainer *container;
	GtkWidget *window;
	GtkWidget *grid, *parent;

	priv = self->priv;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));

	window = my_utils_builder_load_from_path( st_delete_dossier_xml, st_delete_dossier_id );
	g_return_if_fail( window && GTK_IS_WINDOW( window ));
	grid = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "grid-container" );
	g_return_if_fail( grid && GTK_IS_GRID( grid ));

	parent = my_utils_container_get_child_by_name( container, "alignment2-parent" );
	gtk_widget_reparent( grid, parent );

	priv->dd_prefs = ofa_dossier_delete_prefs_new();
	ofa_dossier_delete_prefs_init_dialog( priv->dd_prefs, container );
}

static void
init_account_page( ofaPreferences *self )
{
	GtkContainer *container;
	GtkWidget *button;
	gboolean bvalue;

	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));

	button = my_utils_container_get_child_by_name( container, "p4-delete-with-child" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	bvalue = ofa_prefs_account_delete_root_with_children();
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalue );
}

static void
init_locales_page( ofaPreferences *self )
{
	/*ofaPreferencesPrivate *priv;
	GSList *slist;

	priv = self->priv;*/

	get_locales();

	/*init_locale_date( self, "p3-date-enter",   "PrefDateEnter",   &priv->p3_date_enter,   MY_DATE_DMYY );
	init_locale_date( self, "p3-date-display", "PrefDateDisplay", &priv->p3_date_display, MY_DATE_DMYY );
	init_locale_date( self, "p3-date-check",   "PrefDateCheck",   &priv->p3_date_check,   MY_DATE_DMMM );

	slist = ofa_settings_get_string_list( "ofxAmountDecimalDots" );
	init_locale_decimal( self, "p3-decimal-dot", slist, ".", &priv->p3_accept_dot );
	init_locale_decimal( self, "p3-decimal-comma", slist, ",", &priv->p3_accept_comma );

	init_locale_sep( self, "p3-decimal-sep", "ofxAmountDecimalSep", &priv->p3_decimal_sep, "," );
	init_locale_sep( self, "p3-thousand-sep", "ofxAmountThousandSep", &priv->p3_thousand_sep, " " );*/
}

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

	} else if( stderr && g_utf8_strlen( stderr, -1 )){
		g_warning( "%s: stderr='%s'", thisfn, stderr );
		g_free( stderr );

	} else {
		/*g_debug( "%s: stdout='%s'", thisfn, stdout );*/
		lines = g_strsplit( stdout, "\n", -1 );
		g_free( stdout );
		iter = lines;
		while( *iter ){
			if( g_utf8_strlen( *iter, -1 )){
				g_debug( "%s: iter='%s'", thisfn, *iter );
			}
			iter++;
		}
		g_strfreev( lines );
	}
}

#if 0
static void
init_locale_date( ofaPreferences *self, const gchar *wname, const gchar *pref, gint *pdata, gint def_value )
{
	GtkContainer *container;
	GtkWidget *combo;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	gint i, ivalue, idx;
	GtkTreeIter iter;

	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));
	combo = my_utils_container_get_child_by_name( container, wname );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));

	tmodel = GTK_TREE_MODEL( gtk_list_store_new( DATE_N_COLUMNS, G_TYPE_INT, G_TYPE_STRING ));
	gtk_combo_box_set_model( GTK_COMBO_BOX( combo ), tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", DATE_COL_LABEL );

	ivalue = ofa_settings_get_uint( pref );
	if( ivalue == -1 ){
		ivalue = def_value;
	}
	*pdata = ivalue;
	idx = -1;

	for( i=0 ; st_fmt_date[i].code ; ++i ){
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				DATE_COL_CODE,  st_fmt_date[i].code,
				DATE_COL_LABEL, gettext( st_fmt_date[i].label ),
				-1 );

		if( ivalue == st_fmt_date[i].code ){
			idx = i;
		}
	}

	if( idx != -1 ){
		gtk_combo_box_set_active( GTK_COMBO_BOX( combo ), idx );
	}

	g_signal_connect(
			G_OBJECT( combo ),
			"changed", G_CALLBACK( on_format_date_changed ), self );

	g_object_set_data( G_OBJECT( combo ), DATA_DATE, pdata );
}

static void
init_locale_decimal( ofaPreferences *self, const gchar *wname, GSList *slist, const gchar *sep, gboolean *pdata )
{
	GtkContainer *container;
	GtkWidget *check;
	gboolean bvalue;

	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));
	check = my_utils_container_get_child_by_name( container, wname );
	g_return_if_fail( check && GTK_IS_TOGGLE_BUTTON( check ));

	bvalue = g_slist_find_custom( slist, sep, ( GCompareFunc ) find_str ) != NULL;
	*pdata = bvalue;

	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check ), bvalue );

	g_signal_connect(
			G_OBJECT( check ),
			"toggled", G_CALLBACK( on_decimal_toggled ), self );

	g_object_set_data( G_OBJECT( check ), DATA_DECIMAL, pdata );
}

static gint
find_str( const gchar *a, const gchar *b )
{
	return( g_utf8_collate( a, b ));
}

static void
init_locale_sep( ofaPreferences *self, const gchar *wname, const gchar *pref, gchar **pdata, const gchar *def_value )
{
	GtkContainer *container;
	GtkWidget *entry;
	gchar *svalue;

	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));
	entry = my_utils_container_get_child_by_name( container, wname );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));

	svalue = ofa_settings_get_string( pref );
	if( !svalue || !g_utf8_strlen( svalue, -1 )){
		svalue = g_strdup( def_value );
	}
	g_free( *pdata );
	*pdata = g_strdup( svalue );

	gtk_entry_set_text( GTK_ENTRY( entry ), svalue );
	g_free( svalue );

	g_signal_connect(
			G_OBJECT( entry ),
			"changed", G_CALLBACK( on_sep_changed ), self );

	g_object_set_data( G_OBJECT( entry ), DATA_SEPARATOR, pdata );
}
#endif

static void
init_export_page( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;
	GtkContainer *container;
	GtkWidget *target;

	priv = self->priv;

	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));
	target = my_utils_container_get_child_by_name( container, "alignment5-parent" );
	g_return_if_fail( target && GTK_IS_CONTAINER( target ));

	priv->export_settings = ofa_export_settings_prefs_new();
	ofa_export_settings_prefs_attach_to( priv->export_settings, GTK_CONTAINER( target ));
	ofa_export_settings_prefs_init_dlg( priv->export_settings );
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
	GtkWidget *page;
	sPagePlugin *splug;

	priv = self->priv;

	page = ofa_ipreferences_run_init( instance, GTK_NOTEBOOK( priv->book ));

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

#if 0
static void
on_format_date_changed( GtkComboBox *combo, ofaPreferences *self )
{
	gint *pdata;
	GtkTreeIter iter;
	GtkTreeModel *tmodel;

	if( gtk_combo_box_get_active_iter( combo, &iter )){
		tmodel = gtk_combo_box_get_model( combo );
		pdata = ( gint * ) g_object_get_data( G_OBJECT( combo ), DATA_DATE );
		gtk_tree_model_get( tmodel, &iter, DATE_COL_CODE, pdata, -1 );
	}
}

static void
on_decimal_toggled( GtkToggleButton *check, ofaPreferences *self )
{
	gboolean *pdata;

	pdata = ( gboolean * ) g_object_get_data( G_OBJECT( check ), DATA_DECIMAL );
	*pdata = gtk_toggle_button_get_active( check );
}

static void
on_sep_changed( GtkCellEditable *editable, ofaPreferences *self )
{
	const gchar *text;
	gchar **pdata;

	text = gtk_entry_get_text( GTK_ENTRY( editable ));
	pdata = ( gchar ** ) g_object_get_data( G_OBJECT( editable ), DATA_SEPARATOR );
	g_free( *pdata );
	*pdata = g_strdup( text );

}
#endif

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_update( OFA_PREFERENCES( dialog )));
}

static gboolean
do_update( ofaPreferences *self )
{
	ofaPreferencesPrivate *priv;

	priv = self->priv;

	do_update_quitting_page( self );
	ofa_dossier_delete_prefs_set_settings( priv->dd_prefs );
	do_update_account_page( self );
	do_update_locales_page( self );
	ofa_export_settings_prefs_apply( priv->export_settings );
	enumerate_prefs_plugins( self, update_prefs_plugin );

	priv->updated = TRUE;

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

static void
do_update_locales_page( ofaPreferences *self )
{
	/*ofaPreferencesPrivate *priv;
	GString *text;

	priv = self->priv;

	ofa_settings_set_uint( "PrefDateEnter",   priv->p3_date_enter );
	ofa_settings_set_uint( "PrefDateDisplay", priv->p3_date_display );
	ofa_settings_set_uint( "PrefDateCheck",   priv->p3_date_check );

	text = g_string_new( "" );
	if( priv->p3_accept_dot ){
		g_string_append_printf( text, ".;" );
	}
	if( priv->p3_accept_comma ){
		g_string_append_printf( text, ",;" );
	}
	ofa_settings_set_string( "ofxAmountDecimalDots", text->str );
	g_string_free( text, TRUE );

	ofa_settings_set_string( "ofxAmountDecimalSep", priv->p3_decimal_sep );
	ofa_settings_set_string( "ofxAmountThousandSep", priv->p3_thousand_sep );*/
}

static void
update_prefs_plugin( ofaPreferences *self, ofaIPreferences *instance )
{
	GtkWidget *page;

	page = find_prefs_plugin( self, instance );
	g_return_if_fail( page && GTK_IS_WIDGET( page ));

	ofa_ipreferences_run_done( instance, page );
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
