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

#define _GNU_SOURCE
#include <glib/gi18n.h>
#include <math.h>
#include <stdlib.h>

#include "my/my-date-editable.h"
#include "my/my-utils.h"

#include "api/ofa-account-editable.h"
#include "api/ofa-amount.h"
#include "api/ofa-date-filter-hv-bin.h"
#include "api/ofa-hub.h"
#include "api/ofa-idate-filter.h"
#include "api/ofa-igetter.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-itheme-manager.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-bat.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-concil.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofs-concil-id.h"

#include "core/ofa-iconcil.h"

#include "ui/ofa-bat-select.h"
#include "ui/ofa-bat-utils.h"
#include "ui/ofa-itreeview-column.h"
#include "ui/ofa-itreeview-display.h"
#include "ui/ofa-reconcil-page.h"
#include "ui/ofa-reconcil-render.h"

/* private instance data
 */
typedef struct {

	/* UI - account
	 */
	GtkWidget           *acc_id_entry;
	GtkWidget           *acc_label;
	GtkWidget           *acc_header_label;
	GtkWidget           *acc_debit_label;
	GtkWidget           *acc_credit_label;
	ofoAccount          *account;
	ofoCurrency         *acc_currency;
	ofxAmount            acc_debit;
	ofxAmount            acc_credit;

	/* UI - filtering mode
	 */
	GtkComboBox         *mode_combo;
	gint                 mode;

	/* UI - effect dates filter
	 */
	ofaDateFilterHVBin  *effect_filter;

	/* UI - manual conciliation
	 */
	GtkEntry            *date_concil;
	GDate                dconcil;

	/* UI - assisted conciliation
	 */
	GtkWidget           *bat_name;
	GtkWidget           *count_label;
	GtkWidget           *unused_label;
	GtkWidget           *label3;
	GtkButton           *clear;

	/* UI - actions
	 */
	GtkWidget           *actions_frame;
	GtkWidget           *reconciliate_btn;
	GtkWidget           *decline_btn;
	GtkWidget           *unreconciliate_btn;
	GtkWidget           *print_btn;

	/* UI - entries view
	 */
	GtkTreeModel        *tstore;		/* the GtkTreeStore */
	GtkTreeModel        *tfilter;		/* GtkTreeModelFilter stacked on the TreeStore */
	GtkTreeModel        *tsort;			/* GtkTreeModelSort stacked on the TreeModelFilter */
	GtkTreeView         *tview;			/* the treeview built on the sorted model */
	GtkTreeViewColumn   *sort_column;
	gint                 activate_action;

	/* UI
	 */
	GtkWidget           *msg_label;
	GtkWidget           *top_paned;

	/* UI - reconciliated balance
	 * this is the balance of the account
	 * with deduction of unreconciliated entries and bat lines
	 */
	GtkWidget           *select_debit;
	GtkWidget           *select_credit;
	GtkWidget           *select_light;
	GtkWidget           *bal_footer_label;
	GtkWidget           *bal_debit_label;
	GtkWidget           *bal_credit_label;

	/* internals
	 */
	ofaHub              *hub;
	GList               *hub_handlers;
	GList               *bats;			/* loaded ofoBat objects */
}
	ofaReconcilPagePrivate;

/* set against the COL_DRECONCIL column to be used in on_cell_data_func() */
#define DATA_COLUMN_ID      "ofa-data-column-id"

/* columns in the combo box which let us select which type of entries
 * are displayed
 */
enum {
	ENT_COL_CODE = 0,
	ENT_COL_LABEL,
	ENT_N_COLUMNS
};

typedef struct {
	gint         code;
	const gchar *label;
}
	sConcil;

/*
 * ofaEntryConcil:
 * this is the conciliation mode: it must be valid for the view be
 * enabled
 */
typedef enum {
	ENT_CONCILED_MIN = 1,
	ENT_CONCILED_YES = ENT_CONCILED_MIN,
	ENT_CONCILED_NO,
	ENT_CONCILED_ALL,
	ENT_CONCILED_SESSION
}
	ofaEntryConcil;

static const sConcil st_concils[] = {
		{ ENT_CONCILED_YES,     N_( "Reconciliated" ) },
		{ ENT_CONCILED_NO,      N_( "Not reconciliated" ) },
		{ ENT_CONCILED_SESSION, N_( "Reconciliation session" ) },
		{ ENT_CONCILED_ALL,     N_( "All" ) },
		{ 0 }
};

/* when activating, and depending of the current selection, the
 * possible action may be conciliate or unconciliate or do nothing
 */
enum {
	ACTIV_NONE = 0,
	ACTIV_CONCILIATE,
	ACTIV_UNCONCILIATE
};

#define COLOR_BAT_CONCIL_FONT                "#008000"	/* middle green */
#define COLOR_BAT_UNCONCIL_FONT              "#00ff00"	/* pure green */

static const gchar *st_reconciliation        = "ofaReconcilPage-settings";
static const gchar *st_pref_columns          = "ReconciliationColumns";
static const gchar *st_effect_dates          = "ReconciliationEffects";
static const gchar *st_resource_ui           = "/org/trychlos/openbook/ui/ofa-reconcil-page.ui";
static const gchar *st_resource_light_green  = "/org/trychlos/openbook/ui/light-green-14.png";
static const gchar *st_resource_light_yellow = "/org/trychlos/openbook/ui/light-yellow-14.png";
static const gchar *st_resource_light_empty  = "/org/trychlos/openbook/ui/light-empty-14.png";
static const gchar *st_ui_name               = "ReconciliationWindow";

/* it appears that Gtk+ displays a counter intuitive sort indicator:
 * when asking for ascending sort, Gtk+ displays a 'v' indicator
 * while we would prefer the '^' version -
 * we are defining the inverse indicator, and we are going to sort
 * in reverse order to have our own illusion
 */
#define OFA_SORT_ASCENDING                   GTK_SORT_DESCENDING
#define OFA_SORT_DESCENDING                  GTK_SORT_ASCENDING

static const gchar *st_default_reconciliated_class = "5"; /* default account class to be reconciliated */

#define DEBUG_FILTER                         FALSE
#define DEBUG_RECONCILIATE                   FALSE
#define DEBUG_UNCONCILIATE                   FALSE

/* column ordering in the reconciliation store
 */
enum {
	COL_ACCOUNT,
	COL_DOPE,
	COL_LEDGER,
	COL_PIECE,
	COL_NUMBER,
	COL_LABEL,
	COL_DEBIT,
	COL_CREDIT,
	COL_IDCONCIL,
	COL_DRECONCIL,
	COL_TYPE,
	COL_OBJECT,				/* may be an ofoEntry or an ofoBatLine
	 	 	 	 	 	 	 * as long as it implements ofaIConcil interface */
	N_COLUMNS
};

static const ofsTreeviewColumnId st_treeview_column_ids[] = {
		{ COL_ACCOUNT,   ITVC_ACC_ID },
		{ COL_DOPE,      ITVC_DOPE },
		{ COL_LEDGER,    ITVC_LED_ID },
		{ COL_PIECE,     ITVC_ENT_REF },
		{ COL_NUMBER,    ITVC_ENT_ID },
		{ COL_LABEL,     ITVC_ENT_LABEL },
		{ COL_DEBIT,     ITVC_DEBIT },
		{ COL_CREDIT,    ITVC_CREDIT },
		{ COL_IDCONCIL,  ITVC_CONCIL_ID },
		{ COL_DRECONCIL, ITVC_CONCIL_DATE },
		{ COL_TYPE,      ITVC_TYPE },
		{ -1 }
};

static void         itreeview_column_iface_init( ofaITreeviewColumnInterface *iface );
static guint        itreeview_column_get_interface_version( const ofaITreeviewColumn *instance );
static void         itreeview_display_iface_init( ofaITreeviewDisplayInterface *iface );
static guint        itreeview_display_get_interface_version( const ofaITreeviewDisplay *instance );
static gchar       *itreeview_display_get_label( const ofaITreeviewDisplay *instance, guint column_id );
static gboolean     itreeview_display_get_def_visible( const ofaITreeviewDisplay *instance, guint column_id );
static GtkWidget   *v_setup_view( ofaPage *page );
static void         setup_treeview_header( ofaReconcilPage *self, GtkContainer *parent );
static void         setup_treeview( ofaReconcilPage *self, GtkContainer *parent );
static void         setup_treeview_footer( ofaReconcilPage *self, GtkContainer *parent );
static void         setup_account_selection( ofaReconcilPage *self, GtkContainer *parent );
static void         setup_entries_filter( ofaReconcilPage *self, GtkContainer *parent );
static void         setup_date_filter( ofaReconcilPage *self, GtkContainer *parent );
static void         setup_manual_rappro( ofaReconcilPage *self, GtkContainer *parent );
static void         setup_size_group( ofaReconcilPage *self, GtkContainer *parent );
static void         setup_auto_rappro( ofaReconcilPage *self, GtkContainer *parent );
static void         setup_action_buttons( ofaReconcilPage *self, GtkContainer *parent );
static GtkWidget   *v_get_top_focusable_widget( const ofaPage *page );
static void         account_on_entry_changed( GtkEntry *entry, ofaReconcilPage *self );
static gchar       *account_on_preselect( GtkEditable *editable, ofeAccountAllowed allowed, ofaReconcilPage *self );
static void         account_clear_content( ofaReconcilPage *self );
static ofoAccount  *account_get_reconciliable( ofaReconcilPage *self );
static void         account_set_header_balance( ofaReconcilPage *self );
static void         do_display_entries( ofaReconcilPage *self );
static void         insert_entry( ofaReconcilPage *self, ofoEntry *entry );
static void         set_row_entry( ofaReconcilPage *self, GtkTreeModel *tstore, GtkTreeIter *iter, ofoEntry *entry );
static void         on_mode_combo_changed( GtkComboBox *box, ofaReconcilPage *self );
static void         select_mode( ofaReconcilPage *self, gint mode );
static void         on_effect_dates_changed( ofaIDateFilter *filter, gint who, gboolean empty, const GDate *date, ofaReconcilPage *self );
static void         on_date_concil_changed( GtkEditable *editable, ofaReconcilPage *self );
static void         on_select_bat( GtkButton *button, ofaReconcilPage *self );
static void         do_select_bat( ofaReconcilPage *self );
static void         bat_on_import_clicked( GtkButton *button, ofaReconcilPage *self );
static void         bat_on_clear_clicked( GtkButton *button, ofaReconcilPage *self );
static void         bat_clear_content( ofaReconcilPage *self );
static void         do_display_bat_files( ofaReconcilPage *self );
static void         display_bat_by_id( ofaReconcilPage *self, ofxCounter bat_id );
static void         display_bat_file( ofaReconcilPage *self, ofoBat *bat );
static void         insert_batline( ofaReconcilPage *self, ofoBatLine *batline );
static void         set_row_batline( ofaReconcilPage *self, GtkTreeModel *store, GtkTreeIter *iter, ofoBatLine *batline );
static void         display_bat_name( ofaReconcilPage *self );
static void         display_bat_counts( ofaReconcilPage *self );
static gint         on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaReconcilPage *self );
static void         on_header_clicked( GtkTreeViewColumn *column, ofaReconcilPage *self );
static gboolean     tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaReconcilPage *self );
static gboolean     tview_is_visible_entry( ofaReconcilPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoEntry *entry );
static gboolean     tview_is_visible_batline( ofaReconcilPage *self, ofoBatLine *batline );
static gboolean     is_session_conciliated( ofaReconcilPage *self, ofoConcil *concil );
static void         on_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaReconcilPage *self );
static gboolean     tview_select_fn( GtkTreeSelection *selection, GtkTreeModel *tmodel, GtkTreePath *path, gboolean is_selected, ofaReconcilPage *self );
static void         on_tview_selection_changed( GtkTreeSelection *select, ofaReconcilPage *self );
static void         examine_selection( ofaReconcilPage *self, ofxAmount *debit, ofxAmount *credit, ofoConcil **concil, guint *count, guint *unconcil_rows, gboolean *unique, gboolean *is_child );
static void         on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaReconcilPage *self );
static gboolean     on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaReconcilPage *self );
static void         collapse_node( ofaReconcilPage *self, GtkWidget *widget );
static void         collapse_node_by_iter( ofaReconcilPage *self, GtkTreeView *tview, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void         expand_node( ofaReconcilPage *self, GtkWidget *widget );
static void         expand_node_by_iter( ofaReconcilPage *self, GtkTreeView *tview, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void         expand_node_by_store_iter( ofaReconcilPage *self, GtkTreeIter *store_iter );
static gboolean     check_for_enable_view( ofaReconcilPage *self );
static void         default_expand_view( ofaReconcilPage *self );
static void         expand_tview_selection( ofaReconcilPage *self );
static void         on_reconciliate_clicked( GtkButton *button, ofaReconcilPage *self );
static void         do_reconciliate( ofaReconcilPage *self );
static gboolean     user_confirm_reconciliation( ofaReconcilPage *self, ofxAmount debit, ofxAmount credit );
static GDate       *get_date_for_new_concil( ofaReconcilPage *self, GDate *date );
static void         on_decline_clicked( GtkButton *button, ofaReconcilPage *self );
static void         do_decline( ofaReconcilPage *self );
static void         on_unreconciliate_clicked( GtkButton *button, ofaReconcilPage *self );
static void         do_unconciliate( ofaReconcilPage *self );
static void         set_reconciliated_balance( ofaReconcilPage *self );
static void         update_concil_data_by_store_iter( ofaReconcilPage *self, GtkTreeIter *store_iter, ofxCounter id, const GDate *dval );
static const GDate *get_bat_line_dope( ofaReconcilPage *self, ofoBatLine *batline );
static void         get_indice_concil_id_by_path( ofaReconcilPage *self, GtkTreeModel *tmodel, GtkTreePath *path, gint *indice, ofxCounter *concil_id );
static GtkTreeIter *search_for_entry_by_number( ofaReconcilPage *self, ofxCounter number );
static gboolean     search_for_parent_by_amount( ofaReconcilPage *self, ofoBase *object, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean     search_for_parent_by_concil( ofaReconcilPage *self, ofoBase *object, GtkTreeModel *tmodel, GtkTreeIter *iter );
static GList       *convert_selected_to_store_refs( ofaReconcilPage *self, GList *selected );
static void         get_settings( ofaReconcilPage *self );
static void         set_settings( ofaReconcilPage *self );
static void         connect_to_hub_signaling_system( ofaReconcilPage *self );
static void         on_hub_new_object( ofaHub *hub, ofoBase *object, ofaReconcilPage *self );
static void         on_new_entry( ofaReconcilPage *self, ofoEntry *entry );
static void         remediate_entry_orphan( ofaReconcilPage *self, ofoEntry *entry );
static void         remediate_orphan( ofaReconcilPage *self, GtkTreeIter *parent_iter );
static void         on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaReconcilPage *self );
static void         on_updated_entry( ofaReconcilPage *self, ofoEntry *entry );
static void         on_hub_deleted_object( ofaHub *hub, ofoBase *object, ofaReconcilPage *self );
static void         on_deleted_entry( ofaReconcilPage *self, ofoEntry *entry );
static void         on_print_clicked( GtkButton *button, ofaReconcilPage *self );
static void         set_message( ofaReconcilPage *page, const gchar *msg );

G_DEFINE_TYPE_EXTENDED( ofaReconcilPage, ofa_reconcil_page, OFA_TYPE_PAGE, 0,
		G_ADD_PRIVATE( ofaReconcilPage )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ITREEVIEW_COLUMN, itreeview_column_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ITREEVIEW_DISPLAY, itreeview_display_iface_init ))

static void
reconciliation_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_reconcil_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( OFA_IS_RECONCIL_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_reconcil_page_parent_class )->finalize( instance );
}

