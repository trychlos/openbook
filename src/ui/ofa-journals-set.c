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
#include "ui/ofa-journal-properties.h"
#include "ui/ofa-journals-set.h"
#include "ui/ofo-dossier.h"
#include "ui/ofo-journal.h"

/* private instance data
 */
struct _ofaJournalsSetPrivate {
	gboolean dispose_has_run;

	/* internals
	 */
	gint     exe_id;					/* internal identifier of the current exercice */
};

/* column ordering in the selection listview
 */
enum {
	COL_MNEMO = 0,
	COL_LABEL,
	COL_CLOSING,
	COL_OBJECT,
	N_COLUMNS
};

G_DEFINE_TYPE( ofaJournalsSet, ofa_journals_set, OFA_TYPE_MAIN_PAGE )

static GtkWidget *v_setup_view( ofaMainPage *page );
static GtkWidget *setup_tree_view( ofaMainPage *page );
static GtkWidget *v_setup_buttons( ofaMainPage *page );
static void       v_init_view( ofaMainPage *page );
static void       insert_dataset( ofaJournalsSet *self );
static void       insert_new_row( ofaJournalsSet *self, ofoJournal *journal, gboolean with_selection );
static gint       on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaJournalsSet *self );
static void       setup_first_selection( ofaJournalsSet *self );
static void       on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainPage *page );
static void       on_journal_selected( GtkTreeSelection *selection, ofaJournalsSet *self );
static void       v_on_new_clicked( GtkButton *button, ofaMainPage *page );
static void       v_on_update_clicked( GtkButton *button, ofaMainPage *page );
static void       v_on_delete_clicked( GtkButton *button, ofaMainPage *page );
static gboolean   delete_confirmed( ofaJournalsSet *self, ofoJournal *journal );
static void       on_view_entries( GtkButton *button, ofaJournalsSet *self );
static void       on_new_object( ofoDossier *dossier, ofoBase *object, ofaJournalsSet *self );
static void       on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaJournalsSet *self );
static void       on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaJournalsSet *self );
static void       on_reloaded_dataset( ofoDossier *dossier, GType type, ofaJournalsSet *self );

static void
journals_set_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_journals_set_finalize";
	ofaJournalsSetPrivate *priv;

	g_return_if_fail( OFA_IS_JOURNALS_SET( instance ));

	priv = OFA_JOURNALS_SET( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_journals_set_parent_class )->finalize( instance );
}

static void
journals_set_dispose( GObject *instance )
{
	ofaJournalsSetPrivate *priv;

	g_return_if_fail( OFA_IS_JOURNALS_SET( instance ));

	priv = ( OFA_JOURNALS_SET( instance ))->private;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_journals_set_parent_class )->dispose( instance );
}

static void
ofa_journals_set_init( ofaJournalsSet *self )
{
	static const gchar *thisfn = "ofa_journals_set_init";

	g_return_if_fail( OFA_IS_JOURNALS_SET( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaJournalsSetPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
ofa_journals_set_class_init( ofaJournalsSetClass *klass )
{
	static const gchar *thisfn = "ofa_journals_set_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = journals_set_dispose;
	G_OBJECT_CLASS( klass )->finalize = journals_set_finalize;

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
	g_signal_connect(
			G_OBJECT( ofa_main_page_get_dossier( page )),
			OFA_SIGNAL_NEW_OBJECT,
			G_CALLBACK( on_new_object ),
			page );

	g_signal_connect(
			G_OBJECT( ofa_main_page_get_dossier( page )),
			OFA_SIGNAL_UPDATED_OBJECT,
			G_CALLBACK( on_updated_object ),
			page );

	g_signal_connect(
			G_OBJECT( ofa_main_page_get_dossier( page )),
			OFA_SIGNAL_DELETED_OBJECT,
			G_CALLBACK( on_deleted_object ),
			page );

	g_signal_connect(
			G_OBJECT( ofa_main_page_get_dossier( page )),
			OFA_SIGNAL_RELOADED_DATASET,
			G_CALLBACK( on_reloaded_dataset ),
			page );

	return( setup_tree_view( page ));
}

static GtkWidget *
setup_tree_view( ofaMainPage *page )
{
	GtkFrame *frame;
	GtkScrolledWindow *scroll;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_margin_left( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_top( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( frame ), 4 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_container_set_border_width( GTK_CONTAINER( scroll ), 4 );
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( scroll ));

	tview = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_vexpand( GTK_WIDGET( tview ), TRUE );
	gtk_tree_view_set_headers_visible( tview, TRUE );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( tview ));
	g_signal_connect(G_OBJECT( tview ), "row-activated", G_CALLBACK( on_row_activated ), page );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
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

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Last closing" ),
			text_cell, "text", COL_CLOSING,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( tview, column );

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_journal_selected ), page );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ), ( GtkTreeIterCompareFunc ) on_sort_model, page, NULL );

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	return( GTK_WIDGET( frame ));
}

