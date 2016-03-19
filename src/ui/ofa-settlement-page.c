/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-ientry-account.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-itreeview-column.h"
#include "ui/ofa-itreeview-display.h"
#include "ui/ofa-settlement-page.h"

/*
 * ofaEntrySettlementPage:
 */
typedef enum {
	ENT_SETTLEMENT_PAGE_YES = 1,
	ENT_SETTLEMENT_PAGE_NO,
	ENT_SETTLEMENT_PAGE_ALL,
	ENT_SETTLEMENT_PAGE_SESSION
}
	ofaEntrySettlementPage;

/* priv instance data
 */
struct _ofaSettlementPagePrivate {

	/* internals
	 */
	ofaHub            *hub;
	GList             *hub_handlers;
	gchar             *account_number;
	ofoCurrency       *account_currency;
	ofaEntrySettlementPage settlement;

	/* sorting the view
	 */
	gint               sort_column_id;
	gint               sort_sens;
	GtkTreeViewColumn *sort_column;

	/* UI
	 */
	GtkWidget         *top_paned;
	GtkTreeView       *tview;

	/* frame 1: account selection
	 */
	GtkWidget         *account_entry;
	GtkWidget         *account_label;

	/* footer
	 */
	GtkWidget         *footer_label;
	GtkWidget         *debit_balance;
	GtkWidget         *credit_balance;
	GtkWidget         *currency_balance;

	/* actions
	 */
	GtkWidget         *settle_btn;
	GtkWidget         *unsettle_btn;
};

/* columns in the combo box which let us select which type of entries
 * are displayed
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
		{ ENT_SETTLEMENT_PAGE_YES,     N_( "Settled entries" ) },
		{ ENT_SETTLEMENT_PAGE_NO,      N_( "Unsettled entries" ) },
		{ ENT_SETTLEMENT_PAGE_SESSION, N_( "SettlementPage session" ) },
		{ ENT_SETTLEMENT_PAGE_ALL,     N_( "All entries" ) },
		{ 0 }
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
	ENT_COL_SETTLEMENT_PAGE,
	ENT_COL_STATUS,
	ENT_COL_OBJECT,
	ENT_N_COLUMNS
};

/* when enumerating the selected rows
 * this structure is used twice:
 * - each time the selection is updated, in order to update the footer fields
 * - when settling or unsettling the selection
 */
typedef struct {
	ofaSettlementPage *self;
	gint           rows;				/* count of. */
	gint           settled;				/* count of. */
	gint           unsettled;			/* count of. */
	gdouble        debit;				/* sum of debit amounts */
	gdouble        credit;				/* sum of credit amounts */
	gint           set_number;
}
	sEnumSelected;

/* the id of the column is set against sortable columns */
#define DATA_COLUMN_ID                  "ofa-data-column-id"

/* it appears that Gtk+ displays a counter intuitive sort indicator:
 * when asking for ascending sort, Gtk+ displays a 'v' indicator
 * while we would prefer the '^' version -
 * we are defining the inverse indicator, and we are going to sort
 * in reverse order to have our own illusion
 */
#define OFA_SORT_ASCENDING              GTK_SORT_DESCENDING
#define OFA_SORT_DESCENDING             GTK_SORT_ASCENDING

#define COLOR_SETTLED                   "#e0e0e0"		/* light gray background */

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-settlement-page.ui";
static const gchar *st_ui_name          = "SettlementPageWindow";

static const gchar *st_pref_settlement  = "ofaSettlementPage-settings";
static const gchar *st_pref_columns     = "ofaSettlementPage-columns";

static const ofsTreeviewColumnId st_treeview_column_ids[] = {
		{ ENT_COL_DOPE,       ITVC_DOPE },
		{ ENT_COL_DEFF,       ITVC_DEFFECT },
		{ ENT_COL_REF,        ITVC_ENT_REF },
		{ ENT_COL_LEDGER,     ITVC_LED_ID },
		{ ENT_COL_LABEL,      ITVC_ENT_LABEL },
		{ ENT_COL_SETTLEMENT_PAGE, ITVC_STLMT_NUMBER },
		{ ENT_COL_DEBIT,      ITVC_DEBIT },
		{ ENT_COL_CREDIT,     ITVC_CREDIT },
		{ -1 }
};

