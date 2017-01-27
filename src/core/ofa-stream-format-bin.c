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
#include "my/my-field-combo.h"
#include "my/my-ibin.h"
#include "my/my-thousand-combo.h"
#include "my/my-utils.h"

#include "ofa-stream-format-bin.h"

/* private instance data
 */
typedef struct {
	gboolean         dispose_has_run;

	/* initialization data
	 */
	ofaStreamFormat *format;
	gboolean         name_sensitive;
	gboolean         mode_sensitive;
	gboolean         updatable;

	/* UI
	 */
	GtkWidget       *name_entry;
	GtkWidget       *mode_combo;
	GtkWidget       *has_encoding;
	GtkWidget       *encoding_combo;
	GtkWidget       *has_date;
	myDateCombo     *date_combo;
	GtkWidget       *has_thousand;
	myThousandCombo *thousand_combo;
	GtkWidget       *has_decimal;
	myDecimalCombo  *decimal_combo;
	GtkWidget       *has_field;
	myFieldCombo    *field_combo;
	GtkWidget       *field_parent;
	GtkWidget       *field_label;
	GtkWidget       *has_strdelim;
	GtkWidget       *strdelim_entry;
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

static void          setup_bin( ofaStreamFormatBin *bin );
static void          name_init( ofaStreamFormatBin *self );
static void          name_on_changed( GtkEntry *entry, ofaStreamFormatBin *self );
static void          name_set_sensitive( ofaStreamFormatBin *self );
static void          mode_init( ofaStreamFormatBin *self );
static void          mode_insert_row( ofaStreamFormatBin *self, GtkListStore *store, ofeSFMode mode );
static void          mode_on_changed( GtkComboBox *combo, ofaStreamFormatBin *self );
static void          mode_set_sensitive( ofaStreamFormatBin *self );
static void          encoding_init( ofaStreamFormatBin *self );
static GList        *encoding_get_available( void );
static GList        *encoding_get_defaults( void );
static gchar        *encoding_get_selected( ofaStreamFormatBin *self );
static void          encoding_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self );
static void          encoding_on_changed( GtkComboBox *box, ofaStreamFormatBin *self );
static void          date_format_init( ofaStreamFormatBin *self );
static void          date_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self );
static void          date_on_changed( myDateCombo *combo, myDateFormat format, ofaStreamFormatBin *self );
static void          thousand_sep_init( ofaStreamFormatBin *self );
static void          thousand_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self );
static void          thousand_on_changed( myThousandCombo *combo, const gchar *thousand_sep, ofaStreamFormatBin *self );
static void          decimal_dot_init( ofaStreamFormatBin *self );
static void          decimal_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self );
static void          decimal_on_changed( myDecimalCombo *combo, const gchar *decimal_sep, ofaStreamFormatBin *self );
static void          field_sep_init( ofaStreamFormatBin *self );
static void          field_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self );
static void          field_on_changed( myFieldCombo *combo, const gchar *field_sep, ofaStreamFormatBin *self );
static void          str_delimiter_init( ofaStreamFormatBin *self );
static void          str_delim_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self );
static void          str_delim_on_changed( GtkEntry *entry, ofaStreamFormatBin *self );
static void          headers_init( ofaStreamFormatBin *self );
static void          headers_on_with_toggled( GtkToggleButton *button, ofaStreamFormatBin *self );
static void          headers_on_count_changed( GtkSpinButton *button, ofaStreamFormatBin *self );
static void          headers_set_sensitive( ofaStreamFormatBin *self );
static void          setup_format( ofaStreamFormatBin *bin );
static void          setup_updatable( ofaStreamFormatBin *self );
static gboolean      is_validable( ofaStreamFormatBin *self, gchar **error_message );
static gboolean      do_apply( ofaStreamFormatBin *self );
static void          ibin_iface_init( myIBinInterface *iface );
static guint         ibin_get_interface_version( void );
static GtkSizeGroup *ibin_get_size_group( const myIBin *instance, guint column );
static gboolean      ibin_is_valid( const myIBin *instance, gchar **msgerr );
static void          ibin_apply( myIBin *instance );

