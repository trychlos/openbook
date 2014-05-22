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

#include "ui/ofa-guided-input.h"
#include "ui/ofa-model-properties.h"
#include "ui/ofa-models-set.h"
#include "ui/ofo-model.h"
#include "ui/ofo-journal.h"

/* private class data
 */
struct _ofaModelsSetClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaModelsSetPrivate {
	gboolean       dispose_has_run;

	/* properties
	 */

	/* internals
	 */

	/* UI
	 */
	GtkNotebook   *book;				/* one page per journal d'imputation */
	GtkWidget     *others;				/* a page for unclassed models */
	GtkButton     *update_btn;
	GtkButton     *delete_btn;
	GtkButton     *guided_input_btn;
	GtkTreeView   *current;				/* tree view of the current page */
};

/* column ordering in the selection listview
 */
enum {
	COL_MNEMO = 0,
	COL_LABEL,
	COL_OBJECT,
	N_COLUMNS
};

/* data attached to each page of the model category notebook
 */
#define DATA_PAGE_JOURNAL                "data-page-journal-id"
#define DATA_PAGE_VIEW                   "data-page-treeview"

static GObjectClass *st_parent_class = NULL;

static GType      register_type( void );
static void       class_init( ofaModelsSetClass *klass );
static void       instance_init( GTypeInstance *instance, gpointer klass );
static void       instance_constructed( GObject *instance );
static void       instance_dispose( GObject *instance );
static void       instance_finalize( GObject *instance );
static void       setup_set_page( ofaModelsSet *self );
static void       on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaModelsSet *self );
static GtkWidget *setup_models_view( ofaModelsSet *self );
static GtkWidget *setup_buttons_box( ofaModelsSet *self );
static void       setup_first_selection( ofaModelsSet *self );
static GtkWidget *notebook_create_page( ofaModelsSet *self, GtkNotebook *book, gint jou_id, const gchar *jou_label, gint position );
static GtkWidget *notebook_find_page( ofaModelsSet *self, gint jou_id );
static void       store_set_model( GtkTreeModel *model, GtkTreeIter *iter, const ofoModel *ofomodel );
static void       on_model_selected( GtkTreeSelection *selection, ofaModelsSet *self );
static void       enable_buttons( ofaModelsSet *self, GtkTreeSelection *selection );
static void       on_new_model( GtkButton *button, ofaModelsSet *self );
static void       on_update_model( GtkButton *button, ofaModelsSet *self );
static void       on_delete_model( GtkButton *button, ofaModelsSet *self );
static gboolean   delete_confirmed( ofaModelsSet *self, ofoModel *model );
static void       insert_new_row( ofaModelsSet *self, ofoModel *model );
static void       on_guided_input( GtkButton *button, ofaModelsSet *self );

GType
ofa_models_set_get_type( void )
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
	static const gchar *thisfn = "ofa_models_set_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaModelsSetClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaModelsSet ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFA_TYPE_MAIN_PAGE, "ofaModelsSet", &info, 0 );

	return( type );
}

