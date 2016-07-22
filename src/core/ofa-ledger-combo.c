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

#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofo-ledger.h"

#include "core/ofa-ledger-combo.h"
#include "core/ofa-ledger-store.h"

/* private instance data
 */
typedef struct {
	gboolean        dispose_has_run;

	/* runtime data
	 */
	ofaLedgerStore *store;

	/* sorted combo
	 */
	GtkTreeModel   *sort_model;
	gint            sort_column_id;
}
	ofaLedgerComboPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void         on_ledger_changed( ofaLedgerCombo *combo, void *empty );
static void         create_combo_columns( ofaLedgerCombo *self, const gint *columns );
static gint         on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaLedgerCombo *self );

G_DEFINE_TYPE_EXTENDED( ofaLedgerCombo, ofa_ledger_combo, GTK_TYPE_COMBO_BOX, 0,
		G_ADD_PRIVATE( ofaLedgerCombo ))

static void
ledger_combo_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledger_combo_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGER_COMBO( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_combo_parent_class )->finalize( instance );
}

static void
ledger_combo_dispose( GObject *instance )
{
	ofaLedgerComboPrivate *priv;

	g_return_if_fail( instance && OFA_IS_LEDGER_COMBO( instance ));

	priv = ofa_ledger_combo_get_instance_private( OFA_LEDGER_COMBO( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_combo_parent_class )->dispose( instance );
}

static void
ofa_ledger_combo_init( ofaLedgerCombo *self )
{
	static const gchar *thisfn = "ofa_ledger_combo_init";
	ofaLedgerComboPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_LEDGER_COMBO( self ));

	priv = ofa_ledger_combo_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->sort_model = NULL;
	priv->sort_column_id = -1;
}

static void
ofa_ledger_combo_class_init( ofaLedgerComboClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_combo_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledger_combo_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledger_combo_finalize;

	/**
	 * ofaLedgerCombo::ofa-changed:
	 *
	 * This signal is sent on the #ofaLedgerCombo when the selection is
	 * changed.
	 *
	 * Arguments is the selected ledger mnemo.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaLedgerCombo *combo,
	 * 						const gchar  *mnemo,
	 * 						gpointer      user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_LEDGER_COMBO,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_STRING );
}

/**
 * ofa_ledger_combo_new:
 *
 * Returns: a new #ofaLedgerCombo combobox instance.
 */
ofaLedgerCombo *
ofa_ledger_combo_new( void )
{
	ofaLedgerCombo *self;

	self = g_object_new( OFA_TYPE_LEDGER_COMBO, NULL );

	g_signal_connect( self, "changed", G_CALLBACK( on_ledger_changed ), NULL );

	return( self );
}

static void
on_ledger_changed( ofaLedgerCombo *combo, void *empty )
{
	const gchar *mnemo;

	mnemo = gtk_combo_box_get_active_id( GTK_COMBO_BOX( combo ));
	if( mnemo ){
		g_signal_emit_by_name( GTK_COMBO_BOX( combo ), "ofa-changed", mnemo );
	}
}

/**
 * ofa_ledger_combo_set_columns:
 * @combo: this #ofaLedgerCombo instance.
 * @columns: a -1-terminated list of columns to be displayed.
 *
 * Setup the displayable columns.
 *
 * The @combo combobox will be sorted on the first displayed (the
 * leftmost) column.
 */
void
ofa_ledger_combo_set_columns( ofaLedgerCombo *combo, const gint *columns )
{
	ofaLedgerComboPrivate *priv;

	g_return_if_fail( combo && OFA_IS_LEDGER_COMBO( combo ));

	priv = ofa_ledger_combo_get_instance_private( combo );

	g_return_if_fail( !priv->dispose_has_run );

	create_combo_columns( combo, columns );
}

static void
create_combo_columns( ofaLedgerCombo *self, const gint *columns )
{
	ofaLedgerComboPrivate *priv;
	GtkCellRenderer *cell;
	gint i;

	priv = ofa_ledger_combo_get_instance_private( self );

	for( i=0 ; columns[i] >= 0 ; ++i ){

		if( columns[i] == LEDGER_COL_MNEMO ){
			cell = gtk_cell_renderer_text_new();
			gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( self ), cell, FALSE );
			gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( self ), cell, "text", columns[i] );
		}

		if( columns[i] == LEDGER_COL_LABEL ){
			cell = gtk_cell_renderer_text_new();
			gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( self ), cell, FALSE );
			gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( self ), cell, "text", columns[i] );
		}

		if( columns[i] == LEDGER_COL_LAST_ENTRY ){
			cell = gtk_cell_renderer_text_new();
			gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( self ), cell, FALSE );
			gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( self ), cell, "text", columns[i] );
		}

		if( columns[i] == LEDGER_COL_LAST_CLOSE ){
			cell = gtk_cell_renderer_text_new();
			gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( self ), cell, FALSE );
			gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( self ), cell, "text", columns[i] );
		}
	}

	gtk_combo_box_set_id_column ( GTK_COMBO_BOX( self ), LEDGER_COL_MNEMO );
	priv->sort_column_id = columns[0];
}

