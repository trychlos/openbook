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

#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-isortable.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-tvbin.h"

/* private instance data
 */
typedef struct {
	gboolean            dispose_has_run;

	/* setup
	 */
	gchar              *settings_key;

	/* UI
	 */
	GtkWidget          *treeview;
}
	ofaTVBinPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	INSERT,
	DELETE,
	N_SIGNALS
};

static guint        st_signals[ N_SIGNALS ] = { 0 };

static const gchar *st_action_group_name    = "tvbin";

static void         init_top_widget( ofaTVBin *self );
static void         tview_on_row_selected( GtkTreeSelection *selection, ofaTVBin *self );
static void         tview_on_row_activated( GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, ofaTVBin *self );
static gboolean     tview_on_key_pressed( GtkWidget *treeview, GdkEventKey *event, ofaTVBin *self );
static void         add_column( ofaTVBin *self, GtkTreeViewColumn *column, gint column_id, const gchar *menu );
static const gchar *get_settings_key( ofaTVBin *self );
static void         iactionable_iface_init( ofaIActionableInterface *iface );
static guint        iactionable_get_interface_version( void );
static void         icontext_iface_init( ofaIContextInterface *iface );
static guint        icontext_get_interface_version( void );
static GtkWidget   *icontext_get_focused_widget( ofaIContext *instance );
static void         isortable_iface_init( ofaISortableInterface *iface );
static guint        isortable_get_interface_version( void );
static gint         isortable_sort_model( const ofaISortable *instance, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );
static const gchar *isortable_get_settings_key( const ofaISortable *instance );
static void         itvcolumnable_iface_init( ofaITVColumnableInterface *iface );
static guint        itvcolumnable_get_interface_version( void );
static const gchar *itvcolumnable_get_settings_key( const ofaITVColumnable *instance );

G_DEFINE_TYPE_EXTENDED( ofaTVBin, ofa_tvbin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaTVBin )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IACTIONABLE, iactionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ICONTEXT, icontext_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISORTABLE, isortable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ITVCOLUMNABLE, itvcolumnable_iface_init ))

static void
bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tvbin_finalize";
	ofaTVBinPrivate *priv;

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVBIN( instance ));

	/* free data members here */
	priv = ofa_tvbin_get_instance_private( OFA_TVBIN( instance ));

	g_free( priv->settings_key );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tvbin_parent_class )->finalize( instance );
}

static void
bin_dispose( GObject *instance )
{
	ofaTVBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_TVBIN( instance ));

	priv = ofa_tvbin_get_instance_private( OFA_TVBIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		ofa_itvcolumnable_record_settings( OFA_ITVCOLUMNABLE( instance ));

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tvbin_parent_class )->dispose( instance );
}

static void
ofa_tvbin_init( ofaTVBin *self )
{
	static const gchar *thisfn = "ofa_tvbin_init";
	ofaTVBinPrivate *priv;

	g_return_if_fail( OFA_IS_TVBIN( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_tvbin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_key = g_strdup( G_OBJECT_TYPE_NAME( self ));
	/* at this time, self is "only" an ofaTVBin */
	g_debug( "%s: settings defaut settings key to '%s'", thisfn, priv->settings_key );

	init_top_widget( self );
}

static void
init_top_widget( ofaTVBin *self )
{
	ofaTVBinPrivate *priv;
	GtkWidget *frame, *scrolled;
	GtkTreeSelection *selection;

	priv = ofa_tvbin_get_instance_private( self );

	frame = gtk_frame_new( NULL );
	gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_IN );
	gtk_container_add( GTK_CONTAINER( self ), frame );

	scrolled = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW( scrolled ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_container_add( GTK_CONTAINER( frame ), scrolled );

	priv->treeview = gtk_tree_view_new();
	gtk_widget_set_hexpand( priv->treeview, TRUE );
	gtk_widget_set_vexpand( priv->treeview, TRUE );
	gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( priv->treeview ), TRUE );
	gtk_container_add( GTK_CONTAINER( scrolled ), priv->treeview );

	g_signal_connect( priv->treeview, "row-activated", G_CALLBACK( tview_on_row_activated ), self );
	g_signal_connect( priv->treeview, "key-press-event", G_CALLBACK( tview_on_key_pressed ), self );

	selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->treeview ));
	g_signal_connect( selection, "changed", G_CALLBACK( tview_on_row_selected ), self );
}

