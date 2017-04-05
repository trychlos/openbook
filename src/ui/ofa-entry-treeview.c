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
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvfilterable.h"
#include "api/ofa-itvsortable.h"
#include "api/ofa-prefs.h"
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "ui/ofa-entry-store.h"
#include "ui/ofa-entry-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean                      dispose_has_run;

	/* initialization
	 */
	ofaIGetter                   *getter;
	gchar                        *settings_prefix;

	/* runtime
	 */
	GtkTreeModelFilterVisibleFunc filter_fn;
	void                         *filter_data;

	/* twin columns
	 */
	GtkTreeViewColumn            *debit_column;
	GtkTreeViewColumn            *credit_column;
}
	ofaEntryTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

/* when editing an entry, we may have two levels of errors:
 * fatal error: the entry is not valid and cannot be saved
 *              (e.g. a mandatory data is empty)
 * warning: the entry may be valid, but will not be applied in standard
 *          conditions (e.g. effect date is before the exercice)
 */
#define RGBA_NORMAL                     "#000000"		/* black */
#define RGBA_ERROR                      "#ff0000"		/* full red */
#define RGBA_WARNING                    "#ff8000"		/* orange */

/* status colors */
#define RGBA_PAST                       "#d8ffa0"		/* green background */
#define RGBA_VALIDATED                  "#ffe8a8"		/* pale gold background */
#define RGBA_DELETED                    "#808080"		/* gray foreground */
#define RGBA_FUTURE                     "#c0ffff"		/* pale blue background */

static void      setup_columns( ofaEntryTreeview *self );
static void      on_selection_changed( ofaEntryTreeview *self, GtkTreeSelection *selection, void *empty );
static void      on_selection_activated( ofaEntryTreeview *self, GtkTreeSelection *selection, void *empty );
static void      on_selection_delete( ofaEntryTreeview *self, GtkTreeSelection *selection, void *empty );
static void      get_and_send( ofaEntryTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static GList    *get_selected_with_selection( ofaEntryTreeview *self, GtkTreeSelection *selection );
static void      cell_data_render_background( GtkCellRenderer *renderer, ofeEntryStatus status, gint err_level );
static void      cell_data_render_text( GtkCellRendererText *renderer, ofeEntryStatus status, gint err_level );
static gint      get_row_errlevel( ofaEntryTreeview *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean  tvbin_v_filter( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gint      tvbin_v_sort( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaEntryTreeview, ofa_entry_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaEntryTreeview ))

static void
entry_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_entry_treeview_finalize";
	ofaEntryTreeviewPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ENTRY_TREEVIEW( instance ));

	/* free data members here */
	priv = ofa_entry_treeview_get_instance_private( OFA_ENTRY_TREEVIEW( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_entry_treeview_parent_class )->finalize( instance );
}

static void
entry_treeview_dispose( GObject *instance )
{
	ofaEntryTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ENTRY_TREEVIEW( instance ));

	priv = ofa_entry_treeview_get_instance_private( OFA_ENTRY_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_entry_treeview_parent_class )->dispose( instance );
}

