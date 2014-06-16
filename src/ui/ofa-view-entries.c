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
#include "ui/ofa-journal-combo.h"
#include "ui/ofa-main-page.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-view-entries.h"
#include "api/ofo-base.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-journal.h"

/* private instance data
 */
struct _ofaViewEntriesPrivate {
	gboolean        dispose_has_run;

	/* internals
	 */
	ofoDossier      *dossier;			/* dossier */
	GDate            d_from;
	GDate            d_to;

	/* UI
	 */
	GtkContainer    *top_box;			/* reparented from dialog */

	GtkToggleButton *journal_btn;
	ofaJournalCombo *journal_combo;
	GtkComboBox     *journal_box;
	gchar           *jou_mnemo;

	GtkToggleButton *account_btn;
	GtkEntry        *account_entry;
	GtkButton       *account_select;
	GtkLabel        *account_label;
	gchar           *acc_number;

	GtkEntry        *we_from;
	GtkLabel        *wl_from;
	GtkEntry        *we_to;
	GtkLabel        *wl_to;

	GtkTreeView     *entries_tview;
};

/* columns in the entries view
 */
enum {
	ENT_COL_DOPE = 0,
	ENT_COL_DEFF,
	ENT_COL_NUMBER,
	ENT_COL_REF,
	ENT_COL_LABEL,
	ENT_COL_JOURNAL,
	ENT_COL_ACCOUNT,
	ENT_COL_DEBIT,
	ENT_COL_CREDIT,
	ENT_COL_CURRENCY,
	ENT_COL_RAPPRO,
	ENT_COL_STATUS,
	ENT_COL_OBJECT,
	ENT_N_COLUMNS
};

/* the id of the column is set against some columns of interest */
#define DATA_COLUMN_ID             "ofa-data-column-id"

static const gchar  *st_ui_xml    = PKGUIDIR "/ofa-view-entries.ui";
static const gchar  *st_ui_id     = "ViewEntriesDlg";

G_DEFINE_TYPE( ofaViewEntries, ofa_view_entries, OFA_TYPE_MAIN_PAGE )

static GtkWidget   *v_setup_view( ofaMainPage *page );
static void         reparent_from_dialog( ofaViewEntries *self, GtkContainer *parent );
static void         setup_gen_selection( ofaViewEntries *self );
static void         setup_journal_selection( ofaViewEntries *self );
static void         setup_account_selection( ofaViewEntries *self );
static void         setup_dates_selection( ofaViewEntries *self );
static GtkTreeView *setup_entries_treeview( ofaViewEntries *self );
static GtkWidget   *v_setup_buttons( ofaMainPage *page );
static void         v_init_view( ofaMainPage *page );
static void         on_gen_selection_toggled( GtkToggleButton *button, ofaViewEntries *self );
static void         on_journal_changed( const gchar *mnemo, ofaViewEntries *self );
static void         display_entries_from_journal( ofaViewEntries *self );
static void         on_account_changed( GtkEntry *entry, ofaViewEntries *self );
static void         on_account_select( GtkButton *button, ofaViewEntries *self );
static void         display_entries_from_account( ofaViewEntries *self );
static void         on_d_from_changed( GtkEntry *entry, ofaViewEntries *self );
static gboolean     on_d_from_focus_out( GtkEntry *entry, GdkEvent *event, ofaViewEntries *self );
static void         on_d_to_changed( GtkEntry *entry, ofaViewEntries *self );
static gboolean     on_d_to_focus_out( GtkEntry *entry, GdkEvent *event, ofaViewEntries *self );
static void         on_date_changed( ofaViewEntries *self, GtkEntry *entry, GDate *date, GtkLabel *label );
static gboolean     layout_dates_is_valid( ofaViewEntries *self );
static void         refresh_display( ofaViewEntries *self );
static void         display_entries( ofaViewEntries *self, GList *entries );
static void         display_entry( ofaViewEntries *self, GtkTreeModel *tmodel, ofoEntry *entry );
static void         on_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaViewEntries *self );

static void
view_entries_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_view_entries_finalize";
	ofaViewEntriesPrivate *priv;

	g_return_if_fail( OFA_IS_VIEW_ENTRIES( instance ));

	priv = OFA_VIEW_ENTRIES( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	g_free( priv->jou_mnemo );
	g_free( priv->acc_number );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_view_entries_parent_class )->finalize( instance );
}

static void
view_entries_dispose( GObject *instance )
{
	ofaViewEntriesPrivate *priv;

	g_return_if_fail( OFA_IS_VIEW_ENTRIES( instance ));

	priv = OFA_VIEW_ENTRIES( instance )->private;

	if( !priv->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_view_entries_parent_class )->dispose( instance );
}