static GtkWidget *
v_setup_buttons( ofaMainPage *page )
{
	GtkWidget *buttons_box;
	GtkFrame *frame;
	GtkButton *button;

	g_return_val_if_fail( OFA_IS_JOURNALS_SET( page ), NULL );

	buttons_box = OFA_MAIN_PAGE_CLASS( ofa_journals_set_parent_class )->setup_buttons( page );

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_size_request( GTK_WIDGET( frame ), -1, 25 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_NONE );
	gtk_box_pack_start( GTK_BOX( buttons_box ), GTK_WIDGET( frame ), FALSE, FALSE, 0 );

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "View _entries..." )));
	gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_view_entries ), page );
	gtk_box_pack_start( GTK_BOX( buttons_box ), GTK_WIDGET( button ), FALSE, FALSE, 0 );

	return( buttons_box );
}

static void
v_init_view( ofaMainPage *page )
{
	insert_dataset( OFA_JOURNALS_SET( page ));
}

static void
insert_dataset( ofaJournalsSet *self )
{
	ofoDossier *dossier;
	GList *dataset, *iset;
	ofoJournal *journal;

	dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ));
	dataset = ofo_journal_get_dataset( dossier );
	self->private->exe_id = ofo_dossier_get_current_exe_id( dossier );

	for( iset=dataset ; iset ; iset=iset->next ){

		journal = OFO_JOURNAL( iset->data );
		insert_new_row( self, journal, FALSE );
	}

	setup_first_selection( self );
}

static void
insert_new_row( ofaJournalsSet *self, ofoJournal *journal, gboolean with_selection )
{
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;
	gchar *sclo;

	tview = GTK_TREE_VIEW( ofa_main_page_get_treeview( OFA_MAIN_PAGE( self )));
	tmodel = gtk_tree_view_get_model( tview );
	sclo = my_utils_display_from_date(
			ofo_journal_get_cloture( journal, self->private->exe_id ),
			MY_UTILS_DATE_DMMM );

	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( tmodel ),
			&iter,
			-1,
			COL_MNEMO,   ofo_journal_get_mnemo( journal ),
			COL_LABEL,   ofo_journal_get_label( journal ),
			COL_CLOSING, sclo,
			COL_OBJECT,  journal,
			-1 );

	g_free( sclo );

	/* select the newly added journal */
	if( with_selection ){
		path = gtk_tree_model_get_path( tmodel, &iter );
		gtk_tree_view_set_cursor( tview, path, NULL, FALSE );
		gtk_tree_path_free( path );
		gtk_widget_grab_focus( GTK_WIDGET( tview ));
	}
}

static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaJournalsSet *self )
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

static void
setup_first_selection( ofaJournalsSet *self )
{
	GtkTreeView *tview;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	tview = GTK_TREE_VIEW( ofa_main_page_get_treeview( OFA_MAIN_PAGE( self )));
	model = gtk_tree_view_get_model( tview );
	if( gtk_tree_model_get_iter_first( model, &iter )){
		select = gtk_tree_view_get_selection( tview );
		gtk_tree_selection_select_iter( select, &iter );
	}

	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}

static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainPage *page )
{
	v_on_update_clicked( NULL, page );
}

static void
on_journal_selected( GtkTreeSelection *selection, ofaJournalsSet *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoJournal *journal;

	journal = NULL;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &journal, -1 );
		g_object_unref( journal );
	}

	gtk_widget_set_sensitive(
			ofa_main_page_get_update_btn( OFA_MAIN_PAGE( self )),
			journal && OFO_IS_JOURNAL( journal ));

	gtk_widget_set_sensitive(
			ofa_main_page_get_delete_btn( OFA_MAIN_PAGE( self )),
			journal &&
					OFO_IS_JOURNAL( journal ) &&
					ofo_journal_is_deletable( journal,
							ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ))));
}

