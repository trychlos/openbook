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
	GtkTreeView       *tview;
	ofaLedgerStore    *store;
	gchar             *settings_key;

	/* sorted model
	 */
	GtkTreeModel      *sort_model;			/* a sort model stacked on top of the store */
	GtkTreeViewColumn *sort_column;
	gint               sort_column_id;
	gint               sort_order;
}
	ofaLedgerTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	INSERT,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void         setup_top_widget( ofaLedgerTreeview *self );
static void         create_treeview_columns( ofaLedgerTreeview *view, const gint *columns );
static void         tview_on_header_clicked( GtkTreeViewColumn *column, ofaLedgerTreeview *self );
static void         tview_set_sort_indicator( ofaLedgerTreeview *self );
static gint         tview_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaLedgerTreeview *self );
static gint         tview_on_sort_int( const gchar *a, const gchar *b );
static gint         tview_on_sort_png( const GdkPixbuf *pnga, const GdkPixbuf *pngb );
static gboolean     tview_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaLedgerTreeview *self );
static void         tview_on_key_insert( ofaLedgerTreeview *page );
static void         tview_on_key_delete( ofaLedgerTreeview *page );
static void         on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaLedgerTreeview *self );
static void         on_row_selected( GtkTreeSelection *selection, ofaLedgerTreeview *self );
static GList       *get_selected( ofaLedgerTreeview *self );
static void         select_row_by_mnemo( ofaLedgerTreeview *tview, const gchar *ledger );
static gboolean     find_row_by_mnemo( ofaLedgerTreeview *tview, const gchar *ledger, GtkTreeIter *iter );
static const gchar *get_default_settings_key( ofaLedgerTreeview *self );
static void         get_sort_settings( ofaLedgerTreeview *self );
static void         set_sort_settings( ofaLedgerTreeview *self );

G_DEFINE_TYPE_EXTENDED( ofaLedgerTreeview, ofa_ledger_treeview, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaLedgerTreeview ))

static void
ledger_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledger_treeview_finalize";
	ofaLedgerTreeviewPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGER_TREEVIEW( instance ));

	/* free data members here */
	priv = ofa_ledger_treeview_get_instance_private( OFA_LEDGER_TREEVIEW( instance ));

	g_free( priv->settings_key );

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
	priv->settings_key = g_strdup( get_default_settings_key( self ));
	priv->sort_model = NULL;
}

static void
ofa_ledger_treeview_class_init( ofaLedgerTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledger_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledger_treeview_finalize;

	/**
	 * ofaLedgerTreeview::changed:
	 *
	 * This signal is sent on the #ofaLedgerTreeview when the selection
	 * is changed.
	 *
	 * Arguments is the list of selected mnemos.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaLedgerTreeview *view,
	 * 						GList           *sel_mnemos,
	 * 						gpointer         user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
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
	 * ofaLedgerTreeview::activated:
	 *
	 * This signal is sent on the #ofaLedgerTreeview when the selection is
	 * activated.
	 *
	 * Arguments is the list of selected mnemos.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaLedgerTreeview *view,
	 * 						GList           *sel_mnemos,
	 * 						gpointer         user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-activated",
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
	 * ofaLedgerTreeview::insert:
	 *
	 * This signal is sent on the #ofaLedgerTreeview when the insertion
	 * key is hit.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaLedgerTreeview *view,
	 * 						gpointer         user_data );
	 */
	st_signals[ INSERT ] = g_signal_new_class_handler(
				"ofa-insert",
				OFA_TYPE_LEDGER_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0,
				G_TYPE_NONE );

	/**
	 * ofaLedgerTreeview::delete:
	 *
	 * This signal is sent on the #ofaLedgerTreeview when the deletion
	 * key is hit.
	 *
	 * Arguments is the list of selected mnemos.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaLedgerTreeview *view,
	 * 						GList           *sel_mnemos,
	 * 						gpointer         user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-delete",
				OFA_TYPE_LEDGER_TREEVIEW,
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
 * ofa_ledger_treeview_new:
 */
ofaLedgerTreeview *
ofa_ledger_treeview_new( void )
{
	ofaLedgerTreeview *view;

	view = g_object_new( OFA_TYPE_LEDGER_TREEVIEW, NULL );

	setup_top_widget( view );

	return( view );
}

/*
 * if not already done, create a GtkTreeView inside of a GtkScrolledWindow
 */
static void
setup_top_widget( ofaLedgerTreeview *self )
{
	ofaLedgerTreeviewPrivate *priv;
	GtkTreeSelection *select;
	GtkWidget *top_widget, *scrolled;

	priv = ofa_ledger_treeview_get_instance_private( self );

	top_widget = gtk_frame_new( NULL );
	gtk_container_add( GTK_CONTAINER( self ), top_widget );
	gtk_frame_set_shadow_type( GTK_FRAME( top_widget ), GTK_SHADOW_IN );

	scrolled = gtk_scrolled_window_new( NULL, NULL );
	gtk_container_add( GTK_CONTAINER( top_widget ), scrolled );
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW( scrolled ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );

	priv->tview = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_hexpand( GTK_WIDGET( priv->tview ), TRUE );
	gtk_widget_set_vexpand( GTK_WIDGET( priv->tview ), TRUE );
	gtk_container_add( GTK_CONTAINER( scrolled ), GTK_WIDGET( priv->tview ));
	gtk_tree_view_set_headers_visible( priv->tview, TRUE );

	g_signal_connect( priv->tview, "row-activated", G_CALLBACK( on_row_activated ), self );
	g_signal_connect( priv->tview, "key-press-event", G_CALLBACK( tview_on_key_pressed ), self );

	select = gtk_tree_view_get_selection( priv->tview );
	g_signal_connect( select, "changed", G_CALLBACK( on_row_selected ), self );
}

/**
 * ofa_ledger_treeview_set_columns:
 * @view: this #ofaLedgerTreeview instance.
 * @columns: a -1-terminated list of columns to be displayed.
 *
 * Setup the displayable columns.
 */
void
ofa_ledger_treeview_set_columns( ofaLedgerTreeview *view, const gint *columns )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ));

	priv = ofa_ledger_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	create_treeview_columns( view, columns );
}

