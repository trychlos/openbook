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

#include "core/ofa-file-format.h"

#include "ui/my-date-combo.h"
#include "ui/my-decimal-combo.h"
#include "ui/ofa-file-format-piece.h"

/* private instance data
 */
struct _ofaFileFormatPiecePrivate {
	gboolean           dispose_has_run;

	/* runtime data
	 */
	gchar          *prefs_name;
	ofaFileFormat  *settings;
	ofaFFmt         format;

	/* UI
	 */
	GtkContainer   *parent;				/* from the hosting dialog */
	GtkContainer   *container;			/* our top container */

	GtkWidget      *format_combo;		/* file format */
	GtkWidget      *encoding_combo;
	myDateCombo    *date_combo;
	myDecimalCombo *decimal_combo;
	GtkWidget      *fieldsep_label;
	GtkWidget      *fieldsep_combo;
	GtkWidget      *headers_btn;
	GtkWidget      *folder_btn;
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

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_window_xml       = PKGUIDIR "/ofa-file-format-piece.ui";
static const gchar *st_window_id        = "FileFormatPiece";

G_DEFINE_TYPE( ofaFileFormatPiece, ofa_file_format_piece, G_TYPE_OBJECT )

static void     on_parent_finalized( ofaFileFormatPiece *self, GObject *finalized_parent );
static void     init_file_format( ofaFileFormatPiece *self );
static void     on_ffmt_changed( GtkComboBox *box, ofaFileFormatPiece *self );
static void     init_encoding( ofaFileFormatPiece *self );
static void     on_encoding_changed( GtkComboBox *box, ofaFileFormatPiece *self );
static void     init_date_format( ofaFileFormatPiece *self );
static void     on_date_changed( myDateCombo *combo, myDateFormat format, ofaFileFormatPiece *self );
static void     init_decimal_dot( ofaFileFormatPiece *self );
static void     on_decimal_changed( myDecimalCombo *combo, const gchar *decimal_sep, ofaFileFormatPiece *self );
static void     init_field_separator( ofaFileFormatPiece *self );
static void     on_field_changed( GtkComboBox *box, ofaFileFormatPiece *self );
static void     init_headers( ofaFileFormatPiece *self );
static void     on_headers_toggled( GtkToggleButton *button, ofaFileFormatPiece *self );
static gboolean is_validable( ofaFileFormatPiece *self );
static gint     get_file_format( ofaFileFormatPiece *self );
static gchar   *get_charmap( ofaFileFormatPiece *self );
static gchar   *get_field_sep( ofaFileFormatPiece *self );
static gboolean do_apply( ofaFileFormatPiece *self );
static GList   *get_available_charmaps( void );

static void
file_format_piece_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_file_format_piece_finalize";
	ofaFileFormatPiecePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_FILE_FORMAT_PIECE( instance ));

	/* free data members here */
	priv = OFA_FILE_FORMAT_PIECE( instance )->priv;

	g_free( priv->prefs_name );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_file_format_piece_parent_class )->finalize( instance );
}

static void
file_format_piece_dispose( GObject *instance )
{
	ofaFileFormatPiecePrivate *priv;

	g_return_if_fail( instance && OFA_IS_FILE_FORMAT_PIECE( instance ));

	priv = OFA_FILE_FORMAT_PIECE( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->settings );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_file_format_piece_parent_class )->dispose( instance );
}

