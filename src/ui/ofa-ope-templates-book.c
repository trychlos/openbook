/*
 * Open Freelance OpeTemplateing
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
 *
 * Open Freelance OpeTemplateing is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Freelance OpeTemplateing is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Freelance OpeTemplateing; see the file COPYING. If not,
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
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"

#include "ui/ofa-buttons-box.h"
#include "ui/ofa-guided-input.h"
#include "ui/ofa-ope-template-properties.h"
#include "ui/ofa-ope-template-store.h"
#include "ui/ofa-ope-templates-book.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaOpeTemplatesBookPrivate {
	gboolean             dispose_has_run;

	ofaMainWindow       *main_window;
	ofoDossier          *dossier;
	GList               *dos_handlers;
	gchar               *dname;			/* to be used after dossier finalization */

	ofaOpeTemplateStore *ope_store;
	GList               *ope_handlers;
	GtkNotebook         *book;
};

/* the ledger mnemo is attached to each page of the notebook,
 * and also attached to the underlying treemodelfilter
 */
#define DATA_PAGE_LEDGER                    "ofa-data-page-ledger"

/* the column identifier is attached to each column header
 */
#define DATA_COLUMN_ID                      "ofa-data-column-id"

/* a settings which holds the order of ledger mnemos as a string list
 */
static const gchar *st_ledger_order         = "OpeTemplateBookOrder";

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	CLOSED,
	N_SIGNALS
};

static guint        st_signals[ N_SIGNALS ] = { 0 };

static void       create_notebook( ofaOpeTemplatesBook *book );
static void       on_book_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaOpeTemplatesBook *self );
static void       on_row_inserted( GtkTreeModel *tmodel, GtkTreePath *path, GtkTreeIter *iter, ofaOpeTemplatesBook *book );
static void       on_ofa_row_inserted( ofaOpeTemplateStore *store, const ofoOpeTemplate *ope, ofaOpeTemplatesBook *book );
static GtkWidget *book_get_page_by_ledger( ofaOpeTemplatesBook *self, const gchar *ledger, gboolean create );
static GtkWidget *book_create_page( ofaOpeTemplatesBook *self, const gchar *ledger );
static GtkWidget *book_create_scrolled_window( ofaOpeTemplatesBook *book, const gchar *ledger );
static GtkWidget *book_create_treeview( ofaOpeTemplatesBook *book, const gchar *ledger, GtkContainer *parent );
static void       book_create_columns( ofaOpeTemplatesBook *book, const gchar *ledger, GtkTreeView *tview );
static gboolean   is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, const gchar *ledger );
static void       on_tview_row_selected( GtkTreeSelection *selection, ofaOpeTemplatesBook *self );
static void       on_tview_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaOpeTemplatesBook *self );
static gboolean   on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaOpeTemplatesBook *self );
static void       on_tview_insert( ofaOpeTemplatesBook *self );
static void       on_tview_delete( ofaOpeTemplatesBook *self );
static void       on_tview_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaOpeTemplatesBook *self );
static void       do_insert_ope_template( ofaOpeTemplatesBook *self );
static void       do_update_ope_template( ofaOpeTemplatesBook *self );
static void       do_duplicate_ope_template( ofaOpeTemplatesBook *self );
static void       do_delete_ope_template( ofaOpeTemplatesBook *self );
static gboolean   delete_confirmed( ofaOpeTemplatesBook *self, ofoOpeTemplate *ope );
static void       do_guided_input( ofaOpeTemplatesBook *self );
static void       dossier_signals_connect( ofaOpeTemplatesBook *book );
static void       on_new_object( ofoDossier *dossier, ofoBase *object, ofaOpeTemplatesBook *book );
static void       on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaOpeTemplatesBook *book );
static void       on_updated_ledger_label( ofaOpeTemplatesBook *book, ofoLedger *ledger );
static void       on_updated_ope_template( ofaOpeTemplatesBook *book, ofoOpeTemplate *template );
static void       on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaOpeTemplatesBook *book );
static void       on_deleted_ledger_object( ofaOpeTemplatesBook *book, ofoLedger *ledger );
static void       on_reloaded_dataset( ofoDossier *dossier, GType type, ofaOpeTemplatesBook *book );
static GtkWidget *get_current_tree_view( const ofaOpeTemplatesBook *self );
static void       select_row_by_mnemo( ofaOpeTemplatesBook *self, const gchar *mnemo );
static void       select_row_by_iter( ofaOpeTemplatesBook *self, GtkTreeView *tview, GtkTreeModel *tfilter, GtkTreeIter *iter );
static void       on_action_closed( ofaOpeTemplatesBook *book );
static void       write_settings( ofaOpeTemplatesBook *book );

G_DEFINE_TYPE( ofaOpeTemplatesBook, ofa_ope_templates_book, GTK_TYPE_BIN )

