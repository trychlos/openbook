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

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofo-base.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"

#include "ui/my-cell-renderer-amount.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-ledger-combo.h"
#include "ui/ofa-main-page.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-view-entries.h"

/* private instance data
 */
struct _ofaViewEntriesPrivate {
	gboolean        dispose_has_run;

	/* internals
	 */
	ofoDossier      *dossier;			/* dossier */
	myDate          *d_from;
	myDate          *d_to;

	/* UI
	 */
	GtkContainer    *top_box;			/* reparented from dialog */

	/* frame 1: general selection
	 */
	GtkToggleButton *ledger_btn;
	ofaLedgerCombo *ledger_combo;
	GtkComboBox     *ledger_box;
	gchar           *jou_mnemo;

	GtkToggleButton *account_btn;
	GtkEntry        *account_entry;
	GtkButton       *account_select;
	gchar           *acc_number;

	GtkLabel        *f1_label;

	/* frame 2: effect dates layout
	 */
	GtkEntry        *we_from;
	GtkLabel        *wl_from;
	GtkEntry        *we_to;
	GtkLabel        *wl_to;

	/* frame 3: entry status
	 */
	gboolean         display_rough;
	gboolean         display_validated;
	gboolean         display_deleted;

	/* frame 4: visible columns
	 */
	GtkCheckButton  *account_checkbox;
	GtkCheckButton  *ledger_checkbox;
	GtkCheckButton  *currency_checkbox;

	gboolean         dope_visible;
	gboolean         deffect_visible;
	gboolean         ref_visible;
	gboolean         ledger_visible;
	gboolean         account_visible;
	gboolean         rappro_visible;
	gboolean         status_visible;
	gboolean         currency_visible;

	/* frame 5: edition switch
	 */
	GtkSwitch       *edit_switch;

	/* entries list view
	 */
	GtkTreeView     *entries_tview;
	GtkTreeModel    *tfilter;			/* GtkTreeModelFilter of the listview */
	ofoEntry        *inserted;			/* a new entry being inserted */

	/* footer
	 */
	GtkLabel        *comment;
	GHashTable      *balances_hash;
};

/* columns in the entries view
 */
enum {
	ENT_COL_DOPE = 0,
	ENT_COL_DEFF,
	ENT_COL_NUMBER,
	ENT_COL_REF,
	ENT_COL_LABEL,
	ENT_COL_LEDGER,
	ENT_COL_ACCOUNT,
	ENT_COL_DEBIT,
	ENT_COL_CREDIT,
	ENT_COL_CURRENCY,
	ENT_COL_RAPPRO,
	ENT_COL_STATUS,
	ENT_COL_OBJECT,
	ENT_COL_VALID,
	ENT_N_COLUMNS
};

/* the balance per currency, mostly useful when displaying per ledger
 */
typedef struct {
	gdouble debits;
	gdouble credits;
}
	perCurrency;

/* the id of the column is set against some columns of interest,
 * typically whose that we want be able to show/hide */
#define DATA_COLUMN_ID                  "ofa-data-column-id"

/* a pointer to the boolean xxx_visible is set against the check
 * buttons - so that we are able to have only one callback
 * this same pointer is set against the columns to be used
 * when actually displaying the columns in on_cell_data_func() */
#define DATA_PRIV_VISIBLE               "ofa-data-priv-visible"

static const gchar           *st_ui_xml                   = PKGUIDIR "/ofa-view-entries.piece.ui";
static const gchar           *st_ui_id                    = "ViewEntriesWindow";

static       GtkCellRenderer *st_renderers[ENT_N_COLUMNS] = { 0 };

G_DEFINE_TYPE( ofaViewEntries, ofa_view_entries, OFA_TYPE_MAIN_PAGE )

static GtkWidget     *v_setup_view( ofaMainPage *page );
static void           reparent_from_dialog( ofaViewEntries *self, GtkContainer *parent );
static void           setup_gen_selection( ofaViewEntries *self );
static void           setup_ledger_selection( ofaViewEntries *self );
static void           setup_account_selection( ofaViewEntries *self );
static void           setup_dates_selection( ofaViewEntries *self );
static void           setup_status_selection( ofaViewEntries *self );
static void           setup_display_columns( ofaViewEntries *self );
static void           setup_edit_switch( ofaViewEntries *self );
static GtkTreeView   *setup_entries_treeview( ofaViewEntries *self );
static void           setup_footer( ofaViewEntries *self );
static void           setup_signaling_connect( ofaViewEntries *self );
static GtkWidget     *v_setup_buttons( ofaMainPage *page );
static void           v_init_view( ofaMainPage *page );
static void           on_gen_selection_toggled( GtkToggleButton *button, ofaViewEntries *self );
static void           onledger_changed( const gchar *mnemo, ofaViewEntries *self );
static void           display_entries_from_ledger( ofaViewEntries *self );
static void           on_account_changed( GtkEntry *entry, ofaViewEntries *self );
static void           on_account_select( GtkButton *button, ofaViewEntries *self );
static void           display_entries_from_account( ofaViewEntries *self );
static gboolean       on_d_from_focus_out( GtkEntry *entry, GdkEvent *event, ofaViewEntries *self );
static gboolean       on_d_to_focus_out( GtkEntry *entry, GdkEvent *event, ofaViewEntries *self );
static gboolean       on_date_focus_out( ofaViewEntries *self, GtkEntry *entry, myDate *date );
static gboolean       layout_dates_is_valid( ofaViewEntries *self );
static void           refresh_display( ofaViewEntries *self );
static void           display_entries( ofaViewEntries *self, GList *entries );
static void           reset_balances_hash( ofaViewEntries *self );
static void           compute_balances( ofaViewEntries *self );
static perCurrency   *find_balance_by_currency( ofaViewEntries *self, const gchar *dev_code );
static void           display_entry( ofaViewEntries *self, GtkTreeModel *tmodel, ofoEntry *entry );
static GtkWidget     *reset_balances_widgets( ofaViewEntries *self );
static void           display_balance( const gchar *dev_code, perCurrency *pc, ofaViewEntries *self );
static void           update_balance_amounts( ofaViewEntries *self, const gchar *sdeb, const gchar *scre, const gchar *dev_code, gint column_id, const gchar *text );
static void           update_balance_currency( ofaViewEntries *self, const gchar *sdeb, const gchar *scre, const gchar *prev_code, const gchar *text );
static gboolean       is_visible_row( GtkTreeModel *tfilter, GtkTreeIter *iter, ofaViewEntries *self );
static void           on_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaViewEntries *self );
static void           on_entry_status_toggled( GtkToggleButton *button, ofaViewEntries *self );
static void           on_visible_column_toggled( GtkToggleButton *button, ofaViewEntries *self );
static void           on_edit_switched( GtkSwitch *switch_btn, GParamSpec *pspec, ofaViewEntries *self );
static void           set_renderers_editable( ofaViewEntries *self, gboolean editable );
static void           on_cell_edited( GtkCellRendererText *cell, gchar *path, gchar *text, ofaViewEntries *self );
static void           on_row_selected( GtkTreeSelection *select, ofaViewEntries *self );
static gboolean       check_row_for_valid( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean       check_row_for_valid_dope( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean       check_row_for_valid_deffect( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean       check_row_for_valid_label( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean       check_row_for_valid_ledger( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean       check_row_for_valid_account( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean       check_row_for_valid_currency( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean       check_row_for_valid_amounts( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void           set_comment( ofaViewEntries *self, const gchar *str );
static gboolean       save_entry( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean       find_entry_by_number( ofaViewEntries *self, gint number, GtkTreeModel **tmodel, GtkTreeIter *iter );
static void           on_dossier_new_object( ofoDossier *dossier, ofoBase *object, ofaViewEntries *self );
static void           do_new_entry( ofaViewEntries *self, ofoEntry *entry );
static void           on_dossier_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaViewEntries *self );
static void           do_update_account_number( ofaViewEntries *self, const gchar *prev, const gchar *number );
static void           do_updateledger_mnemo( ofaViewEntries *self, const gchar *prev, const gchar *mnemo );
static void           do_update_currency_code( ofaViewEntries *self, const gchar *prev, const gchar *code );
static void           on_dossier_deleted_object( ofoDossier *dossier, ofoBase *object, ofaViewEntries *self );
static void           do_on_deleted_entry( ofaViewEntries *self, ofoEntry *entry );
static void           on_dossier_validated_entry( ofoDossier *dossier, ofoBase *object, ofaViewEntries *self );
static gboolean       on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, ofaViewEntries *self );
static ofaEntryStatus get_entry_status_from_row( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void           insert_new_row( ofaViewEntries *self );
static void           delete_row( ofaViewEntries *self );
static gboolean       delete_confirmed( const ofaViewEntries *self, const gchar *message );

static void
view_entries_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_view_entries_finalize";
	ofaViewEntriesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( OFA_IS_VIEW_ENTRIES( instance ));

	priv = OFA_VIEW_ENTRIES( instance )->private;

	/* free data members here */
	g_free( priv->jou_mnemo );
	g_free( priv->acc_number );
	reset_balances_hash( OFA_VIEW_ENTRIES( instance ));
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
		g_object_unref( priv->d_from );
		g_object_unref( priv->d_to );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_view_entries_parent_class )->dispose( instance );
}

static void
ofa_view_entries_init( ofaViewEntries *self )
{
	static const gchar *thisfn = "ofa_view_entries_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( OFA_IS_VIEW_ENTRIES( self ));

	self->private = g_new0( ofaViewEntriesPrivate, 1 );

	self->private->d_from = my_date_new();
	self->private->d_to = my_date_new();
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
	setup_ledger_selection( OFA_VIEW_ENTRIES( page ));
	setup_account_selection( OFA_VIEW_ENTRIES( page ));
	setup_dates_selection( OFA_VIEW_ENTRIES( page ));
	setup_status_selection( OFA_VIEW_ENTRIES( page ));
	setup_display_columns( OFA_VIEW_ENTRIES( page ));
	setup_edit_switch( OFA_VIEW_ENTRIES( page ));
	priv->entries_tview = setup_entries_treeview( OFA_VIEW_ENTRIES( page ));
	setup_footer( OFA_VIEW_ENTRIES( page ));

	/* force a 'toggled' message on the radio button group */
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->account_btn ), TRUE );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->ledger_btn ), TRUE );

	/* connect to dossier signaling system */
	setup_signaling_connect( OFA_VIEW_ENTRIES( page ));

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

	btn = ( GtkToggleButton * ) my_utils_container_get_child_by_name( priv->top_box, "f1-btn-ledger" );
	g_return_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ));
	g_signal_connect( G_OBJECT( btn ), "toggled", G_CALLBACK( on_gen_selection_toggled ), self );
	priv->ledger_btn = btn;

	btn = ( GtkToggleButton * ) my_utils_container_get_child_by_name( priv->top_box, "f1-btn-account" );
	g_return_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ));
	g_signal_connect( G_OBJECT( btn ), "toggled", G_CALLBACK( on_gen_selection_toggled ), self );
	priv->account_btn = btn;
}

