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

#include "ui/ofa-model-properties.h"
#include "ui/ofa-models-set.h"
#include "ui/ofo-model.h"
#include "ui/ofo-mod-family.h"

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
	ofaMainWindow *main_window;
	ofoDossier    *dossier;
	GList         *set;					/* a copy of the list of models (ledgers) */

	/* UI
	 */
	GtkNotebook   *book;				/* one page per model category */
	GtkWidget     *others;				/* a page for unclassed models */
	GtkButton     *update_btn;
	GtkButton     *delete_btn;
	ofoModel      *selected;			/* current selection */
	GtkTreeModel  *model;
	GtkTreeIter   *iter;				/* a copy of the selected iter */
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
#define DATA_PAGE_FAMILY                "data-page-family"
#define DATA_PAGE_VIEW                  "data-page-treeview"

static GObjectClass *st_parent_class = NULL;

static GType      register_type( void );
static void       class_init( ofaModelsSetClass *klass );
static void       instance_init( GTypeInstance *instance, gpointer klass );
static void       instance_dispose( GObject *instance );
static void       instance_finalize( GObject *instance );
static void       setup_set_page( ofaModelsSet *self, gint theme_id );
static void       on_set_page_finalized( ofaModelsSet *self, GtkWidget *page );
static void       on_family_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaModelsSet *self );
static GtkWidget *setup_models_view( ofaModelsSet *self );
static GtkWidget *setup_buttons_box( ofaModelsSet *self );
static void       setup_first_selection( ofaModelsSet *self );
static GtkWidget *notebook_create_page( ofaModelsSet *self, GtkNotebook *book, gint fam_id, const gchar *fam_label, gint position );
static GtkWidget *notebook_find_page( ofaModelsSet *self, gint fam_id );
static void       store_set_model( GtkTreeModel *model, GtkTreeIter *iter, const ofoModel *ofomodel );
static void       on_model_selected( GtkTreeSelection *selection, ofaModelsSet *self );
static void       on_new_model( GtkButton *button, ofaModelsSet *self );
static void       on_update_model( GtkButton *button, ofaModelsSet *self );
static void       on_delete_model( GtkButton *button, ofaModelsSet *self );
static gboolean   delete_confirmed( ofaModelsSet *self, ofoModel *model );
static void       insert_new_row( ofaModelsSet *self, ofoModel *model );
static void       models_set_free( ofaModelsSet *self );

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

	type = g_type_register_static( G_TYPE_OBJECT, "ofaModelsSet", &info, 0 );

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
instance_dispose( GObject *window )
{
	static const gchar *thisfn = "ofa_models_set_instance_dispose";
	ofaModelsSetPrivate *priv;

	g_return_if_fail( OFA_IS_MODELS_SET( window ));

	priv = ( OFA_MODELS_SET( window ))->private;

	if( !priv->dispose_has_run ){
		g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

		priv->dispose_has_run = TRUE;

		if( priv->set ){
			models_set_free( OFA_MODELS_SET( window ));
		}
		if( priv->iter ){
			gtk_tree_iter_free( priv->iter );
		}

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( window );
		}
	}
}

static void
instance_finalize( GObject *window )
{
	static const gchar *thisfn = "ofa_models_set_instance_finalize";
	ofaModelsSet *self;

	g_return_if_fail( OFA_IS_MODELS_SET( window ));

	g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

	self = OFA_MODELS_SET( window );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( window );
	}
}

/**
 * ofa_models_set_run:
 * @main: the main window of the application.
 * @dossier: the currently opened dossier
 * @theme_id: the identifier of the page in the main notebook
 *
 * Display the chart of accounts, letting the user edit it
 */
void
ofa_models_set_run( ofaMainWindow *main_window, ofoDossier *dossier, gint theme_id )
{
	static const gchar *thisfn = "ofa_models_set_run";
	ofaModelsSet *self;
	GtkWidget *page;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( main_window ));
	g_return_if_fail( OFO_IS_DOSSIER( dossier ));

	g_debug( "%s: main_window=%p, dossier=%p, theme_id=%d",
			thisfn, ( void * ) main_window, ( void * ) dossier, theme_id );

	page = ofa_main_window_get_notebook_page( main_window, theme_id );
	if( !page ){

		self = g_object_new( OFA_TYPE_MODELS_SET, NULL );
		self->private->main_window = main_window;
		self->private->dossier = dossier;
		self->private->set = ofo_dossier_get_models_set( dossier );

		setup_set_page( self, theme_id );
	}
}

