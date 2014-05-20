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
#include "ui/ofa-account-notebook.h"
#include "ui/ofo-account.h"

/* private class data
 */
struct _ofaAccountNotebookClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaAccountNotebookPrivate {
	gboolean       dispose_has_run;

	/* input data
	 */
	GtkNotebook   *book;				/* with one page per account class */
	ofoDossier    *dossier;
	ofaAccountNotebookCb pfnSelect;
	gpointer             user_data_select;
	ofaAccountNotebookCb pfnDoubleClic;
	gpointer             user_data_double_clic;

	/* runtime
	 */
	GtkTreeView   *view;				/* the tree view of the current page */
};

/* column ordering in the listview
 */
enum {
	COL_NUMBER = 0,
	COL_LABEL,
	COL_DEBIT,
	COL_CREDIT,
	COL_OBJECT,
	N_COLUMNS
};

static const gchar  *st_class_labels[] = {
		N_( "Class I" ),
		N_( "Class II" ),
		N_( "Class III" ),
		N_( "Class IV" ),
		N_( "Financial accounts" ),
		N_( "Expense accounts" ),
		N_( "Revenue accounts" ),
		N_( "Class VIII" ),
		N_( "Class IX" ),
		NULL
};

/* data attached to each page of the classes notebook
 */
#define DATA_PAGE_CLASS                 "data-page-class"

static GObjectClass *st_parent_class = NULL;

static GType      register_type( void );
static void       class_init( ofaAccountNotebookClass *klass );
static void       instance_init( GTypeInstance *instance, gpointer klass );
static void       instance_dispose( GObject *instance );
static void       instance_finalize( GObject *instance );
static void       setup_book( ofaAccountNotebook *self );
static GtkWidget *book_create_page( ofaAccountNotebook *self, GtkNotebook *book, gint class, gint position );
static void       store_set_account( GtkTreeModel *model, GtkTreeIter *iter, const ofoAccount *account );
static void       setup_first_selection( ofaAccountNotebook *self );
static void       on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaAccountNotebook *self );
static void       on_account_selected( GtkTreeSelection *selection, ofaAccountNotebook *self );
static void       on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaAccountNotebook *self );
static GtkWidget *find_page( ofaAccountNotebook *self, const gchar *number );

GType
ofa_account_notebook_get_type( void )
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
	static const gchar *thisfn = "ofa_account_notebook_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaAccountNotebookClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaAccountNotebook ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "ofaAccountNotebook", &info, 0 );

	return( type );
}