static void
ope_templates_book_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ope_templates_book_finalize";
	ofaOpeTemplatesBookPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATES_BOOK( instance ));

	/* free data members here */
	priv = OFA_OPE_TEMPLATES_BOOK( instance )->priv;

	g_free( priv->dname );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_templates_book_parent_class )->finalize( instance );
}

static void
ope_templates_book_dispose( GObject *instance )
{
	ofaOpeTemplatesBookPrivate *priv;
	GList *it;

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATES_BOOK( instance ));

	priv = ( OFA_OPE_TEMPLATES_BOOK( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		/* disconnect from ofoDossier */
		if( priv->dossier &&
				OFO_IS_DOSSIER( priv->dossier ) && !ofo_dossier_has_dispose_run( priv->dossier )){
			for( it=priv->dos_handlers ; it ; it=it->next ){
				g_signal_handler_disconnect( priv->dossier, ( gulong ) it->data );
			}
		}

		/* disconnect from ofaOpeTemplateStore */
		if( priv->ope_store && OFA_IS_OPE_TEMPLATE_STORE( priv->ope_store )){
			for( it=priv->ope_handlers ; it ; it=it->next ){
				g_signal_handler_disconnect( priv->ope_store, ( gulong ) it->data );
			}
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_templates_book_parent_class )->dispose( instance );
}

static void
ofa_ope_templates_book_init( ofaOpeTemplatesBook *self )
{
	static const gchar *thisfn = "ofa_ope_templates_book_init";

	g_return_if_fail( self && OFA_IS_OPE_TEMPLATES_BOOK( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_OPE_TEMPLATES_BOOK, ofaOpeTemplatesBookPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
ofa_ope_templates_book_class_init( ofaOpeTemplatesBookClass *klass )
{
	static const gchar *thisfn = "ofa_ope_templates_book_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ope_templates_book_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_templates_book_finalize;

	g_type_class_add_private( klass, sizeof( ofaOpeTemplatesBookPrivate ));

	/**
	 * ofaOpeTemplatesBook::changed:
	 *
	 * This signal is sent on the #ofaOpeTemplatesBook when the selection
	 * in the current treeview is changed.
	 *
	 * Argument is the selected operation template mnemo.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaOpeTemplatesBook *store,
	 * 						const gchar      *mnemo,
	 * 						gpointer          user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_OPE_TEMPLATES_BOOK,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_STRING );

	/**
	 * ofaOpeTemplatesBook::activated:
	 *
	 * This signal is sent on the #ofaOpeTemplatesBook when the selection
	 * in the current treeview is activated.
	 *
	 * Argument is the selected operation template mnemo.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaOpeTemplatesBook *store,
	 * 						const gchar      *mnemo,
	 * 						gpointer          user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-activated",
				OFA_TYPE_OPE_TEMPLATES_BOOK,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_STRING );

	/**
	 * ofaOpeTemplatesBook::closed:
	 *
	 * This signal is sent on the #ofaOpeTemplatesBook when the book is
	 * about to be closed.
	 *
	 * The #ofaOpeTemplatesBook takes advantage of this signal to save
	 * its own settings.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaOpeTemplatesBook *store,
	 * 						gpointer           user_data );
	 */
	st_signals[ CLOSED ] = g_signal_new_class_handler(
				"ofa-closed",
				OFA_TYPE_OPE_TEMPLATES_BOOK,
				G_SIGNAL_ACTION,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0,
				G_TYPE_NONE );
}

/**
 * ofa_ope_templates_book_new:
 *
 * Creates the structured content, i.e. one notebook with one page per
 * ledger.
 *
 * Does NOT insert the data (see: ofa_ope_templates_book_set_main_window).
 */
ofaOpeTemplatesBook *
ofa_ope_templates_book_new( void  )
{
	ofaOpeTemplatesBook *book;

	book = g_object_new( OFA_TYPE_OPE_TEMPLATES_BOOK, NULL );

	create_notebook( book );

	g_signal_connect( book, "ofa-closed", G_CALLBACK( on_action_closed ), book );

	return( book );
}

static void
create_notebook( ofaOpeTemplatesBook *self )
{
	ofaOpeTemplatesBookPrivate *priv;
	GtkWidget *book;

	priv = self->priv;

	book = gtk_notebook_new();
	gtk_container_add( GTK_CONTAINER( self ), book );
	priv->book = GTK_NOTEBOOK( book );

	gtk_notebook_popup_enable( priv->book );
	gtk_notebook_set_scrollable( priv->book, TRUE );

	g_signal_connect(
			G_OBJECT( priv->book ),
			"switch-page", G_CALLBACK( on_book_page_switched ), book );

	gtk_widget_show_all( GTK_WIDGET( self ));
}

/*
 * we have switch to this given page (wpage, npage)
 * just setup the selection
 */
static void
on_book_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaOpeTemplatesBook *self )
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

/**
 * ofa_ope_templates_book_set_main_window:
 *
 * This is required in order to get the dossier which will permit to
 * create the underlying list store.
 */
void
ofa_ope_templates_book_set_main_window( ofaOpeTemplatesBook *book, ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_ope_templates_book_set_main_window";
	ofaOpeTemplatesBookPrivate *priv;
	gulong handler;
	GList *strlist, *it;

	g_debug( "%s: book=%p, main_window=%p", thisfn, ( void * ) book, ( void * ) main_window );

	g_return_if_fail( book && OFA_IS_OPE_TEMPLATES_BOOK( book ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv = book->priv;

	if( !priv->dispose_has_run ){

		/* the notebook must have been created first */
		g_return_if_fail( priv->book && GTK_IS_NOTEBOOK( priv->book ));

		priv->main_window = main_window;
		priv->dossier = ofa_main_window_get_dossier( main_window );
		priv->ope_store = ofa_ope_template_store_new( priv->dossier );

		/* create one page per ledger
		 * if strlist is set, then create one page per ledger
		 * other needed pages will be created on fly */
		priv->dname = g_strdup( ofo_dossier_get_name( priv->dossier ));
		strlist = ofa_settings_dossier_get_string_list( priv->dname, st_ledger_order );
		for( it=strlist ; it ; it=it->next ){
			book_get_page_by_ledger( book, ( const gchar * ) it->data, TRUE );
		}
		ofa_settings_free_string_list( strlist );

		handler = g_signal_connect(
				priv->ope_store, "row-inserted", G_CALLBACK( on_row_inserted ), book );
		priv->ope_handlers = g_list_prepend( priv->ope_handlers, ( gpointer ) handler );

		handler = g_signal_connect(
				priv->ope_store, "ofa-row-inserted", G_CALLBACK( on_ofa_row_inserted ), book );
		priv->ope_handlers = g_list_prepend( priv->ope_handlers, ( gpointer ) handler );

		ofa_ope_template_store_load_dataset( priv->ope_store );

		dossier_signals_connect( book );
	}
}

/*
 * is triggered by the store when a row is inserted
 * we try to optimize the search by keeping the ledger of the last
 * inserted row;
 */
static void
on_row_inserted( GtkTreeModel *tmodel, GtkTreePath *path, GtkTreeIter *iter, ofaOpeTemplatesBook *book )
{
	ofoOpeTemplate *ope;

	gtk_tree_model_get( tmodel, iter, OPE_TEMPLATE_COL_OBJECT, &ope, -1 );
	g_return_if_fail( ope && OFO_IS_OPE_TEMPLATE( ope ));
	g_object_unref( ope );

	on_ofa_row_inserted( NULL, ope, book );
}

/*
 * store may be NULL when called from above on_row_inserted()
 */
static void
on_ofa_row_inserted( ofaOpeTemplateStore *store, const ofoOpeTemplate *ope, ofaOpeTemplatesBook *book )
{
	const gchar *ledger;

	ledger = ofo_ope_template_get_ledger( ope );
	book_get_page_by_ledger( book, ledger, TRUE );
}

/*
 * Returns the notebook's page container which is dedicated to the
 * given ledger.
 *
 * If the page doesn't exist, and @bcreate is %TRUE, then it is created.
 */
static GtkWidget *
book_get_page_by_ledger( ofaOpeTemplatesBook *book, const gchar *ledger, gboolean bcreate )
{
	static const gchar *thisfn = "ofa_ope_templates_book_get_page_by_ledger";
	ofaOpeTemplatesBookPrivate *priv;
	gint count, i;
	GtkWidget *found, *page_widget;
	const gchar *page_ledger;

	priv = book->priv;
	found = NULL;
	count = gtk_notebook_get_n_pages( priv->book );

	/* search for an existing page */
	for( i=0 ; i<count ; ++i ){
		page_widget = gtk_notebook_get_nth_page( priv->book, i );
		page_ledger = ( const gchar * ) g_object_get_data( G_OBJECT( page_widget ), DATA_PAGE_LEDGER );
		if( !g_utf8_collate( page_ledger, ledger )){
			found = page_widget;
			break;
		}
	}

	/* if not exists, create it (if allowed) */
	if( !found && bcreate ){
		found = book_create_page( book, ledger );
		if( !found ){
			g_warning( "%s: unable to create the page for ledger=%s", thisfn, ledger );
			return( NULL );
		} else {
			gtk_widget_show_all( found );
		}
	}

	return( found );
}

/*
 * @ledger: ledger mnemo
 *
 * creates the page widget for the given ledger
 */
static GtkWidget *
book_create_page( ofaOpeTemplatesBook *book, const gchar *ledger )
{
	static const gchar *thisfn = "ofa_ope_templates_book_create_page";
	GtkWidget *scrolled, *tview;

	g_debug( "%s: book=%p, ledger=%s", thisfn, ( void * ) book, ledger );

	scrolled = book_create_scrolled_window( book, ledger );
	if( scrolled ){
		tview = book_create_treeview( book, ledger, GTK_CONTAINER( scrolled ));
		if( tview ){
			book_create_columns( book, ledger, GTK_TREE_VIEW( tview ));
		}
	}

	return( scrolled );
}

/*
 * creates the page widget as a scrolled window
 * attach it to the notebook
 * set label and shortcut
 */
static GtkWidget *
book_create_scrolled_window( ofaOpeTemplatesBook *book, const gchar *ledger )
{
	static const gchar *thisfn = "ofa_ope_templates_book_create_scrolled_window";
	ofaOpeTemplatesBookPrivate *priv;
	GtkWidget *scrolled, *label;
	ofoLedger *ledger_obj;
	const gchar *ledger_label;
	gint page_num;

	priv = book->priv;

	scrolled = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW( scrolled ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );

	ledger_obj = ofo_ledger_get_by_mnemo( priv->dossier, ledger );
	if( ledger_obj ){
		g_return_val_if_fail( OFO_IS_LEDGER( ledger_obj ), NULL );
		ledger_label = ofo_ledger_get_label( ledger_obj );
	} else {
		ledger_label = UNKNOWN_LEDGER_LABEL;
	}

	label = gtk_label_new( ledger_label );

	page_num = gtk_notebook_append_page( priv->book, scrolled, label );
	if( page_num == -1 ){
		g_warning( "%s: unable to add a page to the notebook for ledger=%s", thisfn, ledger );
		return( NULL );
	}
	gtk_notebook_set_tab_reorderable( priv->book, scrolled, TRUE );
	g_object_set_data( G_OBJECT( scrolled ), DATA_PAGE_LEDGER, g_strdup( ledger ));

	return( scrolled );
}

/*
 * creates the treeview
 * attach it to the container parent (the scrolled window)
 * setup the model filter
 */
static GtkWidget *
book_create_treeview( ofaOpeTemplatesBook *book, const gchar *ledger, GtkContainer *parent )
{
	static const gchar *thisfn = "ofa_ope_templates_book_create_treeview";
	ofaOpeTemplatesBookPrivate *priv;
	GtkWidget *tview;
	GtkTreeModel *tfilter;
	GtkTreeSelection *select;

	priv = book->priv;

	tview = gtk_tree_view_new();
	gtk_container_add( parent, tview );

	gtk_widget_set_hexpand( tview, TRUE );
	gtk_widget_set_vexpand( tview, TRUE );
	gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( tview ), TRUE );

	tfilter = gtk_tree_model_filter_new( GTK_TREE_MODEL( priv->ope_store ), NULL );
	g_debug( "%s: store=%p, tfilter=%p", thisfn, ( void * ) priv->ope_store, ( void * ) tfilter );
	gtk_tree_model_filter_set_visible_func(
			GTK_TREE_MODEL_FILTER( tfilter ),
			( GtkTreeModelFilterVisibleFunc ) is_visible_row, g_strdup( ledger ), NULL );

	gtk_tree_view_set_model( GTK_TREE_VIEW( tview ), tfilter );
	g_object_unref( tfilter );

	g_signal_connect(
			G_OBJECT( tview ), "row-activated", G_CALLBACK( on_tview_row_activated), book );
	g_signal_connect(
			G_OBJECT( tview ), "key-press-event", G_CALLBACK( on_tview_key_pressed ), book );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect(
			G_OBJECT( select ), "changed", G_CALLBACK( on_tview_row_selected ), book );

	return( tview );
}

/*
 * creates the columns in the GtkTreeView
 */
static void
book_create_columns( ofaOpeTemplatesBook *book, const gchar *ledger, GtkTreeView *tview )
{
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
				_( "Mnemo" ),
				cell, "text", OPE_TEMPLATE_COL_MNEMO,
				NULL );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( OPE_TEMPLATE_COL_MNEMO ));
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, cell, ( GtkTreeCellDataFunc ) on_tview_cell_data_func, book, NULL );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			cell, "text", OPE_TEMPLATE_COL_LABEL,
			NULL );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( OPE_TEMPLATE_COL_LABEL ));
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, cell, ( GtkTreeCellDataFunc ) on_tview_cell_data_func, book, NULL );
}