/**
 * ofa_ledger_combo_set_hub:
 * @combo: this #ofaLedgerCombo instance.
 * @hub: the #ofaHub of the application.
 *
 * Allocates and associates a #ofaLedgerStore to the @combo.
 *
 * This is required in order to create the underlying tree store.
 */
void
ofa_ledger_combo_set_hub( ofaLedgerCombo *combo, ofaHub *hub )
{
	ofaLedgerComboPrivate *priv;

	g_return_if_fail( combo && OFA_IS_LEDGER_COMBO( combo ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_ledger_combo_get_instance_private( combo );

	g_return_if_fail( !priv->dispose_has_run );

	priv->store = ofa_ledger_store_new( hub );

	priv->sort_model = gtk_tree_model_sort_new_with_model( GTK_TREE_MODEL( priv->store ));
	/* the sortable model maintains its own reference on the store */
	g_object_unref( priv->store );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( priv->sort_model ), ( GtkTreeIterCompareFunc ) on_sort_model, combo, NULL );
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( priv->sort_model ), priv->sort_column_id, GTK_SORT_ASCENDING );

	gtk_combo_box_set_model( GTK_COMBO_BOX( combo ), priv->sort_model );
	/* ofaLedgerCombo maintains its own reference on the sortable model */
	g_object_unref( priv->sort_model );
}

static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaLedgerCombo *self )
{
	ofaLedgerComboPrivate *priv;
	gint cmp;
	gchar *stra, *strb;

	priv = ofa_ledger_combo_get_instance_private( self );

	gtk_tree_model_get( tmodel, a, priv->sort_column_id, &stra, -1 );
	gtk_tree_model_get( tmodel, a, priv->sort_column_id, &strb, -1 );

	cmp = my_collate( stra, strb );

	g_free( stra );
	g_free( strb );

	return( cmp );
}

/**
 * ofa_ledger_combo_get_selected:
 * @combo: this #ofaLedgerCombo instance.
 *
 * Returns: the mnemonic of the currently selected ledger, as a newly
 * allocated string which should be g_free() by the caller.
 */
gchar *
ofa_ledger_combo_get_selected( ofaLedgerCombo *combo )
{
	ofaLedgerComboPrivate *priv;
	const gchar *mnemo;

	g_return_val_if_fail( combo && OFA_IS_LEDGER_COMBO( combo ), NULL );

	priv = ofa_ledger_combo_get_instance_private( combo );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	mnemo = gtk_combo_box_get_active_id( GTK_COMBO_BOX( combo ));

	return( g_strdup( mnemo ));
}

/**
 * ofa_ledger_combo_set_selected:
 * @combo: this #ofaLedgerCombo instance.
 * @mnemo: the mnemonic of the ledger to be selected.
 *
 * Set the current selection.
 */
void
ofa_ledger_combo_set_selected( ofaLedgerCombo *combo, const gchar *mnemo )
{
	ofaLedgerComboPrivate *priv;

	g_return_if_fail( combo && OFA_IS_LEDGER_COMBO( combo ));

	priv = ofa_ledger_combo_get_instance_private( combo );

	g_return_if_fail( !priv->dispose_has_run );

	gtk_combo_box_set_active_id( GTK_COMBO_BOX( combo ), mnemo );
}
