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

#include "api/ofo-account.h"
#include "api/ofo-class.h"
#include "api/ofo-devise.h"
#include "api/ofo-dossier.h"

#include "core/my-utils.h"

#include "ui/ofa-account-notebook.h"
#include "ui/ofa-account-properties.h"
#include "ui/ofa-main-page.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaAccountNotebookPrivate {
	gboolean             dispose_has_run;

	/* input data
	 */
	ofaMainWindow       *main_window;
	ofoDossier          *dossier;
	GtkContainer        *parent;
	gboolean             has_import;
	gboolean             has_export;
	gboolean             has_view_entries;
	ofaAccountNotebookCb pfnSelected;
	ofaAccountNotebookCb pfnActivated;
	ofaAccountNotebookCb pfnViewEntries;
	gpointer             user_data;

	/* internals
	 */
	GList               *handlers;

	/* UI
	 */
	GtkGrid             *top_grid;
	GtkNotebook         *book;
	GtkButton           *btn_update;
	GtkButton           *btn_delete;
	GtkButton           *btn_consult;
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

/* there are only default labels in the case where we were not able to
 * get the correct #ofoClass object
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
#define DATA_PAGE_CLASS                 "ofa-data-page-class"
#define DATA_COLUMN_ID                  "ofa-data-column-id"

static const gchar *st_ui_xml = PKGUIDIR "/ofa-account-notebook.ui";
static const gchar *st_ui_id  = "AccountNotebookWindow";

G_DEFINE_TYPE( ofaAccountNotebook, ofa_account_notebook, G_TYPE_OBJECT )

static void       on_parent_window_finalized( ofaAccountNotebook *self, gpointer this_was_the_dialog );
static void       dossier_signals_connect( ofaAccountNotebook *self );
static void       on_new_object( ofoDossier *dossier, ofoBase *object, ofaAccountNotebook *self );
static void       on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaAccountNotebook *self );
static void       on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaAccountNotebook *self );
static void       on_reloaded_dataset( ofoDossier *dossier, GType type, ofaAccountNotebook *self );
static void       init_ui( ofaAccountNotebook *self );
static void       reparent_from_window( ofaAccountNotebook *self );
static void       setup_account_book( ofaAccountNotebook *self );
static void       setup_buttons( ofaAccountNotebook *self );
static void       on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaAccountNotebook *self );
static gboolean   on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, ofaAccountNotebook *self );
static void       insert_dataset( ofaAccountNotebook *self );
static void       insert_row( ofaAccountNotebook *self, ofoAccount *account, gboolean with_selection );
static gint       book_get_page_by_class( ofaAccountNotebook *self, gint class_num, gboolean create, GtkTreeView **tview, GtkTreeModel **tmodel );
static gint       book_create_page( ofaAccountNotebook *self, GtkNotebook *book, gint class, GtkWidget **page );
static void       on_row_selected( GtkTreeSelection *selection, ofaAccountNotebook *self );
static void       on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaAccountNotebook *self );
static gint       on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaAccountNotebook *self );
static void       on_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaAccountNotebook *self );
static void       update_buttons_sensitivity( ofaAccountNotebook *self, ofoAccount *account );
static void       set_row_by_iter( ofaAccountNotebook *self, ofoAccount *account, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void       select_row_by_iter( ofaAccountNotebook *self, GtkTreeView *tview, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void       select_row_by_number( ofaAccountNotebook *self, const gchar *number );
static gboolean   find_row_by_number( ofaAccountNotebook *self, const gchar *number, GtkTreeModel *tmodel, GtkTreeIter *iter, gboolean *bvalid );
static void       remove_row_by_number( ofaAccountNotebook *self, const gchar *number );
static void       set_row( ofaAccountNotebook *self, ofoAccount *account, gboolean with_selection );
static gboolean   book_activate_page_by_class( ofaAccountNotebook *self, gint class_num );
static void       on_updated_class_label( ofaAccountNotebook *self, ofoClass *class );
static void       on_deleted_class_label( ofaAccountNotebook *self, ofoClass *class );
static void       on_new_clicked( GtkButton *button, ofaAccountNotebook *self );
static void       on_update_clicked( GtkButton *button, ofaAccountNotebook *self );
static void       do_update_with_account( ofaAccountNotebook *self, ofoAccount *account );
static void       on_delete_clicked( GtkButton *button, ofaAccountNotebook *self );
static gboolean   delete_confirmed( ofaAccountNotebook *self, ofoAccount *account );
static void       on_view_entries( GtkButton *button, ofaAccountNotebook *self );

static void
account_notebook_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_notebook_finalize";
	ofaAccountNotebookPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_NOTEBOOK( instance ));

	priv = OFA_ACCOUNT_NOTEBOOK( instance )->private;

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_notebook_parent_class )->finalize( instance );
}

static void
account_notebook_dispose( GObject *instance )
{
	ofaAccountNotebookPrivate *priv;
	gulong handler_id;
	GList *iha;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_NOTEBOOK( instance ));

	priv = ( OFA_ACCOUNT_NOTEBOOK( instance ))->private;

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
	G_OBJECT_CLASS( ofa_account_notebook_parent_class )->dispose( instance );
}

static void
ofa_account_notebook_init( ofaAccountNotebook *self )
{
	static const gchar *thisfn = "ofa_account_notebook_init";

	g_return_if_fail( self && OFA_IS_ACCOUNT_NOTEBOOK( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaAccountNotebookPrivate, 1 );

	self->private->dispose_has_run = FALSE;
	self->private->handlers = NULL;
}

static void
ofa_account_notebook_class_init( ofaAccountNotebookClass *klass )
{
	static const gchar *thisfn = "ofa_account_notebook_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_notebook_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_notebook_finalize;
}

/**
 * ofa_account_notebook_new:
 *
 * Creates the structured content, i.e. one notebook with one page per
 * account class.
 * The notebook must be created and provided by the parent dialog.
 *
 * Does NOT insert the data (see: ofa_account_notebook_init_view).
 *
 * +-----------------------------------------------------------------------+
 * | grid (this is the grid main page)                                     |
 * | +-----------------------------------------------+-------------------+ |
 * | | book                                          |                   | |
 * | |  (provided by the parent dialog)              |                   | |
 * | |                                               |                   | |
 * | | each page of the book contains accounts for   |                   | |
 * | | the corresponding class (if any)              |                   | |
 * | +-----------------------------------------------+-------------------+ |
 * +-----------------------------------------------------------------------+
 */
