/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#include "my/my-date.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-account-editable.h"
#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "core/ofa-entry-store.h"

#include "ui/ofa-entry-treeview.h"
#include "ui/ofa-settlement-page.h"

/* priv instance data
 */
typedef struct {

	/* runtime
	 */
	ofaIGetter        *getter;
	gchar             *settings_prefix;

	/* UI
	 */
	GtkWidget         *paned;
	ofaEntryTreeview  *tview;
	ofaEntryStore     *store;

	/* frame 1: account selection
	 */
	GtkWidget         *account_entry;
	GtkWidget         *account_label;
	gchar             *account_number;
	ofoCurrency       *account_currency;

	/* frame 2: filtering mode
	 */
	GtkWidget         *filter_combo;
	gchar             *filter_id;

	/* footer
	 */
	GtkWidget         *footer_label;
	GtkWidget         *debit_balance;
	GtkWidget         *credit_balance;
	GtkWidget         *currency_balance;
	const gchar       *last_style;

	/* actions
	 */
	GSimpleAction     *settle_action;
	GSimpleAction     *unsettle_action;
}
	ofaSettlementPagePrivate;

/* filtering the EntryTreeview:
 * - displaying only settled entries
 * - displaying only unsettled ones
 * - displaying all
 * - displaying unsettled + the entries which have been settled this day
 */
enum {
	STLMT_FILTER_YES = 1,
	STLMT_FILTER_NO,
	STLMT_FILTER_ALL,
	STLMT_FILTER_SESSION
};

/* columns in the filtering combo box which let us select which type of
 * entries are displayed
 */
enum {
	SET_COL_CODE = 0,
	SET_COL_LABEL,
	SET_N_COLUMNS
};

typedef struct {
	gint         code;
	const gchar *label;
}
	sSettlementPage;

static const sSettlementPage st_settlements[] = {
		{ STLMT_FILTER_YES,     N_( "Settled entries" ) },
		{ STLMT_FILTER_NO,      N_( "Unsettled entries" ) },
		{ STLMT_FILTER_SESSION, N_( "Settlement session" ) },
		{ STLMT_FILTER_ALL,     N_( "All entries" ) },
		{ 0 }
};

/* when enumerating the selected rows
 * this structure is used twice:
 * - each time the selection is updated, in order to update the footer fields
 * - when settling or unsettling the selection
 */
typedef struct {
	ofaSettlementPage *self;
	gint               rows;				/* count of. */
	gint               settled;				/* count of. */
	gint               unsettled;			/* count of. */
	gdouble            debit;				/* sum of debit amounts */
	gdouble            credit;				/* sum of credit amounts */
	gint               set_number;
}
	sEnumSelected;

#define COLOR_SETTLED                   "#e0e0e0"		/* light gray background */

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-settlement-page.ui";
static const gchar *st_ui_name1         = "SettlementPageView1";
static const gchar *st_ui_name2         = "SettlementPageView2";

static void       paned_page_v_setup_view( ofaPanedPage *page, GtkPaned *paned );
static GtkWidget *setup_view1( ofaSettlementPage *self );
static void       setup_footer( ofaSettlementPage *self, GtkContainer *parent );
static void       setup_treeview( ofaSettlementPage *self, GtkContainer *parent );
static void       tview_on_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaSettlementPage *self );
static gboolean   tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaSettlementPage *self );
static gboolean   tview_is_session_settled( ofaSettlementPage *self, ofoEntry *entry );
static void       tview_on_row_selected( ofaEntryTreeview *view, GList *selected, ofaSettlementPage *self );
static void       tview_enum_selected( ofoEntry *entry, sEnumSelected *ses );
static GtkWidget *setup_view2( ofaSettlementPage *self );
static void       setup_account_selection( ofaSettlementPage *self, GtkContainer *parent );
static void       setup_settlement_selection( ofaSettlementPage *self, GtkContainer *parent );
static void       setup_actions( ofaSettlementPage *self, GtkContainer *parent );
static void       paned_page_v_init_view( ofaPanedPage *page );
static void       on_account_changed( GtkEntry *entry, ofaSettlementPage *self );
static void       on_settlement_changed( GtkComboBox *box, ofaSettlementPage *self );
static void       display_entries( ofaSettlementPage *self );
static void       action_on_settle_activated( GSimpleAction *action, GVariant *empty, ofaSettlementPage *self );
static void       action_on_unsettle_activated( GSimpleAction *action, GVariant *empty, ofaSettlementPage *self );
static void       update_selection( ofaSettlementPage *self, gboolean settle );
static void       update_row( ofoEntry *entry, sEnumSelected *ses );
static void       read_settings( ofaSettlementPage *self );
static void       write_settings( ofaSettlementPage *self );