static void
create_treeview_columns( ofaLedgerTreeview *self, const gint *columns )
{
	ofaLedgerTreeviewPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	gint i;

	priv = ofa_ledger_treeview_get_instance_private( self );

	for( i=0 ; columns[i] >= 0 ; ++i ){

		if( columns[i] == LEDGER_COL_MNEMO ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "Mnemo" ), cell, "text", columns[i], NULL );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
			g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
		}

		if( columns[i] == LEDGER_COL_LABEL ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "Label" ), cell, "text", columns[i], NULL );
			gtk_tree_view_column_set_expand( column, TRUE );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
			g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
		}

		if( columns[i] == LEDGER_COL_LAST_ENTRY ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "Last entry" ), cell, "text", columns[i], NULL );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
			g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
		}

		if( columns[i] == LEDGER_COL_LAST_CLOSE ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "Last closing" ), cell, "text", columns[i], NULL );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
			g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
		}

		if( columns[i] == LEDGER_COL_NOTES ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "Notes" ), cell, "text", columns[i], NULL );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
			g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
		}

		if( columns[i] == LEDGER_COL_NOTES_PNG ){
			cell = gtk_cell_renderer_pixbuf_new();
			column = gtk_tree_view_column_new_with_attributes(
							"", cell, "pixbuf", columns[i], NULL );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
			g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
		}

		if( columns[i] == LEDGER_COL_UPD_USER ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "User" ), cell, "text", columns[i], NULL );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
			g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
		}

		if( columns[i] == LEDGER_COL_UPD_STAMP ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "Timestamp" ), cell, "text", columns[i], NULL );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
			g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );
		}
	}

	gtk_widget_show_all( GTK_WIDGET( self ));
}

/**
 * ofa_ledger_treeview_set_hub:
 * @view: this #ofaLedgerTreeview instance.
 * @hub: the #ofaHub object of the application.
 *
 * Setup the underlying store, define the sortable tree model and
 * attach it to the @view.
 *
 * As this function allocates actual data and sort them, it should be
 * the last called during the #ofaLedgerTreeview initialization.
 *
 * Another reason is that some sortable columns have to be explicitely
 * setup with the sort model, so columns should be defined before.
 */
void
ofa_ledger_treeview_set_hub( ofaLedgerTreeview *view, ofaHub *hub )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_ledger_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	/* the treeview must have been created first */
	g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));

	priv->store = ofa_ledger_store_new( hub );

	priv->sort_model = gtk_tree_model_sort_new_with_model( GTK_TREE_MODEL( priv->store ));
	/* the sortable model maintains its own reference on the store */
	g_object_unref( priv->store );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( priv->sort_model ), ( GtkTreeIterCompareFunc ) tview_on_sort_model, view, NULL );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->sort_model ), LEDGER_COL_NOTES_PNG, ( GtkTreeIterCompareFunc ) tview_on_sort_model, view, NULL );

	gtk_tree_view_set_model( priv->tview, priv->sort_model );
	/* the treeview maintains its own reference on the sortable model */
	g_object_unref( priv->sort_model );

	get_sort_settings( view );
	tview_set_sort_indicator( view );
}