static void
v_on_new_clicked( GtkButton *button, ofaMainPage *page )
{
	ofoJournal *journal;

	g_return_if_fail( page && OFA_IS_JOURNALS_SET( page ));

	journal = ofo_journal_new();

	if( ofa_journal_properties_run(
			ofa_main_page_get_main_window( page ), journal )){

		insert_new_row( OFA_JOURNALS_SET( page ), journal, TRUE );

	} else {
		g_object_unref( journal );
	}
}

static void
v_on_update_clicked( GtkButton *button, ofaMainPage *page )
{
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoJournal *journal;

	g_return_if_fail( page && OFA_IS_JOURNALS_SET( page ));

	tview = GTK_TREE_VIEW( ofa_main_page_get_treeview( page ));
	select = gtk_tree_view_get_selection( tview );

	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &journal, -1 );
		g_object_unref( journal );

		if( ofa_journal_properties_run(
				ofa_main_page_get_main_window( page ), journal )){

			gtk_list_store_set(
					GTK_LIST_STORE( tmodel ),
					&iter,
					COL_MNEMO,   ofo_journal_get_mnemo( journal ),
					COL_LABEL,   ofo_journal_get_label( journal ),
					-1 );
		}
	}

	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}

/*
 * un journal peut être supprimé tant qu'aucune écriture n'y a été
 * enregistrée, et après confirmation de l'utilisateur
 */
static void
v_on_delete_clicked( GtkButton *button, ofaMainPage *page )
{
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoDossier *dossier;
	ofoJournal *journal;

	g_return_if_fail( page && OFA_IS_JOURNALS_SET( page ));

	tview = GTK_TREE_VIEW( ofa_main_page_get_treeview( page ));
	select = gtk_tree_view_get_selection( tview );

	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &journal, -1 );
		g_object_unref( journal );

		dossier = ofa_main_page_get_dossier( page );
		g_return_if_fail( ofo_journal_is_deletable( journal, dossier ));

		if( delete_confirmed( OFA_JOURNALS_SET( page ), journal ) &&
				ofo_journal_delete( journal, dossier )){

			/* remove the row from the model
			 * this will cause an automatic new selection */
			gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
		}
	}

	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}

static gboolean
delete_confirmed( ofaJournalsSet *self, ofoJournal *journal )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want to delete the '%s - %s' journal ?" ),
			ofo_journal_get_mnemo( journal ),
			ofo_journal_get_label( journal ));

	delete_ok = ofa_main_page_delete_confirmed( OFA_MAIN_PAGE( self ), msg );

	g_free( msg );

	return( delete_ok );
}

static void
on_view_entries( GtkButton *button, ofaJournalsSet *self )
{
	static const gchar *thisfn = "ofa_journals_set_on_view_entries";

	g_return_if_fail( OFA_IS_JOURNALS_SET( self ));

	g_warning( "%s: TO BE WRITTEN", thisfn );
}

/*
 * OFA_SIGNAL_NEW_OBJECT signal handler
 */
static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaJournalsSet *self )
{
	static const gchar *thisfn = "ofa_journals_set_on_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_JOURNAL( object )){
	}
}

/*
 * OFA_SIGNAL_UPDATE_OBJECT signal handler
 */
static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaJournalsSet *self )
{
	static const gchar *thisfn = "ofa_journals_set_on_updated_object";

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn, ( void * ) dossier,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), prev_id, ( void * ) self );

	if( OFO_IS_JOURNAL( object )){
	}
}

/*
 * OFA_SIGNAL_DELETED_OBJECT signal handler
 */
static void
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaJournalsSet *self )
{
	static const gchar *thisfn = "ofa_journals_set_on_deleted_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn, ( void * ) dossier,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), ( void * ) self );

	if( OFO_IS_JOURNAL( object )){
	}
}

/*
 * OFA_SIGNAL_RELOADED_DATASET signal handler
 */
static void
on_reloaded_dataset( ofoDossier *dossier, GType type, ofaJournalsSet *self )
{
	static const gchar *thisfn = "ofa_journals_set_on_reloaded_dataset";
	GtkTreeView *tview;
	GtkTreeModel *tmodel;

	g_debug( "%s: dossier=%p, type=%lu, self=%p",
			thisfn, ( void * ) dossier, type, ( void * ) self );

	if( type == OFO_TYPE_JOURNAL ){
		tview = GTK_TREE_VIEW( ofa_main_page_get_treeview( OFA_MAIN_PAGE( self )));
		tmodel = gtk_tree_view_get_model( tview );
		gtk_list_store_clear( GTK_LIST_STORE( tmodel ));
		insert_dataset( self );
	}
}
