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
	GtkButton     *guided_input_btn;
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

G_DEFINE_TYPE( ofaModelsSet, ofa_models_set, OFA_TYPE_MAIN_PAGE )

static GtkWidget *v_setup_view( ofaMainPage *page );
static GtkWidget *v_setup_buttons( ofaMainPage *page );
static void       v_init_view( ofaMainPage *page );
static GtkWidget *book_create_page( ofaModelsSet *self, GtkNotebook *book, const gchar *journal, const gchar *journal_label );
static gboolean   book_activate_page_by_journal( ofaModelsSet *self, const gchar *journal );
static gint       book_get_page_by_journal( ofaModelsSet *self, const gchar *journal );
static gint       on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaModelsSet *self );
static void       on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainPage *page );
static void       insert_new_row( ofaModelsSet *self, ofoModel *model, gboolean with_selection );
static void       setup_first_selection( ofaModelsSet *self );
static void       on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaModelsSet *self );
static void       on_model_selected( GtkTreeSelection *selection, ofaModelsSet *self );
static void       enable_buttons( ofaModelsSet *self, GtkTreeSelection *selection );
static void       v_on_new_clicked( GtkButton *button, ofaMainPage *page );
static void       v_on_update_clicked( GtkButton *button, ofaMainPage *page );
static void       v_on_delete_clicked( GtkButton *button, ofaMainPage *page );
static gboolean   delete_confirmed( ofaModelsSet *self, ofoModel *model );
static void       on_guided_input( GtkButton *button, ofaModelsSet *self );

static void
models_set_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_models_set_finalize";
	ofaModelsSetPrivate *priv;

	g_return_if_fail( OFA_IS_MODELS_SET( instance ));

	priv = OFA_MODELS_SET( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_models_set_parent_class )->finalize( instance );
}

static void
models_set_dispose( GObject *instance )
{
	ofaModelsSetPrivate *priv;

	g_return_if_fail( OFA_IS_MODELS_SET( instance ));

	priv = ( OFA_MODELS_SET( instance ))->private;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_models_set_parent_class )->dispose( instance );
}