static void           itreeview_column_iface_init( ofaITreeviewColumnInterface *iface );
static guint          itreeview_column_get_interface_version( const ofaITreeviewColumn *instance );
static void           itreeview_display_iface_init( ofaITreeviewDisplayInterface *iface );
static guint          itreeview_display_get_interface_version( const ofaITreeviewDisplay *instance );
static gchar         *itreeview_display_get_label( const ofaITreeviewDisplay *instance, guint column_id );
static gboolean       itreeview_display_get_def_visible( const ofaITreeviewDisplay *instance, guint column_id );
static void           iaccount_entry_iface_init( ofaIEntryAccountInterface *iface );
static GtkWidget     *v_setup_view( ofaPage *page );
static void           setup_footer( ofaSettlementPage *self, GtkContainer *parent );
static void           setup_entries_treeview( ofaSettlementPage *self, GtkContainer *parent );
static void           setup_account_selection( ofaSettlementPage *self, GtkContainer *parent );
static void           setup_settlement_selection( ofaSettlementPage *self, GtkContainer *parent );
static void           setup_signaling_connect( ofaSettlementPage *self );
static void           on_account_changed( GtkEntry *entry, ofaSettlementPage *self );
static void           on_settlement_changed( GtkComboBox *box, ofaSettlementPage *self );
static gint           on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaSettlementPage *self );
static gint           cmp_strings( ofaSettlementPage *self, const gchar *stra, const gchar *strb );
static gint           cmp_amounts( ofaSettlementPage *self, const gchar *stra, const gchar *strb );
static gint           cmp_counters( ofaSettlementPage *self, const gchar *stra, const gchar *strb );
static gboolean       is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaSettlementPage *self );
static gboolean       is_session_settled( ofaSettlementPage *self, ofoEntry *entry );
static void           on_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaSettlementPage *self );
static void           on_header_clicked( GtkTreeViewColumn *column, ofaSettlementPage *self );
static gboolean       settlement_status_is_valid( ofaSettlementPage *self );
static void           try_display_entries( ofaSettlementPage *self );
static void           display_entries( ofaSettlementPage *self, GList *entries );
static void           display_entry( ofaSettlementPage *self, GtkTreeModel *tmodel, ofoEntry *entry );
static void           set_row_entry( ofaSettlementPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoEntry *entry );
static void           on_entries_treeview_selection_changed( GtkTreeSelection *select, ofaSettlementPage *self );
static void           enum_selected( GtkTreeModel *tmodel, GtkTreePath *path, GtkTreeIter *iter, sEnumSelected *ses );
static void           on_settle_clicked( GtkButton *button, ofaSettlementPage *self );
static void           on_unsettle_clicked( GtkButton *button, ofaSettlementPage *self );
static void           update_selection( ofaSettlementPage *self, gboolean settle );
static void           update_row( GtkTreeModel *tmodel, GtkTreeIter *iter, sEnumSelected *ses );
static void           get_settings( ofaSettlementPage *self );
static void           set_settings( ofaSettlementPage *self );
static void           on_hub_new_object( ofaHub *hub, ofoBase *object, ofaSettlementPage *self );
static void           on_new_entry( ofaSettlementPage *self, ofoEntry *entry );
static void           on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaSettlementPage *self );
static void           on_updated_entry( ofaSettlementPage *self, ofoEntry *entry );
static gboolean       find_entry_by_number( ofaSettlementPage *self, GtkTreeModel *tmodel, ofxCounter number, GtkTreeIter *iter );

G_DEFINE_TYPE_EXTENDED( ofaSettlementPage, ofa_settlement_page, OFA_TYPE_PAGE, 0,
		G_ADD_PRIVATE( ofaSettlementPage )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IENTRY_ACCOUNT, iaccount_entry_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ITREEVIEW_COLUMN, itreeview_column_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ITREEVIEW_DISPLAY, itreeview_display_iface_init ))

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

	g_free( priv->account_number );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_settlement_page_parent_class )->finalize( instance );
}

static void
settlement_page_dispose( GObject *instance )
{
	ofaSettlementPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_SETTLEMENT_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		set_settings( OFA_SETTLEMENT_PAGE( instance ));

		/* unref object members here */
		priv = ofa_settlement_page_get_instance_private( OFA_SETTLEMENT_PAGE( instance ));

		ofa_hub_disconnect_handlers( priv->hub, priv->hub_handlers );
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

	priv->settlement = -1;
	priv->sort_column_id = -1;
	priv->sort_sens = -1;
}

static void
ofa_settlement_page_class_init( ofaSettlementPageClass *klass )
{
	static const gchar *thisfn = "ofa_settlement_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = settlement_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = settlement_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
}

/*
 * ofaITreeviewColumn interface management
 */
static void
itreeview_column_iface_init( ofaITreeviewColumnInterface *iface )
{
	static const gchar *thisfn = "ofa_entry_page_itreeview_column_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = itreeview_column_get_interface_version;
}

static guint
itreeview_column_get_interface_version( const ofaITreeviewColumn *instance )
{
	return( 1 );
}

/*
 * ofaITreeviewDisplay interface management
 */
static void
itreeview_display_iface_init( ofaITreeviewDisplayInterface *iface )
{
	static const gchar *thisfn = "ofa_entry_page_itreeview_display_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = itreeview_display_get_interface_version;
	iface->get_label = itreeview_display_get_label;
	iface->get_def_visible = itreeview_display_get_def_visible;
}

static guint
itreeview_display_get_interface_version( const ofaITreeviewDisplay *instance )
{
	return( 1 );
}

static gchar *
itreeview_display_get_label( const ofaITreeviewDisplay *instance, guint column_id )
{
	return( ofa_itreeview_column_get_menu_label(
					OFA_ITREEVIEW_COLUMN( instance ), column_id, st_treeview_column_ids ));
}

static gboolean
itreeview_display_get_def_visible( const ofaITreeviewDisplay *instance, guint column_id )
{
	gboolean visible;

	switch( column_id ){
		case ENT_COL_ACCOUNT:
			visible = FALSE;
			break;
		case ENT_COL_LEDGER:
			visible = TRUE;
			break;
		default:
			visible = ofa_itreeview_column_get_def_visible(
							OFA_ITREEVIEW_COLUMN( instance ), column_id, st_treeview_column_ids );
			break;
	}
	return( visible );
}

