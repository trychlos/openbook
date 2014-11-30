/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>

#include "api/ofo-class.h"
#include "api/ofo-dossier.h"

#include "ui/my-buttons-box.h"
#include "ui/ofa-class-properties.h"
#include "ui/ofa-classes-page.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-page.h"
#include "ui/ofa-page-prot.h"

/* private instance data
 */
struct _ofaClassesPagePrivate {

	/* internals
	 */
	GList       *handlers;

	/* UI
	 */
	GtkTreeView *tview;					/* the main treeview of the page */
	GtkWidget   *update_btn;
	GtkWidget   *delete_btn;
};

/* column ordering in the selection listview
 */
enum {
	COL_ID = 0,
	COL_NUMBER,
	COL_LABEL,
	COL_OBJECT,
	N_COLUMNS
};

G_DEFINE_TYPE( ofaClassesPage, ofa_classes_page, OFA_TYPE_PAGE )

static GtkWidget *v_setup_view( ofaPage *page );
static GtkWidget *setup_tree_view( ofaPage *page );
static gboolean   on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaClassesPage *self );
static void       v_init_view( ofaPage *page );
static ofoClass  *tview_get_selected( ofaClassesPage *page, GtkTreeModel **tmodel, GtkTreeIter *iter );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       insert_dataset( ofaClassesPage *self );
static void       insert_new_row( ofaClassesPage *self, ofoClass *class, gboolean with_selection );
static gint       on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaClassesPage *self );
static void       setup_first_selection( ofaClassesPage *self );
static void       on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page );
static void       on_row_selected( GtkTreeSelection *selection, ofaClassesPage *self );
static void       v_on_button_clicked( ofaPage *page, guint button_id );
static void       on_new_clicked( ofaClassesPage *page );
static void       on_update_clicked( ofaClassesPage *page );
static void       on_delete_clicked( ofaClassesPage *page );
static void       try_to_delete_current_row( ofaClassesPage *self );
static gboolean   delete_confirmed( ofaClassesPage *self, ofoClass *class );
static void       do_delete( ofaClassesPage *page, ofoClass *class, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void       on_new_object( ofoDossier *dossier, ofoBase *object, ofaClassesPage *self );
static void       on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaClassesPage *self );
static void       on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaClassesPage *self );
static void       on_reloaded_dataset( ofoDossier *dossier, GType type, ofaClassesPage *self );
static gboolean   find_row_by_id( ofaClassesPage *self, gint id, GtkTreeModel **tmodel, GtkTreeIter *iter );

static void
classes_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_classes_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CLASSES_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_classes_page_parent_class )->finalize( instance );
}

static void
classes_page_dispose( GObject *instance )
{
	ofaClassesPagePrivate *priv;
	gulong handler_id;
	GList *iha;
	ofoDossier *dossier;

	g_return_if_fail( instance && OFA_IS_CLASSES_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = ( OFA_CLASSES_PAGE( instance ))->priv;

		/* note when deconnecting the handlers that the dossier may
		 * have been already finalized (e.g. when the application
		 * terminates) */
		dossier = ofa_page_get_dossier( OFA_PAGE( instance ));
		if( OFO_IS_DOSSIER( dossier )){
			for( iha=priv->handlers ; iha ; iha=iha->next ){
				handler_id = ( gulong ) iha->data;
				g_signal_handler_disconnect( dossier, handler_id );
			}
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_classes_page_parent_class )->dispose( instance );
}

static void
ofa_classes_page_init( ofaClassesPage *self )
{
	static const gchar *thisfn = "ofa_classes_page_init";

	g_return_if_fail( self && OFA_IS_CLASSES_PAGE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_CLASSES_PAGE, ofaClassesPagePrivate );
	self->priv->handlers = NULL;
}

static void
ofa_classes_page_class_init( ofaClassesPageClass *klass )
{
	static const gchar *thisfn = "ofa_classes_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = classes_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = classes_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;
	OFA_PAGE_CLASS( klass )->on_button_clicked = v_on_button_clicked;

	g_type_class_add_private( klass, sizeof( ofaClassesPagePrivate ));
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	ofaClassesPagePrivate *priv;
	ofoDossier *dossier;
	gulong handler;

	priv = OFA_CLASSES_PAGE( page )->priv;
	dossier = ofa_page_get_dossier( page );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						SIGNAL_DOSSIER_NEW_OBJECT, G_CALLBACK( on_new_object ), page );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), page );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						SIGNAL_DOSSIER_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), page );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						SIGNAL_DOSSIER_RELOAD_DATASET, G_CALLBACK( on_reloaded_dataset ), page );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	return( setup_tree_view( page ));
}

