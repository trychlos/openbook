/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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
#include "api/ofo-rate.h"

#include "core/ofa-rate-store.h"

#include "ui/ofa-rate-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean      dispose_has_run;

	/* initialization
	 */
	ofaHub       *hub;

	/* UI
	 */
	ofaRateStore *store;
}
	ofaRateTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void     setup_columns( ofaRateTreeview *self );
static void     on_selection_changed( ofaRateTreeview *self, GtkTreeSelection *selection, void *empty );
static void     on_selection_activated( ofaRateTreeview *self, GtkTreeSelection *selection, void *empty );
static void     on_selection_delete( ofaRateTreeview *self, GtkTreeSelection *selection, void *empty );
static void     get_and_send( ofaRateTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static ofoRate *get_selected_with_selection( ofaRateTreeview *self, GtkTreeSelection *selection );
static gint     tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaRateTreeview, ofa_rate_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaRateTreeview ))

static void
rate_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_rate_treeview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RATE_TREEVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rate_treeview_parent_class )->finalize( instance );
}

static void
rate_treeview_dispose( GObject *instance )
{
	ofaRateTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_RATE_TREEVIEW( instance ));

	priv = ofa_rate_treeview_get_instance_private( OFA_RATE_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rate_treeview_parent_class )->dispose( instance );
}

static void
ofa_rate_treeview_init( ofaRateTreeview *self )
{
	static const gchar *thisfn = "ofa_rate_treeview_init";
	ofaRateTreeviewPrivate *priv;

	g_return_if_fail( self && OFA_IS_RATE_TREEVIEW( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_rate_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->store = NULL;
}

static void
ofa_rate_treeview_class_init( ofaRateTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_rate_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = rate_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = rate_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->sort = tvbin_v_sort;

	/**
	 * ofaRateTreeview::ofa-ratchanged:
	 *
	 * #ofaTVBin sends a 'ofa-selchanged' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaRateTreeview proxyes it with this 'ofa-ratchanged' signal,
	 * providing the #ofoRate selected object.
	 *
	 * Argument is the current #ofoRate object, may be %NULL.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaRateTreeview *view,
	 * 						ofoRate       *object,
	 * 						gpointer       user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-ratchanged",
				OFA_TYPE_RATE_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaRateTreeview::ofa-ratactivated:
	 *
	 * #ofaTVBin sends a 'ofa-selactivated' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaRateTreeview proxyes it with this 'ofa-ratactivated' signal,
	 * providing the #ofoRate selected object.
	 *
	 * Argument is the current #ofoRate object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaRateTreeview *view,
	 * 						ofoRate       *object,
	 * 						gpointer       user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-ratactivated",
				OFA_TYPE_RATE_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaRateTreeview::ofa-ratdelete:
	 *
	 * #ofaTVBin sends a 'ofa-seldelete' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaRateTreeview proxyes it with this 'ofa-ratdelete' signal,
	 * providing the #ofoRate selected object.
	 *
	 * Argument is the current #ofoRate object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaRateTreeview *view,
	 * 						ofoRate       *object,
	 * 						gpointer       user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-ratdelete",
				OFA_TYPE_RATE_TREEVIEW,
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
 * ofa_rate_treeview_new:
 * @hub: the #ofaHub object of the application.
 *
 * Returns: a new #ofaRateTreeview instance.
 */
ofaRateTreeview *
ofa_rate_treeview_new( ofaHub *hub )
{
	ofaRateTreeview *view;
	ofaRateTreeviewPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	view = g_object_new( OFA_TYPE_RATE_TREEVIEW,
					"ofa-tvbin-hub",    hub,
					"ofa-tvbin-shadow", GTK_SHADOW_IN,
					NULL );

	priv = ofa_rate_treeview_get_instance_private( view );

	priv->hub = hub;

	/* signals sent by ofaTVBin base class are intercepted to provide
	 * a #ofoRate object instead of just the raw GtkTreeSelection
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
 * ofa_rate_treeview_set_settings_key:
 * @view: this #ofaRateTreeview instance.
 * @key: [allow-none]: the prefix of the settings key.
 *
 * Setup the setting key, or reset it to its default if %NULL.
 */
void
ofa_rate_treeview_set_settings_key( ofaRateTreeview *view, const gchar *key )
{
	static const gchar *thisfn = "ofa_rate_treeview_set_settings_key";
	ofaRateTreeviewPrivate *priv;

	g_debug( "%s: view=%p, key=%s", thisfn, ( void * ) view, key );

	g_return_if_fail( view && OFA_IS_RATE_TREEVIEW( view ));

	priv = ofa_rate_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	/* we do not manage any settings here, so directly pass it to the
	 * base class */
	ofa_tvbin_set_name( OFA_TVBIN( view ), key );
}

/**
 * ofa_rate_treeview_setup_columns:
 * @view: this #ofaRateTreeview instance.
 *
 * Setup the treeview columns.
 */
void
ofa_rate_treeview_setup_columns( ofaRateTreeview *view )
{
	ofaRateTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_RATE_TREEVIEW( view ));

	priv = ofa_rate_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	setup_columns( view );
}

/*
 * Defines the treeview columns
 */
static void
setup_columns( ofaRateTreeview *self )
{
	static const gchar *thisfn = "ofa_rate_treeview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), RATE_COL_MNEMO,     _( "Mnemo" ),  _( "Mnemonic" ));
	ofa_tvbin_add_column_text_x ( OFA_TVBIN( self ), RATE_COL_LABEL,     _( "Label" ),      NULL );
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), RATE_COL_NOTES,     _( "Notes" ),      NULL );
	ofa_tvbin_add_column_pixbuf ( OFA_TVBIN( self ), RATE_COL_NOTES_PNG,    "",         _( "Notes indicator" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), RATE_COL_UPD_USER,  _( "User" ),   _( "Last update user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), RATE_COL_UPD_STAMP,     NULL,      _( "Last update timestamp" ));

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), RATE_COL_LABEL );
}

/**
 * ofa_rate_treeview_setup_store:
 * @view: this #ofaRateTreeview instance.
 *
 * Initialize the underlying store.
 * Read the settings and show the columns accordingly.
 */
void
ofa_rate_treeview_setup_store( ofaRateTreeview *view )
{
	ofaRateTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_RATE_TREEVIEW( view ));

	priv = ofa_rate_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	if( !ofa_itvcolumnable_get_columns_count( OFA_ITVCOLUMNABLE( view ))){
		setup_columns( view );
	}

	priv->store = ofa_rate_store_new( priv->hub );
	ofa_tvbin_set_store( OFA_TVBIN( view ), GTK_TREE_MODEL( priv->store ));
	g_object_unref( priv->store );

	ofa_itvsortable_set_default_sort( OFA_ITVSORTABLE( view ), RATE_COL_MNEMO, GTK_SORT_ASCENDING );
}

