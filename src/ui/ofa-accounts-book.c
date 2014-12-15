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

#include "api/my-utils.h"
#include "api/ofo-account.h"
#include "api/ofo-class.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"

#include "core/ofa-preferences.h"

#include "ui/ofa-account-properties.h"
#include "ui/ofa-accounts-book.h"
#include "ui/ofa-buttons-box.h"
#include "ui/ofa-page.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-view-entries.h"

/* private instance data
 */
struct _ofaAccountsBookPrivate {
	gboolean       dispose_has_run;

	ofaMainWindow *main_window;
	ofoDossier    *dossier;
	GList         *handlers;

	GtkNotebook   *book;
	gint           number_col;
};

/* these are only default labels in the case where we were not able to
 * get the correct #ofoClass objects
 */
static const gchar  *st_class_labels[] = {
		N_( "Class I" ),
		N_( "Class II" ),
		N_( "Class III" ),
		N_( "Class IV" ),
		N_( "Class V" ),
		N_( "Class VI" ),
		N_( "Class VII" ),
		N_( "Class VIII" ),
		N_( "Class IX" ),
		NULL
};

/* data attached to each page of the classes notebook
 */
#define DATA_PAGE_CLASS         "ofa-data-page-class"
#define DATA_COLUMN_ID          "ofa-data-column-id"

static void       istore_iface_init( ofaAccountIStoreInterface *iface );
static guint      istore_get_interface_version( const ofaAccountIStore *instance );
static void       istore_attach_to( ofaAccountIStore *instance, GtkContainer *parent );
static void       istore_set_columns( ofaAccountIStore *instance, GtkTreeStore *store, ofaAccountColumns columns, gint class );
static void       dossier_signals_connect( ofaAccountsBook *self );
static void       on_new_object( ofoDossier *dossier, ofoBase *object, ofaAccountsBook *self );
static void       on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaAccountsBook *self );
static void       on_updated_class_label( ofaAccountsBook *self, ofoClass *class );
static void       on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaAccountsBook *self );
static void       on_deleted_class_label( ofaAccountsBook *self, ofoClass *class );
static void       on_reloaded_dataset( ofoDossier *dossier, GType type, ofaAccountsBook *self );
static GtkWidget *book_get_page_by_class( ofaAccountsBook *self, gint class_num, gboolean create );
static GtkWidget *book_create_page( ofaAccountsBook *self, gint class );
static void       on_book_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaAccountsBook *self );
static gboolean   on_book_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaAccountsBook *self );
static void       on_tview_row_selected( GtkTreeSelection *selection, ofaAccountsBook *self );
static void       on_tview_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaAccountsBook *self );
static gboolean   on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaAccountsBook *self );
static void       tview_collapse_node( ofaAccountsBook *self, GtkWidget *widget );
static void       tview_expand_node( ofaAccountsBook *self, GtkWidget *widget );
static void       on_tview_insert( ofaAccountsBook *self );
static void       on_tview_delete( ofaAccountsBook *self );
static void       on_tview_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaAccountsBook *self );
static void       do_insert_account( ofaAccountsBook *self );
static void       do_update_account( ofaAccountsBook *self );
static void       do_delete_account( ofaAccountsBook *self );
static gboolean   delete_confirmed( ofaAccountsBook *self, ofoAccount *account );
static GtkWidget *get_accounts_book( ofaAccountsBook *self );
static GtkWidget *get_current_tree_view( const ofaAccountsBook *self );
static void       select_row_by_number( ofaAccountsBook *self, const gchar *number );
static void       select_row_by_iter( ofaAccountsBook *self, GtkTreeView *tview, GtkTreeModel *tmodel, GtkTreeIter *iter );

G_DEFINE_TYPE_EXTENDED( ofaAccountsBook, ofa_accounts_book, G_TYPE_OBJECT, 0, \
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ACCOUNT_ISTORE, istore_iface_init ))

static void
accounts_book_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_accounts_book_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNTS_BOOK( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_accounts_book_parent_class )->finalize( instance );
}

