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

	/* input data
	 */
	GtkComboBox *combo;
	ofoDossier  *dossier;
	GList       *handlers;
};

/* columns ordering in the combo box
 */
enum {
	COL_CODE = 0,
	COL_LABEL,
	COL_SYMBOL,
	COL_DIGITS,
	COL_NOTES,
	COL_UPD_USER,
	COL_UPD_STAMP,
	N_COLUMNS
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

G_DEFINE_TYPE( ofaCurrencyCombo, ofa_currency_combo, G_TYPE_OBJECT )

static void     on_parent_finalized( ofaCurrencyCombo *self, gpointer finalized_parent );
static void     setup_combo( ofaCurrencyCombo *combo, ofaCurrencyColumns columns );
static void     load_dataset( ofaCurrencyCombo *combo, ofoDossier *dossier, const gchar *code );
static void     insert_row( ofaCurrencyCombo *self, GtkTreeModel *tmodel, const ofoCurrency *currency );
static void     on_currency_changed( GtkComboBox *box, ofaCurrencyCombo *self );
static void     on_currency_changed_cleanup_handler( ofaCurrencyCombo *combo, gchar *code );
static void     setup_signaling_connect( ofaCurrencyCombo *self, ofoDossier *dossier );
static void     on_new_object( ofoDossier *dossier, ofoBase *object, ofaCurrencyCombo *self );
static void     on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaCurrencyCombo *self );
static gboolean find_currency_by_code( ofaCurrencyCombo *self, const gchar *code, GtkTreeModel **tmodel, GtkTreeIter *iter );
static void     on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaCurrencyCombo *self );
static void     on_reload_dataset( ofoDossier *dossier, GType type, ofaCurrencyCombo *self );

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
	GList *it;

	g_return_if_fail( instance && OFA_IS_CURRENCY_COMBO( instance ));

	priv = ( OFA_CURRENCY_COMBO( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		if( priv->dossier && OFO_IS_DOSSIER( priv->dossier )){
			for( it=priv->handlers ; it ; it=it->next ){
				g_signal_handler_disconnect( priv->dossier, ( gulong ) it->data );
			}
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
	 * This signal is sent when the selection is changed.
	 *
	 * Arguments is the selected currency.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaCurrencyCombo *combo,
	 * 						const gchar    *code,
	 * 						gpointer        user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_CURRENCY_COMBO,
				G_SIGNAL_RUN_CLEANUP,
				G_CALLBACK( on_currency_changed_cleanup_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );
}

static void
on_parent_finalized( ofaCurrencyCombo *self, gpointer finalized_parent )
{
	static const gchar *thisfn = "ofa_currency_combo_on_parent_finalized";

	g_debug( "%s: self=%p, finalized_parent=%p",
			thisfn, ( void * ) self, ( void * ) finalized_parent );

	g_return_if_fail( self && OFA_IS_CURRENCY_COMBO( self ));

	g_object_unref( self );
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

/**
 * ofa_currency_combo_attach_to:
 */
void
ofa_currency_combo_attach_to( ofaCurrencyCombo *combo,
									GtkContainer *parent, ofaCurrencyColumns columns )
{
	ofaCurrencyComboPrivate *priv;

	g_return_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ));
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv = combo->priv;

	if( !priv->dispose_has_run ){

		g_object_weak_ref( G_OBJECT( parent ), ( GWeakNotify ) on_parent_finalized, combo );

		priv->combo = GTK_COMBO_BOX( gtk_combo_box_new());
		gtk_container_add( parent, GTK_WIDGET( priv->combo ));

		setup_combo( combo, columns );
		gtk_widget_show_all( GTK_WIDGET( parent ));
	}
}

static void
setup_combo( ofaCurrencyCombo *combo, ofaCurrencyColumns columns )
{
	ofaCurrencyComboPrivate *priv;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;

	priv = combo->priv;

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* code, label, symbol */
			G_TYPE_STRING, G_TYPE_STRING, 					/* digits, notes */
			G_TYPE_STRING, G_TYPE_STRING ));				/* upd_user, upd_stamp */

	gtk_combo_box_set_model( priv->combo, tmodel );
	g_object_unref( tmodel );

	if( columns & CURRENCY_COL_CODE ){
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", COL_CODE );
	}

	if( columns & CURRENCY_COL_LABEL ){
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", COL_LABEL );
	}

	if( columns & CURRENCY_COL_SYMBOL ){
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", COL_SYMBOL );
	}

	if( columns & CURRENCY_COL_DIGITS ){
		cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->combo ), cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->combo ), cell, "text", COL_DIGITS );
	}

	gtk_combo_box_set_id_column ( priv->combo, COL_CODE );

	g_signal_connect( G_OBJECT( priv->combo ), "changed", G_CALLBACK( on_currency_changed ), combo );
}

/**
 * ofa_currency_combo_init_view:
 */
void
ofa_currency_combo_init_view( ofaCurrencyCombo *combo, ofoDossier *dossier, const gchar *code )
{
	ofaCurrencyComboPrivate *priv;

	g_return_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	priv = combo->priv;

	if( !priv->dispose_has_run ){

		load_dataset( combo, dossier, code );
		setup_signaling_connect( combo, dossier );
	}
}