/*
 * ofaIEntryAccount interface management
 */
static void
iaccount_entry_iface_init( ofaIEntryAccountInterface *iface )
{
	static const gchar *thisfn = "ofa_account_filter_vv_bin_iaccount_entry_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_settlement_page_v_setup_view";
	ofaSettlementPagePrivate *priv;
	GtkWidget *page_widget, *widget;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_settlement_page_get_instance_private( OFA_SETTLEMENT_PAGE( page ));

	priv->hub = ofa_page_get_hub( page );
	g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );

	page_widget = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	widget = my_utils_container_attach_from_resource( GTK_CONTAINER( page_widget ), st_resource_ui, st_ui_name, "top" );
	g_return_val_if_fail( widget && GTK_IS_PANED( widget ), NULL );
	priv->top_paned = widget;

	/* build first the targets of the data, and only after the triggers */
	setup_footer( OFA_SETTLEMENT_PAGE( page ), GTK_CONTAINER( widget ));
	setup_entries_treeview( OFA_SETTLEMENT_PAGE( page ), GTK_CONTAINER( widget ));
	setup_settlement_selection( OFA_SETTLEMENT_PAGE( page ), GTK_CONTAINER( widget ));
	setup_account_selection( OFA_SETTLEMENT_PAGE( page ), GTK_CONTAINER( widget ));

	get_settings( OFA_SETTLEMENT_PAGE( page ));

	/* connect to dossier signaling system */
	setup_signaling_connect( OFA_SETTLEMENT_PAGE( page ));

	return( page_widget );
}

static void
setup_footer( ofaSettlementPage *self, GtkContainer *parent )
{
	ofaSettlementPagePrivate *priv;
	GtkWidget *widget;

	priv = ofa_settlement_page_get_instance_private( self );

	widget = my_utils_container_get_child_by_name( parent, "settle-btn" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_settle_clicked ), self );
	priv->settle_btn = widget;

	widget = my_utils_container_get_child_by_name( parent, "unsettle-btn" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_unsettle_clicked ), self );
	priv->unsettle_btn = widget;

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
setup_entries_treeview( ofaSettlementPage *self, GtkContainer *parent )
{
	static const gchar *thisfn = "ofa_settlement_page_setup_entries_treeview";
	ofaSettlementPagePrivate *priv;
	GtkTreeView *tview;
	GtkTreeModel *tmodel, *tfilter, *tsort;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	gint column_id;
	GtkTreeViewColumn *sort_column;

	priv = ofa_settlement_page_get_instance_private( self );

	tview = ( GtkTreeView * ) my_utils_container_get_child_by_name( parent, "treeview" );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			ENT_N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_ULONG,		/* dope, deff, number */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* ref, label, ledger */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* account, debit, credit */
			G_TYPE_STRING, G_TYPE_STRING,					/* settlement, status */
			G_TYPE_OBJECT ));								/* ofoEntry */

	tfilter = gtk_tree_model_filter_new( tmodel, NULL );
	g_object_unref( tmodel );

	gtk_tree_model_filter_set_visible_func(
			GTK_TREE_MODEL_FILTER( tfilter ),
			( GtkTreeModelFilterVisibleFunc ) is_visible_row,
			self,
			NULL );

	tsort = gtk_tree_model_sort_new_with_model( tfilter );
	g_object_unref( tfilter );

	gtk_tree_view_set_model( tview, tsort );
	g_object_unref( tsort );

	g_debug( "%s: tstore=%p, tfilter=%p, tsort=%p",
			thisfn, ( void * ) tmodel, ( void * ) tfilter, ( void * ) tsort );

	/* default is to sort by ascending operation date
	 */
	sort_column = NULL;
	if( priv->sort_column_id < 0 ){
		priv->sort_column_id = ENT_COL_DOPE;
	}
	if( priv->sort_sens < 0 ){
		priv->sort_sens = OFA_SORT_ASCENDING;
	}

	/* operation date
	 */
	column_id = ENT_COL_DOPE;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Operation" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	ofa_itreeview_display_add_column( OFA_ITREEVIEW_DISPLAY( self ), column, column_id );

	/* effect date
	 */
	column_id = ENT_COL_DEFF;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Effect" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	ofa_itreeview_display_add_column( OFA_ITREEVIEW_DISPLAY( self ), column, column_id );

	/* piece's reference
	 */
	column_id = ENT_COL_REF;
	text_cell = gtk_cell_renderer_text_new();
	g_object_set( G_OBJECT( text_cell ), "ellipsize", PANGO_ELLIPSIZE_END, NULL );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Piece" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	ofa_itreeview_display_add_column( OFA_ITREEVIEW_DISPLAY( self ), column, column_id );

	/* ledger
	 */
	column_id = ENT_COL_LEDGER;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Ledger" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	ofa_itreeview_display_add_column( OFA_ITREEVIEW_DISPLAY( self ), column, column_id );

	/* account
	 */

	/* label
	 */
	column_id = ENT_COL_LABEL;
	text_cell = gtk_cell_renderer_text_new();
	g_object_set( G_OBJECT( text_cell ), "ellipsize", PANGO_ELLIPSIZE_END, NULL );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_column_set_resizable( column, TRUE );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	/* debit
	 */
	column_id = ENT_COL_DEBIT;
	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Debit" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_set_min_width( column, 110 );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	/* credit
	 */
	column_id = ENT_COL_CREDIT;
	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Credit" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_set_min_width( column, 110 );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	/* settlement number
	 */
	column_id = ENT_COL_SETTLEMENT_PAGE;
	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
			_( "SettlementPage" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}
	gtk_tree_view_column_set_cell_data_func( column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_MULTIPLE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_entries_treeview_selection_changed ), self );

	/* default is to sort by ascending operation date
	 */
	g_return_if_fail( sort_column && GTK_IS_TREE_VIEW_COLUMN( sort_column ));
	gtk_tree_view_column_set_sort_indicator( sort_column, TRUE );
	priv->sort_column = sort_column;
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tsort ), priv->sort_column_id, priv->sort_sens );

	priv->tview = tview;
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
	ofa_ientry_account_init(
			OFA_IENTRY_ACCOUNT( self ), OFA_MAIN_WINDOW( ofa_page_get_main_window( OFA_PAGE( self ))),
			GTK_ENTRY( widget ), ACCOUNT_ALLOW_SETTLEABLE );
	if( my_strlen( priv->account_number )){
		gtk_entry_set_text( GTK_ENTRY( widget ), priv->account_number );
	}
}

