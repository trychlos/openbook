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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-settings.h"

#include "core/my-date-combo.h"
#include "core/my-decimal-combo.h"
#include "core/my-field-combo.h"
#include "core/ofa-file-format-bin.h"

/* private instance data
 */
struct _ofaFileFormatBinPrivate {
	gboolean        dispose_has_run;

	/* initialization data
	 */
	ofaFileFormat  *settings;

	/* UI
	 */
	GtkWidget      *format_combo;		/* file format */
	GtkWidget      *settings_frame;
	GtkWidget      *encoding_combo;
	myDateCombo    *date_combo;
	myDecimalCombo *decimal_combo;
	myFieldCombo   *field_combo;
	GtkWidget      *field_parent;
	GtkWidget      *field_label;
	GtkWidget      *dispo_frame;
	GtkWidget      *headers_btn;
	GtkWidget      *headers_count;

	/* runtime data
	 */
	ofaFFtype         format;
};

/* column ordering in the file format combobox
 */
enum {
	EXP_COL_FORMAT = 0,
	EXP_COL_LABEL,
	EXP_N_COLUMNS
};

/* column ordering in the output encoding combobox
 */
enum {
	ENC_COL_CODE = 0,
	ENC_N_COLUMNS
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_window_xml       = PKGUIDIR "/ofa-file-format-bin.ui";
static const gchar *st_window_id        = "FileFormatBin";

G_DEFINE_TYPE( ofaFileFormatBin, ofa_file_format_bin, GTK_TYPE_BIN )

static void     load_dialog( ofaFileFormatBin *bin );
static void     setup_bin( ofaFileFormatBin *bin );
static void     init_file_format( ofaFileFormatBin *self );
static void     on_fftype_changed( GtkComboBox *box, ofaFileFormatBin *self );
static void     init_encoding( ofaFileFormatBin *self );
static void     on_encoding_changed( GtkComboBox *box, ofaFileFormatBin *self );
static void     init_date_format( ofaFileFormatBin *self );
static void     on_date_changed( myDateCombo *combo, myDateFormat format, ofaFileFormatBin *self );
static void     init_decimal_dot( ofaFileFormatBin *self );
static void     on_decimal_changed( myDecimalCombo *combo, const gchar *decimal_sep, ofaFileFormatBin *self );
static void     init_field_separator( ofaFileFormatBin *self );
static void     on_field_changed( myFieldCombo *combo, const gchar *field_sep, ofaFileFormatBin *self );
static void     init_headers( ofaFileFormatBin *self );
static void     on_headers_toggled( GtkToggleButton *button, ofaFileFormatBin *self );
static void     on_headers_count_changed( GtkSpinButton *button, ofaFileFormatBin *self );
static gboolean is_validable( ofaFileFormatBin *self );
static gint     get_file_format( ofaFileFormatBin *self );
static gchar   *get_charmap( ofaFileFormatBin *self );
static gboolean do_apply( ofaFileFormatBin *self );
static GList   *get_available_charmaps( void );

static void
file_format_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_file_format_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_FILE_FORMAT_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_file_format_bin_parent_class )->finalize( instance );
}

static void
file_format_bin_dispose( GObject *instance )
{
	ofaFileFormatBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_FILE_FORMAT_BIN( instance ));

	priv = OFA_FILE_FORMAT_BIN( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		g_clear_object( &priv->settings );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_file_format_bin_parent_class )->dispose( instance );
}

