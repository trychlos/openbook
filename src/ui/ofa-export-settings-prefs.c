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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-settings.h"

#include "ui/ofa-export-settings.h"
#include "ui/ofa-export-settings-prefs.h"

/* private instance data
 */
struct _ofaExportSettingsPrefsPrivate {
	gboolean           dispose_has_run;

	/* runtime data
	 */
	ofaExportSettings *settings;

	/* UI
	 */
	GtkContainer      *parent;			/* from the hosting dialog */
	GtkContainer      *container;		/* our top container */

	GtkWidget         *encoding_combo;
	GtkWidget         *date_combo;
	GtkWidget         *decimal_combo;
	GtkWidget         *fieldsep_combo;
	GtkWidget         *folder_btn;
};

/* column ordering in the output encoding combobox
 */
enum {
	ENC_COL_CODE = 0,
	ENC_N_COLUMNS
};

/* output date format
 */
enum {
	DATE_COL_CODE = 0,					/* int */
	DATE_COL_LABEL,
	DATE_N_COLUMNS
};

typedef struct {
	gint         code;
}
	sDateFormat;

static const sDateFormat st_date_format[] = {
		{ MY_DATE_DMYY },
		{ MY_DATE_YYMD },
		{ MY_DATE_SQL },
		{ 0 }
};

/* decimal dot
 */
enum {
	DEC_COL_CODE,
	DEC_COL_LABEL,
	DEC_N_COLUMNS
};

typedef struct {
	const gchar *code;
	const gchar *label;
}
	sDec;

static const sDec st_dec[] = {
		{ ".", N_( ". (dot)" )},
		{ ",", N_( ", (comma)" )},
		{ 0 }
};

/* field separator
 */
enum {
	SEP_COL_CODE,
	SEP_COL_LABEL,
	SEP_N_COLUMNS
};

typedef struct {
	const gchar *code;
	const gchar *label;
}
	sFieldSep;

static const sFieldSep st_field_sep[] = {
		{ ";", N_( "; (semi colon)" )},
		{ 0 }
};

static const gchar *st_window_xml       = PKGUIDIR "/ofa-export-settings.piece.ui";
static const gchar *st_window_id        = "ExportSettingsPrefsWindow";

G_DEFINE_TYPE( ofaExportSettingsPrefs, ofa_export_settings_prefs, G_TYPE_OBJECT )

static void     init_encoding( ofaExportSettingsPrefs *self );
static void     init_date_format( ofaExportSettingsPrefs *self );
static void     init_decimal_dot( ofaExportSettingsPrefs *self );
static void     init_field_separator( ofaExportSettingsPrefs *self );
static void     init_folder( ofaExportSettingsPrefs *self );
static gboolean do_apply( ofaExportSettingsPrefs *self );
static GList   *get_available_charmaps( void );

static void
export_settings_prefs_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_export_settings_prefs_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EXPORT_SETTINGS_PREFS( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_export_settings_prefs_parent_class )->finalize( instance );
}

static void
export_settings_prefs_dispose( GObject *instance )
{
	ofaExportSettingsPrefsPrivate *priv;

	g_return_if_fail( instance && OFA_IS_EXPORT_SETTINGS_PREFS( instance ));

	priv = OFA_EXPORT_SETTINGS_PREFS( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->settings );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_export_settings_prefs_parent_class )->dispose( instance );
}