G_DEFINE_TYPE_EXTENDED( ofaStreamFormatBin, ofa_stream_format_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaStreamFormatBin )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IBIN, ibin_iface_init ))

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
		g_clear_object( &priv->format );
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
	priv->name_sensitive = TRUE;
	priv->mode_sensitive = TRUE;
	priv->updatable = TRUE;
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
		priv->format = g_object_ref( format );
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
name_set_sensitive( ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;

	priv = ofa_stream_format_bin_get_instance_private( self );

	gtk_widget_set_sensitive( priv->name_entry, priv->name_sensitive && priv->updatable );
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

/*
 * does not use gtk_widget_show/hide as this will be superseded
 * by gtk_widget_show_all from myIDialog...
 */
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
			gtk_widget_set_sensitive( priv->headers_btn, TRUE );
			gtk_widget_set_sensitive( priv->headers_label, FALSE );
			gtk_widget_set_sensitive( priv->headers_count, FALSE );
			break;

		case OFA_SFMODE_IMPORT:
			/* hide toggle
			 * have spin button */
			gtk_widget_set_sensitive( priv->headers_btn, FALSE );
			gtk_widget_set_sensitive( priv->headers_label, TRUE );
			gtk_widget_set_sensitive( priv->headers_count, TRUE );
			break;

		default:
			gtk_widget_set_sensitive( priv->headers_btn, TRUE );
			gtk_widget_set_sensitive( priv->headers_label, TRUE );
			gtk_widget_set_sensitive( priv->headers_count, TRUE );
	}

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
mode_set_sensitive( ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;

	priv = ofa_stream_format_bin_get_instance_private( self );

	gtk_widget_set_sensitive( priv->mode_combo, priv->mode_sensitive && priv->updatable );
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
	if( !g_list_length( charmaps )){
		charmaps = encoding_get_defaults();
	}

	for( it=charmaps, i=0 ; it ; it=it->next, ++i ){
		cstr = ( const gchar * ) it->data;
		gtk_list_store_insert_with_values( store, &iter, -1,
				MAP_COL_CODE, cstr,
				-1 );
	}

	g_list_free_full( charmaps, ( GDestroyNotify ) g_free );

	gtk_combo_box_set_id_column( GTK_COMBO_BOX( priv->encoding_combo ), MAP_COL_CODE );

	g_signal_connect( priv->encoding_combo, "changed", G_CALLBACK( encoding_on_changed ), self );

	priv->has_encoding = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-has-charmap" );
	g_return_if_fail( priv->has_encoding && GTK_IS_CHECK_BUTTON( priv->has_encoding ));

	g_signal_connect( priv->has_encoding, "toggled", G_CALLBACK( encoding_on_has_toggled ), self );
}

/*
 * On Fedora, the 'locale -m' command returns available charmaps
 * alphabetically sorted.
 *
 * Fedora 24:
 * 'locale -m' returns an empty list
 * 'iconv -l' returns a lot of comma-separated known character sets with aliases
 * grep -e ^module /usr/lib64/gconv/gconv-modules | awk '{ print $2 }' | grep -v INTERNAL | sort -u returns 270 lines
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

/*
 * when unable to get locally installed charsets,
 * we provide this set of defaults from charsets (7) man page
 */
static GList *
encoding_get_defaults( void )
{
	static gchar* st_charsets[] = {
			"ASCII",
			"BIG5",
			"ISO-2022",
			"ISO-4873",
			"ISO-8859-1",
			"ISO-8859-2",
			"ISO-8859-3",
			"ISO-8859-4",
			"ISO-8859-5",
			"ISO-8859-6",
			"ISO-8859-7",
			"ISO-8859-8",
			"ISO-8859-9",
			"ISO-8859-10",
			"ISO-8859-11",
			"ISO-8859-12",
			"ISO-8859-13",
			"ISO-8859-14",
			"ISO-8859-15",
			"ISO-8859-16",
			"GB-2312",
			"JIS-X-0208",
			"KOI8-R",
			"KOI8-U",
			"KS-X-1001",
			"TIS-620",
			"UTF-8",
			"UTF-16",
			NULL,
	};
	gint i;
	GList *charmaps;

	charmaps = NULL;

	for( i=0 ; st_charsets[i] ; ++i ){
		charmaps = g_list_prepend( charmaps, g_strdup( st_charsets[i] ));
	}

	return( g_list_reverse( charmaps ));
}

