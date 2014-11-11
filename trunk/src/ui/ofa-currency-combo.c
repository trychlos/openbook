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
	gboolean         dispose_has_run;

	/* input data
	 */
	GtkContainer    *container;
	ofoDossier      *dossier;
	gchar           *combo_name;
	gchar           *label_name;
	ofaCurrencyComboCb pfnSelected;
	gpointer         user_data;

	/* runtime
	 */
	GtkComboBox     *combo;
};

/* column ordering in the currency combobox
 */
enum {
	COL_CODE = 0,
	COL_LABEL,
	N_COLUMNS
};

G_DEFINE_TYPE( ofaCurrencyCombo, ofa_currency_combo, G_TYPE_OBJECT )

static void  on_currency_changed( GtkComboBox *box, ofaCurrencyCombo *self );

static void
currency_combo_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_currency_combo_finalize";
	ofaCurrencyComboPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CURRENCY_COMBO( instance ));

	/* free data members here */
	priv = OFA_CURRENCY_COMBO( instance )->priv;
	g_free( priv->combo_name );
	g_free( priv->label_name );

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
on_dialog_finalized( ofaCurrencyCombo *self, gpointer this_was_the_dialog )
{
	g_return_if_fail( self && OFA_IS_CURRENCY_COMBO( self ));
	g_object_unref( self );
}

/**
 * ofa_currency_combo_init_dialog:
 */
ofaCurrencyCombo *
ofa_currency_combo_new( const ofsCurrencyComboParms *parms )
{
	static const gchar *thisfn = "ofa_currency_combo_new";
	ofaCurrencyCombo *self;
	ofaCurrencyComboPrivate *priv;
	GtkWidget *combo;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkCellRenderer *text_cell;
	const GList *set, *elt;
	gint idx, i;
	ofoCurrency *currency;

	g_return_val_if_fail( parms, NULL );

	g_debug( "%s: parms=%p", thisfn, ( void * ) parms );

	g_return_val_if_fail( GTK_IS_CONTAINER( parms->container ), NULL );
	g_return_val_if_fail( OFO_IS_DOSSIER( parms->dossier ), NULL );
	g_return_val_if_fail( parms->combo_name && g_utf8_strlen( parms->combo_name, -1 ), NULL );

	combo = my_utils_container_get_child_by_name( parms->container, parms->combo_name );
	g_return_val_if_fail( combo && GTK_IS_COMBO_BOX( combo ), NULL );

	self = g_object_new( OFA_TYPE_CURRENCY_COMBO, NULL );

	priv = self->priv;

	/* parms data */
	priv->container = parms->container;
	priv->dossier = parms->dossier;
	priv->combo_name = g_strdup( parms->combo_name );
	priv->label_name = g_strdup( parms->label_name );
	priv->pfnSelected = parms->pfnSelected;
	priv->user_data = parms->user_data;

	/* setup a weak reference on the dialog to auto-unref */
	g_object_weak_ref( G_OBJECT( priv->container ), ( GWeakNotify ) on_dialog_finalized, self );

	/* runtime data */
	priv->combo = GTK_COMBO_BOX( combo );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING ));
	gtk_combo_box_set_model( priv->combo, tmodel );
	g_object_unref( tmodel );

	if( parms->disp_code ){
		text_cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), text_cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), text_cell, "text", COL_CODE );
	}

	if( parms->disp_label ){
		text_cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), text_cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), text_cell, "text", COL_LABEL );
	}

	set = ofo_currency_get_dataset( parms->dossier );
	idx = OFO_BASE_UNSET_ID;

	for( elt=set, i=0 ; elt ; elt=elt->next, ++i ){
		currency = OFO_CURRENCY( elt->data );
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				COL_CODE,  ofo_currency_get_code( currency ),
				COL_LABEL, ofo_currency_get_label( currency ),
				-1 );
		if( parms->initial_code &&
				!g_utf8_collate( parms->initial_code, ofo_currency_get_code( currency ))){
			idx = i;
		}
	}

	g_signal_connect( G_OBJECT( combo ), "changed", G_CALLBACK( on_currency_changed ), self );

	if( idx != OFO_BASE_UNSET_ID ){
		gtk_combo_box_set_active( priv->combo, idx );
	}

	return( self );
}

static void
on_currency_changed( GtkComboBox *box, ofaCurrencyCombo *self )
{
	ofaCurrencyComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkWidget *widget;
	gchar *code, *label;

	/*g_debug( "ofa_currency_combo_on_currency_changed: dialog=%p (%s)",
			( void * ) self->priv->dialog, G_OBJECT_TYPE_NAME( self->priv->dialog ));*/

	priv = self->priv;

	if( gtk_combo_box_get_active_iter( box, &iter )){

		tmodel = gtk_combo_box_get_model( box );
		gtk_tree_model_get( tmodel, &iter,
				COL_CODE, &code,
				COL_LABEL, &label,
				-1 );

		if( priv->label_name ){
			widget = my_utils_container_get_child_by_name( priv->container, priv->label_name );
			if( widget && GTK_IS_LABEL( widget )){
				gtk_label_set_text( GTK_LABEL( widget ), label );
			}
		}

		if( priv->pfnSelected ){
			( *priv->pfnSelected )( code, priv->user_data );
		}

		g_free( label );
		g_free( code );
	}
}

/**
 * ofa_currency_combo_get_selection:
 * @self:
 * @mnemo: [allow-none]:
 * @label: [allow_none]:
 *
 * Returns the intern identifier of the currently selected currency.
 */
gint
ofa_currency_combo_get_selection( ofaCurrencyCombo *self, gchar **code, gchar **label )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gint id;
	gchar *local_code, *local_label;

	g_return_val_if_fail( self && OFA_IS_CURRENCY_COMBO( self ), NULL );

	id = OFO_BASE_UNSET_ID;

	if( !self->priv->dispose_has_run ){

		if( gtk_combo_box_get_active_iter( self->priv->combo, &iter )){
			tmodel = gtk_combo_box_get_model( self->priv->combo );
			gtk_tree_model_get( tmodel, &iter,
					COL_CODE,  &local_code,
					COL_LABEL, &local_label,
					-1 );
			if( code ){
				*code = g_strdup( local_code );
			}
			if( label ){
				*label = g_strdup( local_label );
			}
			g_free( local_label );
			g_free( local_code );
		}
	}

	return( id );
}
