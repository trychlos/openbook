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

#include <glib/gi18n.h>

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-dosexe-combo.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaDosexeComboPrivate {
	gboolean           dispose_has_run;

	/* input data on instanciation
	 */
	GtkContainer      *container;
	gchar             *combo_name;
	ofaMainWindow     *main_window;

	/* runtime
	 */
	GtkComboBox       *combo;
	GtkTreeModel      *tmodel;
};

/* column ordering in the exercices combobox
 */
enum {
	EXE_COL_ID = 0,
	EXE_COL_LABEL,
	EXE_N_COLUMNS
};

/* signals defined here
 */
enum {
	EXE_SELECTED = 0,
	N_SIGNALS
};

static gint st_signals[ N_SIGNALS ] = { 0 };

G_DEFINE_TYPE( ofaDosexeCombo, ofa_dosexe_combo, G_TYPE_OBJECT )

static void     setup_dataset( ofaDosexeCombo *self, gint id );
static void     on_selection_changed( GtkComboBox *box, ofaDosexeCombo *self );

static void
dosexe_combo_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dosexe_combo_finalize";
	ofaDosexeComboPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSEXE_COMBO( instance ));

	/* free data members here */
	priv = OFA_DOSEXE_COMBO( instance )->priv;
	g_free( priv->combo_name );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dosexe_combo_parent_class )->finalize( instance );
}

static void
dosexe_combo_dispose( GObject *instance )
{
	ofaDosexeComboPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSEXE_COMBO( instance ));

	priv = ( OFA_DOSEXE_COMBO( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dosexe_combo_parent_class )->dispose( instance );
}

static void
ofa_dosexe_combo_init( ofaDosexeCombo *self )
{
	static const gchar *thisfn = "ofa_dosexe_combo_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSEXE_COMBO( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_DOSEXE_COMBO, ofaDosexeComboPrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
ofa_dosexe_combo_class_init( ofaDosexeComboClass *klass )
{
	static const gchar *thisfn = "ofa_dosexe_combo_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dosexe_combo_dispose;
	G_OBJECT_CLASS( klass )->finalize = dosexe_combo_finalize;

	g_type_class_add_private( klass, sizeof( ofaDosexeComboPrivate ));

	/**
	 * ofaDosexeCombo::ofa-signal-exercice-selected:
	 *
	 * The signal is emitted when the selection changes.
	 *
	 * Handler is of type:
	 * 		void user_handler ( ofaDosexeCombo *combo,
	 * 								gint        exe_id ,
	 * 								gpointer    user_data );
	 */
	st_signals[ EXE_SELECTED ] = g_signal_new_class_handler(
				DOSEXE_SIGNAL_EXE_SELECTED,
				OFA_TYPE_DOSEXE_COMBO,
				G_SIGNAL_RUN_CLEANUP | G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_INT );
}

static void
on_container_finalized( ofaDosexeCombo *self, gpointer this_was_the_container )
{
	g_return_if_fail( self && OFA_IS_DOSEXE_COMBO( self ));
	g_object_unref( self );
}

/**
 * ofa_dosexe_combo_new:
 */
ofaDosexeCombo *
ofa_dosexe_combo_new( const ofaDosexeComboParms *parms )
{
	static const gchar *thisfn = "ofa_dosexe_combo_new";
	ofaDosexeCombo *self;
	ofaDosexeComboPrivate *priv;
	GtkWidget *combo;
	GtkCellRenderer *text_cell;

	g_return_val_if_fail( parms, NULL );

	g_debug( "%s: parms=%p", thisfn, ( void * ) parms );

	g_return_val_if_fail( GTK_IS_CONTAINER( parms->container ), NULL );
	g_return_val_if_fail( parms->combo_name && g_utf8_strlen( parms->combo_name, -1 ), NULL );
	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( parms->main_window ), NULL );

	combo = my_utils_container_get_child_by_name( parms->container, parms->combo_name );
	g_return_val_if_fail( combo && GTK_IS_COMBO_BOX( combo ), NULL );

	self = g_object_new( OFA_TYPE_DOSEXE_COMBO, NULL );

	priv = self->priv;

	/* parms data */
	priv->container = parms->container;
	priv->combo_name = g_strdup( parms->combo_name );
	priv->main_window = parms->main_window;

	/* setup a weak reference on the container to auto-unref */
	g_object_weak_ref( G_OBJECT( priv->container ), ( GWeakNotify ) on_container_finalized, self );

	/* runtime data */
	priv->combo = GTK_COMBO_BOX( combo );

	priv->tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			EXE_N_COLUMNS,
			G_TYPE_INT, G_TYPE_STRING ));
	gtk_combo_box_set_model( priv->combo, priv->tmodel );
	g_object_unref( priv->tmodel );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), text_cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), text_cell, "text", EXE_COL_LABEL );

	g_signal_connect( G_OBJECT( combo ), "changed", G_CALLBACK( on_selection_changed ), self );

	setup_dataset( self, parms->exe_id );

	return( self );
}

