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

#include "my/my-utils.h"

#include "api/ofa-buttons-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-settings.h"
#include "api/ofo-class.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-class-properties.h"
#include "ui/ofa-class-page.h"
#include "ui/ofa-class-store.h"

/* private instance data
 */
typedef struct {

	/* internals
	 */
	ofaHub            *hub;
	gboolean           is_writable;

	/* UI
	 */
	ofaClassStore     *store;
	GtkTreeView       *tview;				/* the main treeview of the page */
	GtkWidget         *update_btn;
	GtkWidget         *delete_btn;

	/* sorted model
	 */
	GtkTreeModel      *sort_model;			/* a sort model stacked on top of the store */
	GtkTreeViewColumn *sort_column;
	gint               sort_column_id;
	gint               sort_sens;
}
	ofaClassPagePrivate;

static GtkWidget *v_setup_view( ofaPage *page );
static GtkWidget *setup_tree_view( ofaClassPage *self );
static void       tview_on_header_clicked( GtkTreeViewColumn *column, ofaClassPage *self );
static void       tview_set_sort_indicator( ofaClassPage *self );
static gint       tview_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaClassPage *self );
static gint       tview_on_sort_int( const gchar *a, const gchar *b );
static gint       tview_on_sort_png( const GdkPixbuf *pnga, const GdkPixbuf *pngb );
static gboolean   tview_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaClassPage *self );
static GtkWidget *v_setup_buttons( ofaPage *page );
static ofoClass  *tview_get_selected( ofaClassPage *page, GtkTreeModel **tmodel, GtkTreeIter *iter );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       setup_first_selection( ofaClassPage *self );
static void       on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page );
static void       on_row_selected( GtkTreeSelection *selection, ofaClassPage *self );
static void       on_new_clicked( GtkButton *button, ofaClassPage *page );
static void       on_update_clicked( GtkButton *button, ofaClassPage *page );
static void       on_delete_clicked( GtkButton *button, ofaClassPage *page );
static void       try_to_delete_current_row( ofaClassPage *self );
static gboolean   delete_confirmed( ofaClassPage *self, ofoClass *class );
static void       do_delete( ofaClassPage *page, ofoClass *class, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void       get_sort_settings( ofaClassPage *self );
static void       set_sort_settings( ofaClassPage *self );

G_DEFINE_TYPE_EXTENDED( ofaClassPage, ofa_class_page, OFA_TYPE_PAGE, 0,
		G_ADD_PRIVATE( ofaClassPage ))

static void
classes_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_class_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CLASS_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_class_page_parent_class )->finalize( instance );
}

static void
classes_page_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_CLASS_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_class_page_parent_class )->dispose( instance );
}

static void
ofa_class_page_init( ofaClassPage *self )
{
	static const gchar *thisfn = "ofa_class_page_init";

	g_return_if_fail( self && OFA_IS_CLASS_PAGE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_class_page_class_init( ofaClassPageClass *klass )
{
	static const gchar *thisfn = "ofa_class_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = classes_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = classes_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_class_page_v_setup_view";
	ofaClassPagePrivate *priv;
	GtkWidget *tview;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_class_page_get_instance_private( OFA_CLASS_PAGE( page ));

	priv->hub = ofa_igetter_get_hub( OFA_IGETTER( page ));
	g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );
	priv->is_writable = ofa_hub_dossier_is_writable( priv->hub );

	tview = setup_tree_view( OFA_CLASS_PAGE( page ));
	setup_first_selection( OFA_CLASS_PAGE( page ));

	return( tview );
}

static GtkWidget *
setup_tree_view( ofaClassPage *self )
{
	ofaClassPagePrivate *priv;
	GtkFrame *frame;
	GtkScrolledWindow *scroll;
	GtkTreeView *tview;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	gint column_id;

	priv = ofa_class_page_get_instance_private( self );

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	my_utils_widget_set_margins( GTK_WIDGET( frame ), 4, 4, 4, 0 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_container_set_border_width( GTK_CONTAINER( scroll ), 4 );
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( scroll ));

	tview = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_hexpand( GTK_WIDGET( tview ), TRUE );
	gtk_widget_set_vexpand( GTK_WIDGET( tview ), TRUE );
	gtk_tree_view_set_headers_visible( tview, TRUE );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( tview ));

	g_signal_connect( tview, "row-activated", G_CALLBACK( on_row_activated ), self );
	g_signal_connect( tview, "key-press-event", G_CALLBACK( tview_on_key_pressed ), self );
	priv->tview = tview;

	column_id = CLASS_COL_NUMBER;
	cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Number" ),
			cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );

	column_id = CLASS_COL_LABEL;
	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );

	column_id = CLASS_COL_NOTES_PNG;
	cell = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(
				"", cell, "pixbuf", column_id, NULL );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( tview_on_header_clicked ), self );

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_row_selected ), self );

	priv->store = ofa_class_store_new( priv->hub );

	priv->sort_model = gtk_tree_model_sort_new_with_model( GTK_TREE_MODEL( priv->store ));
	/* the sortable model maintains its own reference to the store */
	g_object_unref( priv->store );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( priv->sort_model ), ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->sort_model ), CLASS_COL_NOTES_PNG, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );

	gtk_tree_view_set_model( priv->tview, priv->sort_model );
	/* the treeview maintains its own reference to the sortable model */
	g_object_unref( priv->sort_model );

	get_sort_settings( self );
	tview_set_sort_indicator( self );

	return( GTK_WIDGET( frame ));
}