/*
 * tmodel here is the ofaListStore
 */
static gboolean
is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, const gchar *ledger )
{
	gboolean is_visible;
	ofoOpeTemplate *ope;

	is_visible = FALSE;

	gtk_tree_model_get( tmodel, iter, OPE_TEMPLATE_COL_OBJECT, &ope, -1 );
	g_return_val_if_fail( ope && OFO_IS_OPE_TEMPLATE( ope ), FALSE );
	g_object_unref( ope );

	is_visible = ( !g_utf8_collate( ledger, ofo_ope_template_get_ledger( ope )));

	return( is_visible );
}

static void
on_tview_row_selected( GtkTreeSelection *selection, ofaOpeTemplatesBook *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *mnemo;

	mnemo = NULL;

	/* selection may be null when called from on_delete_clicked() */
	if( selection ){
		if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
			gtk_tree_model_get( tmodel, &iter, OPE_TEMPLATE_COL_MNEMO, &mnemo, -1 );
			g_signal_emit_by_name( self, "ofa-changed", mnemo );
			g_free( mnemo );
		}
	}

	/*update_buttons_sensitivity( self, account );*/
}

static void
on_tview_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaOpeTemplatesBook *self )
{
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *mnemo;

	mnemo = NULL;

	select = gtk_tree_view_get_selection( tview );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, OPE_TEMPLATE_COL_MNEMO, &mnemo, -1 );
		g_signal_emit_by_name( self, "ofa-activated", mnemo );
		g_free( mnemo );
	}
}

