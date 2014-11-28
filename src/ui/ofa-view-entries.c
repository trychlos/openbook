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
#include "api/ofa-settings.h"
#include "api/ofo-base.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"

#include "ui/my-cell-renderer-amount.h"
#include "ui/my-cell-renderer-date.h"
#include "ui/my-editable-date.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-ledger-combo.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-page.h"
#include "ui/ofa-page-prot.h"
#include "ui/ofa-view-entries.h"

/* columns in the entries view
 * declared before the private data in order to be able to dimension
 * the renderers array
 */
enum {
	ENT_COL_DOPE = 0,
	ENT_COL_DEFF,
	ENT_COL_NUMBER,						/* entry number */
	ENT_COL_REF,
	ENT_COL_LEDGER,
	ENT_COL_ACCOUNT,
	ENT_COL_LABEL,
	ENT_COL_DRECONCIL,
	ENT_COL_DEBIT,
	ENT_COL_CREDIT,
	ENT_COL_CURRENCY,
	ENT_COL_STATUS,
										/*  below columns are not visible */
	ENT_COL_OBJECT,
	ENT_COL_MSGERR,
	ENT_COL_MSGWARN,
	ENT_COL_DOPE_SET,					/* operation date set by the user */
	ENT_COL_DEFF_SET,					/* effect date set by the user */
	ENT_COL_CURRENCY_SET,				/* currency set by the user */
	ENT_N_COLUMNS
};

/* priv instance data
 */
struct _ofaViewEntriesPrivate {

	/* internals
	 */
	ofoDossier        *dossier;			/* dossier */
	const GDate       *dossier_opening;
	GDate              d_from;
	GDate              d_to;

	/* UI
	 */
	GtkContainer      *top_box;			/* reparented from dialog */

	/* frame 1: general selection
	 */
	GtkToggleButton   *ledger_btn;
	ofaLedgerCombo    *ledger_combo;
	GtkComboBox       *ledger_box;
	gchar             *jou_mnemo;

	GtkToggleButton   *account_btn;
	GtkEntry          *account_entry;
	GtkButton         *account_select;
	gchar             *acc_number;

	GtkLabel          *f1_label;

	/* frame 2: effect dates layout
	 */
	GtkEntry          *we_from;
	GtkLabel          *wl_from;
	GtkEntry          *we_to;
	GtkLabel          *wl_to;

	/* frame 3: entry status
	 */
	gboolean           display_rough;
	gboolean           display_validated;
	gboolean           display_deleted;

	/* frame 4: visible columns
	 */
	GtkCheckButton    *account_checkbox;
	GtkCheckButton    *ledger_checkbox;
	GtkCheckButton    *currency_checkbox;

	gboolean           dope_visible;
	gboolean           deffect_visible;
	gboolean           ref_visible;
	gboolean           ledger_visible;
	gboolean           account_visible;
	gboolean           dreconcil_visible;
	gboolean           status_visible;

	/* frame 5: edition switch
	 */
	GtkSwitch         *edit_switch;

	/* entries list view
	 */
	GtkCellRenderer   *renderers[ENT_N_COLUMNS];
	GtkTreeView       *entries_tview;
	GtkTreeModel      *tfilter;			/* GtkTreeModelFilter stacked on the GtkListStore */
	GtkTreeModel      *tsort;			/* GtkTreeModelSort stacked on the GtkTreeModelFilter */
	GtkTreeModel      *tstore;
	GtkTreeViewColumn *sort_column;

	/* footer
	 */
	GtkLabel          *comment;
	GHashTable        *balances_hash;
};

/* the balance per currency, mostly useful when displaying per ledger
 */
typedef struct {
	gdouble debits;
	gdouble credits;
}
	sCurrency;

/* the id of the column is set against some columns of interest,
 * typically whose that we want be able to show/hide */
#define DATA_COLUMN_ID                  "ofa-data-column-id"

/* a pointer to the boolean xxx_visible is set against the check
 * buttons - so that we are able to have only one callback
 * this same pointer is set against the columns to be used
 * when actually displaying the columns in on_cell_data_func() */
#define DATA_PRIV_VISIBLE               "ofa-data-priv-visible"

/* set against status toggle buttons in order to be able to set
 * the user prefs */
#define DATA_ROW_STATUS                 "ofa-data-row-status"

/* it appears that Gtk+ displays a counter intuitive sort indicator:
 * when asking for ascending sort, Gtk+ displays a 'v' indicator
 * while we would prefer the '^' version -
 * we are defining the inverse indicator, and we are going to sort
 * in reverse order to have our own illusion
 */
#define OFA_SORT_ASCENDING              GTK_SORT_DESCENDING
#define OFA_SORT_DESCENDING             GTK_SORT_ASCENDING

/* when editing an entry, we may have two levels of errors:
 * fatal error: the entry is not valid and cannot be saved
 *              (e.g. a mandatory data is empty)
 * warning: the entry may be valid, but will not be applied in standard
 *          conditions (e.g. effect date is before the exercice)
 */
#define RGBA_NORMAL                     "#000000"		/* black */
#define RGBA_ERROR                      "#ff0000"		/* full red */
#define RGBA_WARNING                    "#ff8000"		/* orange */

/* error levels, in ascending order
 */
enum {
	ENT_ERR_NONE = 0,
	ENT_ERR_WARNING,
	ENT_ERR_ERROR
};

/* other colors */
#define RGBA_VALIDATED                  "#e6cf00"		/* gold background */
#define RGBA_DELETED                    "#808080"		/* gray foreground */
#define RGBA_BALANCE                    PAGE_RGBA_FOOTER

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-view-entries.piece.ui";
static const gchar *st_ui_id            = "ViewEntriesWindow";

static const gchar *st_pref_selection   = "ViewEntriesSelection";
static const gchar *st_pref_ledger      = "ViewEntriesLedger";
static const gchar *st_pref_account     = "ViewEntriesAccount";
static const gchar *st_pref_d_from      = "ViewEntriesDFrom";
static const gchar *st_pref_d_to        = "ViewEntriesDTo";
static const gchar *st_pref_st_rough    = "ViewEntriesStRough";
static const gchar *st_pref_st_valid    = "ViewEntriesStValidated";
static const gchar *st_pref_st_deleted  = "ViewEntriesStDeleted";
static const gchar *st_pref_columns     = "ViewEntriesColumns";
static const gchar *st_pref_sort_c      = "ViewEntriesSortC";
static const gchar *st_pref_sort_s      = "ViewEntriesSortS";

#define SEL_LEDGER                      "Ledger"
#define SEL_ACCOUNT                     "Account"

G_DEFINE_TYPE( ofaViewEntries, ofa_view_entries, OFA_TYPE_PAGE )