static void
ofa_export_settings_prefs_init( ofaExportSettingsPrefs *self )
{
	static const gchar *thisfn = "ofa_export_settings_prefs_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXPORT_SETTINGS_PREFS( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_EXPORT_SETTINGS_PREFS, ofaExportSettingsPrefsPrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
ofa_export_settings_prefs_class_init( ofaExportSettingsPrefsClass *klass )
{
	static const gchar *thisfn = "ofa_export_settings_prefs_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = export_settings_prefs_dispose;
	G_OBJECT_CLASS( klass )->finalize = export_settings_prefs_finalize;

	g_type_class_add_private( klass, sizeof( ofaExportSettingsPrefsPrivate ));
}

/**
 * ofa_export_settings_prefs_new:
 */
ofaExportSettingsPrefs *
ofa_export_settings_prefs_new( void )
{
	ofaExportSettingsPrefs *self;

	self = g_object_new( OFA_TYPE_EXPORT_SETTINGS_PREFS, NULL );

	return( self );
}

/**
 * ofa_export_settings_prefs_attach_to:
 *
 * This attach the widgets to the designed parent.
 * This must be called only once, at initialization time.
 */
void
ofa_export_settings_prefs_attach_to( ofaExportSettingsPrefs *settings, GtkContainer *new_parent )
{
	ofaExportSettingsPrefsPrivate *priv;
	GtkWidget *window, *widget;

	g_return_if_fail( settings && OFA_IS_EXPORT_SETTINGS_PREFS( settings ));
	g_return_if_fail( new_parent && GTK_IS_CONTAINER( new_parent ));

	priv = settings->priv;

	g_return_if_fail( priv->parent == NULL );

	if( !priv->dispose_has_run ){

		window = my_utils_builder_load_from_path( st_window_xml, st_window_id );
		g_return_if_fail( window && GTK_IS_CONTAINER( window ));

		widget = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "p5-top-grid" );
		g_return_if_fail( widget && GTK_IS_CONTAINER( widget ));

		gtk_widget_reparent( widget, GTK_WIDGET( new_parent ));
		priv->parent = new_parent;
		priv->container = GTK_CONTAINER( widget );
	}
}

/**
 * ofa_export_settings_prefs_init_dialog:
 *
 * This initializes the combo boxes. This must be done after having
 * attached the widgets to the containing parent.
 */
void
ofa_export_settings_prefs_init_dialog( ofaExportSettingsPrefs *settings )
{
	ofaExportSettingsPrefsPrivate *priv;

	g_return_if_fail( settings && OFA_IS_EXPORT_SETTINGS_PREFS( settings ));

	priv = settings->priv;

	g_return_if_fail( priv->parent && GTK_IS_CONTAINER( priv->parent ));
	g_return_if_fail( priv->container && GTK_IS_CONTAINER( priv->container ));

	if( !priv->dispose_has_run ){

		priv->settings = ofa_export_settings_new( NULL );

		init_encoding( settings );
		init_date_format( settings );
		init_decimal_dot( settings );
		init_field_separator( settings );
		init_folder( settings );
	}
}

static void
init_encoding( ofaExportSettingsPrefs *self )
{
	ofaExportSettingsPrefsPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkCellRenderer *cell;
	GList *charmaps, *it;
	gint i, idx;
	const gchar *cstr, *svalue;

	priv = self->priv;

	priv->encoding_combo =
			my_utils_container_get_child_by_name( priv->container, "p5-encoding" );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			ENC_N_COLUMNS,
			G_TYPE_STRING ));
	gtk_combo_box_set_model( GTK_COMBO_BOX( priv->encoding_combo ), tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(
			GTK_CELL_LAYOUT( priv->encoding_combo ), cell, FALSE );
	gtk_cell_layout_add_attribute(
			GTK_CELL_LAYOUT( priv->encoding_combo ), cell, "text", ENC_COL_CODE );

	svalue = ofa_export_settings_get_charmap( priv->settings );
	charmaps = get_available_charmaps();
	idx = -1;

	for( it=charmaps, i=0 ; it ; it=it->next, ++i ){
		cstr = ( const gchar * ) it->data;
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				ENC_COL_CODE, cstr,
				-1 );
		if( !g_utf8_collate( svalue, cstr )){
			idx = i;
		}
	}

	if( idx != -1 ){
		gtk_combo_box_set_active( GTK_COMBO_BOX( priv->encoding_combo ), idx );
	}

	g_list_free_full( charmaps, ( GDestroyNotify ) g_free );
}

static void
init_date_format( ofaExportSettingsPrefs *self )
{
	ofaExportSettingsPrefsPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkCellRenderer *cell;
	gint i, idx;
	myDateFormat fmt;

	priv = self->priv;

	priv->date_combo =
			my_utils_container_get_child_by_name( priv->container, "p5-date" );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			DATE_N_COLUMNS,
			G_TYPE_INT, G_TYPE_STRING ));
	gtk_combo_box_set_model( GTK_COMBO_BOX( priv->date_combo ), tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(
			GTK_CELL_LAYOUT( priv->date_combo ), cell, FALSE );
	gtk_cell_layout_add_attribute(
			GTK_CELL_LAYOUT( priv->date_combo ), cell, "text", DATE_COL_LABEL );

	fmt = ofa_export_settings_get_date_format( priv->settings );
	idx = -1;

	for( i=0 ; st_date_format[i].code ; ++i ){
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				DATE_COL_CODE,  st_date_format[i].code,
				DATE_COL_LABEL, my_date_get_format_str( st_date_format[i].code ),
				-1 );
		if( fmt == st_date_format[i].code ){
			idx = i;
		}
	}

	if( idx != -1 ){
		gtk_combo_box_set_active( GTK_COMBO_BOX( priv->date_combo ), idx );
	}
}

