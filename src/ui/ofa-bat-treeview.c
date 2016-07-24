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
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-isortable.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-bat.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-bat-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean           dispose_has_run;

	/* runtime data
	 */
	gboolean           delete_authorized;

	/* UI
	 */
	GtkTreeView       *tview;
	ofaBatStore       *store;
	gchar             *settings_key;
}
	ofaBatTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void         isortable_iface_init( ofaISortableInterface *iface );
static guint        isortable_get_interface_version( void );
static gint         isortable_sort_model( const ofaISortable *instance, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );
static void         attach_top_widget( ofaBatTreeview *self );
static void         on_row_selected( GtkTreeSelection *selection, ofaBatTreeview *self );
static void         on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaBatTreeview *self );
static void         get_and_send( ofaBatTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static gboolean     on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaBatTreeview *self );
static void         try_to_delete_current_row( ofaBatTreeview *self );
static gboolean     delete_confirmed( ofaBatTreeview *self, ofoBat *bat );
static const gchar *get_default_settings_key( ofaBatTreeview *self );

G_DEFINE_TYPE_EXTENDED( ofaBatTreeview, ofa_bat_treeview, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaBatTreeview )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISORTABLE, isortable_iface_init ))

static void
bat_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_bat_treeview_finalize";
	ofaBatTreeviewPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BAT_TREEVIEW( instance ));

	/* free data members here */
	priv = ofa_bat_treeview_get_instance_private( OFA_BAT_TREEVIEW( instance ));

	g_free( priv->settings_key );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_treeview_parent_class )->finalize( instance );
}