static void
ofa_models_set_init( ofaModelsSet *self )
{
	static const gchar *thisfn = "ofa_models_set_init";

	g_return_if_fail( OFA_IS_MODELS_SET( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaModelsSetPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
ofa_models_set_class_init( ofaModelsSetClass *klass )
{
	static const gchar *thisfn = "ofa_models_set_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = models_set_dispose;
	G_OBJECT_CLASS( klass )->finalize = models_set_finalize;

	OFA_MAIN_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_MAIN_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_MAIN_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_MAIN_PAGE_CLASS( klass )->on_new_clicked = v_on_new_clicked;
	OFA_MAIN_PAGE_CLASS( klass )->on_update_clicked = v_on_update_clicked;
	OFA_MAIN_PAGE_CLASS( klass )->on_delete_clicked = v_on_delete_clicked;
}

static GtkWidget *
v_setup_view( ofaMainPage *page )
{
	ofaModelsSet *self;
	GtkNotebook *book;
	GList *dataset, *iset;
	ofoJournal *journal;

	self = OFA_MODELS_SET( page );

	book = GTK_NOTEBOOK( gtk_notebook_new());
	gtk_widget_set_margin_left( GTK_WIDGET( book ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( book ), 4 );
	gtk_widget_set_hexpand( GTK_WIDGET( book ), TRUE );
	gtk_notebook_set_scrollable( book, TRUE );
	self->private->book = GTK_NOTEBOOK( book );

	dataset = ofo_journal_get_dataset( ofa_main_page_get_dossier( page ));

	for( iset=dataset ; iset ; iset=iset->next ){
		journal = OFO_JOURNAL( iset->data );
		book_create_page( self, book, ofo_journal_get_mnemo( journal ), ofo_journal_get_label( journal ));
	}

	/* connect after the pages have been created */
	g_signal_connect( G_OBJECT( book ), "switch-page", G_CALLBACK( on_page_switched ), self );

	return( GTK_WIDGET( book ));
}

static GtkWidget *
v_setup_buttons( ofaMainPage *page )
{
	GtkBox *buttons_box;
	GtkFrame *frame;
	GtkButton *button;

	buttons_box = GTK_BOX(
					OFA_MAIN_PAGE_CLASS( ofa_models_set_parent_class )->setup_buttons( page ));
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
	GList *dataset, *iset;

	dataset = ofo_model_get_dataset( ofa_main_page_get_dossier( page ));

	for( iset=dataset ; iset ; iset=iset->next ){

		insert_new_row( OFA_MODELS_SET( page ), OFO_MODEL( iset->data ), FALSE );
	}

	setup_first_selection( OFA_MODELS_SET( page ));
}

static GtkWidget *
book_create_page( ofaModelsSet *self, GtkNotebook *book, const gchar *journal, const gchar *journal_label )
{
	GtkScrolledWindow *scroll;
	GtkLabel *label;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );

	label = GTK_LABEL( gtk_label_new_with_mnemonic( journal_label ));
	gtk_notebook_insert_page( book, GTK_WIDGET( scroll ), GTK_WIDGET( label ), -1 );
	gtk_notebook_set_tab_reorderable( book, GTK_WIDGET( scroll ), TRUE );
	g_object_set_data( G_OBJECT( scroll ), DATA_PAGE_JOURNAL, g_strdup( journal ));

	tview = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_vexpand( GTK_WIDGET( tview ), TRUE );
	gtk_tree_view_set_headers_visible( tview, TRUE );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( tview ));
	g_object_set_data( G_OBJECT( scroll ), DATA_PAGE_VIEW, tview );
	g_signal_connect(G_OBJECT( tview ), "row-activated", G_CALLBACK( on_row_activated ), self );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( tview, tmodel );
	g_object_unref( tmodel );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Mnemo" ),
			text_cell, "text", COL_MNEMO,
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
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_model_selected ), self );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ), ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	gtk_widget_show_all( GTK_WIDGET( scroll ));

	return( GTK_WIDGET( scroll ));
}

static gboolean
book_activate_page_by_journal( ofaModelsSet *self, const gchar *mnemo )
{
	gint page_num;

	page_num = book_get_page_by_journal( self, mnemo );
	if( page_num < 0 ){
		page_num = book_get_page_by_journal( self, "__XX__" );
		if( page_num < 0 ){
				book_create_page( self, self->private->book, "__XX__", _( "Unclassed" ));
				page_num = book_get_page_by_journal( self, "__XX__" );
		}
	}
	g_return_val_if_fail( page_num >= 0, FALSE );

	gtk_notebook_set_current_page( self->private->book, page_num );
	return( TRUE );
}

static gint
book_get_page_by_journal( ofaModelsSet *self, const gchar *mnemo )
{
	gint count, i;
	GtkWidget *page_widget;
	const gchar *journal;

	count = gtk_notebook_get_n_pages( self->private->book );

	for( i=0 ; i<count ; ++i ){
		page_widget = gtk_notebook_get_nth_page( self->private->book, i );
		journal = ( const gchar * ) g_object_get_data( G_OBJECT( page_widget ), DATA_PAGE_JOURNAL );
		if( !g_utf8_collate( journal, mnemo )){
			return( i );
		}
	}

	return( -1 );
}

static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaModelsSet *self )
{
	gchar *amnemo, *bmnemo, *afold, *bfold;
	gint cmp;

	gtk_tree_model_get( tmodel, a, COL_MNEMO, &amnemo, -1 );
	afold = g_utf8_casefold( amnemo, -1 );

	gtk_tree_model_get( tmodel, b, COL_MNEMO, &bmnemo, -1 );
	bfold = g_utf8_casefold( bmnemo, -1 );

	cmp = g_utf8_collate( afold, bfold );

	g_free( amnemo );
	g_free( afold );
	g_free( bmnemo );
	g_free( bfold );

	return( cmp );
}

/*
 * double click on a row opens the rate properties
 */
static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainPage *page )
{
	v_on_update_clicked( NULL, page );
}

