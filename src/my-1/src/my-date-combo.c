/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "my/my-date-combo.h"

/* private instance data
 */
typedef struct {
	gboolean      dispose_has_run;
}
	myDateComboPrivate;

/* column ordering in the date combobox
 */
enum {
	COL_LABEL = 0,						/* the format as a displayable label */
	COL_FORMAT,							/* the myDateFormat format */
	N_COLUMNS
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void setup_combo( myDateCombo *combo );
static void populate_combo( myDateCombo *combo );
static void on_format_changed( myDateCombo *combo, void *empty );

G_DEFINE_TYPE_EXTENDED( myDateCombo, my_date_combo, GTK_TYPE_COMBO_BOX, 0,
		G_ADD_PRIVATE( myDateCombo ))

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

	priv = my_date_combo_get_instance_private( MY_DATE_COMBO( instance ));

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
	myDateComboPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && MY_IS_DATE_COMBO( self ));

	priv = my_date_combo_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
my_date_combo_class_init( myDateComboClass *klass )
{
	static const gchar *thisfn = "my_date_combo_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = date_combo_dispose;
	G_OBJECT_CLASS( klass )->finalize = date_combo_finalize;

	/**
	 * myDateCombo::my-changed:
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
				"my-changed",
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

/**
 * my_date_combo_new:
 */
myDateCombo *
my_date_combo_new( void )
{
	myDateCombo *self;

	self = g_object_new( MY_TYPE_DATE_COMBO, NULL );

	setup_combo( self );
	/* we can populate the combobox once as its population is fixed */
	populate_combo( self );

	return( self );
}

static void
setup_combo( myDateCombo *combo )
{
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
					N_COLUMNS,
					G_TYPE_STRING, G_TYPE_INT ));
	gtk_combo_box_set_model( GTK_COMBO_BOX( combo ), tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", COL_LABEL );

	g_signal_connect( G_OBJECT( combo ), "changed", G_CALLBACK( on_format_changed ), NULL );

	gtk_widget_show_all( GTK_WIDGET( combo ));
}

static void
populate_combo( myDateCombo *combo )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gint dfmt;
	const gchar *cstr;

	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ));
	g_return_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ));

	for( dfmt=MY_DATE_FIRST ; dfmt<MY_DATE_LAST ; ++dfmt ){
		cstr = my_date_get_format_str( dfmt );
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				COL_LABEL,  cstr,
				COL_FORMAT, dfmt,
				-1 );
	}
}

static void
on_format_changed( myDateCombo *combo, void *empty )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gint format;

	if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX( combo ), &iter )){
		tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ));
		gtk_tree_model_get( tmodel, &iter, COL_FORMAT, &format, -1 );
		g_signal_emit_by_name( combo, "my-changed", format );
	}
}

/**
 * my_date_combo_get_selected:
 * @self:
 *
 * Returns the currently selected date format.
 */
myDateFormat
my_date_combo_get_selected( myDateCombo *combo )
{
	myDateComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gint format;

	g_return_val_if_fail( combo && MY_IS_DATE_COMBO( combo ), 0 );

	priv = my_date_combo_get_instance_private( combo );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	format = 0;

	if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX( combo ), &iter )){
		tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ));
		g_return_val_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ), 0 );
		gtk_tree_model_get( tmodel, &iter, COL_FORMAT, &format, -1 );
	}

	return( format );
}

/**
 * my_date_combo_set_selected:
 * @self: this #myDateCombo instance.
 * @format: the format to be selected
 */
void
my_date_combo_set_selected( myDateCombo *combo, myDateFormat format )
{
	static const gchar *thisfn = "my_date_combo_set_selected";
	myDateComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	myDateFormat dfmt;

	g_debug( "%s: combo=%p, format=%d", thisfn, ( void * ) combo, format );

	g_return_if_fail( combo && MY_IS_DATE_COMBO( combo ));

	priv = my_date_combo_get_instance_private( combo );

	g_return_if_fail( !priv->dispose_has_run );

	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ));
	g_return_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ));

	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, &iter, COL_FORMAT, &dfmt, -1 );
			if( dfmt == format ){
				gtk_combo_box_set_active_iter( GTK_COMBO_BOX( combo ), &iter );
				break;
			}
			if( !gtk_tree_model_iter_next( tmodel, &iter )){
				break;
			}
		}
	}
}
