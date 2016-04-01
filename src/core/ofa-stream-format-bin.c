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
#include "my/my-field-combo.h"
#include "my/my-utils.h"

#include "ofa-stream-format-bin.h"

/* private instance data
 */
typedef struct {
	gboolean         dispose_has_run;

	/* initialization data
	 */
	ofaStreamFormat *settings;

	/* UI
	 */
	GtkWidget       *name_entry;
	GtkWidget       *mode_combo;
	GtkWidget       *has_encoding;
	GtkWidget       *encoding_combo;
	GtkWidget       *has_date;
	myDateCombo     *date_combo;
	GtkWidget       *has_thousand;
	GtkWidget       *thousand_entry;
	GtkWidget       *has_decimal;
	myDecimalCombo  *decimal_combo;
	GtkWidget       *has_field;
	myFieldCombo    *field_combo;
	GtkWidget       *field_parent;
	GtkWidget       *field_label;
	GtkWidget       *has_strdelim;
	GtkWidget       *str_delim_entry;
	GtkWidget       *has_headers;
	GtkWidget       *headers_btn;
	GtkWidget       *headers_label;
	GtkWidget       *headers_count;
	GtkSizeGroup    *group0;
	GtkSizeGroup    *group1;
}
	ofaStreamFormatBinPrivate;

/* column ordering in the mode combobox
 */
enum {
	MODE_COL_MODE = 0,
	MODE_COL_LABEL,
	MODE_N_COLUMNS
};

/* column ordering in the charmap encoding combobox
 */
enum {
	MAP_COL_CODE = 0,
	MAP_N_COLUMNS
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-stream-format-bin.ui";

static void     setup_bin( ofaStreamFormatBin *bin );
static void     name_init( ofaStreamFormatBin *self );
static void     name_on_changed( GtkEntry *entry, ofaStreamFormatBin *self );
static void     mode_init( ofaStreamFormatBin *self );
static void     mode_insert_row( ofaStreamFormatBin *self, GtkListStore *store, ofeSFMode mode );
static void     mode_on_changed( GtkComboBox *combo, ofaStreamFormatBin *self );
static void     encoding_init( ofaStreamFormatBin *self );
static GList   *encoding_get_available( void );
static gchar   *encoding_get_selected( ofaStreamFormatBin *self );
static void     encoding_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self );
static void     encoding_on_changed( GtkComboBox *box, ofaStreamFormatBin *self );
static void     date_format_init( ofaStreamFormatBin *self );
static void     date_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self );
static void     date_on_changed( myDateCombo *combo, myDateFormat format, ofaStreamFormatBin *self );
static void     thousand_sep_init( ofaStreamFormatBin *self );
static void     thousand_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self );
static void     thousand_on_changed( GtkEntry *entry, ofaStreamFormatBin *self );
static void     decimal_dot_init( ofaStreamFormatBin *self );
static void     decimal_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self );
static void     decimal_on_changed( myDecimalCombo *combo, const gchar *decimal_sep, ofaStreamFormatBin *self );
static void     field_sep_init( ofaStreamFormatBin *self );
static void     field_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self );
static void     field_on_changed( myFieldCombo *combo, const gchar *field_sep, ofaStreamFormatBin *self );
static void     str_delimiter_init( ofaStreamFormatBin *self );
static void     str_delim_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self );
static void     str_delim_on_changed( GtkEntry *entry, ofaStreamFormatBin *self );
static void     headers_init( ofaStreamFormatBin *self );
static void     headers_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self );
static void     headers_on_with_toggled( GtkToggleButton *button, ofaStreamFormatBin *self );
static void     headers_on_count_changed( GtkSpinButton *button, ofaStreamFormatBin *self );
static void     setup_format( ofaStreamFormatBin *bin );
static gboolean is_validable( ofaStreamFormatBin *self, gchar **error_message );
static gboolean do_apply( ofaStreamFormatBin *self );

G_DEFINE_TYPE_EXTENDED( ofaStreamFormatBin, ofa_stream_format_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaStreamFormatBin ))

static void
stream_format_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_stream_format_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_STREAM_FORMAT_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_stream_format_bin_parent_class )->finalize( instance );
}