static void
class_init( ofaModelsSetClass *klass )
{
	static const gchar *thisfn = "ofa_models_set_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->constructed = instance_constructed;
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofaModelsSetClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_models_set_instance_init";
	ofaModelsSet *self;

	g_return_if_fail( OFA_IS_MODELS_SET( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_MODELS_SET( instance );

	self->private = g_new0( ofaModelsSetPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_constructed( GObject *instance )
{
	static const gchar *thisfn = "ofa_models_set_instance_constructed";

	g_return_if_fail( OFA_IS_MODELS_SET( instance ));

	/* first chain up to the parent class */
	if( G_OBJECT_CLASS( st_parent_class )->constructed ){
		G_OBJECT_CLASS( st_parent_class )->constructed( instance );
	}

	/* then setup the page
	 */

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	setup_set_page( OFA_MODELS_SET( instance ));
}

static void
instance_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofa_models_set_instance_dispose";
	ofaModelsSetPrivate *priv;

	g_return_if_fail( OFA_IS_MODELS_SET( instance ));

	priv = ( OFA_MODELS_SET( instance ))->private;

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
	static const gchar *thisfn = "ofa_models_set_instance_finalize";
	ofaModelsSet *self;

	g_return_if_fail( OFA_IS_MODELS_SET( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFA_MODELS_SET( instance );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( instance );
	}
}

/**
 * ofa_models_set_run:
 *
 * When called by the main_window, the page has been created, showed
 * and activated - there is nothing left to do here....
 */
void
ofa_models_set_run( ofaMainPage *this )
{
	static const gchar *thisfn = "ofa_models_set_run";

	g_return_if_fail( this && OFA_IS_MODELS_SET( this ));

	g_debug( "%s: this=%p (%s)",
			thisfn, ( void * ) this, G_OBJECT_TYPE_NAME( this ));
}

/*
 * setup a notebook whose each page is for a category
 * in each page, the models for the category are listed in a treeview
 */
static void
setup_set_page( ofaModelsSet *self )
{
	GtkGrid *grid;
	GtkWidget *models_book;
	GtkWidget *buttons_box;

	grid = ofa_main_page_get_grid( OFA_MAIN_PAGE( self ));

	models_book = setup_models_view( self );
	gtk_grid_attach( grid, models_book, 0, 0, 1, 1 );

	buttons_box = setup_buttons_box( self );
	gtk_grid_attach( grid, buttons_box, 1, 0, 1, 1 );

	setup_first_selection( self );
}

/*
 */
static void
on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaModelsSet *self )
{
	GtkTreeSelection *select;

	self->private->current =
			GTK_TREE_VIEW( g_object_get_data( G_OBJECT( wpage ), DATA_PAGE_VIEW ));

	select = gtk_tree_view_get_selection( self->private->current );
	enable_buttons( self, select );
}

static GtkWidget *
setup_models_view( ofaModelsSet *self )
{
	GtkNotebook *book;
	ofoDossier *dossier;
	GList *jouset, *ijou;
	GList *models, *imod;
	ofoModel *ofomodel;
	gint jou_id;
	GtkWidget *page;
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkTreeIter iter;

	book = GTK_NOTEBOOK( gtk_notebook_new());
	gtk_widget_set_margin_left( GTK_WIDGET( book ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( book ), 4 );
	self->private->book = GTK_NOTEBOOK( book );

	dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ));
	jouset = ofo_journal_get_dataset( dossier );

	for( ijou=jouset ; ijou ; ijou=ijou->next ){
		notebook_create_page( self, book,
				ofo_journal_get_id( OFO_JOURNAL( ijou->data )),
				ofo_journal_get_label( OFO_JOURNAL( ijou->data )),
				-1 );
	}

	models = ofo_dossier_get_models_set( dossier );
	ofa_main_page_set_dataset(
			OFA_MAIN_PAGE( self ), models );

	for( imod=models ; imod ; imod=imod->next ){
		ofomodel = OFO_MODEL( imod->data );
		jou_id = ofo_model_get_journal( ofomodel );
		page = notebook_find_page( self, jou_id );

		g_return_val_if_fail( page && GTK_IS_WIDGET( page ), NULL );

		view = GTK_TREE_VIEW( g_object_get_data( G_OBJECT( page ), DATA_PAGE_VIEW ));
		model = gtk_tree_view_get_model( view );
		gtk_list_store_append( GTK_LIST_STORE( model ), &iter );
		store_set_model( model, &iter, ofomodel );
	}

	g_signal_connect( G_OBJECT( book ), "switch-page", G_CALLBACK( on_page_switched ), self );

	return( GTK_WIDGET( book ));
}

static GtkWidget *
setup_buttons_box( ofaModelsSet *self )
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
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_new_model ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_Update..." )));
	gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_update_model ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	self->private->update_btn = button;

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_Delete..." )));
	gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_delete_model ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	self->private->delete_btn = button;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_size_request( GTK_WIDGET( frame ), -1, 25 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_NONE );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( frame ), FALSE, FALSE, 0 );

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_Guided input..." )));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_guided_input ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	self->private->guided_input_btn = button;

	return( GTK_WIDGET( buttons_box ));
}

static void
setup_first_selection( ofaModelsSet *self )
{
	GtkWidget *first_tab;
	GtkTreeView *first_treeview;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	first_tab = gtk_notebook_get_nth_page( GTK_NOTEBOOK( self->private->book ), 0 );
	if( first_tab ){
		first_treeview = GTK_TREE_VIEW( g_object_get_data( G_OBJECT( first_tab ), DATA_PAGE_VIEW ));
		model = gtk_tree_view_get_model( first_treeview );
		if( gtk_tree_model_get_iter_first( model, &iter )){
			select = gtk_tree_view_get_selection( first_treeview );
			gtk_tree_selection_select_iter( select, &iter );
		}
	}
}