static void
setup_settlement_selection( ofaSettlementPage *self, GtkContainer *parent )
{
	ofaSettlementPagePrivate *priv;
	GtkWidget *combo, *label, *columns;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	gint i, idx;
	GtkTreeIter iter;

	priv = ofa_settlement_page_get_instance_private( self );

	columns = my_utils_container_get_child_by_name( parent, "f2-columns" );
	g_return_if_fail( columns && GTK_IS_CONTAINER( columns ));
	ofa_itreeview_display_attach_menu_button( OFA_ITREEVIEW_DISPLAY( self ), GTK_CONTAINER( columns ));
	//g_signal_connect( self, "icolumns-toggled", G_CALLBACK( on_column_toggled ), NULL );
	ofa_itreeview_display_init_visible( OFA_ITREEVIEW_DISPLAY( self ), st_pref_columns );

	combo = my_utils_container_get_child_by_name( parent, "entries-filter" );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));

	label = my_utils_container_get_child_by_name( parent, "entries-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), combo );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
					SET_N_COLUMNS,
					G_TYPE_INT, G_TYPE_STRING ));
	gtk_combo_box_set_model(GTK_COMBO_BOX( combo ), tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", SET_COL_LABEL );

	idx = -1;

	for( i=0 ; st_settlements[i].code ; ++i ){
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				SET_COL_CODE,  st_settlements[i].code,
				SET_COL_LABEL, gettext( st_settlements[i].label ),
				-1 );
		if( st_settlements[i].code == priv->settlement ){
			idx = i;
		}
	}

	g_signal_connect( combo, "changed", G_CALLBACK( on_settlement_changed ), self );

	if( idx != -1 ){
		gtk_combo_box_set_active( GTK_COMBO_BOX( combo ), idx );
	}
}