static void
init_decimal_dot( ofaExportSettingsPrefs *self )
{
	ofaExportSettingsPrefsPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkCellRenderer *cell;
	gint i, idx;
	gchar value;

	priv = self->priv;

	priv->decimal_combo =
			my_utils_container_get_child_by_name( priv->container, "p5-decimal" );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			DEC_N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING ));
	gtk_combo_box_set_model( GTK_COMBO_BOX( priv->decimal_combo ), tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(
			GTK_CELL_LAYOUT( priv->decimal_combo ), cell, FALSE );
	gtk_cell_layout_add_attribute(
			GTK_CELL_LAYOUT( priv->decimal_combo ), cell, "text", DEC_COL_LABEL );

	value = ofa_export_settings_get_decimal_sep( priv->settings );
	idx = -1;

	for( i=0 ; st_dec[i].code ; ++i ){
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				DEC_COL_CODE,  st_dec[i].code,
				DEC_COL_LABEL, st_dec[i].label,
				-1 );
		if( value == st_dec[i].code[0] ){
			idx = i;
		}
	}

	if( idx != -1 ){
		gtk_combo_box_set_active( GTK_COMBO_BOX( priv->decimal_combo ), idx );
	}
}

static void
init_field_separator( ofaExportSettingsPrefs *self )
{
	ofaExportSettingsPrefsPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkCellRenderer *cell;
	gint i, idx;
	gchar value;

	priv = self->priv;

	priv->fieldsep_combo =
			my_utils_container_get_child_by_name( priv->container, "p5-fieldsep" );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			SEP_N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING ));
	gtk_combo_box_set_model( GTK_COMBO_BOX( priv->fieldsep_combo ), tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(
			GTK_CELL_LAYOUT( priv->fieldsep_combo ), cell, FALSE );
	gtk_cell_layout_add_attribute(
			GTK_CELL_LAYOUT( priv->fieldsep_combo ), cell, "text", SEP_COL_LABEL );

	value = ofa_export_settings_get_field_sep( priv->settings );
	idx = -1;

	for( i=0 ; st_field_sep[i].code ; ++i ){
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				SEP_COL_CODE,  st_field_sep[i].code,
				SEP_COL_LABEL, st_field_sep[i].label,
				-1 );
		if( value == st_field_sep[i].code[0] ){
			idx = i;
		}
	}

	if( idx != -1 ){
		gtk_combo_box_set_active( GTK_COMBO_BOX( priv->fieldsep_combo ), idx );
	}
}

static void
init_folder( ofaExportSettingsPrefs *self )
{
	ofaExportSettingsPrefsPrivate *priv;
	const gchar *svalue;

	priv = self->priv;

	priv->folder_btn =
			my_utils_container_get_child_by_name( priv->container, "p5-folder" );

	svalue = ofa_export_settings_get_folder( priv->settings );

	gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER( priv->folder_btn ), svalue );
}

/**
 * ofa_export_settings_show_folder:
 *
 * Show or hide the folder frame.
 */
void
ofa_export_settings_prefs_show_folder( ofaExportSettingsPrefs *settings, gboolean show )
{
	ofaExportSettingsPrefsPrivate *priv;
	GtkWidget *frame;

	g_return_if_fail( settings && OFA_IS_EXPORT_SETTINGS_PREFS( settings ));

	priv = settings->priv;

	g_return_if_fail( priv->parent && GTK_IS_CONTAINER( priv->parent ));
	g_return_if_fail( priv->container && GTK_IS_CONTAINER( priv->container ));

	if( !priv->dispose_has_run ){

		frame = my_utils_container_get_child_by_name( priv->container, "p5-frame-folder" );
		if( show ){
			gtk_widget_show( frame );
		} else {
			gtk_widget_hide( frame );
		}
	}
}

