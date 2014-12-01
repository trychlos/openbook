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

/* private instance data
 */
struct _ofaLedgerComboPrivate {
	gboolean           dispose_has_run;

	/* runtime data
	 */
	GtkComboBox       *combo;
	GtkWidget         *label;
	ofoDossier        *dossier;
};

/* column ordering in the ledger combobox
 */
enum {
	JOU_COL_MNEMO = 0,
	JOU_COL_LABEL,
	JOU_N_COLUMNS
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

G_DEFINE_TYPE( ofaLedgerCombo, ofa_ledger_combo, G_TYPE_OBJECT )

static void     on_parent_finalized( ofaLedgerCombo *self, gpointer finalized_parent );
static void     setup_combo( ofaLedgerCombo *self, gboolean display_mnemo, gboolean display_label );
static void     on_ledger_changed( GtkComboBox *box, ofaLedgerCombo *self );
static void     on_ledger_changed_cleanup_handler( ofaLedgerCombo *self, gchar *mnemo, gchar *label );
static void     load_dataset( ofaLedgerCombo *self, const gchar *mnemo );
static void     insert_row( ofaLedgerCombo *self, GtkTreeModel *tmodel, const ofoLedger *ledger );
static void     setup_signaling_connect( ofaLedgerCombo *self, ofoDossier *dossier );
static void     on_new_object( ofoDossier *dossier, ofoBase *object, ofaLedgerCombo *self );
static void     on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaLedgerCombo *self );
static gboolean find_ledger_by_mnemo( ofaLedgerCombo *self, const gchar *mnemo, GtkTreeModel **tmodel, GtkTreeIter *iter );
static void     on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaLedgerCombo *self );
static void     on_reload_dataset( ofoDossier *dossier, GType type, ofaLedgerCombo *self );

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
	 * This signal is sent when the selection is changed.
	 *
	 * Arguments is the selected ledger.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaLedgerCombo *combo,
	 * 						const gchar  *mnemo,
	 * 						const gchar  *label,
	 * 						gpointer      user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_LEDGER_COMBO,
				G_SIGNAL_RUN_CLEANUP,
				G_CALLBACK( on_ledger_changed_cleanup_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_POINTER, G_TYPE_POINTER );
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

/**
 * ofa_ledger_attach_to:
 * @combo: this #ofaLedgerCombo instance.
 * @display_mnemo: whether the combo box should display the ledger mnemo.
 * @display_label: whether the combo box should display the ledger label.
 * @parent: the #GtkContainer parent of the combo box.
 *
 * Create a new combo box and attach it to the @parent.
 */
void
ofa_ledger_combo_attach_to( ofaLedgerCombo *combo, gboolean display_mnemo, gboolean display_label, GtkContainer *parent )
{
	static const gchar *thisfn = "ofa_ledger_combo_attach_to";
	ofaLedgerComboPrivate *priv;
	GtkWidget *box;

	g_debug( "%s: combo=%p, display_mnemo=%s, display_label=%s, parent=%p",
			thisfn, ( void * ) combo,
			display_mnemo ? "True":"False",
			display_label ? "True":"False", ( void * ) parent );

	g_return_if_fail( combo && OFA_IS_LEDGER_COMBO( combo ));
	g_return_if_fail( display_mnemo || display_label );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv = combo->priv;

	if( !priv->dispose_has_run ){

		g_object_weak_ref( G_OBJECT( parent ), ( GWeakNotify ) on_parent_finalized, combo );

		box = gtk_combo_box_new();
		gtk_container_add( parent, box );
		priv->combo = GTK_COMBO_BOX( box );

		setup_combo( combo, display_mnemo, display_label );
	}

}

static void
on_parent_finalized( ofaLedgerCombo *self, gpointer finalized_parent )
{
	static const gchar *thisfn = "ofa_ledger_combo_on_parent_finalized";

	g_debug( "%s: self=%p, finalized_parent=%p",
			thisfn, ( void * ) self, ( void * ) finalized_parent );

	g_return_if_fail( self && OFA_IS_LEDGER_COMBO( self ));

	g_object_unref( self );
}

