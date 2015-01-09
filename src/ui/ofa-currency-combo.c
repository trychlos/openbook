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
#include "ui/ofa-currency-store.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaCurrencyComboPrivate {
	gboolean           dispose_has_run;
	gboolean           from_widget_finalized;

	ofaMainWindow     *main_window;
	ofoDossier        *dossier;
	GList             *handlers;
	ofaCurrencyColumns columns;

	ofaCurrencyStore  *store;
	GtkComboBox       *combo;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void create_combo_box( ofaCurrencyCombo *combo );
static void on_widget_finalized( ofaCurrencyCombo *combo, gpointer finalized_parent );
static void create_combo_columns( ofaCurrencyCombo *combo );
static void on_currency_changed( GtkComboBox *box, ofaCurrencyCombo *self );

G_DEFINE_TYPE( ofaCurrencyCombo, ofa_currency_combo, G_TYPE_OBJECT )

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
		if( !priv->from_widget_finalized ){
			g_object_weak_unref(
					G_OBJECT( priv->combo ), ( GWeakNotify ) on_widget_finalized, instance );
		}
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

	/**
	 * ofaCurrencyCombo::changed:
	 *
	 * This signal is sent on the #ofaCurrencyCombo when the selection
	 * from the GtkcomboBox is changed.
	 *
	 * Argument is the selected currency ISO 3A code.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaCurrencyCombo *combo,
	 * 						const gchar    *code,
	 * 						gpointer        user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_CURRENCY_COMBO,
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
 * ofa_currency_combo_new:
 */
ofaCurrencyCombo *
ofa_currency_combo_new( void )
{
	ofaCurrencyCombo *self;

	self = g_object_new( OFA_TYPE_CURRENCY_COMBO, NULL );

	create_combo_box( self );

	return( self );
}

static void
create_combo_box( ofaCurrencyCombo *combo )
{
	ofaCurrencyComboPrivate *priv;

	priv = combo->priv;

	priv->combo = GTK_COMBO_BOX( gtk_combo_box_new());

	g_signal_connect(
			G_OBJECT( priv->combo ), "changed", G_CALLBACK( on_currency_changed ), combo );
}

/**
 * ofa_currency_combo_attach_to:
 */
void
ofa_currency_combo_attach_to( ofaCurrencyCombo *combo, GtkContainer *parent )
{
	ofaCurrencyComboPrivate *priv;

	g_return_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ));
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	g_debug( "ofa_currency_combo_attach_to: combo=%p, parent=%p (%s)",
			( void * ) combo,
			( void * ) parent, G_OBJECT_TYPE_NAME( parent ));

	priv = combo->priv;

	if( !priv->dispose_has_run ){

		gtk_container_add( parent, GTK_WIDGET( priv->combo ));
		g_object_weak_ref( G_OBJECT( priv->combo ), ( GWeakNotify ) on_widget_finalized, combo );

		gtk_widget_show_all( GTK_WIDGET( parent ));
	}
}

static void
on_widget_finalized( ofaCurrencyCombo *combo, gpointer finalized_widget )
{
	static const gchar *thisfn = "ofa_currency_combo_on_widget_finalized";

	g_debug( "%s: combo=%p, finalized_widget=%p (%s)",
			thisfn, ( void * ) combo, ( void * ) finalized_widget, G_OBJECT_TYPE_NAME( finalized_widget ));

	g_return_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ));

	g_object_unref( combo );
}

/**
 * ofa_currency_combo_set_columns:
 */
void
ofa_currency_combo_set_columns( ofaCurrencyCombo *combo, ofaCurrencyColumns columns )
{
	ofaCurrencyComboPrivate *priv;

	g_return_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ));

	priv = combo->priv;

	if( !priv->dispose_has_run ){

		/* the combobox must have been created first */
		g_return_if_fail( priv->combo && GTK_IS_COMBO_BOX( priv->combo ));

		priv->columns = columns;
		create_combo_columns( combo );
	}
}

static void
create_combo_columns( ofaCurrencyCombo *combo )
{
	ofaCurrencyComboPrivate *priv;
	GtkCellRenderer *cell;

	priv = combo->priv;

	if( priv->columns & CURRENCY_DISP_CODE ){
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", CURRENCY_COL_CODE );
	}

	if( priv->columns & CURRENCY_DISP_LABEL ){
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", CURRENCY_COL_LABEL );
	}

	if( priv->columns & CURRENCY_DISP_SYMBOL ){
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", CURRENCY_COL_SYMBOL );
	}

	if( priv->columns & CURRENCY_DISP_DIGITS ){
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", CURRENCY_COL_DIGITS );
	}

	gtk_combo_box_set_id_column ( priv->combo, CURRENCY_COL_CODE );
}

/**
 * ofa_currency_combo_set_main_window:
 *
 * This is required in order to get the dossier which will permit to
 * create the underlying tree store.
 */
void
ofa_currency_combo_set_main_window( ofaCurrencyCombo *combo, ofaMainWindow *main_window )
{
	ofaCurrencyComboPrivate *priv;

	g_return_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv = combo->priv;

	if( !priv->dispose_has_run ){

		/* the combobox must have been created first */
		g_return_if_fail( priv->combo && GTK_IS_COMBO_BOX( priv->combo ));

		priv->main_window = main_window;
		priv->dossier = ofa_main_window_get_dossier( main_window );
		priv->store = ofa_currency_store_new( priv->dossier );

		gtk_combo_box_set_model( priv->combo, GTK_TREE_MODEL( priv->store ));

		/*dossier_signals_connect( combo );*/
	}
}

static void
on_currency_changed( GtkComboBox *box, ofaCurrencyCombo *self )
{
	ofaCurrencyComboPrivate *priv;
	const gchar *code;

	priv = self->priv;

	code = gtk_combo_box_get_active_id( priv->combo );
	if( code ){
		g_signal_emit_by_name( self, "changed", code );
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
