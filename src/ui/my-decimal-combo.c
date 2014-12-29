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

#include "ui/my-decimal-combo.h"

/* private instance data
 */
struct _myDecimalComboPrivate {
	gboolean      dispose_has_run;

	/* UI
	 */
	GtkContainer *container;
	GtkComboBox  *combo;
};

/* column ordering in the date combobox
 */
enum {
	COL_LABEL = 0,						/* the displayable label */
	COL_CHARSEP,						/* the decimal separator as a string */
	N_COLUMNS
};

typedef struct {
	const gchar *code;
	const gchar *label;
}
	sDec;

static const sDec st_dec[] = {
		{ ".", N_( ". (dot)" )},
		{ ",", N_( ", (comma)" )},
		{ 0 }
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

G_DEFINE_TYPE( myDecimalCombo, my_decimal_combo, G_TYPE_OBJECT )

static void       on_widget_finalized( myDecimalCombo *self, gpointer finalized_parent );
static GtkWidget *get_combo_box( myDecimalCombo *self );
static void       setup_combo( myDecimalCombo *self );
static void       populate_combo( myDecimalCombo *self );
static void       on_decimal_changed( GtkComboBox *box, myDecimalCombo *self );

static void
decimal_combo_finalize( GObject *instance )
{
	static const gchar *thisfn = "my_decimal_combo_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && MY_IS_DECIMAL_COMBO( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_decimal_combo_parent_class )->finalize( instance );
}

static void
decimal_combo_dispose( GObject *instance )
{
	myDecimalComboPrivate *priv;

	g_return_if_fail( instance && MY_IS_DECIMAL_COMBO( instance ));

	priv = ( MY_DECIMAL_COMBO( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_decimal_combo_parent_class )->dispose( instance );
}

static void
my_decimal_combo_init( myDecimalCombo *self )
{
	static const gchar *thisfn = "my_decimal_combo_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && MY_IS_DECIMAL_COMBO( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, MY_TYPE_DECIMAL_COMBO, myDecimalComboPrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
my_decimal_combo_class_init( myDecimalComboClass *klass )
{
	static const gchar *thisfn = "my_decimal_combo_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = decimal_combo_dispose;
	G_OBJECT_CLASS( klass )->finalize = decimal_combo_finalize;

	g_type_class_add_private( klass, sizeof( myDecimalComboPrivate ));

	/**
	 * myDecimalCombo::changed:
	 *
	 * This signal is sent when the selection is changed.
	 *
	 * Arguments is the selected date format.
	 *
	 * Handler is of type:
	 * void ( *handler )( myDecimalCombo *combo,
	 * 						const gchar  *decimal_sep,
	 * 						gpointer      user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				MY_TYPE_DECIMAL_COMBO,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_STRING );
}

static void
on_widget_finalized( myDecimalCombo *self, gpointer finalized_widget )
{
	static const gchar *thisfn = "my_decimal_combo_on_widget_finalized";

	g_debug( "%s: self=%p, finalized_widget=%p", thisfn, ( void * ) self, ( void * ) finalized_widget );

	g_return_if_fail( self && MY_IS_DECIMAL_COMBO( self ));

	g_object_unref( self );
}

/**
 * my_decimal_combo_new:
 */
myDecimalCombo *
my_decimal_combo_new( void )
{
	myDecimalCombo *self;

	self = g_object_new( MY_TYPE_DECIMAL_COMBO, NULL );

	return( self );
}

/**
 * my_decimal_combo_attach_to:
 */
void
my_decimal_combo_attach_to( myDecimalCombo *self, GtkContainer *new_parent )
{
	myDecimalComboPrivate *priv;
	GtkWidget *box;

	g_return_if_fail( self && MY_IS_DECIMAL_COMBO( self ));
	g_return_if_fail( new_parent && GTK_IS_CONTAINER( new_parent ));

	priv = self->priv;

	if( !priv->dispose_has_run ){

		box = get_combo_box( self );
		gtk_container_add( new_parent, box );
		g_object_weak_ref( G_OBJECT( box ), ( GWeakNotify ) on_widget_finalized, self );

		gtk_widget_show_all( GTK_WIDGET( new_parent ));
	}
}

static GtkWidget *
get_combo_box( myDecimalCombo *self )
{
	myDecimalComboPrivate *priv;

	priv = self->priv;

	if( !priv->combo ){
		priv->combo = GTK_COMBO_BOX( gtk_combo_box_new());
		setup_combo( self );
		/* we can populate the combobox once as its population is fixed */
		populate_combo( self );
	}

	return( GTK_WIDGET( priv->combo ));
}

static void
setup_combo( myDecimalCombo *self )
{
	myDecimalComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;

	priv = self->priv;
	g_return_if_fail( priv->combo && GTK_IS_COMBO_BOX( priv->combo ));

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING ));
	gtk_combo_box_set_model( priv->combo, tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", COL_LABEL );

	g_signal_connect( G_OBJECT( priv->combo ), "changed", G_CALLBACK( on_decimal_changed ), self );
}

static void
populate_combo( myDecimalCombo *self )
{
	myDecimalComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gint i;

	priv = self->priv;
	g_return_if_fail( priv->combo && GTK_IS_COMBO_BOX( priv->combo ));

	tmodel = gtk_combo_box_get_model( priv->combo );
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
on_decimal_changed( GtkComboBox *combo, myDecimalCombo *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *decimal_sep;

	if( gtk_combo_box_get_active_iter( combo, &iter )){
		tmodel = gtk_combo_box_get_model( combo );
		gtk_tree_model_get( tmodel, &iter, COL_CHARSEP, &decimal_sep, -1 );
		g_signal_emit_by_name( self, "changed", decimal_sep );
		g_free( decimal_sep );
	}
}

/**
 * my_decimal_combo_get_selected:
 * @self:
 *
 * Returns: the currently selected decimal separator, as a newly
 * allocated string which should be g_free() by the caller.
 */
gchar *
my_decimal_combo_get_selected( myDecimalCombo *self )
{
	myDecimalComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *decimal_sep;

	g_return_val_if_fail( self && MY_IS_DECIMAL_COMBO( self ), NULL );

	priv = self->priv;
	decimal_sep = NULL;

	if( !priv->dispose_has_run ){

		if( gtk_combo_box_get_active_iter( priv->combo, &iter )){
			tmodel = gtk_combo_box_get_model( priv->combo );
			gtk_tree_model_get( tmodel, &iter, COL_CHARSEP, &decimal_sep, -1 );
		}
	}

	return( decimal_sep );
}

/**
 * my_decimal_combo_set_selected:
 * @self: this #myDecimalCombo instance.
 * @decimal: the initially selected decimal separator
 */
void
my_decimal_combo_set_selected( myDecimalCombo *self, const gchar *decimal_sep )
{
	static const gchar *thisfn = "my_decimal_combo_set_selected";
	myDecimalComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *sep;
	gint cmp;

	g_debug( "%s: self=%p, decimal_sep=%s", thisfn, ( void * ) self, decimal_sep );

	priv = self->priv;
	g_return_if_fail( priv->combo && GTK_IS_COMBO_BOX( priv->combo ));

	if( !priv->dispose_has_run ){

		tmodel = gtk_combo_box_get_model( priv->combo );
		g_return_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ));

		if( gtk_tree_model_get_iter_first( tmodel, &iter )){
			while( TRUE ){
				gtk_tree_model_get( tmodel, &iter, COL_CHARSEP, &sep, -1 );
				cmp = g_utf8_collate( sep, decimal_sep );
				g_free( sep );
				if( !cmp ){
					gtk_combo_box_set_active_iter( priv->combo, &iter );
					break;
				}
				if( !gtk_tree_model_iter_next( tmodel, &iter )){
					break;
				}
			}
		}
	}
}
