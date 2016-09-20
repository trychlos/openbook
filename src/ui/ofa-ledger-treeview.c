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
#include <string.h>

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvsortable.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-ledger.h"

#include "core/ofa-ledger-store.h"

#include "ui/ofa-ledger-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean           dispose_has_run;

	/* UI
	 */
	ofaLedgerStore    *store;
}
	ofaLedgerTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void       setup_columns( ofaLedgerTreeview *self );
static void       on_selection_changed( ofaLedgerTreeview *self, GtkTreeSelection *selection, void *empty );
static void       on_selection_activated( ofaLedgerTreeview *self, GtkTreeSelection *selection, void *empty );
static void       on_selection_delete( ofaLedgerTreeview *self, GtkTreeSelection *selection, void *empty );
static void       get_and_send( ofaLedgerTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static gboolean   find_row_by_mnemo( ofaLedgerTreeview *self, const gchar *ledger, GtkTreeIter *iter );
static gint       v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaLedgerTreeview, ofa_ledger_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaLedgerTreeview ))

static void
ledger_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledger_treeview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGER_TREEVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_treeview_parent_class )->finalize( instance );
}

static void
ledger_treeview_dispose( GObject *instance )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_LEDGER_TREEVIEW( instance ));

	priv = ofa_ledger_treeview_get_instance_private( OFA_LEDGER_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_treeview_parent_class )->dispose( instance );
}

static void
ofa_ledger_treeview_init( ofaLedgerTreeview *self )
{
	static const gchar *thisfn = "ofa_ledger_treeview_init";
	ofaLedgerTreeviewPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_LEDGER_TREEVIEW( self ));

	priv = ofa_ledger_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->store = NULL;
}

static void
ofa_ledger_treeview_class_init( ofaLedgerTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledger_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledger_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->sort = v_sort;

	/**
	 * ofaLedgerTreeview::ofa-ledchanged:
	 *
	 * #ofaTVBin sends a 'ofa-selchanged' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaLedgerTreeview proxyes it with this 'ofa-ledchanged' signal,
	 * providing the #ofoLedger selected object.
	 *
	 * Argument is the list of path of selected rows.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaLedgerTreeview *view,
	 * 						GList           *list,
	 * 						gpointer         user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-ledchanged",
				OFA_TYPE_LEDGER_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );

	/**
	 * ofaLedgerTreeview::ofa-ledactivated:
	 *
	 * #ofaTVBin sends a 'ofa-selactivated' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaLedgerTreeview proxyes it with this 'ofa-ledactivated' signal,
	 * providing the #ofoLedger selected object.
	 * The signal is not sent (not proxyied) if the selection is not
	 * unique.
	 *
	 * Argument is the current #ofoLedger object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaLedgerTreeview *view,
	 * 						ofoLedger       *object,
	 * 						gpointer         user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-ledactivated",
				OFA_TYPE_LEDGER_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaLedgerTreeview::ofa-leddelete:
	 *
	 * #ofaTVBin sends a 'ofa-seldelete' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaLedgerTreeview proxyes it with this 'ofa-leddelete' signal,
	 * providing the #ofoLedger selected object.
	 * The signal is not sent (not proxyied) if the selection is not
	 * unique.
	 *
	 * Argument is the current #ofoLedger object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaLedgerTreeview *view,
	 * 						ofoLedger       *object,
	 * 						gpointer         user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-leddelete",
				OFA_TYPE_LEDGER_TREEVIEW,
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
 * ofa_ledger_treeview_new:
 */
ofaLedgerTreeview *
ofa_ledger_treeview_new( void )
{
	ofaLedgerTreeview *view;

	view = g_object_new( OFA_TYPE_LEDGER_TREEVIEW,
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
 * ofa_ledger_treeview_set_settings_key:
 * @view: this #ofaLedgerTreeview instance.
 * @key: [allow-none]: the prefix of the settings key.
 *
 * Setup the setting key, or reset it to its default if %NULL.
 */
void
ofa_ledger_treeview_set_settings_key( ofaLedgerTreeview *view, const gchar *key )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ));

	priv = ofa_ledger_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	/* we do not manage any settings here, so directly pass it to the
	 * base class */
	ofa_tvbin_set_name( OFA_TVBIN( view ), key );
}

