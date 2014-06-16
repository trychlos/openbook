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

#include "core/my-utils.h"
#include "ui/ofa-main-page.h"
#include "ui/ofa-guided-input.h"
#include "ui/ofa-model-properties.h"
#include "ui/ofa-models-set.h"
#include "api/ofo-base.h"
#include "api/ofo-dossier.h"
#include "api/ofo-model.h"
#include "api/ofo-journal.h"

/* private instance data
 */
struct _ofaModelsSetPrivate {
	gboolean       dispose_has_run;

	/* properties
	 */

	/* internals
	 */
	GList         *handlers;

	/* UI
	 */
	GtkNotebook   *book;				/* one page per journal d'imputation */
	GtkTreeView   *tview;				/* the treeview of the current page */
	GtkTreeModel  *tmodel;
	GtkButton     *duplicate_btn;
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

/* an antry model not attached to any journal
 */
#define UNKNOWN_JOURNAL_MNEMO            "__xx__"
#define UNKNOWN_JOURNAL_LABEL            _( "Unclassed" )

/* data attached to each page of the model category notebook
 */
#define DATA_PAGE_JOURNAL                "data-page-journal-id"
#define DATA_PAGE_VIEW                   "data-page-treeview"

G_DEFINE_TYPE( ofaModelsSet, ofa_models_set, OFA_TYPE_MAIN_PAGE )

static GtkWidget *v_setup_view( ofaMainPage *page );
static void       setup_dossier_signaling( ofaModelsSet *self );
static GtkWidget *v_setup_buttons( ofaMainPage *page );
static void       v_init_view( ofaMainPage *page );
static void       insert_dataset( ofaModelsSet *self );
static GtkWidget *book_create_page( ofaModelsSet *self, GtkNotebook *book, const gchar *journal, const gchar *journal_label );
static gboolean   book_activate_page_by_journal( ofaModelsSet *self, const gchar *journal );
static gint       book_get_page_by_journal( ofaModelsSet *self, const gchar *journal );
static gint       on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaModelsSet *self );
static void       on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainPage *page );
static void       insert_new_row( ofaModelsSet *self, ofoModel *model, gboolean with_selection );
static void       set_row_by_iter( ofaModelsSet *self, ofoModel *model, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void       setup_first_selection( ofaModelsSet *self );
static void       on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaModelsSet *self );
static void       on_model_selected( GtkTreeSelection *selection, ofaModelsSet *self );
static void       enable_buttons( ofaModelsSet *self, GtkTreeSelection *selection );
static void       v_on_new_clicked( GtkButton *button, ofaMainPage *page );
static void       on_new_object( ofoDossier *dossier, ofoBase *object, ofaModelsSet *self );
static void       v_on_update_clicked( GtkButton *button, ofaMainPage *page );
static void       on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaModelsSet *self );
static void       v_on_delete_clicked( GtkButton *button, ofaMainPage *page );
static gboolean   delete_confirmed( ofaModelsSet *self, ofoModel *model );
static void       on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaModelsSet *self );
static void       on_duplicate( GtkButton *button, ofaModelsSet *self );
static void       on_guided_input( GtkButton *button, ofaModelsSet *self );
static void       on_reloaded_dataset( ofoDossier *dossier, GType type, ofaModelsSet *self );

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
	gulong handler_id;
	GList *iha;
	ofoDossier *dossier;

	g_return_if_fail( OFA_IS_MODELS_SET( instance ));

	priv = ( OFA_MODELS_SET( instance ))->private;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		/* note when deconnecting the handlers that the dossier may
		 * have been already finalized (e.g. when the application
		 * terminates) */
		dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( instance ));
		if( OFO_IS_DOSSIER( dossier )){
			for( iha=priv->handlers ; iha ; iha=iha->next ){
				handler_id = ( gulong ) iha->data;
				g_signal_handler_disconnect( dossier, handler_id );
			}
		}
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

	setup_dossier_signaling( self );

	book = GTK_NOTEBOOK( gtk_notebook_new());
	gtk_widget_set_margin_left( GTK_WIDGET( book ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( book ), 4 );
	gtk_widget_set_hexpand( GTK_WIDGET( book ), TRUE );
	gtk_notebook_set_scrollable( book, TRUE );
	gtk_notebook_popup_enable( book );

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

static void
setup_dossier_signaling( ofaModelsSet *self )
{
	ofaModelsSetPrivate *priv;
	ofoDossier *dossier;
	gulong handler;

	priv = self->private;
	dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ));

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						OFA_SIGNAL_NEW_OBJECT, G_CALLBACK( on_new_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						OFA_SIGNAL_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						OFA_SIGNAL_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						OFA_SIGNAL_RELOAD_DATASET, G_CALLBACK( on_reloaded_dataset ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );
}