static void
setup_signaling_connect( ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	gulong handler;

	priv = ofa_settlement_page_get_instance_private( self );

	handler = g_signal_connect(
			priv->hub, SIGNAL_HUB_NEW, G_CALLBACK( on_hub_new_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect(
			priv->hub, SIGNAL_HUB_UPDATED, G_CALLBACK( on_hub_updated_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );
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

	account = ofo_account_get_by_number( priv->hub, priv->account_number );

	if( account && OFO_IS_ACCOUNT( account ) && !ofo_account_is_root( account )){
		cur_code = ofo_account_get_currency( account );

		if( my_strlen( cur_code )){
			priv->account_currency = ofo_currency_get_by_code( priv->hub, cur_code );
			g_return_if_fail( priv->account_currency && OFO_IS_CURRENCY( priv->account_currency ));
		}

		gtk_label_set_text( GTK_LABEL( priv->account_label ), ofo_account_get_label( account ));
		try_display_entries( self );

	} else {
		gtk_label_set_text( GTK_LABEL( priv->account_label ), "" );
	}

	set_settings( self );
}

static void
on_settlement_changed( GtkComboBox *box, ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	GtkTreeIter iter;
	GtkTreeModel *tmodel, *tsort, *tfilter;

	priv = ofa_settlement_page_get_instance_private( self );

	if( gtk_combo_box_get_active_iter( box, &iter )){
		tmodel = gtk_combo_box_get_model( box );
		gtk_tree_model_get( tmodel, &iter,
				SET_COL_CODE, &priv->settlement,
				-1 );
		tsort = gtk_tree_view_get_model( priv->tview );
		tfilter = gtk_tree_model_sort_get_model( GTK_TREE_MODEL_SORT( tsort ));
		gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( tfilter ));

		set_settings( self );
	}
}

/*
 * sorting the treeview
 *
 * actually sort the content of the store, as the entry itself is only
 * updated when it is about to be saved -
 * the difference is small, but actually exists
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaSettlementPage *self )
{
	static const gchar *thisfn = "ofa_settlement_page_on_sort_model";
	ofaSettlementPagePrivate *priv;
	gint cmp;
	gchar *sdopea, *sdeffa, *srefa, *slabela, *sleda, *sacca, *sdeba, *screa, *sstlmta;
	gchar *sdopeb, *sdeffb, *srefb, *slabelb, *sledb, *saccb, *sdebb, *screb, *sstlmtb;

	gtk_tree_model_get( tmodel, a,
			ENT_COL_DOPE,       &sdopea,
			ENT_COL_DEFF,       &sdeffa,
			ENT_COL_REF,        &srefa,
			ENT_COL_LABEL,      &slabela,
			ENT_COL_LEDGER,     &sleda,
			ENT_COL_ACCOUNT,    &sacca,
			ENT_COL_DEBIT,      &sdeba,
			ENT_COL_CREDIT,     &screa,
			ENT_COL_SETTLEMENT_PAGE, &sstlmta,
			-1 );

	gtk_tree_model_get( tmodel, b,
			ENT_COL_DOPE,       &sdopeb,
			ENT_COL_DEFF,       &sdeffb,
			ENT_COL_REF,        &srefb,
			ENT_COL_LABEL,      &slabelb,
			ENT_COL_LEDGER,     &sledb,
			ENT_COL_ACCOUNT,    &saccb,
			ENT_COL_DEBIT,      &sdebb,
			ENT_COL_CREDIT,     &screb,
			ENT_COL_SETTLEMENT_PAGE, &sstlmtb,
			-1 );

	cmp = 0;

	priv = ofa_settlement_page_get_instance_private( self );

	switch( priv->sort_column_id ){
		case ENT_COL_DOPE:
			cmp = my_date_compare_by_str( sdopea, sdopeb, ofa_prefs_date_display());
			break;
		case ENT_COL_DEFF:
			cmp = my_date_compare_by_str( sdeffa, sdeffb, ofa_prefs_date_display());
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
		case ENT_COL_SETTLEMENT_PAGE:
			cmp = cmp_counters( self, sstlmta, sstlmtb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, priv->sort_column_id );
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
	g_free( sstlmta );
	g_free( sstlmtb );

	/* return -1 if a > b, so that the order indicator points to the smallest:
	 * ^: means from smallest to greatest (ascending order)
	 * v: means from greatest to smallest (descending order)
	 */
	return( -cmp );
}

static gint
cmp_strings( ofaSettlementPage *self, const gchar *stra, const gchar *strb )
{
	return( my_collate( stra, strb ));
}

static gint
cmp_amounts( ofaSettlementPage *self, const gchar *stra, const gchar *strb )
{
	ofxAmount a, b;

	if( stra && strb ){
		a = ofa_amount_from_str( stra );
		b = ofa_amount_from_str( strb );

		return( a < b ? -1 : ( a > b ? 1 : 0 ));
	}

	return( my_collate( stra, strb ));
}

static gint
cmp_counters( ofaSettlementPage *self, const gchar *stra, const gchar *strb )
{
	ofxCounter a, b;

	if( stra && strb ){
		a = atol( stra );
		b = atol( strb );

		return( a < b ? -1 : ( a > b ? 1 : 0 ));
	}

	return( my_collate( stra, strb ));
}

/*
 * a row is visible if it is consistant with the selected settlement status
 */
static gboolean
is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	gboolean visible;
	ofoEntry *entry;
	gint entry_set_number;
	const gchar *ent_account;

	priv = ofa_settlement_page_get_instance_private( self );

	visible = FALSE;

	/* make sure an account is selected */
	if( !my_strlen( priv->account_number )){
		return( FALSE );
	}

	gtk_tree_model_get( tmodel, iter, ENT_COL_OBJECT, &entry, -1 );
	if( entry ){
		g_return_val_if_fail( OFO_IS_ENTRY( entry ), FALSE );
		g_object_unref( entry );

		if( ofo_entry_get_status( entry ) == ENT_STATUS_DELETED ){
			return( FALSE );
		}

		ent_account = ofo_entry_get_account( entry );
		if( g_utf8_collate( ent_account, priv->account_number )){
			return( FALSE );
		}

		entry_set_number = ofo_entry_get_settlement_number( entry );

		switch( priv->settlement ){
			case ENT_SETTLEMENT_PAGE_YES:
				visible = ( entry_set_number > 0 );
				break;
			case ENT_SETTLEMENT_PAGE_NO:
				visible = ( entry_set_number <= 0 );
				break;
			case ENT_SETTLEMENT_PAGE_SESSION:
				if( entry_set_number <= 0 ){
					visible = TRUE;
				} else {
					visible = is_session_settled( self, entry );
				}
				break;
			case ENT_SETTLEMENT_PAGE_ALL:
				visible = TRUE;
				break;
			default:
				break;
		}
	}

	return( visible );
}

static gboolean
is_session_settled( ofaSettlementPage *self, ofoEntry *entry )
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
 * light gray background on settled entries
 */
