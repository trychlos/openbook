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
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-double-renderer.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvsortable.h"
#include "api/ofa-preferences.h"

#include "ofa-recurrent-model-store.h"
#include "ofa-recurrent-model-treeview.h"
#include "ofo-recurrent-model.h"
#include "ofo-recurrent-run.h"

/* private instance data
 */
typedef struct {
	gboolean                dispose_has_run;

	/* UI
	 */
	ofaRecurrentModelStore *store;
}
	ofaRecurrentModelTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void      setup_columns( ofaRecurrentModelTreeview *self );
static void      on_cell_data_fn( GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, ofaRecurrentModelTreeview *self );
static void      on_selection_changed( ofaRecurrentModelTreeview *self, GtkTreeSelection *selection, void *empty );
static void      on_selection_activated( ofaRecurrentModelTreeview *self, GtkTreeSelection *selection, void *empty );
static void      on_selection_delete( ofaRecurrentModelTreeview *self, GtkTreeSelection *selection, void *empty );
static void      get_and_send( ofaRecurrentModelTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static GList    *get_selected_with_selection( ofaRecurrentModelTreeview *self, GtkTreeSelection *selection );
static gint      tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentModelTreeview, ofa_recurrent_model_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaRecurrentModelTreeview ))

static void
recurrent_model_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recurrent_model_treeview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECURRENT_MODEL_TREEVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_model_treeview_parent_class )->finalize( instance );
}

static void
recurrent_model_treeview_dispose( GObject *instance )
{
	ofaRecurrentModelTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_RECURRENT_MODEL_TREEVIEW( instance ));

	priv = ofa_recurrent_model_treeview_get_instance_private( OFA_RECURRENT_MODEL_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_model_treeview_parent_class )->dispose( instance );
}

static void
ofa_recurrent_model_treeview_init( ofaRecurrentModelTreeview *self )
{
	static const gchar *thisfn = "ofa_recurrent_model_treeview_init";
	ofaRecurrentModelTreeviewPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECURRENT_MODEL_TREEVIEW( self ));

	priv = ofa_recurrent_model_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->store = NULL;
}

