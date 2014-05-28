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
#include "ui/ofo-dossier.h"

/* private class data
 */
struct _ofaAccountNotebookClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaAccountNotebookPrivate {
	gboolean     dispose_has_run;

	/* input data
	 */
	GtkNotebook         *book;			/* with one page per account class */
	ofoDossier          *dossier;
	ofaAccountNotebookCb pfnSelect;
	gpointer             user_data_select;
	ofaAccountNotebookCb pfnDoubleClic;
	gpointer             user_data_double_clic;

	/* runtime
	 */
	GtkTreeView *view;					/* the tree view of the current page */
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
#define DATA_PAGE_CLASS                 "ofa-data-page-class"
#define DATA_COLUMN_ID                  "ofa-data-column-id"

static GObjectClass *st_parent_class = NULL;

static GType      register_type( void );
static void       class_init( ofaAccountNotebookClass *klass );
static void       instance_init( GTypeInstance *instance, gpointer klass );
static void       instance_dispose( GObject *instance );
static void       instance_finalize( GObject *instance );
static void       setup_book( ofaAccountNotebook *self );
static GtkWidget *book_create_page( ofaAccountNotebook *self, GtkNotebook *book, gint class );
static gboolean   book_activate_page_by_class( ofaAccountNotebook *self, gint class_num );
static gint       book_get_page_by_class( ofaAccountNotebook *self, gint class_num );
static gint       on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaAccountNotebook *self );
static void       on_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaAccountNotebook *self );
static void       insert_new_row( ofaAccountNotebook *self, ofoAccount *account, gboolean with_selection );
static void       setup_first_selection( ofaAccountNotebook *self, const gchar *account_number );
static gboolean   on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, ofaAccountNotebook *self );
static void       on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaAccountNotebook *self );
static void       on_account_selected( GtkTreeSelection *selection, ofaAccountNotebook *self );
static void       on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaAccountNotebook *self );

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

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	chart = ofo_account_get_dataset( self->private->dossier );

	for( ic=chart, prev_class=0 ; ic ; ic=ic->next ){

		class = ofo_account_get_class( OFO_ACCOUNT( ic->data ));
		if( class != prev_class ){
			book_create_page( self, self->private->book, class );
			prev_class = class;
		}
	}

	g_signal_connect(
			G_OBJECT( self->private->book ), "switch-page", G_CALLBACK( on_page_switched ), self );

	g_signal_connect(
			G_OBJECT( self->private->book ), "key-press-event", G_CALLBACK( on_key_pressed_event ), self );
}

static GtkWidget *
book_create_page( ofaAccountNotebook *self, GtkNotebook *book, gint class )
{
	static const gchar *thisfn = "ofa_account_notebook_book_create_page";
	GtkScrolledWindow *scroll;
	GtkWidget *label;
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	gchar *str;

	g_debug( "%s: self=%p, book=%p, class=%d",
			thisfn, ( void * ) self, ( void * ) book, class );

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );

	label = gtk_label_new( st_class_labels[class-1] );
	str = g_strdup_printf( "Alt-%d", class );
	gtk_widget_set_tooltip_text( label, str );
	g_free( str );
	gtk_notebook_insert_page( book, GTK_WIDGET( scroll ), label, -1 );
	gtk_notebook_set_tab_reorderable( book, GTK_WIDGET( scroll ), TRUE );
	g_object_set_data( G_OBJECT( scroll ), DATA_PAGE_CLASS, GINT_TO_POINTER( class ));

	view = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_vexpand( GTK_WIDGET( view ), TRUE );
	gtk_tree_view_set_headers_visible( view, TRUE );
	g_signal_connect(
			G_OBJECT( view ), "row-activated", G_CALLBACK( on_row_activated), self );
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
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( COL_NUMBER ));
	gtk_tree_view_append_column( view, column );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", COL_LABEL,
			NULL );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( COL_LABEL ));
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( view, column );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new();
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( COL_DEBIT ));
	gtk_tree_view_column_pack_end( column, text_cell, TRUE );
	gtk_tree_view_column_set_title( column, _( "Debit" ));
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_add_attribute( column, text_cell, "text", COL_DEBIT );
	gtk_tree_view_column_set_min_width( column, 120 );
	gtk_tree_view_append_column( view, column );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	gtk_cell_renderer_set_padding( text_cell, 12, 0 );
	column = gtk_tree_view_column_new();
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( COL_CREDIT ));
	gtk_tree_view_column_pack_end( column, text_cell, TRUE );
	gtk_tree_view_column_set_title( column, _( "Credit" ));
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_add_attribute( column, text_cell, "text", COL_CREDIT );
	gtk_tree_view_column_set_min_width( column, 120 );
	gtk_tree_view_append_column( view, column );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	select = gtk_tree_view_get_selection( view );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect(
			G_OBJECT( select ), "changed", G_CALLBACK( on_account_selected ), self );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( model ), ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( model ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	gtk_widget_show_all( GTK_WIDGET( scroll ));

	return( GTK_WIDGET( scroll ));
}