static GtkWidget *
v_setup_buttons( ofaMainPage *page )
{
	GtkBox *buttons_box;
	GtkFrame *frame;
	GtkButton *button;

	buttons_box = GTK_BOX(
					OFA_MAIN_PAGE_CLASS( ofa_models_set_parent_class )->setup_buttons( page ));

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "Dup_licate" )));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_duplicate ), page );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	OFA_MODELS_SET( page )->private->duplicate_btn = button;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_size_request( GTK_WIDGET( frame ), -1, 12 );
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
	insert_dataset( OFA_MODELS_SET( page ));
}

static void
insert_dataset( ofaModelsSet *self )
{
	GList *dataset, *iset;

	dataset = ofo_model_get_dataset( ofa_main_page_get_dossier( OFA_MAIN_PAGE( self )));

	for( iset=dataset ; iset ; iset=iset->next ){
		insert_new_row( self, OFO_MODEL( iset->data ), FALSE );
	}

	setup_first_selection( self );
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
		page_num = book_get_page_by_journal( self, UNKNOWN_JOURNAL_MNEMO );
		if( page_num < 0 ){
				book_create_page( self, self->private->book, UNKNOWN_JOURNAL_MNEMO, UNKNOWN_JOURNAL_LABEL );
				page_num = book_get_page_by_journal( self, UNKNOWN_JOURNAL_MNEMO );
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
	ofaModelsSetPrivate *priv;
	GtkTreeIter iter;
	GtkTreePath *path;

	/* find the page for this journal and activates it
	 * creating a new page if the journal has not been found */
	if( book_activate_page_by_journal( self, ofo_model_get_journal( model ))){

		priv = self->private;

		g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));
		g_return_if_fail( priv->tmodel && GTK_IS_TREE_MODEL( priv->tmodel ));

		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( priv->tmodel ),
				&iter,
				-1,
				COL_MNEMO,  ofo_model_get_mnemo( model ),
				COL_OBJECT, model,
				-1 );

		set_row_by_iter( self, model, priv->tmodel, &iter );

		/* select the newly added row */
		if( with_selection ){
			path = gtk_tree_model_get_path( priv->tmodel, &iter );
			gtk_tree_view_set_cursor( priv->tview, path, NULL, FALSE );
			gtk_tree_path_free( path );
			gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
		}
	}
}

static void
set_row_by_iter( ofaModelsSet *self, ofoModel *model, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gtk_list_store_set(
			GTK_LIST_STORE( tmodel ),
			iter,
			COL_MNEMO,  ofo_model_get_mnemo( model ),
			COL_LABEL,  ofo_model_get_label( model ),
			-1 );
}

static void
setup_first_selection( ofaModelsSet *self )
{
	ofaModelsSetPrivate *priv;
	GtkWidget *first_tab;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	priv = self->private;

	first_tab = gtk_notebook_get_nth_page( GTK_NOTEBOOK( self->private->book ), 0 );

	if( first_tab ){
		gtk_notebook_set_current_page( GTK_NOTEBOOK( self->private->book ), 0 );
		priv->tview =
				( GtkTreeView * ) my_utils_container_get_child_by_type(
												GTK_CONTAINER( first_tab ),
												GTK_TYPE_TREE_VIEW );
		g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));

		priv->tmodel = gtk_tree_view_get_model( priv->tview );
		if( gtk_tree_model_get_iter_first( priv->tmodel, &iter )){
			select = gtk_tree_view_get_selection( priv->tview );
			gtk_tree_selection_select_iter( select, &iter );
		}

		gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
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
 *
 * After reloading the dataset, the notebook switches to the first page
 * before having created the treeview
 */