static void
accounts_book_dispose( GObject *instance )
{
	ofaAccountsBookPrivate *priv;
	GList *it;

	g_return_if_fail( instance && OFA_IS_ACCOUNTS_BOOK( instance ));

	priv = ( OFA_ACCOUNTS_BOOK( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		if( priv->dossier && OFO_IS_DOSSIER( priv->dossier )){
			for( it=priv->handlers ; it ; it=it->next ){
				g_signal_handler_disconnect( priv->dossier, ( gulong ) it->data );
			}
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_accounts_book_parent_class )->dispose( instance );
}

static void
ofa_accounts_book_init( ofaAccountsBook *self )
{
	static const gchar *thisfn = "ofa_accounts_book_init";

	g_return_if_fail( self && OFA_IS_ACCOUNTS_BOOK( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_ACCOUNTS_BOOK, ofaAccountsBookPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
ofa_accounts_book_class_init( ofaAccountsBookClass *klass )
{
	static const gchar *thisfn = "ofa_accounts_book_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = accounts_book_dispose;
	G_OBJECT_CLASS( klass )->finalize = accounts_book_finalize;

	g_type_class_add_private( klass, sizeof( ofaAccountsBookPrivate ));
}

static void
istore_iface_init( ofaAccountIStoreInterface *iface )
{
	static const gchar *thisfn = "ofa_account_combo_istore_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = istore_get_interface_version;
	iface->attach_to = istore_attach_to;
	iface->set_columns = istore_set_columns;
}

static guint
istore_get_interface_version( const ofaAccountIStore *instance )
{
	return( 1 );
}

static void
istore_attach_to( ofaAccountIStore *instance, GtkContainer *parent )
{
	GtkWidget *book;

	g_return_if_fail( instance && OFA_IS_ACCOUNTS_BOOK( instance ));

	g_debug( "ofa_accounts_book_istore_attach_to: instance=%p (%s), parent=%p (%s)",
			( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			( void * ) parent, G_OBJECT_TYPE_NAME( parent ));

	book = get_accounts_book( OFA_ACCOUNTS_BOOK( instance ));

	gtk_container_add( parent, book );
}

static void
istore_set_columns( ofaAccountIStore *instance, GtkTreeStore *store, ofaAccountColumns columns, gint class )
{
	ofaAccountsBookPrivate *priv;
	GtkTreeView *tview;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	gint col_number;
	GtkWidget *page_widget;

	priv = OFA_ACCOUNTS_BOOK( instance )->priv;

	get_accounts_book( OFA_ACCOUNTS_BOOK( instance ));
	page_widget = book_get_page_by_class( OFA_ACCOUNTS_BOOK( instance ), class, TRUE );
	tview = ( GtkTreeView * )
					my_utils_container_get_child_by_type(
							GTK_CONTAINER( page_widget ), GTK_TYPE_TREE_VIEW );
	gtk_tree_view_set_model( tview, GTK_TREE_MODEL( store ));

	priv->number_col = ofa_account_istore_get_column_number( instance, ACCOUNT_COL_NUMBER );

	if( columns & ACCOUNT_COL_NUMBER ){
		col_number = ofa_account_istore_get_column_number( instance, ACCOUNT_COL_NUMBER );
		cell = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
					_( "Number" ),
					cell, "text", col_number,
					NULL );
		g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( col_number ));
		gtk_tree_view_append_column( tview, column );
		gtk_tree_view_column_set_cell_data_func(
				column, cell, ( GtkTreeCellDataFunc ) on_tview_cell_data_func, instance, NULL );
	}

	if( columns & ACCOUNT_COL_LABEL ){
		col_number = ofa_account_istore_get_column_number( instance, ACCOUNT_COL_LABEL );
		cell = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
				_( "Label" ),
				cell, "text", col_number,
				NULL );
		g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( col_number ));
		gtk_tree_view_column_set_expand( column, TRUE );
		gtk_tree_view_append_column( tview, column );
		gtk_tree_view_column_set_cell_data_func(
				column, cell, ( GtkTreeCellDataFunc ) on_tview_cell_data_func, instance, NULL );
	}

	if( columns & ACCOUNT_COL_SETTLEABLE ){
		col_number = ofa_account_istore_get_column_number( instance, ACCOUNT_COL_SETTLEABLE );
		cell = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
					_( "S" ),
					cell, "text", col_number,
					NULL );
		g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( col_number ));
		gtk_tree_view_append_column( tview, column );
		gtk_tree_view_column_set_cell_data_func(
				column, cell, ( GtkTreeCellDataFunc ) on_tview_cell_data_func, instance, NULL );
	}

	if( columns & ACCOUNT_COL_RECONCILIABLE ){
		col_number = ofa_account_istore_get_column_number( instance, ACCOUNT_COL_RECONCILIABLE );
		cell = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
					_( "R" ),
					cell, "text", col_number,
					NULL );
		g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( col_number ));
		gtk_tree_view_append_column( tview, column );
		gtk_tree_view_column_set_cell_data_func(
				column, cell, ( GtkTreeCellDataFunc ) on_tview_cell_data_func, instance, NULL );
	}

	if( columns & ACCOUNT_COL_FORWARD ){
		col_number = ofa_account_istore_get_column_number( instance, ACCOUNT_COL_FORWARD );
		cell = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
					_( "F" ),
					cell, "text", col_number,
					NULL );
		g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( col_number ));
		gtk_tree_view_append_column( tview, column );
		gtk_tree_view_column_set_cell_data_func(
				column, cell, ( GtkTreeCellDataFunc ) on_tview_cell_data_func, instance, NULL );
	}

	if( columns & ACCOUNT_COL_EXE_DEBIT ){
		col_number = ofa_account_istore_get_column_number( instance, ACCOUNT_COL_EXE_DEBIT );
		cell = gtk_cell_renderer_text_new();
		gtk_cell_renderer_set_alignment( cell, 1.0, 0.5 );
		column = gtk_tree_view_column_new();
		g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( col_number ));
		gtk_tree_view_column_pack_end( column, cell, TRUE );
		gtk_tree_view_column_set_title( column, _( "Debit" ));
		gtk_tree_view_column_set_alignment( column, 1.0 );
		gtk_tree_view_column_add_attribute( column, cell, "text", col_number );
		gtk_tree_view_column_set_min_width( column, 100 );
		gtk_tree_view_append_column( tview, column );
		gtk_tree_view_column_set_cell_data_func(
				column, cell, ( GtkTreeCellDataFunc ) on_tview_cell_data_func, instance, NULL );
	}

	if( columns & ACCOUNT_COL_EXE_CREDIT ){
		col_number = ofa_account_istore_get_column_number( instance, ACCOUNT_COL_EXE_DEBIT );
		cell = gtk_cell_renderer_text_new();
		gtk_cell_renderer_set_alignment( cell, 1.0, 0.5 );
		column = gtk_tree_view_column_new();
		g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( col_number ));
		gtk_tree_view_column_pack_end( column, cell, TRUE );
		gtk_tree_view_column_set_title( column, _( "Credit" ));
		gtk_tree_view_column_set_alignment( column, 1.0 );
		gtk_tree_view_column_add_attribute( column, cell, "text", col_number );
		gtk_tree_view_column_set_min_width( column, 100 );
		gtk_tree_view_append_column( tview, column );
		gtk_tree_view_column_set_cell_data_func(
				column, cell, ( GtkTreeCellDataFunc ) on_tview_cell_data_func, instance, NULL );
	}

	if( columns & ACCOUNT_COL_CURRENCY ){
		col_number = ofa_account_istore_get_column_number( instance, ACCOUNT_COL_CURRENCY );
		cell = gtk_cell_renderer_text_new();
		gtk_cell_renderer_set_alignment( cell, 0.0, 0.5 );
		column = gtk_tree_view_column_new();
		g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( col_number ));
		gtk_tree_view_column_pack_end( column, cell, FALSE );
		gtk_tree_view_column_set_alignment( column, 0.0 );
		gtk_tree_view_column_add_attribute( column, cell, "text", col_number );
		gtk_tree_view_column_set_min_width( column, 40 );
		gtk_tree_view_append_column( tview, column );
		gtk_tree_view_column_set_cell_data_func(
				column, cell, ( GtkTreeCellDataFunc ) on_tview_cell_data_func, instance, NULL );
	}

	gtk_widget_show_all( page_widget );
}

