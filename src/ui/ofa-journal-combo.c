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

#include "core/my-utils.h"
#include "ui/ofa-journal-combo.h"
#include "api/ofo-journal.h"
#include "api/ofo-dossier.h"

/* private instance data
 */
struct _ofaJournalComboPrivate {
	gboolean           dispose_has_run;

	/* input data
	 */
	GtkContainer      *container;
	ofoDossier        *dossier;
	gchar             *combo_name;
	gchar             *label_name;
	ofaJournalComboCb  pfnSelected;
	gpointer           user_data;

	/* runtime
	 */
	GtkComboBox       *combo;
	GtkTreeModel      *tmodel;
};

/* column ordering in the journal combobox
 */
enum {
	JOU_COL_MNEMO = 0,
	JOU_COL_LABEL,
	JOU_N_COLUMNS
};

G_DEFINE_TYPE( ofaJournalCombo, ofa_journal_combo, G_TYPE_OBJECT )

static void     load_dataset( ofaJournalCombo *self, const gchar *initial_mnemo );
static void     insert_new_row( ofaJournalCombo *self, const ofoJournal *journal );
static void     setup_signaling_connect( ofaJournalCombo *self );
static void     on_journal_changed( GtkComboBox *box, ofaJournalCombo *self );
static void     on_new_object( ofoDossier *dossier, ofoBase *object, ofaJournalCombo *self );
static void     on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaJournalCombo *self );
static gboolean find_journal_by_mnemo( ofaJournalCombo *self, const gchar *mnemo, GtkTreeModel **tmodel, GtkTreeIter *iter );
static void     on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaJournalCombo *self );
static void     on_reload_dataset( ofoDossier *dossier, GType type, ofaJournalCombo *self );

static void
journal_combo_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_journal_combo_finalize";
	ofaJournalComboPrivate *priv;

	g_return_if_fail( OFA_IS_JOURNAL_COMBO( instance ));

	priv = OFA_JOURNAL_COMBO( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	g_free( priv->combo_name );
	g_free( priv->label_name );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_journal_combo_parent_class )->finalize( instance );
}

static void
journal_combo_dispose( GObject *instance )
{
	ofaJournalComboPrivate *priv;

	g_return_if_fail( OFA_IS_JOURNAL_COMBO( instance ));

	priv = ( OFA_JOURNAL_COMBO( instance ))->private;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_journal_combo_parent_class )->dispose( instance );
}

static void
ofa_journal_combo_init( ofaJournalCombo *self )
{
	static const gchar *thisfn = "ofa_journal_combo_init";

	g_return_if_fail( OFA_IS_JOURNAL_COMBO( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaJournalComboPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
ofa_journal_combo_class_init( ofaJournalComboClass *klass )
{
	static const gchar *thisfn = "ofa_journal_combo_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = journal_combo_dispose;
	G_OBJECT_CLASS( klass )->finalize = journal_combo_finalize;
}

static void
on_container_finalized( ofaJournalCombo *self, gpointer this_was_the_container )
{
	g_return_if_fail( self && OFA_IS_JOURNAL_COMBO( self ));
	g_object_unref( self );
}

/**
 * ofa_journal_combo_new:
 */
ofaJournalCombo *
ofa_journal_combo_new( const ofaJournalComboParms *parms )
{
	static const gchar *thisfn = "ofa_journal_combo_new";
	ofaJournalCombo *self;
	ofaJournalComboPrivate *priv;
	GtkWidget *combo;
	GtkCellRenderer *text_cell;

	g_return_val_if_fail( parms, NULL );

	g_debug( "%s: parms=%p", thisfn, ( void * ) parms );

	g_return_val_if_fail( GTK_IS_CONTAINER( parms->container ), NULL );
	g_return_val_if_fail( OFO_IS_DOSSIER( parms->dossier ), NULL );
	g_return_val_if_fail( parms->combo_name && g_utf8_strlen( parms->combo_name, -1 ), NULL );

	combo = my_utils_container_get_child_by_name( parms->container, parms->combo_name );
	g_return_val_if_fail( combo && GTK_IS_COMBO_BOX( combo ), NULL );

	self = g_object_new( OFA_TYPE_JOURNAL_COMBO, NULL );

	priv = self->private;

	/* parms data */
	priv->container = parms->container;
	priv->dossier = parms->dossier;
	priv->combo_name = g_strdup( parms->combo_name );
	priv->label_name = g_strdup( parms->label_name );
	priv->pfnSelected = parms->pfnSelected;
	priv->user_data = parms->user_data;

	/* setup a weak reference on the container to auto-unref */
	g_object_weak_ref( G_OBJECT( priv->container ), ( GWeakNotify ) on_container_finalized, self );

	/* runtime data */
	priv->combo = GTK_COMBO_BOX( combo );
	gtk_combo_box_set_id_column ( priv->combo, JOU_COL_MNEMO );

	priv->tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			JOU_N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING ));
	gtk_combo_box_set_model( priv->combo, priv->tmodel );
	g_object_unref( priv->tmodel );

	if( parms->disp_mnemo ){
		text_cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), text_cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), text_cell, "text", JOU_COL_MNEMO );
	}

	if( parms->disp_label ){
		text_cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), text_cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), text_cell, "text", JOU_COL_LABEL );
	}

	g_signal_connect( G_OBJECT( combo ), "changed", G_CALLBACK( on_journal_changed ), self );

	load_dataset( self, parms->initial_mnemo );

	setup_signaling_connect( self );

	return( self );
}