static void
setup_dataset( ofaDosexeCombo *self, gint initial_id )
{
	ofaDosexeComboPrivate *priv;
	ofoDossier *dossier;
	GList *set, *elt;
	gint exe_id, idx, i;
	ofaDossierStatus status;
	const GDate *dbegin, *dend;
	GtkTreeIter iter;
	gchar *sbegin, *send;
	GString *text;

	priv = self->priv;
	dossier = ofa_main_window_get_dossier( priv->main_window );
	set = ofo_dossier_get_exercices_list( dossier );

	for( elt=set, i=0, idx=-1 ; elt ; elt=elt->next, ++i ){
		exe_id = GPOINTER_TO_INT( elt->data );

		status = ofo_dossier_get_exe_status( dossier, exe_id );
		dbegin = ofo_dossier_get_exe_begin( dossier, exe_id );
		dend = ofo_dossier_get_exe_end( dossier, exe_id );

		sbegin = my_date_to_str( dbegin, MY_DATE_DMYY );
		send = my_date_to_str( dend, MY_DATE_DMYY );

		if( status == DOS_STATUS_CLOSED ){
			text = g_string_new( "" );
			g_string_append_printf( text, _( "Archived exercice from %s to %s" ), sbegin, send );

		} else {
			g_return_if_fail( status == DOS_STATUS_OPENED );
			text = g_string_new( _( "Current exercice " ));
			if( my_date_is_valid( dbegin )){
				g_string_append_printf( text, _( "from %s " ), sbegin );
			}
			if( my_date_is_valid( dend )){
				g_string_append_printf( text, _( "to %s " ), send );
			}
		}

		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( self->priv->tmodel ),
				&iter,
				-1,
				EXE_COL_ID, exe_id,
				EXE_COL_LABEL, text->str,
				-1 );

		g_string_free( text, TRUE );
		g_free( sbegin );
		g_free( send );

		if( initial_id > 0 && exe_id == initial_id ){
			idx = i;
		}
	}

	if( idx != -1 ){
		gtk_combo_box_set_active( priv->combo, idx );
	}

	g_list_free( set );
}

static void
on_selection_changed( GtkComboBox *box, ofaDosexeCombo *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gint exe_id;

	if( gtk_combo_box_get_active_iter( box, &iter )){
		tmodel = gtk_combo_box_get_model( box );
		gtk_tree_model_get( tmodel, &iter, EXE_COL_ID, &exe_id, -1 );
		g_signal_emit_by_name( self, DOSEXE_SIGNAL_EXE_SELECTED, exe_id );
	}
}

/**
 * ofa_dosexe_combo_set_active:
 * @instance:
 * @exe_id: the exercice identifier to select.
 */
void
ofa_dosexe_combo_set_active( ofaDosexeCombo *instance, gint exe_id )
{
	ofaDosexeComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gint id;

	g_return_if_fail( instance && OFA_IS_DOSEXE_COMBO( instance ));

	priv = instance->priv;

	tmodel = gtk_combo_box_get_model( priv->combo );
	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, &iter, EXE_COL_ID, &id, -1 );
			if( id == exe_id ){
				gtk_combo_box_set_active_iter( priv->combo, &iter );
				break;
			}
			if( !gtk_tree_model_iter_next( tmodel, &iter )){
				break;
			}
		}
	}
}
