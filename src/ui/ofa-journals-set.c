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
#include "ui/ofa-journal-properties.h"
#include "ui/ofa-journals-set.h"
#include "ui/ofo-journal.h"

/* private class data
 */
struct _ofaJournalsSetClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaJournalsSetPrivate {
	gboolean       dispose_has_run;

	/* properties
	 */

	/* internals
	 */

	/* UI
	 */
	GtkTreeView   *view;				/* the treeview on the journals set */
	GtkButton     *update_btn;
	GtkButton     *delete_btn;
};

/* column ordering in the selection listview
 */
enum {
	COL_MNEMO = 0,
	COL_LABEL,
	COL_CLOTURE,
	COL_OBJECT,
	N_COLUMNS
};

static GObjectClass *st_parent_class = NULL;

static GType      register_type( void );
static void       class_init( ofaJournalsSetClass *klass );
static void       instance_init( GTypeInstance *instance, gpointer klass );
static void       instance_constructed( GObject *instance );
static void       instance_dispose( GObject *instance );
static void       instance_finalize( GObject *instance );
static void       setup_set_page( ofaJournalsSet *self );
static GtkWidget *setup_journals_view( ofaJournalsSet *self );
static GtkWidget *setup_buttons_box( ofaJournalsSet *self );
static void       setup_first_selection( ofaJournalsSet *self );
static void       store_set_journal( GtkTreeModel *model, GtkTreeIter *iter, const ofoJournal *journal );
static void       on_journal_selected( GtkTreeSelection *selection, ofaJournalsSet *self );
static void       on_new_journal( GtkButton *button, ofaJournalsSet *self );
static void       on_update_journal( GtkButton *button, ofaJournalsSet *self );
static void       on_delete_journal( GtkButton *button, ofaJournalsSet *self );
static void       error_undeletable( ofaJournalsSet *self, ofoJournal *journal );
static gboolean   delete_confirmed( ofaJournalsSet *self, ofoJournal *journal );
static void       insert_new_row( ofaJournalsSet *self, ofoJournal *journal );

GType
ofa_journals_set_get_type( void )
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
	static const gchar *thisfn = "ofa_journals_set_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaJournalsSetClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaJournalsSet ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFA_TYPE_MAIN_PAGE, "ofaJournalsSet", &info, 0 );

	return( type );
}

static void
class_init( ofaJournalsSetClass *klass )
{
	static const gchar *thisfn = "ofa_journals_set_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->constructed = instance_constructed;
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofaJournalsSetClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_journals_set_instance_init";
	ofaJournalsSet *self;

	g_return_if_fail( OFA_IS_JOURNALS_SET( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",

			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_JOURNALS_SET( instance );

	self->private = g_new0( ofaJournalsSetPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_constructed( GObject *instance )
{
	static const gchar *thisfn = "ofa_journals_set_instance_constructed";

	g_return_if_fail( OFA_IS_JOURNALS_SET( instance ));

	/* first, chain up to the parent class */
	if( G_OBJECT_CLASS( st_parent_class )->constructed ){
		G_OBJECT_CLASS( st_parent_class )->constructed( instance );
	}

	/* then initialize our page
	 */
	g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	setup_set_page( OFA_JOURNALS_SET( instance ));
}

static void
instance_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofa_journals_set_instance_dispose";
	ofaJournalsSetPrivate *priv;

	g_return_if_fail( OFA_IS_JOURNALS_SET( instance ));

	priv = ( OFA_JOURNALS_SET( instance ))->private;

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
	static const gchar *thisfn = "ofa_journals_set_instance_finalize";
	ofaJournalsSet *self;

	g_return_if_fail( OFA_IS_JOURNALS_SET( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFA_JOURNALS_SET( instance );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( instance );
	}
}

/**
 * ofa_journals_set_run:
 *
 * When called by the main_window, the page has been created, showed
 * and activated - there is nothing left to do here....
 */
void
ofa_journals_set_run( ofaMainPage *this )
{
	static const gchar *thisfn = "ofa_journals_set_run";

	g_return_if_fail( this && OFA_IS_JOURNALS_SET( this ));

	g_debug( "%s: this=%p (%s)",
			thisfn, ( void * ) this, G_OBJECT_TYPE_NAME( this ));
}

/*
 * +-----------------------------------------------------------------------+
 * | grid1 (this is the notebook page)                                     |
 * | +-----------------------------------------------+-------------------+ |
 * | | list of journals                              | buttons           | |
 * | |                                               |                   | |
 * | |                                               |                   | |
 * | +-----------------------------------------------+-------------------+ |
 * +-----------------------------------------------------------------------+
 */
static void
setup_set_page( ofaJournalsSet *self )
{
	GtkGrid *grid;
	GtkWidget *view;
	GtkWidget *buttons_box;

	grid = ofa_main_page_get_grid( OFA_MAIN_PAGE( self ));

	view = setup_journals_view( self );
	gtk_grid_attach( grid, view, 0, 0, 1, 1 );

	buttons_box = setup_buttons_box( self );
	gtk_grid_attach( grid, buttons_box, 1, 0, 1, 1 );

	setup_first_selection( self );
}

static GtkWidget *
setup_journals_view( ofaJournalsSet *self )
{
	GtkFrame *frame;
	GtkScrolledWindow *scroll;
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	ofoDossier *dossier;
	GList *dataset, *ijou;
	ofoJournal *journal;
	GtkTreeIter iter;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_margin_left( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_top( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( frame ), 4 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_container_set_border_width( GTK_CONTAINER( scroll ), 4 );
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( scroll ));

	view = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_vexpand( GTK_WIDGET( view ), TRUE );
	gtk_tree_view_set_headers_visible( view, TRUE );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( view ));
	self->private->view = GTK_TREE_VIEW( view );

	model = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( view, model );
	g_object_unref( model );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Mnemo" ),
			text_cell, "text", COL_MNEMO,
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
	column = gtk_tree_view_column_new_with_attributes(
			_( "Last closing" ),
			text_cell, "text", COL_CLOTURE,
			NULL );
	gtk_tree_view_append_column( view, column );

	select = gtk_tree_view_get_selection( view );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_journal_selected ), self );

	dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ));
	dataset = ofo_dossier_get_journals_set( dossier );
	ofa_main_page_set_dataset( OFA_MAIN_PAGE( self ), dataset );

	for( ijou=dataset ; ijou ; ijou=ijou->next ){

		journal = OFO_JOURNAL( ijou->data );
		gtk_list_store_append( GTK_LIST_STORE( model ), &iter );
		store_set_journal( model, &iter, journal );
	}

	return( GTK_WIDGET( frame ));
}

