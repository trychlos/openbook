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

#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-double-renderer.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvsortable.h"
#include "api/ofa-prefs.h"

#include "ofa-recurrent-model-store.h"
#include "ofa-recurrent-model-treeview.h"
#include "ofo-recurrent-model.h"
#include "ofo-recurrent-run.h"

/* private instance data
 */
typedef struct {
	gboolean                dispose_has_run;

	/* initialization
	 */
	ofaIGetter             *getter;
	gchar                  *settings_prefix;

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

static void   setup_columns( ofaRecurrentModelTreeview *self );
static void   on_cell_data_fn( GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, ofaRecurrentModelTreeview *self );
static void   on_selection_changed( ofaRecurrentModelTreeview *self, GtkTreeSelection *selection, void *empty );
static void   on_selection_activated( ofaRecurrentModelTreeview *self, GtkTreeSelection *selection, void *empty );
static void   on_selection_delete( ofaRecurrentModelTreeview *self, GtkTreeSelection *selection, void *empty );
static void   get_and_send( ofaRecurrentModelTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static GList *get_selected_with_selection( ofaRecurrentModelTreeview *self, GtkTreeSelection *selection );
static gint   tvbin_v_sort( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentModelTreeview, ofa_recurrent_model_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaRecurrentModelTreeview ))

static void
recurrent_model_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recurrent_model_treeview_finalize";
	ofaRecurrentModelTreeviewPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECURRENT_MODEL_TREEVIEW( instance ));

	/* free data members here */
	priv = ofa_recurrent_model_treeview_get_instance_private( OFA_RECURRENT_MODEL_TREEVIEW( instance ));

	g_free( priv->settings_prefix );

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
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
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
 * @getter: a #ofaIGetter instance.
 * @settings_prefix: the key prefix in user settings.
 *
 * Returns: a new #ofaRecurrentModelTreeview instance.
 */
