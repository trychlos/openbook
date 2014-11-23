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

/* private instance data
 */
struct _ofaExportSettingsPrivate {
	gboolean      dispose_has_run;

	/* runtime data
	 */
	gchar        *user_pref;
	gboolean      initialized;

	/* UI
	 */
	GtkContainer *parent;				/* from the hosting dialog */
	GtkContainer *container;			/* our top container */

	GtkWidget    *encoding_combo;
	GtkWidget    *date_combo;
	GtkWidget    *decimal_combo;
	GtkWidget    *fieldsep_combo;
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
	const gchar *label;
}
	sDateFormat;

static const sDateFormat st_date_format[] = {
		{ MY_DATE_DMYY, N_( "DD/MM/YYYY" )},
		{ MY_DATE_YYMD, N_( "YYYYMMDD" )},
		{ MY_DATE_SQL,  N_( "YYYY-MM-DD" )},
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
static const gchar *st_window_id        = "ExportSettingsWindow";

static const gchar *st_pref_charmap     = "ExportSettingsCharmap";
static const gchar *st_pref_date        = "ExportSettingsDate";
static const gchar *st_pref_decimal     = "ExportSettingsDecimalSep";
static const gchar *st_pref_field_sep   = "ExportSettingsFieldSep";

static const gchar *st_def_charmap      = "UTF-8";
static const gint   st_def_date         = MY_DATE_SQL;
static const gchar *st_def_decimal      = ".";
static const gchar *st_def_field_sep    = ";";

G_DEFINE_TYPE( ofaExportSettings, ofa_export_settings, G_TYPE_OBJECT )

static void     init_encoding( ofaExportSettings *self );
static void     init_date_format( ofaExportSettings *self );
static void     init_decimal_dot( ofaExportSettings *self );
static void     init_field_separator( ofaExportSettings *self );
static gboolean do_apply( ofaExportSettings *self );
static GList   *get_available_charmaps( void );
static gchar   *settings_get_string_ex( const gchar *name, const gchar *default_pref );
static void     settings_set_string_ex( const gchar *name, const gchar *default_pref, const gchar *value );
static gint     settings_get_uint_ex( const gchar *name, const gchar *default_pref );
static void     settings_set_uint_ex( const gchar *name, const gchar *default_pref, gint value );

static void
export_settings_finalize( GObject *instance )
{
	ofaExportSettingsPrivate *priv;

	static const gchar *thisfn = "ofa_export_settings_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EXPORT_SETTINGS( instance ));

	/* free data members here */
	priv = OFA_EXPORT_SETTINGS( instance )->priv;

	g_free( priv->user_pref );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_export_settings_parent_class )->finalize( instance );
}

static void
export_settings_dispose( GObject *instance )
{
	ofaExportSettingsPrivate *priv;

	g_return_if_fail( instance && OFA_IS_EXPORT_SETTINGS( instance ));

	priv = OFA_EXPORT_SETTINGS( instance )->priv;

	if( !priv->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_export_settings_parent_class )->dispose( instance );
}

static void
ofa_export_settings_init( ofaExportSettings *self )
{
	static const gchar *thisfn = "ofa_export_settings_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXPORT_SETTINGS( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_EXPORT_SETTINGS, ofaExportSettingsPrivate );

	self->priv->dispose_has_run = FALSE;
	self->priv->initialized = FALSE;
}

static void
ofa_export_settings_class_init( ofaExportSettingsClass *klass )
{
	static const gchar *thisfn = "ofa_export_settings_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = export_settings_dispose;
	G_OBJECT_CLASS( klass )->finalize = export_settings_finalize;

	g_type_class_add_private( klass, sizeof( ofaExportSettingsPrivate ));
}

/**
 * ofa_export_settings_new:
 */
ofaExportSettings *
ofa_export_settings_new( void )
{
	ofaExportSettings *self;

	self = g_object_new( OFA_TYPE_EXPORT_SETTINGS, NULL );

	return( self );
}

/**
 * ofa_export_settings_attach_to:
 *
 * This attach the widgets to the designed parent.
 * This must be called only once, at initialization time.
 */