static void
ofa_file_format_bin_init( ofaFileFormatBin *self )
{
	static const gchar *thisfn = "ofa_file_format_bin_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_FILE_FORMAT_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_FILE_FORMAT_BIN, ofaFileFormatBinPrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
ofa_file_format_bin_class_init( ofaFileFormatBinClass *klass )
{
	static const gchar *thisfn = "ofa_file_format_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = file_format_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = file_format_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaFileFormatBinPrivate ));

	/**
	 * ofaFileFormatBin::changed:
	 *
	 * This signal is sent when one of the data is changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaFileFormatBin *bin,
	 * 						gpointer          user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_FILE_FORMAT_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0,
				G_TYPE_NONE );
}

/**
 * ofa_file_format_bin_new:
 */
ofaFileFormatBin *
ofa_file_format_bin_new( ofaFileFormat *settings )
{
	ofaFileFormatBin *self;

	self = g_object_new( OFA_TYPE_FILE_FORMAT_BIN, NULL );

	self->priv->settings = g_object_ref( settings );

	load_dialog( self );
	setup_bin( self );

	return( self );
}

/*
 */
static void
load_dialog( ofaFileFormatBin *bin )
{
	GtkWidget *window, *widget;

	window = my_utils_builder_load_from_path( st_window_xml, st_window_id );
	g_return_if_fail( window && GTK_IS_CONTAINER( window ));

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "top" );
	g_return_if_fail( widget && GTK_IS_CONTAINER( widget ));

	gtk_widget_reparent( widget, GTK_WIDGET( bin ));
}

static void
setup_bin( ofaFileFormatBin *bin )
{
	ofaFileFormatBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_FILE_FORMAT_BIN( bin ));

	priv = bin->priv;

	priv->settings_frame = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "settings-frame" );
	priv->dispo_frame = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dispo-frame" );

	init_encoding( bin );
	init_date_format( bin );
	init_decimal_dot( bin );
	init_field_separator( bin );
	init_headers( bin );

	/* export format at the end so that it is able to rely on
	   precomputed widgets */
	init_file_format( bin );

	gtk_widget_show_all( GTK_WIDGET( bin ));
}

static void
init_file_format( ofaFileFormatBin *self )
{
	ofaFileFormatBinPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkCellRenderer *cell;
	gint i, idx;
	ofaFFtype fmt;
	const gchar *cstr;

	priv = self->priv;

	priv->format_combo =
			my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-export-format" );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			EXP_N_COLUMNS,
			G_TYPE_INT, G_TYPE_STRING ));
	gtk_combo_box_set_model( GTK_COMBO_BOX( priv->format_combo ), tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(
			GTK_CELL_LAYOUT( priv->format_combo ), cell, FALSE );
	gtk_cell_layout_add_attribute(
			GTK_CELL_LAYOUT( priv->format_combo ), cell, "text", EXP_COL_LABEL );

	fmt = ofa_file_format_get_fftype( priv->settings );
	idx = -1;

	for( i=1 ; TRUE ; ++i ){
		cstr = ofa_file_format_get_fftype_str( i );
		if( cstr ){
			gtk_list_store_insert_with_values(
					GTK_LIST_STORE( tmodel ),
					&iter,
					-1,
					EXP_COL_FORMAT, i,
					EXP_COL_LABEL,  cstr,
					-1 );
			if( fmt == i ){
				idx = i-1;
			}
		} else {
			break;
		}
	}

	g_signal_connect(
			G_OBJECT( priv->format_combo ), "changed", G_CALLBACK( on_fftype_changed ), self );

	/* default to export as csv */
	if( idx == -1 ){
		idx = 0;
	}
	gtk_combo_box_set_active( GTK_COMBO_BOX( priv->format_combo ), idx );
}

static void
on_fftype_changed( GtkComboBox *box, ofaFileFormatBin *self )
{
	ofaFileFormatBinPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	priv = self->priv;

	if( !gtk_combo_box_get_active_iter( GTK_COMBO_BOX( priv->format_combo ), &iter )){
		g_return_if_reached();
	}
	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( priv->format_combo ));
	g_return_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ));
	gtk_tree_model_get( tmodel, &iter, EXP_COL_FORMAT, &priv->format, -1 );

	gtk_widget_set_sensitive( priv->settings_frame, priv->format != OFA_FFTYPE_OTHER );
	gtk_widget_set_sensitive( priv->field_label, priv->format == OFA_FFTYPE_CSV );
	gtk_widget_set_sensitive( priv->field_parent, priv->format == OFA_FFTYPE_CSV );
	gtk_widget_set_sensitive( priv->dispo_frame, priv->format != OFA_FFTYPE_OTHER );

	g_signal_emit_by_name( self, "changed" );
}