static GtkWidget     *v_setup_view( ofaPage *page );
static void           reparent_from_dialog( ofaViewEntries *self, GtkContainer *parent );
static void           setup_gen_selection( ofaViewEntries *self );
static void           setup_ledger_selection( ofaViewEntries *self );
static void           setup_account_selection( ofaViewEntries *self );
static void           setup_dates_selection( ofaViewEntries *self );
static void           setup_status_selection( ofaViewEntries *self );
static void           setup_display_columns( ofaViewEntries *self );
static gboolean       has_column_id( GList *id_list, gint id );
static void           setup_edit_switch( ofaViewEntries *self );
static GtkTreeView   *setup_entries_treeview( ofaViewEntries *self );
static gint           on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaViewEntries *self );
static gint           cmp_strings( ofaViewEntries *self, const gchar *stra, const gchar *strb );
static gint           cmp_amounts( ofaViewEntries *self, const gchar *stra, const gchar *strb );
static void           on_header_clicked( GtkTreeViewColumn *column, ofaViewEntries *self );
static void           setup_footer( ofaViewEntries *self );
static void           setup_signaling_connect( ofaViewEntries *self );
static GtkWidget     *v_setup_buttons( ofaPage *page );
static void           v_init_view( ofaPage *page );
static void           set_visible_columns( ofaViewEntries *self );
static GtkWidget     *v_get_top_focusable_widget( const ofaPage *page );
static void           on_gen_selection_toggled( GtkToggleButton *button, ofaViewEntries *self );
static void           on_ledger_changed( const gchar *mnemo, ofaViewEntries *self );
static void           display_entries_from_ledger( ofaViewEntries *self );
static void           on_account_changed( GtkEntry *entry, ofaViewEntries *self );
static void           on_account_select( GtkButton *button, ofaViewEntries *self );
static void           display_entries_from_account( ofaViewEntries *self );
static gboolean       on_d_from_focus_out( GtkEntry *entry, GdkEvent *event, ofaViewEntries *self );
static gboolean       on_d_to_focus_out( GtkEntry *entry, GdkEvent *event, ofaViewEntries *self );
static gboolean       on_date_focus_out( ofaViewEntries *self, GtkEntry *entry, GDate *date, const gchar *pref );
static gboolean       layout_dates_is_valid( ofaViewEntries *self );
static void           refresh_display( ofaViewEntries *self );
static void           display_entries( ofaViewEntries *self, GList *entries );
static void           reset_balances_hash( ofaViewEntries *self );
static void           compute_balances( ofaViewEntries *self );
static sCurrency     *find_balance_by_currency( ofaViewEntries *self, const gchar *dev_code );
static void           display_entry( ofaViewEntries *self, ofoEntry *entry, GtkTreeIter *iter );
static GtkWidget     *reset_balances_widgets( ofaViewEntries *self );
static void           display_balance( const gchar *dev_code, sCurrency *pc, ofaViewEntries *self );
static void           set_balance_currency_label_position( ofaViewEntries *self );
static void           set_balance_currency_label_margin( GtkWidget *widget, ofaViewEntries *self );
static gboolean       is_visible_row( GtkTreeModel *tfilter, GtkTreeIter *iter, ofaViewEntries *self );
static void           on_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaViewEntries *self );
static void           on_entry_status_toggled( GtkToggleButton *button, ofaViewEntries *self );
static void           on_visible_column_toggled( GtkToggleButton *button, ofaViewEntries *self );
static void           on_edit_switched( GtkSwitch *switch_btn, GParamSpec *pspec, ofaViewEntries *self );
static void           set_renderers_editable( ofaViewEntries *self, gboolean editable );
static void           on_cell_edited( GtkCellRendererText *cell, gchar *path, gchar *text, ofaViewEntries *self );
static gint           get_data_set_indicator( ofaViewEntries *self, gint column_id );
static void           set_data_set_indicator( ofaViewEntries *self, gint column_id, GtkTreeIter *iter );
static void           on_row_selected( GtkTreeSelection *select, ofaViewEntries *self );
static void           check_row_for_valid( ofaViewEntries *self, GtkTreeIter *iter );
static gboolean       check_row_for_valid_dope( ofaViewEntries *self, GtkTreeIter *iter );
static gboolean       check_row_for_valid_deffect( ofaViewEntries *self, GtkTreeIter *iter );
static gboolean       check_row_for_valid_label( ofaViewEntries *self, GtkTreeIter *iter );
static gboolean       check_row_for_valid_ledger( ofaViewEntries *self, GtkTreeIter *iter );
static gboolean       check_row_for_valid_account( ofaViewEntries *self, GtkTreeIter *iter );
static gboolean       check_row_for_valid_currency( ofaViewEntries *self, GtkTreeIter *iter );
static gboolean       check_row_for_valid_amounts( ofaViewEntries *self, GtkTreeIter *iter );
static void           check_row_for_cross_deffect( ofaViewEntries *self, GtkTreeIter *iter );
static gboolean       set_default_deffect( ofaViewEntries *self, GtkTreeIter *iter );
static GDate         *get_min_deffect( ofaViewEntries *self, const GDate *dope, ofoLedger *ledger );
static GDate         *get_max_past_deffect( ofaViewEntries *self );
static gboolean       check_row_for_cross_currency( ofaViewEntries *self, GtkTreeIter *iter );
static void           reset_error_msg( ofaViewEntries *self, GtkTreeIter *iter );
static void           set_error_msg( ofaViewEntries *self, GtkTreeIter *iter, const gchar *str );
static void           set_warning_msg( ofaViewEntries *self, GtkTreeIter *iter, const gchar *str );
static void           display_error_msg( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean       save_entry( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean       find_entry_by_number( ofaViewEntries *self, gint number, GtkTreeIter *iter );
static void           on_dossier_new_object( ofoDossier *dossier, ofoBase *object, ofaViewEntries *self );
static void           on_dossier_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaViewEntries *self );
static void           do_update_account_number( ofaViewEntries *self, const gchar *prev, const gchar *number );
static void           do_update_ledger_mnemo( ofaViewEntries *self, const gchar *prev, const gchar *mnemo );
static void           do_update_currency_code( ofaViewEntries *self, const gchar *prev, const gchar *code );
static void           on_dossier_deleted_object( ofoDossier *dossier, ofoBase *object, ofaViewEntries *self );
static void           do_on_deleted_entry( ofaViewEntries *self, ofoEntry *entry );
static void           on_dossier_validated_entry( ofoDossier *dossier, ofoBase *object, ofaViewEntries *self );
static gboolean       on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, ofaViewEntries *self );
static ofaEntryStatus get_row_status( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static GDate         *get_row_deffect( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gint           get_row_errlevel( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
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

	/* free data members here */
	priv = OFA_VIEW_ENTRIES( instance )->priv;

	g_free( priv->jou_mnemo );
	g_free( priv->acc_number );
	reset_balances_hash( OFA_VIEW_ENTRIES( instance ));

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_view_entries_parent_class )->finalize( instance );
}

static void
view_entries_dispose( GObject *instance )
{
	g_return_if_fail( OFA_IS_VIEW_ENTRIES( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_view_entries_parent_class )->dispose( instance );
}

static void
ofa_view_entries_init( ofaViewEntries *self )
{
	static const gchar *thisfn = "ofa_view_entries_init";
	ofaViewEntriesPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( OFA_IS_VIEW_ENTRIES( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_VIEW_ENTRIES, ofaViewEntriesPrivate );
	priv = self->priv;

	my_date_clear( &priv->d_from );
	my_date_clear( &priv->d_to );

	/* default visible columns */
	priv->dope_visible = TRUE;
	priv->deffect_visible = FALSE;
	priv->ref_visible = FALSE;
	priv->ledger_visible = TRUE;
	priv->account_visible = TRUE;
	priv->dreconcil_visible = FALSE;
	priv->status_visible = FALSE;
}

static void
ofa_view_entries_class_init( ofaViewEntriesClass *klass )
{
	static const gchar *thisfn = "ofa_view_entries_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = view_entries_dispose;
	G_OBJECT_CLASS( klass )->finalize = view_entries_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;

	g_type_class_add_private( klass, sizeof( ofaViewEntriesPrivate ));
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	ofaViewEntriesPrivate *priv;
	GtkWidget *frame;

	priv = OFA_VIEW_ENTRIES( page )->priv;

	priv->dossier = ofa_page_get_dossier( page );
	priv->dossier_opening = ofo_dossier_get_exe_begin( priv->dossier );

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
	self->priv->top_box = GTK_CONTAINER( box );

	/* attach our box to the parent's frame */
	gtk_widget_reparent( box, GTK_WIDGET( parent ));
}

static void
setup_gen_selection( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GtkToggleButton *btn;
	gchar *text;

	priv = self->priv;

	btn = ( GtkToggleButton * ) my_utils_container_get_child_by_name( priv->top_box, "f1-btn-ledger" );
	g_return_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ));
	g_signal_connect( G_OBJECT( btn ), "toggled", G_CALLBACK( on_gen_selection_toggled ), self );
	priv->ledger_btn = btn;
	gtk_toggle_button_set_active( priv->ledger_btn, FALSE );

	btn = ( GtkToggleButton * ) my_utils_container_get_child_by_name( priv->top_box, "f1-btn-account" );
	g_return_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ));
	g_signal_connect( G_OBJECT( btn ), "toggled", G_CALLBACK( on_gen_selection_toggled ), self );
	priv->account_btn = btn;
	gtk_toggle_button_set_active( priv->account_btn, FALSE );

	text = ofa_settings_get_string( st_pref_selection );
	if( text && g_utf8_strlen( text, -1 )){
		if( !g_utf8_collate( text, SEL_ACCOUNT )){
			gtk_toggle_button_set_active( priv->account_btn, TRUE );
		/* default to select by ledger */
		} else {
			gtk_toggle_button_set_active( priv->ledger_btn, TRUE );
		}
	}
	g_free( text );
}

static void
setup_ledger_selection( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	ofaLedgerComboParms parms;
	gchar *initial_mnemo;

	priv = self->priv;

	initial_mnemo = ofa_settings_get_string( st_pref_ledger );

	parms.container = priv->top_box;
	parms.dossier = priv->dossier;
	parms.combo_name = "f1-ledger";
	parms.label_name = NULL;
	parms.disp_mnemo = FALSE;
	parms.disp_label = TRUE;
	parms.pfnSelected = ( ofaLedgerComboCb ) on_ledger_changed;
	parms.user_data = self;
	parms.initial_mnemo = initial_mnemo;

	priv->ledger_combo = ofa_ledger_combo_new( &parms );

	priv->ledger_box = ( GtkComboBox * ) my_utils_container_get_child_by_name( priv->top_box, "f1-ledger" );
	g_return_if_fail( priv->ledger_box && GTK_IS_COMBO_BOX( priv->ledger_box ));

	g_free( initial_mnemo );
}

static void
setup_account_selection( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GtkWidget *image;
	GtkButton *btn;
	GtkWidget *widget;
	gchar *text;

	priv = self->priv;

	btn = ( GtkButton * ) my_utils_container_get_child_by_name( priv->top_box, "f1-account-select" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	image = gtk_image_new_from_icon_name( "gtk-index", GTK_ICON_SIZE_BUTTON );
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

	text = ofa_settings_get_string( st_pref_account );
	if( text && g_utf8_strlen( text, -1 )){
		gtk_entry_set_text( priv->account_entry, text );
	}
	g_free( text );
}

static void
setup_dates_selection( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	gchar *text;

	priv = self->priv;

	priv->we_from = GTK_ENTRY( my_utils_container_get_child_by_name( priv->top_box, "f2-from" ));
	priv->wl_from = GTK_LABEL( my_utils_container_get_child_by_name( priv->top_box, "f2-from-label" ));

	my_editable_date_init( GTK_EDITABLE( priv->we_from ));
	my_editable_date_set_format( GTK_EDITABLE( priv->we_from ), MY_DATE_DMYY );
	my_editable_date_set_label( GTK_EDITABLE( priv->we_from ), GTK_WIDGET( priv->wl_from ), MY_DATE_DMMM );
	my_editable_date_set_mandatory( GTK_EDITABLE( priv->we_from ), FALSE );

	g_signal_connect(
			G_OBJECT( priv->we_from ),
			"focus-out-event", G_CALLBACK( on_d_from_focus_out ), self );

	text = ofa_settings_get_string( st_pref_d_from );
	if( text && g_utf8_strlen( text, -1 )){
		my_date_set_from_sql( &priv->d_from, text );
	}
	g_free( text );
	my_editable_date_set_date( GTK_EDITABLE( priv->we_from ), &priv->d_from );

	priv->we_to = GTK_ENTRY( my_utils_container_get_child_by_name( priv->top_box, "f2-to" ));
	priv->wl_to = GTK_LABEL( my_utils_container_get_child_by_name( priv->top_box, "f2-to-label" ));

	my_editable_date_init( GTK_EDITABLE( priv->we_to ));
	my_editable_date_set_format( GTK_EDITABLE( priv->we_to ), MY_DATE_DMYY );
	my_editable_date_set_label( GTK_EDITABLE( priv->we_to ), GTK_WIDGET( priv->wl_to ), MY_DATE_DMMM );
	my_editable_date_set_mandatory( GTK_EDITABLE( priv->we_to ), FALSE );

	g_signal_connect(
			G_OBJECT( priv->we_to ),
			"focus-out-event", G_CALLBACK( on_d_to_focus_out ), self );

	text = ofa_settings_get_string( st_pref_d_to );
	if( text && g_utf8_strlen( text, -1 )){
		my_date_set_from_sql( &priv->d_to, text );
	}
	g_free( text );
	my_editable_date_set_date( GTK_EDITABLE( priv->we_to ), &priv->d_to );
}

static void
setup_status_selection( ofaViewEntries *self )
{
	static const gchar *thisfn = "ofa_view_entries_setup_status_selection";
	ofaViewEntriesPrivate *priv;
	GtkWidget *button;
	gboolean brough, bvalid, bdeleted;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = self->priv;

	button = my_utils_container_get_child_by_name( priv->top_box, "f3-rough" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "toggled", G_CALLBACK( on_entry_status_toggled), self );
	g_object_set_data( G_OBJECT( button ), DATA_PRIV_VISIBLE, &priv->display_rough );
	g_object_set_data( G_OBJECT( button ), DATA_ROW_STATUS, ( gpointer ) st_pref_st_rough );
	brough = ofa_settings_get_boolean( st_pref_st_rough );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), brough );

	button = my_utils_container_get_child_by_name( priv->top_box, "f3-validated" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "toggled", G_CALLBACK( on_entry_status_toggled), self );
	g_object_set_data( G_OBJECT( button ), DATA_PRIV_VISIBLE, &priv->display_validated );
	g_object_set_data( G_OBJECT( button ), DATA_ROW_STATUS, ( gpointer ) st_pref_st_valid );
	bvalid = ofa_settings_get_boolean( st_pref_st_valid );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bvalid );

	button = my_utils_container_get_child_by_name( priv->top_box, "f3-deleted" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "toggled", G_CALLBACK( on_entry_status_toggled), self );
	g_object_set_data( G_OBJECT( button ), DATA_PRIV_VISIBLE, &priv->display_deleted );
	g_object_set_data( G_OBJECT( button ), DATA_ROW_STATUS, ( gpointer ) st_pref_st_deleted );
	bdeleted = ofa_settings_get_boolean( st_pref_st_deleted );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), bdeleted );

	/* for now, do not display deleted entries */
	gtk_widget_set_sensitive( button, FALSE );
}