static void
class_init( ofaAccountNotebookClass *klass )
{
	static const gchar *thisfn = "ofa_account_notebook_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofaAccountNotebookClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_account_notebook_instance_init";
	ofaAccountNotebook *self;

	g_return_if_fail( OFA_IS_ACCOUNT_NOTEBOOK( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_ACCOUNT_NOTEBOOK( instance );

	self->private = g_new0( ofaAccountNotebookPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_notebook_instance_dispose";
	ofaAccountNotebookPrivate *priv;

	g_return_if_fail( OFA_IS_ACCOUNT_NOTEBOOK( instance ));

	priv = ( OFA_ACCOUNT_NOTEBOOK( instance ))->private;

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
	static const gchar *thisfn = "ofa_account_notebook_instance_finalize";
	ofaAccountNotebook *self;

	g_return_if_fail( OFA_IS_ACCOUNT_NOTEBOOK( instance ));

	g_debug( "%s: instance=%p (%s)", thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFA_ACCOUNT_NOTEBOOK( instance );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( instance );
	}
}

static void
on_dialog_finalized( ofaAccountNotebook *self, gpointer this_was_the_dialog )
{
	g_return_if_fail( self && OFA_IS_ACCOUNT_NOTEBOOK( self ));
	g_object_unref( self );
}

/**
 * ofa_account_notebook_init_dialog:
 */
ofaAccountNotebook *
ofa_account_notebook_init_dialog( ofaAccountNotebookParms *parms  )
{
	static const gchar *thisfn = "ofa_account_notebook_init_dialog";
	ofaAccountNotebook *self;

	g_return_val_if_fail( parms, NULL );

	g_debug( "%s: parms=%p", thisfn, ( void * ) parms );

	g_return_val_if_fail( parms->book && GTK_IS_NOTEBOOK( parms->book ), NULL );
	g_return_val_if_fail( parms->dossier && OFO_IS_DOSSIER( parms->dossier), NULL );

	self = g_object_new( OFA_TYPE_ACCOUNT_NOTEBOOK, NULL );

	self->private->book = parms->book;
	self->private->dossier = parms->dossier;
	self->private->pfnSelect = parms->pfnSelect;
	self->private->user_data_select = parms->user_data_select;
	self->private->pfnDoubleClic = parms->pfnDoubleClic;
	self->private->user_data_double_clic = parms->user_data_double_clic;

	/* setup a weak reference on the dialog to auto-unref */
	g_object_weak_ref( G_OBJECT( self->private->book ), ( GWeakNotify ) on_dialog_finalized, self );

	setup_book( self );
	setup_first_selection( self );

	return( self );
}

/*
 * +-----------------------------------------------------------------------+
 * | grid1 (this is the notebook page)                                     |
 * | +-----------------------------------------------+-------------------+ |
 * | | book1                                         |                   | |
 * | | each page of the book contains accounts for   |                   | |
 * | | the corresponding class (if any)              |                   | |
 * | +-----------------------------------------------+-------------------+ |
 * +-----------------------------------------------------------------------+
 */
static void
setup_book( ofaAccountNotebook *self )
{
	static const gchar *thisfn = "ofa_account_notebook_setup_book";
	GList *chart, *ic;
	gint class, prev_class;
	GtkWidget *page, *tview;
	GtkTreeModel *tmodel;
	ofoAccount *account;
	GtkTreeIter iter;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	chart = ofo_dossier_get_accounts_chart( self->private->dossier );

	for( ic=chart, prev_class=0, tmodel=NULL ; ic ; ic=ic->next ){

		/* create a page per class
		 */
		class = ofo_account_get_class( OFO_ACCOUNT( ic->data ));
		if( class != prev_class ){
			page = book_create_page( self, self->private->book, class, -1 );
			tview = my_utils_container_get_child_by_type( GTK_CONTAINER( page ), GTK_TYPE_TREE_VIEW );
			g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));
			tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( tview ));
			prev_class = class;
		}

		account = OFO_ACCOUNT( ic->data );
		gtk_list_store_append( GTK_LIST_STORE( tmodel ), &iter );
		store_set_account( tmodel, &iter, account );
	}

	g_signal_connect(
			G_OBJECT( self->private->book ), "switch-page", G_CALLBACK( on_page_switched ), self );
}

static GtkWidget *
book_create_page( ofaAccountNotebook *self, GtkNotebook *book, gint class, gint position )
{
	static const gchar *thisfn = "ofa_account_notebook_book_create_page";
	GtkScrolledWindow *scroll;
	GtkLabel *label;
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );

	label = GTK_LABEL( gtk_label_new( st_class_labels[class-1] ));
	gtk_notebook_insert_page( book, GTK_WIDGET( scroll ), GTK_WIDGET( label ), position );
	gtk_notebook_set_tab_reorderable( book, GTK_WIDGET( scroll ), TRUE );
	g_object_set_data( G_OBJECT( scroll ), DATA_PAGE_CLASS, GINT_TO_POINTER( class ));
	g_debug( "%s: adding page for class %d", thisfn, class );

	view = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_vexpand( GTK_WIDGET( view ), TRUE );
	gtk_tree_view_set_headers_visible( view, TRUE );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( view ));

	model = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( view, model );
	g_object_unref( model );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Number" ),
			text_cell, "text", COL_NUMBER,
			NULL );
	gtk_tree_view_append_column( view, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( view, column );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_end( column, text_cell, TRUE );
	/* doesn't work as expected
	label = GTK_LABEL( gtk_label_new( _( "Débit" )));
	gtk_widget_set_margin_right( GTK_WIDGET( label ), 12 );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_tree_view_column_set_widget( column, GTK_WIDGET( label ));*/
	gtk_tree_view_column_set_title( column, _( "Debit" ));
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_add_attribute( column, text_cell, "text", COL_DEBIT );
	gtk_tree_view_column_set_min_width( column, 120 );
	gtk_tree_view_append_column( view, column );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	gtk_cell_renderer_set_padding( text_cell, 12, 0 );
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_end( column, text_cell, TRUE );
	/*label = GTK_LABEL( gtk_label_new( _( "Débit" )));
	gtk_widget_set_margin_right( GTK_WIDGET( label ), 12 );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_tree_view_column_set_widget( column, GTK_WIDGET( label ));*/
	gtk_tree_view_column_set_title( column, _( "Credit" ));
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_add_attribute( column, text_cell, "text", COL_CREDIT );
	gtk_tree_view_column_set_min_width( column, 120 );
	gtk_tree_view_append_column( view, column );

	select = gtk_tree_view_get_selection( view );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_account_selected ), self );

	g_signal_connect( G_OBJECT( view ), "row-activated", G_CALLBACK( on_row_activated), self );

	return( GTK_WIDGET( scroll ));
}