static GtkWidget *
setup_tree_view( ofaPage *page )
{
	ofaClassesPagePrivate *priv;
	GtkFrame *frame;
	GtkScrolledWindow *scroll;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = OFA_CLASSES_PAGE( page )->priv;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_margin_left( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_top( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( frame ), 4 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_container_set_border_width( GTK_CONTAINER( scroll ), 4 );
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( scroll ));

	tview = GTK_TREE_VIEW( gtk_tree_view_new());
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
on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaClassesPage *self )
{
	gboolean stop;

	stop = FALSE;

	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Insert ){
			on_new_clicked( self );

		} else if( event->keyval == GDK_KEY_Delete ){
			try_to_delete_current_row( self );
		}
	}

	return( stop );
}

static void
v_init_view( ofaPage *page )
{
	insert_dataset( OFA_CLASSES_PAGE( page ));
}

static ofoClass *
tview_get_selected( ofaClassesPage *page, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	ofaClassesPagePrivate *priv;
	GtkTreeSelection *select;
	ofoClass *class;

	priv = page->priv;
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
	g_return_val_if_fail( page && OFA_IS_CLASSES_PAGE( page ), NULL );

	return( GTK_WIDGET( OFA_CLASSES_PAGE( page )->priv->tview ));
}

static void
insert_dataset( ofaClassesPage *self )
{
	GList *dataset, *iset;
	ofoClass *class;

	dataset = ofo_class_get_dataset(
						ofa_page_get_dossier( OFA_PAGE( self )));

	for( iset=dataset ; iset ; iset=iset->next ){

		class = OFO_CLASS( iset->data );
		insert_new_row( self, class, FALSE );
	}

	setup_first_selection( self );
}

static void
insert_new_row( ofaClassesPage *self, ofoClass *class, gboolean with_selection )
{
	ofaClassesPagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;
	gint id;
	gchar *str;

	priv = self->priv;
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
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaClassesPage *self )
{
	gint anum, bnum;

	gtk_tree_model_get( tmodel, a, COL_ID, &anum, -1 );
	gtk_tree_model_get( tmodel, b, COL_ID, &bnum, -1 );

	return( anum < bnum ? -1 : ( anum > bnum ? 1 : 0 ));
}

static void
setup_first_selection( ofaClassesPage *self )
{
	ofaClassesPagePrivate *priv;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	priv = self->priv;
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
	on_update_clicked( OFA_CLASSES_PAGE( page ));
}

static void
on_row_selected( GtkTreeSelection *selection, ofaClassesPage *self )
{
	ofaClassesPagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoClass *class;

	class = NULL;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &class, -1 );
		g_object_unref( class );
	}

	priv = self->priv;

	if( !priv->update_btn ){
		priv->update_btn = ofa_page_get_button_by_id( OFA_PAGE( self ), BUTTONS_BOX_PROPERTIES );
		priv->delete_btn = ofa_page_get_button_by_id( OFA_PAGE( self ), BUTTONS_BOX_DELETE );
	}

	if( priv->update_btn ){
		gtk_widget_set_sensitive(
				priv->update_btn,
				class && OFO_IS_CLASS( class ));
	}

	if( priv->delete_btn ){
		gtk_widget_set_sensitive(
				priv->delete_btn,
				class && OFO_IS_CLASS( class ) && ofo_class_is_deletable( class ));
	}
}