/**
 * ofa_ledger_treeview_setup_columns:
 * @view: this #ofaLedgerTreeview instance.
 *
 * Setup the treeview columns.
 */
void
ofa_ledger_treeview_setup_columns( ofaLedgerTreeview *view )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ));

	priv = ofa_ledger_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	setup_columns( view );
}

/*
 * Defines the treeview columns
 */
static void
setup_columns( ofaLedgerTreeview *self )
{
	static const gchar *thisfn = "ofa_ledger_treeview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), LEDGER_COL_MNEMO,      _( "Mnemo" ),      _( "Mnemonic" ));
	ofa_tvbin_add_column_text_x ( OFA_TVBIN( self ), LEDGER_COL_LABEL,      _( "Label" ),          NULL );
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), LEDGER_COL_LAST_ENTRY, _( "Last entry" ), _( "Last entry number" ));
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), LEDGER_COL_LAST_CLOSE, _( "Last close" ), _( "Last close date" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), LEDGER_COL_NOTES,      _( "Notes" ),          NULL );
	ofa_tvbin_add_column_pixbuf ( OFA_TVBIN( self ), LEDGER_COL_NOTES_PNG,     "",             _( "Notes indicator" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), LEDGER_COL_UPD_USER,   _( "User" ),       _( "Last update user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), LEDGER_COL_UPD_STAMP,      NULL,          _( "Last update timestamp" ));

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), LEDGER_COL_LABEL );
}

/**
 * ofa_ledger_treeview_set_hub:
 * @view: this #ofaLedgerTreeview instance.
 * @hub: the #ofaHub object of the application.
 *
 * Initialize the underlying store.
 * Read the settings and show the columns accordingly.
 */
void
ofa_ledger_treeview_set_hub( ofaLedgerTreeview *view, ofaHub *hub )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_ledger_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	priv->store = ofa_ledger_store_new( hub );
	ofa_tvbin_set_store( OFA_TVBIN( view ), GTK_TREE_MODEL( priv->store ));
	g_object_unref( priv->store );

	ofa_itvsortable_set_default_sort( OFA_ITVSORTABLE( view ), LEDGER_COL_MNEMO, GTK_SORT_ASCENDING );
}

static void
on_selection_changed( ofaLedgerTreeview *self, GtkTreeSelection *selection, void *empty )
{
	GList *list;

	list = gtk_tree_selection_get_selected_rows( selection, NULL );
	g_signal_emit_by_name( self, "ofa-ledchanged", list );
	g_list_free_full( list, ( GDestroyNotify ) gtk_tree_path_free );
}

static void
on_selection_activated( ofaLedgerTreeview *self, GtkTreeSelection *selection, void *empty )
{
	if( gtk_tree_selection_count_selected_rows( selection ) == 1 ){
		get_and_send( self, selection, "ofa-ledactivated" );
	}
}

/*
 * Delete key pressed
 * ofaTVBin base class makes sure the selection is not empty.
 */
static void
on_selection_delete( ofaLedgerTreeview *self, GtkTreeSelection *selection, void *empty )
{
	if( gtk_tree_selection_count_selected_rows( selection ) == 1 ){
		get_and_send( self, selection, "ofa-leddelete" );
	}
}

/*
 * we have make sure that selection contains one and only one row
 *
 * gtk_tree_selection_get_selected_rows() works even if selection mode
 * is GTK_SELECTION_MULTIPLE (which may happen here)
 */
static void
get_and_send( ofaLedgerTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	GList *list;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoLedger *ledger;

	list = gtk_tree_selection_get_selected_rows( selection, &tmodel );
	g_return_if_fail( g_list_length( list ) == 1 );

	if( gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) list->data )){
		gtk_tree_model_get( tmodel, &iter, LEDGER_COL_OBJECT, &ledger, -1 );
		g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));
		g_object_unref( ledger );
		g_signal_emit_by_name( self, signal, ledger );
	}

	g_list_free_full( list, ( GDestroyNotify ) gtk_tree_path_free );
}

/**
 * ofa_ledger_treeview_get_selected:
 * @view: this #ofaLedgerTreeview instance.
 *
 * Returns: the list of ledger's selected mnemonics.
 *
 * The returned list should be ofa_ledger_treeview_free_selected() by
 * the caller.
 */
