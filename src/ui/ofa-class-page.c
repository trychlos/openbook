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

#include "my/my-utils.h"

#include "api/ofa-buttons-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofo-class.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-class-properties.h"
#include "ui/ofa-class-page.h"

/* private instance data
 */
typedef struct {

	/* internals
	 */
	ofaHub      *hub;
	GList       *hub_handlers;
	gboolean     is_writable;

	/* UI
	 */
	GtkTreeView *tview;					/* the main treeview of the page */
	GtkWidget   *update_btn;
	GtkWidget   *delete_btn;
}
	ofaClassPagePrivate;

/* column ordering in the selection listview
 */
enum {
	COL_ID = 0,
	COL_NUMBER,
	COL_LABEL,
	COL_OBJECT,
	N_COLUMNS
};

static GtkWidget *v_setup_view( ofaPage *page );
static GtkWidget *setup_tree_view( ofaPage *page );
static gboolean   on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaClassPage *self );
static GtkWidget *v_setup_buttons( ofaPage *page );
static ofoClass  *tview_get_selected( ofaClassPage *page, GtkTreeModel **tmodel, GtkTreeIter *iter );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       insert_dataset( ofaClassPage *self );
static void       insert_new_row( ofaClassPage *self, ofoClass *class, gboolean with_selection );
static gint       on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaClassPage *self );
static void       setup_first_selection( ofaClassPage *self );
static void       on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page );
static void       on_row_selected( GtkTreeSelection *selection, ofaClassPage *self );
static void       on_new_clicked( GtkButton *button, ofaClassPage *page );
static void       on_update_clicked( GtkButton *button, ofaClassPage *page );
static void       on_delete_clicked( GtkButton *button, ofaClassPage *page );
static void       try_to_delete_current_row( ofaClassPage *self );
static gboolean   delete_confirmed( ofaClassPage *self, ofoClass *class );
static void       do_delete( ofaClassPage *page, ofoClass *class, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void       on_hub_new_object( ofaHub *hub, ofoBase *object, ofaClassPage *self );
static void       on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaClassPage *self );
static void       on_hub_deleted_object( ofaHub *hub, ofoBase *object, ofaClassPage *self );
static void       on_hub_reload_dataset( ofaHub *hub, GType type, ofaClassPage *self );
static gboolean   find_row_by_id( ofaClassPage *self, gint id, GtkTreeModel **tmodel, GtkTreeIter *iter );

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
	ofaClassPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_CLASS_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = ofa_class_page_get_instance_private( OFA_CLASS_PAGE( instance ));

		ofa_hub_disconnect_handlers( priv->hub, &priv->hub_handlers );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_class_page_parent_class )->dispose( instance );
}

static void
ofa_class_page_init( ofaClassPage *self )
{
	static const gchar *thisfn = "ofa_class_page_init";
	ofaClassPagePrivate *priv;

	g_return_if_fail( self && OFA_IS_CLASS_PAGE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_class_page_get_instance_private( self );

	priv->hub_handlers = NULL;
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
	gulong handler;
	GtkWidget *tview;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_class_page_get_instance_private( OFA_CLASS_PAGE( page ));

	priv->hub = ofa_igetter_get_hub( OFA_IGETTER( page ));
	g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );
	priv->is_writable = ofa_hub_dossier_is_writable( priv->hub );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_NEW, G_CALLBACK( on_hub_new_object ), page );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_UPDATED, G_CALLBACK( on_hub_updated_object ), page );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_DELETED, G_CALLBACK( on_hub_deleted_object ), page );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_RELOAD, G_CALLBACK( on_hub_reload_dataset ), page );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	tview = setup_tree_view( page );

	insert_dataset( OFA_CLASS_PAGE( page ));

	return( tview );
}

static GtkWidget *
setup_tree_view( ofaPage *page )
{
	ofaClassPagePrivate *priv;
	GtkFrame *frame;
	GtkScrolledWindow *scroll;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = ofa_class_page_get_instance_private( OFA_CLASS_PAGE( page ));

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
	g_signal_connect(
			G_OBJECT( tview ), "row-activated", G_CALLBACK( on_row_activated ), page );
	g_signal_connect(
			G_OBJECT( tview ), "key-press-event", G_CALLBACK( on_tview_key_pressed ), page );
	priv->tview = tview;

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( tview, tmodel );
	g_object_unref( tmodel );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Number" ),
			text_cell, "text", COL_NUMBER,
			NULL );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( tview, column );

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_row_selected ), page );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ), ( GtkTreeIterCompareFunc ) on_sort_model, page, NULL );

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	return( GTK_WIDGET( frame ));
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaClassPage *self )
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
		gtk_tree_model_get( *tmodel, iter, COL_OBJECT, &class, -1 );
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
insert_dataset( ofaClassPage *self )
{
	ofaClassPagePrivate *priv;
	GList *dataset, *iset;
	ofoClass *class;

	priv = ofa_class_page_get_instance_private( self );

	dataset = ofo_class_get_dataset( priv->hub );

	for( iset=dataset ; iset ; iset=iset->next ){
		class = OFO_CLASS( iset->data );
		insert_new_row( self, class, FALSE );
	}

	setup_first_selection( self );
}

