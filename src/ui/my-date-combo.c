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

#include "ui/my-date-combo.h"

/* private instance data
 */
struct _myDateComboPrivate {
	gboolean      dispose_has_run;

	/* UI
	 */
	GtkContainer *container;
	GtkComboBox  *combo;
};

/* column ordering in the date combobox
 */
enum {
	COL_LABEL = 0,
	COL_FORMAT,
	N_COLUMNS
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

G_DEFINE_TYPE( myDateCombo, my_date_combo, G_TYPE_OBJECT )

static void on_parent_finalized( myDateCombo *self, gpointer finalized_parent );
static void setup_combo( myDateCombo *self );
static void on_format_changed( GtkComboBox *box, myDateCombo *self );

static void
date_combo_finalize( GObject *instance )
{
	static const gchar *thisfn = "my_date_combo_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && MY_IS_DATE_COMBO( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_date_combo_parent_class )->finalize( instance );
}

static void
date_combo_dispose( GObject *instance )
{
	myDateComboPrivate *priv;

	g_return_if_fail( instance && MY_IS_DATE_COMBO( instance ));

	priv = ( MY_DATE_COMBO( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_date_combo_parent_class )->dispose( instance );
}

static void
my_date_combo_init( myDateCombo *self )
{
	static const gchar *thisfn = "my_date_combo_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && MY_IS_DATE_COMBO( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, MY_TYPE_DATE_COMBO, myDateComboPrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
my_date_combo_class_init( myDateComboClass *klass )
{
	static const gchar *thisfn = "my_date_combo_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = date_combo_dispose;
	G_OBJECT_CLASS( klass )->finalize = date_combo_finalize;

	g_type_class_add_private( klass, sizeof( myDateComboPrivate ));

	/**
	 * myDateCombo::changed:
	 *
	 * This signal is sent when the selection is changed.
	 *
	 * Arguments is the selected date format.
	 *
	 * Handler is of type:
	 * void ( *handler )( myDateCombo   *combo,
	 * 						myDateFormat format,
	 * 						gpointer     user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				MY_TYPE_DATE_COMBO,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_INT );
}

static void
on_parent_finalized( myDateCombo *self, gpointer finalized_parent )
{
	static const gchar *thisfn = "my_date_combo_on_parent_finalized";

	g_debug( "%s: self=%p, finalized_parent=%p", thisfn, ( void * ) self, ( void * ) finalized_parent );

	g_return_if_fail( self && MY_IS_DATE_COMBO( self ));

	g_object_unref( self );
}

/**
 * my_date_combo_new:
 */
myDateCombo *
my_date_combo_new( void )
{
	myDateCombo *self;

	self = g_object_new( MY_TYPE_DATE_COMBO, NULL );

	return( self );
}

/**
 * my_date_combo_attach_to:
 */
void
my_date_combo_attach_to( myDateCombo *self, GtkContainer *new_parent )
{
	myDateComboPrivate *priv;
	GtkWidget *box;

	g_return_if_fail( self && MY_IS_DATE_COMBO( self ));
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
setup_combo( myDateCombo *self )
{
	myDateComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;

	priv = self->priv;

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_INT ));
	gtk_combo_box_set_model( priv->combo, tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", COL_LABEL );

	g_signal_connect( G_OBJECT( priv->combo ), "changed", G_CALLBACK( on_format_changed ), self );
}

static void
on_format_changed( GtkComboBox *combo, myDateCombo *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gint format;

	if( gtk_combo_box_get_active_iter( combo, &iter )){
		tmodel = gtk_combo_box_get_model( combo );
		gtk_tree_model_get( tmodel, &iter, COL_FORMAT, &format, -1 );
		g_signal_emit_by_name( self, "changed", format );
	}
}

/**
 * my_date_combo_init_view:
 * @self: this #myDateCombo instance.
 * @format: the initially selected format
 */
void
my_date_combo_init_view( myDateCombo *self, myDateFormat format )
{
	static const gchar *thisfn = "my_date_combo_init_view";
	myDateComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter, first_iter, selected_iter;
	gint i;
	const gchar *cstr;
	gboolean have_first, found;

	g_debug( "%s: self=%p, format=%d", thisfn, ( void * ) self, format );

	priv = self->priv;

	found = FALSE;
	have_first = FALSE;
	tmodel = gtk_combo_box_get_model( priv->combo );
	gtk_list_store_clear( GTK_LIST_STORE( tmodel ));

	for( i=1 ; i<MY_DATE_LAST ; ++i ){
		cstr = my_date_get_format_str( i );
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				COL_LABEL,  cstr,
				COL_FORMAT, i,
				-1 );

		if( i == format ){
			found = TRUE;
			selected_iter = iter;

		} else if( !have_first ){
			have_first = TRUE;
			first_iter = iter;
		}
	}

	if( !found ){
		selected_iter = first_iter;
	}

	gtk_combo_box_set_active_iter( priv->combo, &selected_iter );
}

/**
 * my_date_combo_get_selected:
 * @self:
 *
 * Returns the currently selected date format.
 */
myDateFormat
my_date_combo_get_selected( myDateCombo *self )
{
	myDateComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gint format;

	g_return_val_if_fail( self && MY_IS_DATE_COMBO( self ), 0 );

	priv = self->priv;

	if( !priv->dispose_has_run ){

		if( gtk_combo_box_get_active_iter( priv->combo, &iter )){
			tmodel = gtk_combo_box_get_model( priv->combo );
			gtk_tree_model_get( tmodel, &iter, COL_FORMAT, &format, -1 );
		}
	}

	return( format );
}
