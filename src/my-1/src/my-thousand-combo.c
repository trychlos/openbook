/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#include "my/my-char.h"
#include "my/my-thousand-combo.h"

/* private instance data
 */
typedef struct {
	gboolean      dispose_has_run;
}
	myThousandComboPrivate;

/* column ordering in the thousand_separator combobox
 */
enum {
	COL_LABEL = 0,						/* the displayable label */
	COL_CHARSEP,						/* the thousand separator as a string */
	N_COLUMNS
};

/* Characters which are usable as field separator
 * Have to be defined in myChar.
 */
static const gunichar st_chars[] = {
		MY_CHAR_COMMA,
		MY_CHAR_DOT,
		MY_CHAR_SPACE,
		0
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void setup_combo( myThousandCombo *combo );
static void populate_combo( myThousandCombo *combo );
static void on_thousand_changed( myThousandCombo *combo, void *empty );

G_DEFINE_TYPE_EXTENDED( myThousandCombo, my_thousand_combo, GTK_TYPE_COMBO_BOX, 0,
		G_ADD_PRIVATE( myThousandCombo ))

static void
thousand_combo_finalize( GObject *instance )
{
	static const gchar *thisfn = "my_thousand_combo_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && MY_IS_THOUSAND_COMBO( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_thousand_combo_parent_class )->finalize( instance );
}

static void
thousand_combo_dispose( GObject *instance )
{
	myThousandComboPrivate *priv;

	g_return_if_fail( instance && MY_IS_THOUSAND_COMBO( instance ));

	priv = my_thousand_combo_get_instance_private( MY_THOUSAND_COMBO( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_thousand_combo_parent_class )->dispose( instance );
}

static void
my_thousand_combo_init( myThousandCombo *self )
{
	static const gchar *thisfn = "my_thousand_combo_init";
	myThousandComboPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && MY_IS_THOUSAND_COMBO( self ));

	priv = my_thousand_combo_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
my_thousand_combo_class_init( myThousandComboClass *klass )
{
	static const gchar *thisfn = "my_thousand_combo_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = thousand_combo_dispose;
	G_OBJECT_CLASS( klass )->finalize = thousand_combo_finalize;

	/**
	 * myThousandCombo::my-changed:
	 *
	 * This signal is sent when the selection is changed.
	 *
	 * Arguments is the selected date format.
	 *
	 * Handler is of type:
	 * void ( *handler )( myThousandCombo *combo,
	 * 						const gchar  *thousand_sep,
	 * 						gpointer      user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"my-changed",
				MY_TYPE_THOUSAND_COMBO,
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
 * my_thousand_combo_new:
 */
myThousandCombo *
my_thousand_combo_new( void )
{
	myThousandCombo *self;

	self = g_object_new( MY_TYPE_THOUSAND_COMBO, NULL );

	setup_combo( self );
	/* we can populate the combobox once as its population is fixed */
	populate_combo( self );

	return( self );
}

static void
setup_combo( myThousandCombo *combo )
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

	g_signal_connect( G_OBJECT( combo ), "changed", G_CALLBACK( on_thousand_changed ), NULL );

	gtk_widget_show_all( GTK_WIDGET( combo ));
}

static void
populate_combo( myThousandCombo *combo )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gint i;
	gchar *str;

	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ));
	g_return_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ));

	for( i=0 ; st_chars[i] ; ++i ){
		str = g_strdup_printf( "%c", st_chars[i] );
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				COL_LABEL,   my_char_get_label( st_chars[i] ),
				COL_CHARSEP, str,
				-1 );
		g_free( str );
	}
}

static void
on_thousand_changed( myThousandCombo *combo, void *empty )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *thousand_sep;

	if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX( combo ), &iter )){
		tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ));
		gtk_tree_model_get( tmodel, &iter, COL_CHARSEP, &thousand_sep, -1 );
		g_signal_emit_by_name( combo, "my-changed", thousand_sep );
		g_free( thousand_sep );
	}
}

/**
 * my_thousand_combo_get_selected:
 * @self:
 *
 * Returns: the currently selected thousand separator, as a newly
 * allocated string which should be g_free() by the caller.
 */
gchar *
my_thousand_combo_get_selected( myThousandCombo *combo )
{
	myThousandComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *thousand_sep;

	g_return_val_if_fail( combo && MY_IS_THOUSAND_COMBO( combo ), NULL );

	priv = my_thousand_combo_get_instance_private( combo );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	thousand_sep = NULL;

	if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX( combo ), &iter )){
		tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ));
		gtk_tree_model_get( tmodel, &iter, COL_CHARSEP, &thousand_sep, -1 );
	}

	return( thousand_sep );
}

/**
 * my_thousand_combo_set_selected:
 * @combo: this #myThousandCombo instance.
 * @thousand: the initially selected thousand separator
 */
void
my_thousand_combo_set_selected( myThousandCombo *combo, const gchar *thousand_sep )
{
	static const gchar *thisfn = "my_thousand_combo_set_selected";
	myThousandComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *sep;
	gint cmp;

	g_debug( "%s: combo=%p, thousand_sep=%s", thisfn, ( void * ) combo, thousand_sep );

	g_return_if_fail( combo && MY_IS_THOUSAND_COMBO( combo ));

	priv = my_thousand_combo_get_instance_private( combo );

	g_return_if_fail( !priv->dispose_has_run );

	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ));
	g_return_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ));

	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, &iter, COL_CHARSEP, &sep, -1 );
			cmp = g_utf8_collate( sep, thousand_sep );
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