static void
setup_display_columns( ofaViewEntries *self )
{
	static const gchar *thisfn = "ofa_view_entries_setup_display_columns";
	ofaViewEntriesPrivate *priv;
	GtkWidget *widget;
	GList *id_list;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = self->priv;

	id_list = ofa_settings_get_uint_list( st_pref_columns );

	widget = my_utils_container_get_child_by_name( priv->top_box, "f4-dope" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_visible_column_toggled), self );
	priv->dope_visible = has_column_id( id_list, ENT_COL_DOPE );
	g_object_set_data( G_OBJECT( widget ), DATA_PRIV_VISIBLE, &priv->dope_visible );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), priv->dope_visible );

	widget = my_utils_container_get_child_by_name( priv->top_box, "f4-deffect" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_visible_column_toggled), self );
	priv->deffect_visible = has_column_id( id_list, ENT_COL_DEFF );
	g_object_set_data( G_OBJECT( widget ), DATA_PRIV_VISIBLE, &priv->deffect_visible );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), priv->deffect_visible );

	widget = my_utils_container_get_child_by_name( priv->top_box, "f4-ref" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_visible_column_toggled), self );
	priv->ref_visible = has_column_id( id_list, ENT_COL_REF );
	g_object_set_data( G_OBJECT( widget ), DATA_PRIV_VISIBLE, &priv->ref_visible );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), priv->ref_visible );

	widget = my_utils_container_get_child_by_name( priv->top_box, "f4-ledger" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_visible_column_toggled), self );
	priv->ledger_visible = has_column_id( id_list, ENT_COL_LEDGER );
	g_object_set_data( G_OBJECT( widget ), DATA_PRIV_VISIBLE, &priv->ledger_visible );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), priv->ledger_visible );
	priv->ledger_checkbox = GTK_CHECK_BUTTON( widget );

	widget = my_utils_container_get_child_by_name( priv->top_box, "f4-account" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_visible_column_toggled), self );
	priv->account_visible = has_column_id( id_list, ENT_COL_ACCOUNT );
	g_object_set_data( G_OBJECT( widget ), DATA_PRIV_VISIBLE, &priv->account_visible );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), priv->account_visible );
	priv->account_checkbox = GTK_CHECK_BUTTON( widget );

	widget = my_utils_container_get_child_by_name( priv->top_box, "f4-rappro" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_visible_column_toggled), self );
	priv->dreconcil_visible = has_column_id( id_list, ENT_COL_DRECONCIL );
	g_object_set_data( G_OBJECT( widget ), DATA_PRIV_VISIBLE, &priv->dreconcil_visible );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), priv->dreconcil_visible );

	widget = my_utils_container_get_child_by_name( priv->top_box, "f4-status" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_visible_column_toggled), self );
	priv->status_visible = has_column_id( id_list, ENT_COL_STATUS );
	g_object_set_data( G_OBJECT( widget ), DATA_PRIV_VISIBLE, &priv->status_visible );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), priv->status_visible );

	g_list_free( id_list );
}

/*
 * is the column identifier found in the list read from user prefs ?
 */
static gboolean
has_column_id( GList *id_list, gint id )
{
	gboolean found;
	GList *it;

	for( it=id_list, found=FALSE ; it ; it=it->next ){
		if( GPOINTER_TO_INT( it->data ) == id ){
			found = TRUE;
			break;
		}
	}

	return( found );
}

static void
setup_edit_switch( ofaViewEntries *self )
{
	static const gchar *thisfn = "ofa_view_entries_setup_edit_switch";
	ofaViewEntriesPrivate *priv;
	GtkWidget *widget;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = self->priv;

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
 *
 *   As we want both filter and sort the view, we have to stack a
 *   GtkTreeModelSort onto the GtkTreeModelFilter, itself being stacked
 *   onto the GtkTreeModel of the GtkListStore
 */
static GtkTreeView *
setup_entries_treeview( ofaViewEntries *self )
{
	static const gchar *thisfn = "ofa_view_entries_setup_entries_treeview";
	ofaViewEntriesPrivate *priv;
	GtkTreeView *tview;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	gint column_id;
	gint sort_id, sort_sens;
	GtkTreeViewColumn *sort_column;

	priv = self->priv;

	tview = ( GtkTreeView * ) my_utils_container_get_child_by_name( priv->top_box, "p1-entries" );
	g_return_val_if_fail( tview && GTK_IS_TREE_VIEW( tview ), NULL );

	priv->tstore = GTK_TREE_MODEL( gtk_list_store_new(
			ENT_N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,		/* dope, deff, number */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* ref, ledger, account */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* label, dreconcil, debit */
			G_TYPE_STRING,	G_TYPE_STRING, G_TYPE_STRING,	/* credit, currency, status */
			/* below columns are not visible */
			G_TYPE_OBJECT,									/* the entry itself */
			G_TYPE_STRING, G_TYPE_STRING,					/* error msg, warning msg */
			G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN	/* dope_set, deff_set, currency_set */
			));

	priv->tfilter = gtk_tree_model_filter_new( priv->tstore, NULL );
	g_object_unref( priv->tstore );
	gtk_tree_model_filter_set_visible_func(
			GTK_TREE_MODEL_FILTER( priv->tfilter ),
			( GtkTreeModelFilterVisibleFunc ) is_visible_row,
			self,
			NULL );

	priv->tsort = gtk_tree_model_sort_new_with_model( priv->tfilter );
	g_object_unref( priv->tfilter );

	gtk_tree_view_set_model( tview, priv->tsort );
	g_object_unref( priv->tsort );

	g_debug( "%s: self=%p, view=%p, tstore=%p, tfilter=%p, tsort=%p",
			thisfn, ( void * ) self, ( void * ) tview,
					( void * ) priv->tstore, ( void * ) priv->tfilter, ( void * ) priv->tsort );

	/* default is to sort by ascending operation date
	 */
	sort_column = NULL;
	sort_id = ofa_settings_get_uint( st_pref_sort_c );
	if( sort_id < 0 ){
		sort_id = ENT_COL_DOPE;
	}
	sort_sens = ofa_settings_get_uint( st_pref_sort_s );
	if( sort_sens < 0 ){
		sort_sens = OFA_SORT_ASCENDING;
	}

	/* operation date
	 */
	column_id = ENT_COL_DOPE;
	text_cell = gtk_cell_renderer_text_new();
	my_cell_renderer_date_init( text_cell );
	priv->renderers[column_id] = text_cell;
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Operation" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	g_object_set_data( G_OBJECT( column ), DATA_PRIV_VISIBLE, &priv->dope_visible );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( sort_id == column_id ){
		sort_column = column;
	}

	/* effect date
	 */
	column_id = ENT_COL_DEFF;
	text_cell = gtk_cell_renderer_text_new();
	my_cell_renderer_date_init( text_cell );
	priv->renderers[column_id] = text_cell;
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Effect" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	g_object_set_data( G_OBJECT( column ), DATA_PRIV_VISIBLE, &priv->deffect_visible );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( sort_id == column_id ){
		sort_column = column;
	}

	/* piece's reference
	 */
	column_id = ENT_COL_REF;
	text_cell = gtk_cell_renderer_text_new();
	priv->renderers[column_id] = text_cell;
	g_object_set( G_OBJECT( text_cell ), "ellipsize", PANGO_ELLIPSIZE_END, NULL );
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Piece" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	g_object_set_data( G_OBJECT( column ), DATA_PRIV_VISIBLE, &priv->ref_visible );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( sort_id == column_id ){
		sort_column = column;
	}

	/* ledger
	 */
	column_id = ENT_COL_LEDGER;
	text_cell = gtk_cell_renderer_text_new();
	priv->renderers[column_id] = text_cell;
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Ledger" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	g_object_set_data( G_OBJECT( column ), DATA_PRIV_VISIBLE, &priv->ledger_visible );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( sort_id == column_id ){
		sort_column = column;
	}

	/* account
	 */
	column_id = ENT_COL_ACCOUNT;
	text_cell = gtk_cell_renderer_text_new();
	priv->renderers[column_id] = text_cell;
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Account" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	g_object_set_data( G_OBJECT( column ), DATA_PRIV_VISIBLE, &priv->account_visible );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( sort_id == column_id ){
		sort_column = column;
	}

	/* label
	 */
	column_id = ENT_COL_LABEL;
	text_cell = gtk_cell_renderer_text_new();
	priv->renderers[column_id] = text_cell;
	g_object_set( G_OBJECT( text_cell ), "ellipsize", PANGO_ELLIPSIZE_END, NULL );
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( sort_id == column_id ){
		sort_column = column;
	}

	/* reconciliation date
	 */
	column_id = ENT_COL_DRECONCIL;
	text_cell = gtk_cell_renderer_text_new();
	priv->renderers[column_id] = text_cell;
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Reconcil." ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	g_object_set_data( G_OBJECT( column ), DATA_PRIV_VISIBLE, &priv->dreconcil_visible );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( sort_id == column_id ){
		sort_column = column;
	}

	/* debit
	 */
	column_id = ENT_COL_DEBIT;
	text_cell = gtk_cell_renderer_text_new();
	priv->renderers[column_id] = text_cell;
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	my_cell_renderer_amount_init( text_cell );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Debit" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_set_min_width( column, 110 );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( sort_id == column_id ){
		sort_column = column;
	}

	/* credit
	 */
	column_id = ENT_COL_CREDIT;
	text_cell = gtk_cell_renderer_text_new();
	priv->renderers[column_id] = text_cell;
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	my_cell_renderer_amount_init( text_cell );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Credit" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_set_min_width( column, 110 );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( sort_id == column_id ){
		sort_column = column;
	}

	/* currency
	 */
	column_id = ENT_COL_CURRENCY;
	text_cell = gtk_cell_renderer_text_new();
	priv->renderers[column_id] = text_cell;
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Cur." ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_min_width( column, 32 );	/* "EUR" width */
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( sort_id == column_id ){
		sort_column = column;
	}

	/* entry status
	 */
	column_id = ENT_COL_STATUS;
	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 0.5, 0.5 );
	priv->renderers[column_id] = text_cell;
	g_object_set_data( G_OBJECT( text_cell ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_cell_edited ), self );
	column = gtk_tree_view_column_new_with_attributes(
			_( "St." ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_alignment( column, 0.5 );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	g_object_set_data( G_OBJECT( column ), DATA_PRIV_VISIBLE, &priv->status_visible );
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( sort_id == column_id ){
		sort_column = column;
	}

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_row_selected ), self );

	/* default is to sort by ascending operation date
	 */
	g_return_val_if_fail( sort_column && GTK_IS_TREE_VIEW_COLUMN( sort_column ), NULL );
	gtk_tree_view_column_set_sort_indicator( sort_column, TRUE );
	priv->sort_column = sort_column;
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( priv->tsort ), sort_id, sort_sens );

	g_signal_connect( G_OBJECT( tview ), "key-press-event", G_CALLBACK( on_key_pressed_event ), self );

	return( tview );
}