static gchar *
encoding_get_selected( ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;
	const gchar *charmap;

	priv = ofa_stream_format_bin_get_instance_private( self );

	charmap = gtk_combo_box_get_active_id( GTK_COMBO_BOX( priv->encoding_combo ));

	return( g_strdup( charmap ));
}

static void
encoding_on_has_toggled( GtkToggleButton *btn, ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;
	gboolean active;

	priv = ofa_stream_format_bin_get_instance_private( self );

	active = gtk_toggle_button_get_active( btn );
	gtk_widget_set_sensitive( priv->encoding_combo, active && priv->updatable );

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
	gtk_widget_set_sensitive( GTK_WIDGET( priv->date_combo ), active && priv->updatable );

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
	GtkWidget *parent, *label;

	priv = ofa_stream_format_bin_get_instance_private( self );

	priv->thousand_combo = my_thousand_combo_new();

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-thousand-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->thousand_combo ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-thousand-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->thousand_combo ));

	g_signal_connect( priv->thousand_combo, "my-changed", G_CALLBACK( thousand_on_changed ), self );

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
	gtk_widget_set_sensitive( GTK_WIDGET( priv->thousand_combo ), active && priv->updatable );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
thousand_on_changed( myThousandCombo *combo, const gchar *thousand_sep, ofaStreamFormatBin *self )
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
	gtk_widget_set_sensitive( GTK_WIDGET( priv->decimal_combo ), active && priv->updatable );

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
	gtk_widget_set_sensitive( GTK_WIDGET( priv->field_combo ), active && priv->updatable );

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

	priv->strdelim_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-string-delimiter" );
	g_return_if_fail( priv->strdelim_entry && GTK_IS_ENTRY( priv->strdelim_entry ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ffb-string-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->strdelim_entry );

	g_signal_connect( priv->strdelim_entry, "changed", G_CALLBACK( str_delim_on_changed ), self );

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
	gtk_widget_set_sensitive( priv->strdelim_entry, active && priv->updatable );

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

static void
headers_set_sensitive( ofaStreamFormatBin *self )
{
	static const gchar *thisfn = "ofa_stream_format_bin_headers_st_sensitive";
	ofaStreamFormatBinPrivate *priv;
	ofeSFMode mode;

	priv = ofa_stream_format_bin_get_instance_private( self );

	mode = priv->format ? ofa_stream_format_get_mode( priv->format ) : ofa_stream_format_get_default_mode();

	switch( mode ){
		case OFA_SFMODE_EXPORT:
			gtk_widget_set_sensitive( priv->headers_btn, priv->updatable );
			break;

		case OFA_SFMODE_IMPORT:
			gtk_widget_set_sensitive( priv->headers_count, priv->updatable );
			break;

		default:
			g_warning( "%s: mode=%d is not Export not Import", thisfn, mode );
	}
}

/**
 * ofa_stream_format_bin_set_name_sensitive:
 * @bin: this #ofaStreamFormatBin instance.
 * @sensitive: whether the name may be modified.
 */
void
ofa_stream_format_bin_set_name_sensitive( ofaStreamFormatBin *bin, gboolean sensitive )
{
	ofaStreamFormatBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_STREAM_FORMAT_BIN( bin ));

	priv = ofa_stream_format_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run);

	priv->name_sensitive = sensitive;

	name_set_sensitive( bin );
}

/**
 * ofa_stream_format_bin_set_mode_sensitive:
 * @bin: this #ofaStreamFormatBin instance.
 * @sensitive: whether the mode may be modified.
 */
