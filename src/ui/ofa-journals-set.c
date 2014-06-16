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
#include "ui/ofa-main-window.h"
#include "ui/ofa-view-entries.h"
#include "ui/ofa-journal-properties.h"
#include "ui/ofa-journal-treeview.h"
#include "ui/ofa-journals-set.h"
#include "api/ofo-dossier.h"
#include "api/ofo-journal.h"

/* private instance data
 */
struct _ofaJournalsSetPrivate {
	gboolean            dispose_has_run;

	/* internals
	 */
	gint                exe_id;			/* internal identifier of the current exercice */

	/* UI
	 */
	ofaJournalTreeview *tview;
	GtkButton          *entries_btn;
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
static void       on_row_activated( const gchar *mnemo, ofaJournalsSet *self );
static void       on_row_selected( const gchar *mnemo, ofaJournalsSet *self );
static void       v_on_new_clicked( GtkButton *button, ofaMainPage *page );
static void       v_on_update_clicked( GtkButton *button, ofaMainPage *page );
static void       v_on_delete_clicked( GtkButton *button, ofaMainPage *page );
static gboolean   delete_confirmed( ofaJournalsSet *self, ofoJournal *journal );
static void       on_view_entries( GtkButton *button, ofaJournalsSet *self );

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
	GtkWidget *frame;

	frame = setup_tree_view( page );

	return( frame );
}

static GtkWidget *
setup_tree_view( ofaMainPage *page )
{
	ofaJournalsSetPrivate *priv;
	GtkFrame *frame;
	JournalTreeviewParms parms;

	priv = OFA_JOURNALS_SET( page )->private;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_margin_left( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_top( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( frame ), 4 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	parms.main_window = ofa_main_page_get_main_window( page );
	parms.parent = GTK_CONTAINER( frame );
	parms.allow_multiple_selection = FALSE;
	parms.pfnActivation = ( JournalTreeviewCb ) on_row_activated;
	parms.pfnSelection = ( JournalTreeviewCb ) on_row_selected;
	parms.user_data = page;

	priv->tview = ofa_journal_treeview_new( &parms );

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
	OFA_JOURNALS_SET( page )->private->entries_btn = button;

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
	ofa_journal_treeview_init_view( self->private->tview, NULL );
}

/*
 * JournalTreeview callback
 */
static void
on_row_activated( const gchar *mnemo, ofaJournalsSet *self )
{
	v_on_update_clicked( NULL, OFA_MAIN_PAGE( self ));
}

/*
 * JournalTreeview callback
 */
static void
on_row_selected( const gchar *mnemo, ofaJournalsSet *self )
{
	ofoJournal *journal;

	journal = ofo_journal_get_by_mnemo(
						ofa_main_page_get_dossier( OFA_MAIN_PAGE( self )), mnemo );

	gtk_widget_set_sensitive(
			ofa_main_page_get_update_btn( OFA_MAIN_PAGE( self )),
			journal && OFO_IS_JOURNAL( journal ));

	gtk_widget_set_sensitive(
			ofa_main_page_get_delete_btn( OFA_MAIN_PAGE( self )),
			journal &&
					OFO_IS_JOURNAL( journal ) &&
					ofo_journal_is_deletable( journal,
							ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ))));

	gtk_widget_set_sensitive(
			GTK_WIDGET( self->private->entries_btn ),
			journal && OFO_IS_JOURNAL( journal ) && ofo_journal_has_entries( journal ));
}

static void
v_on_new_clicked( GtkButton *button, ofaMainPage *page )
{
	ofoJournal *journal;

	g_return_if_fail( page && OFA_IS_JOURNALS_SET( page ));

	journal = ofo_journal_new();

	if( ofa_journal_properties_run(
			ofa_main_page_get_main_window( page ), journal )){

		/* this is managed by ofaJournalTreeview convenience class
		 * graceful to dossier signaling system  */
		/*insert_new_row( OFA_JOURNALS_SET( page ), journal, TRUE );*/

	} else {
		g_object_unref( journal );
	}
}

static void
v_on_update_clicked( GtkButton *button, ofaMainPage *page )
{
	ofaJournalsSetPrivate *priv;
	ofoJournal *journal;

	g_return_if_fail( page && OFA_IS_JOURNALS_SET( page ));

	priv = OFA_JOURNALS_SET( page )->private;

	journal = ofa_journal_treeview_get_selected( priv->tview );

	if( journal &&
			ofa_journal_properties_run(
					ofa_main_page_get_main_window( page ), journal )){

			/* this is managed by the ofaJournalTreeview convenience
			 * class, graceful to the dossier signaling system */
	}

	ofa_journal_treeview_grab_focus( priv->tview );
}

/*
 * un journal peut être supprimé tant qu'aucune écriture n'y a été
 * enregistrée, et après confirmation de l'utilisateur
 */
static void
v_on_delete_clicked( GtkButton *button, ofaMainPage *page )
{
	ofaJournalsSetPrivate *priv;
	ofoDossier *dossier;
	ofoJournal *journal;

	g_return_if_fail( page && OFA_IS_JOURNALS_SET( page ));

	priv = OFA_JOURNALS_SET( page )->private;

	journal = ofa_journal_treeview_get_selected( priv->tview );

	dossier = ofa_main_page_get_dossier( page );
	g_return_if_fail( ofo_journal_is_deletable( journal, dossier ));

	if( delete_confirmed( OFA_JOURNALS_SET( page ), journal ) &&
			ofo_journal_delete( journal )){

		/* this is managed by the ofaJournalTreeview convenience
		 * class, graceful to the dossier signaling system */
	}

	ofa_journal_treeview_grab_focus( priv->tview );
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
	ofaJournalsSetPrivate *priv;
	ofaMainPage *page;
	ofoJournal *journal;

	g_return_if_fail( OFA_IS_JOURNALS_SET( self ));

	priv = self->private;

	journal = ofa_journal_treeview_get_selected( priv->tview );

	if( journal ){
		page = ofa_main_window_activate_theme(
						ofa_main_page_get_main_window( OFA_MAIN_PAGE( self )),
						THM_VIEW_ENTRIES );
		if( page ){
			ofa_view_entries_display_entries(
							OFA_VIEW_ENTRIES( page ),
							OFO_TYPE_JOURNAL,
							ofo_journal_get_mnemo( journal ),
							NULL,
							NULL );
		}
	}
}