static void
setup_ledger_selection( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	ofaLedgerComboParms parms;

	priv = self->private;

	parms.container = priv->top_box;
	parms.dossier = priv->dossier;
	parms.combo_name = "f1-ledger";
	parms.label_name = NULL;
	parms.disp_mnemo = FALSE;
	parms.disp_label = TRUE;
	parms.pfnSelected = ( ofaLedgerComboCb ) onledger_changed;
	parms.user_data = self;
	parms.initial_mnemo = NULL;

	priv->ledger_combo = ofa_ledger_combo_new( &parms );

	priv->ledger_box = ( GtkComboBox * ) my_utils_container_get_child_by_name( priv->top_box, "f1-ledger" );
	g_return_if_fail( priv->ledger_box && GTK_IS_COMBO_BOX( priv->ledger_box ));
}

static void
setup_account_selection( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GtkWidget *image;
	GtkButton *btn;
	GtkWidget *widget;

	priv = self->private;

	btn = ( GtkButton * ) my_utils_container_get_child_by_name( priv->top_box, "f1-account-select" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	image = gtk_image_new_from_stock( GTK_STOCK_INDEX, GTK_ICON_SIZE_BUTTON );
	gtk_button_set_image( btn, image );
	g_signal_connect( G_OBJECT( btn ), "clicked", G_CALLBACK( on_account_select ), self );
	priv->account_select = btn;

	widget = my_utils_container_get_child_by_name( priv->top_box, "f1-account-entry" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_account_changed ), self );
	priv->account_entry = GTK_ENTRY( widget );

	widget = my_utils_container_get_child_by_name( priv->top_box, "f1-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->f1_label = GTK_LABEL( widget );
}

static void
setup_dates_selection( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	myDateParse parms;

	priv = self->private;

	memset( &parms, '\0', sizeof( parms ));
	parms.entry = my_utils_container_get_child_by_name( priv->top_box, "f2-from" );
	parms.entry_format = MY_DATE_DMYY;
	parms.label = my_utils_container_get_child_by_name( priv->top_box, "f2-from-label" );
	parms.label_format = MY_DATE_DMMM;
	parms.date = my_date2_from_date( priv->d_from );
	my_date_parse_from_entry( &parms );

	g_signal_connect(
			G_OBJECT( parms.entry ),
			"focus-out-event", G_CALLBACK( on_d_from_focus_out ), self );

	priv->we_from = GTK_ENTRY( parms.entry );
	priv->wl_from = GTK_LABEL( parms.label );

	memset( &parms, '\0', sizeof( parms ));
	parms.entry = my_utils_container_get_child_by_name( priv->top_box, "f2-to" );
	parms.entry_format = MY_DATE_DMYY;
	parms.label = my_utils_container_get_child_by_name( priv->top_box, "f2-to-label" );
	parms.label_format = MY_DATE_DMMM;
	parms.date = my_date2_from_date( priv->d_to );
	my_date_parse_from_entry( &parms );

	g_signal_connect(
			G_OBJECT( parms.entry ),
			"focus-out-event", G_CALLBACK( on_d_to_focus_out ), self );

	priv->we_to = GTK_ENTRY( parms.entry );
	priv->wl_to = GTK_LABEL( parms.label );
}

static void
setup_status_selection( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GtkWidget *button;

	priv = self->private;

	button = my_utils_container_get_child_by_name( priv->top_box, "f3-rough" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "toggled", G_CALLBACK( on_entry_status_toggled), self );
	g_object_set_data( G_OBJECT( button ), DATA_PRIV_VISIBLE, &priv->display_rough );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), TRUE );

	button = my_utils_container_get_child_by_name( priv->top_box, "f3-validated" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "toggled", G_CALLBACK( on_entry_status_toggled), self );
	g_object_set_data( G_OBJECT( button ), DATA_PRIV_VISIBLE, &priv->display_validated );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), TRUE );

	button = my_utils_container_get_child_by_name( priv->top_box, "f3-deleted" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "toggled", G_CALLBACK( on_entry_status_toggled), self );
	g_object_set_data( G_OBJECT( button ), DATA_PRIV_VISIBLE, &priv->display_deleted );
	gtk_widget_set_sensitive( button, FALSE );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), FALSE );
}

static void
setup_display_columns( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GtkWidget *widget;

	priv = self->private;

	widget = my_utils_container_get_child_by_name( priv->top_box, "f4-dope" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_visible_column_toggled), self );
	g_object_set_data( G_OBJECT( widget ), DATA_PRIV_VISIBLE, &priv->dope_visible );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), TRUE );

	widget = my_utils_container_get_child_by_name( priv->top_box, "f4-deffect" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_visible_column_toggled), self );
	g_object_set_data( G_OBJECT( widget ), DATA_PRIV_VISIBLE, &priv->deffect_visible );

	widget = my_utils_container_get_child_by_name( priv->top_box, "f4-ref" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_visible_column_toggled), self );
	g_object_set_data( G_OBJECT( widget ), DATA_PRIV_VISIBLE, &priv->ref_visible );

	widget = my_utils_container_get_child_by_name( priv->top_box, "f4-ledger" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_visible_column_toggled), self );
	g_object_set_data( G_OBJECT( widget ), DATA_PRIV_VISIBLE, &priv->ledger_visible );
	priv->ledger_checkbox = GTK_CHECK_BUTTON( widget );

	widget = my_utils_container_get_child_by_name( priv->top_box, "f4-account" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_visible_column_toggled), self );
	g_object_set_data( G_OBJECT( widget ), DATA_PRIV_VISIBLE, &priv->account_visible );
	priv->account_checkbox = GTK_CHECK_BUTTON( widget );

	widget = my_utils_container_get_child_by_name( priv->top_box, "f4-rappro" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_visible_column_toggled), self );
	g_object_set_data( G_OBJECT( widget ), DATA_PRIV_VISIBLE, &priv->rappro_visible );

	widget = my_utils_container_get_child_by_name( priv->top_box, "f4-status" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_visible_column_toggled), self );
	g_object_set_data( G_OBJECT( widget ), DATA_PRIV_VISIBLE, &priv->status_visible );

	widget = my_utils_container_get_child_by_name( priv->top_box, "f4-currency" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_visible_column_toggled), self );
	g_object_set_data( G_OBJECT( widget ), DATA_PRIV_VISIBLE, &priv->currency_visible );
	priv->currency_checkbox = GTK_CHECK_BUTTON( widget );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), TRUE );
}