/*
 * Returns :
 * TRUE to stop other dos_handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaOpeTemplatesBook *self )
{
	gboolean stop;

	stop = FALSE;

	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Insert ){
			on_tview_insert( self );

		} else if( event->keyval == GDK_KEY_Delete ){
			on_tview_delete( self );
		}
	}

	return( stop );
}

static void
on_tview_insert( ofaOpeTemplatesBook *self )
{
	do_insert_ope_template( self );
}

static void
on_tview_delete( ofaOpeTemplatesBook *self )
{
	ofaOpeTemplatesBookPrivate *priv;
	gchar *mnemo;
	ofoOpeTemplate *ope;

	priv = self->priv;

	mnemo = ofa_ope_templates_book_get_selected( self );
	ope = ofo_ope_template_get_by_mnemo( priv->dossier, mnemo );
	g_free( mnemo );

	if( ofo_ope_template_is_deletable( ope, priv->dossier )){
		do_delete_ope_template( self );
	}
}

/*
 * no particular style here
 */
static void
on_tview_cell_data_func( GtkTreeViewColumn *tcolumn,
						GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
						ofaOpeTemplatesBook *self )
{
	g_return_if_fail( GTK_IS_CELL_RENDERER_TEXT( cell ));
}

static void
do_insert_ope_template( ofaOpeTemplatesBook *self )
{
	ofaOpeTemplatesBookPrivate *priv;
	ofoOpeTemplate *ope;
	gint page_n;
	GtkWidget *page_w;
	const gchar *ledger;

	priv = self->priv;
	ledger = NULL;

	page_n = gtk_notebook_get_current_page( priv->book );
	if( page_n >= 0 ){
		page_w = gtk_notebook_get_nth_page( priv->book, page_n );
		ledger = ( const gchar * ) g_object_get_data( G_OBJECT( page_w ), DATA_PAGE_LEDGER );
	}

	ope = ofo_ope_template_new();

	if( !ofa_ope_template_properties_run( priv->main_window, ope, ledger )){
		g_object_unref( ope );

	} else {
		select_row_by_mnemo( self, ofo_ope_template_get_mnemo( ope ));
	}
}

