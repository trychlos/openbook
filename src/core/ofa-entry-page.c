/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-date-renderer.h"
#include "my/my-double-renderer.h"
#include "my/my-igridlist.h"
#include "my/my-stamp.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-account-editable.h"
#include "api/ofa-amount.h"
#include "api/ofa-counter.h"
#include "api/ofa-date-filter-hv-bin.h"
#include "api/ofa-entry-page.h"
#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-idate-filter.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-igetter.h"
#include "api/ofa-isignaler.h"
#include "api/ofa-istore.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-operation-group.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-prefs.h"
#include "api/ofo-base.h"
#include "api/ofo-account.h"
#include "api/ofo-concil.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofs-concil-id.h"
#include "api/ofs-currency.h"

#include "core/ofa-account-select.h"
#include "core/ofa-entry-page-delconf.h"
#include "core/ofa-entry-properties.h"
#include "core/ofa-entry-store.h"
#include "core/ofa-entry-treeview.h"
#include "core/ofa-iconcil.h"
#include "core/ofa-ledger-combo.h"
#include "core/ofa-ledger-store.h"
#include "core/ofa-reconcil-group.h"
#include "core/ofa-settlement-group.h"

/* priv instance data
 */
typedef struct {

	/* runtime
	 */
	ofaIGetter          *getter;
	ofoDossier          *dossier;					/* dossier */
	gboolean             is_writable;
	const GDate         *dossier_opening;
	gboolean             initializing;
	gchar               *settings_prefix;
	GList               *store_handlers;

	/* GtkStack and extended filters
	 */
	GtkWidget           *stack;
	GtkWidget           *expander;
	GtkWidget           *ext_grid;
	GtkWidget           *ext_init_btn;
	GtkWidget           *ext_reset_btn;
	GtkWidget           *ext_apply_btn;
	guint                ext_rows;
	gint                 btn_size;

	/* frame 1: general selection
	 */
	GtkWidget           *ledger_btn;
	ofaLedgerCombo      *ledger_combo;
	GtkWidget           *ledger_parent;
	gchar               *jou_mnemo;

	GtkWidget           *account_btn;
	GtkWidget           *account_entry;
	gchar               *acc_number;
	gboolean             acc_valid;

	GtkWidget           *f1_label;

	/* frame 2: effect dates layout
	 */
	ofaDateFilterHVBin  *effect_filter;

	/* frame 3: entry status
	 */
	GtkWidget           *deleted_btn;
	GtkWidget           *rough_btn;
	GtkWidget           *validated_btn;

	/* frame 4: entry period
	 */
	GtkWidget           *past_btn;
	GtkWidget           *current_btn;
	GtkWidget           *future_btn;

	/* frame 5: edition switch
	 */
	GtkWidget           *edit_switch;

	/* entries list view
	 */
	ofaEntryTreeview    *tview;
	ofaEntryStore       *store;

	/* actions
	 */
	GSimpleAction       *new_action;
	GSimpleAction       *update_action;
	GSimpleAction       *delete_action;
	GSimpleAction       *vope_action;			/* display operation */
	GSimpleAction       *vconcil_action;		/* display reconciliation group */
	GSimpleAction       *vsettle_action;		/* display settlement group */
	GSimpleAction       *export_action;			/* export */

	/* footer
	 */
	GtkWidget           *comment;
	GtkWidget           *bottom_paned;
	GtkWidget           *bottom_balance;
	GtkWidget           *bottom_debit;
	GtkWidget           *bottom_credit;
	GtkWidget           *bottom_currency;
	GList               *balances;

	/* the current row
	 */
	gboolean             editable_row;
	GList               *sel_opes;
	ofxCounter           sel_concil_id;
	ofxCounter           sel_settle_id;
}
	ofaEntryPagePrivate;

/* the id of the column is set against each cell and each column
 * of the entry treeview
 */
#define DATA_COLUMN_ID                  "ofa-data-column-id"

/* a pointer to the entry status ENT_STATUS_xxx that this check button
 * toggles - so that we are able to have only one callback
 */
#define STATUS_BTN_DATA                 "ofa-data-priv-visible"

/* set against status toggle buttons in order to be able to set
 * the user prefs */
#define DATA_ROW_STATUS                 "ofa-data-row-status"

/* error levels, in ascending order
 */
enum {
	ENT_ERR_NONE = 0,
	ENT_ERR_WARNING,
	ENT_ERR_ERROR
};

/* the extended filter columns in the dynamic grid
 */
enum {
	XFIL_COL_OPERATOR = 0,
	XFIL_COL_FIELD,
	XFIL_COL_CONDITION,
	XFIL_COL_VALUE,
	XFIL_COL_STATUS,
	XFIL_N_COLUMNS
};

/* columns in the Operator (And/Or) combo box
 */
enum {
	OPE_COL_LABEL = 0,
	OPE_COL_OPERATOR,
	OPE_N_COLUMNS
};

/* operators
 */
enum {
	OPERATOR_NONE = 0,
	OPERATOR_AND,
	OPERATOR_OR,
};

typedef struct {
	const gchar *label;
	guint        operator;
}
	sOperator;

static const sOperator st_operators[] = {
		{ N_( "And" ), OPERATOR_AND },
		{ N_( "Or" ),  OPERATOR_OR },
		{ 0 }
};

/* columns in the Field combo box
 */
enum {
	FLD_COL_LABEL = 0,
	FLD_COL_ID,
	FLD_N_COLUMNS
};

/* columns in the Condition combo box
 */
enum {
	COND_COL_LABEL = 0,
	COND_COL_COND,
	COND_N_COLUMNS
};

/* conditions
 */
enum {
	COND_EQUAL = 1,
	COND_LT,
	COND_GT,
	COND_LE,
	COND_GE,
	COND_NE,
	COND_BEGINS,
	COND_NOTBEGINS,
	COND_CONTAINS,
	COND_NOTCONTAINS,
};

typedef struct {
	const gchar *label;
	guint        cond;
}
	sCondition;

static const sCondition st_conditions[] = {
		{ N_( "=" ),                   COND_EQUAL },
		{ N_( "<" ),                   COND_LT },
		{ N_( ">" ),                   COND_GT },
		{ N_( "<=" ),                  COND_LE },
		{ N_( ">=" ),                  COND_GE },
		{ N_( "<>" ),                  COND_NE },
		{ N_( "Begins with" ),         COND_BEGINS },
		{ N_( "Does not begin with" ), COND_NOTBEGINS },
		{ N_( "Contains" ),            COND_CONTAINS },
		{ N_( "Does not contain" ),    COND_NOTCONTAINS },
		{ 0 }
};

/* a structure wich holds an extended filter criterium
 */
typedef struct {
	guint  operator;
	gint   field;
	guint  condition;
	gchar *value;
}
	sExtend;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-entry-page.ui";
static const gchar *st_green_check_png  = "/org/trychlos/openbook/core/ofa-entry-page-green-check-68.png";
static const gchar *st_red_cross_png    = "/org/trychlos/openbook/core/ofa-entry-page-red-cross-68.png";
static const gchar *st_ui_id            = "EntryPageWindow";

#define SEL_LEDGER                      "Ledger"
#define SEL_ACCOUNT                     "Account"

static void       page_v_setup_page( ofaPage *page );
static void       setup_gen_selection( ofaEntryPage *self );
static void       setup_account_selection( ofaEntryPage *self );
static void       setup_ledger_selection( ofaEntryPage *self );
static void       setup_dates_filter( ofaEntryPage *self );
static void       setup_status_filter( ofaEntryPage *self );
static void       setup_period_filter( ofaEntryPage *self );
static void       setup_edit_switch( ofaEntryPage *self );
static void       setup_treeview( ofaEntryPage *self );
static gboolean   tview_is_visible_row( GtkTreeModel *tfilter, GtkTreeIter *iter, ofaEntryPage *self );
static gboolean   tview_apply_stdfilter( ofaEntryPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean   tview_apply_extfilter( ofaEntryPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean   tview_apply_extfilter_by_row( ofaEntryPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, guint i, gboolean visible );
static gboolean   tview_apply_extfilter_for_amount( ofaEntryPage *self, sExtend *criterium, const gchar *entry_value );
static gboolean   tview_apply_extfilter_for_counter( ofaEntryPage *self, sExtend *criterium, const gchar *entry_value );
static gboolean   tview_apply_extfilter_for_date( ofaEntryPage *self, sExtend *criterium, const gchar *entry_value );
static gboolean   tview_apply_extfilter_for_stamp( ofaEntryPage *self, sExtend *criterium, const gchar *entry_value );
static gboolean   tview_apply_extfilter_for_string( ofaEntryPage *self, sExtend *criterium, const gchar *entry_value );
static void       tview_on_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRenderer *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaEntryPage *self );
static void       tview_on_row_selected( ofaTVBin *bin, GtkTreeSelection *selection, ofaEntryPage *self );
static void       tview_on_row_activated( ofaTVBin *bin, GList *selected, ofaEntryPage *self );
static void       tview_on_row_insert( ofaTVBin *bin, ofaEntryPage *self );
static void       tview_on_row_delete( ofaTVBin *bin, GtkTreeSelection *selection, ofaEntryPage *self );
static void       setup_ext_filter( ofaEntryPage *self );
static void       extfilter_on_expander_toggled( GtkExpander *expander, GParamSpec *param_spec, ofaEntryPage *self );
static void       extfilter_set_expander_toggled( ofaEntryPage *self, GtkExpander *expander );
static void       extfilter_on_stack_switched( GtkStack *stack, GParamSpec *param_spec, ofaEntryPage *self );
static void       extfilter_set_stack_switched( ofaEntryPage *self, GtkStack *stack );
static void       igridlist_iface_init( myIGridlistInterface *iface );
static guint      igridlist_get_interface_version( void );
static void       igridlist_setup_row( const myIGridlist *instance, GtkGrid *grid, guint row, void *criterium );
static void       setup_row_widgets( ofaEntryPage *self, GtkGrid *grid, guint row );
static void       setup_row_values( ofaEntryPage *self, GtkGrid *grid, guint row, sExtend *crit );
static void       extfilter_on_operator_changed( GtkComboBox *combo, ofaEntryPage *self );
static void       extfilter_on_field_changed( GtkComboBox *combo, ofaEntryPage *self );
static void       extfilter_on_condition_changed( GtkComboBox *combo, ofaEntryPage *self );
static void       extfilter_on_value_changed( GtkEntry *entry, ofaEntryPage *self );
static void       extfilter_on_row_changed( myIGridlist *instance, GtkGrid *grid, void *empty );
static void       extfilter_check_rows( ofaEntryPage *self, GtkGrid *grid );
static gboolean   extfilter_get_row_valid( ofaEntryPage *self, GtkGrid *grid, guint row );
static void       extfilter_set_valid_image( ofaEntryPage *self, GtkGrid *grid, guint row, gboolean valid );
static void       extfilter_on_init_from_clicked( GtkButton *button, ofaEntryPage *self );
static void       extfilter_on_init_status( ofaEntryPage *self, GtkWidget *btn, ofeEntryStatus status, gboolean *first );
static void       extfilter_on_init_period( ofaEntryPage *self, GtkWidget *btn, ofeEntryPeriod period, gboolean *first );
static void       extfilter_on_reset_clicked( GtkButton *button, ofaEntryPage *self );
static void       extfilter_on_apply_clicked( GtkButton *button, ofaEntryPage *self );
static sExtend   *extfilter_get_criterium( ofaEntryPage *self, guint row );
static gboolean   extfilter_get_is_valid_criterium( ofaEntryPage *self, sExtend *criterium, guint row );
static void       extfilter_free_criterium( ofaEntryPage *self, sExtend *criterium );
static void       setup_footer( ofaEntryPage *self );
static void       setup_actions( ofaEntryPage *self );
static GtkWidget *page_v_get_top_focusable_widget( const ofaPage *page );
static void       gen_selection_on_toggled( GtkToggleButton *button, ofaEntryPage *self );
static void       ledger_on_changed( ofaLedgerCombo *combo, const gchar *mnemo, ofaEntryPage *self );
static gboolean   ledger_display_from( ofaEntryPage *self );
static void       account_on_changed( GtkEntry *entry, ofaEntryPage *self );
static gboolean   account_on_entry_key_pressed( GtkWidget *entry, GdkEventKey *event, ofaEntryPage *self );
static void       account_do_select( ofaEntryPage *self );
static gboolean   account_display_from( ofaEntryPage *self );
static void       effect_filter_on_changed( ofaIDateFilter *filter, gint who, gboolean empty, const GDate *date, ofaEntryPage *self );
static void       status_on_toggled( GtkToggleButton *button, ofaEntryPage *self );
static void       period_on_toggled( GtkToggleButton *button, ofaEntryPage *self );
static void       edit_on_switched( GtkSwitch *switch_btn, GParamSpec *pspec, ofaEntryPage *self );
static void       edit_set_cells_editable( ofaEntryPage *self, GtkTreeSelection *selection, gboolean editable );
static void       edit_on_cell_edited( GtkCellRendererText *cell, gchar *path, gchar *text, ofaEntryPage *self );
static gint       edit_get_column_set_id( ofaEntryPage *self, gint column_id );
static void       edit_set_column_set_indicator( ofaEntryPage *self, gint column_id, GtkTreeIter *store_iter );
static void       refresh_display( ofaEntryPage *self );
static void       balances_compute( ofaEntryPage *self );
static void       balance_display( ofsCurrency *pc, ofaEntryPage *self );
static void       check_row_for_valid( ofaEntryPage *self, GtkTreeIter *iter );
static gboolean   check_row_for_valid_dope( ofaEntryPage *self, GtkTreeIter *iter );
static gboolean   check_row_for_valid_deffect( ofaEntryPage *self, GtkTreeIter *iter );
static gboolean   check_row_for_valid_label( ofaEntryPage *self, GtkTreeIter *iter );
static gboolean   check_row_for_valid_ledger( ofaEntryPage *self, GtkTreeIter *iter );
static gboolean   check_row_for_valid_account( ofaEntryPage *self, GtkTreeIter *iter );
static gboolean   check_row_for_valid_currency( ofaEntryPage *self, GtkTreeIter *iter );
static gboolean   check_row_for_valid_amounts( ofaEntryPage *self, GtkTreeIter *iter );
static void       check_row_for_cross_deffect( ofaEntryPage *self, GtkTreeIter *iter );
static gboolean   set_default_deffect( ofaEntryPage *self, GtkTreeIter *iter );
static gboolean   check_row_for_cross_currency( ofaEntryPage *self, GtkTreeIter *iter );
static void       reset_error_msg( ofaEntryPage *self, GtkTreeIter *iter );
static void       set_error_msg( ofaEntryPage *self, GtkTreeIter *iter, const gchar *str );
/*static void       set_warning_msg( ofaEntryPage *self, GtkTreeIter *iter, const gchar *str );*/
static gboolean   save_entry( ofaEntryPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void       remediate_entry_account( ofaEntryPage *self, ofoEntry *entry, const gchar *prev_account, ofxAmount prev_debit, ofxAmount prev_credit );
static void       remediate_entry_ledger( ofaEntryPage *self, ofoEntry *entry, const gchar *prev_ledger, ofxAmount prev_debit, ofxAmount prev_credit );
static void       action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaEntryPage *self );
static void       insert_new_row( ofaEntryPage *self );
static void       action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaEntryPage *self );
static void       do_update( ofaEntryPage *self, ofoEntry *entry );
static void       action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaEntryPage *self );
static void       delete_row( ofaEntryPage *self, GtkTreeSelection *selection );
static void       action_on_vope_activated( GSimpleAction *action, GVariant *empty, ofaEntryPage *self );
static void       action_on_vconcil_activated( GSimpleAction *action, GVariant *empty, ofaEntryPage *self );
static void       action_on_vsettle_activated( GSimpleAction *action, GVariant *empty, ofaEntryPage *self );
static void       action_on_export_activated( GSimpleAction *action, GVariant *empty, ofaEntryPage *self );
static gboolean   row_is_editable( ofaEntryPage *self, GtkTreeSelection *selection );
static ofxCounter row_get_operation_id( ofaEntryPage *self, GtkTreeSelection *selection );
static ofxCounter row_get_concil_id( ofaEntryPage *self, GtkTreeSelection *selection );
static ofxCounter row_get_settlement_id( ofaEntryPage *self, GtkTreeSelection *selection );
static void       row_display_message( ofaEntryPage *self, GtkTreeSelection *selection );
static gint       row_get_errlevel( ofaEntryPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void       read_settings( ofaEntryPage *self );
static void       read_settings_selection( ofaEntryPage *self, myISettings *settings );
static void       read_settings_period_status( ofaEntryPage *self, myISettings *settings );
static void       write_settings( ofaEntryPage *self );
static void       write_settings_selection( ofaEntryPage *self, myISettings *settings );
static void       write_settings_period_status( ofaEntryPage *self, myISettings *settings );
static void       store_on_changed( ofaEntryStore *store, ofaEntryPage *self );
static void       store_on_need_refilter( ofaIStore *store, ofaEntryPage *self );

G_DEFINE_TYPE_EXTENDED( ofaEntryPage, ofa_entry_page, OFA_TYPE_PAGE, 0,
		G_ADD_PRIVATE( ofaEntryPage )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IGRIDLIST, igridlist_iface_init ))