static void
reconciliation_dispose( GObject *instance )
{
	ofaReconcilPagePrivate *priv;

	g_return_if_fail( OFA_IS_RECONCIL_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		set_settings( OFA_RECONCIL_PAGE( instance ));

		/* unref object members here */
		priv = ofa_reconcil_page_get_instance_private( OFA_RECONCIL_PAGE( instance ));

		ofa_hub_disconnect_handlers( priv->hub, &priv->hub_handlers );

		if( priv->bats ){
			g_list_free( priv->bats );
			priv->bats = NULL;
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_reconcil_page_parent_class )->dispose( instance );
}

static void
ofa_reconcil_page_init( ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_init";
	ofaReconcilPagePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( OFA_IS_RECONCIL_PAGE( self ));

	priv = ofa_reconcil_page_get_instance_private( self );

	my_date_clear( &priv->dconcil );
}

static void
ofa_reconcil_page_class_init( ofaReconcilPageClass *klass )
{
	static const gchar *thisfn = "ofa_reconcil_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = reconciliation_dispose;
	G_OBJECT_CLASS( klass )->finalize = reconciliation_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;
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
	return( ofa_itreeview_column_get_def_visible(
					OFA_ITREEVIEW_COLUMN( instance ), column_id, st_treeview_column_ids ));
}

/*
 * grid: the first row contains 'n' columns for selection and filters
 * the second row contains another grid which manages the treeview,
 * along with header and footer
 */
static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_reconcil_page_v_setup_view";
	ofaReconcilPagePrivate *priv;
	GtkWidget *widget, *page_widget;
	GtkTreeSelection *select;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_reconcil_page_get_instance_private( OFA_RECONCIL_PAGE( page ));

	priv->hub = ofa_igetter_get_hub( OFA_IGETTER( page ));
	g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );

	page_widget = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	widget = my_utils_container_attach_from_resource( GTK_CONTAINER( page_widget ), st_resource_ui, st_ui_name, "top" );
	g_return_val_if_fail( widget && GTK_IS_PANED( widget ), NULL );
	priv->top_paned = widget;

	setup_treeview_header( OFA_RECONCIL_PAGE( page ), GTK_CONTAINER( page_widget ));
	setup_treeview( OFA_RECONCIL_PAGE( page ), GTK_CONTAINER( page_widget ));
	setup_treeview_footer( OFA_RECONCIL_PAGE( page ), GTK_CONTAINER( page_widget ));

	setup_account_selection( OFA_RECONCIL_PAGE( page ), GTK_CONTAINER( page_widget ));
	setup_entries_filter( OFA_RECONCIL_PAGE( page ), GTK_CONTAINER( page_widget ));
	setup_date_filter( OFA_RECONCIL_PAGE( page ), GTK_CONTAINER( page_widget ));
	setup_manual_rappro( OFA_RECONCIL_PAGE( page ), GTK_CONTAINER( page_widget ));
	setup_size_group( OFA_RECONCIL_PAGE( page ), GTK_CONTAINER( page_widget ));
	setup_auto_rappro( OFA_RECONCIL_PAGE( page ), GTK_CONTAINER( page_widget ));
	setup_action_buttons( OFA_RECONCIL_PAGE( page ), GTK_CONTAINER( page_widget ));

	connect_to_hub_signaling_system( OFA_RECONCIL_PAGE( page ));

	get_settings( OFA_RECONCIL_PAGE( page ));

	select = gtk_tree_view_get_selection( priv->tview );
	gtk_tree_selection_unselect_all( select );
	on_tview_selection_changed( select, OFA_RECONCIL_PAGE( page ));

	return( page_widget );
}

static void
setup_treeview_header( ofaReconcilPage *self, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;

	priv = ofa_reconcil_page_get_instance_private( self );

	priv->acc_header_label = my_utils_container_get_child_by_name( parent, "header-label" );
	g_return_if_fail( priv->acc_header_label && GTK_IS_LABEL( priv->acc_header_label ));

	priv->acc_debit_label = my_utils_container_get_child_by_name( parent, "header-debit" );
	g_return_if_fail( priv->acc_debit_label && GTK_IS_LABEL( priv->acc_debit_label ));

	priv->acc_credit_label = my_utils_container_get_child_by_name( parent, "header-credit" );
	g_return_if_fail( priv->acc_credit_label && GTK_IS_LABEL( priv->acc_credit_label ));
}

/*
 * The treeview displays both entries and bank account transaction (bat)
 * lines. It is based on a filtered sorted tree store.
 *
 * Entries are 'parent' row. If a bat line is a good candidate to a
 * reconciliation, then it will be displayed as a child of the entry.
 * An entry has zero or one child, never more.
 */
static void
setup_treeview( ofaReconcilPage *self, GtkContainer *parent )
{
	static const gchar *thisfn = "ofa_reconcil_page_setup_treeview";
	ofaReconcilPagePrivate *priv;
	GtkWidget *tview;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	gint column_id;

	priv = ofa_reconcil_page_get_instance_private( self );

	tview = my_utils_container_get_child_by_name( parent, "treeview" );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));
	priv->tview = GTK_TREE_VIEW( tview );

	gtk_tree_view_set_headers_visible( priv->tview, TRUE );
	g_signal_connect( tview, "row-activated", G_CALLBACK( on_row_activated ), self );
	g_signal_connect( tview, "key-press-event", G_CALLBACK( on_key_pressed ), self );

	priv->tstore = GTK_TREE_MODEL( gtk_tree_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING,					/* account, dope */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_ULONG,		/* ledger, piece, number */
			G_TYPE_STRING,									/* label */
			G_TYPE_STRING, G_TYPE_STRING,					/* debit, credit */
			G_TYPE_STRING, G_TYPE_STRING,					/* idconcil, dcreconcil */
			G_TYPE_STRING,									/* type */
			G_TYPE_OBJECT ));

	priv->tfilter = gtk_tree_model_filter_new( priv->tstore, NULL );
	g_object_unref( priv->tstore );
	gtk_tree_model_filter_set_visible_func(
			GTK_TREE_MODEL_FILTER( priv->tfilter ),
			( GtkTreeModelFilterVisibleFunc ) tview_is_visible_row,
			self,
			NULL );

	priv->tsort = gtk_tree_model_sort_new_with_model( priv->tfilter );
	g_object_unref( priv->tfilter );

	gtk_tree_view_set_model( priv->tview, priv->tsort );
	g_object_unref( priv->tsort );

	g_debug( "%s: treestore=%p, tfilter=%p, tsort=%p",
			thisfn, ( void * ) priv->tstore, ( void * ) priv->tfilter, ( void * ) priv->tsort );

	/* account is not displayed */

	/* operation date
	 */
	column_id = COL_DOPE;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Ope." ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_min_width( column, 80 );
	gtk_tree_view_append_column( priv->tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	ofa_itreeview_display_add_column( OFA_ITREEVIEW_DISPLAY( self ), column, column_id );

	/* default is to sort by ascending operation date
	 */
	gtk_tree_view_column_set_sort_indicator( column, TRUE );
	priv->sort_column = column;
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, OFA_SORT_ASCENDING );

	/* ledger
	 */
	column_id = COL_LEDGER;
	text_cell = gtk_cell_renderer_text_new();
	g_object_set( G_OBJECT( text_cell ), "ellipsize", PANGO_ELLIPSIZE_END, NULL );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Ledger" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( priv->tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	ofa_itreeview_display_add_column( OFA_ITREEVIEW_DISPLAY( self ), column, column_id );

	/* piece's reference
	 */
	column_id = COL_PIECE;
	text_cell = gtk_cell_renderer_text_new();
	g_object_set( G_OBJECT( text_cell ), "ellipsize", PANGO_ELLIPSIZE_END, NULL );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Piece" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_min_width( column, 80 );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_append_column( priv->tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	ofa_itreeview_display_add_column( OFA_ITREEVIEW_DISPLAY( self ), column, column_id );

	/* entry number is not displayed */

	/* entry label
	 */
	column_id = COL_LABEL;
	text_cell = gtk_cell_renderer_text_new();
	g_object_set( G_OBJECT( text_cell ), "ellipsize", PANGO_ELLIPSIZE_END, NULL );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_append_column( priv->tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );

	/* debit
	 */
	column_id = COL_DEBIT;
	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_end( column, text_cell, TRUE );
	gtk_tree_view_column_set_title( column, _( "Debit" ));
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_add_attribute( column, text_cell, "text", column_id );
	gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( priv->tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );

	/* credit
	 */
	column_id = COL_CREDIT;
	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_end( column, text_cell, TRUE );
	gtk_tree_view_column_set_title( column, _( "Credit" ));
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_add_attribute( column, text_cell, "text", column_id );
	gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( priv->tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );

	/* conciliation id
	 */
	column_id = COL_IDCONCIL;
	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new();
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_pack_end( column, text_cell, FALSE );
	gtk_tree_view_column_set_alignment( column, 0.5 );
	gtk_tree_view_column_set_title( column, _( "Id." ));
	gtk_tree_view_column_add_attribute( column, text_cell, "text", column_id );
	//gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( priv->tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	ofa_itreeview_display_add_column( OFA_ITREEVIEW_DISPLAY( self ), column, column_id );

	/* reconciliation date
	 */
	column_id = COL_DRECONCIL;
	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 0.0, 0.5 );
	column = gtk_tree_view_column_new();
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_pack_end( column, text_cell, FALSE );
	gtk_tree_view_column_set_alignment( column, 0.5 );
	gtk_tree_view_column_set_title( column, _( "Reconcil." ));
	gtk_tree_view_column_add_attribute( column, text_cell, "text", column_id );
	gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( priv->tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );

	/* type
	 */
	column_id = COL_TYPE;
	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 0.5, 0.5 );
	column = gtk_tree_view_column_new();
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_pack_end( column, text_cell, FALSE );
	gtk_tree_view_column_set_alignment( column, 0.5 );
	gtk_tree_view_column_set_title( column, _( "Type" ));
	gtk_tree_view_column_add_attribute( column, text_cell, "text", column_id );
	gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( priv->tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, self, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( column, "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	ofa_itreeview_display_add_column( OFA_ITREEVIEW_DISPLAY( self ), column, column_id );

	select = gtk_tree_view_get_selection( priv->tview);
	gtk_tree_selection_set_mode( select, GTK_SELECTION_MULTIPLE );
	gtk_tree_selection_set_select_function( select, ( GtkTreeSelectionFunc ) tview_select_fn, self, NULL );
	g_signal_connect( select, "changed", G_CALLBACK( on_tview_selection_changed ), self );

	gtk_widget_set_sensitive( tview, FALSE );
}

/*
 * two widgets (debit/credit) display the bank balance of the
 * account, by deducting the unreconciliated entries from the balance
 * in our book - this is supposed simulate the actual bank balance
 */
static void
setup_treeview_footer( ofaReconcilPage *self, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;

	priv = ofa_reconcil_page_get_instance_private( self );

	priv->msg_label = my_utils_container_get_child_by_name( parent, "footer-msg" );
	g_return_if_fail( priv->msg_label && GTK_IS_LABEL( priv->msg_label ));

	priv->select_debit = my_utils_container_get_child_by_name( parent, "select-debit" );
	g_return_if_fail( priv->select_debit && GTK_IS_LABEL( priv->select_debit ));
	my_utils_widget_set_style( priv->select_debit, "labelhelp" );

	priv->select_credit = my_utils_container_get_child_by_name( parent, "select-credit" );
	g_return_if_fail( priv->select_credit && GTK_IS_LABEL( priv->select_credit ));
	my_utils_widget_set_style( priv->select_credit, "labelhelp" );

	priv->select_light = my_utils_container_get_child_by_name( parent, "select-light" );
	g_return_if_fail( priv->select_light && GTK_IS_IMAGE( priv->select_light ));

	priv->bal_footer_label = my_utils_container_get_child_by_name( parent, "footer-label" );
	g_return_if_fail( priv->bal_footer_label && GTK_IS_LABEL( priv->bal_footer_label ));

	priv->bal_debit_label = my_utils_container_get_child_by_name( parent, "footer-debit" );
	g_return_if_fail( priv->bal_debit_label && GTK_IS_LABEL( priv->bal_debit_label ));

	priv->bal_credit_label = my_utils_container_get_child_by_name( parent, "footer-credit" );
	g_return_if_fail( priv->bal_credit_label && GTK_IS_LABEL( priv->bal_credit_label ));
}

/*
 * account selection is an entry + a select button
 */
static void
setup_account_selection( ofaReconcilPage *self, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;

	priv = ofa_reconcil_page_get_instance_private( self );

	priv->acc_id_entry = my_utils_container_get_child_by_name( parent, "account-number" );
	g_return_if_fail( priv->acc_id_entry && GTK_IS_ENTRY( priv->acc_id_entry ));
	g_signal_connect( priv->acc_id_entry, "changed", G_CALLBACK( account_on_entry_changed ), self );
	ofa_account_editable_init(
			GTK_EDITABLE( priv->acc_id_entry ), OFA_IGETTER( self ), ACCOUNT_ALLOW_RECONCILIABLE );
	ofa_account_editable_set_preselect_cb(
			GTK_EDITABLE( priv->acc_id_entry ), ( AccountPreSelectCb ) account_on_preselect, self );

	priv->acc_label = my_utils_container_get_child_by_name( parent, "account-label" );
	g_return_if_fail( priv->acc_label && GTK_IS_LABEL( priv->acc_label ));
	my_utils_widget_set_style( priv->acc_label, "labelnormal" );
}

/*
 * the combo box for filtering the displayed entries
 */
static void
setup_entries_filter( ofaReconcilPage *self, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;
	GtkWidget *columns, *combo, *label;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeIter iter;
	gint i;

	priv = ofa_reconcil_page_get_instance_private( self );

	columns = my_utils_container_get_child_by_name( parent, "f2-columns" );
	g_return_if_fail( columns && GTK_IS_CONTAINER( columns ));
	ofa_itreeview_display_attach_menu_button( OFA_ITREEVIEW_DISPLAY( self ), GTK_CONTAINER( columns ));
	ofa_itreeview_display_init_visible( OFA_ITREEVIEW_DISPLAY( self ), st_pref_columns );

	combo = my_utils_container_get_child_by_name( parent, "entries-filter" );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));
	priv->mode_combo = GTK_COMBO_BOX( combo );

	label = my_utils_container_get_child_by_name( parent, "entries-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), combo );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new( ENT_N_COLUMNS, G_TYPE_INT, G_TYPE_STRING ));
	gtk_combo_box_set_model( priv->mode_combo, tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->mode_combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->mode_combo ), cell, "text", ENT_COL_LABEL );

	for( i=0 ; st_concils[i].code ; ++i ){
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				ENT_COL_CODE,  st_concils[i].code,
				ENT_COL_LABEL, gettext( st_concils[i].label ),
				-1 );
	}

	g_signal_connect( priv->mode_combo, "changed", G_CALLBACK( on_mode_combo_changed ), self );
}

static void
setup_date_filter( ofaReconcilPage *self, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;
	GtkWidget *filter_parent;

	priv = ofa_reconcil_page_get_instance_private( self );

	priv->effect_filter = ofa_date_filter_hv_bin_new();
	ofa_idate_filter_set_prefs( OFA_IDATE_FILTER( priv->effect_filter ), st_effect_dates );

	filter_parent = my_utils_container_get_child_by_name( parent, "effect-date-filter" );
	g_return_if_fail( filter_parent && GTK_IS_CONTAINER( filter_parent ));
	gtk_container_add( GTK_CONTAINER( filter_parent ), GTK_WIDGET( priv->effect_filter ));

	g_signal_connect(
			priv->effect_filter, "ofa-focus-out", G_CALLBACK( on_effect_dates_changed ), self );
}