static void
ofa_tvbin_class_init( ofaTVBinClass *klass )
{
	static const gchar *thisfn = "ofa_tvbin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = bin_finalize;

	/**
	 * ofaTVBin::ofa-selchanged:
	 *
	 * This signal is sent on the #ofaTVBin when the selection is
	 * changed.
	 *
	 * Argument is the current #GtkTreeSelection.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaTVBin           *bin,
	 * 						GtkTreeSelection *selection,
	 * 						gpointer          user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-selchanged",
				OFA_TYPE_TVBIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaTVBin::ofa-selactivated:
	 *
	 * This signal is sent on the #ofaTVBin when the selection is
	 * activated.
	 *
	 * Argument is the current #GtkTreeSelection.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaTVBin           *bin,
	 * 						GtkTreeSelection *selection,
	 * 						gpointer          user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-selactivated",
				OFA_TYPE_TVBIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaTVBin::ofa-insert:
	 *
	 * This signal is sent on the #ofaTVBin when the Insert key is
	 * pressed.
	 *
	 * No arg.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaTVBin  *bin,
	 * 						gpointer user_data );
	 */
	st_signals[ INSERT ] = g_signal_new_class_handler(
				"ofa-insert",
				OFA_TYPE_TVBIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0,
				G_TYPE_NONE );

	/**
	 * ofaTVBin::ofa-seldelete:
	 *
	 * This signal is sent on the #ofaTVBin when the Delete key is
	 * pressed.
	 *
	 * Argument is the current #GtkTreeSelection.
	 * The signal is not sent if there is no current selection.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaTVBin           *bin,
	 * 						GtkTreeSelection *selection,
	 * 						gpointer          user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-seldelete",
				OFA_TYPE_TVBIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );
}

static void
tview_on_row_selected( GtkTreeSelection *selection, ofaTVBin *self )
{
	g_signal_emit_by_name( self, "ofa-selchanged", selection );
}

static void
tview_on_row_activated( GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, ofaTVBin *self )
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection( treeview );
	g_return_if_fail( gtk_tree_selection_count_selected_rows( selection ) > 0 );

	g_signal_emit_by_name( self, "ofa-selactivated", selection );
}

/*
 * Handles here Insert and Delete keys.
 *
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
tview_on_key_pressed( GtkWidget *treeview, GdkEventKey *event, ofaTVBin *self )
{
	gboolean stop;
	GtkTreeSelection *selection;

	stop = FALSE;

	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete ){
			selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( treeview ));
			if( gtk_tree_selection_count_selected_rows( selection ) > 0 ){
				g_signal_emit_by_name( self, "ofa-seldelete", selection );
			}

		} else if( event->keyval == GDK_KEY_Insert || event->keyval == GDK_KEY_KP_Insert ){
			g_signal_emit_by_name( self, "ofa-insert" );
		}
	}

	return( stop );
}

/*
 * ofa_tvbin_add_column_amount:
 * @bin: this #ofaTVBin instance.
 * @column_id: the source column id,
 *  is also used as sortable column identifier.
 * @header: [allow-none]: the header of the column,
 *  if NULL, then takes default.
 * @menu: [allow-none]: the label of the popup menu,
 *  if NULL, then takes the column header.
 *
 * Appends an amount column to the treeview, defaulting to non visible.
 */
void
ofa_tvbin_add_column_amount( ofaTVBin *bin, gint column_id, const gchar *header, const gchar *menu )
{
	ofaTVBinPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
					header ? header : _( "Amount" ), cell, "text", column_id, NULL );
	gtk_tree_view_column_set_alignment( column, 1.0 );

	add_column( bin, column, column_id, menu );
}