static void
setup_edit_switch( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GtkWidget *widget;

	priv = self->private;

	widget = my_utils_container_get_child_by_name( priv->top_box, "f5-edition-switch" );
	g_return_if_fail( widget && GTK_IS_SWITCH( widget ));
	g_signal_connect( G_OBJECT( widget ), "notify::active", G_CALLBACK( on_edit_switched ), self );
	priv->edit_switch = GTK_SWITCH( widget );

	g_object_set( G_OBJECT( widget ), "active", FALSE, NULL );
}

/*
 * - renderers are stored in an array in order to be able to switch them
 *   editable or not
 * - the column id is set as a data against the cell renderer in order
 *   to be able to identify the edited cell renderer in on_cell_edited()
 *   method
 * - the column id is set as a data against the column in order to be
 *   able to switch their visibility state in on_cell_data_func() method
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
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,		/* dope, deff, number */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* ref, label, ledger */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* account, debit, credit */
			G_TYPE_STRING,	G_TYPE_STRING, G_TYPE_STRING,	/* currency, rappro, status */
			G_TYPE_OBJECT,
			G_TYPE_BOOLEAN ));								/* valid */
	priv->tfilter = gtk_tree_model_filter_new( tmodel, NULL );
	g_object_unref( tmodel );
	gtk_tree_view_set_model( tview, priv->tfilter );
	g_object_unref( priv->tfilter );
	gtk_tree_model_filter_set_visible_func(
			GTK_TREE_MODEL_FILTER( priv->tfilter ),
			( GtkTreeModelFilterVisibleFunc ) is_visible_row,
			self,
			NULL );

	text_cell = gtk_cell_renderer_text_new();
	st_renderers[ENT_COL_DOPE] = text_cell;
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_DOPE ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Operation" ),
			text_cell, "text", ENT_COL_DOPE,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_DOPE ));
	g_object_set_data( G_OBJECT( column ), DATA_PRIV_VISIBLE, &priv->dope_visible );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	st_renderers[ENT_COL_DEFF] = text_cell;
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_DEFF ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Effect" ),
			text_cell, "text", ENT_COL_DEFF,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_DEFF ));
	g_object_set_data( G_OBJECT( column ), DATA_PRIV_VISIBLE, &priv->deffect_visible );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	st_renderers[ENT_COL_REF] = text_cell;
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_REF ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Piece" ),
			text_cell, "text", ENT_COL_REF,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_REF ));
	g_object_set_data( G_OBJECT( column ), DATA_PRIV_VISIBLE, &priv->ref_visible );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	st_renderers[ENT_COL_LEDGER] = text_cell;
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_LEDGER ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Ledger" ),
			text_cell, "text", ENT_COL_LEDGER,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_LEDGER ));
	g_object_set_data( G_OBJECT( column ), DATA_PRIV_VISIBLE, &priv->ledger_visible );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	st_renderers[ENT_COL_ACCOUNT] = text_cell;
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_ACCOUNT ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Account" ),
			text_cell, "text", ENT_COL_ACCOUNT,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_ACCOUNT ));
	g_object_set_data( G_OBJECT( column ), DATA_PRIV_VISIBLE, &priv->account_visible );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	st_renderers[ENT_COL_LABEL] = text_cell;
	g_object_set( G_OBJECT( text_cell ), "ellipsize", PANGO_ELLIPSIZE_END, NULL );
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_LABEL ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", ENT_COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	st_renderers[ENT_COL_DEBIT] = text_cell;
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_DEBIT ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	my_cell_renderer_amount_init( text_cell );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Debit" ),
			text_cell, "text", ENT_COL_DEBIT,
			NULL );
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_set_min_width( column, 110 );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	st_renderers[ENT_COL_CREDIT] = text_cell;
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_CREDIT ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	my_cell_renderer_amount_init( text_cell );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Credit" ),
			text_cell, "text", ENT_COL_CREDIT,
			NULL );
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_set_min_width( column, 110 );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	st_renderers[ENT_COL_CURRENCY] = text_cell;
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_CURRENCY ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	column = gtk_tree_view_column_new_with_attributes(
			"",
			text_cell, "text", ENT_COL_CURRENCY,
			NULL );
	gtk_tree_view_column_set_min_width( column, 32 );	/* "EUR" width */
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_CURRENCY ));
	g_object_set_data( G_OBJECT( column ), DATA_PRIV_VISIBLE, &priv->currency_visible );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	st_renderers[ENT_COL_RAPPRO] = text_cell;
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_RAPPRO ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Reconcil." ),
			text_cell, "text", ENT_COL_RAPPRO,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_RAPPRO ));
	g_object_set_data( G_OBJECT( column ), DATA_PRIV_VISIBLE, &priv->rappro_visible );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	text_cell = gtk_cell_renderer_text_new();
	st_renderers[ENT_COL_STATUS] = text_cell;
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_STATUS ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Status" ),
			text_cell, "text", ENT_COL_STATUS,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( ENT_COL_STATUS ));
	g_object_set_data( G_OBJECT( column ), DATA_PRIV_VISIBLE, &priv->status_visible );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_row_selected ), self );

	g_signal_connect( G_OBJECT( tview ), "key-press-event", G_CALLBACK( on_key_pressed_event ), self );

	return( tview );
}

static void
setup_footer( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GtkWidget *widget;

	priv = self->private;

	widget = my_utils_container_get_child_by_name( priv->top_box, "pt-comment" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->comment = GTK_LABEL( widget );
}

static void
setup_signaling_connect( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;

	priv = self->private;

	g_signal_connect(
			G_OBJECT( priv->dossier ),
			OFA_SIGNAL_NEW_OBJECT, G_CALLBACK( on_dossier_new_object ), self );

	g_signal_connect(
			G_OBJECT( priv->dossier ),
			OFA_SIGNAL_UPDATED_OBJECT, G_CALLBACK( on_dossier_updated_object ), self );

	g_signal_connect(
			G_OBJECT( priv->dossier ),
			OFA_SIGNAL_DELETED_OBJECT, G_CALLBACK( on_dossier_deleted_object ), self );

	g_signal_connect(
			G_OBJECT( priv->dossier ),
			OFA_SIGNAL_VALIDATED_ENTRY, G_CALLBACK( on_dossier_validated_entry ), self );
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

	if( button == priv->ledger_btn ){
		/* make the selection frame sensitive */
		gtk_widget_set_sensitive( GTK_WIDGET( priv->ledger_box ), is_active );

		/* update the default visibility of the columns */
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->ledger_checkbox ), !is_active );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->account_checkbox ), is_active );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->currency_checkbox ), is_active );

		/* and display the entries */
		if( is_active ){
			display_entries_from_ledger( self );
		}

	} else {
		gtk_widget_set_sensitive( GTK_WIDGET( priv->account_entry ), is_active );
		gtk_widget_set_sensitive( GTK_WIDGET( priv->account_select ), is_active );
		gtk_widget_set_sensitive( GTK_WIDGET( priv->f1_label ), is_active );

		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->ledger_checkbox ), is_active );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->account_checkbox ), !is_active );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->currency_checkbox ), !is_active );

		if( is_active ){
			display_entries_from_account( self );
		}
	}
}

/*
 * ofaLedgerCombo callback
 */
static void
onledger_changed( const gchar *mnemo, ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;

	priv = self->private;

	g_free( priv->jou_mnemo );
	priv->jou_mnemo = g_strdup( mnemo );

	display_entries_from_ledger( self );
}

static void
display_entries_from_ledger( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GList *entries;

	priv = self->private;

	if( priv->jou_mnemo && layout_dates_is_valid( self )){
		entries = ofo_entry_get_dataset_by_ledger(
							priv->dossier, priv->jou_mnemo, priv->d_from, priv->d_to );
		display_entries( self, entries );
		ofo_entry_free_dataset( entries );
	}
}

