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
#include <stdlib.h>

#include "core/my-utils.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-guided-ex.h"
#include "ui/ofa-journal-combo.h"
#include "ui/ofa-main-page.h"
#include "ui/ofa-main-window.h"
#include "api/ofo-base.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-journal.h"
#include "api/ofo-model.h"
#include "api/ofo-taux.h"

/* private instance data
 */
struct _ofaGuidedExPrivate {
	gboolean        dispose_has_run;

	/* internals
	 */
	ofoDossier      *dossier;			/* dossier */
	GDate            last_closed_exe;	/* 	- last closed exercice */

	const ofoModel  *model;				/* model */

	gchar           *journal;			/* journal */
	GDate            last_closed_jou;	/*	- last periodic closing date */

	gboolean         deffet_has_focus;
	gboolean         deffet_changed_while_focus;
	GDate            last_closing;		/* max of closing exercice and closing journal dates */

	/* UI - the pane
	 */
	GtkPaned        *pane;

	/* UI - left part treeview selection of the entry model
	 */
	GtkTreeView     *left_tview;
	GtkButton       *left_select;

	/* UI - right part guided input
	 *      most if not all elements are taken from ofa-guided-input.ui
	 *      dialog box definition
	 */
	GtkContainer    *right_box;				/* the reparented container from dialog */
	GtkLabel        *right_model_label;
	ofaJournalCombo *right_journal_combo;
	GtkEntry        *right_dope;
	GtkEntry        *right_deffet;
	GtkGrid         *right_entries_grid;	/* entries view container */
	gint             right_entries_count;	/* count of rows added to this grid */
	GtkEntry        *right_comment;
	GtkButton       *right_ok;

	/* data
	 */
	GDate            dope;
	GDate            deff;
	gdouble          total_debits;
	gdouble          total_credits;
};

/* columns in the left tree view which handles the entry models
 */
enum {
	LEFT_COL_MNEMO = 0,
	LEFT_COL_LABEL,
	LEFT_COL_OBJECT,
	LEFT_N_COLUMNS
};
/*
 * columns in the grid view
 */
enum {
	COL_RANG = 0,
	FIRST_COLUMN,
	COL_ACCOUNT = FIRST_COLUMN,
	COL_ACCOUNT_SELECT,
	COL_LABEL,
	COL_DEBIT,
	COL_CREDIT,
	N_COLUMNS
};

/*
 * helpers
 */
typedef struct {
	gint	        column_id;
	gchar          *letter;
	const gchar * (*get_label)( const ofoModel *, gint );
	gboolean      (*is_locked)( const ofoModel *, gint );
	gint            width;
	float           xalign;
	gboolean        expand;
}
	sColumnDef;

#define AMOUNTS_WIDTH                  10
#define RANG_WIDTH                      3
#define TOTAUX_TOP_MARGIN               8

/* space between widgets in a detail line */
#define DETAIL_SPACE                    2

/*
 * this works because column_id is greater than zero
 * and this is ok because the column #0 is used by the number of the row
 */
static sColumnDef st_col_defs[] = {
		{ COL_ACCOUNT,
				"A",
				ofo_model_get_detail_account,
				ofo_model_get_detail_account_locked, 10, 0.0, FALSE },
		{ COL_ACCOUNT_SELECT,
				NULL,
				NULL,
				NULL, 0, 0, FALSE },
		{ COL_LABEL,
				"L",
				ofo_model_get_detail_label,
				ofo_model_get_detail_label_locked,   20, 0.0, TRUE },
		{ COL_DEBIT,
				"D",
				ofo_model_get_detail_debit,
				ofo_model_get_detail_debit_locked,   AMOUNTS_WIDTH, 1.0, FALSE },
		{ COL_CREDIT,
				"C",
				ofo_model_get_detail_credit,
				ofo_model_get_detail_credit_locked,  AMOUNTS_WIDTH, 1.0, FALSE },
		{ 0 }
};

#define DATA_COLUMN				    "data-entry-left"
#define DATA_ROW				    "data-entry-top"

static const gchar  *st_ui_xml    = PKGUIDIR "/ofa-guided-input.ui";
static const gchar  *st_ui_id     = "GuidedInputDlg";

static GDate         st_last_dope = { 0 };
static GDate         st_last_deff = { 0 };

G_DEFINE_TYPE( ofaGuidedEx, ofa_guided_ex, OFA_TYPE_MAIN_PAGE )

static GtkWidget *v_setup_view( ofaMainPage *page );
static void       setup_from_dossier( ofaGuidedEx *self );
static GtkWidget *setup_view_left( ofaGuidedEx *self );
static GtkWidget *setup_left_treeview( ofaGuidedEx *self );
static void       on_left_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaGuidedEx *self );
static void       on_left_row_selected( GtkTreeSelection *selection, ofaGuidedEx *self );
static gint       on_left_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaGuidedEx *self );
static gboolean   on_left_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaGuidedEx *self );
static void       left_collapse_node( ofaGuidedEx *self, GtkWidget *widget );
static void       left_expand_node( ofaGuidedEx *self, GtkWidget *widget );
static void       on_left_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaGuidedEx *self );
static void       enable_left_select( ofaGuidedEx *self );
static gboolean   is_left_select_enableable( ofaGuidedEx *self );
static void       on_left_select_clicked( GtkButton *button, ofaGuidedEx *self );
static void       select_model( ofaGuidedEx *self );
static GtkWidget *setup_view_right( ofaGuidedEx *self );
static void       reparent_from_dialog( ofaGuidedEx *self, GtkContainer *parent );
static void       setup_journal_combo( ofaGuidedEx *self );
static void       setup_right_dates( ofaGuidedEx *self );
static void       init_journal_combo( ofaGuidedEx *self );
static void       init_view_entries( ofaGuidedEx *self );
static GtkWidget *v_setup_buttons( ofaMainPage *page );
static void       v_init_view( ofaMainPage *page );
static void       init_left_view( ofaGuidedEx *self, GtkWidget *child );
static void       insert_left_journal_row( ofaGuidedEx *self, ofoJournal *journal );
static void       insert_left_model_row( ofaGuidedEx *self, ofoModel *model );
static gboolean   find_left_journal_by_mnemo( ofaGuidedEx *self, const gchar *mnemo, GtkTreeModel **tmodel, GtkTreeIter *iter );
static void       add_row_entry( ofaGuidedEx *self, gint i );
static void       add_row_entry_set( ofaGuidedEx *self, gint col_id, gint row );
static void       add_button( ofaGuidedEx *self, const gchar *stock_id, gint column, gint row );
static void       remove_row_entry( ofaGuidedEx *self, gint row );
static const sColumnDef *find_column_def_from_col_id( ofaGuidedEx *self, gint col_id );
static const sColumnDef *find_column_def_from_letter( ofaGuidedEx *self, gchar letter );
static void       on_journal_changed( const gchar *mnemo, ofaGuidedEx *self );
static void       on_dope_changed( GtkEntry *entry, ofaGuidedEx *self );
static gboolean   on_dope_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedEx *self );
static gboolean   on_dope_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedEx *self );
static void       on_deffet_changed( GtkEntry *entry, ofaGuidedEx *self );
static gboolean   on_deffet_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedEx *self );
static gboolean   on_deffet_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedEx *self );
static void       on_account_selection( ofaGuidedEx *self, gint row );
static void       on_button_clicked( GtkButton *button, ofaGuidedEx *self );
static void       on_entry_changed( GtkEntry *entry, ofaGuidedEx *self );
static gboolean   on_entry_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedEx *self );
static gboolean   on_entry_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedEx *self );
static gboolean   on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaGuidedEx *self );
static void       check_for_account( ofaGuidedEx *self, GtkEntry *entry  );
static void       check_for_enable_dlg( ofaGuidedEx *self );
static gboolean   is_dialog_validable( ofaGuidedEx *self );
static void       update_all_formulas( ofaGuidedEx *self );
static void       update_formula( ofaGuidedEx *self, const gchar *formula, GtkEntry *entry );
static gdouble    compute_formula_solde( ofaGuidedEx *self, gint column_id, gint row );
static void       update_all_totals( ofaGuidedEx *self );
static gdouble    get_amount( ofaGuidedEx *self, gint col_id, gint row );
static gboolean   check_for_journal( ofaGuidedEx *self );
static gboolean   check_for_dates( ofaGuidedEx *self );
static gboolean   check_for_all_entries( ofaGuidedEx *self );
static gboolean   check_for_entry( ofaGuidedEx *self, gint row );
static void       set_comment( ofaGuidedEx *self, const gchar *comment );
static void       on_right_ok( GtkButton *button, ofaGuidedEx *self );
static gboolean   do_update( ofaGuidedEx *self );
static void       reset_entries_amounts( ofaGuidedEx *self );
static ofoEntry  *entry_from_detail( ofaGuidedEx *self, gint row, const gchar *piece );
static void       display_ok_message( ofaGuidedEx *self, gint count );