void
ofa_export_settings_attach_to( ofaExportSettings *settings, GtkContainer *new_parent )
{
	ofaExportSettingsPrivate *priv;
	GtkWidget *window, *widget;

	g_return_if_fail( settings && OFA_IS_EXPORT_SETTINGS( settings ));
	g_return_if_fail( new_parent && GTK_IS_CONTAINER( new_parent ));

	priv = settings->priv;

	g_return_if_fail( priv->parent == NULL );

	if( !priv->dispose_has_run ){

		window = my_utils_builder_load_from_path( st_window_xml, st_window_id );
		g_return_if_fail( window && GTK_IS_CONTAINER( window ));

		widget = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "p5-top-frame" );
		g_return_if_fail( widget && GTK_IS_CONTAINER( widget ));

		gtk_widget_reparent( widget, GTK_WIDGET( new_parent ));
		priv->parent = new_parent;
		priv->container = GTK_CONTAINER( widget );
	}
}

/**
 * ofa_export_settings_init_dlg:
 *
 * This initializes the combo boxes. This must be done after having
 * attached the widgets to the containing parent.
 */
void
ofa_export_settings_init_dlg( ofaExportSettings *settings )
{
	ofaExportSettingsPrivate *priv;

	g_return_if_fail( settings && OFA_IS_EXPORT_SETTINGS( settings ));

	priv = settings->priv;

	g_return_if_fail( priv->parent && GTK_IS_CONTAINER( priv->parent ));
	g_return_if_fail( priv->container && GTK_IS_CONTAINER( priv->container ));

	if( !priv->dispose_has_run ){

		init_encoding( settings );
		init_date_format( settings );
		init_decimal_dot( settings );
		init_field_separator( settings );

		priv->initialized = TRUE;
	}
}

static void
init_encoding( ofaExportSettings *self )
{
	ofaExportSettingsPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkCellRenderer *cell;
	GList *charmaps, *it;
	gint i, idx;
	gchar *text;
	const gchar *cstr;

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

	text = settings_get_string_ex( priv->user_pref, st_pref_charmap );
	if( !text || !g_utf8_strlen( text, -1 )){
		g_free( text );
		text = g_strdup( st_def_charmap );
	}

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
		if( !g_utf8_collate( text, cstr )){
			idx = i;
		}
	}

	if( idx != -1 ){
		gtk_combo_box_set_active( GTK_COMBO_BOX( priv->encoding_combo ), idx );
	}

	g_free( text );
	g_list_free_full( charmaps, ( GDestroyNotify ) g_free );
}

static void
init_date_format( ofaExportSettings *self )
{
	ofaExportSettingsPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkCellRenderer *cell;
	gint i, idx, fmt;

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

	fmt = settings_get_uint_ex( priv->user_pref, st_pref_date );
	if( fmt < 0 ){
		fmt = st_def_date;
	}

	idx = -1;

	for( i=0 ; st_date_format[i].code ; ++i ){
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				DATE_COL_CODE,  st_date_format[i].code,
				DATE_COL_LABEL, st_date_format[i].label,
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
init_decimal_dot( ofaExportSettings *self )
{
	ofaExportSettingsPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkCellRenderer *cell;
	gint i, idx;
	gchar *text;

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

	text = settings_get_string_ex( priv->user_pref, st_pref_decimal );
	if( !text || !g_utf8_strlen( text, -1 )){
		g_free( text );
		text = g_strdup( st_def_decimal );
	}

	idx = -1;

	for( i=0 ; st_dec[i].code ; ++i ){
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				DEC_COL_CODE,  st_dec[i].code,
				DEC_COL_LABEL, st_dec[i].label,
				-1 );
		if( !g_utf8_collate( text, st_dec[i].code )){
			idx = i;
		}
	}

	if( idx != -1 ){
		gtk_combo_box_set_active( GTK_COMBO_BOX( priv->decimal_combo ), idx );
	}

	g_free( text );
}

static void
init_field_separator( ofaExportSettings *self )
{
	ofaExportSettingsPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkCellRenderer *cell;
	gint i, idx;
	gchar *text;

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

	text = settings_get_string_ex( priv->user_pref, st_pref_field_sep );
	if( !text || !g_utf8_strlen( text, -1 )){
		g_free( text );
		text = g_strdup( st_def_field_sep );
	}

	idx = -1;

	for( i=0 ; st_field_sep[i].code ; ++i ){
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				SEP_COL_CODE,  st_field_sep[i].code,
				SEP_COL_LABEL, st_field_sep[i].label,
				-1 );
		if( !g_utf8_collate( text, st_field_sep[i].code )){
			idx = i;
		}
	}

	if( idx != -1 ){
		gtk_combo_box_set_active( GTK_COMBO_BOX( priv->fieldsep_combo ), idx );
	}

	g_free( text );
}