static void
on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaModelsSet *self )
{
	static const gchar *thisfn = "ofa_models_set_on_page_switched";
	ofaModelsSetPrivate *priv;

	g_debug( "%s: book=%p, wpage=%p, npage=%d, page=%p",
			thisfn, ( void * ) book, ( void * ) wpage, npage, ( void * ) self );

	priv = self->private;

	priv->tview = ( GtkTreeView * )
							my_utils_container_get_child_by_type(
									GTK_CONTAINER( wpage ), GTK_TYPE_TREE_VIEW );
	if( priv->tview ){
		g_return_if_fail( GTK_IS_TREE_VIEW( priv->tview ));
		priv->tmodel = gtk_tree_view_get_model( priv->tview );
		enable_buttons( self, gtk_tree_view_get_selection(priv-> tview ));

	} else {
		enable_buttons( self, NULL );
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

	select_ok = selection
					? gtk_tree_selection_get_selected( selection, &tmodel, &iter )
					: FALSE;

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
			GTK_WIDGET( self->private->duplicate_btn ), select_ok );
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

		/* managed by dossier signaling system */

	} else {
		g_object_unref( model );
	}
}

static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaModelsSet *self )
{
	static const gchar *thisfn = "ofa_models_set_on_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_MODEL( object )){
		insert_new_row( self, OFO_MODEL( object ), TRUE );
	}
}

/*
 * We cannot rely here on the standard dossier signaling system.
 * This is due because we display the entry models in a notebook per
 * journal - So managing a journal change implies having both the
 * previous identifier and the previous journal.
 * As this is the only use case, we consider upgrading the dossier
 * signaling system of no worth..
 */
static void
v_on_update_clicked( GtkButton *button, ofaMainPage *page )
{
	static const gchar *thisfn = "ofa_models_set_v_on_update_clicked";
	ofaModelsSetPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	gchar *prev_journal;
	const gchar *new_journal;
	ofoModel *model;

	g_return_if_fail( OFA_IS_MODELS_SET( page ));

	priv = OFA_MODELS_SET( page )->private;

	g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));
	g_return_if_fail( priv->tmodel && GTK_IS_TREE_MODEL( priv->tmodel ));

	g_debug( "%s: button=%p, page=%p", thisfn, ( void * ) button, ( void * ) page );

	select = gtk_tree_view_get_selection( priv->tview );
	if( gtk_tree_selection_get_selected( select, NULL, &iter )){

		gtk_tree_model_get( priv->tmodel, &iter, COL_OBJECT, &model, -1 );
		g_object_unref( model );

		prev_journal = g_strdup( ofo_model_get_journal( model ));

		if( ofa_model_properties_run(
				ofa_main_page_get_main_window( page ),
				model,
				NULL )){

			new_journal = ofo_model_get_journal( model );

			if( g_utf8_collate( prev_journal, new_journal )){
				gtk_list_store_remove( GTK_LIST_STORE( priv->tmodel ), &iter );
				insert_new_row( OFA_MODELS_SET( page ), model, TRUE );

			} else {
				set_row_by_iter( OFA_MODELS_SET( page ), model, priv->tmodel, &iter );
			}
		}

		g_free( prev_journal );
	}
}

