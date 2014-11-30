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

#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofo-account.h"
#include "api/ofo-class.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"

#include "core/ofa-preferences.h"

#include "ui/my-buttons-box.h"
#include "ui/ofa-account-properties.h"
#include "ui/ofa-accounts-book.h"
#include "ui/ofa-page.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaAccountsBookPrivate {
	gboolean          dispose_has_run;

	/* input data
	 */
	ofaMainWindow    *main_window;
	ofoDossier       *dossier;
	GtkContainer     *parent;
	gboolean          has_import;
	gboolean          has_export;
	gboolean          has_view_entries;
	ofaAccountsBookCb pfnSelected;
	ofaAccountsBookCb pfnActivated;
	ofaAccountsBookCb pfnViewEntries;
	gpointer          user_data;

	/* internals
	 */
	GList            *handlers;

	/* UI
	 */
	GtkGrid          *grid;
	GtkNotebook      *book;
	myButtonsBox     *buttons_box;
	GtkWidget        *btn_update;
	GtkWidget        *btn_delete;
	GtkWidget        *btn_consult;
};

/* column ordering in the listview
 */
enum {
	COL_NUMBER = 0,
	COL_LABEL,
	COL_DEBIT,
	COL_CREDIT,
	COL_CURRENCY,
	COL_OBJECT,
	N_COLUMNS
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

/* a structure used when moving a subtree to another place
 */
typedef struct {
	ofoAccount          *account;
	GtkTreeModel        *tmodel;
	GtkTreeRowReference *ref;
}
	sChild;

/* data attached to each page of the classes notebook
 */
#define DATA_PAGE_CLASS         "ofa-data-page-class"
#define DATA_COLUMN_ID          "ofa-data-column-id"

G_DEFINE_TYPE( ofaAccountsBook, ofa_accounts_book, G_TYPE_OBJECT )

static void       on_parent_window_finalized( ofaAccountsBook *self, gpointer this_was_the_dialog );
static void       dossier_signals_connect( ofaAccountsBook *self );
static void       on_new_object( ofoDossier *dossier, ofoBase *object, ofaAccountsBook *self );
static void       on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaAccountsBook *self );
static void       on_updated_class_label( ofaAccountsBook *self, ofoClass *class );
static void       on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaAccountsBook *self );
static void       on_deleted_account( ofaAccountsBook *self, const ofoAccount *account );
static void       on_deleted_class_label( ofaAccountsBook *self, ofoClass *class );
static void       on_reloaded_dataset( ofoDossier *dossier, GType type, ofaAccountsBook *self );
static void       init_ui( ofaAccountsBook *self );
static void       setup_account_book( ofaAccountsBook *self );
static void       on_book_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaAccountsBook *self );
static gboolean   on_book_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaAccountsBook *self );
static gint       book_get_page_by_class( ofaAccountsBook *self, gint class_num, gboolean create, GtkTreeView **tview, GtkTreeModel **tmodel );
static gint       book_create_page( ofaAccountsBook *self, GtkNotebook *book, gint class, GtkWidget **page );
static void       book_expand_all_pages( ofaAccountsBook *self );
static gboolean   on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaAccountsBook *self );
static void       tview_collapse_node( ofaAccountsBook *self, GtkWidget *widget );
static void       tview_expand_node( ofaAccountsBook *self, GtkWidget *widget );
static void       on_tview_row_selected( GtkTreeSelection *selection, ofaAccountsBook *self );
static void       on_tview_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaAccountsBook *self );
static gint       on_tview_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaAccountsBook *self );
static void       on_tview_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaAccountsBook *self );
static void       setup_buttons( ofaAccountsBook *self );
static void       insert_dataset( ofaAccountsBook *self );
static void       tstore_insert_row( ofaAccountsBook *self, ofoAccount *account, gboolean with_selection );
static void       tstore_insert_row_in_page( ofaAccountsBook *self, ofoAccount *account, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void       tstore_set_row_by_iter( ofaAccountsBook *self, ofoAccount *account, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean   tstore_find_parent_iter( ofaAccountsBook *self, const ofoAccount *account, GtkTreeModel *tmodel, GtkTreeIter *parent_iter );
static void       tstore_realign_stored_children( ofaAccountsBook *self, const ofoAccount *account, GtkTreeModel *tmodel, GtkTreeIter *parent_iter );
static GList     *tstore_realign_stored_children_rec( const ofoAccount *account, GtkTreeModel *tmodel, GtkTreeIter *iter, GList *children );
static void       tstore_realign_stored_children_move( sChild *child_str, ofaAccountsBook *self );
static void       tstore_child_free( sChild *child_str );
static void       update_buttons_sensitivity( ofaAccountsBook *self, ofoAccount *account );
static void       select_row_by_iter( ofaAccountsBook *self, GtkTreeView *tview, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void       select_row_by_number( ofaAccountsBook *self, const gchar *number );
static gboolean   tstore_find_row_by_number( ofaAccountsBook *self, const gchar *number, GtkTreeModel *tmodel, GtkTreeIter *iter, gboolean *bvalid );
static gboolean   tstore_find_row_by_number_rec( ofaAccountsBook *self, const gchar *number, GtkTreeModel *tmodel, GtkTreeIter *iter, gint *last );
static void       remove_row_by_number( ofaAccountsBook *self, const gchar *number );
static void       set_row( ofaAccountsBook *self, ofoAccount *account, gboolean with_selection );
static void       on_button_clicked( GtkWidget *button, ofaAccountsBook *self );
static void       on_new_clicked( ofaAccountsBook *self );
static void       on_update_clicked( ofaAccountsBook *self );
static void       on_delete_clicked( ofaAccountsBook *self );
static void       try_to_delete_current_row( ofaAccountsBook *self );
static gboolean   delete_confirmed( ofaAccountsBook *self, ofoAccount *account );
static void       on_view_entries( GtkButton *button, ofaAccountsBook *self );

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
	gulong handler_id;
	GList *iha;

	g_return_if_fail( instance && OFA_IS_ACCOUNTS_BOOK( instance ));

	priv = ( OFA_ACCOUNTS_BOOK( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		/* note when deconnecting the handlers that the dossier may
		 * have been already finalized (e.g. when the application
		 * terminates) */
		if( OFO_IS_DOSSIER( priv->dossier )){
			for( iha=priv->handlers ; iha ; iha=iha->next ){
				handler_id = ( gulong ) iha->data;
				g_signal_handler_disconnect( priv->dossier, handler_id );
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
	self->priv->handlers = NULL;
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
ofa_accounts_book_new( ofsAccountsBookParms *parms  )
{
	static const gchar *thisfn = "ofa_accounts_book_new";
	ofaAccountsBook *self;
	ofaAccountsBookPrivate *priv;

	g_return_val_if_fail( parms, NULL );

	g_debug( "%s: parms=%p", thisfn, ( void * ) parms );

	g_return_val_if_fail( parms->main_window && OFA_IS_MAIN_WINDOW( parms->main_window ), NULL );

	self = g_object_new( OFA_TYPE_ACCOUNTS_BOOK, NULL );
	priv = self->priv;

	priv->main_window = parms->main_window;
	priv->dossier = ofa_main_window_get_dossier( parms->main_window );
	priv->parent = parms->parent;
	priv->has_import = parms->has_import;
	priv->has_export = parms->has_export;
	priv->has_view_entries = parms->has_view_entries;
	priv->pfnSelected = parms->pfnSelected;
	priv->pfnActivated = parms->pfnActivated;
	priv->pfnViewEntries = parms->pfnViewEntries;
	priv->user_data = parms->user_data;

	/* setup a weak reference on the parent container to auto-unref */
	g_object_weak_ref( G_OBJECT( priv->parent ), ( GWeakNotify ) on_parent_window_finalized, self );

	/* connect to the dossier in order to get advertised on updates */
	dossier_signals_connect( self );

	/* manage the UI */
	init_ui( self );

	return( self );
}

static void
on_parent_window_finalized( ofaAccountsBook *self, gpointer this_was_the_dialog )
{
	static const gchar *thisfn = "ofa_accounts_book_on_parent_window_finalized";

	g_return_if_fail( self && OFA_IS_ACCOUNTS_BOOK( self ));

	g_debug( "%s: self=%p, parent_dialog=%p",
			thisfn, ( void * ) self, ( void * ) this_was_the_dialog );

	g_object_unref( self );
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

	if( OFO_IS_ACCOUNT( object )){
		tstore_insert_row( self, OFO_ACCOUNT( object ), TRUE );

	} else if( OFO_IS_CLASS( object )){
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
	const gchar *acc_num;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn, ( void * ) dossier,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), prev_id, ( void * ) self );

	if( OFO_IS_ACCOUNT( object )){
		acc_num = ofo_account_get_number( OFO_ACCOUNT( object ));
		if( prev_id && g_utf8_collate( prev_id, acc_num )){
			remove_row_by_number( self, prev_id );
			tstore_insert_row( self, OFO_ACCOUNT( object ), TRUE );
		} else {
			set_row( self, OFO_ACCOUNT( object ), TRUE );
		}

	} else if( OFO_IS_CLASS( object )){
		on_updated_class_label( self, OFO_CLASS( object ));
	}
}

/*
 * a class label has changed : update the corresponding tab label
 */
static void
on_updated_class_label( ofaAccountsBook *self, ofoClass *class )
{
	gint page_n;
	GtkWidget *page_w;
	gint class_num;

	class_num = ofo_class_get_number( class );
	page_n = book_get_page_by_class( self, class_num, FALSE, NULL, NULL );
	if( page_n >= 0 ){
		page_w = gtk_notebook_get_nth_page( self->priv->book, page_n );
		g_return_if_fail( page_w && GTK_IS_WIDGET( page_w ));
		gtk_notebook_set_tab_label_text( self->priv->book, page_w, ofo_class_get_label( class ));
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

	if( OFO_IS_ACCOUNT( object )){
		on_deleted_account( self, OFO_ACCOUNT( object ));

	} else if( OFO_IS_CLASS( object )){
		on_deleted_class_label( self, OFO_CLASS( object ));
	}
}

static void
on_deleted_account( ofaAccountsBook *self, const ofoAccount *account )
{
	ofaAccountsBookPrivate *priv;
	GList *children, *it;

	priv = self->priv;

	children = ofo_account_get_children( account, priv->dossier );
	remove_row_by_number( self, ofo_account_get_number( account ));
	for( it=children ; it ; it=it->next ){
		remove_row_by_number( self, ofo_account_get_number( OFO_ACCOUNT( it->data )));
	}
	 if( !ofa_prefs_account_delete_root_with_children()){
			for( it=children ; it ; it=it->next ){
				tstore_insert_row( self, OFO_ACCOUNT( it->data ), FALSE );
			}
	 }
	 g_list_free( children );
}

static void
on_deleted_class_label( ofaAccountsBook *self, ofoClass *class )
{
	gint page_n;
	GtkWidget *page_w;
	gint class_num;

	class_num = ofo_class_get_number( class );
	page_n = book_get_page_by_class( self, class_num, FALSE, NULL, NULL );
	if( page_n >= 0 ){
		page_w = gtk_notebook_get_nth_page( self->priv->book, page_n );
		g_return_if_fail( page_w && GTK_IS_WIDGET( page_w ));
		gtk_notebook_set_tab_label_text( self->priv->book, page_w, st_class_labels[class_num-1] );
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

	if( type == OFO_TYPE_ACCOUNT ){
		while( gtk_notebook_get_n_pages( self->priv->book )){
			gtk_notebook_remove_page( self->priv->book, 0 );
		}
		ofa_accounts_book_init_view( self, NULL );
	}
}

static void
init_ui( ofaAccountsBook *self )
{
	ofaAccountsBookPrivate *priv;

	priv = self->priv;

	priv->grid = GTK_GRID( gtk_grid_new());
	gtk_grid_set_column_spacing( priv->grid, 4 );
	gtk_container_add( priv->parent, GTK_WIDGET( priv->grid ));

	/* setup and connect the notebook */
	setup_account_book( self );
	gtk_grid_attach( priv->grid, GTK_WIDGET( priv->book ), 0, 0, 1, 1 );

	/* setup and connect the buttons */
	setup_buttons( self );
	gtk_grid_attach( priv->grid, GTK_WIDGET( priv->buttons_box ), 1, 0, 1, 1 );
}

static void
setup_account_book( ofaAccountsBook *self )
{
	ofaAccountsBookPrivate *priv;

	priv = self->priv;

	priv->book = GTK_NOTEBOOK( gtk_notebook_new());
	gtk_widget_set_margin_left( GTK_WIDGET( priv->book ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( priv->book ), 4 );
	gtk_notebook_popup_enable( priv->book );

	g_signal_connect(
			G_OBJECT( priv->book ),
			"switch-page", G_CALLBACK( on_book_page_switched ), self );

	g_signal_connect(
			G_OBJECT( priv->book ),
			"key-press-event", G_CALLBACK( on_book_key_pressed ), self );
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
	gboolean stop;
	gint class_num, page_num;

	stop = FALSE;

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
			page_num = book_get_page_by_class( self, class_num, FALSE, NULL, NULL );
			if( page_num >= 0 ){
				gtk_notebook_set_current_page( self->priv->book, page_num );
				stop = TRUE;
			}
		}
	}

	return( stop );
}

/*
 * Returns the number of the notebook's page which is dedicated to the
 * given class number.
 *
 * If the page doesn't exist, and @bcreate is %TRUE, then it is created.
 *
 * If tview/tmodel pointers are provided, the values are set.
 */
static gint
book_get_page_by_class( ofaAccountsBook *self,
								gint class_num, gboolean bcreate,
								GtkTreeView **tview, GtkTreeModel **tmodel )
{
	static const gchar *thisfn = "ofa_accounts_book_book_get_page_by_class";
	gint count, i, found;
	GtkWidget *page_widget;
	gint page_class;
	GtkWidget *my_tview;

	if( !ofo_class_is_valid_number( class_num )){
		g_warning( "%s: invalid class number: %d", thisfn, class_num );
		return( -1 );
	}

	found = -1;
	count = gtk_notebook_get_n_pages( self->priv->book );

	/* search for an existing page */
	for( i=0 ; i<count ; ++i ){
		page_widget = gtk_notebook_get_nth_page( self->priv->book, i );
		page_class = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( page_widget ), DATA_PAGE_CLASS ));
		if( page_class == class_num ){
			found = i;
			break;
		}
	}

	/* if not exists, create it (if allowed) */
	if( found == -1 && bcreate ){
		found = book_create_page( self, self->priv->book, class_num, &page_widget );
		if( found == -1 ){
			g_warning( "%s: unable to create the page for class %d", thisfn, class_num );
			return( -1 );
		}
	}

	/* retrieve tree informations */
	if( found >= 0 ){
		g_return_val_if_fail( page_widget && GTK_IS_SCROLLED_WINDOW( page_widget ), -1 );

		if( tview || tmodel ){
			my_tview = my_utils_container_get_child_by_type(
								GTK_CONTAINER( page_widget ), GTK_TYPE_TREE_VIEW );
			g_return_val_if_fail( my_tview && GTK_IS_TREE_VIEW( my_tview ), -1 );

			if( tview ){
				*tview = GTK_TREE_VIEW( my_tview );
			}
			if( tmodel ){
				*tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( my_tview ));
			}
		}
	}

	return( found );
}

/*
 * returns the page number
 * setting the page widget if a variable pointer is provided
 */
static gint
book_create_page( ofaAccountsBook *self, GtkNotebook *book, gint class, GtkWidget **page )
{
	static const gchar *thisfn = "ofa_accounts_book_book_create_page";
	gint page_num;
	GtkScrolledWindow *scroll;
	GtkWidget *label;
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	gchar *str;
	ofoClass *obj_class;
	const gchar *obj_label;

	g_debug( "%s: self=%p, book=%p, class=%d, page=%p",
			thisfn, ( void * ) self, ( void * ) book, class, ( void * ) page );

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );

	obj_class = ofo_class_get_by_number( self->priv->dossier, class );
	if( obj_class && OFO_IS_CLASS( obj_class )){
		obj_label = ofo_class_get_label( obj_class );
	} else {
		obj_label = st_class_labels[class-1];
	}
	label = gtk_label_new( obj_label );
	str = g_strdup_printf( "Alt-%d", class );
	gtk_widget_set_tooltip_text( label, str );
	g_free( str );
	page_num = gtk_notebook_append_page( book, GTK_WIDGET( scroll ), label );
	if( page_num == -1 ){
		return( -1 );
	}
	gtk_notebook_set_tab_reorderable( book, GTK_WIDGET( scroll ), TRUE );
	g_object_set_data( G_OBJECT( scroll ), DATA_PAGE_CLASS, GINT_TO_POINTER( class ));

	view = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_hexpand( GTK_WIDGET( view ), TRUE );
	gtk_widget_set_vexpand( GTK_WIDGET( view ), TRUE );
	gtk_tree_view_set_headers_visible( view, TRUE );
	g_signal_connect(
			G_OBJECT( view ), "row-activated", G_CALLBACK( on_tview_row_activated), self );
	g_signal_connect(
			G_OBJECT( view ), "key-press-event", G_CALLBACK( on_tview_key_pressed ), self );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( view ));

	model = GTK_TREE_MODEL( gtk_tree_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( view, model );
	g_object_unref( model );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Number" ),
			text_cell, "text", COL_NUMBER,
			NULL );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( COL_NUMBER ));
	gtk_tree_view_append_column( view, column );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_tview_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", COL_LABEL,
			NULL );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( COL_LABEL ));
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( view, column );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_tview_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new();
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( COL_DEBIT ));
	gtk_tree_view_column_pack_end( column, text_cell, TRUE );
	gtk_tree_view_column_set_title( column, _( "Debit" ));
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_add_attribute( column, text_cell, "text", COL_DEBIT );
	gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( view, column );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_tview_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new();
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( COL_CREDIT ));
	gtk_tree_view_column_pack_end( column, text_cell, TRUE );
	gtk_tree_view_column_set_title( column, _( "Credit" ));
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_add_attribute( column, text_cell, "text", COL_CREDIT );
	gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( view, column );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_tview_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 0.0, 0.5 );
	column = gtk_tree_view_column_new();
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( COL_CURRENCY ));
	gtk_tree_view_column_pack_end( column, text_cell, FALSE );
	gtk_tree_view_column_set_alignment( column, 0.0 );
	gtk_tree_view_column_add_attribute( column, text_cell, "text", COL_CURRENCY );
	gtk_tree_view_column_set_min_width( column, 40 );
	gtk_tree_view_append_column( view, column );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_tview_cell_data_func, self, NULL );

	select = gtk_tree_view_get_selection( view );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect(
			G_OBJECT( select ), "changed", G_CALLBACK( on_tview_row_selected ), self );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( model ), ( GtkTreeIterCompareFunc ) on_tview_sort_model, self, NULL );
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( model ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	gtk_widget_show_all( GTK_WIDGET( scroll ));

	if( page ){
		*page = GTK_WIDGET( scroll );
	}

	return( page_num );
}