static void
load_dataset( ofaJournalCombo *self, const gchar *initial_mnemo )
{
	ofaJournalComboPrivate *priv;
	const GList *set, *elt;
	gint idx, i;
	ofoJournal *journal;

	priv = self->private;
	set = ofo_journal_get_dataset( priv->dossier );

	for( elt=set, i=0, idx=-1 ; elt ; elt=elt->next, ++i ){
		journal = OFO_JOURNAL( elt->data );
		insert_new_row( self, journal );
		if( initial_mnemo &&
				!g_utf8_collate( initial_mnemo, ofo_journal_get_mnemo( journal ))){
			idx = i;
		}
	}

	if( idx != -1 ){
		gtk_combo_box_set_active( priv->combo, idx );
	}
}

static void
insert_new_row( ofaJournalCombo *self, const ofoJournal *journal )
{
	GtkTreeIter iter;

	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( self->private->tmodel ),
			&iter,
			-1,
			JOU_COL_MNEMO, ofo_journal_get_mnemo( journal ),
			JOU_COL_LABEL, ofo_journal_get_label( journal ),
			-1 );
}

static void
setup_signaling_connect( ofaJournalCombo *self )
{
	ofaJournalComboPrivate *priv;

	priv = self->private;

	g_signal_connect(
			G_OBJECT( priv->dossier ),
			OFA_SIGNAL_NEW_OBJECT, G_CALLBACK( on_new_object ), self );

	g_signal_connect(
			G_OBJECT( priv->dossier ),
			OFA_SIGNAL_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), self );

	g_signal_connect(
			G_OBJECT( priv->dossier ),
			OFA_SIGNAL_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), self );

	g_signal_connect(
			G_OBJECT( priv->dossier ),
			OFA_SIGNAL_RELOAD_DATASET, G_CALLBACK( on_reload_dataset ), self );
}

static void
on_journal_changed( GtkComboBox *box, ofaJournalCombo *self )
{
	ofaJournalComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkWidget *widget;
	gchar *mnemo, *label;

	/*g_debug( "ofa_journal_combo_on_journal_changed: dialog=%p (%s)",
			( void * ) self->private->dialog, G_OBJECT_TYPE_NAME( self->private->dialog ));*/

	priv = self->private;

	if( gtk_combo_box_get_active_iter( box, &iter )){

		tmodel = gtk_combo_box_get_model( box );
		gtk_tree_model_get( tmodel, &iter,
				JOU_COL_MNEMO, &mnemo,
				JOU_COL_LABEL, &label,
				-1 );

		if( priv->label_name ){
			widget = my_utils_container_get_child_by_name(
							priv->container, priv->label_name );
			if( widget && GTK_IS_LABEL( widget )){
				gtk_label_set_text( GTK_LABEL( widget ), label );
			}
		}

		if( priv->pfnSelected ){
			( *priv->pfnSelected )( mnemo, priv->user_data );
		}

		g_free( label );
		g_free( mnemo );
	}
}