static void
guided_ex_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_guided_ex_finalize";
	ofaGuidedExPrivate *priv;

	g_return_if_fail( OFA_IS_GUIDED_EX( instance ));

	priv = OFA_GUIDED_EX( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	g_free( priv->journal );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_guided_ex_parent_class )->finalize( instance );
}

static void
guided_ex_dispose( GObject *instance )
{
	ofaGuidedExPrivate *priv;

	g_return_if_fail( OFA_IS_GUIDED_EX( instance ));

	priv = OFA_GUIDED_EX( instance )->private;

	if( !priv->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_guided_ex_parent_class )->dispose( instance );
}

static void
ofa_guided_ex_init( ofaGuidedEx *self )
{
	static const gchar *thisfn = "ofa_guided_ex_init";

	g_return_if_fail( OFA_IS_GUIDED_EX( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaGuidedExPrivate, 1 );

	g_date_clear( &self->private->dope, 1 );
	g_date_clear( &self->private->deff, 1 );
}

static void
ofa_guided_ex_class_init( ofaGuidedExClass *klass )
{
	static const gchar *thisfn = "ofa_guided_ex_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = guided_ex_dispose;
	G_OBJECT_CLASS( klass )->finalize = guided_ex_finalize;

	OFA_MAIN_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_MAIN_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_MAIN_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
}

static GtkWidget *
v_setup_view( ofaMainPage *page )
{
	GtkPaned *child;

	setup_from_dossier( OFA_GUIDED_EX( page ));

	child = GTK_PANED( gtk_paned_new( GTK_ORIENTATION_HORIZONTAL ));
	gtk_paned_add1( child, setup_view_left( OFA_GUIDED_EX( page )));
	gtk_paned_add2( child, setup_view_right( OFA_GUIDED_EX( page )));
	OFA_GUIDED_EX( page )->private->pane = child;

	return( GTK_WIDGET( child ));
}

static void
setup_from_dossier( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;
	const GDate *date;

	priv = self->private;

	priv->dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ));

	date = ofo_dossier_get_last_closed_exercice( priv->dossier );
	if( date && g_date_valid( date )){
		memcpy( &priv->last_closed_exe, date, sizeof( GDate ));
	} else {
		g_date_clear( &priv->last_closed_exe, 1 );
	}

}

/*
 * the left pane is a treeview whose level 0 are the journals, and
 * level 1 the entry models defined on the corresponding journal
 */
static GtkWidget *
setup_view_left( ofaGuidedEx *self )
{
	GtkFrame *frame;
	GtkWidget *tview;
	GtkBox *box, *box2;
	GtkButton *button;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 ));
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( box ));

	tview = setup_left_treeview( self );
	gtk_box_pack_start( box, tview, TRUE, TRUE, 0 );

	box2 = GTK_BOX( gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 4 ));
	gtk_box_pack_end( box, GTK_WIDGET( box2 ), FALSE, FALSE, 0 );

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_Select" )));
	gtk_widget_set_margin_bottom( GTK_WIDGET( button ), 4 );
	gtk_widget_set_margin_right( GTK_WIDGET( button ), 4 );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_left_select_clicked ), self );
	gtk_box_pack_end( box2, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	self->private->left_select = button;
	enable_left_select( self );

	return( GTK_WIDGET( frame ));
}

/*
 * the left pane is a treeview whose level 0 are the journals, and
 * level 1 the entry models defined on the corresponding journal
 */
static GtkWidget *
setup_left_treeview( ofaGuidedEx *self )
{
	GtkScrolledWindow *scroll;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_container_set_border_width( GTK_CONTAINER( scroll ), 4 );
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );

	tview = GTK_TREE_VIEW( gtk_tree_view_new());
	/*gtk_widget_set_vexpand( GTK_WIDGET( tview ), TRUE );*/
	gtk_tree_view_set_headers_visible( tview, FALSE );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( tview ));
	g_signal_connect(G_OBJECT( tview ), "row-activated", G_CALLBACK( on_left_row_activated ), self );
	g_signal_connect( G_OBJECT( tview ), "key-press-event", G_CALLBACK( on_left_key_pressed ), self );
	self->private->left_tview = tview;

	tmodel = GTK_TREE_MODEL( gtk_tree_store_new(
			LEFT_N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( tview, tmodel );
	g_object_unref( tmodel );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Mnemonic" ),
			text_cell, "text", LEFT_COL_MNEMO,
			NULL );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_left_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", LEFT_COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_left_cell_data_func, self, NULL );

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_left_row_selected ), self );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ), ( GtkTreeIterCompareFunc ) on_left_sort_model, self, NULL );

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	return( GTK_WIDGET( scroll ));
}

static void
on_left_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaGuidedEx *self )
{
	if( is_left_select_enableable( self )){
		select_model( self );
	}
}