static void
bat_treeview_dispose( GObject *instance )
{
	ofaBatTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_BAT_TREEVIEW( instance ));

	priv = ofa_bat_treeview_get_instance_private( OFA_BAT_TREEVIEW( instance ));

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
	ofaBatTreeviewPrivate *priv;

	g_return_if_fail( self && OFA_IS_BAT_TREEVIEW( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_bat_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_key = g_strdup( get_default_settings_key( self ));
}

static void
ofa_bat_treeview_class_init( ofaBatTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_bat_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_treeview_finalize;

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
 * ofaISortable interface management
 */
static void
isortable_iface_init( ofaISortableInterface *iface )
{
	static const gchar *thisfn = "ofa_bat_treeview_isortable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = isortable_get_interface_version;
	iface->sort_model = isortable_sort_model;
}

static guint
isortable_get_interface_version( void )
{
	return( 1 );
}

static gint
isortable_sort_model( const ofaISortable *instance, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_bat_treeview_isortable_sort_model";
	gint cmp;
	gchar *ida, *uria, *formata, *begina, *enda, *riba, *cura, *bsoldea, *esoldea, *notesa, *counta, *unuseda, *accounta, *updusera, *updstampa;
	gchar *idb, *urib, *formatb, *beginb, *endb, *ribb, *curb, *bsoldeb, *esoldeb, *notesb, *countb, *unusedb, *accountb, *upduserb, *updstampb;
	GdkPixbuf *pnga, *pngb;

	gtk_tree_model_get( tmodel, a,
			BAT_COL_ID,             &ida,
			BAT_COL_URI,            &uria,
			BAT_COL_FORMAT,         &formata,
			BAT_COL_BEGIN,          &begina,
			BAT_COL_END,            &enda,
			BAT_COL_RIB,            &riba,
			BAT_COL_CURRENCY,       &cura,
			BAT_COL_BEGIN_SOLDE,    &bsoldea,
			BAT_COL_END_SOLDE,      &esoldea,
			BAT_COL_NOTES,          &notesa,
			BAT_COL_NOTES_PNG,      &pnga,
			BAT_COL_COUNT,          &counta,
			BAT_COL_UNUSED,         &unuseda,
			BAT_COL_ACCOUNT,        &accounta,
			BAT_COL_UPD_USER,       &updusera,
			BAT_COL_UPD_STAMP,      &updstampa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			BAT_COL_ID,             &idb,
			BAT_COL_URI,            &urib,
			BAT_COL_FORMAT,         &formatb,
			BAT_COL_BEGIN,          &beginb,
			BAT_COL_END,            &endb,
			BAT_COL_RIB,            &ribb,
			BAT_COL_CURRENCY,       &curb,
			BAT_COL_BEGIN_SOLDE,    &bsoldeb,
			BAT_COL_END_SOLDE,      &esoldeb,
			BAT_COL_NOTES,          &notesb,
			BAT_COL_NOTES_PNG,      &pngb,
			BAT_COL_COUNT,          &countb,
			BAT_COL_UNUSED,         &unusedb,
			BAT_COL_ACCOUNT,        &accountb,
			BAT_COL_UPD_USER,       &upduserb,
			BAT_COL_UPD_STAMP,      &updstampb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case BAT_COL_ID:
			cmp = ofa_isortable_sort_str_int( ida, idb );
			break;
		case BAT_COL_URI:
			cmp = my_collate( uria, urib );
			break;
		case BAT_COL_FORMAT:
			cmp = my_collate( formata, formatb );
			break;
		case BAT_COL_BEGIN:
			cmp = my_date_compare_by_str( begina, beginb, ofa_prefs_date_display());
			break;
		case BAT_COL_END:
			cmp = my_date_compare_by_str( enda, endb, ofa_prefs_date_display());
			break;
		case BAT_COL_RIB:
			cmp = my_collate( riba, ribb );
			break;
		case BAT_COL_CURRENCY:
			cmp = my_collate( cura, curb );
			break;
		case BAT_COL_BEGIN_SOLDE:
			cmp = ofa_isortable_sort_str_amount( bsoldea, bsoldeb );
			break;
		case BAT_COL_END_SOLDE:
			cmp = ofa_isortable_sort_str_amount( esoldea, esoldeb );
			break;
		case BAT_COL_NOTES:
			cmp = my_collate( begina, beginb );
			break;
		case BAT_COL_NOTES_PNG:
			cmp = ofa_isortable_sort_png( pnga, pngb );
			break;
		case BAT_COL_COUNT:
			cmp = ofa_isortable_sort_str_int( counta, countb );
			break;
		case BAT_COL_UNUSED:
			cmp = ofa_isortable_sort_str_int( unuseda, unusedb );
			break;
		case BAT_COL_ACCOUNT:
			cmp = my_collate( accounta, accountb );
			break;
		case BAT_COL_UPD_USER:
			cmp = my_collate( updusera, upduserb );
			break;
		case BAT_COL_UPD_STAMP:
			cmp = my_collate( updstampa, updstampb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( ida );
	g_free( uria );
	g_free( formata );
	g_free( begina );
	g_free( enda );
	g_free( riba );
	g_free( cura );
	g_free( bsoldea );
	g_free( esoldea );
	g_free( notesa );
	g_free( counta );
	g_free( unuseda );
	g_free( accounta );
	g_free( updusera );
	g_free( updstampa );
	g_object_unref( pnga );

	g_free( idb );
	g_free( urib );
	g_free( formatb );
	g_free( beginb );
	g_free( endb );
	g_free( ribb );
	g_free( curb );
	g_free( bsoldeb );
	g_free( esoldeb );
	g_free( notesb );
	g_free( countb );
	g_free( unusedb );
	g_free( accountb );
	g_free( upduserb );
	g_free( updstampb );
	g_object_unref( pngb );

	return( cmp );
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

	priv = ofa_bat_treeview_get_instance_private( self );

	top_widget = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );

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

	g_signal_connect( priv->tview, "row-activated", G_CALLBACK( on_row_activated ), self );
	g_signal_connect( priv->tview, "key-press-event", G_CALLBACK( on_tview_key_pressed ), self );

	select = gtk_tree_view_get_selection( priv->tview );
	g_signal_connect( select, "changed", G_CALLBACK( on_row_selected ), self );

	gtk_container_add( GTK_CONTAINER( self ), top_widget );
}

/**
 * ofa_bat_treeview_set_columns:
 * @view: this #ofaBatTreeview view.
 * @columns: a -1-terminated array of the columns to be displayed.
 *
 * Initialize the displayed columns.
 */
void
ofa_bat_treeview_set_columns( ofaBatTreeview *view, const gint *columns )
{
	ofaBatTreeviewPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	gint i;

	g_return_if_fail( view && OFA_IS_BAT_TREEVIEW( view ));
	g_return_if_fail( columns );

	priv = ofa_bat_treeview_get_instance_private( view );
	g_return_if_fail( !priv->dispose_has_run );

	for( i=0 ; columns[i] >= 0 ; ++i ){

		if( columns[i] == BAT_COL_ID ){
			cell = gtk_cell_renderer_text_new();
			gtk_cell_renderer_set_alignment( cell, 1.0, 0.5 );
			column = gtk_tree_view_column_new_with_attributes(
							_( "Id." ), cell, "text", columns[i], NULL );
			gtk_tree_view_column_set_alignment( column, 1.0 );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
		}

		if( columns[i] == BAT_COL_URI ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "URI" ), cell, "text", columns[i], NULL );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
		}

		if( columns[i] == BAT_COL_FORMAT ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "Format" ), cell, "text", columns[i], NULL );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
		}

		if( columns[i] == BAT_COL_BEGIN ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "Begin" ), cell, "text", columns[i], NULL );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
		}

		if( columns[i] == BAT_COL_END ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "End" ), cell, "text", columns[i], NULL );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
		}

		if( columns[i] == BAT_COL_COUNT ){
			cell = gtk_cell_renderer_text_new();
			gtk_cell_renderer_set_alignment( cell, 1.0, 0.5 );
			column = gtk_tree_view_column_new_with_attributes(
							_( "Count" ), cell, "text", columns[i], NULL );
			gtk_tree_view_column_set_alignment( column, 1.0 );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
		}

		if( columns[i] == BAT_COL_UNUSED ){
			cell = gtk_cell_renderer_text_new();
			gtk_cell_renderer_set_alignment( cell, 1.0, 0.5 );
			column = gtk_tree_view_column_new_with_attributes(
							_( "Unused" ), cell, "text", columns[i], NULL );
			gtk_tree_view_column_set_alignment( column, 1.0 );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
		}

		if( columns[i] == BAT_COL_RIB ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "RIB" ), cell, "text", columns[i], NULL );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
		}

		if( columns[i] == BAT_COL_BEGIN_SOLDE ){
			cell = gtk_cell_renderer_text_new();
			gtk_cell_renderer_set_alignment( cell, 1.0, 0.5 );
			column = gtk_tree_view_column_new_with_attributes(
							_( "Begin solde" ), cell, "text", columns[i], NULL );
			gtk_tree_view_column_set_alignment( column, 1.0 );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
		}

		if( columns[i] == BAT_COL_END_SOLDE ){
			cell = gtk_cell_renderer_text_new();
			gtk_cell_renderer_set_alignment( cell, 1.0, 0.5 );
			column = gtk_tree_view_column_new_with_attributes(
							_( "End solde" ), cell, "text", columns[i], NULL );
			gtk_tree_view_column_set_alignment( column, 1.0 );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
		}

		if( columns[i] == BAT_COL_CURRENCY ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "Cur." ), cell, "text", BAT_COL_CURRENCY, NULL );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
		}

		if( columns[i] == BAT_COL_ACCOUNT ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "Account" ), cell, "text", BAT_COL_ACCOUNT, NULL );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
		}

		if( columns[i] == BAT_COL_NOTES ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "Notes" ), cell, "text", BAT_COL_NOTES, NULL );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
		}

		if( columns[i] == BAT_COL_NOTES_PNG ){
			cell = gtk_cell_renderer_pixbuf_new();
			column = gtk_tree_view_column_new_with_attributes(
							"", cell, "pixbuf", BAT_COL_NOTES_PNG, NULL );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
		}

		if( columns[i] == BAT_COL_UPD_USER ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "User" ), cell, "text", BAT_COL_UPD_USER, NULL );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
		}

		if( columns[i] == BAT_COL_UPD_STAMP ){
			cell = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(
							_( "Timestamp" ), cell, "text", BAT_COL_UPD_STAMP, NULL );
			gtk_tree_view_append_column( priv->tview, column );
			gtk_tree_view_column_set_sort_column_id( column, columns[i] );
		}
	}

	gtk_widget_show_all( GTK_WIDGET( view ));
}

