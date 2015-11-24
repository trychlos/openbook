/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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
#include <math.h>
#include <stdlib.h>

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-plugin.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-bat.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-concil.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofs-concil-id.h"

#include "core/ofa-iconcil.h"

#include "ui/my-editable-date.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-bat-select.h"
#include "ui/ofa-bat-utils.h"
#include "ui/ofa-date-filter-hv-bin.h"
#include "ui/ofa-idate-filter.h"
#include "ui/ofa-itreeview-column.h"
#include "ui/ofa-itreeview-display.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-page.h"
#include "ui/ofa-page-prot.h"
#include "ui/ofa-reconcil-page.h"
#include "ui/ofa-reconcil-render.h"

/* private instance data
 */
struct _ofaReconcilPagePrivate {

	/* UI - account
	 */
	GtkWidget           *acc_id_entry;
	GtkWidget           *acc_label;
	GtkWidget           *acc_header_label;
	GtkWidget           *acc_debit_label;
	GtkWidget           *acc_credit_label;
	ofoAccount          *account;
	gdouble              acc_precision;
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

	/* UI - reconciliated balance
	 * this is the balance of the account
	 * with deduction of unreconciliated entries and bat lines
	 */
	GtkWidget           *select_debit;
	GtkWidget           *select_credit;
	GtkWidget           *bal_footer_label;
	GtkWidget           *bal_debit_label;
	GtkWidget           *bal_credit_label;

	/* internals
	 */
	ofoDossier          *dossier;
	GList               *handlers;
	GList               *bats;			/* loaded ofoBat objects */
};

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

#define COLOR_BAT_CONCIL_FONT           "#008000"	/* middle green */
#define COLOR_BAT_UNCONCIL_FONT         "#00ff00"	/* pure green */

static const gchar *st_reconciliation   = "Reconciliation";
static const gchar *st_pref_columns     = "ReconciliationColumns";
static const gchar *st_effect_dates     = "ReconciliationEffects";
static const gchar *st_ui_xml           = PKGUIDIR "/ofa-reconcil-page.ui";
static const gchar *st_ui_name          = "ReconciliationWindow";

/* it appears that Gtk+ displays a counter intuitive sort indicator:
 * when asking for ascending sort, Gtk+ displays a 'v' indicator
 * while we would prefer the '^' version -
 * we are defining the inverse indicator, and we are going to sort
 * in reverse order to have our own illusion
 */
#define OFA_SORT_ASCENDING              GTK_SORT_DESCENDING
#define OFA_SORT_DESCENDING             GTK_SORT_ASCENDING

static const gchar *st_default_reconciliated_class = "5"; /* default account class to be reconciliated */

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
		{ -1 }
};