static void
entry_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_entry_page_finalize";
	ofaEntryPagePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( OFA_IS_ENTRY_PAGE( instance ));

	/* free data members here */
	priv = ofa_entry_page_get_instance_private( OFA_ENTRY_PAGE( instance ));

	g_free( priv->settings_prefix );
	g_free( priv->jou_mnemo );
	g_free( priv->acc_number );
	g_list_free( priv->sel_opes );
	ofs_currency_list_free( &priv->balances );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_entry_page_parent_class )->finalize( instance );
}

static void
entry_page_dispose( GObject *instance )
{
	ofaEntryPagePrivate *priv;
	GList *it;

	g_return_if_fail( OFA_IS_ENTRY_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		write_settings( OFA_ENTRY_PAGE( instance ));

		priv = ofa_entry_page_get_instance_private( OFA_ENTRY_PAGE( instance ));

		/* disconnect ofaEntryStore signal handlers */
		for( it=priv->store_handlers ; it ; it=it->next ){
			g_signal_handler_disconnect( priv->store, ( gulong ) it->data );
		}
		g_list_free( priv->store_handlers );

		/* unref object members here */
		g_clear_object( &priv->new_action );
		g_clear_object( &priv->update_action );
		g_clear_object( &priv->delete_action );
		g_clear_object( &priv->vope_action );
		g_clear_object( &priv->vconcil_action );
		g_clear_object( &priv->vsettle_action );
		g_clear_object( &priv->export_action );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_entry_page_parent_class )->dispose( instance );
}

static void
ofa_entry_page_init( ofaEntryPage *self )
{
	static const gchar *thisfn = "ofa_entry_page_init";
	ofaEntryPagePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( OFA_IS_ENTRY_PAGE( self ));

	priv = ofa_entry_page_get_instance_private( self );

	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));

	/* prevent the entries dataset to be loaded during initialization */
	priv->initializing = TRUE;

	priv->ext_rows = 0;
}

static void
ofa_entry_page_class_init( ofaEntryPageClass *klass )
{
	static const gchar *thisfn = "ofa_entry_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = entry_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = entry_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_page = page_v_setup_page;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = page_v_get_top_focusable_widget;
}

static void
page_v_setup_page( ofaPage *page )
{
	static const gchar *thisfn = "ofa_entry_page_v_setup_page";
	ofaEntryPagePrivate *priv;
	ofaHub *hub;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_entry_page_get_instance_private( OFA_ENTRY_PAGE( page ));

	priv->getter = ofa_page_get_getter( OFA_PAGE( page ));

	hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv->dossier = ofa_hub_get_dossier( hub );
	priv->dossier_opening = ofo_dossier_get_exe_begin( priv->dossier );
	priv->is_writable = ofa_hub_is_writable_dossier( hub );

	my_utils_container_attach_from_resource( GTK_CONTAINER( page ), st_resource_ui, st_ui_id, "px-top" );

	setup_gen_selection( OFA_ENTRY_PAGE( page ));
	setup_ledger_selection( OFA_ENTRY_PAGE( page ));
	setup_account_selection( OFA_ENTRY_PAGE( page ));
	setup_dates_filter( OFA_ENTRY_PAGE( page ));
	setup_status_filter( OFA_ENTRY_PAGE( page ));
	setup_period_filter( OFA_ENTRY_PAGE( page ));
	setup_edit_switch( OFA_ENTRY_PAGE( page ));
	setup_treeview( OFA_ENTRY_PAGE( page ));
	setup_ext_filter( OFA_ENTRY_PAGE( page ));
	setup_footer( OFA_ENTRY_PAGE( page ));
	setup_actions( OFA_ENTRY_PAGE( page ));

	read_settings( OFA_ENTRY_PAGE( page ));
	extfilter_set_stack_switched( OFA_ENTRY_PAGE( page ), GTK_STACK( priv->stack ));

	/* allow the entry dataset to be loaded */
	g_debug( "%s: end of initialization phase", thisfn );
	priv->initializing = FALSE;

	/* trigger the general selection toggle */
	if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->account_btn ))){
		gen_selection_on_toggled(  GTK_TOGGLE_BUTTON( priv->account_btn ), OFA_ENTRY_PAGE( page ));
	} else {
		gen_selection_on_toggled(  GTK_TOGGLE_BUTTON( priv->ledger_btn ), OFA_ENTRY_PAGE( page ));
	}
}

/*
 * toggle between ledger and account as major filter
 */
static void
setup_gen_selection( ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	GtkWidget *btn;

	priv = ofa_entry_page_get_instance_private( self );

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "f1-btn-ledger" );
	g_return_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ));
	g_signal_connect( btn, "toggled", G_CALLBACK( gen_selection_on_toggled ), self );
	priv->ledger_btn = btn;

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "f1-btn-account" );
	g_return_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ));
	g_signal_connect( btn, "toggled", G_CALLBACK( gen_selection_on_toggled ), self );
	priv->account_btn =  btn;
}

static void
setup_account_selection( ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	GtkWidget *widget;

	priv = ofa_entry_page_get_instance_private( self );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "f1-account-entry" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( widget, "changed", G_CALLBACK( account_on_changed ), self );
	priv->account_entry = widget;
	ofa_account_editable_init( GTK_EDITABLE( widget ), priv->getter, ACCOUNT_ALLOW_DETAIL );

	g_signal_connect( widget, "key-press-event", G_CALLBACK( account_on_entry_key_pressed ), self );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "f1-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->f1_label = widget;
}

static void
setup_ledger_selection( ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	static const gint st_ledger_cols[] = { LEDGER_COL_LABEL, -1 };

	priv = ofa_entry_page_get_instance_private( self );

	priv->ledger_parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "f1-ledger-parent" );
	g_return_if_fail( priv->ledger_parent && GTK_IS_CONTAINER( priv->ledger_parent ));
	priv->ledger_combo = ofa_ledger_combo_new();
	gtk_container_add( GTK_CONTAINER( priv->ledger_parent ), GTK_WIDGET( priv->ledger_combo ));
	ofa_ledger_combo_set_columns( priv->ledger_combo, st_ledger_cols );
	ofa_ledger_combo_set_getter( priv->ledger_combo, priv->getter );

	g_signal_connect( priv->ledger_combo, "ofa-changed", G_CALLBACK( ledger_on_changed ), self );
}

static void
setup_dates_filter( ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	GtkWidget *container;
	gchar *settings_key;

	priv = ofa_entry_page_get_instance_private( self );

	priv->effect_filter = ofa_date_filter_hv_bin_new( priv->getter );
	settings_key = g_strdup_printf( "%s-effect", priv->settings_prefix );
	ofa_idate_filter_set_settings_key( OFA_IDATE_FILTER( priv->effect_filter ), settings_key );
	g_free( settings_key );
	g_signal_connect( priv->effect_filter, "ofa-focus-out", G_CALLBACK( effect_filter_on_changed ), self );

	container = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "effect-date-filter" );
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));
	gtk_container_add( GTK_CONTAINER( container ), GTK_WIDGET( priv->effect_filter ));
}

static void
setup_status_filter( ofaEntryPage *self )
{
	static const gchar *thisfn = "ofa_entry_page_setup_status_filter";
	ofaEntryPagePrivate *priv;
	GtkWidget *button;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = ofa_entry_page_get_instance_private( self );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "f3-deleted" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( button, "toggled", G_CALLBACK( status_on_toggled), self );
	g_object_set_data( G_OBJECT( button ), STATUS_BTN_DATA, GINT_TO_POINTER( ENT_STATUS_DELETED ));
	priv->deleted_btn = button;

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "f3-rough" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( button, "toggled", G_CALLBACK( status_on_toggled), self );
	g_object_set_data( G_OBJECT( button ), STATUS_BTN_DATA, GINT_TO_POINTER( ENT_STATUS_ROUGH ));
	priv->rough_btn = button;

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "f3-validated" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( button, "toggled", G_CALLBACK( status_on_toggled), self );
	g_object_set_data( G_OBJECT( button ), STATUS_BTN_DATA, GINT_TO_POINTER( ENT_STATUS_VALIDATED ));
	priv->validated_btn = button;
}

static void
setup_period_filter( ofaEntryPage *self )
{
	static const gchar *thisfn = "ofa_entry_page_setup_period_filter";
	ofaEntryPagePrivate *priv;
	GtkWidget *button;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = ofa_entry_page_get_instance_private( self );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "f4-past" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( button, "toggled", G_CALLBACK( period_on_toggled), self );
	g_object_set_data( G_OBJECT( button ), STATUS_BTN_DATA, GINT_TO_POINTER( ENT_PERIOD_PAST ));
	priv->past_btn = button;

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "f4-current" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( button, "toggled", G_CALLBACK( period_on_toggled), self );
	g_object_set_data( G_OBJECT( button ), STATUS_BTN_DATA, GINT_TO_POINTER( ENT_PERIOD_CURRENT ));
	priv->current_btn = button;

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "f4-future" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( button, "toggled", G_CALLBACK( period_on_toggled), self );
	g_object_set_data( G_OBJECT( button ), STATUS_BTN_DATA, GINT_TO_POINTER( ENT_PERIOD_FUTURE ));
	priv->future_btn = button;
}

static void
setup_edit_switch( ofaEntryPage *self )
{
	static const gchar *thisfn = "ofa_entry_page_setup_edit_switch";
	ofaEntryPagePrivate *priv;
	GtkWidget *widget;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = ofa_entry_page_get_instance_private( self );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "f5-edition-switch" );
	g_return_if_fail( widget && GTK_IS_SWITCH( widget ));
	g_signal_connect( widget, "notify::active", G_CALLBACK( edit_on_switched ), self );
	priv->edit_switch = widget;

	g_object_set( G_OBJECT( widget ), "active", FALSE, NULL );
}

static void
setup_treeview( ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	GtkWidget *parent;
	gulong handler;

	priv = ofa_entry_page_get_instance_private( self );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "entries-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->tview = ofa_entry_treeview_new( priv->getter, priv->settings_prefix );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->tview ));
	ofa_tvbin_set_selection_mode( OFA_TVBIN( priv->tview ), GTK_SELECTION_BROWSE );
	ofa_entry_treeview_set_filter_func( priv->tview, ( GtkTreeModelFilterVisibleFunc ) tview_is_visible_row, self );
	ofa_tvbin_set_cell_data_func( OFA_TVBIN( priv->tview ), ( GtkTreeCellDataFunc ) tview_on_cell_data_func, self );
	ofa_tvbin_set_cell_edited_func( OFA_TVBIN( priv->tview ), G_CALLBACK( edit_on_cell_edited ), self );

	/* we keep the ofaTVBin message as we need model and iter */
	g_signal_connect( priv->tview, "ofa-selchanged", G_CALLBACK( tview_on_row_selected ), self );
	g_signal_connect( priv->tview, "ofa-entactivated", G_CALLBACK( tview_on_row_activated ), self );
	g_signal_connect( priv->tview, "ofa-insert", G_CALLBACK( tview_on_row_insert ), self );
	g_signal_connect( priv->tview, "ofa-seldelete", G_CALLBACK( tview_on_row_delete ), self );

	priv->store = ofa_entry_store_new( priv->getter );
	ofa_tvbin_set_store( OFA_TVBIN( priv->tview ), GTK_TREE_MODEL( priv->store ));
	handler = g_signal_connect( priv->store, "ofa-istore-need-refilter", G_CALLBACK( store_on_need_refilter ), self );
	priv->store_handlers = g_list_prepend( priv->store_handlers, ( gpointer ) handler );
	g_object_unref( priv->store );

	handler = g_signal_connect( priv->store, "ofa-changed", G_CALLBACK( store_on_changed ), self );
	priv->store_handlers = g_list_prepend( priv->store_handlers, ( gpointer ) handler );
}

/*
 * The GtkTreeModelFilter behavior depends of the current switched stack
 */
static gboolean
tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	const gchar *name;
	gboolean visible;

	priv = ofa_entry_page_get_instance_private( self );

	if( priv->stack ){
		name = gtk_stack_get_visible_child_name( GTK_STACK( priv->stack ));

		if( !my_collate( name, "standard" )){
			visible = tview_apply_stdfilter( self, tmodel, iter );
		} else {
			visible = tview_apply_extfilter( self, tmodel, iter );
		}

	} else {
		visible = tview_apply_stdfilter( self, tmodel, iter );
	}

	return( visible );
}

/*
 * Standard filter:
 * a row is visible if it is consistant with the selected modes:
 * - general type selection
 * - status of the entry
 * - effect date layout
 */
static gboolean
tview_apply_stdfilter( ofaEntryPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	ofaEntryPagePrivate *priv;
	gboolean visible, ok;
	gchar *account, *ledger, *sdate;
	ofoEntry *entry;
	ofeEntryStatus status;
	GDate deffect;
	const GDate *effect_filter;
	ofeEntryPeriod period;

	priv = ofa_entry_page_get_instance_private( self );

	visible = TRUE;

	gtk_tree_model_get( tmodel, iter,
			ENTRY_COL_LEDGER,    &ledger,
			ENTRY_COL_ACCOUNT,   &account,
			ENTRY_COL_DEFFECT,   &sdate,
			ENTRY_COL_STATUS_I,  &status,
			ENTRY_COL_IPERIOD_I, &period,
			ENTRY_COL_OBJECT,    &entry,
			-1 );

	if( entry && OFO_IS_ENTRY( entry )){
		g_object_unref( entry );

		if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->ledger_btn ))){
			if( !ledger || my_collate( priv->jou_mnemo, ledger )){
				visible = FALSE;
			}
		} else if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->account_btn ))){
			if( !account || my_collate( priv->acc_number, account )){
				visible = FALSE;
			}
		}

		if( visible ){
			switch( status ){
				case ENT_STATUS_DELETED:
					visible &= gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->deleted_btn ));
					break;
				case ENT_STATUS_ROUGH:
					visible &= gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->rough_btn ));
					break;
				case ENT_STATUS_VALIDATED:
					visible &= gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->validated_btn ));
					break;
			}
		}

		if( visible ){
			switch( period ){
				case ENT_PERIOD_PAST:
					visible &= gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->past_btn ));
					break;
				case ENT_PERIOD_CURRENT:
					visible &= gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->current_btn ));
					break;
				case ENT_PERIOD_FUTURE:
					visible &= gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->future_btn ));
					break;
			}
		}

		if( visible ){
			my_date_set_from_str( &deffect, sdate, ofa_prefs_date_get_display_format( priv->getter ));
			effect_filter = ofa_idate_filter_get_date(
					OFA_IDATE_FILTER( priv->effect_filter ), IDATE_FILTER_FROM );
			ok = !my_date_is_valid( effect_filter ) ||
					!my_date_is_valid( &deffect ) ||
					my_date_compare( effect_filter, &deffect ) <= 0;
			visible &= ok;
		}
		if( visible ){
			effect_filter = ofa_idate_filter_get_date(
					OFA_IDATE_FILTER( priv->effect_filter ), IDATE_FILTER_TO );
			ok = !my_date_is_valid( effect_filter ) ||
					!my_date_is_valid( &deffect ) ||
					my_date_compare( effect_filter, &deffect ) >= 0;
			visible &= ok;
		}
	}

	g_free( sdate );
	g_free( account );
	g_free( ledger );

	return( visible );
}

/*
 * Extended filter
 * A row is visible if it complies with full operator+filed+condition+value datas
 */