/*
 * setup a notebook whose each page is for a category
 * in each page, the models for the category are listed in a treeview
 */
static void
setup_set_page( ofaModelsSet *self, gint theme_id )
{
	GtkNotebook *book;
	GtkGrid *grid;
	GtkLabel *label;
	GtkWidget *models_book;
	GtkWidget *buttons_box;
	gint page_num;
	GtkWidget *first_tab;
	GtkWidget *first_treeview;

	book = ofa_main_window_get_notebook( self->private->main_window );

	/* this is the new page of the main notebook
	 */
	grid = GTK_GRID( gtk_grid_new());
	g_object_weak_ref( G_OBJECT( grid ), ( GWeakNotify ) on_set_page_finalized, self );
	gtk_grid_set_column_spacing( grid, 4 );

	label = GTK_LABEL( gtk_label_new_with_mnemonic( _( "Modèles" )));
	gtk_notebook_append_page( book, GTK_WIDGET( grid ), GTK_WIDGET( label ));
	gtk_notebook_set_tab_reorderable( book, GTK_WIDGET( grid ), TRUE );

	g_object_set_data( G_OBJECT( grid ), OFA_DATA_THEME_ID, GINT_TO_POINTER ( theme_id ));

	/* build the children for this page
	 */
	models_book = setup_models_view( self );
	gtk_grid_attach( grid, models_book, 0, 0, 1, 1 );

	buttons_box = setup_buttons_box( self );
	gtk_grid_attach( grid, buttons_box, 1, 0, 1, 1 );

	gtk_widget_show_all( GTK_WIDGET( self->private->main_window ));
	page_num = gtk_notebook_page_num( book, GTK_WIDGET( grid ));
	gtk_notebook_set_current_page( book, page_num );
	first_tab = gtk_notebook_get_nth_page( GTK_NOTEBOOK( models_book ), 0 );
	if( first_tab ){
		first_treeview = GTK_WIDGET( g_object_get_data( G_OBJECT( first_tab ), DATA_PAGE_VIEW ));
		gtk_widget_grab_focus( first_treeview );
	}

	setup_first_selection( self );
}

/*
 * return FALSE to let the widget being deleted
 */
static void
on_set_page_finalized( ofaModelsSet *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_models_set_on_set_page_finalized";

	g_debug( "%s: self=%p (%s), page=%p",
			thisfn,
			( void * ) self, G_OBJECT_TYPE_NAME( self ),
			( void * ) page );

	g_object_unref( self );
}

/*
 */
static void
on_family_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaModelsSet *self )
{
	static const gchar *thisfn = "ofa_models_set_on_family_page_switched";
	GtkTreeView *view;
	GtkTreeSelection *select;
	GtkTreeIter iter;

	view = GTK_TREE_VIEW( g_object_get_data( G_OBJECT( wpage ), DATA_PAGE_VIEW ));
	select = gtk_tree_view_get_selection( view );

	if( gtk_tree_selection_get_selected( select, NULL, &iter )){
		g_debug( "%s: re-selecting current iter", thisfn );
		/* select_iter is not enough to send the 'changed' signal
		 * so unselect before, then re-select after */
		/*gtk_tree_selection_unselect_iter( select, &iter );
		gtk_tree_selection_select_iter( select, &iter );*/

	} else {
		g_debug( "%s: no selection", thisfn );
	}
}