static void
on_account_changed( GtkEntry *entry, ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	ofoAccount *account;
	gchar *str;

	priv = self->private;

	g_free( priv->acc_number );
	priv->acc_number = g_strdup( gtk_entry_get_text( entry ));

	account = ofo_account_get_by_number( priv->dossier, priv->acc_number );

	if( account ){
		str = g_strdup_printf( "%s: %s", _( "Account" ), ofo_account_get_label( account ));
		gtk_label_set_text( priv->f1_label, str );
		g_free( str );
		display_entries_from_account( self );

	} else {
		gtk_label_set_text( priv->f1_label, "" );
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
							priv->dossier, priv->acc_number, priv->d_from, priv->d_to );
		display_entries( self, entries );
		ofo_entry_free_dataset( entries );
	}
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_d_from_focus_out( GtkEntry *entry, GdkEvent *event, ofaViewEntries *self )
{
	return( on_date_focus_out( self, entry, self->private->d_from ));
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_d_to_focus_out( GtkEntry *entry, GdkEvent *event, ofaViewEntries *self )
{
	return( on_date_focus_out( self, entry, self->private->d_to ));
}

static gboolean
on_date_focus_out( ofaViewEntries *self, GtkEntry *entry, myDate *date )
{
	const gchar *text;

	text = gtk_entry_get_text( entry );
	if( !text || !g_utf8_strlen( text, -1 )){
		my_date_clear( date );
	}

	refresh_display( self );

	return( FALSE );
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
	if( str && g_utf8_strlen( str, -1 ) && !my_date_is_valid( priv->d_from )){
		return( FALSE );
	}

	str = gtk_entry_get_text( priv->we_to );
	if( str && g_utf8_strlen( str, -1 ) && !my_date_is_valid( priv->d_to )){
		return( FALSE );
	}

	if( my_date_is_valid( priv->d_from ) &&
			my_date_is_valid( priv->d_to ) &&
			my_date_compare( priv->d_from, priv->d_to ) > 0 ){
		return( FALSE );
	}

	return( TRUE );
}

static void
refresh_display( ofaViewEntries *self )
{
	/* this was the first version, where a change on any dates led to
	 * a full reload of entries - With introduction of GtkTreeModelFilter,
	 * then we just act on the filter
	 */
	/*if( gtk_toggle_button_get_active( self->private->ledger_btn )){
		display_entries_from_ledger( self );
	} else {
		display_entries_from_account( self );
	}*/

	if( self->private->tfilter ){
		gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( self->private->tfilter ));
		compute_balances( self );
	}
}

/*
 * the hash is used to store the balance per currency
 */
static void
display_entries( ofaViewEntries *self, GList *entries )
{
	ofaViewEntriesPrivate *priv;
	GtkTreeModel *tmodel;
	GList *iset;

	priv = self->private;

	tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));
	gtk_list_store_clear( GTK_LIST_STORE( tmodel ));

	for( iset=entries ; iset ; iset=iset->next ){

		display_entry( self, tmodel, OFO_ENTRY( iset->data ));
	}

	compute_balances( self );
}

/*
 * the hash is used to store the balance per currency
 */
static void
reset_balances_hash( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;

	priv = self->private;

	if( priv->balances_hash ){
		g_hash_table_unref( priv->balances_hash );
		priv->balances_hash = NULL;
	}
}

/*
 * we parse the debit/credit strings rather than using the ofoEntry
 * doubles, so that this same function may be used when modifying a
 * row
 *
 * This same function is used, initially when displaying the entries
 * dataset, and then each time we modify the display filter
 */
static void
compute_balances( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GtkWidget *box;
	GtkTreeIter iter;
	gchar *sdeb, *scre, *dev_code;
	perCurrency *pc;

	priv = self->private;
	reset_balances_hash( self );
	priv->balances_hash = g_hash_table_new_full(
					( GHashFunc ) g_str_hash,			/* hash_func */
					( GEqualFunc ) g_str_equal,			/* key_equal_func */
					( GDestroyNotify ) g_free, 			/* key_destroy_func */
					( GDestroyNotify ) g_free );		/* value_destroy_func */

	if( gtk_tree_model_get_iter_first( priv->tfilter, &iter )){
		while( TRUE ){

			gtk_tree_model_get(
					priv->tfilter,
					&iter,
					ENT_COL_DEBIT,    &sdeb,
					ENT_COL_CREDIT,   &scre,
					ENT_COL_CURRENCY, &dev_code,
					-1 );

			pc = find_balance_by_currency( self, dev_code );

			pc->debits += my_double_from_string( sdeb );
			pc->credits += my_double_from_string( scre );

			g_free( sdeb );
			g_free( scre );
			g_free( dev_code );

			if( !gtk_tree_model_iter_next( priv->tfilter, &iter )){
				break;
			}
		}
	}

	box = reset_balances_widgets( self );
	g_hash_table_foreach( priv->balances_hash, ( GHFunc ) display_balance, self );
	gtk_widget_show_all( box );
}

/*
 * the hash is used to store the balance per currency
 */
static perCurrency *
find_balance_by_currency( ofaViewEntries *self, const gchar *dev_code )
{
	ofaViewEntriesPrivate *priv;
	perCurrency *pc;

	priv = self->private;

	pc = ( perCurrency * ) g_hash_table_lookup( priv->balances_hash, dev_code );
	if( !pc ){
		pc = g_new0( perCurrency, 1 );
		pc->debits = 0;
		pc->credits = 0;
		g_hash_table_insert( priv->balances_hash, g_strdup( dev_code ), pc );
	}

	return( pc );
}

static void
display_entry( ofaViewEntries *self, GtkTreeModel *tmodel, ofoEntry *entry )
{
	GtkTreeIter iter;
	gchar *sdope, *sdeff, *sdeb, *scre, *srappro, *status;
	const myDate *d;

	sdope = my_date_to_str( ofo_entry_get_dope( entry ), MY_DATE_DMYY );
	sdeff = my_date_to_str( ofo_entry_get_deffect( entry ), MY_DATE_DMYY );
	sdeb = g_strdup_printf( "%'.2lf", ofo_entry_get_debit( entry ));
	scre = g_strdup_printf( "%'.2lf", ofo_entry_get_credit( entry ));
	d = ofo_entry_get_concil_dval( entry );
	if( my_date_is_valid( d )){
		srappro = my_date_to_str( d, MY_DATE_DMYY );
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
				ENT_COL_LEDGER,   ofo_entry_get_ledger( entry ),
				ENT_COL_ACCOUNT,  ofo_entry_get_account( entry ),
				ENT_COL_DEBIT,    sdeb,
				ENT_COL_CREDIT,   scre,
				ENT_COL_CURRENCY, ofo_entry_get_currency( entry ),
				ENT_COL_RAPPRO,   srappro,
				ENT_COL_STATUS,   status,
				ENT_COL_OBJECT,   entry,
				ENT_COL_VALID,    TRUE,
				-1 );

	g_free( status );
	g_free( srappro );
	g_free( scre );
	g_free( sdeb );
	g_free( sdeff );
	g_free( sdope );
}

static GtkWidget *
reset_balances_widgets( ofaViewEntries *self )
{
	GtkWidget *box;

	box = my_utils_container_get_child_by_name( self->private->top_box, "pt-box" );
	g_return_val_if_fail( box && GTK_IS_BOX( box ), NULL );
	gtk_container_foreach( GTK_CONTAINER( box ), ( GtkCallback ) gtk_widget_destroy, NULL );

	return( box );
}

static void
display_balance( const gchar *dev_code, perCurrency *pc, ofaViewEntries *self )
{
	GtkWidget *box, *row, *label;
	gchar *str;

	if( pc->debits || pc->credits ){

		box = my_utils_container_get_child_by_name( self->private->top_box, "pt-box" );
		g_return_if_fail( box && GTK_IS_BOX( box ));

		row = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 4 );
		gtk_box_pack_start( GTK_BOX( box ), row, FALSE, FALSE, 0 );

		label = gtk_label_new( dev_code );
		gtk_widget_set_margin_right( label, 12 );
		gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
		gtk_label_set_text( GTK_LABEL( label ), dev_code );
		gtk_box_pack_end( GTK_BOX( row ), label, FALSE, FALSE, 4 );

		label = gtk_label_new( NULL );
		gtk_misc_set_alignment( GTK_MISC( label ), 1, 0.5 );
		gtk_label_set_width_chars( GTK_LABEL( label ), 12 );
		str = g_strdup_printf( "%'.2lf", pc->credits );
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );
		gtk_box_pack_end( GTK_BOX( row ), label, FALSE, FALSE, 4 );

		label = gtk_label_new( NULL );
		gtk_misc_set_alignment( GTK_MISC( label ), 1, 0.5 );
		gtk_label_set_width_chars( GTK_LABEL( label ), 12 );
		str = g_strdup_printf( "%'.2lf", pc->debits );
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );
		gtk_box_pack_end( GTK_BOX( row ), label, FALSE, FALSE, 4 );
	}
}

