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

#include "ui/my-utils.h"
#include "ui/ofa-taux-properties.h"
#include "ui/ofa-taux-set.h"
#include "ui/ofo-taux.h"

/* private class data
 */
struct _ofaTauxSetClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaTauxSetPrivate {
	gboolean       dispose_has_run;

	/* properties
	 */

	/* internals
	 */

	/* UI
	 */
	GtkTreeView   *view;				/* the treeview on the taux set */
	GtkButton     *update_btn;
	GtkButton     *delete_btn;
};

/* column ordering in the selection listview
 */
enum {
	COL_MNEMO = 0,
	COL_LABEL,
	COL_OBJECT,
	N_COLUMNS
};

static GObjectClass *st_parent_class = NULL;

static GType      register_type( void );
static void       class_init( ofaTauxSetClass *klass );
static void       instance_init( GTypeInstance *instance, gpointer klass );
static void       instance_constructed( GObject *instance );
static void       instance_dispose( GObject *instance );
static void       instance_finalize( GObject *instance );
static void       setup_set_page( ofaTauxSet *self );
static GtkWidget *setup_taux_view( ofaTauxSet *self );
static GtkWidget *setup_buttons_box( ofaTauxSet *self );
static void       setup_first_selection( ofaTauxSet *self );
static void       store_set_taux( GtkTreeModel *model, GtkTreeIter *iter, const ofoTaux *taux );
static void       on_taux_selected( GtkTreeSelection *selection, ofaTauxSet *self );
static void       on_new_taux( GtkButton *button, ofaTauxSet *self );
static void       on_update_taux( GtkButton *button, ofaTauxSet *self );
static void       on_delete_taux( GtkButton *button, ofaTauxSet *self );
static gboolean   delete_confirmed( ofaTauxSet *self, ofoTaux *taux );
static void       insert_new_row( ofaTauxSet *self, ofoTaux *taux );

GType
ofa_taux_set_get_type( void )
{
	static GType window_type = 0;

	if( !window_type ){
		window_type = register_type();
	}

	return( window_type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_taux_set_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaTauxSetClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaTauxSet ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFA_TYPE_MAIN_PAGE, "ofaTauxSet", &info, 0 );

	return( type );
}

static void
class_init( ofaTauxSetClass *klass )
{
	static const gchar *thisfn = "ofa_taux_set_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->constructed = instance_constructed;
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofaTauxSetClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_taux_set_instance_init";
	ofaTauxSet *self;

	g_return_if_fail( OFA_IS_TAUX_SET( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",

			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_TAUX_SET( instance );

	self->private = g_new0( ofaTauxSetPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_constructed( GObject *instance )
{
	static const gchar *thisfn = "ofa_taux_set_instance_constructed";

	g_return_if_fail( OFA_IS_TAUX_SET( instance ));

	/* first, chain up to the parent class */
	if( G_OBJECT_CLASS( st_parent_class )->constructed ){
		G_OBJECT_CLASS( st_parent_class )->constructed( instance );
	}

	/* then initialize our page
	 */
	g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	setup_set_page( OFA_TAUX_SET( instance ));
}

static void
instance_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofa_taux_set_instance_dispose";
	ofaTauxSetPrivate *priv;

	g_return_if_fail( OFA_IS_TAUX_SET( instance ));

	priv = ( OFA_TAUX_SET( instance ))->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		priv->dispose_has_run = TRUE;

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( instance );
		}
	}
}

static void
instance_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_taux_set_instance_finalize";
	ofaTauxSet *self;

	g_return_if_fail( OFA_IS_TAUX_SET( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFA_TAUX_SET( instance );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( instance );
	}
}

/**
 * ofa_taux_set_run:
 *
 * When called by the main_window, the page has been created, showed
 * and activated - there is nothing left to do here....
 */
void
ofa_taux_set_run( ofaMainPage *this )
{
	static const gchar *thisfn = "ofa_taux_set_run";

	g_return_if_fail( this && OFA_IS_TAUX_SET( this ));

	g_debug( "%s: this=%p (%s)",
			thisfn, ( void * ) this, G_OBJECT_TYPE_NAME( this ));
}

/*
 * +-----------------------------------------------------------------------+
 * | grid1 (this is the notebook page)                                     |
 * | +-----------------------------------------------+-------------------+ |
 * | | list of taux                              | buttons           | |
 * | |                                               |                   | |
 * | |                                               |                   | |
 * | +-----------------------------------------------+-------------------+ |
 * +-----------------------------------------------------------------------+
 */
static void
setup_set_page( ofaTauxSet *self )
{
	GtkGrid *grid;
	GtkWidget *view;
	GtkWidget *buttons_box;

	grid = ofa_main_page_get_grid( OFA_MAIN_PAGE( self ));

	view = setup_taux_view( self );
	gtk_grid_attach( grid, view, 0, 0, 1, 1 );

	buttons_box = setup_buttons_box( self );
	gtk_grid_attach( grid, buttons_box, 1, 0, 1, 1 );

	setup_first_selection( self );
}

static GtkWidget *
setup_taux_view( ofaTauxSet *self )
{
	GtkFrame *frame;
	GtkScrolledWindow *scroll;
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	ofoDossier *dossier;
	GList *dataset, *it;
	ofoTaux *taux;
	GtkTreeIter iter;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_margin_left( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_top( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( frame ), 4 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_container_set_border_width( GTK_CONTAINER( scroll ), 4 );
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( scroll ));

	view = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_vexpand( GTK_WIDGET( view ), TRUE );
	gtk_tree_view_set_headers_visible( view, TRUE );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( view ));
	self->private->view = GTK_TREE_VIEW( view );

	model = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( view, model );
	g_object_unref( model );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Mnemo" ),
			text_cell, "text", COL_MNEMO,
			NULL );
	gtk_tree_view_append_column( view, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( view, column );

	/*text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Begin of val." ),
			text_cell, "text", COL_BEGIN,
			NULL );
	gtk_tree_view_append_column( view, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "End of val." ),
			text_cell, "text", COL_END,
			NULL );
	gtk_tree_view_append_column( view, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Rate" ),
			text_cell, "text", COL_TAUX,
			NULL );
	gtk_tree_view_append_column( view, column );
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_set_min_width( column, 80 );*/

	select = gtk_tree_view_get_selection( view );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_taux_selected ), self );

	dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ));
	dataset = ofo_taux_get_dataset( dossier );
	ofa_main_page_set_dataset( OFA_MAIN_PAGE( self ), dataset );

	for( it=dataset ; it ; it=it->next ){

		taux = OFO_TAUX( it->data );
		gtk_list_store_append( GTK_LIST_STORE( model ), &iter );
		store_set_taux( model, &iter, taux );
	}

	return( GTK_WIDGET( frame ));
}