static void
stream_format_bin_dispose( GObject *instance )
{
	ofaStreamFormatBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_STREAM_FORMAT_BIN( instance ));

	priv = ofa_stream_format_bin_get_instance_private( OFA_STREAM_FORMAT_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
		g_clear_object( &priv->group1 );
		g_clear_object( &priv->settings );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_stream_format_bin_parent_class )->dispose( instance );
}

static void
ofa_stream_format_bin_init( ofaStreamFormatBin *self )
{
	static const gchar *thisfn = "ofa_stream_format_bin_init";
	ofaStreamFormatBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_STREAM_FORMAT_BIN( self ));

	priv = ofa_stream_format_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_stream_format_bin_class_init( ofaStreamFormatBinClass *klass )
{
	static const gchar *thisfn = "ofa_stream_format_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = stream_format_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = stream_format_bin_finalize;

	/**
	 * ofaStreamFormatBin::changed:
	 *
	 * This signal is sent when one of the data is changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaStreamFormatBin *bin,
	 * 						gpointer          user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_STREAM_FORMAT_BIN,
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
 * ofa_stream_format_bin_new:
 * @format: [allow-none]: a #ofaStreamFormat instance, usually read from
 *  settings.
 *
 * Returns: a new #ofaStreamFormatBin instance.
 */
ofaStreamFormatBin *
ofa_stream_format_bin_new( ofaStreamFormat *format )
{
	ofaStreamFormatBin *self;
	ofaStreamFormatBinPrivate *priv;

	g_return_val_if_fail( !format || OFA_IS_STREAM_FORMAT( format ), NULL );

	self = g_object_new( OFA_TYPE_STREAM_FORMAT_BIN, NULL );

	priv = ofa_stream_format_bin_get_instance_private( self );

	setup_bin( self );

	if( format ){
		priv->settings = g_object_ref( format );
		setup_format( self );
	}

	return( self );
}

/*
 */
static void
setup_bin( ofaStreamFormatBin *bin )
{
	ofaStreamFormatBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel;

	priv = ofa_stream_format_bin_get_instance_private( bin );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "ffb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "ffb-col1-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group1 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "ffb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( bin ), GTK_WINDOW( toplevel ), "top" );

	name_init( bin );
	mode_init( bin );
	encoding_init( bin );
	date_format_init( bin );
	thousand_sep_init( bin );
	decimal_dot_init( bin );
	field_sep_init( bin );
	str_delimiter_init( bin );
	headers_init( bin );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
	gtk_widget_show_all( GTK_WIDGET( bin ));
}

static void
name_init( ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;
	GtkWidget *label;

	priv = ofa_stream_format_bin_get_instance_private( self );

	priv->name_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-name" );
	g_return_if_fail( priv->name_entry && GTK_IS_ENTRY( priv->name_entry ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-name-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->name_entry );

	g_signal_connect( priv->name_entry, "changed", G_CALLBACK( name_on_changed ), self );
}

static void
name_on_changed( GtkEntry *entry, ofaStreamFormatBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
mode_init( ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;
	GtkWidget *label;
	GtkListStore *store;
	GtkCellRenderer *cell;

	priv = ofa_stream_format_bin_get_instance_private( self );

	priv->mode_combo = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-mode-combo" );
	g_return_if_fail( priv->mode_combo && GTK_IS_COMBO_BOX( priv->mode_combo ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-mode-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->mode_combo );

	store = gtk_list_store_new( MODE_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING );
	gtk_combo_box_set_model( GTK_COMBO_BOX( priv->mode_combo ), GTK_TREE_MODEL( store ));
	g_object_unref( store );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->mode_combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->mode_combo ), cell, "text", MODE_COL_LABEL );

	gtk_combo_box_set_id_column( GTK_COMBO_BOX( priv->mode_combo ), MODE_COL_MODE );

	mode_insert_row( self, store, OFA_SFMODE_EXPORT );
	mode_insert_row( self, store, OFA_SFMODE_IMPORT );

	g_signal_connect( priv->mode_combo, "changed", G_CALLBACK( mode_on_changed ), self );
}

static void
mode_insert_row( ofaStreamFormatBin *self, GtkListStore *store, ofeSFMode mode )
{
	gchar *mode_str;
	const gchar *mode_label;
	GtkTreeIter iter;

	mode_str = g_strdup_printf( "%d", mode );
	mode_label = ofa_stream_format_get_mode_str( mode );

	gtk_list_store_insert_with_values( store, &iter, -1,
					MODE_COL_MODE,  mode_str,
					MODE_COL_LABEL, mode_label,
					-1 );

	g_free( mode_str );
}

static void
mode_on_changed( GtkComboBox *box, ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;
	const gchar *mode_str;
	ofeSFMode mode;

	priv = ofa_stream_format_bin_get_instance_private( self );

	mode_str = gtk_combo_box_get_active_id( box );
	mode = my_strlen( mode_str ) ? atoi( mode_str ) : 0;

	switch( mode ){
		case OFA_SFMODE_EXPORT:
			/* have toggle button
			 * remove spin button */
			gtk_widget_show( priv->headers_btn );
			gtk_widget_hide( priv->headers_label );
			gtk_widget_hide( priv->headers_count );
			break;

		case OFA_SFMODE_IMPORT:
			/* hide toggle
			 * have spin button */
			gtk_widget_hide( priv->headers_btn );
			gtk_widget_show( priv->headers_label );
			gtk_widget_show( priv->headers_count );
			break;

		default:
			gtk_widget_hide( priv->headers_btn );
			gtk_widget_hide( priv->headers_label );
			gtk_widget_hide( priv->headers_count );
	}

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
encoding_init( ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;
	GtkListStore *store;
	GtkTreeIter iter;
	GtkCellRenderer *cell;
	GList *charmaps, *it;
	gint i;
	const gchar *cstr;
	GtkWidget *label;

	priv = ofa_stream_format_bin_get_instance_private( self );

	priv->encoding_combo = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-encoding" );
	g_return_if_fail( priv->encoding_combo && GTK_IS_COMBO_BOX( priv->encoding_combo ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-encoding-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->encoding_combo );

	store = gtk_list_store_new( MAP_N_COLUMNS, G_TYPE_STRING );
	gtk_combo_box_set_model( GTK_COMBO_BOX( priv->encoding_combo ), GTK_TREE_MODEL( store ));
	g_object_unref( store );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->encoding_combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->encoding_combo ), cell, "text", MAP_COL_CODE );

	charmaps = encoding_get_available();

	for( it=charmaps, i=0 ; it ; it=it->next, ++i ){
		cstr = ( const gchar * ) it->data;
		gtk_list_store_insert_with_values( store, &iter, -1,
				MAP_COL_CODE, cstr,
				-1 );
	}
	gtk_combo_box_set_id_column( GTK_COMBO_BOX( priv->encoding_combo ), MAP_COL_CODE );

	g_list_free_full( charmaps, ( GDestroyNotify ) g_free );

	g_signal_connect( priv->encoding_combo, "changed", G_CALLBACK( encoding_on_changed ), self );

	priv->has_encoding = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-has-charmap" );
	g_return_if_fail( priv->has_encoding && GTK_IS_CHECK_BUTTON( priv->has_encoding ));

	g_signal_connect( priv->has_encoding, "toggled", G_CALLBACK( encoding_on_has_toggled ), self );
}

/*
 * on Fedora, the 'locales -m' command returns available charmaps
 * alphabetically sorted.
 */
static GList *
encoding_get_available( void )
{
	static const gchar *thisfn = "ofa_stream_format_bin_encoding_get_available";
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

static gchar *
encoding_get_selected( ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *charmap;

	priv = ofa_stream_format_bin_get_instance_private( self );
	charmap = NULL;

	if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX( priv->encoding_combo ), &iter )){
		tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( priv->encoding_combo ));
		g_return_val_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ), FALSE );
		gtk_tree_model_get( tmodel, &iter, MAP_COL_CODE, &charmap, -1 );
	}

	return( charmap );
}

