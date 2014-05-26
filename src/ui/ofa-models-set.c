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

#include "ui/ofa-main-page.h"
#include "ui/ofa-guided-input.h"
#include "ui/ofa-model-properties.h"
#include "ui/ofa-models-set.h"
#include "ui/ofo-base.h"
#include "ui/ofo-model.h"
#include "ui/ofo-journal.h"

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

static ofaMainPageClass *st_parent_class = NULL;

static GType      register_type( void );
static void       class_init( ofaModelsSetClass *klass );
static void       instance_init( GTypeInstance *instance, gpointer klass );
static void       instance_dispose( GObject *instance );
static void       instance_finalize( GObject *instance );
static GtkWidget *v_setup_view( ofaMainPage *page );
static GtkWidget *v_setup_buttons( ofaMainPage *page );
static void       v_init_view( ofaMainPage *page );
static void       setup_first_selection( ofaModelsSet *self );
static GtkWidget *notebook_create_page( ofaModelsSet *self, GtkNotebook *book, gint jou_id, const gchar *jou_label, gint position );
static GtkWidget *notebook_find_page( ofaModelsSet *self, gint jou_id );
static void       on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaModelsSet *self );
static void       store_set_model( GtkTreeModel *model, GtkTreeIter *iter, const ofoModel *ofomodel );
static void       on_model_selected( GtkTreeSelection *selection, ofaModelsSet *self );
static void       enable_buttons( ofaModelsSet *self, GtkTreeSelection *selection );
static void       v_on_new_clicked( GtkButton *button, ofaMainPage *page );
static void       v_on_update_clicked( GtkButton *button, ofaMainPage *page );
static void       v_on_delete_clicked( GtkButton *button, ofaMainPage *page );
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

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = ( ofaMainPageClass * ) g_type_class_peek_parent( klass );
	g_return_if_fail( st_parent_class && OFA_IS_MAIN_PAGE_CLASS( st_parent_class ));

	G_OBJECT_CLASS( klass )->dispose = instance_dispose;
	G_OBJECT_CLASS( klass )->finalize = instance_finalize;

	OFA_MAIN_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_MAIN_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_MAIN_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_MAIN_PAGE_CLASS( klass )->on_new_clicked = v_on_new_clicked;
	OFA_MAIN_PAGE_CLASS( klass )->on_update_clicked = v_on_update_clicked;
	OFA_MAIN_PAGE_CLASS( klass )->on_delete_clicked = v_on_delete_clicked;
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
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->dispose( instance );
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

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->finalize( instance );
}

static GtkWidget *
v_setup_view( ofaMainPage *page )
{
	ofaModelsSet *self;
	GtkNotebook *book;
	GList *jouset, *ijou;

	self = OFA_MODELS_SET( page );

	book = GTK_NOTEBOOK( gtk_notebook_new());
	gtk_widget_set_margin_left( GTK_WIDGET( book ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( book ), 4 );
	g_signal_connect( G_OBJECT( book ), "switch-page", G_CALLBACK( on_page_switched ), self );
	self->private->book = GTK_NOTEBOOK( book );

	jouset = ofo_journal_get_dataset( ofa_main_page_get_dossier( page ));

	for( ijou=jouset ; ijou ; ijou=ijou->next ){
		notebook_create_page( self, book,
				ofo_journal_get_id( OFO_JOURNAL( ijou->data )),
				ofo_journal_get_label( OFO_JOURNAL( ijou->data )),
				-1 );
	}

	return( GTK_WIDGET( book ));
}

static GtkWidget *
v_setup_buttons( ofaMainPage *page )
{
	GtkBox *buttons_box;
	GtkFrame *frame;
	GtkButton *button;

	buttons_box = GTK_BOX( st_parent_class->setup_buttons( page ));
	gtk_widget_set_margin_right( GTK_WIDGET( buttons_box ), 4 );

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_size_request( GTK_WIDGET( frame ), -1, 25 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_NONE );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( frame ), FALSE, FALSE, 0 );

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_Guided input..." )));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_guided_input ), page );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	OFA_MODELS_SET( page )->private->guided_input_btn = button;

	return( GTK_WIDGET( buttons_box ));
}