static void         itreeview_column_iface_init( ofaITreeviewColumnInterface *iface );
static guint        itreeview_column_get_interface_version( const ofaITreeviewColumn *instance );
static void         itreeview_display_iface_init( ofaITreeviewDisplayInterface *iface );
static guint        itreeview_display_get_interface_version( const ofaITreeviewDisplay *instance );
static gchar       *itreeview_display_get_label( const ofaITreeviewDisplay *instance, guint column_id );
static gboolean     itreeview_display_get_def_visible( const ofaITreeviewDisplay *instance, guint column_id );
static GtkWidget   *v_setup_view( ofaPage *page );
static void         setup_treeview_header( ofaPage *page, GtkContainer *parent );
static void         setup_treeview( ofaPage *page, GtkContainer *parent );
static void         setup_treeview_footer( ofaPage *page, GtkContainer *parent );
static void         setup_account_selection( ofaPage *page, GtkContainer *parent );
static void         setup_entries_filter( ofaPage *page, GtkContainer *parent );
static void         setup_date_filter( ofaPage *page, GtkContainer *parent );
static void         setup_manual_rappro( ofaPage *page, GtkContainer *parent );
static void         setup_size_group( ofaPage *page, GtkContainer *parent );
static void         setup_auto_rappro( ofaPage *page, GtkContainer *parent );
static void         setup_action_buttons( ofaPage *page, GtkContainer *parent );
static GtkWidget   *v_get_top_focusable_widget( const ofaPage *page );
static void         on_account_entry_changed( GtkEntry *entry, ofaReconcilPage *self );
static void         on_account_select_clicked( GtkButton *button, ofaReconcilPage *self );
static void         do_account_selection( ofaReconcilPage *self );
static void         clear_account_content( ofaReconcilPage *self );
static ofoAccount  *get_reconciliable_account( ofaReconcilPage *self );
static void         set_account_balance( ofaReconcilPage *self );
static void         do_display_entries( ofaReconcilPage *self );
static void         insert_entry( ofaReconcilPage *self, ofoEntry *entry );
static void         set_row_entry( ofaReconcilPage *self, GtkTreeModel *tstore, GtkTreeIter *iter, ofoEntry *entry );
static void         on_mode_combo_changed( GtkComboBox *box, ofaReconcilPage *self );
static void         select_mode( ofaReconcilPage *self, gint mode );
static void         on_effect_dates_changed( ofaIDateFilter *filter, gint who, gboolean empty, const GDate *date, ofaReconcilPage *self );
static void         on_date_concil_changed( GtkEditable *editable, ofaReconcilPage *self );
static void         on_select_bat( GtkButton *button, ofaReconcilPage *self );
static void         do_select_bat( ofaReconcilPage *self );
static void         on_import_clicked( GtkButton *button, ofaReconcilPage *self );
static void         on_clear_clicked( GtkButton *button, ofaReconcilPage *self );
static void         clear_bat_content( ofaReconcilPage *self );
static void         do_display_bat_files( ofaReconcilPage *self );
static void         display_bat_by_id( ofaReconcilPage *self, ofxCounter bat_id );
static void         display_bat_file( ofaReconcilPage *self, ofoBat *bat );
static void         insert_batline( ofaReconcilPage *self, ofoBatLine *batline );
static void         set_row_batline( ofaReconcilPage *self, GtkTreeModel *store, GtkTreeIter *iter, ofoBatLine *batline );
static void         display_bat_name( ofaReconcilPage *self );
static void         display_bat_counts( ofaReconcilPage *self );
static gint         on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaReconcilPage *self );
static void         on_header_clicked( GtkTreeViewColumn *column, ofaReconcilPage *self );
static gboolean     is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaReconcilPage *self );
static gboolean     is_visible_entry( ofaReconcilPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoEntry *entry );
static gboolean     is_visible_batline( ofaReconcilPage *self, ofoBatLine *batline );
static gboolean     is_entry_session_conciliated( ofaReconcilPage *self, ofoEntry *entry, ofoConcil *concil );
static void         on_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaReconcilPage *self );
static gboolean     tview_select_fn( GtkTreeSelection *selection, GtkTreeModel *tmodel, GtkTreePath *path, gboolean is_selected, ofaReconcilPage *self );
static void         on_tview_selection_changed( GtkTreeSelection *select, ofaReconcilPage *self );
static void         examine_selection( ofaReconcilPage *self, ofxAmount *debit, ofxAmount *credit, ofoConcil **concil, guint *unconcil_rows, gboolean *unique, gboolean *is_child );
static void         on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaReconcilPage *self );
static gboolean     on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaReconcilPage *self );
static void         collapse_node( ofaReconcilPage *self, GtkWidget *widget );
static void         collapse_node_by_iter( ofaReconcilPage *self, GtkTreeView *tview, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void         expand_node( ofaReconcilPage *self, GtkWidget *widget );
static void         expand_node_by_iter( ofaReconcilPage *self, GtkTreeView *tview, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean     check_for_enable_view( ofaReconcilPage *self );
static void         default_expand_view( ofaReconcilPage *self );
static void         expand_tview_selection( ofaReconcilPage *self );
static void         on_reconciliate_clicked( GtkButton *button, ofaReconcilPage *self );
static void         do_reconciliate( ofaReconcilPage *self );
static GtkTreePath *do_reconciliate_review_hierarchy( ofaReconcilPage *self, GList *selected, GtkTreePath *parent );
static GDate       *get_date_for_new_concil( ofaReconcilPage *self, GDate *date );
static void         on_decline_clicked( GtkButton *button, ofaReconcilPage *self );
static void         do_decline( ofaReconcilPage *self );
static void         on_unreconciliate_clicked( GtkButton *button, ofaReconcilPage *self );
static void         do_unconciliate( ofaReconcilPage *self );
static void         set_reconciliated_balance( ofaReconcilPage *self );
static void         update_concil_data_by_path( ofaReconcilPage *self, GtkTreePath *path, ofxCounter id, const GDate *dval );
static void         update_concil_data_by_iter( ofaReconcilPage *self, GtkTreeModel *store_model, GtkTreeIter *store_iter, ofxCounter id, const GDate *dval );
static const GDate *get_bat_line_dope( ofaReconcilPage *self, ofoBatLine *batline );
static void         get_indice_concil_id_by_path( ofaReconcilPage *self, GtkTreeModel *tmodel, GtkTreePath *path, gint *indice, ofxCounter *concil_id );
static GtkTreeIter *search_for_entry_by_number( ofaReconcilPage *self, ofxCounter number );
static gboolean     search_for_parent_by_amount( ofaReconcilPage *self, ofoBase *object, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean     search_for_parent_by_concil( ofaReconcilPage *self, ofoBase *object, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void         get_settings( ofaReconcilPage *self );
static void         set_settings( ofaReconcilPage *self );
static void         dossier_signaling_connect( ofaReconcilPage *self );
static void         on_dossier_new_object( ofoDossier *dossier, ofoBase *object, ofaReconcilPage *self );
static void         on_new_entry( ofaReconcilPage *self, ofoEntry *entry );
static void         insert_new_entry( ofaReconcilPage *self, ofoEntry *entry );
static void         remediate_bat_lines( ofaReconcilPage *self, GtkTreeModel *tstore, ofoEntry *entry, GtkTreeIter *entry_iter );
static void         on_dossier_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaReconcilPage *self );
static void         on_updated_entry( ofaReconcilPage *self, ofoEntry *entry );
static void         on_print_clicked( GtkButton *button, ofaReconcilPage *self );

G_DEFINE_TYPE_EXTENDED( ofaReconcilPage, ofa_reconcil_page, OFA_TYPE_PAGE, 0, \
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ITREEVIEW_COLUMN, itreeview_column_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ITREEVIEW_DISPLAY, itreeview_display_iface_init ));

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
	GList *it;

	g_return_if_fail( OFA_IS_RECONCIL_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = ( OFA_RECONCIL_PAGE( instance ))->priv;

		if( priv->handlers && priv->dossier && OFO_IS_DOSSIER( priv->dossier )){
			for( it=priv->handlers ; it ; it=it->next ){
				g_signal_handler_disconnect( priv->dossier, ( gulong ) it->data );
			}
			priv->handlers = NULL;
		}
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

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( OFA_IS_RECONCIL_PAGE( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_RECONCIL_PAGE, ofaReconcilPagePrivate );

	my_date_clear( &self->priv->dconcil );
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

	g_type_class_add_private( klass, sizeof( ofaReconcilPagePrivate ));
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

	priv = OFA_RECONCIL_PAGE( page )->priv;
	priv->dossier = ofa_page_get_dossier( page );

	page_widget = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	widget = my_utils_container_attach_from_ui( GTK_CONTAINER( page_widget ), st_ui_xml, st_ui_name, "top" );
	g_return_val_if_fail( widget && GTK_IS_CONTAINER( widget ), NULL );

	setup_treeview_header( page, GTK_CONTAINER( widget ));
	setup_treeview( page, GTK_CONTAINER( widget ) );
	setup_treeview_footer( page, GTK_CONTAINER( widget ));

	setup_account_selection( page, GTK_CONTAINER( widget ));
	setup_entries_filter( page, GTK_CONTAINER( widget ));
	setup_date_filter( page, GTK_CONTAINER( widget ));
	setup_manual_rappro( page, GTK_CONTAINER( widget ));
	setup_size_group( page, GTK_CONTAINER( widget ));
	setup_auto_rappro( page, GTK_CONTAINER( widget ));
	setup_action_buttons( page, GTK_CONTAINER( widget ));

	dossier_signaling_connect( OFA_RECONCIL_PAGE( page ));

	get_settings( OFA_RECONCIL_PAGE( page ));

	select = gtk_tree_view_get_selection( priv->tview );
	gtk_tree_selection_unselect_all( select );
	on_tview_selection_changed( select, OFA_RECONCIL_PAGE( page ));

	return( page_widget );
}

static void
setup_treeview_header( ofaPage *page, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;

	priv = OFA_RECONCIL_PAGE( page )->priv;

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
setup_treeview( ofaPage *page, GtkContainer *parent )
{
	static const gchar *thisfn = "ofa_reconcil_page_setup_treeview";
	ofaReconcilPagePrivate *priv;
	GtkWidget *tview;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	gint column_id;

	priv = OFA_RECONCIL_PAGE( page )->priv;

	tview = my_utils_container_get_child_by_name( parent, "treeview" );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));
	priv->tview = GTK_TREE_VIEW( tview );

	gtk_tree_view_set_headers_visible( priv->tview, TRUE );
	g_signal_connect( G_OBJECT( tview ), "row-activated", G_CALLBACK( on_row_activated ), page );
	g_signal_connect( G_OBJECT( tview ), "key-press-event", G_CALLBACK( on_key_pressed ), page );

	priv->tstore = GTK_TREE_MODEL( gtk_tree_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING,					/* account, dope */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_ULONG,		/* ledger, piece, number */
			G_TYPE_STRING,									/* label */
			G_TYPE_STRING, G_TYPE_STRING,					/* debit, credit */
			G_TYPE_STRING, G_TYPE_STRING,					/* idconcil, dcreconcil */
			G_TYPE_OBJECT ));

	priv->tfilter = gtk_tree_model_filter_new( priv->tstore, NULL );
	g_object_unref( priv->tstore );
	gtk_tree_model_filter_set_visible_func(
			GTK_TREE_MODEL_FILTER( priv->tfilter ),
			( GtkTreeModelFilterVisibleFunc ) is_visible_row,
			page,
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
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, page, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), page );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, page, NULL );
	ofa_itreeview_display_add_column( OFA_ITREEVIEW_DISPLAY( page ), column, column_id );

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
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, page, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), page );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, page, NULL );
	ofa_itreeview_display_add_column( OFA_ITREEVIEW_DISPLAY( page ), column, column_id );

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
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, page, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), page );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, page, NULL );
	ofa_itreeview_display_add_column( OFA_ITREEVIEW_DISPLAY( page ), column, column_id );

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
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, page, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), page );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, page, NULL );

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
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, page, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), page );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, page, NULL );

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
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, page, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), page );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, page, NULL );

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
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, page, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), page );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, page, NULL );
	ofa_itreeview_display_add_column( OFA_ITREEVIEW_DISPLAY( page ), column, column_id );

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
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, page, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), page );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, page, NULL );

	select = gtk_tree_view_get_selection( priv->tview);
	gtk_tree_selection_set_mode( select, GTK_SELECTION_MULTIPLE );
	gtk_tree_selection_set_select_function( select, ( GtkTreeSelectionFunc ) tview_select_fn, page, NULL );
	g_signal_connect( select, "changed", G_CALLBACK( on_tview_selection_changed ), page );

	gtk_widget_set_sensitive( tview, FALSE );
}