/**
 * ofa_accounts_book_new:
 *
 * Creates the structured content, i.e. one notebook with one page per
 * account class.
 *
 * Does NOT insert the data (see: ofa_accounts_book_init_view).
 *
 * +-----------------------------------------------------------------------+
 * | parent container:                                                     |
 * |   this is the grid of the main page,                                  |
 * |   or any another container (i.e. a frame)                             |
 * | +-------------------------------------------------------------------+ |
 * | | creates a grid which will contain the book and the buttons        | |
 * | | +---------------------------------------------+-----------------+ + |
 * | | | creates a notebook where each page contains | creates         | | |
 * | | |   the account of the corresponding class    |   a buttons box | | |
 * | | |                                             |                 | | |
 * | | +---------------------------------------------+-----------------+ | |
 * | +-------------------------------------------------------------------+ |
 * +-----------------------------------------------------------------------+
 */
ofaAccountsBook *
ofa_accounts_book_new( void  )
{
	ofaAccountsBook *self;

	self = g_object_new( OFA_TYPE_ACCOUNTS_BOOK, NULL );

	return( self );
}

/**
 * ofa_accounts_book_set_main_window:
 *
 * Returns the top focusable widget, here the treeview of the current
 * page.
 */
void
ofa_accounts_book_set_main_window( ofaAccountsBook *book, ofaMainWindow *main_window )
{
	ofaAccountsBookPrivate *priv;

	g_return_if_fail( book && OFA_IS_ACCOUNTS_BOOK( book ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv = book->priv;

	if( !priv->dispose_has_run ){

		priv->main_window = main_window;
		priv->dossier = ofa_main_window_get_dossier( main_window );

		ofa_account_istore_set_dossier( OFA_ACCOUNT_ISTORE( book ), priv->dossier );

		dossier_signals_connect( book );
	}
}

static void
dossier_signals_connect( ofaAccountsBook *self )
{
	ofaAccountsBookPrivate *priv;
	gulong handler;

	priv = self->priv;

	handler = g_signal_connect(
						G_OBJECT( priv->dossier),
						SIGNAL_DOSSIER_NEW_OBJECT, G_CALLBACK( on_new_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( priv->dossier),
						SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( priv->dossier),
						SIGNAL_DOSSIER_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( priv->dossier),
						SIGNAL_DOSSIER_RELOAD_DATASET, G_CALLBACK( on_reloaded_dataset ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );
}

/*
 * SIGNAL_DOSSIER_NEW_OBJECT signal handler
 */
static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaAccountsBook *self )
{
	static const gchar *thisfn = "ofa_accounts_book_on_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn, ( void * ) dossier,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), ( void * ) self );

	if( OFO_IS_CLASS( object )){
		on_updated_class_label( self, OFO_CLASS( object ));
	}
}

/*
 * OFA_SIGNAL_UPDATE_OBJECT signal handler
 */
static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaAccountsBook *self )
{
	static const gchar *thisfn = "ofa_accounts_book_on_updated_object";

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn, ( void * ) dossier,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), prev_id, ( void * ) self );

	if( OFO_IS_CLASS( object )){
		on_updated_class_label( self, OFO_CLASS( object ));
	}
}

/*
 * a class label has changed : update the corresponding tab label
 */
static void
on_updated_class_label( ofaAccountsBook *self, ofoClass *class )
{
	ofaAccountsBookPrivate *priv;
	GtkWidget *page_w;
	gint class_num;

	priv = self->priv;

	class_num = ofo_class_get_number( class );
	page_w = book_get_page_by_class( self, class_num, FALSE );
	if( page_w ){
		g_return_if_fail( GTK_IS_WIDGET( page_w ));
		gtk_notebook_set_tab_label_text( priv->book, page_w, ofo_class_get_label( class ));
	}
}