/**
 * ofa_export_settings_apply:
 *
 * This take the current selection out of the dialog box, setting the
 * user preferences.
 *
 * Returns: %TRUE if selection is ok
 */
gboolean
ofa_export_settings_apply( ofaExportSettings *settings )
{
	ofaExportSettingsPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_EXPORT_SETTINGS( settings ), FALSE );

	priv = settings->priv;

	g_return_val_if_fail( priv->parent && GTK_IS_CONTAINER( priv->parent ), FALSE );
	g_return_val_if_fail( priv->container && GTK_IS_CONTAINER( priv->container ), FALSE );
	g_return_val_if_fail( priv->initialized, FALSE );

	if( !priv->dispose_has_run ){

		return( do_apply( settings ));
	}

	g_return_val_if_reached( FALSE );
	return( FALSE );
}

static gboolean
do_apply( ofaExportSettings *self )
{
	ofaExportSettingsPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *charmap, *decimal_sep, *field_sep;
	gint ivalue;

	priv = self->priv;

	/* charmap */
	if( !gtk_combo_box_get_active_iter( GTK_COMBO_BOX( priv->encoding_combo ), &iter )){
		g_return_val_if_reached( FALSE );
	}
	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( priv->encoding_combo ));
	g_return_val_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ), FALSE );
	gtk_tree_model_get( tmodel, &iter, ENC_COL_CODE, &charmap, -1 );
	settings_set_string_ex( priv->user_pref, st_pref_charmap, charmap );
	g_free( charmap );

	/* date format */
	if( !gtk_combo_box_get_active_iter( GTK_COMBO_BOX( priv->date_combo ), &iter )){
		g_return_val_if_reached( FALSE );
	}
	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( priv->date_combo ));
	g_return_val_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ), FALSE );
	gtk_tree_model_get( tmodel, &iter, DATE_COL_CODE, &ivalue, -1 );
	settings_set_uint_ex( priv->user_pref, st_pref_date, ivalue );

	/* decimal separator */
	if( !gtk_combo_box_get_active_iter( GTK_COMBO_BOX( priv->decimal_combo ), &iter )){
		g_return_val_if_reached( FALSE );
	}
	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( priv->decimal_combo ));
	g_return_val_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ), FALSE );
	gtk_tree_model_get( tmodel, &iter, DEC_COL_CODE, &decimal_sep, -1 );
	settings_set_string_ex( priv->user_pref, st_pref_decimal, decimal_sep );
	g_free( decimal_sep );

	/* field separator */
	if( !gtk_combo_box_get_active_iter( GTK_COMBO_BOX( priv->fieldsep_combo ), &iter )){
		g_return_val_if_reached( FALSE );
	}
	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( priv->fieldsep_combo ));
	g_return_val_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ), FALSE );
	gtk_tree_model_get( tmodel, &iter, SEP_COL_CODE, &field_sep, -1 );
	settings_set_string_ex( priv->user_pref, st_pref_field_sep, field_sep );
	g_free( field_sep );

	return( TRUE );
}

/*
 * on Fedora, the 'locales -m' command returns available charmaps
 * alphabetically sorted.
 */