/*
 * two widgets (debit/credit) display the bank balance of the
 * account, by deducting the unreconciliated entries from the balance
 * in our book - this is supposed simulate the actual bank balance
 */
static void
setup_treeview_footer( ofaPage *page, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;

	priv = OFA_RECONCIL_PAGE( page )->priv;

	priv->select_debit = my_utils_container_get_child_by_name( parent, "select-debit" );
	g_return_if_fail( priv->select_debit && GTK_IS_LABEL( priv->select_debit ));
	my_utils_widget_set_style( priv->select_debit, "labelhelp" );

	priv->select_credit = my_utils_container_get_child_by_name( parent, "select-credit" );
	g_return_if_fail( priv->select_credit && GTK_IS_LABEL( priv->select_credit ));
	my_utils_widget_set_style( priv->select_credit, "labelhelp" );

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
setup_account_selection( ofaPage *page, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;
	GtkWidget *button;

	priv = OFA_RECONCIL_PAGE( page )->priv;

	priv->acc_id_entry = my_utils_container_get_child_by_name( parent, "account-number" );
	g_return_if_fail( priv->acc_id_entry && GTK_IS_ENTRY( priv->acc_id_entry ));
	g_signal_connect(
			priv->acc_id_entry, "changed", G_CALLBACK( on_account_entry_changed ), page );

	button = my_utils_container_get_child_by_name( parent, "account-select" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( button, "clicked", G_CALLBACK( on_account_select_clicked ), page );

	priv->acc_label = my_utils_container_get_child_by_name( parent, "account-label" );
	g_return_if_fail( priv->acc_label && GTK_IS_LABEL( priv->acc_label ));
	my_utils_widget_set_style( priv->acc_label, "labelnormal" );
}

/*
 * the combo box for filtering the displayed entries
 */
static void
setup_entries_filter( ofaPage *page, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;
	GtkWidget *columns, *combo, *label;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeIter iter;
	gint i;

	priv = OFA_RECONCIL_PAGE( page )->priv;

	columns = my_utils_container_get_child_by_name( parent, "f2-columns" );
	g_return_if_fail( columns && GTK_IS_CONTAINER( columns ));
	ofa_itreeview_display_attach_menu_button( OFA_ITREEVIEW_DISPLAY( page ), GTK_CONTAINER( columns ));
	//g_signal_connect( self, "ofa-toggled", G_CALLBACK( on_column_toggled ), NULL );
	ofa_itreeview_display_init_visible( OFA_ITREEVIEW_DISPLAY( page ), st_pref_columns );

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

	g_signal_connect(
			G_OBJECT( priv->mode_combo ),
			"changed", G_CALLBACK( on_mode_combo_changed ), page );
}

static void
setup_date_filter( ofaPage *page, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;
	GtkWidget *filter_parent;

	priv = OFA_RECONCIL_PAGE( page )->priv;

	priv->effect_filter = ofa_date_filter_hv_bin_new();
	ofa_idate_filter_set_prefs( OFA_IDATE_FILTER( priv->effect_filter ), st_effect_dates );

	filter_parent = my_utils_container_get_child_by_name( parent, "effect-date-filter" );
	g_return_if_fail( filter_parent && GTK_IS_CONTAINER( filter_parent ));
	gtk_container_add( GTK_CONTAINER( filter_parent ), GTK_WIDGET( priv->effect_filter ));

	g_signal_connect(
			priv->effect_filter, "ofa-focus-out", G_CALLBACK( on_effect_dates_changed ), page );
}

static void
setup_manual_rappro( ofaPage *page, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;
	GtkWidget *entry, *label;

	priv = OFA_RECONCIL_PAGE( page )->priv;

	entry = my_utils_container_get_child_by_name( parent, "manual-date" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->date_concil = GTK_ENTRY( entry );

	label = my_utils_container_get_child_by_name( parent, "manual-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	label = my_utils_container_get_child_by_name( parent, "manual-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_editable_date_init( GTK_EDITABLE( priv->date_concil ));
	my_editable_date_set_format( GTK_EDITABLE( priv->date_concil ), ofa_prefs_date_display());
	my_editable_date_set_date( GTK_EDITABLE( priv->date_concil ), &priv->dconcil );
	my_editable_date_set_label( GTK_EDITABLE( priv->date_concil ), label, ofa_prefs_date_check());

	g_signal_connect(
			G_OBJECT( priv->date_concil ), "changed", G_CALLBACK( on_date_concil_changed ), page );
}

/*
 * setup a size group between effect dates filter and manual
 * reconciliation to get the entries aligned
 */
static void
setup_size_group( ofaPage *page, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;
	GtkSizeGroup *group;
	GtkWidget *label;

	priv = OFA_RECONCIL_PAGE( page )->priv;
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
setup_auto_rappro( ofaPage *page, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;
	GtkWidget *button, *label;

	priv = OFA_RECONCIL_PAGE( page )->priv;

	button = my_utils_container_get_child_by_name( parent, "assist-select" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_select_bat ), page );

	button = my_utils_container_get_child_by_name( parent, "assist-import" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_import_clicked ), page );

	button = my_utils_container_get_child_by_name( parent, "assist-clear" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_clear_clicked ), page );
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
setup_action_buttons( ofaPage *page, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;
	GtkWidget *button;

	priv = OFA_RECONCIL_PAGE( page )->priv;

	priv->actions_frame = my_utils_container_get_child_by_name( parent, "f6-actions" );
	g_return_if_fail( priv->actions_frame && GTK_IS_FRAME( priv->actions_frame ));

	button = my_utils_container_get_child_by_name( parent, "reconciliate-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect(
			G_OBJECT( button ), "clicked", G_CALLBACK( on_reconciliate_clicked ), page );
	priv->reconciliate_btn = button;

	button = my_utils_container_get_child_by_name( parent, "decline-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect(
			G_OBJECT( button ), "clicked", G_CALLBACK( on_decline_clicked ), page );
	priv->decline_btn = button;

	button = my_utils_container_get_child_by_name( parent, "unreconciliate-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect(
			G_OBJECT( button ), "clicked", G_CALLBACK( on_unreconciliate_clicked ), page );
	priv->unreconciliate_btn = button;

	button = my_utils_container_get_child_by_name( parent, "print-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect(
			G_OBJECT( button ), "clicked", G_CALLBACK( on_print_clicked ), page );
	priv->print_btn = button;
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_RECONCIL_PAGE( page ), NULL );

	return( GTK_WIDGET( OFA_RECONCIL_PAGE( page )->priv->tview ));
}

/*
 * the treeview is disabled (insensitive) while the account is not ok
 * (and priv->account is NULL)
 */
static void
on_account_entry_changed( GtkEntry *entry, ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_on_account_entry_changed";
	ofaReconcilPagePrivate *priv;
	GtkTreeSelection *select;

	priv = self->priv;

	priv->acc_precision = 0;
	gtk_label_set_text( GTK_LABEL( priv->acc_label ), "" );
	priv->account = get_reconciliable_account( self );

	g_debug( "%s: entry=%p, self=%p, account=%p",
			thisfn, ( void * ) entry, ( void * ) self, ( void * ) priv->account );

	if( priv->account ){
		clear_account_content( self );
		set_account_balance( self );
		do_display_entries( self );
		set_reconciliated_balance( self );
	}

	check_for_enable_view( self );

	select = gtk_tree_view_get_selection( priv->tview );
	on_tview_selection_changed( select, self );
}

static void
on_account_select_clicked( GtkButton *button, ofaReconcilPage *self )
{
	do_account_selection( self );
}

/*
 * setting the entry (gtk_entry_set_text) will trigger a 'changed'
 * message, which itself will update the account properties in the
 * dialog
 */
static void
do_account_selection( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	const gchar *account_number;
	gchar *number;

	priv = self->priv;

	account_number = gtk_entry_get_text( GTK_ENTRY( priv->acc_id_entry ));
	if( !my_strlen( account_number )){
		account_number = st_default_reconciliated_class;
	}

	number = ofa_account_select_run(
					ofa_page_get_main_window( OFA_PAGE( self )),
					account_number,
					ACCOUNT_ALLOW_RECONCILIABLE );

	if( my_strlen( number )){
		gtk_entry_set_text( GTK_ENTRY( priv->acc_id_entry ), number );
	}

	g_free( number );
}

/*
 *  reset all the account-related content
 * remove all entries from the treeview
 */
static void
clear_account_content( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;

	priv = self->priv;
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
get_reconciliable_account( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	const gchar *number;
	gboolean ok;
	ofoAccount *account;
	const gchar *label, *cur_code;
	ofoCurrency *currency;

	priv = self->priv;
	ok = FALSE;

	number = gtk_entry_get_text( GTK_ENTRY( priv->acc_id_entry ));
	account = ofo_account_get_by_number( priv->dossier, number );

	if( account ){
		g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

		ok = !ofo_account_is_root( account ) &&
				!ofo_account_is_closed( account ) &&
				ofo_account_is_reconciliable( account );
	}

	/* init account label */
	label = account ? ofo_account_get_label( account ) : "";
	gtk_label_set_text( GTK_LABEL( priv->acc_label ), label );
	if( ok ){
		my_utils_widget_remove_style( priv->acc_label, "labelerror" );
	} else {
		my_utils_widget_set_style( priv->acc_label, "labelerror" );
	}

	if( ok ){
		/* init account precision depending of the currency */
		cur_code = ofo_account_get_currency( account );
		g_return_val_if_fail( my_strlen( cur_code ), NULL );
		currency = ofo_currency_get_by_code( priv->dossier, cur_code );
		g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), NULL );
		priv->acc_precision = ofo_currency_get_precision( currency );

		/* only update user preferences if account is ok */
		set_settings( self );
	}

	return( ok ? account : NULL );
}

/*
 * set the treeview header with the account balance
 * this is called when changing to a valid account
 * or when remediating to an event signaled by through the dossier
 */
static void
set_account_balance( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	gchar *sdiff, *samount;

	priv = self->priv;

	if( priv->account ){
		priv->acc_debit = ofo_account_get_val_debit( priv->account )
				+ ofo_account_get_rough_debit( priv->account );
		priv->acc_credit = ofo_account_get_val_credit( priv->account )
				+ ofo_account_get_rough_credit( priv->account );

		if( priv->acc_credit >= priv->acc_debit ){
			sdiff = my_double_to_str( priv->acc_credit - priv->acc_debit );
			samount = g_strdup_printf( _( "%s CR" ), sdiff );
			gtk_label_set_text( GTK_LABEL( priv->acc_credit_label ), samount );
		} else {
			sdiff = my_double_to_str( priv->acc_debit - priv->acc_credit );
			samount = g_strdup_printf( _( "%s DB" ), sdiff );
			gtk_label_set_text( GTK_LABEL( priv->acc_debit_label ), samount );
		}
		g_free( sdiff );
		g_free( samount );
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

	priv = self->priv;

	entries = ofo_entry_get_dataset_by_account(
					priv->dossier, ofo_account_get_number( priv->account ));

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

	priv = self->priv;

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

	priv = self->priv;

	sdope = my_date_to_str( ofo_entry_get_dope( entry ), ofa_prefs_date_display());
	amount = ofo_entry_get_debit( entry );
	if( amount ){
		sdeb = my_double_to_str( amount );
	} else {
		sdeb = g_strdup( "" );
	}
	amount = ofo_entry_get_credit( entry );
	if( amount ){
		scre = my_double_to_str( amount );
	} else {
		scre = g_strdup( "" );
	}
	concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ), priv->dossier );
	dconcil = concil ? ofo_concil_get_dval( concil ) : NULL;
	sdrap = my_date_to_str( dconcil, ofa_prefs_date_display());
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

	priv = self->priv;
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

	priv = self->priv;

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

	priv = self->priv;

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

	priv = self->priv;

	my_date_set_from_date( &date, my_editable_date_get_date( editable, &valid ));
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
 * select an already imported Bank Account Transaction list file
 *
 * Only reset the BAT lines id we try to load another BAT file
 * Hitting Cancel on BAT selection doesn't change anything
 */
static void
do_select_bat( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	ofxCounter prev_id, bat_id;

	priv = self->priv;
	prev_id = priv->bats ? ofo_bat_get_id( OFO_BAT( priv->bats->data )) : -1;
	bat_id = ofa_bat_select_run( ofa_page_get_main_window( OFA_PAGE( self )), prev_id );
	if( bat_id > 0 ){
		display_bat_by_id( self, bat_id );
	}
}

/*
 * try to import a bank account transaction list
 *
 * As coming here means that the user has selected a file, then we
 * begin with clearing the current bat lines (even if the import may
 * be unsuccessful)
 */
static void
on_import_clicked( GtkButton *button, ofaReconcilPage *self )
{
	ofxCounter imported_id;

	imported_id = ofa_bat_utils_import( ofa_page_get_main_window( OFA_PAGE( self )));
	if( imported_id > 0 ){
		display_bat_by_id( self, imported_id );
	}
}

static void
on_clear_clicked( GtkButton *button, ofaReconcilPage *self )
{
	clear_bat_content( self );
}

/*
 *  clear the proposed reconciliations from the model before displaying
 *  the just imported new ones
 *
 *  this means not only removing old BAT lines, but also resetting the
 *  proposed reconciliation date in the entries
 */
static void
clear_bat_content( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;

	priv = self->priv;

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

	priv = self->priv;

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

	priv = self->priv;

	for( it=priv->bats ; it ; it=it->next ){
		bat = ( ofoBat * ) it->data;
		if( ofo_bat_get_id( bat ) == bat_id ){
			my_utils_dialog_warning( _( "The selected BAT file is already loaded" ));
			return;
		}
	}

	bat = ofo_bat_get_by_id( priv->dossier, bat_id );
	if( bat ){
		priv->bats = g_list_prepend( priv->bats, bat );
		display_bat_file( self, bat );
	}
}

static void
display_bat_file( ofaReconcilPage *self, ofoBat *bat )
{
	ofaReconcilPagePrivate *priv;
	GList *batlines, *it;

	priv = self->priv;

	batlines = ofo_bat_line_get_dataset( priv->dossier, ofo_bat_get_id( bat ));
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

	priv = self->priv;

	if( search_for_parent_by_concil( self, OFO_BASE( batline ), priv->tstore, &parent_iter )){
		gtk_tree_store_insert( GTK_TREE_STORE( priv->tstore ), &insert_iter, &parent_iter, -1 );
		set_row_batline( self, priv->tstore, &insert_iter, batline );

	} else if( search_for_parent_by_amount( self, OFO_BASE( batline ), priv->tstore, &parent_iter )){
		gtk_tree_model_get( priv->tstore, &parent_iter, COL_OBJECT, &object, -1 );
		g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
		g_object_unref( object );
		gtk_tree_store_insert( GTK_TREE_STORE( priv->tstore ), &insert_iter, &parent_iter, -1 );
		set_row_batline( self, priv->tstore, &insert_iter, batline );
		update_concil_data_by_iter( self, priv->tstore, &parent_iter, 0, ofo_bat_line_get_deffect( batline ));
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

	priv = self->priv;

	bat_amount = ofo_bat_line_get_amount( batline );
	if( bat_amount < 0 ){
		sdeb = my_double_to_str( -bat_amount );
		scre = g_strdup( "" );
	} else {
		sdeb = g_strdup( "" );
		scre = my_double_to_str( bat_amount );
	}

	dope = get_bat_line_dope( self, batline );
	sdope = my_date_to_str( dope, ofa_prefs_date_display());
	sid = g_strdup( "" );
	sdreconcil = g_strdup( "" );
	concil = ofa_iconcil_get_concil( OFA_ICONCIL( batline ), priv->dossier );
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

	priv = self->priv;
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
	gint total, used, unused;

	priv = self->priv;

	total = 0;
	used = 0;
	for( it=priv->bats ; it ; it=it->next ){
		total += ofo_bat_get_lines_count( OFO_BAT( it->data ), priv->dossier );
		used += ofo_bat_get_used_count( OFO_BAT( it->data ), priv->dossier );
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

	cmp = 0;
	priv = self->priv;

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
			concil_a = ofa_iconcil_get_concil( OFA_ICONCIL( object_a ), priv->dossier );
			id_a = concil_a ? ofo_concil_get_id( concil_a ) : 0;
			concil_b = ofa_iconcil_get_concil( OFA_ICONCIL( object_b ), priv->dossier );
			id_b = concil_b ? ofo_concil_get_id( concil_b ) : 0;
			cmp = id_a < id_b ? -1 : ( id_a > id_b ? 1 : 0 );
			break;
		case COL_DRECONCIL:
			concil_a = OFO_IS_ENTRY( object_a ) ?
					ofa_iconcil_get_concil(
							OFA_ICONCIL( object_a ), priv->dossier ) :
						NULL;
			date_a = concil_a ? ofo_concil_get_dval( concil_a ) : NULL;
			concil_b = OFO_IS_ENTRY( object_b ) ?
					ofa_iconcil_get_concil(
							OFA_ICONCIL( object_b ), priv->dossier ) :
					NULL;
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
is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	gboolean visible, ok;
	GObject *object;
	const GDate *deffect, *filter;

	priv = self->priv;
	gtk_tree_model_get( tmodel, iter, COL_OBJECT, &object, -1 );
	/* as we insert the row before populating it, it may happen that
	 * the object be not set */
	if( !object || ofo_dossier_has_dispose_run( priv->dossier )){
		return( FALSE );
	}
	g_return_val_if_fail( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object ), FALSE );
	g_object_unref( object );

	visible = OFO_IS_ENTRY( object ) ?
			is_visible_entry( self, tmodel, iter, OFO_ENTRY( object )) :
			is_visible_batline( self, OFO_BAT_LINE( object ));

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
		/* ... against upper limit */
		filter = ofa_idate_filter_get_date(
				OFA_IDATE_FILTER( priv->effect_filter ), IDATE_FILTER_TO );
		ok = !my_date_is_valid( filter ) ||
				my_date_compare( filter, deffect ) >= 0;
		visible &= ok;
	}

	return( visible );
}

static gboolean
is_visible_entry( ofaReconcilPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoEntry *entry )
{
	ofaReconcilPagePrivate *priv;
	gboolean visible;
	ofoConcil *concil;
	const gchar *selected_account, *entry_account;

	priv = self->priv;

	/* do not display deleted entries */
	if( ofo_entry_get_status( entry ) == ENT_STATUS_DELETED ){
		return( FALSE );
	}

	/* check account is right
	 * do not rely on the initial dataset query as we may have inserted
	 *  a new entry */
	selected_account = gtk_entry_get_text( GTK_ENTRY( priv->acc_id_entry ));
	entry_account = ofo_entry_get_account( entry );
	if( g_utf8_collate( selected_account, entry_account )){
		return( FALSE );
	}

	concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ), priv->dossier );
	visible = TRUE;

	switch( priv->mode ){
		case ENT_CONCILED_ALL:
			/*g_debug( "%s: mode=%d, visible=True", thisfn, mode );*/
			break;
		case ENT_CONCILED_YES:
			visible = ( concil != NULL );
			/*g_debug( "%s: mode=%d, visible=%s", thisfn, mode, visible ? "True":"False" );*/
			break;
		case ENT_CONCILED_NO:
			visible = ( concil == NULL );
			/*g_debug( "%s: mode=%d, visible=%s", thisfn, mode, visible ? "True":"False" );*/
			break;
		case ENT_CONCILED_SESSION:
			if( concil ){
				visible = is_entry_session_conciliated( self, entry, concil );
			} else {
				visible = TRUE;
			}
			/*g_debug( "%s: mode=%d, visible=%s", thisfn, mode, visible ? "True":"False" );*/
			break;
		default:
			/* when display mode is not set */
			visible = FALSE;
	}

	return( visible );
}