/*
 * this is called after having inserted the dataset
 * and only if not account is set to be selected
 */
static void
book_expand_all_pages( ofaAccountsBook *self )
{
	gint pages_count, i;
	GtkWidget *page_w;
	GtkTreeView *tview;

	pages_count = gtk_notebook_get_n_pages( self->priv->book );
	for( i=0 ; i<pages_count ; ++i ){
		page_w = gtk_notebook_get_nth_page( self->priv->book, i );
		g_return_if_fail( page_w && GTK_IS_WIDGET( page_w ));
		tview = ( GtkTreeView * ) my_utils_container_get_child_by_type( GTK_CONTAINER( page_w ), GTK_TYPE_TREE_VIEW );
		g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));
		gtk_tree_view_expand_all( tview );
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
			on_new_clicked( self );

		} else if( event->keyval == GDK_KEY_Delete ){
			try_to_delete_current_row( self );
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
on_tview_row_selected( GtkTreeSelection *selection, ofaAccountsBook *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoAccount *account;

	account = NULL;

	/* selection may be null when called from on_delete_clicked() */
	if( selection ){
		if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
			gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &account, -1 );
			g_object_unref( account );
		}
	}

	update_buttons_sensitivity( self, account );

	if( self->priv->pfnSelected ){
		( *self->priv->pfnSelected )( account, self->priv->user_data );
	}
}