static void
ofa_entry_treeview_init( ofaEntryTreeview *self )
{
	static const gchar *thisfn = "ofa_entry_treeview_init";
	ofaEntryTreeviewPrivate *priv;

	g_return_if_fail( self && OFA_IS_ENTRY_TREEVIEW( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_entry_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->filter_fn = NULL;
	priv->filter_data = NULL;
}

static void
ofa_entry_treeview_class_init( ofaEntryTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_entry_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = entry_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = entry_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->filter = tvbin_v_filter;
	OFA_TVBIN_CLASS( klass )->sort = tvbin_v_sort;

	/**
	 * ofaEntryTreeview::ofa-entchanged:
	 *
	 * #ofaTVBin sends a 'ofa-selchanged' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaEntryTreeview proxyes it with this 'ofa-entchanged' signal,
	 * signal, providing the selected objects.
	 *
	 * Argument is the list of selected objects; may be %NULL.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaEntryTreeview *view,
	 * 						GList          *list,
	 * 						gpointer        user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-entchanged",
				OFA_TYPE_ENTRY_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );

	/**
	 * ofaEntryTreeview::ofa-entactivated:
	 *
	 * #ofaTVBin sends a 'ofa-selactivated' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaEntryTreeview proxyes it with this 'ofa-entactivated' signal,
	 * providing the selected objects.
	 *
	 * Argument is the list of selected objects.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaEntryTreeview *view,
	 * 						GList          *list,
	 * 						gpointer        user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-entactivated",
				OFA_TYPE_ENTRY_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );

	/**
	 * ofaEntryTreeview::ofa-entdelete:
	 *
	 * #ofaTVBin sends a 'ofa-seldelete' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaEntryTreeview proxyes it with this 'ofa-entdelete' signal,
	 * providing the selected objects.
	 *
	 * Argument is the list of selected objects.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaEntryTreeview *view,
	 * 						GList          *list,
	 * 						gpointer        user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-entdelete",
				OFA_TYPE_ENTRY_TREEVIEW,
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
 * ofa_entry_treeview_new:
 * @getter: a #ofaIGetter instance.
 * @settings_prefix: the key prefix in user settings.
 *
 * Returns: a new #ofaEntryTreeview instance.
 */
ofaEntryTreeview *
ofa_entry_treeview_new( ofaIGetter *getter, const gchar *settings_prefix )
{
	ofaEntryTreeview *view;
	ofaEntryTreeviewPrivate *priv;
	gchar *str;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	view = g_object_new( OFA_TYPE_ENTRY_TREEVIEW,
				"ofa-tvbin-getter",  getter,
				"ofa-tvbin-selmode", GTK_SELECTION_MULTIPLE,
				"ofa-tvbin-shadow",  GTK_SHADOW_IN,
				NULL );

	priv = ofa_entry_treeview_get_instance_private( view );

	priv->getter = getter;

	if( my_strlen( settings_prefix )){
		str = g_strdup_printf( "%s-%s", settings_prefix, priv->settings_prefix );
		g_free( priv->settings_prefix );
		priv->settings_prefix = str;
	}

	ofa_tvbin_set_name( OFA_TVBIN( view ), priv->settings_prefix );

	/* signals sent by ofaTVBin base class are intercepted to provide
	 * a #ofoEntry object instead of just the raw GtkTreeSelection
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
 * ofa_entry_treeview_setup_columns:
 * @view: this #ofaEntryTreeview instance.
 *
 * Setup the treeview columns.
 */
void
ofa_entry_treeview_setup_columns( ofaEntryTreeview *view )
{
	ofaEntryTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_ENTRY_TREEVIEW( view ));

	priv = ofa_entry_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	setup_columns( view );
}

/*
 * Defines the treeview columns
 */
static void
setup_columns( ofaEntryTreeview *self )
{
	static const gchar *thisfn = "ofa_entry_treeview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), ENTRY_COL_DOPE,          _( "Ope." ),        _( "Operation date" ));
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), ENTRY_COL_DEFFECT,       _( "Effect" ),      _( "Effect date" ));
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), ENTRY_COL_LABEL,         _( "Label" ),           NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ENTRY_COL_REF,           _( "Ref." ),        _( "Piece reference" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ENTRY_COL_CURRENCY,      _( "Currency" ),        NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ENTRY_COL_LEDGER,        _( "Ledger" ),          NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ENTRY_COL_OPE_TEMPLATE,  _( "Template" ),    _( "Operation template" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ENTRY_COL_ACCOUNT,       _( "Account" ),         NULL );
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), ENTRY_COL_DEBIT,         _( "Debit" ),           NULL );
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), ENTRY_COL_CREDIT,        _( "Credit" ),          NULL );
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), ENTRY_COL_OPE_NUMBER,    _( "Ope." ),        _( "Operation number" ));
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), ENTRY_COL_STLMT_NUMBER,  _( "Set.num" ),     _( "Settlement number" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ENTRY_COL_STLMT_USER,    _( "Set.user" ),    _( "Settlement user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), ENTRY_COL_STLMT_STAMP,   _( "Set.stamp" ),   _( "Settlement timestamp" ));
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), ENTRY_COL_ENT_NUMBER,    _( "Ent.num" ),     _( "Entry number" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ENTRY_COL_UPD_USER,      _( "Ent.user" ),    _( "Last update user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), ENTRY_COL_UPD_STAMP,     _( "Ent.stamp" ),   _( "Last update timestamp" ));
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), ENTRY_COL_CONCIL_NUMBER, _( "Concil.num" ),  _( "Conciliation number" ));
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), ENTRY_COL_CONCIL_DATE,   _( "Concil.date" ), _( "Conciliation date" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ENTRY_COL_STATUS,        _( "Status" ),      _( "Status" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ENTRY_COL_RULE,          _( "Rule" ),            NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ENTRY_COL_NOTES,         _( "Notes" ),           NULL );
	ofa_tvbin_add_column_pixbuf ( OFA_TVBIN( self ), ENTRY_COL_NOTES_PNG,        "",              _( "Notes indicator" ));

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), ENTRY_COL_LABEL );
	ofa_itvcolumnable_twins_group_new( OFA_ITVCOLUMNABLE( self ), "amount", ENTRY_COL_DEBIT, ENTRY_COL_CREDIT, -1 );
}