static void
on_left_row_selected( GtkTreeSelection *selection, ofaGuidedEx *self )
{
	enable_left_select( self );
}

static gint
on_left_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaGuidedEx *self )
{
	return( 0 );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 *
 * Handles left and right arrows to expand/collapse nodes
 */
static gboolean
on_left_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaGuidedEx *self )
{
	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Left ){
			left_collapse_node( self, widget );
		} else if( event->keyval == GDK_KEY_Right ){
			left_expand_node( self, widget );
		}
	}

	return( FALSE );
}

static void
left_collapse_node( ofaGuidedEx *self, GtkWidget *widget )
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
left_expand_node( ofaGuidedEx *self, GtkWidget *widget )
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

/*
 * display yellow background the journal rows
 */
static void
on_left_cell_data_func( GtkTreeViewColumn *tcolumn,
							GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
							ofaGuidedEx *self )
{
	GObject *object;
	GdkRGBA color;

	g_return_if_fail( GTK_IS_CELL_RENDERER_TEXT( cell ));

	gtk_tree_model_get( tmodel, iter,
			LEFT_COL_OBJECT, &object,
			-1 );
	g_object_unref( object );

	g_object_set( G_OBJECT( cell ),
						"style-set", FALSE,
						"background-set", FALSE,
						NULL );

	g_return_if_fail( OFO_IS_JOURNAL( object ) || OFO_IS_MODEL( object ));

	if( OFO_IS_JOURNAL( object )){
		gdk_rgba_parse( &color, "#ffffb0" );
		g_object_set( G_OBJECT( cell ), "background-rgba", &color, NULL );
		g_object_set( G_OBJECT( cell ), "style", PANGO_STYLE_ITALIC, NULL );
	}
}

static void
enable_left_select( ofaGuidedEx *self )
{
	gtk_widget_set_sensitive(
			GTK_WIDGET( self->private->left_select ), is_left_select_enableable( self ));
}

static gboolean
is_left_select_enableable( ofaGuidedEx *self )
{
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBase *object;
	gboolean ok;

	ok = FALSE;

	select = gtk_tree_view_get_selection( self->private->left_tview );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, LEFT_COL_OBJECT, &object, -1 );
		g_object_unref( object );
		ok = OFO_IS_MODEL( object );
	}

	return( ok );
}

static void
on_left_select_clicked( GtkButton *button, ofaGuidedEx *self )
{
	select_model( self );
}

static void
select_model( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBase *object;
	gint i;

	priv = self->private;

	object = NULL;
	select = gtk_tree_view_get_selection( priv->left_tview );
	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, LEFT_COL_OBJECT, &object, -1 );
		g_object_unref( object );
	}
	g_return_if_fail( object && OFO_IS_MODEL( object ));

	/* clear the previous model */
	for( i=1 ; i<=priv->right_entries_count ; ++i ){
		remove_row_entry( self, i );
	}

	priv->model = OFO_MODEL( object );

	gtk_label_set_text(
				priv->right_model_label,
				ofo_model_get_label( priv->model ));

	init_journal_combo( self );
	init_view_entries( self );

	gtk_widget_show_all( gtk_paned_get_child2( priv->pane ));
}

/*
 * note that we may have no current ofoModel at this time
 */
static GtkWidget *
setup_view_right( ofaGuidedEx *self )
{
	GtkFrame *frame;
	ofaGuidedExPrivate *priv;
	GtkScrolledWindow *scroll;
	GtkWidget *widget;

	priv = self->private;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_NONE );

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_container_set_border_width( GTK_CONTAINER( scroll ), 0 );
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( scroll ));

	/* setup the box from the ofaGuidedInput dialog */
	reparent_from_dialog( self, GTK_CONTAINER( scroll ));

	widget = my_utils_container_get_child_by_name( priv->right_box, "p1-model-label" );
	g_return_val_if_fail( widget && GTK_IS_LABEL( widget ), NULL );
	priv->right_model_label = GTK_LABEL( widget );

	setup_journal_combo( self );
	setup_right_dates( self );

	widget = my_utils_container_get_child_by_name( priv->right_box, "p1-entries" );
	g_return_val_if_fail( widget && GTK_IS_GRID( widget ), NULL );
	priv->right_entries_grid = GTK_GRID( widget );

	widget = my_utils_container_get_child_by_name( priv->right_box, "p1-comment" );
	g_return_val_if_fail( widget && GTK_IS_ENTRY( widget ), NULL );
	priv->right_comment = GTK_ENTRY( widget );

	widget = my_utils_container_get_child_by_name( priv->right_box, "box-ok" );
	g_return_val_if_fail( widget && GTK_IS_BUTTON( widget ), NULL );
	priv->right_ok = GTK_BUTTON( widget );
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_right_ok ), self );

	check_for_enable_dlg( self );

	return( GTK_WIDGET( frame ));
}

static void
reparent_from_dialog( ofaGuidedEx *self, GtkContainer *parent )
{
	GtkWidget *dialog;
	GtkWidget *box;

	/* load our dialog */
	dialog = my_utils_builder_load_from_path( st_ui_xml, st_ui_id );
	g_return_if_fail( dialog && GTK_IS_WINDOW( dialog ));

	box = my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "px-box" );
	g_return_if_fail( box && GTK_IS_BOX( box ));
	self->private->right_box = GTK_CONTAINER( box );

	/* attach our box to the parent's frame */
	gtk_widget_reparent( box, GTK_WIDGET( parent ));
}

static void
setup_journal_combo( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;
	ofaJournalComboParms parms;
	GtkWidget *combo;

	priv = self->private;

	parms.container = priv->right_box;
	parms.dossier = priv->dossier;
	parms.combo_name = "p1-journal";
	parms.label_name = NULL;
	parms.disp_mnemo = FALSE;
	parms.disp_label = TRUE;
	parms.pfn = ( ofaJournalComboCb ) on_journal_changed;
	parms.user_data = self;
	parms.initial_mnemo = NULL;

	priv->right_journal_combo = ofa_journal_combo_init_combo( &parms );

	combo = my_utils_container_get_child_by_name( priv->right_box, "p1-journal" );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));
	gtk_widget_set_sensitive( combo, priv->model && !ofo_model_get_journal_locked( priv->model ));
}