GList *
ofa_ledger_treeview_get_selected( ofaLedgerTreeview *view )
{
	ofaLedgerTreeviewPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GList *sel_rows, *irow;
	GList *sel_mnemos;
	gchar *ledger;

	g_return_val_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ), NULL );

	priv = ofa_ledger_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	sel_mnemos = NULL;
	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));
	sel_rows = gtk_tree_selection_get_selected_rows( selection, &tmodel );

	for( irow=sel_rows ; irow ; irow=irow->next ){
		if( gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) irow->data )){
			gtk_tree_model_get( tmodel, &iter, LEDGER_COL_MNEMO, &ledger, -1 );
			sel_mnemos = g_list_append( sel_mnemos, ledger );
		}
	}

	g_list_free_full( sel_rows, ( GDestroyNotify ) gtk_tree_path_free );

	return( sel_mnemos );
}

/**
 * ofa_ledger_treeview_set_selected:
 * @view: this #ofaLedgerTreeview instance.
 * @ledger: [allow-none]: the mnemonic of the ledger to be selected.
 *
 * Select the specified @ledger.
 */
void
ofa_ledger_treeview_set_selected( ofaLedgerTreeview *view, const gchar *ledger )
{
	ofaLedgerTreeviewPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	g_return_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ));

	priv = ofa_ledger_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));
	gtk_tree_selection_unselect_all( selection );

	if( my_strlen( ledger ) && find_row_by_mnemo( view, ledger, &iter )){
		ofa_tvbin_select_row( OFA_TVBIN( view ), &iter );
	}
}

/*
 * this works because there is no filter on this view
 */
static gboolean
find_row_by_mnemo( ofaLedgerTreeview *self, const gchar *ledger, GtkTreeIter *iter )
{
	GtkTreeModel *tmodel;
	gchar *mnemo;
	gint cmp;

	tmodel = ofa_tvbin_get_tree_model( OFA_TVBIN( self ));

	if( gtk_tree_model_get_iter_first( tmodel, iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, iter, LEDGER_COL_MNEMO, &mnemo, -1 );
			cmp = g_utf8_collate( mnemo, ledger );
			g_free( mnemo );
			if( cmp == 0 ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( tmodel, iter )){
				break;
			}
		}
	}

	return( FALSE );
}

#if 0
/**
 * ofa_ledger_treeview_set_selection_mode:
 */
void
ofa_ledger_treeview_set_selection_mode( ofaLedgerTreeview *view, GtkSelectionMode mode )
{
	ofaLedgerTreeviewPrivate *priv;
	GtkTreeSelection *select;

	g_return_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ));

	priv = ofa_ledger_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));
	select = gtk_tree_view_get_selection( priv->tview );
	gtk_tree_selection_set_mode( select, mode );
}

/**
 * ofa_ledger_treeview_set_hexpand:
 * @view: this #ofaLedgerTreeview instance.
 * @hexpand: whether the treeview should expand horizontally to the
 *  available size.
 */
void
ofa_ledger_treeview_set_hexpand( ofaLedgerTreeview *view, gboolean hexpand )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ));

	priv = ofa_ledger_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );
	g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));

	gtk_widget_set_hexpand( GTK_WIDGET( priv->tview ), hexpand );
}

/**
 * ofa_ledger_treeview_get_treeview:
 * @view: this #ofaLedgerTreeview instance.
 *
 * Returns: the underlying #GtkTreeView widget.
 */
GtkWidget *
ofa_ledger_treeview_get_treeview( ofaLedgerTreeview *view )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_val_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ), NULL );

	priv = ofa_ledger_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );
	g_return_val_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ), NULL );

	return( GTK_WIDGET( priv->tview ));
}

/*
 * return the default settings key
 */
static const gchar *
get_default_settings_key( ofaLedgerTreeview *self )
{
	return( G_OBJECT_TYPE_NAME( self ));
}

/*
 * sort_settings: sort_column_id;sort_order;
 *
 * Note that we record the actual sort order (gtk_sort_ascending for
 * ascending order); only the sort indicator of the column is reversed.
 */