static void
tview_on_header_clicked( GtkTreeViewColumn *column, ofaLedgerTreeview *self )
{
	ofaLedgerTreeviewPrivate *priv;

	priv = ofa_ledger_treeview_get_instance_private( self );

	if( column != priv->sort_column ){
		gtk_tree_view_column_set_sort_indicator( priv->sort_column, FALSE );
		priv->sort_column = column;
		priv->sort_column_id = gtk_tree_view_column_get_sort_column_id( column );
		priv->sort_order = GTK_SORT_ASCENDING;

	} else {
		priv->sort_order = priv->sort_order == GTK_SORT_ASCENDING ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING;
	}

	set_sort_settings( self );
	tview_set_sort_indicator( self );
}

/*
 * It happens that Gtk+ makes use of up arrow '^' (resp. a down arrow 'v')
 * to indicate a descending (resp. ascending) sort order. This is counter-
 * intuitive as we are expecting the arrow pointing to the smallest item.
 *
 * So inverse the sort order of the sort indicator.
 */
static void
tview_set_sort_indicator( ofaLedgerTreeview *self )
{
	ofaLedgerTreeviewPrivate *priv;

	priv = ofa_ledger_treeview_get_instance_private( self );

	if( priv->sort_model ){
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( priv->sort_model ), priv->sort_column_id, priv->sort_order );
	}

	if( priv->sort_column ){
		gtk_tree_view_column_set_sort_indicator(
				priv->sort_column, TRUE );
		gtk_tree_view_column_set_sort_order(
				priv->sort_column,
				priv->sort_order == GTK_SORT_ASCENDING ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING );
	}
}

/*
 * sorting the treeview
 */
static gint
tview_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaLedgerTreeview *self )
{
	static const gchar *thisfn = "ofa_ledger_treeview_tview_on_sort_model";
	ofaLedgerTreeviewPrivate *priv;
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
	priv = ofa_ledger_treeview_get_instance_private( self );

	switch( priv->sort_column_id ){
		case LEDGER_COL_MNEMO:
			cmp = my_collate( mnemoa, mnemob );
			break;
		case LEDGER_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case LEDGER_COL_LAST_ENTRY:
			cmp = tview_on_sort_int( entrya, entryb );
			break;
		case LEDGER_COL_LAST_CLOSE:
			cmp = my_date_compare_by_str( closea, closeb, ofa_prefs_date_display());
			break;
		case LEDGER_COL_NOTES:
			cmp = my_collate( notesa, notesb );
			break;
		case LEDGER_COL_NOTES_PNG:
			cmp = tview_on_sort_png( pnga, pngb );
			break;
		case LEDGER_COL_UPD_USER:
			cmp = my_collate( updusera, upduserb );
			break;
		case LEDGER_COL_UPD_STAMP:
			cmp = my_collate( updstampa, updstampb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, priv->sort_column_id );
			break;
	}

	g_free( mnemoa );
	g_free( labela );
	g_free( entrya );
	g_free( closea );
	g_free( notesa );
	g_free( updusera );
	g_free( updstampa );
	g_object_unref( pnga );

	g_free( mnemob );
	g_free( labelb );
	g_free( entryb );
	g_free( closeb );
	g_free( notesb );
	g_free( upduserb );
	g_free( updstampb );
	g_object_unref( pngb );

	return( cmp );
}

static gint
tview_on_sort_int( const gchar *a, const gchar *b )
{
	int inta, intb;

	if( !my_strlen( a )){
		if( !my_strlen( b )){
			return( 0 );
		}
		return( -1 );
	}
	inta = atoi( a );

	if( !my_strlen( b )){
		return( 1 );
	}
	intb = atoi( b );

	return( inta < intb ? -1 : ( inta > intb ? 1 : 0 ));
}

static gint
tview_on_sort_png( const GdkPixbuf *pnga, const GdkPixbuf *pngb )
{
	gsize lena, lenb;

	if( !pnga ){
		return( -1 );
	}
	lena = gdk_pixbuf_get_byte_length( pnga );

	if( !pngb ){
		return( 1 );
	}
	lenb = gdk_pixbuf_get_byte_length( pngb );

	if( lena < lenb ){
		return( -1 );
	}
	if( lena > lenb ){
		return( 1 );
	}

	return( memcmp( pnga, pngb, lena ));
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
tview_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaLedgerTreeview *self )
{
	gboolean stop;

	stop = FALSE;

	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Insert ){
			tview_on_key_insert( self );
		} else if( event->keyval == GDK_KEY_Delete ){
			tview_on_key_delete( self );
		}
	}

	return( stop );
}