static void
setup_right_dates( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;
	GtkWidget *entry;
	gchar *str;

	priv = self->private;

	memcpy( &priv->dope, &st_last_dope, sizeof( GDate ));
	str = my_utils_display_from_date( &priv->dope, MY_UTILS_DATE_DDMM );
	entry = my_utils_container_get_child_by_name( priv->right_box, "p1-dope" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	gtk_entry_set_text( GTK_ENTRY( entry ), str );
	g_free( str );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_dope_changed ), self );
	g_signal_connect( G_OBJECT( entry ), "focus-in-event", G_CALLBACK( on_dope_focus_in ), self );
	g_signal_connect( G_OBJECT( entry ), "focus-out-event", G_CALLBACK( on_dope_focus_out ), self );
	priv->right_dope = GTK_ENTRY( entry );

	memcpy( &priv->deff, &st_last_deff, sizeof( GDate ));
	str = my_utils_display_from_date( &priv->deff, MY_UTILS_DATE_DDMM );
	entry = my_utils_container_get_child_by_name( priv->right_box, "p1-deffet" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	gtk_entry_set_text( GTK_ENTRY( entry ), str );
	g_free( str );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_deffet_changed ), self );
	g_signal_connect( G_OBJECT( entry ), "focus-in-event", G_CALLBACK( on_deffet_focus_in ), self );
	g_signal_connect( G_OBJECT( entry ), "focus-out-event", G_CALLBACK( on_deffet_focus_out ), self );
	priv->right_deffet = GTK_ENTRY( entry );
}

static void
init_journal_combo( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;
	GtkWidget *combo;

	priv = self->private;

	ofa_journal_combo_set_selection( priv->right_journal_combo, ofo_model_get_journal( priv->model ));

	combo = my_utils_container_get_child_by_name( priv->right_box, "p1-journal" );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));

	gtk_widget_set_sensitive( combo, priv->model && !ofo_model_get_journal_locked( priv->model ));
}

static void
init_view_entries( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;
	gint count,i;
	GtkLabel *label;
	GtkEntry *entry;

	priv = self->private;

	count = ofo_model_get_detail_count( priv->model );
	for( i=0 ; i<count ; ++i ){
		add_row_entry( self, i );
	}

	label = GTK_LABEL( gtk_label_new( _( "Total :" )));
	gtk_widget_set_sensitive( GTK_WIDGET( label ), FALSE );
	gtk_widget_set_margin_top( GTK_WIDGET( label ), TOTAUX_TOP_MARGIN );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( priv->right_entries_grid, GTK_WIDGET( label ), COL_LABEL, count+1, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_widget_set_margin_top( GTK_WIDGET( entry ), TOTAUX_TOP_MARGIN );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_width_chars( entry, AMOUNTS_WIDTH );
	gtk_grid_attach( priv->right_entries_grid, GTK_WIDGET( entry ), COL_DEBIT, count+1, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_widget_set_margin_top( GTK_WIDGET( entry ), TOTAUX_TOP_MARGIN );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_width_chars( entry, AMOUNTS_WIDTH );
	gtk_grid_attach( priv->right_entries_grid, GTK_WIDGET( entry ), COL_CREDIT, count+1, 1, 1 );

	label = GTK_LABEL( gtk_label_new( _( "Diff :" )));
	gtk_widget_set_sensitive( GTK_WIDGET( label ), FALSE );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( priv->right_entries_grid, GTK_WIDGET( label ), COL_LABEL, count+2, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_width_chars( entry, AMOUNTS_WIDTH );
	gtk_grid_attach( priv->right_entries_grid, GTK_WIDGET( entry ), COL_DEBIT, count+2, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_width_chars( entry, AMOUNTS_WIDTH );
	gtk_grid_attach( priv->right_entries_grid, GTK_WIDGET( entry ), COL_CREDIT, count+2, 1, 1 );

	priv->right_entries_count = count+2;
}

static GtkWidget *
v_setup_buttons( ofaMainPage *page )
{
	return( NULL );
}

static void
v_init_view( ofaMainPage *page )
{
	init_left_view( OFA_GUIDED_EX( page ),
						gtk_paned_get_child1( OFA_GUIDED_EX( page )->private->pane ));
}

static void
init_left_view( ofaGuidedEx *self, GtkWidget *child )
{
	GList *dataset, *ise;

	dataset = ofo_journal_get_dataset( self->private->dossier );
	for( ise=dataset ; ise ; ise=ise->next ){
		insert_left_journal_row( self, OFO_JOURNAL( ise->data ));
	}

	dataset = ofo_model_get_dataset( self->private->dossier );
	for( ise=dataset ; ise ; ise=ise->next ){
		insert_left_model_row( self, OFO_MODEL( ise->data ));
	}
}

static void
insert_left_journal_row( ofaGuidedEx *self, ofoJournal *journal )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	tmodel = gtk_tree_view_get_model( self->private->left_tview );
	gtk_tree_store_insert_with_values(
			GTK_TREE_STORE( tmodel ),
			&iter,
			NULL,
			-1,
			LEFT_COL_MNEMO, ofo_journal_get_mnemo( journal ),
			LEFT_COL_LABEL, ofo_journal_get_label( journal ),
			LEFT_COL_OBJECT, journal,
			-1 );
}

static void
insert_left_model_row( ofaGuidedEx *self, ofoModel *model )
{
	static const gchar *thisfn = "ofa_guided_ex_insert_left_model_row";
	GtkTreeModel *tmodel;
	GtkTreeIter parent_iter, iter;
	gboolean found;

	found = find_left_journal_by_mnemo( self, ofo_model_get_journal( model ), &tmodel, &parent_iter );
	if( !found ){
		g_debug( "%s: unable to find journal %s for model %s",
				thisfn,
				ofo_model_get_journal( model ),
				ofo_model_get_mnemo( model ));
	}

	tmodel = gtk_tree_view_get_model( self->private->left_tview );
	gtk_tree_store_insert_with_values(
			GTK_TREE_STORE( tmodel ),
			&iter,
			found ? &parent_iter : NULL,
			-1,
			LEFT_COL_MNEMO, ofo_model_get_mnemo( model ),
			LEFT_COL_LABEL, ofo_model_get_label( model ),
			LEFT_COL_OBJECT, model,
			-1 );
}

/*
 * returns TRUE if found
 */
static gboolean
find_left_journal_by_mnemo( ofaGuidedEx *self, const gchar *mnemo, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	GtkTreeModel *my_tmodel;
	GtkTreeIter my_iter;
	gchar *journal;

	my_tmodel = gtk_tree_view_get_model( self->private->left_tview );
	if( tmodel ){
		*tmodel = my_tmodel;
	}
	if( gtk_tree_model_get_iter_first( my_tmodel, &my_iter )){
		while( TRUE ){
			gtk_tree_model_get( my_tmodel, &my_iter, LEFT_COL_MNEMO, &journal, -1 );
			if( !g_utf8_collate( mnemo, journal )){
				if( iter ){
					*iter = my_iter;
				}
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( my_tmodel, &my_iter )){
				break;
			}
		}
	}

	return( FALSE );
}

static void
add_row_entry( ofaGuidedEx *self, gint i )
{
	GtkEntry *entry;
	gchar *str;

	/* col #0: rang: number of the entry */
	str = g_strdup_printf( "%2d", i+1 );
	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_text( entry, str );
	gtk_entry_set_width_chars( entry, RANG_WIDTH );
	gtk_grid_attach( self->private->right_entries_grid, GTK_WIDGET( entry ), COL_RANG, i+1, 1, 1 );
	g_free( str );

	/* other columns starting with COL_ACCOUNT=1 */
	add_row_entry_set( self, COL_ACCOUNT, i+1 );
	add_button( self, GTK_STOCK_INDEX, COL_ACCOUNT_SELECT, i+1 );
	add_row_entry_set( self, COL_LABEL, i+1 );
	add_row_entry_set( self, COL_DEBIT, i+1 );
	add_row_entry_set( self, COL_CREDIT, i+1 );
}

static void
add_row_entry_set( ofaGuidedEx *self, gint col_id, gint row )
{
	GtkEntry *entry;
	const sColumnDef *col_def;
	const gchar *str;
	gboolean locked;

	col_def = find_column_def_from_col_id( self, col_id );
	g_return_if_fail( col_def );

	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_hexpand( GTK_WIDGET( entry ), col_def->expand );
	gtk_entry_set_width_chars( entry, col_def->width );
	gtk_entry_set_alignment( entry, col_def->xalign );

	str = (*col_def->get_label)( self->private->model, row-1 );

	if( str && !ofo_model_detail_is_formula( str )){
		gtk_entry_set_text( entry, str );
	}

	locked = (*col_def->is_locked)( self->private->model, row-1 );
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), !locked );

	g_object_set_data( G_OBJECT( entry ), DATA_COLUMN, GINT_TO_POINTER( col_id ));
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));

	if( !locked ){
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_entry_changed ), self );
		g_signal_connect( G_OBJECT( entry ), "focus-in-event", G_CALLBACK( on_entry_focus_in ), self );
		g_signal_connect( G_OBJECT( entry ), "focus-out-event", G_CALLBACK( on_entry_focus_out ), self );
		g_signal_connect( G_OBJECT( entry ), "key-press-event", G_CALLBACK( on_key_pressed ), self );
	}

	gtk_grid_attach( self->private->right_entries_grid, GTK_WIDGET( entry ), col_id, row, 1, 1 );
}