static void
insert_new_row( ofaClassPage *self, ofoClass *class, gboolean with_selection )
{
	ofaClassPagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;
	gint id;
	gchar *str;

	priv = ofa_class_page_get_instance_private( self );

	tmodel = gtk_tree_view_get_model( priv->tview );

	id = ofo_class_get_number( class );
	str = g_strdup_printf( "%d", id );

	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( tmodel ),
			&iter,
			-1,
			COL_ID,     id,
			COL_NUMBER, str,
			COL_LABEL,  ofo_class_get_label( class ),
			COL_OBJECT, class,
			-1 );

	g_free( str );

	/* select the newly added class */
	if( with_selection ){
		path = gtk_tree_model_get_path( tmodel, &iter );
		gtk_tree_view_set_cursor( priv->tview, path, NULL, FALSE );
		gtk_tree_path_free( path );
		gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
	}
}

static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaClassPage *self )
{
	gint anum, bnum;

	gtk_tree_model_get( tmodel, a, COL_ID, &anum, -1 );
	gtk_tree_model_get( tmodel, b, COL_ID, &bnum, -1 );

	return( anum < bnum ? -1 : ( anum > bnum ? 1 : 0 ));
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
		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &class, -1 );
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

		/* remove the row from the tmodel
		 * this will cause an automatic new selection */
		gtk_list_store_remove( GTK_LIST_STORE( tmodel ), iter );
	}
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
on_hub_new_object( ofaHub *hub, ofoBase *object, ofaClassPage *self )
{
	static const gchar *thisfn = "ofa_class_page_on_hub_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_CLASS( object )){
		insert_new_row( self, OFO_CLASS( object ), TRUE );
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 *
 * Modifying the class number is forbidden
 */
static void
on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaClassPage *self )
{
	static const gchar *thisfn = "ofa_class_page_on_hub_updated_object";
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gint prev_num, class_num;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_CLASS( object )){

		prev_num = atoi( prev_id );
		class_num = ofo_class_get_number( OFO_CLASS( object ));

		if( find_row_by_id( self, prev_num, &tmodel, &iter )){
			if( prev_num != class_num ){
				gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
				insert_new_row( self, OFO_CLASS( object ), TRUE );
			} else if( find_row_by_id( self, class_num, &tmodel, &iter )){
				gtk_list_store_set(
						GTK_LIST_STORE( tmodel ),
						&iter,
						COL_LABEL,  ofo_class_get_label( OFO_CLASS( object )),
						-1 );
			}
		}
	}
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
on_hub_deleted_object( ofaHub *hub, ofoBase *object, ofaClassPage *self )
{
	static const gchar *thisfn = "ofa_class_page_on_hub_deleted_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
on_hub_reload_dataset( ofaHub *hub, GType type, ofaClassPage *self )
{
	static const gchar *thisfn = "ofa_class_page_on_hub_reload_dataset";
	ofaClassPagePrivate *priv;
	GtkTreeModel *tmodel;

	g_debug( "%s: hub=%p, type=%lu, self=%p",
			thisfn,
			( void * ) hub,
			type,
			( void * ) self );

	priv = ofa_class_page_get_instance_private( self );

	if( type == OFO_TYPE_CLASS ){
		tmodel = gtk_tree_view_get_model( priv->tview );
		gtk_list_store_clear( GTK_LIST_STORE( tmodel ));
		insert_dataset( self );
	}
}

static gboolean
find_row_by_id( ofaClassPage *self, gint id, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	static const gchar *thisfn = "ofa_class_page_find_row_by_id";
	ofaClassPagePrivate *priv;
	gint num;

	priv = ofa_class_page_get_instance_private( self );

	*tmodel = gtk_tree_view_get_model( priv->tview );

	if( gtk_tree_model_get_iter_first( *tmodel, iter )){
		while( TRUE ){
			gtk_tree_model_get( *tmodel, iter, COL_ID, &num, -1 );
			if( num == id ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( *tmodel, iter )){
				return( FALSE );
			}
		}
	}

	g_warning( "%s: id=%d not found", thisfn, id );
	return( FALSE );
}
