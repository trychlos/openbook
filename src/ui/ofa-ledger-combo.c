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

#include "api/my-utils.h"
#include "api/ofo-ledger.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-ledger-istore.h"
#include "ui/ofa-ledger-combo.h"

/* private instance data
 */
struct _ofaLedgerComboPrivate {
	gboolean     dispose_has_run;

	/* runtime data
	 */
	GtkComboBox *combo;
	gint         mnemo_col_number;
};

static void     istore_iface_init( ofaLedgerIStoreInterface *iface );
static guint    istore_get_interface_version( const ofaLedgerIStore *instance );
static void     istore_attach_to( ofaLedgerIStore *instance, GtkContainer *parent );
static void     istore_set_columns( ofaLedgerIStore *instance, GtkListStore *store, ofaLedgerColumns columns );
static void     on_ledger_changed( GtkComboBox *box, ofaLedgerCombo *self );

G_DEFINE_TYPE_EXTENDED( ofaLedgerCombo, ofa_ledger_combo, G_TYPE_OBJECT, 0, \
		G_IMPLEMENT_INTERFACE( OFA_TYPE_LEDGER_ISTORE, istore_iface_init ));

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

	priv = ( OFA_LEDGER_COMBO( instance ))->priv;

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

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_LEDGER_COMBO( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_LEDGER_COMBO, ofaLedgerComboPrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
ofa_ledger_combo_class_init( ofaLedgerComboClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_combo_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledger_combo_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledger_combo_finalize;

	g_type_class_add_private( klass, sizeof( ofaLedgerComboPrivate ));
}

static void
istore_iface_init( ofaLedgerIStoreInterface *iface )
{
	static const gchar *thisfn = "ofa_ledger_combo_istore_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = istore_get_interface_version;
	iface->attach_to = istore_attach_to;
	iface->set_columns = istore_set_columns;
}

static guint
istore_get_interface_version( const ofaLedgerIStore *instance )
{
	return( 1 );
}

static void
istore_attach_to( ofaLedgerIStore *instance, GtkContainer *parent )
{
	ofaLedgerComboPrivate *priv;

	g_return_if_fail( instance && OFA_IS_LEDGER_COMBO( instance ));

	g_debug( "istore_attach_to: instance=%p (%s), parent=%p (%s)",
			( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			( void * ) parent, G_OBJECT_TYPE_NAME( parent ));

	priv = OFA_LEDGER_COMBO( instance )->priv;

	if( !priv->combo ){
		priv->combo = GTK_COMBO_BOX( gtk_combo_box_new());
	}

	gtk_container_add( parent, GTK_WIDGET( priv->combo ));
}

static void
istore_set_columns( ofaLedgerIStore *instance, GtkListStore *store, ofaLedgerColumns columns )
{
	ofaLedgerComboPrivate *priv;
	GtkCellRenderer *cell;
	gint col_number;

	priv = OFA_LEDGER_COMBO( instance )->priv;

	if( !priv->combo ){
		priv->combo = GTK_COMBO_BOX( gtk_combo_box_new());
	}

	g_signal_connect(
			G_OBJECT( priv->combo ), "changed", G_CALLBACK( on_ledger_changed ), instance );

	gtk_combo_box_set_model( priv->combo, GTK_TREE_MODEL( store ));

	col_number = ofa_ledger_istore_get_column_number( instance, LEDGER_COL_MNEMO );
	priv->mnemo_col_number = col_number;

	if( columns & LEDGER_COL_MNEMO ){
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", col_number );
	}

	if( columns & LEDGER_COL_LABEL ){
		col_number = ofa_ledger_istore_get_column_number( instance, LEDGER_COL_LABEL );
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", col_number );
	}

	if( columns & LEDGER_COL_LAST_ENTRY ){
		col_number = ofa_ledger_istore_get_column_number( instance, LEDGER_COL_LAST_ENTRY );
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", col_number );
	}

	if( columns & LEDGER_COL_LAST_CLOSE ){
		col_number = ofa_ledger_istore_get_column_number( instance, LEDGER_COL_LAST_CLOSE );
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", col_number );
	}

	gtk_combo_box_set_id_column ( priv->combo, priv->mnemo_col_number );
	gtk_widget_show_all( GTK_WIDGET( priv->combo ));
}

/**
 * ofa_ledger_combo_new:
 */
ofaLedgerCombo *
ofa_ledger_combo_new( void )
{
	ofaLedgerCombo *self;

	self = g_object_new( OFA_TYPE_LEDGER_COMBO, NULL );

	return( self );
}

static void
on_ledger_changed( GtkComboBox *box, ofaLedgerCombo *self )
{
	ofaLedgerComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *mnemo;

	priv = self->priv;

	if( gtk_combo_box_get_active_iter( box, &iter )){
		tmodel = gtk_combo_box_get_model( box );
		gtk_tree_model_get( tmodel, &iter, priv->mnemo_col_number, &mnemo, -1 );
		g_signal_emit_by_name( G_OBJECT( self ), "changed", mnemo );
		g_free( mnemo );
	}
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
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *mnemo;

	g_return_val_if_fail( combo && OFA_IS_LEDGER_COMBO( combo ), NULL );

	priv = combo->priv;
	mnemo = NULL;

	if( !priv->dispose_has_run ){

		if( gtk_combo_box_get_active_iter( priv->combo, &iter )){
			tmodel = gtk_combo_box_get_model( priv->combo );
			gtk_tree_model_get( tmodel, &iter, priv->mnemo_col_number, &mnemo, -1 );
		}
	}

	return( mnemo );
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

	priv = combo->priv;

	if( !priv->dispose_has_run ){

		gtk_combo_box_set_active_id( priv->combo, mnemo );
	}
}
