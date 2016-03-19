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

#include "my/my-utils.h"

#include "api/ofa-periodicity.h"
#include "api/ofa-periodicity-bin.h"

/* private instance data
 */
struct _ofaPeriodicityBinPrivate {
	gboolean      dispose_has_run;

	/* UI
	 */
	GtkListStore *periodicity_store;
	GtkWidget    *periodicity_combo;
	GtkListStore *detail_store;
	GtkWidget    *detail_combo;

	/* data
	 */
	gchar        *periodicity_code;
	gchar        *detail_code;
};

/* columns in the Periodicity store
 */
enum {
	PER_COL_CODE = 0,
	PER_COL_LABEL,
	PER_COL_N_COLUMNS
};

/* columns in the Detail store
 */
enum {
	DET_COL_CODE = 0,
	DET_COL_LABEL,
	DET_COL_N_COLUMNS
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void       setup_bin( ofaPeriodicityBin *self );
static GtkWidget *periodicity_create_combo( ofaPeriodicityBin *self );
static void       periodicity_on_enum( const gchar *code, const gchar *label, ofaPeriodicityBin *self );
static void       periodicity_on_selection_changed( GtkComboBox *combo, ofaPeriodicityBin *self );
static GtkWidget *detail_create_combo( ofaPeriodicityBin *self );
static void       detail_populate( ofaPeriodicityBin *self );
static void       detail_on_enum( const gchar *code, const gchar *label, ofaPeriodicityBin *self );
static void       detail_on_selection_changed( GtkComboBox *combo, ofaPeriodicityBin *self );

G_DEFINE_TYPE_EXTENDED( ofaPeriodicityBin, ofa_periodicity_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaPeriodicityBin ))

static void
periodicity_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_periodicity_bin_finalize";
	ofaPeriodicityBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PERIODICITY_BIN( instance ));

	/* free data members here */
	priv = ofa_periodicity_bin_get_instance_private( OFA_PERIODICITY_BIN( instance ));

	g_free( priv->periodicity_code );
	g_free( priv->detail_code );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_periodicity_bin_parent_class )->finalize( instance );
}

static void
periodicity_bin_dispose( GObject *instance )
{
	ofaPeriodicityBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_PERIODICITY_BIN( instance ));

	priv = ofa_periodicity_bin_get_instance_private( OFA_PERIODICITY_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_periodicity_bin_parent_class )->dispose( instance );
}

