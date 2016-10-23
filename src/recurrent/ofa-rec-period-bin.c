/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include "my/my-utils.h"

#include "api/ofa-hub.h"

#include "recurrent/ofa-rec-period-bin.h"
#include "recurrent/ofa-rec-period-store.h"

/* private instance data
 */
typedef struct {
	gboolean      dispose_has_run;

	/* initialization
	 */
	ofaHub       *hub;

	/* UI
	 */
	GtkWidget    *period_combo;
	GtkWidget    *detail_combo;
	GtkListStore *detail_store;

	/* data
	 */
	ofoRecPeriod *period;
	ofxCounter    det_id;
}
	ofaRecPeriodBinPrivate;

/* columns in the Detail store
 */
enum {
	DET_COL_ID = 0,
	DET_COL_NUMBER,
	DET_COL_VALUE,
	DET_COL_ORDER,
	DET_COL_LABEL,
	DET_COL_N_COLUMNS
};

/* signals defined here
 */
enum {
	PERCHANGED = 0,
	DETCHANGED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void       setup_bin( ofaRecPeriodBin *self );
static GtkWidget *period_create_combo( ofaRecPeriodBin *self );
static gint       period_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, void *empty );
static gboolean   period_set_selected( ofaRecPeriodBin *self, const gchar *period_id );
static void       period_on_selection_changed( GtkComboBox *combo, ofaRecPeriodBin *self );
static GtkWidget *detail_create_combo( ofaRecPeriodBin *self );
static void       detail_populate( ofaRecPeriodBin *self );
static gint       detail_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, void *empty );
static void       detail_set_selected( ofaRecPeriodBin *self, ofxCounter detail_id );
static void       detail_on_selection_changed( GtkComboBox *combo, ofaRecPeriodBin *self );

G_DEFINE_TYPE_EXTENDED( ofaRecPeriodBin, ofa_rec_period_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaRecPeriodBin ))

static void
rec_period_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_rec_period_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_REC_PERIOD_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rec_period_bin_parent_class )->finalize( instance );
}

static void
rec_period_bin_dispose( GObject *instance )
{
	ofaRecPeriodBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_REC_PERIOD_BIN( instance ));

	priv = ofa_rec_period_bin_get_instance_private( OFA_REC_PERIOD_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rec_period_bin_parent_class )->dispose( instance );
}

static void
ofa_rec_period_bin_init( ofaRecPeriodBin *self )
{
	static const gchar *thisfn = "ofa_rec_period_bin_init";
	ofaRecPeriodBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_REC_PERIOD_BIN( self ));

	priv = ofa_rec_period_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_rec_period_bin_class_init( ofaRecPeriodBinClass *klass )
{
	static const gchar *thisfn = "ofa_rec_period_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = rec_period_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = rec_period_bin_finalize;

	/**
	 * ofaRecPeriodBin::ofa-perchanged:
	 *
	 * This signal is sent on the #ofaRecPeriodBin when the selected
	 * periodicitiy is changed.
	 *
	 * Argument is the selected #ofoRecPeriod object, or %NULL.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaRecPeriodBin *bin,
	 * 						ofoRecPeriod  *period,
	 * 						gpointer       user_data );
	 */
	st_signals[ PERCHANGED ] = g_signal_new_class_handler(
				"ofa-perchanged",
				OFA_TYPE_REC_PERIOD_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaRecPeriodBin::ofa-detchanged:
	 *
	 * This signal is sent on the #ofaRecPeriodBin when the selected
	 * periodicitiy detail is changed.
	 *
	 * Argument is the selected #ofoRecPeriod object and the selected
	 * detail. This later may be -1 if no detail is selected.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaRecPeriodBin *bin,
	 * 						ofoRecPeriod  *period,
	 * 						ofxCounter     detail_id,
	 * 						gpointer       user_data );
	 */
	st_signals[ DETCHANGED ] = g_signal_new_class_handler(
				"ofa-detchanged",
				OFA_TYPE_REC_PERIOD_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_OBJECT, G_TYPE_LONG );
}

/**
 * ofa_rec_period_bin_new:
 */
ofaRecPeriodBin *
ofa_rec_period_bin_new( ofaHub *hub )
{
	ofaRecPeriodBin *self;
	ofaRecPeriodBinPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	self = g_object_new( OFA_TYPE_REC_PERIOD_BIN, NULL );
	priv = ofa_rec_period_bin_get_instance_private( self );
	priv->hub = hub;

	setup_bin( self );

	return( self );
}