static void
remove_row_entry( ofaGuidedEx *self, gint row )
{
	gint i;
	GtkWidget *widget;

	for( i=0 ; i<N_COLUMNS ; ++i ){
		widget = gtk_grid_get_child_at( self->private->right_entries_grid, i, row );
		if( widget ){
			gtk_widget_destroy( widget );
		}
	}
}

static void
add_button( ofaGuidedEx *self, const gchar *stock_id, gint column, gint row )
{
	GtkWidget *image;
	GtkButton *button;

	image = gtk_image_new_from_stock( stock_id, GTK_ICON_SIZE_BUTTON );
	button = GTK_BUTTON( gtk_button_new());
	g_object_set_data( G_OBJECT( button ), DATA_COLUMN, GINT_TO_POINTER( column ));
	g_object_set_data( G_OBJECT( button ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_button_set_image( button, image );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_button_clicked ), self );
	gtk_grid_attach( self->private->right_entries_grid, GTK_WIDGET( button ), column, row, 1, 1 );
}

static const sColumnDef *
find_column_def_from_col_id( ofaGuidedEx *self, gint col_id )
{
	gint i;

	for( i=0 ; st_col_defs[i].column_id ; ++i ){
		if( st_col_defs[i].column_id == col_id ){
			return(( const sColumnDef * ) &st_col_defs[i] );
		}
	}

	return( NULL );
}

static const sColumnDef *
find_column_def_from_letter( ofaGuidedEx *self, gchar letter )
{
	gint i;

	for( i=0 ; st_col_defs[i].column_id ; ++i ){
		if( st_col_defs[i].letter[0] == letter ){
			return(( const sColumnDef * ) &st_col_defs[i] );
		}
	}

	return( NULL );
}

/*
 * ofaJournalCombo callback
 *
 * setup the last closing date as the maximum of :
 * - the last exercice closing date
 * - the last journal closing date
 *
 * this last closing date is the lower limit of the effect dates
 */
static void
on_journal_changed( const gchar *mnemo, ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;
	ofoJournal *journal;
	gint exe_id;
	const GDate *date;

	priv = self->private;

	g_free( priv->journal );
	priv->journal = g_strdup( mnemo );
	journal = ofo_journal_get_by_mnemo( priv->dossier, mnemo );
	g_date_clear( &priv->last_closed_jou, 1 );

	memcpy( &priv->last_closing, &priv->last_closed_exe, sizeof( GDate ));

	if( journal ){
		exe_id = ofo_dossier_get_current_exe_id( priv->dossier );
		date = ofo_journal_get_cloture( journal, exe_id );

		if( date && g_date_valid( date )){
			memcpy( &priv->last_closed_jou, date, sizeof( GDate ));

			if( g_date_valid( &priv->last_closed_exe )){
				if( g_date_compare( date, &priv->last_closed_exe ) > 0 ){
					memcpy( &priv->last_closing, date, sizeof( GDate ));
				}
			} else {
				memcpy( &priv->last_closing, date, sizeof( GDate ));
			}
		}
	}

	check_for_enable_dlg( self );
}

/*
 * setting the deffet also triggers the change signal of the deffet
 * field (and so the comment)
 * => we should only react to the content while the focus is in the
 *    field
 * More, we shouldn't triggers an automatic changes to a field which
 * has been manually modified
 */
static void
set_date_comment( ofaGuidedEx *self, const gchar *label, const GDate *date )
{
	gchar *str, *comment;

	str = my_utils_display_from_date( date, MY_UTILS_DATE_DMMM );
	if( !g_utf8_strlen( str, -1 )){
		g_free( str );
		str = g_strdup( _( "invalid" ));
	}
	comment = g_strdup_printf( "%s : %s", label, str );
	set_comment( self, comment );
	g_free( comment );
	g_free( str );
}

static void
on_dope_changed( GtkEntry *entry, ofaGuidedEx *self )
{
	/*static const gchar *thisfn = "ofa_guided_ex_on_dope_changed";*/
	ofaGuidedExPrivate *priv;
	gchar *str;

	/*g_debug( "%s: entry=%p, self=%p", thisfn, ( void * ) entry, ( void * ) self );*/

	priv = self->private;

	/* check the operation date */
	g_date_set_parse( &priv->dope, gtk_entry_get_text( entry ));
	set_date_comment( self, _( "Operation date" ), &priv->dope );

	/* setup the effect date if it has not been manually changed */
	if( g_date_valid( &priv->dope ) &&
			!priv->deffet_changed_while_focus ){

		if( g_date_valid( &priv->last_closing ) &&
			g_date_compare( &priv->last_closing, &priv->dope ) > 0 ){

			memcpy( &priv->deff, &priv->last_closing, sizeof( GDate ));
			g_date_add_days( &priv->deff, 1 );

		} else {
			memcpy( &priv->deff, &priv->dope, sizeof( GDate ));
		}

		str = my_utils_display_from_date( &priv->deff, MY_UTILS_DATE_DDMM );
		gtk_entry_set_text( priv->right_deffet, str );
		g_free( str );
	}

	check_for_enable_dlg( self );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_dope_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedEx *self )
{
	set_date_comment( self, _( "Operation date" ), &self->private->dope );

	return( FALSE );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_dope_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedEx *self )
{
	set_comment( self, "" );

	return( FALSE );
}