void
ofa_stream_format_bin_set_mode_sensitive( ofaStreamFormatBin *bin, gboolean sensitive )
{
	ofaStreamFormatBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_STREAM_FORMAT_BIN( bin ));

	priv = ofa_stream_format_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run);

	priv->mode_sensitive = sensitive;

	mode_set_sensitive( bin );
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

	g_clear_object( &priv->format );
	priv->format = g_object_ref( format );

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
	gtk_entry_set_text( GTK_ENTRY( priv->name_entry ), ofa_stream_format_get_name( priv->format ));

	/* mode */
	mode = ofa_stream_format_get_mode( priv->format );
	str = g_strdup_printf( "%d", mode );
	gtk_combo_box_set_active_id( GTK_COMBO_BOX( priv->mode_combo ), str );
	g_free( str );
	mode_on_changed( GTK_COMBO_BOX( priv->mode_combo ), self );

	/* encoding */
	has = ofa_stream_format_get_has_charmap( priv->format );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->has_encoding ), has );
	encoding_on_has_toggled( GTK_TOGGLE_BUTTON( priv->has_encoding ), self );
	gtk_combo_box_set_active_id( GTK_COMBO_BOX( priv->encoding_combo ), ofa_stream_format_get_charmap( priv->format ));

	/* date format */
	has = ofa_stream_format_get_has_date( priv->format );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->has_date ), has );
	date_on_has_toggled( GTK_TOGGLE_BUTTON( priv->has_date ), self );
	my_date_combo_set_selected( priv->date_combo, ofa_stream_format_get_date_format( priv->format ));

	/* thousand separator */
	has = ofa_stream_format_get_has_thousand( priv->format );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->has_thousand ), has );
	thousand_on_has_toggled( GTK_TOGGLE_BUTTON( priv->has_thousand ), self );
	str = g_strdup_printf( "%c", ofa_stream_format_get_thousand_sep( priv->format ));
	my_thousand_combo_set_selected( MY_THOUSAND_COMBO( priv->thousand_combo ), str );
	g_free( str );

	/* decimal separator */
	has = ofa_stream_format_get_has_decimal( priv->format );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->has_decimal ), has );
	decimal_on_has_toggled( GTK_TOGGLE_BUTTON( priv->has_decimal ), self );
	str = g_strdup_printf( "%c", ofa_stream_format_get_decimal_sep( priv->format ));
	my_decimal_combo_set_selected( priv->decimal_combo, str );
	g_free( str );

	/* field separator */
	has = ofa_stream_format_get_has_field( priv->format );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->has_field ), has );
	field_on_has_toggled( GTK_TOGGLE_BUTTON( priv->has_field ), self );
	str = g_strdup_printf( "%c", ofa_stream_format_get_field_sep( priv->format ));
	my_field_combo_set_selected( priv->field_combo, str );
	g_free( str );

	/* string delimiter */
	has = ofa_stream_format_get_has_strdelim( priv->format );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->has_strdelim ), has );
	str_delim_on_has_toggled( GTK_TOGGLE_BUTTON( priv->has_strdelim ), self );
	str = g_strdup_printf( "%c", ofa_stream_format_get_string_delim( priv->format ));
	if( my_strlen( str )){
		gtk_entry_set_text( GTK_ENTRY( priv->strdelim_entry ), str );
	}
	g_free( str );

	/* headers */
	switch( mode ){
		case OFA_SFMODE_EXPORT:
			bvalue = ofa_stream_format_get_with_headers( priv->format );
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->headers_btn ), bvalue );
			headers_on_with_toggled( GTK_TOGGLE_BUTTON( priv->headers_btn ), self );
			break;

		case OFA_SFMODE_IMPORT:
			count = ofa_stream_format_get_headers_count( priv->format );
			adjust = gtk_adjustment_new( count, 0, 9999, 1, 10, 10 );
			gtk_spin_button_set_adjustment( GTK_SPIN_BUTTON( priv->headers_count), adjust );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( priv->headers_count ), count );
			break;

		default:
			g_warning( "%s: mode=%d is not Export not Import", thisfn, mode );
	}
}

/**
 * ofa_stream_format_bin_set_updatable:
 * @bin: this #ofaStreamFormatBin instance.
 * @updatable: whether the stream format may be updated by the user.
 *
 * Defaults to %TRUE.
 */
void
ofa_stream_format_bin_set_updatable( ofaStreamFormatBin *bin, gboolean updatable )
{
	ofaStreamFormatBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_STREAM_FORMAT_BIN( bin ));

	priv = ofa_stream_format_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	priv->updatable = updatable;

	setup_updatable( bin );
}