static GList *
get_available_charmaps( void )
{
	static const gchar *thisfn = "ofa_export_settings_get_available_charmaps";
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

/*
 * Rationale: we may store user preferences both for the default export
 * settings and for each particular export settings (e.g. for accounts,
 * ledgers, and so on.)
 * So the caller of this convenience class may say that preferences must
 * be first searched for this particular object, only then searching for
 * the default.
 */
static gchar *
settings_get_string_ex( const gchar *name, const gchar *default_pref )
{
	gchar *pref_name, *text;

	text = NULL;

	if( name && g_utf8_strlen( name, -1 )){
		pref_name = g_strdup_printf( "%s%s", name, default_pref );
		text = ofa_settings_get_string( pref_name );
		g_free( pref_name );
	}

	if( !text || !g_utf8_strlen( text, -1 )){
		text = ofa_settings_get_string( default_pref );
	}

	return( text );
}

static void
settings_set_string_ex( const gchar *name, const gchar *default_pref, const gchar *value )
{
	gchar *pref_name;

	if( name && g_utf8_strlen( name, -1 )){
		pref_name = g_strdup_printf( "%s%s", name, default_pref );
	} else {
		pref_name = g_strdup( default_pref );
	}

	ofa_settings_set_string( pref_name, value );
	g_free( pref_name );
}

static gint
settings_get_uint_ex( const gchar *name, const gchar *default_pref )
{
	gchar *pref_name;
	gint value;

	value = -1;

	if( name && g_utf8_strlen( name, -1 )){
		pref_name = g_strdup_printf( "%s%s", name, default_pref );
		value = ofa_settings_get_uint( pref_name );
		g_free( pref_name );
	}

	if( value < 0 ){
		value = ofa_settings_get_uint( default_pref );
	}

	return( value );
}

static void
settings_set_uint_ex( const gchar *name, const gchar *default_pref, gint value )
{
	gchar *pref_name;

	if( name && g_utf8_strlen( name, -1 )){
		pref_name = g_strdup_printf( "%s%s", name, default_pref );
	} else {
		pref_name = g_strdup( default_pref );
	}

	ofa_settings_set_uint( pref_name, value );
	g_free( pref_name );
}

/**
 * ofa_export_settings_get_settings:
 * @name: [allow-none]: the name of this particular export settings.
 *
 * Returns: a newly allocated #ofsExportSettings structure, which
 * contains suitable settings for the export operation.
 *
 * If @name is %NULL or empty, then settings are default settings set
 * from user preferences dialog. Else settings are those selected on
 * the previous export operation for this particular name.
 */
ofsExportSettings *
ofa_export_settings_get_settings( const gchar *name )
{
	ofsExportSettings *settings;
	gchar *text;
	gint value;

	settings = g_new0( ofsExportSettings, 1 );

	/* charmap */
	text = settings_get_string_ex( name, st_pref_charmap );
	if( !text || !g_utf8_strlen( text, -1 )){
		g_free( text );
		text = g_strdup( st_def_charmap );
	}
	settings->charmap = text;

	/* date format */
	value = settings_get_uint_ex( name, st_pref_date );
	if( value < 0 ){
		value = st_def_date;
	}
	settings->date_format = value;

	/* decimal separator */
	text = settings_get_string_ex( name, st_pref_decimal );
	if( !text || !g_utf8_strlen( text, -1 )){
		g_free( text );
		text = g_strdup( st_def_decimal );
	}
	settings->decimal_sep = text;

	/* field separator */
	text = settings_get_string_ex( name, st_pref_field_sep );
	if( !text || !g_utf8_strlen( text, -1 )){
		g_free( text );
		text = g_strdup( st_def_field_sep );
	}
	settings->field_sep = text;

	return( settings );
}

/**
 * ofa_export_settings_free_settings:
 *
 * Free the #ofsExportSettings returned from
 * #ofa_export_settings_get_settings() method.
 */
void
ofa_export_settings_free_settings( ofsExportSettings *settings )
{
	g_return_if_fail( settings );

	g_free( settings->field_sep );
	g_free( settings->decimal_sep );
	g_free( settings->charmap );
	g_free( settings );
}