static void
do_update_ope_template( ofaOpeTemplatesBook *self )
{
	ofaOpeTemplatesBookPrivate *priv;
	ofoOpeTemplate *ope;
	gchar *mnemo;
	GtkWidget *tview;

	priv = self->priv;

	mnemo = ofa_ope_templates_book_get_selected( self );
	if( mnemo ){
		ope = ofo_ope_template_get_by_mnemo( priv->dossier, mnemo );
		g_return_if_fail( ope && OFO_IS_OPE_TEMPLATE( ope ));

		ofa_ope_template_properties_run( priv->main_window, ope, NULL );
	}
	g_free( mnemo );

	tview = ofa_ope_templates_book_get_top_focusable_widget( self );
	if( tview ){
		gtk_widget_grab_focus( tview );
	}
}

static void
do_duplicate_ope_template( ofaOpeTemplatesBook *self )
{
	static const gchar *thisfn = "ofa_ope_templates_book_do_duplicate_ope_template";
	ofaOpeTemplatesBookPrivate *priv;
	gchar *mnemo, *new_mnemo;
	ofoOpeTemplate *ope;
	ofoOpeTemplate *duplicate;
	gchar *str;

	g_return_if_fail( OFA_IS_OPE_TEMPLATES_BOOK( self ));

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = self->priv;

	mnemo = ofa_ope_templates_book_get_selected( self );
	if( mnemo ){
		ope = ofo_ope_template_get_by_mnemo( priv->dossier, mnemo );
		g_return_if_fail( ope && OFO_IS_OPE_TEMPLATE( ope ));

		duplicate = ofo_ope_template_new_from_template( ope );
		new_mnemo = ofo_ope_template_get_mnemo_new_from( ope, priv->dossier );
		ofo_ope_template_set_mnemo( duplicate, new_mnemo );

		str = g_strdup_printf( "%s (%s)", ofo_ope_template_get_label( ope ), _( "Duplicate" ));
		ofo_ope_template_set_label( duplicate, str );
		g_free( str );

		if( !ofo_ope_template_insert( duplicate, priv->dossier )){
			g_object_unref( duplicate );
		} else {
			select_row_by_mnemo( self, new_mnemo );
		}
		g_free( new_mnemo );
	}
	g_free( mnemo );
}

