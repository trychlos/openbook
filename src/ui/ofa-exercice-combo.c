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
#include "api/ofa-dossier-misc.h"
#include "api/ofa-settings.h"

#include "ui/ofa-exercice-combo.h"
#include "ui/ofa-exercice-store.h"

/* private instance data
 */
struct _ofaExerciceComboPrivate {
	gboolean          dispose_has_run;

	/* UI
	 */
	ofaExerciceStore *store;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

G_DEFINE_TYPE( ofaExerciceCombo, ofa_exercice_combo, GTK_TYPE_COMBO_BOX )

static void setup_combo( ofaExerciceCombo *combo );
static void on_exercice_changed( ofaExerciceCombo *combo, void *empty );
static void on_exercice_changed_cleanup_handler( ofaExerciceCombo *combo, gchar *label, gchar *dbname );

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
				"ofa-changed",
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

/**
 * ofa_exercice_combo_new:
 */
ofaExerciceCombo *
ofa_exercice_combo_new( void )
{
	ofaExerciceCombo *combo;

	combo = g_object_new( OFA_TYPE_EXERCICE_COMBO, NULL );

	setup_combo( combo );

	return( combo );
}

static void
setup_combo( ofaExerciceCombo *combo )
{
	ofaExerciceComboPrivate *priv;
	GtkCellRenderer *cell;

	priv = combo->priv;

	priv->store = ofa_exercice_store_new();
	gtk_combo_box_set_model( GTK_COMBO_BOX( combo ), GTK_TREE_MODEL( priv->store ));
	g_object_unref( priv->store );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", EXERCICE_COL_LABEL );

	g_signal_connect( G_OBJECT( combo ), "changed", G_CALLBACK( on_exercice_changed ), NULL );
}

static void
on_exercice_changed( ofaExerciceCombo *combo, void *empty )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *label, *dbname;

	label = NULL;

	if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX( combo ), &iter )){
		tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ));
		gtk_tree_model_get( tmodel, &iter,
				EXERCICE_COL_LABEL, &label,
				EXERCICE_COL_DBNAME, &dbname,
				-1 );
		g_signal_emit_by_name( combo, "ofa-changed", label, dbname );
	}
}

static void
on_exercice_changed_cleanup_handler( ofaExerciceCombo *combo, gchar *label, gchar *dbname )
{
	static const gchar *thisfn = "ofa_exercice_combo_on_exercice_changed_cleanup_handler";

	g_debug( "%s: combo=%p, label=%s, dbname=%s", thisfn, ( void * ) combo, label, dbname );

	g_free( dbname );
	g_free( label );
}

/**
 * ofa_exercice_combo_set_dossier:
 * @combo: this #ofaExerciceCombo instance.
 * @dname: the name of the dossier from which we want known exercices
 */
void
ofa_exercice_combo_set_dossier( ofaExerciceCombo *combo, const gchar *dname )
{
	static const gchar *thisfn = "ofa_exercice_combo_set_dossier";
	ofaExerciceComboPrivate *priv;

	g_debug( "%s: combo=%p, dname=%s", thisfn, ( void * ) combo, dname );

	priv = combo->priv;

	ofa_exercice_store_set_dossier( priv->store, dname );

	gtk_combo_box_set_active( GTK_COMBO_BOX( combo ), 0 );
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