static GtkWidget *
setup_buttons_box( ofaTauxSet *self )
{
	GtkBox *buttons_box;
	GtkFrame *frame;
	GtkButton *button;

	buttons_box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 ));
	gtk_widget_set_margin_right( GTK_WIDGET( buttons_box ), 4 );

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_NONE );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( frame ), FALSE, FALSE, 30 );

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_New..." )));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_new_taux ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_Update..." )));
	gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_update_taux ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	self->private->update_btn = button;

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_Delete..." )));
	gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_delete_taux ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	self->private->delete_btn = button;

	return( GTK_WIDGET( buttons_box ));
}

static void
setup_first_selection( ofaTauxSet *self )
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	model = gtk_tree_view_get_model( self->private->view );
	if( gtk_tree_model_get_iter_first( model, &iter )){
		select = gtk_tree_view_get_selection( self->private->view );
		gtk_tree_selection_select_iter( select, &iter );
	}
}

static void
store_set_taux( GtkTreeModel *model, GtkTreeIter *iter, const ofoTaux *taux )
{
	gtk_list_store_set(
			GTK_LIST_STORE( model ),
			iter,
			COL_MNEMO,  ofo_taux_get_mnemo( taux ),
			COL_LABEL,  ofo_taux_get_label( taux ),
			COL_OBJECT, taux,
			-1 );
}

static void
on_taux_selected( GtkTreeSelection *selection, ofaTauxSet *self )
{
	gboolean select_ok;

	select_ok = gtk_tree_selection_get_selected( selection, NULL, NULL );

	gtk_widget_set_sensitive( GTK_WIDGET( self->private->update_btn ), select_ok );
	gtk_widget_set_sensitive( GTK_WIDGET( self->private->delete_btn ), select_ok );
}