static void
on_tview_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaAccountsBook *self )
{
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoAccount *account;

	account = NULL;

	select = gtk_tree_view_get_selection( tview );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &account, -1 );
		g_object_unref( account );
	}

	if( self->priv->pfnActivated ){
		( *self->priv->pfnActivated)( account, self->priv->user_data );
	}
}

/*
 * sorting the treeview per account number
 */
static gint
on_tview_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaAccountsBook *self )
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
 *
 * detail accounts who have no currency are red written.
 */
static void
on_tview_cell_data_func( GtkTreeViewColumn *tcolumn,
						GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
						ofaAccountsBook *self )
{
	ofoAccount *account;
	GString *number;
	gint level;
	gint column;
	GdkRGBA color;
	ofoCurrency *currency;

	g_return_if_fail( GTK_IS_CELL_RENDERER_TEXT( cell ));

	gtk_tree_model_get( tmodel, iter, COL_OBJECT, &account, -1 );
	g_object_unref( account );
	level = ofo_account_get_level_from_number( ofo_account_get_number( account ));
	g_return_if_fail( level >= 2 );

	column = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( tcolumn ), DATA_COLUMN_ID ));
	if( column == COL_NUMBER ){
		number = g_string_new( " " );
		g_string_append_printf( number, "%s", ofo_account_get_number( account ));
		g_object_set( G_OBJECT( cell ), "text", number->str, NULL );
		g_string_free( number, TRUE );
	}

	g_object_set( G_OBJECT( cell ),
						"style-set",      FALSE,
						"weight-set",     FALSE,
						"foreground-set", FALSE,
						"background-set", FALSE,
						NULL );

	if( ofo_account_is_root( account )){

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
		currency = ofo_currency_get_by_code( self->priv->dossier, ofo_account_get_currency( account ));
		if( !currency ){
			gdk_rgba_parse( &color, "#800000" );
			g_object_set( G_OBJECT( cell ), "foreground-rgba", &color, NULL );
		}
	}
}

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