static void
load_dataset( ofaCurrencyCombo *combo, ofoDossier *dossier, const gchar *code )
{
	ofaCurrencyComboPrivate *priv;
	const GList *dataset, *it;
	GtkTreeModel *tmodel;
	ofoCurrency *currency;
	gint i, idx;

	priv = combo->priv;
	dataset = ofo_currency_get_dataset( dossier );
	tmodel = gtk_combo_box_get_model( priv->combo );
	idx = -1;

	for( i=0, it=dataset ; it ; ++i, it=it->next ){
		currency = OFO_CURRENCY( it->data );
		insert_row( combo, tmodel, currency );

		if( code && !g_utf8_collate( code, ofo_currency_get_code( currency ))){
			idx = i;
		}
	}

	if( idx != -1 ){
		gtk_combo_box_set_active( priv->combo, idx );
	}
}

static void
insert_row( ofaCurrencyCombo *self, GtkTreeModel *tmodel, const ofoCurrency *currency )
{
	GtkTreeIter iter;
	gchar *str;

	str = g_strdup_printf( "%d", ofo_currency_get_digits( currency ));

	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( tmodel ),
			&iter,
			-1,
			COL_CODE,   ofo_currency_get_code( currency ),
			COL_LABEL,  ofo_currency_get_label( currency ),
			COL_SYMBOL, ofo_currency_get_symbol( currency ),
			COL_DIGITS, str,
			-1 );

	g_free( str );
}

static void
on_currency_changed( GtkComboBox *box, ofaCurrencyCombo *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *code;

	if( gtk_combo_box_get_active_iter( box, &iter )){
		tmodel = gtk_combo_box_get_model( box );
		gtk_tree_model_get( tmodel, &iter, COL_CODE, &code, -1 );
		g_signal_emit_by_name( self, "changed", code );
	}
}

static void
on_currency_changed_cleanup_handler( ofaCurrencyCombo *combo, gchar *code )
{
	static const gchar *thisfn = "ofa_currency_combo_on_currency_changed_cleanup_handler";

	g_debug( "%s: combo=%p, code=%s", thisfn, ( void * ) combo, code );

	g_free( code );
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

static void
setup_signaling_connect( ofaCurrencyCombo *combo, ofoDossier *dossier )
{
	ofaCurrencyComboPrivate *priv;
	gulong handler;

	priv = combo->priv;

	handler = g_signal_connect( G_OBJECT( dossier ),
					SIGNAL_DOSSIER_NEW_OBJECT, G_CALLBACK( on_new_object ), combo );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect( G_OBJECT( dossier ),
			SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), combo );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect( G_OBJECT( dossier ),
			SIGNAL_DOSSIER_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), combo );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect( G_OBJECT( dossier ),
			SIGNAL_DOSSIER_RELOAD_DATASET, G_CALLBACK( on_reload_dataset ), combo );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );
}

static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaCurrencyCombo *self )
{
	static const gchar *thisfn = "ofa_currency_combo_on_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_CURRENCY( object )){
		insert_row( self, gtk_combo_box_get_model( self->priv->combo ), OFO_CURRENCY( object ));
	}
}

static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaCurrencyCombo *self )
{
	static const gchar *thisfn = "ofa_currency_combo_on_updated_object";
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	const gchar *code, *new_code;
	gchar *str;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_CURRENCY( object )){
		new_code = ofo_currency_get_code( OFO_CURRENCY( object ));
		code = prev_id ? prev_id : new_code;
		if( find_currency_by_code( self, code, &tmodel, &iter )){

			str = g_strdup_printf( "%d", ofo_currency_get_digits( OFO_CURRENCY( object )));
			gtk_list_store_set(
					GTK_LIST_STORE( tmodel ),
					&iter,
					COL_CODE,   new_code,
					COL_LABEL,  ofo_currency_get_label( OFO_CURRENCY( object )),
					COL_SYMBOL, ofo_currency_get_symbol( OFO_CURRENCY( object )),
					COL_DIGITS, str,
					-1 );
			g_free( str );
		}
	}
}

static gboolean
find_currency_by_code( ofaCurrencyCombo *self, const gchar *code, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	ofaCurrencyComboPrivate *priv;
	gchar *str;
	gint cmp;

	priv = self->priv;
	*tmodel = gtk_combo_box_get_model( priv->combo );

	if( gtk_tree_model_get_iter_first( *tmodel, iter )){
		while( TRUE ){
			gtk_tree_model_get( *tmodel, iter, COL_CODE, &str, -1 );
			cmp = g_utf8_collate( str, code );
			g_free( str );
			if( cmp == 0 ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( *tmodel, iter )){
				break;
			}
		}
	}

	return( FALSE );
}

static void
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaCurrencyCombo *self )
{
	static const gchar *thisfn = "ofa_currency_combo_on_deleted_object";
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_CURRENCY( object )){
		if( find_currency_by_code(
				self,
				ofo_currency_get_code( OFO_CURRENCY( object )), &tmodel, &iter )){

			gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
		}
	}
}

static void
on_reload_dataset( ofoDossier *dossier, GType type, ofaCurrencyCombo *self )
{
	static const gchar *thisfn = "ofa_currency_combo_on_reload_dataset";
	ofaCurrencyComboPrivate *priv;

	g_debug( "%s: dossier=%p, type=%lu, self=%p",
			thisfn, ( void * ) dossier, type, ( void * ) self );

	priv = self->priv;

	if( type == OFO_TYPE_CURRENCY ){
		gtk_list_store_clear( GTK_LIST_STORE( gtk_combo_box_get_model( priv->combo )));
		load_dataset( self, priv->dossier, NULL );
	}
}