static void
on_cell_data_func( GtkTreeViewColumn *tcolumn,
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

	gtk_tree_model_get( tmodel, iter, ENT_COL_OBJECT, &entry, -1 );
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
 * Gtk+ changes automatically the sort order
 * we reset yet the sort column id
 *
 * as a side effect of our inversion of indicators, clicking on a new
 * header makes the sort order descending as the default
 */
static void
on_header_clicked( GtkTreeViewColumn *column, ofaSettlementPage *self )
{
	static const gchar *thisfn = "ofa_settlement_page_on_header_clicked";
	ofaSettlementPagePrivate *priv;
	gint sort_column_id, new_column_id;
	GtkSortType sort_order;
	GtkTreeModel *tsort;

	priv = ofa_settlement_page_get_instance_private( self );

	gtk_tree_view_column_set_sort_indicator( priv->sort_column, FALSE );
	gtk_tree_view_column_set_sort_indicator( column, TRUE );
	priv->sort_column = column;

	tsort = gtk_tree_view_get_model( priv->tview );
	gtk_tree_sortable_get_sort_column_id( GTK_TREE_SORTABLE( tsort ), &sort_column_id, &sort_order );

	new_column_id = gtk_tree_view_column_get_sort_column_id( column );
	gtk_tree_sortable_set_sort_column_id( GTK_TREE_SORTABLE( tsort ), new_column_id, sort_order );

	g_debug( "%s: setting new_column_id=%u, new_sort_order=%s",
			thisfn, new_column_id,
			sort_order == OFA_SORT_ASCENDING ? "OFA_SORT_ASCENDING":"OFA_SORT_DESCENDING" );

	priv->sort_column_id = new_column_id;
	priv->sort_sens = sort_order;

	set_settings( self );
}

/*
 * at least a settlement status must be set
 */
static gboolean
settlement_status_is_valid( ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;

	priv = ofa_settlement_page_get_instance_private( self );

	return( priv->settlement >= 1 );
}

static void
try_display_entries( ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	GList *entries;

	priv = ofa_settlement_page_get_instance_private( self );

	if( priv->account_number &&
			settlement_status_is_valid( self ) &&
			priv->tview && GTK_IS_TREE_VIEW( priv->tview )){

		entries = ofo_entry_get_dataset_by_account( priv->hub, priv->account_number );
		display_entries( self, entries );
		ofo_entry_free_dataset( entries );
	}
}

static void
display_entries( ofaSettlementPage *self, GList *entries )
{
	ofaSettlementPagePrivate *priv;
	GtkTreeModel *tsort, *tfilter, *tstore;
	GList *iset;

	priv = ofa_settlement_page_get_instance_private( self );

	tsort = gtk_tree_view_get_model( priv->tview );
	tfilter = gtk_tree_model_sort_get_model( GTK_TREE_MODEL_SORT( tsort ));
	tstore = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( tfilter ));
	gtk_list_store_clear( GTK_LIST_STORE( tstore ));

	for( iset=entries ; iset ; iset=iset->next ){
		display_entry( self, tstore, OFO_ENTRY( iset->data ));
	}
}

static void
display_entry( ofaSettlementPage *self, GtkTreeModel *tmodel, ofoEntry *entry )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( tmodel ), &iter, -1 );
	set_row_entry( self, tmodel, &iter, entry );
}

static void
set_row_entry( ofaSettlementPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoEntry *entry )
{
	ofaSettlementPagePrivate *priv;
	gchar *sdope, *sdeff, *sdeb, *scre, *str_number;
	gdouble amount;
	gint set_number;

	priv = ofa_settlement_page_get_instance_private( self );

	sdope = my_date_to_str( ofo_entry_get_dope( entry ), ofa_prefs_date_display());
	sdeff = my_date_to_str( ofo_entry_get_deffect( entry ), ofa_prefs_date_display());
	amount = ofo_entry_get_debit( entry );
	if( amount ){
		sdeb = ofa_amount_to_str( amount, priv->account_currency );
	} else {
		sdeb = g_strdup( "" );
	}
	amount = ofo_entry_get_credit( entry );
	if( amount ){
		scre = ofa_amount_to_str( amount, priv->account_currency );
	} else {
		scre = g_strdup( "" );
	}
	set_number = ofo_entry_get_settlement_number( entry );
	if( set_number <= 0 ){
		str_number = g_strdup( "" );
	} else {
		str_number = g_strdup_printf( "%u", set_number );
	}

	gtk_list_store_set(
				GTK_LIST_STORE( tmodel ),
				iter,
				ENT_COL_DOPE,       sdope,
				ENT_COL_DEFF,       sdeff,
				ENT_COL_NUMBER,     ofo_entry_get_number( entry ),
				ENT_COL_REF,        ofo_entry_get_ref( entry ),
				ENT_COL_LABEL,      ofo_entry_get_label( entry ),
				ENT_COL_LEDGER,     ofo_entry_get_ledger( entry ),
				ENT_COL_ACCOUNT,    ofo_entry_get_account( entry ),
				ENT_COL_DEBIT,      sdeb,
				ENT_COL_CREDIT,     scre,
				ENT_COL_SETTLEMENT_PAGE, str_number,
				ENT_COL_OBJECT,     entry,
				-1 );

	g_free( str_number );
	g_free( scre );
	g_free( sdeb );
	g_free( sdeff );
	g_free( sdope );
}

/*
 * recompute the balance per currency each time the selection changes
 */