static GtkWidget *
notebook_create_page( ofaModelsSet *self, GtkNotebook *book, gint jou_id, const gchar *jou_label, gint position )
{
	GtkScrolledWindow *scroll;
	GtkLabel *label;
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );

	label = GTK_LABEL( gtk_label_new_with_mnemonic( jou_label ));
	gtk_notebook_insert_page( book, GTK_WIDGET( scroll ), GTK_WIDGET( label ), position );
	gtk_notebook_set_tab_reorderable( book, GTK_WIDGET( scroll ), TRUE );
	g_object_set_data( G_OBJECT( scroll ), DATA_PAGE_JOURNAL, GINT_TO_POINTER( jou_id ));

	view = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_vexpand( GTK_WIDGET( view ), TRUE );
	gtk_tree_view_set_headers_visible( view, TRUE );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( view ));
	g_object_set_data( G_OBJECT( scroll ), DATA_PAGE_VIEW, view );

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

	select = gtk_tree_view_get_selection( view );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_model_selected ), self );

	return( GTK_WIDGET( scroll ));
}

static GtkWidget *
notebook_find_page( ofaModelsSet *self, gint jou_id )
{
	gint count, i;
	GtkWidget *page, *found;
	gint page_jou;

	count = gtk_notebook_get_n_pages( self->private->book );
	found = NULL;

	for( i=0 ; i<count ; ++i ){
		page = gtk_notebook_get_nth_page( self->private->book, i );
		page_jou = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( page ), DATA_PAGE_JOURNAL ));
		if( page_jou == jou_id ){
			found = page;
			break;
		}
	}

	if( !found ){
		if( !self->private->others ){
			self->private->others =
					notebook_create_page( self, self->private->book, -1, _( "Unjournalized" ), -1 );
		}
		found = self->private->others;
	}

	return( found );
}

static void
store_set_model( GtkTreeModel *model, GtkTreeIter *iter, const ofoModel *ofomodel )
{
	gtk_list_store_set(
			GTK_LIST_STORE( model ),
			iter,
			COL_MNEMO,  ofo_model_get_mnemo( ofomodel ),
			COL_LABEL,  ofo_model_get_label( ofomodel ),
			COL_OBJECT, ofomodel,
			-1 );
}

static void
on_model_selected( GtkTreeSelection *selection, ofaModelsSet *self )
{
	enable_buttons( self, selection );
}

static void
enable_buttons( ofaModelsSet *self, GtkTreeSelection *selection )
{
	gboolean select_ok;

	select_ok = gtk_tree_selection_get_selected( selection, NULL, NULL );

	gtk_widget_set_sensitive( GTK_WIDGET( self->private->update_btn ), select_ok );
	gtk_widget_set_sensitive( GTK_WIDGET( self->private->delete_btn ), select_ok );
	gtk_widget_set_sensitive( GTK_WIDGET( self->private->guided_input_btn ), select_ok );
}

static void
on_new_model( GtkButton *button, ofaModelsSet *self )
{
	static const gchar *thisfn = "ofa_models_set_on_new_model";
	ofoModel *model;
	ofaMainWindow *main_window;

	g_return_if_fail( OFA_IS_MODELS_SET( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	main_window = ofa_main_page_get_main_window( OFA_MAIN_PAGE( self ));
	model = ofo_model_new();

	if( ofa_model_properties_run( main_window, model )){
		insert_new_row( self, model );
	}
}

static void
on_update_model( GtkButton *button, ofaModelsSet *self )
{
	static const gchar *thisfn = "ofa_models_set_on_update_model";
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *prev_mnemo;
	const gchar *new_mnemo;
	ofoModel *model;

	g_return_if_fail( OFA_IS_MODELS_SET( self ));
	g_return_if_fail( self->private->current && GTK_IS_TREE_VIEW( self->private->current ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	select = gtk_tree_view_get_selection( self->private->current );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &model, -1 );
		g_object_unref( model );

		prev_mnemo = g_strdup( ofo_model_get_mnemo( model ));

		if( ofa_model_properties_run(
				ofa_main_page_get_main_window( OFA_MAIN_PAGE( self )), model )){

			new_mnemo = ofo_model_get_mnemo( model );
			if( g_utf8_collate( prev_mnemo, new_mnemo )){
				gtk_list_store_remove(
						GTK_LIST_STORE( tmodel ),
						&iter );
				insert_new_row( self, model );

			} else {
				store_set_model( tmodel, &iter, model );
			}
		}

		g_free( prev_mnemo );
	}
}

/*
 * un model peut être supprimé tant qu'aucune écriture n'y a été
 * enregistrée, et après confirmation de l'utilisateur
 */
static void
on_delete_model( GtkButton *button, ofaModelsSet *self )
{
	static const gchar *thisfn = "ofa_models_set_on_delete_model";
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoModel *model;
	ofoDossier *dossier;

	g_return_if_fail( OFA_IS_MODELS_SET( self ));
	g_return_if_fail( self->private->current && GTK_IS_TREE_VIEW( self->private->current ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	select = gtk_tree_view_get_selection( self->private->current );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &model, -1 );
		g_object_unref( model );

		dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ));

		if( delete_confirmed( self, model ) &&
				ofo_dossier_delete_model( dossier, model )){

			/* update our set of models */
			ofa_main_page_set_dataset(
					OFA_MAIN_PAGE( self ), ofo_dossier_get_models_set( dossier ));

			/* remove the row from the model
			 * this will cause an automatic new selection */
			gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
		}
	}
}