static gboolean
tview_apply_extfilter( ofaEntryPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	ofaEntryPagePrivate *priv;
	gboolean visible;
	guint rows_count, i;

	priv = ofa_entry_page_get_instance_private( self );

	visible = TRUE;
	rows_count = my_igridlist_get_details_count( MY_IGRIDLIST( self ), GTK_GRID( priv->ext_grid ));

	if( rows_count == 0 ){
		visible = FALSE;

	} else {
		for( i=0 ; i<rows_count ; ++i ){
			visible &= tview_apply_extfilter_by_row( self, tmodel, iter, i, visible );
		}
	}

	//g_debug( "tview_apply_extfilter: visible=%s", visible ? "True":"False" );

	return( visible );
}

/*
 * @i: a counter from zero.
 * @prev_criteria: whether the previous criteria were successful.
 *
 * Evaluates extended criteria for one criterium row.
 * Invalid rows are just ignored.
 */
static gboolean
tview_apply_extfilter_by_row( ofaEntryPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, guint i, gboolean prev_criteria )
{
	static const gchar *thisfn = "ofa_entry_page_tview_apply_extfilter_by_row";
	ofaEntryPagePrivate *priv;
	sExtend *criterium;
	guint row;
	gboolean valid, crit_ok, this_crit;
	gchar *entry_value;
	ofeBoxType type;
	GtkTreeViewColumn *column;

	priv = ofa_entry_page_get_instance_private( self );

	row = i; 				/* this myIGridlist does not have header */
	criterium = extfilter_get_criterium( self, row );
	valid = extfilter_get_is_valid_criterium( self, criterium, row );
	crit_ok = FALSE;
	this_crit = crit_ok;

	if( valid ){
		gtk_tree_model_get( tmodel, iter, criterium->field, &entry_value, -1 );
		/* entry_value may be null between gtk_tree_store_insert_row() and
		 * gtk_tree_store_set_row_by_iter() */
		if( entry_value ){
			column = ofa_itvcolumnable_get_column( OFA_ITVCOLUMNABLE( priv->tview ), criterium->field );
			type = ofa_itvcolumnable_get_column_type( OFA_ITVCOLUMNABLE( priv->tview ), column );
			switch( type ){
				case OFA_TYPE_AMOUNT:
					crit_ok = tview_apply_extfilter_for_amount( self, criterium, entry_value );
					break;
				case OFA_TYPE_COUNTER:
				case OFA_TYPE_INTEGER:
					crit_ok = tview_apply_extfilter_for_counter( self, criterium, entry_value );
					break;
				case OFA_TYPE_DATE:
					crit_ok = tview_apply_extfilter_for_date( self, criterium, entry_value );
					break;
				case OFA_TYPE_STRING:
					crit_ok = tview_apply_extfilter_for_string( self, criterium, entry_value );
					break;
				case OFA_TYPE_TIMESTAMP:
					crit_ok = tview_apply_extfilter_for_stamp( self, criterium, entry_value );
					break;
				default:
					break;
			}
			g_free( entry_value );
		}
	}

	if( valid ){
		if( row == 0 ){
			this_crit = crit_ok;

		} else {
			switch( criterium->operator ){
				case OPERATOR_AND:
					this_crit = prev_criteria && crit_ok;
					break;
				case OPERATOR_OR:
					this_crit = prev_criteria || crit_ok;
					break;
				default:
					// should not happen
					g_warning( "%s: row=%u, unknown operator=%u", thisfn, row, criterium->operator );
					this_crit = FALSE;
					break;
			}
		}
	}

	extfilter_free_criterium( self, criterium );

	if( 0 ){
		g_debug( "%s: prev_criteria=%s, ok=%s, this_crit=%s",
				thisfn,
				prev_criteria ? "True":"False", crit_ok ? "True":"False", this_crit ? "True":"False" );
	}

	return( this_crit );
}

static gboolean
tview_apply_extfilter_for_amount( ofaEntryPage *self, sExtend *criterium, const gchar *entry_value )
{
	static const gchar *thisfn = "ofa_entry_page_tview_apply_extfilter_for_amount";
	ofaEntryPagePrivate *priv;
	gboolean ok;
	ofxAmount crit_amount, entry_amount;

	priv = ofa_entry_page_get_instance_private( self );

	crit_amount = ofa_amount_from_str( criterium->value, priv->getter );
	entry_amount = ofa_amount_from_str( entry_value, priv->getter );

	switch( criterium->condition ){
		case COND_EQUAL:
			ok = ( entry_amount == crit_amount );
			break;
		case COND_LE:
			ok = ( entry_amount <= crit_amount );
			break;
		case COND_LT:
			ok = ( entry_amount < crit_amount );
			break;
		case COND_GE:
			ok = ( entry_amount >= crit_amount );
			break;
		case COND_GT:
			ok = ( entry_amount > crit_amount );
			break;
		case COND_NE:
			ok = ( entry_amount != crit_amount );
			break;

		// does not apply
		case COND_BEGINS:
		case COND_NOTBEGINS:
		case COND_CONTAINS:
		case COND_NOTCONTAINS:
			ok = FALSE;
			break;

		default:
			g_warning( "%s: condition=%u is unknown (value=%s)", thisfn, criterium->condition, criterium->value );
			ok = FALSE;
			break;
	}

	return( ok );
}

static gboolean
tview_apply_extfilter_for_counter( ofaEntryPage *self, sExtend *criterium, const gchar *entry_value )
{
	static const gchar *thisfn = "ofa_entry_page_tview_apply_extfilter_for_counter";
	gboolean ok;
	ofxCounter crit_counter, entry_counter;

	crit_counter = atol( criterium->value );
	entry_counter = atol( entry_value );

	switch( criterium->condition ){
		case COND_EQUAL:
			ok = ( entry_counter == crit_counter );
			break;
		case COND_LE:
			ok = ( entry_counter <= crit_counter );
			break;
		case COND_LT:
			ok = ( entry_counter < crit_counter );
			break;
		case COND_GE:
			ok = ( entry_counter >= crit_counter );
			break;
		case COND_GT:
			ok = ( entry_counter > crit_counter );
			break;
		case COND_NE:
			ok = ( entry_counter != crit_counter );
			break;

		// does not apply
		case COND_BEGINS:
		case COND_NOTBEGINS:
		case COND_CONTAINS:
		case COND_NOTCONTAINS:
			ok = FALSE;
			break;

		default:
			g_warning( "%s: condition=%u is unknown (value=%s)", thisfn, criterium->condition, criterium->value );
			ok = FALSE;
			break;
	}

	//g_debug( "%s: criterium=%lu, entry=%lu, ok=%s", thisfn, crit_counter, entry_counter, ok ? "True":"False" );

	return( ok );
}

static gboolean
tview_apply_extfilter_for_date( ofaEntryPage *self, sExtend *criterium, const gchar *entry_value )
{
	static const gchar *thisfn = "ofa_entry_page_tview_apply_extfilter_for_date";
	ofaEntryPagePrivate *priv;
	gboolean ok;
	GDate crit_date, entry_date;

	priv = ofa_entry_page_get_instance_private( self );

	my_date_set_from_str( &crit_date, criterium->value, ofa_prefs_date_get_display_format( priv->getter ));
	my_date_set_from_str( &entry_date, entry_value, ofa_prefs_date_get_display_format( priv->getter ));

	switch( criterium->condition ){
		case COND_EQUAL:
			ok = ( my_date_compare_ex( &entry_date, &crit_date, FALSE ) == 0 );
			break;
		case COND_LE:
			ok = ( my_date_compare_ex( &entry_date, &crit_date, FALSE ) <= 0 );
			break;
		case COND_LT:
			ok = ( my_date_compare_ex( &entry_date, &crit_date, FALSE ) < 0 );
			break;
		case COND_GE:
			ok = ( my_date_compare_ex( &entry_date, &crit_date, FALSE ) >= 0 );
			break;
		case COND_GT:
			ok = ( my_date_compare_ex( &entry_date, &crit_date, FALSE ) > 0 );
			break;
		case COND_NE:
			ok = ( my_date_compare_ex( &entry_date, &crit_date, FALSE ) != 0 );
			break;

		// does not apply
		case COND_BEGINS:
		case COND_NOTBEGINS:
		case COND_CONTAINS:
		case COND_NOTCONTAINS:
			ok = FALSE;
			break;

		default:
			g_warning( "%s: condition=%u is unknown (value=%s)", thisfn, criterium->condition, criterium->value );
			ok = FALSE;
			break;
	}

	return( ok );
}

static gboolean
tview_apply_extfilter_for_stamp( ofaEntryPage *self, sExtend *criterium, const gchar *entry_value )
{
	static const gchar *thisfn = "ofa_entry_page_tview_apply_extfilter_for_stamp";
	gboolean ok;
	myStampVal *crit_stamp, *entry_stamp;

	crit_stamp = my_stamp_new_from_str( criterium->value, MY_STAMP_DMYYHM );
	entry_stamp = my_stamp_new_from_str( entry_value, MY_STAMP_DMYYHM );

	switch( criterium->condition ){
		case COND_EQUAL:
			ok = ( my_stamp_compare( entry_stamp, crit_stamp ) == 0 );
			break;
		case COND_LE:
			ok = ( my_stamp_compare( entry_stamp, crit_stamp ) <= 0 );
			break;
		case COND_LT:
			ok = ( my_stamp_compare( entry_stamp, crit_stamp ) < 0 );
			break;
		case COND_GE:
			ok = ( my_stamp_compare( entry_stamp, crit_stamp ) >= 0 );
			break;
		case COND_GT:
			ok = ( my_stamp_compare( entry_stamp, crit_stamp ) > 0 );
			break;
		case COND_NE:
			ok = ( my_stamp_compare( entry_stamp, crit_stamp ) != 0 );
			break;

		// does not apply
		case COND_BEGINS:
		case COND_NOTBEGINS:
		case COND_CONTAINS:
		case COND_NOTCONTAINS:
			ok = FALSE;
			break;

		default:
			g_warning( "%s: condition=%u is unknown (value=%s)", thisfn, criterium->condition, criterium->value );
			ok = FALSE;
			break;
	}

	my_stamp_free( entry_stamp );
	my_stamp_free( crit_stamp );

	return( ok );
}

/*
 * All conditions are considered case insensitive
 */
static gboolean
tview_apply_extfilter_for_string( ofaEntryPage *self, sExtend *criterium, const gchar *entry_value )
{
	static const gchar *thisfn = "ofa_entry_page_tview_apply_extfilter_for_string";
	gboolean ok;
	gchar *crit_str, *entry_str;

	crit_str = my_casefold( criterium->value );
	entry_str = my_casefold( entry_value );

	switch( criterium->condition ){
		case COND_EQUAL:
			ok = ( my_collate( entry_str, crit_str ) == 0 );
			break;
		case COND_LE:
			ok = ( my_collate( entry_str, crit_str ) <= 0 );
			break;
		case COND_LT:
			ok = ( my_collate( entry_str, crit_str ) < 0 );
			break;
		case COND_GE:
			ok = ( my_collate( entry_str, crit_str ) >= 0 );
			break;
		case COND_GT:
			ok = ( my_collate( entry_str, crit_str ) > 0 );
			break;
		case COND_NE:
			ok = ( my_collate( entry_str, crit_str ) != 0 );
			break;
		case COND_BEGINS:
			ok = g_str_has_prefix( entry_str, crit_str );
			break;
		case COND_NOTBEGINS:
			ok = !g_str_has_prefix( entry_str, crit_str );
			break;
		case COND_CONTAINS:
			ok = ( g_strrstr( entry_str, crit_str ) != NULL );
			break;
		case COND_NOTCONTAINS:
			ok = ( g_strrstr( entry_str, crit_str ) == NULL );
			break;

		default:
			g_warning( "%s: condition=%u is unknown (value=%s)", thisfn, criterium->condition, criterium->value );
			ok = FALSE;
			break;
	}

	g_free( entry_str );
	g_free( crit_str );

	return( ok );
}

static void
tview_on_cell_data_func( GtkTreeViewColumn *tcolumn,
							GtkCellRenderer *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
							ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;

	g_return_if_fail( tcolumn && GTK_IS_TREE_VIEW_COLUMN( tcolumn ));
	g_return_if_fail( cell && GTK_IS_CELL_RENDERER( cell ));
	g_return_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ));
	g_return_if_fail( self && OFA_IS_ENTRY_PAGE( self ));

	priv = ofa_entry_page_get_instance_private( self );

	g_return_if_fail( !OFA_PAGE( self )->prot->dispose_has_run );

	ofa_entry_treeview_cell_data_render( priv->tview, tcolumn, cell, tmodel, iter );

	/* is the cell editable ? */
	if( GTK_IS_CELL_RENDERER_TEXT( cell )){
		g_object_set( G_OBJECT( cell ), "editable-set", TRUE, "editable", priv->editable_row, NULL );
	}
}

/*
 * selection mode is GTK_SELECTION_BROWSE
 *
 * The 'Edition' toggle switch is enabled when the row is editable
 * (depending of the entry status and the dossier)
 */
static void
tview_on_row_selected( ofaTVBin *bin, GtkTreeSelection *selection, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	gboolean editable;
	ofxCounter id;

	priv = ofa_entry_page_get_instance_private( self );

	if( !priv->initializing ){

		editable = row_is_editable( self, selection );
		gtk_widget_set_sensitive( priv->edit_switch, editable );
		edit_set_cells_editable( self, selection, editable );

		id = row_get_operation_id( self, selection );
		g_simple_action_set_enabled( priv->vope_action, id > 0 );
		g_list_free( priv->sel_opes );
		priv->sel_opes = id > 0 ? g_list_append( NULL, ( gpointer ) id ) : NULL;

		id = row_get_concil_id( self, selection );
		g_simple_action_set_enabled( priv->vconcil_action, id > 0 );
		priv->sel_concil_id = id;

		id = row_get_settlement_id( self, selection );
		g_simple_action_set_enabled( priv->vsettle_action, id > 0 );
		priv->sel_settle_id = id;

		row_display_message( self, selection );
	}
}

static void
tview_on_row_activated( ofaTVBin *bin, GList *selected, ofaEntryPage *self )
{
	ofoEntry *entry;

	entry = ( ofoEntry * ) selected->data;
	do_update( self, entry );
}

static void
tview_on_row_insert( ofaTVBin *bin, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;

	priv = ofa_entry_page_get_instance_private( self );

	if( gtk_switch_get_active( GTK_SWITCH( priv->edit_switch ))){
		insert_new_row( self );
	}
}

/*
 * The 'Suppr'. key has been hit on the entry treeview
 */
static void
tview_on_row_delete( ofaTVBin *bin, GtkTreeSelection *selection, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;

	priv = ofa_entry_page_get_instance_private( self );

	if( priv->editable_row ){
		delete_row( self, selection );
	}
}

/*
 * Starting with 0.68, an extended filter is made available to the #ofaEntryPage.
 * A #GtkStackSwitcher let the user switch between standard and extended filters.
 * A #GtkExpander, associated with two #GtkRevealers, shows or hides the criteria.
 *
 * The status of the expander is not saved in user settings. Instead of that,
 * it is initially opened so that criteria are visible.
 */
static void
setup_ext_filter( ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	GtkWidget *expander, *stack, *grid, *btn;

	priv = ofa_entry_page_get_instance_private( self );

	expander = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "expander1" );
	g_return_if_fail( expander && GTK_IS_EXPANDER( expander ));
	priv->expander = expander;
	g_signal_connect( expander, "notify::expanded", G_CALLBACK( extfilter_on_expander_toggled ), self );

	stack = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "stack1" );
	g_return_if_fail( stack && GTK_IS_STACK( stack ));
	priv->stack = stack;
	g_signal_connect( stack, "notify::visible-child-name", G_CALLBACK( extfilter_on_stack_switched ), self );

	grid = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ext-grid" );
	g_return_if_fail( grid && GTK_IS_GRID( grid ));
	priv->ext_grid = grid;

	my_igridlist_init(
			MY_IGRIDLIST( self ), GTK_GRID( priv->ext_grid ),
			FALSE, TRUE, XFIL_N_COLUMNS );
	my_igridlist_set_has_row_number( MY_IGRIDLIST( self ), GTK_GRID( priv->ext_grid ), FALSE );
	g_signal_connect( self, "my-row-changed", G_CALLBACK( extfilter_on_row_changed ), NULL );

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ext-init-from-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	priv->ext_init_btn = btn;
	g_signal_connect( btn, "clicked", G_CALLBACK( extfilter_on_init_from_clicked ), self );

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ext-reset-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	priv->ext_reset_btn = btn;
	g_signal_connect( btn, "clicked", G_CALLBACK( extfilter_on_reset_clicked ), self );

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ext-apply-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	priv->ext_apply_btn = btn;
	g_signal_connect( btn, "clicked", G_CALLBACK( extfilter_on_apply_clicked ), self );

	gtk_expander_set_expanded( GTK_EXPANDER( expander ), TRUE );
	gtk_stack_set_visible_child_name( GTK_STACK( stack ), "standard" );
}