static gboolean
book_activate_page_by_class( ofaAccountNotebook *self, gint class_num )
{
	gint page_num;

	page_num = book_get_page_by_class( self, class_num );
	if( page_num >= 0 ){
		gtk_notebook_set_current_page( self->private->book, page_num );
		return( TRUE );
	}

	return( FALSE );
}

static gint
book_get_page_by_class( ofaAccountNotebook *self, gint class_num )
{
	static const gchar *thisfn = "ofa_account_notebook_book_get_page_by_class";
	gint count, i;
	GtkWidget *page_widget;
	gint page_class;

	count = gtk_notebook_get_n_pages( self->private->book );

	for( i=0 ; i<count ; ++i ){
		page_widget = gtk_notebook_get_nth_page( self->private->book, i );
		page_class = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( page_widget ), DATA_PAGE_CLASS ));
		if( page_class == class_num ){
			return( i );
		}
	}

	g_warning( "%s: unable to find the book's page for class %d accounts", thisfn, class_num );

	return( -1 );
}


/*
 * sorting the treeview per account number
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaAccountNotebook *self )
{
	gchar *anumber, *bnumber;
	gint cmp;

	gtk_tree_model_get( tmodel, a, COL_NUMBER, &anumber, -1 );
	gtk_tree_model_get( tmodel, b, COL_NUMBER, &bnumber, -1 );

	cmp = g_utf8_collate( anumber, bnumber );

	g_free( anumber );
	g_free( bnumber );

	return( cmp );
}

/*
 * level 1: not displayed (should not appear)
 * level 2 and root: bold, colored background
 * level 3 and root: colored background
 * other root: italic
 */
static void
on_cell_data_func( GtkTreeViewColumn *tcolumn,
						GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
						ofaAccountNotebook *self )
{
	ofoAccount *account;
	GString *number;
	gint level;
	gint column;
	GdkRGBA color;

	g_return_if_fail( GTK_IS_CELL_RENDERER_TEXT( cell ));

	gtk_tree_model_get( tmodel, iter, COL_OBJECT, &account, -1 );
	g_object_unref( account );
	level = ofo_account_get_level_from_number( ofo_account_get_number( account ));
	g_return_if_fail( level >= 2 );

	column = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( tcolumn ), DATA_COLUMN_ID ));
	if( column == COL_NUMBER ){
		number = g_string_new( "" );
		g_string_printf( number, "%*c", level-2, ' ' );
		g_string_append_printf( number, "%s", ofo_account_get_number( account ));
		g_object_set( G_OBJECT( cell ), "text", number->str, NULL );
		g_string_free( number, TRUE );
	}

	g_object_set( G_OBJECT( cell ), "style", PANGO_STYLE_NORMAL, NULL );
	g_object_set( G_OBJECT( cell ), "weight", PANGO_WEIGHT_NORMAL, NULL );
	g_object_set( G_OBJECT( cell ), "foreground-set", FALSE, NULL );
	g_object_set( G_OBJECT( cell ), "background-set", FALSE, NULL );

	if( ofo_account_is_root( account )){

		if( level == 2 ){
			gdk_rgba_parse( &color, "#80ffff" );
			g_object_set( G_OBJECT( cell ), "background-rgba", &color, NULL );
			g_object_set( G_OBJECT( cell ), "weight", PANGO_WEIGHT_BOLD, NULL );

		} else if( level == 3 ){
			gdk_rgba_parse( &color, "#0000ff" );
			g_object_set( G_OBJECT( cell ), "foreground-rgba", &color, NULL );

		} else {
			g_object_set( G_OBJECT( cell ), "style", PANGO_STYLE_ITALIC, NULL );
		}
	}
}

