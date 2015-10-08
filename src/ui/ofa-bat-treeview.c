/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>

#include "api/my-utils.h"
#include "api/ofa-settings.h"
#include "api/ofo-bat.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-bat-treeview.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaBatTreeviewPrivate {
	gboolean             dispose_has_run;

	/* UI
	 */
	GtkTreeView         *tview;
	ofaBatStore         *store;

	/* runtime data
	 */
	gboolean             delete_authorized;
	const ofaMainWindow *main_window;
	ofoDossier          *dossier;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void       attach_top_widget( ofaBatTreeview *self );
static void       on_row_selected( GtkTreeSelection *selection, ofaBatTreeview *self );
static void       on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaBatTreeview *self );
static void       get_and_send( ofaBatTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static gboolean   on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaBatTreeview *self );
static void       try_to_delete_current_row( ofaBatTreeview *self );
static gboolean   delete_confirmed( ofaBatTreeview *self, ofoBat *bat );

G_DEFINE_TYPE( ofaBatTreeview, ofa_bat_treeview, GTK_TYPE_BIN );

static void
bat_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_bat_treeview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BAT_TREEVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_treeview_parent_class )->finalize( instance );
}

static void
bat_treeview_dispose( GObject *instance )
{
	ofaBatTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_BAT_TREEVIEW( instance ));

	priv = OFA_BAT_TREEVIEW( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_treeview_parent_class )->dispose( instance );
}