/*
 * sorting the treeview
 *
 * actually sort the content of the store, as the entry itself is only
 * updated when it is about to be saved -
 * the difference is small, but actually exists
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaViewEntries *self )
{
	static const gchar *thisfn = "ofa_view_entries_on_sort_model";
	ofaViewEntriesPrivate *priv;
	gint cmp, sort_column_id;
	ofxCounter numa, numb;
	GtkSortType sort_order;
	gchar *sdopea, *sdeffa, *srefa,
			*slabela, *sleda, *sacca, *sdeba, *screa, *scura, *sdcona, *sstaa;
	gchar *sdopeb, *sdeffb, *srefb,
			*slabelb, *sledb, *saccb, *sdebb, *screb, *scurb, *sdconb, *sstab;

	gtk_tree_model_get( tmodel, a,
			ENT_COL_DOPE,      &sdopea,
			ENT_COL_DEFF,      &sdeffa,
			ENT_COL_NUMBER,    &numa,
			ENT_COL_REF,       &srefa,
			ENT_COL_LABEL,     &slabela,
			ENT_COL_LEDGER,    &sleda,
			ENT_COL_ACCOUNT,   &sacca,
			ENT_COL_DEBIT,     &sdeba,
			ENT_COL_CREDIT,    &screa,
			ENT_COL_CURRENCY,  &scura,
			ENT_COL_DRECONCIL, &sdcona,
			ENT_COL_STATUS,    &sstaa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			ENT_COL_DOPE,      &sdopeb,
			ENT_COL_DEFF,      &sdeffb,
			ENT_COL_NUMBER,    &numb,
			ENT_COL_REF,       &srefb,
			ENT_COL_LABEL,     &slabelb,
			ENT_COL_LEDGER,    &sledb,
			ENT_COL_ACCOUNT,   &saccb,
			ENT_COL_DEBIT,     &sdebb,
			ENT_COL_CREDIT,    &screb,
			ENT_COL_CURRENCY,  &scurb,
			ENT_COL_DRECONCIL, &sdconb,
			ENT_COL_STATUS,    &sstab,
			-1 );

	cmp = 0;
	priv = self->priv;

	gtk_tree_sortable_get_sort_column_id(
			GTK_TREE_SORTABLE( priv->tsort ), &sort_column_id, &sort_order );

	switch( sort_column_id ){
		case ENT_COL_DOPE:
			cmp = my_date_compare_by_str( sdopea, sdopeb, MY_DATE_DMYY );
			break;
		case ENT_COL_DEFF:
			cmp = my_date_compare_by_str( sdeffa, sdeffb, MY_DATE_DMYY );
			break;
		case ENT_COL_NUMBER:
			cmp = ( numa < numb ? -1 : ( numa > numb ? 1 : 0 ));
			break;
		case ENT_COL_REF:
			cmp = cmp_strings( self, srefa, srefb );
			break;
		case ENT_COL_LABEL:
			cmp = cmp_strings( self, slabela, slabelb );
			break;
		case ENT_COL_LEDGER:
			cmp = cmp_strings( self, sleda, sledb );
			break;
		case ENT_COL_ACCOUNT:
			cmp = cmp_strings( self, sacca, saccb );
			break;
		case ENT_COL_DEBIT:
			cmp = cmp_amounts( self, sdeba, sdebb );
			break;
		case ENT_COL_CREDIT:
			cmp = cmp_amounts( self, screa, screb );
			break;
		case ENT_COL_CURRENCY:
			cmp = cmp_strings( self, scura, scurb );
			break;
		case ENT_COL_DRECONCIL:
			cmp = my_date_compare_by_str( sdcona, sdconb, MY_DATE_DMYY );
			break;
		case ENT_COL_STATUS:
			cmp = cmp_strings( self, sstaa, sstab );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, sort_column_id );
			break;
	}

	g_free( sdopea );
	g_free( sdopeb );
	g_free( sdeffa );
	g_free( sdeffb );
	g_free( srefa );
	g_free( srefb );
	g_free( slabela );
	g_free( slabelb );
	g_free( sleda );
	g_free( sledb );
	g_free( sacca );
	g_free( saccb );
	g_free( sdeba );
	g_free( screa );
	g_free( sdebb );
	g_free( screb );
	g_free( scura );
	g_free( scurb );
	g_free( sdcona );
	g_free( sdconb );
	g_free( sstaa );
	g_free( sstab );

	/* return -1 if a > b, so that the order indicator points to the smallest:
	 * ^: means from smallest to greatest (ascending order)
	 * v: means from greatest to smallest (descending order)
	 */
	return( -cmp );
}

static gint
cmp_strings( ofaViewEntries *self, const gchar *stra, const gchar *strb )
{
	if( !stra || !g_utf8_strlen( stra, -1 )){
		if( !strb || !g_utf8_strlen( strb, -1 )){
			/* the two strings are both empty */
			return( 0 );
		}
		/* a is empty while b is set */
		return( -1 );
	} else if( !strb || !g_utf8_strlen( strb, -1 )){
		/* a is set while b is empty */
		return( 1 );
	}

	/* both a and b are set */
	return( g_utf8_collate( stra, strb ));
}

static gint
cmp_amounts( ofaViewEntries *self, const gchar *stra, const gchar *strb )
{
	ofxAmount a, b;

	if( !stra || !g_utf8_strlen( stra, -1 )){
		if( !strb || !g_utf8_strlen( strb, -1 )){
			/* the two strings are both empty */
			return( 0 );
		}
		/* a is empty while b is set */
		return( -1 );
	} else if( !strb || !g_utf8_strlen( strb, -1 )){
		/* a is set while b is empty */
		return( 1 );
	}

	/* both a and b are set */
	a = my_double_set_from_str( stra );
	b = my_double_set_from_str( strb );

	return( a < b ? -1 : ( a > b ? 1 : 0 ));
}

/*
 * Gtk+ changes automatically the sort order
 * we reset yet the sort column id
 *
 * as a side effect of our inversion of indicators, clicking on a new
 * header makes the sort order descending as the default
 */
static void
on_header_clicked( GtkTreeViewColumn *column, ofaViewEntries *self )
{
	static const gchar *thisfn = "ofa_view_entries_on_header_clicked";
	ofaViewEntriesPrivate *priv;
	gint sort_column_id, new_column_id;
	GtkSortType sort_order;

	priv = self->priv;

	gtk_tree_view_column_set_sort_indicator( priv->sort_column, FALSE );
	gtk_tree_view_column_set_sort_indicator( column, TRUE );
	priv->sort_column = column;

	gtk_tree_sortable_get_sort_column_id( GTK_TREE_SORTABLE( priv->tsort ), &sort_column_id, &sort_order );

	g_debug( "%s: current sort_column_id=%u, sort_order=%s",
			thisfn, sort_column_id,
			sort_order == OFA_SORT_ASCENDING ? "OFA_SORT_ASCENDING":"OFA_SORT_DESCENDING" );

	new_column_id = gtk_tree_view_column_get_sort_column_id( column );

	gtk_tree_sortable_set_sort_column_id( GTK_TREE_SORTABLE( priv->tsort ), new_column_id, sort_order );

	g_debug( "%s: setting new_column_id=%u, new_sort_order=%s",
			thisfn, new_column_id,
			sort_order == OFA_SORT_ASCENDING ? "OFA_SORT_ASCENDING":"OFA_SORT_DESCENDING" );

	ofa_settings_set_uint( st_pref_sort_c, new_column_id );
	ofa_settings_set_uint( st_pref_sort_s, sort_order );
}

static void
setup_footer( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GtkWidget *widget;

	priv = self->priv;

	widget = my_utils_container_get_child_by_name( priv->top_box, "pt-comment" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->comment = GTK_LABEL( widget );
}

static void
setup_signaling_connect( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;

	priv = self->priv;

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
v_setup_buttons( ofaPage *page )
{
	return( NULL );
}

static void
v_init_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_view_entries_v_init_view";

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	set_visible_columns( OFA_VIEW_ENTRIES( page ));
}

static void
set_visible_columns( ofaViewEntries *self )
{
	GList *columns, *it;
	gboolean *priv_flag, is_visible;
	gint col_id;
	GList *id_list;

	id_list = NULL;
	columns = gtk_tree_view_get_columns( self->priv->entries_tview );
	for( it=columns ; it ; it=it->next ){
		col_id = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( it->data ), DATA_COLUMN_ID ));
		if( col_id >= 0 ){
			priv_flag = ( gboolean * ) g_object_get_data( G_OBJECT( it->data ), DATA_PRIV_VISIBLE );
			is_visible = !priv_flag || *priv_flag;
			gtk_tree_view_column_set_visible( GTK_TREE_VIEW_COLUMN( it->data ), is_visible );
			if( is_visible ){
				id_list = g_list_prepend( id_list, GINT_TO_POINTER( col_id ));
			}
		}
	}

	ofa_settings_set_uint_list( st_pref_columns, id_list );
	g_list_free( id_list );
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_VIEW_ENTRIES( page ), NULL );

	return( GTK_WIDGET( OFA_VIEW_ENTRIES( page )->priv->entries_tview ));
}

/*
 * toggle between display per ledger or display per account
 */
static void
on_gen_selection_toggled( GtkToggleButton *button, ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	gboolean is_active;

	priv = self->priv;

	is_active = gtk_toggle_button_get_active( button );
	/*g_debug( "on_gen_selection_toggled: is_active=%s", is_active ? "True":"False" );*/

	if( button == priv->ledger_btn ){
		/* make the selection frame sensitive */
		gtk_widget_set_sensitive( GTK_WIDGET( priv->ledger_box ), is_active );

		/* update the default visibility of the columns */
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->ledger_checkbox ), !is_active );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->account_checkbox ), is_active );

		/* and display the entries */
		if( is_active ){
			display_entries_from_ledger( self );
			ofa_settings_set_string( st_pref_selection, SEL_LEDGER );
		}

	} else {
		gtk_widget_set_sensitive( GTK_WIDGET( priv->account_entry ), is_active );
		gtk_widget_set_sensitive( GTK_WIDGET( priv->account_select ), is_active );
		gtk_widget_set_sensitive( GTK_WIDGET( priv->f1_label ), is_active );

		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->ledger_checkbox ), is_active );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->account_checkbox ), !is_active );

		if( is_active ){
			display_entries_from_account( self );
			ofa_settings_set_string( st_pref_selection, SEL_ACCOUNT );
		}
	}
}

/*
 * ofaLedgerCombo callback
 */
static void
on_ledger_changed( const gchar *mnemo, ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;

	priv = self->priv;

	g_free( priv->jou_mnemo );
	priv->jou_mnemo = g_strdup( mnemo );
	ofa_settings_set_string( st_pref_ledger, mnemo );

	display_entries_from_ledger( self );
}