static void
update_balance_amounts( ofaViewEntries *self, const gchar *sdeb, const gchar *scre, const gchar *dev_code, gint column_id, const gchar *text )
{
	ofaViewEntriesPrivate *priv;
	perCurrency *pc;
	GtkWidget *box;

	priv = self->private;

	pc = find_balance_by_currency( self, dev_code );
	switch( column_id ){
		case ENT_COL_DEBIT:
			pc->debits -= my_double_from_string( sdeb );
			pc->debits += my_double_from_string( text );
			break;
		case ENT_COL_CREDIT:
			pc->credits -= my_double_from_string( scre );
			pc->credits += my_double_from_string( text );
			break;
	}

	box = reset_balances_widgets( self );
	g_hash_table_foreach( priv->balances_hash, ( GHFunc ) display_balance, self );
	gtk_widget_show_all( box );
}

static void
update_balance_currency( ofaViewEntries *self, const gchar *sdeb, const gchar *scre, const gchar *dev_code, const gchar *text )
{
	ofaViewEntriesPrivate *priv;
	perCurrency *pc;
	GtkWidget *box;
	gdouble debit, credit;

	priv = self->private;
	debit = my_double_from_string( sdeb );
	credit = my_double_from_string( scre );

	pc = find_balance_by_currency( self, dev_code );
	pc->debits -= debit;
	pc->credits -= credit;

	pc = find_balance_by_currency( self, text );
	pc->debits += debit;
	pc->credits += credit;

	box = reset_balances_widgets( self );
	g_hash_table_foreach( priv->balances_hash, ( GHFunc ) display_balance, self );
	gtk_widget_show_all( box );
}

/*
 * a row is visible if it is consistant with the selected modes:
 * - status of the entry
 * - effect date layout
 */
static gboolean
is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	gboolean visible;
	ofoEntry *entry;
	ofaEntryStatus status;
	const myDate *deffect;

	priv = self->private;
	visible = FALSE;

	gtk_tree_model_get( tmodel, iter, ENT_COL_OBJECT, &entry, -1 );

	if( entry ){
		g_object_unref( entry );
	}

	status = get_entry_status_from_row( self, tmodel, iter );

	switch( status ){
		case ENT_STATUS_ROUGH:
			visible = priv->display_rough;
			break;
		case ENT_STATUS_VALIDATED:
			visible = priv->display_validated;
			break;
		case ENT_STATUS_DELETED:
			visible = priv->display_deleted;
			break;
	}

	if( entry ){
		deffect = ofo_entry_get_deffect( entry );
		visible &= !my_date_is_valid( priv->d_from ) || my_date_compare( priv->d_from, deffect ) <= 0;
		visible &= !my_date_is_valid( priv->d_to ) || my_date_compare( priv->d_to, deffect ) >= 0;
	}

	return( visible );
}

/*
 * default to not display ledger (resp. account) when selection is made
 *  per ledger (resp. account)
 *
 * deleted entries re italic on white background
 * rough entries are standard (blanck on white)
 *  - unvalid entries have red foreground
 * validated entries are on light yellow background
 */
static void
on_cell_data_func( GtkTreeViewColumn *tcolumn,
						GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
						ofaViewEntries *self )
{
	gint column;
	gboolean *priv_flag;
	ofoEntry *entry;
	ofaEntryStatus status;
	GdkRGBA color;
	gboolean is_valid;

	g_return_if_fail( GTK_IS_CELL_RENDERER_TEXT( cell ));

	column = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( tcolumn ), DATA_COLUMN_ID ));
	if( column >= 0 && column < ENT_N_COLUMNS ){
		priv_flag = ( gboolean * ) g_object_get_data( G_OBJECT( tcolumn ), DATA_PRIV_VISIBLE );
		if( priv_flag ){
			gtk_tree_view_column_set_visible( tcolumn, *priv_flag );
		}
	}

	gtk_tree_model_get( tmodel, iter,
						ENT_COL_OBJECT, &entry,
						ENT_COL_VALID,  &is_valid,
						-1 );
	if( entry ){
		g_object_unref( entry );
	}

	status = get_entry_status_from_row( self, tmodel, iter );

	g_object_set( G_OBJECT( cell ),
						"style-set",      FALSE,
						"background-set", FALSE,
						"foreground-set", FALSE,
						NULL );
	switch( status ){
		case ENT_STATUS_VALIDATED:
			gdk_rgba_parse( &color, "#ffffc0" );
			g_object_set( G_OBJECT( cell ), "background-rgba", &color, NULL );
			break;
		case ENT_STATUS_DELETED:
			g_object_set( G_OBJECT( cell ), "style", PANGO_STYLE_ITALIC, NULL );
			break;
		case ENT_STATUS_ROUGH:
			if( !is_valid ){
				gdk_rgba_parse( &color, "#ff0000" );
				g_object_set( G_OBJECT( cell ), "foreground-rgba", &color, NULL );
			}
			break;
		default:
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
			str = my_date2_to_str( begin, MY_DATE_DMYY );
			gtk_entry_set_text( priv->we_from, str );
			g_free( str );
		} else {
			gtk_entry_set_text( priv->we_from, "" );
		}

		if( end && g_date_valid( end )){
			str = my_date2_to_str( end, MY_DATE_DMYY );
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

		} else if( type == OFO_TYPE_LEDGER ){

			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->ledger_btn ), TRUE );
			ofa_ledger_combo_set_selection( priv->ledger_combo, id );
		}
	}
}

/*
 * display columns
 */
static void
on_visible_column_toggled( GtkToggleButton *button, ofaViewEntries *self )
{
	gboolean *priv_flag;

	priv_flag = ( gboolean * ) g_object_get_data( G_OBJECT( button ), DATA_PRIV_VISIBLE );
	*priv_flag = gtk_toggle_button_get_active( button );

	if( self->private->entries_tview ){
		gtk_widget_queue_draw( GTK_WIDGET( self->private->entries_tview ));
	}
}

/*
 * display entries based on their validation status (rough, validated
 *  or deleted)
 */
static void
on_entry_status_toggled( GtkToggleButton *button, ofaViewEntries *self )
{
	gboolean *priv_flag;

	priv_flag = ( gboolean * ) g_object_get_data( G_OBJECT( button ), DATA_PRIV_VISIBLE );
	*priv_flag = gtk_toggle_button_get_active( button );
	refresh_display( self );
}

/*
 * a callback for the 'notify' GObject signal
 *
 * The notify signal is emitted on an object when one of its properties
 * has been changed. Note that getting this signal doesn't guarantee
 * that the value of the property has actually changed, it may also be
 * emitted when the setter for the property is called to reinstate the
 * previous value.
 */
static void
on_edit_switched( GtkSwitch *switch_btn, GParamSpec *pspec, ofaViewEntries *self )
{
	gboolean is_active;

	g_object_get( G_OBJECT( switch_btn ), "active", &is_active, NULL );
	set_renderers_editable( self, is_active );
}

/*
 * rappro date and status are never editable
 */
static void
set_renderers_editable( ofaViewEntries *self, gboolean editable )
{
	gint i;

	for( i=0 ; i<ENT_N_COLUMNS ; ++i ){
		if( st_renderers[i] && i != ENT_COL_RAPPRO && i != ENT_COL_STATUS ){
			g_object_set( G_OBJECT( st_renderers[i] ), "editable", editable, NULL );
		}
	}
}

/*
 * makes use of the two tree models:
 * - [priv->tfilter, iter] is the tree model filter
 * - [tmodel, child_iter] is the underlying list store
 */