/**
 * ofa_entry_treeview_set_filter:
 * @view: this #ofaEntryTreeview instance.
 * @filter_fn: an external filter function.
 * @filter_data: the data to be passed to the @filter_fn function.
 *
 * Setup the filtering function.
 */
void
ofa_entry_treeview_set_filter_func( ofaEntryTreeview *view, GtkTreeModelFilterVisibleFunc filter_fn, void *filter_data )
{
	ofaEntryTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_ENTRY_TREEVIEW( view ));

	priv = ofa_entry_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	priv->filter_fn = filter_fn;
	priv->filter_data = filter_data;
}

static void
on_selection_changed( ofaEntryTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-entchanged" );
}

static void
on_selection_activated( ofaEntryTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-entactivated" );
}

/*
 * Delete key pressed
 * ofaTVBin base class makes sure the selection is not empty.
 */
static void
on_selection_delete( ofaEntryTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-entdelete" );
}

/*
 * Entry may be %NULL when selection is empty (on 'ofa-entchanged' signal)
 */
static void
get_and_send( ofaEntryTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	GList *list;

	list = get_selected_with_selection( self, selection );
	g_signal_emit_by_name( self, signal, list );
	ofa_entry_treeview_free_selected( list );
}

/**
 * ofa_entry_treeview_get_selected:
 * @view: this #ofaEntryTreeview instance.
 *
 * Returns: the list of selected objects, which may be %NULL.
 *
 * The returned list should be ofa_entry_treeview_free_selected()
 * by the caller.
 */
GList *
ofa_entry_treeview_get_selected( ofaEntryTreeview *view )
{
	static const gchar *thisfn = "ofa_entry_treeview_get_selected";
	ofaEntryTreeviewPrivate *priv;
	GtkTreeSelection *selection;

	g_debug( "%s: view=%p", thisfn, ( void * ) view );

	g_return_val_if_fail( view && OFA_IS_ENTRY_TREEVIEW( view ), NULL );

	priv = ofa_entry_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));

	return( get_selected_with_selection( view, selection ));
}

/*
 * get_selected_with_selection:
 * @view: this #ofaEntryTreeview instance.
 * @selection: the current #GtkTreeSelection.
 *
 * Return: the list of selected objects, or %NULL.
 */
static GList *
get_selected_with_selection( ofaEntryTreeview *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GList *selected_rows, *irow, *selected_objects;
	ofoEntry *entry;

	selected_objects = NULL;
	selected_rows = gtk_tree_selection_get_selected_rows( selection, &tmodel );
	for( irow=selected_rows ; irow ; irow=irow->next ){
		if( gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) irow->data )){
			gtk_tree_model_get( tmodel, &iter, ENTRY_COL_OBJECT, &entry, -1 );
			g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), NULL );
			selected_objects = g_list_prepend( selected_objects, entry );
		}
	}

	g_list_free_full( selected_rows, ( GDestroyNotify ) gtk_tree_path_free );

	return( selected_objects );
}

/**
 * ofa_entry_treeview_set_selected:
 * @view: this #ofaEntryTreeview instance.
 * @entry: the number of the entry to be selected.
 *
 * Selects the entry identified by @entry.
 */
void
ofa_entry_treeview_set_selected( ofaEntryTreeview *view, ofxCounter entry )
{
	static const gchar *thisfn = "ofa_entry_treeview_set_selected";
	ofaEntryTreeviewPrivate *priv;
	GtkWidget *treeview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofxCounter row_number;

	g_debug( "%s: view=%p, entry=%lu", thisfn, ( void * ) view, entry );

	g_return_if_fail( view && OFA_IS_ENTRY_TREEVIEW( view ));

	priv = ofa_entry_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	treeview = ofa_tvbin_get_tree_view( OFA_TVBIN( view ));
	if( treeview ){
		tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( treeview ));
		if( gtk_tree_model_get_iter_first( tmodel, &iter )){
			while( TRUE ){
				gtk_tree_model_get( tmodel, &iter, ENTRY_COL_ENT_NUMBER_I, &row_number, -1 );
				if( row_number == entry ){
					ofa_tvbin_select_row( OFA_TVBIN( view ), &iter );
					break;
				}
				if( !gtk_tree_model_iter_next( tmodel, &iter )){
					break;
				}
			}
		}
	}
}