/**
 * ofa_bat_treeview_set_settings_key:
 * @view: this #ofaBatTreeview instance.
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
ofa_bat_treeview_set_settings_key( ofaBatTreeview *view, const gchar *key )
{
	ofaBatTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_BAT_TREEVIEW( view ));

	priv = ofa_bat_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->settings_key );
	priv->settings_key = g_strdup( my_strlen( key ) ? key : get_default_settings_key( view ));

	ofa_isortable_set_settings_key( OFA_ISORTABLE( view ), priv->settings_key );
}

/**
 * ofa_bat_treeview_set_hub:
 * @view: this #ofaBatTreeview instance.
 * @hub: the current #ofaHub object.
 *
 * Setup the current hub.
 * Initialize the underlying store.
 */
void
ofa_bat_treeview_set_hub( ofaBatTreeview *view, ofaHub *hub )
{
	ofaBatTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_BAT_TREEVIEW( view ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_bat_treeview_get_instance_private( view );
	g_return_if_fail( !priv->dispose_has_run );

	priv->store = ofa_bat_store_new( hub );

	ofa_isortable_set_treeview( OFA_ISORTABLE( view ), priv->tview );
	ofa_isortable_add_sortable_column( OFA_ISORTABLE( view ), BAT_COL_NOTES_PNG );
	ofa_isortable_set_default_sort( OFA_ISORTABLE( view ), BAT_COL_ID, GTK_SORT_DESCENDING );
	ofa_isortable_set_store( OFA_ISORTABLE( view ), OFA_ISTORE( priv->store ));
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

/**
 * ofa_bat_treeview_get_selected:
 * @view: this #ofaBatTreeview instance.
 *
 * Return: the identifier of the currently selected BAT file, or 0.
 */
ofoBat *
ofa_bat_treeview_get_selected( ofaBatTreeview *view )
{
	static const gchar *thisfn = "ofa_bat_treeview_get_selected";
	ofaBatTreeviewPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreeSelection *select;
	ofoBat *bat;

	g_debug( "%s: view=%p", thisfn, ( void * ) view );

	g_return_val_if_fail( view && OFA_IS_BAT_TREEVIEW( view ), NULL );

	priv = ofa_bat_treeview_get_instance_private( view );
	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	bat = NULL;
	select = gtk_tree_view_get_selection( priv->tview );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, BAT_COL_OBJECT, &bat, -1 );
		g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );
		g_object_unref( bat );
	}

	return( bat );
}