/*
 * bat lines are visible with the same criteria that the entries
 * even when reconciliated, it keeps to be displayed besides of its
 * entry
 */
static gboolean
is_visible_batline( ofaReconcilPage *self, ofoBatLine *batline )
{
	ofaReconcilPagePrivate *priv;
	gboolean visible;
	ofoConcil *concil;
	GList *ids;
	ofxCounter number;
	GtkTreeIter *iter;
	GtkTreeModel *tstore;
	ofoEntry *entry;

	priv = self->priv;
	visible = FALSE;
	concil = ofa_iconcil_get_concil( OFA_ICONCIL( batline ), priv->dossier );

	switch( priv->mode ){
		case ENT_CONCILED_ALL:
			/*g_debug( "%s: mode=%d, visible=True", thisfn, mode );*/
			visible = TRUE;
			break;
		case ENT_CONCILED_YES:
			visible = ( concil != NULL );
			/*g_debug( "%s: mode=%d, visible=%s", thisfn, mode, visible ? "True":"False" );*/
			break;
		case ENT_CONCILED_NO:
			visible = ( concil == NULL );
			/*g_debug( "%s: mode=%d, visible=%s", thisfn, mode, visible ? "True":"False" );*/
			break;
		case ENT_CONCILED_SESSION:
			if( !concil ){
				visible = TRUE;
			} else {
				ids = ofo_concil_get_ids( concil );
				number = ofs_concil_id_get_first( ids, CONCIL_TYPE_ENTRY );
				iter = search_for_entry_by_number( self, number );
				if( iter ){
					tstore = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));
					gtk_tree_model_get( tstore, iter, COL_OBJECT, &entry, -1 );
					g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
					g_object_unref( entry );
					gtk_tree_iter_free( iter );
					visible = is_entry_session_conciliated( self, entry, NULL );
				}
			}
			/*g_debug( "%s: mode=%d, visible=%s", thisfn, mode, visible ? "True":"False" );*/
			break;
		default:
			/* when display mode is not set */
			break;
	}

	return( visible );
}