static void
on_entries_treeview_selection_changed( GtkTreeSelection *select, ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	sEnumSelected ses;
	gchar *samount;

	priv = ofa_settlement_page_get_instance_private( self );

	memset( &ses, '\0', sizeof( sEnumSelected ));
	ses.self = self;
	gtk_tree_selection_selected_foreach( select, ( GtkTreeSelectionForeachFunc ) enum_selected, &ses );

	gtk_widget_set_sensitive( priv->settle_btn, ses.unsettled > 0 );
	gtk_widget_set_sensitive( priv->unsettle_btn, ses.settled > 0 );

	my_utils_widget_remove_style(
			priv->footer_label, ses.debit == ses.credit ? "labelwarning" : "labelinfo" );
	my_utils_widget_set_style(
			priv->footer_label, ses.debit == ses.credit ? "labelinfo" : "labelwarning" );

	samount = ofa_amount_to_str( ses.debit, priv->account_currency );
	gtk_label_set_text( GTK_LABEL( priv->debit_balance ), samount );
	g_free( samount );
	my_utils_widget_remove_style(
			priv->debit_balance, ses.debit == ses.credit ? "labelwarning" : "labelinfo" );
	my_utils_widget_set_style(
			priv->debit_balance, ses.debit == ses.credit ? "labelinfo" : "labelwarning" );

	samount = ofa_amount_to_str( ses.credit, priv->account_currency );
	gtk_label_set_text( GTK_LABEL( priv->credit_balance ), samount );
	g_free( samount );
	my_utils_widget_remove_style(
			priv->credit_balance, ses.debit == ses.credit ? "labelwarning" : "labelinfo" );
	my_utils_widget_set_style(
			priv->credit_balance, ses.debit == ses.credit ? "labelinfo" : "labelwarning" );

	gtk_label_set_text( GTK_LABEL( priv->currency_balance ), ofo_currency_get_code( priv->account_currency ));
	my_utils_widget_remove_style(
			priv->currency_balance, ses.debit == ses.credit ? "labelwarning" : "labelinfo" );
	my_utils_widget_set_style(
			priv->currency_balance, ses.debit == ses.credit ? "labelinfo" : "labelwarning" );
}

/*
 * a function called each time the selection is changed, for each selected row
 */
static void
enum_selected( GtkTreeModel *tmodel, GtkTreePath *path, GtkTreeIter *iter, sEnumSelected *ses )
{
	gchar *sdeb, *scre, *snum;
	gdouble amount;
	gint number;

	ses->rows += 1;

	gtk_tree_model_get( tmodel, iter,
			ENT_COL_DEBIT,      &sdeb,
			ENT_COL_CREDIT,     &scre,
			ENT_COL_SETTLEMENT_PAGE, &snum,
			-1 );

	number = atoi( snum );
	if( number > 0 ){
		ses->settled += 1;
	} else {
		ses->unsettled += 1;
	}

	amount = ofa_amount_from_str( sdeb );
	ses->debit += amount;

	amount = ofa_amount_from_str( scre );
	ses->credit += amount;

	g_free( sdeb );
	g_free( scre );
	g_free( snum );
}

static void
on_settle_clicked( GtkButton *button, ofaSettlementPage *self )
{
	update_selection( self, TRUE );
}

static void
on_unsettle_clicked( GtkButton *button, ofaSettlementPage *self )
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
	GtkTreeSelection *select;
	sEnumSelected ses;
	GList *selected_paths, *ipath;
	GtkTreeModel *tsort, *tfilter, *tstore;
	gchar *path_str;
	GtkTreeIter sort_iter, filter_iter, *store_iter;
	GList *list_iters, *it;
	ofoDossier *dossier;

	priv = ofa_settlement_page_get_instance_private( self );

	memset( &ses, '\0', sizeof( sEnumSelected ));
	ses.self = self;
	if( settle ){
		dossier = ofa_hub_get_dossier( priv->hub );
		ses.set_number = ofo_dossier_get_next_settlement( dossier );
	} else {
		ses.set_number = -1;
	}

	select = gtk_tree_view_get_selection( priv->tview );
	selected_paths = gtk_tree_selection_get_selected_rows( select, &tsort );
	tfilter = gtk_tree_model_sort_get_model( GTK_TREE_MODEL_SORT( tsort ));
	tstore = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( tfilter ));
	list_iters = NULL;

	/* convert the list of selected path on the tsort to a list of
	 * selected iters on the underlying tstore */
	for( ipath=selected_paths ; ipath ; ipath=ipath->next ){
		path_str = gtk_tree_path_to_string( ( GtkTreePath * ) ipath->data );
		if( !gtk_tree_model_get_iter_from_string( tsort, &sort_iter, path_str )){
			g_return_if_reached();
		}
		g_free( path_str );
		gtk_tree_model_sort_convert_iter_to_child_iter(
				GTK_TREE_MODEL_SORT( tsort ), &filter_iter, &sort_iter );
		store_iter = g_new0( GtkTreeIter, 1 );
		gtk_tree_model_filter_convert_iter_to_child_iter(
				GTK_TREE_MODEL_FILTER( tfilter ), store_iter, &filter_iter );
		list_iters = g_list_append( list_iters, store_iter );
	}
	g_list_free_full( selected_paths, ( GDestroyNotify ) gtk_tree_path_free );

	/* now update the rows based on this list */
	for( it=list_iters ; it ; it=it->next ){
		update_row( tstore, ( GtkTreeIter * ) it->data, &ses );
	}
	g_list_free_full( list_iters, ( GDestroyNotify ) g_free );

	gtk_widget_set_sensitive( priv->settle_btn, ses.unsettled > 0 );
	gtk_widget_set_sensitive( priv->unsettle_btn, ses.settled > 0 );

	gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( tfilter ));
}