/**
 * ofa_account_notebook_init_view:
 *
 * Populates the view, setting the first selection on account 'number'
 * if specified, or on the first visible account (if any) of the first
 * book's page.
 */
void
ofa_account_notebook_init_view( ofaAccountNotebook *self, const gchar *number )
{
	GList *dataset, *iset;

	g_return_if_fail( self && OFA_IS_ACCOUNT_NOTEBOOK( self ));

	if( !self->private->dispose_has_run ){

		dataset = ofo_account_get_dataset( self->private->dossier );
		for( iset=dataset ; iset ; iset=iset->next ){
			insert_new_row( self, OFO_ACCOUNT( iset->data ), FALSE );
		}
	}

	setup_first_selection( self, number );
}

/**
 * ofa_account_notebook_init_view:
 *
 * Populates the view, setting the first selection on account 'number'
 * if specified, or on the first visible account (if any) of the first
 * book's page.
 */
static void
insert_new_row( ofaAccountNotebook *self, ofoAccount *account, gboolean with_selection )
{
	gint page_num;
	GtkWidget *page_widget;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *sdeb, *scre;
	GtkTreePath *path;

	page_num = book_get_page_by_class( self, ofo_account_get_class( account ));
	if( page_num >= 0 ){
		page_widget = gtk_notebook_get_nth_page( self->private->book, page_num );

		tview = ( GtkTreeView * )
						my_utils_container_get_child_by_type(
								GTK_CONTAINER( page_widget ), GTK_TYPE_TREE_VIEW );
		g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));

		tmodel = gtk_tree_view_get_model( tview );

		if( ofo_account_is_root( account )){
			sdeb = g_strdup( "" );
			scre = g_strdup( "" );
		} else {
			sdeb = g_strdup_printf( "%.2f €",
					ofo_account_get_deb_mnt( account )+ofo_account_get_bro_deb_mnt( account ));
			scre = g_strdup_printf( "%.2f €",
					ofo_account_get_cre_mnt( account )+ofo_account_get_bro_cre_mnt( account ));
		}

		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				COL_NUMBER, ofo_account_get_number( account ),
				COL_LABEL,  ofo_account_get_label( account ),
				COL_DEBIT,  sdeb,
				COL_CREDIT, scre,
				COL_OBJECT, account,
				-1 );

		/* select the newly added account */
		if( with_selection ){
			path = gtk_tree_model_get_path( tmodel, &iter );
			gtk_tree_view_set_cursor( GTK_TREE_VIEW( tview ), path, NULL, FALSE );
			gtk_tree_path_free( path );
			gtk_widget_grab_focus( GTK_WIDGET( tview ));
		}
	}
}