/*
 * insert the accounts from the dataset, creating each new class page
 * as needed
 */
static void
insert_dataset( ofaAccountsBook *self )
{
	GList *chart, *ic;

	chart = ofo_account_get_dataset( self->priv->dossier );

	for( ic=chart ; ic ; ic=ic->next ){
		tstore_insert_row( self, OFO_ACCOUNT( ic->data ), FALSE );
	}
}

/*
 * insert a new row in the ad-hoc page of the notebook, creating the
 * page as needed
 */
static void
tstore_insert_row( ofaAccountsBook *self, ofoAccount *account, gboolean with_selection )
{
	/*static const gchar *thisfn = "ofa_accounts_book_tstore_insert_row";*/
	gint page_num;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter, parent_iter;
	GtkTreePath *path;

	page_num = book_get_page_by_class(
						self, ofo_account_get_class( account ), TRUE, &tview, &tmodel );

	if( page_num >= 0 ){
		tstore_insert_row_in_page( self, account, tmodel, &iter );

		if( with_selection ){
			if( gtk_tree_model_iter_parent( tmodel, &parent_iter, &iter )){
				path = gtk_tree_model_get_path( tmodel, &parent_iter );
			} else {
				path = gtk_tree_model_get_path( tmodel, &iter );
			}
			gtk_tree_view_expand_row( tview, path, TRUE );
			gtk_tree_path_free( path );

			gtk_notebook_set_current_page( self->priv->book, page_num );
			select_row_by_iter( self, tview, tmodel, &iter );
		}
	}
}

