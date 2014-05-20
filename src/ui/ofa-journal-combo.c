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

#include "ui/my-utils.h"
#include "ui/ofa-journal-combo.h"
#include "ui/ofo-journal.h"

/* private class data
 */
struct _ofaJournalComboClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaJournalComboPrivate {
	gboolean    dispose_has_run;

	/* input data
	 */
	GtkDialog         *dialog;
	ofoDossier        *dossier;
	gchar             *combo_name;
	gchar             *label_name;
	ofaJournalComboCb  pfn;
	gpointer           user_data;

	/* runtime
	 */
	GtkComboBox *combo;
	gint         current_journal_id;	/* current selection */
};

/* column ordering in the journal combobox
 */
enum {
	JOU_COL_ID = 0,
	JOU_COL_MNEMO,
	JOU_COL_LABEL,
	JOU_N_COLUMNS
};

static GObjectClass *st_parent_class = NULL;

static GType register_type( void );
static void  class_init( ofaJournalComboClass *klass );
static void  instance_init( GTypeInstance *instance, gpointer klass );
static void  instance_dispose( GObject *instance );
static void  instance_finalize( GObject *instance );
static void  on_journal_changed( GtkComboBox *box, ofaJournalCombo *self );

GType
ofa_journal_combo_get_type( void )
{
	static GType window_type = 0;

	if( !window_type ){
		window_type = register_type();
	}

	return( window_type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_journal_combo_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaJournalComboClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaJournalCombo ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "ofaJournalCombo", &info, 0 );

	return( type );
}

static void
class_init( ofaJournalComboClass *klass )
{
	static const gchar *thisfn = "ofa_journal_combo_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofaJournalComboClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_journal_combo_instance_init";
	ofaJournalCombo *self;

	g_return_if_fail( OFA_IS_JOURNAL_COMBO( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_JOURNAL_COMBO( instance );

	self->private = g_new0( ofaJournalComboPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *window )
{
	static const gchar *thisfn = "ofa_journal_combo_instance_dispose";
	ofaJournalComboPrivate *priv;

	g_return_if_fail( OFA_IS_JOURNAL_COMBO( window ));

	priv = ( OFA_JOURNAL_COMBO( window ))->private;

	if( !priv->dispose_has_run ){
		g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

		priv->dispose_has_run = TRUE;

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( window );
		}
	}
}

static void
instance_finalize( GObject *window )
{
	static const gchar *thisfn = "ofa_journal_combo_instance_finalize";
	ofaJournalCombo *self;

	g_return_if_fail( OFA_IS_JOURNAL_COMBO( window ));

	g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

	self = OFA_JOURNAL_COMBO( window );

	g_free( self->private->combo_name );
	g_free( self->private->label_name );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( window );
	}
}

/**
 * ofa_journal_combo_init_dialog:
 */
ofaJournalCombo *
ofa_journal_combo_init_dialog( ofaJournalComboParms *parms )
{
	static const gchar *thisfn = "ofa_journal_combo_init_dialog";
	ofaJournalCombo *self;
	ofaJournalComboPrivate *priv;
	GtkWidget *combo;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkCellRenderer *text_cell;
	GList *set, *elt;
	gint idx, i;
	ofoJournal *journal;

	g_return_val_if_fail( parms, NULL );

	g_debug( "%s: parms=%p", thisfn, ( void * ) parms );

	g_return_val_if_fail( GTK_IS_DIALOG( parms->dialog ), NULL );
	g_return_val_if_fail( OFO_IS_DOSSIER( parms->dossier ), NULL );
	g_return_val_if_fail( parms->combo_name && g_utf8_strlen( parms->combo_name, -1 ), NULL );

	combo = my_utils_container_get_child_by_name( GTK_CONTAINER( parms->dialog ), parms->combo_name );
	g_return_val_if_fail( combo && GTK_IS_COMBO_BOX( combo ), NULL );

	self = g_object_new( OFA_TYPE_JOURNAL_COMBO, NULL );

	priv = self->private;

	/* parms data */
	priv->dialog = parms->dialog;
	priv->dossier = parms->dossier;
	priv->combo_name = g_strdup( parms->combo_name );
	priv->label_name = g_strdup( parms->label_name );
	priv->pfn = parms->pfn;
	priv->user_data = parms->user_data;

	/* runtime data */
	priv->combo = GTK_COMBO_BOX( combo );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			JOU_N_COLUMNS,
			G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING ));
	gtk_combo_box_set_model( priv->combo, tmodel );
	g_object_unref( tmodel );

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

	set = ofo_dossier_get_journals_set( parms->dossier );

	for( elt=set, i=0, idx=-1 ; elt ; elt=elt->next, ++i ){
		gtk_list_store_append( GTK_LIST_STORE( tmodel ), &iter );
		journal = OFO_JOURNAL( elt->data );
		gtk_list_store_set(
				GTK_LIST_STORE( tmodel ),
				&iter,
				JOU_COL_ID,    ofo_journal_get_id( journal ),
				JOU_COL_MNEMO, ofo_journal_get_mnemo( journal ),
				JOU_COL_LABEL, ofo_journal_get_label( journal ),
				-1 );
		if( parms->initial_id == ofo_journal_get_id( journal )){
			idx = i;
		}
	}

	g_signal_connect( G_OBJECT( combo ), "changed", G_CALLBACK( on_journal_changed ), self );

	if( idx != -1 ){
		gtk_combo_box_set_active( priv->combo, idx );
	}

	return( self );
}

static void
on_journal_changed( GtkComboBox *box, ofaJournalCombo *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkWidget *widget;
	gchar *mnemo, *label;

	/*g_debug( "ofa_journal_combo_on_journal_changed: dialog=%p (%s)",
			( void * ) self->private->dialog, G_OBJECT_TYPE_NAME( self->private->dialog ));*/

	if( gtk_combo_box_get_active_iter( box, &iter )){

		tmodel = gtk_combo_box_get_model( box );
		gtk_tree_model_get( tmodel, &iter,
				JOU_COL_ID, &self->private->current_journal_id,
				JOU_COL_MNEMO, &mnemo,
				JOU_COL_LABEL, &label,
				-1 );

		if( self->private->label_name ){
			widget = my_utils_container_get_child_by_name(
					GTK_CONTAINER( self->private->dialog ), self->private->label_name );
			if( widget && GTK_IS_LABEL( widget )){
				gtk_label_set_text( GTK_LABEL( widget ), label );
			}
		}

		if( self->private->pfn ){
			( *self->private->pfn )
					( self->private->current_journal_id, mnemo, label, self->private->user_data );
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
					JOU_COL_ID, &id,
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