static void
extfilter_on_expander_toggled( GtkExpander *expander, GParamSpec *param_spec, ofaEntryPage *self )
{
	extfilter_set_expander_toggled( self, expander );
}

static void
extfilter_set_expander_toggled( ofaEntryPage *self, GtkExpander *expander )
{
	ofaEntryPagePrivate *priv;
	gboolean expanded;
	GtkWidget *revealer, *label;

	priv = ofa_entry_page_get_instance_private( self );

	expanded = gtk_expander_get_expanded( expander );
	revealer = gtk_stack_get_visible_child( GTK_STACK( priv->stack ));
	g_return_if_fail( revealer && GTK_IS_REVEALER( revealer ));
	gtk_revealer_set_reveal_child( GTK_REVEALER( revealer ), expanded );

	label = gtk_expander_get_label_widget( expander );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), expanded ? _( "Hide criteria" ) : _( "Show criteria" ));
}

static void
extfilter_on_stack_switched( GtkStack *stack, GParamSpec *param_spec, ofaEntryPage *self )
{
	extfilter_set_stack_switched( self, stack );
}

static void
extfilter_set_stack_switched( ofaEntryPage *self, GtkStack *stack )
{
	ofaEntryPagePrivate *priv;
	const gchar *name;

	priv = ofa_entry_page_get_instance_private( self );

	name = gtk_stack_get_visible_child_name( stack );

	if( !my_collate( name, "standard" )){
		gtk_widget_set_sensitive( priv->ext_init_btn, FALSE );
		gtk_widget_set_sensitive( priv->ext_reset_btn, FALSE );

	} else {
		extfilter_on_row_changed( MY_IGRIDLIST( self ), GTK_GRID( priv->ext_grid ), self );
	}

	gtk_widget_set_sensitive( priv->ext_apply_btn, FALSE );
	extfilter_set_expander_toggled( self, GTK_EXPANDER( priv->expander ));

	refresh_display( self );
}

/*
 * myIGridlist interface management
 */
static void
igridlist_iface_init( myIGridlistInterface *iface )
{
	static const gchar *thisfn = "ofa_entry_page_igridlist_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = igridlist_get_interface_version;
	iface->setup_row = igridlist_setup_row;
}

static guint
igridlist_get_interface_version( void )
{
	return( 1 );
}

static void
igridlist_setup_row( const myIGridlist *instance, GtkGrid *grid, guint row, void *criterium )
{
	ofaEntryPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_ENTRY_PAGE( instance ));

	priv = ofa_entry_page_get_instance_private( OFA_ENTRY_PAGE( instance ));
	g_return_if_fail( grid == GTK_GRID( priv->ext_grid ));

	setup_row_widgets( OFA_ENTRY_PAGE( instance ), grid, row );

	if( criterium ){
		setup_row_values( OFA_ENTRY_PAGE( instance ), grid, row, ( sExtend * ) criterium );
	}
}

/*
 * Operator (And/Or) - not on the first row
 * + field
 * + condition
 * + value
 */
static void
setup_row_widgets( ofaEntryPage *self, GtkGrid *grid, guint row )
{
	ofaEntryPagePrivate *priv;
	GtkWidget *combo, *entry, *treeview;
	GtkCellRenderer *cell;
	GtkListStore *store;
	guint i;
	GtkTreeIter iter;
	GList *children, *it;
	GtkTreeViewColumn *column;
	const gchar *title;
	GtkRequisition rq;

	priv = ofa_entry_page_get_instance_private( self );

	/* operator combo box - not on first row */
	combo = gtk_combo_box_new();

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", OPE_COL_LABEL );

	store = gtk_list_store_new( OPE_N_COLUMNS, G_TYPE_STRING, G_TYPE_UINT );
	gtk_combo_box_set_model( GTK_COMBO_BOX( combo ), GTK_TREE_MODEL( store ));

	for( i=0 ; st_operators[i].label ; ++i ){
		gtk_list_store_insert_with_values(
				store, &iter, -1,
				OPE_COL_LABEL,    gettext( st_operators[i].label ),
				OPE_COL_OPERATOR, st_operators[i].operator,
				-1 );
	}
	my_igridlist_set_widget( MY_IGRIDLIST( self ), grid, combo, 1+XFIL_COL_OPERATOR, row, 1, 1 );
	gtk_widget_set_sensitive( combo, row > 0 );
	g_signal_connect( combo, "changed", G_CALLBACK( extfilter_on_operator_changed ), self );

	/* field combo box
	 * does not consider columns which do not have a title */
	combo = gtk_combo_box_new();

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", FLD_COL_LABEL );

	store = gtk_list_store_new( FLD_N_COLUMNS, G_TYPE_STRING, G_TYPE_UINT );
	gtk_combo_box_set_model( GTK_COMBO_BOX( combo ), GTK_TREE_MODEL( store ));

	treeview = ofa_tvbin_get_tree_view( OFA_TVBIN( priv->tview ));
	children = gtk_tree_view_get_columns( GTK_TREE_VIEW( treeview ));
	for( it=children ; it ; it=it->next ){
		g_return_if_fail( it->data && GTK_IS_TREE_VIEW_COLUMN( it->data ));
		column = GTK_TREE_VIEW_COLUMN( it->data );
		title = gtk_tree_view_column_get_title( column );
		if( my_strlen( title )){
			gtk_list_store_insert_with_values(
					store, &iter, -1,
					FLD_COL_LABEL, ofa_itvcolumnable_get_menu_label( OFA_ITVCOLUMNABLE( priv->tview ), column ),
					FLD_COL_ID,    ofa_itvcolumnable_get_column_id( OFA_ITVCOLUMNABLE( priv->tview ), column ),
					-1 );
		}
	}
	g_list_free( children );
	my_igridlist_set_widget( MY_IGRIDLIST( self ), grid, combo, 1+XFIL_COL_FIELD, row, 1, 1 );
	g_signal_connect( combo, "changed", G_CALLBACK( extfilter_on_field_changed ), self );

	/* condition combo box */
	combo = gtk_combo_box_new();

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", COND_COL_LABEL );

	store = gtk_list_store_new( COND_N_COLUMNS, G_TYPE_STRING, G_TYPE_UINT );
	gtk_combo_box_set_model( GTK_COMBO_BOX( combo ), GTK_TREE_MODEL( store ));

	for( i=0 ; st_conditions[i].label ; ++i ){
		gtk_list_store_insert_with_values(
				store, &iter, -1,
				COND_COL_LABEL, gettext( st_conditions[i].label ),
				COND_COL_COND, st_conditions[i].cond,
				-1 );
	}
	my_igridlist_set_widget( MY_IGRIDLIST( self ), grid, combo, 1+XFIL_COL_CONDITION, row, 1, 1 );
	g_signal_connect( combo, "changed", G_CALLBACK( extfilter_on_condition_changed ), self );

	/* value entry */
	entry = gtk_entry_new();
	gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), ACC_NUMBER_MAX_LENGTH );
	my_igridlist_set_widget( MY_IGRIDLIST( self ), grid, entry, 1+XFIL_COL_VALUE, row, 1, 1 );
	g_signal_connect( entry, "changed", G_CALLBACK( extfilter_on_value_changed ), self );
	gtk_widget_get_preferred_size( entry, NULL, &rq );
	priv->btn_size = rq.height;
}

static void
setup_row_values( ofaEntryPage *self, GtkGrid *grid, guint row, sExtend *crit )
{
	ofaEntryPagePrivate *priv;
	GtkWidget *combo, *entry;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	guint operator, field, condition;

	priv = ofa_entry_page_get_instance_private( self );

	/* operator */
	if( crit->operator != OPERATOR_NONE ){
		combo = gtk_grid_get_child_at( GTK_GRID( priv->ext_grid ), 1+XFIL_COL_OPERATOR, row );
		g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));
		tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ));
		if( gtk_tree_model_get_iter_first( tmodel, &iter )){
			while( TRUE ){
				gtk_tree_model_get( tmodel, &iter, OPE_COL_OPERATOR, &operator, -1 );
				if( operator == crit->operator ){
					gtk_combo_box_set_active_iter( GTK_COMBO_BOX( combo ), &iter );
					break;
				}
				if( !gtk_tree_model_iter_next( tmodel, &iter )){
					break;
				}
			}
		}
	}

	/* field */
	combo = gtk_grid_get_child_at( GTK_GRID( priv->ext_grid ), 1+XFIL_COL_FIELD, row );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));
	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ));
	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, &iter, FLD_COL_ID, &field, -1 );
			if( field == crit->field ){
				gtk_combo_box_set_active_iter( GTK_COMBO_BOX( combo ), &iter );
				break;
			}
			if( !gtk_tree_model_iter_next( tmodel, &iter )){
				break;
			}
		}
	}

	/* condition */
	combo = gtk_grid_get_child_at( GTK_GRID( priv->ext_grid ), 1+XFIL_COL_CONDITION, row );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));
	tmodel = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ));
	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, &iter, COND_COL_COND, &condition, -1 );
			if( condition == crit->condition ){
				gtk_combo_box_set_active_iter( GTK_COMBO_BOX( combo ), &iter );
				break;
			}
			if( !gtk_tree_model_iter_next( tmodel, &iter )){
				break;
			}
		}
	}

	/* value */
	if( my_strlen( crit->value )){
		entry = gtk_grid_get_child_at( GTK_GRID( priv->ext_grid ), 1+XFIL_COL_VALUE, row );
		g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
		gtk_entry_set_text( GTK_ENTRY( entry ), crit->value );
	}
}

static void
extfilter_on_operator_changed( GtkComboBox *combo, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;

	priv = ofa_entry_page_get_instance_private( self );

	extfilter_check_rows( self, GTK_GRID( priv->ext_grid ));
}

static void
extfilter_on_field_changed( GtkComboBox *combo, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;

	priv = ofa_entry_page_get_instance_private( self );

	extfilter_check_rows( self, GTK_GRID( priv->ext_grid ));
}

static void
extfilter_on_condition_changed( GtkComboBox *combo, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;

	priv = ofa_entry_page_get_instance_private( self );

	extfilter_check_rows( self, GTK_GRID( priv->ext_grid ));
}

static void
extfilter_on_value_changed( GtkEntry *entry, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;

	priv = ofa_entry_page_get_instance_private( self );

	extfilter_check_rows( self, GTK_GRID( priv->ext_grid ));
}

/*
 * Adding an empty row does not require to enable the Apply button.
 * Moving a row up or down may require to update some fields sensitivity.
 * Removing a row should enable the button as filters may have changed.
 */
static void
extfilter_on_row_changed( myIGridlist *instance, GtkGrid *grid, void *empty )
{
	ofaEntryPagePrivate *priv;
	guint rows_count;

	priv = ofa_entry_page_get_instance_private( OFA_ENTRY_PAGE( instance ));

	rows_count = my_igridlist_get_details_count( instance, grid );

	gtk_widget_set_sensitive( priv->ext_init_btn, rows_count == 0 );
	gtk_widget_set_sensitive( priv->ext_reset_btn, rows_count > 0 );

	extfilter_check_rows( OFA_ENTRY_PAGE( instance ), grid );

	priv->ext_rows = rows_count;
}

/*
 * Has to check each row, only enabling the 'Apply' button if all are valid.
 * On first row, make sure the 'Operator' combo box is empty and disabled.
 * On other rows, make sure it is enabled.
 */
static void
extfilter_check_rows( ofaEntryPage *self, GtkGrid *grid )
{
	ofaEntryPagePrivate *priv;
	guint rows_count, row, invalid_count;
	gboolean valid_row;
	GtkWidget *oper_combo;

	priv = ofa_entry_page_get_instance_private( self );

	invalid_count = 0;
	rows_count = my_igridlist_get_details_count( MY_IGRIDLIST( self ), grid );

	for( row=0 ; row<rows_count ; ++row ){

		oper_combo = gtk_grid_get_child_at( grid, 1+XFIL_COL_OPERATOR, row );
		g_return_if_fail( oper_combo && GTK_IS_COMBO_BOX( oper_combo ));
		if( row == 0 ){
			gtk_combo_box_set_active( GTK_COMBO_BOX( oper_combo ), -1 );
			gtk_widget_set_sensitive( oper_combo, FALSE );
		} else {
			gtk_widget_set_sensitive( oper_combo, TRUE );
		}

		valid_row = extfilter_get_row_valid( self, grid, row );
		extfilter_set_valid_image( self, grid, row, valid_row );
		if( !valid_row ){
			invalid_count += 1;
		}
	}

	gtk_widget_set_sensitive( priv->ext_apply_btn, rows_count > 0 && invalid_count == 0 );
}

static gboolean
extfilter_get_row_valid( ofaEntryPage *self, GtkGrid *grid, guint row )
{
	sExtend *crit;
	gboolean valid;

	crit = extfilter_get_criterium( self, row );
	valid = extfilter_get_is_valid_criterium( self, crit, row );
	extfilter_free_criterium( self, crit );

	return( valid );
}

static void
extfilter_set_valid_image( ofaEntryPage *self, GtkGrid *grid, guint row, gboolean valid )
{
	static const gchar *thisfn = "ofa_entry_page_extfilter_set_valid_image";
	ofaEntryPagePrivate *priv;
	GdkPixbuf *pixbuf;
	GError *error;
	GtkWidget *image;

	priv = ofa_entry_page_get_instance_private( self );

	error = NULL;
	pixbuf = gdk_pixbuf_new_from_resource_at_scale(
					valid ? st_green_check_png : st_red_cross_png,
					priv->btn_size, -1, TRUE, &error );
	if( !pixbuf ){
		g_warning( "%s: %s", thisfn, error->message );
		g_error_free( error );

	} else {
		image = gtk_image_new_from_pixbuf( pixbuf );
		g_object_unref( pixbuf );
		my_igridlist_set_widget( MY_IGRIDLIST( self ), grid, image, 1+XFIL_COL_STATUS, row, 1, 1 );
	}
}

/*
 * Init extended filter from values taken from standard filter.
 *
 * We may assert here that the stack is switched on extended filter,
 * and that there is no user row in the #myIGridlist.
 */
static void
extfilter_on_init_from_clicked( GtkButton *button, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	sExtend *extend;
	gboolean first;
	const GDate *date;

	priv = ofa_entry_page_get_instance_private( self );
	first = TRUE;

	/* check for status
	 * as a side effect, if no status is selected in standard filter,
	 * all status will be accepted here */
	extfilter_on_init_status( self, priv->deleted_btn,   ENT_STATUS_DELETED,   &first );
	extfilter_on_init_status( self, priv->rough_btn,     ENT_STATUS_ROUGH,     &first );
	extfilter_on_init_status( self, priv->validated_btn, ENT_STATUS_VALIDATED, &first );

	/* check for period */
	extfilter_on_init_period( self, priv->past_btn,      ENT_PERIOD_PAST,      &first );
	extfilter_on_init_period( self, priv->current_btn,   ENT_PERIOD_CURRENT,   &first );
	extfilter_on_init_period( self, priv->future_btn,    ENT_PERIOD_FUTURE,    &first );

	/* check for general selection ledger vs. account */
	extend = g_new0( sExtend, 1 );
	extend->operator = first ? OPERATOR_NONE : OPERATOR_AND;
	if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->ledger_btn ))){
		extend->field = ENTRY_COL_LEDGER;
		extend->value = priv->jou_mnemo;
	} else if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->account_btn ))){
		extend->field = ENTRY_COL_ACCOUNT;
		extend->value = priv->acc_number;
	}
	if( extend->field ){
		extend->condition = COND_EQUAL;
		my_igridlist_add_row( MY_IGRIDLIST( self ), GTK_GRID( priv->ext_grid ), extend );
		first = FALSE;
	}
	g_free( extend );

	/* check for effect date */
	date = ofa_idate_filter_get_date( OFA_IDATE_FILTER( priv->effect_filter ), IDATE_FILTER_FROM );
	if( my_date_is_valid( date )){
		extend = g_new0( sExtend, 1 );
		extend->operator = first ? OPERATOR_NONE : OPERATOR_AND;
		extend->field = ENTRY_COL_DEFFECT;
		extend->condition = COND_GE;
		extend->value = my_date_to_str( date, ofa_prefs_date_get_display_format( priv->getter ));
		my_igridlist_add_row( MY_IGRIDLIST( self ), GTK_GRID( priv->ext_grid ), extend );
		first = FALSE;
		g_free( extend->value );
		g_free( extend );
	}
	date = ofa_idate_filter_get_date( OFA_IDATE_FILTER( priv->effect_filter ), IDATE_FILTER_TO );
	if( my_date_is_valid( date )){
		extend = g_new0( sExtend, 1 );
		extend->operator = first ? OPERATOR_NONE : OPERATOR_AND;
		extend->field = ENTRY_COL_DEFFECT;
		extend->condition = COND_LE;
		extend->value = my_date_to_str( date, ofa_prefs_date_get_display_format( priv->getter ));
		my_igridlist_add_row( MY_IGRIDLIST( self ), GTK_GRID( priv->ext_grid ), extend );
		first = FALSE;
		g_free( extend->value );
		g_free( extend );
	}

	/* and apply this filter (which should not change anything) */
	refresh_display( self );
}