ofaAccountNotebook *
ofa_account_notebook_new( ofaAccountNotebookParms *parms  )
{
	static const gchar *thisfn = "ofa_account_notebook_new";
	ofaAccountNotebook *self;
	ofaAccountNotebookPrivate *priv;

	g_return_val_if_fail( parms, NULL );

	g_debug( "%s: parms=%p", thisfn, ( void * ) parms );

	g_return_val_if_fail( parms->main_window && OFA_IS_MAIN_WINDOW( parms->main_window ), NULL );

	self = g_object_new( OFA_TYPE_ACCOUNT_NOTEBOOK, NULL );
	priv = self->private;

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

	/* setup a weak reference on the dialog to auto-unref */
	g_object_weak_ref( G_OBJECT( priv->parent ), ( GWeakNotify ) on_parent_window_finalized, self );

	/* connect to the dossier in order to get advertised on updates */
	dossier_signals_connect( self );

	/* manage the UI */
	init_ui( self );

	return( self );
}

static void
on_parent_window_finalized( ofaAccountNotebook *self, gpointer this_was_the_dialog )
{
	static const gchar *thisfn = "ofa_account_notebook_on_parent_window_finalized";

	g_return_if_fail( self && OFA_IS_ACCOUNT_NOTEBOOK( self ));

	g_debug( "%s: self=%p, parent_dialog=%p",
			thisfn, ( void * ) self, ( void * ) this_was_the_dialog );

	g_object_unref( self );
}