static void
on_cell_edited( GtkCellRendererText *cell, gchar *path_str, gchar *text, ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	gint column_id;
	GtkTreeModel *tmodel;
	GtkTreePath *path;
	GtkTreeIter iter, child_iter;
	gchar *sdeb, *scre, *dev_code, *str;
	gboolean is_valid;
	gdouble amount;

	priv = self->private;

	if( priv->tfilter ){
		path = gtk_tree_path_new_from_string( path_str );
		if( gtk_tree_model_get_iter( priv->tfilter, &iter, path )){

			gtk_tree_model_filter_convert_iter_to_child_iter(
					GTK_TREE_MODEL_FILTER( priv->tfilter ),
					&child_iter,
					&iter );

			tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));
			column_id = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( cell ), DATA_COLUMN_ID ));
			sdeb = NULL;
			scre = NULL;
			dev_code = NULL;

			/* needed to update the balances per currency */
			if( column_id == ENT_COL_DEBIT ||
					column_id == ENT_COL_CREDIT ||
					column_id == ENT_COL_CURRENCY ){

				gtk_tree_model_get(
						priv->tfilter,
						&iter,
						ENT_COL_DEBIT,    &sdeb,
						ENT_COL_CREDIT,   &scre,
						ENT_COL_CURRENCY, &dev_code,
						-1 );
			}

			/* reformat amounts */
			if( column_id == ENT_COL_DEBIT || column_id == ENT_COL_CREDIT ){
				amount = my_double_from_string( text );
				str = g_strdup_printf( "%'.2lf", amount );
				/*g_debug( "on_cell_edited: text='%s', amount=%lf, str='%s'", text, amount, str );*/
			} else {
				str = g_strdup( text );
			}

			/* need to set store in two passes in order to have the
			 *  values available when we are checking them */
			gtk_list_store_set(
					GTK_LIST_STORE( tmodel ),
					&child_iter,
					column_id, str,
					-1 );

			g_free( str );

			is_valid = check_row_for_valid( self, priv->tfilter, &iter );

			gtk_list_store_set(
					GTK_LIST_STORE( tmodel ),
					&child_iter,
					ENT_COL_VALID, is_valid,
					-1 );

			gtk_tree_path_free( path );

			if( dev_code ){
				if( column_id == ENT_COL_DEBIT || column_id == ENT_COL_CREDIT ){
					update_balance_amounts( self, sdeb, scre, dev_code, column_id, text );
				} else if( column_id == ENT_COL_CURRENCY ){
					update_balance_currency( self, sdeb, scre, dev_code, text );
				}
			}

			g_free( sdeb );
			g_free( scre );
			g_free( dev_code );

			if( is_valid ){
				save_entry( self, priv->tfilter, &iter );
			}
		}
	}
}

static void
on_row_selected( GtkTreeSelection *select, ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GtkTreeIter iter;
	ofoEntry *entry;
	gboolean is_editable, is_active;

	priv = self->private;

	entry = NULL;
	if( gtk_tree_selection_get_selected( select, NULL, &iter )){
		gtk_tree_model_get( priv->tfilter, &iter, ENT_COL_OBJECT, &entry, -1 );
		if( entry ){
			g_object_unref( entry );
		}

		set_comment( self, "" );

		is_editable =
				( get_entry_status_from_row( self, priv->tfilter, &iter ) == ENT_STATUS_ROUGH );
		gtk_widget_set_sensitive(  GTK_WIDGET( priv->edit_switch ), is_editable );
		g_object_get( G_OBJECT( priv->edit_switch ), "active", &is_active, NULL );
		set_renderers_editable( self, is_editable && is_active );

		/* re-display an eventual error message */
		if( is_editable ){
			check_row_for_valid( self, priv->tfilter, &iter );
		}
	}
}

static gboolean
check_row_for_valid( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gboolean is_valid;

	set_comment( self, "" );

	is_valid = check_row_for_valid_dope( self, tmodel, iter ) &&
				check_row_for_valid_deffect( self, tmodel, iter ) &&
				check_row_for_valid_ledger( self, tmodel, iter ) &&
				check_row_for_valid_account( self, tmodel, iter ) &&
				check_row_for_valid_label( self, tmodel, iter ) &&
				check_row_for_valid_amounts( self, tmodel, iter ) &&
				check_row_for_valid_currency( self, tmodel, iter );

	return( is_valid );
}

static gboolean
check_row_for_valid_dope( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gchar *str;
	GDate date;
	gboolean is_valid;

	is_valid = FALSE;
	g_date_clear( &date, 1 );
	gtk_tree_model_get( tmodel, iter, ENT_COL_DOPE, &str, -1 );
	if( str && g_utf8_strlen( str, -1 )){
		g_date_set_parse( &date, str );
		if( g_date_valid( &date )){
			is_valid = TRUE;
		} else {
			set_comment( self, _( "Invalid operation date" ));
		}
	} else {
		set_comment( self, _( "Empty operation date" ));
	}
	g_free( str );

	return( is_valid );
}

/*
 * effect date of any new entry must be:
 * - greater than the last closing date of the exercice (if any)
 * - greater than the last closing date of the ledger (if any)
 */
static gboolean
check_row_for_valid_deffect( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gchar *sope, *str, *mnemo, *msg, *msg2, *msg3;
	myDate *dope, *deff, *last_close, *close_exe;
	const myDate *close_ledger;
	ofoLedger *ledger;
	gboolean is_valid;

	is_valid = FALSE;
	gtk_tree_model_get( tmodel, iter,
				ENT_COL_DOPE, &sope, ENT_COL_DEFF, &str, ENT_COL_LEDGER, &mnemo, -1 );
	if( sope && g_utf8_strlen( sope, -1 ) &&
			str && g_utf8_strlen( str, -1 ) && mnemo && g_utf8_strlen( mnemo, -1 )){

		dope = my_date_new_from_str( sope, MY_DATE_DMYY );
		if( my_date_is_valid( dope )){

			deff = my_date_new_from_str( str, MY_DATE_DMYY );
			if( my_date_is_valid( deff ) && my_date_compare( deff, dope ) >= 0 ){
				last_close = my_date_new();
				close_exe = ofo_dossier_get_last_closed_exercice( self->private->dossier );
				if( my_date_is_valid( close_exe )){
					my_date_set_from_date( last_close, close_exe );
				}
				ledger = ofo_ledger_get_by_mnemo( self->private->dossier, mnemo );
				if( ledger ){
					close_ledger = ofo_ledger_get_last_closing( ledger );
					if( my_date_is_valid( close_ledger )){
						if( !my_date_is_valid( last_close ) || my_date_compare( close_ledger, last_close ) > 0 ){
							my_date_set_from_date( last_close, close_ledger );
						}
					}
					if( !my_date_is_valid( last_close ) || my_date_compare( deff, last_close ) > 0 ){
						is_valid = TRUE;
					} else {
						msg2 = my_date_to_str( last_close, MY_DATE_DMYY );
						msg3 = my_date_to_str( deff, MY_DATE_DMYY );
						msg = g_strdup_printf( _( "Effect date (%s) lesser than last closing date (%s)" ), msg3, msg2 );
						set_comment( self, msg );
						g_free( msg );
						g_free( msg2 );
						g_free( msg3 );
					}
				} else {
					msg = g_strdup_printf( _( "Unknwown ledger: %s" ), mnemo );
					set_comment( self, msg );
					g_free( msg );
				}
				g_object_unref( last_close );
				g_object_unref( close_exe );

			} else {
				set_comment( self, _( "Invalid effect date, or lesser than operation date" ));
			}
			g_object_unref( deff );

		} else {
			set_comment( self, _( "Invalid operation date" ));
		}
		g_object_unref( dope );

	} else {
		set_comment( self, _( "Empty operation date, effect date or ledger" ));
	}
	g_free( sope );
	g_free( str );
	g_free( mnemo );

	return( is_valid );
}

static gboolean
check_row_for_valid_label( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gchar *str;
	gboolean is_valid;

	is_valid = FALSE;
	gtk_tree_model_get( tmodel, iter, ENT_COL_LABEL, &str, -1 );
	if( str && g_utf8_strlen( str, -1 )){
		is_valid = TRUE;
	} else {
		set_comment( self, _( "Empty label" ));
	}
	g_free( str );

	return( is_valid );
}

static gboolean
check_row_for_valid_ledger( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gchar *str, *msg;
	gboolean is_valid;

	is_valid = FALSE;
	gtk_tree_model_get( tmodel, iter, ENT_COL_LEDGER, &str, -1 );
	if( str && g_utf8_strlen( str, -1 )){
		if( ofo_ledger_get_by_mnemo( self->private->dossier, str )){
			is_valid = TRUE;
		} else {
			msg = g_strdup_printf( _( "Unknwown ledger: %s" ), str );
			set_comment( self, msg );
			g_free( msg );
		}
	} else {
		set_comment( self, _( "Empty ledger mnemonic" ));
	}
	g_free( str );

	return( is_valid );
}