static void
init_encoding( ofaFileFormatBin *self )
{
	ofaFileFormatBinPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkCellRenderer *cell;
	GList *charmaps, *it;
	gint i, idx;
	const gchar *cstr, *svalue;
	GtkWidget *label;

	priv = self->priv;

	priv->encoding_combo = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-encoding" );
	g_return_if_fail( priv->encoding_combo && GTK_IS_COMBO_BOX( priv->encoding_combo ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "label2x" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->encoding_combo );

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

	svalue = ofa_file_format_get_charmap( priv->settings );
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

	g_signal_connect(
			G_OBJECT( priv->encoding_combo ), "changed", G_CALLBACK( on_encoding_changed ), self );
}

static void
on_encoding_changed( GtkComboBox *box, ofaFileFormatBin *self )
{
	g_signal_emit_by_name( self, "changed" );
}

static void
init_date_format( ofaFileFormatBin *self )
{
	ofaFileFormatBinPrivate *priv;
	GtkWidget *widget, *label;

	priv = self->priv;

	priv->date_combo = my_date_combo_new();

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-parent-date" );
	g_return_if_fail( widget && GTK_IS_CONTAINER( widget ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "label3x" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->date_combo ));

	gtk_container_add( GTK_CONTAINER( widget ), GTK_WIDGET( priv->date_combo ));
	my_date_combo_set_selected( priv->date_combo, ofa_file_format_get_date_format( priv->settings ));

	g_signal_connect( priv->date_combo, "ofa-changed", G_CALLBACK( on_date_changed ), self );
}

static void
on_date_changed( myDateCombo *combo, myDateFormat format, ofaFileFormatBin *self )
{
	g_signal_emit_by_name( self, "changed" );
}

static void
init_decimal_dot( ofaFileFormatBin *self )
{
	ofaFileFormatBinPrivate *priv;
	GtkWidget *parent, *label;
	gchar *sep;

	priv = self->priv;
	priv->decimal_combo = my_decimal_combo_new();

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-decimal-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->decimal_combo ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "label4x" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->decimal_combo ));

	sep = g_strdup_printf( "%c", ofa_file_format_get_decimal_sep( priv->settings ));
	my_decimal_combo_set_selected( priv->decimal_combo, sep );
	g_free( sep );

	g_signal_connect( priv->decimal_combo, "ofa-changed", G_CALLBACK( on_decimal_changed ), self );
}

static void
on_decimal_changed( myDecimalCombo *combo, const gchar *decimal_sep, ofaFileFormatBin *self )
{
	g_signal_emit_by_name( self, "changed" );
}

static void
init_field_separator( ofaFileFormatBin *self )
{
	ofaFileFormatBinPrivate *priv;
	gchar *sep;
	GtkWidget *label;

	priv = self->priv;

	priv->field_combo = my_field_combo_new();
	priv->field_label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-field-label" );
	priv->field_parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-field-parent" );
	g_return_if_fail( priv->field_parent && GTK_IS_CONTAINER( priv->field_parent ));

	gtk_container_add( GTK_CONTAINER( priv->field_parent ), GTK_WIDGET( priv->field_combo ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-field-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->field_combo ));

	sep = g_strdup_printf( "%c", ofa_file_format_get_field_sep( priv->settings ));
	/*g_debug( "init_field_dot: sep='%s'", sep );*/
	my_field_combo_set_selected( priv->field_combo, sep );
	g_free( sep );

	g_signal_connect( priv->field_combo, "ofa-changed", G_CALLBACK( on_field_changed ), self );
}

static void
on_field_changed( myFieldCombo *combo, const gchar *field_sep, ofaFileFormatBin *self )
{
	g_signal_emit_by_name( self, "changed" );
}