/**
 * ofa_journal_combo_get_selection:
 * @self:
 * @mnemo: [allow-none]:
 * @label: [allow_none]:
 *
 * Returns the intern identifier of the currently selected journal.
 */
gint
ofa_journal_combo_get_selection( ofaJournalCombo *self, gchar **mnemo, gchar **label )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gint id;
	gchar *local_mnemo, *local_label;

	g_return_val_if_fail( self && OFA_IS_JOURNAL_COMBO( self ), NULL );

	id = -1;

	if( !self->private->dispose_has_run ){

		if( gtk_combo_box_get_active_iter( self->private->combo, &iter )){

			tmodel = gtk_combo_box_get_model( self->private->combo );
			gtk_tree_model_get( tmodel, &iter,
					JOU_COL_MNEMO, &local_mnemo,
					JOU_COL_LABEL, &local_label,
					-1 );

			if( mnemo ){
				*mnemo = g_strdup( local_mnemo );
			}
			if( label ){
				*label = g_strdup( local_label );
			}

			g_free( local_label );
			g_free( local_mnemo );
		}
	}

	return( id );
}

/**
 * ofa_journal_combo_set_selection:
 */
void
ofa_journal_combo_set_selection( ofaJournalCombo *self, const gchar *mnemo )
{
	g_return_if_fail( self && OFA_IS_JOURNAL_COMBO( self ));

	if( !self->private->dispose_has_run ){

		gtk_combo_box_set_active_id( self->private->combo, mnemo );
	}
}

static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaJournalCombo *self )
{
	static const gchar *thisfn = "ofa_journal_combo_on_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_JOURNAL( object )){
		insert_new_row( self, OFO_JOURNAL( object ));
	}
}

static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaJournalCombo *self )
{
	static const gchar *thisfn = "ofa_journal_combo_on_updated_object";
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	const gchar *mnemo, *new_mnemo;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_JOURNAL( object )){
		new_mnemo = ofo_journal_get_mnemo( OFO_JOURNAL( object ));
		mnemo = prev_id ? prev_id : new_mnemo;
		if( find_journal_by_mnemo( self, mnemo, &tmodel, &iter )){
			gtk_list_store_set(
					GTK_LIST_STORE( tmodel ),
					&iter,
					JOU_COL_MNEMO, new_mnemo,
					JOU_COL_LABEL, ofo_journal_get_label( OFO_JOURNAL( object )),
					-1 );
		}
	}
}

static gboolean
find_journal_by_mnemo( ofaJournalCombo *self, const gchar *mnemo, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	ofaJournalComboPrivate *priv;
	gchar *str;
	gint cmp;

	priv = self->private;
	*tmodel = priv->tmodel;

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
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaJournalCombo *self )
{
	static const gchar *thisfn = "ofa_journal_combo_on_deleted_object";
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_JOURNAL( object )){
		if( find_journal_by_mnemo(
				self,
				ofo_journal_get_mnemo( OFO_JOURNAL( object )), &tmodel, &iter )){

			gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
		}
	}
}

static void
on_reload_dataset( ofoDossier *dossier, GType type, ofaJournalCombo *self )
{
	static const gchar *thisfn = "ofa_journal_combo_on_reload_dataset";
	ofaJournalComboPrivate *priv;

	g_debug( "%s: dossier=%p, type=%lu, self=%p",
			thisfn, ( void * ) dossier, type, ( void * ) self );

	priv = self->private;

	if( type == OFO_TYPE_JOURNAL ){
		gtk_list_store_clear( GTK_LIST_STORE( priv->tmodel ));
		load_dataset( self, NULL );
	}
}