static void
dossier_signals_connect( ofaAccountNotebook *self )
{
	ofaAccountNotebookPrivate *priv;
	gulong handler;

	priv = self->private;

	handler = g_signal_connect(
						G_OBJECT( priv->dossier),
						OFA_SIGNAL_NEW_OBJECT, G_CALLBACK( on_new_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( priv->dossier),
						OFA_SIGNAL_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( priv->dossier),
						OFA_SIGNAL_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( priv->dossier),
						OFA_SIGNAL_RELOAD_DATASET, G_CALLBACK( on_reloaded_dataset ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );
}

/*
 * OFA_SIGNAL_NEW_OBJECT signal handler
 */
static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaAccountNotebook *self )
{
	static const gchar *thisfn = "ofa_account_notebook_on_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn, ( void * ) dossier,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), ( void * ) self );

	if( OFO_IS_ACCOUNT( object )){
		insert_row( self, OFO_ACCOUNT( object ), TRUE );

	} else if( OFO_IS_CLASS( object )){
		on_updated_class_label( self, OFO_CLASS( object ));
	}
}

/*
 * OFA_SIGNAL_UPDATE_OBJECT signal handler
 */
static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaAccountNotebook *self )
{
	static const gchar *thisfn = "ofa_account_notebook_on_updated_object";
	const gchar *acc_num;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn, ( void * ) dossier,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), prev_id, ( void * ) self );

	if( OFO_IS_ACCOUNT( object )){
		acc_num = ofo_account_get_number( OFO_ACCOUNT( object ));
		if( prev_id && g_utf8_collate( prev_id, acc_num )){
			remove_row_by_number( self, prev_id );
			insert_row( self, OFO_ACCOUNT( object ), TRUE );
		} else {
			set_row( self, OFO_ACCOUNT( object ), TRUE );
		}

	} else if( OFO_IS_CLASS( object )){
		on_updated_class_label( self, OFO_CLASS( object ));
	}
}

/*
 * OFA_SIGNAL_DELETED_OBJECT signal handler
 */
static void
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaAccountNotebook *self )
{
	static const gchar *thisfn = "ofa_account_notebook_on_deleted_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn, ( void * ) dossier,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), ( void * ) self );

	if( OFO_IS_ACCOUNT( object )){
		remove_row_by_number( self, ofo_account_get_number( OFO_ACCOUNT( object )));

	} else if( OFO_IS_CLASS( object )){
		on_deleted_class_label( self, OFO_CLASS( object ));
	}
}

/*
 * OFA_SIGNAL_RELOAD_DATASET signal handler
 */
static void
on_reloaded_dataset( ofoDossier *dossier, GType type, ofaAccountNotebook *self )
{
	static const gchar *thisfn = "ofa_account_notebook_on_reloaded_dataset";

	g_debug( "%s: dossier=%p, type=%lu, self=%p",
			thisfn, ( void * ) dossier, type, ( void * ) self );

	if( type == OFO_TYPE_ACCOUNT ){
		while( gtk_notebook_get_n_pages( self->private->book )){
			gtk_notebook_remove_page( self->private->book, 0 );
		}
		insert_dataset( self );
	}
}

static void
init_ui( ofaAccountNotebook *self )
{
	/* load our UI and attach it to the provided parent container */
	reparent_from_window( self );

	/* setup and connect the notebook */
	setup_account_book( self );

	/* setup and connect the buttons */
	setup_buttons( self );
}

static void
reparent_from_window( ofaAccountNotebook *self )
{
	GtkWidget *window;
	GtkWidget *grid;

	/* load our window */
	window = my_utils_builder_load_from_path( st_ui_xml, st_ui_id );
	g_return_if_fail( window && GTK_IS_WINDOW( window ));

	grid = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "top-grid" );
	g_return_if_fail( grid && GTK_IS_GRID( grid ));
	self->private->top_grid = GTK_GRID( grid );

	/* attach our grid to the parent's frame */
	gtk_widget_reparent( grid, GTK_WIDGET( self->private->parent ));
}

static void
setup_account_book( ofaAccountNotebook *self )
{
	ofaAccountNotebookPrivate *priv;
	GtkWidget *book;

	priv = self->private;

	book = my_utils_container_get_child_by_type(
						GTK_CONTAINER( priv->top_grid ), GTK_TYPE_NOTEBOOK );
	g_return_if_fail( book && GTK_IS_NOTEBOOK( book ));

	priv->book = GTK_NOTEBOOK( book );

	g_signal_connect(
			G_OBJECT( priv->book ),
			"switch-page", G_CALLBACK( on_page_switched ), self );

	g_signal_connect(
			G_OBJECT( priv->book ),
			"key-press-event", G_CALLBACK( on_key_pressed_event ), self );
}