static void
extfilter_on_init_status( ofaEntryPage *self, GtkWidget *btn, ofeEntryStatus status, gboolean *first )
{
	ofaEntryPagePrivate *priv;
	sExtend *extend;

	priv = ofa_entry_page_get_instance_private( self );

	if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( btn ))){
		extend = g_new0( sExtend, 1 );
		extend->operator = *first ? OPERATOR_NONE : OPERATOR_OR;
		extend->field = ENTRY_COL_STATUS;
		extend->condition = COND_EQUAL;
		extend->value = ( gchar * ) ofo_entry_status_get_abr( status );
		my_igridlist_add_row( MY_IGRIDLIST( self ), GTK_GRID( priv->ext_grid ), extend );
		*first = FALSE;
		g_free( extend );
	}
}

static void
extfilter_on_init_period( ofaEntryPage *self, GtkWidget *btn, ofeEntryPeriod period, gboolean *first )
{
	ofaEntryPagePrivate *priv;
	sExtend *extend;

	priv = ofa_entry_page_get_instance_private( self );

	if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( btn ))){
		extend = g_new0( sExtend, 1 );
		extend->operator = *first ? OPERATOR_NONE : OPERATOR_AND;
		extend->field = ENTRY_COL_IPERIOD;
		extend->condition = COND_EQUAL;
		extend->value = ( gchar * ) ofo_entry_period_get_abr( period );
		my_igridlist_add_row( MY_IGRIDLIST( self ), GTK_GRID( priv->ext_grid ), extend );
		*first = FALSE;
		g_free( extend );
	}
}

/*
 * Remove all user rows from the myIGridlist
 */
static void
extfilter_on_reset_clicked( GtkButton *button, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	guint count, i;

	priv = ofa_entry_page_get_instance_private( self );

	count = my_igridlist_get_details_count( MY_IGRIDLIST( self ), GTK_GRID( priv->ext_grid ));

	for( i=0 ; i<count ; ++i ){
		my_igridlist_remove_row( MY_IGRIDLIST( self ), GTK_GRID( priv->ext_grid ), -1 );
	}

	gtk_widget_set_sensitive( priv->ext_apply_btn, FALSE );

	refresh_display( self );
}

static void
extfilter_on_apply_clicked( GtkButton *button, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;

	priv = ofa_entry_page_get_instance_private( self );

	gtk_widget_set_sensitive( priv->ext_apply_btn, FALSE );

	refresh_display( self );
}

/*
 * The returned #sExtend criterium should be extfilter_free_criterium()
 * by the caller.
 */
static sExtend *
extfilter_get_criterium( ofaEntryPage *self, guint row )
{
	ofaEntryPagePrivate *priv;
	sExtend *criterium;
	GtkWidget *combo, *entry;
	GtkTreeIter combo_iter;
	GtkTreeModel *combo_model;

	priv = ofa_entry_page_get_instance_private( self );

	criterium = g_new0( sExtend, 1 );

	/* operator */
	criterium->operator = OPERATOR_NONE;
	if( row > 0 ){
		combo = gtk_grid_get_child_at( GTK_GRID( priv->ext_grid ), 1+XFIL_COL_OPERATOR, row );
		g_return_val_if_fail( combo && GTK_IS_COMBO_BOX( combo ), FALSE );
		if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX( combo ), &combo_iter )){
			combo_model = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ));
			gtk_tree_model_get( combo_model, &combo_iter, OPE_COL_OPERATOR, &criterium->operator, -1 );
		}
	}

	/* field */
	criterium->field = -1;
	combo = gtk_grid_get_child_at( GTK_GRID( priv->ext_grid ), 1+XFIL_COL_FIELD, row );
	g_return_val_if_fail( combo && GTK_IS_COMBO_BOX( combo ), FALSE );
	if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX( combo ), &combo_iter )){
		combo_model = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ));
		gtk_tree_model_get( combo_model, &combo_iter, FLD_COL_ID, &criterium->field, -1 );
	}

	/* condition */
	criterium->condition = 0;
	combo = gtk_grid_get_child_at( GTK_GRID( priv->ext_grid ), 1+XFIL_COL_CONDITION, row );
	g_return_val_if_fail( combo && GTK_IS_COMBO_BOX( combo ), FALSE );
	if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX( combo ), &combo_iter )){
		combo_model = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ));
		gtk_tree_model_get( combo_model, &combo_iter, COND_COL_COND, &criterium->condition, -1 );
	}

	/* value */
	criterium->value = NULL;
	entry = gtk_grid_get_child_at( GTK_GRID( priv->ext_grid ), 1+XFIL_COL_VALUE, row );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	criterium->value = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	return( criterium );
}

static gboolean
extfilter_get_is_valid_criterium( ofaEntryPage *self, sExtend *criterium, guint row )
{
	/* check for operator
	 * must be set if row greater than zero */
	if( row > 0 ){
		if( criterium->operator != OPERATOR_AND && criterium->operator != OPERATOR_OR ){
			return( FALSE );
		}
	}

	/* check for field */
	if( criterium->field < 0 ){
		return( FALSE );
	}

	/* check for condition */
	if( criterium->condition == 0 ){
		return( FALSE );
	}

	/* check for value */
	if( my_strlen( criterium->value ) == 0 ){
		return( FALSE );
	}

	/* ok */
	return( TRUE );
}

static void
extfilter_free_criterium( ofaEntryPage *self, sExtend *criterium )
{
	g_free( criterium->value );
	g_free( criterium );
}

static void
setup_footer( ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	GtkWidget *widget;

	priv = ofa_entry_page_get_instance_private( self );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "pt-comment" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->comment = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "bottom-paned" );
	g_return_if_fail( widget && GTK_IS_PANED( widget ));
	priv->bottom_paned = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "bot-balance" );
	g_return_if_fail( widget && GTK_IS_GRID( widget ));
	priv->bottom_balance = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "bot-debit" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	ofa_itvcolumnable_twins_group_add_widget( OFA_ITVCOLUMNABLE( priv->tview ), "amount", widget );
	priv->bottom_debit = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "bot-credit" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	ofa_itvcolumnable_twins_group_add_widget( OFA_ITVCOLUMNABLE( priv->tview ), "amount", widget );
	priv->bottom_credit = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "bot-currency" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->bottom_currency = widget;
}

static void
setup_actions( ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	GMenu *menu;

	priv = ofa_entry_page_get_instance_private( self );

	/* new action */
	priv->new_action = g_simple_action_new( "new", NULL );
	g_signal_connect( priv->new_action, "activate", G_CALLBACK( action_on_new_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->new_action ),
			_( "New..." ));
	g_simple_action_set_enabled( priv->new_action, FALSE );

	/* update action */
	priv->update_action = g_simple_action_new( "update", NULL );
	g_signal_connect( priv->update_action, "activate", G_CALLBACK( action_on_update_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->update_action ),
			_( "View/edit properties..." ));
	g_simple_action_set_enabled( priv->update_action, FALSE );

	/* delete action */
	priv->delete_action = g_simple_action_new( "delete", NULL );
	g_signal_connect( priv->delete_action, "activate", G_CALLBACK( action_on_delete_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->delete_action ),
			_( "Delete..." ));
	g_simple_action_set_enabled( priv->delete_action, FALSE );

	/* display operation action */
	priv->vope_action = g_simple_action_new( "vope", NULL );
	g_signal_connect( priv->vope_action, "activate", G_CALLBACK( action_on_vope_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->vope_action ),
			_( "View operation..." ));
	g_simple_action_set_enabled( priv->vope_action, FALSE );

	/* display conciliation group action */
	priv->vconcil_action = g_simple_action_new( "vconcil", NULL );
	g_signal_connect( priv->vconcil_action, "activate", G_CALLBACK( action_on_vconcil_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->vconcil_action ),
			_( "View conciliation group..." ));
	g_simple_action_set_enabled( priv->vconcil_action, FALSE );

	/* display settlement group action */
	priv->vsettle_action = g_simple_action_new( "vsettle", NULL );
	g_signal_connect( priv->vsettle_action, "activate", G_CALLBACK( action_on_vsettle_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->vsettle_action ),
			_( "View settlement group..." ));
	g_simple_action_set_enabled( priv->vsettle_action, FALSE );

	/* display settlement group action */
	priv->export_action = g_simple_action_new( "export", NULL );
	g_signal_connect( priv->export_action, "activate", G_CALLBACK( action_on_export_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->export_action ),
			_( "Export the view content..." ));
	g_simple_action_set_enabled( priv->export_action, TRUE );

	menu = ofa_iactionable_get_menu( OFA_IACTIONABLE( self ), priv->settings_prefix );
	ofa_icontext_set_menu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( self ),
			menu );

	menu = ofa_itvcolumnable_get_menu( OFA_ITVCOLUMNABLE( priv->tview ));
	ofa_icontext_append_submenu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( priv->tview ),
			OFA_IACTIONABLE_VISIBLE_COLUMNS_ITEM, menu );
}

static GtkWidget *
page_v_get_top_focusable_widget( const ofaPage *page )
{
	ofaEntryPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_ENTRY_PAGE( page ), NULL );

	priv = ofa_entry_page_get_instance_private( OFA_ENTRY_PAGE( page ));

	return( ofa_tvbin_get_tree_view( OFA_TVBIN( priv->tview )));
}

/*
 * toggle between display per ledger or display per account
 */
static void
gen_selection_on_toggled( GtkToggleButton *button, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	gboolean is_active;

	priv = ofa_entry_page_get_instance_private( self );

	if( !priv->initializing ){
		is_active = gtk_toggle_button_get_active( button );

		if( button == GTK_TOGGLE_BUTTON( priv->account_btn )){
			/* update the frames sensitivity */
			gtk_widget_set_sensitive( priv->ledger_parent, !is_active );
			gtk_widget_set_sensitive( priv->account_entry, is_active );
			gtk_widget_set_sensitive( priv->f1_label, is_active );

			/* and display the entries */
			if( is_active ){
				g_idle_add(( GSourceFunc ) account_display_from, self );
			}
		} else {
			/* update the frames sensitivity */
			gtk_widget_set_sensitive( priv->ledger_parent, is_active );
			gtk_widget_set_sensitive( priv->account_entry, !is_active );
			gtk_widget_set_sensitive( priv->f1_label, !is_active );

			/* and display the entries */
			if( is_active ){
				g_idle_add(( GSourceFunc ) ledger_display_from, self );
			}
		}
	}
}

/*
 * ofaLedgerCombo signal handler
 */
static void
ledger_on_changed( ofaLedgerCombo *combo, const gchar *mnemo, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;

	priv = ofa_entry_page_get_instance_private( self );

	g_free( priv->jou_mnemo );
	priv->jou_mnemo = g_strdup( mnemo );

	if( !priv->initializing ){
		if( my_strlen( priv->jou_mnemo )){
			g_idle_add(( GSourceFunc ) ledger_display_from, self );
		}
	}
}

/*
 * executed in an idle loop
 */
static gboolean
ledger_display_from( ofaEntryPage *self )
{
	static const gchar *thisfn = "ofa_entry_page_ledger_display_from";
	ofaEntryPagePrivate *priv;

	priv = ofa_entry_page_get_instance_private( self );

	g_debug( "%s: self=%p, ledger=%s", thisfn, ( void * ) self, priv->jou_mnemo );

	ofa_tvbin_refilter( OFA_TVBIN( priv->tview ));
	balances_compute( self );

	return( G_SOURCE_REMOVE );
}

static void
account_on_changed( GtkEntry *entry, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	ofoAccount *account;

	priv = ofa_entry_page_get_instance_private( self );

	priv->acc_valid = FALSE;
	g_free( priv->acc_number );
	priv->acc_number = g_strdup( gtk_entry_get_text( entry ));

	account = ofo_account_get_by_number( priv->getter, priv->acc_number );

	if( account && !ofo_account_is_root( account )){
		gtk_label_set_text( GTK_LABEL( priv->f1_label ), ofo_account_get_label( account ));
		priv->acc_valid = TRUE;

	} else {
		gtk_label_set_text( GTK_LABEL( priv->f1_label ), "" );
	}

	if( !priv->initializing ){
		if( my_strlen( priv->acc_number )){
			g_idle_add(( GSourceFunc ) account_display_from, self );
		}
	}
}

/*
 * if account is invalid, and Tab is pressed, then directly opens the
 * AccountSelect dialogbox
 */
static gboolean
account_on_entry_key_pressed( GtkWidget *entry, GdkEventKey *event, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	gboolean stop;

	priv = ofa_entry_page_get_instance_private( self );

	stop = FALSE;

	/* a row may be inserted any where */
	if( event->keyval == GDK_KEY_Tab && !priv->acc_valid ){
		account_do_select( self );
		stop = TRUE;
	}

	return( stop );
}

/*
 * button may be NULL when called from above account_on_entry_key_pressed()
 */
static void
account_do_select( ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	gchar *acc_number;

	priv = ofa_entry_page_get_instance_private( self );

	acc_number = ofa_account_select_run_modal(
							priv->getter, NULL,
							gtk_entry_get_text( GTK_ENTRY( priv->account_entry )),
							ACCOUNT_ALLOW_DETAIL );

	if( acc_number ){
		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), acc_number );
		g_free( acc_number );
	}
}

static gboolean
account_display_from( ofaEntryPage *self )
{
	static const gchar *thisfn = "ofa_entry_page_account_display_from";
	ofaEntryPagePrivate *priv;

	priv = ofa_entry_page_get_instance_private( self );

	g_debug( "%s: self=%p, account=%s", thisfn, ( void * ) self, priv->acc_number );

	ofa_tvbin_refilter( OFA_TVBIN( priv->tview ));
	balances_compute( self );

	return( G_SOURCE_REMOVE );
}

static void
effect_filter_on_changed( ofaIDateFilter *filter, gint who, gboolean empty, const GDate *date, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;

	priv = ofa_entry_page_get_instance_private( self );

	if( !priv->initializing ){
		refresh_display( self );
	}
}

/*
 * display entries based on their status (delete, rough, validated)
 */
static void
status_on_toggled( GtkToggleButton *button, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;

	priv = ofa_entry_page_get_instance_private( self );

	if( !priv->initializing ){
		refresh_display( self );
	}
}

/*
 * display entries based on their period (past, current, future)
 */
static void
period_on_toggled( GtkToggleButton *button, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;

	priv = ofa_entry_page_get_instance_private( self );

	if( !priv->initializing ){
		refresh_display( self );
	}
}

/*
 * a callback for the 'notify' GObject signal
 *
 * The notify signal is emitted on an object when one of its properties
 * has been changed. Note that getting this signal doesn't guarantee
 * that the value of the property has actually changed, it may also be
 * emitted when the setter for the property is called to reinstate the
 * previous value.
 *
 * VERY DANGEROUS: all columns are editable here
 */
static void
edit_on_switched( GtkSwitch *switch_btn, GParamSpec *pspec, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	GtkTreeSelection *selection;
	gboolean editable;

	priv = ofa_entry_page_get_instance_private( self );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( priv->tview ));
	editable = row_is_editable( self, selection );
	edit_set_cells_editable( self, selection, editable );
}

/*
 * Reset the editability status of the row when:
 * - the selection changes
 * - the edit switch is toggled
 *
 * @editable: whether the entry is rough or future, and the dossier is
 *  a current exercice which has not been opened in read-only mode.
 *
 * This is called on each selection change.
 */