static void
do_delete_ope_template( ofaOpeTemplatesBook *self )
{
	ofaOpeTemplatesBookPrivate *priv;
	ofoOpeTemplate *ope;
	gchar *mnemo;
	GtkWidget *tview;

	priv = self->priv;

	mnemo = ofa_ope_templates_book_get_selected( self );
	if( mnemo ){
		ope = ofo_ope_template_get_by_mnemo( priv->dossier, mnemo );
		g_return_if_fail( ope &&
				OFO_IS_OPE_TEMPLATE( ope ) &&
				ofo_ope_template_is_deletable( ope, priv->dossier ));

		if( delete_confirmed( self, ope ) &&
				ofo_ope_template_delete( ope, priv->dossier )){

			/* nothing to do here, all being managed by signal dos_handlers
			 * just reset the selection as this is not managed by the
			 * ope notebook (and doesn't have to)
			 * asking for selection of the just deleted ope makes
			 * almost sure that we are going to select the most close
			 * row */
			on_tview_row_selected( NULL, self );
			ofa_ope_templates_book_set_selected( self, mnemo );
		}
	}
	g_free( mnemo );

	tview = ofa_ope_templates_book_get_top_focusable_widget( self );
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
delete_confirmed( ofaOpeTemplatesBook *self, ofoOpeTemplate *ope )
{
	ofaOpeTemplatesBookPrivate *priv;
	gchar *msg;
	gboolean delete_ok;

	priv = self->priv;

	msg = g_strdup_printf( _( "Are you sure you want to delete the '%s - %s' entry model ?" ),
			ofo_ope_template_get_mnemo( ope ),
			ofo_ope_template_get_label( ope ));

	delete_ok = ofa_main_window_confirm_deletion( priv->main_window, msg );

	g_free( msg );

	return( delete_ok );
}

static void
do_guided_input( ofaOpeTemplatesBook *self )
{
	ofaOpeTemplatesBookPrivate *priv;
	ofoOpeTemplate *ope;
	gchar *mnemo;

	priv = self->priv;

	mnemo = ofa_ope_templates_book_get_selected( self );
	if( mnemo ){
		ope = ofo_ope_template_get_by_mnemo( priv->dossier, mnemo );
		g_return_if_fail( ope && OFO_IS_OPE_TEMPLATE( ope ));

		ofa_guided_input_run( priv->main_window, ope );
	}
	g_free( mnemo );
}

static void
dossier_signals_connect( ofaOpeTemplatesBook *book )
{
	ofaOpeTemplatesBookPrivate *priv;
	gulong handler;

	priv = book->priv;

	handler = g_signal_connect(
						G_OBJECT( priv->dossier),
						SIGNAL_DOSSIER_NEW_OBJECT, G_CALLBACK( on_new_object ), book );
	priv->dos_handlers = g_list_prepend( priv->dos_handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( priv->dossier),
						SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), book );
	priv->dos_handlers = g_list_prepend( priv->dos_handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( priv->dossier),
						SIGNAL_DOSSIER_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), book );
	priv->dos_handlers = g_list_prepend( priv->dos_handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( priv->dossier),
						SIGNAL_DOSSIER_RELOAD_DATASET, G_CALLBACK( on_reloaded_dataset ), book );
	priv->dos_handlers = g_list_prepend( priv->dos_handlers, ( gpointer ) handler );
}

/*
 * SIGNAL_DOSSIER_NEW_OBJECT signal handler
 */
static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaOpeTemplatesBook *book )
{
	static const gchar *thisfn = "ofa_ope_templates_book_on_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), book=%p",
			thisfn, ( void * ) dossier,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), ( void * ) book );
}

/*
 * OFA_SIGNAL_UPDATE_OBJECT signal handler
 */
static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaOpeTemplatesBook *book )
{
	static const gchar *thisfn = "ofa_ope_templates_book_on_updated_object";

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, book=%p",
			thisfn, ( void * ) dossier,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), prev_id, ( void * ) book );

	if( OFO_IS_LEDGER( object )){
		on_updated_ledger_label( book, OFO_LEDGER( object ));

	} else if( OFO_IS_OPE_TEMPLATE( object )){
		on_updated_ope_template( book, OFO_OPE_TEMPLATE( object ));
	}
}

/*
 * a ledger label has changed : update the corresponding tab label
 */
static void
on_updated_ledger_label( ofaOpeTemplatesBook *book, ofoLedger *ledger )
{
	ofaOpeTemplatesBookPrivate *priv;
	GtkWidget *page_w;
	const gchar *mnemo;

	priv = book->priv;

	mnemo = ofo_ledger_get_mnemo( ledger );
	page_w = book_get_page_by_ledger( book, mnemo, FALSE );
	if( page_w ){
		g_return_if_fail( GTK_IS_WIDGET( page_w ));
		gtk_notebook_set_tab_label_text( priv->book, page_w, ofo_ledger_get_label( ledger ));
	}
}

