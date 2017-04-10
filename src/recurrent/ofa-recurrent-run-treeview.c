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

#include "my/my-date.h"
#include "my/my-double-renderer.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvsortable.h"
#include "api/ofa-prefs.h"

#include "ofa-recurrent-run-store.h"
#include "ofa-recurrent-run-treeview.h"
#include "ofo-recurrent-model.h"
#include "ofo-recurrent-run.h"

/* private instance data
 */
typedef struct {
	gboolean     dispose_has_run;

	/* initialization
	 */
	ofaIGetter  *getter;
	gchar       *settings_prefix;

	/* runtime
	 */
	gint         visible;
	const GDate *from;
	const GDate *to;
}
	ofaRecurrentRunTreeviewPrivate;

#define RGBA_VALIDATED                  "#ffe8a8"		/* pale gold background */
#define RGBA_DELETED                    "#808080"		/* light gray foreground */

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void     setup_columns( ofaRecurrentRunTreeview *self );
static void     on_cell_data_func( GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaRecurrentRunTreeview *self );
static void     cell_data_render_background( ofaRecurrentRunTreeview *self, GtkCellRenderer *renderer, ofeRecurrentStatus status );
static void     cell_data_render_text( ofaRecurrentRunTreeview *self, GtkCellRendererText *renderer, ofeRecurrentStatus status );
static void     cell_data_set_editable( ofaRecurrentRunTreeview *self, GtkCellRenderer *renderer, GtkTreeViewColumn *column, ofoRecurrentModel *recmodel, ofoRecurrentRun *recrun, ofeRecurrentStatus status );
static void     on_cell_edited( GtkCellRendererText *cell, gchar *path_str, gchar *text, ofaRecurrentRunTreeview *self );
static void     on_selection_changed( ofaRecurrentRunTreeview *self, GtkTreeSelection *selection, void *empty );
static void     on_selection_activated( ofaRecurrentRunTreeview *self, GtkTreeSelection *selection, void *empty );
static void     on_selection_delete( ofaRecurrentRunTreeview *self, GtkTreeSelection *selection, void *empty );
static void     get_and_send( ofaRecurrentRunTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static GList   *get_selected_with_selection( ofaRecurrentRunTreeview *self, GtkTreeSelection *selection );
static gboolean tvbin_v_filter( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gint     tvbin_v_sort( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentRunTreeview, ofa_recurrent_run_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaRecurrentRunTreeview ))

static void
recurrent_run_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recurrent_run_treeview_finalize";
	ofaRecurrentRunTreeviewPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECURRENT_RUN_TREEVIEW( instance ));

	/* free data members here */
	priv = ofa_recurrent_run_treeview_get_instance_private( OFA_RECURRENT_RUN_TREEVIEW( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_run_treeview_parent_class )->finalize( instance );
}

static void
recurrent_run_treeview_dispose( GObject *instance )
{
	ofaRecurrentRunTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_RECURRENT_RUN_TREEVIEW( instance ));

	priv = ofa_recurrent_run_treeview_get_instance_private( OFA_RECURRENT_RUN_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_run_treeview_parent_class )->dispose( instance );
}

static void
ofa_recurrent_run_treeview_init( ofaRecurrentRunTreeview *self )
{
	static const gchar *thisfn = "ofa_recurrent_run_treeview_init";
	ofaRecurrentRunTreeviewPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECURRENT_RUN_TREEVIEW( self ));

	priv = ofa_recurrent_run_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->visible = REC_VISIBLE_NONE;
}