/*
 * ofa_tvbin_add_column_date:
 * @bin: this #ofaTVBin instance.
 * @column_id: the source column id,
 *  is also used as sortable column identifier.
 * @header: [allow-none]: the header of the column,
 *  if NULL, then takes default.
 * @menu: [allow-none]: the label of the popup menu,
 *  if NULL, then takes the column header.
 *
 * Appends a date column to the treeview, defaulting to non visible.
 */
void
ofa_tvbin_add_column_date( ofaTVBin *bin, gint column_id, const gchar *header, const gchar *menu )
{
	ofaTVBinPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
					header ? header : _( "Date" ), cell, "text", column_id, NULL );

	add_column( bin, column, column_id, menu );
}

/*
 * ofa_tvbin_add_column_int:
 * @bin: this #ofaTVBin instance.
 * @column_id: the source column id,
 *  is also used as sortable column identifier.
 * @header: [allow-none]: the header of the column,
 *  if NULL, then takes default.
 * @menu: [allow-none]: the label of the popup menu,
 *  if NULL, then takes the column header.
 *
 * Appends an integer column to the treeview, defaulting to non visible.
 */
void
ofa_tvbin_add_column_int( ofaTVBin *bin, gint column_id, const gchar *header, const gchar *menu )
{
	ofaTVBinPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
					header ? header : _( "Int" ), cell, "text", column_id, NULL );
	gtk_tree_view_column_set_alignment( column, 1.0 );

	add_column( bin, column, column_id, menu );
}

/*
 * ofa_tvbin_add_column_pixbuf:
 * @bin: this #ofaTVBin instance.
 * @column_id: the source column id,
 *  is also used as sortable column identifier.
 * @header: [allow-none]: the header of the column,
 *  if NULL, then takes default.
 * @menu: [allow-none]: the label of the popup menu,
 *  if NULL, then takes the column header.
 *
 * Appends a pixbuf column to the treeview, defaulting to non visible.
 */
void
ofa_tvbin_add_column_pixbuf( ofaTVBin *bin, gint column_id, const gchar *header, const gchar *menu )
{
	ofaTVBinPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	cell = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(
					header ? header : _( "Pixbuf" ), cell, "pixbuf", column_id, NULL );

	add_column( bin, column, column_id, menu );
}

/*
 * ofa_tvbin_add_column_stamp:
 * @bin: this #ofaTVBin instance.
 * @column_id: the source column id,
 *  is also used as sortable column identifier.
 * @header: [allow-none]: the header of the column,
 *  if NULL, then takes default.
 * @menu: [allow-none]: the label of the popup menu,
 *  if NULL, then takes the column header.
 *
 * Appends a timestamp column to the treeview, defaulting to non visible.
 */
void
ofa_tvbin_add_column_stamp( ofaTVBin *bin, gint column_id, const gchar *header, const gchar *menu )
{
	ofaTVBinPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
					header ? header : _( "Timestamp" ), cell, "text", column_id, NULL );

	add_column( bin, column, column_id, menu );
}

/*
 * ofa_tvbin_add_column_text:
 * @bin: this #ofaTVBin instance.
 * @column_id: the source column id,
 *  is also used as sortable column identifier.
 * @header: [allow-none]: the header of the column,
 *  if NULL, then takes default.
 * @menu: [allow-none]: the label of the popup menu,
 *  if NULL, then takes the column header.
 *
 * Appends a text column to the treeview, defaulting to non visible.
 */
void
ofa_tvbin_add_column_text( ofaTVBin *bin, gint column_id, const gchar *header, const gchar *menu )
{
	ofaTVBinPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
					header ? header : _( "Text" ), cell, "text", column_id, NULL );

	add_column( bin, column, column_id, menu );
}