static void
ofa_periodicity_bin_init( ofaPeriodicityBin *self )
{
	static const gchar *thisfn = "ofa_periodicity_bin_init";
	ofaPeriodicityBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PERIODICITY_BIN( self ));

	priv = ofa_periodicity_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_periodicity_bin_class_init( ofaPeriodicityBinClass *klass )
{
	static const gchar *thisfn = "ofa_periodicity_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = periodicity_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = periodicity_bin_finalize;

	/**
	 * ofaPeriodicityBin::ofa-changed:
	 *
	 * This signal is sent on the #ofaPeriodicityBin when the selection
	 * from the GtkcomboBox is changed.
	 *
	 * Argument is the selected periodicity code.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaPeriodicityBin *bin,
	 * 						const gchar     *periodicity,
	 * 						const gchar     *detail,
	 * 						gpointer         user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_PERIODICITY_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_STRING, G_TYPE_STRING );
}

/**
 * ofa_periodicity_bin_new:
 */
ofaPeriodicityBin *
ofa_periodicity_bin_new( void )
{
	ofaPeriodicityBin *self;

	self = g_object_new( OFA_TYPE_PERIODICITY_BIN, NULL );

	setup_bin( self );

	return( self );
}

static void
setup_bin( ofaPeriodicityBin *self )
{
	GtkWidget *grid, *combo;

	grid = gtk_grid_new();
	gtk_grid_set_column_spacing( GTK_GRID( grid ), 4 );
	gtk_container_add( GTK_CONTAINER( self ), grid );

	combo = periodicity_create_combo( self );
	gtk_grid_attach( GTK_GRID( grid ), combo, 0, 0, 1, 1 );

	combo = detail_create_combo( self );
	gtk_grid_attach( GTK_GRID( grid ), combo, 1, 0, 1, 1 );
}

static GtkWidget *
periodicity_create_combo( ofaPeriodicityBin *self )
{
	ofaPeriodicityBinPrivate *priv;
	GtkWidget *combo;
	GtkCellRenderer *cell;

	priv = ofa_periodicity_bin_get_instance_private( self );

	combo = gtk_combo_box_new();
	priv->periodicity_combo = combo;

	priv->periodicity_store = gtk_list_store_new( PER_COL_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING );
	gtk_combo_box_set_model( GTK_COMBO_BOX( combo ), GTK_TREE_MODEL( priv->periodicity_store ));
	g_object_unref( priv->periodicity_store );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", PER_COL_LABEL );

	gtk_combo_box_set_id_column ( GTK_COMBO_BOX( combo ), PER_COL_CODE );

	ofa_periodicity_enum(( PeriodicityEnumCb ) periodicity_on_enum, self );

	g_signal_connect( combo, "changed", G_CALLBACK( periodicity_on_selection_changed ), self );

	return( combo );
}

static void
periodicity_on_enum( const gchar *code, const gchar *label, ofaPeriodicityBin *self )
{
	ofaPeriodicityBinPrivate *priv;
	GtkTreeIter iter;

	priv = ofa_periodicity_bin_get_instance_private( self );

	gtk_list_store_insert_with_values( priv->periodicity_store, &iter, -1,
			PER_COL_CODE,  code,
			PER_COL_LABEL, label,
			-1 );
}

static void
periodicity_on_selection_changed( GtkComboBox *combo, ofaPeriodicityBin *self )
{
	ofaPeriodicityBinPrivate *priv;

	priv = ofa_periodicity_bin_get_instance_private( self );

	g_free( priv->periodicity_code );
	priv->periodicity_code = g_strdup( gtk_combo_box_get_active_id( combo ));

	g_free( priv->detail_code );
	priv->detail_code = NULL;

	detail_populate( self );

	g_signal_emit_by_name( G_OBJECT( self ), "ofa-changed", priv->periodicity_code, priv->detail_code );
}

static GtkWidget *
detail_create_combo( ofaPeriodicityBin *self )
{
	ofaPeriodicityBinPrivate *priv;
	GtkWidget *combo;
	GtkCellRenderer *cell;

	priv = ofa_periodicity_bin_get_instance_private( self );

	combo = gtk_combo_box_new();
	priv->detail_combo = combo;

	priv->detail_store = gtk_list_store_new( DET_COL_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING );
	gtk_combo_box_set_model( GTK_COMBO_BOX( combo ), GTK_TREE_MODEL( priv->detail_store ));
	g_object_unref( priv->detail_store );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", DET_COL_LABEL );

	gtk_combo_box_set_id_column ( GTK_COMBO_BOX( combo ), DET_COL_CODE );

	g_signal_connect( combo, "changed", G_CALLBACK( detail_on_selection_changed ), self );

	return( combo );
}

static void
detail_populate( ofaPeriodicityBin *self )
{
	ofaPeriodicityBinPrivate *priv;

	priv = ofa_periodicity_bin_get_instance_private( self );

	gtk_list_store_clear( priv->detail_store );

	ofa_periodicity_enum_detail( priv->periodicity_code, ( PeriodicityEnumCb ) detail_on_enum, self );
}

static void
detail_on_enum( const gchar *code, const gchar *label, ofaPeriodicityBin *self )
{
	ofaPeriodicityBinPrivate *priv;
	GtkTreeIter iter;

	priv = ofa_periodicity_bin_get_instance_private( self );

	gtk_list_store_insert_with_values( priv->detail_store, &iter, -1,
			DET_COL_CODE,  code,
			DET_COL_LABEL, label,
			-1 );
}

static void
detail_on_selection_changed( GtkComboBox *combo, ofaPeriodicityBin *self )
{
	ofaPeriodicityBinPrivate *priv;

	priv = ofa_periodicity_bin_get_instance_private( self );

	g_free( priv->detail_code );
	priv->detail_code = g_strdup( gtk_combo_box_get_active_id( combo ));

	g_signal_emit_by_name( G_OBJECT( self ), "ofa-changed", priv->periodicity_code, priv->detail_code );
}

/**
 * ofa_periodicity_bin_get_selected:
 * @bin:
 * @periodicity: [out]:
 * @detail: [out]:
 * @valid: [out][alllow-none]:
 * @msgerr: [out][allow-none]:
 *
 * Returns: the currently selected periodicity and detail as two newly
 * allocated strings which should be g_free() by the caller.
 */
void
ofa_periodicity_bin_get_selected( ofaPeriodicityBin *bin, gchar **periodicity, gchar **detail, gboolean *valid, gchar **msgerr )
{
	ofaPeriodicityBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_PERIODICITY_BIN( bin ));
	g_return_if_fail( periodicity && detail );

	priv = ofa_periodicity_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	*periodicity = g_strdup( priv->periodicity_code );
	*detail = g_strdup( priv->detail_code );

	if( valid ){
		*valid = TRUE;
	}
	if( msgerr ){
		*msgerr = NULL;
	}
	if( !my_strlen( priv->periodicity_code )){
		if( valid ){
			*valid = FALSE;
		}
		if( msgerr ){
			*msgerr = g_strdup( _( "Empty periodicity" ));
		}
	}
	if( !my_strlen( priv->detail_code )){
		if( valid ){
			*valid = FALSE;
		}
		if( msgerr ){
			*msgerr = g_strdup( _( "Empty periodicity detail" ));
		}
	}
}

/**
 * ofa_periodicity_bin_set_selected:
 * @bin:
 */
void
ofa_periodicity_bin_set_selected( ofaPeriodicityBin *bin, const gchar *periodicity, const gchar *detail )
{
	ofaPeriodicityBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_PERIODICITY_BIN( bin ));

	priv = ofa_periodicity_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	if( my_strlen( periodicity )){
		gtk_combo_box_set_active_id( GTK_COMBO_BOX( priv->periodicity_combo ), periodicity );
	}
	if( my_strlen( detail )){
		gtk_combo_box_set_active_id( GTK_COMBO_BOX( priv->detail_combo ), detail );
	}
}

/**
 * ofa_periodicity_bin_get_periodicity_combo:
 * @bin: this #ofaPeriodicityBin widget.
 *
 * Returns: the first combobox.
 */
GtkWidget *
ofa_periodicity_bin_get_periodicity_combo( ofaPeriodicityBin *bin )
{
	ofaPeriodicityBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_PERIODICITY_BIN( bin ), NULL );

	priv = ofa_periodicity_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->periodicity_combo );
}