static void
tview_on_header_clicked( GtkTreeViewColumn *column, ofaClassPage *self )
{
	ofaClassPagePrivate *priv;

	priv = ofa_class_page_get_instance_private( self );

	if( column != priv->sort_column ){
		gtk_tree_view_column_set_sort_indicator( priv->sort_column, FALSE );
		priv->sort_column = column;
		priv->sort_column_id = gtk_tree_view_column_get_sort_column_id( column );
		priv->sort_sens = GTK_SORT_ASCENDING;

	} else {
		priv->sort_sens = priv->sort_sens == GTK_SORT_ASCENDING ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING;
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
tview_set_sort_indicator( ofaClassPage *self )
{
	ofaClassPagePrivate *priv;

	priv = ofa_class_page_get_instance_private( self );

	if( priv->sort_model ){
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( priv->sort_model ), priv->sort_column_id, priv->sort_sens );
	}

	if( priv->sort_column ){
		gtk_tree_view_column_set_sort_indicator(
				priv->sort_column, TRUE );
		gtk_tree_view_column_set_sort_order(
				priv->sort_column,
				priv->sort_sens == GTK_SORT_ASCENDING ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING );
	}
}

static gint
tview_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaClassPage *self )
{
	static const gchar *thisfn = "ofa_class_page_tview_on_sort_model";
	ofaClassPagePrivate *priv;
	gchar *numa, *labela;
	gchar *numb, *labelb;
	gint ida, idb;
	GdkPixbuf *pnga, *pngb;
	gint cmp;

	gtk_tree_model_get( tmodel, a,
			CLASS_COL_ID,        &ida,
			CLASS_COL_NUMBER,    &numa,
			CLASS_COL_LABEL,     &labela,
			CLASS_COL_NOTES_PNG, &pnga,
			-1 );

	gtk_tree_model_get( tmodel, b,
			CLASS_COL_ID,        &idb,
			CLASS_COL_NUMBER,    &numb,
			CLASS_COL_LABEL,     &labelb,
			CLASS_COL_NOTES_PNG, &pngb,
			-1 );

	cmp = 0;
	priv = ofa_class_page_get_instance_private( self );

	switch( priv->sort_column_id ){
		case CLASS_COL_ID:
			cmp = ( ida < idb ? -1 : ( ida > idb ? 1 : 0 ));
			break;
		case CLASS_COL_NUMBER:
			cmp = tview_on_sort_int( numa, numb );
			break;
		case CLASS_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case CLASS_COL_NOTES_PNG:
			cmp = tview_on_sort_png( pnga, pngb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, priv->sort_column_id );
			break;
	}

	g_free( numa );
	g_free( labela );
	g_object_unref( pnga );

	g_free( numb );
	g_free( labelb );
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
tview_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaClassPage *self )
{
	gboolean stop;

	stop = FALSE;

	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Insert ){
			on_new_clicked( NULL, self );

		} else if( event->keyval == GDK_KEY_Delete ){
			try_to_delete_current_row( self );
		}
	}

	return( stop );
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	ofaClassPagePrivate *priv;
	ofaButtonsBox *buttons_box;
	GtkWidget *btn;

	priv = ofa_class_page_get_instance_private( OFA_CLASS_PAGE( page ));

	buttons_box = ofa_buttons_box_new();
	my_utils_widget_set_margins( GTK_WIDGET( buttons_box ), 4, 4, 0, 0 );

	btn = ofa_buttons_box_add_button_with_mnemonic(
			buttons_box, BUTTON_NEW, G_CALLBACK( on_new_clicked ), page );
	gtk_widget_set_sensitive( btn, priv->is_writable );

	priv->update_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_PROPERTIES, G_CALLBACK( on_update_clicked ), page );

	priv->delete_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_DELETE, G_CALLBACK( on_delete_clicked ), page );

	return( GTK_WIDGET( buttons_box ));
}