/**
 * ofa_entry_treeview_cell_data_render:
 * @view: this #ofaEntryTreeview instance.
 * @column: the #GtkTreeViewColumn treeview colum.
 * @renderer: a #GtkCellRenderer attached to the column.
 * @model: the #GtkTreeModel of the treeview.
 * @iter: the #GtkTreeIter which addresses the row.
 *
 * Paints the row.
 *
 * Foreground and background colors only depend of entry status and
 * maybe of the error level.
 */
void
ofa_entry_treeview_cell_data_render( ofaEntryTreeview *view,
				GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter )
{
	ofaEntryTreeviewPrivate *priv;
	ofeEntryStatus status;
	gint err_level;

	g_return_if_fail( view && OFA_IS_ENTRY_TREEVIEW( view ));
	g_return_if_fail( column && GTK_IS_TREE_VIEW_COLUMN( column ));
	g_return_if_fail( renderer && GTK_IS_CELL_RENDERER( renderer ));
	g_return_if_fail( model && GTK_IS_TREE_MODEL( model ));

	priv = ofa_entry_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	err_level = get_row_errlevel( view, model, iter );
	gtk_tree_model_get( model, iter, ENTRY_COL_STATUS_I, &status, -1 );

	cell_data_render_background( renderer, status, err_level );

	if( GTK_IS_CELL_RENDERER_TEXT( renderer )){
		cell_data_render_text( GTK_CELL_RENDERER_TEXT( renderer ), status, err_level );
	}
}

static void
cell_data_render_background( GtkCellRenderer *renderer, ofeEntryStatus status, gint err_level )
{
	GdkRGBA color;

	g_object_set( G_OBJECT( renderer ), "cell-background-set", FALSE, NULL );

	switch( status ){

		case ENT_STATUS_PAST:
			gdk_rgba_parse( &color, RGBA_PAST );
			g_object_set( G_OBJECT( renderer ), "cell-background-rgba", &color, NULL );
			break;

		case ENT_STATUS_VALIDATED:
			gdk_rgba_parse( &color, RGBA_VALIDATED );
			g_object_set( G_OBJECT( renderer ), "cell-background-rgba", &color, NULL );
			break;

		case ENT_STATUS_FUTURE:
			gdk_rgba_parse( &color, RGBA_FUTURE );
			g_object_set( G_OBJECT( renderer ), "cell-background-rgba", &color, NULL );
			break;

		default:
			break;
	}
}

static void
cell_data_render_text( GtkCellRendererText *renderer, ofeEntryStatus status, gint err_level )
{
	GdkRGBA color;
	const gchar *color_str;

	g_return_if_fail( renderer && GTK_IS_CELL_RENDERER_TEXT( renderer ));

	g_object_set( G_OBJECT( renderer ),
						"style-set",      FALSE,
						"foreground-set", FALSE,
						NULL );

	switch( status ){

		case ENT_STATUS_DELETED:
			gdk_rgba_parse( &color, RGBA_DELETED );
			g_object_set( G_OBJECT( renderer ), "foreground-rgba", &color, NULL );
			g_object_set( G_OBJECT( renderer ), "style", PANGO_STYLE_ITALIC, NULL );
			break;

		case ENT_STATUS_ROUGH:
			switch( err_level ){
				case ENTRY_ERR_ERROR:
					color_str = RGBA_ERROR;
					break;
				case ENTRY_ERR_WARNING:
					color_str = RGBA_WARNING;
					break;
				default:
					color_str = RGBA_NORMAL;
					break;
			}
			gdk_rgba_parse( &color, color_str );
			g_object_set( G_OBJECT( renderer ), "foreground-rgba", &color, NULL );
			break;

		default:
			break;
	}
}

static gint
get_row_errlevel( ofaEntryTreeview *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gchar *msgerr, *msgwarn;
	gint err_level;

	gtk_tree_model_get( tmodel, iter, ENTRY_COL_MSGERR, &msgerr, ENTRY_COL_MSGWARN, &msgwarn, -1 );

	if( my_strlen( msgerr )){
		err_level = ENTRY_ERR_ERROR;

	} else if( my_strlen( msgwarn )){
		err_level = ENTRY_ERR_WARNING;

	} else {
		err_level = ENTRY_ERR_NONE;
	}

	g_free( msgerr );
	g_free( msgwarn );

	return( err_level );
}