/*
 * SIGNAL_DOSSIER_DELETED_OBJECT signal handler
 */
static void
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaAccountsBook *self )
{
	static const gchar *thisfn = "ofa_accounts_book_on_deleted_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn, ( void * ) dossier,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), ( void * ) self );

	if( OFO_IS_CLASS( object )){
		on_deleted_class_label( self, OFO_CLASS( object ));
	}
}

static void
on_deleted_class_label( ofaAccountsBook *self, ofoClass *class )
{
	ofaAccountsBookPrivate *priv;
	GtkWidget *page_w;
	gint class_num;

	priv = self->priv;

	class_num = ofo_class_get_number( class );
	page_w = book_get_page_by_class( self, class_num, FALSE );
	if( page_w ){
		g_return_if_fail( GTK_IS_WIDGET( page_w ));
		gtk_notebook_set_tab_label_text( priv->book, page_w, st_class_labels[class_num-1] );
	}
}

/*
 * SIGNAL_DOSSIER_RELOAD_DATASET signal handler
 */
static void
on_reloaded_dataset( ofoDossier *dossier, GType type, ofaAccountsBook *self )
{
	static const gchar *thisfn = "ofa_accounts_book_on_reloaded_dataset";

	g_debug( "%s: dossier=%p, type=%lu, self=%p",
			thisfn, ( void * ) dossier, type, ( void * ) self );
}

/*
 * Returns the notebook's page container which is dedicated to the
 * given class number.
 *
 * If the page doesn't exist, and @bcreate is %TRUE, then it is created.
 */
static GtkWidget *
book_get_page_by_class( ofaAccountsBook *self, gint class_num, gboolean bcreate )
{
	static const gchar *thisfn = "ofa_accounts_book_get_page_by_class";
	ofaAccountsBookPrivate *priv;
	gint count, i;
	GtkWidget *found, *page_widget;
	gint page_class;

	priv = self->priv;

	if( !ofo_class_is_valid_number( class_num )){
		g_warning( "%s: invalid class number: %d", thisfn, class_num );
		return( NULL );
	}

	found = NULL;
	count = gtk_notebook_get_n_pages( priv->book );

	/* search for an existing page */
	for( i=0 ; i<count ; ++i ){
		page_widget = gtk_notebook_get_nth_page( priv->book, i );
		page_class = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( page_widget ), DATA_PAGE_CLASS ));
		if( page_class == class_num ){
			found = page_widget;
			break;
		}
	}

	/* if not exists, create it (if allowed) */
	if( !found && bcreate ){
		found = book_create_page( self, class_num );
		if( !found ){
			g_warning( "%s: unable to create the page for class %d", thisfn, class_num );
			return( NULL );
		}
	}

	return( found );
}

/*
 * creates the page widget for the given class number
 */
static GtkWidget *
book_create_page( ofaAccountsBook *self, gint class )
{
	static const gchar *thisfn = "ofa_accounts_book_create_page";
	ofaAccountsBookPrivate *priv;
	GtkWidget *scrolled, *label, *tview;
	ofoClass *class_obj;
	const gchar *class_label;
	gchar *str;
	GtkTreeSelection *select;

	g_debug( "%s: self=%p, class=%d", thisfn, ( void * ) self, class );

	priv = self->priv;

	scrolled = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW( scrolled ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );

	class_obj = ofo_class_get_by_number( priv->dossier, class );
	if( class_obj && OFO_IS_CLASS( class_obj )){
		class_label = ofo_class_get_label( class_obj );
	} else {
		class_label = st_class_labels[class-1];
	}
	label = gtk_label_new( class_label );
	str = g_strdup_printf( "Alt-%d", class );
	gtk_widget_set_tooltip_text( label, str );
	g_free( str );
	if( gtk_notebook_append_page( priv->book, scrolled, label ) == -1 ){
		return( NULL );
	}
	gtk_notebook_set_tab_reorderable( priv->book, scrolled, TRUE );
	g_object_set_data( G_OBJECT( scrolled ), DATA_PAGE_CLASS, GINT_TO_POINTER( class ));

	tview = gtk_tree_view_new();
	gtk_widget_set_hexpand( tview, TRUE );
	gtk_widget_set_vexpand( tview, TRUE );
	gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( tview ), TRUE );

	g_signal_connect(
			G_OBJECT( tview ), "row-activated", G_CALLBACK( on_tview_row_activated), self );
	g_signal_connect(
			G_OBJECT( tview ), "key-press-event", G_CALLBACK( on_tview_key_pressed ), self );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect(
			G_OBJECT( select ), "changed", G_CALLBACK( on_tview_row_selected ), self );

	gtk_container_add( GTK_CONTAINER( scrolled ), tview );

	return( scrolled );
}

/*
 * we have switch to this given page (wpage, npage)
 * just setup the selection
 */