static void
v_on_button_clicked( ofaPage *page, guint button_id )
{
	g_return_if_fail( OFA_IS_CLASSES_PAGE( page ));

	switch( button_id ){
		case BUTTONS_BOX_NEW:
			on_new_clicked( OFA_CLASSES_PAGE( page ));
			break;
		case BUTTONS_BOX_PROPERTIES:
			on_update_clicked( OFA_CLASSES_PAGE( page ));
			break;
		case BUTTONS_BOX_DELETE:
			on_delete_clicked( OFA_CLASSES_PAGE( page ));
			break;
	}
}

static void
on_new_clicked( ofaClassesPage *page )
{
	static const gchar *thisfn = "ofa_classes_page_on_new_clicked";
	ofoClass *class;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	class = ofo_class_new();

	if( !ofa_class_properties_run(
			ofa_page_get_main_window( OFA_PAGE( page )), class )){

		g_object_unref( class );
	}
}

static void
on_update_clicked( ofaClassesPage *page )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoClass *class;

	class = tview_get_selected( page, &tmodel, &iter );
	if( class ){
		ofa_class_properties_run(
				ofa_page_get_main_window( OFA_PAGE( page )), class );
	}
	gtk_widget_grab_focus( GTK_WIDGET( page->priv->tview ));
}

static void
on_delete_clicked( ofaClassesPage *page )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoClass *class;

	class = tview_get_selected( page, &tmodel, &iter );
	if( class ){
		do_delete( page, class, tmodel, &iter );
	}
	gtk_widget_grab_focus( GTK_WIDGET( page->priv->tview ));
}

static void
try_to_delete_current_row( ofaClassesPage *self )
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
delete_confirmed( ofaClassesPage *self, ofoClass *class )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s' class ?" ),
			ofo_class_get_label( class ));

	delete_ok = ofa_main_window_confirm_deletion(
						ofa_page_get_main_window( OFA_PAGE( self )), msg );

	g_free( msg );

	return( delete_ok );
}

static void
do_delete( ofaClassesPage *page, ofoClass *class, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	g_return_if_fail( ofo_class_is_deletable( class ));

	if( delete_confirmed( page, class )){

		ofo_class_delete( class );

		/* remove the row from the tmodel
		 * this will cause an automatic new selection */
		gtk_list_store_remove( GTK_LIST_STORE( tmodel ), iter );
	}
}

static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaClassesPage *self )
{
	static const gchar *thisfn = "ofa_classes_page_on_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_CLASS( object )){
		insert_new_row( self, OFO_CLASS( object ), TRUE );
	}
}

/*
 * modifying the class number is forbidden
 */
static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaClassesPage *self )
{
	static const gchar *thisfn = "ofa_classes_page_on_updated_object";
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gint prev_num, class_num;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) dossier,
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

static void
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaClassesPage *self )
{
	static const gchar *thisfn = "ofa_classes_page_on_deleted_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );
}

/*
 * SIGNAL_DOSSIER_RELOAD_DATASET signal handler
 */
static void
on_reloaded_dataset( ofoDossier *dossier, GType type, ofaClassesPage *self )
{
	static const gchar *thisfn = "ofa_classes_page_on_reloaded_dataset";
	ofaClassesPagePrivate *priv;
	GtkTreeModel *tmodel;

	g_debug( "%s: dossier=%p, type=%lu, self=%p",
			thisfn,
			( void * ) dossier,
			type,
			( void * ) self );

	priv = self->priv;

	if( type == OFO_TYPE_CLASS ){
		tmodel = gtk_tree_view_get_model( priv->tview );
		gtk_list_store_clear( GTK_LIST_STORE( tmodel ));
		insert_dataset( self );
	}
}

static gboolean
find_row_by_id( ofaClassesPage *self, gint id, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	static const gchar *thisfn = "ofa_classes_page_find_row_by_id";
	ofaClassesPagePrivate *priv;
	gint num;

	priv = self->priv;
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