/**
 * ofa_export_settings_prefs_apply:
 *
 * This take the current selection out of the dialog box, setting the
 * user preferences.
 *
 * Returns: %TRUE if selection is ok
 */
gboolean
ofa_export_settings_prefs_apply( ofaExportSettingsPrefs *settings )
{
	ofaExportSettingsPrefsPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_EXPORT_SETTINGS_PREFS( settings ), FALSE );

	priv = settings->priv;

	g_return_val_if_fail( priv->parent && GTK_IS_CONTAINER( priv->parent ), FALSE );
	g_return_val_if_fail( priv->container && GTK_IS_CONTAINER( priv->container ), FALSE );

	if( !priv->dispose_has_run ){

		return( do_apply( settings ));
	}

	g_return_val_if_reached( FALSE );
	return( FALSE );
}

static gboolean
do_apply( ofaExportSettingsPrefs *self )
{
	ofaExportSettingsPrefsPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *charmap, *decimal_sep, *field_sep, *folder;
	gint ivalue;

	priv = self->priv;

	/* charmap */
	if( !gtk_combo_box_get_active_iter( GTK_COMBO_BOX( priv->encoding_combo ), &iter )){
		g_return_val_if_reached( FALSE );
	}
	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( priv->encoding_combo ));
	g_return_val_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ), FALSE );
	gtk_tree_model_get( tmodel, &iter, ENC_COL_CODE, &charmap, -1 );

	/* date format */
	if( !gtk_combo_box_get_active_iter( GTK_COMBO_BOX( priv->date_combo ), &iter )){
		g_return_val_if_reached( FALSE );
	}
	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( priv->date_combo ));
	g_return_val_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ), FALSE );
	gtk_tree_model_get( tmodel, &iter, DATE_COL_CODE, &ivalue, -1 );

	/* decimal separator */
	if( !gtk_combo_box_get_active_iter( GTK_COMBO_BOX( priv->decimal_combo ), &iter )){
		g_return_val_if_reached( FALSE );
	}
	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( priv->decimal_combo ));
	g_return_val_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ), FALSE );
	gtk_tree_model_get( tmodel, &iter, DEC_COL_CODE, &decimal_sep, -1 );

	/* field separator */
	if( !gtk_combo_box_get_active_iter( GTK_COMBO_BOX( priv->fieldsep_combo ), &iter )){
		g_return_val_if_reached( FALSE );
	}
	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( priv->fieldsep_combo ));
	g_return_val_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ), FALSE );
	gtk_tree_model_get( tmodel, &iter, SEP_COL_CODE, &field_sep, -1 );

	/* folder */
	folder = gtk_file_chooser_get_current_folder( GTK_FILE_CHOOSER( priv->folder_btn ));

	ofa_export_settings_set( priv->settings, charmap, ivalue, decimal_sep[0], field_sep[0], folder );

	g_free( folder );
	g_free( field_sep );
	g_free( decimal_sep );
	g_free( charmap );

	return( TRUE );
}

/*
 * on Fedora, the 'locales -m' command returns available charmaps
 * alphabetically sorted.
 */
static GList *
get_available_charmaps( void )
{
	static const gchar *thisfn = "ofa_export_settings_prefs_get_available_charmaps";
	gchar *stdout, *stderr;
	gint exit_status;
	GError *error;
	gchar **lines, **iter;
	GList *charmaps;

	stdout = NULL;
	stderr = NULL;
	error = NULL;
	charmaps = NULL;

	if( !g_spawn_command_line_sync( "locale -m", &stdout, &stderr, &exit_status, &error )){
		g_warning( "%s: %s", thisfn, error->message );
		g_error_free( error );

	} else if( stderr && g_utf8_strlen( stderr, -1 )){
		g_warning( "%s: stderr='%s'", thisfn, stderr );
		g_free( stderr );

	} else {
		lines = g_strsplit( stdout, "\n", -1 );
		g_free( stdout );
		iter = lines;
		while( *iter ){
			if( g_utf8_strlen( *iter, -1 )){
				charmaps = g_list_prepend( charmaps, g_strdup( *iter ));
			}
			iter++;
		}
		g_strfreev( lines );
	}

	return( g_list_reverse( charmaps ));
}