static GtkWidget *
setup_buttons_box( ofaJournalsSet *self )
{
	GtkBox *buttons_box;
	GtkFrame *frame;
	GtkButton *button;

	buttons_box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 ));
	gtk_widget_set_margin_right( GTK_WIDGET( buttons_box ), 4 );

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_NONE );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( frame ), FALSE, FALSE, 30 );

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_New..." )));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_new_journal ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_Update..." )));
	gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_update_journal ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	self->private->update_btn = button;

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_Delete..." )));
	gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_delete_journal ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	self->private->delete_btn = button;

	return( GTK_WIDGET( buttons_box ));
}

static void
setup_first_selection( ofaJournalsSet *self )
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	model = gtk_tree_view_get_model( self->private->view );
	if( gtk_tree_model_get_iter_first( model, &iter )){
		select = gtk_tree_view_get_selection( self->private->view );
		gtk_tree_selection_select_iter( select, &iter );
	}
}

static void
store_set_journal( GtkTreeModel *model, GtkTreeIter *iter, const ofoJournal *journal )
{
	const GDate *dclo;
	gchar *sclo;

	dclo = ofo_journal_get_cloture( journal );
	sclo = my_utils_display_from_date( dclo, MY_UTILS_DATE_DMMM );

	gtk_list_store_set(
			GTK_LIST_STORE( model ),
			iter,
			COL_MNEMO,   ofo_journal_get_mnemo( journal ),
			COL_LABEL,   ofo_journal_get_label( journal ),
			COL_CLOTURE, sclo,
			COL_OBJECT, journal,
			-1 );

	/*
	g_debug( "ofa_journals_set_store_set_journal: %s %s",
			ofo_journal_get_mnemo( journal ), ofo_journal_get_label( journal ));
			*/

	g_free( sclo );
}

static void
on_journal_selected( GtkTreeSelection *selection, ofaJournalsSet *self )
{
	gboolean select_ok;

	select_ok = gtk_tree_selection_get_selected( selection, NULL, NULL );

	gtk_widget_set_sensitive( GTK_WIDGET( self->private->update_btn ), select_ok );
	gtk_widget_set_sensitive( GTK_WIDGET( self->private->delete_btn ), select_ok );
}