static void
ofa_recurrent_run_treeview_class_init( ofaRecurrentRunTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_recurrent_run_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_run_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_run_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->filter = tvbin_v_filter;
	OFA_TVBIN_CLASS( klass )->sort = tvbin_v_sort;

	/**
	 * ofaRecurrentRunTreeview::ofa-recchanged:
	 *
	 * #ofaTVBin sends a 'ofa-selchanged' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaRecurrentRunTreeview proxyes it with this 'ofa-recchanged'
	 * signal, providing the selected objects.
	 *
	 * Argument is the list of selected objects; may be %NULL.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaRecurrentRunTreeview *view,
	 * 						GList                 *list,
	 * 						gpointer               user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-recchanged",
				OFA_TYPE_RECURRENT_RUN_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );

	/**
	 * ofaRecurrentRunTreeview::ofa-recactivated:
	 *
	 * #ofaTVBin sends a 'ofa-selactivated' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaRecurrentRunTreeview proxyes it with this 'ofa-recactivated'
	 * signal, providing the selected objects.
	 *
	 * Argument is the list of selected objects.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaRecurrentRunTreeview *view,
	 * 						GList                 *list,
	 * 						gpointer               user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-recactivated",
				OFA_TYPE_RECURRENT_RUN_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );

	/**
	 * ofaRecurrentRunTreeview::ofa-recdelete:
	 *
	 * #ofaTVBin sends a 'ofa-seldelete' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaRecurrentRunTreeview proxyes it with this 'ofa-recdelete'
	 * signal, providing the selected objects.
	 *
	 * Argument is the list of selected objects.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaRecurrentRunTreeview *view,
	 * 						GList                 *list,
	 * 						gpointer               user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-recdelete",
				OFA_TYPE_RECURRENT_RUN_TREEVIEW,
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
 * ofa_recurrent_run_treeview_new:
 * @getter: a #ofaIGetter instance.
 * @settings_prefix: the key prefix in user settings.
 *
 * Returns: a new empty #ofaRecurrentRunTreeview composite object.
 *
 * Rationale: this same #ofaRecurrentRunTreeview class is used both by
 * the #ofaRecurrentRunPage and by the #ofaRecurrentNew dialog. This
 * later should not be updated when new operations are inserted.
 */
ofaRecurrentRunTreeview *
ofa_recurrent_run_treeview_new( ofaIGetter *getter, const gchar *settings_prefix )
{
	ofaRecurrentRunTreeview *view;
	ofaRecurrentRunTreeviewPrivate *priv;
	gchar *str;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	view = g_object_new( OFA_TYPE_RECURRENT_RUN_TREEVIEW,
					"ofa-tvbin-getter",  getter,
					"ofa-tvbin-selmode", GTK_SELECTION_MULTIPLE,
					"ofa-tvbin-shadow",  GTK_SHADOW_IN,
					NULL );

	priv = ofa_recurrent_run_treeview_get_instance_private( view );

	priv->getter = getter;

	if( my_strlen( settings_prefix )){
		str = g_strdup_printf( "%s-%s", settings_prefix, priv->settings_prefix );
		g_free( priv->settings_prefix );
		priv->settings_prefix = str;
	}

	ofa_tvbin_set_name( OFA_TVBIN( view ), priv->settings_prefix );

	setup_columns( view );

	ofa_tvbin_set_cell_data_func( OFA_TVBIN( view ), ( GtkTreeCellDataFunc ) on_cell_data_func, view );
	ofa_tvbin_set_cell_edited_func( OFA_TVBIN( view ), ( GCallback ) on_cell_edited, view );

	/* signals sent by ofaTVBin base class are intercepted to provide
	 * a #ofoRecurrentRun object instead of just the raw GtkTreeSelection
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
setup_columns( ofaRecurrentRunTreeview *self )
{
	static const gchar *thisfn = "ofa_recurrent_run_treeview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_RUN_COL_MNEMO,     _( "Mnemo" ),     _( "Mnemonic" ));
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), REC_RUN_COL_NUMSEQ,    _( "Seq." ),      _( "Sequence number" ));
	ofa_tvbin_add_column_text_x ( OFA_TVBIN( self ), REC_RUN_COL_LABEL,     _( "Label" ),         NULL );
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), REC_RUN_COL_DATE,      _( "Operation" ), _( "Operation date" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), REC_RUN_COL_STATUS,    _( "Status" ),        NULL );
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), REC_RUN_COL_AMOUNT1,   _( "Amount n° 1" ),   NULL);
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), REC_RUN_COL_AMOUNT2,   _( "Amount n° 2" ),   NULL);
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), REC_RUN_COL_AMOUNT3,   _( "Amount n° 3" ),   NULL);

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), REC_RUN_COL_LABEL );
}

/*
 * make amount cells editable on waiting operations
 * (and dossier is writable)
 */
static void
on_cell_data_func( GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaRecurrentRunTreeview *self )
{
	ofoRecurrentRun *recrun;
	ofoRecurrentModel *recmodel;
	ofeRecurrentStatus status;

	gtk_tree_model_get( tmodel, iter,
			REC_RUN_COL_OBJECT,   &recrun,
			REC_RUN_COL_MODEL,    &recmodel,
			REC_RUN_COL_STATUS_I, &status,
			-1 );
	g_return_if_fail( recrun && OFO_IS_RECURRENT_RUN( recrun ));
	g_object_unref( recrun );
	g_return_if_fail( recmodel && OFO_IS_RECURRENT_MODEL( recmodel ));
	g_object_unref( recmodel );

	cell_data_render_background( self, renderer, status );

	if( GTK_IS_CELL_RENDERER_TEXT( renderer )){
		cell_data_render_text( self, GTK_CELL_RENDERER_TEXT( renderer ), status );
	}

	cell_data_set_editable( self, renderer, column, recmodel, recrun, status );
}

static void
cell_data_render_background( ofaRecurrentRunTreeview *self, GtkCellRenderer *renderer, ofeRecurrentStatus status )
{
	GdkRGBA color;

	g_object_set( G_OBJECT( renderer ), "cell-background-set", FALSE, NULL );

	switch( status ){
		case REC_STATUS_VALIDATED:
			gdk_rgba_parse( &color, RGBA_VALIDATED );
			g_object_set( G_OBJECT( renderer ), "cell-background-rgba", &color, NULL );
			break;

		default:
			break;
	}
}

static void
cell_data_render_text( ofaRecurrentRunTreeview *self, GtkCellRendererText *renderer, ofeRecurrentStatus status )
{
	GdkRGBA color;

	g_return_if_fail( renderer && GTK_IS_CELL_RENDERER_TEXT( renderer ));

	g_object_set( G_OBJECT( renderer ),
						"style-set",      FALSE,
						"foreground-set", FALSE,
						NULL );

	switch( status ){
		case REC_STATUS_CANCELLED:
			gdk_rgba_parse( &color, RGBA_DELETED );
			g_object_set( G_OBJECT( renderer ), "foreground-rgba", &color, NULL );
			g_object_set( G_OBJECT( renderer ), "style", PANGO_STYLE_ITALIC, NULL );
			break;

		default:
			break;
	}
}

static void
cell_data_set_editable( ofaRecurrentRunTreeview *self, GtkCellRenderer *renderer, GtkTreeViewColumn *column,
								ofoRecurrentModel *recmodel, ofoRecurrentRun *recrun, ofeRecurrentStatus status )
{
	ofaRecurrentRunTreeviewPrivate *priv;
	guint column_id;
	const gchar *csdef;
	gboolean editable;
	ofaHub *hub;

	priv = ofa_recurrent_run_treeview_get_instance_private( self );

	column_id = ofa_itvcolumnable_get_column_id( OFA_ITVCOLUMNABLE( self ), column );

	switch( column_id ){
		case REC_RUN_COL_AMOUNT1:
			csdef = ofo_recurrent_model_get_def_amount1( recmodel );
			break;
		case REC_RUN_COL_AMOUNT2:
			csdef = ofo_recurrent_model_get_def_amount2( recmodel );
			break;
		case REC_RUN_COL_AMOUNT3:
			csdef = ofo_recurrent_model_get_def_amount3( recmodel );
			break;
		default:
			csdef = NULL;
			break;
	}

	switch( column_id ){
		/* only waiting operations are editable */
		case REC_RUN_COL_AMOUNT1:
		case REC_RUN_COL_AMOUNT2:
		case REC_RUN_COL_AMOUNT3:
			hub = ofa_igetter_get_hub( priv->getter );
			editable = ofa_hub_is_writable_dossier( hub );
			editable &= ( my_strlen( csdef ) > 0 );
			editable &= ( status == REC_STATUS_WAITING );
			g_object_set( G_OBJECT( renderer ), "editable-set", TRUE, "editable", editable, NULL );
			break;
		default:
			break;
	}
}

static void
on_cell_edited( GtkCellRendererText *cell, gchar *path_str, gchar *text, ofaRecurrentRunTreeview *self )
{
	ofaRecurrentRunTreeviewPrivate *priv;
	gint column_id;
	GtkTreePath *path;
	GtkTreeModel *sort_model, *filter_model, *store;
	GtkTreeIter sort_iter, filter_iter, iter;
	gchar *str;
	gdouble amount;
	ofoRecurrentRun *recrun;

	priv = ofa_recurrent_run_treeview_get_instance_private( self );

	sort_model = ofa_tvbin_get_tree_model( OFA_TVBIN( self ));
	if( sort_model ){
		filter_model = gtk_tree_model_sort_get_model( GTK_TREE_MODEL_SORT( sort_model ));
		store = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( filter_model ));

		amount = 0;
		path = gtk_tree_path_new_from_string( path_str );
		if( gtk_tree_model_get_iter( sort_model, &sort_iter, path )){

			gtk_tree_model_sort_convert_iter_to_child_iter(
					GTK_TREE_MODEL_SORT( sort_model ), &filter_iter, &sort_iter );
			gtk_tree_model_filter_convert_iter_to_child_iter(
					GTK_TREE_MODEL_FILTER( filter_model ), &iter, &filter_iter );

			gtk_tree_model_get( GTK_TREE_MODEL( store ), &iter, REC_RUN_COL_OBJECT, &recrun, -1 );
			g_return_if_fail( recrun && OFO_IS_RECURRENT_RUN( recrun ));
			g_object_unref( recrun );

			column_id = ofa_itvcolumnable_get_column_id_renderer( OFA_ITVCOLUMNABLE( self ), GTK_CELL_RENDERER( cell ));

			switch( column_id ){
				case REC_RUN_COL_AMOUNT1:
				case REC_RUN_COL_AMOUNT2:
				case REC_RUN_COL_AMOUNT3:
					/* reformat amounts before storing them */
					amount = ofa_amount_from_str( text, priv->getter );
					str = ofa_amount_to_str( amount, NULL, priv->getter );
					gtk_list_store_set( GTK_LIST_STORE( store ), &iter, column_id, str, -1 );
					g_free( str );
					break;
				default:
					break;
			}

			switch( column_id ){
				case REC_RUN_COL_AMOUNT1:
					ofo_recurrent_run_set_amount1( recrun, amount );
					break;
				case REC_RUN_COL_AMOUNT2:
					ofo_recurrent_run_set_amount2( recrun, amount );
					break;
				case REC_RUN_COL_AMOUNT3:
					ofo_recurrent_run_set_amount3( recrun, amount );
					break;
				default:
					break;
			}

			switch( column_id ){
				case REC_RUN_COL_AMOUNT1:
				case REC_RUN_COL_AMOUNT2:
				case REC_RUN_COL_AMOUNT3:
					ofo_recurrent_run_update( recrun );
					break;
				default:
					break;
			}
		}
		gtk_tree_path_free( path );
	}
}

static void
on_selection_changed( ofaRecurrentRunTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-recchanged" );
}

static void
on_selection_activated( ofaRecurrentRunTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-recactivated" );
}

/*
 * Delete key pressed
 * ofaTVBin base class makes sure the selection is not empty.
 */
static void
on_selection_delete( ofaRecurrentRunTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-recdelete" );
}

/*
 * gtk_tree_selection_get_selected_rows() works even if selection mode
 * is GTK_SELECTION_MULTIPLE (which may happen here)
 */
static void
get_and_send( ofaRecurrentRunTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	GList *list;

	list = get_selected_with_selection( self, selection );
	g_signal_emit_by_name( self, signal, list );
	ofa_recurrent_run_treeview_free_selected( list );
}

/**
 * ofa_recurrent_run_treeview_get_visible:
 * @view: this #ofaRecurrentRunTreeview instance.
 *
 * Returns: the Or'ed visibility status.
 */
gint
ofa_recurrent_run_treeview_get_visible( ofaRecurrentRunTreeview *view )
{
	ofaRecurrentRunTreeviewPrivate *priv;

	g_return_val_if_fail( view && OFA_IS_RECURRENT_RUN_TREEVIEW( view ), 0 );

	priv = ofa_recurrent_run_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	return( priv->visible );
}

/**
 * ofa_recurrent_run_treeview_set_visible:
 * @view: this #ofaRecurrentRunTreeview instance.
 * @visible: the Or'ed visibility status.
 *
 * Set the visibility status.
 */
void
ofa_recurrent_run_treeview_set_visible( ofaRecurrentRunTreeview *view, gint visible )
{
	ofaRecurrentRunTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_RECURRENT_RUN_TREEVIEW( view ));

	priv = ofa_recurrent_run_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	priv->visible = visible;

	ofa_tvbin_refilter( OFA_TVBIN( view ));
}