/*
 * we do not have any way to know if the ledger attached to the operation
 *  template has changed or not - So just make sure the correct page is
 *  shown
 */
static void
on_updated_ope_template( ofaOpeTemplatesBook *book, ofoOpeTemplate *template )
{
	ofaOpeTemplatesBookPrivate *priv;
	GtkWidget *page_w;
	const gchar *ledger;
	gint page_n;

	priv = book->priv;

	ledger = ofo_ope_template_get_ledger( template );
	page_w = book_get_page_by_ledger( book, ledger, TRUE );
	g_return_if_fail( page_w && GTK_IS_WIDGET( page_w ));
	select_row_by_mnemo( book, ofo_ope_template_get_mnemo( template ));

	page_n = gtk_notebook_page_num( priv->book, page_w );
	gtk_notebook_set_current_page( priv->book, page_n );
}

/*
 * SIGNAL_DOSSIER_DELETED_OBJECT signal handler
 */
static void
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaOpeTemplatesBook *book )
{
	static const gchar *thisfn = "ofa_ope_templates_book_on_deleted_object";

	g_debug( "%s: dossier=%p, object=%p (%s), book=%p",
			thisfn, ( void * ) dossier,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), ( void * ) book );

	if( OFO_IS_LEDGER( object )){
		on_deleted_ledger_object( book, OFO_LEDGER( object ));
	}
}

static void
on_deleted_ledger_object( ofaOpeTemplatesBook *book, ofoLedger *ledger )
{
	ofaOpeTemplatesBookPrivate *priv;
	GtkWidget *page_w;
	const gchar *mnemo;

	priv = book->priv;

	mnemo = ofo_ledger_get_mnemo( ledger );
	page_w = book_get_page_by_ledger( book, mnemo, FALSE );
	if( page_w ){
		g_return_if_fail( GTK_IS_WIDGET( page_w ));
		gtk_notebook_set_tab_label_text( priv->book, page_w, UNKNOWN_LEDGER_LABEL );
		g_object_set_data( G_OBJECT( page_w ), DATA_PAGE_LEDGER, UNKNOWN_LEDGER_MNEMO );
	}
}

/*
 * SIGNAL_DOSSIER_RELOAD_DATASET signal handler
 */
static void
on_reloaded_dataset( ofoDossier *dossier, GType type, ofaOpeTemplatesBook *book )
{
	static const gchar *thisfn = "ofa_ope_templates_book_on_reloaded_dataset";

	g_debug( "%s: dossier=%p, type=%lu, book=%p",
			thisfn, ( void * ) dossier, type, ( void * ) book );
}

static GtkWidget *
get_current_tree_view( const ofaOpeTemplatesBook *self )
{
	ofaOpeTemplatesBookPrivate *priv;
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
 * ofa_ope_templates_book_get_selected:
 * @book:
 *
 * Returns: the currently selected account number, as a newly allocated
 * string which should be g_free() by the caller.
 */
gchar *
ofa_ope_templates_book_get_selected( ofaOpeTemplatesBook *book )
{
	ofaOpeTemplatesBookPrivate *priv;
	gchar *mnemo;
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	g_return_val_if_fail( book && OFA_IS_OPE_TEMPLATES_BOOK( book ), NULL );

	priv = book->priv;
	mnemo = NULL;

	if( !priv->dispose_has_run ){

		tview = ( GtkTreeView * ) get_current_tree_view( book );
		if( tview ){
			select = gtk_tree_view_get_selection( tview );
			if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
				gtk_tree_model_get( tmodel, &iter, OPE_TEMPLATE_COL_MNEMO, &mnemo, -1 );
			}
		}
	}

	return( mnemo );
}

/**
 * ofa_ope_templates_book_set_selected:
 *
 * Let the user reset the selection after the end of setup and
 * initialization phases
 */
void
ofa_ope_templates_book_set_selected( ofaOpeTemplatesBook *book, const gchar *mnemo )
{
	ofaOpeTemplatesBookPrivate *priv;

	g_return_if_fail( book && OFA_IS_OPE_TEMPLATES_BOOK( book ));

	priv = book->priv;

	if( !priv->dispose_has_run ){

		select_row_by_mnemo( book, mnemo );
	}
}

/*
 * select the row with the given number, or the most close one
 * doesn't create the page class if it doesn't yet exist
 */