ofaRecurrentModelTreeview *
ofa_recurrent_model_treeview_new( ofaIGetter *getter, const gchar *settings_prefix )
{
	ofaRecurrentModelTreeview *view;
	ofaRecurrentModelTreeviewPrivate *priv;
	gchar *str;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	view = g_object_new( OFA_TYPE_RECURRENT_MODEL_TREEVIEW,
				"ofa-tvbin-getter",  getter,
				"ofa-tvbin-selmode", GTK_SELECTION_MULTIPLE,
				"ofa-tvbin-shadow",  GTK_SHADOW_IN,
				NULL );

	priv = ofa_recurrent_model_treeview_get_instance_private( view );

	priv->getter = getter;

	if( my_strlen( settings_prefix )){
		str = g_strdup_printf( "%s-%s", settings_prefix, priv->settings_prefix );
		g_free( priv->settings_prefix );
		priv->settings_prefix = str;
	}

	ofa_tvbin_set_name( OFA_TVBIN( view ), priv->settings_prefix );

	setup_columns( view );

	ofa_tvbin_set_cell_data_func( OFA_TVBIN( view ), ( GtkTreeCellDataFunc ) on_cell_data_fn, view );

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

/*
 * Defines the treeview columns
 */
static void
setup_columns( ofaRecurrentModelTreeview *self )
{
	static const gchar *thisfn = "ofa_recurrent_model_treeview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_MNEMO,              _( "Mnemo" ),    _( "Mnemonic" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_CRE_USER,           _( "User" ),     _( "Creation user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), REC_MODEL_COL_CRE_STAMP,              NULL,        _( "Creation timestamp" ));
	ofa_tvbin_add_column_text_x ( OFA_TVBIN( self ), REC_MODEL_COL_LABEL,              _( "Label" ),        NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_OPE_TEMPLATE,       _( "Template" ), _( "Operation template" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_PERIOD_ID,          _( "Period." ),  _( "Periodicity identifier" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_PERIOD_ID_S,        _( "Period." ),  _( "Periodicity label" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_PERIOD_EVERY,       _( "Every" ),    _( "Periodicity every" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_PERIOD_DET_I,       _( "Detail" ),   _( "Periodicity details (as integers)" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_PERIOD_DET_S,       _( "Detail" ),   _( "Periodicity details (as labels)" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_DEF_AMOUNT1,        _( "Amount 1" ), _( "Updatable amount n° 1" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_DEF_AMOUNT2,        _( "Amount 2" ), _( "Updatable amount n° 2" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_DEF_AMOUNT3,        _( "Amount 3" ), _( "Updatable amount n° 3" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_MODEL_COL_ENABLED,            _( "Enabled" ),      NULL );
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), REC_MODEL_COL_END,                _( "End" ),      _( "End date" ));
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

		if( !ofo_recurrent_model_get_enabled( recmodel )){
			gdk_rgba_parse( &color, "#808080" );
			g_object_set( G_OBJECT( renderer ), "foreground-rgba", &color, NULL );
			g_object_set( G_OBJECT( renderer ), "style", PANGO_STYLE_ITALIC, NULL );
		}
	}
}

/**
 * ofa_recurrent_model_treeview_setup_store:
 * @view: this #ofaRecurrentModelTreeview instance.
 *
 * Initialize the underlying store.
 * Read the settings and show the columns accordingly.
 */
void
ofa_recurrent_model_treeview_setup_store( ofaRecurrentModelTreeview *view )
{
	ofaRecurrentModelTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_RECURRENT_MODEL_TREEVIEW( view ));

	priv = ofa_recurrent_model_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	if( !ofa_itvcolumnable_get_columns_count( OFA_ITVCOLUMNABLE( view ))){
		setup_columns( view );
	}

	priv->store = ofa_recurrent_model_store_new( priv->getter );
	ofa_tvbin_set_store( OFA_TVBIN( view ), GTK_TREE_MODEL( priv->store ));
	g_object_unref( priv->store );

	ofa_itvsortable_set_default_sort( OFA_ITVSORTABLE( view ), REC_MODEL_COL_MNEMO, GTK_SORT_ASCENDING );
}

static void
on_selection_changed( ofaRecurrentModelTreeview *self, GtkTreeSelection *selection, void *empty )
{
	//g_debug( "on_selection_changed" );
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
	//g_debug( "list_count=%d", g_list_length( list ));
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

/**
 * ofa_recurrent_model_treeview_unselect:
 * @view: this #ofaRecurrentModelTreeview instance.
 * @model: [allow-none]: a #ofoRecurrentModel object.
 *
 * Unselect the @model from the @view.
 * Unselect all if @model is %NULL.
 */
void
ofa_recurrent_model_treeview_unselect( ofaRecurrentModelTreeview *view, ofoRecurrentModel *model )
{
	ofaRecurrentModelTreeviewPrivate *priv;
	GtkTreeIter store_iter, tview_iter;
	GtkTreeSelection *selection;

	g_return_if_fail( view && OFA_IS_RECURRENT_MODEL_TREEVIEW( view ));
	g_return_if_fail( !model || OFO_IS_RECURRENT_MODEL( model ));

	priv = ofa_recurrent_model_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));

	if( model &&
			ofa_recurrent_model_store_get_iter( priv->store, model, &store_iter ) &&
			ofa_tvbin_store_iter_to_treeview_iter( OFA_TVBIN( view ), &store_iter, &tview_iter )){

		//g_debug( "gtk_tree_selection_unselect_iter" );
		gtk_tree_selection_unselect_iter( selection, &tview_iter );

	} else {
		gtk_tree_selection_unselect_all( selection );
	}
}

static gint
tvbin_v_sort( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_recurrent_model_treeview_v_sort";
	ofaRecurrentModelTreeviewPrivate *priv;
	gint cmp;
	gchar *mnemoa, *creusera, *crestampa, *labela, *templa, *perida, *peridsa, *perdetia, *perdetsa,
			*amount1a, *amount2a, *amount3a, *enaa, *enda, *notesa, *updusera, *updstampa;
	gchar *mnemob, *creuserb, *crestampb, *labelb, *templb, *peridb, *peridsb, *perdetib, *perdetsb,
			*amount1b, *amount2b, *amount3b, *enab, *endb, *notesb, *upduserb, *updstampb;
	GdkPixbuf *pnga, *pngb;
	guint perna, pernb;

	priv = ofa_recurrent_model_treeview_get_instance_private( OFA_RECURRENT_MODEL_TREEVIEW( tvbin ));

	gtk_tree_model_get( tmodel, a,
			REC_MODEL_COL_MNEMO,              &mnemoa,
			REC_MODEL_COL_CRE_USER,           &creusera,
			REC_MODEL_COL_CRE_STAMP,          &crestampa,
			REC_MODEL_COL_LABEL,              &labela,
			REC_MODEL_COL_OPE_TEMPLATE,       &templa,
			REC_MODEL_COL_PERIOD_ID,          &perida,
			REC_MODEL_COL_PERIOD_ID_S,        &peridsa,
			REC_MODEL_COL_PERIOD_EVERY_I,     &perna,
			REC_MODEL_COL_PERIOD_DET_I,       &perdetia,
			REC_MODEL_COL_PERIOD_DET_S,       &perdetsa,
			REC_MODEL_COL_DEF_AMOUNT1,        &amount1a,
			REC_MODEL_COL_DEF_AMOUNT2,        &amount2a,
			REC_MODEL_COL_DEF_AMOUNT3,        &amount3a,
			REC_MODEL_COL_ENABLED,            &enaa,
			REC_MODEL_COL_END,                &enda,
			REC_MODEL_COL_NOTES,              &notesa,
			REC_MODEL_COL_NOTES_PNG,          &pnga,
			REC_MODEL_COL_UPD_USER,           &updusera,
			REC_MODEL_COL_UPD_STAMP,          &updstampa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			REC_MODEL_COL_MNEMO,              &mnemob,
			REC_MODEL_COL_CRE_USER,           &creuserb,
			REC_MODEL_COL_CRE_STAMP,          &crestampb,
			REC_MODEL_COL_LABEL,              &labelb,
			REC_MODEL_COL_OPE_TEMPLATE,       &templb,
			REC_MODEL_COL_PERIOD_ID,          &peridb,
			REC_MODEL_COL_PERIOD_ID_S,        &peridsb,
			REC_MODEL_COL_PERIOD_EVERY_I,     &pernb,
			REC_MODEL_COL_PERIOD_DET_I,       &perdetib,
			REC_MODEL_COL_PERIOD_DET_S,       &perdetsb,
			REC_MODEL_COL_DEF_AMOUNT1,        &amount1b,
			REC_MODEL_COL_DEF_AMOUNT2,        &amount2b,
			REC_MODEL_COL_DEF_AMOUNT3,        &amount3b,
			REC_MODEL_COL_ENABLED,            &enab,
			REC_MODEL_COL_END,                &endb,
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
		case REC_MODEL_COL_CRE_USER:
			cmp = my_collate( creusera, creuserb );
			break;
		case REC_MODEL_COL_CRE_STAMP:
			cmp = my_collate( crestampa, crestampb );
			break;
		case REC_MODEL_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case REC_MODEL_COL_OPE_TEMPLATE:
			cmp = my_collate( templa, templb );
			break;
		case REC_MODEL_COL_PERIOD_ID:
			cmp = my_collate( perida, peridb );
			break;
		case REC_MODEL_COL_PERIOD_ID_S:
			cmp = my_collate( peridsa, peridsb );
			break;
		case REC_MODEL_COL_PERIOD_EVERY:
			cmp = perna < pernb ? -1 : ( perna > pernb ? 1 : 0 );
			break;
		case REC_MODEL_COL_PERIOD_DET_I:
			cmp = my_collate( perdetia, perdetib );
			break;
		case REC_MODEL_COL_PERIOD_DET_S:
			cmp = my_collate( perdetsa, perdetsb );
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
		case REC_MODEL_COL_ENABLED:
			cmp = my_collate( enaa, enab );
			break;
		case REC_MODEL_COL_END:
			cmp = my_date_compare_by_str( enda, endb, ofa_prefs_date_get_display_format( priv->getter ));
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
	g_free( creusera );
	g_free( crestampa );
	g_free( labela );
	g_free( templa );
	g_free( perida );
	g_free( peridsa );
	g_free( perdetia );
	g_free( perdetsa );
	g_free( amount1a );
	g_free( amount2a );
	g_free( amount3a );
	g_free( enaa );
	g_free( enda );
	g_free( notesa );
	g_free( updusera );
	g_free( updstampa );
	g_clear_object( &pnga );

	g_free( mnemob );
	g_free( creuserb );
	g_free( crestampb );
	g_free( labelb );
	g_free( templb );
	g_free( peridb );
	g_free( peridsb );
	g_free( perdetib );
	g_free( perdetsb );
	g_free( amount1b );
	g_free( amount2b );
	g_free( amount3b );
	g_free( enab );
	g_free( endb );
	g_free( notesb );
	g_free( upduserb );
	g_free( updstampb );
	g_clear_object( &pngb );

	return( cmp );
}