/**
 * ofa_bat_treeview_set_selected:
 * @view: this #ofaBatTreeview instance.
 * @id: the BAT identifier to be selected.
 *
 * Selects the BAT file identified by @id.
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

	priv = ofa_bat_treeview_get_instance_private( view );
	g_return_if_fail( !priv->dispose_has_run );

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
				gtk_tree_view_set_cursor( priv->tview, path, NULL, FALSE );
				gtk_tree_path_free( path );
				break;
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( priv->store ), &iter )){
				break;
			}
		}
	}
}

/**
 * ofa_bat_treeview_get_treeview:
 * @view: this #ofaBatTreeview instance.
 *
 * Returns: the #GtkTreeView widget.
 */
GtkWidget *
ofa_bat_treeview_get_treeview( ofaBatTreeview *view )
{
	ofaBatTreeviewPrivate *priv;

	g_return_val_if_fail( view && OFA_IS_BAT_TREEVIEW( view ), NULL );

	priv = ofa_bat_treeview_get_instance_private( view );
	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( GTK_WIDGET( priv->tview ));
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

	priv = ofa_bat_treeview_get_instance_private( self );

	if( priv->delete_authorized ){
		bat = ofa_bat_treeview_get_selected( self );
		if( bat && ofo_bat_is_deletable( bat )){
			ofa_bat_treeview_delete_bat( self, bat );
		}
	}
}

/**
 * ofa_bat_treeview_delete_bat:
 * @view: this #ofaBatTreeview instance.
 * @bat: a #ofoBat object.
 *
 * Deletes the @bat object after user confirmation.
 */
void
ofa_bat_treeview_delete_bat( ofaBatTreeview *view, ofoBat *bat )
{
	ofaBatTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_BAT_TREEVIEW( view ));
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

	priv = ofa_bat_treeview_get_instance_private( view );
	g_return_if_fail( !priv->dispose_has_run );

	if( delete_confirmed( view, bat )){
		ofo_bat_delete( bat );
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

/*
 * return the default settings key
 */
static const gchar *
get_default_settings_key( ofaBatTreeview *self )
{
	return( G_OBJECT_TYPE_NAME( self ));
}