static void
select_row_by_mnemo( ofaOpeTemplatesBook *book, const gchar *mnemo )
{
	ofaOpeTemplatesBookPrivate *priv;
	ofoOpeTemplate *ope;
	const gchar *ledger;
	GtkWidget *page_w;
	gint page_n;
	GtkWidget *tview;
	GtkTreeModel *tfilter;
	GtkTreeIter store_iter, filter_iter;
	GtkTreePath *path;

	priv = book->priv;

	if( mnemo && g_utf8_strlen( mnemo, -1 )){
		ope = ofo_ope_template_get_by_mnemo( priv->dossier, mnemo );
		if( ope ){
			g_return_if_fail( OFO_IS_OPE_TEMPLATE( ope ));
			ledger = ofo_ope_template_get_ledger( ope );
			g_debug( "select_row_by_mnemo: ledger=%s", ledger );
			if( ledger && g_utf8_strlen( ledger, -1 )){
				page_w = book_get_page_by_ledger( book, ledger, FALSE );
				if( page_w ){
					page_n = gtk_notebook_page_num( priv->book, page_w );
					gtk_notebook_set_current_page( priv->book, page_n );

					ofa_ope_template_store_get_by_mnemo( priv->ope_store, mnemo, &store_iter );

					tview = my_utils_container_get_child_by_type(
									GTK_CONTAINER( page_w ), GTK_TYPE_TREE_VIEW );
					tfilter = gtk_tree_view_get_model( GTK_TREE_VIEW( tview ));
					gtk_tree_model_filter_convert_child_iter_to_iter(
							GTK_TREE_MODEL_FILTER( tfilter ), &filter_iter, &store_iter );

					path = gtk_tree_model_get_path( tfilter, &filter_iter );
					gtk_tree_view_expand_to_path( GTK_TREE_VIEW( tview ), path );
					gtk_tree_path_free( path );

					select_row_by_iter( book, GTK_TREE_VIEW( tview ), tfilter, &filter_iter );
				}
			}
		}
	}
}

static void
select_row_by_iter( ofaOpeTemplatesBook *book, GtkTreeView *tview, GtkTreeModel *tfilter, GtkTreeIter *iter )
{
	GtkTreePath *path;

	path = gtk_tree_model_get_path( tfilter, iter );
	gtk_tree_view_set_cursor( GTK_TREE_VIEW( tview ), path, NULL, FALSE );
	gtk_tree_path_free( path );
	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}

/**
 * ofa_ope_templates_book_get_top_focusable_widget:
 *
 * Returns the top focusable widget, here the treeview of the current
 * page.
 */
GtkWidget *
ofa_ope_templates_book_get_top_focusable_widget( const ofaOpeTemplatesBook *book )
{
	ofaOpeTemplatesBookPrivate *priv;
	GtkWidget *tview;

	g_return_val_if_fail( book && OFA_IS_OPE_TEMPLATES_BOOK( book ), NULL );

	priv = book->priv;

	if( !priv->dispose_has_run ){

		tview = get_current_tree_view( book );

		return( tview );
	}

	return( NULL );
}

/**
 * ofa_ope_templates_book_button_clicked:
 */
void
ofa_ope_templates_book_button_clicked( ofaOpeTemplatesBook *book, gint button_id )
{
	static const gchar *thisfn = "ofa_ope_templates_book_button_clicked";
	ofaOpeTemplatesBookPrivate *priv;

	g_return_if_fail( book && OFA_IS_OPE_TEMPLATES_BOOK( book ));

	priv = book->priv;

	if( !priv->dispose_has_run ){

		switch( button_id ){
			case BUTTON_NEW:
				do_insert_ope_template( book );
				break;
			case BUTTON_PROPERTIES:
				do_update_ope_template( book );
				break;
			case BUTTON_DUPLICATE:
				do_duplicate_ope_template( book );
				break;
			case BUTTON_DELETE:
				do_delete_ope_template( book );
				break;
			case BUTTON_GUIDED_INPUT:
				do_guided_input( book );
				break;
			default:
				g_warning( "%s: unmanaged button_id=%d", thisfn, button_id );
				break;
		}
	}
}

static void
on_action_closed( ofaOpeTemplatesBook *book )
{
	static const gchar *thisfn = "ofa_ope_templates_book_on_action_closed";

	g_debug( "%s: book=%p", thisfn, ( void * ) book );

	write_settings( book );
}

static void
write_settings( ofaOpeTemplatesBook *book )
{
	ofaOpeTemplatesBookPrivate *priv;
	GList *strlist;
	gint i, count;
	GtkWidget *page;
	const gchar *mnemo;

	priv = book->priv;
	strlist = NULL;

	/* record in settings the pages position */
	count = gtk_notebook_get_n_pages( priv->book );
	for( i=0 ; i<count ; ++i ){
		page = gtk_notebook_get_nth_page( priv->book, i );
		mnemo = ( const gchar * ) g_object_get_data( G_OBJECT( page ), DATA_PAGE_LEDGER );
		strlist = g_list_append( strlist, ( gpointer ) mnemo );
	}

	ofa_settings_dossier_set_string_list( priv->dname, st_ledger_order, strlist );

	g_list_free( strlist );
}