static void
setup_first_selection( ofaAccountNotebook *self, const gchar *asked_number )
{
	gint page_num;
	GtkWidget *page_widget;
	GtkWidget *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeSelection *select;
	gchar *model_number;
	gint cmp;

	page_num = 0;
	if( asked_number && g_utf8_strlen( asked_number, -1 )){
		page_num = book_get_page_by_class( self, ofo_account_get_class_from_number( asked_number ));
		if( page_num <= 0 ){
			page_num = 0;
		}
	}
	gtk_notebook_set_current_page( self->private->book, page_num );

	page_widget = gtk_notebook_get_nth_page( self->private->book, page_num );
	tview = my_utils_container_get_child_by_type(
							GTK_CONTAINER( page_widget ), GTK_TYPE_TREE_VIEW );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));

	tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( tview ));

	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE && asked_number && g_utf8_strlen( asked_number, -1 )){
			gtk_tree_model_get( tmodel, &iter, COL_NUMBER, &model_number, -1 );
			cmp = g_utf8_collate( model_number, asked_number );
			g_free( model_number );
			if( cmp >= 0 ){
				break;
			}
			if( !gtk_tree_model_iter_next( tmodel, &iter )){
				gtk_tree_model_get_iter_first( tmodel, &iter );
				break;
			}
		}
	}

	if( gtk_list_store_iter_is_valid( GTK_LIST_STORE( tmodel ), &iter )){
		path = gtk_tree_model_get_path( tmodel, &iter );
		gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW( tview ), path, NULL, FALSE, 0, 0 );
		gtk_tree_path_free( path );
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
		gtk_tree_selection_select_iter( select, &iter );
	}

	gtk_widget_grab_focus( tview );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, ofaAccountNotebook *self )
{
	gboolean stop;
	gint class_num;

	stop = FALSE;
	class_num = -1;

	if( event->state == GDK_MOD1_MASK ||
		event->state == ( GDK_MOD1_MASK | GDK_SHIFT_MASK )){
		switch( event->keyval ){
			case GDK_KEY_1:
			case GDK_KEY_ampersand:
				class_num = 1;
				break;
			case GDK_KEY_2:
			case GDK_KEY_eacute:
				class_num = 2;
				break;
			case GDK_KEY_3:
			case GDK_KEY_quotedbl:
				class_num = 3;
				break;
			case GDK_KEY_4:
			case GDK_KEY_apostrophe:
				class_num = 4;
				break;
			case GDK_KEY_5:
			case GDK_KEY_parenleft:
				class_num = 5;
				break;
			case GDK_KEY_6:
			case GDK_KEY_minus:
				class_num = 6;
				break;
			case GDK_KEY_7:
			case GDK_KEY_egrave:
				class_num = 7;
				break;
			case GDK_KEY_8:
			case GDK_KEY_underscore:
				class_num = 8;
				break;
			case GDK_KEY_9:
			case GDK_KEY_ccedilla:
				class_num = 9;
				break;
		}
	}

	if( class_num > 0 ){
		stop = book_activate_page_by_class( self, class_num );
	}

	return( stop );
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

	if( self->private->pfnSelect &&
			gtk_tree_selection_get_selected( selection, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &account, -1 );
		g_object_unref( account );

		( *self->private->pfnSelect )( account, self->private->user_data_select );
	}
}

static void
on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaAccountNotebook *self )
{
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoAccount *account;

	if( self->private->pfnDoubleClic ){

		select = gtk_tree_view_get_selection( tview );
		if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

			gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &account, -1 );
			g_object_unref( account );

			( *self->private->pfnDoubleClic)( account, self->private->user_data_double_clic );
		}
	}
}

/**
 * ofa_account_notebook_get_selected:
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
			g_object_unref( account );
		}
	}

	return( account );
}

/**
 * ofa_account_notebook_set_selected:
 *
 * Let the user reset the selection after the end of setup and
 * initialization phases
 */
void
ofa_account_notebook_set_selected( ofaAccountNotebook *self, const gchar *number )
{
	setup_first_selection( self, number );
}

/**
 * ofa_account_notebook_grab_focus:
 *
 * Reset the focus on the treeview.
 */
void
ofa_account_notebook_grab_focus( ofaAccountNotebook *self )
{
	g_return_if_fail( self && OFA_IS_ACCOUNT_NOTEBOOK( self ));

	if( !self->private->dispose_has_run && self->private->view ){

		g_return_if_fail( GTK_IS_TREE_VIEW( self->private->view ));

		gtk_widget_grab_focus( GTK_WIDGET( self->private->view ));
	}
}

/**
 * ofa_account_notebook_insert:
 */
gboolean
ofa_account_notebook_insert( ofaAccountNotebook *self, ofoAccount *account )
{
	g_return_val_if_fail( self && OFA_IS_ACCOUNT_NOTEBOOK( self ), FALSE );
	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );

	if( !self->private->dispose_has_run ){

		insert_new_row( self, account, TRUE );
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
	gint page_num;
	GtkWidget *page_widget;
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

		page_num = book_get_page_by_class( self, ofo_account_get_class_from_number( number ));
		g_return_val_if_fail( page_num >= 0, FALSE );

		page_widget = gtk_notebook_get_nth_page( self->private->book, page_num );
		tview = my_utils_container_get_child_by_type( GTK_CONTAINER( page_widget ), GTK_TYPE_TREE_VIEW );
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
			g_warning( "%s: number %s not found", thisfn, number );
			return( FALSE );
		}

		gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
	}

	return( TRUE );
}