/**
 * ofa_recurrent_run_treeview_set_ope_date:
 * @view: this #ofaRecurrentRunTreeview instance.
 * @from: [allow-none]: the minimal operation date.
 * @to: [allow-none]: the maximal operation date.
 *
 * Set the operation date filter.
 */
void
ofa_recurrent_run_treeview_set_ope_date( ofaRecurrentRunTreeview *view, const GDate *from, const GDate *to )
{
	ofaRecurrentRunTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_RECURRENT_RUN_TREEVIEW( view ));

	priv = ofa_recurrent_run_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	priv->from = from;
	priv->to = to;

	ofa_tvbin_refilter( OFA_TVBIN( view ));
}

/**
 * ofa_recurrent_run_treeview_get_selected:
 *
 * Returns: the list of #ofoRecurrentRun selected objects.
 *
 * The returned list should be ofa_recurrent_run_treeview_free_selected() by
 * the caller.
 */
GList *
ofa_recurrent_run_treeview_get_selected( ofaRecurrentRunTreeview *view )
{
	ofaRecurrentRunTreeviewPrivate *priv;
	GtkTreeSelection *selection;

	g_return_val_if_fail( view && OFA_IS_RECURRENT_RUN_TREEVIEW( view ), NULL );

	priv = ofa_recurrent_run_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));

	return( get_selected_with_selection( view, selection ));
}