static void
edit_set_cells_editable( ofaEntryPage *self, GtkTreeSelection *selection, gboolean editable )
{
	static const gchar *thisfn = "ofa_entry_page_edit_set_cells_editable";
	ofaEntryPagePrivate *priv;
	gboolean is_active;
	gboolean new_enabled, update_enabled, delete_enabled;
	gint count;

	priv = ofa_entry_page_get_instance_private( self );

	count = gtk_tree_selection_count_selected_rows( selection );
	is_active = gtk_switch_get_active( GTK_SWITCH( priv->edit_switch ));
	priv->editable_row = editable && is_active;

	/* new: if dossier is writable and edition is on */
	new_enabled = priv->is_writable && is_active;
	g_simple_action_set_enabled( priv->new_action, new_enabled );

	/* edit/view: if one row is selected */
	update_enabled = ( count == 1 );
	g_simple_action_set_enabled( priv->update_action, update_enabled );

	/* delete: if dossier is writable and edition is on and row is editable and count > 0 */
	delete_enabled = priv->editable_row && count > 0;
	g_simple_action_set_enabled( priv->delete_action, delete_enabled );

	if( 0 ){
		g_debug( "%s: new_enabled=%s, update_enabled=%s, delete_enabled=%s",
				thisfn, new_enabled ? "True":"False",
				update_enabled ? "True":"False", delete_enabled ? "True":"False" );
		gboolean stat = g_action_get_enabled( G_ACTION( priv->update_action ));
		g_debug( "edit_set_cells_editable: update_action_status=%s", stat ? "True":"False" );
	}
}

static void
edit_on_cell_edited( GtkCellRendererText *cell, gchar *path_str, gchar *text, ofaEntryPage *self )
{
	static const gchar *thisfn = "ofa_entry_page_edit_on_cell_edited";
	ofaEntryPagePrivate *priv;
	gint column_id;
	GtkTreeModel *sort_model, *filter_model;
	GtkTreePath *sort_path;
	GtkTreeIter sort_iter, filter_iter, store_iter;
	gchar *str;
	gdouble amount;
	ofoEntry *entry;

	g_debug( "%s: cell=%p, path=%s, text=%s, self=%p",
			thisfn, ( void * ) cell, path_str, text, ( void * ) self );

	priv = ofa_entry_page_get_instance_private( self );

	sort_model = ofa_tvbin_get_tree_model( OFA_TVBIN( priv->tview ));
	filter_model = gtk_tree_model_sort_get_model( GTK_TREE_MODEL_SORT( sort_model ));
	sort_path = gtk_tree_path_new_from_string( path_str );

	if( gtk_tree_model_get_iter( sort_model, &sort_iter, sort_path )){

		gtk_tree_model_sort_convert_iter_to_child_iter(
				GTK_TREE_MODEL_SORT( sort_model ), &filter_iter, &sort_iter );
		gtk_tree_model_filter_convert_iter_to_child_iter(
				GTK_TREE_MODEL_FILTER( filter_model), &store_iter, &filter_iter );

		column_id = ofa_itvcolumnable_get_column_id_renderer( OFA_ITVCOLUMNABLE( priv->tview ), GTK_CELL_RENDERER( cell ));
		edit_set_column_set_indicator( self, column_id, &store_iter );

		/* also set the operation date so that it will not get modified
		 * when checking the effect date - only for already recorded
		 * entries as we are so sure that the operation date was valid */
		gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), &store_iter, ENTRY_COL_OBJECT, &entry, -1 );
		if( entry ){
			g_object_unref( entry );
			if( ofo_entry_get_number( entry ) > 0 ){
				edit_set_column_set_indicator( self, ENTRY_COL_DOPE, &store_iter );
			}
		}

		/* reformat amounts before storing them */
		if( column_id == ENTRY_COL_DEBIT || column_id == ENTRY_COL_CREDIT ){
			amount = ofa_amount_from_str( text, priv->getter );
			str = ofa_amount_to_str( amount, NULL, priv->getter );
		} else {
			str = g_strdup( text );
		}
		gtk_list_store_set( GTK_LIST_STORE( priv->store ), &store_iter, column_id, str, -1 );
		g_free( str );

		check_row_for_valid( self, &store_iter );
		balances_compute( self );

		if( row_get_errlevel( self, GTK_TREE_MODEL( priv->store ), &store_iter ) == ENT_ERR_NONE ){
			save_entry( self, GTK_TREE_MODEL( priv->store ), &store_iter );
		}
	}
	gtk_tree_path_free( sort_path );
}

/*
 * a data has been edited by the user: set the corresponding flag (if
 * any) so that we do not try later to reset a default value
 */
static gint
edit_get_column_set_id( ofaEntryPage *self, gint column_id )
{
	gint column_set_id;

	column_set_id = 0;

	switch( column_id ){
		case ENTRY_COL_DOPE:
			column_set_id = ENTRY_COL_DOPE_SET;
			break;
		case ENTRY_COL_DEFFECT:
			column_set_id = ENTRY_COL_DEFFECT_SET;
			break;
		case ENTRY_COL_CURRENCY:
			column_set_id = ENTRY_COL_CURRENCY_SET;
			break;
		default:
			break;
	}

	return( column_set_id );
}

/*
 * a data has been edited by the user: set the corresponding flag (if
 * any) so that we do not try later to reset a default value
 */
static void
edit_set_column_set_indicator( ofaEntryPage *self, gint column_id, GtkTreeIter *store_iter )
{
	ofaEntryPagePrivate *priv;
	gint column_set_id;

	priv = ofa_entry_page_get_instance_private( self );

	column_set_id = edit_get_column_set_id( self, column_id );
	if( column_set_id > 0 ){
		gtk_list_store_set( GTK_LIST_STORE( priv->store ), store_iter, column_set_id, TRUE, -1 );
	}
}

/**
 * ofa_entry_page_display_entries:
 * @page: this #ofaEntryPage instance.
 * @type: the #ofoBase GType of the @id (i.e. either OFO_TYPE_ACCOUNT
 *  or OFO_TYPE_LEDGER).
 * @id: the identifier.
 * @begin: [allow-none]: the beginning date of the effect filter.
 * @end: [allow-none]: the ending date of the effect filter.
 */
void
ofa_entry_page_display_entries( ofaEntryPage *page, GType type, const gchar *id, const GDate *begin, const GDate *end )
{
	static const gchar *thisfn = "ofa_entry_page_display_entries";
	ofaEntryPagePrivate *priv;

	g_return_if_fail( page && OFA_IS_ENTRY_PAGE( page ));
	g_return_if_fail( my_strlen( id ));
	g_return_if_fail( !OFA_PAGE( page )->prot->dispose_has_run );

	g_debug( "%s: page=%p, type=%lu, id=%s, begin=%p, end=%p",
			thisfn, ( void * ) page, type, id, ( void * ) begin, ( void * ) end );

	priv = ofa_entry_page_get_instance_private( page );

	/* start by setting the from/to dates as these changes do not
	 * automatically trigger a display refresh */
	ofa_idate_filter_set_date(
			OFA_IDATE_FILTER( priv->effect_filter ), IDATE_FILTER_FROM, begin );
	ofa_idate_filter_set_date(
			OFA_IDATE_FILTER( priv->effect_filter ), IDATE_FILTER_TO, end );

	/* then setup the general selection: changes on theses entries
	 * will automativally trigger a display refresh */
	if( type == OFO_TYPE_ACCOUNT ){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->account_btn ), TRUE );
		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), id );

	} else if( type == OFO_TYPE_LEDGER ){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->ledger_btn ), TRUE );
		ofa_ledger_combo_set_selected( priv->ledger_combo, id );
	}
}

/**
 * ofa_entry_page_display_operations:
 * @page: this #ofaEntryPage instance.
 * @opes: a list of operations number.
 *
 * Display the entries which are related to the @opes operations.
 */
void
ofa_entry_page_display_operations( ofaEntryPage *page, GList *opes )
{
	static const gchar *thisfn = "ofa_entry_page_display_operations";
	ofaEntryPagePrivate *priv;
	GList *it;
	sExtend *extend;
	ofxCounter number;
	gboolean first;

	g_return_if_fail( page && OFA_IS_ENTRY_PAGE( page ));
	g_return_if_fail( !OFA_PAGE( page )->prot->dispose_has_run );

	g_debug( "%s: page=%p, opes=%p",
			thisfn, ( void * ) page, ( void * ) opes );

	priv = ofa_entry_page_get_instance_private( page );

	/* setup an extended filter on the operation number */
	first = TRUE;
	for( it=opes ; it ; it=it->next ){
		number = ( ofxCounter ) GPOINTER_TO_UINT( it->data );
		extend = g_new0( sExtend, 1 );
		extend->operator = first ? OPERATOR_NONE : OPERATOR_OR;
		extend->field = ENTRY_COL_OPE_NUMBER;
		extend->condition = COND_EQUAL;
		extend->value = g_strdup_printf( "%lu", number );
		my_igridlist_add_row( MY_IGRIDLIST( page ), GTK_GRID( priv->ext_grid ), extend );
		first = FALSE;
		g_free( extend->value );
		g_free( extend );
	}

	gtk_expander_set_expanded( GTK_EXPANDER( priv->expander ), TRUE );
	gtk_stack_set_visible_child_name( GTK_STACK( priv->stack ), "extended" );
}

static void
refresh_display( ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	GtkTreeSelection *selection;

	priv = ofa_entry_page_get_instance_private( self );

	// refresh treeview
	ofa_tvbin_refilter( OFA_TVBIN( priv->tview ));

	// refresh balance
	balances_compute( self );

	// refresh selection
	selection = ofa_tvbin_get_selection( OFA_TVBIN( priv->tview ));
	tview_on_row_selected( OFA_TVBIN( priv->tview ), selection, self );
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
balances_compute( ofaEntryPage *self )
{
	static const gchar *thisfn = "ofa_entry_page_balances_compute";
	ofaEntryPagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *sdeb, *scre, *dev_code;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = ofa_entry_page_get_instance_private( self );

	ofs_currency_list_free( &priv->balances );
	tmodel = ofa_tvbin_get_tree_model( OFA_TVBIN( priv->tview ));

	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get(
					tmodel,
					&iter,
					ENTRY_COL_DEBIT,    &sdeb,
					ENTRY_COL_CREDIT,   &scre,
					ENTRY_COL_CURRENCY, &dev_code,
					-1 );

			if( my_strlen( dev_code ) && ( my_strlen( sdeb ) || my_strlen( scre ))){
				ofs_currency_add_by_code(
						&priv->balances, priv->getter, dev_code,
						ofa_amount_from_str( sdeb, priv->getter ), ofa_amount_from_str( scre, priv->getter ));
			}

			g_free( sdeb );
			g_free( scre );
			g_free( dev_code );

			if( !gtk_tree_model_iter_next( tmodel, &iter )){
				break;
			}
		}
	}

	g_list_foreach( priv->balances, ( GFunc ) balance_display, self );
	gtk_widget_show_all( priv->bottom_balance );
}

static void
balance_display( ofsCurrency *pc, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	gchar *str;
	const gchar *cstyle;

	priv = ofa_entry_page_get_instance_private( self );

	my_style_remove( priv->bottom_debit, "labelbalance" );
	my_style_remove( priv->bottom_debit, "labelwarning" );
	my_style_remove( priv->bottom_credit, "labelbalance" );
	my_style_remove( priv->bottom_credit, "labelwarning" );
	my_style_remove( priv->bottom_currency, "labelbalance" );
	my_style_remove( priv->bottom_currency, "labelwarning" );

	if( !ofs_currency_is_zero( pc )){
		cstyle = ofs_currency_is_balanced( pc ) ? "labelbalance" : "labelwarning";

		my_style_add( priv->bottom_debit, cstyle );
		str = ofa_amount_to_str( pc->debit, pc->currency, priv->getter );
		gtk_label_set_text( GTK_LABEL( priv->bottom_debit ), str );
		g_free( str );

		my_style_add( priv->bottom_credit, cstyle );
		str = ofa_amount_to_str( pc->credit, pc->currency, priv->getter );
		gtk_label_set_text( GTK_LABEL( priv->bottom_credit ), str );
		g_free( str );

		my_style_add( priv->bottom_currency, cstyle );
		gtk_label_set_text( GTK_LABEL( priv->bottom_currency ), ofo_currency_get_code( pc->currency ));

	} else {
		gtk_label_set_text( GTK_LABEL( priv->bottom_debit ), "" );
		gtk_label_set_text( GTK_LABEL( priv->bottom_credit ), "" );
		gtk_label_set_text( GTK_LABEL( priv->bottom_currency ), "" );
	}
}

/*
 * @iter: a valid #GtkTreeIter on the underlying #GtkListStore
 *
 * Individual checks in general are only able to detect errors.
 */
static void
check_row_for_valid( ofaEntryPage *self, GtkTreeIter *iter )
{
	ofaEntryPagePrivate *priv;
	gboolean v_dope, v_deffect, v_ledger, v_account, v_currency;
	gchar *prev_msg;

	priv = ofa_entry_page_get_instance_private( self );

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
	gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), iter, ENTRY_COL_MSGERR, &prev_msg, -1 );
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

	row_display_message( self, ofa_tvbin_get_selection( OFA_TVBIN( priv->tview )));
}

static gboolean
check_row_for_valid_dope( ofaEntryPage *self, GtkTreeIter *iter )
{
	ofaEntryPagePrivate *priv;
	gchar *sdope, *msg;
	GDate date;
	gboolean is_valid;

	priv = ofa_entry_page_get_instance_private( self );

	is_valid = FALSE;
	gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), iter, ENTRY_COL_DOPE, &sdope, -1 );

	if( my_strlen( sdope )){
		my_date_set_from_str( &date, sdope, ofa_prefs_date_get_display_format( priv->getter ));
		if( my_date_is_valid( &date )){
			is_valid = TRUE;

		} else {
			msg = g_strdup_printf( _( "Operation date '%s' is invalid" ), sdope );
			set_error_msg( self, iter, msg );
			g_free( msg );
		}
	} else {
		set_error_msg( self, iter, _( "Operation date is empty" ));
	}

	g_free( sdope );

	return( is_valid );
}

/*
 * check for intrinsic validity of effect date
 */
static gboolean
check_row_for_valid_deffect( ofaEntryPage *self, GtkTreeIter *iter )
{
	ofaEntryPagePrivate *priv;
	gchar *sdeffect, *msg;
	GDate deff;
	gboolean is_valid;
	gboolean dope_set;
	gint dope_data;

	priv = ofa_entry_page_get_instance_private( self );

	is_valid = FALSE;

	gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), iter, ENTRY_COL_DEFFECT, &sdeffect, -1 );

	if( my_strlen( sdeffect )){
		my_date_set_from_str( &deff, sdeffect, ofa_prefs_date_get_display_format( priv->getter ));
		if( my_date_is_valid( &deff )){
			is_valid = TRUE;

		} else {
			msg = g_strdup_printf( _( "Effect date '%s' is invalid" ), sdeffect );
			set_error_msg( self, iter, msg );
			g_free( msg );
		}
	} else {
		set_error_msg( self, iter, _( "Effect date is empty" ));
	}

	/* if effect date is valid, and operation date has not been set by
	 * the user, then set default operation date to effect date */
	if( is_valid ){
		dope_data = edit_get_column_set_id( self, ENTRY_COL_DOPE );
		gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), iter, dope_data, &dope_set, -1 );
		if( !dope_set ){
			gtk_list_store_set(
					GTK_LIST_STORE( GTK_TREE_MODEL( priv->store ) ), iter, ENTRY_COL_DOPE, sdeffect, -1 );
		}
	}

	g_free( sdeffect );

	return( is_valid );
}

static gboolean
check_row_for_valid_ledger( ofaEntryPage *self, GtkTreeIter *iter )
{
	ofaEntryPagePrivate *priv;
	gchar *str, *msg;
	gboolean is_valid;

	priv = ofa_entry_page_get_instance_private( self );

	is_valid = FALSE;
	gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), iter, ENTRY_COL_LEDGER, &str, -1 );

	if( my_strlen( str )){
		if( ofo_ledger_get_by_mnemo( priv->getter, str )){
			is_valid = TRUE;

		} else {
			msg = g_strdup_printf( _( "Ledger '%s' is unknown or invalid" ), str );
			set_error_msg( self, iter, msg );
			g_free( msg );
		}
	} else {
		set_error_msg( self, iter, _( "Ledger identifier is empty" ));
	}
	g_free( str );

	return( is_valid );
}