static gboolean
tvbin_v_filter( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	ofaEntryTreeviewPrivate *priv;

	priv = ofa_entry_treeview_get_instance_private( OFA_ENTRY_TREEVIEW( tvbin ));

	return( priv->filter_fn ? ( *priv->filter_fn )( tmodel, iter, priv->filter_data ) : TRUE );
}

static gint
tvbin_v_sort( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_entry_treeview_v_sort";
	ofaEntryTreeviewPrivate *priv;
	gint cmp;
	gchar *dopea, *deffa, *labela, *refa, *cura, *ledgera, *templatea, *accounta, *debita, *credita, *openuma,
			*stlmtnuma, *stlmtusera, *stlmtstampa, *entnuma, *updusera, *updstampa, *concilnuma, *concildatea,
			*statusa, *rulea, *notesa;
	gchar *dopeb, *deffb, *labelb, *refb, *curb, *ledgerb, *templateb, *accountb, *debitb, *creditb, *openumb,
			*stlmtnumb, *stlmtuserb, *stlmtstampb, *entnumb, *upduserb, *updstampb, *concilnumb, *concildateb,
			*statusb, *ruleb, *notesb;
	GdkPixbuf *pnga, *pngb;

	priv = ofa_entry_treeview_get_instance_private( OFA_ENTRY_TREEVIEW( tvbin ));

	gtk_tree_model_get( tmodel, a,
			ENTRY_COL_DOPE,          &dopea,
			ENTRY_COL_DEFFECT,       &deffa,
			ENTRY_COL_LABEL,         &labela,
			ENTRY_COL_REF,           &refa,
			ENTRY_COL_CURRENCY,      &cura,
			ENTRY_COL_LEDGER,        &ledgera,
			ENTRY_COL_OPE_TEMPLATE,  &templatea,
			ENTRY_COL_ACCOUNT,       &accounta,
			ENTRY_COL_DEBIT,         &debita,
			ENTRY_COL_CREDIT,        &credita,
			ENTRY_COL_OPE_NUMBER,    &openuma,
			ENTRY_COL_STLMT_NUMBER,  &stlmtnuma,
			ENTRY_COL_STLMT_USER,    &stlmtusera,
			ENTRY_COL_STLMT_STAMP,   &stlmtstampa,
			ENTRY_COL_ENT_NUMBER,    &entnuma,
			ENTRY_COL_UPD_USER,      &updusera,
			ENTRY_COL_UPD_STAMP,     &updstampa,
			ENTRY_COL_CONCIL_NUMBER, &concilnuma,
			ENTRY_COL_CONCIL_DATE,   &concildatea,
			ENTRY_COL_STATUS,        &statusa,
			ENTRY_COL_RULE,          &rulea,
			ENTRY_COL_NOTES,         &notesa,
			ENTRY_COL_NOTES_PNG,     &pnga,
			-1 );

	gtk_tree_model_get( tmodel, b,
			ENTRY_COL_DOPE,          &dopeb,
			ENTRY_COL_DEFFECT,       &deffb,
			ENTRY_COL_LABEL,         &labelb,
			ENTRY_COL_REF,           &refb,
			ENTRY_COL_CURRENCY,      &curb,
			ENTRY_COL_LEDGER,        &ledgerb,
			ENTRY_COL_OPE_TEMPLATE,  &templateb,
			ENTRY_COL_ACCOUNT,       &accountb,
			ENTRY_COL_DEBIT,         &debitb,
			ENTRY_COL_CREDIT,        &creditb,
			ENTRY_COL_OPE_NUMBER,    &openumb,
			ENTRY_COL_STLMT_NUMBER,  &stlmtnumb,
			ENTRY_COL_STLMT_USER,    &stlmtuserb,
			ENTRY_COL_STLMT_STAMP,   &stlmtstampb,
			ENTRY_COL_ENT_NUMBER,    &entnumb,
			ENTRY_COL_UPD_USER,      &upduserb,
			ENTRY_COL_UPD_STAMP,     &updstampb,
			ENTRY_COL_CONCIL_NUMBER, &concilnumb,
			ENTRY_COL_CONCIL_DATE,   &concildateb,
			ENTRY_COL_STATUS,        &statusb,
			ENTRY_COL_RULE,          &ruleb,
			ENTRY_COL_NOTES,         &notesb,
			ENTRY_COL_NOTES_PNG,     &pngb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case ENTRY_COL_DOPE:
			cmp = my_date_compare_by_str( dopea, dopeb, ofa_prefs_date_get_display_format( priv->getter ));
			break;
		case ENTRY_COL_DEFFECT:
			cmp = my_date_compare_by_str( deffa, deffb, ofa_prefs_date_get_display_format( priv->getter ));
			break;
		case ENTRY_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case ENTRY_COL_REF:
			cmp = my_collate( refa, refb );
			break;
		case ENTRY_COL_CURRENCY:
			cmp = my_collate( cura, curb );
			break;
		case ENTRY_COL_LEDGER:
			cmp = my_collate( ledgera, ledgerb );
			break;
		case ENTRY_COL_OPE_TEMPLATE:
			cmp = my_collate( templatea, templateb );
			break;
		case ENTRY_COL_ACCOUNT:
			cmp = my_collate( accounta, accountb );
			break;
		case ENTRY_COL_DEBIT:
			cmp = ofa_itvsortable_sort_str_amount( OFA_ITVSORTABLE( tvbin ), debita, debitb );
			break;
		case ENTRY_COL_CREDIT:
			cmp = ofa_itvsortable_sort_str_amount( OFA_ITVSORTABLE( tvbin ), credita, creditb );
			break;
		case ENTRY_COL_OPE_NUMBER:
			cmp = ofa_itvsortable_sort_str_int( openuma, openumb );
			break;
		case ENTRY_COL_STLMT_NUMBER:
			cmp = ofa_itvsortable_sort_str_int( stlmtnuma, stlmtnumb );
			break;
		case ENTRY_COL_STLMT_USER:
			cmp = my_collate( stlmtusera, stlmtuserb );
			break;
		case ENTRY_COL_STLMT_STAMP:
			cmp = my_collate( stlmtstampa, stlmtstampb );
			break;
		case ENTRY_COL_ENT_NUMBER:
			cmp = ofa_itvsortable_sort_str_int( entnuma, entnumb );
			break;
		case ENTRY_COL_UPD_USER:
			cmp = my_collate( updusera, upduserb );
			break;
		case ENTRY_COL_UPD_STAMP:
			cmp = my_collate( updstampa, updstampb );
			break;
		case ENTRY_COL_CONCIL_NUMBER:
			cmp = ofa_itvsortable_sort_str_int( concilnuma, concilnumb );
			break;
		case ENTRY_COL_CONCIL_DATE:
			cmp = my_date_compare_by_str( concildatea, concildateb, ofa_prefs_date_get_display_format( priv->getter ));
			break;
		case ENTRY_COL_STATUS:
			cmp = ofa_itvsortable_sort_str_int( statusa, statusb );
			break;
		case ENTRY_COL_RULE:
			cmp = my_collate( rulea, ruleb );
			break;
		case ENTRY_COL_NOTES:
			cmp = my_collate( notesa, notesb );
			break;
		case ENTRY_COL_NOTES_PNG:
			cmp = ofa_itvsortable_sort_png( pnga, pngb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( dopea );
	g_free( deffa );
	g_free( labela );
	g_free( refa );
	g_free( cura );
	g_free( ledgera );
	g_free( templatea );
	g_free( accounta );
	g_free( debita );
	g_free( credita );
	g_free( openuma );
	g_free( stlmtnuma );
	g_free( stlmtusera );
	g_free( stlmtstampa );
	g_free( entnuma );
	g_free( updusera );
	g_free( updstampa );
	g_free( concilnuma );
	g_free( concildatea );
	g_free( statusa );
	g_free( rulea );
	g_free( notesa );
	g_clear_object( &pnga );

	g_free( dopeb );
	g_free( deffb );
	g_free( labelb );
	g_free( refb );
	g_free( curb );
	g_free( ledgerb );
	g_free( templateb );
	g_free( accountb );
	g_free( debitb );
	g_free( creditb );
	g_free( openumb );
	g_free( stlmtnumb );
	g_free( stlmtuserb );
	g_free( stlmtstampb );
	g_free( entnumb );
	g_free( upduserb );
	g_free( updstampb );
	g_free( concilnumb );
	g_free( concildateb );
	g_free( statusb );
	g_free( ruleb );
	g_free( notesb );
	g_clear_object( &pngb );

	return( cmp );
}
