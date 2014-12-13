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
#include "api/ofo-base.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-currency-combo.h"

/* private instance data
 */
struct _ofaCurrencyComboPrivate {
	gboolean     dispose_has_run;

	/* runtime data
	 */
	GtkComboBox *combo;
	gint         code_col_number;
};

static void     istore_iface_init( ofaCurrencyIStoreInterface *iface );
static guint    istore_get_interface_version( const ofaCurrencyIStore *instance );
static void     istore_attach_to( ofaCurrencyIStore *instance, GtkContainer *parent );
static void     istore_set_columns( ofaCurrencyIStore *instance, GtkListStore *store, ofaCurrencyColumns columns );
static void     on_currency_changed( GtkComboBox *box, ofaCurrencyCombo *self );

G_DEFINE_TYPE_EXTENDED( ofaCurrencyCombo, ofa_currency_combo, G_TYPE_OBJECT, 0, \
		G_IMPLEMENT_INTERFACE( OFA_TYPE_CURRENCY_ISTORE, istore_iface_init ));

static void
currency_combo_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_currency_combo_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CURRENCY_COMBO( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currency_combo_parent_class )->finalize( instance );
}

static void
currency_combo_dispose( GObject *instance )
{
	ofaCurrencyComboPrivate *priv;

	g_return_if_fail( instance && OFA_IS_CURRENCY_COMBO( instance ));

	priv = ( OFA_CURRENCY_COMBO( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currency_combo_parent_class )->dispose( instance );
}

static void
ofa_currency_combo_init( ofaCurrencyCombo *self )
{
	static const gchar *thisfn = "ofa_currency_combo_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_CURRENCY_COMBO( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_CURRENCY_COMBO, ofaCurrencyComboPrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
ofa_currency_combo_class_init( ofaCurrencyComboClass *klass )
{
	static const gchar *thisfn = "ofa_currency_combo_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = currency_combo_dispose;
	G_OBJECT_CLASS( klass )->finalize = currency_combo_finalize;

	g_type_class_add_private( klass, sizeof( ofaCurrencyComboPrivate ));
}

static void
istore_iface_init( ofaCurrencyIStoreInterface *iface )
{
	static const gchar *thisfn = "ofa_currency_combo_istore_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = istore_get_interface_version;
	iface->attach_to = istore_attach_to;
	iface->set_columns = istore_set_columns;
}

static guint
istore_get_interface_version( const ofaCurrencyIStore *instance )
{
	return( 1 );
}

static void
istore_attach_to( ofaCurrencyIStore *instance, GtkContainer *parent )
{
	ofaCurrencyComboPrivate *priv;

	g_return_if_fail( instance && OFA_IS_CURRENCY_COMBO( instance ));

	g_debug( "istore_attach_to: instance=%p (%s), parent=%p (%s)",
			( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			( void * ) parent, G_OBJECT_TYPE_NAME( parent ));

	priv = OFA_CURRENCY_COMBO( instance )->priv;

	if( !priv->combo ){
		priv->combo = GTK_COMBO_BOX( gtk_combo_box_new());
	}

	gtk_container_add( parent, GTK_WIDGET( priv->combo ));
}

static void
istore_set_columns( ofaCurrencyIStore *instance, GtkListStore *store, ofaCurrencyColumns columns )
{
	ofaCurrencyComboPrivate *priv;
	GtkCellRenderer *cell;
	gint col_number;

	priv = OFA_CURRENCY_COMBO( instance )->priv;

	if( !priv->combo ){
		priv->combo = GTK_COMBO_BOX( gtk_combo_box_new());
	}

	g_signal_connect(
			G_OBJECT( priv->combo ), "changed", G_CALLBACK( on_currency_changed ), instance );

	gtk_combo_box_set_model( priv->combo, GTK_TREE_MODEL( store ));

	col_number = ofa_currency_istore_get_column_number( instance, CURRENCY_COL_CODE );
	priv->code_col_number = col_number;

	if( columns & CURRENCY_COL_CODE ){
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", col_number );
	}

	if( columns & CURRENCY_COL_LABEL ){
		col_number = ofa_currency_istore_get_column_number( instance, CURRENCY_COL_LABEL );
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", col_number );
	}

	if( columns & CURRENCY_COL_SYMBOL ){
		col_number = ofa_currency_istore_get_column_number( instance, CURRENCY_COL_SYMBOL );
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", col_number );
	}

	if( columns & CURRENCY_COL_DIGITS ){
		col_number = ofa_currency_istore_get_column_number( instance, CURRENCY_COL_DIGITS );
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", col_number );
	}

	gtk_combo_box_set_id_column ( priv->combo, priv->code_col_number );
	gtk_widget_show_all( GTK_WIDGET( priv->combo ));
}

/**
 * ofa_currency_combo_new:
 */
ofaCurrencyCombo *
ofa_currency_combo_new( void )
{
	ofaCurrencyCombo *self;

	self = g_object_new( OFA_TYPE_CURRENCY_COMBO, NULL );

	return( self );
}

static void
on_currency_changed( GtkComboBox *box, ofaCurrencyCombo *self )
{
	ofaCurrencyComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *code;

	priv = self->priv;

	if( gtk_combo_box_get_active_iter( box, &iter )){
		tmodel = gtk_combo_box_get_model( box );
		g_debug( "on_currency_changed: code_col_number=%d", priv->code_col_number );
		gtk_tree_model_get( tmodel, &iter, priv->code_col_number, &code, -1 );
		g_signal_emit_by_name( self, "changed", code );
		g_free( code );
	}
}

/**
 * ofa_currency_combo_get_selected:
 * @combo:
 *
 * Returns: the currently selected currency as a newly allocated string
 * which should be g_free() by the caller.
 */
gchar *
ofa_currency_combo_get_selected( ofaCurrencyCombo *combo )
{
	ofaCurrencyComboPrivate *priv;

	g_return_val_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ), NULL );

	priv = combo->priv;

	if( !priv->dispose_has_run ){

		return( g_strdup( gtk_combo_box_get_active_id( priv->combo )));
	}

	return( NULL );
}

/**
 * ofa_currency_combo_set_selected:
 * @combo:
 */
void
ofa_currency_combo_set_selected( ofaCurrencyCombo *combo, const gchar *code )
{
	ofaCurrencyComboPrivate *priv;

	g_return_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ));
	g_return_if_fail( code && g_utf8_strlen( code, -1 ));

	priv = combo->priv;

	if( !priv->dispose_has_run ){

		gtk_combo_box_set_active_id( priv->combo, code );
	}
}