/*
 * gtk_tree_selection_get_selected_rows() works even if selection mode
 * is GTK_SELECTION_MULTIPLE (which is the default here)
 */
static GList *
get_selected_with_selection( ofaRecurrentRunTreeview *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GList *selected_rows, *irow, *selected_objects;
	ofoRecurrentRun *run;

	selected_objects = NULL;
	selected_rows = gtk_tree_selection_get_selected_rows( selection, &tmodel );
	for( irow=selected_rows ; irow ; irow=irow->next ){
		if( gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) irow->data )){
			gtk_tree_model_get( tmodel, &iter, REC_RUN_COL_OBJECT, &run, -1 );
			g_return_val_if_fail( run && OFO_IS_RECURRENT_RUN( run ), NULL );
			selected_objects = g_list_prepend( selected_objects, run );
		}
	}

	g_list_free_full( selected_rows, ( GDestroyNotify ) gtk_tree_path_free );

	return( selected_objects );
}

/**
 * ofa_recurrent_run_treeview_unselect:
 * @view: this #ofaRecurrentRunTreeview instance.
 * @run: [allow-none]: a #ofoRecurrentRun object.
 *
 * Unselect the @run from the @view.
 */
void
ofa_recurrent_run_treeview_unselect( ofaRecurrentRunTreeview *view, ofoRecurrentRun *run )
{
	ofaRecurrentRunTreeviewPrivate *priv;
	GtkTreeIter store_iter, tview_iter;
	GtkTreeSelection *selection;
	GtkTreeModel *store;

	g_return_if_fail( view && OFA_IS_RECURRENT_RUN_TREEVIEW( view ));
	g_return_if_fail( !run || OFO_IS_RECURRENT_RUN( run ));

	priv = ofa_recurrent_run_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	store = ofa_tvbin_get_store( OFA_TVBIN( view ));
	g_return_if_fail( store && OFA_IS_RECURRENT_RUN_STORE( store ));

	if( run &&
			ofa_recurrent_run_store_get_iter( OFA_RECURRENT_RUN_STORE( store ), run, &store_iter ) &&
			ofa_tvbin_store_iter_to_treeview_iter( OFA_TVBIN( view ), &store_iter, &tview_iter )){

		selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));
		gtk_tree_selection_unselect_iter( selection, &tview_iter );
	}
}