static void
store_set_account( GtkTreeModel *model, GtkTreeIter *iter, const ofoAccount *account )
{
	gchar *sdeb, *scre;

	if( ofo_account_is_root( account )){
		sdeb = g_strdup( "" );
		scre = g_strdup( "" );
	} else {
		sdeb = g_strdup_printf( "%.2f €", ofo_account_get_deb_mnt( account ));
		scre = g_strdup_printf( "%.2f €", ofo_account_get_cre_mnt( account ));
	}
	gtk_list_store_set(
			GTK_LIST_STORE( model ),
			iter,
			COL_NUMBER, ofo_account_get_number( account ),
			COL_LABEL,  ofo_account_get_label( account ),
			COL_DEBIT,  sdeb,
			COL_CREDIT, scre,
			COL_OBJECT, account,
			-1 );
	g_free( scre );
	g_free( sdeb );
}

static void
setup_first_selection( ofaAccountNotebook *self )
{
	GtkWidget *first_tab;
	GtkWidget *first_treeview;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	first_tab = gtk_notebook_get_nth_page( GTK_NOTEBOOK( self->private->book ), 0 );
	if( first_tab ){
		first_treeview = my_utils_container_get_child_by_type( GTK_CONTAINER( first_tab ), GTK_TYPE_TREE_VIEW );
		g_return_if_fail( first_treeview && GTK_IS_TREE_VIEW( first_treeview ));
		model = gtk_tree_view_get_model( GTK_TREE_VIEW( first_treeview ));
		if( gtk_tree_model_get_iter_first( model, &iter )){
			select = gtk_tree_view_get_selection( GTK_TREE_VIEW( first_treeview ));
			gtk_tree_selection_select_iter( select, &iter );
		}
	}
}

static void
on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaAccountNotebook *self )
{
	GtkTreeSelection *select;

	self->private->view =
			GTK_TREE_VIEW( my_utils_container_get_child_by_type(
					GTK_CONTAINER( wpage ), GTK_TYPE_TREE_VIEW ));

	if( self->private->view ){
		g_return_if_fail( GTK_IS_TREE_VIEW( self->private->view ));
		select = gtk_tree_view_get_selection( self->private->view );
		on_account_selected( select, self );
	}
}

static void
on_account_selected( GtkTreeSelection *selection, ofaAccountNotebook *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoAccount *account;
	const gchar *number;

	if( self->private->pfnSelect &&
			gtk_tree_selection_get_selected( selection, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &account, -1 );
		g_object_unref( account );
		number = ofo_account_get_number( account );

		( *self->private->pfnSelect )( number, self->private->user_data_select );
	}
}

static void
on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaAccountNotebook *self )
{
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoAccount *account;
	const gchar *number;

	if( self->private->pfnDoubleClic ){

		select = gtk_tree_view_get_selection( tview );
		if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

			gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &account, -1 );
			g_object_unref( account );
			number = ofo_account_get_number( account );

			( *self->private->pfnDoubleClic)( number, self->private->user_data_double_clic );
		}
	}
}

/**
 * ofa_account_notebook_get_selected:
 *
 * Returns a new reference on the currently selected account, which
 * should be g_object_unref() by the caller after usage.
 */
ofoAccount *
ofa_account_notebook_get_selected( ofaAccountNotebook *self )
{
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoAccount *account;

	g_return_val_if_fail( self && OFA_IS_ACCOUNT_NOTEBOOK( self ), NULL );

	account = NULL;

	if( !self->private->dispose_has_run && self->private->view ){

		g_return_val_if_fail( GTK_IS_TREE_VIEW( self->private->view ), NULL );

		select = gtk_tree_view_get_selection( self->private->view );
		if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
			gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &account, -1 );
		}
	}

	return( account );
}

/**
 * ofa_account_notebook_insert:
 */
