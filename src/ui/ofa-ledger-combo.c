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

#include "ui/ofa-ledger-combo.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaLedgerComboPrivate {
	gboolean             dispose_has_run;

	/* runtime data
	 */
	GtkComboBox         *combo;
	ofaLedgerColumns     columns;

	const ofaMainWindow *main_window;
	ofoDossier          *dossier;
	ofaLedgerStore      *store;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void     create_combo_box( ofaLedgerCombo *combo );
static void     on_ledger_changed( GtkComboBox *box, ofaLedgerCombo *self );
static void     on_widget_finalized( ofaLedgerCombo *combo, gpointer finalized_parent );
static void     create_combo_columns( ofaLedgerCombo *combo );

G_DEFINE_TYPE( ofaLedgerCombo, ofa_ledger_combo, G_TYPE_OBJECT );

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

	/**
	 * ofaLedgerCombo::changed:
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
				"changed",
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
 */
ofaLedgerCombo *
ofa_ledger_combo_new( void )
{
	ofaLedgerCombo *self;

	self = g_object_new( OFA_TYPE_LEDGER_COMBO, NULL );

	create_combo_box( self );

	return( self );
}

static void
create_combo_box( ofaLedgerCombo *combo )
{
	ofaLedgerComboPrivate *priv;

	priv = combo->priv;

	priv->combo = GTK_COMBO_BOX( gtk_combo_box_new());

	g_signal_connect(
			G_OBJECT( priv->combo ), "changed", G_CALLBACK( on_ledger_changed ), combo );
}

static void
on_ledger_changed( GtkComboBox *box, ofaLedgerCombo *self )
{
	ofaLedgerComboPrivate *priv;
	const gchar *mnemo;

	priv = self->priv;

	mnemo = gtk_combo_box_get_active_id( priv->combo );
	if( mnemo ){
		g_signal_emit_by_name( self, "changed", mnemo );
	}
}

/**
 * ofa_ledger_combo_attach_to:
 */
void
ofa_ledger_combo_attach_to( ofaLedgerCombo *combo, GtkContainer *parent )
{
	ofaLedgerComboPrivate *priv;

	g_return_if_fail( combo && OFA_IS_LEDGER_COMBO( combo ));
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	g_debug( "ofa_ledger_combo_attach_to: combo=%p, parent=%p (%s)",
			( void * ) combo,
			( void * ) parent, G_OBJECT_TYPE_NAME( parent ));

	priv = combo->priv;

	if( !priv->dispose_has_run ){

		gtk_container_add( parent, GTK_WIDGET( priv->combo ));
		g_object_weak_ref( G_OBJECT( priv->combo ), ( GWeakNotify ) on_widget_finalized, combo );

		gtk_widget_show_all( GTK_WIDGET( parent ));
	}
}

static void
on_widget_finalized( ofaLedgerCombo *combo, gpointer finalized_widget )
{
	static const gchar *thisfn = "ofa_ledger_combo_on_widget_finalized";

	g_debug( "%s: combo=%p, finalized_widget=%p (%s)",
			thisfn, ( void * ) combo, ( void * ) finalized_widget, G_OBJECT_TYPE_NAME( finalized_widget ));

	g_return_if_fail( combo && OFA_IS_LEDGER_COMBO( combo ));

	g_object_unref( combo );
}

/**
 * ofa_ledger_combo_set_columns:
 */
void
ofa_ledger_combo_set_columns( ofaLedgerCombo *combo, ofaLedgerColumns columns )
{
	ofaLedgerComboPrivate *priv;

	g_return_if_fail( combo && OFA_IS_LEDGER_COMBO( combo ));

	priv = combo->priv;

	if( !priv->dispose_has_run ){

		/* the combobox must have been created first */
		g_return_if_fail( priv->combo && GTK_IS_COMBO_BOX( priv->combo ));

		priv->columns = columns;
		create_combo_columns( combo );
	}
}

static void
create_combo_columns( ofaLedgerCombo *combo )
{
	ofaLedgerComboPrivate *priv;
	GtkCellRenderer *cell;

	priv = combo->priv;

	if( priv->columns & LEDGER_DISP_MNEMO ){
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", LEDGER_COL_MNEMO );
	}

	if( priv->columns & LEDGER_DISP_LABEL ){
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", LEDGER_COL_LABEL );
	}

	if( priv->columns & LEDGER_DISP_LAST_ENTRY ){
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", LEDGER_COL_LAST_ENTRY );
	}

	if( priv->columns & LEDGER_DISP_LAST_CLOSE ){
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", LEDGER_COL_LAST_CLOSE );
	}

	gtk_combo_box_set_id_column ( priv->combo, LEDGER_COL_MNEMO );
}

/**
 * ofa_ledger_combo_set_main_window:
 *
 * This is required in order to get the dossier which will permit to
 * create the underlying tree store.
 */
void
ofa_ledger_combo_set_main_window( ofaLedgerCombo *combo, const ofaMainWindow *main_window )
{
	ofaLedgerComboPrivate *priv;

	g_return_if_fail( combo && OFA_IS_LEDGER_COMBO( combo ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv = combo->priv;

	if( !priv->dispose_has_run ){

		/* the combobox must have been created first */
		g_return_if_fail( priv->combo && GTK_IS_COMBO_BOX( priv->combo ));

		priv->main_window = main_window;
		priv->dossier = ofa_main_window_get_dossier( main_window );
		priv->store = ofa_ledger_store_new( priv->dossier );

		gtk_combo_box_set_model( priv->combo, GTK_TREE_MODEL( priv->store ));

		/*dossier_signals_connect( combo );*/
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
	const gchar *mnemo;

	g_return_val_if_fail( combo && OFA_IS_LEDGER_COMBO( combo ), NULL );

	priv = combo->priv;
	mnemo = NULL;

	if( !priv->dispose_has_run ){

		mnemo = gtk_combo_box_get_active_id( priv->combo );
	}

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

	priv = combo->priv;

	if( !priv->dispose_has_run ){

		gtk_combo_box_set_active_id( priv->combo, mnemo );
	}
}