static void
on_deffet_changed( GtkEntry *entry, ofaGuidedEx *self )
{
	/*static const gchar *thisfn = "ofa_guided_ex_on_deffet_changed";*/

	/* the focus is not set to the entry
	widget = gtk_window_get_focus( GTK_WINDOW( self->private->main_window ));
	g_debug( "%s: entry=%p, self=%p, focus_widget=%p",
			thisfn, ( void * ) entry, ( void * ) self, ( void * ) widget );*/

	if( self->private->deffet_has_focus ){

		self->private->deffet_changed_while_focus = TRUE;
		g_date_set_parse( &self->private->deff, gtk_entry_get_text( entry ));
		set_date_comment( self, _( "Effect date" ), &self->private->deff );

		check_for_enable_dlg( self );
	}
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_deffet_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedEx *self )
{
	self->private->deffet_has_focus = TRUE;
	set_date_comment( self, _( "Effect date" ), &self->private->deff );

	return( FALSE );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_deffet_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedEx *self )
{
	self->private->deffet_has_focus = FALSE;
	set_comment( self, "" );

	return( FALSE );
}

static void
on_account_selection( ofaGuidedEx *self, gint row )
{
	GtkEntry *entry;
	gchar *number;

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->right_entries_grid, COL_ACCOUNT, row ));

	number = ofa_account_select_run(
						ofa_main_page_get_main_window( OFA_MAIN_PAGE( self )),
						gtk_entry_get_text( entry ));

	if( number && g_utf8_strlen( number, -1 )){
		gtk_entry_set_text( entry, number );
	}

	g_free( number );
}

static void
on_button_clicked( GtkButton *button, ofaGuidedEx *self )
{
	gint column, row;

	column = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_COLUMN ));
	row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_ROW ));

	switch( column ){
		case COL_ACCOUNT_SELECT:
			on_account_selection( self, row );
			break;
	}
}

static void
on_entry_changed( GtkEntry *entry, ofaGuidedEx *self )
{
	check_for_enable_dlg( self );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_entry_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedEx *self )
{
	gint row;
	const gchar *comment;

	row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( entry ), DATA_ROW ));
	if( row > 0 ){
		comment = ofo_model_get_detail_comment( self->private->model, row-1 );
		set_comment( self, comment ? comment : "" );
	}

	return( FALSE );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_entry_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedEx *self )
{
	set_comment( self, "" );

	return( FALSE );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaGuidedEx *self )
{
	gint left;

	/* check the entry we are leaving
	 */
	left = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( widget ), DATA_COLUMN ));

	switch( left ){
		case COL_ACCOUNT:
			if( event->state == 0 && event->keyval == GDK_KEY_Tab ){
				check_for_account( self, GTK_ENTRY( widget ));
			}
			break;
	}

	return( FALSE );
}

/*
 * check that the account exists
 * else open a dialog for selection
 */
static void
check_for_account( ofaGuidedEx *self, GtkEntry *entry  )
{
	ofoAccount *account;
	const gchar *asked_account;
	gchar *number;

	asked_account = gtk_entry_get_text( entry );
	account = ofo_account_get_by_number( self->private->dossier, asked_account );
	if( !account || ofo_account_is_root( account )){
		number = ofa_account_select_run(
							ofa_main_page_get_main_window( OFA_MAIN_PAGE( self )),
							asked_account );
		if( number ){
			gtk_entry_set_text( entry, number );
			g_free( number );
		}
	}
}

/*
 * this is called after each field changes
 * so a good place to handle all modifications
 *
 * Note that we control *all* fields so that we are able to visually
 * highlight the erroneous ones
 */
static void
check_for_enable_dlg( ofaGuidedEx *self )
{
	gtk_widget_set_sensitive(
			GTK_WIDGET( self->private->right_ok ), is_dialog_validable( self ));
}

static gboolean
is_dialog_validable( ofaGuidedEx *self )
{
	gboolean ok;

	ok = FALSE;

	if( self->private->model ){

		update_all_formulas( self );
		update_all_totals( self );

		ok = TRUE;
		ok &= check_for_journal( self );
		ok &= check_for_dates( self );
		ok &= check_for_all_entries( self );
	}

	return( ok );
}

static void
update_all_formulas( ofaGuidedEx *self )
{
	gint count, idx, col_id;
	const sColumnDef *col_def;
	const gchar *str;
	GtkWidget *entry;

	count = ofo_model_get_detail_count( self->private->model );
	for( idx=0 ; idx<count ; ++idx ){
		for( col_id=FIRST_COLUMN ; col_id<N_COLUMNS ; ++col_id ){
			col_def = find_column_def_from_col_id( self, col_id );
			if( col_def && col_def->get_label ){
				str = ( *col_def->get_label )( self->private->model, idx );
				if( ofo_model_detail_is_formula( str )){
					entry = gtk_grid_get_child_at( self->private->right_entries_grid, col_id, idx+1 );
					if( entry && GTK_IS_ENTRY( entry )){
						update_formula( self, str, GTK_ENTRY( entry ));
					}
				}
			}
		}
	}
}

static void
update_formula( ofaGuidedEx *self, const gchar *formula, GtkEntry *entry )
{
	static const gchar *thisfn = "ofa_guided_ex_update_formula";
	gchar **tokens, **iter;
	gchar init;
	ofoTaux *rate;
	gdouble solde;
	gchar *str;
	gint col, row;
	GtkWidget *widget;
	const sColumnDef *col_def;

	tokens = g_regex_split_simple( "[-+*/]", formula+1, 0, 0 );
	iter = tokens;
	g_debug( "%s: formula='%s'", thisfn, formula );
	while( *iter ){
		/*g_debug( "%s: iter='%s'", thisfn, *iter );*/
		/*
		 * we have:
		 * - [ALDC]<number>
		 *    A: account
		 *     L: label
		 *      D: debit
		 *       C: credit
		 * or:
		 * - a token:
		 *   SOLDE
		 *   IDEM (same column, previous row)
		 * or:
		 * - a rate mnemonic
		 */
		if( !g_utf8_collate( *iter, "SOLDE" )){
			col = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( entry ), DATA_COLUMN ));
			row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( entry ), DATA_ROW ));
			solde = compute_formula_solde( self, col, row );
			str = g_strdup_printf( "%.2lf", solde );
			gtk_entry_set_text( entry, str );
			g_free( str );

		} else if( !g_utf8_collate( *iter, "IDEM" )){
			row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( entry ), DATA_ROW ));
			col = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( entry ), DATA_COLUMN ));
			widget = gtk_grid_get_child_at( self->private->right_entries_grid, col, row-1 );
			if( widget && GTK_IS_ENTRY( widget )){
				gtk_entry_set_text( entry, gtk_entry_get_text( GTK_ENTRY( widget )));
			}

		} else {
			init = *iter[0];
			row = atoi(( *iter )+1 );
			/*g_debug( "%s: init=%c row=%d", thisfn, init, row );*/
			if( row > 0 && row <= ofo_model_get_detail_count( self->private->model )){
				col_def = find_column_def_from_letter( self, init );
				if( col_def ){
					widget = gtk_grid_get_child_at( self->private->right_entries_grid, col_def->column_id, row );
					if( widget && GTK_IS_ENTRY( widget )){
						gtk_entry_set_text( entry, gtk_entry_get_text( GTK_ENTRY( widget )));
					} else {
						g_warning( "%s: no entry found at col=%d, row=%d", thisfn, col_def->column_id, row );
					}
				} else {
					g_warning( "%s: col_def not found for '%c' letter", thisfn, init );
				}

			} else {
				g_debug( "%s: searching for taux %s", thisfn, *iter );
				rate = ofo_taux_get_by_mnemo( self->private->dossier, *iter );
				if( rate && OFO_IS_TAUX( rate )){
					g_warning( "%s: TODO", thisfn );
				} else {
					g_warning( "%s: taux %s not found", thisfn, *iter );
				}
			}
		}
		iter++;
	}

	g_strfreev( tokens );
}