static void
on_book_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaAccountsBook *self )
{
	GtkWidget *tview;
	GtkTreeSelection *select;

	tview = my_utils_container_get_child_by_type( GTK_CONTAINER( wpage ), GTK_TYPE_TREE_VIEW );
	if( tview ){
		g_return_if_fail( GTK_IS_TREE_VIEW( tview ));
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
		on_tview_row_selected( select, self );
	}
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_book_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaAccountsBook *self )
{
	ofaAccountsBookPrivate *priv;
	gboolean stop;
	GtkWidget *page_widget;
	gint class_num, page_num;

	stop = FALSE;
	priv = self->priv;

	if( event->state == GDK_MOD1_MASK ||
		event->state == ( GDK_MOD1_MASK | GDK_SHIFT_MASK )){

		class_num = -1;

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

		if( class_num > 0 ){
			page_widget = book_get_page_by_class( self, class_num, FALSE );
			if( page_widget ){
				page_num = gtk_notebook_page_num( priv->book, page_widget );
				gtk_notebook_set_current_page( priv->book, page_num );
				stop = TRUE;
			}
		}
	}

	return( stop );
}

static void
on_tview_row_selected( GtkTreeSelection *selection, ofaAccountsBook *self )
{
	ofaAccountsBookPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *account;

	priv = self->priv;
	account = NULL;

	/* selection may be null when called from on_delete_clicked() */
	if( selection ){
		if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
			gtk_tree_model_get( tmodel, &iter, priv->number_col, &account, -1 );
			g_signal_emit_by_name( self, "changed", account );
			g_free( account );
		}
	}

	/*update_buttons_sensitivity( self, account );*/
}

static void
on_tview_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaAccountsBook *self )
{
	ofaAccountsBookPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *account;

	priv = self->priv;
	account = NULL;

	select = gtk_tree_view_get_selection( tview );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, priv->number_col, &account, -1 );
		g_signal_emit_by_name( self, "activated", account );
		g_free( account );
	}
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaAccountsBook *self )
{
	gboolean stop;

	stop = FALSE;

	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Left ){
			tview_collapse_node( self, widget );

		} else if( event->keyval == GDK_KEY_Right ){
			tview_expand_node( self, widget );

		} else if( event->keyval == GDK_KEY_Insert ){
			on_tview_insert( self );

		} else if( event->keyval == GDK_KEY_Delete ){
			on_tview_delete( self );
		}
	}

	return( stop );
}

static void
tview_collapse_node( ofaAccountsBook *self, GtkWidget *widget )
{
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter, parent;
	GtkTreePath *path;

	if( GTK_IS_TREE_VIEW( widget )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ));
		if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

			if( gtk_tree_model_iter_has_child( tmodel, &iter )){
				path = gtk_tree_model_get_path( tmodel, &iter );
				gtk_tree_view_collapse_row( GTK_TREE_VIEW( widget ), path );
				gtk_tree_path_free( path );

			} else if( gtk_tree_model_iter_parent( tmodel, &parent, &iter )){
				path = gtk_tree_model_get_path( tmodel, &parent );
				gtk_tree_view_collapse_row( GTK_TREE_VIEW( widget ), path );
				gtk_tree_path_free( path );
			}
		}
	}
}

static void
tview_expand_node( ofaAccountsBook *self, GtkWidget *widget )
{
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;

	if( GTK_IS_TREE_VIEW( widget )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ));
		if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

			if( gtk_tree_model_iter_has_child( tmodel, &iter )){
				path = gtk_tree_model_get_path( tmodel, &iter );
				gtk_tree_view_expand_row( GTK_TREE_VIEW( widget ), path, FALSE );
				gtk_tree_path_free( path );
			}
		}
	}
}

static void
on_tview_insert( ofaAccountsBook *self )
{
	do_insert_account( self );
}

static void
on_tview_delete( ofaAccountsBook *self )
{
	ofaAccountsBookPrivate *priv;
	gchar *account_number;
	ofoAccount *account_obj;

	priv = self->priv;

	account_number = ofa_accounts_book_get_selected( self );
	account_obj = ofo_account_get_by_number( priv->dossier, account_number );
	g_free( account_number );

	if( ofo_account_is_deletable( account_obj, priv->dossier )){
		do_delete_account( self );
	}
}

/*
 * level 1: not displayed (should not appear)
 * level 2 and root: bold, colored background
 * level 3 and root: colored background
 * other root: italic
 *
 * detail accounts who have no currency are red written.
 */
static void
on_tview_cell_data_func( GtkTreeViewColumn *tcolumn,
						GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
						ofaAccountsBook *self )
{
	ofaAccountsBookPrivate *priv;
	gchar *account_num;
	ofoAccount *account_obj;
	GString *number;
	gint level;
	gint column;
	GdkRGBA color;
	ofoCurrency *currency;

	g_return_if_fail( GTK_IS_CELL_RENDERER_TEXT( cell ));

	priv = self->priv;

	gtk_tree_model_get( tmodel, iter, priv->number_col, &account_num, -1 );
	if( account_num ){
		account_obj = ofo_account_get_by_number( priv->dossier, account_num );
		g_return_if_fail( account_obj && OFO_IS_ACCOUNT( account_obj ));

		level = ofo_account_get_level_from_number( ofo_account_get_number( account_obj ));
		g_return_if_fail( level >= 2 );

		column = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( tcolumn ), DATA_COLUMN_ID ));
		if( column == priv->number_col ){
			number = g_string_new( " " );
			g_string_append_printf( number, "%s", account_num );
			g_object_set( G_OBJECT( cell ), "text", number->str, NULL );
			g_string_free( number, TRUE );
		}

		g_object_set( G_OBJECT( cell ),
							"style-set",      FALSE,
							"weight-set",     FALSE,
							"foreground-set", FALSE,
							"background-set", FALSE,
							NULL );

		if( ofo_account_is_root( account_obj )){

			if( level == 2 ){
				gdk_rgba_parse( &color, "#c0ffff" );
				g_object_set( G_OBJECT( cell ), "background-rgba", &color, NULL );
				g_object_set( G_OBJECT( cell ), "weight", PANGO_WEIGHT_BOLD, NULL );

			} else if( level == 3 ){
				gdk_rgba_parse( &color, "#0000ff" );
				g_object_set( G_OBJECT( cell ), "foreground-rgba", &color, NULL );
				g_object_set( G_OBJECT( cell ), "weight", PANGO_WEIGHT_BOLD, NULL );

			} else {
				gdk_rgba_parse( &color, "#0000ff" );
				g_object_set( G_OBJECT( cell ), "foreground-rgba", &color, NULL );
				g_object_set( G_OBJECT( cell ), "style", PANGO_STYLE_ITALIC, NULL );
			}

		} else {
			currency = ofo_currency_get_by_code( priv->dossier, ofo_account_get_currency( account_obj ));
			if( !currency ){
				gdk_rgba_parse( &color, "#800000" );
				g_object_set( G_OBJECT( cell ), "foreground-rgba", &color, NULL );
			}
		}
	}
}