static gboolean
check_row_for_valid_account( ofaEntryPage *self, GtkTreeIter *iter )
{
	ofaEntryPagePrivate *priv;
	gchar *acc_number, *cur_code, *msg;
	gboolean is_valid;
	ofoAccount *account;
	gint cur_data;
	gboolean cur_set;

	priv = ofa_entry_page_get_instance_private( self );

	is_valid = FALSE;
	gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), iter,
			ENTRY_COL_ACCOUNT, &acc_number, ENTRY_COL_CURRENCY, &cur_code, -1 );

	if( my_strlen( acc_number )){
		account = ofo_account_get_by_number( priv->getter, acc_number );
		if( account ){
			if( !ofo_account_is_root( account )){
				is_valid = TRUE;

			} else {
				msg = g_strdup_printf( _( "Account %s is a root account" ), acc_number );
				set_error_msg( self, iter, msg );
				g_free( msg );
			}
		} else {
			msg = g_strdup_printf( _( "Account '%s' is unknown" ), acc_number );
			set_error_msg( self, iter, msg );
			g_free( msg );
		}
	} else {
		set_error_msg( self, iter, _( "Account number is empty" ));
	}
	g_free( acc_number );

	/* if account is valid, and currency code has not yet
	 * been set by the user, then setup the default currency */
	if( is_valid ){
		cur_data = edit_get_column_set_id( self, ENTRY_COL_CURRENCY );
		gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), iter, cur_data, &cur_set, -1 );
		if( !cur_set ){
			gtk_list_store_set( GTK_LIST_STORE( priv->store ), iter,
					ENTRY_COL_CURRENCY, ofo_account_get_currency( account ), -1 );
		}
	}

	return( is_valid );
}

static gboolean
check_row_for_valid_label( ofaEntryPage *self, GtkTreeIter *iter )
{
	ofaEntryPagePrivate *priv;
	gchar *str;
	gboolean is_valid;

	priv = ofa_entry_page_get_instance_private( self );

	is_valid = FALSE;
	gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), iter, ENTRY_COL_LABEL, &str, -1 );

	if( my_strlen( str )){
		is_valid = TRUE;
	} else {
		set_error_msg( self, iter, _( "Entry label is empty" ));
	}
	g_free( str );

	return( is_valid );
}

static gboolean
check_row_for_valid_currency( ofaEntryPage *self, GtkTreeIter *iter )
{
	ofaEntryPagePrivate *priv;
	gchar *code, *msg;
	gboolean is_valid;

	priv = ofa_entry_page_get_instance_private( self );

	is_valid = FALSE;
	gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), iter, ENTRY_COL_CURRENCY, &code, -1 );

	if( my_strlen( code )){
		if( ofo_currency_get_by_code( priv->getter, code )){
			is_valid = TRUE;

		} else {
			msg = g_strdup_printf( _( "Currency '%s' is unknown" ), code );
			set_error_msg( self, iter, msg );
			g_free( msg );
		}
	} else {
		set_error_msg( self, iter, _( "Currency is empty" ));
	}
	g_free( code );

	return( is_valid );
}

static gboolean
check_row_for_valid_amounts( ofaEntryPage *self, GtkTreeIter *iter )
{
	ofaEntryPagePrivate *priv;
	gchar *sdeb, *scre;
	gdouble debit, credit;
	gboolean is_valid;

	priv = ofa_entry_page_get_instance_private( self );

	is_valid = FALSE;
	gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), iter, ENTRY_COL_DEBIT, &sdeb, ENTRY_COL_CREDIT, &scre, -1 );

	if( my_strlen( sdeb ) || my_strlen( scre )){
		debit = ofa_amount_from_str( sdeb, priv->getter );
		credit = ofa_amount_from_str( scre, priv->getter );
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
 * effect date of any new entry must be greater or equal to minimal
 * effect date as computed from dossier and ledger
 */
static void
check_row_for_cross_deffect( ofaEntryPage *self, GtkTreeIter *iter )
{
	ofaEntryPagePrivate *priv;
	gchar *sdope, *sdeffect, *mnemo, *msg, *sdmin, *sdeff;
	GDate dope, deff, deff_min;
	ofoLedger *ledger;

	priv = ofa_entry_page_get_instance_private( self );

	gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), iter,
				ENTRY_COL_DOPE,    &sdope,
				ENTRY_COL_DEFFECT, &sdeffect,
				ENTRY_COL_LEDGER,  &mnemo,
				-1 );

	my_date_set_from_str( &dope, sdope, ofa_prefs_date_get_display_format( priv->getter ));
	g_return_if_fail( my_date_is_valid( &dope ));

	my_date_set_from_str( &deff, sdeffect, ofa_prefs_date_get_display_format( priv->getter ));
	g_return_if_fail( my_date_is_valid( &deff ));

	g_return_if_fail( my_strlen( mnemo ));
	ledger = ofo_ledger_get_by_mnemo( priv->getter, mnemo );
	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));

	ofo_dossier_get_min_deffect( priv->dossier, ledger, &deff_min );
	if( !my_date_is_valid( &deff_min )){
		my_date_set_from_date( &deff_min, &dope );
	}

	/* if effect date is greater or equal than minimal effect date for
	 * the row, then it is valid and will normally apply to account and
	 * ledger */
	if( my_date_compare( &deff, &deff_min ) < 0 ){
		sdmin = my_date_to_str( &deff_min, ofa_prefs_date_get_display_format( priv->getter ));
		sdeff = my_date_to_str( &deff, ofa_prefs_date_get_display_format( priv->getter ));
		msg = g_strdup_printf(
				_( "Effect date %s is less than the min effect date %s" ),
				sdeff, sdmin );
		set_error_msg( self, iter, msg );
		g_free( msg );
		g_free( sdeff );
		g_free( sdmin );
	}

	g_free( sdope );
	g_free( sdeffect );
	g_free( mnemo );
}

/*
 * set a default effect date if operation date and ledger are valid
 * => effect date must not have been set by the user
 *
 * Returns: %TRUE if a default date has actually been set
 */
static gboolean
set_default_deffect( ofaEntryPage *self, GtkTreeIter *iter )
{
	ofaEntryPagePrivate *priv;
	gboolean set;
	gboolean deff_set;
	gint deff_data;
	gchar *sdope, *mnemo, *sdeff;
	GDate dope, deff_min;
	ofoLedger *ledger;

	priv = ofa_entry_page_get_instance_private( self );

	set = FALSE;

	deff_data = edit_get_column_set_id( self, ENTRY_COL_DEFFECT );
	gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), iter, deff_data, &deff_set, -1 );
	if( !deff_set ){
		gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), iter,
					ENTRY_COL_DOPE,   &sdope,
					ENTRY_COL_LEDGER, &mnemo,
					-1 );

		my_date_set_from_str( &dope, sdope, ofa_prefs_date_get_display_format( priv->getter ));
		g_return_val_if_fail( my_date_is_valid( &dope ), FALSE );

		g_return_val_if_fail( my_strlen( mnemo ), FALSE );
		ledger = ofo_ledger_get_by_mnemo( priv->getter, mnemo );
		g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );

		ofo_dossier_get_min_deffect( priv->dossier, ledger, &deff_min );
		if( !my_date_is_valid( &deff_min ) || my_date_compare( &deff_min, &dope ) < 0 ){
			my_date_set_from_date( &deff_min, &dope );
		}

		sdeff = my_date_to_str( &deff_min, ofa_prefs_date_get_display_format( priv->getter ));
		gtk_list_store_set( GTK_LIST_STORE( priv->store ), iter, ENTRY_COL_DEFFECT, sdeff, -1 );
		g_free( sdeff );

		g_free( sdope );
		g_free( mnemo );

		set = TRUE;
	}

	return( set );
}

/*
 * @tmodel: the GtkTreeModelSort
 * @iter: an iter on the sorted model
 */
static gboolean
check_row_for_cross_currency( ofaEntryPage *self, GtkTreeIter *iter )
{
	ofaEntryPagePrivate *priv;
	gchar *number, *code, *msg;
	gboolean is_valid;
	const gchar *account_currency;
	ofoAccount *account;

	priv = ofa_entry_page_get_instance_private( self );

	is_valid = FALSE;
	gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), iter, ENTRY_COL_ACCOUNT, &number, ENTRY_COL_CURRENCY, &code, -1 );

	g_return_val_if_fail( my_strlen( number ), FALSE );
	account = ofo_account_get_by_number( priv->getter, number );
	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !ofo_account_is_root( account ), FALSE );

	account_currency = ofo_account_get_currency( account );

	g_return_val_if_fail( my_strlen( code ), FALSE );

	if( !g_utf8_collate( account_currency, code )){
		is_valid = TRUE;

	} else {
		msg = g_strdup_printf(
				_( "Account %s expects %s currency while entry has %s" ),
				number, account_currency, code );
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
reset_error_msg( ofaEntryPage *self, GtkTreeIter *iter )
{
	ofaEntryPagePrivate *priv;

	priv = ofa_entry_page_get_instance_private( self );

	gtk_list_store_set(
			GTK_LIST_STORE( priv->store ), iter, ENTRY_COL_MSGERR, "", ENTRY_COL_MSGWARN, "", -1 );
}

/*
 * @iter: a #GtkTreeIter valid on the underlying GtkListStore
 *
 * Set an error message for the current row
 */
static void
set_error_msg( ofaEntryPage *self, GtkTreeIter *iter, const gchar *msg )
{
	ofaEntryPagePrivate *priv;

	priv = ofa_entry_page_get_instance_private( self );

	gtk_list_store_set(
			GTK_LIST_STORE( priv->store ), iter, ENTRY_COL_MSGERR, msg, -1 );
}

/*
 * @iter: a #GtkTreeIter valid on the underlying GtkListStore
 *
 * Set a warning message for the current row
 */
#if 0
static void
set_warning_msg( ofaEntryPage *self, GtkTreeIter *iter, const gchar *msg )
{
	ofaEntryPagePrivate *priv;

	priv = ofa_entry_page_get_instance_private( self );

	gtk_list_store_set(
			GTK_LIST_STORE( priv->tstore ), iter, ENTRY_COL_MSGWARN, msg, -1 );
}
#endif

/*
 * save a modified or new entry
 */
static gboolean
save_entry( ofaEntryPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	ofaEntryPagePrivate *priv;
	gchar *sdope, *sdeff, *ref, *label, *ledger, *account, *sdeb, *scre, *currency;
	GDate dope, deff;
	gint number;
	ofoEntry *entry;
	gboolean is_new, ok;
	gchar *prev_account, *prev_ledger;
	ofxAmount prev_debit, prev_credit;

	priv = ofa_entry_page_get_instance_private( self );

	ok = FALSE;
	prev_debit = 0;						/* so that gcc -pedantic is happy */
	prev_credit = 0;
	prev_account = NULL;
	prev_ledger = NULL;

	gtk_tree_model_get(
			tmodel,
			iter,
			ENTRY_COL_DOPE,         &sdope,
			ENTRY_COL_DEFFECT,      &sdeff,
			ENTRY_COL_ENT_NUMBER_I, &number,
			ENTRY_COL_REF,          &ref,
			ENTRY_COL_LABEL,        &label,
			ENTRY_COL_LEDGER,       &ledger,
			ENTRY_COL_ACCOUNT,      &account,
			ENTRY_COL_DEBIT,        &sdeb,
			ENTRY_COL_CREDIT,       &scre,
			ENTRY_COL_CURRENCY,     &currency,
			ENTRY_COL_OBJECT,       &entry,
			-1 );
	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	/*g_debug( "save_entry: ref_count=%d", G_OBJECT( entry )->ref_count );*/
	g_object_unref( entry );

	is_new = ofo_entry_get_number( entry ) == 0;

	if( !is_new ){
		prev_account = g_strdup( ofo_entry_get_account( entry ));
		prev_ledger = g_strdup( ofo_entry_get_ledger( entry ));
		prev_debit = ofo_entry_get_debit( entry );
		prev_credit = ofo_entry_get_credit( entry );
	}

	my_date_set_from_str( &dope, sdope, ofa_prefs_date_get_display_format( priv->getter ));
	g_return_val_if_fail( my_date_is_valid( &dope ), FALSE );
	ofo_entry_set_dope( entry, &dope );

	my_date_set_from_str( &deff, sdeff, ofa_prefs_date_get_display_format( priv->getter ));
	g_return_val_if_fail( my_date_is_valid( &deff ), FALSE );
	ofo_entry_set_deffect( entry, &deff );

	ofo_entry_set_ref( entry, my_strlen( ref ) ? ref : NULL );
	ofo_entry_set_label( entry, label );
	ofo_entry_set_ledger( entry, ledger );
	ofo_entry_set_account( entry, account );
	ofo_entry_set_debit( entry, ofa_amount_from_str( sdeb, priv->getter ));
	ofo_entry_set_credit( entry, ofa_amount_from_str( scre, priv->getter ));
	ofo_entry_set_currency( entry, currency );

	if( is_new ){
		ok = ofo_entry_insert( entry );
	} else {
		ok = ofo_entry_update( entry );
		remediate_entry_account( self, entry, prev_account, prev_debit, prev_credit );
		remediate_entry_ledger( self, entry, prev_ledger, prev_debit, prev_credit );
	}

	ofa_tvbin_refilter( OFA_TVBIN( priv->tview ));

	g_free( prev_account );
	g_free( prev_ledger );
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
 * update balances of account and/or ledger if something relevant has
 * changed
 * note that the status cannot be modified here
 */
static void
remediate_entry_account( ofaEntryPage *self, ofoEntry *entry, const gchar *prev_account, ofxAmount prev_debit, ofxAmount prev_credit )
{
	static const gchar *thisfn = "ofa_entry_page_remediate_entry_account";
	ofaEntryPagePrivate *priv;
	const gchar *account;
	ofxAmount amount, debit, credit;
	ofeEntryStatus status;
	ofeEntryPeriod period;
	ofoAccount *account_new, *account_prev;
	gint cmp;

	g_debug( "%s: self=%p, entry=%p, prev_account=%s, prev_debit=%lf, prev_credit=%lf",
			thisfn, ( void * ) self, ( void * ) entry, prev_account, prev_debit, prev_credit );

	priv = ofa_entry_page_get_instance_private( self );

	g_return_if_fail( ofo_entry_is_editable( entry ));

	account = ofo_entry_get_account( entry );
	cmp = g_utf8_collate( account, prev_account );

	debit = ofo_entry_get_debit( entry );
	credit = ofo_entry_get_credit( entry );

	status = ofo_entry_get_status( entry );
	g_return_if_fail( status == ENT_STATUS_ROUGH );

	period = ofo_entry_get_period( entry );
	g_return_if_fail( period == ENT_PERIOD_CURRENT || period == ENT_PERIOD_FUTURE );

	if( cmp != 0 || debit != prev_debit || credit != prev_credit ){

		account_new = ofo_account_get_by_number( priv->getter, account );
		g_return_if_fail( account_new && OFO_IS_ACCOUNT( account_new ));
		if( cmp != 0 ){
			account_prev = ofo_account_get_by_number( priv->getter, prev_account );
			g_return_if_fail( account_prev && OFO_IS_ACCOUNT( account_prev ));
		} else {
			account_prev = account_new;
		}

		switch( period ){
			case ENT_PERIOD_CURRENT:
				amount = ofo_account_get_current_rough_debit( account_prev );
				ofo_account_set_current_rough_debit( account_prev, amount-prev_debit );
				amount = ofo_account_get_current_rough_credit( account_prev );
				ofo_account_set_current_rough_credit( account_prev, amount-prev_credit );
				amount = ofo_account_get_current_rough_debit( account_new );
				ofo_account_set_current_rough_debit( account_new, amount+debit );
				amount = ofo_account_get_current_rough_credit( account_new );
				ofo_account_set_current_rough_credit( account_new, amount+credit );
				break;
			case ENT_PERIOD_FUTURE:
				amount = ofo_account_get_futur_rough_debit( account_prev );
				ofo_account_set_futur_rough_debit( account_prev, amount-prev_debit );
				amount = ofo_account_get_futur_rough_credit( account_prev );
				ofo_account_set_futur_rough_credit( account_prev, amount-prev_credit );
				amount = ofo_account_get_futur_rough_debit( account_new );
				ofo_account_set_futur_rough_debit( account_new, amount+debit );
				amount = ofo_account_get_futur_rough_credit( account_new );
				ofo_account_set_futur_rough_credit( account_new, amount+credit );
				break;
			default:
				break;
		}

		if( cmp != 0 ){
			ofo_account_update_amounts( account_prev );
		}
		ofo_account_update_amounts( account_new );
	}
}

static void
remediate_entry_ledger( ofaEntryPage *self, ofoEntry *entry, const gchar *prev_ledger, ofxAmount prev_debit, ofxAmount prev_credit )
{
	static const gchar *thisfn = "ofa_entry_page_remediate_entry_ledger";
	ofaEntryPagePrivate *priv;
	const gchar *ledger, *currency;
	ofxAmount amount, debit, credit;
	ofeEntryStatus status;
	ofeEntryPeriod period;
	ofoLedger *ledger_new, *ledger_prev;
	gboolean ledger_has_changed;

	g_debug( "%s: self=%p, entry=%p, prev_ledger=%s, prev_debit=%lf, prev_credit=%lf",
			thisfn, ( void * ) self, ( void * ) entry, prev_ledger, prev_debit, prev_credit );

	priv = ofa_entry_page_get_instance_private( self );

	g_return_if_fail( ofo_entry_is_editable( entry ));

	ledger = ofo_entry_get_ledger( entry );
	currency = ofo_entry_get_currency( entry );
	debit = ofo_entry_get_debit( entry );
	credit = ofo_entry_get_credit( entry );
	ledger_has_changed = ( g_utf8_collate( ledger, prev_ledger ) != 0 );

	status = ofo_entry_get_status( entry );
	g_return_if_fail( status == ENT_STATUS_ROUGH );

	period = ofo_entry_get_period( entry );
	g_return_if_fail( period == ENT_PERIOD_CURRENT || period == ENT_PERIOD_FUTURE );

	/* if ledger has changed or debit has changed or credit has changed */
	if( ledger_has_changed || debit != prev_debit || credit != prev_credit ){

		ledger_new = ofo_ledger_get_by_mnemo( priv->getter, ledger );
		g_return_if_fail( ledger_new && OFO_IS_LEDGER( ledger_new ));
		if( ledger_has_changed ){
			ledger_prev = ofo_ledger_get_by_mnemo( priv->getter, prev_ledger );
			g_return_if_fail( ledger_prev && OFO_IS_LEDGER( ledger_prev ));
		} else {
			ledger_prev = ledger_new;
		}

		switch( period ){
			case ENT_PERIOD_CURRENT:
				amount = ofo_ledger_get_current_rough_debit( ledger_prev, currency );
				ofo_ledger_set_current_rough_debit( ledger_prev, amount-prev_debit, currency );
				amount = ofo_ledger_get_current_rough_credit( ledger_prev, currency );
				ofo_ledger_set_current_rough_credit( ledger_prev, amount-prev_credit, currency );
				amount = ofo_ledger_get_current_rough_debit( ledger_new, currency );
				ofo_ledger_set_current_rough_debit( ledger_new, amount+debit, currency );
				amount = ofo_ledger_get_current_rough_credit( ledger_new, currency );
				ofo_ledger_set_current_rough_credit( ledger_new, amount+credit, currency );
				break;
			case ENT_PERIOD_FUTURE:
				amount = ofo_ledger_get_futur_rough_debit( ledger_prev, currency );
				ofo_ledger_set_futur_rough_debit( ledger_prev, amount-prev_debit, currency );
				amount = ofo_ledger_get_futur_rough_credit( ledger_prev, currency );
				ofo_ledger_set_futur_rough_credit( ledger_prev, amount-prev_credit, currency );
				amount = ofo_ledger_get_futur_rough_debit( ledger_new, currency );
				ofo_ledger_set_futur_rough_debit( ledger_new, amount+debit, currency );
				amount = ofo_ledger_get_futur_rough_credit( ledger_new, currency );
				ofo_ledger_set_futur_rough_credit( ledger_new, amount+credit, currency );
				break;
			default:
				break;
		}

		if( ledger_has_changed ){
			ofo_ledger_update_balance( ledger_prev, currency );
		}
		ofo_ledger_update_balance( ledger_new, currency );
	}
}

static void
action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaEntryPage *self )
{
	insert_new_row( self );
}

/*
 * insert a new entry at the current position
 */
static void
insert_new_row( ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	ofoEntry *entry;

	priv = ofa_entry_page_get_instance_private( self );

	/* set default values that we are able to guess */
	entry = ofo_entry_new( priv->getter );

	if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->ledger_btn ))){
		if( my_strlen( priv->jou_mnemo )){
			ofo_entry_set_ledger( entry, priv->jou_mnemo );
		}
	} else if( my_strlen( priv->acc_number )){
		ofo_entry_set_account( entry, priv->acc_number );
	}

	do_update( self, entry );