static void
encoding_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;
	gboolean active;

	priv = ofa_stream_format_bin_get_instance_private( self );

	active = gtk_toggle_button_get_active( btn );
	gtk_widget_set_sensitive( priv->encoding_combo, active );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
encoding_on_changed( GtkComboBox *box, ofaStreamFormatBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
date_format_init( ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;
	GtkWidget *widget, *label;

	priv = ofa_stream_format_bin_get_instance_private( self );

	priv->date_combo = my_date_combo_new();

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-date-parent" );
	g_return_if_fail( widget && GTK_IS_CONTAINER( widget ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-date-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->date_combo ));

	gtk_container_add( GTK_CONTAINER( widget ), GTK_WIDGET( priv->date_combo ));

	g_signal_connect( priv->date_combo, "my-changed", G_CALLBACK( date_on_changed ), self );

	priv->has_date = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-has-datefmt" );
	g_return_if_fail( priv->has_date && GTK_IS_CHECK_BUTTON( priv->has_date ));

	g_signal_connect( priv->has_date, "toggled", G_CALLBACK( date_on_has_toggled ), self );
}

static void
date_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;
	gboolean active;

	priv = ofa_stream_format_bin_get_instance_private( self );

	active = gtk_toggle_button_get_active( btn );
	gtk_widget_set_sensitive( GTK_WIDGET( priv->date_combo ), active );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
date_on_changed( myDateCombo *combo, myDateFormat format, ofaStreamFormatBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
thousand_sep_init( ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;
	GtkWidget *label;

	priv = ofa_stream_format_bin_get_instance_private( self );

	priv->thousand_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-thousand" );
	g_return_if_fail( priv->thousand_entry && GTK_IS_ENTRY( priv->thousand_entry ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-thousand-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->thousand_entry );

	g_signal_connect( priv->thousand_entry, "changed", G_CALLBACK( thousand_on_changed ), self );

	priv->has_thousand = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-has-thousand" );
	g_return_if_fail( priv->has_thousand && GTK_IS_CHECK_BUTTON( priv->has_thousand ));

	g_signal_connect( priv->has_thousand, "toggled", G_CALLBACK( thousand_on_has_toggled ), self );
}

static void
thousand_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;
	gboolean active;

	priv = ofa_stream_format_bin_get_instance_private( self );

	active = gtk_toggle_button_get_active( btn );
	gtk_widget_set_sensitive( priv->thousand_entry, active );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
thousand_on_changed( GtkEntry *entry, ofaStreamFormatBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
decimal_dot_init( ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;
	GtkWidget *parent, *label;

	priv = ofa_stream_format_bin_get_instance_private( self );

	priv->decimal_combo = my_decimal_combo_new();

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-decimal-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->decimal_combo ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-decimal-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->decimal_combo ));

	g_signal_connect( priv->decimal_combo, "my-changed", G_CALLBACK( decimal_on_changed ), self );

	priv->has_decimal = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-has-decimal" );
	g_return_if_fail( priv->has_decimal && GTK_IS_CHECK_BUTTON( priv->has_decimal ));

	g_signal_connect( priv->has_decimal, "toggled", G_CALLBACK( decimal_on_has_toggled ), self );
}

static void
decimal_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;
	gboolean active;

	priv = ofa_stream_format_bin_get_instance_private( self );

	active = gtk_toggle_button_get_active( btn );
	gtk_widget_set_sensitive( GTK_WIDGET( priv->decimal_combo ), active );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
decimal_on_changed( myDecimalCombo *combo, const gchar *decimal_sep, ofaStreamFormatBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
field_sep_init( ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;
	GtkWidget *label;

	priv = ofa_stream_format_bin_get_instance_private( self );

	priv->field_combo = my_field_combo_new();
	priv->field_parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-field-parent" );
	g_return_if_fail( priv->field_parent && GTK_IS_CONTAINER( priv->field_parent ));

	gtk_container_add( GTK_CONTAINER( priv->field_parent ), GTK_WIDGET( priv->field_combo ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-field-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->field_combo ));
	priv->field_label = label;

	g_signal_connect( priv->field_combo, "my-changed", G_CALLBACK( field_on_changed ), self );

	priv->has_field = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-has-fieldsep" );
	g_return_if_fail( priv->has_field && GTK_IS_CHECK_BUTTON( priv->has_field ));

	g_signal_connect( priv->has_field, "toggled", G_CALLBACK( field_on_has_toggled ), self );
}

static void
field_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;
	gboolean active;

	priv = ofa_stream_format_bin_get_instance_private( self );

	active = gtk_toggle_button_get_active( btn );
	gtk_widget_set_sensitive( GTK_WIDGET( priv->field_combo ), active );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
field_on_changed( myFieldCombo *combo, const gchar *field_sep, ofaStreamFormatBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
str_delimiter_init( ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;
	GtkWidget *label;

	priv = ofa_stream_format_bin_get_instance_private( self );

	priv->str_delim_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-string-delimiter" );
	g_return_if_fail( priv->str_delim_entry && GTK_IS_ENTRY( priv->str_delim_entry ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-string-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->str_delim_entry );

	g_signal_connect( priv->str_delim_entry, "changed", G_CALLBACK( str_delim_on_changed ), self );

	priv->has_strdelim = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-has-strdelim" );
	g_return_if_fail( priv->has_strdelim && GTK_IS_CHECK_BUTTON( priv->has_strdelim ));

	g_signal_connect( priv->has_strdelim, "toggled", G_CALLBACK( str_delim_on_has_toggled ), self );
}

static void
str_delim_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;
	gboolean active;

	priv = ofa_stream_format_bin_get_instance_private( self );

	active = gtk_toggle_button_get_active( btn );
	gtk_widget_set_sensitive( priv->str_delim_entry, active );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
str_delim_on_changed( GtkEntry *entry, ofaStreamFormatBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
headers_init( ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;

	priv = ofa_stream_format_bin_get_instance_private( self );

	priv->headers_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-headers" );
	g_return_if_fail( priv->headers_btn && GTK_IS_TOGGLE_BUTTON( priv->headers_btn ));

	g_signal_connect( priv->headers_btn, "toggled", G_CALLBACK( headers_on_with_toggled ), self );

	priv->headers_label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-count-label" );
	g_return_if_fail( priv->headers_label && GTK_IS_LABEL( priv->headers_label ));

	priv->headers_count = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-count" );
	g_return_if_fail( priv->headers_count && GTK_IS_SPIN_BUTTON( priv->headers_count ));

	gtk_label_set_mnemonic_widget( GTK_LABEL( priv->headers_label ), priv->headers_count );

	g_signal_connect( priv->headers_count, "value-changed", G_CALLBACK( headers_on_count_changed ), self );

	priv->has_headers = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-has-headers" );
	g_return_if_fail( priv->has_headers && GTK_IS_CHECK_BUTTON( priv->has_headers ));

	g_signal_connect( priv->has_headers, "toggled", G_CALLBACK( headers_on_has_toggled ), self );
}

static void
headers_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;
	gboolean active;

	priv = ofa_stream_format_bin_get_instance_private( self );

	active = gtk_toggle_button_get_active( btn );
	gtk_widget_set_sensitive( priv->headers_btn, active );
	gtk_widget_set_sensitive( priv->headers_count, active );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
headers_on_with_toggled( GtkToggleButton *button, ofaStreamFormatBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
headers_on_count_changed( GtkSpinButton *button, ofaStreamFormatBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_stream_format_bin_get_size_group:
 * @bin: this #ofaStreamFormatBin instance.
 * @column: the desired column number.
 *
 * Returns: the #GtkSizeGroup which managed the @column.
 */
GtkSizeGroup *
ofa_stream_format_bin_get_size_group( const ofaStreamFormatBin *bin, guint column )
{
	static const gchar *thisfn = "ofa_stream_format_bin_get_size_group";
	ofaStreamFormatBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_STREAM_FORMAT_BIN( bin ), NULL );

	priv = ofa_stream_format_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}
	if( column == 1 ){
		return( priv->group1 );
	}

	g_warning( "%s: unkknown column=%d", thisfn, column );
	return( NULL );
}

/**
 * ofa_stream_format_bin_get_name_entry:
 * @bin: this #ofaStreamFormatBin instance.
 *
 * Returns: the #GtkEntry which managed the name of the format.
 */
GtkWidget *
ofa_stream_format_bin_get_name_entry( const ofaStreamFormatBin *bin )
{
	ofaStreamFormatBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_STREAM_FORMAT_BIN( bin ), NULL );

	priv = ofa_stream_format_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->name_entry );
}

/**
 * ofa_stream_format_bin_get_mode_combo:
 * @bin: this #ofaStreamFormatBin instance.
 *
 * Returns: the #GtkComboBox which managed the mode of the format.
 */
GtkWidget *
ofa_stream_format_bin_get_mode_combo( const ofaStreamFormatBin *bin )
{
	ofaStreamFormatBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_STREAM_FORMAT_BIN( bin ), NULL );

	priv = ofa_stream_format_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->mode_combo );
}

/**
 * ofa_stream_format_bin_set_format:
 * @bin: this #ofaStreamFormatBin instance.
 * @format: the new #ofaStreamFormat object to be considered.
 *
 * Release the reference previously taken on the initial object, and
 * then take a new reference on @format.
 */
void
ofa_stream_format_bin_set_format( ofaStreamFormatBin *bin, ofaStreamFormat *format )
{
	ofaStreamFormatBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_STREAM_FORMAT_BIN( bin ));
	g_return_if_fail( format && OFA_IS_STREAM_FORMAT( format ));

	priv = ofa_stream_format_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	g_clear_object( &priv->settings );
	priv->settings = g_object_ref( format );

	setup_format( bin );
}

static void
setup_format( ofaStreamFormatBin *self )
{
	static const gchar *thisfn = "ofa_stream_format_bin_setup_format";
	ofaStreamFormatBinPrivate *priv;
	gchar *str;
	ofeSFMode mode;
	gboolean has, bvalue;
	gint count;
	GtkAdjustment *adjust;

	priv = ofa_stream_format_bin_get_instance_private( self );

	/* name */
	gtk_entry_set_text( GTK_ENTRY( priv->name_entry ), ofa_stream_format_get_name( priv->settings ));

	/* mode */
	mode = ofa_stream_format_get_mode( priv->settings );
	str = g_strdup_printf( "%d", mode );
	gtk_combo_box_set_active_id( GTK_COMBO_BOX( priv->mode_combo ), str );
	g_free( str );

	/* encoding */
	has = ofa_stream_format_get_has_charmap( priv->settings );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->has_encoding ), has );
	encoding_on_has_toggled( GTK_TOGGLE_BUTTON( priv->has_encoding ), self );
	gtk_combo_box_set_active_id( GTK_COMBO_BOX( priv->encoding_combo ), ofa_stream_format_get_charmap( priv->settings ));

	/* date format */
	has = ofa_stream_format_get_has_date( priv->settings );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->has_date ), has );
	date_on_has_toggled( GTK_TOGGLE_BUTTON( priv->has_date ), self );
	my_date_combo_set_selected( priv->date_combo, ofa_stream_format_get_date_format( priv->settings ));

	/* thousand separator */
	has = ofa_stream_format_get_has_thousand( priv->settings );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->has_thousand ), has );
	thousand_on_has_toggled( GTK_TOGGLE_BUTTON( priv->has_thousand ), self );
	str = g_strdup_printf( "%c", ofa_stream_format_get_thousand_sep( priv->settings ));
	if( my_strlen( str )){
		gtk_entry_set_text( GTK_ENTRY( priv->thousand_entry ), str );
	}
	g_free( str );

	/* decimal separator */
	has = ofa_stream_format_get_has_decimal( priv->settings );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->has_decimal ), has );
	decimal_on_has_toggled( GTK_TOGGLE_BUTTON( priv->has_decimal ), self );
	str = g_strdup_printf( "%c", ofa_stream_format_get_decimal_sep( priv->settings ));
	my_decimal_combo_set_selected( priv->decimal_combo, str );
	g_free( str );

	/* field separator */
	has = ofa_stream_format_get_has_field( priv->settings );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->has_field ), has );
	field_on_has_toggled( GTK_TOGGLE_BUTTON( priv->has_field ), self );
	str = g_strdup_printf( "%c", ofa_stream_format_get_field_sep( priv->settings ));
	my_field_combo_set_selected( priv->field_combo, str );
	g_free( str );

	/* string delimiter */
	has = ofa_stream_format_get_has_strdelim( priv->settings );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->has_strdelim ), has );
	str_delim_on_has_toggled( GTK_TOGGLE_BUTTON( priv->has_strdelim ), self );
	str = g_strdup_printf( "%c", ofa_stream_format_get_string_delim( priv->settings ));
	gtk_entry_set_text( GTK_ENTRY( priv->str_delim_entry ), str );
	g_free( str );

	/* headers */
	has = ofa_stream_format_get_has_headers( priv->settings );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->has_headers ), has );
	headers_on_has_toggled( GTK_TOGGLE_BUTTON( priv->has_headers ), self );
	switch( mode ){
		case OFA_SFMODE_EXPORT:
			bvalue = ofa_stream_format_get_with_headers( priv->settings );
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->headers_btn ), bvalue );
			headers_on_with_toggled( GTK_TOGGLE_BUTTON( priv->headers_btn ), self );
			break;

		case OFA_SFMODE_IMPORT:
			count = ofa_stream_format_get_headers_count( priv->settings );
			adjust = gtk_adjustment_new( count, 0, 9999, 1, 10, 10 );
			gtk_spin_button_set_adjustment( GTK_SPIN_BUTTON( priv->headers_count), adjust );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( priv->headers_count ), count );
			break;

		default:
			g_warning( "%s: mode=%d is not Export not Import", thisfn, mode );
	}

	/* mode */
	mode_on_changed( GTK_COMBO_BOX( priv->mode_combo ), self );
}