static void
init_headers( ofaFileFormatBin *self )
{
	ofaFileFormatBinPrivate *priv;
	ofaFFmode mode;
	gboolean bvalue;
	gint count;
	GtkWidget *widget;
	GtkAdjustment *adjust;

	priv = self->priv;

	mode = ofa_file_format_get_ffmode( priv->settings );

	if( mode == OFA_FFMODE_EXPORT ){
		priv->headers_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-headers" );
		g_return_if_fail( priv->headers_btn && GTK_IS_TOGGLE_BUTTON( priv->headers_btn ));

		bvalue = ofa_file_format_has_headers( priv->settings );
		g_signal_connect(
				G_OBJECT( priv->headers_btn ), "toggled", G_CALLBACK( on_headers_toggled ), self );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->headers_btn ), bvalue );

		widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "label4x1" );
		g_return_if_fail( widget && GTK_IS_LABEL( widget ));
		gtk_widget_destroy( widget );

		widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-count" );
		g_return_if_fail( widget && GTK_IS_SPIN_BUTTON( widget ));
		gtk_widget_destroy( widget );

	} else {
		widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "label1x1" );
		g_return_if_fail( widget && GTK_IS_LABEL( widget ));
		gtk_widget_destroy( widget );

		widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-headers" );
		g_return_if_fail( widget && GTK_IS_TOGGLE_BUTTON( widget ));
		gtk_widget_destroy( widget );

		priv->headers_count = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-count" );
		g_return_if_fail( priv->headers_count && GTK_IS_SPIN_BUTTON( priv->headers_count ));

		widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "label4x1" );
		g_return_if_fail( widget && GTK_IS_LABEL( widget ));
		gtk_label_set_mnemonic_widget( GTK_LABEL( widget ), priv->headers_count );

		count = ofa_file_format_get_headers_count( priv->settings );
		adjust = gtk_adjustment_new( count, 0, 9999, 1, 10, 10 );
		gtk_spin_button_set_adjustment( GTK_SPIN_BUTTON( priv->headers_count), adjust );

		g_signal_connect(
				G_OBJECT( priv->headers_count ), "value-changed", G_CALLBACK( on_headers_count_changed ), self );
		gtk_spin_button_set_value( GTK_SPIN_BUTTON( priv->headers_count ), count );
	}
}

static void
on_headers_toggled( GtkToggleButton *button, ofaFileFormatBin *self )
{
	g_signal_emit_by_name( self, "changed" );
}

static void
on_headers_count_changed( GtkSpinButton *button, ofaFileFormatBin *self )
{
	g_signal_emit_by_name( self, "changed" );
}

/**
 * ofa_file_format_bin_is_validable:
 *
 * Returns: %TRUE if selection is ok
 */
gboolean
ofa_file_format_bin_is_validable( ofaFileFormatBin *settings )
{
	ofaFileFormatBinPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_FILE_FORMAT_BIN( settings ), FALSE );

	priv = settings->priv;

	if( !priv->dispose_has_run ){

		return( is_validable( settings ));
	}

	g_return_val_if_reached( FALSE );
	return( FALSE );
}

static gboolean
is_validable( ofaFileFormatBin *self )
{
	ofaFileFormatBinPrivate *priv;
	gchar *charmap, *decimal_sep, *field_sep;
	gint iformat, ivalue;

	priv = self->priv;

	/* import/export format */
	iformat = get_file_format( self );
	if( iformat < 1 ){
		return( FALSE );
	}
	/* doesn't check configuration when the import/export is 'other' format */
	if( iformat == OFA_FFTYPE_OTHER ){
		return( TRUE );
	}

	/* charmap */
	charmap = get_charmap( self );
	if( !my_strlen( charmap )){
		g_free( charmap );
		return( FALSE );
	}
	g_free( charmap );

	/* date format */
	ivalue = my_date_combo_get_selected( priv->date_combo );
	if( ivalue < MY_DATE_FIRST ){
		return( FALSE );
	}

	/* decimal separator */
	decimal_sep = my_decimal_combo_get_selected( priv->decimal_combo );
	if( !my_strlen( decimal_sep )){
		g_free( decimal_sep );
		return( FALSE );
	}
	g_free( decimal_sep );

	/* field separator */
	field_sep = my_field_combo_get_selected( priv->field_combo );
	if( !my_strlen( field_sep )){
		g_free( field_sep );
		return( FALSE );
	}
	g_free( field_sep );

	return( TRUE );
}