static void
on_selection_changed( ofaRateTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-ratchanged" );
}

static void
on_selection_activated( ofaRateTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-ratactivated" );
}

/*
 * Delete key pressed
 * ofaTVBin base class makes sure the selection is not empty.
 */
static void
on_selection_delete( ofaRateTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-ratdelete" );
}

/*
 * Rate may be %NULL when selection is empty (on 'ofa-ratchanged' signal)
 */
static void
get_and_send( ofaRateTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	ofoRate *rate;

	rate = get_selected_with_selection( self, selection );
	g_return_if_fail( !rate || OFO_IS_RATE( rate ));

	g_signal_emit_by_name( self, signal, rate );
}

/**
 * ofa_rate_treeview_get_selected:
 * @view: this #ofaRateTreeview instance.
 *
 * Return: the currently selected rate, or %NULL.
 */
ofoRate *
ofa_rate_treeview_get_selected( ofaRateTreeview *view )
{
	static const gchar *thisfn = "ofa_rate_treeview_get_selected";
	ofaRateTreeviewPrivate *priv;
	GtkTreeSelection *selection;
	ofoRate *rate;

	g_debug( "%s: view=%p", thisfn, ( void * ) view );

	g_return_val_if_fail( view && OFA_IS_RATE_TREEVIEW( view ), NULL );

	priv = ofa_rate_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));
	rate = get_selected_with_selection( view, selection );

	return( rate );
}

/*
 * get_selected_with_selection:
 * @view: this #ofaRateTreeview instance.
 * @selection:
 *
 * Return: the currently selected rate, or %NULL.
 */
static ofoRate *
get_selected_with_selection( ofaRateTreeview *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoRate *rate;

	rate = NULL;
	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, RATE_COL_OBJECT, &rate, -1 );
		g_return_val_if_fail( rate && OFO_IS_RATE( rate ), NULL );
		g_object_unref( rate );
	}

	return( rate );
}

static gint
tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_rate_treeview_v_sort";
	gint cmp;
	gchar *mnemoa, *labela, *notesa, *updusera, *updstampa;
	gchar *mnemob, *labelb, *notesb, *upduserb, *updstampb;
	GdkPixbuf *pnga, *pngb;

	gtk_tree_model_get( tmodel, a,
			RATE_COL_MNEMO,     &mnemoa,
			RATE_COL_LABEL,     &labela,
			RATE_COL_NOTES,     &notesa,
			RATE_COL_NOTES_PNG, &pnga,
			RATE_COL_UPD_USER,  &updusera,
			RATE_COL_UPD_STAMP, &updstampa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			RATE_COL_MNEMO,     &mnemob,
			RATE_COL_LABEL,     &labelb,
			RATE_COL_NOTES,     &notesb,
			RATE_COL_NOTES_PNG, &pngb,
			RATE_COL_UPD_USER,  &upduserb,
			RATE_COL_UPD_STAMP, &updstampb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case RATE_COL_MNEMO:
			cmp = my_collate( mnemoa, mnemob );
			break;
		case RATE_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case RATE_COL_NOTES:
			cmp = my_collate( notesa, notesb );
			break;
		case RATE_COL_NOTES_PNG:
			cmp = ofa_itvsortable_sort_png( pnga, pngb );
			break;
		case RATE_COL_UPD_USER:
			cmp = my_collate( updusera, upduserb );
			break;
		case RATE_COL_UPD_STAMP:
			cmp = my_collate( updstampa, updstampb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( mnemoa );
	g_free( labela );
	g_free( notesa );
	g_free( updusera );
	g_free( updstampa );
	g_clear_object( &pnga );

	g_free( mnemob );
	g_free( labelb );
	g_free( notesb );
	g_free( upduserb );
	g_free( updstampb );
	g_clear_object( &pngb );

	return( cmp );
}