static ofoClass *
tview_get_selected( ofaClassPage *page, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	ofaClassPagePrivate *priv;
	GtkTreeSelection *select;
	ofoClass *class;

	priv = ofa_class_page_get_instance_private( page );

	class = NULL;
	select = gtk_tree_view_get_selection( priv->tview );
	if( gtk_tree_selection_get_selected( select, tmodel, iter )){
		gtk_tree_model_get( *tmodel, iter, CLASS_COL_OBJECT, &class, -1 );
		g_object_unref( class );
	}
	return( class );
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaClassPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_CLASS_PAGE( page ), NULL );

	priv = ofa_class_page_get_instance_private( OFA_CLASS_PAGE( page ));

	return( GTK_WIDGET( priv->tview ));
}

static void
setup_first_selection( ofaClassPage *self )
{
	ofaClassPagePrivate *priv;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	priv = ofa_class_page_get_instance_private( self );

	model = gtk_tree_view_get_model( priv->tview );
	if( gtk_tree_model_get_iter_first( model, &iter )){
		select = gtk_tree_view_get_selection( priv->tview );
		gtk_tree_selection_select_iter( select, &iter );
	}

	gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
}

static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page )
{
	on_update_clicked( NULL, OFA_CLASS_PAGE( page ));
}

static void
on_row_selected( GtkTreeSelection *selection, ofaClassPage *self )
{
	ofaClassPagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoClass *class;
	gboolean is_class;

	class = NULL;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, CLASS_COL_OBJECT, &class, -1 );
		g_object_unref( class );
	}

	priv = ofa_class_page_get_instance_private( self );

	is_class = class && OFO_IS_CLASS( class );

	if( priv->update_btn ){
		gtk_widget_set_sensitive( priv->update_btn,
				is_class );
	}

	if( priv->delete_btn ){
		gtk_widget_set_sensitive( priv->delete_btn,
				priv->is_writable && is_class && ofo_class_is_deletable( class ));
	}
}

static void
on_new_clicked( GtkButton *button, ofaClassPage *self )
{
	static const gchar *thisfn = "ofa_class_page_on_new_clicked";
	ofoClass *class;
	GtkWindow *toplevel;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	class = ofo_class_new();
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_class_properties_run( OFA_IGETTER( self ), toplevel, class );
}

static void
on_update_clicked( GtkButton *button, ofaClassPage *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoClass *class;
	GtkWindow *toplevel;

	class = tview_get_selected( self, &tmodel, &iter );
	if( class ){
		toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
		ofa_class_properties_run( OFA_IGETTER( self ), toplevel, class );
	}
}

static void
on_delete_clicked( GtkButton *button, ofaClassPage *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoClass *class;

	class = tview_get_selected( self, &tmodel, &iter );
	if( class ){
		do_delete( self, class, tmodel, &iter );
	}
	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( self )));
}

static void
try_to_delete_current_row( ofaClassPage *self )
{
	ofoClass *class;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	class = tview_get_selected( self, &tmodel, &iter );
	if( ofo_class_is_deletable( class )){
		do_delete( self, class, tmodel, &iter );
	}
}

static gboolean
delete_confirmed( ofaClassPage *self, ofoClass *class )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s' class ?" ),
			ofo_class_get_label( class ));

	delete_ok = my_utils_dialog_question( msg, _( "_Delete" ));

	g_free( msg );

	return( delete_ok );
}

static void
do_delete( ofaClassPage *self, ofoClass *class, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gboolean deletable;

	deletable = ofo_class_is_deletable( class );
	g_return_if_fail( deletable );

	if( delete_confirmed( self, class )){
		ofo_class_delete( class );
	}
}

/*
 * sort_settings: sort_column_id;sort_sens;
 *
 * Note that we record the actual sort order (gtk_sort_ascending for
 * ascending order); only the sort indicator of the column is reversed.
 */
static void
get_sort_settings( ofaClassPage *self )
{
	ofaClassPagePrivate *priv;
	GList *slist, *it, *columns;
	const gchar *cstr;
	gchar *sort_key;
	GtkTreeViewColumn *column;

	priv = ofa_class_page_get_instance_private( self );

	/* default is to sort by ascending class number
	 */
	priv->sort_column = NULL;
	priv->sort_column_id = CLASS_COL_NUMBER;
	priv->sort_sens = GTK_SORT_ASCENDING;

	/* get the settings (if any)
	 */
	sort_key = g_strdup_printf( "%s-sort", G_OBJECT_TYPE_NAME( self ));
	slist = ofa_settings_user_get_string_list( sort_key );

	it = slist ? slist : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->sort_column_id = atoi( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->sort_sens = atoi( cstr );
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
set_sort_settings( ofaClassPage *self )
{
	ofaClassPagePrivate *priv;
	gchar *str, *sort_key;

	priv = ofa_class_page_get_instance_private( self );

	sort_key = g_strdup_printf( "%s-sort", G_OBJECT_TYPE_NAME( self ));
	str = g_strdup_printf( "%d;%d;", priv->sort_column_id, priv->sort_sens );

	ofa_settings_user_set_string( sort_key, str );

	g_free( str );
	g_free( sort_key );
}