static gint
get_file_format( ofaFileFormatBin *self )
{
	ofaFileFormatBinPrivate *priv;
	ofaFFtype format;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	priv = self->priv;
	format = -1;

	if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX( priv->format_combo ), &iter )){
		tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( priv->format_combo ));
		g_return_val_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ), FALSE );
		gtk_tree_model_get( tmodel, &iter, EXP_COL_FORMAT, &format, -1 );
	}

	return( format );
}

static gchar *
get_charmap( ofaFileFormatBin *self )
{
	ofaFileFormatBinPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *charmap;

	priv = self->priv;
	charmap = NULL;

	if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX( priv->encoding_combo ), &iter )){
		tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( priv->encoding_combo ));
		g_return_val_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ), FALSE );
		gtk_tree_model_get( tmodel, &iter, ENC_COL_CODE, &charmap, -1 );
	}

	return( charmap );
}

/**
 * ofa_file_format_bin_apply:
 *
 * This take the current selection out of the dialog box, setting the
 * user preferences.
 *
 * Returns: %TRUE if selection is ok
 */
gboolean
ofa_file_format_bin_apply( ofaFileFormatBin *settings )
{
	ofaFileFormatBinPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_FILE_FORMAT_BIN( settings ), FALSE );

	priv = settings->priv;

	if( !priv->dispose_has_run ){

		if( !is_validable( settings )){
			return( FALSE );
		}

		return( do_apply( settings ));
	}

	g_return_val_if_reached( FALSE );
	return( FALSE );
}

static gboolean
do_apply( ofaFileFormatBin *self )
{
	ofaFileFormatBinPrivate *priv;
	gchar *charmap, *decimal_sep, *field_sep;
	gint iformat, ivalue, iheaders;
	ofaFFmode mode;

	priv = self->priv;
	charmap = NULL;
	ivalue = -1;
	iheaders = -1;
	decimal_sep = g_strdup( "");
	field_sep = g_strdup( "");

	iformat = get_file_format( self );
	mode = ofa_file_format_get_ffmode( priv->settings );

	if( iformat != OFA_FFTYPE_OTHER ){
		charmap = get_charmap( self );
		ivalue = my_date_combo_get_selected( priv->date_combo );
		decimal_sep = my_decimal_combo_get_selected( priv->decimal_combo );
		field_sep = my_field_combo_get_selected( priv->field_combo );

		if( mode == OFA_FFMODE_EXPORT ){
			iheaders = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->headers_btn ));
		} else {
			iheaders = gtk_spin_button_get_value( GTK_SPIN_BUTTON( priv->headers_count ));
		}
	}

	ofa_file_format_set( priv->settings,
								NULL,
								iformat,
								mode, charmap, ivalue, decimal_sep[0], field_sep[0], iheaders );

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
	static const gchar *thisfn = "ofa_file_format_bin_get_available_charmaps";
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

	} else if( my_strlen( stderr )){
		g_warning( "%s: stderr='%s'", thisfn, stderr );
		g_free( stderr );

	} else {
		lines = g_strsplit( stdout, "\n", -1 );
		g_free( stdout );
		iter = lines;
		while( *iter ){
			if( my_strlen( *iter )){
				charmaps = g_list_prepend( charmaps, g_strdup( *iter ));
			}
			iter++;
		}
		g_strfreev( lines );
	}

	return( g_list_reverse( charmaps ));
}