static void
do_insert_account( ofaAccountsBook *self )
{
	ofaAccountsBookPrivate *priv;
	ofoAccount *account;

	priv = self->priv;
	account = ofo_account_new();

	if( !ofa_account_properties_run( priv->main_window, account )){
		g_object_unref( account );
	}
}

static void
do_update_account( ofaAccountsBook *self )
{
	ofaAccountsBookPrivate *priv;
	ofoAccount *account;
	gchar *number;
	GtkWidget *tview;

	priv = self->priv;

	number = ofa_accounts_book_get_selected( self );
	if( number ){
		account = ofo_account_get_by_number( priv->dossier, number );
		g_return_if_fail( account && OFO_IS_ACCOUNT( account ));

		ofa_account_properties_run( priv->main_window, account );
	}
	g_free( number );

	tview = ofa_accounts_book_get_top_focusable_widget( self );
	if( tview ){
		gtk_widget_grab_focus( tview );
	}
}

static void
do_delete_account( ofaAccountsBook *self )
{
	ofaAccountsBookPrivate *priv;
	ofoAccount *account;
	gchar *number;
	GtkWidget *tview;

	priv = self->priv;

	number = ofa_accounts_book_get_selected( self );
	if( number ){
		account = ofo_account_get_by_number( priv->dossier, number );
		g_return_if_fail( account &&
				OFO_IS_ACCOUNT( account ) &&
				ofo_account_is_deletable( account, priv->dossier ));

		if( delete_confirmed( self, account ) &&
				ofo_account_delete( account, priv->dossier )){

			/* nothing to do here, all being managed by signal handlers
			 * just reset the selection as this is not managed by the
			 * account notebook (and doesn't have to)
			 * asking for selection of the just deleted account makes
			 * almost sure that we are going to select the most close
			 * row */
			on_tview_row_selected( NULL, self );
			ofa_accounts_book_set_selected( self, number );
		}
	}
	g_free( number );

	tview = ofa_accounts_book_get_top_focusable_widget( self );
	if( tview ){
		gtk_widget_grab_focus( tview );
	}
}

/*
 * - this is a root account with children and the preference is set so
 *   that all accounts will be deleted
 * - this is a root account and the preference is not set
 * - this is a detail account
 */
static gboolean
delete_confirmed( ofaAccountsBook *self, ofoAccount *account )
{
	ofaAccountsBookPrivate *priv;
	gchar *msg;
	gboolean delete_ok;

	priv = self->priv;

	if( ofo_account_is_root( account )){
		if( ofo_account_has_children( account, priv->dossier ) &&
				ofa_prefs_account_delete_root_with_children()){
			msg = g_strdup_printf( _(
					"You are about to delete the %s - %s account.\n"
					"This is a root account which has children.\n"
					"Are you sure ?" ),
					ofo_account_get_number( account ),
					ofo_account_get_label( account ));
		} else {
			msg = g_strdup_printf( _(
					"You are about to delete the %s - %s account.\n"
					"This is a root account. Are you sure ?" ),
					ofo_account_get_number( account ),
					ofo_account_get_label( account ));
		}
	} else {
		msg = g_strdup_printf( _( "Are you sure you want delete the '%s - %s' account ?" ),
					ofo_account_get_number( account ),
					ofo_account_get_label( account ));
	}

	delete_ok = ofa_main_window_confirm_deletion( priv->main_window, msg );

	g_free( msg );

	return( delete_ok );
}

static void
do_view_entries( ofaAccountsBook *self )
{
	ofaAccountsBookPrivate *priv;
	gchar *number;
	GtkWidget *tview;
	ofaPage *page;

	priv = self->priv;

	number = ofa_accounts_book_get_selected( self );
	page = ofa_main_window_activate_theme( priv->main_window, THM_VIEW_ENTRIES );
	ofa_view_entries_display_entries( OFA_VIEW_ENTRIES( page ), OFO_TYPE_ACCOUNT, number, NULL, NULL );
	g_free( number );

	tview = ofa_accounts_book_get_top_focusable_widget( self );
	if( tview ){
		gtk_widget_grab_focus( tview );
	}
}