static gboolean
check_row_for_valid_account( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gchar *str, *msg;
	gboolean is_valid;
	ofoAccount *account;

	is_valid = FALSE;
	gtk_tree_model_get( tmodel, iter, ENT_COL_ACCOUNT, &str, -1 );
	if( str && g_utf8_strlen( str, -1 )){
		account = ofo_account_get_by_number( self->private->dossier, str );
		if( account ){
			if( !ofo_account_is_root( account )){
				is_valid = TRUE;
			} else {
				msg = g_strdup_printf( _( "Account %s is a root account" ), str );
				set_comment( self, msg );
				g_free( msg );
			}
		} else {
			msg = g_strdup_printf( _( "Unknwown account: %s" ), str );
			set_comment( self, msg );
			g_free( msg );
		}
	} else {
		set_comment( self, _( "Empty account number" ));
	}
	g_free( str );

	return( is_valid );
}

static gboolean
check_row_for_valid_currency( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gchar *number, *code, *msg;
	gboolean is_valid;
	const gchar *account_currency;
	ofoAccount *account;
	GtkTreeModel *child_tmodel;
	GtkTreeIter child_iter;

	is_valid = FALSE;
	gtk_tree_model_get( tmodel, iter, ENT_COL_ACCOUNT, &number, ENT_COL_CURRENCY, &code, -1 );
	if( number && g_utf8_strlen( number, -1 )){
		account = ofo_account_get_by_number( self->private->dossier, number );
		if( account ){
			if( !ofo_account_is_root( account )){
				account_currency = ofo_account_get_currency( account );
				if( code && g_utf8_strlen( code, -1 )){
					if( !g_utf8_collate( account_currency, code )){
						if( ofo_currency_get_by_code( self->private->dossier, code )){
							is_valid = TRUE;
						} else {
							msg = g_strdup_printf( _( "Unknown currency: %s" ), code );
							set_comment( self, msg );
							g_free( msg );
						}
					} else {
						msg = g_strdup_printf( _( "Account expects %s currency while entry has %s" ), account_currency, code );
						set_comment( self, msg );
						g_free( msg );
					}
				} else {
					child_tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( tmodel ));
					gtk_tree_model_filter_convert_iter_to_child_iter( GTK_TREE_MODEL_FILTER( tmodel ), &child_iter, iter );
					gtk_list_store_set(
							GTK_LIST_STORE( child_tmodel ),
							&child_iter,
							ENT_COL_CURRENCY, account_currency,
							-1 );
				}
			} else {
				msg = g_strdup_printf( _( "Account %s is a root account" ), number );
				set_comment( self, msg );
				g_free( msg );
			}
		} else {
			msg = g_strdup_printf( _( "Unknwown account: %s" ), number );
			set_comment( self, msg );
			g_free( msg );
		}
	} else {
		set_comment( self, _( "Account number is empty" ));
	}
	g_free( number );
	g_free( code );

	return( is_valid );
}

static gboolean
check_row_for_valid_amounts( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gchar *sdeb, *scre;
	gdouble debit, credit;
	gboolean is_valid;

	is_valid = FALSE;
	gtk_tree_model_get( tmodel, iter, ENT_COL_DEBIT, &sdeb, ENT_COL_CREDIT, &scre, -1 );
	if(( sdeb && g_utf8_strlen( sdeb, -1 )) || ( scre && g_utf8_strlen( scre, -1 ))){
		debit = my_double_from_string( sdeb );
		credit = my_double_from_string( scre );
		if(( debit && !credit ) || ( !debit && credit )){
			is_valid = TRUE;
		} else {
			set_comment( self, _( "Only one of debit and credit must be set" ));
		}
	} else {
		set_comment( self, _( "Debit and credit are both empty" ));
	}
	g_free( sdeb );
	g_free( scre );

	return( is_valid );
}

static void
set_comment( ofaViewEntries *self, const gchar *str )
{
	gtk_label_set_text( self->private->comment, str );
}

static gboolean
save_entry( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	ofaViewEntriesPrivate *priv;
	gchar *sdope, *sdeff, *ref, *label, *ledger, *account, *sdeb, *scre, *currency;
	myDate *dope, *deff;
	gint number;
	gdouble debit, credit;
	ofoEntry *entry;
	gboolean ok;

	priv = self->private;
	ok = FALSE;

	gtk_tree_model_get(
			tmodel,
			iter,
			ENT_COL_DOPE,     &sdope,
			ENT_COL_DEFF,     &sdeff,
			ENT_COL_NUMBER,   &number,
			ENT_COL_REF,      &ref,
			ENT_COL_LABEL,    &label,
			ENT_COL_LEDGER,   &ledger,
			ENT_COL_ACCOUNT,  &account,
			ENT_COL_DEBIT,    &sdeb,
			ENT_COL_CREDIT,   &scre,
			ENT_COL_CURRENCY, &currency,
			ENT_COL_OBJECT,   &entry,
			-1 );

	dope = my_date_new_from_str( sdope, MY_DATE_DMYY );
	g_return_val_if_fail( my_date_is_valid( dope ), FALSE );

	deff = my_date_new_from_str( sdeff, MY_DATE_DMYY );
	g_return_val_if_fail( my_date_is_valid( deff ), FALSE );

	debit = my_double_from_string( sdeb );
	credit = my_double_from_string( scre );
	g_debug( "save_entry: sdeb='%s', debit=%lf, scre='%s', credit=%lf", sdeb, debit, scre, credit );

	if( entry ){
		g_object_unref( entry );

		ofo_entry_set_dope( entry, dope );
		ofo_entry_set_deffect( entry, deff );
		ofo_entry_set_ref( entry, ref );
		ofo_entry_set_label( entry, label );
		ofo_entry_set_ledger( entry, ledger );
		ofo_entry_set_account( entry, account );
		ofo_entry_set_debit( entry, debit );
		ofo_entry_set_credit( entry, credit );
		ofo_entry_set_currency( entry, currency );

		ok = ofo_entry_update( entry, priv->dossier );

	} else {
		entry = ofo_entry_new_with_data( priv->dossier,
					dope, deff, label, ref, account, currency, ledger, NULL, debit, credit );
		priv->inserted = entry;
		ok = ofo_entry_insert( entry, priv->dossier );
	}

	g_object_unref( dope );
	g_object_unref( deff );
	g_free( currency );
	g_free( scre );
	g_free( sdeb );
	g_free( account );
	g_free( ledger );
	g_free( label );
	g_free( ref );
	g_free( sdeff );
	g_free( sdope );

	return( ok );
}

static gboolean
find_entry_by_number( ofaViewEntries *self, gint number, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	gint tnumber;

	*tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->private->tfilter ));
	if( gtk_tree_model_get_iter_first( *tmodel, iter )){
		while( TRUE ){
			gtk_tree_model_get( *tmodel, iter, ENT_COL_NUMBER, &tnumber, -1 );
			if( tnumber == number ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( *tmodel, iter )){
				return( FALSE );
			}
		}
	}
	return( FALSE );
}

static void
on_dossier_new_object( ofoDossier *dossier, ofoBase *object, ofaViewEntries *self )
{
	static const gchar *thisfn = "ofa_view_entries_on_dossier_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_ENTRY( object )){
		do_new_entry( self, OFO_ENTRY( object ));
	}
}

static void
do_new_entry( ofaViewEntries *self, ofoEntry *entry )
{
	ofaViewEntriesPrivate *priv;
	GtkTreeModel *tmodel;

	priv = self->private;

	if( priv->inserted && entry != priv->inserted ){
		tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->private->tfilter ));
		display_entry( self, tmodel, entry );
	}

	priv->inserted = NULL;
	compute_balances( self );
}

/*
 * a ledger mnemo, an account number, a currency code may has changed
 */
static void
on_dossier_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaViewEntries *self )
{
	static const gchar *thisfn = "ofa_view_entries_on_dossier_updated_object";

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, user_data=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( prev_id ){
		if( OFO_IS_ACCOUNT( object )){
			do_update_account_number( self, prev_id, ofo_account_get_number( OFO_ACCOUNT( object )));

		} else if( OFO_IS_LEDGER( object )){
			do_updateledger_mnemo( self, prev_id, ofo_ledger_get_mnemo( OFO_LEDGER( object )));

		} else if( OFO_IS_CURRENCY( object )){
			do_update_currency_code( self, prev_id, ofo_currency_get_code( OFO_CURRENCY( object )));
		}
	}
}

