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

#include <glib/gi18n.h>

#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvsortable.h"
#include "api/ofo-currency.h"

#include "core/ofa-currency-store.h"

#include "ui/ofa-currency-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean          dispose_has_run;

	/* UI
	 */
	ofaCurrencyStore *store;
}
	ofaCurrencyTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void         setup_columns( ofaCurrencyTreeview *self );
static void         on_selection_changed( ofaCurrencyTreeview *self, GtkTreeSelection *selection, void *empty );
static void         on_selection_activated( ofaCurrencyTreeview *self, GtkTreeSelection *selection, void *empty );
static void         on_selection_delete( ofaCurrencyTreeview *self, GtkTreeSelection *selection, void *empty );
static void         get_and_send( ofaCurrencyTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static ofoCurrency *get_selected_with_selection( ofaCurrencyTreeview *self, GtkTreeSelection *selection );
static gint         v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaCurrencyTreeview, ofa_currency_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaCurrencyTreeview ))

static void
currency_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_currency_treeview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CURRENCY_TREEVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currency_treeview_parent_class )->finalize( instance );
}

static void
currency_treeview_dispose( GObject *instance )
{
	ofaCurrencyTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_CURRENCY_TREEVIEW( instance ));

	priv = ofa_currency_treeview_get_instance_private( OFA_CURRENCY_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currency_treeview_parent_class )->dispose( instance );
}

static void
ofa_currency_treeview_init( ofaCurrencyTreeview *self )
{
	static const gchar *thisfn = "ofa_currency_treeview_init";
	ofaCurrencyTreeviewPrivate *priv;

	g_return_if_fail( self && OFA_IS_CURRENCY_TREEVIEW( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_currency_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->store = NULL;
}

static void
ofa_currency_treeview_class_init( ofaCurrencyTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_currency_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = currency_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = currency_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->sort = v_sort;

	/**
	 * ofaCurrencyTreeview::ofa-curchanged:
	 *
	 * #ofaTVBin sends a 'ofa-selchanged' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaCurrencyTreeview proxyes it with this 'ofa-curchanged' signal,
	 * providing the #ofoCurrency selected object.
	 *
	 * Argument is the current #ofoCurrency object, may be %NULL.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaCurrencyTreeview *view,
	 * 						ofoCurrency       *object,
	 * 						gpointer           user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-curchanged",
				OFA_TYPE_CURRENCY_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaCurrencyTreeview::ofa-curactivated:
	 *
	 * #ofaTVBin sends a 'ofa-selactivated' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaCurrencyTreeview proxyes it with this 'ofa-curactivated' signal,
	 * providing the #ofoCurrency selected object.
	 *
	 * Argument is the current #ofoCurrency object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaCurrencyTreeview *view,
	 * 						ofoCurrency       *object,
	 * 						gpointer           user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-curactivated",
				OFA_TYPE_CURRENCY_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaCurrencyTreeview::ofa-curdelete:
	 *
	 * #ofaTVBin sends a 'ofa-seldelete' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaCurrencyTreeview proxyes it with this 'ofa-curdelete' signal,
	 * providing the #ofoCurrency selected object.
	 *
	 * Argument is the current #ofoCurrency object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaCurrencyTreeview *view,
	 * 						ofoCurrency       *object,
	 * 						gpointer           user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-curdelete",
				OFA_TYPE_CURRENCY_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );
}

/**
 * ofa_currency_treeview_new:
 */
ofaCurrencyTreeview *
ofa_currency_treeview_new( void )
{
	ofaCurrencyTreeview *view;

	view = g_object_new( OFA_TYPE_CURRENCY_TREEVIEW,
					"ofa-tvbin-shadow", GTK_SHADOW_IN,
					NULL );

	/* signals sent by ofaTVBin base class are intercepted to provide
	 * a #ofoCurrency object instead of just the raw GtkTreeSelection
	 */
	g_signal_connect( view, "ofa-selchanged", G_CALLBACK( on_selection_changed ), NULL );
	g_signal_connect( view, "ofa-selactivated", G_CALLBACK( on_selection_activated ), NULL );

	/* the 'ofa-seldelete' signal is sent in response to the Delete key press.
	 * There may be no current selection.
	 * in this case, the signal is just ignored (not proxied).
	 */
	g_signal_connect( view, "ofa-seldelete", G_CALLBACK( on_selection_delete ), NULL );

	return( view );
}

/**
 * ofa_currency_treeview_set_settings_key:
 * @view: this #ofaCurrencyTreeview instance.
 * @key: [allow-none]: the prefix of the settings key.
 *
 * Setup the setting key, or reset it to its default if %NULL.
 */
void
ofa_currency_treeview_set_settings_key( ofaCurrencyTreeview *view, const gchar *key )
{
	static const gchar *thisfn = "ofa_currency_treeview_set_settings_key";
	ofaCurrencyTreeviewPrivate *priv;

	g_debug( "%s: view=%p, key=%s", thisfn, ( void * ) view, key );

	g_return_if_fail( view && OFA_IS_CURRENCY_TREEVIEW( view ));

	priv = ofa_currency_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	/* we do not manage any settings here, so directly pass it to the
	 * base class */
	ofa_tvbin_set_name( OFA_TVBIN( view ), key );
}

/**
 * ofa_currency_treeview_setup_columns:
 * @view: this #ofaCurrencyTreeview instance.
 *
 * Setup the treeview columns.
 */
void
ofa_currency_treeview_setup_columns( ofaCurrencyTreeview *view )
{
	ofaCurrencyTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_CURRENCY_TREEVIEW( view ));

	priv = ofa_currency_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	setup_columns( view );
}

/*
 * Defines the treeview columns
 */
static void
setup_columns( ofaCurrencyTreeview *self )
{
	static const gchar *thisfn = "ofa_currency_treeview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), CURRENCY_COL_CODE,      _( "Code" ),   _( "ISO 3A code" ));
	ofa_tvbin_add_column_text_x ( OFA_TVBIN( self ), CURRENCY_COL_LABEL,     _( "Label" ),      NULL );
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), CURRENCY_COL_SYMBOL,    _( "Symbol" ),     NULL );
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), CURRENCY_COL_DIGITS,    _( "Digits" ), _( "Digits count" ));
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), CURRENCY_COL_NOTES,     _( "Notes" ),      NULL );
	ofa_tvbin_add_column_pixbuf ( OFA_TVBIN( self ), CURRENCY_COL_NOTES_PNG,    "",         _( "Notes indicator" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), CURRENCY_COL_UPD_USER,  _( "User" ),   _( "Last update user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), CURRENCY_COL_UPD_STAMP,     NULL,      _( "Last update timestamp" ));

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), CURRENCY_COL_LABEL );
}