/*
 * insert a new row in the current page
 * (that we already have checked is the ad-hoc page)
 */
static void
tstore_insert_row_in_page( ofaAccountsBook *self, ofoAccount *account,
									GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	GtkTreeIter parent_iter;
	gboolean parent_found;

	parent_found = tstore_find_parent_iter( self, account, tmodel, &parent_iter );
	gtk_tree_store_insert_with_values(
			GTK_TREE_STORE( tmodel ),
			iter,
			parent_found ? &parent_iter : NULL,
			-1,
			COL_NUMBER,   ofo_account_get_number( account ),
			COL_OBJECT,   account,
			-1 );

	tstore_set_row_by_iter( self, account, tmodel, iter );
	tstore_realign_stored_children( self, account, tmodel, iter );
}

/*
 * update the tree store with the new properties of an account
 * this function must only be used if the item already exists in the
 * tree store (not a new row), and the number has not been changed
 */
static void
tstore_set_row_by_iter( ofaAccountsBook *self,
						ofoAccount *account, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gchar *sdeb, *scre;
	ofoCurrency *currency;
	gchar *cdev;

	if( ofo_account_is_root( account )){
		sdeb = g_strdup( "" );
		scre = g_strdup( "" );
		cdev = g_strdup( "" );

	} else {
		sdeb = my_double_to_str(
				ofo_account_get_deb_amount( account )+ofo_account_get_day_deb_amount( account ));
		scre = my_double_to_str(
				ofo_account_get_cre_amount( account )+ofo_account_get_day_cre_amount( account ));
		currency =
				ofo_currency_get_by_code( self->priv->dossier, ofo_account_get_currency( account ));
		if( currency ){
			cdev = g_strdup( ofo_currency_get_code( currency ));
		} else {
			cdev = g_strdup( "" );
		}
	}

	gtk_tree_store_set(
			GTK_TREE_STORE( tmodel ),
			iter,
			COL_LABEL,    ofo_account_get_label( account ),
			COL_DEBIT,    sdeb,
			COL_CREDIT,   scre,
			COL_CURRENCY, cdev,
			-1 );

	g_free( sdeb );
	g_free( scre );
	g_free( cdev );
}