static void
do_update_account_number( ofaViewEntries *self, const gchar *prev, const gchar *number )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *str;
	gint cmp;

	tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->private->tfilter ));
	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, &iter, ENT_COL_ACCOUNT, &str, -1 );
			cmp = g_utf8_collate( str, prev );
			g_free( str );
			if( cmp == 0 ){
				gtk_list_store_set( GTK_LIST_STORE( tmodel ), &iter, ENT_COL_ACCOUNT, number, -1 );
			}
			if( !gtk_tree_model_iter_next( tmodel, &iter )){
				break;
			}
		}
	}
}

static void
do_updateledger_mnemo( ofaViewEntries *self, const gchar *prev, const gchar *mnemo )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *str;
	gint cmp;

	tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->private->tfilter ));
	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, &iter, ENT_COL_LEDGER, &str, -1 );
			cmp = g_utf8_collate( str, prev );
			g_free( str );
			if( cmp == 0 ){
				gtk_list_store_set( GTK_LIST_STORE( tmodel ), &iter, ENT_COL_LEDGER, mnemo, -1 );
			}
			if( !gtk_tree_model_iter_next( tmodel, &iter )){
				break;
			}
		}
	}
}

static void
do_update_currency_code( ofaViewEntries *self, const gchar *prev, const gchar *code )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *str;
	gint cmp;

	tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->private->tfilter ));
	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, &iter, ENT_COL_CURRENCY, &str, -1 );
			cmp = g_utf8_collate( str, prev );
			g_free( str );
			if( cmp == 0 ){
				gtk_list_store_set( GTK_LIST_STORE( tmodel ), &iter, ENT_COL_CURRENCY, code, -1 );
			}
			if( !gtk_tree_model_iter_next( tmodel, &iter )){
				break;
			}
		}
	}
}

/*
 * a ledger mnemo, an account number, a currency code or an entry may
 *  have been deleted
 */
static void
on_dossier_deleted_object( ofoDossier *dossier, ofoBase *object, ofaViewEntries *self )
{
	static const gchar *thisfn = "ofa_view_entries_on_dossier_deleted_object";

	g_debug( "%s: dossier=%p, object=%p (%s), user_data=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_ENTRY( object )){
		do_on_deleted_entry( self, OFO_ENTRY( object ));
	}
}

static void
do_on_deleted_entry( ofaViewEntries *self, ofoEntry *entry )
{
	static const gchar *thisfn = "ofa_view_entries_on_deleted_entries";

	g_debug( "%s: self=%p, entry=%p", thisfn, ( void * ) self, ( void * ) entry );
}

static void
on_dossier_validated_entry( ofoDossier *dossier, ofoBase *object, ofaViewEntries *self )
{
	static const gchar *thisfn = "ofa_view_entries_on_dossier_validated_entry";
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *str;

	g_debug( "%s: dossier=%p, object=%p (%s), user_data=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	g_return_if_fail( object && OFO_IS_ENTRY( object ));

	if( find_entry_by_number( self, ofo_entry_get_number( OFO_ENTRY( object )), &tmodel, &iter )){
		str = g_strdup_printf( "%d", ofo_entry_get_status( OFO_ENTRY( object )));
		gtk_list_store_set(
				GTK_LIST_STORE( tmodel ),
				&iter,
				ENT_COL_STATUS, str,
				-1 );
		g_free( str );
	}
}

static gboolean
on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, ofaViewEntries *self )
{
	gboolean stop;

	stop = FALSE;

	if( gtk_switch_get_active( self->private->edit_switch )){

		if( event->keyval == GDK_KEY_Insert || event->keyval == GDK_KEY_KP_Insert ){
			insert_new_row( self );
			stop = TRUE;
		}

		if( event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete ){
			delete_row( self );
			stop = TRUE;
		}
	}

	return( stop );
}

static ofaEntryStatus
get_entry_status_from_row( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gchar *str;
	ofaEntryStatus status;

	gtk_tree_model_get( tmodel, iter, ENT_COL_STATUS, &str, -1 );
	status = atoi( str );
	g_free( str );

	return( status );
}

static void
insert_new_row( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter, child_iter, new_iter, filter_iter;
	gboolean is_empty;
	gchar *str;
	gint pos;
	const gchar *ledger, *account, *dev_code;
	GtkTreePath *path;
	GtkTreeViewColumn *column;
	ofoAccount *account_object;

	priv = self->private;
	is_empty = FALSE;
	select = gtk_tree_view_get_selection( priv->entries_tview );

	/* find the currently selected position in order to insert a line
	 * above the current pos. */
	if( !gtk_tree_selection_get_selected( select, NULL, &iter )){
		if( !gtk_tree_model_get_iter_first( priv->tfilter, &iter )){
			is_empty = TRUE;
		}
	}
	tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));
	if( !is_empty ){
		gtk_tree_model_filter_convert_iter_to_child_iter(
				GTK_TREE_MODEL_FILTER( priv->tfilter ), &child_iter, &iter );
		str = gtk_tree_model_get_string_from_iter( tmodel, &child_iter );
		pos = atoi( str );
		g_free( str );
	} else {
		pos = -1;
	}

	/* set default values that we are able to guess */
	str = g_strdup_printf( "%d", ENT_STATUS_ROUGH );

	if( gtk_toggle_button_get_active( priv->ledger_btn )){
		ledger = priv->jou_mnemo;
		account = NULL;
		dev_code = "";
	} else {
		ledger = NULL;
		account = priv->acc_number;
		account_object = ofo_account_get_by_number( priv->dossier, account );
		dev_code = ofo_account_get_currency( account_object );
	}

	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( tmodel ),
			&new_iter,
			pos,
			ENT_COL_STATUS,   str,
			ENT_COL_LEDGER,   ledger,
			ENT_COL_ACCOUNT,  account,
			ENT_COL_DEBIT,    "",
			ENT_COL_CREDIT,   "",
			ENT_COL_CURRENCY, dev_code,
			-1 );

	g_free( str );

	/* set the selection and the cursor on this new line */
	gtk_tree_model_filter_convert_child_iter_to_iter(
			GTK_TREE_MODEL_FILTER( priv->tfilter ), &filter_iter, &new_iter );
	gtk_tree_selection_select_iter( select, &filter_iter );
	path = gtk_tree_model_get_path( priv->tfilter, &filter_iter );
	column = gtk_tree_view_get_column( priv->entries_tview, 0 );
	gtk_tree_view_set_cursor( priv->entries_tview, path, column, TRUE );
	gtk_tree_path_free( path );

	/* force the edition on this line */
	gtk_switch_set_active( priv->edit_switch, TRUE );
}

static void
delete_row( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter, child_iter;
	ofoEntry *entry;
	gchar *msg, *label;

	priv = self->private;
	select = gtk_tree_view_get_selection( priv->entries_tview );
	if( gtk_tree_selection_get_selected( select, NULL, &iter )){
		gtk_tree_model_get(
				priv->tfilter,
				&iter,
				ENT_COL_LABEL,    &label,
				ENT_COL_OBJECT,   &entry,
				-1 );
		if( get_entry_status_from_row( self, priv->tfilter, &iter) == ENT_STATUS_ROUGH ){
			msg = g_strdup_printf(
					_( "Are you sure you want to remove the '%s' entry" ),
					label );
			if( delete_confirmed( self, msg )){
				gtk_tree_model_filter_convert_iter_to_child_iter(
						GTK_TREE_MODEL_FILTER( priv->tfilter ), &child_iter, &iter );
				tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));
				gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &child_iter );
				if( entry ){
					ofo_entry_delete( entry, priv->dossier );
				}
				compute_balances( self );
			}
			g_free( msg );
		}
		g_free( label );
		if( entry ){
			g_object_unref( entry );
		}
	}
}

/*
 * Returns: %TRUE if the deletion is confirmed by the user.
 */
static gboolean
delete_confirmed( const ofaViewEntries *self, const gchar *message )
{
	GtkWidget *dialog;
	gint response;

	dialog = gtk_message_dialog_new(
			NULL,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			"%s", message );

	gtk_dialog_add_buttons( GTK_DIALOG( dialog ),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_DELETE, GTK_RESPONSE_OK,
			NULL );

	response = gtk_dialog_run( GTK_DIALOG( dialog ));

	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
}