static GtkWidget *
setup_models_view( ofaModelsSet *self )
{
	GtkNotebook *book;
	GList *famset, *ifam;
	GList *imod;
	ofoModel *ofomodel;
	gint fam_id;
	GtkWidget *page;
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkTreeIter iter;

	book = GTK_NOTEBOOK( gtk_notebook_new());
	gtk_widget_set_margin_left( GTK_WIDGET( book ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( book ), 4 );

	famset = ofo_dossier_get_mod_families_set( self->private->dossier );

	for( ifam=famset ; ifam ; ifam=ifam->next ){
		notebook_create_page( self, book,
				ofo_mod_family_get_id( OFO_MOD_FAMILY( ifam->data )),
				ofo_mod_family_get_label( OFO_MOD_FAMILY( ifam->data )),
				-1 );
	}

	g_list_free( famset );

	for( imod=self->private->set ; imod ; imod=imod->next ){
		ofomodel = OFO_MODEL( imod->data );
		fam_id = ofo_model_get_family( ofomodel );
		page = notebook_find_page( self, fam_id );

		g_return_val_if_fail( page && GTK_IS_WIDGET( page ), NULL );

		view = GTK_TREE_VIEW( g_object_get_data( G_OBJECT( page ), DATA_PAGE_VIEW ));
		model = gtk_tree_view_get_model( view );
		gtk_list_store_append( GTK_LIST_STORE( model ), &iter );
		store_set_model( model, &iter, ofomodel );
	}

	g_signal_connect( G_OBJECT( book ), "switch-page", G_CALLBACK( on_family_page_switched ), self );

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

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_Nouveau..." )));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_new_model ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_Modifier..." )));
	gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_update_model ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	self->private->update_btn = button;

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_Supprimer..." )));
	gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_delete_model ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	self->private->delete_btn = button;

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
notebook_create_page( ofaModelsSet *self, GtkNotebook *book, gint fam_id, const gchar *fam_label, gint position )
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

	label = GTK_LABEL( gtk_label_new( fam_label ));
	gtk_notebook_insert_page( book, GTK_WIDGET( scroll ), GTK_WIDGET( label ), position );
	gtk_notebook_set_tab_reorderable( book, GTK_WIDGET( scroll ), TRUE );
	g_object_set_data( G_OBJECT( scroll ), DATA_PAGE_FAMILY, GINT_TO_POINTER( fam_id ));

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
			_( "Mnémo" ),
			text_cell, "text", COL_MNEMO,
			NULL );
	gtk_tree_view_append_column( view, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Intitulé" ),
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
notebook_find_page( ofaModelsSet *self, gint fam_id )
{
	gint count, i;
	GtkWidget *page, *found;
	gint page_fam;

	count = gtk_notebook_get_n_pages( self->private->book );
	found = NULL;

	for( i=0 ; i<count ; ++i ){
		page = gtk_notebook_get_nth_page( self->private->book, i );
		page_fam = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( page ), DATA_PAGE_FAMILY ));
		if( page_fam == fam_id ){
			found = page;
			break;
		}
	}

	if( !found ){
		if( !self->private->others ){
			self->private->others =
					notebook_create_page( self, self->private->book, -1, _( "Non classés" ), -1 );
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
	static const gchar *thisfn = "ofa_models_set_on_model_selected";
	gboolean select_ok;
	GtkTreeModel *model;
	GtkTreeIter iter;
	ofoModel *ofomodel;

	select_ok = FALSE;

	if( self->private->selected ){
		self->private->selected = NULL;
		self->private->model = NULL;
		gtk_tree_iter_free( self->private->iter );
		self->private->iter = NULL;
	}

	if( gtk_tree_selection_get_selected( selection, &model, &iter )){

		gtk_tree_model_get( model, &iter, COL_OBJECT, &ofomodel, -1 );
		g_return_if_fail( OFO_IS_MODEL( ofomodel ));

		g_debug( "%s: selecting %s - %s",
				thisfn, ofo_model_get_mnemo( ofomodel ), ofo_model_get_label( ofomodel ));

		select_ok = TRUE;
		self->private->selected = ofomodel;
		self->private->model = model;
		self->private->iter = gtk_tree_iter_copy( &iter );

		g_object_unref( ofomodel );
	}

	gtk_widget_set_sensitive( GTK_WIDGET( self->private->update_btn ), select_ok );
	gtk_widget_set_sensitive( GTK_WIDGET( self->private->delete_btn ), select_ok );
}

static void
on_new_model( GtkButton *button, ofaModelsSet *self )
{
	static const gchar *thisfn = "ofa_models_set_on_new_model";
	ofoModel *model;

	g_return_if_fail( OFA_IS_MODELS_SET( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	model = ofo_model_new();
	if( ofa_model_properties_run( self->private->main_window, model )){
		insert_new_row( self, model );
	}
}

static void
on_update_model( GtkButton *button, ofaModelsSet *self )
{
	static const gchar *thisfn = "ofa_models_set_on_update_model";
	gchar *prev_mnemo;
	const gchar *new_mnemo;
	ofoModel *model;

	g_return_if_fail( OFA_IS_MODELS_SET( self ));
	g_return_if_fail( OFO_IS_MODEL( self->private->selected ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	model = self->private->selected;
	prev_mnemo = g_strdup( ofo_model_get_mnemo( model ));

	if( ofa_model_properties_run( self->private->main_window, model )){

		new_mnemo = ofo_model_get_mnemo( model );
		if( g_utf8_collate( prev_mnemo, new_mnemo )){
			gtk_list_store_remove(
					GTK_LIST_STORE( self->private->model ),
					self->private->iter );
			insert_new_row( self, model );

		} else {
			store_set_model( self->private->model, self->private->iter, model );
		}
	}

	g_free( prev_mnemo );
}

/*
 * un model peut être supprimé tant qu'aucune écriture n'y a été
 * enregistrée, et après confirmation de l'utilisateur
 */
static void
on_delete_model( GtkButton *button, ofaModelsSet *self )
{
	static const gchar *thisfn = "ofa_models_set_on_delete_model";

	g_return_if_fail( OFA_IS_MODELS_SET( self ));
	g_return_if_fail( OFO_IS_MODEL( self->private->selected ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	if( delete_confirmed( self, self->private->selected ) &&
			ofo_dossier_delete_model( self->private->dossier, self->private->selected )){

		/* update our set of models */
		models_set_free( self );
		self->private->set = ofo_dossier_get_models_set( self->private->dossier );

		/* remove the row from the model
		 * this will cause an automatic new selection */
		gtk_list_store_remove( GTK_LIST_STORE( self->private->model ), self->private->iter );
	}
}

static gboolean
delete_confirmed( ofaModelsSet *self, ofoModel *model )
{
	GtkWidget *dialog;
	gchar *msg;
	gint response;

	msg = g_strdup_printf( _( "Etes-vous sûr de vouloir supprimer le modèle '%s - %s' ?" ),
			ofo_model_get_mnemo( model ),
			ofo_model_get_label( model ));

	dialog = gtk_message_dialog_new(
			GTK_WINDOW( self->private->main_window ),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			"%s", msg );

	gtk_dialog_add_buttons( GTK_DIALOG( dialog ),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_DELETE, GTK_RESPONSE_OK,
			NULL );

	response = gtk_dialog_run( GTK_DIALOG( dialog ));

	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
}

static void
insert_new_row( ofaModelsSet *self, ofoModel *ofomodel )
{
	gint fam_id;
	GtkWidget *page;
	gint page_num;
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkTreeIter iter, new_iter;
	GValue value = G_VALUE_INIT;
	const gchar *mnemo, *model_mnemo;
	gboolean iter_found;
	GtkTreePath *path;

	/* update our set of models */
	models_set_free( self );
	self->private->set = ofo_dossier_get_models_set( self->private->dossier );

	/* activate the page of the correct class, or create a new one */
	fam_id = ofo_model_get_family( ofomodel );
	page = notebook_find_page( self, fam_id );
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
	path = gtk_tree_model_get_path( self->private->model, &new_iter );
	gtk_tree_view_set_cursor( view, path, NULL, FALSE );
	gtk_widget_grab_focus( GTK_WIDGET( view ));
	gtk_tree_path_free( path );
}

static void
models_set_free( ofaModelsSet *self )
{
	g_list_free( self->private->set );
	self->private->set = NULL;
}