/*
 * @self: this #ofaAccountsBook
 * @account: [in]: the account for which we are searching the closest
 *  parent
 * @tmodel: [in]: the GtkTreeModel.
 * @parent_iter: [out]: the GtkTreeIter of the closes parent if found.
 *
 *
 * Search for the GtkTreeIter corresponding to the closest parent of
 * this account
 *
 * Returns %TRUE if a parent has been found, and @parent_iter adresses
 * this parent, else @parent_iter is undefined.
 */
static gboolean
tstore_find_parent_iter( ofaAccountsBook *self, const ofoAccount *account,
								GtkTreeModel *tmodel, GtkTreeIter *parent_iter )
{
	const gchar *number;
	gchar *candidate_number;
	gboolean found;

	number = ofo_account_get_number( account );
	candidate_number = g_strdup( number );
	found = FALSE;

	while( !found && g_utf8_strlen( candidate_number, -1 ) > 1 ){
		candidate_number[g_utf8_strlen( candidate_number, -1 )-1] = '\0';
		found = tstore_find_row_by_number( self, candidate_number, tmodel, parent_iter, NULL );
	}

	g_free( candidate_number );

	return( found );
}

/*
 * tstore_find_row_by_number:
 * @self: this #ofaAccountsBook
 * @number: [in]: the account number we are searching for.
 * @tmodel: [in]: the GtkTreeModel.
 * @iter: [out]: the last iter equal or immediately greater than the
 *  searched value.
 * @bvalid: [allow-none][out]: set to TRUE if the returned iter is
 *  valid.
 *
 * rows are sorted by account number
 * we exit the search as soon as we get a number greater than the
 * searched one
 *
 * Returns TRUE if we have found an exact match, and @iter addresses
 * this exact match.
 *
 * Returns FALSE if we do not have found a match, and @iter addresses
 * the first number greater than the searched value.
 */
static gboolean
tstore_find_row_by_number( ofaAccountsBook *self, const gchar *number,
							GtkTreeModel *tmodel, GtkTreeIter *iter, gboolean *bvalid )
{
	gint last;
	gboolean found;

	if( bvalid ){
		*bvalid = FALSE;
	}

	if( gtk_tree_model_get_iter_first( tmodel, iter )){
		if( bvalid ){
			*bvalid = TRUE;
		}
		last = -1;
		found = tstore_find_row_by_number_rec( self, number, tmodel, iter, &last );
		return( found );
	}

	return( FALSE );
}

/*
 * enter here with a valid @iter
 * start by compare this @iter with the searched value
 * returns TRUE (and exit the recursion) if the @iter is equal to
 *   the searched value
 * returns FALSE (and exit the recursion) if the @iter is greater than
 *   the searched value
 * else if have children
 *   get the first child and recurse
 * else get next iter
 *
 * when exiting this function, iter is set to the last number which is
 * less or equal to the searched value
 */
static gboolean
tstore_find_row_by_number_rec( ofaAccountsBook *self, const gchar *number,
									GtkTreeModel *tmodel, GtkTreeIter *iter, gint *last )
{
	gchar *cmp_number;
	GtkTreeIter cmp_iter, child_iter, last_iter;

	cmp_iter = *iter;

	while( TRUE ){
		gtk_tree_model_get( tmodel, &cmp_iter, COL_NUMBER, &cmp_number, -1 );
		*last = g_utf8_collate( cmp_number, number );
		g_free( cmp_number );
		if( *last == 0 ){
			*iter = cmp_iter;
			return( TRUE );
		}
		if( *last > 0 ){
			*iter = cmp_iter;
			return( FALSE );
		}
		if( gtk_tree_model_iter_children( tmodel, &child_iter, &cmp_iter )){
			tstore_find_row_by_number_rec( self, number, tmodel, &child_iter, last );
			if( *last == 0 ){
				*iter = child_iter;
				return( TRUE );
			}
			if( *last > 0 ){
				*iter = child_iter;
				return( FALSE );
			}
		}
		last_iter = cmp_iter;
		if( !gtk_tree_model_iter_next( tmodel, &cmp_iter )){
			break;
		}
	}

	*iter = last_iter;
	return( FALSE );
}

/*
 * the @account has just come inserted at @parent_iter
 * its possible children have to be reinserted under it
 * when entering here, @parent_iter should not have yet any child iter
 */