static void
ofa_view_entries_init( ofaViewEntries *self )
{
	static const gchar *thisfn = "ofa_view_entries_init";

	g_return_if_fail( OFA_IS_VIEW_ENTRIES( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaViewEntriesPrivate, 1 );
}

static void
ofa_view_entries_class_init( ofaViewEntriesClass *klass )
{
	static const gchar *thisfn = "ofa_view_entries_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = view_entries_dispose;
	G_OBJECT_CLASS( klass )->finalize = view_entries_finalize;

	OFA_MAIN_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_MAIN_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_MAIN_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
}

static GtkWidget *
v_setup_view( ofaMainPage *page )
{
	ofaViewEntriesPrivate *priv;
	GtkWidget *frame;

	priv = OFA_VIEW_ENTRIES( page )->private;

	priv->dossier = ofa_main_page_get_dossier( page );

	frame = gtk_frame_new( NULL );
	gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_NONE );
	reparent_from_dialog( OFA_VIEW_ENTRIES( page ), GTK_CONTAINER( frame ));

	setup_gen_selection( OFA_VIEW_ENTRIES( page ));
	setup_journal_selection( OFA_VIEW_ENTRIES( page ));
	setup_account_selection( OFA_VIEW_ENTRIES( page ));
	setup_dates_selection( OFA_VIEW_ENTRIES( page ));

	/* force a 'toggled' message on the radio button group */
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->account_btn ), TRUE );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->journal_btn ), TRUE );

	priv->entries_tview = setup_entries_treeview( OFA_VIEW_ENTRIES( page ));

	return( frame );
}

static void
reparent_from_dialog( ofaViewEntries *self, GtkContainer *parent )
{
	GtkWidget *dialog;
	GtkWidget *box;

	/* load our dialog */
	dialog = my_utils_builder_load_from_path( st_ui_xml, st_ui_id );
	g_return_if_fail( dialog && GTK_IS_WINDOW( dialog ));

	box = my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "px-box" );
	g_return_if_fail( box && GTK_IS_BOX( box ));
	self->private->top_box = GTK_CONTAINER( box );

	/* attach our box to the parent's frame */
	gtk_widget_reparent( box, GTK_WIDGET( parent ));
}

static void
setup_gen_selection( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GtkToggleButton *btn;

	priv = self->private;

	btn = ( GtkToggleButton * ) my_utils_container_get_child_by_name( priv->top_box, "p1-btn-journal" );
	g_return_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ));
	g_signal_connect( G_OBJECT( btn ), "toggled", G_CALLBACK( on_gen_selection_toggled ), self );
	priv->journal_btn = btn;

	btn = ( GtkToggleButton * ) my_utils_container_get_child_by_name( priv->top_box, "p1-btn-account" );
	g_return_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ));
	g_signal_connect( G_OBJECT( btn ), "toggled", G_CALLBACK( on_gen_selection_toggled ), self );
	priv->account_btn = btn;
}

static void
setup_journal_selection( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	ofaJournalComboParms parms;

	priv = self->private;

	parms.container = priv->top_box;
	parms.dossier = priv->dossier;
	parms.combo_name = "p1-journal";
	parms.label_name = NULL;
	parms.disp_mnemo = FALSE;
	parms.disp_label = TRUE;
	parms.pfn = ( ofaJournalComboCb ) on_journal_changed;
	parms.user_data = self;
	parms.initial_mnemo = NULL;

	priv->journal_combo = ofa_journal_combo_init_combo( &parms );

	priv->journal_box = ( GtkComboBox * ) my_utils_container_get_child_by_name( priv->top_box, "p1-journal" );
	g_return_if_fail( priv->journal_box && GTK_IS_COMBO_BOX( priv->journal_box ));
}