/**
 * ofa_currency_treeview_set_hub:
 * @view: this #ofaCurrencyTreeview instance.
 * @hub: the current #ofaHub object.
 *
 * Initialize the underlying store.
 * Read the settings and show the columns accordingly.
 */
void
ofa_currency_treeview_set_hub( ofaCurrencyTreeview *view, ofaHub *hub )
{
	ofaCurrencyTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_CURRENCY_TREEVIEW( view ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_currency_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	if( !ofa_itvcolumnable_get_columns_count( OFA_ITVCOLUMNABLE( view ))){
		setup_columns( view );
	}

	priv->store = ofa_currency_store_new( hub );
	ofa_tvbin_set_store( OFA_TVBIN( view ), GTK_TREE_MODEL( priv->store ));
	g_object_unref( priv->store );

	ofa_itvsortable_set_default_sort( OFA_ITVSORTABLE( view ), CURRENCY_COL_CODE, GTK_SORT_ASCENDING );
}

static void
on_selection_changed( ofaCurrencyTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-curchanged" );
}

static void
on_selection_activated( ofaCurrencyTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-curactivated" );
}

/*
 * Delete key pressed
 * ofaTVBin base class makes sure the selection is not empty.
 */
static void
on_selection_delete( ofaCurrencyTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-curdelete" );
}

/*
 * Currency may be %NULL when selection is empty (on 'ofa-curchanged' signal)
 */
static void
get_and_send( ofaCurrencyTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	ofoCurrency *currency;

	currency = get_selected_with_selection( self, selection );
	g_return_if_fail( !currency || OFO_IS_CURRENCY( currency ));

	g_signal_emit_by_name( self, signal, currency );
}

/**
 * ofa_currency_treeview_get_selected:
 * @view: this #ofaCurrencyTreeview instance.
 *
 * Return: the currently selected currency, or %NULL.
 */
ofoCurrency *
ofa_currency_treeview_get_selected( ofaCurrencyTreeview *view )
{
	static const gchar *thisfn = "ofa_currency_treeview_get_selected";
	ofaCurrencyTreeviewPrivate *priv;
	GtkTreeSelection *selection;
	ofoCurrency *currency;

	g_debug( "%s: view=%p", thisfn, ( void * ) view );

	g_return_val_if_fail( view && OFA_IS_CURRENCY_TREEVIEW( view ), NULL );

	priv = ofa_currency_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));
	currency = get_selected_with_selection( view, selection );

	return( currency );
}

