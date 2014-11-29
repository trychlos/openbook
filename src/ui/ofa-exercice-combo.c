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
#include "api/ofa-settings.h"

#include "ui/ofa-dossier-misc.h"
#include "ui/ofa-exercice-combo.h"

/* private instance data
 */
struct _ofaExerciceComboPrivate {
	gboolean         dispose_has_run;

	/* UI
	 */
	GtkContainer    *container;
	GtkComboBox     *combo;
};

/* column ordering in the exercice combobox
 */
enum {
	COL_LABEL = 0,
	COL_DBNAME,
	N_COLUMNS
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

G_DEFINE_TYPE( ofaExerciceCombo, ofa_exercice_combo, G_TYPE_OBJECT )

static void on_parent_finalized( ofaExerciceCombo *self, gpointer finalized_parent );
static void setup_combo( ofaExerciceCombo *self );
static void on_exercice_changed( GtkComboBox *box, ofaExerciceCombo *self );
static void on_exercice_changed_cleanup_handler( ofaExerciceCombo *self, gchar *label, gchar *dbname );

static void
exercice_combo_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_exercice_combo_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EXERCICE_COMBO( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exercice_combo_parent_class )->finalize( instance );
}

static void
exercice_combo_dispose( GObject *instance )
{
	ofaExerciceComboPrivate *priv;

	g_return_if_fail( instance && OFA_IS_EXERCICE_COMBO( instance ));

	priv = ( OFA_EXERCICE_COMBO( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exercice_combo_parent_class )->dispose( instance );
}

static void
ofa_exercice_combo_init( ofaExerciceCombo *self )
{
	static const gchar *thisfn = "ofa_exercice_combo_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXERCICE_COMBO( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_EXERCICE_COMBO, ofaExerciceComboPrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
ofa_exercice_combo_class_init( ofaExerciceComboClass *klass )
{
	static const gchar *thisfn = "ofa_exercice_combo_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = exercice_combo_dispose;
	G_OBJECT_CLASS( klass )->finalize = exercice_combo_finalize;

	g_type_class_add_private( klass, sizeof( ofaExerciceComboPrivate ));

	/**
	 * ofaExerciceCombo::changed:
	 *
	 * This signal is sent when the selection is changed.
	 *
	 * Arguments is the label and the database name of the selected
	 * exercice.
	 *
	 * The cleanup handler will take care of freeing these strings.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaExerciceCombo *combo,
	 * 						const gchar    *label,
	 * 						const gchar    *dbname,
	 * 						gpointer        user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_EXERCICE_COMBO,
				G_SIGNAL_RUN_CLEANUP,
				G_CALLBACK( on_exercice_changed_cleanup_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_POINTER, G_TYPE_POINTER );
}

static void
on_parent_finalized( ofaExerciceCombo *self, gpointer finalized_parent )
{
	static const gchar *thisfn = "ofa_exercice_combo_on_parent_finalized";

	g_debug( "%s: self=%p, finalized_parent=%p", thisfn, ( void * ) self, ( void * ) finalized_parent );

	g_return_if_fail( self && OFA_IS_EXERCICE_COMBO( self ));

	g_object_unref( self );
}

/**
 * ofa_exercice_combo_new:
 */
ofaExerciceCombo *
ofa_exercice_combo_new( void )
{
	ofaExerciceCombo *self;

	self = g_object_new( OFA_TYPE_EXERCICE_COMBO, NULL );

	return( self );
}

/**
 * ofa_exercice_combo_attach_to:
 */
void
ofa_exercice_combo_attach_to( ofaExerciceCombo *self, GtkContainer *new_parent )
{
	ofaExerciceComboPrivate *priv;
	GtkWidget *box;

	g_return_if_fail( self && OFA_IS_EXERCICE_COMBO( self ));
	g_return_if_fail( new_parent && GTK_IS_CONTAINER( new_parent ));

	priv = self->priv;

	if( !priv->dispose_has_run ){

		g_object_weak_ref( G_OBJECT( new_parent ), ( GWeakNotify ) on_parent_finalized, self );

		box = gtk_combo_box_new();
		gtk_container_add( new_parent, box );
		priv->combo = GTK_COMBO_BOX( box );

		setup_combo( self );
	}
}

static void
setup_combo( ofaExerciceCombo *self )
{
	ofaExerciceComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;

	priv = self->priv;

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING ));
	gtk_combo_box_set_model( priv->combo, tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", COL_LABEL );

	g_signal_connect( G_OBJECT( priv->combo ), "changed", G_CALLBACK( on_exercice_changed ), self );
}

static void
on_exercice_changed( GtkComboBox *combo, ofaExerciceCombo *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *label, *dbname;

	label = NULL;

	if( gtk_combo_box_get_active_iter( combo, &iter )){
		tmodel = gtk_combo_box_get_model( combo );
		gtk_tree_model_get( tmodel, &iter,
				COL_LABEL, &label,
				COL_DBNAME, &dbname,
				-1 );
		g_signal_emit_by_name( self, "changed", label, dbname );
	}
}

static void
on_exercice_changed_cleanup_handler( ofaExerciceCombo *self, gchar *label, gchar *dbname )
{
	static const gchar *thisfn = "ofa_exercice_combo_on_exercice_changed_cleanup_handler";

	g_debug( "%s: self=%p, label=%s, dbname=%s", thisfn, ( void * ) self, label, dbname );

	g_free( dbname );
	g_free( label );
}

/**
 * ofa_exercice_combo_init_view:
 * @self: this #ofaExerciceCombo instance.
 * @dname: the name of the dossier from which we want known exercices
 */
void
ofa_exercice_combo_init_view( ofaExerciceCombo *self, const gchar *dname )
{
	static const gchar *thisfn = "ofa_exercice_combo_init_view";
	ofaExerciceComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter, first_iter;
	GSList *list, *it;
	gchar **array;
	gboolean have_first;

	g_debug( "%s: self=%p, dname=%s", thisfn, ( void * ) self, dname );

	priv = self->priv;

	have_first = FALSE;
	list = ofa_dossier_misc_get_exercices( dname );
	tmodel = gtk_combo_box_get_model( priv->combo );
	gtk_list_store_clear( GTK_LIST_STORE( tmodel ));

	for( it=list ; it ; it=it->next ){
		array = g_strsplit(( const gchar * ) it->data, ";", -1 );
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				COL_LABEL,  *array,
				COL_DBNAME, *(array+1),
				-1 );
		g_strfreev( array );

		if( !have_first ){
			have_first = TRUE;
			first_iter = iter;
		}
	}
	ofa_dossier_misc_free_exercices( list );

	gtk_combo_box_set_active_iter( priv->combo, &first_iter );
}

#if 0
/**
 * ofa_exercice_combo_get_selection:
 * @self:
 *
 * Returns the intern identifier of the currently selected exercice.
 */
gchar *
ofa_exercice_combo_get_selected( ofaExerciceCombo *self )
{
	ofaExerciceComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *label;

	g_return_val_if_fail( self && OFA_IS_EXERCICE_COMBO( self ), NULL );

	priv = self->priv;
	label = NULL;

	if( !priv->dispose_has_run ){

		if( gtk_combo_box_get_active_iter( priv->combo, &iter )){
			tmodel = gtk_combo_box_get_model( priv->combo );
			gtk_tree_model_get( tmodel, &iter,
					COL_LABEL, &label,
					-1 );
		}
	}

	return( label );
}
#endif