/*
 * buttons are provided by the main page for a first part
 * to which we add the 'View entries' button
 */
static void
setup_buttons( ofaAccountNotebook *self )
{
	ofaAccountNotebookPrivate *priv;
	GtkBox *buttons_box;
	GtkWidget *button;
	GtkFrame *frame;

	priv = self->private;

	/* get the standard buttons and connect our signals */
	buttons_box = ofa_main_page_get_buttons_box_new( priv->has_import, priv->has_export );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( buttons_box ), PAGE_BUTTON_NEW );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_new_clicked ), self );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( buttons_box ), PAGE_BUTTON_UPDATE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_update_clicked ), self );
	priv->btn_update = GTK_BUTTON( button );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( buttons_box ), PAGE_BUTTON_DELETE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_delete_clicked ), self );
	priv->btn_delete = GTK_BUTTON( button );

	/* add our account-specific buttons */
	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_NONE );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( frame ), FALSE, FALSE, 8 );

	if( priv->has_view_entries ){
		button = gtk_button_new_with_mnemonic( _( "View _entries..." ));
		gtk_widget_set_sensitive( button, FALSE );
		g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_view_entries ), self );
		gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
		priv->btn_consult = GTK_BUTTON( button );
	}

	/* attach the buttons box to the parent grid */
	gtk_grid_attach( priv->top_grid, GTK_WIDGET( buttons_box ), 1, 0, 1, 1 );
}