static void
setup_manual_rappro( ofaReconcilPage *self, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;
	GtkWidget *entry, *label;

	priv = ofa_reconcil_page_get_instance_private( self );

	entry = my_utils_container_get_child_by_name( parent, "manual-date" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->date_concil = GTK_ENTRY( entry );

	label = my_utils_container_get_child_by_name( parent, "manual-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	label = my_utils_container_get_child_by_name( parent, "manual-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( priv->date_concil ));
	my_date_editable_set_format( GTK_EDITABLE( priv->date_concil ), ofa_prefs_date_display());
	my_date_editable_set_label( GTK_EDITABLE( priv->date_concil ), label, ofa_prefs_date_check());
	my_date_editable_set_date( GTK_EDITABLE( priv->date_concil ), &priv->dconcil );

	g_signal_connect( priv->date_concil, "changed", G_CALLBACK( on_date_concil_changed ), self );
}

/*
 * setup a size group between effect dates filter and manual
 * reconciliation to get the entries aligned
 */
static void
setup_size_group( ofaReconcilPage *self, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;
	GtkSizeGroup *group;
	GtkWidget *label;

	priv = ofa_reconcil_page_get_instance_private( self );

	group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	label = ofa_idate_filter_get_prompt(
			OFA_IDATE_FILTER( priv->effect_filter ), IDATE_FILTER_FROM );
	if( label ){
		gtk_size_group_add_widget( group, label );
	}

	label = my_utils_container_get_child_by_name( parent, "manual-prompt" );
	gtk_size_group_add_widget( group, label );

	g_object_unref( group );
}

static void
setup_auto_rappro( ofaReconcilPage *self, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;
	GtkWidget *button, *label;

	priv = ofa_reconcil_page_get_instance_private( self );

	button = my_utils_container_get_child_by_name( parent, "assist-select" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( button, "clicked", G_CALLBACK( on_select_bat ), self );

	button = my_utils_container_get_child_by_name( parent, "assist-import" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( button, "clicked", G_CALLBACK( bat_on_import_clicked ), self );

	button = my_utils_container_get_child_by_name( parent, "assist-clear" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( button, "clicked", G_CALLBACK( bat_on_clear_clicked ), self );
	priv->clear = GTK_BUTTON( button );

	label = my_utils_container_get_child_by_name( parent, "assist-name" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->bat_name = label;

	label = my_utils_container_get_child_by_name( parent, "assist-count1" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->count_label = label;

	label = my_utils_container_get_child_by_name( parent, "assist-count2" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelbatunconcil" );
	priv->unused_label = label;

	label = my_utils_container_get_child_by_name( parent, "assist-count3" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->label3 = label;
}

static void
setup_action_buttons( ofaReconcilPage *self, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;
	GtkWidget *button;

	priv = ofa_reconcil_page_get_instance_private( self );

	priv->actions_frame = my_utils_container_get_child_by_name( parent, "f6-actions" );
	g_return_if_fail( priv->actions_frame && GTK_IS_FRAME( priv->actions_frame ));

	button = my_utils_container_get_child_by_name( parent, "reconciliate-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( button, "clicked", G_CALLBACK( on_reconciliate_clicked ), self );
	priv->reconciliate_btn = button;

	button = my_utils_container_get_child_by_name( parent, "decline-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( button, "clicked", G_CALLBACK( on_decline_clicked ), self );
	priv->decline_btn = button;

	button = my_utils_container_get_child_by_name( parent, "unreconciliate-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( button, "clicked", G_CALLBACK( on_unreconciliate_clicked ), self );
	priv->unreconciliate_btn = button;

	button = my_utils_container_get_child_by_name( parent, "print-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( button, "clicked", G_CALLBACK( on_print_clicked ), self );
	priv->print_btn = button;
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaReconcilPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_RECONCIL_PAGE( page ), NULL );

	priv = ofa_reconcil_page_get_instance_private( OFA_RECONCIL_PAGE( page ));

	return( GTK_WIDGET( priv->tview ));
}

/*
 * the treeview is disabled (insensitive) while the account is not ok
 * (and priv->account is NULL)
 *
 * Note that @entry may be %NULL, when we are trying to revalidate the
 * entered account identifier after having cleared the loaded BAT files
 * (cf. bat_on_clear_clicked()).
 */
static void
account_on_entry_changed( GtkEntry *entry, ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_account_on_entry_changed";
	ofaReconcilPagePrivate *priv;
	GtkTreeSelection *select;

	priv = ofa_reconcil_page_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->acc_label ), "" );
	priv->account = account_get_reconciliable( self );

	g_debug( "%s: entry=%p, self=%p, account=%p",
			thisfn, ( void * ) entry, ( void * ) self, ( void * ) priv->account );

	if( priv->account ){
		account_clear_content( self );
		account_set_header_balance( self );
		do_display_entries( self );
		set_reconciliated_balance( self );
	}

	check_for_enable_view( self );

	select = gtk_tree_view_get_selection( priv->tview );
	on_tview_selection_changed( select, self );
}

static gchar *
account_on_preselect( GtkEditable *editable, ofeAccountAllowed allowed, ofaReconcilPage *self )
{
	const gchar *text;

	text = gtk_entry_get_text( GTK_ENTRY( editable ));
	if( !my_strlen( text )){
		text = st_default_reconciliated_class;
	}

	return( g_strdup( text ));
}

/*
 *  reset all the account-related content
 * remove all entries from the treeview
 */
static void
account_clear_content( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;

	priv = ofa_reconcil_page_get_instance_private( self );

	priv->acc_currency = NULL;
	priv->acc_debit = 0;
	priv->acc_credit = 0;
	gtk_label_set_text( GTK_LABEL( priv->acc_debit_label ), "" );
	gtk_label_set_text( GTK_LABEL( priv->acc_credit_label ), "" );

	/* clear the store
	 * be lazzy: rather than deleting the entries, just delete all and
	 * reinsert bat lines */
	gtk_tree_store_clear( GTK_TREE_STORE( priv->tstore ));
	gtk_label_set_text( GTK_LABEL( priv->bal_debit_label ), "" );
	gtk_label_set_text( GTK_LABEL( priv->bal_credit_label ), "" );

	/* reinsert bat lines (if any) */
	if( priv->bats ){
		do_display_bat_files( self );
	}
}

/*
 * check that the account addressed by the account entry is valid for
 * a reconciliation session
 */
static ofoAccount *
account_get_reconciliable( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	const gchar *number;
	gboolean ok;
	ofoAccount *account;
	const gchar *label, *bat_account;
	ofoBat *bat;
	gchar *msgerr;

	priv = ofa_reconcil_page_get_instance_private( self );

	ok = FALSE;
	msgerr = NULL;
	number = gtk_entry_get_text( GTK_ENTRY( priv->acc_id_entry ));
	account = ofo_account_get_by_number( priv->hub, number );

	if( account ){
		g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

		ok = !ofo_account_is_root( account ) &&
				!ofo_account_is_closed( account ) &&
				ofo_account_is_reconciliable( account );
	}

	/* if at least one BAT file is loaded, check that this new account
	 * is compatible with these BATs */
	if( ok && priv->bats ){
		bat = ( ofoBat * ) priv->bats->data;
		g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );
		bat_account = ofo_bat_get_account( bat );
		if( my_strlen( bat_account ) && my_collate( bat_account, number )){
			msgerr = g_strdup_printf(
					_( "Selected account %s is not compatible with loaded BAT files which are associated to %s account" ),
					number, bat_account );
			ok = FALSE;
		}
	}

	/* init account label */
	label = account ? ofo_account_get_label( account ) : "";
	gtk_label_set_text( GTK_LABEL( priv->acc_label ), label );
	if( ok ){
		my_utils_widget_remove_style( priv->acc_label, "labelerror" );
	} else {
		my_utils_widget_set_style( priv->acc_label, "labelerror" );
	}

	set_message( self, msgerr );

	return( ok ? account : NULL );
}

/*
 * set the treeview header with the account balance
 * this is called when changing to a valid account
 * or when remediating to an event signaled by through the dossier
 */
static void
account_set_header_balance( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	gchar *sdiff, *samount;
	const gchar *cur_code;

	priv = ofa_reconcil_page_get_instance_private( self );

	if( priv->account ){
		cur_code = ofo_account_get_currency( priv->account );
		g_return_if_fail( my_strlen( cur_code ));

		priv->acc_currency = ofo_currency_get_by_code( priv->hub, cur_code );
		g_return_if_fail( priv->acc_currency && OFO_IS_CURRENCY( priv->acc_currency ));

		priv->acc_debit = ofo_account_get_val_debit( priv->account )
				+ ofo_account_get_rough_debit( priv->account )
				+ ofo_account_get_futur_debit( priv->account );
		priv->acc_credit = ofo_account_get_val_credit( priv->account )
				+ ofo_account_get_rough_credit( priv->account )
				+ ofo_account_get_futur_credit( priv->account );

		if( priv->acc_credit >= priv->acc_debit ){
			sdiff = ofa_amount_to_str( priv->acc_credit - priv->acc_debit, priv->acc_currency );
			samount = g_strdup_printf( _( "%s CR" ), sdiff );
			gtk_label_set_text( GTK_LABEL( priv->acc_credit_label ), samount );

		} else {
			sdiff = ofa_amount_to_str( priv->acc_debit - priv->acc_credit, priv->acc_currency );
			samount = g_strdup_printf( _( "%s DB" ), sdiff );
			gtk_label_set_text( GTK_LABEL( priv->acc_debit_label ), samount );
		}
		g_free( sdiff );
		g_free( samount );

		/* only update user preferences if account is ok */
		set_settings( self );
	}
}

/*
 * insert each entry of the selected account
 * the store is expected to be empty or only contain bat lines
 */
static void
do_display_entries( ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_do_display_entries";
	ofaReconcilPagePrivate *priv;
	GList *entries, *it;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = ofa_reconcil_page_get_instance_private( self );

	entries = ofo_entry_get_dataset_by_account(
					priv->hub, ofo_account_get_number( priv->account ));

	for( it=entries ; it ; it=it->next ){
		insert_entry( self, OFO_ENTRY( it->data ));
	}

	ofo_entry_free_dataset( entries );
}

/*
 * insert the entry:
 * - either as a child of an already inserted conciliation group
 * - or at level zero
 * we do not try here to propose to conciliate an entry against another entry
 * (though it would be theorically possible)
 */
static void
insert_entry( ofaReconcilPage *self, ofoEntry *entry )
{
	ofaReconcilPagePrivate *priv;
	GtkTreeIter parent_iter, insert_iter, child_iter;
	ofoBase *object;

	priv = ofa_reconcil_page_get_instance_private( self );

	if( search_for_parent_by_concil( self, OFO_BASE( entry ), priv->tstore, &parent_iter )){
		gtk_tree_store_insert( GTK_TREE_STORE( priv->tstore ), &insert_iter, &parent_iter, -1 );
		set_row_entry( self, priv->tstore, &insert_iter, entry );

	} else if( 0 && search_for_parent_by_amount( self, OFO_BASE( entry ), priv->tstore, &parent_iter )){
		gtk_tree_model_get( priv->tstore, &parent_iter, COL_OBJECT, &object, -1 );
		g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));

		if( OFO_IS_BAT_LINE( object )){
			gtk_tree_store_remove( GTK_TREE_STORE( priv->tstore ), &parent_iter );
			gtk_tree_store_insert( GTK_TREE_STORE( priv->tstore ), &insert_iter, NULL, -1 );
			set_row_entry( self, priv->tstore, &insert_iter, entry );
			gtk_tree_store_insert( GTK_TREE_STORE( priv->tstore ), &child_iter, &insert_iter, -1 );
			set_row_batline( self, priv->tstore, &child_iter, OFO_BAT_LINE( object ));
		} else {
			gtk_tree_store_insert( GTK_TREE_STORE( priv->tstore ), &insert_iter, &parent_iter, -1 );
			set_row_entry( self, priv->tstore, &insert_iter, entry );
		}
		g_object_unref( object );

	} else {
		gtk_tree_store_insert( GTK_TREE_STORE( priv->tstore ), &insert_iter, NULL, -1 );
		set_row_entry( self, priv->tstore, &insert_iter, entry );
	}
}

static void
set_row_entry( ofaReconcilPage *self, GtkTreeModel *tstore, GtkTreeIter *iter, ofoEntry *entry )
{
	ofaReconcilPagePrivate *priv;
	ofxAmount amount;
	gchar *sdope, *sdeb, *scre, *sdrap, *sid;
	const GDate *dconcil;
	ofoConcil *concil;

	priv = ofa_reconcil_page_get_instance_private( self );

	sdope = my_date_to_str( ofo_entry_get_dope( entry ), ofa_prefs_date_display());
	amount = ofo_entry_get_debit( entry );
	if( amount ){
		sdeb = ofa_amount_to_str( amount, priv->acc_currency );
	} else {
		sdeb = g_strdup( "" );
	}
	amount = ofo_entry_get_credit( entry );
	if( amount ){
		scre = ofa_amount_to_str( amount, priv->acc_currency );
	} else {
		scre = g_strdup( "" );
	}
	concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ));
	dconcil = concil ? ofo_concil_get_dval( concil ) : NULL;
	sdrap = concil ? my_date_to_str( dconcil, ofa_prefs_date_display()) : g_strdup( "" );
	sid = concil ? g_strdup_printf( "%lu", ofo_concil_get_id( concil )) : g_strdup( "" );

	gtk_tree_store_set(
			GTK_TREE_STORE( tstore ),
			iter,
			COL_ACCOUNT,   ofo_entry_get_account( entry ),
			COL_DOPE,      sdope,
			COL_LEDGER,    ofo_entry_get_ledger( entry ),
			COL_PIECE,     ofo_entry_get_ref( entry ),
			COL_NUMBER,    ofo_entry_get_number( entry ),
			COL_LABEL,     ofo_entry_get_label( entry ),
			COL_DEBIT,     sdeb,
			COL_CREDIT,    scre,
			COL_IDCONCIL,  sid,
			COL_DRECONCIL, sdrap,
			COL_TYPE,      ofa_iconcil_get_instance_type( OFA_ICONCIL( entry )),
			COL_OBJECT,    entry,
			-1 );

	g_free( sdope );
	g_free( sdeb );
	g_free( scre );
	g_free( sdrap );
	g_free( sid );
}

static void
on_mode_combo_changed( GtkComboBox *box, ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	GtkTreeIter iter;
	GtkTreeModel *tmodel;

	priv = ofa_reconcil_page_get_instance_private( self );

	priv->mode = -1;

	if( gtk_combo_box_get_active_iter( priv->mode_combo, &iter )){
		tmodel = gtk_combo_box_get_model( priv->mode_combo );
		gtk_tree_model_get( tmodel, &iter, ENT_COL_CODE, &priv->mode, -1 );
	}

	if( check_for_enable_view( self )){
		gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( priv->tfilter ));

		/* only update user preferences if view is enabled */
		set_settings( self );
	}
}

/*
 * called when reading the settings
 */
static void
select_mode( ofaReconcilPage *self, gint mode )
{
	ofaReconcilPagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gint box_mode;

	priv = ofa_reconcil_page_get_instance_private( self );

	tmodel = gtk_combo_box_get_model( priv->mode_combo );
	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, &iter, ENT_COL_CODE, &box_mode, -1 );
			if( box_mode == mode ){
				gtk_combo_box_set_active_iter( priv->mode_combo, &iter );
				break;
			}
			if( !gtk_tree_model_iter_next( tmodel, &iter )){
				break;
			}
		}
	}
}

/*
 * effect dates filter are not stored in settings
 */
static void
on_effect_dates_changed( ofaIDateFilter *filter, gint who, gboolean empty, const GDate *date, ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;

	priv = ofa_reconcil_page_get_instance_private( self );

	gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( priv->tfilter ));
}

/*
 * modifying the manual reconciliation date
 */
static void
on_date_concil_changed( GtkEditable *editable, ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	GDate date;
	gboolean valid;

	priv = ofa_reconcil_page_get_instance_private( self );

	my_date_set_from_date( &date, my_date_editable_get_date( editable, &valid ));
	if( valid ){
		my_date_set_from_date( &priv->dconcil, &date );
	}

	set_settings( self );
}

/*
 * select an already imported Bank Account Transaction list file
 */
static void
on_select_bat( GtkButton *button, ofaReconcilPage *self )
{
	do_select_bat( self );
}

/*
 * Select an already imported Bank Account Transaction list file.
 * Hitting Cancel on BAT selection doesn't change anything
 */
static void
do_select_bat( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	ofxCounter prev_id, bat_id;
	GtkWindow *toplevel;

	priv = ofa_reconcil_page_get_instance_private( self );

	prev_id = priv->bats ? ofo_bat_get_id( OFO_BAT( priv->bats->data )) : -1;
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	bat_id = ofa_bat_select_run( OFA_IGETTER( self ), toplevel, prev_id );
	if( bat_id > 0 ){
		display_bat_by_id( self, bat_id );
	}
}

/*
 * try to import a bank account transaction list
 */
static void
bat_on_import_clicked( GtkButton *button, ofaReconcilPage *self )
{
	ofxCounter imported_id;
	GtkWindow *toplevel;

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	imported_id = ofa_bat_utils_import( OFA_IGETTER( self ), toplevel );
	if( imported_id > 0 ){
		display_bat_by_id( self, imported_id );
	}
}

static void
bat_on_clear_clicked( GtkButton *button, ofaReconcilPage *self )
{
	bat_clear_content( self );
	account_on_entry_changed( NULL, self );
}

/*
 *  clear the proposed reconciliations from the model before displaying
 *  the just imported new ones
 *
 *  this means not only removing old BAT lines, but also resetting the
 *  proposed reconciliation date in the entries
 */
static void
bat_clear_content( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;

	priv = ofa_reconcil_page_get_instance_private( self );

	g_list_free( priv->bats );
	priv->bats = NULL;

	/* also reinit the bat name labels */
	gtk_label_set_text( GTK_LABEL( priv->bat_name ), "" );
	gtk_label_set_text( GTK_LABEL( priv->count_label ), "" );
	gtk_label_set_text( GTK_LABEL( priv->unused_label ), "" );
	gtk_label_set_text( GTK_LABEL( priv->label3 ), "" );

	/* clear the store
	 * be lazzy: rather than deleting the bat lines, just delete all and
	 * reinsert entries */
	gtk_tree_store_clear( GTK_TREE_STORE( priv->tstore ));
	if( priv->account ){
		do_display_entries( self );
	}

	/* and update the bank reconciliated balance */
	set_reconciliated_balance( self );
}