gboolean
ofa_account_notebook_insert( ofaAccountNotebook *self, ofoAccount *account )
{
	gint account_class;
	gint page_num, cmp;
	GtkWidget *page;
	GtkWidget *tview;
	GtkTreeModel *tmodel;
	gboolean iter_found;
	GtkTreeIter iter, new_iter;
	const gchar *account_num;
	gchar *iter_num;
	GtkTreePath *path;

	g_return_val_if_fail( self && OFA_IS_ACCOUNT_NOTEBOOK( self ), FALSE );
	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );

	if( !self->private->dispose_has_run ){

		/* activate the page of the correct class, or create a new one */
		page = find_page( self, ofo_account_get_number( account ));
		if( !page ){
			account_class = ofo_account_get_class( account );
			page = book_create_page(
					self, self->private->book, account_class, account_class-1 );
		}
		gtk_widget_show_all( page );
		page_num = gtk_notebook_page_num( self->private->book, page );
		gtk_notebook_set_current_page( self->private->book, page_num );

		/* insert the new row at the right place */
		tview = my_utils_container_get_child_by_type( GTK_CONTAINER( page ), GTK_TYPE_TREE_VIEW );
		g_return_val_if_fail( tview && GTK_IS_TREE_VIEW( tview ), FALSE );

		tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( tview ));
		iter_found = FALSE;
		if( gtk_tree_model_get_iter_first( tmodel, &iter )){
			account_num = ofo_account_get_number( account );
			while( !iter_found ){
				gtk_tree_model_get( tmodel, &iter, COL_NUMBER, &iter_num, -1 );
				cmp = g_utf8_collate( iter_num, account_num );
				g_free( iter_num );
				if( cmp > 0 ){
					iter_found = TRUE;
					break;
				}
				if( !gtk_tree_model_iter_next( tmodel, &iter )){
					break;
				}
			}
		}
		if( !iter_found ){
			gtk_list_store_append( GTK_LIST_STORE( tmodel ), &new_iter );
		} else {
			gtk_list_store_insert_before( GTK_LIST_STORE( tmodel ), &new_iter, &iter );
		}
		store_set_account( tmodel, &new_iter, account );

		/* select the newly added account */
		path = gtk_tree_model_get_path( tmodel, &new_iter );
		gtk_tree_view_set_cursor( GTK_TREE_VIEW( tview ), path, NULL, FALSE );
		gtk_widget_grab_focus( tview );
		gtk_tree_path_free( path );
	}

	return( TRUE );
}

/**
 * ofa_account_notebook_remove:
 */
gboolean
ofa_account_notebook_remove( ofaAccountNotebook *self, const gchar *number )
{
	static const gchar *thisfn = "ofa_account_notebook_remove";
	GtkWidget *page;
	GtkWidget *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *iter_num;
	gint cmp;
	gboolean iter_found;

	g_debug( "%s: self=%p, number=%s", thisfn, ( void * ) self, number );

	g_return_val_if_fail( self && OFA_IS_ACCOUNT_NOTEBOOK( self ), FALSE );
	g_return_val_if_fail( number && g_utf8_strlen( number, -1 ), FALSE );

	if( !self->private->dispose_has_run ){

		page = find_page( self, number );
		g_return_val_if_fail( page && GTK_IS_CONTAINER( page ), FALSE );

		tview = my_utils_container_get_child_by_type( GTK_CONTAINER( page ), GTK_TYPE_TREE_VIEW );
		g_return_val_if_fail( tview && GTK_IS_TREE_VIEW( tview ), FALSE );

		iter_found = FALSE;
		tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( tview ));

		if( gtk_tree_model_get_iter_first( tmodel, &iter )){
			while( !iter_found ){
				gtk_tree_model_get( tmodel, &iter, COL_NUMBER, &iter_num, -1 );
				cmp = g_utf8_collate( iter_num, number );
				g_free( iter_num );
				if( cmp == 0 ){
					iter_found = TRUE;
					break;
				}
				if( !gtk_tree_model_iter_next( tmodel, &iter )){
					break;
				}
			}
		}
		if( !iter_found ){
			g_debug( "%s: number %s not found", thisfn, number );
			return( FALSE );
		}

		gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
	}

	return( TRUE );
}

static GtkWidget *
find_page( ofaAccountNotebook *self, const gchar *number )
{
	gint account_class;
	gint page_class;
	gint count, i;
	GtkWidget *page, *page_found;

	account_class = ofo_account_get_class_from_number( number );
	count = gtk_notebook_get_n_pages( self->private->book );

	for( i=0, page_found=NULL ; i<count ; ++i ){
		page = gtk_notebook_get_nth_page( self->private->book, i );
		page_class = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( page ), DATA_PAGE_CLASS ));
		if( page_class == account_class ){
			page_found = page;
			break;
		}
	}

	return( page_found );
}