static void
display_entries_from_ledger( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GList *entries;

	priv = self->priv;

	if( priv->jou_mnemo && layout_dates_is_valid( self )){
		entries = ofo_entry_get_dataset_by_ledger(
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
	gchar *str;

	priv = self->priv;

	g_free( priv->acc_number );
	priv->acc_number = g_strdup( gtk_entry_get_text( entry ));

	account = ofo_account_get_by_number( priv->dossier, priv->acc_number );

	if( account && !ofo_account_is_root( account )){
		str = g_strdup_printf( "%s: %s", _( "Account" ), ofo_account_get_label( account ));
		gtk_label_set_text( priv->f1_label, str );
		g_free( str );
		display_entries_from_account( self );
		ofa_settings_set_string( st_pref_account, priv->acc_number );

	} else {
		gtk_label_set_text( priv->f1_label, "" );
		ofa_settings_set_string( st_pref_account, "" );
	}

}

static void
on_account_select( GtkButton *button, ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	gchar *acc_number;

	priv = self->priv;

	acc_number = ofa_account_select_run(
							ofa_page_get_main_window( OFA_PAGE( self )),
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

	priv = self->priv;

	if( priv->acc_number && layout_dates_is_valid( self )){
		entries = ofo_entry_get_dataset_by_account(
							priv->dossier, priv->acc_number, &priv->d_from, &priv->d_to );
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
	return( on_date_focus_out( self, entry, &self->priv->d_from, st_pref_d_from ));
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_d_to_focus_out( GtkEntry *entry, GdkEvent *event, ofaViewEntries *self )
{
	return( on_date_focus_out( self, entry, &self->priv->d_to, st_pref_d_to ));
}

static gboolean
on_date_focus_out( ofaViewEntries *self, GtkEntry *entry, GDate *date, const gchar *pref )
{
	gchar *sdate;

	my_date_set_from_date( date, my_editable_date_get_date( GTK_EDITABLE( entry ), NULL ));
	refresh_display( self );

	if( my_date_is_valid( date )){
		sdate = my_date_to_str( date, MY_DATE_SQL );
		ofa_settings_set_string( pref, sdate );
		g_free( sdate );
	}

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

	priv = self->priv;

	if( priv->we_from ){
		str = gtk_entry_get_text( priv->we_from );
		if( str && g_utf8_strlen( str, -1 ) && !my_date_is_valid( &priv->d_from )){
			return( FALSE );
		}
	}

	if( priv->we_to ){
		str = gtk_entry_get_text( priv->we_to );
		if( str && g_utf8_strlen( str, -1 ) && !my_date_is_valid( &priv->d_to )){
			return( FALSE );
		}
	}

	if( my_date_is_valid( &priv->d_from ) &&
			my_date_is_valid( &priv->d_to ) &&
			my_date_compare( &priv->d_from, &priv->d_to ) > 0 ){
		return( FALSE );
	}

	return( TRUE );
}

static void
refresh_display( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;

	priv = self->priv;

	if( priv->tfilter ){
		gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( priv->tfilter ));
		compute_balances( self );
	}
}

/*
 * the hash is used to store the balance per currency
 */
static void
display_entries( ofaViewEntries *self, GList *entries )
{
	static const gchar *thisfn = "ofa_view_entries_display_entries";
	ofaViewEntriesPrivate *priv;
	GtkTreeIter iter;
	GList *iset;

	g_debug( "%s: self=%p, entries=%p", thisfn, ( void * ) self, ( void * ) entries );

	priv = self->priv;

	if( priv->tstore ){

		gtk_list_store_clear( GTK_LIST_STORE( priv->tstore ));

		for( iset=entries ; iset ; iset=iset->next ){
			gtk_list_store_insert( GTK_LIST_STORE( priv->tstore ), &iter, -1 );
			display_entry( self, OFO_ENTRY( iset->data ), &iter );
		}

		compute_balances( self );
	}
}

/*
 * the hash is used to store the balance per currency
 */
static void
reset_balances_hash( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;

	priv = self->priv;

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
	static const gchar *thisfn = "ofa_view_entries_compute_balances";
	ofaViewEntriesPrivate *priv;
	GtkWidget *box;
	GtkTreeIter iter;
	gchar *sdeb, *scre, *dev_code;
	sCurrency *pc;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = self->priv;

	reset_balances_hash( self );
	priv->balances_hash = g_hash_table_new_full(
					( GHashFunc ) g_str_hash,			/* hash_func */
					( GEqualFunc ) g_str_equal,			/* key_equal_func */
					( GDestroyNotify ) g_free, 			/* key_destroy_func */
					( GDestroyNotify ) g_free );		/* value_destroy_func */

	if( gtk_tree_model_get_iter_first( priv->tsort, &iter )){
		while( TRUE ){
			gtk_tree_model_get(
					priv->tsort,
					&iter,
					ENT_COL_DEBIT,    &sdeb,
					ENT_COL_CREDIT,   &scre,
					ENT_COL_CURRENCY, &dev_code,
					-1 );

			if( dev_code && g_utf8_strlen( dev_code, -1 )){
				pc = find_balance_by_currency( self, dev_code );
				pc->debits += my_double_set_from_str( sdeb );
				pc->credits += my_double_set_from_str( scre );
			}

			g_free( sdeb );
			g_free( scre );
			g_free( dev_code );

			if( !gtk_tree_model_iter_next( priv->tsort, &iter )){
				break;
			}
		}
	}

	box = reset_balances_widgets( self );
	g_hash_table_foreach( priv->balances_hash, ( GHFunc ) display_balance, self );
	set_balance_currency_label_position( self );
	gtk_widget_show_all( box );
}

/*
 * the hash is used to store the balance per currency
 */
static sCurrency *
find_balance_by_currency( ofaViewEntries *self, const gchar *dev_code )
{
	ofaViewEntriesPrivate *priv;
	sCurrency *pc;

	priv = self->priv;

	pc = ( sCurrency * ) g_hash_table_lookup( priv->balances_hash, dev_code );
	if( !pc ){
		pc = g_new0( sCurrency, 1 );
		pc->debits = 0;
		pc->credits = 0;
		g_hash_table_insert( priv->balances_hash, g_strdup( dev_code ), pc );
	}

	return( pc );
}

/*
 * iter is on the list store
 */
static void
display_entry( ofaViewEntries *self, ofoEntry *entry, GtkTreeIter *iter )
{
	ofaViewEntriesPrivate *priv;
	gchar *sdope, *sdeff, *sdeb, *scre, *srappro;
	const GDate *d;

	priv = self->priv;

	sdope = my_date_to_str( ofo_entry_get_dope( entry ), MY_DATE_DMYY );
	sdeff = my_date_to_str( ofo_entry_get_deffect( entry ), MY_DATE_DMYY );
	sdeb = my_double_to_str( ofo_entry_get_debit( entry ));
	scre = my_double_to_str( ofo_entry_get_credit( entry ));
	d = ofo_entry_get_concil_dval( entry );
	srappro = my_date_to_str( d, MY_DATE_DMYY );

	gtk_list_store_set(
				GTK_LIST_STORE( priv->tstore ),
				iter,
				ENT_COL_DOPE,         sdope,
				ENT_COL_DEFF,         sdeff,
				ENT_COL_NUMBER,       ofo_entry_get_number( entry ),
				ENT_COL_REF,          ofo_entry_get_ref( entry ),
				ENT_COL_LABEL,        ofo_entry_get_label( entry ),
				ENT_COL_LEDGER,       ofo_entry_get_ledger( entry ),
				ENT_COL_ACCOUNT,      ofo_entry_get_account( entry ),
				ENT_COL_DEBIT,        sdeb,
				ENT_COL_CREDIT,       scre,
				ENT_COL_CURRENCY,     ofo_entry_get_currency( entry ),
				ENT_COL_DRECONCIL,    srappro,
				ENT_COL_STATUS,       ofo_entry_get_abr_status( entry ),
				ENT_COL_OBJECT,       entry,
				ENT_COL_MSGERR,       "",
				ENT_COL_MSGWARN,      "",
				ENT_COL_DOPE_SET,     FALSE,
				ENT_COL_DEFF_SET,     FALSE,
				ENT_COL_CURRENCY_SET, FALSE,
				-1 );

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

	box = my_utils_container_get_child_by_name( self->priv->top_box, "pt-box" );
	g_return_val_if_fail( box && GTK_IS_BOX( box ), NULL );
	gtk_container_foreach( GTK_CONTAINER( box ), ( GtkCallback ) gtk_widget_destroy, NULL );

	return( box );
}

static void
display_balance( const gchar *dev_code, sCurrency *pc, ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GtkWidget *box, *row, *label;
	gchar *str;
	GdkRGBA color;

	if( pc->debits || pc->credits ){

		priv = self->priv;
		gdk_rgba_parse( &color, RGBA_BALANCE );

		box = my_utils_container_get_child_by_name( priv->top_box, "pt-box" );
		g_return_if_fail( box && GTK_IS_BOX( box ));

		row = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 4 );
		gtk_box_pack_start( GTK_BOX( box ), row, FALSE, FALSE, 0 );

		label = gtk_label_new( dev_code );
		gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
		gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
		gtk_label_set_text( GTK_LABEL( label ), dev_code );
		gtk_box_pack_end( GTK_BOX( row ), label, FALSE, FALSE, 4 );

		label = gtk_label_new( NULL );
		gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
		gtk_misc_set_alignment( GTK_MISC( label ), 1, 0.5 );
		gtk_label_set_width_chars( GTK_LABEL( label ), 12 );
		str = my_double_to_str( pc->credits );
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );
		gtk_box_pack_end( GTK_BOX( row ), label, FALSE, FALSE, 4 );

		label = gtk_label_new( NULL );
		gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
		gtk_misc_set_alignment( GTK_MISC( label ), 1, 0.5 );
		gtk_label_set_width_chars( GTK_LABEL( label ), 12 );
		str = my_double_to_str( pc->debits );
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );
		gtk_box_pack_end( GTK_BOX( row ), label, FALSE, FALSE, 4 );
	}
}

static void
set_balance_currency_label_position( ofaViewEntries *self )
{
	GtkWidget *box;

	box = my_utils_container_get_child_by_name( self->priv->top_box, "pt-box" );
	g_return_if_fail( box && GTK_IS_BOX( box ));
	gtk_container_foreach( GTK_CONTAINER( box ), ( GtkCallback ) set_balance_currency_label_margin, self );
}

static void
set_balance_currency_label_margin( GtkWidget *widget, ofaViewEntries *self )
{
	/* this returns 0 :( */
	/*width = gtk_tree_view_column_get_width( priv->currency_column );
	g_debug( "display_balance: width=%d", width );*/
	/* 30 is less of 1 char, 40 is more of 1/2 char */
	if( GTK_IS_BOX( widget )){
		gtk_widget_set_margin_right( widget, 36 + (self->priv->status_visible ? 48 : 0));
	}
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
	const GDate *deffect;

	priv = self->priv;
	visible = FALSE;

	if( priv->entries_tview ){
		gtk_tree_model_get( tmodel, iter, ENT_COL_OBJECT, &entry, -1 );

		if( entry && OFO_IS_ENTRY( entry )){
			g_object_unref( entry );

			status = get_row_status( self, tmodel, iter );

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

			deffect = get_row_deffect( self, tmodel, iter );
			visible &= !my_date_is_valid( &priv->d_from ) || my_date_compare_ex( &priv->d_from, deffect, FALSE ) <= 0;
			visible &= !my_date_is_valid( &priv->d_to ) || my_date_compare_ex( &priv->d_to, deffect, TRUE ) >= 0;
		}
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
	ofaEntryStatus status;
	GdkRGBA color;
	gint err_level;
	const gchar *color_str;

	g_return_if_fail( GTK_IS_CELL_RENDERER_TEXT( cell ));

	err_level = get_row_errlevel( self, tmodel, iter );
	status = get_row_status( self, tmodel, iter );

	g_object_set( G_OBJECT( cell ),
						"style-set",      FALSE,
						"background-set", FALSE,
						"foreground-set", FALSE,
						NULL );

	switch( status ){

		case ENT_STATUS_VALIDATED:
			gdk_rgba_parse( &color, RGBA_VALIDATED );
			g_object_set( G_OBJECT( cell ), "background-rgba", &color, NULL );
			break;

		case ENT_STATUS_DELETED:
			gdk_rgba_parse( &color, RGBA_DELETED );
			g_object_set( G_OBJECT( cell ), "foreground-rgba", &color, NULL );
			g_object_set( G_OBJECT( cell ), "style", PANGO_STYLE_ITALIC, NULL );
			break;

		case ENT_STATUS_ROUGH:
			switch( err_level ){
				case ENT_ERR_ERROR:
					color_str = RGBA_ERROR;
					break;
				case ENT_ERR_WARNING:
					color_str = RGBA_WARNING;
					break;
				default:
					color_str = RGBA_NORMAL;
					break;
			}
			gdk_rgba_parse( &color, color_str );
			g_object_set( G_OBJECT( cell ), "foreground-rgba", &color, NULL );
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

	if( !OFA_PAGE( self )->prot->dispose_has_run ){

		g_debug( "%s: self=%p, type=%lu, id=%s, begin=%p, end=%p",
				thisfn, ( void * ) self, type, id, ( void * ) begin, ( void * ) end );

		priv = self->priv;

		/* start by setting the from/to dates as these changes do not
		 * automatically trigger a display refresh */
		str = my_date_to_str( begin, MY_DATE_DMYY );
		gtk_entry_set_text( priv->we_from, str );
		g_free( str );

		str = my_date_to_str( end, MY_DATE_DMYY );
		gtk_entry_set_text( priv->we_to, str );
		g_free( str );

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
	ofaViewEntriesPrivate *priv;
	gboolean *priv_flag;

	priv = self->priv;
	priv_flag = ( gboolean * ) g_object_get_data( G_OBJECT( button ), DATA_PRIV_VISIBLE );
	*priv_flag = gtk_toggle_button_get_active( button );

	if( priv->entries_tview ){
		set_visible_columns( self );
		set_balance_currency_label_position( self );
		gtk_widget_queue_draw( GTK_WIDGET( priv->entries_tview ));
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
	const gchar *pref;

	priv_flag = ( gboolean * ) g_object_get_data( G_OBJECT( button ), DATA_PRIV_VISIBLE );
	*priv_flag = gtk_toggle_button_get_active( button );
	refresh_display( self );

	pref = g_object_get_data( G_OBJECT( button ), DATA_ROW_STATUS );
	ofa_settings_set_boolean( pref, *priv_flag );
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
 * reconciliation date and status are never editable
 */
static void
set_renderers_editable( ofaViewEntries *self, gboolean editable )
{
	ofaViewEntriesPrivate *priv;
	gint i;

	priv = self->priv;

	for( i=0 ; i<ENT_N_COLUMNS ; ++i ){
		if( priv->renderers[i] && i != ENT_COL_DRECONCIL && i != ENT_COL_STATUS ){
			g_object_set( G_OBJECT( priv->renderers[i] ), "editable", editable, NULL );
		}
	}
}

static void
on_cell_edited( GtkCellRendererText *cell, gchar *path_str, gchar *text, ofaViewEntries *self )
{
	static const gchar *thisfn = "ofa_view_entries_on_cell_edited";
	ofaViewEntriesPrivate *priv;
	gint column_id;
	GtkTreePath *path;
	GtkTreeIter sort_iter, filter_iter, iter;
	gchar *str;
	gdouble amount;

	g_debug( "%s: cell=%p, path=%s, text=%s, self=%p",
			thisfn, ( void * ) cell, path_str, text, ( void * ) self );

	priv = self->priv;

	if( priv->tsort ){
		path = gtk_tree_path_new_from_string( path_str );
		if( gtk_tree_model_get_iter( priv->tsort, &sort_iter, path )){

			gtk_tree_model_sort_convert_iter_to_child_iter(
					GTK_TREE_MODEL_SORT( priv->tsort ), &filter_iter, &sort_iter );
			gtk_tree_model_filter_convert_iter_to_child_iter(
					GTK_TREE_MODEL_FILTER( priv->tfilter), &iter, &filter_iter );

			column_id = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( cell ), DATA_COLUMN_ID ));
			set_data_set_indicator( self, column_id, &iter );

			/* reformat amounts before storing them */
			if( column_id == ENT_COL_DEBIT || column_id == ENT_COL_CREDIT ){
				amount = my_double_set_from_str( text );
				str = my_double_to_str( amount );
			} else {
				str = g_strdup( text );
			}
			gtk_list_store_set(
					GTK_LIST_STORE( priv->tstore ), &iter, column_id, str, -1 );
			g_free( str );

			check_row_for_valid( self, &iter );
			compute_balances( self );

			if( get_row_errlevel( self, priv->tstore, &iter ) == ENT_ERR_NONE ){
				save_entry( self, priv->tstore, &iter );
			}
		}
		gtk_tree_path_free( path );
	}
}

/*
 * a data has been edited by the user: set the corresponding flag (if
 * any) so that we do not try later to reset a default value
 */
static gint
get_data_set_indicator( ofaViewEntries *self, gint column_id )
{
	gint column_data;

	column_data = 0;

	switch( column_id ){
		case ENT_COL_DOPE:
			column_data = ENT_COL_DOPE_SET;
			break;
		case ENT_COL_DEFF:
			column_data = ENT_COL_DEFF_SET;
			break;
		case ENT_COL_CURRENCY:
			column_data = ENT_COL_CURRENCY_SET;
			break;
		default:
			break;
	}

	return( column_data );
}

/*
 * a data has been edited by the user: set the corresponding flag (if
 * any) so that we do not try later to reset a default value
 */
static void
set_data_set_indicator( ofaViewEntries *self, gint column_id, GtkTreeIter *iter )
{
	ofaViewEntriesPrivate *priv;
	gint column_data;

	priv = self->priv;
	column_data = get_data_set_indicator( self, column_id );

	if( column_data > 0 ){
		gtk_list_store_set( GTK_LIST_STORE( priv->tstore ), iter, column_data, TRUE, -1 );
	}
}

static void
on_row_selected( GtkTreeSelection *select, ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GtkTreeIter iter;
	gboolean is_editable, is_active;

	priv = self->priv;

	if( gtk_tree_selection_get_selected( select, NULL, &iter )){

		is_editable = ( get_row_status( self, priv->tsort, &iter ) == ENT_STATUS_ROUGH );
		is_editable &= ofo_dossier_is_entries_allowed( priv->dossier );

		gtk_widget_set_sensitive(  GTK_WIDGET( priv->edit_switch ), is_editable );
		g_object_get( G_OBJECT( priv->edit_switch ), "active", &is_active, NULL );
		set_renderers_editable( self, is_editable && is_active );

		/* reset the field or re-display an eventual error message */
		display_error_msg( self, priv->tsort, &iter );
	}
}

/*
 * @iter: a valid #GtkTreeIter on the underlying #GtkListStore
 *
 * Individual checks in general are only able to detect errors.
 */
static void
check_row_for_valid( ofaViewEntries *self, GtkTreeIter *iter )
{
	ofaViewEntriesPrivate *priv;
	gboolean v_dope, v_deffect, v_ledger, v_account, v_currency;
	gchar *prev_msg;

	priv = self->priv;
	reset_error_msg( self, iter );

	/* checks begin from right so that the last computed error message
	 * (for the leftest column) will be displayed first */
	check_row_for_valid_amounts( self, iter );
	check_row_for_valid_label( self, iter );

	/* check account before currency in order to be able to set a
	 * suitable default value */
	v_account = check_row_for_valid_account( self, iter );
	v_currency = check_row_for_valid_currency( self, iter );

	if( v_account && v_currency ){
		check_row_for_cross_currency( self, iter );
	}

	/* check ledger, deffect, dope in sequence in order to be able to
	 * safely reinit error message after having set default effect date */
	gtk_tree_model_get( priv->tstore, iter, ENT_COL_MSGERR, &prev_msg, -1 );
	v_ledger = check_row_for_valid_ledger( self, iter );
	v_deffect = check_row_for_valid_deffect( self, iter );
	v_dope = check_row_for_valid_dope( self, iter );

	if( v_dope && !v_deffect && v_ledger ){
		if( set_default_deffect( self, iter )){
			v_deffect = TRUE;
			set_error_msg( self, iter, prev_msg );
		}
	}
	g_free( prev_msg );

	if( v_dope && v_deffect && v_ledger ){
		check_row_for_cross_deffect( self, iter );
	}

	display_error_msg( self, priv->tstore, iter );
}

static gboolean
check_row_for_valid_dope( ofaViewEntries *self, GtkTreeIter *iter )
{
	ofaViewEntriesPrivate *priv;
	gchar *sdope, *msg;
	GDate date;
	gboolean is_valid;

	priv = self->priv;
	is_valid = FALSE;
	gtk_tree_model_get( priv->tstore, iter, ENT_COL_DOPE, &sdope, -1 );

	if( sdope && g_utf8_strlen( sdope, -1 )){
		my_date_set_from_str( &date, sdope, MY_DATE_DMYY );
		if( my_date_is_valid( &date )){
			is_valid = TRUE;

		} else {
			msg = g_strdup_printf( _( "Invalid operation date: %s" ), sdope );
			set_error_msg( self, iter, msg );
			g_free( msg );
		}
	} else {
		set_error_msg( self, iter, _( "Empty operation date" ));
	}

	g_free( sdope );

	return( is_valid );
}

/*
 * check for intrinsic validity of effect date
 */
static gboolean
check_row_for_valid_deffect( ofaViewEntries *self, GtkTreeIter *iter )
{
	ofaViewEntriesPrivate *priv;
	gchar *sdeffect, *msg;
	GDate deff;
	gboolean is_valid;
	gboolean dope_set;
	gint dope_data;

	priv = self->priv;
	is_valid = FALSE;

	gtk_tree_model_get( priv->tstore, iter, ENT_COL_DEFF, &sdeffect, -1 );

	if( sdeffect && g_utf8_strlen( sdeffect, -1 )){
		my_date_set_from_str( &deff, sdeffect, MY_DATE_DMYY );
		if( my_date_is_valid( &deff )){
			is_valid = TRUE;

		} else {
			msg = g_strdup_printf( _( "Invalid effect date: %s" ), sdeffect );
			set_error_msg( self, iter, msg );
			g_free( msg );
		}
	} else {
		set_error_msg( self, iter, _( "Empty effect date" ));
	}

	/* if effect date is valid, and operation date has not been set by
	 * the user, then set default operation date to effect date */
	if( is_valid ){
		dope_data = get_data_set_indicator( self, ENT_COL_DOPE );
		gtk_tree_model_get( priv->tstore, iter, dope_data, &dope_set, -1 );
		if( !dope_set ){
			gtk_list_store_set(
					GTK_LIST_STORE( priv->tstore ), iter, ENT_COL_DOPE, sdeffect, -1 );
		}
	}

	g_free( sdeffect );

	return( is_valid );
}

static gboolean
check_row_for_valid_ledger( ofaViewEntries *self, GtkTreeIter *iter )
{
	ofaViewEntriesPrivate *priv;
	gchar *str, *msg;
	gboolean is_valid;

	priv = self->priv;
	is_valid = FALSE;
	gtk_tree_model_get( priv->tstore, iter, ENT_COL_LEDGER, &str, -1 );

	if( str && g_utf8_strlen( str, -1 )){
		if( ofo_ledger_get_by_mnemo( priv->dossier, str )){
			is_valid = TRUE;

		} else {
			msg = g_strdup_printf( _( "Unknwown ledger: %s" ), str );
			set_error_msg( self, iter, msg );
			g_free( msg );
		}
	} else {
		set_error_msg( self, iter, _( "Empty ledger mnemonic" ));
	}
	g_free( str );

	return( is_valid );
}

static gboolean
check_row_for_valid_account( ofaViewEntries *self, GtkTreeIter *iter )
{
	ofaViewEntriesPrivate *priv;
	gchar *acc_number, *cur_code, *msg;
	gboolean is_valid;
	ofoAccount *account;
	gint cur_data;
	gboolean cur_set;

	priv = self->priv;
	is_valid = FALSE;
	gtk_tree_model_get( priv->tstore, iter,
			ENT_COL_ACCOUNT, &acc_number, ENT_COL_CURRENCY, &cur_code, -1 );

	if( acc_number && g_utf8_strlen( acc_number, -1 )){
		account = ofo_account_get_by_number( priv->dossier, acc_number );
		if( account ){
			if( !ofo_account_is_root( account )){
				is_valid = TRUE;

			} else {
				msg = g_strdup_printf( _( "Account %s is a root account" ), acc_number );
				set_error_msg( self, iter, msg );
				g_free( msg );
			}
		} else {
			msg = g_strdup_printf( _( "Unknwown account: %s" ), acc_number );
			set_error_msg( self, iter, msg );
			g_free( msg );
		}
	} else {
		set_error_msg( self, iter, _( "Empty account number" ));
	}
	g_free( acc_number );

	/* if account is valid, and currency code has not yet
	 * been set by the user, then setup the default currency */
	if( is_valid ){
		cur_data = get_data_set_indicator( self, ENT_COL_CURRENCY );
		gtk_tree_model_get( priv->tstore, iter, cur_data, &cur_set, -1 );
		if( !cur_set ){
			gtk_list_store_set( GTK_LIST_STORE( priv->tstore ), iter,
					ENT_COL_CURRENCY, ofo_account_get_currency( account ), -1 );
		}
	}

	return( is_valid );
}

static gboolean
check_row_for_valid_label( ofaViewEntries *self, GtkTreeIter *iter )
{
	ofaViewEntriesPrivate *priv;
	gchar *str;
	gboolean is_valid;

	priv = self->priv;
	is_valid = FALSE;
	gtk_tree_model_get( priv->tstore, iter, ENT_COL_LABEL, &str, -1 );

	if( str && g_utf8_strlen( str, -1 )){
		is_valid = TRUE;
	} else {
		set_error_msg( self, iter, _( "Empty label" ));
	}
	g_free( str );

	return( is_valid );
}

static gboolean
check_row_for_valid_currency( ofaViewEntries *self, GtkTreeIter *iter )
{
	ofaViewEntriesPrivate *priv;
	gchar *code, *msg;
	gboolean is_valid;

	priv = self->priv;
	is_valid = FALSE;
	gtk_tree_model_get( priv->tstore, iter, ENT_COL_CURRENCY, &code, -1 );

	if( code && g_utf8_strlen( code, -1 )){
		if( ofo_currency_get_by_code( priv->dossier, code )){
			is_valid = TRUE;

		} else {
			msg = g_strdup_printf( _( "Unknown currency: %s" ), code );
			set_error_msg( self, iter, msg );
			g_free( msg );
		}
	} else {
		set_error_msg( self, iter, _( "Empty currency" ));
	}
	g_free( code );

	return( is_valid );
}

static gboolean
check_row_for_valid_amounts( ofaViewEntries *self, GtkTreeIter *iter )
{
	ofaViewEntriesPrivate *priv;
	gchar *sdeb, *scre;
	gdouble debit, credit;
	gboolean is_valid;

	priv = self->priv;
	is_valid = FALSE;
	gtk_tree_model_get( priv->tstore, iter, ENT_COL_DEBIT, &sdeb, ENT_COL_CREDIT, &scre, -1 );

	if(( sdeb && g_utf8_strlen( sdeb, -1 )) || ( scre && g_utf8_strlen( scre, -1 ))){
		debit = my_double_set_from_str( sdeb );
		credit = my_double_set_from_str( scre );
		if(( debit && !credit ) || ( !debit && credit )){
			is_valid = TRUE;

		} else if( debit && credit ) {
			set_error_msg( self, iter, _( "Only one of debit and credit must be set" ));

		} else {
			set_error_msg( self, iter, _( "Debit and credit are both empty" ));
		}
	} else {
		set_error_msg( self, iter, _( "Debit and credit are both empty" ));
	}
	g_free( sdeb );
	g_free( scre );

	return( is_valid );
}

/*
 * effect date of any new entry must be:
 * - greater than the opening date of the exercice (should)
 * - greater than the last closing date of the ledger (if any)
 */
static void
check_row_for_cross_deffect( ofaViewEntries *self, GtkTreeIter *iter )
{
	ofaViewEntriesPrivate *priv;
	gchar *sdope, *sdeffect, *mnemo, *msg, *sdmin, *sdeff, *sdmax;
	GDate dope, deff, deff_min, dmax_past;
	ofoLedger *ledger;

	priv = self->priv;

	gtk_tree_model_get( priv->tstore, iter,
				ENT_COL_DOPE,   &sdope,
				ENT_COL_DEFF,   &sdeffect,
				ENT_COL_LEDGER, &mnemo,
				-1 );

	my_date_set_from_str( &dope, sdope, MY_DATE_DMYY );
	g_return_if_fail( my_date_is_valid( &dope ));

	my_date_set_from_str( &deff, sdeffect, MY_DATE_DMYY );
	g_return_if_fail( my_date_is_valid( &deff ));

	g_return_if_fail( mnemo && g_utf8_strlen( mnemo, -1 ));
	ledger = ofo_ledger_get_by_mnemo( priv->dossier, mnemo );
	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));

	my_date_set_from_date( &deff_min, get_min_deffect( self, &dope, ledger ));
	g_return_if_fail( my_date_is_valid( &deff_min ));

	/* if effect date is greater or equal than minimal effect date for
	 * the row, then it is valid and will normally apply to account and
	 * ledger */
	if( my_date_compare( &deff, &deff_min ) < 0 ){
		/* if max_past is defined and lesser than effect date, then
		 * this later is invalid */
		my_date_set_from_date( &dmax_past, get_max_past_deffect( self ));
		if( my_date_is_valid( &dmax_past )){

			if( my_date_compare( &deff, &dmax_past ) > 0 ){
				sdmin = my_date_to_str( &deff_min, MY_DATE_DMYY );
				sdeff = my_date_to_str( &deff, MY_DATE_DMYY );
				sdmax = my_date_to_str( &dmax_past, MY_DATE_DMYY );
				msg = g_strdup_printf(
						_( "Effect date %s is between the max past %s and the min effect date %s" ),
						sdeff, sdmax, sdmin );
				set_error_msg( self, iter, msg );
				g_free( msg );
				g_free( sdeff );
				g_free( sdmin );
				g_free( sdmax );

			} else {
				/* effect date if lesser than or equal to max past */
				sdmax = my_date_to_str( &dmax_past, MY_DATE_DMYY );
				sdeff = my_date_to_str( &deff, MY_DATE_DMYY );
				msg = g_strdup_printf(
						_( "Effect date %s lesser than or equal to max past %s (will not apply to account nor ledger)" ),
						sdeff, sdmax );
				set_warning_msg( self, iter, msg );
				g_free( msg );
				g_free( sdeff );
				g_free( sdmax );
			}
		} else {
			/* there is no max_past so minimal effect date applies */
			sdmin = my_date_to_str( &deff_min, MY_DATE_DMYY );
			sdeff = my_date_to_str( &deff, MY_DATE_DMYY );
			msg = g_strdup_printf(
					_( "Effect date %s lesser than mini effect date %s" ), sdeff, sdmin );
			set_error_msg( self, iter, msg );
			g_free( msg );
			g_free( sdeff );
			g_free( sdmin );
		}
	}

	g_free( sdope );
	g_free( sdeffect );
	g_free( mnemo );
}

/*
 * set a default effect date is operation date and ledger are valid
 * => effect date must not have been set by the user
 *
 * Returns: %TRUE if a default date has actually been set
 */
static gboolean
set_default_deffect( ofaViewEntries *self, GtkTreeIter *iter )
{
	ofaViewEntriesPrivate *priv;
	gboolean set;
	gboolean deff_set;
	gint deff_data;
	gchar *sdope, *mnemo, *sdeff;
	GDate dope, deff_min;
	ofoLedger *ledger;

	priv = self->priv;
	set = FALSE;

	deff_data = get_data_set_indicator( self, ENT_COL_DEFF );
	gtk_tree_model_get( priv->tstore, iter, deff_data, &deff_set, -1 );
	if( !deff_set ){
		gtk_tree_model_get( priv->tstore, iter,
					ENT_COL_DOPE,   &sdope,
					ENT_COL_LEDGER, &mnemo,
					-1 );

		my_date_set_from_str( &dope, sdope, MY_DATE_DMYY );
		g_return_val_if_fail( my_date_is_valid( &dope ), FALSE );

		g_return_val_if_fail( mnemo && g_utf8_strlen( mnemo, -1 ), FALSE );
		ledger = ofo_ledger_get_by_mnemo( priv->dossier, mnemo );
		g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );

		my_date_set_from_date( &deff_min, get_min_deffect( self, &dope, ledger ));
		sdeff = my_date_to_str( &deff_min, MY_DATE_DMYY );
		gtk_list_store_set( GTK_LIST_STORE( priv->tstore ), iter, ENT_COL_DEFF, sdeff, -1 );
		g_free( sdeff );

		g_free( sdope );
		g_free( mnemo );

		set = TRUE;
	}

	return( set );
}

/*
 * returns the minimal effect date, given :
 * - the last last closing date of the ledger,
 * - the possible opening date of the current exercice of the dossier.
 * - the operation date
 *
 * This is the minimal effect date for standard use cases, where entry
 * will be normally applied to account and ledger:
 * - for a new dossier where open exercice date has not been set, no
 *   entry is allowed here
 * - at soon as a ledger is closed, the closing date becomes a new
 *   minimal date
 * - when the exercice itself is closed, then the open exercice date
 *   for N+1 becomes the new minimal effect date
 * - and so on
 *
 * Due to the comparison with (supposed valid) operation date, this
 * function always returns a valid minimal date. Starting with this
 * minimal effect date, the entry will normally be applied to account
 * and ledger.
 */
static GDate *
get_min_deffect( ofaViewEntries *self, const GDate *dope, ofoLedger *ledger )
{
	ofaViewEntriesPrivate *priv;
	static GDate dmin, last_close;
	guint to_add;

	priv = self->priv;
	g_date_clear( &dmin, 1 );
	to_add = 0;

	/* minimal effect date from dossier point of view: may be undefined */
	if( my_date_is_valid( priv->dossier_opening )){
		my_date_set_from_date( &dmin, priv->dossier_opening );
	}
	if( my_date_is_valid( &dmin )){
		g_date_add_days( &dmin, to_add );	/* the minimal deffect from the dossier */
	}

	/* minimal effect date from ledger point of view: may be undefined */
	my_date_set_from_date( &last_close, ofo_ledger_get_last_close( ledger ));
	if( my_date_is_valid( &last_close )){
		g_date_add_days( &last_close, 1 );
		if( !my_date_is_valid( &dmin ) || my_date_compare( &dmin, &last_close ) < 0 ){
			my_date_set_from_date( &dmin, &last_close );
		}
	}

	/* minimal effect date from operation point of view */
	if( !my_date_is_valid( &dmin ) ||
			( my_date_is_valid( dope ) && my_date_compare( &dmin, dope ) < 0 )){
		my_date_set_from_date( &dmin, dope );
	}

	return( &dmin );
}

/*
 * Use case: when opening a new dossier, be able to import unsettled or
 * unreconciliated entries. They will not be applied to accounts nor
 * ledgers, but their recording in our books is allowed.
 *
 * This is so a maximal date: before this date, entry will be allowed
 * to be imported, while not applied.
 * After this date, entries enter in a normal behavior where they must
 * satisfy with ledger closing requirements.
 *
 * This date may be undefined: all entries are submitted to standard
 * behavior as long as the situation will stay the same (as it only
 * depends of the setup of the dossier).
 *
 * When defined, this date is strictly lesser than above minimal effect
 * date.
 */
static GDate *
get_max_past_deffect( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	static GDate dmax;
	guint to_substract;

	priv = self->priv;
	g_date_clear( &dmax, 1 );
	to_substract = 0;

	/* minimal effect date from dossier point of view */
	if( my_date_is_valid( priv->dossier_opening )){
		my_date_set_from_date( &dmax, priv->dossier_opening );
		to_substract = 1;
	}
	if( my_date_is_valid( &dmax )){
		g_date_subtract_days( &dmax, to_substract );	/* the maximal deffect for past entries */
	}

	return( &dmax );
}

/*
 * @tmodel: the GtkTreeModelSort
 * @iter: an iter on the sorted model
 */
static gboolean
check_row_for_cross_currency( ofaViewEntries *self, GtkTreeIter *iter )
{
	ofaViewEntriesPrivate *priv;
	gchar *number, *code, *msg;
	gboolean is_valid;
	const gchar *account_currency;
	ofoAccount *account;

	priv = self->priv;
	is_valid = FALSE;
	gtk_tree_model_get( priv->tstore, iter, ENT_COL_ACCOUNT, &number, ENT_COL_CURRENCY, &code, -1 );

	g_return_val_if_fail( number && g_utf8_strlen( number, -1 ), FALSE );
	account = ofo_account_get_by_number( priv->dossier, number );
	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !ofo_account_is_root( account ), FALSE );

	account_currency = ofo_account_get_currency( account );

	g_return_val_if_fail( code && g_utf8_strlen( code, -1 ), FALSE );

	if( !g_utf8_collate( account_currency, code )){
		is_valid = TRUE;

	} else {
		msg = g_strdup_printf( _( "Account expects %s currency while entry has %s" ), account_currency, code );
		set_error_msg( self, iter, msg );
		g_free( msg );
	}
	g_free( number );
	g_free( code );

	return( is_valid );
}