static void
tview_on_key_insert( ofaLedgerTreeview *page )
{
	g_signal_emit_by_name( page, "ofa-insert" );
}

static void
tview_on_key_delete( ofaLedgerTreeview *page )
{
	GList *sel_objects;

	sel_objects = get_selected( page );
	g_signal_emit_by_name( page, "ofa-delete", sel_objects );
	ofa_ledger_treeview_free_selected( sel_objects );
}

static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaLedgerTreeview *self )
{
	GList *sel_objects;

	sel_objects = get_selected( self );
	g_signal_emit_by_name( self, "ofa-activated", sel_objects );
	ofa_ledger_treeview_free_selected( sel_objects );
}

static void
on_row_selected( GtkTreeSelection *selection, ofaLedgerTreeview *self )
{
	GList *sel_objects;

	sel_objects = get_selected( self );
	g_signal_emit_by_name( self, "ofa-changed", sel_objects );
	ofa_ledger_treeview_free_selected( sel_objects );
}

static GList *
get_selected( ofaLedgerTreeview *self )
{
	ofaLedgerTreeviewPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GList *sel_rows, *irow;
	GList *sel_mnemos;
	gchar *ledger;

	priv = ofa_ledger_treeview_get_instance_private( self );

	sel_mnemos = NULL;
	select = gtk_tree_view_get_selection( priv->tview );
	sel_rows = gtk_tree_selection_get_selected_rows( select, &tmodel );

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
 * ofa_ledger_treeview_get_selection:
 * @view:
 *
 * Returns: the #GtkTreeSelection object.
 */
GtkTreeSelection *
ofa_ledger_treeview_get_selection( ofaLedgerTreeview *view )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_val_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ), NULL );

	priv = ofa_ledger_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( gtk_tree_view_get_selection( priv->tview ));
}

/**
 * ofa_ledger_treeview_get_selected:
 *
 * Returns: the list of #ofoLedger ledgers selected mnemonics.
 *
 * The returned list should be ofa_ledger_treeview_free_selected() by
 * the caller.
 */
GList *
ofa_ledger_treeview_get_selected( ofaLedgerTreeview *view )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_val_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ), NULL );

	priv = ofa_ledger_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( get_selected( view ));
}

/**
 * ofa_ledger_treeview_set_selected:
 */
void
ofa_ledger_treeview_set_selected( ofaLedgerTreeview *tview, const gchar *ledger )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_if_fail( tview && OFA_IS_LEDGER_TREEVIEW( tview ));

	priv = ofa_ledger_treeview_get_instance_private( tview );

	g_return_if_fail( !priv->dispose_has_run );

	select_row_by_mnemo( tview, ledger );
}

static void
select_row_by_mnemo( ofaLedgerTreeview *tview, const gchar *ledger )
{
	ofaLedgerTreeviewPrivate *priv;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	priv = ofa_ledger_treeview_get_instance_private( tview );

	if( find_row_by_mnemo( tview, ledger, &iter )){
		select = gtk_tree_view_get_selection( priv->tview );
		gtk_tree_selection_select_iter( select, &iter );
	}
}

static gboolean
find_row_by_mnemo( ofaLedgerTreeview *tview, const gchar *ledger, GtkTreeIter *iter )
{
	ofaLedgerTreeviewPrivate *priv;
	gchar *mnemo;
	gint cmp;

	priv = ofa_ledger_treeview_get_instance_private( tview );

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( priv->store ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), iter, LEDGER_COL_MNEMO, &mnemo, -1 );
			cmp = g_utf8_collate( mnemo, ledger );
			g_free( mnemo );
			if( cmp == 0 ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( priv->store ), iter )){
				break;
			}
		}
	}

	return( FALSE );
}

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
 * ofa_ledger_treeview_set_settings_key:
 * @view: this #ofaLedgerTreeview instance.
 * @key: [allow-none]: the desired settings key.
 *
 * Setup the desired settings key.
 *
 * The settings key defaults to the class name 'ofaLedgerTreeview'.
 *
 * When called with null or empty @key, this reset the settings key to
 * the default.
 */
void
ofa_ledger_treeview_set_settings_key( ofaLedgerTreeview *view, const gchar *key )
{
	ofaLedgerTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_LEDGER_TREEVIEW( view ));

	priv = ofa_ledger_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->settings_key );
	priv->settings_key = g_strdup( my_strlen( key ) ? key : get_default_settings_key( view ));
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