static gboolean
delete_confirmed( ofaModelsSet *self, ofoModel *model )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want to delete the '%s - %s' entry model ?" ),
			ofo_model_get_mnemo( model ),
			ofo_model_get_label( model ));

	delete_ok = ofa_main_page_delete_confirmed( OFA_MAIN_PAGE( self ), msg );

	g_free( msg );

	return( delete_ok );
}

static void
insert_new_row( ofaModelsSet *self, ofoModel *ofomodel )
{
	gint journal;
	GtkWidget *page;
	gint page_num;
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkTreeIter iter, new_iter;
	GValue value = G_VALUE_INIT;
	const gchar *mnemo, *model_mnemo;
	gboolean iter_found;
	GtkTreePath *path;
	ofoDossier *dossier;

	/* update our set of models */
	dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ));
	ofa_main_page_set_dataset(
			OFA_MAIN_PAGE( self ), ofo_dossier_get_models_set( dossier ));

	/* activate the page of the correct class, or create a new one */
	journal = ofo_model_get_journal( ofomodel );
	page = notebook_find_page( self, journal );
	g_return_if_fail( page && GTK_IS_WIDGET( page ));

	gtk_widget_show_all( page );
	page_num = gtk_notebook_page_num( self->private->book, page );
	gtk_notebook_set_current_page( self->private->book, page_num );

	/* insert the new row at the right place */
	view = GTK_TREE_VIEW( g_object_get_data( G_OBJECT( page ), DATA_PAGE_VIEW ));
	model = gtk_tree_view_get_model( view );
	iter_found = FALSE;
	if( gtk_tree_model_get_iter_first( model, &iter )){
		model_mnemo = ofo_model_get_mnemo( ofomodel );
		while( !iter_found ){
			gtk_tree_model_get_value( model, &iter, COL_MNEMO, &value );
			mnemo = g_value_get_string( &value );
			if( g_utf8_collate( mnemo, model_mnemo ) > 0 ){
				iter_found = TRUE;
				break;
			}
			if( !gtk_tree_model_iter_next( model, &iter )){
				break;
			}
			g_value_unset( &value );
		}
	}
	if( !iter_found ){
		gtk_list_store_append( GTK_LIST_STORE( model ), &new_iter );
	} else {
		gtk_list_store_insert_before( GTK_LIST_STORE( model ), &new_iter, &iter );
	}
	store_set_model( model, &new_iter, ofomodel );

	/* select the newly added model */
	path = gtk_tree_model_get_path( model, &new_iter );
	gtk_tree_view_set_cursor( view, path, NULL, FALSE );
	gtk_widget_grab_focus( GTK_WIDGET( view ));
	gtk_tree_path_free( path );
}

static void
on_guided_input( GtkButton *button, ofaModelsSet *self )
{
	static const gchar *thisfn = "ofa_models_set_on_guided_input";
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoModel *model;

	g_return_if_fail( OFA_IS_MODELS_SET( self ));
	g_return_if_fail( self->private->current && GTK_IS_TREE_VIEW( self->private->current ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	select = gtk_tree_view_get_selection( self->private->current );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &model, -1 );
		g_object_unref( model );

		ofa_guided_input_run(
				ofa_main_page_get_main_window( OFA_MAIN_PAGE( self )), model );
	}
}