/**
 * ofa_stream_format_bin_is_valid:
 * @bin: this #ofaStreamFormatBin instance.
 * @error_message: [allow-none]: pointer to an error message.
 *  If set, then this is a newly allocated string which should be
 *  g_free() by the caller.
 *
 * Returns: %TRUE if selection is ok.
 */
gboolean
ofa_stream_format_bin_is_valid( ofaStreamFormatBin *bin, gchar **error_message )
{
	ofaStreamFormatBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_STREAM_FORMAT_BIN( bin ), FALSE );

	priv = ofa_stream_format_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( is_validable( bin, error_message ));
}

static gboolean
is_validable( ofaStreamFormatBin *self, gchar **error_message )
{
	ofaStreamFormatBinPrivate *priv;
	const gchar *cstr;
	ofeSFMode mode;
	gchar *charmap, *decimal_sep, *field_sep;
	gint ivalue;
	gboolean has;

	priv = ofa_stream_format_bin_get_instance_private( self );

	if( error_message ){
		*error_message = NULL;
	}

	/* name */
	cstr = gtk_entry_get_text( GTK_ENTRY( priv->name_entry ));
	if( !my_strlen( cstr )){
		if( error_message ){
			*error_message = g_strdup( _( "Empty name" ));
		}
		return( FALSE );
	}

	/* mode */
	cstr = gtk_combo_box_get_active_id( GTK_COMBO_BOX( priv->mode_combo ));
	if( !my_strlen( cstr )){
		if( error_message ){
			*error_message = g_strdup( _( "No selected mode" ));
		}
		return( FALSE );
	}
	mode = atoi( cstr );
	if( mode != OFA_SFMODE_EXPORT && mode != OFA_SFMODE_IMPORT ){
		if( error_message ){
			*error_message = g_strdup_printf( _( "Invalid or unknown mode: %s" ), cstr );
		}
		return( FALSE );
	}

	/* charmap */
	has = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->has_encoding ));
	if( has ){
		charmap = encoding_get_selected( self );
		if( !my_strlen( charmap )){
			g_free( charmap );
			if( error_message ){
				*error_message = g_strdup( _( "Invalid or unknown characters encoding type" ));
			}
			return( FALSE );
		}
		g_free( charmap );
	}

	/* date format */
	has = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->has_date ));
	if( has ){
		ivalue = my_date_combo_get_selected( priv->date_combo );
		if( ivalue < MY_DATE_FIRST ){
			if( error_message ){
				*error_message = g_strdup( _( "Invalid or unknown date format" ));
			}
			return( FALSE );
		}
	}

	/* thousand separator - empty is valid */

	/* decimal separator */
	has = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->has_decimal ));
	if( has ){
		decimal_sep = my_decimal_combo_get_selected( priv->decimal_combo );
		if( !my_strlen( decimal_sep )){
			g_free( decimal_sep );
			if( error_message ){
				*error_message = g_strdup( _( "Invalid or unknown decimal separator" ));
			}
			return( FALSE );
		}
		g_free( decimal_sep );
	}

	/* field separator */
	has = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->has_field ));
	if( has ){
		field_sep = my_field_combo_get_selected( priv->field_combo );
		if( !my_strlen( field_sep )){
			g_free( field_sep );
			if( error_message ){
				*error_message = g_strdup( _( "Invalid or unknown field separator" ));
			}
			return( FALSE );
		}
		g_free( field_sep );
	}

	return( TRUE );
}