/*
 * ofa_tvbin_add_column_text_lx:
 * @bin: this #ofaTVBin instance.
 * @column_id: the source column id,
 *  is also used as sortable column identifier.
 * @header: [allow-none]: the header of the column,
 *  if NULL, then takes default.
 * @menu: [allow-none]: the label of the popup menu,
 *  if NULL, then takes the column header.
 *
 * Appends a text column to the treeview, defaulting to non visible.
 * The text ellipsizes on the left (only the right part may be visible).
 * This column is marked expandable.
 */
void
ofa_tvbin_add_column_text_lx( ofaTVBin *bin, gint column_id, const gchar *header, const gchar *menu )
{
	ofaTVBinPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	cell = gtk_cell_renderer_text_new();
	g_object_set( G_OBJECT( cell ), "ellipsize-set", TRUE, "ellipsize", PANGO_ELLIPSIZE_START, NULL );

	column = gtk_tree_view_column_new_with_attributes(
					header ? header : _( "Text" ), cell, "text", column_id, NULL );
	gtk_tree_view_column_set_expand( column, TRUE );

	add_column( bin, column, column_id, menu );
}

static void
add_column( ofaTVBin *self, GtkTreeViewColumn *column, gint column_id, const gchar *menu )
{
	ofaTVBinPrivate *priv;

	priv = ofa_tvbin_get_instance_private( self );

	gtk_tree_view_column_set_visible( column, FALSE );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	gtk_tree_view_column_set_reorderable( column, TRUE );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_column_set_sizing( column, GTK_TREE_VIEW_COLUMN_GROW_ONLY );

	gtk_tree_view_append_column( GTK_TREE_VIEW( priv->treeview ), column );

	ofa_itvcolumnable_add_column( OFA_ITVCOLUMNABLE( self ), column, st_action_group_name, menu );
}

/*
 * ofa_tvbin_get_menu:
 * @bin: this #ofaTVBin instance.
 *
 * Returns: the current #GMenu used to display columns.
 */
GMenu *
ofa_tvbin_get_menu( ofaTVBin *bin )
{
	ofaTVBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_TVBIN( bin ), NULL );

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( ofa_iactionable_get_menu( OFA_IACTIONABLE( bin ), st_action_group_name ));
}

/*
 * ofa_tvbin_get_selection:
 * @bin: this #ofaTVBin instance.
 *
 * Returns: the current #GtkTreeSelection, or %NULL.
 */
GtkTreeSelection *
ofa_tvbin_get_selection( ofaTVBin *bin )
{
	ofaTVBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_TVBIN( bin ), NULL );

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( priv->treeview ){
		return( gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->treeview )));
	}

	return( NULL );
}

/*
 * ofa_tvbin_get_treeview:
 * @bin: this #ofaTVBin instance.
 *
 * Returns: the current #GtkTreeView, or %NULL.
 */
GtkWidget *
ofa_tvbin_get_treeview( ofaTVBin *bin )
{
	ofaTVBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_TVBIN( bin ), NULL );

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->treeview );
}

/**
 * ofa_tvbin_set_selection_mode:
 * @bin: this #ofaTVBin instance.
 * @mode: the selection mode to be set.
 *
 * Setup the desired selection mode.
 */
void
ofa_tvbin_set_selection_mode( ofaTVBin *bin, GtkSelectionMode mode )
{
	ofaTVBinPrivate *priv;
	GtkTreeSelection *selection;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	if( priv->treeview ){
		selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->treeview ));
		gtk_tree_selection_set_mode( selection, mode );
	}
}

/**
 * ofa_tvbin_set_settings_key:
 * @bin: this #ofaTVBin instance.
 * @key: [allow-none]: the desired settings key.
 *
 * Setup the desired settings key.
 *
 * The settings key defaults to the class name 'ofaBatTreeview'.
 *
 * When called with null or empty @key, this reset the settings key to
 * the default.
 */