static void
on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaAccountNotebook *self )
{
	GtkTreeView *tview;
	GtkTreeSelection *select;

	tview = ( GtkTreeView * )
					my_utils_container_get_child_by_type(
							GTK_CONTAINER( wpage ), GTK_TYPE_TREE_VIEW );

	if( tview ){
		g_return_if_fail( GTK_IS_TREE_VIEW( tview ));
		select = gtk_tree_view_get_selection( tview );
		on_row_selected( select, self );
	}
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

/**
 * ofa_account_notebook_init_view:
 * @number: [allow-none]
 *
 * Populates the view, setting the first selection on account 'number'
 * if specified, or on the first visible account (if any) of the first
 * book's page.
 */
void
ofa_account_notebook_init_view( ofaAccountNotebook *self, const gchar *number )
{
	static const gchar *thisfn = "ofa_account_notebook_init_view";
	GtkWidget *page_w;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	g_return_if_fail( self && OFA_IS_ACCOUNT_NOTEBOOK( self ));

	g_debug( "%s: self=%p, number=%s", thisfn, ( void * ) self, number );

	if( !self->private->dispose_has_run ){

		insert_dataset( self );

		if( number && g_utf8_strlen( number, -1 )){
			select_row_by_number( self, number );

		} else {
			page_w = gtk_notebook_get_nth_page( self->private->book, 0 );
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
insert_dataset( ofaAccountNotebook *self )
{
	GList *chart, *ic;

	chart = ofo_account_get_dataset( self->private->dossier );

	for( ic=chart ; ic ; ic=ic->next ){
		insert_row( self, OFO_ACCOUNT( ic->data ), FALSE );
	}
}

/*
 * insert a new row in the ad-hoc page of the notebooc, creating the
 * page as needed
 */
static void
insert_row( ofaAccountNotebook *self, ofoAccount *account, gboolean with_selection )
{
	static const gchar *thisfn = "ofa_account_notebook_insert_row";
	gint page_num;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	page_num = book_get_page_by_class(
						self, ofo_account_get_class( account ), TRUE, &tview, &tmodel );

	if( page_num >= 0 ){
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				COL_NUMBER,   ofo_account_get_number( account ),
				COL_OBJECT,   account,
				-1 );

		set_row_by_iter( self, account, tmodel, &iter );

		if( with_selection ){
			gtk_notebook_set_current_page( self->private->book, page_num );
			select_row_by_iter( self, tview, tmodel, &iter );
		}
	} else {
		g_debug( "%s: unable to get a page for insertion of account %s",
				thisfn, ofo_account_get_number( account ));
	}
}

/*
 * returns the number of the notebook's page which is dedicated to the
 * given class number
 *
 * if tview/tmodel pointers are provided, the values are set
 */
static gint
book_get_page_by_class( ofaAccountNotebook *self,
								gint class_num, gboolean bcreate,
								GtkTreeView **tview, GtkTreeModel **tmodel )
{
	static const gchar *thisfn = "ofa_account_notebook_book_get_page_by_class";
	gint count, i, found;
	GtkWidget *page_widget;
	gint page_class;
	GtkTreeView *my_tview;

	if( !ofo_class_is_valid_number( class_num )){
		g_warning( "%s: invalid class number: %d", thisfn, class_num );
		return( -1 );
	}

	found = -1;
	count = gtk_notebook_get_n_pages( self->private->book );

	/* search for an existing page */
	for( i=0 ; i<count ; ++i ){
		page_widget = gtk_notebook_get_nth_page( self->private->book, i );
		page_class = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( page_widget ), DATA_PAGE_CLASS ));
		if( page_class == class_num ){
			found = i;
			break;
		}
	}

	/* if not exists, create it (if allowed) */
	if( found == -1 && bcreate ){
		found = book_create_page( self, self->private->book, class_num, &page_widget );
	}

	/* retrieve tree informations */
	if( found >= 0 ){
		g_return_val_if_fail( page_widget && GTK_IS_SCROLLED_WINDOW( page_widget ), -1 );

		my_tview = ( GtkTreeView * )
						my_utils_container_get_child_by_type(
								GTK_CONTAINER( page_widget ), GTK_TYPE_TREE_VIEW );
		g_return_val_if_fail( my_tview && GTK_IS_TREE_VIEW( my_tview ), -1 );

		if( tview ){
			*tview = my_tview;
		}
		if( tmodel ){
			*tmodel = gtk_tree_view_get_model( my_tview );
		}
	}

	if( found < 0 ){
		g_debug( "%s: unable to get the page for class %d", thisfn, class_num );
	}
	return( found );
}

/*
 * returns the page number
 * setting the page widget if a variable pointer is provided
 */
static gint
book_create_page( ofaAccountNotebook *self, GtkNotebook *book, gint class, GtkWidget **page )
{
	static const gchar *thisfn = "ofa_account_notebook_book_create_page";
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

	obj_class = ofo_class_get_by_number( self->private->dossier, class );
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
			G_OBJECT( view ), "row-activated", G_CALLBACK( on_row_activated), self );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( view ));

	model = GTK_TREE_MODEL( gtk_list_store_new(
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
	gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( view, column );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

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
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 0.0, 0.5 );
	column = gtk_tree_view_column_new();
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( COL_CURRENCY ));
	gtk_tree_view_column_pack_end( column, text_cell, FALSE );
	gtk_tree_view_column_set_alignment( column, 0.0 );
	gtk_tree_view_column_add_attribute( column, text_cell, "text", COL_CURRENCY );
	gtk_tree_view_column_set_min_width( column, 40 );
	gtk_tree_view_append_column( view, column );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	select = gtk_tree_view_get_selection( view );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect(
			G_OBJECT( select ), "changed", G_CALLBACK( on_row_selected ), self );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( model ), ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( model ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	gtk_widget_show_all( GTK_WIDGET( scroll ));

	if( page ){
		*page = GTK_WIDGET( scroll );
	}

	return( page_num );
}

static void
on_row_selected( GtkTreeSelection *selection, ofaAccountNotebook *self )
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

	if( self->private->pfnSelected ){
		( *self->private->pfnSelected )( account, self->private->user_data );
	}
}