/*
 * @iter: a #GtkTreeIter valid on the underlying GtkListStore
 *
 * Reset error and warning messages
 */
static void
reset_error_msg( ofaViewEntries *self, GtkTreeIter *iter )
{
	ofaViewEntriesPrivate *priv;

	priv = self->priv;

	gtk_list_store_set(
			GTK_LIST_STORE( priv->tstore ), iter, ENT_COL_MSGERR, "", ENT_COL_MSGWARN, "", -1 );
}

/*
 * @iter: a #GtkTreeIter valid on the underlying GtkListStore
 *
 * Set an error message for the current row
 */
static void
set_error_msg( ofaViewEntries *self, GtkTreeIter *iter, const gchar *msg )
{
	ofaViewEntriesPrivate *priv;

	priv = self->priv;

	gtk_list_store_set(
			GTK_LIST_STORE( priv->tstore ), iter, ENT_COL_MSGERR, msg, -1 );
}

/*
 * @iter: a #GtkTreeIter valid on the underlying GtkListStore
 *
 * Set a warning message for the current row
 */
static void
set_warning_msg( ofaViewEntries *self, GtkTreeIter *iter, const gchar *msg )
{
	ofaViewEntriesPrivate *priv;

	priv = self->priv;

	gtk_list_store_set(
			GTK_LIST_STORE( priv->tstore ), iter, ENT_COL_MSGWARN, msg, -1 );
}