static void
insert_new_row( ofaModelsSet *self, ofoModel *model, gboolean with_selection )
{
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;

	/* find the page for this journal and activates it
	 * creating a new page if the journal has not been found */
	if( book_activate_page_by_journal( self, ofo_model_get_journal( model ))){

		tview = ofa_main_page_get_treeview( OFA_MAIN_PAGE( self ));
		g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));

		tmodel = gtk_tree_view_get_model( tview );

		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				COL_MNEMO,  ofo_model_get_mnemo( model ),
				COL_LABEL,  ofo_model_get_label( model ),
				COL_OBJECT, model,
				-1 );

		/* select the newly added row */
		if( with_selection ){
			path = gtk_tree_model_get_path( tmodel, &iter );
			gtk_tree_view_set_cursor( tview, path, NULL, FALSE );
			gtk_tree_path_free( path );
			gtk_widget_grab_focus( GTK_WIDGET( tview ));
		}
	}
}

static void
setup_first_selection( ofaModelsSet *self )
{
	GtkWidget *first_tab;
	GtkTreeView *first_treeview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	first_tab = gtk_notebook_get_nth_page( GTK_NOTEBOOK( self->private->book ), 0 );

	if( first_tab ){
		gtk_notebook_set_current_page( GTK_NOTEBOOK( self->private->book ), 0 );
		first_treeview =
				( GtkTreeView * ) my_utils_container_get_child_by_type(
												GTK_CONTAINER( first_tab ),
												GTK_TYPE_TREE_VIEW );
		g_return_if_fail( first_treeview && GTK_IS_TREE_VIEW( first_treeview ));

		tmodel = gtk_tree_view_get_model( first_treeview );
		if( gtk_tree_model_get_iter_first( tmodel, &iter )){
			select = gtk_tree_view_get_selection( first_treeview );
			gtk_tree_selection_select_iter( select, &iter );
		}

		gtk_widget_grab_focus( GTK_WIDGET( first_treeview ));
	}
}

/*
 * note that switching the notebook page, though *visually* selects the
 * first row, actually does NOT send any selection message (so does not
 * trigger #on_model_selected()) when the selection does not change on
 * the treeview:
 *
 * - when activating a notebook for the first time, and if there is at
 *   least one row, then select this row (and emit the signal)
 *
 * - when coming back another time to this same page, then the selection
 *   does not change, and though no message is sent.
 */
static void
on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaModelsSet *self )
{
	static const gchar *thisfn = "ofa_models_set_on_page_switched";
	GtkTreeView *tview;

	g_debug( "%s: book=%p, wpage=%p, npage=%d, page=%p",
			thisfn, ( void * ) book, ( void * ) wpage, npage, ( void * ) self );

	tview = ( GtkTreeView * )
					my_utils_container_get_child_by_type(
							GTK_CONTAINER( wpage ), GTK_TYPE_TREE_VIEW );
	if( tview ){
		g_return_if_fail( GTK_IS_TREE_VIEW( tview ));
		enable_buttons( self, gtk_tree_view_get_selection( tview ));
	}
}

static void
on_model_selected( GtkTreeSelection *selection, ofaModelsSet *self )
{
	/*static const gchar *thisfn = "ofa_models_set_on_model_selected";*/

	/*g_debug( "%s: selection=%p, self=%p", thisfn, ( void * ) selection, ( void * ) self );*/

	enable_buttons( self, selection );
}