/*
 * re-display all loaded bat files
 * should only be called on a cleared tree store
 */
static void
do_display_bat_files( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	GList *it;

	priv = ofa_reconcil_page_get_instance_private( self );

	for( it=priv->bats ; it ; it=it->next ){
		display_bat_file( self, OFO_BAT( it->data ));
	}
}

/*
 * about to display a newly-imported or a newly-selected BAT file
 * check that it is not already displayed
 */
static void
display_bat_by_id( ofaReconcilPage *self, ofxCounter bat_id )
{
	ofaReconcilPagePrivate *priv;
	ofoBat *bat;
	GList *it;
	const gchar *bat_prev, *account_id, *bat_account;
	gchar *msg;
	GtkWindow *toplevel;

	priv = ofa_reconcil_page_get_instance_private( self );

	bat_prev = NULL;
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));

	for( it=priv->bats ; it ; it=it->next ){
		bat = ( ofoBat * ) it->data;
		if( !bat_prev ){
			bat_prev = ofo_bat_get_account( bat );
		}
		if( ofo_bat_get_id( bat ) == bat_id ){
			my_utils_msg_dialog(
					toplevel, GTK_MESSAGE_WARNING,
					_( "The selected BAT file is already loaded" ));
			return;
		}
	}

	bat = ofo_bat_get_by_id( priv->hub, bat_id );
	if( bat ){
		bat_account = ofo_bat_get_account( bat );
		/* check that this BAT is compatible with an already loaded account */
		if( priv->account ){
			account_id = ofo_account_get_number( priv->account );
			if( my_strlen( bat_account ) && my_collate( account_id, bat_account )){
				msg = g_strdup_printf(
						_( "Selected BAT file is associated with %s account, while current account is %s" ),
						bat_account, account_id );
				my_utils_msg_dialog( toplevel, GTK_MESSAGE_WARNING, msg );
				g_free( msg );
				return;
			}
		}
		/* check that this BAT is compatible with already loaded BATs */
		if( my_strlen( bat_prev ) && my_strlen( bat_account ) && my_collate( bat_prev, bat_account )){
			msg = g_strdup_printf(
					_( "Selected BAT file is associated with %s account which is not compatible with previously loaded BAT files (account=%s)" ),
					bat_account, bat_prev );
			my_utils_msg_dialog( toplevel, GTK_MESSAGE_WARNING, msg );
			g_free( msg );
		}
		priv->bats = g_list_prepend( priv->bats, bat );
		display_bat_file( self, bat );
	}
}

static void
display_bat_file( ofaReconcilPage *self, ofoBat *bat )
{
	ofaReconcilPagePrivate *priv;
	GList *batlines, *it;

	priv = ofa_reconcil_page_get_instance_private( self );

	batlines = ofo_bat_line_get_dataset( priv->hub, ofo_bat_get_id( bat ));
	for( it=batlines ; it ; it=it->next ){
		insert_batline( self, OFO_BAT_LINE( it->data ));
	}
	ofo_bat_line_free_dataset( batlines );
	default_expand_view( self );

	display_bat_name( self );
	display_bat_counts( self );

	set_reconciliated_balance( self );
}

/*
 */
static void
insert_batline( ofaReconcilPage *self, ofoBatLine *batline )
{
	static const gchar *thisfn = "ofa_reconcil_page_insert_batline";
	ofaReconcilPagePrivate *priv;
	GtkTreeIter parent_iter, insert_iter;
	ofoBase *object;

	priv = ofa_reconcil_page_get_instance_private( self );

	if( search_for_parent_by_concil( self, OFO_BASE( batline ), priv->tstore, &parent_iter )){
		gtk_tree_store_insert( GTK_TREE_STORE( priv->tstore ), &insert_iter, &parent_iter, -1 );
		set_row_batline( self, priv->tstore, &insert_iter, batline );

	} else if( search_for_parent_by_amount( self, OFO_BASE( batline ), priv->tstore, &parent_iter )){
		gtk_tree_model_get( priv->tstore, &parent_iter, COL_OBJECT, &object, -1 );
		g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
		g_object_unref( object );
		gtk_tree_store_insert( GTK_TREE_STORE( priv->tstore ), &insert_iter, &parent_iter, -1 );
		set_row_batline( self, priv->tstore, &insert_iter, batline );
		update_concil_data_by_store_iter( self, &parent_iter, 0, ofo_bat_line_get_deffect( batline ));
		if( 0 ){
			g_debug( "%s: insert_with_parent_by_amount: parent=%s, child=%s",
					thisfn,
					OFO_IS_ENTRY( object ) ? ofo_entry_get_label( OFO_ENTRY( object )) : ofo_bat_line_get_label( OFO_BAT_LINE( object )),
					ofo_bat_line_get_label( batline ));
		}
	} else {
		gtk_tree_store_insert( GTK_TREE_STORE( priv->tstore ), &insert_iter, NULL, -1 );
		set_row_batline( self, priv->tstore, &insert_iter, batline );
	}
}

/*
 */
static void
set_row_batline( ofaReconcilPage *self, GtkTreeModel *store, GtkTreeIter *iter, ofoBatLine *batline )
{
	ofaReconcilPagePrivate *priv;
	ofxAmount bat_amount;
	gchar *sdeb, *scre, *sdope, *sdreconcil, *sid;
	ofoConcil *concil;
	const GDate *dope;

	priv = ofa_reconcil_page_get_instance_private( self );

	bat_amount = ofo_bat_line_get_amount( batline );
	if( bat_amount < 0 ){
		sdeb = ofa_amount_to_str( -bat_amount, priv->acc_currency );
		scre = g_strdup( "" );
	} else {
		sdeb = g_strdup( "" );
		scre = ofa_amount_to_str( bat_amount, priv->acc_currency );
	}

	dope = get_bat_line_dope( self, batline );
	sdope = my_date_to_str( dope, ofa_prefs_date_display());
	sid = g_strdup( "" );
	sdreconcil = g_strdup( "" );
	concil = ofa_iconcil_get_concil( OFA_ICONCIL( batline ));
	if( concil ){
		g_free( sid );
		sid = g_strdup_printf( "%lu", ofo_concil_get_id( concil ));
		g_free( sdreconcil );
		sdreconcil = my_date_to_str( ofo_concil_get_dval( concil ), ofa_prefs_date_display());
	}

	gtk_tree_store_set(
				GTK_TREE_STORE( store ),
				iter,
				COL_DOPE,      sdope,
				COL_PIECE,     ofo_bat_line_get_ref( batline ),
				COL_NUMBER,    ofo_bat_line_get_line_id( batline ),
				COL_LABEL,     ofo_bat_line_get_label( batline ),
				COL_DEBIT,     sdeb,
				COL_CREDIT,    scre,
				COL_IDCONCIL,  sid,
				COL_DRECONCIL, sdreconcil,
				COL_TYPE,      ofa_iconcil_get_instance_type( OFA_ICONCIL( batline )),
				COL_OBJECT,    batline,
				-1 );

	g_free( sdreconcil );
	g_free( sdope );
	g_free( sdeb );
	g_free( scre );
	g_free( sid );
}

/*
 * display the uri of the loaded bat file
 * just display 'multiple selection' if appropriate
 */
static void
display_bat_name( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	guint count;
	const gchar *cstr;

	priv = ofa_reconcil_page_get_instance_private( self );

	count = g_list_length( priv->bats );
	if( count == 0 ){
		cstr = "";
	} else if( count == 1 ){
		cstr = ofo_bat_get_uri( OFO_BAT( priv->bats->data ));
	} else {
		cstr = _( "<i>(multiple selection)</i>" );
	}
	gtk_label_set_markup( GTK_LABEL( priv->bat_name ), cstr );
}

static void
display_bat_counts( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	GList *it;
	gchar *str;
	ofoBat *bat;
	gint total, used, bat_used, unused;
	const gchar *bat_account;

	priv = ofa_reconcil_page_get_instance_private( self );

	total = 0;
	used = 0;
	for( it=priv->bats ; it ; it=it->next ){
		bat = OFO_BAT( it->data );
		total += ofo_bat_get_lines_count( bat );
		bat_used = ofo_bat_get_used_count( bat );
		used += bat_used;
		bat_account = ofo_bat_get_account( bat );
		if( bat_used == 0 && my_strlen( bat_account )){
			ofo_bat_set_account( bat, NULL );
			ofo_bat_update( bat );
		}
	}
	unused = total-used;

	str = g_strdup_printf( "(%u-", total );
	gtk_label_set_text( GTK_LABEL( priv->count_label ), str );
	g_free( str );

	str = g_markup_printf_escaped( "<i>%u</i>", unused );
	gtk_label_set_markup( GTK_LABEL( priv->unused_label ), str );
	g_free( str );

	gtk_label_set_text( GTK_LABEL( priv->label3 ), ")" );
}

static ofoBat *
bat_find_loaded_by_id( ofaReconcilPage *self, ofxCounter bat_id )
{
	ofaReconcilPagePrivate *priv;
	GList *it;
	ofoBat *bat;

	priv = ofa_reconcil_page_get_instance_private( self );

	for( it=priv->bats ; it ; it=it->next ){
		bat = ( ofoBat * ) it->data;
		if( ofo_bat_get_id( bat ) == bat_id ){
			return( bat );
		}
	}

	g_return_val_if_reached( NULL );
}

static void
batline_associates_account( ofaReconcilPage *self, ofoBatLine *batline, const gchar *account )
{
	static const gchar *thisfn = "ofa_reconcil_page_batline_associates_account";
	ofxCounter bat_id;
	ofoBat *bat;
	const gchar *bat_account;

	bat_id = ofo_bat_line_get_bat_id( batline );

	bat = bat_find_loaded_by_id( self, bat_id );
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

	bat_account = ofo_bat_get_account( bat );
	/*g_debug( "%s: account=%s, bat_id=%ld, bat_account=%s", thisfn, account, bat_id, bat_account );*/

	if( !my_strlen( bat_account )){
		ofo_bat_set_account( bat, account );
		ofo_bat_update( bat );

	} else if( my_collate( bat_account, account )){
		g_warning( "%s: trying to associate BAT id=%ld to account %s, while already associated to account=%s",
				thisfn, bat_id, account, bat_account );
		g_return_if_reached();
	}
}

/*
 * sorting the treeview
 *
 * sort the visible rows (entries as parent, and bat lines as children)
 * by operation date + entry number (entries only)
 *
 * for bat lines, operation date may be set to effect date (valeur) if
 * not provided in the bat file; entry number is set to zero
 *
 * We are only sorting the root lines of the treeview, but these root
 * lines may be entries or unreconciliated bat lines
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_on_sort_model";
	static const gchar *empty = "";
	ofaReconcilPagePrivate *priv;
	gint cmp, sort_column_id;
	GtkSortType sort_order;
	ofoBase *object_a, *object_b;
	const GDate *date_a, *date_b;
	const gchar *str_a, *str_b;
	gdouble amount, amount_a, amount_b;
	gint int_a, int_b;
	ofoConcil *concil_a, *concil_b;
	ofxCounter id_a, id_b;

	priv = ofa_reconcil_page_get_instance_private( self );

	cmp = 0;

	gtk_tree_model_get( tmodel, a, COL_OBJECT, &object_a, -1 );
	g_return_val_if_fail( object_a && ( OFO_IS_ENTRY( object_a ) || OFO_IS_BAT_LINE( object_a )), 0 );
	g_object_unref( object_a );

	gtk_tree_model_get( tmodel, b, COL_OBJECT, &object_b, -1 );
	g_return_val_if_fail( object_b && ( OFO_IS_ENTRY( object_b ) || OFO_IS_BAT_LINE( object_b )), 0 );
	g_object_unref( object_b );

	gtk_tree_sortable_get_sort_column_id(
			GTK_TREE_SORTABLE( priv->tsort ), &sort_column_id, &sort_order );

	switch( sort_column_id ){
		case COL_DOPE:
			date_a = OFO_IS_ENTRY( object_a ) ?
						ofo_entry_get_dope( OFO_ENTRY( object_a )) :
							get_bat_line_dope( self, OFO_BAT_LINE( object_a ));
			date_b = OFO_IS_ENTRY( object_b ) ?
						ofo_entry_get_dope( OFO_ENTRY( object_b )) :
						get_bat_line_dope( self, OFO_BAT_LINE( object_b ));
			cmp = my_date_compare( date_a, date_b );
			break;
		case COL_LEDGER:
			str_a = OFO_IS_ENTRY( object_a ) ?
						ofo_entry_get_ledger( OFO_ENTRY( object_a )) : empty;
			str_b = OFO_IS_ENTRY( object_b ) ?
						ofo_entry_get_ledger( OFO_ENTRY( object_b )) : empty;
			cmp = my_collate( str_a, str_b );
			break;
		case COL_PIECE:
			str_a = OFO_IS_ENTRY( object_a ) ?
						ofo_entry_get_ref( OFO_ENTRY( object_a )) : empty;
			str_b = OFO_IS_ENTRY( object_b ) ?
						ofo_entry_get_ref( OFO_ENTRY( object_b )) : empty;
			cmp = my_collate( str_a, str_b );
			break;
		case COL_NUMBER:
			int_a = OFO_IS_ENTRY( object_a ) ?
						ofo_entry_get_number( OFO_ENTRY( object_a )) :
							ofo_bat_line_get_line_id( OFO_BAT_LINE( object_a ));
			int_b = OFO_IS_ENTRY( object_b ) ?
						ofo_entry_get_number( OFO_ENTRY( object_b )) :
							ofo_bat_line_get_line_id( OFO_BAT_LINE( object_b ));
			cmp = int_a > int_b ? 1 : ( int_a < int_b ? -1 : 0 );
			break;
		case COL_LABEL:
			str_a = OFO_IS_ENTRY( object_a ) ?
						ofo_entry_get_label( OFO_ENTRY( object_a )) :
							ofo_bat_line_get_label( OFO_BAT_LINE( object_a ));
			str_b = OFO_IS_ENTRY( object_b ) ?
						ofo_entry_get_label( OFO_ENTRY( object_b )) :
							ofo_bat_line_get_label( OFO_BAT_LINE( object_b ));
			cmp = g_utf8_collate( str_a, str_b );
			break;
		case COL_DEBIT:
			if( OFO_IS_BAT_LINE( object_a )){
				amount = ofo_bat_line_get_amount( OFO_BAT_LINE( object_a ));
				amount_a = amount < 0 ? -amount : 0;
			} else {
				amount_a = ofo_entry_get_debit( OFO_ENTRY( object_a ));
			}
			if( OFO_IS_BAT_LINE( object_b )){
				amount = ofo_bat_line_get_amount( OFO_BAT_LINE( object_b ));
				amount_b = amount < 0 ? -amount : 0;
			} else {
				amount_b = ofo_entry_get_debit( OFO_ENTRY( object_b ));
			}
			cmp = amount_a > amount_b ? 1 : ( amount_a < amount_b ? -1 : 0 );
			break;
		case COL_CREDIT:
			if( OFO_IS_BAT_LINE( object_a )){
				amount = ofo_bat_line_get_amount( OFO_BAT_LINE( object_a ));
				amount_a = amount < 0 ? 0 : amount;
			} else {
				amount_a = ofo_entry_get_credit( OFO_ENTRY( object_a ));
			}
			if( OFO_IS_BAT_LINE( object_b )){
				amount = ofo_bat_line_get_amount( OFO_BAT_LINE( object_b ));
				amount_b = amount < 0 ? 0 : amount;
			} else {
				amount_b = ofo_entry_get_credit( OFO_ENTRY( object_b ));
			}
			cmp = amount_a > amount_b ? 1 : ( amount_a < amount_b ? -1 : 0 );
			break;
		case COL_IDCONCIL:
			concil_a = ofa_iconcil_get_concil( OFA_ICONCIL( object_a ));
			id_a = concil_a ? ofo_concil_get_id( concil_a ) : 0;
			concil_b = ofa_iconcil_get_concil( OFA_ICONCIL( object_b ));
			id_b = concil_b ? ofo_concil_get_id( concil_b ) : 0;
			cmp = id_a < id_b ? -1 : ( id_a > id_b ? 1 : 0 );
			break;
		case COL_DRECONCIL:
			concil_a = OFO_IS_ENTRY( object_a ) ?
					ofa_iconcil_get_concil( OFA_ICONCIL( object_a )) : NULL;
			date_a = concil_a ? ofo_concil_get_dval( concil_a ) : NULL;
			concil_b = OFO_IS_ENTRY( object_b ) ?
					ofa_iconcil_get_concil( OFA_ICONCIL( object_b )) : NULL;
			date_b = concil_b ? ofo_concil_get_dval( concil_b ) : NULL;
			g_return_val_if_fail( my_date_is_valid( date_a ), 0 );
			g_return_val_if_fail( my_date_is_valid( date_b ), 0 );
			cmp = my_date_compare( date_a, date_b );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, sort_column_id );
			break;
	}

	/* return -1 if a > b, so that the order indicator points to the smallest:
	 * ^: means from smallest to greatest (ascending order)
	 * v: means from greatest to smallest (descending order)
	 */
	return( -cmp );
}