static void
on_new_journal( GtkButton *button, ofaJournalsSet *self )
{
	static const gchar *thisfn = "ofa_journals_set_on_new_journal";
	ofoJournal *journal;
	ofaMainWindow *main_window;

	g_return_if_fail( OFA_IS_JOURNALS_SET( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	main_window = ofa_main_page_get_main_window( OFA_MAIN_PAGE( self ));

	journal = ofo_journal_new();
	if( ofa_journal_properties_run( main_window, journal )){
		insert_new_row( self, journal );
		g_signal_emit_by_name( self,
				MAIN_PAGE_SIGNAL_JOURNAL_UPDATED, MAIN_PAGE_OBJECT_CREATED, journal );
	}
}

static void
on_update_journal( GtkButton *button, ofaJournalsSet *self )
{
	static const gchar *thisfn = "ofa_journals_set_on_update_journal";
	GtkTreeSelection *select;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *prev_mnemo;
	const gchar *new_mnemo;
	ofoJournal *journal;

	g_return_if_fail( OFA_IS_JOURNALS_SET( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	select = gtk_tree_view_get_selection( self->private->view );
	if( gtk_tree_selection_get_selected( select, &model, &iter )){

		gtk_tree_model_get( model, &iter, COL_OBJECT, &journal, -1 );
		g_object_unref( journal );

		prev_mnemo = g_strdup( ofo_journal_get_mnemo( journal ));

		if( ofa_journal_properties_run(
				ofa_main_page_get_main_window( OFA_MAIN_PAGE( self )), journal )){

			new_mnemo = ofo_journal_get_mnemo( journal );
			if( g_utf8_collate( prev_mnemo, new_mnemo )){
				gtk_list_store_remove( GTK_LIST_STORE( model ), &iter );
				insert_new_row( self, journal );

			} else {
				store_set_journal( model, &iter, journal );
			}

			g_signal_emit_by_name( self,
					MAIN_PAGE_SIGNAL_JOURNAL_UPDATED, MAIN_PAGE_OBJECT_UPDATED, journal );
		}

		g_free( prev_mnemo );
	}
}

/*
 * un journal peut être supprimé tant qu'aucune écriture n'y a été
 * enregistrée, et après confirmation de l'utilisateur
 */
static void
on_delete_journal( GtkButton *button, ofaJournalsSet *self )
{
	static const gchar *thisfn = "ofa_journals_set_on_delete_journal";
	GtkTreeSelection *select;
	GtkTreeModel *model;
	GtkTreeIter iter;
	ofoJournal *journal;
	const GDate *dmax;
	ofoDossier *dossier;

	g_return_if_fail( OFA_IS_JOURNALS_SET( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	select = gtk_tree_view_get_selection( self->private->view );
	if( gtk_tree_selection_get_selected( select, &model, &iter )){

		gtk_tree_model_get( model, &iter, COL_OBJECT, &journal, -1 );
		g_object_unref( journal );

		dmax = ofo_journal_get_maxdate( journal );
		if( g_date_valid( dmax )){
			error_undeletable( self, journal );
			return;
		}

		dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ));

		if( delete_confirmed( self, journal ) &&
				ofo_dossier_delete_journal( dossier, journal )){

			/* update our set of journals */
			ofa_main_page_set_dataset(
					OFA_MAIN_PAGE( self ), ofo_dossier_get_journals_set( dossier ));

			g_signal_emit_by_name( self,
					MAIN_PAGE_SIGNAL_JOURNAL_UPDATED, MAIN_PAGE_OBJECT_DELETED, journal );

			/* remove the row from the model
			 * this will cause an automatic new selection */
			gtk_list_store_remove( GTK_LIST_STORE( model ), &iter );
		}
	}
}

static void
error_undeletable( ofaJournalsSet *self, ofoJournal *journal )
{
	GtkMessageDialog *dlg;
	gchar *msg;
	ofaMainWindow *main_window;

	msg = g_strdup_printf(
				_( "We are unable to delete the '%s - %s' journal "
					"as at least one entry has been recorded" ),
				ofo_journal_get_mnemo( journal ),
				ofo_journal_get_label( journal ));

	main_window = ofa_main_page_get_main_window( OFA_MAIN_PAGE( self ));

	dlg = GTK_MESSAGE_DIALOG( gtk_message_dialog_new(
				GTK_WINDOW( main_window ),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", msg ));

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( GTK_WIDGET( dlg ));
	g_free( msg );
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
insert_new_row( ofaJournalsSet *self, ofoJournal *journal )
{
	GtkTreeModel *model;
	GtkTreeIter iter, new_iter;
	GValue value = G_VALUE_INIT;
	const gchar *mnemo, *journal_mnemo;
	gboolean iter_found;
	GtkTreePath *path;
	ofoDossier *dossier;

	/* update our set of journals */
	dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ));
	ofa_main_page_set_dataset(
			OFA_MAIN_PAGE( self ), ofo_dossier_get_journals_set( dossier ));

	/* insert the new row at the right place */
	model = gtk_tree_view_get_model( self->private->view );
	iter_found = FALSE;
	if( gtk_tree_model_get_iter_first( model, &iter )){
		journal_mnemo = ofo_journal_get_mnemo( journal );
		while( !iter_found ){
			gtk_tree_model_get_value( model, &iter, COL_MNEMO, &value );
			mnemo = g_value_get_string( &value );
			if( g_utf8_collate( mnemo, journal_mnemo ) > 0 ){
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
	store_set_journal( model, &new_iter, journal );

	/* select the newly added journal */
	path = gtk_tree_model_get_path( model, &new_iter );
	gtk_tree_view_set_cursor( self->private->view, path, NULL, FALSE );
	gtk_widget_grab_focus( GTK_WIDGET( self->private->view ));
	gtk_tree_path_free( path );
}