static gdouble
compute_formula_solde( ofaGuidedEx *self, gint column_id, gint row )
{
	gdouble dsold, csold;
	gint count, idx;

	count = ofo_model_get_detail_count( self->private->model );
	csold = 0.0;
	dsold = 0.0;
	for( idx=0 ; idx<count ; ++idx ){
		if( column_id != COL_DEBIT || row != idx+1 ){
			dsold += get_amount( self, COL_DEBIT, idx+1 );
		}
		if( column_id != COL_CREDIT || row != idx+1 ){
			csold += get_amount( self, COL_CREDIT, idx+1 );
		}
	}

	return( column_id == COL_DEBIT ? csold-dsold : dsold-csold );
}

/*
 * totals and diffs are set are rows (count+1) and (count+2) respectively
 */
static void
update_all_totals( ofaGuidedEx *self )
{
	gdouble dsold, csold;
	gint count, idx;
	GtkWidget *entry;
	gchar *str, *str2;

	count = ofo_model_get_detail_count( self->private->model );
	csold = 0.0;
	dsold = 0.0;
	for( idx=0 ; idx<count ; ++idx ){
		dsold += get_amount( self, COL_DEBIT, idx+1 );
		csold += get_amount( self, COL_CREDIT, idx+1 );
	}

	self->private->total_debits = dsold;
	self->private->total_credits = csold;

	entry = gtk_grid_get_child_at( self->private->right_entries_grid, COL_DEBIT, count+1 );
	if( entry && GTK_IS_ENTRY( entry )){
		str = g_strdup_printf( "%.2lf", dsold );
		gtk_entry_set_text( GTK_ENTRY( entry ), str );
		g_free( str );
	}

	entry = gtk_grid_get_child_at( self->private->right_entries_grid, COL_CREDIT, count+1 );
	if( entry && GTK_IS_ENTRY( entry )){
		str = g_strdup_printf( "%.2lf", csold );
		gtk_entry_set_text( GTK_ENTRY( entry ), str );
		g_free( str );
	}

	if( dsold > csold ){
		str = g_strdup_printf( "%.2lf", dsold-csold );
		str2 = g_strdup( "" );
	} else if( dsold < csold ){
		str = g_strdup( "" );
		str2 = g_strdup_printf( "%.2lf", csold-dsold );
	} else {
		str = g_strdup( "" );
		str2 = g_strdup( "" );
	}

	entry = gtk_grid_get_child_at( self->private->right_entries_grid, COL_DEBIT, count+2 );
	if( entry && GTK_IS_ENTRY( entry )){
		gtk_entry_set_text( GTK_ENTRY( entry ), str2 );
	}

	entry = gtk_grid_get_child_at( self->private->right_entries_grid, COL_CREDIT, count+2 );
	if( entry && GTK_IS_ENTRY( entry )){
		gtk_entry_set_text( GTK_ENTRY( entry ), str );
	}

	g_free( str );
	g_free( str2 );
}

static gdouble
get_amount( ofaGuidedEx *self, gint col_id, gint row )
{
	const sColumnDef *col_def;
	GtkWidget *entry;

	col_def = find_column_def_from_col_id( self, col_id );
	if( col_def ){
		entry = gtk_grid_get_child_at( self->private->right_entries_grid, col_def->column_id, row );
		if( entry && GTK_IS_ENTRY( entry )){
			return( g_strtod( gtk_entry_get_text( GTK_ENTRY( entry )), NULL ));
		}
	}

	return( 0.0 );
}

/*
 * Returns TRUE if a journal is set
 */
static gboolean
check_for_journal( ofaGuidedEx *self )
{
	static const gchar *thisfn = "ofa_guided_ex_check_for_journal";
	gboolean ok;

	ok = self->private->journal && g_utf8_strlen( self->private->journal, -1 );
	if( !ok ){
		g_debug( "%s: journal=%s", thisfn, self->private->journal );
	}

	return( ok );
}

/*
 * Returns TRUE if the dates are set and valid
 *
 * The first valid effect date is older than:
 * - the last exercice closing date of the dossier (if set)
 * - the last closing date of the journal (if set)
 */
static gboolean
check_for_dates( ofaGuidedEx *self )
{
	static const gchar *thisfn = "ofa_guided_ex_check_for_dates";
	ofaGuidedExPrivate *priv;
	gboolean ok, oki;

	ok = TRUE;
	priv = self->private;

	oki = g_date_valid( &priv->dope );
	my_utils_entry_set_valid( priv->right_dope, oki );
	ok &= oki;
	if( !oki ){
		g_debug( "%s: operation date is invalid", thisfn );
	}

	oki = g_date_valid( &priv->deff );
	my_utils_entry_set_valid( priv->right_deffet, oki );
	ok &= oki;
	if( !oki ){
		g_debug( "%s: effect date is invalid", thisfn );
	}

	if( g_date_valid( &priv->last_closing )){
		oki = g_date_compare( &priv->last_closing, &priv->deff ) < 0;
		ok &= oki;
		if( !oki ){
			g_debug( "%s: effect date less than last closing", thisfn );
		}
	}

	return( ok );
}

/*
 * Returns TRUE if the entries are valid:
 * - for entries which have a not-nul balance:
 *   > account is valid
 *   > label is set
 * - totals are the same (no diff) and not nuls
 *
 * Note that have to check *all* entries in order to be able to visually
 * highlight the erroneous fields
 */