static void
v_init_view( ofaMainPage *page )
{
	GList *models, *imod;
	ofoModel *ofomodel;
	gint jou_id;
	GtkWidget *book_page;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	models = ofo_model_get_dataset( ofa_main_page_get_dossier( page ));

	for( imod=models ; imod ; imod=imod->next ){
		ofomodel = OFO_MODEL( imod->data );
		jou_id = ofo_model_get_journal( ofomodel );
		book_page = notebook_find_page( OFA_MODELS_SET( page ), jou_id );

		if( book_page && GTK_IS_WIDGET( book_page )){
			tview = GTK_TREE_VIEW( g_object_get_data( G_OBJECT( book_page ), DATA_PAGE_VIEW ));
			tmodel = gtk_tree_view_get_model( tview );
			gtk_list_store_append( GTK_LIST_STORE( tmodel ), &iter );
			store_set_model( tmodel, &iter, ofomodel );

		} else {
			g_warning( "ofa_models_set_v_init_view: unable to find the page for journal %d", jou_id );
		}
	}

	setup_first_selection( OFA_MODELS_SET( page ));
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
		gtk_widget_grab_focus( GTK_WIDGET( first_treeview ));
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
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoModel *model;
	gboolean select_ok;


	select_ok = gtk_tree_selection_get_selected( selection, &tmodel, &iter );
	if( select_ok ){
		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &model, -1 );
		g_object_unref( model );

		gtk_widget_set_sensitive(
				ofa_main_page_get_update_btn( OFA_MAIN_PAGE( self )),
				select_ok );
		gtk_widget_set_sensitive(
				ofa_main_page_get_update_btn( OFA_MAIN_PAGE( self )),
				model && OFO_IS_MODEL( model ) && ofo_model_is_deletable( model ));
		gtk_widget_set_sensitive(
				GTK_WIDGET( self->private->guided_input_btn ),
				select_ok );
	}
}

static void
v_on_new_clicked( GtkButton *button, ofaMainPage *page )
{
	static const gchar *thisfn = "ofa_models_set_v_on_new_clicked";
	ofoModel *model;
	gint page_n;
	GtkWidget *page_w;
	gint journal_id;

	g_return_if_fail( OFA_IS_MODELS_SET( page ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) page );

	model = ofo_model_new();
	page_n = gtk_notebook_get_current_page( OFA_MODELS_SET( page )->private->book );
	page_w = gtk_notebook_get_nth_page( OFA_MODELS_SET( page )->private->book, page_n );
	journal_id = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( page_w ), DATA_PAGE_JOURNAL ));

	if( ofa_model_properties_run(
			ofa_main_page_get_main_window( page ), model, journal_id )){

		insert_new_row( OFA_MODELS_SET( page ), model );

	} else {
		g_object_unref( model );
	}
}

static void
v_on_update_clicked( GtkButton *button, ofaMainPage *page )
{
	static const gchar *thisfn = "ofa_models_set_v_on_update_clicked";
	ofaModelsSet *self;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *prev_mnemo;
	const gchar *new_mnemo;
	ofoModel *model;

	g_return_if_fail( OFA_IS_MODELS_SET( page ));
	self = OFA_MODELS_SET( page );
	g_return_if_fail( self->private->current && GTK_IS_TREE_VIEW( self->private->current ));

	g_debug( "%s: button=%p, page=%p", thisfn, ( void * ) button, ( void * ) page );

	select = gtk_tree_view_get_selection( self->private->current );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &model, -1 );
		g_object_unref( model );

		prev_mnemo = g_strdup( ofo_model_get_mnemo( model ));

		if( ofa_model_properties_run(
				ofa_main_page_get_main_window( OFA_MAIN_PAGE( self )), model, OFO_BASE_UNSET_ID )){

			new_mnemo = ofo_model_get_mnemo( model );
			if( g_utf8_collate( prev_mnemo, new_mnemo )){
				gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
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
v_on_delete_clicked( GtkButton *button, ofaMainPage *page )
{
	static const gchar *thisfn = "ofa_models_set_v_on_delete_clicked";
	ofaModelsSet *self;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoModel *model;

	g_return_if_fail( OFA_IS_MODELS_SET( page ));
	self = OFA_MODELS_SET( page );
	g_return_if_fail( self->private->current && GTK_IS_TREE_VIEW( self->private->current ));

	g_debug( "%s: button=%p, page=%p", thisfn, ( void * ) button, ( void * ) page );

	select = gtk_tree_view_get_selection( self->private->current );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &model, -1 );
		g_object_unref( model );

		if( delete_confirmed( self, model ) &&
				ofo_model_delete( model, ofa_main_page_get_dossier( page ))){

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
			OFA_MAIN_PAGE( self ), ofo_model_get_dataset( dossier ));

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
