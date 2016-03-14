/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/my-utils.h"
#include "api/ofa-periodicity.h"
#include "api/ofa-periodicity-combo.h"

/* private instance data
 */
struct _ofaPeriodicityComboPrivate {
	gboolean      dispose_has_run;
	GtkListStore *store;
};

/* columns in the store
 */
enum {
	COL_CODE = 0,
	COL_LABEL,
	COL_N_COLUMNS
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void create_combo( ofaPeriodicityCombo *self );
static void on_enum_periodicity( const gchar *code, const gchar *label, ofaPeriodicityCombo *self );
static void on_selection_changed( ofaPeriodicityCombo *self, void *empty );

G_DEFINE_TYPE_EXTENDED( ofaPeriodicityCombo, ofa_periodicity_combo, GTK_TYPE_COMBO_BOX, 0,
		G_ADD_PRIVATE( ofaPeriodicityCombo ))

static void
periodicity_combo_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_periodicity_combo_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PERIODICITY_COMBO( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_periodicity_combo_parent_class )->finalize( instance );
}

static void
periodicity_combo_dispose( GObject *instance )
{
	ofaPeriodicityComboPrivate *priv;

	g_return_if_fail( instance && OFA_IS_PERIODICITY_COMBO( instance ));

	priv = ofa_periodicity_combo_get_instance_private( OFA_PERIODICITY_COMBO( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_periodicity_combo_parent_class )->dispose( instance );
}

static void
ofa_periodicity_combo_init( ofaPeriodicityCombo *self )
{
	static const gchar *thisfn = "ofa_periodicity_combo_init";
	ofaPeriodicityComboPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PERIODICITY_COMBO( self ));

	priv = ofa_periodicity_combo_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_periodicity_combo_class_init( ofaPeriodicityComboClass *klass )
{
	static const gchar *thisfn = "ofa_periodicity_combo_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = periodicity_combo_dispose;
	G_OBJECT_CLASS( klass )->finalize = periodicity_combo_finalize;

	/**
	 * ofaPeriodicityCombo::ofa-changed:
	 *
	 * This signal is sent on the #ofaPeriodicityCombo when the selection
	 * from the GtkcomboBox is changed.
	 *
	 * Argument is the selected periodicity code.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaPeriodicityCombo *combo,
	 * 						const gchar       *code,
	 * 						gpointer           user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_PERIODICITY_COMBO,
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
 * ofa_periodicity_combo_new:
 */
ofaPeriodicityCombo *
ofa_periodicity_combo_new( void )
{
	ofaPeriodicityCombo *self;

	self = g_object_new( OFA_TYPE_PERIODICITY_COMBO, NULL );

	create_combo( self );

	g_signal_connect( self, "changed", G_CALLBACK( on_selection_changed ), NULL );

	return( self );
}

static void
create_combo( ofaPeriodicityCombo *self )
{
	ofaPeriodicityComboPrivate *priv;
	GtkCellRenderer *cell;

	priv = ofa_periodicity_combo_get_instance_private( self );

	priv->store = gtk_list_store_new( COL_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING );
	gtk_combo_box_set_model( GTK_COMBO_BOX( self ), GTK_TREE_MODEL( priv->store ));
	g_object_unref( priv->store );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( self ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( self ), cell, "text", COL_LABEL );

	gtk_combo_box_set_id_column ( GTK_COMBO_BOX( self ), COL_CODE );

	ofa_periodicity_enum(( PeriodicityEnumCb ) on_enum_periodicity, self );
}

static void
on_enum_periodicity( const gchar *code, const gchar *label, ofaPeriodicityCombo *self )
{
	ofaPeriodicityComboPrivate *priv;
	GtkTreeIter iter;

	priv = ofa_periodicity_combo_get_instance_private( self );

	gtk_list_store_insert_with_values( priv->store, &iter, -1,
			COL_CODE,  code,
			COL_LABEL, label,
			-1 );
}

static void
on_selection_changed( ofaPeriodicityCombo *combo, void *empty )
{
	const gchar *code;

	code = gtk_combo_box_get_active_id( GTK_COMBO_BOX( combo ));
	if( code ){
		g_signal_emit_by_name( combo, "ofa-changed", code );
	}
}

/**
 * ofa_periodicity_combo_get_selected:
 * @combo:
 *
 * Returns: the currently selected currency as a newly allocated string
 * which should be g_free() by the caller.
 */
gchar *
ofa_periodicity_combo_get_selected( ofaPeriodicityCombo *combo )
{
	ofaPeriodicityComboPrivate *priv;

	g_return_val_if_fail( combo && OFA_IS_PERIODICITY_COMBO( combo ), NULL );

	priv = ofa_periodicity_combo_get_instance_private( combo );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( g_strdup( gtk_combo_box_get_active_id( GTK_COMBO_BOX( combo ))));
}

/**
 * ofa_periodicity_combo_set_selected:
 * @combo:
 */
void
ofa_periodicity_combo_set_selected( ofaPeriodicityCombo *combo, const gchar *code )
{
	ofaPeriodicityComboPrivate *priv;

	g_return_if_fail( combo && OFA_IS_PERIODICITY_COMBO( combo ));
	g_return_if_fail( my_strlen( code ));

	priv = ofa_periodicity_combo_get_instance_private( combo );

	g_return_if_fail( !priv->dispose_has_run );

	gtk_combo_box_set_active_id( GTK_COMBO_BOX( combo ), code );
}