/*
 * Gtk+ changes automatically the sort order
 * we reset yet the sort column id
 *
 * as a side effect of our inversion of indicators, clicking on a new
 * header makes the sort order descending as the default
 */
static void
on_header_clicked( GtkTreeViewColumn *column, ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_on_header_clicked";
	ofaReconcilPagePrivate *priv;
	gint sort_column_id, new_column_id;
	GtkSortType sort_order;

	priv = ofa_reconcil_page_get_instance_private( self );

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
}

/*
 * a row is visible if it is consistant with the selected mode:
 * - entry: vs. the selected mode
 * - bat line: vs. the reconciliation status:
 *   > reconciliated (and validated): invisible
 *   > not reconciliated (or not validated): visible
 *
 * tmodel here is the main GtkTreeModelSort on which the view is built
 */
static gboolean
tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_tview_is_visible_row";
	ofaReconcilPagePrivate *priv;
	gboolean visible, ok;
	GObject *object;
	const GDate *deffect, *filter;

	priv = ofa_reconcil_page_get_instance_private( self );

	gtk_tree_model_get( tmodel, iter, COL_OBJECT, &object, -1 );
	/* as we insert the row before populating it, it may happen that
	 * the object be not set */
	if( !object ){
		return( FALSE );
	}
	g_return_val_if_fail( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object ), FALSE );
	g_object_unref( object );

	visible = OFO_IS_ENTRY( object ) ?
			tview_is_visible_entry( self, tmodel, iter, OFO_ENTRY( object )) :
			tview_is_visible_batline( self, OFO_BAT_LINE( object ));

	if( DEBUG_FILTER ){
		g_debug( "%s: visible=%s", thisfn, visible ? "True":"False" );
	}

	if( visible ){
		/* check against effect dates filter */
		deffect = OFO_IS_ENTRY( object ) ?
				ofo_entry_get_deffect( OFO_ENTRY( object )) :
				ofo_bat_line_get_deffect( OFO_BAT_LINE( object ));
		g_return_val_if_fail( my_date_is_valid( deffect ), FALSE );
		/* ... against lower limit */
		filter = ofa_idate_filter_get_date(
						OFA_IDATE_FILTER( priv->effect_filter ), IDATE_FILTER_FROM );
		ok = !my_date_is_valid( filter ) ||
				my_date_compare( filter, deffect ) <= 0;
		visible &= ok;
		if( DEBUG_FILTER ){
			g_debug( "%s: check effect date against lower limit: ok=%s, visible=%s",
					thisfn, ok ? "True":"False", visible ? "True":"False" );
		}
		/* ... against upper limit */
		filter = ofa_idate_filter_get_date(
						OFA_IDATE_FILTER( priv->effect_filter ), IDATE_FILTER_TO );
		ok = !my_date_is_valid( filter ) ||
				my_date_compare( filter, deffect ) >= 0;
		visible &= ok;
		if( DEBUG_FILTER ){
			g_debug( "%s: check effect date against upper limit: ok=%s, visible=%s",
					thisfn, ok ? "True":"False", visible ? "True":"False" );
		}
	}

	if( DEBUG_FILTER ){
		g_debug( "%s: returning visible=%s", thisfn, visible ? "True":"False" );
	}
	return( visible );
}

static gboolean
tview_is_visible_entry( ofaReconcilPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoEntry *entry )
{
	static const gchar *thisfn = "ofa_reconcil_page_tview_is_visible_entry";
	ofaReconcilPagePrivate *priv;
	gboolean visible;
	ofoConcil *concil;
	const gchar *selected_account, *entry_account;

	priv = ofa_reconcil_page_get_instance_private( self );

	if( DEBUG_FILTER ){
		gchar *sdeb = ofa_amount_to_str( ofo_entry_get_debit( entry ), priv->acc_currency );
		gchar *scre = ofa_amount_to_str( ofo_entry_get_credit( entry ), priv->acc_currency );
		g_debug( "%s: entry=%s, debit=%s, credit=%s",
				thisfn, ofo_entry_get_label( entry ), sdeb, scre );
		g_free( sdeb );
		g_free( scre );
	}

	/* do not display deleted entries */
	if( ofo_entry_get_status( entry ) == ENT_STATUS_DELETED ){
		if( DEBUG_FILTER ){
			g_debug( "%s: entry is deleted", thisfn );
		}
		return( FALSE );
	}

	/* check account is right
	 * do not rely on the initial dataset query as we may have inserted
	 *  a new entry */
	selected_account = gtk_entry_get_text( GTK_ENTRY( priv->acc_id_entry ));
	entry_account = ofo_entry_get_account( entry );
	if( g_utf8_collate( selected_account, entry_account )){
		if( DEBUG_FILTER ){
			g_debug( "%s: selected_account=%s, entry_account=%s",
					thisfn, selected_account, entry_account );
		}
		return( FALSE );
	}

	concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ));
	if( DEBUG_FILTER ){
		g_debug( "%s: concil=%p, id=%ld",
				thisfn, ( void * ) concil, concil ? ofo_concil_get_id( concil ) : 0 );
	}

	visible = TRUE;

	switch( priv->mode ){
		case ENT_CONCILED_ALL:
			break;
		case ENT_CONCILED_YES:
			visible = ( concil != NULL );
			break;
		case ENT_CONCILED_NO:
			visible = ( concil == NULL );
			break;
		case ENT_CONCILED_SESSION:
			if( concil ){
				visible = is_session_conciliated( self, concil );
				if( DEBUG_FILTER ){
					g_debug( "%s: is_session_conciliated=%s",
							thisfn, visible ? "True":"False" );
				}
			} else {
				visible = TRUE;
			}
			break;
		default:
			/* when display mode is not set */
			break;
	}

	return( visible );
}

/*
 * bat lines are visible with the same criteria that the entries
 * even when reconciliated, it keeps to be displayed besides of its
 * entry
 */
static gboolean
tview_is_visible_batline( ofaReconcilPage *self, ofoBatLine *batline )
{
	static const gchar *thisfn = "ofa_reconcil_page_tview_is_visible_batline";
	ofaReconcilPagePrivate *priv;
	gboolean visible;
	ofoConcil *concil;

	priv = ofa_reconcil_page_get_instance_private( self );

	if( DEBUG_FILTER ){
		gchar *samount = ofa_amount_to_str( ofo_bat_line_get_amount( batline ), priv->acc_currency );
		g_debug( "%s: batline=%s, amount=%s", thisfn, ofo_bat_line_get_label( batline ), samount );
		g_free( samount );
	}

	concil = ofa_iconcil_get_concil( OFA_ICONCIL( batline ));
	if( DEBUG_FILTER ){
		g_debug( "%s: concil=%p, id=%ld",
				thisfn, ( void * ) concil, concil ? ofo_concil_get_id( concil ) : 0 );
	}

	visible = TRUE;

	switch( priv->mode ){
		case ENT_CONCILED_ALL:
			visible = TRUE;
			break;
		case ENT_CONCILED_YES:
			visible = ( concil != NULL );
			break;
		case ENT_CONCILED_NO:
			visible = ( concil == NULL );
			break;
		case ENT_CONCILED_SESSION:
			if( !concil ){
				visible = TRUE;
			} else {
				visible = is_session_conciliated( self, concil );
				if( DEBUG_FILTER ){
					g_debug( "%s: is_session_conciliated=%s",
							thisfn, visible ? "True":"False" );
				}
			}
			break;
		default:
			/* when display mode is not set */
			break;
	}

	return( visible );
}

/*
 * conciliated during this day session ?
 */
static gboolean
is_session_conciliated( ofaReconcilPage *self, ofoConcil *concil )
{
	gboolean is_session;
	const GTimeVal *stamp;
	GDate date, dnow;

	g_return_val_if_fail( concil && OFO_IS_CONCIL( concil ), FALSE );

	stamp = ofo_concil_get_stamp( concil );
	my_date_set_from_stamp( &date, stamp );
	my_date_set_now( &dnow );

	is_session = my_date_compare( &date, &dnow ) == 0;

	return( is_session );
}

/*
 * row       foreground  style   background
 * --------  ----------  ------  ----------
 * entry     normal      normal  normal
 * bat line  BAT_COLOR   italic  normal
 * proposal  normal      italic  BAT_BACKGROUND
 *
 * BAT lines are always displayed besides of their corresponding entry
 */
static void
on_cell_data_func( GtkTreeViewColumn *tcolumn,
						GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
						ofaReconcilPage *self )
{
	GObject *object;
	GdkRGBA color;
	gint id;
	ofoConcil *concil;

	g_return_if_fail( GTK_IS_CELL_RENDERER_TEXT( cell ));

	gtk_tree_model_get( tmodel, iter,
			COL_OBJECT, &object,
			-1 );
	g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
	g_object_unref( object );

	g_object_set( G_OBJECT( cell ),
						"style-set",      FALSE,
						"foreground-set", FALSE,
						"background-set", FALSE,
						NULL );

	id = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( tcolumn ), DATA_COLUMN_ID ));
	concil =  ofa_iconcil_get_concil( OFA_ICONCIL( object ));

	if( gtk_tree_model_iter_has_child( tmodel, iter )){
		/* DEBUG */
		if( 0 ){
			gchar *id_str, *dval_str;
			gtk_tree_model_get( tmodel, iter, COL_IDCONCIL, &id_str, COL_DRECONCIL, &dval_str, -1 );
			g_debug( "on_cell_data_func: type=%s, id=%ld, column_id=%d, concil=%p, concil_id=%s, concil_dval=%s",
					ofa_iconcil_get_instance_type( OFA_ICONCIL( object )),
					ofa_iconcil_get_instance_id( OFA_ICONCIL( object )),
					id, ( void * ) concil, id_str, dval_str );
			g_free( id_str );
			g_free( dval_str );
		}
		if( !concil && id == COL_DRECONCIL ){
			gdk_rgba_parse( &color, COLOR_BAT_UNCONCIL_FONT );
			g_object_set( G_OBJECT( cell ), "foreground-rgba", &color, NULL );
			g_object_set( G_OBJECT( cell ), "style", PANGO_STYLE_ITALIC, NULL );
		}
	}

	/* bat lines (normal if reconciliated, italic else */
	if( OFO_IS_BAT_LINE( object )){
		if( concil ){
			gdk_rgba_parse( &color, COLOR_BAT_CONCIL_FONT );
		} else {
			gdk_rgba_parse( &color, COLOR_BAT_UNCONCIL_FONT );
			g_object_set( G_OBJECT( cell ), "style", PANGO_STYLE_ITALIC, NULL );
		}
		g_object_set( G_OBJECT( cell ), "foreground-rgba", &color, NULL );
	}
}

/*
 * if a hierarchy is involved, only accept selection inside this hierarchy
 * else, accept anything
 * a hierarchy is identified by its first level path (an integer)
 * or also are allowed:
 * a single not-null hierarchy + a single not-null conciliation group
 */
static gboolean
tview_select_fn( GtkTreeSelection *selection, GtkTreeModel *tmodel, GtkTreePath *path, gboolean is_selected, ofaReconcilPage *self )
{
	GList *selected, *it;
	gint hierarchy, indice;
	ofxCounter council_id, id;
	gboolean accept;

	/* always accept unselect */
	if( is_selected ){
		return( TRUE );
	}

	/* examine the current selection, getting the first not-null hierarchy
	 * and the first not null conciliation group */
	hierarchy = -1;
	council_id = 0;
	accept = TRUE;
	selected = gtk_tree_selection_get_selected_rows( selection, &tmodel );
	for( it=selected ; it ; it=it->next ){
		get_indice_concil_id_by_path( self, tmodel, ( GtkTreePath * ) it->data, &indice, &id );
		if( indice != -1 ){
			if( hierarchy == -1 ){
				hierarchy = indice;
			} else if( hierarchy != indice ){
				accept = FALSE;
				break;
			}
		}
		if( id != 0 ){
			if( council_id == 0 ){
				council_id = id;
			} else if( council_id != id ){
				accept = FALSE;
				break;
			}
		}
	}
	g_list_free_full( selected, ( GDestroyNotify ) gtk_tree_path_free );

	/* examine current row */
	get_indice_concil_id_by_path( self, tmodel, path, &indice, &id );
	if( indice != -1 ){
		if( hierarchy != -1 && hierarchy != indice ){
			accept = FALSE;
		}
	}
	if( id != 0 ){
		if( council_id != 0 && council_id != id ){
			accept = FALSE;
		}
	}

	return( accept );
}

/*
 * - accept is enabled if selected lines are compatible:
 *   i.e. as soon as sum(debit)=sum(credit)
 *
 * - decline is enabled if we have only one batline
 *
 * - does not try here to check if the selection is balanced as this
 *   does not decide of the actions
 */
static void
on_tview_selection_changed( GtkTreeSelection *select, ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	gboolean concil_enabled, decline_enabled, unreconciliate_enabled;
	ofxAmount debit, credit;
	ofoConcil *concil;
	guint count, unconcil_rows;
	gboolean is_child, unique;
	gchar *sdeb, *scre;

	priv = ofa_reconcil_page_get_instance_private( self );

	concil_enabled = FALSE;
	decline_enabled = FALSE;
	unreconciliate_enabled = FALSE;
	priv->activate_action = ACTIV_NONE;

	examine_selection( self,
			&debit, &credit, &concil, &count, &unconcil_rows, &unique, &is_child );

	sdeb = ofa_amount_to_str( debit, priv->acc_currency );
	gtk_label_set_text( GTK_LABEL( priv->select_debit ), sdeb );
	scre = ofa_amount_to_str( credit, priv->acc_currency );
	gtk_label_set_text( GTK_LABEL( priv->select_credit ), scre );

	if( debit || credit ){
		if( !my_collate( sdeb, scre )){
			gtk_image_set_from_resource( GTK_IMAGE( priv->select_light ), st_resource_light_green );
		} else {
			gtk_image_set_from_resource( GTK_IMAGE( priv->select_light ), st_resource_light_yellow );
		}
	} else {
		gtk_image_set_from_resource( GTK_IMAGE( priv->select_light ), st_resource_light_empty );
	}
	g_free( sdeb );
	g_free( scre );

	/* it is important to only enable actions when only one unique
	 * conciliation group is selected, as implementation do not know
	 * how to handle multiple concil groups
	 */
	concil_enabled = ( unique && unconcil_rows > 0 );
	decline_enabled = ( unique && !concil && unconcil_rows == 1 && is_child );
	unreconciliate_enabled = ( unique && concil && unconcil_rows == 0 );

	/* what to do on selection activation ? */
	if( unique ){
		if( !concil || unconcil_rows > 0 ){
			priv->activate_action = ACTIV_CONCILIATE;
		} else {
			priv->activate_action = ACTIV_UNCONCILIATE;
		}
	}

	gtk_widget_set_sensitive( priv->reconciliate_btn, concil_enabled );
	gtk_widget_set_sensitive( priv->decline_btn, decline_enabled );
	gtk_widget_set_sensitive( priv->unreconciliate_btn, unreconciliate_enabled );
}

/*
 * examine the current selection, gathering the required indicators:
 * @debit: the total of debit rows
 * @credit: the total of credit rows
 * @concil: the first not-null conciliation object
 * @count: selection count
 * @unconcil_rows: count of unconciliated rows
 * @unique: whether we have more than one non-null conciliation id
 * @is_child: whether all rows of the selection are a child
 *  (most useful when selecting only one child to decline it)
 */