static void
ofa_bat_treeview_init( ofaBatTreeview *self )
{
	static const gchar *thisfn = "ofa_bat_treeview_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BAT_TREEVIEW( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_BAT_TREEVIEW, ofaBatTreeviewPrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
ofa_bat_treeview_class_init( ofaBatTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_bat_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_treeview_finalize;

	g_type_class_add_private( klass, sizeof( ofaBatTreeviewPrivate ));

	/**
	 * ofaBatTreeview::changed:
	 *
	 * This signal is sent on the #ofaBatTreeview when the selection
	 * is changed.
	 *
	 * Arguments is the selected BAT identifier.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaBatTreeview *view,
	 * 						ofoBat       *bat,
	 * 						gpointer      user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_BAT_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaBatTreeview::activated:
	 *
	 * This signal is sent on the #ofaBatTreeview when the selection is
	 * activated.
	 *
	 * Arguments is the selected BAT identifier.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaBatTreeview *view,
	 * 						ofoBat       *bat,
	 * 						gpointer      user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"activated",
				OFA_TYPE_BAT_TREEVIEW,
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
 * ofa_bat_treeview_new:
 */
ofaBatTreeview *
ofa_bat_treeview_new( void )
{
	ofaBatTreeview *view;

	view = g_object_new( OFA_TYPE_BAT_TREEVIEW, NULL );

	attach_top_widget( view );

	return( view );
}

/*
 * call right after the object instanciation
 * if not already done, create a GtkTreeView inside of a GtkScrolledWindow
 */
static void
attach_top_widget( ofaBatTreeview *self )
{
	ofaBatTreeviewPrivate *priv;
	GtkWidget *top_widget, *frame;
	GtkTreeSelection *select;
	GtkWidget *scrolled;

	priv = self->priv;

	top_widget = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	my_utils_widget_set_margin( top_widget, 4, 4, 4, 4 );

	frame = gtk_frame_new( NULL );
	gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_IN );
	gtk_container_add( GTK_CONTAINER( top_widget ), frame );

	scrolled = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW( scrolled ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_container_add( GTK_CONTAINER( frame ), scrolled );

	priv->tview = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_hexpand( GTK_WIDGET( priv->tview ), TRUE );
	gtk_widget_set_vexpand( GTK_WIDGET( priv->tview ), TRUE );
	gtk_tree_view_set_headers_visible( priv->tview, TRUE );
	gtk_container_add( GTK_CONTAINER( scrolled ), GTK_WIDGET( priv->tview ));

	g_signal_connect(
			G_OBJECT( priv->tview ), "row-activated", G_CALLBACK( on_row_activated ), self );
	g_signal_connect(
			G_OBJECT( priv->tview ), "key-press-event", G_CALLBACK( on_tview_key_pressed ), self );

	select = gtk_tree_view_get_selection( priv->tview );
	g_signal_connect(
			G_OBJECT( select ), "changed", G_CALLBACK( on_row_selected ), self );

	gtk_container_add( GTK_CONTAINER( self ), top_widget );
}

/**
 * ofa_bat_treeview_set_columns:
 * @view: this #ofaBatTreeview view.
 * @columns: a zero-terminated array of the columns to be displayed.
 */
void
ofa_bat_treeview_set_columns( ofaBatTreeview *view, ofaBatColumns *columns )
{
	ofaBatTreeviewPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	gint i;

	g_return_if_fail( view && OFA_IS_BAT_TREEVIEW( view ));
	g_return_if_fail( columns );

	priv = view->priv;

	if( !priv->dispose_has_run ){

		for( i=0 ; columns[i] ; ++i ){

			if( columns[i] == BAT_DISP_ID ){
				cell = gtk_cell_renderer_text_new();
				gtk_cell_renderer_set_alignment( cell, 1.0, 0.5 );
				column = gtk_tree_view_column_new_with_attributes(
								_( "Id." ), cell, "text", BAT_COL_ID, NULL );
				gtk_tree_view_column_set_alignment( column, 1.0 );
				gtk_tree_view_append_column( priv->tview, column );
			}

			if( columns[i] == BAT_DISP_URI ){
				cell = gtk_cell_renderer_text_new();
				column = gtk_tree_view_column_new_with_attributes(
								_( "URI" ), cell, "text", BAT_COL_URI, NULL );
				gtk_tree_view_append_column( priv->tview, column );
			}

			if( columns[i] == BAT_DISP_FORMAT ){
				cell = gtk_cell_renderer_text_new();
				column = gtk_tree_view_column_new_with_attributes(
								_( "Format" ), cell, "text", BAT_COL_FORMAT, NULL );
				gtk_tree_view_append_column( priv->tview, column );
			}

			if( columns[i] == BAT_DISP_BEGIN ){
				cell = gtk_cell_renderer_text_new();
				column = gtk_tree_view_column_new_with_attributes(
								_( "Begin" ), cell, "text", BAT_COL_BEGIN, NULL );
				gtk_tree_view_append_column( priv->tview, column );
			}

			if( columns[i] == BAT_DISP_END ){
				cell = gtk_cell_renderer_text_new();
				column = gtk_tree_view_column_new_with_attributes(
								_( "End" ), cell, "text", BAT_COL_END, NULL );
				gtk_tree_view_append_column( priv->tview, column );
			}

			if( columns[i] == BAT_DISP_COUNT ){
				cell = gtk_cell_renderer_text_new();
				gtk_cell_renderer_set_alignment( cell, 1.0, 0.5 );
				column = gtk_tree_view_column_new_with_attributes(
								_( "Count" ), cell, "text", BAT_COL_COUNT, NULL );
				gtk_tree_view_column_set_alignment( column, 1.0 );
				gtk_tree_view_append_column( priv->tview, column );
			}

			if( columns[i] == BAT_DISP_RIB ){
				cell = gtk_cell_renderer_text_new();
				column = gtk_tree_view_column_new_with_attributes(
								_( "RIB" ), cell, "text", BAT_COL_RIB, NULL );
				gtk_tree_view_append_column( priv->tview, column );
			}

			if( columns[i] == BAT_DISP_BEGIN_SOLDE ){
				cell = gtk_cell_renderer_text_new();
				gtk_cell_renderer_set_alignment( cell, 1.0, 0.5 );
				column = gtk_tree_view_column_new_with_attributes(
								_( "Begin solde" ), cell, "text", BAT_COL_BEGIN_SOLDE, NULL );
				gtk_tree_view_column_set_alignment( column, 1.0 );
				gtk_tree_view_append_column( priv->tview, column );
			}

			if( columns[i] == BAT_DISP_END_SOLDE ){
				cell = gtk_cell_renderer_text_new();
				gtk_cell_renderer_set_alignment( cell, 1.0, 0.5 );
				column = gtk_tree_view_column_new_with_attributes(
								_( "End solde" ), cell, "text", BAT_COL_END_SOLDE, NULL );
				gtk_tree_view_column_set_alignment( column, 1.0 );
				gtk_tree_view_append_column( priv->tview, column );
			}

			if( columns[i] == BAT_DISP_CURRENCY ){
				cell = gtk_cell_renderer_text_new();
				column = gtk_tree_view_column_new_with_attributes(
								_( "Cur." ), cell, "text", BAT_COL_CURRENCY, NULL );
				gtk_tree_view_append_column( priv->tview, column );
			}

			if( columns[i] == BAT_DISP_NOTES ){
				cell = gtk_cell_renderer_text_new();
				column = gtk_tree_view_column_new_with_attributes(
								_( "Notes" ), cell, "text", BAT_COL_NOTES, NULL );
				gtk_tree_view_append_column( priv->tview, column );
			}

			if( columns[i] == BAT_DISP_UPD_USER ){
				cell = gtk_cell_renderer_text_new();
				column = gtk_tree_view_column_new_with_attributes(
								_( "User" ), cell, "text", BAT_COL_UPD_USER, NULL );
				gtk_tree_view_append_column( priv->tview, column );
			}

			if( columns[i] == BAT_DISP_UPD_STAMP ){
				cell = gtk_cell_renderer_text_new();
				column = gtk_tree_view_column_new_with_attributes(
								_( "Timestamp" ), cell, "text", BAT_COL_UPD_STAMP, NULL );
				gtk_tree_view_append_column( priv->tview, column );
			}
		}

		gtk_widget_show_all( GTK_WIDGET( view ));
	}
}

/**
 * ofa_bat_treeview_set_delete:
 */
void
ofa_bat_treeview_set_delete( ofaBatTreeview *view, gboolean authorized )
{
	ofaBatTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_BAT_TREEVIEW( view ));

	priv = view->priv;

	if( !priv->dispose_has_run ){

		priv->delete_authorized = authorized;
	}
}

/**
 * ofa_bat_treeview_set_main_window:
 * @view: this #ofaBatTreeview instance.
 * @main_window: the #ofaMainWindow main window of the application.
 *
 * Setup the main window, and thus the dossier.
 * Initialize the underlying store.
 */
void
ofa_bat_treeview_set_main_window( ofaBatTreeview *view, const ofaMainWindow *main_window )
{
	ofaBatTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_BAT_TREEVIEW( view ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv = view->priv;

	if( !priv->dispose_has_run ){

		priv->main_window = main_window;
		priv->dossier = ofa_main_window_get_dossier( main_window );
		priv->store = ofa_bat_store_new( priv->dossier );
		gtk_tree_view_set_model( priv->tview, GTK_TREE_MODEL( priv->store ));
	}
}

static void
on_row_selected( GtkTreeSelection *selection, ofaBatTreeview *self )
{
	get_and_send( self, selection, "changed" );
}

static void
on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaBatTreeview *self )
{
	GtkTreeSelection *select;

	select = gtk_tree_view_get_selection( tview );
	get_and_send( self, select, "activated" );
}

static void
get_and_send( ofaBatTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	ofoBat *bat;

	bat = ofa_bat_treeview_get_selected( self );
	if( bat ){
		g_signal_emit_by_name( self, signal, bat );
	}
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaBatTreeview *self )
{
	gboolean stop;

	stop = FALSE;

	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Delete ){
			try_to_delete_current_row( self );
		}
	}

	return( stop );
}