static gboolean
tvbin_v_filter( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	ofaRecurrentRunTreeviewPrivate *priv;
	gboolean visible;
	ofoRecurrentRun *object;
	ofeRecurrentStatus status;
	const GDate *dope;

	priv = ofa_recurrent_run_treeview_get_instance_private( OFA_RECURRENT_RUN_TREEVIEW( tvbin ));

	visible = FALSE;

	gtk_tree_model_get( tmodel, iter, REC_RUN_COL_OBJECT, &object, -1 );
	if( object ){
		g_return_val_if_fail( OFO_IS_RECURRENT_RUN( object ), FALSE );
		g_object_unref( object );

		status = ofo_recurrent_run_get_status( object );
		dope = ofo_recurrent_run_get_date( object );

		switch( status ){
			case REC_STATUS_CANCELLED:
				visible = priv->visible & REC_VISIBLE_CANCELLED;
				break;
			case REC_STATUS_WAITING:
				visible = priv->visible & REC_VISIBLE_WAITING;
				break;
			case REC_STATUS_VALIDATED:
				visible = priv->visible & REC_VISIBLE_VALIDATED;
				break;
			default:
				break;
		}

		if( visible && my_date_is_valid( priv->from )){
			visible = my_date_compare( priv->from, dope ) <= 0;
		}

		if( visible && my_date_is_valid( priv->to )){
			visible = my_date_compare( dope, priv->to ) <= 0;
		}
	}

	return( visible );
}