static void
examine_selection( ofaReconcilPage *self,
		ofxAmount *debit, ofxAmount *credit,
		ofoConcil **concil, guint *count, guint *unconcil_rows, gboolean *unique, gboolean *is_child )
{
	ofaReconcilPagePrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GList *selected, *it;
	ofoBase *object;
	ofxAmount amount;
	ofoConcil *row_concil;
	ofxCounter concil_id, row_id;
	GtkTreeIter iter, parent_iter;
	gboolean ok;

	priv = ofa_reconcil_page_get_instance_private( self );

	concil_id = 0;
	*debit = 0;
	*credit = 0;
	*concil = NULL;
	*unconcil_rows = 0;
	*unique = TRUE;
	*is_child = TRUE;

	select = gtk_tree_view_get_selection( priv->tview );
	selected = gtk_tree_selection_get_selected_rows( select, &tmodel );
	if( count ){
		*count = g_list_length( selected );
	}

	for( it=selected ; it ; it=it->next ){
		/* get object by path */
		ok = gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) it->data );
		g_return_if_fail( ok );
		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &object, -1 );
		g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
		g_object_unref( object );

		/* increment debit/credit */
		if( OFO_IS_ENTRY( object )){
			*debit += ofo_entry_get_debit( OFO_ENTRY( object ));
			*credit += ofo_entry_get_credit( OFO_ENTRY( object ));
		} else {
			amount = ofo_bat_line_get_amount( OFO_BAT_LINE( object ));
			if( amount < 0 ){
				*debit += -1*amount;
			} else {
				*credit += amount;
			}
		}

		/* manage conciliation groups */
		row_concil = ofa_iconcil_get_concil( OFA_ICONCIL( object ));
		row_id = row_concil ? ofo_concil_get_id( row_concil ) : 0;
		if( row_concil ){
			if( concil_id == 0 ){
				concil_id = row_id;
				*concil = row_concil;
			}
			/* important: check that we select at most one concil group */
			if( row_id > 0 && row_id != concil_id ){
				*unique = FALSE;
			}
		} else {
			*unconcil_rows += 1;
		}

		/* is it a child or a parent ? */
		if( !gtk_tree_model_iter_parent( tmodel, &parent_iter, &iter )){
			*is_child = FALSE;
		}
	}

	g_list_free_full( selected, ( GDestroyNotify ) gtk_tree_path_free );
}

/*
 * activating a row is a shortcut for toggling conciliate/unconciliate
 * the selection is automatically extended to the parent and all its
 * children if this is possible (and desirable: only one selected row)
 */
static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;

	priv = ofa_reconcil_page_get_instance_private( self );

	expand_tview_selection( self );
	switch( priv->activate_action ){
		case ACTIV_CONCILIATE:
			do_reconciliate( self );
			break;
		case ACTIV_UNCONCILIATE:
			do_unconciliate( self );
			break;
		default:
			break;
	}

	on_tview_selection_changed( NULL, self );
}

/*
 * Returns :
 * TRUE to stop other hub_handlers from being invoked for the event.
 * FALSE to propagate the event further.
 *
 * Handles left and right arrows to expand/collapse nodes
 */
static gboolean
on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaReconcilPage *self )
{
	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Left ){
			collapse_node( self, widget );
		} else if( event->keyval == GDK_KEY_Right ){
			expand_node( self, widget );
		}
	}

	return( FALSE );
}

static void
collapse_node( ofaReconcilPage *self, GtkWidget *widget )
{
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GList *selected;

	if( GTK_IS_TREE_VIEW( widget )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ));
		selected = gtk_tree_selection_get_selected_rows( select, &tmodel );
		if( g_list_length( selected ) == 1 &&
				gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) selected->data )){
			collapse_node_by_iter( self, GTK_TREE_VIEW( widget ), tmodel, &iter );
		}
		g_list_free_full( selected, ( GDestroyNotify ) gtk_tree_path_free );
	}
}

static void
collapse_node_by_iter( ofaReconcilPage *self, GtkTreeView *tview, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	GtkTreePath *path;
	GtkTreeIter parent_iter;

	if( gtk_tree_model_iter_has_child( tmodel, iter )){
		path = gtk_tree_model_get_path( tmodel, iter );
		gtk_tree_view_collapse_row( tview, path );
		gtk_tree_path_free( path );

	} else if( gtk_tree_model_iter_parent( tmodel, &parent_iter, iter )){
		path = gtk_tree_model_get_path( tmodel, &parent_iter );
		gtk_tree_view_collapse_row( tview, path );
		gtk_tree_path_free( path );
	}
}

static void
expand_node( ofaReconcilPage *self, GtkWidget *widget )
{
	GtkTreeSelection *select;
	GList *selected;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	if( GTK_IS_TREE_VIEW( widget )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ));
		selected = gtk_tree_selection_get_selected_rows( select, &tmodel );
		if( g_list_length( selected ) == 1 &&
				gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) selected->data )){
			expand_node_by_iter( self, GTK_TREE_VIEW( widget ), tmodel, &iter );
		}
		g_list_free_full( selected, ( GDestroyNotify ) gtk_tree_path_free );
	}
}

static void
expand_node_by_iter( ofaReconcilPage *self, GtkTreeView *tview, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	GtkTreePath *path;

	if( gtk_tree_model_iter_has_child( tmodel, iter )){
		path = gtk_tree_model_get_path( tmodel, iter );
		gtk_tree_view_expand_row( tview, path, FALSE );
		gtk_tree_path_free( path );
	}
}

static void
expand_node_by_store_iter( ofaReconcilPage *self, GtkTreeIter *store_iter )
{
	ofaReconcilPagePrivate *priv;
	GtkTreePath *store_path, *filter_path, *sort_path;

	priv = ofa_reconcil_page_get_instance_private( self );

	store_path = gtk_tree_model_get_path( priv->tstore, store_iter );
	filter_path = gtk_tree_model_filter_convert_child_path_to_path(
						GTK_TREE_MODEL_FILTER( priv->tfilter ), store_path );
	sort_path = gtk_tree_model_sort_convert_child_path_to_path(
						GTK_TREE_MODEL_SORT( priv->tsort ), filter_path );

	gtk_tree_view_expand_row( priv->tview, sort_path, FALSE );

	gtk_tree_path_free( sort_path );
	gtk_tree_path_free( filter_path );
	gtk_tree_path_free( store_path );
}

/*
 * the view is disabled (insensitive) each time the configuration parms
 * are not valid (invalid account or invalid reconciliation display
 * mode)
 */
static gboolean
check_for_enable_view( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	gboolean enabled;

	priv = ofa_reconcil_page_get_instance_private( self );

	enabled = priv->account && OFO_IS_ACCOUNT( priv->account );
	enabled &= ( priv->mode >= ENT_CONCILED_MIN );

	gtk_widget_set_sensitive( priv->acc_header_label, enabled );
	gtk_widget_set_sensitive( priv->acc_debit_label, enabled );
	gtk_widget_set_sensitive( priv->acc_credit_label, enabled );

	gtk_widget_set_sensitive( GTK_WIDGET( priv->tview ), enabled );

	gtk_widget_set_sensitive( priv->bal_footer_label, enabled );
	gtk_widget_set_sensitive( priv->bal_debit_label, enabled );
	gtk_widget_set_sensitive( priv->bal_credit_label, enabled );

	gtk_widget_set_sensitive( priv->actions_frame, enabled );

	return( enabled );
}

/*
 * default is to expand unreconciliated rows which have a proposed
 * bat line, collapsing the entries for which the proposal has been
 * accepted (and are so reconciliated)
 */
static void
default_expand_view( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoConcil *concil;
	GObject *object;

	priv = ofa_reconcil_page_get_instance_private( self );

	tmodel = gtk_tree_view_get_model( priv->tview );
	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &object, -1 );
			g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
			g_object_unref( object );

			/* if an entry which has a child:
			 * - collapse if entry is reconciliated and batline points
			 *   to this same entry
			 * - else expand */
			concil = ofa_iconcil_get_concil( OFA_ICONCIL( object ));
			if( concil ){
				collapse_node_by_iter( self, priv->tview, tmodel, &iter );
			} else {
				expand_node_by_iter( self, priv->tview, tmodel, &iter );
			}
			if( !gtk_tree_model_iter_next( tmodel, &iter )){
				break;
			}
		}
	}
}

/*
 * when only one row is selected, and this row is member of a hierarchy,
 * this expands the selection to all the hierarchy
 */
static void
expand_tview_selection( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	GtkTreeSelection *selection;
	GList *selected;
	GtkTreeIter *start, iter, parent, child;
	GtkTreeModel *tmodel;

	priv = ofa_reconcil_page_get_instance_private( self );

	selection = gtk_tree_view_get_selection( priv->tview );
	selected = gtk_tree_selection_get_selected_rows( selection, &tmodel );
	if( g_list_length( selected ) == 1 ){
		start = NULL;
		if( !gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) selected->data )){
			g_return_if_reached();
		}
		if( gtk_tree_model_iter_parent( tmodel, &parent, &iter )){
			gtk_tree_selection_select_iter( selection, &parent );
			start = &parent;
		} else {
			start = &iter;
		}
		if( gtk_tree_model_iter_children( tmodel, &child, start )){
			while( TRUE ){
				gtk_tree_selection_select_iter( selection, &child );
				if( !gtk_tree_model_iter_next( tmodel, &child )){
					break;
				}
			}
		}
	}
	g_list_free_full( selected, ( GDestroyNotify ) gtk_tree_path_free );
}

/*
 * use cases:
 * - importing a bat file while the corresponding entries have already
 *   been manually reconciliated: accept bat line
 * - the code is not able to automatically propose the bat line against
 *   the right entry
 * - two entries are presented together to the bank, thus having only
 *   one batline for these
 *
 * In all these cases, entry(ies) and bat line must be manually selected
 * together, so that accept may be enabled
 */
static void
on_reconciliate_clicked( GtkButton *button, ofaReconcilPage *self )
{
	expand_tview_selection( self );
	do_reconciliate( self );
	on_tview_selection_changed( NULL, self );
}

/*
 * we have at most one conciliation group
 * create one if needed
 * add all other lines to this conciliation group
 *
 * Please note that, depending of the active filter, the row may
 * disappear as soon as the conciliation is set, and so the sort
 * path becomes invalid, and so the sort ref also. This is why we
 * are preferentially working on store model.
 */
static void
do_reconciliate( ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_do_reconciliate";
	ofaReconcilPagePrivate *priv;
	ofxAmount debit, credit;
	guint count, unconcil_rows;
	gboolean is_child, unique;
	ofoConcil *concil;
	GDate dval;
	GtkTreeSelection *selection;
	GList *selected, *it, *store_refs, *objects;
	GtkTreeIter store_iter, parent_iter, sort_iter;
	GtkTreeRowReference *store_ref, *ent_parent_ref, *bat_parent_ref, *parent_ref;
	ofoBase *object;
	ofxCounter concil_id;
	gint depth;
	GtkTreePath *sort_path, *store_path, *filter_path, *parent_path;
	const gchar *ent_account;
	GtkWindow *toplevel;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = ofa_reconcil_page_get_instance_private( self );

	examine_selection( self, &debit, &credit, &concil, &count, &unconcil_rows, &unique, &is_child );
	g_return_if_fail( unique );

	/* ask for a user confirmation when amounts are not balanced */
	if( debit != credit ){
		if( !user_confirm_reconciliation( self, debit, credit )){
			return;
		}
	}

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));

	/* compute effect date of a new concil group */
	if( !concil ){
		if( !my_date_is_valid( get_date_for_new_concil( self, &dval ))){
			my_utils_msg_dialog( toplevel, GTK_MESSAGE_WARNING,
					_( "Conciliation is cancelled because unable to get a valid conciliation effect date" ));
			return;
		}
	} else {
		my_date_set_from_date( &dval, ofo_concil_get_dval( concil ));
	}
	concil_id = concil ? ofo_concil_get_id( concil ) : 0;
	ent_account = NULL;

	/* add each line to this same concil group
	 * + in order to get rid of side effects due to filtering model, we
	 *   first convert the paths on sort model to refs on store model
	 */
	selection = gtk_tree_view_get_selection( priv->tview );
	selected = gtk_tree_selection_get_selected_rows( selection, NULL );
	store_refs = convert_selected_to_store_refs( self, selected );
	g_list_free_full( selected, ( GDestroyNotify ) gtk_tree_path_free );

	/* now we are able to create the conciliation group
	 * and to add each selected row to this group
	 * + we take advantage of this first conversion phase to take a ref
	 *   on the future unique parent of the conciliation group
	 * + we take the target Openbook account (if any)
	 */
	ent_parent_ref = NULL;
	bat_parent_ref = NULL;
	for( it=store_refs ; it ; it=it->next ){
		store_ref = ( GtkTreeRowReference * ) it->data;
		g_return_if_fail( store_ref && gtk_tree_row_reference_valid( store_ref ));

		store_path = gtk_tree_row_reference_get_path( store_ref );

		if( !gtk_tree_model_get_iter( priv->tstore, &store_iter, store_path )){
			g_return_if_reached();
		}

		gtk_tree_model_get( priv->tstore, &store_iter, COL_OBJECT, &object, -1 );
		g_return_if_fail( object && OFA_IS_ICONCIL( object ));
		g_object_unref( object );

		/* search for the first toplevel entry
		 * defaulting to the first toplevel batline
		 * depth=1 is a parent */
		depth = gtk_tree_path_get_depth( store_path );
		if( depth == 1 ){
			if( !ent_parent_ref && OFO_IS_ENTRY( object )){
				ent_parent_ref = gtk_tree_row_reference_copy( store_ref );
			}
			if( !bat_parent_ref && OFO_IS_BAT_LINE( object )){
				bat_parent_ref = gtk_tree_row_reference_copy( store_ref );
			}
		}

		if( !concil ){
			concil = ofa_iconcil_new_concil( OFA_ICONCIL( object ), &dval );
			concil_id = ofo_concil_get_id( concil );
		}
		if( !ofa_iconcil_get_concil( OFA_ICONCIL( object ))){
			ofa_iconcil_add_to_concil( OFA_ICONCIL( object ), concil );
		}
		update_concil_data_by_store_iter( self, &store_iter, concil_id, &dval );

		if( !ent_account && OFO_IS_ENTRY( object )){
			/*g_debug( "%s: setting ent_account=%s", thisfn, ent_account );*/
			ent_account = ofo_entry_get_account( OFO_ENTRY( object ));
		}

		gtk_tree_path_free( store_path );
	}
	parent_ref = gtk_tree_row_reference_copy( ent_parent_ref ? ent_parent_ref : bat_parent_ref );
	gtk_tree_row_reference_free( ent_parent_ref );
	gtk_tree_row_reference_free( bat_parent_ref );

	/* due to the filtering model, the conciliated rows may have
	 * disappeared from the viewport; but the store refs are always
	 * valid
	 * iter through these refs, selecting rows which are not descendant
	 * of the parent, they are kept to be removed and reinserted in the
	 * right place
	 * + setup the associated account to the BAT lines
	 */
	objects = NULL;
	for( it=store_refs ; it ; it=it->next ){
		store_ref = ( GtkTreeRowReference * ) it->data;
		g_return_if_fail( store_ref && gtk_tree_row_reference_valid( store_ref ));

		store_path = gtk_tree_row_reference_get_path( store_ref );
		parent_path = gtk_tree_row_reference_get_path( parent_ref );

		gchar *it_str = gtk_tree_path_to_string( store_path );
		gchar *parent_str = gtk_tree_path_to_string( parent_path );
		if( DEBUG_RECONCILIATE ){
			g_debug( "%s: it_path=%s, parent_path=%s", thisfn, it_str, parent_str );
		}
		g_free( parent_str );
		g_free( it_str );

		if( gtk_tree_path_compare( store_path, parent_path ) != 0 ){

			gtk_tree_model_get_iter( priv->tstore, &store_iter, store_path );
			gtk_tree_model_get( priv->tstore, &store_iter, COL_OBJECT, &object, -1 );
			g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));

			if( my_strlen( ent_account ) && OFO_IS_BAT_LINE( object )){
				/*g_debug( "%s: calling batline_associates_account", thisfn );*/
				batline_associates_account( self, OFO_BAT_LINE( object ), ent_account );
			}

			if( !gtk_tree_path_is_descendant( store_path, parent_path )){
				objects = g_list_prepend( objects, object );

				if( DEBUG_RECONCILIATE ){
					g_debug( "%s: removing object=%p (%s)", thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));
				}
				gtk_tree_store_remove( GTK_TREE_STORE( priv->tstore ), &store_iter );
				if( !gtk_tree_row_reference_valid( parent_ref )){
					g_warning( "%s: parent_ref no longer valid", thisfn );
					g_return_if_reached();
				}
			} else {
				g_object_unref( object );
			}
		}

		gtk_tree_path_free( store_path );
		gtk_tree_path_free( parent_path );
	}

	parent_path = gtk_tree_row_reference_get_path( parent_ref );
	if( !gtk_tree_model_get_iter( priv->tstore, &parent_iter, parent_path )){
		g_warning( "%s: unable to get the parent iter on store model", thisfn );
		return;
	}

	for( it=objects ; it ; it=it->next ){
		object = OFO_BASE( it->data );
		g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));

		if( DEBUG_RECONCILIATE ){
			g_debug( "%s: inserting object=%p (%s)", thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));
		}
		gtk_tree_store_insert( GTK_TREE_STORE( priv->tstore ), &store_iter, &parent_iter, -1 );
		if( OFO_IS_ENTRY( object )){
			set_row_entry( self, priv->tstore, &store_iter, OFO_ENTRY( object ));
		} else {
			set_row_batline( self, priv->tstore, &store_iter, OFO_BAT_LINE( object ));
		}

		if( !gtk_tree_row_reference_valid( parent_ref )){
			g_debug( "%s: parent_ref no longer valid", thisfn );
			return;
		}
	}

	gtk_tree_path_free( parent_path );
	g_list_free_full( objects, ( GDestroyNotify ) g_object_unref );

	/* last re-selected the head of the hierarchy if it is displayed */
	gtk_tree_selection_unselect_all( selection );
	parent_path = gtk_tree_row_reference_get_path( parent_ref );
	g_return_if_fail( parent_path );
	filter_path = gtk_tree_model_filter_convert_child_path_to_path(
							GTK_TREE_MODEL_FILTER( priv->tfilter ), parent_path );
	if( filter_path ){
		sort_path = gtk_tree_model_sort_convert_child_path_to_path(
								GTK_TREE_MODEL_SORT( priv->tsort ), filter_path );

		gtk_tree_selection_select_path( selection, sort_path );
		gtk_tree_model_get_iter( priv->tsort, &sort_iter, sort_path );
		collapse_node_by_iter( self, priv->tview, priv->tsort, &sort_iter );
		gtk_tree_path_free( sort_path );
	}
	gtk_tree_path_free( filter_path );
	gtk_tree_path_free( parent_path );
	gtk_tree_row_reference_free( parent_ref );

	set_reconciliated_balance( self );
	if( priv->bats ){
		display_bat_counts( self );
	}
}