#if 0
	GtkTreeIter new_iter;

	/* insert a new row at the end of the list
	 * there is no sens in trying to insert an empty row at a given
	 * position of a sorted list */
	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( priv->store ), &new_iter, -1, ENTRY_COL_OBJECT, entry, -1 );
	//g_signal_emit_by_name( priv->hub, SIGNALER_BASE_NEW, entry );
	g_object_unref( entry );

	/* set the selection and the cursor on this new line */
	ofa_tvbin_select_row( OFA_TVBIN( priv->tview ), &new_iter );

	/* force the edition on this line */
	gtk_switch_set_active( GTK_SWITCH( priv->edit_switch ), TRUE );
#endif
}

static void
action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	GList *selected;
	ofoEntry *entry;

	priv = ofa_entry_page_get_instance_private( self );

	selected = ofa_entry_treeview_get_selected( priv->tview );
	entry = ( ofoEntry * ) selected->data;
	do_update( self, entry );
	ofa_entry_treeview_free_selected( selected );
}

static void
do_update( ofaEntryPage *self, ofoEntry *entry )
{
	ofaEntryPagePrivate *priv;
	GtkWindow *toplevel;

	priv = ofa_entry_page_get_instance_private( self );

	if( entry ){
		toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
		ofa_entry_properties_run( priv->getter, toplevel, entry, priv->editable_row );
	}
}

static void
action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	GtkTreeSelection *selection;

	priv = ofa_entry_page_get_instance_private( self );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( priv->tview ));
	delete_row( self, selection );
}

static void
delete_row( ofaEntryPage *self, GtkTreeSelection *selection )
{
	static const gchar *thisfn = "ofa_entry_page_delete_row";
	ofaEntryPagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter sort_iter;
	ofoEntry *entry;
	GList *entries, *it;

	priv = ofa_entry_page_get_instance_private( self );

	/* editable_row is reset on each selection change
	 * is %TRUE when entry status is rough or future, and edition toggle
	 * switch is on, and dossier is a current exercice which has not been
	 * opened in read-only mode
	 */
	g_return_if_fail( priv->editable_row );

	if( gtk_tree_selection_get_selected( selection, &tmodel, &sort_iter )){
		gtk_tree_model_get( tmodel, &sort_iter, ENTRY_COL_OBJECT, &entry, -1 );
		g_return_if_fail( entry && OFO_IS_ENTRY( entry ));

		if( ofa_entry_page_delconf_run( priv->getter, entry, &entries )){
			/* cleaning up settlement and conciliation is handled by
			 *  #ofoEntry class itself  */
			for( it=entries ; it ; it=it->next ){
				g_debug( "%s: about to delete entry number %lu", thisfn, ofo_entry_get_number( it->data ));
				ofo_entry_delete( OFO_ENTRY( it->data ));
			}
			balances_compute( self );
		}

		g_object_unref( entry );
		g_list_free( entries );
	}
}

/*
 * display the operation
 */
static void
action_on_vope_activated( GSimpleAction *action, GVariant *empty, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;

	priv = ofa_entry_page_get_instance_private( self );

	ofa_operation_group_run( priv->getter, NULL, priv->sel_opes );
}

/*
 * display the reconciliation group
 */
static void
action_on_vconcil_activated( GSimpleAction *action, GVariant *empty, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	GtkWindow *toplevel;

	priv = ofa_entry_page_get_instance_private( self );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_reconcil_group_run( priv->getter, toplevel, priv->sel_concil_id );
}

/*
 * display the settlement group
 */
static void
action_on_vsettle_activated( GSimpleAction *action, GVariant *empty, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	GtkWindow *toplevel;

	priv = ofa_entry_page_get_instance_private( self );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_settlement_group_run( priv->getter, toplevel, priv->sel_settle_id );
}

/*
 * export the content of the treeview
 */
static void
action_on_export_activated( GSimpleAction *action, GVariant *empty, ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	ofaISignaler *signaler;

	priv = ofa_entry_page_get_instance_private( self );

	signaler = ofa_igetter_get_signaler( priv->getter );

	g_signal_emit_by_name( signaler, SIGNALER_EXPORT_ASSISTANT_RUN, OFA_IEXPORTABLE( priv->tview ), TRUE );
}

/*
 * Is the row (+dossier) intrinsically editable (no matter the position
 *  of the 'Edit' switch) ?
 *
 * The row is editable if the entry status is Rough or Future, and the
 * dossier is a current exercice which has not been opened in read-
 * only mode.
 */
static gboolean
row_is_editable( ofaEntryPage *self, GtkTreeSelection *selection )
{
	ofaEntryPagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoEntry *entry;
	gboolean editable;

	priv = ofa_entry_page_get_instance_private( self );

	editable = FALSE;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, ENTRY_COL_OBJECT, &entry, -1 );
		g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
		g_object_unref( entry );

		editable = ofo_entry_is_editable( entry );
		editable &= priv->is_writable;
	}

	return( editable );
}

/*
 * Returns the operation number of the entry
 */
static ofxCounter
row_get_operation_id( ofaEntryPage *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoEntry *entry;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, ENTRY_COL_OBJECT, &entry, -1 );
		g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), 0 );
		g_object_unref( entry );

		return( ofo_entry_get_ope_number( entry ));
	}

	return( 0 );
}

/*
 * Returns the conciliation group id of the entry
 */
static ofxCounter
row_get_concil_id( ofaEntryPage *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoConcil *concil;
	ofoEntry *entry;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, ENTRY_COL_OBJECT, &entry, -1 );
		g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), 0 );
		g_object_unref( entry );

		concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ));
		if( concil ){
			return( ofo_concil_get_id( concil ));
		}
	}

	return( 0 );
}

/*
 * Returns the settlement number of the entry
 */
static ofxCounter
row_get_settlement_id( ofaEntryPage *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoEntry *entry;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, ENTRY_COL_OBJECT, &entry, -1 );
		g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), 0 );
		g_object_unref( entry );

		return( ofo_entry_get_settlement_number( entry ));
	}

	return( 0 );
}

static void
row_display_message( ofaEntryPage *self, GtkTreeSelection *selection )
{
	ofaEntryPagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *msgerr, *msgwarn;
	const gchar *color_str, *text;

	priv = ofa_entry_page_get_instance_private( self );

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, ENTRY_COL_MSGERR, &msgerr, ENTRY_COL_MSGWARN, &msgwarn, -1 );

		if( my_strlen( msgerr )){
			text = msgerr;
			color_str = "labelerror";

		} else if( my_strlen( msgwarn )){
			text = msgwarn;
			color_str = "labelwarning";

		} else {
			text = "";
			color_str = "labelnormal";
		}

		gtk_label_set_text( GTK_LABEL( priv->comment ), text );
		my_style_add( priv->comment, color_str );

		g_free( msgerr );
		g_free( msgwarn );
	}
}

static gint
row_get_errlevel( ofaEntryPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gchar *msgerr, *msgwarn;
	gint err_level;

	gtk_tree_model_get( tmodel, iter, ENTRY_COL_MSGERR, &msgerr, ENTRY_COL_MSGWARN, &msgwarn, -1 );

	if( my_strlen( msgerr )){
		err_level = ENT_ERR_ERROR;

	} else if( my_strlen( msgwarn )){
		err_level = ENT_ERR_WARNING;

	} else {
		err_level = ENT_ERR_NONE;
	}

	g_free( msgerr );
	g_free( msgwarn );

	return( err_level );
}

/*
 * User settings are read during initialization phase,
 * so do not trigger any action.
 */
static void
read_settings( ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	myISettings *settings;

	priv = ofa_entry_page_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );

	read_settings_selection( self, settings );
	read_settings_period_status( self, settings );
}

/*
 * <key>-selection = gen_type; gen_account; gen_ledger; bottom_paned;
 */
static void
read_settings_selection( ofaEntryPage *self, myISettings *settings )
{
	ofaEntryPagePrivate *priv;
	gchar *key;
	GList *strlist, *it;
	const gchar *cstr;
	gint pos;

	priv = ofa_entry_page_get_instance_private( self );

	key = g_strdup_printf( "%s-selection", priv->settings_prefix );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, key );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( !my_collate( cstr, SEL_ACCOUNT )){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->account_btn ), TRUE );
	} else {
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->ledger_btn ), TRUE );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		ofa_ledger_combo_set_selected( priv->ledger_combo, cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		pos = atoi( cstr );
		if( pos < 150 ){
			pos = 150;
		}
		gtk_paned_set_position( GTK_PANED( priv->bottom_paned ), pos );
	}

	my_isettings_free_string_list( settings, strlist );
	g_free( key );
}

/*
 * <key>-status = past; current; future; deleted; rough; validated;
 */
static void
read_settings_period_status( ofaEntryPage *self, myISettings *settings )
{
	ofaEntryPagePrivate *priv;
	gchar *key;
	GList *strlist, *it;
	const gchar *cstr;
	gboolean bval;
	gint count_bvalues;

	priv = ofa_entry_page_get_instance_private( self );

	key = g_strdup_printf( "%s-status", priv->settings_prefix );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, key );
	count_bvalues = 0;

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	bval = my_utils_boolean_from_str( cstr );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->past_btn ), bval );
	count_bvalues += bval ? 1 : 0;

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	bval = my_utils_boolean_from_str( cstr );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->current_btn ), bval );
	count_bvalues += bval ? 1 : 0;

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	bval = my_utils_boolean_from_str( cstr );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->future_btn ), bval );
	count_bvalues += bval ? 1 : 0;

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	bval = my_utils_boolean_from_str( cstr );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->deleted_btn ), bval );
	count_bvalues += bval ? 1 : 0;

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	bval = my_utils_boolean_from_str( cstr );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->rough_btn ), bval );
	count_bvalues += bval ? 1 : 0;

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	bval = my_utils_boolean_from_str( cstr );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->validated_btn ), bval );
	count_bvalues += bval ? 1 : 0;

	if( count_bvalues == 0 ){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->current_btn ), TRUE );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->rough_btn ), TRUE );
	}

	my_isettings_free_string_list( settings, strlist );
	g_free( key );
}

static void
write_settings( ofaEntryPage *self )
{
	ofaEntryPagePrivate *priv;
	myISettings *settings;

	priv = ofa_entry_page_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );

	write_settings_selection( self, settings );
	write_settings_period_status( self, settings );
}

static void
write_settings_selection( ofaEntryPage *self, myISettings *settings )
{
	ofaEntryPagePrivate *priv;
	gchar *key, *str;

	priv = ofa_entry_page_get_instance_private( self );

	str = g_strdup_printf( "%s;%s;%s;%d;",
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->account_btn )) ? SEL_ACCOUNT : SEL_LEDGER,
			my_strlen( priv->acc_number ) ? priv->acc_number : "",
			my_strlen( priv->jou_mnemo ) ? priv->jou_mnemo : "",
			gtk_paned_get_position( GTK_PANED( priv->bottom_paned )));

	key = g_strdup_printf( "%s-selection", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( str );
	g_free( key );
}

static void
write_settings_period_status( ofaEntryPage *self, myISettings *settings )
{
	ofaEntryPagePrivate *priv;
	gchar *key, *str;

	priv = ofa_entry_page_get_instance_private( self );

	str = g_strdup_printf( "%s;%s;%s;%s;%s;%s;",
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->past_btn )) ? "True":"False",
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->current_btn )) ? "True":"False",
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->future_btn )) ? "True":"False",
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->deleted_btn )) ? "True":"False",
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->rough_btn )) ? "True":"False",
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->validated_btn )) ? "True":"False" );

	key = g_strdup_printf( "%s-status", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( str );
	g_free( key );
}

/*
 * ofaEntryStore::ofa-changed signal handler
 *
 * This signal is sent by ofaEntryStore after it has treated an
 * ofaISignaler event.
 */
static void
store_on_changed( ofaEntryStore *store, ofaEntryPage *self )
{
	refresh_display( self );
}

/*
 * ofaEntryStore::ofa-istore-need-refilter
 */
static void
store_on_need_refilter( ofaIStore *store, ofaEntryPage *self )
{
	static const gchar *thisfn = "ofa_entry_page_store_on_need_refilter";

	g_debug( "%s: store=%p, self=%p", thisfn, ( void * ) store, ( void * ) self );

	refresh_display( self );
}