static void
setup_account_selection( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GtkWidget *image;
	GtkButton *btn;
	GtkWidget *widget;

	priv = self->private;

	btn = ( GtkButton * ) my_utils_container_get_child_by_name( priv->top_box, "p1-account-select" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	image = gtk_image_new_from_stock( GTK_STOCK_INDEX, GTK_ICON_SIZE_BUTTON );
	gtk_button_set_image( btn, image );
	g_signal_connect( G_OBJECT( btn ), "clicked", G_CALLBACK( on_account_select ), self );
	priv->account_select = btn;

	widget = my_utils_container_get_child_by_name( priv->top_box, "p1-account-entry" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_account_changed ), self );
	priv->account_entry = GTK_ENTRY( widget );

	widget = my_utils_container_get_child_by_name( priv->top_box, "p1-account-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->account_label = GTK_LABEL( widget );
}

static void
setup_dates_selection( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GtkWidget *widget;

	priv = self->private;

	widget = my_utils_container_get_child_by_name( priv->top_box, "p1-from" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_d_from_changed ), self );
	g_signal_connect( G_OBJECT( widget ), "focus-out-event", G_CALLBACK( on_d_from_focus_out ), self );
	priv->we_from = GTK_ENTRY( widget );

	widget = my_utils_container_get_child_by_name( priv->top_box, "p1-from-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->wl_from = GTK_LABEL( widget );

	widget = my_utils_container_get_child_by_name( priv->top_box, "p1-to" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_d_to_changed ), self );
	g_signal_connect( G_OBJECT( widget ), "focus-out-event", G_CALLBACK( on_d_to_focus_out ), self );
	priv->we_to = GTK_ENTRY( widget );

	widget = my_utils_container_get_child_by_name( priv->top_box, "p1-to-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->wl_to = GTK_LABEL( widget );
}

/*
 * the left pane is a treeview whose level 0 are the journals, and
 * level 1 the entry models defined on the corresponding journal
 */
static GtkTreeView *
setup_entries_treeview( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = self->private;

	tview = ( GtkTreeView * ) my_utils_container_get_child_by_name( priv->top_box, "p1-entries" );
	g_return_val_if_fail( tview && GTK_IS_TREE_VIEW( tview ), NULL );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			ENT_N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
			G_TYPE_STRING, G_TYPE_STRING,
			G_TYPE_OBJECT ));
	gtk_tree_view_set_model( tview, tmodel );
	g_object_unref( tmodel );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Operation" ),
			text_cell, "text", ENT_COL_DOPE,
			NULL );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Journal" ),
			text_cell, "text", ENT_COL_JOURNAL,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_JOURNAL ));
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Account" ),
			text_cell, "text", ENT_COL_ACCOUNT,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_ACCOUNT ));
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", ENT_COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Debit" ),
			text_cell, "text", ENT_COL_DEBIT,
			NULL );
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Credit" ),
			text_cell, "text", ENT_COL_CREDIT,
			NULL );
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			"",
			text_cell, "text", ENT_COL_CURRENCY,
			NULL );
	gtk_tree_view_append_column( tview, column );

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );

	return( tview );
}

static GtkWidget *
v_setup_buttons( ofaMainPage *page )
{
	/*return( OFA_MAIN_PAGE_CLASS( ofa_view_entries_parent_class )->setup_buttons( page ));*/
	return( NULL );
}

static void
v_init_view( ofaMainPage *page )
{
}

static void
on_gen_selection_toggled( GtkToggleButton *button, ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	gboolean is_active;

	priv = self->private;

	is_active = gtk_toggle_button_get_active( button );
	/*g_debug( "on_gen_selection_toggled: is_active=%s", is_active ? "True":"False" );*/

	if( button == priv->journal_btn ){
		gtk_widget_set_sensitive( GTK_WIDGET( priv->journal_box ), is_active );
		if( is_active ){
			display_entries_from_journal( self );
		}

	} else {
		gtk_widget_set_sensitive( GTK_WIDGET( priv->account_entry ), is_active );
		gtk_widget_set_sensitive( GTK_WIDGET( priv->account_select ), is_active );
		gtk_widget_set_sensitive( GTK_WIDGET( priv->account_label ), is_active );
		if( is_active ){
			display_entries_from_account( self );
		}
	}
}

/*
 * ofaJournalCombo callback
 */
static void
on_journal_changed( const gchar *mnemo, ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;

	priv = self->private;

	g_free( priv->jou_mnemo );
	priv->jou_mnemo = g_strdup( mnemo );

	display_entries_from_journal( self );
}

static void
display_entries_from_journal( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GList *entries;

	priv = self->private;

	if( priv->jou_mnemo && layout_dates_is_valid( self )){
		entries = ofo_entry_get_dataset_by_journal(
							priv->dossier, priv->jou_mnemo, &priv->d_from, &priv->d_to );
		display_entries( self, entries );
		ofo_entry_free_dataset( entries );
	}
}

static void
on_account_changed( GtkEntry *entry, ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	ofoAccount *account;

	priv = self->private;

	g_free( priv->acc_number );
	priv->acc_number = g_strdup( gtk_entry_get_text( entry ));

	account = ofo_account_get_by_number( priv->dossier, priv->acc_number );

	if( account ){
		gtk_label_set_text( priv->account_label, ofo_account_get_label( account ));
		display_entries_from_account( self );

	} else {
		gtk_label_set_text( priv->account_label, "" );
	}
}