static void
setup_combo( ofaLedgerCombo *self, gboolean display_mnemo, gboolean display_label )
{
	ofaLedgerComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;

	priv = self->priv;

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			JOU_N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING ));
	gtk_combo_box_set_model( priv->combo, tmodel );
	g_object_unref( tmodel );

	if( display_mnemo ){
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", JOU_COL_MNEMO );
	}

	if( display_label ){
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", JOU_COL_LABEL );
	}

	gtk_combo_box_set_id_column ( priv->combo, JOU_COL_MNEMO );

	g_signal_connect( G_OBJECT( priv->combo ), "changed", G_CALLBACK( on_ledger_changed ), self );
}

static void
on_ledger_changed( GtkComboBox *box, ofaLedgerCombo *self )
{
	ofaLedgerComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *mnemo, *label;

	priv = self->priv;

	if( gtk_combo_box_get_active_iter( box, &iter )){

		tmodel = gtk_combo_box_get_model( box );
		gtk_tree_model_get( tmodel, &iter,
				JOU_COL_MNEMO, &mnemo,
				JOU_COL_LABEL, &label,
				-1 );

		if( priv->label ){
			gtk_label_set_text( GTK_LABEL( priv->label ), label );
		}

		g_signal_emit_by_name( G_OBJECT( self ), "changed", mnemo, label );
	}
}

static void
on_ledger_changed_cleanup_handler( ofaLedgerCombo *self, gchar *mnemo, gchar *label )
{
	static const gchar *thisfn = "ofa_ledger_combo_on_ledger_changed_cleanup_handler";

	g_debug( "%s: self=%p, mnemo=%s, label=%s", thisfn, ( void * ) self, mnemo, label );

	g_free( mnemo );
	g_free( label );
}

/**
 * ofa_ledger_attach_label:
 * @combo: this #ofaLedgerCombo instance.
 * @label: a #GtkLabel which serves as a companion label: it will
 *  automatically receives the newly selected ledger label each time
 *  the selection change in the combo box.
 *
 * Setup the companion lbel.
 */
void
ofa_ledger_combo_attach_label( ofaLedgerCombo *combo, GtkWidget *label )
{
	static const gchar *thisfn = "ofa_ledger_combo_attach_label";
	ofaLedgerComboPrivate *priv;

	g_debug( "%s: combo=%p, label=%p",
			thisfn, ( void * ) combo, ( void * ) label );

	g_return_if_fail( combo && OFA_IS_LEDGER_COMBO( combo ));
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	priv = combo->priv;

	if( !priv->dispose_has_run ){

		priv->label = label;
	}

}

/**
 * ofa_ledger_combo_init_view:
 * @combo: this #ofaLedgerCombo instance.
 * @dossier: the currently opened #ofoDossier dossier.
 * @mnemo: the initially selected ledger mnemo.
 */