static void
setup_bin( ofaRecPeriodBin *self )
{
	GtkWidget *grid, *combo;

	grid = gtk_grid_new();
	gtk_grid_set_column_spacing( GTK_GRID( grid ), 4 );
	gtk_container_add( GTK_CONTAINER( self ), grid );

	combo = period_create_combo( self );
	gtk_grid_attach( GTK_GRID( grid ), combo, 0, 0, 1, 1 );

	combo = detail_create_combo( self );
	gtk_grid_attach( GTK_GRID( grid ), combo, 1, 0, 1, 1 );
}

static GtkWidget *
period_create_combo( ofaRecPeriodBin *self )
{
	ofaRecPeriodBinPrivate *priv;
	GtkWidget *combo;
	GtkCellRenderer *cell;
	ofaRecPeriodStore *store;
	GtkTreeModel *sort_model;

	priv = ofa_rec_period_bin_get_instance_private( self );

	combo = gtk_combo_box_new();
	priv->period_combo = combo;

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", PER_COL_LABEL );

	store = ofa_rec_period_store_new( priv->hub );
	sort_model = gtk_tree_model_sort_new_with_model( GTK_TREE_MODEL( store ));
	/* the sortable model maintains its own reference on the store */
	g_object_unref( store );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( sort_model ), ( GtkTreeIterCompareFunc ) period_on_sort_model, NULL, NULL );
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( sort_model ), GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	gtk_combo_box_set_model( GTK_COMBO_BOX( combo ), GTK_TREE_MODEL( sort_model ));
	/* the combo box maintains its own reference on the sort model */
	g_object_unref( sort_model );

	g_signal_connect( combo, "changed", G_CALLBACK( period_on_selection_changed ), self );

	return( combo );
}

static gint
period_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, void *empty )
{
	gint ordera, orderb, cmp;

	gtk_tree_model_get( tmodel, a, PER_COL_ORDER_I, &ordera, -1 );
	gtk_tree_model_get( tmodel, b, PER_COL_ORDER_I, &orderb, -1 );

	cmp = ordera < orderb ? -1 : ( ordera > orderb ? 1 : 0 );

	return( cmp );
}

static gboolean
period_set_selected( ofaRecPeriodBin *self, const gchar *period_id )
{
	ofaRecPeriodBinPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *row_id;
	gint cmp;

	priv = ofa_rec_period_bin_get_instance_private( self );

	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( priv->period_combo ));
	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, &iter, PER_COL_ID, &row_id, -1 );
			cmp = my_collate( row_id, period_id );
			g_free( row_id );
			if( cmp == 0 ){
				gtk_combo_box_set_active_iter( GTK_COMBO_BOX( priv->period_combo ), &iter );
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( tmodel, &iter )){
				break;
			}
		}
	}

	return( FALSE );
}

static void
period_on_selection_changed( GtkComboBox *combo, ofaRecPeriodBin *self )
{
	ofaRecPeriodBinPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	guint details_count;

	priv = ofa_rec_period_bin_get_instance_private( self );

	priv->det_id = -1;
	details_count = 0;

	if( gtk_combo_box_get_active_iter( combo, &iter )){
		tmodel = gtk_combo_box_get_model( combo );
		gtk_tree_model_get( tmodel, &iter, PER_COL_OBJECT, &priv->period, -1 );
		if( priv->period ){
			g_object_unref( priv->period );
			details_count = ofo_rec_period_get_details_count( priv->period );
		}
	}

	gtk_widget_set_sensitive( priv->detail_combo, details_count > 0 );
	if( details_count > 0 ){
		detail_populate( self );
	}

	g_signal_emit_by_name( G_OBJECT( self ), "ofa-perchanged", priv->period );
}

static GtkWidget *
detail_create_combo( ofaRecPeriodBin *self )
{
	ofaRecPeriodBinPrivate *priv;
	GtkWidget *combo;
	GtkListStore *store;
	GtkCellRenderer *cell;
	GtkTreeModel *sort_model;

	priv = ofa_rec_period_bin_get_instance_private( self );

	combo = gtk_combo_box_new();
	priv->detail_combo = combo;

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", DET_COL_LABEL );

	/* id, order, label */
	store = gtk_list_store_new( DET_COL_N_COLUMNS,
			G_TYPE_ULONG, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING );
	sort_model = gtk_tree_model_sort_new_with_model( GTK_TREE_MODEL( store ));
	/* the sortable model maintains its own reference on the store */
	g_object_unref( store );
	priv->detail_store = store;

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( sort_model ), ( GtkTreeIterCompareFunc ) detail_on_sort_model, NULL, NULL );
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( sort_model ), GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	gtk_combo_box_set_model( GTK_COMBO_BOX( combo ), GTK_TREE_MODEL( sort_model ));
	/* the combo box maintains its own reference on the sort model */
	g_object_unref( sort_model );

	g_signal_connect( combo, "changed", G_CALLBACK( detail_on_selection_changed ), self );

	return( combo );
}