static void
tstore_realign_stored_children( ofaAccountsBook *self, const ofoAccount *account, GtkTreeModel *tmodel, GtkTreeIter *parent_iter )
{
	static const gchar *thisfn = "ofa_accounts_book_tstore_realign_stored_children";
	GList *children;
	GtkTreeIter iter;

	if( gtk_tree_model_iter_has_child( tmodel, parent_iter )){
		g_warning( "%s: newly inserted row already has at least one child", thisfn );

	} else {
		children = NULL;
		iter = *parent_iter;
		children = tstore_realign_stored_children_rec( account, tmodel, &iter, children );
		if( children ){
			g_list_foreach( children, ( GFunc ) tstore_realign_stored_children_move, self );
			g_list_free_full( children, ( GDestroyNotify ) tstore_child_free );
		}
	}
}

/*
 * tstore_realign_stored_children_rec:
 * copy into 'children' GList all children accounts, along with their
 * row reference - it will so be easy to remove them from the model,
 * then reinsert these same accounts
 */
static GList *
tstore_realign_stored_children_rec( const ofoAccount *account, GtkTreeModel *tmodel, GtkTreeIter *iter, GList *children )
{
	ofoAccount *candidate;
	GtkTreeIter child_iter;
	GtkTreePath *path;
	sChild *child_str;

	while( gtk_tree_model_iter_next( tmodel, iter )){
		gtk_tree_model_get( tmodel, iter, COL_OBJECT, &candidate, -1 );
		g_object_unref( candidate );

		if( ofo_account_is_child_of( account, candidate )){
			child_str = g_new0( sChild, 1 );
			path = gtk_tree_model_get_path( tmodel, iter );
			child_str->account = g_object_ref( candidate );
			child_str->tmodel = tmodel;
			child_str->ref = gtk_tree_row_reference_new( tmodel, path );
			children = g_list_prepend( children, child_str );
			gtk_tree_path_free( path );

			if( gtk_tree_model_iter_children( tmodel, &child_iter, iter )){
				children = tstore_realign_stored_children_rec( account, tmodel, &child_iter, children );
			}

		} else {
			break;
		}
	}

	return( children );
}

static void
tstore_realign_stored_children_move( sChild *child_str, ofaAccountsBook *self )
{
	GtkTreePath *path;
	GtkTreeIter iter;

	path = gtk_tree_row_reference_get_path( child_str->ref );
	if( path && gtk_tree_model_get_iter( child_str->tmodel, &iter, path )){
		gtk_tree_store_remove( GTK_TREE_STORE( child_str->tmodel ), &iter );
		gtk_tree_path_free( path );
		tstore_insert_row( self, child_str->account, FALSE );
	}
}

static void
tstore_child_free( sChild *child_str )
{
	g_object_unref( child_str->account );
	gtk_tree_row_reference_free( child_str->ref );
	g_free( child_str );
}