static void
display_error_msg( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	ofaViewEntriesPrivate *priv;
	gchar *msgerr, *msgwarn;
	GdkRGBA color;
	const gchar *color_str, *text;

	priv = self->priv;

	gtk_tree_model_get( tmodel, iter, ENT_COL_MSGERR, &msgerr, ENT_COL_MSGWARN, &msgwarn, -1 );

	if( msgerr && g_utf8_strlen( msgerr, -1 )){
		text = msgerr;
		color_str = RGBA_ERROR;
	} else if( msgwarn && g_utf8_strlen( msgwarn, -1 )){
		text = msgwarn;
		color_str = RGBA_WARNING;
	} else {
		text = "";
		color_str = RGBA_NORMAL;
	}

	gtk_label_set_text( priv->comment, text );
	gdk_rgba_parse( &color, color_str );
	gtk_widget_override_color( GTK_WIDGET( priv->comment ), GTK_STATE_FLAG_NORMAL, &color );

	g_free( msgerr );
	g_free( msgwarn );
}

/*
 * save a modified or new entry
 */
static gboolean
save_entry( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	ofaViewEntriesPrivate *priv;
	gchar *sdope, *sdeff, *ref, *label, *ledger, *account, *sdeb, *scre, *currency;
	GDate dope, deff;
	gint number;
	ofoEntry *entry;
	gboolean ok;

	priv = self->priv;
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
	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	/*g_debug( "save_entry: ref_count=%d", G_OBJECT( entry )->ref_count );*/
	g_object_unref( entry );

	my_date_set_from_str( &dope, sdope, MY_DATE_DMYY );
	g_return_val_if_fail( my_date_is_valid( &dope ), FALSE );
	ofo_entry_set_dope( entry, &dope );

	my_date_set_from_str( &deff, sdeff, MY_DATE_DMYY );
	g_return_val_if_fail( my_date_is_valid( &deff ), FALSE );
	ofo_entry_set_deffect( entry, &deff );

	ofo_entry_set_ref( entry, ref );
	ofo_entry_set_label( entry, label );
	ofo_entry_set_ledger( entry, ledger );
	ofo_entry_set_account( entry, account );
	ofo_entry_set_debit( entry, my_double_set_from_str( sdeb ));
	ofo_entry_set_credit( entry, my_double_set_from_str( scre ));
	ofo_entry_set_currency( entry, currency );

	if( ofo_entry_get_number( entry ) > 0 ){
		ok = ofo_entry_update( entry, priv->dossier );
	} else {
		ok = ofo_entry_insert( entry, priv->dossier );
	}

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

/*
 * Returns: a GtkTreeIter on GtkListStore treemodel
 */
static gboolean
find_entry_by_number( ofaViewEntries *self, gint number, GtkTreeIter *iter )
{
	ofaViewEntriesPrivate *priv;
	gint tnumber;

	priv = self->priv;

	if( gtk_tree_model_get_iter_first( priv->tstore, iter )){
		while( TRUE ){
			gtk_tree_model_get( priv->tstore, iter, ENT_COL_NUMBER, &tnumber, -1 );
			if( tnumber == number ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( priv->tstore, iter )){
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
			do_update_ledger_mnemo( self, prev_id, ofo_ledger_get_mnemo( OFO_LEDGER( object )));

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

	tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->priv->tfilter ));
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
do_update_ledger_mnemo( ofaViewEntries *self, const gchar *prev, const gchar *mnemo )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *str;
	gint cmp;

	tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->priv->tfilter ));
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

	tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->priv->tfilter ));
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
	GtkTreeIter iter;

	g_debug( "%s: dossier=%p, object=%p (%s), user_data=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	g_return_if_fail( object && OFO_IS_ENTRY( object ));

	if( find_entry_by_number( self, ofo_entry_get_number( OFO_ENTRY( object )), &iter )){
		gtk_list_store_set(
				GTK_LIST_STORE( self->priv->tstore ),
				&iter,
				ENT_COL_STATUS, ofo_entry_get_abr_status( OFO_ENTRY( object )),
				-1 );
	}
}