static void
ofa_recurrent_model_treeview_class_init( ofaRecurrentModelTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_recurrent_model_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_model_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_model_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->sort = tvbin_v_sort;

	/**
	 * ofaRecurrentModelTreeview::ofa-recchanged:
	 *
	 * #ofaTVBin sends a 'ofa-selchanged' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaRecurrentModelTreeview proxyes it with this 'ofa-recchanged'
	 * signal, providing the selected objects.
	 *
	 * Argument is the list of selected objects; may be %NULL.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaRecurrentModelTreeview *view,
	 * 						GList                   *list,
	 * 						gpointer                 user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-recchanged",
				OFA_TYPE_RECURRENT_MODEL_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );

	/**
	 * ofaRecurrentModelTreeview::ofa-recactivated:
	 *
	 * #ofaTVBin sends a 'ofa-selactivated' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaRecurrentModelTreeview proxyes it with this 'ofa-recactivated'
	 * signal, providing the selected objects.
	 *
	 * Argument is the list of selected objects.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaRecurrentModelTreeview *view,
	 * 						GList                   *list,
	 * 						gpointer                 user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-recactivated",
				OFA_TYPE_RECURRENT_MODEL_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );

	/**
	 * ofaRecurrentModelTreeview::ofa-recdelete:
	 *
	 * #ofaTVBin sends a 'ofa-seldelete' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaRecurrentModelTreeview proxyes it with this 'ofa-recdelete'
	 * signal, providing the selected objects.
	 *
	 * Argument is the list of selected objects.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaRecurrentModelTreeview *view,
	 * 						GList                   *list,
	 * 						gpointer                 user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-recdelete",
				OFA_TYPE_RECURRENT_MODEL_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );
}

/**
 * ofa_recurrent_model_treeview_new:
 *
 * Returns: a new #ofaRecurrentModelTreeview instance.
 */
ofaRecurrentModelTreeview *
ofa_recurrent_model_treeview_new( void )
{
	ofaRecurrentModelTreeview *view;

	view = g_object_new( OFA_TYPE_RECURRENT_MODEL_TREEVIEW,
				"ofa-tvbin-selmode", GTK_SELECTION_MULTIPLE,
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
 * ofa_recurrent_model_treeview_set_settings_key:
 * @view: this #ofaRecurrentModelTreeview instance.
 * @key: [allow-none]: the prefix of the settings key.
 *
 * Setup the setting key, or reset it to its default if %NULL.
 */
void
ofa_recurrent_model_treeview_set_settings_key( ofaRecurrentModelTreeview *view, const gchar *key )
{
	ofaRecurrentModelTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_RECURRENT_MODEL_TREEVIEW( view ));

	priv = ofa_recurrent_model_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	/* we do not manage any settings here, so directly pass it to the
	 * base class */
	ofa_tvbin_set_name( OFA_TVBIN( view ), key );
}

/**
 * ofa_recurrent_model_treeview_setup_columns:
 * @view: this #ofaRecurrentModelTreeview instance.
 *
 * Setup the treeview columns.
 */
void
ofa_recurrent_model_treeview_setup_columns( ofaRecurrentModelTreeview *view )
{
	ofaRecurrentModelTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_RECURRENT_MODEL_TREEVIEW( view ));

	priv = ofa_recurrent_model_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	setup_columns( view );
	ofa_tvbin_set_cell_data_func( OFA_TVBIN( view ), ( GtkTreeCellDataFunc ) on_cell_data_fn, view );
}

/*
 * Defines the treeview columns
 */
static void
setup_columns( ofaRecurrentModelTreeview *self )
{
	static const gchar *thisfn = "ofa_recurrent_model_treeview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_MNEMO,              _( "Mnemo" ),    _( "Mnemonic" ));
	ofa_tvbin_add_column_text_x ( OFA_TVBIN( self ), REC_MODEL_COL_LABEL,              _( "Label" ),        NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_OPE_TEMPLATE,       _( "Template" ), _( "Operation template" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_PERIODICITY,        _( "Period." ),  _( "Periodicity" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_PERIODICITY_DETAIL, _( "Detail" ),   _( "Periodicity detail" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_DEF_AMOUNT1,        _( "Amount 1" ), _( "Updatable amount n° 1" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_DEF_AMOUNT2,        _( "Amount 2" ), _( "Updatable amount n° 2" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_DEF_AMOUNT3,        _( "Amount 3" ), _( "Updatable amount n° 3" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_ENABLED,            _( "Enabled" ),      NULL );
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), REC_MODEL_COL_NOTES,              _( "Notes" ),        NULL );
	ofa_tvbin_add_column_pixbuf ( OFA_TVBIN( self ), REC_MODEL_COL_NOTES_PNG,             "",           _( "Notes indicator" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_UPD_USER,           _( "User" ),     _( "Last update user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), REC_MODEL_COL_UPD_STAMP,              NULL,        _( "Last update timestamp" ));

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), REC_MODEL_COL_LABEL );
}

/*
 * gray+italic disabled items
 */
static void
on_cell_data_fn( GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaRecurrentModelTreeview *self )
{
	ofoRecurrentModel *recmodel;
	GdkRGBA color;

	if( GTK_IS_CELL_RENDERER_TEXT( renderer )){

		gtk_tree_model_get( tmodel, iter, REC_MODEL_COL_OBJECT, &recmodel, -1 );
		g_return_if_fail( recmodel && OFO_IS_RECURRENT_MODEL( recmodel ));
		g_object_unref( recmodel );

		g_object_set( G_OBJECT( renderer ),
				"style-set",      FALSE,
				"foreground-set", FALSE,
				NULL );

		if( !ofo_recurrent_model_get_is_enabled( recmodel )){
			gdk_rgba_parse( &color, "#808080" );
			g_object_set( G_OBJECT( renderer ), "foreground-rgba", &color, NULL );
			g_object_set( G_OBJECT( renderer ), "style", PANGO_STYLE_ITALIC, NULL );
		}
	}
}

/**
 * ofa_recurrent_model_treeview_set_hub:
 * @view: this #ofaRecurrentModelTreeview instance.
 * @hub: the #ofaHub object of the application.
 *
 * Initialize the underlying store.
 * Read the settings and show the columns accordingly.
 */
void
ofa_recurrent_model_treeview_set_hub( ofaRecurrentModelTreeview *view, ofaHub *hub )
{
	ofaRecurrentModelTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_RECURRENT_MODEL_TREEVIEW( view ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_recurrent_model_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	if( !ofa_itvcolumnable_get_columns_count( OFA_ITVCOLUMNABLE( view ))){
		setup_columns( view );
	}

	priv->store = ofa_recurrent_model_store_new( hub );
	ofa_tvbin_set_store( OFA_TVBIN( view ), GTK_TREE_MODEL( priv->store ));
	g_object_unref( priv->store );

	ofa_itvsortable_set_default_sort( OFA_ITVSORTABLE( view ), REC_MODEL_COL_MNEMO, GTK_SORT_ASCENDING );
}

static void
on_selection_changed( ofaRecurrentModelTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-recchanged" );
}

static void
on_selection_activated( ofaRecurrentModelTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-recactivated" );
}

/*
 * Delete key pressed
 * ofaTVBin base class makes sure the selection is not empty.
 */
static void
on_selection_delete( ofaRecurrentModelTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-recdelete" );
}

/*
 * gtk_tree_selection_get_selected_rows() works even if selection mode
 * is GTK_SELECTION_MULTIPLE (which may happen here)
 */
static void
get_and_send( ofaRecurrentModelTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	GList *list;

	list = get_selected_with_selection( self, selection );
	g_signal_emit_by_name( self, signal, list );
	ofa_recurrent_model_treeview_free_selected( list );
}

/**
 * ofa_recurrent_model_treeview_get_selected:
 * @view: this #ofaRecurrentModelTreeview instance.
 *
 * Returns: the list of selected objects.
 *
 * The returned list should be ofa_recurrent_model_treeview_free_selected()
 * by the caller.
 */
GList *
ofa_recurrent_model_treeview_get_selected( ofaRecurrentModelTreeview *view )
{
	ofaRecurrentModelTreeviewPrivate *priv;
	GtkTreeSelection *selection;

	g_return_val_if_fail( view && OFA_IS_RECURRENT_MODEL_TREEVIEW( view ), NULL );

	priv = ofa_recurrent_model_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));

	return( get_selected_with_selection( view, selection ));
}

/*
 * gtk_tree_selection_get_selected_rows() works even if selection mode
 * is GTK_SELECTION_MULTIPLE (which is the default here)
 */
static GList *
get_selected_with_selection( ofaRecurrentModelTreeview *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GList *selected_rows, *irow, *selected_objects;
	ofoRecurrentModel *model;

	selected_objects = NULL;
	selected_rows = gtk_tree_selection_get_selected_rows( selection, &tmodel );
	for( irow=selected_rows ; irow ; irow=irow->next ){
		if( gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) irow->data )){
			gtk_tree_model_get( tmodel, &iter, REC_MODEL_COL_OBJECT, &model, -1 );
			g_return_val_if_fail( model && OFO_IS_RECURRENT_MODEL( model ), NULL );
			selected_objects = g_list_prepend( selected_objects, model );
		}
	}

	g_list_free_full( selected_rows, ( GDestroyNotify ) gtk_tree_path_free );

	return( selected_objects );
}

static gint
tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_recurrent_model_treeview_v_sort";
	gint cmp;
	gchar *mnemoa, *labela, *templa, *pera, *amount1a, *amount2a, *amount3a, *notesa, *updusera, *updstampa;
	gchar *mnemob, *labelb, *templb, *perb, *amount1b, *amount2b, *amount3b, *notesb, *upduserb, *updstampb;
	GdkPixbuf *pnga, *pngb;
	ofxCounter perdeta, perdetb;

	gtk_tree_model_get( tmodel, a,
			REC_MODEL_COL_MNEMO,              &mnemoa,
			REC_MODEL_COL_LABEL,              &labela,
			REC_MODEL_COL_OPE_TEMPLATE,       &templa,
			REC_MODEL_COL_PERIODICITY,        &pera,
			REC_MODEL_COL_PERIOD_DETAIL_I,    &perdeta,
			REC_MODEL_COL_DEF_AMOUNT1,        &amount1a,
			REC_MODEL_COL_DEF_AMOUNT2,        &amount2a,
			REC_MODEL_COL_DEF_AMOUNT3,        &amount3a,
			REC_MODEL_COL_NOTES,              &notesa,
			REC_MODEL_COL_NOTES_PNG,          &pnga,
			REC_MODEL_COL_UPD_USER,           &updusera,
			REC_MODEL_COL_UPD_STAMP,          &updstampa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			REC_MODEL_COL_MNEMO,              &mnemob,
			REC_MODEL_COL_LABEL,              &labelb,
			REC_MODEL_COL_OPE_TEMPLATE,       &templb,
			REC_MODEL_COL_PERIODICITY,        &perb,
			REC_MODEL_COL_PERIOD_DETAIL_I,    &perdetb,
			REC_MODEL_COL_DEF_AMOUNT1,        &amount1b,
			REC_MODEL_COL_DEF_AMOUNT2,        &amount2b,
			REC_MODEL_COL_DEF_AMOUNT3,        &amount3b,
			REC_MODEL_COL_NOTES,              &notesb,
			REC_MODEL_COL_NOTES_PNG,          &pngb,
			REC_MODEL_COL_UPD_USER,           &upduserb,
			REC_MODEL_COL_UPD_STAMP,          &updstampb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case REC_MODEL_COL_MNEMO:
			cmp = my_collate( mnemoa, mnemob );
			break;
		case REC_MODEL_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case REC_MODEL_COL_OPE_TEMPLATE:
			cmp = my_collate( templa, templb );
			break;
		case REC_MODEL_COL_PERIODICITY:
			cmp = my_collate( pera, perb );
			break;
		case REC_MODEL_COL_PERIODICITY_DETAIL:
			cmp = perdeta < perdetb ? -1 : ( perdeta > perdetb ? 1 : 0 );
			break;
		case REC_MODEL_COL_DEF_AMOUNT1:
			cmp = my_collate( amount1a, amount1b );
			break;
		case REC_MODEL_COL_DEF_AMOUNT2:
			cmp = my_collate( amount2a, amount2b );
			break;
		case REC_MODEL_COL_DEF_AMOUNT3:
			cmp = my_collate( amount3a, amount3b );
			break;
		case REC_MODEL_COL_NOTES:
			cmp = my_collate( notesa, notesb );
			break;
		case REC_MODEL_COL_NOTES_PNG:
			cmp = ofa_itvsortable_sort_png( pnga, pngb );
			break;
		case REC_MODEL_COL_UPD_USER:
			cmp = my_collate( updusera, upduserb );
			break;
		case REC_MODEL_COL_UPD_STAMP:
			cmp = my_collate( updstampa, updstampb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( mnemoa );
	g_free( labela );
	g_free( templa );
	g_free( pera );
	g_free( amount1a );
	g_free( amount2a );
	g_free( amount3a );
	g_free( notesa );
	g_free( updusera );
	g_free( updstampa );
	g_clear_object( &pnga );

	g_free( mnemob );
	g_free( labelb );
	g_free( templb );
	g_free( perb );
	g_free( amount1b );
	g_free( amount2b );
	g_free( amount3b );
	g_free( notesb );
	g_free( upduserb );
	g_free( updstampb );
	g_clear_object( &pngb );

	return( cmp );
}