/*
 * get_selected_with_selection:
 * @view: this #ofaCurrencyTreeview instance.
 * @selection:
 *
 * Return: the currently selected currency, or %NULL.
 */
static ofoCurrency *
get_selected_with_selection( ofaCurrencyTreeview *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoCurrency *currency;

	currency = NULL;
	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, CURRENCY_COL_OBJECT, &currency, -1 );
		g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), NULL );
		g_object_unref( currency );
	}

	return( currency );
}

static gint
v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_currency_treeview_v_sort";
	gint cmp;
	gchar *codea, *labela, *symba, *digita, *notesa, *updusera, *updstampa;
	gchar *codeb, *labelb, *symbb, *digitb, *notesb, *upduserb, *updstampb;
	GdkPixbuf *pnga, *pngb;

	gtk_tree_model_get( tmodel, a,
			CURRENCY_COL_CODE,      &codea,
			CURRENCY_COL_LABEL,     &labela,
			CURRENCY_COL_SYMBOL,    &symba,
			CURRENCY_COL_DIGITS,    &digita,
			CURRENCY_COL_NOTES,     &notesa,
			CURRENCY_COL_NOTES_PNG, &pnga,
			CURRENCY_COL_UPD_USER,  &updusera,
			CURRENCY_COL_UPD_STAMP, &updstampa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			CURRENCY_COL_CODE,      &codeb,
			CURRENCY_COL_LABEL,     &labelb,
			CURRENCY_COL_SYMBOL,    &symbb,
			CURRENCY_COL_DIGITS,    &digitb,
			CURRENCY_COL_NOTES,     &notesb,
			CURRENCY_COL_NOTES_PNG, &pngb,
			CURRENCY_COL_UPD_USER,  &upduserb,
			CURRENCY_COL_UPD_STAMP, &updstampb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case CURRENCY_COL_CODE:
			cmp = my_collate( codea, codeb );
			break;
		case CURRENCY_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case CURRENCY_COL_SYMBOL:
			cmp = my_collate( symba, symbb );
			break;
		case CURRENCY_COL_DIGITS:
			cmp = ofa_itvsortable_sort_str_int( digita, digitb );
			break;
		case CURRENCY_COL_NOTES:
			cmp = my_collate( notesa, notesb );
			break;
		case CURRENCY_COL_NOTES_PNG:
			cmp = ofa_itvsortable_sort_png( pnga, pngb );
			break;
		case CURRENCY_COL_UPD_USER:
			cmp = my_collate( updusera, upduserb );
			break;
		case CURRENCY_COL_UPD_STAMP:
			cmp = my_collate( updstampa, updstampb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( codea );
	g_free( labela );
	g_free( symba );
	g_free( digita );
	g_free( notesa );
	g_free( updusera );
	g_free( updstampa );
	g_clear_object( &pnga );

	g_free( codeb );
	g_free( labelb );
	g_free( symbb );
	g_free( digitb );
	g_free( notesb );
	g_free( upduserb );
	g_free( updstampb );
	g_clear_object( &pngb );

	return( cmp );
}