static gboolean
user_confirm_reconciliation( ofaReconcilPage *self, ofxAmount debit, ofxAmount credit )
{
	gboolean ok;
	gchar *sdeb, *scre, *str;

	sdeb = ofa_amount_to_str( debit, NULL );
	scre = ofa_amount_to_str( credit, NULL );
	str = g_strdup_printf( _( "Caution: reconciliated amounts are not balanced:\n"
			"debit=%s, credit=%s.\n"
			"Are you sure you want reconciliate this group ?" ), sdeb, scre );

	ok = my_utils_dialog_question( str, _( "Reconciliate" ));

	g_free( sdeb );
	g_free( scre );
	g_free( str );

	return( ok );
}

/*
 * search for a valid date in order to initialize a new conciliation group
 * - first examine effect date of selected bat lines
 * - then take manual conciliation date
 *
 * there is no garanty that the returned date is valid
 */
static GDate *
get_date_for_new_concil( ofaReconcilPage *self, GDate *date )
{
	ofaReconcilPagePrivate *priv;
	GtkTreeSelection *selection;
	GList *rows, *it;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBase *object;

	priv = ofa_reconcil_page_get_instance_private( self );

	selection = gtk_tree_view_get_selection( priv->tview );
	rows = gtk_tree_selection_get_selected_rows( selection, &tmodel );
	my_date_clear( date );

	for( it=rows ; it ; it=it->next ){
		gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) it->data );
		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &object, -1 );
		g_return_val_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )), NULL );
		g_object_unref( object );

		if( OFO_IS_BAT_LINE( object )){
			my_date_set_from_date( date, ofo_bat_line_get_deffect( OFO_BAT_LINE( object )));
			break;
		}
	}

	g_list_free_full( rows, ( GDestroyNotify ) gtk_tree_path_free );

	/* else try with the manually provided date
	 * may not be valid */
	if( !my_date_is_valid( date )){
		my_date_set_from_date( date, &priv->dconcil );
	}

	return( date );
}

/*
 * decline a proposition:
 * the selection is limited to one child row
 * it is moved to level 0, and re-selected
 */
static void
on_decline_clicked( GtkButton *button, ofaReconcilPage *self )
{
	do_decline( self );
	on_tview_selection_changed( NULL, self );
}

static void
do_decline( ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_do_decline";
	ofaReconcilPagePrivate *priv;
	GtkTreeSelection *select;
	GList *selected;
	GtkTreeIter sort_iter, filter_iter, store_iter, parent_iter;
	gboolean has_parent, ok;
	GObject *object;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = ofa_reconcil_page_get_instance_private( self );

	select = gtk_tree_view_get_selection( priv->tview );
	selected = gtk_tree_selection_get_selected_rows( select, NULL );
	g_return_if_fail( g_list_length( selected ) == 1 );

	/* get an iter on the underlying store model, making sure it has a
	 *  parent */
	ok = gtk_tree_model_get_iter( priv->tsort, &sort_iter, ( GtkTreePath * ) selected->data );
	g_return_if_fail( ok );

	gtk_tree_model_sort_convert_iter_to_child_iter( GTK_TREE_MODEL_SORT( priv->tsort ), &filter_iter, &sort_iter );
	gtk_tree_model_filter_convert_iter_to_child_iter( GTK_TREE_MODEL_FILTER( priv->tfilter ), &store_iter, &filter_iter );

	has_parent = gtk_tree_model_iter_parent( priv->tstore, &parent_iter, &store_iter );
	g_return_if_fail( has_parent );

	gtk_tree_model_get( priv->tstore, &store_iter, COL_OBJECT, &object, -1 );
	g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));

	/* remove the child and re-insert it at level zero
	 * reset conciliation indications on parent row */
	gtk_tree_store_remove( GTK_TREE_STORE( priv->tstore ), &store_iter );
	gtk_tree_store_insert( GTK_TREE_STORE( priv->tstore ), &store_iter, NULL, -1 );

	if( OFO_IS_ENTRY( object )){
		set_row_entry( self, priv->tstore, &store_iter, OFO_ENTRY( object ));
	} else {
		set_row_batline( self, priv->tstore, &store_iter, OFO_BAT_LINE( object ));
	}
	g_object_unref( object );

	update_concil_data_by_store_iter( self, &parent_iter, 0, NULL );

	g_list_free_full( selected, ( GDestroyNotify ) gtk_tree_path_free );

	/* re-select the newly inserted child */
	gtk_tree_selection_unselect_all( select );
	gtk_tree_model_filter_convert_child_iter_to_iter( GTK_TREE_MODEL_FILTER( priv->tfilter ), &filter_iter, &store_iter );
	gtk_tree_model_sort_convert_child_iter_to_iter( GTK_TREE_MODEL_SORT( priv->tsort ), &sort_iter, &filter_iter );
	gtk_tree_selection_select_iter( select, &sort_iter );
}

static void
on_unreconciliate_clicked( GtkButton *button, ofaReconcilPage *self )
{
	expand_tview_selection( self );
	do_unconciliate( self );
	on_tview_selection_changed( NULL, self );
}

static void
do_unconciliate( ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_do_unconciliate";
	ofaReconcilPagePrivate *priv;
	ofxAmount debit, credit;
	guint count, unconcil_rows;
	gboolean is_child, unique, first;
	ofoConcil *concil, *concil_to_remove;
	GtkTreeSelection *selection;
	GList *selected, *store_refs, *it, *entry_list, *batline_list;
	GtkTreeIter store_iter;
	ofoBase *object;
	GtkTreeRowReference *store_ref;
	GtkTreePath *store_path;
	gint depth;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = ofa_reconcil_page_get_instance_private( self );

	examine_selection( self, &debit, &credit, &concil, &count, &unconcil_rows, &unique, &is_child );
	g_return_if_fail( unique );
	g_return_if_fail( concil && OFO_IS_CONCIL( concil ));

	selection = gtk_tree_view_get_selection( priv->tview );
	selected = gtk_tree_selection_get_selected_rows( selection, NULL );
	store_refs = convert_selected_to_store_refs( self, selected );
	g_list_free_full( selected, ( GDestroyNotify ) gtk_tree_path_free );

	/* first iterate to find a candidate value date
	 * + we also build a list of entries and batlines before removing
	 *   them from the store (butnot the parent)
	 */
	first = TRUE;
	entry_list = NULL;
	batline_list = NULL;
	for( it=store_refs ; it ; it=it->next ){
		store_ref = ( GtkTreeRowReference * ) it->data;
		store_path = gtk_tree_row_reference_get_path( store_ref );
		gtk_tree_model_get_iter( priv->tstore, &store_iter, store_path );
		gtk_tree_model_get( priv->tstore, &store_iter, COL_OBJECT, &object, -1 );
		g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));

		concil_to_remove = ( first ? concil : NULL );
		first = FALSE;

		if( DEBUG_UNCONCILIATE ){
			g_debug( "%s: removing concil=%p for object=%p (type=%s, id=%ld)",
					thisfn, concil_to_remove, ( void * ) object,
					ofa_iconcil_get_instance_type( OFA_ICONCIL( object )),
					ofa_iconcil_get_instance_id( OFA_ICONCIL( object )));
		}

		ofa_iconcil_remove_concil( OFA_ICONCIL( object ), concil_to_remove );

		depth = gtk_tree_path_get_depth( store_path );
		if( DEBUG_UNCONCILIATE ){
			g_debug( "%s: depth=%d", thisfn, depth );
		}
		if( depth > 1 ){
			if( OFO_IS_ENTRY( object )){
				entry_list = g_list_prepend( entry_list, object );
			} else {
				batline_list = g_list_prepend( batline_list, object );
			}
			if( DEBUG_UNCONCILIATE ){
				g_debug( "%s: removing %s", thisfn, G_OBJECT_TYPE_NAME( object ));
			}
			gtk_tree_store_remove( GTK_TREE_STORE( priv->tstore ), &store_iter );
		}
		gtk_tree_path_free( store_path );
	}

	g_list_free_full( store_refs, ( GDestroyNotify ) gtk_tree_row_reference_free );

	/* second iterate to reinsert the entries and the batlines */
	for( it=entry_list ; it ; it=it->next ){
		if( DEBUG_UNCONCILIATE ){
			g_debug( "%s: reinserting entry %s",
					thisfn, ofo_entry_get_label( OFO_ENTRY( it->data )));
		}
		insert_entry( self, OFO_ENTRY( it->data ));
	}
	g_list_free_full( entry_list, ( GDestroyNotify ) g_object_unref );

	for( it=batline_list ; it ; it=it->next ){
		if( DEBUG_UNCONCILIATE ){
			g_debug( "%s: reinserting batline %s",
					thisfn, ofo_bat_line_get_label( OFO_BAT_LINE( it->data )));
		}
		insert_batline( self, OFO_BAT_LINE( it->data ));
	}
	g_list_free_full( batline_list, ( GDestroyNotify ) g_object_unref );

	set_reconciliated_balance( self );
	if( priv->bats ){
		display_bat_counts( self );
	}
}

/*
 * Compute the corresponding bank account balance, from our own account
 * balance, taking into account unreconciliated entries and (maybe) bat
 * lines
 *
 * Note that we have to iterate on the store model in order to count
 * all rows
 */
static void
set_reconciliated_balance( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	gdouble debit, credit;
	gdouble account_debit, account_credit;
	GtkTreeModel *tstore;
	GtkTreeIter iter;
	GObject *object;
	ofoConcil *concil;
	gdouble amount;
	gchar *str, *sdeb, *scre;

	priv = ofa_reconcil_page_get_instance_private( self );

	debit = 0;
	credit = 0;

	if( priv->account ){
		account_debit = ofo_account_get_val_debit( priv->account )
				+ ofo_account_get_rough_debit( priv->account )
				+ ofo_account_get_futur_debit( priv->account );
		account_credit = ofo_account_get_val_credit( priv->account )
				+ ofo_account_get_rough_credit( priv->account )
				+ ofo_account_get_futur_credit( priv->account );
		debit = account_credit;
		credit = account_debit;
		/*g_debug( "initial: debit=%lf, credit=%lf, solde=%lf", debit, credit, debit-credit );*/

		tstore = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));

		if( gtk_tree_model_get_iter_first( tstore, &iter )){
			while( TRUE ){
				gtk_tree_model_get( tstore, &iter, COL_OBJECT, &object, -1 );
				g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
				g_object_unref( object );

				if( OFO_IS_ENTRY( object )){
					if( ofo_entry_get_status( OFO_ENTRY( object )) != ENT_STATUS_DELETED ){
						concil = ofa_iconcil_get_concil( OFA_ICONCIL( object ));
						if( !concil ){
							debit += ofo_entry_get_debit( OFO_ENTRY( object ));
							credit += ofo_entry_get_credit( OFO_ENTRY( object ));
							/*g_debug( "label=%s, debit=%lf, credit=%lf, solde=%lf",
									ofo_entry_get_label( OFO_ENTRY( object )), debit, credit, debit-credit );*/
						}
					}
				} else {
					concil = ofa_iconcil_get_concil( OFA_ICONCIL( object ));
					if( !concil ){
						amount = ofo_bat_line_get_amount( OFO_BAT_LINE( object ));
						if( amount < 0 ){
							debit += -amount;
						} else {
							credit += amount;
						}
					}
				}
				if( !gtk_tree_model_iter_next( tstore, &iter )){
					break;
				}
			}
		}
	}

	/*g_debug( "end: debit=%lf, credit=%lf, solde=%lf", debit, credit, debit-credit );*/
	if( debit > credit ){
		str = ofa_amount_to_str( debit-credit, priv->acc_currency );
		sdeb = g_strdup_printf( _( "%s DB" ), str );
		scre = g_strdup( "" );
	} else {
		sdeb = g_strdup( "" );
		str = ofa_amount_to_str( credit-debit, priv->acc_currency );
		scre = g_strdup_printf( _( "%s CR" ), str );
	}

	gtk_label_set_text( GTK_LABEL( priv->bal_debit_label ), sdeb );
	gtk_label_set_text( GTK_LABEL( priv->bal_credit_label ), scre );

	g_free( str );
	g_free( sdeb );
	g_free( scre );
}

/*
 * update the conciliation id and date
 * the provided iter must be on the store model
 */
static void
update_concil_data_by_store_iter( ofaReconcilPage *self, GtkTreeIter *store_iter, ofxCounter id, const GDate *dval )
{
	ofaReconcilPagePrivate *priv;
	gchar *sid, *sdvaleur;

	g_return_if_fail( store_iter );

	priv = ofa_reconcil_page_get_instance_private( self );

	sid = id > 0 ? g_strdup_printf( "%ld", id ) : g_strdup( "" );
	sdvaleur = my_date_to_str( dval, ofa_prefs_date_display());

	/* set the reconciliation date in the pointed-to row */
	gtk_tree_store_set(
			GTK_TREE_STORE( priv->tstore ),
			store_iter,
			COL_IDCONCIL,  sid,
			COL_DRECONCIL, sdvaleur,
			-1 );

	g_free( sdvaleur );
	g_free( sid );
}

static const GDate *
get_bat_line_dope( ofaReconcilPage *self, ofoBatLine *batline )
{
	const GDate *dope;

	dope = ofo_bat_line_get_dope( batline );
	if( !my_date_is_valid( dope )){
		dope = ofo_bat_line_get_deffect( batline );
	}

	return( dope );
}

/*
 * set
 * - the level zero indice if the row is member of a hierarchy
 * - the counciliation id
 * of the specified row
 */
static void
get_indice_concil_id_by_path( ofaReconcilPage *self, GtkTreeModel *tmodel, GtkTreePath *path, gint *indice, ofxCounter *concil_id )
{
	GtkTreeIter iter;
	gint *indices, depth;
	ofoBase *object;
	ofoConcil *concil;

	*indice = -1;
	*concil_id = 0;

	gtk_tree_model_get_iter( tmodel, &iter, path );
	indices = gtk_tree_path_get_indices_with_depth( path, &depth );
	if( depth > 1 ){
		*indice = indices[0];
	} else if( gtk_tree_model_iter_has_child( tmodel, &iter )){
		*indice = indices[0];
	}
	gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &object, -1 );
	g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
	g_object_unref( object );
	concil = ofa_iconcil_get_concil( OFA_ICONCIL( object ));
	if( concil ){
		*concil_id = ofo_concil_get_id( concil );
	}
}

/*
 * returns an iter on the store model, or NULL
 *
 * if not %NULL, the returned iter should be gtk_tree_iter_free() by
 * the caller
 */
static GtkTreeIter *
search_for_entry_by_number( ofaReconcilPage *self, ofxCounter number )
{
	ofaReconcilPagePrivate *priv;
	GtkTreeModel *child_tmodel;
	GtkTreeIter iter;
	GtkTreeIter *entry_iter;
	ofxCounter ecr_number;
	GObject *object;

	priv = ofa_reconcil_page_get_instance_private( self );

	entry_iter = NULL;
	child_tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));

	if( gtk_tree_model_get_iter_first( child_tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( child_tmodel,
					&iter,
					COL_NUMBER, &ecr_number,
					COL_OBJECT, &object,
					-1 );
			g_return_val_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )), NULL );
			g_object_unref( object );

			/* search for the entry which has the specified number */
			if( OFO_IS_ENTRY( object ) && ecr_number == number ){
				entry_iter = gtk_tree_iter_copy( &iter );
				break;
			}

			if( !gtk_tree_model_iter_next( child_tmodel, &iter )){
				break;
			}
		}
	}

	return( entry_iter );
}