static gboolean
is_entry_session_conciliated( ofaReconcilPage *self, ofoEntry *entry, ofoConcil *concil )
{
	ofaReconcilPagePrivate *priv;
	gboolean is_session;
	const GTimeVal *stamp;
	GDate date, dnow;

	priv = self->priv;

	if( !concil ){
		concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ), priv->dossier );
	}
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
	ofaReconcilPagePrivate *priv;
	GObject *object;
	GdkRGBA color;
	gint id;
	ofoConcil *concil;

	priv = self->priv;

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
	concil =  ofa_iconcil_get_concil( OFA_ICONCIL( object ), priv->dossier );

	if( !concil && id == COL_DRECONCIL && gtk_tree_model_iter_has_child( tmodel, iter )){
		gdk_rgba_parse( &color, COLOR_BAT_UNCONCIL_FONT );
		g_object_set( G_OBJECT( cell ), "foreground-rgba", &color, NULL );
		g_object_set( G_OBJECT( cell ), "style", PANGO_STYLE_ITALIC, NULL );
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
	guint unconcil_rows;
	gboolean is_child, unique;
	gchar *sdeb, *scre;

	priv = self->priv;

	concil_enabled = FALSE;
	decline_enabled = FALSE;
	unreconciliate_enabled = FALSE;
	priv->activate_action = ACTIV_NONE;

	examine_selection( self,
			&debit, &credit, &concil, &unconcil_rows, &unique, &is_child );

	sdeb = my_double_to_str( debit );
	gtk_label_set_text( GTK_LABEL( priv->select_debit ), sdeb );
	g_free( sdeb );
	scre = my_double_to_str( credit );
	gtk_label_set_text( GTK_LABEL( priv->select_credit ), scre );
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
 * @concil_rows: count of conciliated rows
 * @unconcil_rows: count of unconciliated rows
 * @unique: whether we have more than one non-null conciliation id
 * @is_child: whether all rows of the selection are a child
 *  (most useful when selecting only one child to decline it)
 */
static void
examine_selection( ofaReconcilPage *self,
		ofxAmount *debit, ofxAmount *credit,
		ofoConcil **concil, guint *unconcil_rows, gboolean *unique, gboolean *is_child )
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

	priv = self->priv;
	concil_id = 0;

	*debit = 0;
	*credit = 0;
	*concil = NULL;
	*unconcil_rows = 0;
	*unique = TRUE;
	*is_child = TRUE;

	select = gtk_tree_view_get_selection( priv->tview );
	selected = gtk_tree_selection_get_selected_rows( select, &tmodel );

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
		row_concil = ofa_iconcil_get_concil( OFA_ICONCIL( object ), priv->dossier );
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
	expand_tview_selection( self );
	switch( self->priv->activate_action ){
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
 * TRUE to stop other handlers from being invoked for the event.
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

	priv = self->priv;

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

	priv = self->priv;
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
			concil = ofa_iconcil_get_concil( OFA_ICONCIL( object ), priv->dossier );
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

	priv = self->priv;
	selection = gtk_tree_view_get_selection( priv->tview );
	selected = gtk_tree_selection_get_selected_rows( selection, &tmodel );
	if( g_list_length( selected ) == 1 ){
		start = &iter;
		gtk_tree_model_get_iter( tmodel, start, ( GtkTreePath * ) selected->data );
		if( gtk_tree_model_iter_parent( tmodel, &parent, start )){
			gtk_tree_selection_select_iter( selection, &parent );
			start = &parent;
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
 */
static void
do_reconciliate( ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_do_reconciliate";
	ofaReconcilPagePrivate *priv;
	ofxAmount debit, credit;
	guint unconcil_rows;
	gboolean is_child, unique;
	ofoConcil *concil;
	GDate dval;
	GtkTreeSelection *selection;
	GList *selected, *it;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *parent, *ent_parent_path, *bat_parent_path;
	ofoBase *object;
	ofxCounter concil_id;
	gint depth;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = self->priv;

	examine_selection( self, &debit, &credit, &concil, &unconcil_rows, &unique, &is_child );
	g_return_if_fail( unique );

	/* compute effect date of a new concil group */
	if( !concil ){
		if( !my_date_is_valid( get_date_for_new_concil( self, &dval ))){
			my_utils_dialog_warning(
					_( "Conciliation is cancelled because "
						"unable to get a valid conciliation "
						"effect date" ));
			return;
		}
	} else {
		my_date_set_from_date( &dval, ofo_concil_get_dval( concil ));
	}
	concil_id = concil ? ofo_concil_get_id( concil ) : 0;

	/* add each line to this same concil group */
	selection = gtk_tree_view_get_selection( priv->tview );
	selected = gtk_tree_selection_get_selected_rows( selection, &tmodel );
	ent_parent_path = NULL;
	bat_parent_path = NULL;
	for( it=selected ; it ; it=it->next ){
		gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) it->data );
		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &object, -1 );
		g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
		g_object_unref( object );

		if( !concil ){
			concil = ofa_iconcil_new_concil( OFA_ICONCIL( object ), &dval, priv->dossier );
			concil_id = ofo_concil_get_id( concil );
		}
		if( !ofa_iconcil_get_concil( OFA_ICONCIL( object ), priv->dossier )){
			ofa_iconcil_add_to_concil( OFA_ICONCIL( object ), concil, priv->dossier );
		}
		update_concil_data_by_path( self, ( GtkTreePath * ) it->data, concil_id, &dval );

		/* search for the first toplevel entry
		 * defaulting to the first toplevel batline */
		depth = gtk_tree_path_get_depth(( GtkTreePath * ) it->data );
		if( !ent_parent_path && OFO_IS_ENTRY( object ) && depth == 1 ){
			ent_parent_path = gtk_tree_path_copy(( GtkTreePath * ) it->data );
		}
		if( !bat_parent_path && OFO_IS_BAT_LINE( object ) && depth == 1 ){
			bat_parent_path = gtk_tree_path_copy(( GtkTreePath * ) it->data );
		}
	}

	/* review the hierarchy:
	 * keep the toplevel parent, all other rows become children of it */
	parent = ent_parent_path ? ent_parent_path : bat_parent_path;
	parent = do_reconciliate_review_hierarchy( self, selected, parent );
	if( ent_parent_path ){
		gtk_tree_path_free( ent_parent_path );
	}
	if( bat_parent_path ){
		gtk_tree_path_free( bat_parent_path );
	}

	/* last re-selected the head of the hierarchy
	 * keeping the hierarchy expanded to be visible by the user */
	gtk_tree_selection_unselect_all( selection );
	gtk_tree_selection_select_path( selection, parent );
	gtk_tree_model_get_iter( tmodel, &iter, parent );
	collapse_node_by_iter( self, priv->tview, tmodel, &iter );

	gtk_tree_path_free( parent );
	g_list_free_full( selected, ( GDestroyNotify ) gtk_tree_path_free );

	set_reconciliated_balance( self );
	if( priv->bats ){
		display_bat_counts( self );
	}
}

/*
 * review the hierarchy:
 * keep the specified toplevel parent, all other selected rows becoming
 * children of it; we use GtkTreeRowReference because this requires a
 * reorganization of all the selection
 */
static GtkTreePath *
do_reconciliate_review_hierarchy( ofaReconcilPage *self, GList *selected, GtkTreePath *parent )
{
	ofaReconcilPagePrivate *priv;
	GtkTreeRowReference *parent_ref, *ref;
	GList *refs, *it;
	GtkTreePath *it_path, *parent_path;
	GtkTreeIter sort_iter, filter_iter, store_iter, parent_iter;
	ofoBase *object;

	priv = self->priv;
	parent_ref = gtk_tree_row_reference_new( priv->tsort, parent );

	/* build a list of row refs */
	refs = NULL;
	for( it=selected ; it ; it=it->next ){
		ref = gtk_tree_row_reference_new( priv->tsort, ( GtkTreePath * ) it->data );
		refs = g_list_prepend( refs, ref );
	}

	/* iter through the list of refs */
	for( it=refs ; it ; it=it->next ){
		it_path = gtk_tree_row_reference_get_path(( GtkTreeRowReference * ) it->data );
		parent_path = gtk_tree_row_reference_get_path( parent_ref );

		if( gtk_tree_path_compare( it_path, parent_path ) != 0 &&
			 !gtk_tree_path_is_descendant( it_path, parent_path )){

			gtk_tree_model_get_iter( priv->tsort, &sort_iter, it_path );
			gtk_tree_model_sort_convert_iter_to_child_iter(
					GTK_TREE_MODEL_SORT( priv->tsort ), &filter_iter, &sort_iter );
			gtk_tree_model_filter_convert_iter_to_child_iter(
					GTK_TREE_MODEL_FILTER( priv->tfilter ), &store_iter, &filter_iter );

			gtk_tree_model_get_iter( priv->tsort, &sort_iter, parent_path );
			gtk_tree_model_sort_convert_iter_to_child_iter(
					GTK_TREE_MODEL_SORT( priv->tsort ), &filter_iter, &sort_iter );
			gtk_tree_model_filter_convert_iter_to_child_iter(
					GTK_TREE_MODEL_FILTER( priv->tfilter ), &parent_iter, &filter_iter );

			gtk_tree_model_get( priv->tstore, &store_iter, COL_OBJECT, &object, -1 );
			g_return_val_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )), NULL );
			gtk_tree_store_remove( GTK_TREE_STORE( priv->tstore ), &store_iter );
			gtk_tree_store_insert( GTK_TREE_STORE( priv->tstore ), &store_iter, &parent_iter, -1 );
			if( OFO_IS_ENTRY( object )){
				set_row_entry( self, priv->tstore, &store_iter, OFO_ENTRY( object ));
			} else {
				set_row_batline( self, priv->tstore, &store_iter, OFO_BAT_LINE( object ));
			}
			g_object_unref( object );
		}

		gtk_tree_path_free( parent_path );
		gtk_tree_path_free( it_path );
	}

	parent_path = gtk_tree_row_reference_get_path( parent_ref );

	gtk_tree_row_reference_free( parent_ref );
	g_list_free_full( refs, ( GDestroyNotify ) gtk_tree_row_reference_free );

	return( parent_path );
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

	priv = self->priv;
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

	priv = self->priv;

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

	update_concil_data_by_iter( self, priv->tstore, &parent_iter, 0, NULL );

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
	guint unconcil_rows;
	gboolean is_child, unique;
	ofoConcil *concil;
	GtkTreeSelection *selection;
	GList *selected, *it;
	GtkTreeModel *tmodel;
	GtkTreeIter iter, filter_iter, store_iter, child_iter;
	GDate dval;
	ofoBase *object;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = self->priv;

	examine_selection( self, &debit, &credit, &concil, &unconcil_rows, &unique, &is_child );
	g_return_if_fail( unique );
	g_return_if_fail( concil );

	my_date_clear( &dval );
	ofa_iconcil_remove_concil( concil, priv->dossier );

	selection = gtk_tree_view_get_selection( priv->tview );
	selected = gtk_tree_selection_get_selected_rows( selection, &tmodel );

	/* first iterate to find a candidate value date */
	for( it=selected ; it ; it=it->next ){
		gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) it->data );
		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &object, -1 );
		g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
		g_object_unref( object );
		if( OFO_IS_BAT_LINE( object )){
			my_date_set_from_date( &dval, ofo_bat_line_get_deffect( OFO_BAT_LINE( object )));
			break;
		}
	}

	/* second iterate to set this candidate date (if found)
	 * also run into children even if they are not in the selection
	 * (because they may not be visible) */
	for( it=selected ; it ; it=it->next ){
		gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) it->data );
		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &object, -1 );
		g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
		g_object_unref( object );

		gtk_tree_model_sort_convert_iter_to_child_iter(
				GTK_TREE_MODEL_SORT( priv->tsort ), &filter_iter, &iter );
		gtk_tree_model_filter_convert_iter_to_child_iter(
				GTK_TREE_MODEL_FILTER( priv->tfilter ), &store_iter, &filter_iter );

		if( OFO_IS_ENTRY( object )){
			update_concil_data_by_iter( self, priv->tstore, &store_iter, 0, &dval );
		} else {
			update_concil_data_by_iter( self, priv->tstore, &store_iter, 0, NULL );
		}

		if( gtk_tree_model_iter_children( priv->tstore, &child_iter, &store_iter )){
			while( TRUE ){
				gtk_tree_model_get( priv->tstore, &child_iter, COL_OBJECT, &object, -1 );
				g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
				g_object_unref( object );
				update_concil_data_by_iter( self, priv->tstore, &child_iter, 0, NULL );
				if( !gtk_tree_model_iter_next( priv->tstore, &child_iter )){
					break;
				}
			}
		}
	}

	g_list_free_full( selected, ( GDestroyNotify ) gtk_tree_path_free );

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

	priv = self->priv;
	debit = 0;
	credit = 0;

	if( priv->account ){
		account_debit = ofo_account_get_val_debit( priv->account )
				+ ofo_account_get_rough_debit( priv->account );
		account_credit = ofo_account_get_val_credit( priv->account )
				+ ofo_account_get_rough_credit( priv->account );
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
						concil = ofa_iconcil_get_concil( OFA_ICONCIL( object ), priv->dossier );
						if( !concil ){
							debit += ofo_entry_get_debit( OFO_ENTRY( object ));
							credit += ofo_entry_get_credit( OFO_ENTRY( object ));
							/*g_debug( "label=%s, debit=%lf, credit=%lf, solde=%lf",
									ofo_entry_get_label( OFO_ENTRY( object )), debit, credit, debit-credit );*/
						}
					}
				} else {
					concil = ofa_iconcil_get_concil( OFA_ICONCIL( object ), priv->dossier );
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
		str = my_double_to_str( debit-credit );
		sdeb = g_strdup_printf( _( "%s DB" ), str );
		scre = g_strdup( "" );
	} else {
		sdeb = g_strdup( "" );
		str = my_double_to_str( credit-debit );
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
 * path comes from the current selection
 */
static void
update_concil_data_by_path( ofaReconcilPage *self, GtkTreePath *path, ofxCounter id, const GDate *dval )
{
	ofaReconcilPagePrivate *priv;
	GtkTreeIter sort_iter, filter_iter, store_iter;

	priv = self->priv;

	if( gtk_tree_model_get_iter( priv->tsort, &sort_iter, path )){
		gtk_tree_model_sort_convert_iter_to_child_iter(
				GTK_TREE_MODEL_SORT( priv->tsort ), &filter_iter, &sort_iter );
		gtk_tree_model_filter_convert_iter_to_child_iter(
				GTK_TREE_MODEL_FILTER( priv->tfilter ), &store_iter, &filter_iter );
		update_concil_data_by_iter( self, priv->tstore, &store_iter, id, dval );
	}
}

/*
 * update the conciliation id and date
 * the provided iter must be on the store model
 */
static void
update_concil_data_by_iter( ofaReconcilPage *self,
		GtkTreeModel *store_model, GtkTreeIter *store_iter, ofxCounter id, const GDate *dval )
{
	gchar *sid, *sdvaleur;

	g_return_if_fail( store_model && GTK_IS_TREE_STORE( store_model ));
	g_return_if_fail( store_iter );

	sid = id > 0 ? g_strdup_printf( "%ld", id ) : g_strdup( "" );
	sdvaleur = my_date_to_str( dval, ofa_prefs_date_display());

	/* set the reconciliation date in the pointed-to row */
	gtk_tree_store_set(
			GTK_TREE_STORE( store_model ),
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
	ofaReconcilPagePrivate *priv;
	GtkTreeIter iter;
	gint *indices, depth;
	ofoBase *object;
	ofoConcil *concil;

	priv = self->priv;
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
	concil = ofa_iconcil_get_concil( OFA_ICONCIL( object ), priv->dossier );
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
	GtkTreeModel *child_tmodel;
	GtkTreeIter iter;
	GtkTreeIter *entry_iter;
	ofxCounter ecr_number;
	GObject *object;

	entry_iter = NULL;
	child_tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->priv->tfilter ));

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

	priv = self->priv;

	if( ofa_iconcil_get_concil( OFA_ICONCIL( object ), priv->dossier )){
		return( FALSE );
	}

	//const gchar *lab = OFO_IS_BAT_LINE( object ) ? ofo_bat_line_get_label( OFO_BAT_LINE( object )) : NULL;
	//gboolean is_debug = lab ? g_str_has_prefix( lab, "CB DECATHLON 1" ) : FALSE;
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
				if( !ofa_iconcil_get_concil( OFA_ICONCIL( obj ), priv->dossier )){
					if( OFO_IS_ENTRY( obj )){
						iter_balance = ofo_entry_get_credit( OFO_ENTRY( obj ))
											- ofo_entry_get_debit( OFO_ENTRY( obj ));
					} else {
						iter_balance = ofo_bat_line_get_amount( OFO_BAT_LINE( obj ));
					}
					if( is_debug ){
						g_debug( "%s: iter_balance=%lf", thisfn, iter_balance );
					}
					if( fabs( object_balance+iter_balance ) < priv->acc_precision ){
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
	ofaReconcilPagePrivate *priv;
	ofoConcil *concil;
	ofxCounter object_id;
	ofoBase *obj;

	g_return_val_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )), FALSE );

	priv = self->priv;

	concil = ofa_iconcil_get_concil( OFA_ICONCIL( object ), priv->dossier );
	if( concil ){
		object_id = ofo_concil_get_id( concil );
		if( gtk_tree_model_get_iter_first( tmodel, iter )){
			while( TRUE ){
				gtk_tree_model_get( tmodel, iter, COL_OBJECT, &obj, -1 );
				g_return_val_if_fail( obj && ( OFO_IS_ENTRY( obj ) || OFO_IS_BAT_LINE( obj )), FALSE );
				g_object_unref( obj );
				concil = ofa_iconcil_get_concil( OFA_ICONCIL( obj ), priv->dossier );
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
 * This is called at the end of the view setup: all widgets are defined,
 * and triggers are connected.
 *
 * settings format: account;mode;manualconcil[sql];
 */
static void
get_settings( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	GList *slist, *it;
	const gchar *cstr;
	GDate date;
	gchar *sdate;

	priv = self->priv;

	slist = ofa_settings_get_string_list( st_reconciliation );
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
	gchar *smode, *str;

	priv = self->priv;

	account = gtk_entry_get_text( GTK_ENTRY( priv->acc_id_entry ));

	smode = g_strdup_printf( "%d", priv->mode );

	sdate = gtk_entry_get_text( priv->date_concil );
	my_date_set_from_str( &date, sdate, ofa_prefs_date_display());
	if( my_date_is_valid( &date )){
		date_sql = my_date_to_str( &date, MY_DATE_SQL );
	} else {
		date_sql = g_strdup( "" );
	}

	str = g_strdup_printf( "%s;%s;%s;", account ? account : "", smode, date_sql );

	ofa_settings_set_string( st_reconciliation, str );

	g_free( date_sql );
	g_free( str );
	g_free( smode );
}

static void
dossier_signaling_connect( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	gulong handler;

	priv = self->priv;

	handler = g_signal_connect( priv->dossier,
					SIGNAL_DOSSIER_NEW_OBJECT, G_CALLBACK( on_dossier_new_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->dossier,
					SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_dossier_updated_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );
}

static void
on_dossier_new_object( ofoDossier *dossier, ofoBase *object, ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_on_dossier_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
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

	priv = self->priv;

	selected_account = gtk_entry_get_text( GTK_ENTRY( priv->acc_id_entry ));
	entry_account = ofo_entry_get_account( entry );
	if( !g_utf8_collate( selected_account, entry_account )){
		insert_new_entry( self, entry );
	}
}

static void
insert_new_entry( ofaReconcilPage *self, ofoEntry *entry )
{
	ofaReconcilPagePrivate *priv;
	GtkTreeModel *store;
	GtkTreeIter iter;

	priv = self->priv;
	store = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));
	gtk_tree_store_insert( GTK_TREE_STORE( store ), &iter, NULL, -1 );
	set_row_entry( self, store, &iter, entry );
	remediate_bat_lines( self, store, entry, &iter );
	gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( priv->tfilter ));
}

/*
 * search for a BAT line at level 0 (not yet member of a proposal nor
 * reconciliated) which would be suitable for the given entry
 */
static void
remediate_bat_lines( ofaReconcilPage *self, GtkTreeModel *tstore, ofoEntry *entry, GtkTreeIter *entry_iter )
{
	static const gchar *thisfn = "ofa_reconcil_page_remediate_bat_lines";
	ofaReconcilPagePrivate *priv;
	ofxAmount ent_debit, ent_credit, bat_amount;
	GtkTreeModel *store_model;
	GtkTreeIter iter;
	ofoBase *object;
	gchar *bat_sdeb, *bat_scre;

	priv = self->priv;
	ent_debit = ofo_entry_get_debit( entry );
	ent_credit = -1*ofo_entry_get_credit( entry );
	store_model = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));

	if( gtk_tree_model_get_iter_first( tstore, &iter )){
		while( TRUE ){
			gtk_tree_model_get( tstore, &iter, COL_OBJECT, &object, -1 );
			g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
			g_object_unref( object );

			/* search for a batline at level 0, not yet reconciliated,
			 * with the right amounts */
			if( OFO_IS_BAT_LINE( object ) /*&& !ofo_bat_line_is_used( OFO_BAT_LINE( object ))*/){
				bat_amount = ofo_bat_line_get_amount( OFO_BAT_LINE( object ));
				if(( bat_amount > 0 && bat_amount == ent_debit ) ||
						( bat_amount < 0 && bat_amount == ent_credit )){

					g_object_ref( object );
					gtk_tree_store_remove( GTK_TREE_STORE( tstore ), &iter );
					if( bat_amount < 0 ){
						bat_sdeb = my_double_to_str( -bat_amount );
						bat_scre = g_strdup( "" );
					} else {
						bat_sdeb = g_strdup( "" );
						bat_scre = my_double_to_str( bat_amount );
					}
					g_debug( "%s: entry found for bat_sdeb=%s, bat_scre=%s", thisfn, bat_sdeb, bat_scre );
					insert_batline( self, OFO_BAT_LINE( object ));
					update_concil_data_by_iter(
							self, store_model, entry_iter, 0, ofo_bat_line_get_deffect( OFO_BAT_LINE( object )));
					g_object_unref( object );
					g_free( bat_sdeb );
					g_free( bat_scre );
					break;
				}
			}

			if( !gtk_tree_model_iter_next( tstore, &iter )){
				break;
			}
		}
	}
}