static gboolean
on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, ofaViewEntries *self )
{
	gboolean stop;

	stop = FALSE;

	if( gtk_switch_get_active( self->priv->edit_switch )){

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
get_row_status( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gchar *str;
	ofaEntryStatus status;

	gtk_tree_model_get( tmodel, iter, ENT_COL_STATUS, &str, -1 );
	status = ofo_entry_get_status_from_abr( str );
	g_free( str );

	return( status );
}

static GDate *
get_row_deffect( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	static GDate date;

	gtk_tree_model_get( tmodel, iter, ENT_COL_DEFF, &date, -1 );

	return( &date );
}

static gint
get_row_errlevel( ofaViewEntries *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gchar *msgerr, *msgwarn;
	gint err_level;

	gtk_tree_model_get( tmodel, iter, ENT_COL_MSGERR, &msgerr, ENT_COL_MSGWARN, &msgwarn, -1 );

	if( msgerr && g_utf8_strlen( msgerr, -1 )){
		err_level = ENT_ERR_ERROR;

	} else if( msgwarn && g_utf8_strlen( msgwarn, -1 )){
		err_level = ENT_ERR_WARNING;

	} else {
		err_level = ENT_ERR_NONE;
	}

	g_free( msgerr );
	g_free( msgwarn );

	return( err_level );
}

/*
 * insert a new entry at the current position
 */
static void
insert_new_row( ofaViewEntries *self )
{
	ofaViewEntriesPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeIter sort_iter, filter_iter, new_iter;
	ofoEntry *entry;
	ofoAccount *account_object;
	GtkTreePath *path;
	GtkTreeViewColumn *column;

	priv = self->priv;

	/* insert a new row at the end of the list
	 * there is no sens in inserting an empty row in a sorted list */
	gtk_list_store_insert( GTK_LIST_STORE( priv->tstore ), &new_iter, -1 );

	/* set default values that we are able to guess */
	entry = ofo_entry_new();
	ofo_entry_set_status( entry, ENT_STATUS_ROUGH );

	if( gtk_toggle_button_get_active( priv->ledger_btn )){
		if( priv->jou_mnemo && g_utf8_strlen( priv->jou_mnemo, -1 )){
			ofo_entry_set_ledger( entry, priv->jou_mnemo );
		}
	} else {
		if( priv->acc_number && g_utf8_strlen( priv->acc_number, -1 )){
			ofo_entry_set_account( entry, priv->acc_number );
			account_object = ofo_account_get_by_number( priv->dossier, priv->acc_number );
			ofo_entry_set_currency( entry, ofo_account_get_currency( account_object ));
		}
	}

	display_entry( self, entry, &new_iter );
	g_object_unref( entry );

	/* set the selection and the cursor on this new line */
	gtk_tree_model_filter_convert_child_iter_to_iter(
			GTK_TREE_MODEL_FILTER( priv->tfilter ), &filter_iter, &new_iter );
	gtk_tree_model_sort_convert_child_iter_to_iter(
			GTK_TREE_MODEL_SORT( priv->tsort ), &sort_iter, &filter_iter );

	select = gtk_tree_view_get_selection( priv->entries_tview );
	gtk_tree_selection_select_iter( select, &sort_iter );

	path = gtk_tree_model_get_path( priv->tsort, &sort_iter );
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
	GtkTreeIter sort_iter, filter_iter, iter;
	ofoEntry *entry;
	gchar *msg, *label;

	priv = self->priv;
	select = gtk_tree_view_get_selection( priv->entries_tview );

	if( gtk_tree_selection_get_selected( select, NULL, &sort_iter )){
		gtk_tree_model_get(
				priv->tsort,
				&sort_iter,
				ENT_COL_LABEL,    &label,
				ENT_COL_OBJECT,   &entry,
				-1 );

		if( get_row_status( self, priv->tsort, &sort_iter) == ENT_STATUS_ROUGH ){
			msg = g_strdup_printf(
					_( "Are you sure you want to remove the '%s' entry" ),
					label );
			if( delete_confirmed( self, msg )){
				gtk_tree_model_sort_convert_iter_to_child_iter(
						GTK_TREE_MODEL_SORT( priv->tsort ), &filter_iter, &sort_iter );
				gtk_tree_model_filter_convert_iter_to_child_iter(
						GTK_TREE_MODEL_FILTER( priv->tfilter ), &iter, &filter_iter );
				tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));
				gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
				if( entry ){
					ofo_entry_delete( entry, priv->dossier );
				}
				compute_balances( self );
			}
			g_free( msg );
		}
		g_free( label );
		if( entry && OFO_IS_ENTRY( entry )){
			/*g_debug( "delete_row: ref_count=%d", G_OBJECT( entry )->ref_count );*/
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
			_( "_Cancel" ), GTK_RESPONSE_CANCEL,
			_( "_Delete" ), GTK_RESPONSE_OK,
			NULL );

	response = gtk_dialog_run( GTK_DIALOG( dialog ));

	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
}