static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaModelsSet *self )
{
	static const gchar *thisfn = "ofa_models_set_on_updated_object";

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_MODEL( object )){
		/* managed by the button clic handle */

	} else if( OFO_IS_JOURNAL( object )){
		/* a journal has changed */

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
	ofaModelsSetPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	ofoModel *model;

	g_return_if_fail( OFA_IS_MODELS_SET( page ));

	priv = OFA_MODELS_SET( page )->private;

	g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));
	g_return_if_fail( priv->tmodel && GTK_IS_TREE_MODEL( priv->tmodel ));

	g_debug( "%s: button=%p, page=%p", thisfn, ( void * ) button, ( void * ) page );

	select = gtk_tree_view_get_selection( priv->tview );
	if( gtk_tree_selection_get_selected( select, NULL, &iter )){

		gtk_tree_model_get( priv->tmodel, &iter, COL_OBJECT, &model, -1 );
		g_object_unref( model );

		if( delete_confirmed( OFA_MODELS_SET( page ), model ) &&
				ofo_model_delete( model )){

			/* remove the row from the model
			 * this will cause an automatic new selection */
			gtk_list_store_remove( GTK_LIST_STORE( priv->tmodel ), &iter );
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
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaModelsSet *self )
{
	static const gchar *thisfn = "ofa_models_set_on_deleted_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_MODEL( object )){
		/* manage by the button clic handle */

	} else if( OFO_IS_JOURNAL( object )){
		/* a journal has been deleted */
	}
}

static void
on_duplicate( GtkButton *button, ofaModelsSet *self )
{
	static const gchar *thisfn = "ofa_models_set_on_duplicate";
	ofaModelsSetPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	ofoModel *model;
	ofoModel *duplicate;
	gchar *str;

	g_return_if_fail( OFA_IS_MODELS_SET( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	priv = self->private;

	g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));
	g_return_if_fail( priv->tmodel && GTK_IS_TREE_MODEL( priv->tmodel ));

	select = gtk_tree_view_get_selection( priv->tview );
	if( gtk_tree_selection_get_selected( select, NULL, &iter )){

		gtk_tree_model_get( priv->tmodel, &iter, COL_OBJECT, &model, -1 );
		g_object_unref( model );

		duplicate = ofo_model_new();
		ofo_model_copy( model, duplicate );
		str = ofo_model_get_mnemo_new_from( model );
		ofo_model_set_mnemo( duplicate, str );
		g_free( str );
		str = g_strdup_printf( "%s (%s)", ofo_model_get_label( model ), _( "Duplicate" ));
		ofo_model_set_label( duplicate, str );
		g_free( str );

		if( !ofo_model_insert( duplicate )){
			g_object_unref( duplicate );
		}
	}
}

static void
on_guided_input( GtkButton *button, ofaModelsSet *self )
{
	static const gchar *thisfn = "ofa_models_set_on_guided_input";
	ofaModelsSetPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	ofoModel *model;

	g_return_if_fail( OFA_IS_MODELS_SET( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	priv = self->private;

	g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));
	g_return_if_fail( priv->tmodel && GTK_IS_TREE_MODEL( priv->tmodel ));

	select = gtk_tree_view_get_selection( priv->tview );
	if( gtk_tree_selection_get_selected( select, NULL, &iter )){

		gtk_tree_model_get( priv->tmodel, &iter, COL_OBJECT, &model, -1 );
		g_object_unref( model );

		ofa_guided_input_run(
				ofa_main_page_get_main_window( OFA_MAIN_PAGE( self )), model );
	}
}

/*
 * all pages are rebuilt not only if the entry models is reloaded, but
 * also when the journals are reloaded
 */
static void
on_reloaded_dataset( ofoDossier *dossier, GType type, ofaModelsSet *self )
{
	static const gchar *thisfn = "ofa_models_set_on_reloaded_dataset";

	g_debug( "%s: dossier=%p, type=%lu, self=%p",
			thisfn, ( void * ) dossier, type, ( void * ) self );

	if( type == OFO_TYPE_MODEL || type == OFO_TYPE_JOURNAL ){
		while( gtk_notebook_get_n_pages( self->private->book )){
			gtk_notebook_remove_page( self->private->book, 0 );
		}
		insert_dataset( self );
	}
}