/*
 * a ledger mnemo, an account number, a currency code may has changed
 */
static void
on_dossier_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_on_dossier_updated_object";

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, self=%p (%s)",
			thisfn,
			( void * ) dossier,
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
	GtkTreeModel *tstore;
	GtkTreeIter *iter;

	priv = self->priv;
	iter = search_for_entry_by_number( self, ofo_entry_get_number( entry ));

	/* if the entry was present in the store, it is easy to remediate it */
	if( iter ){
		tstore = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));
		set_row_entry( self, tstore, iter, entry );
		gtk_tree_iter_free( iter );
		gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( priv->tfilter ));

	/* else, should it be present now ? */
	} else {
		selected_account = gtk_entry_get_text( GTK_ENTRY( priv->acc_id_entry ));
		entry_account = ofo_entry_get_account( entry );
		if( !g_utf8_collate( selected_account, entry_account )){
			insert_new_entry( self, entry );
		}
	}
}

static void
on_print_clicked( GtkButton *button, ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	const gchar *acc_number;
	const ofaMainWindow *main_window;
	ofaPage *page;

	priv = self->priv;
	acc_number = gtk_entry_get_text( GTK_ENTRY( priv->acc_id_entry ));
	main_window = ofa_page_get_main_window( OFA_PAGE( self ));
	page = ofa_main_window_activate_theme( main_window, THM_RENDER_RECONCIL );
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

	if( !OFA_PAGE( page )->prot->dispose_has_run ){

		priv = page->priv;
		gtk_entry_set_text( GTK_ENTRY( priv->acc_id_entry ), number );
	}
}