/*
 * enumeration called when we are clicking on 'settle' or 'unsettle'
 * button
 *
 * @tstore: the underlying store tree model
 * @iter: an iter on this model
 */
static void
update_row( GtkTreeModel *tstore, GtkTreeIter *iter, sEnumSelected *ses )
{
	ofoEntry *entry;
	gint number;
	gchar *snum;

	/* get the object and update it, according to the clicked button */
	gtk_tree_model_get( tstore, iter, ENT_COL_OBJECT, &entry, -1 );
	g_object_unref( entry );

	ofo_entry_update_settlement( entry, ses->set_number );

	number = ofo_entry_get_settlement_number( entry );
	if( number > 0 ){
		snum = g_strdup_printf( "%u", number );
	} else {
		snum = g_strdup( "" );
	}

	gtk_list_store_set( GTK_LIST_STORE( tstore ), iter,
				ENT_COL_SETTLEMENT_PAGE, snum,
				-1 );

	g_free( snum );

	/* update counters in the structure */
	enum_selected( tstore, NULL, iter, ses );
}

/*
 * settings: account;mode;sort_column_id;sort_sens;paned_position;
 */
static void
get_settings( ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	GList *slist, *it;
	const gchar *cstr;
	gint pos;

	priv = ofa_settlement_page_get_instance_private( self );

	slist = ofa_settings_user_get_string_list( st_pref_settlement );
	it = slist ? slist : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		g_free( priv->account_number );
		priv->account_number = g_strdup( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->settlement = atoi( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->sort_column_id = atoi( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->sort_sens = atoi( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	pos = cstr ? atoi( cstr ) : 0;
	if( pos <= 10 ){
		pos = 150;
	}
	gtk_paned_set_position( GTK_PANED( priv->top_paned ), pos );

	ofa_settings_free_string_list( slist );
}

static void
set_settings( ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	gchar *str;
	gint pos;

	priv = ofa_settlement_page_get_instance_private( self );

	pos = gtk_paned_get_position( GTK_PANED( priv->top_paned ));

	str = g_strdup_printf( "%s;%d;%d;%d;%d;",
			priv->account_number, priv->settlement, priv->sort_column_id, priv->sort_sens, pos );

	ofa_settings_user_set_string( st_pref_settlement, str );

	g_free( str );
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
on_hub_new_object( ofaHub *hub, ofoBase *object, ofaSettlementPage *self )
{
	static const gchar *thisfn = "ofa_settlement_page_on_hub_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_ENTRY( object )){
		on_new_entry( self, OFO_ENTRY( object ));
	}
}

/*
 * insert the new entry in the list store if it is registered on the
 * currently selected account
 */
static void
on_new_entry( ofaSettlementPage *self, ofoEntry *entry )
{
	ofaSettlementPagePrivate *priv;
	const gchar *entry_account;
	GtkTreeModel *tsort, *tfilter, *tstore;

	priv = ofa_settlement_page_get_instance_private( self );

	entry_account = ofo_entry_get_account( entry );

	if( !g_utf8_collate( priv->account_number, entry_account )){
		tsort = gtk_tree_view_get_model( priv->tview );
		tfilter = gtk_tree_model_sort_get_model( GTK_TREE_MODEL_SORT( tsort ));
		tstore = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( tfilter ));
		display_entry( self, tstore, entry );
		gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( tfilter ));
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaSettlementPage *self )
{
	static const gchar *thisfn = "ofa_settlement_page_on_hub_updated_object";

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, self=%p (%s)",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self, G_OBJECT_TYPE_NAME( self ));

	if( OFO_IS_ENTRY( object )){
		on_updated_entry( self, OFO_ENTRY( object ));
	}
}

static void
on_updated_entry( ofaSettlementPage *self, ofoEntry *entry )
{
	ofaSettlementPagePrivate *priv;
	const gchar *entry_account;
	GtkTreeModel *tsort, *tfilter, *tstore;
	GtkTreeIter iter;

	priv = ofa_settlement_page_get_instance_private( self );

	entry_account = ofo_entry_get_account( entry );

	if( !g_utf8_collate( priv->account_number, entry_account )){
		tsort = gtk_tree_view_get_model( priv->tview );
		tfilter = gtk_tree_model_sort_get_model( GTK_TREE_MODEL_SORT( tsort ));
		tstore = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( tfilter ));
		if( find_entry_by_number( self, tstore, ofo_entry_get_number( entry ), &iter )){
			set_row_entry( self, tstore, &iter, entry );
			gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( tfilter ));

		/* the entry was not present, but may appear depending of the
		 * actual modification */
		} else {
			try_display_entries( self );
		}
	}
}

/*
 * return TRUE if we have found the requested entry row
 */
static gboolean
find_entry_by_number( ofaSettlementPage *self, GtkTreeModel *tmodel, ofxCounter entry_number, GtkTreeIter *iter )
{
	ofxCounter row_number;

	if( gtk_tree_model_get_iter_first( tmodel, iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, iter, ENT_COL_NUMBER, &row_number, -1 );
			if( row_number == entry_number ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( tmodel, iter )){
				break;
			}
		}
	}

	return( FALSE );
}

/**
 * ofa_settlement_page_set_account:
 * @page:
 * @number:
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