static void
on_new_taux( GtkButton *button, ofaTauxSet *self )
{
	static const gchar *thisfn = "ofa_taux_set_on_new_taux";
	ofoTaux *taux;
	ofaMainWindow *main_window;

	g_return_if_fail( OFA_IS_TAUX_SET( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	main_window = ofa_main_page_get_main_window( OFA_MAIN_PAGE( self ));

	taux = ofo_taux_new();
	if( ofa_taux_properties_run( main_window, taux )){
		insert_new_row( self, taux );
	}
}

static void
on_update_taux( GtkButton *button, ofaTauxSet *self )
{
	static const gchar *thisfn = "ofa_taux_set_on_update_taux";
	GtkTreeSelection *select;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *prev_mnemo;
	const gchar *new_mnemo;
	ofoTaux *taux;

	g_return_if_fail( OFA_IS_TAUX_SET( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	select = gtk_tree_view_get_selection( self->private->view );
	if( gtk_tree_selection_get_selected( select, &model, &iter )){

		gtk_tree_model_get( model, &iter, COL_OBJECT, &taux, -1 );
		g_object_unref( taux );

		prev_mnemo = g_strdup( ofo_taux_get_mnemo( taux ));
		/*prev_val = my_utils_display_from_date( ofo_taux_get_val_begin( taux ), MY_UTILS_DATE_DMMM );*/

		if( ofa_taux_properties_run(
				ofa_main_page_get_main_window( OFA_MAIN_PAGE( self )), taux )){

			new_mnemo = ofo_taux_get_mnemo( taux );
			/*new_val = my_utils_display_from_date( ofo_taux_get_val_begin( taux ), MY_UTILS_DATE_DMMM );*/

			if( g_utf8_collate( prev_mnemo, new_mnemo )){
				gtk_list_store_remove( GTK_LIST_STORE( model ), &iter );
				insert_new_row( self, taux );

			} else {
				store_set_taux( model, &iter, taux );
			}
		}
		g_free( prev_mnemo );
	}
}

/*
 * un taux peut être supprimé à tout moment
 */
static void
on_delete_taux( GtkButton *button, ofaTauxSet *self )
{
	static const gchar *thisfn = "ofa_taux_set_on_delete_taux";
	GtkTreeSelection *select;
	GtkTreeModel *model;
	GtkTreeIter iter;
	ofoTaux *taux;
	ofoDossier *dossier;

	g_return_if_fail( OFA_IS_TAUX_SET( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	select = gtk_tree_view_get_selection( self->private->view );
	if( gtk_tree_selection_get_selected( select, &model, &iter )){

		gtk_tree_model_get( model, &iter, COL_OBJECT, &taux, -1 );
		g_object_unref( taux );

		dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ));

		if( delete_confirmed( self, taux ) &&
				ofo_taux_delete( taux, dossier )){

			/* update our set of taux */
			ofa_main_page_set_dataset(
					OFA_MAIN_PAGE( self ), ofo_taux_get_dataset( dossier ));

			/* remove the row from the model
			 * this will cause an automatic new selection */
			gtk_list_store_remove( GTK_LIST_STORE( model ), &iter );
		}
	}
}

static gboolean
delete_confirmed( ofaTauxSet *self, ofoTaux *taux )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want to delete the '%s - %s' rate ?" ),
			ofo_taux_get_mnemo( taux ),
			ofo_taux_get_label( taux ));

	delete_ok = ofa_main_page_delete_confirmed( OFA_MAIN_PAGE( self ), msg );

	g_free( msg );

	return( delete_ok );
}

static void
insert_new_row( ofaTauxSet *self, ofoTaux *taux )
{
	GtkTreeModel *model;
	GtkTreeIter iter, new_iter;
	const gchar *taux_mnemo;
	gchar *mod_mnemo;
	gint cmp;
	gboolean iter_found;
	GtkTreePath *path;
	ofoDossier *dossier;

	/* update our set of taux */
	dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ));
	ofa_main_page_set_dataset(
			OFA_MAIN_PAGE( self ), ofo_taux_get_dataset( dossier ));

	/* insert the new row at the right place */
	model = gtk_tree_view_get_model( self->private->view );
	iter_found = FALSE;
	if( gtk_tree_model_get_iter_first( model, &iter )){
		taux_mnemo = ofo_taux_get_mnemo( taux );
		while( !iter_found ){
			gtk_tree_model_get( model, &iter,
					COL_MNEMO, &mod_mnemo,
					-1 );
			cmp = g_utf8_collate( mod_mnemo, taux_mnemo );
			if( cmp > 0 ){
				iter_found = TRUE;
				break;
			}
			if( !gtk_tree_model_iter_next( model, &iter )){
				break;
			}
			g_free( mod_mnemo );
		}
	}
	if( !iter_found ){
		gtk_list_store_append( GTK_LIST_STORE( model ), &new_iter );
	} else {
		gtk_list_store_insert_before( GTK_LIST_STORE( model ), &new_iter, &iter );
	}
	store_set_taux( model, &new_iter, taux );

	/* select the newly added taux */
	path = gtk_tree_model_get_path( model, &new_iter );
	gtk_tree_view_set_cursor( self->private->view, path, NULL, FALSE );
	gtk_widget_grab_focus( GTK_WIDGET( self->private->view ));
	gtk_tree_path_free( path );
}
