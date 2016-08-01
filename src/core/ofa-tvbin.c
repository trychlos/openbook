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

#include "api/ofa-tvbin.h"
#include "api/ofa-isortable.h"
#include "api/ofa-itvcolumnable.h"

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

	/* context menu
	 */
	GMenu              *context_menu;
	GSimpleActionGroup *action_group;
	GtkWidget          *popup_menu;
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

static const gchar *st_group_name           = "tvbin";

static void         init_top_widget( ofaTVBin *self );
static void         init_context_menu( ofaTVBin *self );
static void         tview_on_row_selected( GtkTreeSelection *selection, ofaTVBin *self );
static void         tview_on_row_activated( GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, ofaTVBin *self );
static gboolean     tview_on_button_pressed( GtkWidget *treeview, GdkEventButton *event, ofaTVBin *self );
static void         dump_menu_model( ofaTVBin *self, ofaTVBinPrivate *priv, GMenuModel *model );
static gboolean     tview_on_key_pressed( GtkWidget *treeview, GdkEventKey *event, ofaTVBin *self );
static void         add_column( ofaTVBin *self, GtkTreeViewColumn *column, gint column_id, const gchar *menu );
static const gchar *get_settings_key( ofaTVBin *self );
static void         itvcolumnable_iface_init( ofaITVColumnableInterface *iface );
static guint        itvcolumnable_get_interface_version( void );
static const gchar *itvcolumnable_get_settings_key( const ofaITVColumnable *instance );
static void         isortable_iface_init( ofaISortableInterface *iface );
static guint        isortable_get_interface_version( void );
static gint         isortable_sort_model( const ofaISortable *instance, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );
static const gchar *isortable_get_settings_key( const ofaISortable *instance );

G_DEFINE_TYPE_EXTENDED( ofaTVBin, ofa_tvbin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaTVBin )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ITVCOLUMNABLE, itvcolumnable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISORTABLE, isortable_iface_init ))

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
		if( priv->popup_menu ){
			/* remove a floating reference */
			g_object_ref_sink( priv->popup_menu );
		}
		g_object_unref( priv->context_menu );
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
	init_context_menu( self );
}

static void
init_top_widget( ofaTVBin *self )
{
	ofaTVBinPrivate *priv;
	GtkWidget *frame, *scrolled;
	GtkTreeSelection *select;

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
	g_signal_connect( priv->treeview, "button-press-event", G_CALLBACK( tview_on_button_pressed ), self );
	g_signal_connect( priv->treeview, "key-press-event", G_CALLBACK( tview_on_key_pressed ), self );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->treeview ));
	g_signal_connect( select, "changed", G_CALLBACK( tview_on_row_selected ), self );
}

static void
init_context_menu( ofaTVBin *self )
{
	ofaTVBinPrivate *priv;

	priv = ofa_tvbin_get_instance_private( self );

	/* create the menu model */
	priv->context_menu = g_menu_new();
	priv->action_group = g_simple_action_group_new();

	/* add ofaTVBin-specific items */

	/* let the ofaITVColumnable interface add its items */
	ofa_itvcolumnable_set_context_menu(
			OFA_ITVCOLUMNABLE( self ), priv->context_menu, G_ACTION_GROUP( priv->action_group ), st_group_name );
}

static void
ofa_tvbin_class_init( ofaTVBinClass *klass )
{
	static const gchar *thisfn = "ofa_tvbin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = bin_finalize;

	/**
	 * ofaTVBin::ofa-changed:
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
	 * ofaTVBin::ofa-activated:
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
	 * ofaTVBin::ofa-delete:
	 *
	 * This signal is sent on the #ofaTVBin when the Delete key is
	 * pressed.
	 *
	 * Argument is the current #GtkTreeSelection.
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
	g_signal_emit_by_name( self, "ofa-selactivated", selection );
}

/*
 * Opens a context menu.
 * cf. https://developer.gnome.org/gtk3/stable/gtk-migrating-checklist.html#checklist-popup-menu
 */
static gboolean
tview_on_button_pressed( GtkWidget *treeview, GdkEventButton *event, ofaTVBin *self )
{
	ofaTVBinPrivate *priv;
	gboolean stop;

	stop = FALSE;
	priv = ofa_tvbin_get_instance_private( self );

	/* Ignore double-clicks and triple-clicks */
	if( gdk_event_triggers_context_menu(( GdkEvent * ) event ) &&
			event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_SECONDARY ){

		if( !priv->popup_menu ){
			priv->popup_menu = gtk_menu_new_from_model( G_MENU_MODEL( priv->context_menu ));
			gtk_widget_insert_action_group( priv->popup_menu, st_group_name, G_ACTION_GROUP( priv->action_group ));
			if( 0 ){
				dump_menu_model( self, priv, G_MENU_MODEL( priv->context_menu ));
			}
		}

		gtk_menu_popup( GTK_MENU( priv->popup_menu ), NULL, NULL, NULL, NULL, event->button, event->time );
		stop = TRUE;
	}

	return( stop );
}

static void
dump_menu_model( ofaTVBin *self, ofaTVBinPrivate *priv, GMenuModel *model )
{
	static const gchar *thisfn = "ofa_tvbin_dump_menu_model";
	gint i, count;
	GMenuAttributeIter *attribute_iter;
	GMenuLinkIter *link_iter;
	const gchar *attribute_name, *link_name;
	GMenuModel *link_model;
	GVariant *attribute_value;

	/* iterate through items */
	count = g_menu_model_get_n_items( model );
	g_debug( "%s: model=%p, items_count=%d", thisfn, ( void * ) model, count );
	for( i=0 ; i<count ; ++i ){

		/* iterate through attributes for this item */
		attribute_iter = g_menu_model_iterate_item_attributes( model, i );
		while( g_menu_attribute_iter_get_next( attribute_iter, &attribute_name, &attribute_value )){
			g_debug( "%s: i=%d, attribute_name=%s, attribute_value=%s", thisfn, i, attribute_name, g_variant_get_string( attribute_value, NULL ));
			g_variant_unref( attribute_value );
		}
		g_object_unref( attribute_iter );

		/* iterates through links for this item */
		link_iter = g_menu_model_iterate_item_links( model, i );
		while( g_menu_link_iter_get_next( link_iter, &link_name, &link_model )){
			g_debug( "%s: i=%d, link_name=%s, link_model=%p", thisfn, i, link_name, ( void * ) link_model );
			dump_menu_model( self, priv, link_model );
			g_object_unref( link_model );
		}
		g_object_unref( link_iter );
	}
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
			g_signal_emit_by_name( self, "ofa-seldelete", selection );

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

static void
add_column( ofaTVBin *self, GtkTreeViewColumn *column, gint column_id, const gchar *menu )
{
	ofaTVBinPrivate *priv;

	priv = ofa_tvbin_get_instance_private( self );

	gtk_tree_view_column_set_visible( column, FALSE );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	gtk_tree_view_column_set_reorderable( column, TRUE );
	gtk_tree_view_column_set_sizing( column, GTK_TREE_VIEW_COLUMN_GROW_ONLY );

	gtk_tree_view_append_column( GTK_TREE_VIEW( priv->treeview ), column );

	ofa_itvcolumnable_add_column( OFA_ITVCOLUMNABLE( self ), column, menu );
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