/*
 * when inserting a not yet conciliated entry or batline,
 *  search for another unconciliated row with compatible amount: it will
 *  be used as a parent of the being-inserted row
 */
static gboolean
search_for_parent_by_amount( ofaReconcilPage *self, ofoBase *object, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	static const gchar *thisfn = "ofa_reconcil_page_search_for_parent_by_amount";
	ofaReconcilPagePrivate *priv;
	ofxAmount object_balance, iter_balance;
	ofoBase *obj;

	g_return_val_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )), FALSE );

	priv = ofa_reconcil_page_get_instance_private( self );

	if( ofa_iconcil_get_concil( OFA_ICONCIL( object ))){
		return( FALSE );
	}

	gboolean is_debug = FALSE;

	if( gtk_tree_model_get_iter_first( tmodel, iter )){
		/* get amounts of the being-inserted object
		 * entry: debit is positive, credit is negative
		 * batline: debit is negative, credit is positive */
		if( OFO_IS_ENTRY( object )){
			object_balance = ofo_entry_get_credit( OFO_ENTRY( object ))
									- ofo_entry_get_debit( OFO_ENTRY( object ));
		} else {
			object_balance = ofo_bat_line_get_amount( OFO_BAT_LINE( object ));
		}
		if( is_debug ){
			g_debug( "%s: object_balance=%lf", thisfn, object_balance );
		}
		/* we are searching for an amount opposite to those of the object
		 * considering the first row which does not have yet a child */
		while( TRUE ){
			if( !gtk_tree_model_iter_has_child( tmodel, iter )){
				gtk_tree_model_get( tmodel, iter, COL_OBJECT, &obj, -1 );
				g_return_val_if_fail( obj && ( OFO_IS_ENTRY( obj ) || OFO_IS_BAT_LINE( obj )), FALSE );
				g_object_unref( obj );
				if( !ofa_iconcil_get_concil( OFA_ICONCIL( obj ))){
					iter_balance = 0;
					if( OFO_IS_ENTRY( obj )){
						if( ofo_entry_get_status( OFO_ENTRY( obj )) != ENT_STATUS_DELETED ){
							iter_balance = ofo_entry_get_credit( OFO_ENTRY( obj ))
												- ofo_entry_get_debit( OFO_ENTRY( obj ));
						}
					} else {
						iter_balance = ofo_bat_line_get_amount( OFO_BAT_LINE( obj ));
					}
					if( is_debug ){
						g_debug( "%s: iter_balance=%lf", thisfn, iter_balance );
					}
					if( ofa_amount_is_zero( object_balance+iter_balance, priv->acc_currency )){
						if( is_debug ){
							g_debug( "%s: returning TRUE", thisfn );
						}
						return( TRUE );
					}
				}
			}
			if( !gtk_tree_model_iter_next( tmodel, iter )){
				break;
			}
		}
	}

	return( FALSE );
}

/*
 * when inserting an already conciliated entry or bat line in the store,
 * search for the parent of the same conciliation group (if any)
 * if found, the row will be inserted as a child of the found parent
 * set the iter on the tree store if found
 * return TRUE if found
 */
static gboolean
search_for_parent_by_concil( ofaReconcilPage *self, ofoBase *object, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	ofoConcil *concil;
	ofxCounter object_id;
	ofoBase *obj;

	g_return_val_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )), FALSE );

	concil = ofa_iconcil_get_concil( OFA_ICONCIL( object ));
	if( concil ){
		object_id = ofo_concil_get_id( concil );
		if( gtk_tree_model_get_iter_first( tmodel, iter )){
			while( TRUE ){
				gtk_tree_model_get( tmodel, iter, COL_OBJECT, &obj, -1 );
				g_return_val_if_fail( obj && ( OFO_IS_ENTRY( obj ) || OFO_IS_BAT_LINE( obj )), FALSE );
				g_object_unref( obj );
				concil = ofa_iconcil_get_concil( OFA_ICONCIL( obj ));
				if( concil && ofo_concil_get_id( concil ) == object_id ){
					return( TRUE );
				}
				if( !gtk_tree_model_iter_next( tmodel, iter )){
					break;
				}
			}
		}
	}

	return( FALSE );
}

/*
 * Convert the GList of selected rows (paths on the filter store) to a
 * GList of row references on the store model.
 * The returned list has to be freed with
 * g_list_free_full( list, ( GDestroyNotify ) gtk_tree_row_reference_free )
 */
static GList *
convert_selected_to_store_refs( ofaReconcilPage *self, GList *selected )
{
	ofaReconcilPagePrivate *priv;
	GList *store_refs, *it;
	GtkTreePath *sort_path, *filter_path, *store_path;
	GtkTreeRowReference *ref;

	priv = ofa_reconcil_page_get_instance_private( self );

	store_refs = NULL;

	for( it=selected ; it ; it=it->next ){
		sort_path = ( GtkTreePath * ) it->data;
		g_return_val_if_fail( sort_path, NULL );
		filter_path = gtk_tree_model_sort_convert_path_to_child_path(
								GTK_TREE_MODEL_SORT( priv->tsort ), sort_path );
		store_path = gtk_tree_model_filter_convert_path_to_child_path(
								GTK_TREE_MODEL_FILTER( priv->tfilter ), filter_path );

		/* take a ref on this selected row */
		ref = gtk_tree_row_reference_new( priv->tstore, store_path );
		store_refs = g_list_prepend( store_refs, ref );

		gtk_tree_path_free( filter_path );
		gtk_tree_path_free( store_path );
	}

	return( store_refs );
}

/*
 * This is called at the end of the view setup: all widgets are defined,
 * and triggers are connected.
 *
 * settings format: account;mode;manualconcil[sql];paned_position;
 */
static void
get_settings( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	GList *slist, *it;
	const gchar *cstr;
	GDate date;
	gchar *sdate;
	gint pos;

	priv = ofa_reconcil_page_get_instance_private( self );

	slist = ofa_settings_user_get_string_list( st_reconciliation );
	if( slist ){
		it = slist ? slist : NULL;
		cstr = it ? it->data : NULL;
		if( cstr ){
			gtk_entry_set_text( GTK_ENTRY( priv->acc_id_entry ), cstr );
		}

		it = it ? it->next : NULL;
		cstr = it ? it->data : NULL;
		if( cstr ){
			select_mode( self, atoi( cstr ));
		}

		it = it ? it->next : NULL;
		cstr = it ? it->data : NULL;
		if( cstr ){
			my_date_set_from_str( &date, cstr, MY_DATE_SQL );
			if( my_date_is_valid( &date )){
				sdate = my_date_to_str( &date, ofa_prefs_date_display());
				gtk_entry_set_text( priv->date_concil, sdate );
				g_free( sdate );
			}
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
}

static void
set_settings( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	const gchar *account, *sdate;
	gchar *date_sql;
	GDate date;
	gint pos;
	gchar *smode, *str;

	priv = ofa_reconcil_page_get_instance_private( self );

	account = gtk_entry_get_text( GTK_ENTRY( priv->acc_id_entry ));

	smode = g_strdup_printf( "%d", priv->mode );

	sdate = gtk_entry_get_text( priv->date_concil );
	my_date_set_from_str( &date, sdate, ofa_prefs_date_display());
	if( my_date_is_valid( &date )){
		date_sql = my_date_to_str( &date, MY_DATE_SQL );
	} else {
		date_sql = g_strdup( "" );
	}

	pos = gtk_paned_get_position( GTK_PANED( priv->top_paned ));

	str = g_strdup_printf( "%s;%s;%s;%d;", account ? account : "", smode, date_sql, pos );

	ofa_settings_user_set_string( st_reconciliation, str );

	g_free( date_sql );
	g_free( str );
	g_free( smode );
}

static void
connect_to_hub_signaling_system( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	gulong handler;

	priv = ofa_reconcil_page_get_instance_private( self );

	handler = g_signal_connect(
					priv->hub, SIGNAL_HUB_NEW, G_CALLBACK( on_hub_new_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect(
					priv->hub, SIGNAL_HUB_UPDATED, G_CALLBACK( on_hub_updated_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect(
					priv->hub, SIGNAL_HUB_DELETED, G_CALLBACK( on_hub_deleted_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
on_hub_new_object( ofaHub *hub, ofoBase *object, ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_on_hub_new_object";

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
 * insert the new entry in the tree store if it is registered on the
 * currently selected account
 */
static void
on_new_entry( ofaReconcilPage *self, ofoEntry *entry )
{
	ofaReconcilPagePrivate *priv;
	const gchar *selected_account, *entry_account;

	priv = ofa_reconcil_page_get_instance_private( self );

	selected_account = gtk_entry_get_text( GTK_ENTRY( priv->acc_id_entry ));
	entry_account = ofo_entry_get_account( entry );
	if( !g_utf8_collate( selected_account, entry_account )){
		insert_entry( self, entry );
		remediate_entry_orphan( self, entry );
		set_reconciliated_balance( self );
	}
}

/*
 * given a new entry being just newly inserted, seach for an orphan
 * row which would be candidate for attachment as a child
 */
static void
remediate_entry_orphan( ofaReconcilPage *self, ofoEntry *entry )
{
	GtkTreeIter *iter;

	iter = search_for_entry_by_number( self, ofo_entry_get_number( entry ));
	if( iter ){
		remediate_orphan( self, iter );
		gtk_tree_iter_free( iter );
	}
}

/*
 * parent_iter is on the store model
 */
static void
remediate_orphan( ofaReconcilPage *self, GtkTreeIter *parent_iter )
{
	ofaReconcilPagePrivate *priv;
	GtkTreeIter iter, cur_iter;
	GtkTreePath *parent_path;
	GtkTreeRowReference *parent_ref;
	gchar *parent_sdeb, *parent_scre, *row_sdeb, *row_scre;
	ofoBase *object;
	gboolean found;
	const GDate *dval;

	priv = ofa_reconcil_page_get_instance_private( self );

	gtk_tree_model_get( priv->tstore, parent_iter, COL_DEBIT, &parent_sdeb, COL_CREDIT, &parent_scre, -1 );
	parent_path = gtk_tree_model_get_path( priv->tstore, parent_iter );
	parent_ref = gtk_tree_row_reference_new( priv->tstore, parent_path );
	gtk_tree_path_free( parent_path );

	if( gtk_tree_model_get_iter_first( priv->tstore, &iter )){
		while( TRUE ){
			gtk_tree_model_get( priv->tstore, &iter, COL_DEBIT, &row_sdeb, COL_CREDIT, &row_scre, -1 );
			found = ( !g_utf8_collate( parent_sdeb, row_scre ) && !g_utf8_collate( parent_scre, row_sdeb ));
			g_free( row_sdeb );
			g_free( row_scre );
			if( found ){
				gtk_tree_model_get( priv->tstore, &iter, COL_OBJECT, &object, -1 );
				g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
				/* remove the found line */
				gtk_tree_store_remove( GTK_TREE_STORE( priv->tstore ), &iter );
				/* reinsert it as a child of the parent */
				parent_path = gtk_tree_row_reference_get_path( parent_ref );
				gtk_tree_model_get_iter( priv->tstore, &cur_iter, parent_path );
				gtk_tree_path_free( parent_path );
				gtk_tree_store_insert( GTK_TREE_STORE( priv->tstore ), &iter, &cur_iter, -1 );
				if( OFO_IS_ENTRY( object )){
					set_row_entry( self, priv->tstore, &iter, OFO_ENTRY( object ));
					dval = ofo_entry_get_deffect( OFO_ENTRY( object ));
				} else {
					set_row_batline( self, priv->tstore, &iter, OFO_BAT_LINE( object ));
					dval = ofo_bat_line_get_deffect( OFO_BAT_LINE( object ));
				}
				g_object_unref( object );
				update_concil_data_by_store_iter( self, &cur_iter, 0, dval);
				expand_node_by_store_iter( self, &cur_iter );
				break;
			}
			if( !gtk_tree_model_iter_next( priv->tstore, &iter )){
				break;
			}
		}
	}

	gtk_tree_row_reference_free( parent_ref );
	g_free( parent_sdeb );
	g_free( parent_scre );
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_on_hub_updated_object";

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

/*
 * we are doing our maximum to just requiring a store update
 * by comparing the stored data vs. the new entry data
 * After its modification, the entry may appear in - or disappear from -
 * the treeview...
 */
static void
on_updated_entry( ofaReconcilPage *self, ofoEntry *entry )
{
	ofaReconcilPagePrivate *priv;
	const gchar *selected_account, *entry_account;
	GtkTreeIter *iter;

	priv = ofa_reconcil_page_get_instance_private( self );

	iter = search_for_entry_by_number( self, ofo_entry_get_number( entry ));

	/* if the entry was present in the store, it is easy to remediate it */
	if( iter ){
		set_row_entry( self, priv->tstore, iter, entry );
		gtk_tree_iter_free( iter );
		gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( priv->tfilter ));

	/* else, should it be present now ? */
	} else {
		selected_account = gtk_entry_get_text( GTK_ENTRY( priv->acc_id_entry ));
		entry_account = ofo_entry_get_account( entry );
		if( !g_utf8_collate( selected_account, entry_account )){
			insert_entry( self, entry );
		}
	}
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
on_hub_deleted_object( ofaHub *hub, ofoBase *object, ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_on_hub_deleted_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p (%s)",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self, G_OBJECT_TYPE_NAME( self ));

	if( OFO_IS_ENTRY( object )){
		on_deleted_entry( self, OFO_ENTRY( object ));
	}
}

/* just remove the deleted entry
 * maybe after having previously detached any child
 */
static void
on_deleted_entry( ofaReconcilPage *self, ofoEntry *entry )
{
	ofaReconcilPagePrivate *priv;
	GtkTreeIter *iter, child_iter;
	ofoBase *object;

	priv = ofa_reconcil_page_get_instance_private( self );

	iter = search_for_entry_by_number( self, ofo_entry_get_number( entry ));
	if( iter ){
		while( gtk_tree_model_iter_children( priv->tstore, &child_iter, iter )){
			gtk_tree_model_get( priv->tstore, &child_iter, COL_OBJECT, &object, -1 );
			gtk_tree_store_remove( GTK_TREE_STORE( priv->tstore ), &child_iter );
			gtk_tree_store_insert( GTK_TREE_STORE( priv->tstore ), &child_iter, NULL, -1 );
			if( OFO_IS_ENTRY( object )){
				set_row_entry( self, priv->tstore, &child_iter, OFO_ENTRY( object ));
			} else {
				set_row_batline( self, priv->tstore, &child_iter, OFO_BAT_LINE( object ));
			}
			g_object_unref( object );
			gtk_tree_iter_free( iter );
			iter = search_for_entry_by_number( self, ofo_entry_get_number( entry ));
		}
		gtk_tree_store_remove( GTK_TREE_STORE( priv->tstore ), iter );
		gtk_tree_iter_free( iter );
		set_reconciliated_balance( self );
	}
}

static void
on_print_clicked( GtkButton *button, ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	const gchar *acc_number;
	ofaIThemeManager *manager;
	ofaPage *page;

	priv = ofa_reconcil_page_get_instance_private( self );

	acc_number = gtk_entry_get_text( GTK_ENTRY( priv->acc_id_entry ));
	manager = ofa_igetter_get_theme_manager( OFA_IGETTER( self ));
	page = ofa_itheme_manager_activate( manager, OFA_TYPE_RECONCIL_RENDER );
	ofa_reconcil_render_set_account( OFA_RECONCIL_RENDER( page ), acc_number );
}

/**
 * ofa_reconcil_page_set_account:
 * @page:
 * @number:
 */
void
ofa_reconcil_page_set_account( ofaReconcilPage *page, const gchar *number )
{
	ofaReconcilPagePrivate *priv;

	g_return_if_fail( page && OFA_IS_RECONCIL_PAGE( page ));
	g_return_if_fail( !OFA_PAGE( page )->prot->dispose_has_run );

	priv = ofa_reconcil_page_get_instance_private( page );

	gtk_entry_set_text( GTK_ENTRY( priv->acc_id_entry ), number );
}

static void
set_message( ofaReconcilPage *page, const gchar *msg )
{
	ofaReconcilPagePrivate *priv;

	priv = ofa_reconcil_page_get_instance_private( page );

	if( priv->msg_label ){
		my_utils_widget_set_style( priv->msg_label, "labelerror" );
		gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
	}
}