static gboolean
check_for_all_entries( ofaGuidedEx *self )
{
	static const gchar *thisfn = "ofa_guided_ex_check_for_all_entries";
	gboolean ok, oki;
	gint count, idx;
	gdouble deb, cred;

	ok = TRUE;
	count = ofo_model_get_detail_count( self->private->model );

	for( idx=0 ; idx<count ; ++idx ){
		deb = get_amount( self, COL_DEBIT, idx+1 );
		cred = get_amount( self, COL_CREDIT, idx+1 );
		if( deb+cred != 0.0 ){
			if( !check_for_entry( self, idx+1 )){
				ok = FALSE;
			}
		}
	}

	oki = self->private->total_debits == self->private->total_credits;
	ok &= oki;
	if( !oki ){
		g_debug( "%s: totals are not equal: debits=%2.lf, credits=%.2lf",
				thisfn, self->private->total_debits, self->private->total_credits );
	}

	oki= (self->private->total_debits != 0.0 || self->private->total_credits != 0.0 );
	ok &= oki;
	if( !oki ){
		g_debug( "%s: one total is nul: debits=%2.lf, credits=%.2lf",
				thisfn, self->private->total_debits, self->private->total_credits );
	}

	return( ok );
}

static gboolean
check_for_entry( ofaGuidedEx *self, gint row )
{
	static const gchar *thisfn = "ofa_guided_ex_check_for_entry";
	gboolean ok, oki;
	GtkWidget *entry;
	ofoAccount *account;
	const gchar *str;

	ok = TRUE;

	entry = gtk_grid_get_child_at( self->private->right_entries_grid, COL_ACCOUNT, row );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );

	account = ofo_account_get_by_number(
						self->private->dossier,
						gtk_entry_get_text( GTK_ENTRY( entry )));
	oki = ( account && OFO_IS_ACCOUNT( account ));
	ok &= oki;
	if( !oki ){
		g_debug( "%s: account number=%s", thisfn, gtk_entry_get_text( GTK_ENTRY( entry )));
	}

	entry = gtk_grid_get_child_at( self->private->right_entries_grid, COL_LABEL, row );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	str = gtk_entry_get_text( GTK_ENTRY( entry ));
	oki = ( str && g_utf8_strlen( str, -1 ));
	ok &= oki;
	if( !oki ){
		g_debug( "%s: label=%s", thisfn, gtk_entry_get_text( GTK_ENTRY( entry )));
	}

	return( ok );
}

static void
set_comment( ofaGuidedEx *self, const gchar *comment )
{
	gtk_entry_set_text( self->private->right_comment, comment );
}

/*
 * the right bottom "OK" has been clicked:
 *  try to validate and generate the entries
 */
static void
on_right_ok( GtkButton *button, ofaGuidedEx *self )
{
	if( do_update( self )){
		reset_entries_amounts( self );
	}
}

static void
reset_entries_amounts( ofaGuidedEx *self )
{
	gint i;
	GtkWidget *entry;

	for( i=1 ; i<=self->private->right_entries_count ; ++i ){
		entry = gtk_grid_get_child_at( self->private->right_entries_grid, COL_DEBIT, i );
		gtk_entry_set_text( GTK_ENTRY( entry ), "" );
		entry = gtk_grid_get_child_at( self->private->right_entries_grid, COL_CREDIT, i );
		gtk_entry_set_text( GTK_ENTRY( entry ), "" );
	}
}

/*
 * generate the entries
 * all the entries are created in memory and checked before being
 * serialized. Only after that, journal and accounts are updated.
 */
static gboolean
do_update( ofaGuidedEx *self )
{
	ofaGuidedExPrivate *priv;
	gboolean ok;
	GtkWidget *entry;
	gint count, idx;
	gdouble deb, cred;
	const gchar *piece;
	GList *entries, *it;
	ofoEntry *record;
	gint errors;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = self->private;

	piece = NULL;
	entry = my_utils_container_get_child_by_name( priv->right_box, "p1-piece" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	piece = gtk_entry_get_text( GTK_ENTRY( entry ));

	count = ofo_model_get_detail_count( priv->model );
	entries = NULL;
	errors = 0;
	ok = FALSE;

	for( idx=0 ; idx<count ; ++idx ){
		deb = get_amount( self, COL_DEBIT, idx+1 );
		cred = get_amount( self, COL_CREDIT, idx+1 );
		if( deb+cred != 0.0 ){
			record = entry_from_detail( self, idx+1, piece );
			if( record ){
				entries = g_list_append( entries, record );
			} else {
				errors += 1;
			}
		}
	}

	if( !errors ){
		ok = TRUE;
		for( it=entries ; it ; it=it->next ){
			ok &= ofo_entry_insert( OFO_ENTRY( it->data ), priv->dossier );
			/* TODO:
			 * in case of an error, remove the already recorded entries
			 * of the list, decrementing the journals and the accounts
			 * then restore the last ecr number of the dossier
			 */
		}
		if( ok ){
			display_ok_message( self, g_list_length( entries ));
		}
	}

	g_list_free_full( entries, g_object_unref );

	memcpy( &st_last_dope, &self->private->dope, sizeof( GDate ));
	memcpy( &st_last_deff, &self->private->deff, sizeof( GDate ));

	return( ok );
}

/*
 * create an entry in memory, adding it to the list
 */
static ofoEntry *
entry_from_detail( ofaGuidedEx *self, gint row, const gchar *piece )
{
	GtkWidget *entry;
	const gchar *account_number;
	ofoAccount *account;
	const gchar *label;
	gdouble deb, cre;

	entry = gtk_grid_get_child_at( self->private->right_entries_grid, COL_ACCOUNT, row );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), NULL );
	account_number = gtk_entry_get_text( GTK_ENTRY( entry ));
	account = ofo_account_get_by_number(
						self->private->dossier,
						account_number );
	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), NULL );

	entry = gtk_grid_get_child_at( self->private->right_entries_grid, COL_LABEL, row );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), NULL );
	label = gtk_entry_get_text( GTK_ENTRY( entry ));
	g_return_val_if_fail( label && g_utf8_strlen( label, -1 ), NULL );

	deb = get_amount( self, COL_DEBIT, row );
	cre = get_amount( self, COL_CREDIT, row );

	return( ofo_entry_new_with_data(
							self->private->dossier,
							&self->private->deff, &self->private->dope, label,
							piece, account_number,
							ofo_account_get_devise( account ),
							self->private->journal,
							deb, cre ));
}

static void
display_ok_message( ofaGuidedEx *self, gint count )
{
	GtkWidget *dialog;
	gchar *message;

	message = g_strdup_printf( _( "%d entries have been succesffully created" ), count );

	dialog = gtk_message_dialog_new(
			GTK_WINDOW( ofa_main_page_get_main_window( OFA_MAIN_PAGE( self ))),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_OK,
			"%s", message );

	gtk_dialog_run( GTK_DIALOG( dialog ));

	gtk_widget_destroy( dialog );
}