static void
get_sort_settings( ofaLedgerTreeview *self )
{
	ofaLedgerTreeviewPrivate *priv;
	GList *slist, *it, *columns;
	gchar *sort_key;
	const gchar *cstr;
	GtkTreeViewColumn *column;

	priv = ofa_ledger_treeview_get_instance_private( self );

	/* default is to sort by ascending identifier (alpha order)
	 */
	priv->sort_column = NULL;
	priv->sort_column_id = LEDGER_COL_MNEMO;
	priv->sort_order = GTK_SORT_ASCENDING;

	/* get the settings (if any)
	 */
	sort_key = g_strdup_printf( "%s-sort", priv->settings_key );
	slist = ofa_settings_user_get_string_list( sort_key );

	it = slist ? slist : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->sort_column_id = atoi( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->sort_order = atoi( cstr );
	}

	ofa_settings_free_string_list( slist );
	g_free( sort_key );

	/* setup the sort treeview column
	 */
	columns = gtk_tree_view_get_columns( priv->tview );
	for( it=columns ; it ; it=it->next ){
		column = GTK_TREE_VIEW_COLUMN( it->data );
		if( gtk_tree_view_column_get_sort_column_id( column ) == priv->sort_column_id ){
			priv->sort_column = column;
			break;
		}
	}
	g_list_free( columns );
}

static void
set_sort_settings( ofaLedgerTreeview *self )
{
	ofaLedgerTreeviewPrivate *priv;
	gchar *sort_key, *str;

	priv = ofa_ledger_treeview_get_instance_private( self );

	sort_key = g_strdup_printf( "%s-sort", priv->settings_key );
	str = g_strdup_printf( "%d;%d;", priv->sort_column_id, priv->sort_order );

	ofa_settings_user_set_string( sort_key, str );

	g_free( str );
	g_free( sort_key );
}
#endif

static gint
v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_ledger_treeview_v_sort";
	gint cmp;
	gchar *mnemoa, *labela, *entrya, *closea, *notesa, *updusera, *updstampa;
	gchar *mnemob, *labelb, *entryb, *closeb, *notesb, *upduserb, *updstampb;
	GdkPixbuf *pnga, *pngb;

	gtk_tree_model_get( tmodel, a,
			LEDGER_COL_MNEMO,       &mnemoa,
			LEDGER_COL_LABEL,       &labela,
			LEDGER_COL_LAST_ENTRY,  &entrya,
			LEDGER_COL_LAST_CLOSE,  &closea,
			LEDGER_COL_NOTES,       &notesa,
			LEDGER_COL_NOTES_PNG,   &pnga,
			LEDGER_COL_UPD_USER,    &updusera,
			LEDGER_COL_UPD_STAMP,   &updstampa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			LEDGER_COL_MNEMO,       &mnemob,
			LEDGER_COL_LABEL,       &labelb,
			LEDGER_COL_LAST_ENTRY,  &entryb,
			LEDGER_COL_LAST_CLOSE,  &closeb,
			LEDGER_COL_NOTES,       &notesb,
			LEDGER_COL_NOTES_PNG,   &pngb,
			LEDGER_COL_UPD_USER,    &upduserb,
			LEDGER_COL_UPD_STAMP,   &updstampb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case LEDGER_COL_MNEMO:
			cmp = my_collate( mnemoa, mnemob );
			break;
		case LEDGER_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case LEDGER_COL_LAST_ENTRY:
			cmp = ofa_itvsortable_sort_str_int( entrya, entryb );
			break;
		case LEDGER_COL_LAST_CLOSE:
			cmp = my_date_compare_by_str( closea, closeb, ofa_prefs_date_display());
			break;
		case LEDGER_COL_NOTES:
			cmp = my_collate( notesa, notesb );
			break;
		case LEDGER_COL_NOTES_PNG:
			cmp = ofa_itvsortable_sort_png( pnga, pngb );
			break;
		case LEDGER_COL_UPD_USER:
			cmp = my_collate( updusera, upduserb );
			break;
		case LEDGER_COL_UPD_STAMP:
			cmp = my_collate( updstampa, updstampb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( mnemoa );
	g_free( labela );
	g_free( entrya );
	g_free( closea );
	g_free( notesa );
	g_free( updusera );
	g_free( updstampa );
	g_clear_object( &pnga );

	g_free( mnemob );
	g_free( labelb );
	g_free( entryb );
	g_free( closeb );
	g_free( notesb );
	g_free( upduserb );
	g_free( updstampb );
	g_clear_object( &pngb );

	return( cmp );
}