G_DEFINE_TYPE_EXTENDED( ofaSettlementPage, ofa_settlement_page, OFA_TYPE_PANED_PAGE, 0,
		G_ADD_PRIVATE( ofaSettlementPage ))

static void
settlement_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_settlement_page_finalize";
	ofaSettlementPagePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_SETTLEMENT_PAGE( instance ));

	/* free data members here */
	priv = ofa_settlement_page_get_instance_private( OFA_SETTLEMENT_PAGE( instance ));

	g_free( priv->settings_prefix );
	g_free( priv->account_number );
	g_free( priv->filter_id );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_settlement_page_parent_class )->finalize( instance );
}

static void
settlement_page_dispose( GObject *instance )
{
	ofaSettlementPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_SETTLEMENT_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		write_settings( OFA_SETTLEMENT_PAGE( instance ));

		/* unref object members here */
		priv = ofa_settlement_page_get_instance_private( OFA_SETTLEMENT_PAGE( instance ));

		g_object_unref( priv->store );

		g_object_unref( priv->settle_action );
		g_object_unref( priv->unsettle_action );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_settlement_page_parent_class )->dispose( instance );
}

static void
ofa_settlement_page_init( ofaSettlementPage *self )
{
	static const gchar *thisfn = "ofa_settlement_page_init";
	ofaSettlementPagePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_SETTLEMENT_PAGE( self ));

	priv = ofa_settlement_page_get_instance_private( self );

	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->account_number = NULL;
	priv->filter_id = g_strdup_printf( "%d", STLMT_FILTER_ALL );
	priv->last_style = "labelinvalid";
}

static void
ofa_settlement_page_class_init( ofaSettlementPageClass *klass )
{
	static const gchar *thisfn = "ofa_settlement_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = settlement_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = settlement_page_finalize;

	OFA_PANED_PAGE_CLASS( klass )->setup_view = paned_page_v_setup_view;
	OFA_PANED_PAGE_CLASS( klass )->init_view = paned_page_v_init_view;
}

static void
paned_page_v_setup_view( ofaPanedPage *page, GtkPaned *paned )
{
	static const gchar *thisfn = "ofa_settlement_page_v_setup_view";
	ofaSettlementPagePrivate *priv;
	GtkWidget *view;

	g_debug( "%s: page=%p, paned=%p", thisfn, ( void * ) page, ( void * ) paned );

	priv = ofa_settlement_page_get_instance_private( OFA_SETTLEMENT_PAGE( page ));

	priv->getter = ofa_page_get_getter( OFA_PAGE( page ));

	priv->paned = GTK_WIDGET( paned );

	view = setup_view1( OFA_SETTLEMENT_PAGE( page ));
	gtk_paned_pack1( paned, view, TRUE, FALSE );

	view = setup_view2( OFA_SETTLEMENT_PAGE( page ));
	gtk_paned_pack2( paned, view, FALSE, FALSE );
}

static GtkWidget *
setup_view1( ofaSettlementPage *self )
{
	GtkWidget *box;

	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	my_utils_container_attach_from_resource( GTK_CONTAINER( box ), st_resource_ui, st_ui_name1, "top1" );

	/* build first the targets of the data, and only after the triggers */
	setup_footer( self, GTK_CONTAINER( box ));
	setup_treeview( self, GTK_CONTAINER( box ));

	return( box );
}