static void
try_to_delete_current_row( ofaBatTreeview *self )
{
	ofaBatTreeviewPrivate *priv;
	ofoBat *bat;

	priv = self->priv;

	if( priv->delete_authorized ){
		bat = ofa_bat_treeview_get_selected( self );
		if( bat && ofo_bat_is_deletable( bat, priv->dossier )){
			ofa_bat_treeview_delete_bat( self, bat );
		}
	}
}

/**
 * ofa_bat_treeview_delete_bat:
 */
void
ofa_bat_treeview_delete_bat( ofaBatTreeview *view, ofoBat *bat )
{
	ofaBatTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_BAT_TREEVIEW( view ));
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

	priv = view->priv;

	if( !priv->dispose_has_run ){

		if( delete_confirmed( view, bat )){
			ofo_bat_delete( bat, priv->dossier );
		}
	}
}

static gboolean
delete_confirmed( ofaBatTreeview *self, ofoBat *bat )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup( _( "Are you sure you want delete this imported BAT file\n"
			"(All the corresponding lines will be deleted too) ?" ));

	delete_ok = my_utils_dialog_question( msg, _( "_Delete" ));

	g_free( msg );

	return( delete_ok );
}

/**
 * ofa_bat_treeview_get_selected:
 *
 * Return: the identifier of the currently selected BAT file, or 0.
 */