static void
ofa_file_format_piece_init( ofaFileFormatPiece *self )
{
	static const gchar *thisfn = "ofa_file_format_piece_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_FILE_FORMAT_PIECE( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_FILE_FORMAT_PIECE, ofaFileFormatPiecePrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
ofa_file_format_piece_class_init( ofaFileFormatPieceClass *klass )
{
	static const gchar *thisfn = "ofa_file_format_piece_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = file_format_piece_dispose;
	G_OBJECT_CLASS( klass )->finalize = file_format_piece_finalize;

	g_type_class_add_private( klass, sizeof( ofaFileFormatPiecePrivate ));

	/**
	 * ofaFileFormatPiece::changed:
	 *
	 * This signal is sent when one of the data is changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaFileFormatPiece *piece,
	 * 						gpointer          user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_FILE_FORMAT_PIECE,
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
 * ofa_file_format_piece_new:
 */
ofaFileFormatPiece *
ofa_file_format_piece_new( const gchar *prefs_name )
{
	ofaFileFormatPiece *self;

	self = g_object_new( OFA_TYPE_FILE_FORMAT_PIECE, NULL );

	self->priv->prefs_name = g_strdup( prefs_name );

	return( self );
}

/**
 * ofa_file_format_piece_attach_to:
 *
 * This attach the widgets to the designed parent.
 * This must be called only once, at initialization time.
 */
void
ofa_file_format_piece_attach_to( ofaFileFormatPiece *settings, GtkContainer *new_parent )
{
	ofaFileFormatPiecePrivate *priv;
	GtkWidget *window, *widget;

	g_return_if_fail( settings && OFA_IS_FILE_FORMAT_PIECE( settings ));
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

		g_object_weak_ref( G_OBJECT( new_parent ), ( GWeakNotify ) on_parent_finalized, settings );
	}
}

static void
on_parent_finalized( ofaFileFormatPiece *self, GObject *finalized_parent )
{
	static const gchar *thisfn = "ofa_file_format_piece_on_parent_finalized";

	g_debug( "%s: self=%p, finalized_parent=%p",
			thisfn, ( void * ) self, ( void * ) finalized_parent );

	g_object_unref( self );
}

/**
 * ofa_file_format_piece_display:
 *
 * This initializes the combo boxes. This must be done after having
 * attached the widgets to the containing parent.
 */
void
ofa_file_format_piece_display( ofaFileFormatPiece *settings )
{
	ofaFileFormatPiecePrivate *priv;

	g_return_if_fail( settings && OFA_IS_FILE_FORMAT_PIECE( settings ));

	priv = settings->priv;

	g_return_if_fail( priv->parent && GTK_IS_CONTAINER( priv->parent ));
	g_return_if_fail( priv->container && GTK_IS_CONTAINER( priv->container ));

	if( !priv->dispose_has_run ){

		priv->settings = ofa_file_format_new( priv->prefs_name );

		init_encoding( settings );
		init_date_format( settings );
		init_decimal_dot( settings );
		init_field_separator( settings );
		init_headers( settings );

		/* export format at the end so that it is able to rely on
		   precomputed widgets */
		init_file_format( settings );
	}
}

static void
init_file_format( ofaFileFormatPiece *self )
{
	ofaFileFormatPiecePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkCellRenderer *cell;
	gint i, idx;
	ofaFFmt fmt;
	const gchar *cstr;

	priv = self->priv;

	priv->format_combo =
			my_utils_container_get_child_by_name( priv->container, "p1-export-format" );

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

	fmt = ofa_file_format_get_ffmt( priv->settings );
	idx = -1;

	for( i=1 ; TRUE ; ++i ){
		cstr = ofa_file_format_get_ffmt_str( i );
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
			G_OBJECT( priv->format_combo ), "changed", G_CALLBACK( on_ffmt_changed ), self );

	/* default to export as csv */
	if( idx == -1 ){
		idx = 0;
	}
	gtk_combo_box_set_active( GTK_COMBO_BOX( priv->format_combo ), idx );
}

static void
on_ffmt_changed( GtkComboBox *box, ofaFileFormatPiece *self )
{
	ofaFileFormatPiecePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	priv = self->priv;

	if( !gtk_combo_box_get_active_iter( GTK_COMBO_BOX( priv->format_combo ), &iter )){
		g_return_if_reached();
	}
	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( priv->format_combo ));
	g_return_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ));
	gtk_tree_model_get( tmodel, &iter, EXP_COL_FORMAT, &priv->format, -1 );

	gtk_widget_set_sensitive( priv->fieldsep_label, priv->format == OFA_FFMT_CSV );
	gtk_widget_set_sensitive( priv->fieldsep_combo, priv->format == OFA_FFMT_CSV );

	g_signal_emit_by_name( self, "changed" );
}