static void
setup_footer( ofaSettlementPage *self, GtkContainer *parent )
{
	ofaSettlementPagePrivate *priv;
	GtkWidget *widget;

	priv = ofa_settlement_page_get_instance_private( self );

	widget = my_utils_container_get_child_by_name( parent, "footer-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->footer_label = widget;

	widget = my_utils_container_get_child_by_name( parent, "footer-debit" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->debit_balance = widget;

	widget = my_utils_container_get_child_by_name( parent, "footer-credit" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->credit_balance = widget;

	widget = my_utils_container_get_child_by_name( parent, "footer-currency" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->currency_balance = widget;
}

/*
 * the treeview is filtered on the settlement status
 */
static void
setup_treeview( ofaSettlementPage *self, GtkContainer *parent )
{
	ofaSettlementPagePrivate *priv;
	GtkWidget *tview_parent;

	priv = ofa_settlement_page_get_instance_private( self );

	tview_parent = my_utils_container_get_child_by_name( parent, "entry-treeview" );
	g_return_if_fail( tview_parent && GTK_IS_CONTAINER( tview_parent ));

	priv->tview = ofa_entry_treeview_new( priv->getter );
	gtk_container_add( GTK_CONTAINER( tview_parent ), GTK_WIDGET( priv->tview ));
	ofa_entry_treeview_set_settings_key( priv->tview, priv->settings_prefix );
	ofa_entry_treeview_setup_columns( priv->tview );
	ofa_entry_treeview_set_filter_func( priv->tview, ( GtkTreeModelFilterVisibleFunc ) tview_is_visible_row, self );
	ofa_tvbin_set_cell_data_func( OFA_TVBIN( priv->tview ), ( GtkTreeCellDataFunc ) tview_on_cell_data_func, self );

	/* insertion/delete and activation are not handled here */
	g_signal_connect( priv->tview, "ofa-entchanged", G_CALLBACK( tview_on_row_selected ), self );
}

/*
 * light gray background on settled entries
 */
static void
tview_on_cell_data_func( GtkTreeViewColumn *tcolumn,
						GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
						ofaSettlementPage *self )
{
	ofoEntry *entry;
	ofxCounter number;
	GdkRGBA color;

	g_return_if_fail( GTK_IS_CELL_RENDERER_TEXT( cell ));

	g_object_set( G_OBJECT( cell ),
						"background-set", FALSE,
						NULL );

	gtk_tree_model_get( tmodel, iter, ENTRY_COL_OBJECT, &entry, -1 );
	if( entry ){
		g_return_if_fail( OFO_IS_ENTRY( entry ));
		g_object_unref( entry );

		number = ofo_entry_get_settlement_number( entry );

		if( number > 0 ){
			gdk_rgba_parse( &color, COLOR_SETTLED );
			g_object_set( G_OBJECT( cell ), "background-rgba", &color, NULL );
		}
	}
}

/*
 * a row is visible if it is consistant with the selected settlement status
 */
static gboolean
tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	gboolean visible;
	ofoEntry *entry;
	gint entry_set_number;
	const gchar *ent_account;
	guint filter_int;

	priv = ofa_settlement_page_get_instance_private( self );

	visible = FALSE;

	/* make sure an account is selected */
	if( !my_strlen( priv->account_number )){
		return( FALSE );
	}

	gtk_tree_model_get( tmodel, iter, ENTRY_COL_OBJECT, &entry, -1 );
	if( entry ){
		g_return_val_if_fail( OFO_IS_ENTRY( entry ), FALSE );
		g_object_unref( entry );

		if( ofo_entry_get_status( entry ) == ENT_STATUS_DELETED ){
			return( FALSE );
		}

		ent_account = ofo_entry_get_account( entry );
		if( my_collate( ent_account, priv->account_number )){
			return( FALSE );
		}

		filter_int = atoi( priv->filter_id );
		entry_set_number = ofo_entry_get_settlement_number( entry );

		switch( filter_int ){
			case STLMT_FILTER_YES:
				visible = ( entry_set_number > 0 );
				break;
			case STLMT_FILTER_NO:
				visible = ( entry_set_number <= 0 );
				break;
			case STLMT_FILTER_SESSION:
				if( entry_set_number <= 0 ){
					visible = TRUE;
				} else {
					visible = tview_is_session_settled( self, entry );
				}
				break;
			case STLMT_FILTER_ALL:
				visible = TRUE;
				break;
			default:
				break;
		}
	}

	return( visible );
}

static gboolean
tview_is_session_settled( ofaSettlementPage *self, ofoEntry *entry )
{
	gboolean is_session;
	const GTimeVal *stamp;
	GDate date, dnow;

	stamp = ofo_entry_get_settlement_stamp( entry );
	my_date_set_from_stamp( &date, stamp );
	my_date_set_now( &dnow );
	is_session = my_date_compare( &date, &dnow ) == 0;

	return( is_session );
}

/*
 * recompute the balance per currency each time the selection changes
 */
static void
tview_on_row_selected( ofaEntryTreeview *view, GList *selected, ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	sEnumSelected ses;
	gchar *samount;
	const gchar *code;

	priv = ofa_settlement_page_get_instance_private( self );

	memset( &ses, '\0', sizeof( sEnumSelected ));
	ses.self = self;
	g_list_foreach( selected, ( GFunc ) tview_enum_selected, &ses );

	g_simple_action_set_enabled( priv->settle_action, ses.unsettled > 0 );
	g_simple_action_set_enabled( priv->unsettle_action, ses.settled > 0 );

	if( priv->last_style ){
		my_style_remove( priv->footer_label, priv->last_style );
		my_style_remove( priv->debit_balance, priv->last_style );
		my_style_remove( priv->credit_balance, priv->last_style );
		my_style_remove( priv->currency_balance, priv->last_style );
	}

	samount = priv->account_currency
			? ofa_amount_to_str( ses.debit, priv->account_currency, priv->getter )
			: g_strdup( "" );
	gtk_label_set_text( GTK_LABEL( priv->debit_balance ), samount );
	g_free( samount );

	samount = priv->account_currency
			? ofa_amount_to_str( ses.credit, priv->account_currency, priv->getter )
			: g_strdup( "" );
	gtk_label_set_text( GTK_LABEL( priv->credit_balance ), samount );
	g_free( samount );

	code = priv->account_currency ? ofo_currency_get_code( priv->account_currency ) : "",
	gtk_label_set_text( GTK_LABEL( priv->currency_balance ), code );

	if( ses.rows == 0 ){
		priv->last_style = "labelinvalid";
	} else if( ses.debit == ses.credit ){
		priv->last_style = "labelinfo";
	} else {
		priv->last_style = "labelwarning";
	}

	my_style_add( priv->footer_label, priv->last_style );
	my_style_add( priv->debit_balance, priv->last_style );
	my_style_add( priv->credit_balance, priv->last_style );
	my_style_add( priv->currency_balance, priv->last_style );
}

/*
 * a function called each time the selection is changed, for each selected row
 */
static void
tview_enum_selected( ofoEntry *entry, sEnumSelected *ses )
{
	ofxCounter stlmt_number;
	ofxAmount debit, credit;

	ses->rows += 1;

	stlmt_number = ofo_entry_get_settlement_number( entry );
	debit = ofo_entry_get_debit( entry );
	credit = ofo_entry_get_credit( entry );

	if( stlmt_number > 0 ){
		ses->settled += 1;
	} else {
		ses->unsettled += 1;
	}

	ses->debit += debit;
	ses->credit += credit;
}

static GtkWidget *
setup_view2( ofaSettlementPage *self )
{
	GtkWidget *box;

	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	my_utils_container_attach_from_resource( GTK_CONTAINER( box ), st_resource_ui, st_ui_name2, "top2" );

	setup_settlement_selection( self, GTK_CONTAINER( box ));
	setup_account_selection( self, GTK_CONTAINER( box ));
	setup_actions( self, GTK_CONTAINER( box ));

	return( box );
}

static void
setup_account_selection( ofaSettlementPage *self, GtkContainer *parent )
{
	ofaSettlementPagePrivate *priv;
	GtkWidget *widget;

	priv = ofa_settlement_page_get_instance_private( self );

	/* label must be setup before entry may be changed */
	widget = my_utils_container_get_child_by_name( parent, "account-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->account_label = widget;

	widget = my_utils_container_get_child_by_name( parent, "account-number" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	priv->account_entry = widget;
	g_signal_connect( widget, "changed", G_CALLBACK( on_account_changed ), self );
	ofa_account_editable_init( GTK_EDITABLE( widget ), priv->getter, ACCOUNT_ALLOW_SETTLEABLE );
}

static void
setup_settlement_selection( ofaSettlementPage *self, GtkContainer *parent )
{
	ofaSettlementPagePrivate *priv;
	GtkWidget *combo, *label;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	gint i;
	GtkTreeIter iter;
	gchar *str;

	priv = ofa_settlement_page_get_instance_private( self );

	combo = my_utils_container_get_child_by_name( parent, "entries-filter" );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));
	priv->filter_combo = combo;

	label = my_utils_container_get_child_by_name( parent, "entries-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), combo );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
					SET_N_COLUMNS,
					G_TYPE_STRING, G_TYPE_STRING ));
	gtk_combo_box_set_model( GTK_COMBO_BOX( combo ), tmodel );
	gtk_combo_box_set_id_column( GTK_COMBO_BOX( combo ), SET_COL_CODE );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", SET_COL_LABEL );

	for( i=0 ; st_settlements[i].code ; ++i ){
		str = g_strdup_printf( "%d", st_settlements[i].code );
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				SET_COL_CODE,  str,
				SET_COL_LABEL, gettext( st_settlements[i].label ),
				-1 );
		g_free( str );
	}

	g_signal_connect( combo, "changed", G_CALLBACK( on_settlement_changed ), self );
}

static void
setup_actions( ofaSettlementPage *self, GtkContainer *parent )
{
	ofaSettlementPagePrivate *priv;
	GtkWidget *widget;

	priv = ofa_settlement_page_get_instance_private( self );

	/* settle action */
	priv->settle_action = g_simple_action_new( "settle", NULL );
	g_signal_connect( priv->settle_action, "activate", G_CALLBACK( action_on_settle_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->settle_action ),
			_( "Settle the selection" ));
	widget = my_utils_container_get_child_by_name( parent, "settle-btn" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	ofa_iactionable_set_button(
			OFA_IACTIONABLE( self ), widget, priv->settings_prefix, G_ACTION( priv->settle_action ));

	/* unsettle action */
	priv->unsettle_action = g_simple_action_new( "unsettle", NULL );
	g_signal_connect( priv->unsettle_action, "activate", G_CALLBACK( action_on_unsettle_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->unsettle_action ),
			_( "Unsettle the selection" ));
	widget = my_utils_container_get_child_by_name( parent, "unsettle-btn" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	ofa_iactionable_set_button(
			OFA_IACTIONABLE( self ), widget, priv->settings_prefix, G_ACTION( priv->unsettle_action ));
}

static void
paned_page_v_init_view( ofaPanedPage *page )
{
	static const gchar *thisfn = "ofa_settlement_page_v_init_view";
	ofaSettlementPagePrivate *priv;
	GMenu *menu;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_settlement_page_get_instance_private( OFA_SETTLEMENT_PAGE( page ));

	/* setup contextual menu */
	menu = ofa_iactionable_get_menu( OFA_IACTIONABLE( page ), priv->settings_prefix );
	ofa_icontext_set_menu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( page ),
			menu );

	menu = ofa_itvcolumnable_get_menu( OFA_ITVCOLUMNABLE( priv->tview ));
	ofa_icontext_append_submenu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( priv->tview ),
			OFA_IACTIONABLE_VISIBLE_COLUMNS_ITEM, menu );

	/* install an empty store before setting up the initial values */
	priv->store = ofa_entry_store_new( priv->getter );
	ofa_tvbin_set_store( OFA_TVBIN( priv->tview ), GTK_TREE_MODEL( priv->store ));

	/* as GTK_SELECTION_MULTIPLE is set, we have to explicitely
	 * setup the initial selection if a first row exists */
	ofa_tvbin_select_first_row( OFA_TVBIN( priv->tview ));

	/* setup initial values */
	read_settings( OFA_SETTLEMENT_PAGE( page ));
}

static void
on_account_changed( GtkEntry *entry, ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	ofoAccount *account;
	const gchar *cur_code;

	priv = ofa_settlement_page_get_instance_private( self );

	priv->account_currency = NULL;

	g_free( priv->account_number );
	priv->account_number = g_strdup( gtk_entry_get_text( entry ));

	account = ofo_account_get_by_number( priv->getter, priv->account_number );

	if( account && OFO_IS_ACCOUNT( account ) && !ofo_account_is_root( account )){
		cur_code = ofo_account_get_currency( account );

		if( my_strlen( cur_code )){
			priv->account_currency = ofo_currency_get_by_code( priv->getter, cur_code );
			g_return_if_fail( priv->account_currency && OFO_IS_CURRENCY( priv->account_currency ));
		}

		gtk_label_set_text( GTK_LABEL( priv->account_label ), ofo_account_get_label( account ));
		display_entries( self );

	} else {
		gtk_label_set_text( GTK_LABEL( priv->account_label ), "" );
	}

	ofa_tvbin_refilter( OFA_TVBIN( priv->tview ));
}

static void
on_settlement_changed( GtkComboBox *box, ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	GtkTreeIter iter;
	GtkTreeModel *tmodel;

	priv = ofa_settlement_page_get_instance_private( self );

	if( gtk_combo_box_get_active_iter( box, &iter )){

		g_free( priv->filter_id );
		tmodel = gtk_combo_box_get_model( box );
		gtk_tree_model_get( tmodel, &iter,
				SET_COL_CODE, &priv->filter_id,
				-1 );

		ofa_tvbin_refilter( OFA_TVBIN( priv->tview ));
	}
}

static void
display_entries( ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;

	priv = ofa_settlement_page_get_instance_private( self );

	if( my_strlen( priv->account_number )){
		ofa_entry_store_load( priv->store, priv->account_number, NULL );
	}
}

static void
action_on_settle_activated( GSimpleAction *action, GVariant *empty, ofaSettlementPage *self )
{
	update_selection( self, TRUE );
}

static void
action_on_unsettle_activated( GSimpleAction *action, GVariant *empty, ofaSettlementPage *self )
{
	update_selection( self, FALSE );
}

/*
 * we update here the rows to settled/unsettled
 * due to the GtkTreeModelFilter, this may lead the updated row to
 * disappear from the view -> so update based on GtkListStore iters
 */
static void
update_selection( ofaSettlementPage *self, gboolean settle )
{
	ofaSettlementPagePrivate *priv;
	sEnumSelected ses;
	ofaHub *hub;
	ofoDossier *dossier;
	GList *selected;

	priv = ofa_settlement_page_get_instance_private( self );

	memset( &ses, '\0', sizeof( sEnumSelected ));
	ses.self = self;
	if( settle ){
		hub = ofa_igetter_get_hub( priv->getter );
		dossier = ofa_hub_get_dossier( hub );
		ses.set_number = ofo_dossier_get_next_settlement( dossier );
	} else {
		ses.set_number = -1;
	}

	selected = ofa_entry_treeview_get_selected( priv->tview );
	g_list_foreach( selected, ( GFunc ) update_row, &ses );
	ofa_entry_treeview_free_selected( selected );

	g_simple_action_set_enabled( priv->settle_action, ses.unsettled > 0 );
	g_simple_action_set_enabled( priv->unsettle_action, ses.settled > 0 );

	ofa_tvbin_refilter( OFA_TVBIN( priv->tview ));
}

/*
 * #ofo_entry_update_settlement() triggers a hub signal: the store will
 * so auto-update itself.
 */
static void
update_row( ofoEntry *entry, sEnumSelected *ses )
{
	ofo_entry_update_settlement( entry, ses->set_number );
	tview_enum_selected( entry, ses );
}

/**
 * ofa_settlement_page_set_account:
 * @page: this #ofaSettlementPage instance.
 * @number: the account identifier.
 *
 * Setup an account identifier on the page.
 */
void
ofa_settlement_page_set_account( ofaSettlementPage *page, const gchar *number )
{
	ofaSettlementPagePrivate *priv;

	g_return_if_fail( page && OFA_IS_SETTLEMENT_PAGE( page ));
	g_return_if_fail( !OFA_PAGE( page )->prot->dispose_has_run );

	priv = ofa_settlement_page_get_instance_private( page );

	gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), number );
}

/*
 * settings: mode;account;paned_position;
 *
 * Order is not unimportant: account should be set after the filtering
 * mode; it is so easier to read it in second position.
 *
 * Prevent writing settings when just initializing the data.
 */
static void
read_settings( ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;
	gchar *key;
	guint pos;

	priv = ofa_settlement_page_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, key );

	/* filter mode */
	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_combo_box_set_active_id( GTK_COMBO_BOX( priv->filter_combo ), cstr );
	}

	/* account number */
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), cstr );
	}

	/* paned position */
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	pos = my_strlen( cstr ) ? atoi( cstr ) : 0;
	if( pos < 150 ){
		pos = 150;
	}
	gtk_paned_set_position( GTK_PANED( priv->paned ), pos );

	my_isettings_free_string_list( settings, strlist );
	g_free( key );
}

static void
write_settings( ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	myISettings *settings;
	gchar *str, *key;

	priv = ofa_settlement_page_get_instance_private( self );

	str = g_strdup_printf( "%s;%s;%d;",
			priv->filter_id,
			priv->account_number,
			gtk_paned_get_position( GTK_PANED( priv->paned )));

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( str );
	g_free( key );
}