static gint
tvbin_v_sort( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_recurrent_run_treeview_v_sort";
	ofaRecurrentRunTreeviewPrivate *priv;
	gchar *mnemoa, *labela, *seqa, *datea, *statusa, *amount1a, *amount2a, *amount3a;
	gchar *mnemob, *labelb, *seqb, *dateb, *statusb, *amount1b, *amount2b, *amount3b;
	gint cmp;

	priv = ofa_recurrent_run_treeview_get_instance_private( OFA_RECURRENT_RUN_TREEVIEW( tvbin ));

	gtk_tree_model_get( tmodel, a,
			REC_RUN_COL_MNEMO,   &mnemoa,
			REC_RUN_COL_NUMSEQ,  &seqa,
			REC_RUN_COL_LABEL,   &labela,
			REC_RUN_COL_DATE,    &datea,
			REC_RUN_COL_STATUS,  &statusa,
			REC_RUN_COL_AMOUNT1, &amount1a,
			REC_RUN_COL_AMOUNT2, &amount2a,
			REC_RUN_COL_AMOUNT3, &amount3a,
			-1 );

	gtk_tree_model_get( tmodel, b,
			REC_RUN_COL_MNEMO,   &mnemob,
			REC_RUN_COL_NUMSEQ,  &seqb,
			REC_RUN_COL_LABEL,   &labelb,
			REC_RUN_COL_DATE,    &dateb,
			REC_RUN_COL_STATUS,  &statusb,
			REC_RUN_COL_AMOUNT1, &amount1b,
			REC_RUN_COL_AMOUNT2, &amount2b,
			REC_RUN_COL_AMOUNT3, &amount3b,
			-1 );

	cmp = 0;

	switch( column_id ){
		case REC_RUN_COL_MNEMO:
			cmp = my_collate( mnemoa, mnemob );
			break;
		case REC_RUN_COL_NUMSEQ:
			cmp = ofa_itvsortable_sort_str_int( seqa, seqb );
			break;
		case REC_RUN_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case REC_RUN_COL_DATE:
			cmp = my_date_compare_by_str( datea, dateb, ofa_prefs_date_get_display_format( priv->getter ));
			break;
		case REC_RUN_COL_STATUS:
			cmp = my_collate( statusa, statusb );
			break;
		case REC_RUN_COL_AMOUNT1:
			cmp = ofa_itvsortable_sort_str_amount( OFA_ITVSORTABLE( tvbin ), amount1a, amount1b );
			break;
		case REC_RUN_COL_AMOUNT2:
			cmp = ofa_itvsortable_sort_str_amount( OFA_ITVSORTABLE( tvbin ), amount2a, amount2b );
			break;
		case REC_RUN_COL_AMOUNT3:
			cmp = ofa_itvsortable_sort_str_amount( OFA_ITVSORTABLE( tvbin ), amount3a, amount3b );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( mnemoa );
	g_free( seqa );
	g_free( labela );
	g_free( datea );
	g_free( statusa );
	g_free( amount1a );
	g_free( amount2a );
	g_free( amount3a );

	g_free( mnemob );
	g_free( seqb );
	g_free( labelb );
	g_free( dateb );
	g_free( statusb );
	g_free( amount1b );
	g_free( amount2b );
	g_free( amount3b );

	return( cmp );
}