static void
enable_buttons( ofaModelsSet *self, GtkTreeSelection *selection )
{
	/*static const gchar *thisfn = "ofa_models_set_enable_buttons";*/
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoModel *model;
	gboolean select_ok;

	/*g_debug( "%s: self=%p, selection=%p", thisfn, ( void * ) self, ( void * ) selection );*/

	select_ok = gtk_tree_selection_get_selected( selection, &tmodel, &iter );

	/*g_debug( "%s: select_ok=%s", thisfn, select_ok ? "True":"False" );*/

	if( select_ok ){
		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &model, -1 );
		g_object_unref( model );
		/*g_debug( "%s: current selection is %s", thisfn, ofo_model_get_mnemo( model ));*/

		gtk_widget_set_sensitive(
				ofa_main_page_get_update_btn( OFA_MAIN_PAGE( self )),
				model && OFO_IS_MODEL( model ));
		gtk_widget_set_sensitive(
				ofa_main_page_get_delete_btn( OFA_MAIN_PAGE( self )),
				model && OFO_IS_MODEL( model ) && ofo_model_is_deletable( model ));

	} else {
		gtk_widget_set_sensitive(
				ofa_main_page_get_update_btn( OFA_MAIN_PAGE( self )), FALSE );
		gtk_widget_set_sensitive(
				ofa_main_page_get_delete_btn( OFA_MAIN_PAGE( self )), FALSE );
	}

	gtk_widget_set_sensitive(
			GTK_WIDGET( self->private->guided_input_btn ), select_ok );
}

static void
v_on_new_clicked( GtkButton *button, ofaMainPage *page )
{
	static const gchar *thisfn = "ofa_models_set_v_on_new_clicked";
	ofoModel *model;
	gint page_n;
	GtkWidget *page_w;
	const gchar *mnemo;

	g_return_if_fail( OFA_IS_MODELS_SET( page ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) page );

	model = ofo_model_new();
	page_n = gtk_notebook_get_current_page( OFA_MODELS_SET( page )->private->book );
	page_w = gtk_notebook_get_nth_page( OFA_MODELS_SET( page )->private->book, page_n );
	mnemo = ( const gchar * ) g_object_get_data( G_OBJECT( page_w ), DATA_PAGE_JOURNAL );

	if( ofa_model_properties_run(
			ofa_main_page_get_main_window( page ), model, mnemo )){

		insert_new_row( OFA_MODELS_SET( page ), model, TRUE );

	} else {
		g_object_unref( model );
	}
}

static void
v_on_update_clicked( GtkButton *button, ofaMainPage *page )
{
	static const gchar *thisfn = "ofa_models_set_v_on_update_clicked";
	ofaModelsSet *self;
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *prev_mnemo;
	const gchar *new_mnemo;
	ofoModel *model;

	g_return_if_fail( OFA_IS_MODELS_SET( page ));
	self = OFA_MODELS_SET( page );

	g_debug( "%s: button=%p, page=%p", thisfn, ( void * ) button, ( void * ) page );

	tview = ofa_main_page_get_treeview( page );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));

	select = gtk_tree_view_get_selection( tview );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &model, -1 );
		g_object_unref( model );

		prev_mnemo = g_strdup( ofo_model_get_mnemo( model ));

		if( ofa_model_properties_run(
				ofa_main_page_get_main_window( OFA_MAIN_PAGE( self )),
				model,
				NULL )){

			new_mnemo = ofo_model_get_mnemo( model );
			if( g_utf8_collate( prev_mnemo, new_mnemo )){
				gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
				insert_new_row( self, model, TRUE );

			} else {
				gtk_list_store_set(
						GTK_LIST_STORE( tmodel ),
						&iter,
						COL_MNEMO,  ofo_model_get_mnemo( model ),
						COL_LABEL,  ofo_model_get_label( model ),
						-1 );
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
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoModel *model;

	g_return_if_fail( OFA_IS_MODELS_SET( page ));
	self = OFA_MODELS_SET( page );

	g_debug( "%s: button=%p, page=%p", thisfn, ( void * ) button, ( void * ) page );

	tview = ofa_main_page_get_treeview( page );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));

	select = gtk_tree_view_get_selection( tview );
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
on_guided_input( GtkButton *button, ofaModelsSet *self )
{
	static const gchar *thisfn = "ofa_models_set_on_guided_input";
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoModel *model;

	g_return_if_fail( OFA_IS_MODELS_SET( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	tview = ofa_main_page_get_treeview( OFA_MAIN_PAGE( self ));
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));

	select = gtk_tree_view_get_selection( tview );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &model, -1 );
		g_object_unref( model );

		ofa_guided_input_run(
				ofa_main_page_get_main_window( OFA_MAIN_PAGE( self )), model );
	}
}
