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
#include "api/ofo-base.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"

#include "core/ofa-currency-combo.h"
#include "core/ofa-currency-store.h"

/* private instance data
 */
typedef struct {
	gboolean          dispose_has_run;

	/* runtime
	 */
	ofaCurrencyStore *store;

	/* sorted combo
	 */
	GtkTreeModel     *sort_model;
	gint              sort_column_id;
}
	ofaCurrencyComboPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void create_combo_columns( ofaCurrencyCombo *self, const gint *columns );
static gint on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaCurrencyCombo *self );
static void on_currency_changed( ofaCurrencyCombo *self, void *empty );

G_DEFINE_TYPE_EXTENDED( ofaCurrencyCombo, ofa_currency_combo, GTK_TYPE_COMBO_BOX, 0,
		G_ADD_PRIVATE( ofaCurrencyCombo ))

static void
currency_combo_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_currency_combo_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CURRENCY_COMBO( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currency_combo_parent_class )->finalize( instance );
}

static void
currency_combo_dispose( GObject *instance )
{
	ofaCurrencyComboPrivate *priv;

	g_return_if_fail( instance && OFA_IS_CURRENCY_COMBO( instance ));

	priv = ofa_currency_combo_get_instance_private( OFA_CURRENCY_COMBO( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currency_combo_parent_class )->dispose( instance );
}

static void
ofa_currency_combo_init( ofaCurrencyCombo *self )
{
	static const gchar *thisfn = "ofa_currency_combo_init";
	ofaCurrencyComboPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_CURRENCY_COMBO( self ));

	priv = ofa_currency_combo_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_currency_combo_class_init( ofaCurrencyComboClass *klass )
{
	static const gchar *thisfn = "ofa_currency_combo_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = currency_combo_dispose;
	G_OBJECT_CLASS( klass )->finalize = currency_combo_finalize;

	/**
	 * ofaCurrencyCombo::ofa-changed:
	 *
	 * This signal is sent on the #ofaCurrencyCombo when the selection
	 * from the GtkcomboBox is changed.
	 *
	 * Argument is the selected currency ISO 3A code.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaCurrencyCombo *combo,
	 * 						const gchar    *code,
	 * 						gpointer        user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_CURRENCY_COMBO,
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
 * ofa_currency_combo_new:
 */
ofaCurrencyCombo *
ofa_currency_combo_new( void )
{
	ofaCurrencyCombo *self;

	self = g_object_new( OFA_TYPE_CURRENCY_COMBO, NULL );

	g_signal_connect( self, "changed", G_CALLBACK( on_currency_changed ), NULL );

	return( self );
}

/**
 * ofa_currency_combo_set_columns:
 * @combo: this #ofaCurrencyCombo instance.
 * @columns: a [-1] - terminated array of columns identifiers.
 *
 * Create the desired columns in the specified order.
 */
void
ofa_currency_combo_set_columns( ofaCurrencyCombo *combo, const gint *columns )
{
	ofaCurrencyComboPrivate *priv;

	g_return_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ));

	priv = ofa_currency_combo_get_instance_private( combo );

	g_return_if_fail( !priv->dispose_has_run );

	create_combo_columns( combo, columns );
}

static void
create_combo_columns( ofaCurrencyCombo *self, const gint *columns )
{
	ofaCurrencyComboPrivate *priv;
	gint i;
	GtkCellRenderer *cell;

	priv = ofa_currency_combo_get_instance_private( self );

	for( i=0 ; columns[i] >= 0 ; ++i ){

		if( columns[i] == CURRENCY_COL_CODE ){
			cell = gtk_cell_renderer_text_new();
			gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( self ), cell, FALSE );
			gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( self ), cell, "text", columns[i] );

		} else if( columns[i] == CURRENCY_COL_LABEL ){
			cell = gtk_cell_renderer_text_new();
			gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( self ), cell, TRUE );
			gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( self ), cell, "text", columns[i] );

		} else if( columns[i] == CURRENCY_COL_SYMBOL ){
			cell = gtk_cell_renderer_text_new();
			gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( self ), cell, FALSE );
			gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( self ), cell, "text", columns[i] );

		} else if( columns[i] == CURRENCY_COL_DIGITS ){
			cell = gtk_cell_renderer_text_new();
			gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( self ), cell, FALSE );
			gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( self ), cell, "text", columns[i] );
		}
	}

	gtk_combo_box_set_id_column ( GTK_COMBO_BOX( self ), CURRENCY_COL_CODE );
	priv->sort_column_id = columns[0];
}

/**
 * ofa_currency_combo_set_hub:
 *
 * This is required in order to get the dossier which will permit to
 * create the underlying tree store.
 */
void
ofa_currency_combo_set_hub( ofaCurrencyCombo *combo, ofaHub *hub )
{
	ofaCurrencyComboPrivate *priv;

	g_return_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_currency_combo_get_instance_private( combo );

	g_return_if_fail( !priv->dispose_has_run );

	priv->store = ofa_currency_store_new( hub );

	priv->sort_model = gtk_tree_model_sort_new_with_model( GTK_TREE_MODEL( priv->store ));
	/* the sortable model maintains its own reference on the store */
	g_object_unref( priv->store );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( priv->sort_model ), ( GtkTreeIterCompareFunc ) on_sort_model, combo, NULL );
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( priv->sort_model ), priv->sort_column_id, GTK_SORT_ASCENDING );

	gtk_combo_box_set_model( GTK_COMBO_BOX( combo ), GTK_TREE_MODEL( priv->sort_model ));
	/* the combo box maintains its own reference on the sort model */
	g_object_unref( priv->sort_model );
}

static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaCurrencyCombo *self )
{
	ofaCurrencyComboPrivate *priv;
	gint cmp;
	gchar *stra, *strb;

	priv = ofa_currency_combo_get_instance_private( self );

	gtk_tree_model_get( tmodel, a, priv->sort_column_id, &stra, -1 );
	gtk_tree_model_get( tmodel, b, priv->sort_column_id, &strb, -1 );

	cmp = my_collate( stra, strb );

	g_free( stra );
	g_free( strb );

	return( cmp );
}

static void
on_currency_changed( ofaCurrencyCombo *self, void *empty )
{
	const gchar *code;

	code = gtk_combo_box_get_active_id( GTK_COMBO_BOX( self ));
	if( code ){
		g_signal_emit_by_name( self, "ofa-changed", code );
	}
}

/**
 * ofa_currency_combo_get_selected:
 * @combo:
 *
 * Returns: the currently selected currency as a newly allocated string
 * which should be g_free() by the caller.
 */
gchar *
ofa_currency_combo_get_selected( ofaCurrencyCombo *combo )
{
	ofaCurrencyComboPrivate *priv;

	g_return_val_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ), NULL );

	priv = ofa_currency_combo_get_instance_private( combo );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( g_strdup( gtk_combo_box_get_active_id( GTK_COMBO_BOX( combo ))));
}

/**
 * ofa_currency_combo_set_selected:
 * @combo:
 */
void
ofa_currency_combo_set_selected( ofaCurrencyCombo *combo, const gchar *code )
{
	ofaCurrencyComboPrivate *priv;

	g_return_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ));
	g_return_if_fail( my_strlen( code ));

	priv = ofa_currency_combo_get_instance_private( combo );

	g_return_if_fail( !priv->dispose_has_run );

	gtk_combo_box_set_active_id( GTK_COMBO_BOX( combo ), code );
}