static void
on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaAccountNotebook *self )
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

	if( self->private->pfnActivated ){
		( *self->private->pfnActivated)( account, self->private->user_data );
	}
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
 *
 * detail accounts who have no currency are red written.
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
	ofoDevise *devise;

	g_return_if_fail( GTK_IS_CELL_RENDERER_TEXT( cell ));

	gtk_tree_model_get( tmodel, iter, COL_OBJECT, &account, -1 );
	g_object_unref( account );
	level = ofo_account_get_level_from_number( ofo_account_get_number( account ));
	g_return_if_fail( level >= 2 );

	column = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( tcolumn ), DATA_COLUMN_ID ));
	if( column == COL_NUMBER ){
		number = g_string_new( "" );
		g_string_printf( number, "%*c", 2*(level-2), ' ' );
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
		devise = ofo_devise_get_by_code( self->private->dossier, ofo_account_get_devise( account ));
		if( !devise ){
			gdk_rgba_parse( &color, "#800000" );
			g_object_set( G_OBJECT( cell ), "foreground-rgba", &color, NULL );
		}
	}
}

static void
update_buttons_sensitivity( ofaAccountNotebook *self, ofoAccount *account )
{
	gtk_widget_set_sensitive(
			GTK_WIDGET( self->private->btn_update ),
			account && OFO_IS_ACCOUNT( account ));

	gtk_widget_set_sensitive(
			GTK_WIDGET( self->private->btn_delete ),
			account && OFO_IS_ACCOUNT( account ) && ofo_account_is_deletable( account ));

	if( self->private->btn_consult ){
		gtk_widget_set_sensitive(
				GTK_WIDGET( self->private->btn_consult ),
				account && OFO_IS_ACCOUNT( account ) && !ofo_account_is_root( account ));
	}
}

/*
 * update the list store with the new properties of an account
 * this functionm ust only be used if the item already exists in the
 * list store (not a new row), and the number has not been changed
 */