static void
update_buttons_sensitivity( ofaAccountsBook *self, ofoAccount *account )
{
	ofaAccountsBookPrivate *priv;

	priv = self->priv;

	gtk_widget_set_sensitive(
			priv->btn_update,
			account && OFO_IS_ACCOUNT( account ));

	gtk_widget_set_sensitive(
			priv->btn_delete,
			account && OFO_IS_ACCOUNT( account ) &&
				ofo_account_is_deletable( account, priv->dossier ));

	if( self->priv->btn_consult ){
		gtk_widget_set_sensitive(
				priv->btn_consult,
				account && OFO_IS_ACCOUNT( account ) &&
					!ofo_account_is_root( account ));
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

/*
 * select the row with the given number, or the most close one
 * doesn't create the page class if it doesn't yet exist
 */
static void
select_row_by_number( ofaAccountsBook *self, const gchar *number )
{
	gint page_num;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;
	gboolean bvalid;

	if( number && g_utf8_strlen( number, -1 )){
		page_num = book_get_page_by_class( self,
							ofo_account_get_class_from_number( number ), FALSE, &tview, &tmodel );
		if( page_num > 0 ){
			gtk_notebook_set_current_page( self->priv->book, page_num );
			tstore_find_row_by_number( self, number, tmodel, &iter, &bvalid );
			if( bvalid ){
				path = gtk_tree_model_get_path( tmodel, &iter );
				gtk_tree_view_expand_to_path( tview, path );
				gtk_tree_path_free( path );
				select_row_by_iter( self, tview, tmodel, &iter );
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

/*
 * update the tree store with the new properties of an account
 * this function must only be used if the item already exists in the
 * list store (not a new row), and the number has not been changed
 */
static void
set_row( ofaAccountsBook *self, ofoAccount *account, gboolean with_selection )
{
	gint page_num;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	page_num = book_get_page_by_class( self,
						ofo_account_get_class( account ), FALSE, &tview, &tmodel );

	if( page_num >= 0 &&
			tstore_find_row_by_number( self,
					ofo_account_get_number( account ), tmodel, &iter, NULL )){

		tstore_set_row_by_iter( self, account, tmodel, &iter );

		if( with_selection ){
			select_row_by_iter( self, tview, tmodel, &iter );
		}
	}
}

/**
 * ofa_accounts_book_get_selected:
 */
ofoAccount *
ofa_accounts_book_get_selected( ofaAccountsBook *self )
{
	gint page_n;
	GtkWidget *page_w;
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoAccount *account;

	g_return_val_if_fail( self && OFA_IS_ACCOUNTS_BOOK( self ), NULL );

	account = NULL;

	if( !self->priv->dispose_has_run ){

		page_n = gtk_notebook_get_current_page( self->priv->book );
		g_return_val_if_fail( page_n >= 0, NULL );

		page_w = gtk_notebook_get_nth_page( self->priv->book, page_n );
		g_return_val_if_fail( page_w && GTK_IS_CONTAINER( page_w ), NULL );

		tview = ( GtkTreeView * )
						my_utils_container_get_child_by_type(
								GTK_CONTAINER( page_w ), GTK_TYPE_TREE_VIEW );
		g_return_val_if_fail( tview && GTK_IS_TREE_VIEW( tview ), NULL );

		select = gtk_tree_view_get_selection( tview );
		if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
			gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &account, -1 );
			g_object_unref( account );
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
ofa_accounts_book_set_selected( ofaAccountsBook *self, const gchar *number )
{
	g_return_if_fail( self && OFA_IS_ACCOUNTS_BOOK( self ));

	if( !self->priv->dispose_has_run ){

		select_row_by_number( self, number );
	}
}

/**
 * ofa_accounts_book_get_top_focusable_widget:
 *
 * Returns the top focusable widget, here the treeview of the current
 * page.
 */
GtkWidget *
ofa_accounts_book_get_top_focusable_widget( const ofaAccountsBook *self )
{
	gint page_n;
	GtkWidget *page_w;
	GtkWidget *tview;

	g_return_val_if_fail( self && OFA_IS_ACCOUNTS_BOOK( self ), NULL );

	if( !self->priv->dispose_has_run ){

		page_n = gtk_notebook_get_current_page( self->priv->book );
		g_return_val_if_fail( page_n >= 0, NULL );

		page_w = gtk_notebook_get_nth_page( self->priv->book, page_n );
		g_return_val_if_fail( page_w && GTK_IS_CONTAINER( page_w ), NULL );

		tview = my_utils_container_get_child_by_type(
								GTK_CONTAINER( page_w ), GTK_TYPE_TREE_VIEW );
		g_return_val_if_fail( tview && GTK_IS_TREE_VIEW( tview ), NULL );

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

/*
 * this is the 'clicked' message handler for default buttons
 */
static void
on_button_clicked( GtkWidget *button, ofaAccountsBook *self )
{
	static const gchar *thisfn = "ofa_accounts_book_on_button_clicked";
	guint id;

	id = my_buttons_box_get_button_id( self->priv->buttons_box, button );

	switch( id ){
		case BUTTONS_BOX_NEW:
			on_new_clicked( self );
			break;
		case BUTTONS_BOX_PROPERTIES:
			on_update_clicked( self );
			break;
		case BUTTONS_BOX_DELETE:
			on_delete_clicked( self );
			break;
		default:
			g_warning( "%s: %u: unknown button identifier", thisfn, id );
			break;
	}
}

static void
on_new_clicked( ofaAccountsBook *self )
{
	ofoAccount *account;

	account = ofo_account_new();

	if( !ofa_account_properties_run( self->priv->main_window, account )){
		g_object_unref( account );
	}
}

static void
on_update_clicked( ofaAccountsBook *self )
{
	ofoAccount *account;
	GtkWidget *tview;

	account = ofa_accounts_book_get_selected( self );
	if( account ){
		g_return_if_fail( OFO_IS_ACCOUNT( account ));
		ofa_account_properties_run( self->priv->main_window, account );
	}

	tview = ofa_accounts_book_get_top_focusable_widget( self );
	if( tview ){
		gtk_widget_grab_focus( tview );
	}
}

static void
on_delete_clicked( ofaAccountsBook *self )
{
	ofaAccountsBookPrivate *priv;
	ofoAccount *account;
	gchar *number;
	GtkWidget *tview;

	priv = self->priv;

	account = ofa_accounts_book_get_selected( self );
	if( account ){
		g_return_if_fail( ofo_account_is_deletable( account, priv->dossier ));

		number = g_strdup( ofo_account_get_number( account ));

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

		g_free( number );
	}

	tview = ofa_accounts_book_get_top_focusable_widget( self );
	if( tview ){
		gtk_widget_grab_focus( tview );
	}
}

static void
try_to_delete_current_row( ofaAccountsBook *self )
{
	ofaAccountsBookPrivate *priv;
	ofoAccount *account;

	priv = self->priv;

	account = ofa_accounts_book_get_selected( self );
	if( ofo_account_is_deletable( account, priv->dossier )){
		on_delete_clicked( self );
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

	delete_ok = ofa_main_window_confirm_deletion( self->priv->main_window, msg );

	g_free( msg );

	return( delete_ok );
}

static void
on_view_entries( GtkButton *button, ofaAccountsBook *self )
{
	ofoAccount *account;

	account = ofa_accounts_book_get_selected( self );

	if( self->priv->pfnViewEntries ){
		( *self->priv->pfnViewEntries )( account, self->priv->user_data );
	}
}