void
ofa_tvbin_set_settings_key( ofaTVBin *bin, const gchar *key )
{
	ofaTVBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->settings_key );
	priv->settings_key = g_strdup( my_strlen( key ) ? key : G_OBJECT_TYPE_NAME( bin ));
}

/*
 * ofa_tvbin_set_store:
 * @bin: this #ofaTVBin instance.
 * @store: the #ofaIStore store model.
 *
 * Records the store model.
 *
 * It is expected that all columns have been defined prior the store be
 * set, as this is now that the sort model defines the sort function for
 * each column.
 *
 * The @bin instance takes its own reference on the @store. This later
 * may so be released by the caller.
 */
void
ofa_tvbin_set_store( ofaTVBin *bin, ofaIStore *store )
{
	ofaTVBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	ofa_isortable_set_treeview( OFA_ISORTABLE( bin ), GTK_TREE_VIEW( priv->treeview ));
	ofa_isortable_set_store( OFA_ISORTABLE( bin ), store );

	ofa_itvcolumnable_show_columns( OFA_ITVCOLUMNABLE( bin ), GTK_TREE_VIEW( priv->treeview ));
}

static const gchar *
get_settings_key( ofaTVBin *self )
{
	ofaTVBinPrivate *priv;

	priv = ofa_tvbin_get_instance_private( self );

	return( priv->settings_key );
}

/*
 * ofaIActionable interface management
 */
static void
iactionable_iface_init( ofaIActionableInterface *iface )
{
	static const gchar *thisfn = "ofa_tvbin_iactionable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iactionable_get_interface_version;
}

static guint
iactionable_get_interface_version( void )
{
	return( 1 );
}

/*
 * ofaIContext interface management
 */
static void
icontext_iface_init( ofaIContextInterface *iface )
{
	static const gchar *thisfn = "ofa_bat_treeview_icontext_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = icontext_get_interface_version;
	iface->get_focused_widget = icontext_get_focused_widget;
}

static guint
icontext_get_interface_version( void )
{
	return( 1 );
}

static GtkWidget *
icontext_get_focused_widget( ofaIContext *instance )
{
	ofaTVBinPrivate *priv;

	priv = ofa_tvbin_get_instance_private( OFA_TVBIN( instance ));

	return( priv->treeview );
}

/*
 * ofaISortable interface management
 */
static void
isortable_iface_init( ofaISortableInterface *iface )
{
	static const gchar *thisfn = "ofa_tvbin_isortable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = isortable_get_interface_version;
	iface->get_settings_key = isortable_get_settings_key;
	iface->sort_model = isortable_sort_model;
}

static guint
isortable_get_interface_version( void )
{
	return( 1 );
}

static const gchar *
isortable_get_settings_key( const ofaISortable *instance )
{
	return( get_settings_key( OFA_TVBIN( instance )));
}

static gint
isortable_sort_model( const ofaISortable *instance, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	gint cmp;

	g_return_val_if_fail( OFA_IS_TVBIN( instance ), 0 );

	cmp = 0;

	if( OFA_TVBIN_GET_CLASS( OFA_TVBIN( instance ))->sort ){
	cmp = OFA_TVBIN_GET_CLASS( OFA_TVBIN( instance ))->sort( OFA_TVBIN( instance ), tmodel, a, b, column_id );
	}

	return( cmp );
}

/*
 * ofaITVColumnable interface management
 */
static void
itvcolumnable_iface_init( ofaITVColumnableInterface *iface )
{
	static const gchar *thisfn = "ofa_tvbin_itvcolumnable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = itvcolumnable_get_interface_version;
	iface->get_settings_key = itvcolumnable_get_settings_key;
}

static guint
itvcolumnable_get_interface_version( void )
{
	return( 1 );
}

static const gchar *
itvcolumnable_get_settings_key( const ofaITVColumnable *instance )
{
	return( get_settings_key( OFA_TVBIN( instance )));
}