static void
setup_updatable( ofaStreamFormatBin *self )
{
	ofaStreamFormatBinPrivate *priv;

	priv = ofa_stream_format_bin_get_instance_private( self );

	/* name */
	gtk_widget_set_sensitive( priv->name_entry, priv->updatable );

	/* mode */
	mode_set_sensitive( self );

	/* encoding */
	gtk_widget_set_sensitive( priv->has_encoding, priv->updatable );
	encoding_on_has_toggled( GTK_TOGGLE_BUTTON( priv->has_encoding ), self );

	/* date format */
	gtk_widget_set_sensitive( priv->has_date, priv->updatable );
	date_on_has_toggled( GTK_TOGGLE_BUTTON( priv->has_date ), self );

	/* thousand separator */
	gtk_widget_set_sensitive( priv->has_thousand, priv->updatable );
	thousand_on_has_toggled( GTK_TOGGLE_BUTTON( priv->has_thousand ), self );

	/* decimal separator */
	gtk_widget_set_sensitive( priv->has_decimal, priv->updatable );
	decimal_on_has_toggled( GTK_TOGGLE_BUTTON( priv->has_decimal ), self );

	/* field separator */
	gtk_widget_set_sensitive( priv->has_field, priv->updatable );
	field_on_has_toggled( GTK_TOGGLE_BUTTON( priv->has_field ), self );

	/* string delimiter */
	gtk_widget_set_sensitive( priv->has_strdelim, priv->updatable );
	str_delim_on_has_toggled( GTK_TOGGLE_BUTTON( priv->has_strdelim ), self );

	/* headers */
	headers_set_sensitive( self );
}

static gboolean
is_validable( ofaStreamFormatBin *self, gchar **error_message )
{
	ofaStreamFormatBinPrivate *priv;
	const gchar *cstr;
	ofeSFMode mode;
	gchar *charmap, *thousand_sep, *decimal_sep, *field_sep;
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
			*error_message = g_strdup( _( "Name is empty" ));
		}
		return( FALSE );
	}

	/* mode */
	cstr = gtk_combo_box_get_active_id( GTK_COMBO_BOX( priv->mode_combo ));
	if( !my_strlen( cstr )){
		if( error_message ){
			*error_message = g_strdup( _( "No mode is selected" ));
		}
		return( FALSE );
	}
	mode = atoi( cstr );
	if( mode != OFA_SFMODE_EXPORT && mode != OFA_SFMODE_IMPORT ){
		if( error_message ){
			*error_message = g_strdup_printf( _( "Mode '%s' is unknown or invalid" ), cstr );
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
				*error_message = g_strdup( _( "Characters encoding type is unknown or invalid" ));
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
				*error_message = g_strdup( _( "Date format is unknown or invalid" ));
			}
			return( FALSE );
		}
	}

	/* thousand separator */
	has = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->has_thousand ));
	if( has ){
		thousand_sep = my_thousand_combo_get_selected( priv->thousand_combo );
		if( !my_strlen( thousand_sep )){
			g_free( thousand_sep );
			if( error_message ){
				*error_message = g_strdup( _( "Thousand separator is unknown or invalid" ));
			}
			return( FALSE );
		}
		g_free( thousand_sep );
	}

	/* decimal separator */
	has = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->has_decimal ));
	if( has ){
		decimal_sep = my_decimal_combo_get_selected( priv->decimal_combo );
		if( !my_strlen( decimal_sep )){
			g_free( decimal_sep );
			if( error_message ){
				*error_message = g_strdup( _( "Decimal separator is unknown or invalid" ));
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
				*error_message = g_strdup( _( "Field separator is unknown or invalid" ));
			}
			return( FALSE );
		}
		g_free( field_sep );
	}

	/* string delimiter */
	has = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->has_strdelim ));
	if( has ){
		cstr = gtk_entry_get_text( GTK_ENTRY( priv->strdelim_entry ));
		if( !my_strlen( cstr )){
			if( error_message ){
				*error_message = g_strdup( _( "String delimiter is unknown or invalid" ));
			}
			return( FALSE );
		}
	}

	return( TRUE );
}