static void
init_encoding( ofaFileFormatPiece *self )
{
	ofaFileFormatPiecePrivate *priv;
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
on_encoding_changed( GtkComboBox *box, ofaFileFormatPiece *self )
{
	g_signal_emit_by_name( self, "changed" );
}

static void
init_date_format( ofaFileFormatPiece *self )
{
	ofaFileFormatPiecePrivate *priv;
	GtkWidget *widget;

	priv = self->priv;

	priv->date_combo = my_date_combo_new();

	widget = my_utils_container_get_child_by_name( priv->container, "p5-parent-date" );
	g_return_if_fail( widget && GTK_IS_CONTAINER( widget ));

	my_date_combo_attach_to( priv->date_combo, GTK_CONTAINER( widget ));
	my_date_combo_init_view( priv->date_combo, ofa_file_format_get_date_format( priv->settings ));

	g_signal_connect( priv->date_combo, "changed", G_CALLBACK( on_date_changed ), self );
}

static void
on_date_changed( myDateCombo *combo, myDateFormat format, ofaFileFormatPiece *self )
{
	g_signal_emit_by_name( self, "changed" );
}

static void
init_decimal_dot( ofaFileFormatPiece *self )
{
	ofaFileFormatPiecePrivate *priv;
	GtkWidget *parent;
	gchar *sep;

	priv = self->priv;
	priv->decimal_combo = my_decimal_combo_new();

	parent = my_utils_container_get_child_by_name( priv->container, "p5-decimal-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	my_decimal_combo_attach_to( priv->decimal_combo, GTK_CONTAINER( parent ));
	sep = g_strdup_printf( "%c", ofa_file_format_get_decimal_sep( priv->settings ));
	/*g_debug( "init_decimal_dot: sep='%s'", sep );*/
	my_decimal_combo_init_view( priv->decimal_combo, sep );
	g_free( sep );

	g_signal_connect( priv->decimal_combo, "changed", G_CALLBACK( on_decimal_changed ), self );
}

static void
on_decimal_changed( myDecimalCombo *combo, const gchar *decimal_sep, ofaFileFormatPiece *self )
{
	g_signal_emit_by_name( self, "changed" );
}

static void
init_field_separator( ofaFileFormatPiece *self )
{
	ofaFileFormatPiecePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkCellRenderer *cell;
	gint i, idx;
	gchar value;

	priv = self->priv;

	priv->fieldsep_label =
			my_utils_container_get_child_by_name( priv->container, "p5-field-label" );

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

	value = ofa_file_format_get_field_sep( priv->settings );
	/*g_debug( "init_field_sep: sep='%c'", value );*/
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

	g_signal_connect(
			G_OBJECT( priv->fieldsep_combo ), "changed", G_CALLBACK( on_field_changed ), self );

	if( idx != -1 ){
		gtk_combo_box_set_active( GTK_COMBO_BOX( priv->fieldsep_combo ), idx );
	}
}

static void
on_field_changed( GtkComboBox *box, ofaFileFormatPiece *self )
{
	g_signal_emit_by_name( self, "changed" );
}

static void
init_headers( ofaFileFormatPiece *self )
{
	ofaFileFormatPiecePrivate *priv;
	gboolean bvalue;

	priv = self->priv;

	priv->headers_btn =
			my_utils_container_get_child_by_name( priv->container, "p5-headers" );

	bvalue = ofa_file_format_get_headers( priv->settings );

	g_signal_connect(
			G_OBJECT( priv->headers_btn ), "toggled", G_CALLBACK( on_headers_toggled ), self );

	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->headers_btn ), bvalue );
}

static void
on_headers_toggled( GtkToggleButton *button, ofaFileFormatPiece *self )
{
	g_signal_emit_by_name( self, "changed" );
}

/**
 * ofa_file_format_piece_is_validable:
 *
 * Returns: %TRUE if selection is ok
 */