void
ofa_ledger_combo_init_view( ofaLedgerCombo *combo, ofoDossier *dossier, const gchar *mnemo )
{
	ofaLedgerComboPrivate *priv;

	g_return_if_fail( combo && OFA_IS_LEDGER_COMBO( combo ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	priv = combo->priv;

	if( !priv->dispose_has_run ){

		priv->dossier = dossier;
		load_dataset( combo, mnemo );

		setup_signaling_connect( combo, dossier );
	}
}

static void
load_dataset( ofaLedgerCombo *self, const gchar *mnemo )
{
	ofaLedgerComboPrivate *priv;
	GList *dataset, *it;
	gint idx, i;
	ofoLedger *ledger;
	GtkTreeModel *tmodel;

	priv = self->priv;

	idx = -1;
	dataset = ofo_ledger_get_dataset( priv->dossier );
	tmodel = gtk_combo_box_get_model( priv->combo );

	for( i=0, it=dataset ; it ; ++i, it=it->next ){
		ledger = OFO_LEDGER( it->data );

		insert_row( self, tmodel, ledger );

		if( mnemo && !g_utf8_collate( mnemo, ofo_ledger_get_mnemo( ledger ))){
			idx = i;
		}
	}

	if( idx != -1 ){
		gtk_combo_box_set_active( priv->combo, idx );
	}
}

static void
insert_row( ofaLedgerCombo *self, GtkTreeModel *tmodel, const ofoLedger *ledger )
{
	GtkTreeIter iter;

	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( tmodel ),
			&iter,
			-1,
			JOU_COL_MNEMO, ofo_ledger_get_mnemo( ledger ),
			JOU_COL_LABEL, ofo_ledger_get_label( ledger ),
			-1 );
}

static void
setup_signaling_connect( ofaLedgerCombo *self, ofoDossier *dossier )
{
	g_signal_connect(
			G_OBJECT( dossier ),
			SIGNAL_DOSSIER_NEW_OBJECT, G_CALLBACK( on_new_object ), self );

	g_signal_connect(
			G_OBJECT( dossier ),
			SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), self );

	g_signal_connect(
			G_OBJECT( dossier ),
			SIGNAL_DOSSIER_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), self );

	g_signal_connect(
			G_OBJECT( dossier ),
			SIGNAL_DOSSIER_RELOAD_DATASET, G_CALLBACK( on_reload_dataset ), self );
}

static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaLedgerCombo *self )
{
	static const gchar *thisfn = "ofa_ledger_combo_on_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_LEDGER( object )){
		insert_row( self, gtk_combo_box_get_model( self->priv->combo ), OFO_LEDGER( object ));
	}
}

static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaLedgerCombo *self )
{
	static const gchar *thisfn = "ofa_ledger_combo_on_updated_object";
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	const gchar *mnemo, *new_mnemo;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_LEDGER( object )){
		new_mnemo = ofo_ledger_get_mnemo( OFO_LEDGER( object ));
		mnemo = prev_id ? prev_id : new_mnemo;
		if( find_ledger_by_mnemo( self, mnemo, &tmodel, &iter )){
			gtk_list_store_set(
					GTK_LIST_STORE( tmodel ),
					&iter,
					JOU_COL_MNEMO, new_mnemo,
					JOU_COL_LABEL, ofo_ledger_get_label( OFO_LEDGER( object )),
					-1 );
		}
	}
}

static gboolean
find_ledger_by_mnemo( ofaLedgerCombo *self, const gchar *mnemo, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	ofaLedgerComboPrivate *priv;
	gchar *str;
	gint cmp;

	priv = self->priv;
	*tmodel = gtk_combo_box_get_model( priv->combo );

	if( gtk_tree_model_get_iter_first( *tmodel, iter )){
		while( TRUE ){
			gtk_tree_model_get( *tmodel, iter, JOU_COL_MNEMO, &str, -1 );
			cmp = g_utf8_collate( str, mnemo );
			g_free( str );
			if( cmp == 0 ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( *tmodel, iter )){
				break;
			}
		}
	}

	return( FALSE );
}

static void
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaLedgerCombo *self )
{
	static const gchar *thisfn = "ofa_ledger_combo_on_deleted_object";
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_LEDGER( object )){
		if( find_ledger_by_mnemo(
				self,
				ofo_ledger_get_mnemo( OFO_LEDGER( object )), &tmodel, &iter )){

			gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
		}
	}
}

static void
on_reload_dataset( ofoDossier *dossier, GType type, ofaLedgerCombo *self )
{
	static const gchar *thisfn = "ofa_ledger_combo_on_reload_dataset";
	ofaLedgerComboPrivate *priv;

	g_debug( "%s: dossier=%p, type=%lu, self=%p",
			thisfn, ( void * ) dossier, type, ( void * ) self );

	priv = self->priv;

	if( type == OFO_TYPE_LEDGER ){
		gtk_list_store_clear( GTK_LIST_STORE( gtk_combo_box_get_model( priv->combo )));
		load_dataset( self, NULL );
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
			gtk_tree_model_get( tmodel, &iter,
					JOU_COL_MNEMO, &mnemo,
					-1 );
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