static gboolean
do_apply( ofaStreamFormatBin *self )
{
	static const gchar *thisfn = "ofa_stream_format_bin_do_apply";
	ofaStreamFormatBinPrivate *priv;
	gboolean has_charmap, has_date, has_thousand, has_decimal, has_field, has_str;
	gchar *charmap, *thousand_sep, *decimal_sep, *field_sep, *strdelim;
	gint datefmt, iheaders;
	ofeSFMode mode;
	const gchar *cstr;

	priv = ofa_stream_format_bin_get_instance_private( self );

	cstr = gtk_entry_get_text( GTK_ENTRY( priv->name_entry ));
	g_return_val_if_fail( my_strlen( cstr ), FALSE );
	ofa_stream_format_set_name( priv->format, cstr );

	cstr = gtk_combo_box_get_active_id( GTK_COMBO_BOX( priv->mode_combo ));
	g_return_val_if_fail( my_strlen( cstr ), FALSE );
	mode = atoi( cstr );
	ofa_stream_format_set_mode( priv->format, mode );

	has_charmap = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->has_encoding ));
	charmap = has_charmap ? encoding_get_selected( self ) : NULL;

	has_date = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->has_date ));
	datefmt = has_date ? my_date_combo_get_selected( priv->date_combo ) : 0;

	has_thousand = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->has_thousand ));
	thousand_sep = has_thousand ? my_thousand_combo_get_selected( priv->thousand_combo ) : g_strdup( "" );

	has_decimal = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->has_decimal ));
	decimal_sep = has_decimal ? my_decimal_combo_get_selected( priv->decimal_combo ) : g_strdup( "" );

	has_field = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->has_field ));
	field_sep = has_field ? my_field_combo_get_selected( priv->field_combo ) : g_strdup( "" );

	has_str = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->has_strdelim ));
	cstr = has_str ? gtk_entry_get_text( GTK_ENTRY( priv->strdelim_entry )) : NULL;
	strdelim = g_strdup( cstr ? cstr : "");

	if( mode == OFA_SFMODE_EXPORT ){
		iheaders = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->headers_btn ));
	} else {
		iheaders = gtk_spin_button_get_value( GTK_SPIN_BUTTON( priv->headers_count ));
	}

	g_debug( "%s: format=%p, has_charmap=%s, charmap=%s, has_date=%s, datefmt=%u, "
			"has_thousand=%s, thousand_sep=%s, has_decimal=%s, decimal_sep=%s, "
			"has_field=%s, field_sep=%s, has_str=%s, strdelim=%s, iheaders=%u",
			thisfn, ( void * ) priv->format,
			has_charmap ? "True":"False", charmap, has_date ? "True":"False", datefmt,
			has_thousand ? "True":"False", thousand_sep, has_decimal ? "True":"False", decimal_sep,
			has_field ? "True":"False", field_sep, has_str ? "True":"False", strdelim,
			iheaders );

	ofa_stream_format_set( priv->format,
									has_charmap, charmap,
									has_date, datefmt,
									has_thousand, thousand_sep[0],
									has_decimal, decimal_sep[0],
									has_field, field_sep[0],
									has_str, strdelim[0],
									iheaders );

	g_free( strdelim );
	g_free( field_sep );
	g_free( decimal_sep );
	g_free( thousand_sep );
	g_free( charmap );

	return( TRUE );
}

/*
 * myIBin interface management
 */
static void
ibin_iface_init( myIBinInterface *iface )
{
	static const gchar *thisfn = "ofa_stream_format_bin_ibin_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = ibin_get_interface_version;
	iface->get_size_group = ibin_get_size_group;
	iface->is_valid = ibin_is_valid;
	iface->apply = ibin_apply;
}

static guint
ibin_get_interface_version( void )
{
	return( 1 );
}

static GtkSizeGroup *
ibin_get_size_group( const myIBin *instance, guint column )
{
	static const gchar *thisfn = "ofa_stream_format_bin_ibin_get_size_group";
	ofaStreamFormatBinPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_STREAM_FORMAT_BIN( instance ), NULL );

	priv = ofa_stream_format_bin_get_instance_private( OFA_STREAM_FORMAT_BIN( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}
	if( column == 1 ){
		return( priv->group1 );
	}

	g_warning( "%s: invalid column=%d", thisfn, column );

	return( NULL );
}

gboolean
ibin_is_valid( const myIBin *instance, gchar **msgerr )
{
	ofaStreamFormatBinPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_STREAM_FORMAT_BIN( instance ), FALSE );

	priv = ofa_stream_format_bin_get_instance_private( OFA_STREAM_FORMAT_BIN( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( is_validable( OFA_STREAM_FORMAT_BIN( instance ), msgerr ));
}

static void
ibin_apply( myIBin *instance )
{
	ofaStreamFormatBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_STREAM_FORMAT_BIN( instance ));
	g_return_if_fail( !my_ibin_is_valid( instance, NULL ));

	priv = ofa_stream_format_bin_get_instance_private( OFA_STREAM_FORMAT_BIN( instance ));

	g_return_if_fail( !priv->dispose_has_run );

	do_apply( OFA_STREAM_FORMAT_BIN( instance ));
}