static void
set_row_by_iter( ofaAccountNotebook *self,
						ofoAccount *account, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gchar *sdeb, *scre;
	ofoDevise *devise;
	gchar *cdev;

	if( ofo_account_is_root( account )){
		sdeb = g_strdup( "" );
		scre = g_strdup( "" );
		cdev = g_strdup( "" );

	} else {
		sdeb = g_strdup_printf( "%'.2f",
				ofo_account_get_deb_mnt( account )+ofo_account_get_bro_deb_mnt( account ));
		scre = g_strdup_printf( "%'.2f",
				ofo_account_get_cre_mnt( account )+ofo_account_get_bro_cre_mnt( account ));
		devise = ofo_devise_get_by_code( self->private->dossier, ofo_account_get_devise( account ));
		if( devise ){
			cdev = g_strdup( ofo_devise_get_code( devise ));
		} else {
			cdev = g_strdup( "" );
		}
	}

	gtk_list_store_set(
			GTK_LIST_STORE( tmodel ),
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

static void
select_row_by_iter( ofaAccountNotebook *self, GtkTreeView *tview, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	GtkTreePath *path;

	path = gtk_tree_model_get_path( tmodel, iter );
	gtk_tree_view_set_cursor( GTK_TREE_VIEW( tview ), path, NULL, FALSE );
	gtk_tree_path_free( path );
	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}

/*
 * select the row with the given number, or the most close one
 */
static void
select_row_by_number( ofaAccountNotebook *self, const gchar *number )
{
	gint page_num;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gboolean bvalid;

	if( number && g_utf8_strlen( number, -1 )){
		page_num = book_get_page_by_class( self,
							ofo_account_get_class_from_number( number ), FALSE, &tview, &tmodel );
		if( page_num > 0 ){
			gtk_notebook_set_current_page( self->private->book, page_num );
			find_row_by_number( self, number, tmodel, &iter, &bvalid );
			if( bvalid ){
				select_row_by_iter( self, tview, tmodel, &iter );
			}
		}
	}
}

/*
 * rows are sorted by account number
 * we exit the search as soon as we get a number greater than the
 * searched one
 *
 * @bvalid: [allow-none] set to TRUE if the returned iter is valid
 *
 * Returns TRUE if we have found an exact match
 */
static gboolean
find_row_by_number( ofaAccountNotebook *self, const gchar *number,
							GtkTreeModel *tmodel, GtkTreeIter *iter, gboolean *bvalid )
{
	static const gchar *thisfn = "ofa_account_notebook_find_row_by_number";
	gchar *accnum;
	gint cmp;
	GtkTreeIter prev_iter;

	if( bvalid ){
		*bvalid = FALSE;
	}

	if( gtk_tree_model_get_iter_first( tmodel, iter )){

		if( bvalid ){
			*bvalid = TRUE;
		}

		while( TRUE ){
			gtk_tree_model_get( tmodel, iter, COL_NUMBER, &accnum, -1 );
			cmp = g_utf8_collate( accnum, number );
			g_free( accnum );
			if( cmp == 0 ){
				return( TRUE );
			}
			if( cmp > 0 ){
				return( FALSE );
			}
			prev_iter = *iter;
			if( !gtk_tree_model_iter_next( tmodel, iter )){
				break;
			}
		}
		*iter = prev_iter;
	}

	g_debug( "%s: account number %s not found", thisfn, number );

	return( FALSE );
}

static void
remove_row_by_number( ofaAccountNotebook *self, const gchar *number )
{
	gint page_num;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	page_num = book_get_page_by_class( self,
						ofo_account_get_class_from_number( number ), FALSE, &tview, &tmodel );

	if( page_num >= 0 &&
			find_row_by_number( self, number, tmodel, &iter, NULL )){

		gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
	}
}

/*
 * update the list store with the new properties of an account
 * this function must only be used if the item already exists in the
 * list store (not a new row), and the number has not been changed
 */
static void
set_row( ofaAccountNotebook *self, ofoAccount *account, gboolean with_selection )
{
	gint page_num;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	page_num = book_get_page_by_class( self,
						ofo_account_get_class( account ), FALSE, &tview, &tmodel );

	if( page_num >= 0 &&
			find_row_by_number( self,
					ofo_account_get_number( account ), tmodel, &iter, NULL )){

		set_row_by_iter( self, account, tmodel, &iter );

		if( with_selection ){
			select_row_by_iter( self, tview, tmodel, &iter );
		}
	}
}

static gboolean
book_activate_page_by_class( ofaAccountNotebook *self, gint class_num )
{
	gint page_num;

	page_num = book_get_page_by_class( self, class_num, FALSE, NULL, NULL );
	if( page_num >= 0 ){
		gtk_notebook_set_current_page( self->private->book, page_num );
		return( TRUE );
	}

	return( FALSE );
}

/**
 * ofa_account_notebook_get_selected:
 */
ofoAccount *
ofa_account_notebook_get_selected( ofaAccountNotebook *self )
{
	gint page_n;
	GtkWidget *page_w;
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoAccount *account;

	g_return_val_if_fail( self && OFA_IS_ACCOUNT_NOTEBOOK( self ), NULL );

	account = NULL;

	if( !self->private->dispose_has_run ){

		page_n = gtk_notebook_get_current_page( self->private->book );
		g_return_val_if_fail( page_n >= 0, NULL );

		page_w = gtk_notebook_get_nth_page( self->private->book, page_n );
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
 * ofa_account_notebook_set_selected:
 *
 * Let the user reset the selection after the end of setup and
 * initialization phases
 */
void
ofa_account_notebook_set_selected( ofaAccountNotebook *self, const gchar *number )
{
	g_return_if_fail( self && OFA_IS_ACCOUNT_NOTEBOOK( self ));

	if( !self->private->dispose_has_run ){

		select_row_by_number( self, number );
	}
}

/**
 * ofa_account_notebook_grab_focus:
 *
 * Reset the focus on the treeview.
 */
void
ofa_account_notebook_grab_focus( ofaAccountNotebook *self )
{
	gint page_n;
	GtkWidget *page_w;
	GtkWidget *tview;

	g_return_if_fail( self && OFA_IS_ACCOUNT_NOTEBOOK( self ));

	if( !self->private->dispose_has_run ){

		page_n = gtk_notebook_get_current_page( self->private->book );
		g_return_if_fail( page_n >= 0 );

		page_w = gtk_notebook_get_nth_page( self->private->book, page_n );
		g_return_if_fail( page_w && GTK_IS_CONTAINER( page_w ));

		tview = my_utils_container_get_child_by_type(
								GTK_CONTAINER( page_w ), GTK_TYPE_TREE_VIEW );
		g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));

		gtk_widget_grab_focus( tview );
	}
}

#if 0
/*
 * the iso code 3a of a currency has changed - update the display
 * this should be very rare
 */
static void
on_updated_currency_code( ofaAccountNotebook *self, ofoDevise *devise )
{
	gint pages_count, i;
	GtkWidget *page_w;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoAccount *account;
	gint dev_id;

	dev_id = ofo_devise_get_id( devise );
	pages_count = gtk_notebook_get_n_pages( self->private->book );

	for( i=0 ; i<pages_count ; ++i ){

		page_w = gtk_notebook_get_nth_page( self->private->book, i );
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

				if( ofo_account_get_devise( account ) == dev_id ){
					set_row_by_iter( self, account, tmodel, &iter );
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
 * a class label has changed : update the corresponding tab label
 */
static void
on_updated_class_label( ofaAccountNotebook *self, ofoClass *class )
{
	gint page_n;
	GtkWidget *page_w;
	gint class_num;

	class_num = ofo_class_get_number( class );
	page_n = book_get_page_by_class( self, class_num, FALSE, NULL, NULL );
	if( page_n >= 0 ){
		page_w = gtk_notebook_get_nth_page( self->private->book, page_n );
		g_return_if_fail( page_w && GTK_IS_WIDGET( page_w ));
		gtk_notebook_set_tab_label_text( self->private->book, page_w, ofo_class_get_label( class ));
	}
}

static void
on_deleted_class_label( ofaAccountNotebook *self, ofoClass *class )
{
	gint page_n;
	GtkWidget *page_w;
	gint class_num;

	class_num = ofo_class_get_number( class );
	page_n = book_get_page_by_class( self, class_num, FALSE, NULL, NULL );
	if( page_n >= 0 ){
		page_w = gtk_notebook_get_nth_page( self->private->book, page_n );
		g_return_if_fail( page_w && GTK_IS_WIDGET( page_w ));
		gtk_notebook_set_tab_label_text( self->private->book, page_w, st_class_labels[class_num-1] );
	}
}

static void
on_new_clicked( GtkButton *button, ofaAccountNotebook *self )
{
	ofoAccount *account;

	account = ofo_account_new();

	if( !ofa_account_properties_run( self->private->main_window, account )){
		g_object_unref( account );
	}
}

static void
on_update_clicked( GtkButton *button, ofaAccountNotebook *self )
{
	ofoAccount *account;

	account = ofa_account_notebook_get_selected( self );
	if( account ){
		do_update_with_account( self, account );
	}
}

static void
do_update_with_account( ofaAccountNotebook *self, ofoAccount *account )
{
	if( account ){
		g_return_if_fail( OFO_IS_ACCOUNT( account ));

		ofa_account_properties_run( self->private->main_window, account );
	}

	ofa_account_notebook_grab_focus( self );
}

static void
on_delete_clicked( GtkButton *button, ofaAccountNotebook *self )
{
	ofoAccount *account;
	gchar *number;

	account = ofa_account_notebook_get_selected( self );
	if( account ){
		g_return_if_fail( ofo_account_is_deletable( account ));

		number = g_strdup( ofo_account_get_number( account ));

		if( delete_confirmed( self, account ) &&
				ofo_account_delete( account )){

			/* nothing to do here, all being managed by signal handlers
			 * just reset the selection as this is not managed by the
			 * account notebook (and doesn't have to)
			 * asking for selection of the just deleted account makes
			 * almost sure that we are going to select the most close
			 * row */
			on_row_selected( NULL, self );
			ofa_account_notebook_set_selected( self, number );
		}

		g_free( number );
	}

	ofa_account_notebook_grab_focus( self );
}

static gboolean
delete_confirmed( ofaAccountNotebook *self, ofoAccount *account )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s - %s' account ?" ),
			ofo_account_get_number( account ),
			ofo_account_get_label( account ));

	delete_ok = ofa_main_page_delete_confirmed( NULL, msg );

	g_free( msg );

	return( delete_ok );
}

static void
on_view_entries( GtkButton *button, ofaAccountNotebook *self )
{
	ofoAccount *account;

	account = ofa_account_notebook_get_selected( self );

	if( self->private->pfnViewEntries ){
		( *self->private->pfnViewEntries )( account, self->private->user_data );
	}
}