ofoBat *
ofa_bat_treeview_get_selected( const ofaBatTreeview *view )
{
	static const gchar *thisfn = "ofa_bat_treeview_get_selected";
	ofaBatTreeviewPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreeSelection *select;
	ofoBat *bat;

	g_debug( "%s: view=%p", thisfn, ( void * ) view );

	g_return_val_if_fail( view && OFA_IS_BAT_TREEVIEW( view ), NULL );

	priv = view->priv;
	bat = NULL;

	if( !priv->dispose_has_run ){

		select = gtk_tree_view_get_selection( priv->tview );
		if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
			gtk_tree_model_get( tmodel, &iter, BAT_COL_OBJECT, &bat, -1 );
			g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );
			g_object_unref( bat );
		}
	}

	return( bat );
}

/**
 * ofa_bat_treeview_set_selected:
 */
void
ofa_bat_treeview_set_selected( ofaBatTreeview *view, ofxCounter id )
{
	static const gchar *thisfn = "ofa_bat_treeview_set_selected";
	ofaBatTreeviewPrivate *priv;
	GtkTreeIter iter;
	gchar *sid;
	ofxCounter row_id;
	GtkTreeSelection *select;
	GtkTreePath *path;

	g_debug( "%s: view=%p, id=%ld", thisfn, ( void * ) view, id );

	g_return_if_fail( view && OFA_IS_BAT_TREEVIEW( view ));

	priv = view->priv;

	if( !priv->dispose_has_run ){

		if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( priv->store ), &iter )){
			while( TRUE ){
				gtk_tree_model_get(
						GTK_TREE_MODEL( priv->store ), &iter, BAT_COL_ID, &sid, -1 );
				row_id = atol( sid );
				g_free( sid );
				if( row_id == id ){
					select = gtk_tree_view_get_selection( priv->tview );
					gtk_tree_selection_select_iter( select, &iter );
					/* move the cursor so that it is visible */
					path = gtk_tree_model_get_path( GTK_TREE_MODEL( priv->store ), &iter );
					gtk_tree_view_scroll_to_cell( priv->tview, path, NULL, FALSE, 0, 0 );
					gtk_tree_path_free( path );
					break;
				}
				if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( priv->store ), &iter )){
					break;
				}
			}
		}
	}
}

/**
 * ofa_bat_treeview_get_treeview:
 */
GtkWidget *
ofa_bat_treeview_get_treeview( const ofaBatTreeview *view )
{
	ofaBatTreeviewPrivate *priv;

	g_return_val_if_fail( view && OFA_IS_BAT_TREEVIEW( view ), NULL );

	priv = view->priv;

	if( !priv->dispose_has_run ){

		return( GTK_WIDGET( priv->tview ));
	}

	return( NULL );
}
