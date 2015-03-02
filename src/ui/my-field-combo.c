/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "ui/my-field-combo.h"

/* private instance data
 */
struct _myFieldComboPrivate {
	gboolean      dispose_has_run;
};

/* column ordering in the date combobox
 */
enum {
	COL_LABEL = 0,						/* the displayable label */
	COL_CHARSEP,						/* the field separator as a string */
	N_COLUMNS
};

typedef struct {
	const gchar *code;
	const gchar *label;
}
	sDec;

static const sDec st_dec[] = {
		{ ";", N_( "; (semi-colon)" )},
		{ "|", N_( "| (pipe)" )},
		{ 0 }
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

G_DEFINE_TYPE( myFieldCombo, my_field_combo, GTK_TYPE_COMBO_BOX )

static void setup_combo( myFieldCombo *combo );
static void populate_combo( myFieldCombo *combo );
static void on_field_changed( myFieldCombo *combo, void *empty );

static void
field_combo_finalize( GObject *instance )
{
	static const gchar *thisfn = "my_field_combo_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && MY_IS_FIELD_COMBO( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_field_combo_parent_class )->finalize( instance );
}

static void
field_combo_dispose( GObject *instance )
{
	myFieldComboPrivate *priv;

	g_return_if_fail( instance && MY_IS_FIELD_COMBO( instance ));

	priv = ( MY_FIELD_COMBO( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_field_combo_parent_class )->dispose( instance );
}

static void
my_field_combo_init( myFieldCombo *self )
{
	static const gchar *thisfn = "my_field_combo_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && MY_IS_FIELD_COMBO( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, MY_TYPE_FIELD_COMBO, myFieldComboPrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
my_field_combo_class_init( myFieldComboClass *klass )
{
	static const gchar *thisfn = "my_field_combo_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = field_combo_dispose;
	G_OBJECT_CLASS( klass )->finalize = field_combo_finalize;

	g_type_class_add_private( klass, sizeof( myFieldComboPrivate ));

	/**
	 * myFieldCombo::ofa-changed:
	 *
	 * This signal is sent when the selection is changed.
	 *
	 * Arguments is the newly selected field separator.
	 *
	 * Handler is of type:
	 * void ( *handler )( myFieldCombo  *combo,
	 * 						const gchar *field_sep,
	 * 						gpointer     user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				MY_TYPE_FIELD_COMBO,
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
 * my_field_combo_new:
 */
myFieldCombo *
my_field_combo_new( void )
{
	myFieldCombo *self;

	self = g_object_new( MY_TYPE_FIELD_COMBO, NULL );

	setup_combo( self );
	/* we can populate the combobox once as its population is fixed */
	populate_combo( self );

	return( self );
}

static void
setup_combo( myFieldCombo *combo )
{
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING ));
	gtk_combo_box_set_model( GTK_COMBO_BOX( combo ), tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", COL_LABEL );

	g_signal_connect( G_OBJECT( combo ), "changed", G_CALLBACK( on_field_changed ), NULL );

	gtk_widget_show_all( GTK_WIDGET( combo ));
}

static void
populate_combo( myFieldCombo *combo )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gint i;

	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ));
	g_return_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ));

	for( i=0 ; st_dec[i].code ; ++i ){
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				COL_LABEL,   st_dec[i].label,
				COL_CHARSEP, st_dec[i].code,
				-1 );
	}
}

static void
on_field_changed( myFieldCombo *combo, void *empty )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *field_sep;

	if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX( combo ), &iter )){
		tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ));
		gtk_tree_model_get( tmodel, &iter, COL_CHARSEP, &field_sep, -1 );
		g_signal_emit_by_name( combo, "ofa-changed", field_sep );
		g_free( field_sep );
	}
}

/**
 * my_field_combo_get_selected:
 * @combo:
 *
 * Returns: the currently selected field separator, as a newly
 * allocated string which should be g_free() by the caller.
 */
gchar *
my_field_combo_get_selected( myFieldCombo *combo )
{
	myFieldComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *field_sep;

	g_return_val_if_fail( combo && MY_IS_FIELD_COMBO( combo ), NULL );

	priv = combo->priv;
	field_sep = NULL;

	if( !priv->dispose_has_run ){

		if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX( combo ), &iter )){
			tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ));
			gtk_tree_model_get( tmodel, &iter, COL_CHARSEP, &field_sep, -1 );
		}
	}

	return( field_sep );
}

/**
 * my_field_combo_set_selected:
 * @combo: this #myFieldCombo instance.
 * @field: the initially selected field separator
 */
void
my_field_combo_set_selected( myFieldCombo *combo, const gchar *field_sep )
{
	static const gchar *thisfn = "my_field_combo_set_selected";
	myFieldComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *sep;
	gint cmp;

	g_debug( "%s: combo=%p, field_sep=%s", thisfn, ( void * ) combo, field_sep );

	g_return_if_fail( combo && MY_IS_FIELD_COMBO( combo ));

	priv = combo->priv;

	if( !priv->dispose_has_run ){

		tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ));
		g_return_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ));

		if( gtk_tree_model_get_iter_first( tmodel, &iter )){
			while( TRUE ){
				gtk_tree_model_get( tmodel, &iter, COL_CHARSEP, &sep, -1 );
				cmp = g_utf8_collate( sep, field_sep );
				g_free( sep );
				if( !cmp ){
					gtk_combo_box_set_active_iter( GTK_COMBO_BOX( combo ), &iter );
					break;
				}
				if( !gtk_tree_model_iter_next( tmodel, &iter )){
					break;
				}
			}
		}
	}
}