/**
 * ofa_stream_format_bin_apply:
 *
 * This take the current selection out of the dialog box, setting the
 * user preferences.
 *
 * Returns: %TRUE if selection is ok
 */
gboolean
ofa_stream_format_bin_apply( ofaStreamFormatBin *settings )
{
	ofaStreamFormatBinPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_STREAM_FORMAT_BIN( settings ), FALSE );

	priv = ofa_stream_format_bin_get_instance_private( settings );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	if( !is_validable( settings, NULL )){
		return( FALSE );
	}

	return( do_apply( settings ));
}

static gboolean
do_apply( ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;
	gboolean has_charmap, has_date, has_thousand, has_decimal, has_field, has_str, has_headers;
	gchar *charmap, *thousand_sep, *decimal_sep, *field_sep, *strdelim;
	gint datefmt, iheaders;
	ofeSFMode mode;
	const gchar *cstr;

	priv = ofa_stream_format_bin_get_instance_private( self );

	cstr = gtk_entry_get_text( GTK_ENTRY( priv->name_entry ));
	g_return_val_if_fail( my_strlen( cstr ), FALSE );
	ofa_stream_format_set_name( priv->settings, cstr );

	cstr = gtk_combo_box_get_active_id( GTK_COMBO_BOX( priv->mode_combo ));
	g_return_val_if_fail( my_strlen( cstr ), FALSE );
	mode = atoi( cstr );
	ofa_stream_format_set_mode( priv->settings, mode );

	has_charmap = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->has_encoding ));
	charmap = has_charmap ? encoding_get_selected( self ) : NULL;

	has_date = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->has_date ));
	datefmt = has_date ? my_date_combo_get_selected( priv->date_combo ) : 0;

	has_thousand = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->has_thousand ));
	cstr = has_thousand ? gtk_entry_get_text( GTK_ENTRY( priv->thousand_entry )) : NULL;
	thousand_sep = g_strdup( cstr ? cstr : "" );

	has_decimal = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->has_decimal ));
	decimal_sep = has_decimal ? my_decimal_combo_get_selected( priv->decimal_combo ) : g_strdup( "" );

	has_field = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->has_field ));
	field_sep = has_field ? my_field_combo_get_selected( priv->field_combo ) : g_strdup( NULL );

	has_str = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->has_strdelim ));
	cstr = has_str ? gtk_entry_get_text( GTK_ENTRY( priv->str_delim_entry )) : NULL;
	strdelim = g_strdup( cstr ? cstr : "");

	has_headers = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->has_headers ));
	if( mode == OFA_SFMODE_EXPORT ){
		iheaders = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->headers_btn ));
	} else {
		iheaders = gtk_spin_button_get_value( GTK_SPIN_BUTTON( priv->headers_count ));
	}

	ofa_stream_format_set( priv->settings,
									has_charmap, charmap,
									has_date, datefmt,
									has_thousand, thousand_sep[0],
									has_decimal, decimal_sep[0],
									has_field, field_sep[0],
									has_str, strdelim[0],
									has_headers, iheaders );

	g_free( strdelim );
	g_free( field_sep );
	g_free( decimal_sep );
	g_free( thousand_sep );
	g_free( charmap );

	return( TRUE );
}