gboolean
ofa_file_format_piece_is_validable( ofaFileFormatPiece *settings )
{
	ofaFileFormatPiecePrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_FILE_FORMAT_PIECE( settings ), FALSE );

	priv = settings->priv;

	if( !priv->dispose_has_run ){

		return( is_validable( settings ));
	}

	g_return_val_if_reached( FALSE );
	return( FALSE );
}

static gboolean
is_validable( ofaFileFormatPiece *self )
{
	ofaFileFormatPiecePrivate *priv;
	gchar *charmap, *decimal_sep, *field_sep;
	gint iexport, ivalue;

	priv = self->priv;

	/* export format */
	iexport = get_file_format( self );
	if( iexport != OFA_FFMT_CSV && iexport != OFA_FFMT_FIXED ){
		return( FALSE );
	}

	/* charmap */
	charmap = get_charmap( self );
	if( !charmap || !g_utf8_strlen( charmap, -1 )){
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
	if( !decimal_sep || !g_utf8_strlen( decimal_sep, -1 )){
		g_free( decimal_sep );
		return( FALSE );
	}
	g_free( decimal_sep );

	/* field separator */
	field_sep = get_field_sep( self );
	if( !field_sep || !g_utf8_strlen( field_sep, -1 )){
		g_free( field_sep );
		return( FALSE );
	}
	g_free( field_sep );

	return( TRUE );
}

static gint
get_file_format( ofaFileFormatPiece *self )
{
	ofaFileFormatPiecePrivate *priv;
	ofaFFmt format;
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
get_charmap( ofaFileFormatPiece *self )
{
	ofaFileFormatPiecePrivate *priv;
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

static gchar *
get_field_sep( ofaFileFormatPiece *self )
{
	ofaFileFormatPiecePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *field_sep;

	priv = self->priv;
	field_sep = NULL;

	/* field separator */
	if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX( priv->fieldsep_combo ), &iter )){
		tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( priv->fieldsep_combo ));
		g_return_val_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ), FALSE );
		gtk_tree_model_get( tmodel, &iter, SEP_COL_CODE, &field_sep, -1 );
	}

	return( field_sep );
}

/**
 * ofa_file_format_piece_apply:
 *
 * This take the current selection out of the dialog box, setting the
 * user preferences.
 *
 * Returns: %TRUE if selection is ok
 */
gboolean
ofa_file_format_piece_apply( ofaFileFormatPiece *settings )
{
	ofaFileFormatPiecePrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_FILE_FORMAT_PIECE( settings ), FALSE );

	priv = settings->priv;

	g_return_val_if_fail( priv->parent && GTK_IS_CONTAINER( priv->parent ), FALSE );
	g_return_val_if_fail( priv->container && GTK_IS_CONTAINER( priv->container ), FALSE );

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
do_apply( ofaFileFormatPiece *self )
{
	ofaFileFormatPiecePrivate *priv;
	gchar *charmap, *decimal_sep, *field_sep;
	gint iexport, ivalue;
	gboolean bvalue;

	priv = self->priv;

	iexport = get_file_format( self );
	charmap = get_charmap( self );
	ivalue = my_date_combo_get_selected( priv->date_combo );
	decimal_sep = my_decimal_combo_get_selected( priv->decimal_combo );
	field_sep = get_field_sep( self );
	bvalue = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->headers_btn ));

	ofa_file_format_set( priv->settings,
								NULL,
								iexport, charmap, ivalue, decimal_sep[0], field_sep[0], bvalue );

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
	static const gchar *thisfn = "ofa_file_format_piece_get_available_charmaps";
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

/**
 * ofa_file_format_piece_get_file_format:
 *
 * Returns: the current #ofaFileFormat object.
 *
 * The returned object is owned by this #ofaFileFormatPiece object,
 * and should not be released by the caller.
 */
ofaFileFormat *
ofa_file_format_piece_get_file_format( ofaFileFormatPiece *settings )
{
	ofaFileFormatPiecePrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_FILE_FORMAT_PIECE( settings ), FALSE );

	priv = settings->priv;

	if( !priv->dispose_has_run ){

		return( priv->settings );
	}

	g_return_val_if_reached( FALSE );
	return( FALSE );
}