static void
on_account_select( GtkButton *button, ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	gchar *acc_number;

	priv = self->private;

	acc_number = ofa_account_select_run(
							ofa_main_page_get_main_window( OFA_MAIN_PAGE( self )),
							gtk_entry_get_text( priv->account_entry ));
	if( acc_number ){
		gtk_entry_set_text( priv->account_entry, acc_number );
		g_free( acc_number );
	}
}

static void
display_entries_from_account( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GList *entries;

	priv = self->private;

	if( priv->acc_number && layout_dates_is_valid( self )){
		entries = ofo_entry_get_dataset_by_account(
							priv->dossier, priv->acc_number, &priv->d_from, &priv->d_to );
		display_entries( self, entries );
		ofo_entry_free_dataset( entries );
	}
}

static void
on_d_from_changed( GtkEntry *entry, ofaViewEntries *self )
{
	on_date_changed( self, entry, &self->private->d_from, self->private->wl_from );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_d_from_focus_out( GtkEntry *entry, GdkEvent *event, ofaViewEntries *self )
{
	refresh_display( self );

	return( FALSE );
}

static void
on_d_to_changed( GtkEntry *entry, ofaViewEntries *self )
{
	on_date_changed( self, entry, &self->private->d_to, self->private->wl_to );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_d_to_focus_out( GtkEntry *entry, GdkEvent *event, ofaViewEntries *self )
{
	refresh_display( self );

	return( FALSE );
}

static void
on_date_changed( ofaViewEntries *self, GtkEntry *entry, GDate *priv_date, GtkLabel *label )
{
	GDate date;
	const gchar *str;
	gchar *display;

	g_date_clear( &date, 1 );

	str = gtk_entry_get_text( entry );
	if( str && g_utf8_strlen( str, -1 )){
		g_date_set_parse( &date, str );
	}

	if( g_date_valid( &date )){
		display = my_utils_display_from_date( &date, MY_UTILS_DATE_DMMM );
	} else {
		display = g_strdup( "" );
	}

	gtk_label_set_text( label, display );
	g_free( display );
	memcpy( priv_date, &date, sizeof( GDate ));
}

/*
 * an invalid date is... invalid.
 * but an empty is valid and infinite
 */
static gboolean
layout_dates_is_valid( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	const gchar *str;

	priv = self->private;

	str = gtk_entry_get_text( priv->we_from );
	if( str && g_utf8_strlen( str, -1 ) && !g_date_valid( &priv->d_from )){
		return( FALSE );
	}

	str = gtk_entry_get_text( priv->we_to );
	if( str && g_utf8_strlen( str, -1 ) && !g_date_valid( &priv->d_to )){
		return( FALSE );
	}

	if( g_date_valid( &priv->d_from ) &&
			g_date_valid( &priv->d_to ) &&
			g_date_compare( &priv->d_from, &priv->d_to ) > 0 ){
		return( FALSE );
	}

	return( TRUE );
}

static void
refresh_display( ofaViewEntries *self )
{
	if( gtk_toggle_button_get_active( self->private->journal_btn )){
		display_entries_from_journal( self );
	} else {
		display_entries_from_account( self );
	}
}

static void
display_entries( ofaViewEntries *self, GList *entries )
{
	GtkTreeModel *tmodel;
	GList *iset;
	gdouble debits, credits;
	GtkWidget *total;
	gchar *str;

	tmodel = gtk_tree_view_get_model( self->private->entries_tview );

	gtk_list_store_clear( GTK_LIST_STORE( tmodel ));
	debits = 0;
	credits = 0;

	for( iset=entries ; iset ; iset=iset->next ){
		display_entry( self, tmodel, OFO_ENTRY( iset->data ));
		debits += ofo_entry_get_debit( OFO_ENTRY( iset->data ));
		credits += ofo_entry_get_credit( OFO_ENTRY( iset->data ));
	}

	total = my_utils_container_get_child_by_name( self->private->top_box, "p1-tot-debits" );
	g_return_if_fail( total && GTK_IS_LABEL( total ));
	str = g_strdup_printf( "%'.2lf", debits );
	gtk_label_set_text( GTK_LABEL( total ), str );
	g_free( str );

	total = my_utils_container_get_child_by_name( self->private->top_box, "p1-tot-credits" );
	g_return_if_fail( total && GTK_IS_LABEL( total ));
	str = g_strdup_printf( "%'.2lf", credits );
	gtk_label_set_text( GTK_LABEL( total ), str );
	g_free( str );
}

static void
display_entry( ofaViewEntries *self, GtkTreeModel *tmodel, ofoEntry *entry )
{
	GtkTreeIter iter;
	gchar *sdope, *sdeff, *sdeb, *scre, *srappro, *status;
	const GDate *d;

	sdope = my_utils_display_from_date( ofo_entry_get_dope( entry ), MY_UTILS_DATE_DDMM );
	sdeff = my_utils_display_from_date( ofo_entry_get_deffect( entry ), MY_UTILS_DATE_DDMM );
	sdeb = g_strdup_printf( "%'.2lf", ofo_entry_get_debit( entry ));
	scre = g_strdup_printf( "%'.2lf", ofo_entry_get_credit( entry ));
	d = ofo_entry_get_rappro( entry );
	if( d && g_date_valid( d )){
		srappro = my_utils_display_from_date( d, MY_UTILS_DATE_DDMM );
	} else {
		srappro = g_strdup( "" );
	}
	status = g_strdup_printf( "%d", ofo_entry_get_status( entry ));

	gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				ENT_COL_DOPE,     sdope,
				ENT_COL_DEFF,     sdeff,
				ENT_COL_NUMBER,   ofo_entry_get_number( entry ),
				ENT_COL_REF,      ofo_entry_get_ref( entry ),
				ENT_COL_LABEL,    ofo_entry_get_label( entry ),
				ENT_COL_JOURNAL,  ofo_entry_get_journal( entry ),
				ENT_COL_ACCOUNT,  ofo_entry_get_account( entry ),
				ENT_COL_DEBIT,    sdeb,
				ENT_COL_CREDIT,   scre,
				ENT_COL_CURRENCY, ofo_entry_get_devise( entry ),
				ENT_COL_RAPPRO,   srappro,
				ENT_COL_STATUS,   status,
				ENT_COL_OBJECT,   entry,
				-1 );

	g_free( status );
	g_free( srappro );
	g_free( scre );
	g_free( sdeb );
	g_free( sdeff );
	g_free( sdope );
}

/*
 * do not display journal (resp. account)
 * when selection is made per journal (resp. account)
 */
static void
on_cell_data_func( GtkTreeViewColumn *tcolumn,
						GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
						ofaViewEntries *self )
{
	gint column;

	g_return_if_fail( GTK_IS_CELL_RENDERER_TEXT( cell ));

	column = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( tcolumn ), DATA_COLUMN_ID ));

	switch( column ){
		case ENT_COL_JOURNAL:
			gtk_tree_view_column_set_visible(
					tcolumn,
					gtk_toggle_button_get_active( self->private->account_btn ));
			break;
		case ENT_COL_ACCOUNT:
			gtk_tree_view_column_set_visible(
					tcolumn,
					gtk_toggle_button_get_active( self->private->journal_btn ));
			break;
	}
}