static GtkWidget *
get_accounts_book( ofaAccountsBook *self )
{
	ofaAccountsBookPrivate *priv;

	priv = self->priv;

	if( !priv->book ){

		priv->book = GTK_NOTEBOOK( gtk_notebook_new());
		gtk_notebook_popup_enable( priv->book );

		g_signal_connect(
				G_OBJECT( priv->book ),
				"switch-page", G_CALLBACK( on_book_page_switched ), self );

		g_signal_connect(
				G_OBJECT( priv->book ),
				"key-press-event", G_CALLBACK( on_book_key_pressed ), self );
	}

	return( GTK_WIDGET( priv->book ));
}

static GtkWidget *
get_current_tree_view( const ofaAccountsBook *self )
{
	ofaAccountsBookPrivate *priv;
	gint page_n;
	GtkWidget *page_w;
	GtkWidget *tview;

	priv = self->priv;
	tview = NULL;

	page_n = gtk_notebook_get_current_page( priv->book );
	if( page_n >= 0 ){

		page_w = gtk_notebook_get_nth_page( priv->book, page_n );
		g_return_val_if_fail( page_w && GTK_IS_CONTAINER( page_w ), NULL );

		tview = my_utils_container_get_child_by_type(
								GTK_CONTAINER( page_w ), GTK_TYPE_TREE_VIEW );
		g_return_val_if_fail( tview && GTK_IS_TREE_VIEW( tview ), NULL );
	}

	return( tview );
}

/**
 * ofa_accounts_book_expand_all:
 */
void
ofa_accounts_book_expand_all( ofaAccountsBook *book )
{
	ofaAccountsBookPrivate *priv;
	gint pages_count, i;
	GtkWidget *page_w;
	GtkTreeView *tview;

	g_return_if_fail( book && OFA_IS_ACCOUNTS_BOOK( book ));

	priv = book->priv;

	if( !priv->dispose_has_run ){

		pages_count = gtk_notebook_get_n_pages( priv->book );
		for( i=0 ; i<pages_count ; ++i ){
			page_w = gtk_notebook_get_nth_page( priv->book, i );
			g_return_if_fail( page_w && GTK_IS_WIDGET( page_w ));
			tview = ( GtkTreeView * ) my_utils_container_get_child_by_type( GTK_CONTAINER( page_w ), GTK_TYPE_TREE_VIEW );
			g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));
			gtk_tree_view_expand_all( tview );
		}
	}
}

#if 0
static void
setup_buttons( ofaAccountsBook *self )
{
	ofaAccountsBookPrivate *priv;

	priv = self->priv;

	priv->buttons_box = MY_BUTTONS_BOX(
							ofa_page_create_default_buttons_box(
									2, G_CALLBACK( on_button_clicked ), self ));

	priv->btn_update = my_buttons_box_get_button_by_id( priv->buttons_box, BUTTONS_BOX_PROPERTIES );
	priv->btn_delete = my_buttons_box_get_button_by_id( priv->buttons_box, BUTTONS_BOX_DELETE );

	my_buttons_box_add_spacer( priv->buttons_box );

	if( priv->has_view_entries ){
		priv->btn_consult = gtk_button_new_with_mnemonic( _( "View _entries..." ));
		my_buttons_box_pack_button( priv->buttons_box,
				priv->btn_consult,
				FALSE, G_CALLBACK( on_view_entries ), self );
	}
}

/**
 * ofa_accounts_book_init_view:
 * @number: [allow-none]
 *
 * Populates the view, setting the first selection on account 'number'
 * if specified, or on the first visible account (if any) of the first
 * book's page.
 */
void
ofa_accounts_book_init_view( ofaAccountsBook *self, const gchar *number )
{
	static const gchar *thisfn = "ofa_accounts_book_init_view";
	ofaAccountsBookPrivate *priv;
	GtkWidget *page_w;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	g_return_if_fail( self && OFA_IS_ACCOUNTS_BOOK( self ));

	g_debug( "%s: self=%p, number=%s", thisfn, ( void * ) self, number );

	priv = self->priv;

	if( !priv->dispose_has_run ){

		insert_dataset( self );

		if( number && g_utf8_strlen( number, -1 )){
			/* takes care of expanding the needed rows */
			select_row_by_number( self, number );

		} else {
			book_expand_all_pages( self );
			page_w = gtk_notebook_get_nth_page( priv->book, 0 );
			if( page_w ){
				tview = ( GtkTreeView * ) my_utils_container_get_child_by_type( GTK_CONTAINER( page_w ), GTK_TYPE_TREE_VIEW );
				g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));
				tmodel = gtk_tree_view_get_model( tview );
				if( gtk_tree_model_get_iter_first( tmodel, &iter )){
					select_row_by_iter( self, tview, tmodel, &iter );
				}
			}
		}
	}
}

static void
remove_row_by_number( ofaAccountsBook *self, const gchar *number )
{
	gint page_num;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	page_num = book_get_page_by_class( self,
						ofo_account_get_class_from_number( number ), FALSE, &tview, &tmodel );

	if( page_num >= 0 &&
			tstore_find_row_by_number( self, number, tmodel, &iter, NULL )){

		gtk_tree_store_remove( GTK_TREE_STORE( tmodel ), &iter );
	}
}
#endif

/**
 * ofa_accounts_book_get_selected:
 * @book:
 *
 * Returns: the currently selected account number, as a newly allocated
 * string which should be g_free() by the caller.
 */