static void
detail_populate( ofaRecPeriodBin *self )
{
	ofaRecPeriodBinPrivate *priv;
	guint count, i;
	GtkTreeIter iter;

	priv = ofa_rec_period_bin_get_instance_private( self );

	gtk_list_store_clear( priv->detail_store );

	count = ofo_rec_period_detail_get_count( priv->period );
	for( i=0 ; i<count ; ++i ){
		gtk_list_store_insert_with_values( priv->detail_store, &iter, -1,
				DET_COL_ID,     ofo_rec_period_detail_get_id( priv->period, i ),
				DET_COL_NUMBER, ofo_rec_period_detail_get_number( priv->period, i ),
				DET_COL_VALUE,  ofo_rec_period_detail_get_value( priv->period, i ),
				DET_COL_ORDER,  ofo_rec_period_detail_get_order( priv->period, i ),
				DET_COL_LABEL,  ofo_rec_period_detail_get_label( priv->period, i ),
				-1 );
	}
}

static gint
detail_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, void *empty )
{
	gint ordera, orderb, cmp;

	gtk_tree_model_get( tmodel, a, DET_COL_ORDER, &ordera, -1 );
	gtk_tree_model_get( tmodel, b, DET_COL_ORDER, &orderb, -1 );

	cmp = ordera < orderb ? -1 : ( ordera > orderb ? 1 : 0 );

	return( cmp );
}

static void
detail_set_selected( ofaRecPeriodBin *self, ofxCounter detail_id )
{
	ofaRecPeriodBinPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofxCounter row_id;

	priv = ofa_rec_period_bin_get_instance_private( self );

	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( priv->detail_combo ));
	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, &iter, DET_COL_ID, &row_id, -1 );
			if( row_id == detail_id ){
				gtk_combo_box_set_active_iter( GTK_COMBO_BOX( priv->detail_combo ), &iter );
				break;
			}
			if( !gtk_tree_model_iter_next( tmodel, &iter )){
				break;
			}
		}
	}
}

/* may or not have a selected detail
 * but we are sure we have a selected periodicity
 */
static void
detail_on_selection_changed( GtkComboBox *combo, ofaRecPeriodBin *self )
{
	ofaRecPeriodBinPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	priv = ofa_rec_period_bin_get_instance_private( self );

	priv->det_id = -1;

	if( gtk_combo_box_get_active_iter( combo, &iter )){
		tmodel = gtk_combo_box_get_model( combo );
		gtk_tree_model_get( tmodel, &iter, DET_COL_ID, &priv->det_id, -1 );
	}

	g_signal_emit_by_name( G_OBJECT( self ), "ofa-detchanged", priv->period, priv->det_id );
}

/**
 * ofa_rec_period_bin_get_selected:
 * @bin: this #ofoRecPeriodBin instance.
 * @period: [out]:
 * @detail: [out][allow-none]:
 *
 * Returns: the currently selected periodicity and detail.
 */
void
ofa_rec_period_bin_get_selected( ofaRecPeriodBin *bin, ofoRecPeriod **period, ofxCounter *detail_id )
{
	ofaRecPeriodBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_REC_PERIOD_BIN( bin ));
	g_return_if_fail( period );

	priv = ofa_rec_period_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	*period = priv->period;
	if( detail_id ){
		*detail_id = priv->det_id;
	}
}

/**
 * ofa_rec_period_bin_set_selected:
 * @bin: this #ofoRecPeriodBin instance.
 * @period_id: the #ofoRecPeriod identifier.
 * @detail_id: the detail identifier.
 */
void
ofa_rec_period_bin_set_selected( ofaRecPeriodBin *bin, const gchar *period_id, ofxCounter detail_id )
{
	ofaRecPeriodBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_REC_PERIOD_BIN( bin ));

	priv = ofa_rec_period_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	if( period_set_selected( bin, period_id )){
		detail_set_selected( bin, detail_id );
	}
}

/**
 * ofa_rec_period_bin_get_periodicity_combo:
 * @bin: this #ofaRecPeriodBin instance.
 *
 * Returns: the first combobox.
 */
GtkWidget *
ofa_rec_period_bin_get_periodicity_combo( ofaRecPeriodBin *bin )
{
	ofaRecPeriodBinPrivate *priv;

g_return_val_if_fail( bin && OFA_IS_REC_PERIOD_BIN( bin ), NULL );

	priv = ofa_rec_period_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->period_combo );
}