/**
 * ofa_view_entries_display_entries:
 *
 * @begin: [allow-none]:
 * @end: [allow-none]:
 */
void
ofa_view_entries_display_entries( ofaViewEntries *self, GType type, const gchar *id, const GDate *begin, const GDate *end )
{
	static const gchar *thisfn = "ofa_view_entries_display_entries";
	ofaViewEntriesPrivate *priv;
	gchar *str;

	g_return_if_fail( self && OFA_IS_VIEW_ENTRIES( self ));
	g_return_if_fail( id && g_utf8_strlen( id, -1 ));

	if( !self->private->dispose_has_run ){

		g_debug( "%s: self=%p, type=%lu, id=%s, begin=%p, end=%p",
				thisfn, ( void * ) self, type, id, ( void * ) begin, ( void * ) end );

		priv = self->private;

		/* start by setting the from/to dates as these changes do not
		 * automatically trigger a display refresh */
		if( begin && g_date_valid( begin )){
			str = my_utils_display_from_date( begin, MY_UTILS_DATE_DDMM );
			gtk_entry_set_text( priv->we_from, str );
			g_free( str );
		} else {
			gtk_entry_set_text( priv->we_from, "" );
		}

		if( end && g_date_valid( end )){
			str = my_utils_display_from_date( end, MY_UTILS_DATE_DDMM );
			gtk_entry_set_text( priv->we_to, str );
			g_free( str );
		} else {
			gtk_entry_set_text( priv->we_to, "" );
		}

		/* then setup the general selection: changes on theses entries
		 * will automativally trigger a display refresh */
		if( type == OFO_TYPE_ACCOUNT ){

			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->account_btn ), TRUE );
			gtk_entry_set_text( priv->account_entry, id );

		} else if( type == OFO_TYPE_JOURNAL ){

			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->journal_btn ), TRUE );
			ofa_journal_combo_set_selection( priv->journal_combo, id );
		}
	}
}