gchar *
ofa_accounts_book_get_selected( ofaAccountsBook *book )
{
	ofaAccountsBookPrivate *priv;
	gchar *account;
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	g_return_val_if_fail( book && OFA_IS_ACCOUNTS_BOOK( book ), NULL );

	priv = book->priv;
	account = NULL;

	if( !priv->dispose_has_run ){

		tview = ( GtkTreeView * ) get_current_tree_view( book );
		select = gtk_tree_view_get_selection( tview );
		if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
			gtk_tree_model_get( tmodel, &iter, priv->number_col, &account, -1 );
		}
	}

	return( account );
}

/**
 * ofa_accounts_book_set_selected:
 *
 * Let the user reset the selection after the end of setup and
 * initialization phases
 */
void
ofa_accounts_book_set_selected( ofaAccountsBook *book, const gchar *number )
{
	ofaAccountsBookPrivate *priv;

	g_return_if_fail( book && OFA_IS_ACCOUNTS_BOOK( book ));

	priv = book->priv;

	if( !priv->dispose_has_run ){

		select_row_by_number( book, number );
	}
}

/*
 * select the row with the given number, or the most close one
 * doesn't create the page class if it doesn't yet exist
 */
static void
select_row_by_number( ofaAccountsBook *self, const gchar *number )
{
	ofaAccountsBookPrivate *priv;
	GtkWidget *page_w;
	gint page_n;
	GtkWidget *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;

	priv = self->priv;

	if( number && g_utf8_strlen( number, -1 )){
		page_w = book_get_page_by_class( self,
							ofo_account_get_class_from_number( number ), FALSE );
		if( page_w ){
			page_n = gtk_notebook_page_num( priv->book, page_w );
			gtk_notebook_set_current_page( priv->book, page_n );
			if( ofa_account_istore_get_by_number( OFA_ACCOUNT_ISTORE( self ), number, &iter )){
				tview = my_utils_container_get_child_by_type(
								GTK_CONTAINER( page_w ), GTK_TYPE_TREE_VIEW );
				tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( tview ));
				path = gtk_tree_model_get_path( tmodel, &iter );
				gtk_tree_view_expand_to_path( GTK_TREE_VIEW( tview ), path );
				gtk_tree_path_free( path );
				select_row_by_iter( self, GTK_TREE_VIEW( tview ), tmodel, &iter );
			}
		}
	}
}

static void
select_row_by_iter( ofaAccountsBook *self, GtkTreeView *tview, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	GtkTreePath *path;

	path = gtk_tree_model_get_path( tmodel, iter );
	gtk_tree_view_set_cursor( GTK_TREE_VIEW( tview ), path, NULL, FALSE );
	gtk_tree_path_free( path );
	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}

/**
 * ofa_accounts_book_get_top_focusable_widget:
 *
 * Returns the top focusable widget, here the treeview of the current
 * page.
 */
GtkWidget *
ofa_accounts_book_get_top_focusable_widget( const ofaAccountsBook *book )
{
	ofaAccountsBookPrivate *priv;
	GtkWidget *tview;

	g_return_val_if_fail( book && OFA_IS_ACCOUNTS_BOOK( book ), NULL );

	priv = book->priv;

	if( !priv->dispose_has_run ){

		tview = get_current_tree_view( book );

		return( tview );
	}

	return( NULL );
}

#if 0
/*
 * the iso code 3a of a currency has changed - update the display
 * this should be very rare
 */
static void
on_updated_currency_code( ofaAccountsBook *self, ofoCurrency *currency )
{
	gint pages_count, i;
	GtkWidget *page_w;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoAccount *account;
	gint dev_id;

	dev_id = ofo_currency_get_id( currency );
	pages_count = gtk_notebook_get_n_pages( self->priv->book );

	for( i=0 ; i<pages_count ; ++i ){

		page_w = gtk_notebook_get_nth_page( self->priv->book, i );
		g_return_if_fail( page_w && GTK_IS_CONTAINER( page_w ));

		tview = ( GtkTreeView * )
						my_utils_container_get_child_by_type(
								GTK_CONTAINER( page_w ), GTK_TYPE_TREE_VIEW );
		g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));

		tmodel = gtk_tree_view_get_model( tview );
		if( gtk_tree_model_get_iter_first( tmodel, &iter )){
			while( TRUE ){
				gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &account, -1 );
				g_object_unref( account );

				if( ofo_account_get_currency( account ) == dev_id ){
					tstore_set_row_by_iter( self, account, tmodel, &iter );
				}
				if( !gtk_tree_model_iter_next( tmodel, &iter )){
					break;
				}
			}
		}
	}
}
#endif

/**
 * ofa_accounts_book_button_clicked:
 */
void
ofa_accounts_book_button_clicked( ofaAccountsBook *book, gint button_id )
{
	ofaAccountsBookPrivate *priv;

	g_return_if_fail( book && OFA_IS_ACCOUNTS_BOOK( book ));

	priv = book->priv;

	if( !priv->dispose_has_run ){

		switch( button_id ){
			case BUTTON_NEW:
				do_insert_account( book );
				break;
			case BUTTON_PROPERTIES:
				do_update_account( book );
				break;
			case BUTTON_DELETE:
				do_delete_account( book );
				break;
			case BUTTON_VIEW_ENTRIES:
				do_view_entries( book );
				break;
		}
	}
}
