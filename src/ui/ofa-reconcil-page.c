/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#define _GNU_SOURCE
#include <glib/gi18n.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "my/my-date-editable.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-account-editable.h"
#include "api/ofa-amount.h"
#include "api/ofa-date-filter-hv-bin.h"
#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-idate-filter.h"
#include "api/ofa-igetter.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-itheme-manager.h"
#include "api/ofa-itvcolumnable.h"
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
#include "api/ofs-currency.h"

#include "core/ofa-iconcil.h"

#include "ui/ofa-bat-select.h"
#include "ui/ofa-bat-utils.h"
#include "ui/ofa-reconcil-page.h"
#include "ui/ofa-reconcil-render.h"
#include "ui/ofa-reconcil-store.h"
#include "ui/ofa-reconcil-treeview.h"

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
	GtkWidget           *bat_label1;
	GtkWidget           *bat_unused_label;
	GtkWidget           *bat_count_label;
	GtkButton           *clear;

	/* UI - actions
	 */
	GtkWidget           *actions_frame;
	GSimpleAction       *decline_action;
	GSimpleAction       *reconciliate_action;
	GSimpleAction       *unreconciliate_action;
	GSimpleAction       *print_action;
	GSimpleAction       *expand_action;

	/* expand button:
	 * default is default expand
	 * when clicked with <Ctrl>, then expand all
	 */
	gboolean             ctrl_on_pressed;
	gboolean             ctrl_on_released;

	/* UI - entries view
	 */
	ofaReconcilStore    *store;
	ofaReconcilTreeview *tview;
	gint                 activate_action;

	/* UI
	 */
	GtkWidget           *msg_label;
	GtkWidget           *paned;
	gchar               *settings_prefix;

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
	gboolean             reading_settings;
}
	ofaReconcilPagePrivate;

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

static const gchar *st_resource_ui           = "/org/trychlos/openbook/ui/ofa-reconcil-page.ui";
static const gchar *st_resource_light_green  = "/org/trychlos/openbook/ui/light-green-14.png";
static const gchar *st_resource_light_yellow = "/org/trychlos/openbook/ui/light-yellow-14.png";
static const gchar *st_resource_light_empty  = "/org/trychlos/openbook/ui/light-empty-14.png";
static const gchar *st_ui_name1              = "ReconciliationView1";
static const gchar *st_ui_name2              = "ReconciliationView2";

static const gchar *st_default_reconciliated_class = "5"; /* default account class to be reconciliated */

#define DEBUG_FILTER                         FALSE
#define DEBUG_RECONCILIATE                   FALSE
#define DEBUG_UNCONCILIATE                   FALSE

static GtkWidget           *page_v_get_top_focusable_widget( const ofaPage *page );
static void                 paned_page_v_setup_view( ofaPanedPage *page, GtkPaned *paned );
static GtkWidget           *setup_view1( ofaReconcilPage *self );
static void                 setup_treeview_header( ofaReconcilPage *self, GtkContainer *parent );
static void                 setup_treeview( ofaReconcilPage *self, GtkContainer *parent );
static void                 setup_treeview_footer( ofaReconcilPage *self, GtkContainer *parent );
static gboolean             tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaReconcilPage *self );
static gboolean             tview_is_visible_entry( ofaReconcilPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoEntry *entry );
static gboolean             tview_is_visible_batline( ofaReconcilPage *self, ofoBatLine *batline );
static gboolean             tview_is_session_conciliated( ofaReconcilPage *self, ofoConcil *concil );
static void                 tview_on_selection_changed( ofaTVBin *treeview, GtkTreeSelection *selection, ofaReconcilPage *self );
static void                 tview_examine_selection( ofaReconcilPage *self, GList *selected, ofsCurrency *scurrency, guint *concil_rows, guint *unconcil_rows, gboolean *is_child );
static void                 tview_on_selection_activated( ofaTVBin *treeview, GtkTreeSelection *selection, ofaReconcilPage *self );
static void                 tview_expand_selection( ofaReconcilPage *self );
static GtkWidget           *setup_view2( ofaReconcilPage *self );
static void                 setup_account_selection( ofaReconcilPage *self, GtkContainer *parent );
static void                 setup_entries_filter( ofaReconcilPage *self, GtkContainer *parent );
static void                 setup_date_filter( ofaReconcilPage *self, GtkContainer *parent );
static void                 setup_manual_rappro( ofaReconcilPage *self, GtkContainer *parent );
static void                 setup_size_group( ofaReconcilPage *self, GtkContainer *parent );
static void                 setup_auto_rappro( ofaReconcilPage *self, GtkContainer *parent );
static void                 setup_actions( ofaReconcilPage *self, GtkContainer *parent );
static void                 paned_page_v_init_view( ofaPanedPage *page );
static void                 account_on_entry_changed( GtkEntry *entry, ofaReconcilPage *self );
static gchar               *account_on_preselect( GtkEditable *editable, ofeAccountAllowed allowed, ofaReconcilPage *self );
static void                 account_do_change( ofaReconcilPage *self );
static ofoAccount          *account_get_reconciliable( ofaReconcilPage *self, const gchar *number );
static void                 account_clear_content( ofaReconcilPage *self );
static void                 account_set_header_balance( ofaReconcilPage *self );
static void                 mode_filter_on_changed( GtkComboBox *box, ofaReconcilPage *self );
static void                 mode_filter_select( ofaReconcilPage *self, gint mode );
static void                 effect_dates_filter_on_changed( ofaIDateFilter *filter, gint who, gboolean empty, const GDate *date, ofaReconcilPage *self );
static void                 concil_date_on_changed( GtkEditable *editable, ofaReconcilPage *self );
static void                 bat_on_select_clicked( GtkButton *button, ofaReconcilPage *self );
static void                 bat_do_select( ofaReconcilPage *self );
static void                 bat_on_import_clicked( GtkButton *button, ofaReconcilPage *self );
static void                 bat_on_clear_clicked( GtkButton *button, ofaReconcilPage *self );
static void                 bat_clear_content( ofaReconcilPage *self );
static void                 bat_do_display_all_files( ofaReconcilPage *self );
static void                 bat_display_by_id( ofaReconcilPage *self, ofxCounter bat_id );
static void                 bat_display_file( ofaReconcilPage *self, ofoBat *bat );
static void                 bat_display_name( ofaReconcilPage *self );
static void                 bat_display_counts( ofaReconcilPage *self );
static gboolean             check_for_enable_view( ofaReconcilPage *self );
static void                 action_on_reconciliate_activated( GSimpleAction *action, GVariant *empty, ofaReconcilPage *self );
static void                 do_reconciliate( ofaReconcilPage *self );
static gboolean             do_reconciliate_user_confirm( ofaReconcilPage *self, ofxAmount debit, ofxAmount credit );
static GDate               *do_reconciliate_get_concil_date( ofaReconcilPage *self, GList *selected, GDate *date );
static ofoConcil           *do_reconciliate_get_concil_group( ofaReconcilPage *self, GList *selected );
static void                 action_on_decline_activated( GSimpleAction *action, GVariant *empty, ofaReconcilPage *self );
static void                 do_decline( ofaReconcilPage *self );
static void                 action_on_unreconciliate_activated( GSimpleAction *action, GVariant *empty, ofaReconcilPage *self );
static void                 do_unconciliate( ofaReconcilPage *self );
static GList               *do_unconciliate_get_children_refs( ofaReconcilPage *self, GtkTreeRowReference *parent_ref );
static void                 do_unconciliate_iconcil( ofaReconcilPage *self, GList *store_refs, GList **obj_list );
static void                 set_reconciliated_balance( ofaReconcilPage *self );
static void                 get_tree_models( ofaReconcilPage *self, GtkTreeModel **sort_model, GtkTreeModel **filter_model );
static GList               *selected_to_store_refs( ofaReconcilPage *self, GtkTreeModel *sort_model, GtkTreeModel *filter_model, GtkTreeModel *store_model, GList *selected );
static GtkTreeRowReference *sort_iter_to_store_ref( ofaReconcilPage *self, GtkTreeModel *sort_model, GtkTreeModel *filter_model, GtkTreeModel *store_model, GtkTreeIter *sort_iter );
static GtkTreeRowReference *sort_path_to_store_ref( ofaReconcilPage *self, GtkTreeModel *sort_model, GtkTreeModel *filter_model, GtkTreeModel *store_model, GtkTreePath *sort_path );
static gboolean             store_iter_to_sort_iter( ofaReconcilPage *self, GtkTreeModel *sort_model, GtkTreeModel *filter_model, GtkTreeModel *store_model, GtkTreeIter *store_iter, GtkTreeIter *sort_iter);
static gboolean             store_ref_to_sort_iter( ofaReconcilPage *self, GtkTreeModel *sort_model, GtkTreeModel *filter_model, GtkTreeModel *store_model, GtkTreeRowReference *store_ref, GtkTreeIter *sort_iter);
static void                 row_ref_to_store_iter( ofaReconcilPage *self, GtkTreeModel *tmodel, GtkTreeRowReference *ref, GtkTreeIter *iter );
static gint                 row_ref_cmp( GtkTreeRowReference *a, GtkTreeRowReference *b );
static void                 action_on_print_activated( GSimpleAction *action, GVariant *empty, ofaReconcilPage *self );
static void                 action_on_expand_activated( GSimpleAction *action, GVariant *empty, ofaReconcilPage *self );
static gboolean             expand_on_pressed( GtkWidget *button, GdkEvent *event, ofaReconcilPage *self );
static gboolean             expand_on_released( GtkWidget *button, GdkEvent *event, ofaReconcilPage *self );
static void                 set_message( ofaReconcilPage *page, const gchar *msg );
static void                 get_settings( ofaReconcilPage *self );
static void                 set_settings( ofaReconcilPage *self );
static void                 hub_connect_to_signaling_system( ofaReconcilPage *self );
static void                 hub_on_new_object( ofaHub *hub, ofoBase *object, ofaReconcilPage *self );
static void                 hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaReconcilPage *self );
static void                 hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaReconcilPage *self );

G_DEFINE_TYPE_EXTENDED( ofaReconcilPage, ofa_reconcil_page, OFA_TYPE_PANED_PAGE, 0,
		G_ADD_PRIVATE( ofaReconcilPage ))

static void
reconciliation_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_reconcil_page_finalize";
	ofaReconcilPagePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( OFA_IS_RECONCIL_PAGE( instance ));

	/* free data members here */
	priv = ofa_reconcil_page_get_instance_private( OFA_RECONCIL_PAGE( instance ));

	g_free( priv->settings_prefix );

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

		g_clear_object( &priv->decline_action );
		g_clear_object( &priv->reconciliate_action );
		g_clear_object( &priv->unreconciliate_action );
		g_clear_object( &priv->print_action );
		g_clear_object( &priv->expand_action );
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
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_reconcil_page_class_init( ofaReconcilPageClass *klass )
{
	static const gchar *thisfn = "ofa_reconcil_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = reconciliation_dispose;
	G_OBJECT_CLASS( klass )->finalize = reconciliation_finalize;

	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = page_v_get_top_focusable_widget;

	OFA_PANED_PAGE_CLASS( klass )->setup_view = paned_page_v_setup_view;
	OFA_PANED_PAGE_CLASS( klass )->init_view = paned_page_v_init_view;
}

static GtkWidget *
page_v_get_top_focusable_widget( const ofaPage *page )
{
	ofaReconcilPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_RECONCIL_PAGE( page ), NULL );

	priv = ofa_reconcil_page_get_instance_private( OFA_RECONCIL_PAGE( page ));

	return( GTK_WIDGET( priv->tview ));
}

/*
 * grid: the first row contains 'n' columns for selection and filters
 * the second row contains another grid which manages the treeview,
 * along with header and footer
 */
static void
paned_page_v_setup_view( ofaPanedPage *page, GtkPaned *paned )
{
	static const gchar *thisfn = "ofa_reconcil_page_v_setup_view";
	ofaReconcilPagePrivate *priv;
	GtkWidget *view;

	g_debug( "%s: page=%p, paned=%p", thisfn, ( void * ) page, ( void * ) paned );

	priv = ofa_reconcil_page_get_instance_private( OFA_RECONCIL_PAGE( page ));

	priv->hub = ofa_igetter_get_hub( OFA_IGETTER( page ));
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	priv->paned = GTK_WIDGET( paned );

	view = setup_view1( OFA_RECONCIL_PAGE( page ));
	gtk_paned_pack1( paned, view, TRUE, FALSE );

	view = setup_view2( OFA_RECONCIL_PAGE( page ));
	gtk_paned_pack2( paned, view, FALSE, FALSE );
}

static GtkWidget *
setup_view1( ofaReconcilPage *self )
{
	GtkWidget *box;

	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	my_utils_container_attach_from_resource( GTK_CONTAINER( box ), st_resource_ui, st_ui_name1, "top1" );

	setup_treeview_header( self, GTK_CONTAINER( box ));
	setup_treeview( self, GTK_CONTAINER( box ));
	setup_treeview_footer( self, GTK_CONTAINER( box ));

	return( box );
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
	ofaReconcilPagePrivate *priv;
	GtkWidget *tview_parent;

	priv = ofa_reconcil_page_get_instance_private( self );

	priv->tview = ofa_reconcil_treeview_new();

	tview_parent = my_utils_container_get_child_by_name( parent, "treeview-parent" );
	g_return_if_fail( tview_parent && GTK_IS_CONTAINER( tview_parent ));

	priv->tview = ofa_reconcil_treeview_new();
	gtk_container_add( GTK_CONTAINER( tview_parent ), GTK_WIDGET( priv->tview ));
	ofa_reconcil_treeview_set_settings_key( priv->tview, priv->settings_prefix );
	ofa_reconcil_treeview_setup_columns( priv->tview );
	ofa_reconcil_treeview_set_filter_func( priv->tview, ( GtkTreeModelFilterVisibleFunc ) tview_is_visible_row, self );

	/* insertion/delete are not handled here
	 * we connect to selection signals rather than treeview ones
	 * in order to get (more useful) list of path */
	g_signal_connect( priv->tview, "ofa-selchanged", G_CALLBACK( tview_on_selection_changed ), self );
	g_signal_connect( priv->tview, "ofa-selactivated", G_CALLBACK( tview_on_selection_activated ), self );
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
	my_style_add( priv->select_debit, "labelhelp" );

	priv->select_credit = my_utils_container_get_child_by_name( parent, "select-credit" );
	g_return_if_fail( priv->select_credit && GTK_IS_LABEL( priv->select_credit ));
	my_style_add( priv->select_credit, "labelhelp" );

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

	gtk_tree_model_get( tmodel, iter, RECONCIL_COL_OBJECT, &object, -1 );
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
				visible = tview_is_session_conciliated( self, concil );
				if( DEBUG_FILTER ){
					g_debug( "%s: tview_is_session_conciliated=%s",
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
				visible = tview_is_session_conciliated( self, concil );
				if( DEBUG_FILTER ){
					g_debug( "%s: tview_is_session_conciliated=%s",
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
tview_is_session_conciliated( ofaReconcilPage *self, ofoConcil *concil )
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
 * - reconciliate is enabled as soon as selection contains unconciliated rows
 *
 * - decline is enabled if selection contains *one* unconciliated child
 *
 * - unconciliate is enabled as soon as selection contains a concilition group
 */
static void
tview_on_selection_changed( ofaTVBin *treeview, GtkTreeSelection *selection, ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	gboolean concil_enabled, decline_enabled, unreconciliate_enabled;
	ofsCurrency scur;
	guint concil_rows, unconcil_rows, count;
	gboolean is_child;
	gchar *sdeb, *scre;
	GList *selected;

	priv = ofa_reconcil_page_get_instance_private( self );

	concil_enabled = FALSE;
	decline_enabled = FALSE;
	unreconciliate_enabled = FALSE;
	priv->activate_action = ACTIV_NONE;

	memset( &scur, '\0', sizeof( ofsCurrency ));
	scur.currency = priv->acc_currency;

	selected = gtk_tree_selection_get_selected_rows( selection, NULL );
	count = g_list_length( selected );
	tview_examine_selection( self, selected, &scur, &concil_rows, &unconcil_rows, &is_child );
	g_list_free_full( selected, ( GDestroyNotify ) gtk_tree_path_free );

	sdeb = ofa_amount_to_str( scur.debit, priv->acc_currency );
	gtk_label_set_text( GTK_LABEL( priv->select_debit ), sdeb );
	scre = ofa_amount_to_str( scur.credit, priv->acc_currency );
	gtk_label_set_text( GTK_LABEL( priv->select_credit ), scre );

	if( scur.debit || scur.credit ){
		if( ofs_currency_is_balanced( &scur )){
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
	concil_enabled = ( unconcil_rows > 0 );
	decline_enabled = ( count == 1 && unconcil_rows == 1 && is_child );
	unreconciliate_enabled = ( concil_rows > 0 && unconcil_rows == 0 );

	/* what to do on selection activation ?
	 * do not manage selection activation when we have both conciliated
	 * and unconciliated rows */
	if( concil_rows == 0 && unconcil_rows > 0 ){
		priv->activate_action = ACTIV_CONCILIATE;

	} else if( concil_rows > 0 && unconcil_rows == 0 ){
		priv->activate_action = ACTIV_UNCONCILIATE;
	}

	g_simple_action_set_enabled( priv->reconciliate_action, concil_enabled );
	g_simple_action_set_enabled( priv->decline_action, decline_enabled );
	g_simple_action_set_enabled( priv->unreconciliate_action, unreconciliate_enabled );
}

/*
 * The selection function (cf. ofaReconcilTreeview::on_select_fn())
 * makes sure that selection involves:
 * - at most one hierarchy,
 * - at most one conciliation group,
 * - plus any single rows.
 *
 * examine the current selection, gathering the required indicators:
 * @scur: the total of debits and credits, plus the currency
 * @concil_rows: the count of conciliated rows
 * @unconcil_rows: count of unconciliated rows
 * @is_child: whether all rows of the selection are a child
 *  (most useful when selecting only one child to decline it)
 */
static void
tview_examine_selection( ofaReconcilPage *self, GList *selected,
		ofsCurrency *scur, guint *concil_rows, guint *unconcil_rows, gboolean *is_child )
{
	ofaReconcilPagePrivate *priv;
	GtkTreeModel *tmodel;
	GList *it;
	ofoBase *object;
	ofxAmount amount;
	ofxCounter concil_id;
	GtkTreeIter iter, parent_iter;
	gboolean ok;

	priv = ofa_reconcil_page_get_instance_private( self );

	concil_id = 0;
	scur->debit = 0;
	scur->credit = 0;
	*concil_rows = 0;
	*unconcil_rows = 0;
	*is_child = TRUE;
	tmodel = ofa_tvbin_get_tree_model( OFA_TVBIN( priv->tview ));
	/* g_debug( "tmodel=%p (%s)", ( void * ) tmodel, G_OBJECT_TYPE_NAME( tmodel )); */

	for( it=selected ; it ; it=it->next ){
		ok = gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) it->data );
		g_return_if_fail( ok );
		gtk_tree_model_get( tmodel, &iter,
				RECONCIL_COL_OBJECT, &object, RECONCIL_COL_CONCIL_NUMBER_I, &concil_id, -1 );
		g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
		g_object_unref( object );

		/* increment debit/credit */
		if( OFO_IS_ENTRY( object )){
			scur->debit += ofo_entry_get_debit( OFO_ENTRY( object ));
			scur->credit += ofo_entry_get_credit( OFO_ENTRY( object ));
		} else {
			amount = ofo_bat_line_get_amount( OFO_BAT_LINE( object ));
			if( amount < 0 ){
				scur->debit += -1*amount;
			} else {
				scur->credit += amount;
			}
		}

		/* manage conciliation groups */
		if( concil_id > 0 ){
			*concil_rows += 1;
		} else {
			*unconcil_rows += 1;
		}

		/* is it a child or a parent ? */
		if( !gtk_tree_model_iter_parent( tmodel, &parent_iter, &iter )){
			*is_child = FALSE;
		}
	}
}

/*
 * activating a row is a shortcut for toggling conciliate/unconciliate
 * the selection is automatically extended to the parent and all its
 * children if this is possible (and desirable: only one selected row)
 */
static void
tview_on_selection_activated( ofaTVBin *treeview, GtkTreeSelection *selection, ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;

	priv = ofa_reconcil_page_get_instance_private( self );

	tview_expand_selection( self );

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

	tview_on_selection_changed(
			OFA_TVBIN( priv->tview ), ofa_tvbin_get_selection( OFA_TVBIN( priv->tview )), self );
}

/*
 * when only one row is selected, and this row is member of a hierarchy,
 * this expands the selection to all the hierarchy
 */
static void
tview_expand_selection( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	GtkTreeSelection *selection;
	GList *selected;
	GtkTreeIter *start, iter, parent, child;
	GtkTreeModel *tmodel;

	priv = ofa_reconcil_page_get_instance_private( self );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( priv->tview ));
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

static GtkWidget *
setup_view2( ofaReconcilPage *self )
{
	GtkWidget *box;

	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	my_utils_container_attach_from_resource( GTK_CONTAINER( box ), st_resource_ui, st_ui_name2, "top2" );

	setup_account_selection( self, GTK_CONTAINER( box ));
	setup_entries_filter( self, GTK_CONTAINER( box ));
	setup_date_filter( self, GTK_CONTAINER( box ));
	setup_manual_rappro( self, GTK_CONTAINER( box ));
	setup_size_group( self, GTK_CONTAINER( box ));
	setup_auto_rappro( self, GTK_CONTAINER( box ));
	setup_actions( self, GTK_CONTAINER( box ));

	return( box );
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
	my_style_add( priv->acc_label, "labelnormal" );
}

/*
 * the combo box for filtering the displayed entries
 */
static void
setup_entries_filter( ofaReconcilPage *self, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;
	GtkWidget *combo;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeIter iter;
	gint i;

	priv = ofa_reconcil_page_get_instance_private( self );

	combo = my_utils_container_get_child_by_name( parent, "entries-filter" );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));
	priv->mode_combo = GTK_COMBO_BOX( combo );

#if 0
	label = my_utils_container_get_child_by_name( parent, "entries-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), combo );
#endif

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

	g_signal_connect( priv->mode_combo, "changed", G_CALLBACK( mode_filter_on_changed ), self );
}

static void
setup_date_filter( ofaReconcilPage *self, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;
	GtkWidget *filter_parent;
	gchar *settings_key;

	priv = ofa_reconcil_page_get_instance_private( self );

	priv->effect_filter = ofa_date_filter_hv_bin_new();
	settings_key = g_strdup_printf( "%s-effect", priv->settings_prefix );
	ofa_idate_filter_set_settings_key( OFA_IDATE_FILTER( priv->effect_filter ), settings_key );
	g_free( settings_key );
	g_signal_connect( priv->effect_filter, "ofa-focus-out", G_CALLBACK( effect_dates_filter_on_changed ), self );

	filter_parent = my_utils_container_get_child_by_name( parent, "effect-date-filter" );
	g_return_if_fail( filter_parent && GTK_IS_CONTAINER( filter_parent ));
	gtk_container_add( GTK_CONTAINER( filter_parent ), GTK_WIDGET( priv->effect_filter ));
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
	my_date_editable_set_overwrite( GTK_EDITABLE( priv->date_concil ), ofa_prefs_date_overwrite());

	g_signal_connect( priv->date_concil, "changed", G_CALLBACK( concil_date_on_changed ), self );
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

	button = my_utils_container_get_child_by_name( parent, "bat-select" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( button, "clicked", G_CALLBACK( bat_on_select_clicked ), self );

	button = my_utils_container_get_child_by_name( parent, "bat-import" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( button, "clicked", G_CALLBACK( bat_on_import_clicked ), self );

	button = my_utils_container_get_child_by_name( parent, "bat-clear" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( button, "clicked", G_CALLBACK( bat_on_clear_clicked ), self );
	priv->clear = GTK_BUTTON( button );

	label = my_utils_container_get_child_by_name( parent, "bat-name" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->bat_name = label;

	label = my_utils_container_get_child_by_name( parent, "bat-count1" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->bat_label1 = label;

	label = my_utils_container_get_child_by_name( parent, "bat-count2" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_style_add( label, "labelbatunconcil" );
	priv->bat_unused_label = label;

	label = my_utils_container_get_child_by_name( parent, "bat-count3" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->bat_count_label = label;
}

static void
setup_actions( ofaReconcilPage *self, GtkContainer *parent )
{
	ofaReconcilPagePrivate *priv;
	GtkWidget *button;

	priv = ofa_reconcil_page_get_instance_private( self );

	priv->actions_frame = my_utils_container_get_child_by_name( parent, "f6-actions" );
	g_return_if_fail( priv->actions_frame && GTK_IS_FRAME( priv->actions_frame ));

	/* reconciliate action */
	priv->reconciliate_action = g_simple_action_new( "reconciliate", NULL );
	g_signal_connect( priv->reconciliate_action, "activate", G_CALLBACK( action_on_reconciliate_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->reconciliate_action ),
			_( "Reconciliate the selection" ));
	button = my_utils_container_get_child_by_name( parent, "reconciliate-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	ofa_iactionable_set_button(
			OFA_IACTIONABLE( self ), button, priv->settings_prefix, G_ACTION( priv->reconciliate_action ));

	/* decline action */
	priv->decline_action = g_simple_action_new( "decline", NULL );
	g_signal_connect( priv->decline_action, "activate", G_CALLBACK( action_on_decline_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->decline_action ),
			_( "Decline the selection" ));
	button = my_utils_container_get_child_by_name( parent, "decline-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	ofa_iactionable_set_button(
			OFA_IACTIONABLE( self ), button, priv->settings_prefix, G_ACTION( priv->decline_action ));

	/* unreconciliate action */
	priv->unreconciliate_action = g_simple_action_new( "unreconciliate", NULL );
	g_signal_connect( priv->unreconciliate_action, "activate", G_CALLBACK( action_on_unreconciliate_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->unreconciliate_action ),
			_( "Unreconciliate the selection" ));
	button = my_utils_container_get_child_by_name( parent, "unreconciliate-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	ofa_iactionable_set_button(
			OFA_IACTIONABLE( self ), button, priv->settings_prefix, G_ACTION( priv->unreconciliate_action ));

	/* print action */
	priv->print_action = g_simple_action_new( "print", NULL );
	g_signal_connect( priv->print_action, "activate", G_CALLBACK( action_on_print_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->print_action ),
			_( "Print a conciliation summary" ));
	button = my_utils_container_get_child_by_name( parent, "print-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	ofa_iactionable_set_button(
			OFA_IACTIONABLE( self ), button, priv->settings_prefix, G_ACTION( priv->print_action ));

	/* expand action */
	priv->expand_action = g_simple_action_new( "expand", NULL );
	g_signal_connect( priv->expand_action, "activate", G_CALLBACK( action_on_expand_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->expand_action ),
			_( "Print a conciliation summary" ));
	button = my_utils_container_get_child_by_name( parent, "expand-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	ofa_iactionable_set_button(
			OFA_IACTIONABLE( self ), button, priv->settings_prefix, G_ACTION( priv->expand_action ));

	g_signal_connect( button, "button-press-event", G_CALLBACK( expand_on_pressed ), self );
	g_signal_connect( button, "button-release-event", G_CALLBACK( expand_on_released ), self );
}

static void
paned_page_v_init_view( ofaPanedPage *page )
{
	static const gchar *thisfn = "ofa_reconcil_page_v_init_view";
	ofaReconcilPagePrivate *priv;
	GMenu *menu;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_reconcil_page_get_instance_private( OFA_RECONCIL_PAGE( page ));

	menu = ofa_iactionable_get_menu( OFA_IACTIONABLE( page ), priv->settings_prefix );
	ofa_icontext_set_menu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( page ),
			menu );

	menu = ofa_itvcolumnable_get_menu( OFA_ITVCOLUMNABLE( priv->tview ));
	ofa_icontext_append_submenu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( priv->tview ),
			OFA_IACTIONABLE_VISIBLE_COLUMNS_ITEM, menu );

	/* install an empty store before reading the settings */
	priv->store = ofa_reconcil_store_new( priv->hub );
	ofa_tvbin_set_store( OFA_TVBIN( priv->tview ), GTK_TREE_MODEL( priv->store ));

	/* makes sure to connect to dossier signaling system *after*
	 * the store itself */
	hub_connect_to_signaling_system( OFA_RECONCIL_PAGE( page ));

	get_settings( OFA_RECONCIL_PAGE( page ));
}

/**
 * ofa_reconcil_page_set_account:
 * @page: this #ofaReconcilPage instance.
 * @number: the account identifier.
 *
 * Preselects the specified account @number.
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
	account_do_change( self );
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
 * the treeview is disabled (insensitive) while the account is not ok
 * (and priv->account is NULL)
 *
 * Note that @entry may be %NULL, when we are trying to revalidate the
 * entered account identifier after having cleared the loaded BAT files
 * (cf. bat_on_clear_clicked()).
 */
static void
account_do_change( ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_account_do_change";
	ofaReconcilPagePrivate *priv;
	const gchar *acc_number;

	priv = ofa_reconcil_page_get_instance_private( self );

	/* get an ofoAccount object, or NULL */
	acc_number = gtk_entry_get_text( GTK_ENTRY( priv->acc_id_entry ));
	priv->account = account_get_reconciliable( self, acc_number );
	g_debug( "%s: self=%p, number=%s, account=%p", thisfn, ( void * ) self, acc_number, ( void * ) priv->account );

	if( priv->account ){
		account_clear_content( self );
		account_set_header_balance( self );
		ofa_reconcil_store_load_by_account( priv->store, acc_number );
		ofa_reconcil_treeview_default_expand( priv->tview );
		set_reconciliated_balance( self );
	}

	check_for_enable_view( self );
}

/*
 * check that the specified account is valid for a reconciliation session
 */
static ofoAccount *
account_get_reconciliable( ofaReconcilPage *self, const gchar *number )
{
	ofaReconcilPagePrivate *priv;
	gboolean ok;
	ofoAccount *account;
	const gchar *label, *bat_account;
	ofoBat *bat;
	gchar *msgerr;

	priv = ofa_reconcil_page_get_instance_private( self );

	ok = TRUE;
	msgerr = NULL;

	account = ofo_account_get_by_number( priv->hub, number );

	if( !account ){
		msgerr = g_strdup( _( "Invalid account number" ));
		ok = FALSE;

	} else {
		g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

		ok = !ofo_account_is_root( account ) &&
				!ofo_account_is_closed( account ) &&
				ofo_account_is_reconciliable( account );

		if( !ok ){
			msgerr = g_strdup( _( "Account is not a detail account, or closed, or not reconciliable" ));
		}
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
		my_style_remove( priv->acc_label, "labelerror" );
	} else {
		my_style_add( priv->acc_label, "labelerror" );
	}

	set_message( self, msgerr );
	g_free( msgerr );

	return( ok ? account : NULL );
}

/*
 * A new valid account is selected:
 * - reset all the account-related content
 * - remove all entries from the treeview
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
	gtk_tree_store_clear( GTK_TREE_STORE( priv->store ));
	gtk_label_set_text( GTK_LABEL( priv->bal_debit_label ), "" );
	gtk_label_set_text( GTK_LABEL( priv->bal_credit_label ), "" );

	/* reinsert bat lines (if any) */
	if( priv->bats ){
		bat_do_display_all_files( self );
	}
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

static void
mode_filter_on_changed( GtkComboBox *box, ofaReconcilPage *self )
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
		ofa_tvbin_refilter( OFA_TVBIN( priv->tview ));
		/* only update user preferences if view is enabled */
		set_settings( self );
	}
}

/*
 * called when reading the settings
 */
static void
mode_filter_select( ofaReconcilPage *self, gint mode )
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
effect_dates_filter_on_changed( ofaIDateFilter *filter, gint who, gboolean empty, const GDate *date, ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;

	priv = ofa_reconcil_page_get_instance_private( self );

	ofa_tvbin_refilter( OFA_TVBIN( priv->tview ));
}

/*
 * modifying the manual reconciliation date
 */
static void
concil_date_on_changed( GtkEditable *editable, ofaReconcilPage *self )
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
bat_on_select_clicked( GtkButton *button, ofaReconcilPage *self )
{
	bat_do_select( self );
}

/*
 * Select an already imported Bank Account Transaction list file.
 * Hitting Cancel on BAT selection doesn't change anything
 */
static void
bat_do_select( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	ofxCounter prev_id, bat_id;
	GtkWindow *toplevel;

	priv = ofa_reconcil_page_get_instance_private( self );

	prev_id = priv->bats ? ofo_bat_get_id( OFO_BAT( priv->bats->data )) : -1;
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	bat_id = ofa_bat_select_run( OFA_IGETTER( self ), toplevel, prev_id );
	if( bat_id > 0 ){
		bat_display_by_id( self, bat_id );
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
		bat_display_by_id( self, imported_id );
	}
}

static void
bat_on_clear_clicked( GtkButton *button, ofaReconcilPage *self )
{
	bat_clear_content( self );
	account_do_change( self );
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
	gtk_label_set_text( GTK_LABEL( priv->bat_label1 ), "" );
	gtk_label_set_text( GTK_LABEL( priv->bat_unused_label ), "" );
	gtk_label_set_text( GTK_LABEL( priv->bat_count_label ), "" );

	/* clear the store
	 * be lazzy: rather than deleting the bat lines, just delete all and
	 * reinsert entries */
	gtk_tree_store_clear( GTK_TREE_STORE( priv->store ));
	if( priv->account ){
		ofa_reconcil_store_load_by_account( priv->store, ofo_account_get_number( priv->account ));
	}

	/* and update the bank reconciliated balance */
	set_reconciliated_balance( self );
}

/*
 * re-display all loaded bat files
 * should only be called on a cleared tree store
 */
static void
bat_do_display_all_files( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	GList *it;

	priv = ofa_reconcil_page_get_instance_private( self );

	for( it=priv->bats ; it ; it=it->next ){
		bat_display_file( self, OFO_BAT( it->data ));
	}
}

/*
 * about to display a newly-imported or a newly-selected BAT file
 * check that it is not already displayed
 */
static void
bat_display_by_id( ofaReconcilPage *self, ofxCounter bat_id )
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
		bat_display_file( self, bat );
	}
}

static void
bat_display_file( ofaReconcilPage *self, ofoBat *bat )
{
	ofaReconcilPagePrivate *priv;

	priv = ofa_reconcil_page_get_instance_private( self );

	ofa_reconcil_store_load_by_bat( priv->store, ofo_bat_get_id( bat ));
	ofa_reconcil_treeview_default_expand( priv->tview );
	bat_display_name( self );
	bat_display_counts( self );
	set_reconciliated_balance( self );
}

/*
 * display the uri of the loaded bat file
 * just display 'multiple selection' if appropriate
 */
static void
bat_display_name( ofaReconcilPage *self )
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
bat_display_counts( ofaReconcilPage *self )
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

	gtk_label_set_text( GTK_LABEL( priv->bat_label1 ), "(" );

	str = g_markup_printf_escaped( "<i>%u</i>", unused );
	gtk_label_set_markup( GTK_LABEL( priv->bat_unused_label ), str );
	g_free( str );

	str = g_strdup_printf( "/%u)", total );
	gtk_label_set_text( GTK_LABEL( priv->bat_count_label ), str );
	g_free( str );
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
bat_associates_account( ofaReconcilPage *self, ofoBatLine *batline, const gchar *account )
{
	static const gchar *thisfn = "ofa_reconcil_page_bat_associates_account";
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
action_on_reconciliate_activated( GSimpleAction *action, GVariant *empty, ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;

	priv = ofa_reconcil_page_get_instance_private( self );

	tview_expand_selection( self );
	do_reconciliate( self );
	tview_on_selection_changed(
			OFA_TVBIN( priv->tview ), ofa_tvbin_get_selection( OFA_TVBIN( priv->tview )), self );
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
	ofsCurrency scur;
	guint concil_rows, unconcil_rows;
	gboolean is_child;
	GDate dval;
	GtkTreeSelection *selection;
	GList *selected, *it, *store_refs, *objects;
	GtkTreeIter store_iter, parent_iter, sort_iter;
	GtkTreeRowReference *store_ref, *ent_parent_ref, *bat_parent_ref, *parent_ref;
	ofoBase *object;
	ofxCounter concil_id;
	gint depth;
	GtkTreePath *store_path, *parent_path;
	const gchar *ent_account;
	GtkWindow *toplevel;
	ofoConcil *concil;
	GtkTreeModel *sort_model, *filter_model;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = ofa_reconcil_page_get_instance_private( self );

	memset( &scur, '\0', sizeof( ofsCurrency ));
	scur.currency = priv->acc_currency;
	concil = NULL;

	selection = ofa_tvbin_get_selection( OFA_TVBIN( priv->tview ));
	selected = gtk_tree_selection_get_selected_rows( selection, NULL );
	g_return_if_fail( g_list_length( selected ) >= 1 );

	tview_examine_selection( self, selected, &scur, &concil_rows, &unconcil_rows, &is_child );

	/* convert path on sort model to row references on store model */
	get_tree_models( self, &sort_model, &filter_model );
	store_refs = selected_to_store_refs( self, sort_model, filter_model, GTK_TREE_MODEL( priv->store ), selected );

	/* ask for a user confirmation when amounts are not balanced */
	if( !ofs_currency_is_balanced( &scur )){
		if( !do_reconciliate_user_confirm( self, scur.debit, scur.credit )){
			return;
		}
	}

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));

	/* compute effect date of a new concil group */
	if( concil_rows == 0 ){
		if( !my_date_is_valid( do_reconciliate_get_concil_date( self, selected, &dval ))){
			my_utils_msg_dialog( toplevel, GTK_MESSAGE_WARNING,
					_( "Conciliation is cancelled because unable to get a valid conciliation effect date" ));
			return;
		}
	} else {
		concil = do_reconciliate_get_concil_group( self, selected );
		g_return_if_fail( concil && OFO_IS_CONCIL( concil ));
		my_date_set_from_date( &dval, ofo_concil_get_dval( concil ));
	}
	concil_id = concil ? ofo_concil_get_id( concil ) : 0;
	ent_account = NULL;

	/* we are now able to create the conciliation group
	 * and to add each selected row to this group
	 * + we take advantage of this first phase to take a ref
	 *   on the future (unique) parent of the conciliation group
	 * + we take the target Openbook account (if any)
	 */
	ent_parent_ref = NULL;
	bat_parent_ref = NULL;
	for( it=store_refs ; it ; it=it->next ){
		store_ref = ( GtkTreeRowReference * ) it->data;
		g_return_if_fail( store_ref && gtk_tree_row_reference_valid( store_ref ));
		store_path = gtk_tree_row_reference_get_path( store_ref );
		if( !gtk_tree_model_get_iter( GTK_TREE_MODEL( priv->store ), &store_iter, store_path )){
			g_return_if_reached();
		}
		gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), &store_iter, RECONCIL_COL_OBJECT, &object, -1 );
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
		ofa_reconcil_store_set_concil_data( priv->store, concil_id, &dval, &store_iter );

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

		if( gtk_tree_path_compare( store_path, parent_path ) != 0 ){
			gtk_tree_model_get_iter( GTK_TREE_MODEL( priv->store ), &store_iter, store_path );
			gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), &store_iter, RECONCIL_COL_OBJECT, &object, -1 );
			g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));

			if( my_strlen( ent_account ) && OFO_IS_BAT_LINE( object )){
				/*g_debug( "%s: calling bat_associates_account", thisfn );*/
				bat_associates_account( self, OFO_BAT_LINE( object ), ent_account );
			}

			if( !gtk_tree_path_is_descendant( store_path, parent_path )){
				objects = g_list_prepend( objects, object );

				if( DEBUG_RECONCILIATE ){
					g_debug( "%s: removing object=%p (%s)", thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));
				}
				gtk_tree_store_remove( GTK_TREE_STORE( GTK_TREE_MODEL( priv->store ) ), &store_iter );
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
	if( !gtk_tree_model_get_iter( GTK_TREE_MODEL( priv->store ), &parent_iter, parent_path )){
		g_warning( "%s: unable to get the parent iter on store model", thisfn );
		return;
	}

	for( it=objects ; it ; it=it->next ){
		object = OFO_BASE( it->data );
		g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));

		if( DEBUG_RECONCILIATE ){
			g_debug( "%s: inserting object=%p (%s)", thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));
		}

		ofa_reconcil_store_insert_row( priv->store, OFA_ICONCIL( object ), &parent_iter, NULL );

		if( !gtk_tree_row_reference_valid( parent_ref )){
			g_debug( "%s: parent_ref no longer valid", thisfn );
			return;
		}
	}

	gtk_tree_path_free( parent_path );
	g_list_free_full( objects, ( GDestroyNotify ) g_object_unref );

	/* last re-selected the head of the hierarchy if it is displayed */
	gtk_tree_selection_unselect_all( selection );
	if( store_ref_to_sort_iter( self, sort_model, filter_model, GTK_TREE_MODEL( priv->store ), parent_ref, &sort_iter )){
		ofa_tvbin_select_row( OFA_TVBIN( priv->tview ), &sort_iter );
		ofa_reconcil_treeview_collapse_by_iter( priv->tview, &sort_iter );
	}

	gtk_tree_row_reference_free( parent_ref );
	g_list_free_full( selected, ( GDestroyNotify ) gtk_tree_path_free );

	set_reconciliated_balance( self );
	if( priv->bats ){
		bat_display_counts( self );
	}
}

static gboolean
do_reconciliate_user_confirm( ofaReconcilPage *self, ofxAmount debit, ofxAmount credit )
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
do_reconciliate_get_concil_date( ofaReconcilPage *self, GList *selected, GDate *date )
{
	ofaReconcilPagePrivate *priv;
	GList *it;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBase *object;

	priv = ofa_reconcil_page_get_instance_private( self );

	my_date_clear( date );
	tmodel = ofa_tvbin_get_tree_model( OFA_TVBIN( priv->tview ));

	for( it=selected ; it ; it=it->next ){
		gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) it->data );
		gtk_tree_model_get( tmodel, &iter, RECONCIL_COL_OBJECT, &object, -1 );
		g_return_val_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )), NULL );
		g_object_unref( object );

		if( OFO_IS_BAT_LINE( object )){
			my_date_set_from_date( date, ofo_bat_line_get_deffect( OFO_BAT_LINE( object )));
			break;
		}
	}

	/* else try with the manually provided date
	 * may not be valid */
	if( !my_date_is_valid( date )){
		my_date_set_from_date( date, &priv->dconcil );
	}

	return( date );
}

static ofoConcil *
do_reconciliate_get_concil_group( ofaReconcilPage *self, GList *selected )
{
	ofaReconcilPagePrivate *priv;
	ofoConcil *concil;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GList *it;
	ofxCounter concil_id;

	priv = ofa_reconcil_page_get_instance_private( self );

	concil = NULL;
	tmodel = ofa_tvbin_get_tree_model( OFA_TVBIN( priv->tview ));

	for( it=selected ; it ; it=it->next ){
		if( gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) it->data )){
			gtk_tree_model_get( tmodel, &iter, RECONCIL_COL_CONCIL_NUMBER_I, &concil_id, -1 );
			if( concil_id > 0 ){
				concil = ofo_concil_get_by_id( priv->hub, concil_id );
				if( concil ){
					g_return_val_if_fail( OFO_IS_CONCIL( concil ), NULL );
					break;
				}
			}
		}
	}

	return( concil );
}

/*
 * decline a proposition:
 * - selected children are moved to level 0
 * - first moved child is re-selected
 *
 * Proposed conciliation date of parents which have no more children is
 * reinitialized.
 */
static void
action_on_decline_activated( GSimpleAction *action, GVariant *empty, ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;

	priv = ofa_reconcil_page_get_instance_private( self );

	do_decline( self );
	tview_on_selection_changed(
			OFA_TVBIN( priv->tview ), ofa_tvbin_get_selection( OFA_TVBIN( priv->tview )), self );
}

static void
do_decline( ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_do_decline";
	ofaReconcilPagePrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeModel *sort_model, *filter_model;
	GList *selected, *it, *row_refs, *parent_refs;
	GtkTreeIter iter, parent_iter, first_child_iter;
	GtkTreeRowReference *ref;
	GtkTreePath *path;
	GObject *object;
	ofxCounter concil_id;
	gboolean child_set;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = ofa_reconcil_page_get_instance_private( self );
	child_set = FALSE;

	/* get selection */
	selection = ofa_tvbin_get_selection( OFA_TVBIN( priv->tview ));
	selected = gtk_tree_selection_get_selected_rows( selection, NULL );
	g_return_if_fail( g_list_length( selected ) >= 1 );

	get_tree_models( self, &sort_model, &filter_model );

	/* get row references of the selection
	 * we only consider here unconciliated children (and their parent) */
	row_refs = NULL;
	parent_refs = NULL;
	for( it=selected ; it ; it=it->next ){
		gtk_tree_model_get_iter( sort_model, &iter, ( GtkTreePath * ) it->data );
		gtk_tree_model_get( sort_model, &iter, RECONCIL_COL_CONCIL_NUMBER_I, &concil_id, -1 );
		/* only consider selected children */
		if( !concil_id && gtk_tree_model_iter_parent( sort_model, &parent_iter, &iter )){
			/* get row reference to distinct parents */
			path = gtk_tree_model_get_path( sort_model, &parent_iter );
			ref = gtk_tree_row_reference_new( sort_model, path );
			if( !g_list_find_custom( parent_refs, ref, ( GCompareFunc ) row_ref_cmp )){
				parent_refs = g_list_prepend( parent_refs, ref );
			} else {
				gtk_tree_row_reference_free( ref );
			}
			gtk_tree_path_free( path );
			/* get row reference to selected children */
			ref = gtk_tree_row_reference_new( sort_model, ( GtkTreePath * ) it->data );
			row_refs = g_list_prepend( row_refs, ref );
		}
	}

	/* remove and re-insert selected children
	 * the row_refs list becomes invalid after theses moves */
	for( it=row_refs ; it ; it=it->next ){
		row_ref_to_store_iter( self, sort_model, ( GtkTreeRowReference * ) it->data, &iter );
		gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), &iter, RECONCIL_COL_OBJECT, &object, -1 );
		g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
		gtk_tree_store_remove( GTK_TREE_STORE( priv->store ), &iter );
		ofa_reconcil_store_insert_level_zero( priv->store, OFA_ICONCIL( object ), &iter );
		if( !child_set ){
			first_child_iter = iter;
			child_set = TRUE;
		}
		g_object_unref( object );
	}

	/* update displayed concil data of the parents which no more have children */
	for( it=parent_refs ; it ; it=it->next ){
		row_ref_to_store_iter( self, sort_model, ( GtkTreeRowReference * ) it->data, &iter );
		if( !gtk_tree_model_iter_has_child( GTK_TREE_MODEL( priv->store ), &iter )){
			ofa_reconcil_store_set_concil_data( priv->store, 0, NULL, &iter );
		}
	}

	g_list_free_full( row_refs, ( GDestroyNotify ) gtk_tree_row_reference_free );
	g_list_free_full( parent_refs, ( GDestroyNotify ) gtk_tree_row_reference_free );
	g_list_free_full( selected, ( GDestroyNotify ) gtk_tree_path_free );

	/* re-select the first inserted child */
	gtk_tree_selection_unselect_all( selection );
	if( store_iter_to_sort_iter( self, sort_model, filter_model, GTK_TREE_MODEL( priv->store ), &first_child_iter, &iter )){
		ofa_tvbin_select_row( OFA_TVBIN( priv->tview ), &iter );
	}
}

static void
action_on_unreconciliate_activated( GSimpleAction *action, GVariant *empty, ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;

	priv = ofa_reconcil_page_get_instance_private( self );

	tview_expand_selection( self );
	do_unconciliate( self );
	tview_on_selection_changed(
			OFA_TVBIN( priv->tview ), ofa_tvbin_get_selection( OFA_TVBIN( priv->tview )), self );
}

/*
 * Unconciliate action is enabled when selection only contains part or
 * all of a single conciliated hierarchy (thanks to ofaReconcilTreeview::on_select_fn())
 * (i.e. concil_rows > 0 && unconcil_rows == 0).
 *
 * It is not expected that the current selection covers all the
 * conciliation group to be cleared. So:
 * - have to find the conciliation group id,
 * - iterate through the whole conciliation hierarchy, cleaning up the
 *   conciliation data for each member
 * - delete the conciliation group from the DBMS
 * - reinsert the old children as single rows (but maybe as proposals)
 */
static void
do_unconciliate( ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_do_unconciliate";
	ofaReconcilPagePrivate *priv;
	GtkTreeSelection *selection;
	GList *selected, *it, *store_refs, *obj_list;
	ofsCurrency scur;
	guint concil_rows, unconcil_rows;
	gboolean is_child;
	ofxCounter concil_id;
	GtkTreeModel *sort_model, *filter_model;
	GtkTreeIter iter, parent_iter;
	GtkTreeRowReference *parent_ref;
	ofoConcil *concil;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = ofa_reconcil_page_get_instance_private( self );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( priv->tview ));
	selected = gtk_tree_selection_get_selected_rows( selection, NULL );
	if( !g_list_length( selected )){
		g_warning( "%s: unexpected empty selection", thisfn );
		g_return_if_reached();
	}

	memset( &scur, '\0', sizeof( ofsCurrency ));
	scur.currency = priv->acc_currency;
	tview_examine_selection( self, selected, &scur, &concil_rows, &unconcil_rows, &is_child );
	if( concil_rows == 0 || unconcil_rows > 0 ){
		g_warning( "%s: concil_rows=%u, unconcil_rows=%u", thisfn, concil_rows, unconcil_rows );
		g_return_if_reached();
	}

	get_tree_models( self, &sort_model, &filter_model );

	/* get the conciliation group identifier from the first selected row */
	if( !gtk_tree_model_get_iter( sort_model, &iter, ( GtkTreePath * ) selected->data )){
		g_warning( "%s: unable to get an iter on the sorting model", thisfn );
		g_return_if_reached();
	}
	gtk_tree_model_get( sort_model, &iter, RECONCIL_COL_CONCIL_NUMBER_I, &concil_id, -1 );
	if( concil_id <= 0 ){
		g_warning( "%s: unexpected conciliation group identifier=%ld", thisfn, concil_id );
		g_return_if_reached();
	}

	/* remove the conciliation group from the database */
	concil = ofo_concil_get_by_id( priv->hub, concil_id );
	g_return_if_fail( concil && OFO_IS_CONCIL( concil ));
	ofo_concil_delete( concil );

	/* get a store ref to the parent of the conciliation hierarchy */
	if( !gtk_tree_model_iter_parent( sort_model, &parent_iter, &iter )){
		parent_iter = iter;
	}
	parent_ref = sort_iter_to_store_ref( self, sort_model, filter_model, GTK_TREE_MODEL( priv->store ), &parent_iter );
	g_return_if_fail( gtk_tree_row_reference_valid( parent_ref ));

	/* at this time, we can release the selection */
	g_list_free_full( selected, ( GDestroyNotify ) gtk_tree_path_free );

	/* get store refs of the whole conciliation hierarchy
	 * nb: parent_ref is returned prepended to the list, so do not free it separately */
	store_refs = do_unconciliate_get_children_refs( self, parent_ref );

	/* iterate through the hierarchy
	 * - cleaning up memory conciliation data
	 * - removing children to reinsert them later */
	obj_list = NULL;
	do_unconciliate_iconcil( self, store_refs, &obj_list );
	g_list_free_full( store_refs, ( GDestroyNotify ) gtk_tree_row_reference_free );

	/* last reinsert the unconciliated entries and the batlines */
	for( it=obj_list ; it ; it=it->next ){
		ofa_reconcil_store_insert_row( priv->store, OFA_ICONCIL( it->data ), NULL, NULL );
	}
	g_list_free_full( obj_list, ( GDestroyNotify ) g_object_unref );

	set_reconciliated_balance( self );
	if( priv->bats ){
		bat_display_counts( self );
	}
}

/*
 * get store refs for all children
 */
static GList *
do_unconciliate_get_children_refs( ofaReconcilPage *self, GtkTreeRowReference *parent_ref )
{
	GList *store_refs;
	GtkTreeModel *store_model;
	GtkTreePath *path;
	GtkTreeIter parent_iter, iter;
	gboolean ok;
	GtkTreeRowReference *ref;

	store_refs = NULL;

	/* get a store iter from parent ref */
	store_model = gtk_tree_row_reference_get_model( parent_ref );
	path = gtk_tree_row_reference_get_path( parent_ref );
	ok = gtk_tree_model_get_iter( store_model, &parent_iter, path );
	gtk_tree_path_free( path );
	g_return_val_if_fail( ok, NULL );

	/* iterate through the children */
	if( gtk_tree_model_iter_children( store_model, &iter, &parent_iter )){
		while( TRUE ){
			path = gtk_tree_model_get_path( store_model, &iter );
			ref = gtk_tree_row_reference_new( store_model, path );
			store_refs = g_list_prepend( store_refs, ref );
			gtk_tree_path_free( path );

			if( !gtk_tree_model_iter_next( store_model, &iter )){
				break;
			}
		}
	}

	/* insert parent_ref at the very beginning of the list */
	store_refs = g_list_prepend( store_refs, parent_ref );

	return( store_refs );
}

/*
 * - iterate on the store model to clean up conciliation data
 * - remove children objects, recording them in the @obj_list
 *   leaving untouched the parent
 */
static void
do_unconciliate_iconcil( ofaReconcilPage *self, GList *store_refs, GList **obj_list )
{
	GList *it;
	GtkTreePath *path;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gboolean ok;
	ofaIConcil *object;

	tmodel = gtk_tree_row_reference_get_model(( GtkTreeRowReference * ) store_refs->data );
	g_return_if_fail( tmodel && OFA_IS_RECONCIL_STORE( tmodel ));

	for( it=store_refs ; it ; it=it->next ){
		path = gtk_tree_row_reference_get_path(( GtkTreeRowReference * ) it->data );
		ok = gtk_tree_model_get_iter( tmodel, &iter, path );
		g_return_if_fail( ok );

		gtk_tree_model_get( tmodel, &iter, RECONCIL_COL_OBJECT, &object, -1 );
		g_return_if_fail( object && OFA_IS_ICONCIL( object ));
		ofa_iconcil_clear_data( OFA_ICONCIL( object ));

		if( gtk_tree_path_get_depth( path ) == 1 ){
			g_object_unref( object );
			ofa_reconcil_store_set_concil_data( OFA_RECONCIL_STORE( tmodel ), 0, NULL, &iter );

		} else {
			*obj_list = g_list_prepend( *obj_list, object );
			gtk_tree_store_remove( GTK_TREE_STORE( tmodel ), &iter );
		}

		gtk_tree_path_free( path );
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

		if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( priv->store ), &iter )){
			while( TRUE ){
				gtk_tree_model_get( GTK_TREE_MODEL( priv->store ), &iter, RECONCIL_COL_OBJECT, &object, -1 );
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
				if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( priv->store ), &iter )){
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

static void
get_tree_models( ofaReconcilPage *self, GtkTreeModel **sort_model, GtkTreeModel **filter_model )
{
	ofaReconcilPagePrivate *priv;

	priv = ofa_reconcil_page_get_instance_private( self );

	*sort_model = ofa_tvbin_get_tree_model( OFA_TVBIN( priv->tview ));
	g_return_if_fail( *sort_model && GTK_IS_TREE_MODEL_SORT( *sort_model ));

	*filter_model = gtk_tree_model_sort_get_model( GTK_TREE_MODEL_SORT( *sort_model ));
	g_return_if_fail( *filter_model && GTK_IS_TREE_MODEL_FILTER( *filter_model ));
}

/*
 * Convert the GList of selected rows (paths on the filter store) to a
 * GList of row references on the store model.
 * The returned list has to be freed with
 * g_list_free_full( list, ( GDestroyNotify ) gtk_tree_row_reference_free )
 */
static GList *
selected_to_store_refs( ofaReconcilPage *self,
		GtkTreeModel *sort_model, GtkTreeModel *filter_model, GtkTreeModel *store_model, GList *selected )
{
	GList *store_refs, *it;
	GtkTreeRowReference *ref;

	store_refs = NULL;

	for( it=selected ; it ; it=it->next ){
		ref = sort_path_to_store_ref( self, sort_model, filter_model, store_model, ( GtkTreePath * ) it->data );
		store_refs = g_list_prepend( store_refs, ref );
	}

	return( store_refs );
}

static GtkTreeRowReference *
sort_iter_to_store_ref( ofaReconcilPage *self,
		GtkTreeModel *sort_model, GtkTreeModel *filter_model, GtkTreeModel *store_model, GtkTreeIter *sort_iter )
{
	GtkTreeRowReference *store_ref;
	GtkTreePath *sort_path;

	sort_path = gtk_tree_model_get_path( sort_model, sort_iter );
	store_ref = sort_path_to_store_ref( self, sort_model, filter_model, store_model, sort_path );
	gtk_tree_path_free( sort_path );

	return( store_ref );
}

static GtkTreeRowReference *
sort_path_to_store_ref( ofaReconcilPage *self,
		GtkTreeModel *sort_model, GtkTreeModel *filter_model, GtkTreeModel *store_model, GtkTreePath *sort_path )
{
	GtkTreeRowReference *store_ref;
	GtkTreePath *filter_path, *store_path;

	g_return_val_if_fail( sort_path, NULL );

	filter_path = gtk_tree_model_sort_convert_path_to_child_path(
							GTK_TREE_MODEL_SORT( sort_model ), sort_path );
	store_path = gtk_tree_model_filter_convert_path_to_child_path(
							GTK_TREE_MODEL_FILTER( filter_model ), filter_path );

	store_ref = gtk_tree_row_reference_new( store_model, store_path );

	gtk_tree_path_free( filter_path );
	gtk_tree_path_free( store_path );

	return( store_ref );
}

/*
 * convert a store iter to a sort iter if possible
 * returns %FALSE if not possible (due to filtering model)
 */
static gboolean
store_iter_to_sort_iter( ofaReconcilPage *self,
		GtkTreeModel *sort_model, GtkTreeModel *filter_model, GtkTreeModel *store_model, GtkTreeIter *store_iter, GtkTreeIter *sort_iter)
{
	GtkTreePath *store_path;
	GtkTreeRowReference *store_ref;
	gboolean valid;

	store_path = gtk_tree_model_get_path( store_model, store_iter );
	store_ref = gtk_tree_row_reference_new( store_model, store_path );
	valid = store_ref_to_sort_iter( self, sort_model, filter_model, store_model, store_ref, sort_iter );
	gtk_tree_row_reference_free( store_ref );
	gtk_tree_path_free( store_path );

	return( valid );
}

/*
 * convert a store row reference to a sort iter if possible
 * returns %FALSE if not possible (due to filtering model)
 */
static gboolean
store_ref_to_sort_iter( ofaReconcilPage *self,
		GtkTreeModel *sort_model, GtkTreeModel *filter_model, GtkTreeModel *store_model, GtkTreeRowReference *store_ref, GtkTreeIter *sort_iter)
{
	GtkTreePath *store_path, *filter_path, *sort_path;

	sort_path = NULL;
	store_path = gtk_tree_row_reference_get_path( store_ref );
	g_return_val_if_fail( store_path, FALSE );
	filter_path = gtk_tree_model_filter_convert_child_path_to_path( GTK_TREE_MODEL_FILTER( filter_model ), store_path );

	if( filter_path ){
		sort_path = gtk_tree_model_sort_convert_child_path_to_path( GTK_TREE_MODEL_SORT( sort_model ), filter_path );
		g_return_val_if_fail( sort_path, FALSE );
		gtk_tree_model_get_iter( sort_model, sort_iter, sort_path );
		gtk_tree_path_free( sort_path );
		gtk_tree_path_free( filter_path );
		return( TRUE );
	}

	return( FALSE );
}

/*
 * convert the @ref on @tmodel to a store @iter
 */
static void
row_ref_to_store_iter( ofaReconcilPage *self, GtkTreeModel *tmodel, GtkTreeRowReference *ref, GtkTreeIter *iter )
{
	GtkTreePath *path;
	GtkTreeIter sort_iter, filter_iter;
	GtkTreeModel *filter_model;

	g_return_if_fail( tmodel && GTK_IS_TREE_MODEL_SORT( tmodel ));

	path = gtk_tree_row_reference_get_path( ref );
	if( path ){
		gtk_tree_model_get_iter( tmodel, &sort_iter, path );
		gtk_tree_path_free( path );
		gtk_tree_model_sort_convert_iter_to_child_iter( GTK_TREE_MODEL_SORT( tmodel ), &filter_iter, &sort_iter );
		filter_model = gtk_tree_model_sort_get_model( GTK_TREE_MODEL_SORT( tmodel ));
		g_return_if_fail( filter_model && GTK_IS_TREE_MODEL_FILTER( filter_model ));
		gtk_tree_model_filter_convert_iter_to_child_iter( GTK_TREE_MODEL_FILTER( filter_model ), iter, &filter_iter );
	}
}

/*
 * compare two row references
 */
static gint
row_ref_cmp( GtkTreeRowReference *a, GtkTreeRowReference *b )
{
	GtkTreePath *pa, *pb;
	gint cmp;

	pa = gtk_tree_row_reference_get_path( a );
	pb = gtk_tree_row_reference_get_path( b );

	cmp = gtk_tree_path_compare( pa, pb );

	gtk_tree_path_free( pa );
	gtk_tree_path_free( pb );

	return( cmp );
}

static void
action_on_print_activated( GSimpleAction *action, GVariant *empty, ofaReconcilPage *self )
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

static void
action_on_expand_activated( GSimpleAction *action, GVariant *empty, ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;

	priv = ofa_reconcil_page_get_instance_private( self );

	if( priv->ctrl_on_pressed && priv->ctrl_on_released ){
		ofa_reconcil_treeview_expand_all( priv->tview );
	} else {
		ofa_reconcil_treeview_default_expand( priv->tview );
	}

	priv->ctrl_on_pressed = FALSE;
	priv->ctrl_on_released = FALSE;
}

static gboolean
expand_on_pressed( GtkWidget *button, GdkEvent *event, ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	GdkModifierType modifiers;

	priv = ofa_reconcil_page_get_instance_private( self );

	modifiers = gtk_accelerator_get_default_mod_mask();

	priv->ctrl_on_pressed = ((( GdkEventButton * ) event )->state & modifiers ) == GDK_CONTROL_MASK;

	/*g_debug( "expand_on_pressed: ctrl_on_pressed=%s", priv->ctrl_on_pressed ? "True":"False" );*/

	return( FALSE );
}

static gboolean
expand_on_released( GtkWidget *button, GdkEvent *event, ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	GdkModifierType modifiers;

	priv = ofa_reconcil_page_get_instance_private( self );

	modifiers = gtk_accelerator_get_default_mod_mask();

	priv->ctrl_on_released = ((( GdkEventButton * ) event )->state & modifiers ) == GDK_CONTROL_MASK;

	/*g_debug( "expand_on_released: ctrl_on_released=%s", priv->ctrl_on_released ? "True":"False" );*/

	return( FALSE );
}

static void
set_message( ofaReconcilPage *page, const gchar *msg )
{
	ofaReconcilPagePrivate *priv;

	priv = ofa_reconcil_page_get_instance_private( page );

	if( priv->msg_label ){
		my_style_add( priv->msg_label, "labelerror" );
		gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
	}
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
	gchar *sdate, *settings_key;
	gint pos;

	priv = ofa_reconcil_page_get_instance_private( self );

	priv->reading_settings = TRUE;

	settings_key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	slist = ofa_settings_user_get_string_list( settings_key );
	if( slist ){
		it = slist ? slist : NULL;
		cstr = it ? it->data : NULL;
		if( cstr ){
			gtk_entry_set_text( GTK_ENTRY( priv->acc_id_entry ), cstr );
		}

		it = it ? it->next : NULL;
		cstr = it ? it->data : NULL;
		if( cstr ){
			mode_filter_select( self, atoi( cstr ));
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
		gtk_paned_set_position( GTK_PANED( priv->paned ), MAX( pos, 150 ));

		ofa_settings_free_string_list( slist );
	}

	g_free( settings_key );
	priv->reading_settings = FALSE;
}

static void
set_settings( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	const gchar *account, *sdate;
	gchar *date_sql, *settings_key;
	GDate date;
	gint pos;
	gchar *smode, *str;

	priv = ofa_reconcil_page_get_instance_private( self );

	if( !priv->reading_settings ){

		account = gtk_entry_get_text( GTK_ENTRY( priv->acc_id_entry ));

		smode = g_strdup_printf( "%d", priv->mode );

		sdate = gtk_entry_get_text( priv->date_concil );
		my_date_set_from_str( &date, sdate, ofa_prefs_date_display());
		if( my_date_is_valid( &date )){
			date_sql = my_date_to_str( &date, MY_DATE_SQL );
		} else {
			date_sql = g_strdup( "" );
		}

		pos = gtk_paned_get_position( GTK_PANED( priv->paned ));

		str = g_strdup_printf( "%s;%s;%s;%d;", account ? account : "", smode, date_sql, pos );

		settings_key = g_strdup_printf( "%s-settings", priv->settings_prefix );
		ofa_settings_user_set_string( settings_key, str );
		g_free( settings_key );

		g_free( date_sql );
		g_free( str );
		g_free( smode );
	}
}

static void
hub_connect_to_signaling_system( ofaReconcilPage *self )
{
	ofaReconcilPagePrivate *priv;
	gulong handler;

	priv = ofa_reconcil_page_get_instance_private( self );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_NEW, G_CALLBACK( hub_on_new_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_UPDATED, G_CALLBACK( hub_on_updated_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_DELETED, G_CALLBACK( hub_on_deleted_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
hub_on_new_object( ofaHub *hub, ofoBase *object, ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_hub_on_new_object";
	ofaReconcilPagePrivate *priv;

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	priv = ofa_reconcil_page_get_instance_private( self );

	if( OFO_IS_BAT_LINE( object ) || OFO_IS_CONCIL( object ) || OFO_IS_ENTRY( object )){
		ofa_tvbin_refilter( OFA_TVBIN( priv->tview ));
		ofa_reconcil_treeview_default_expand( priv->tview );
		set_reconciliated_balance( self );
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_hub_on_updated_object";
	ofaReconcilPagePrivate *priv;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, self=%p (%s)",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_reconcil_page_get_instance_private( self );

	if( OFO_IS_BAT_LINE( object ) || OFO_IS_CONCIL( object ) || OFO_IS_ENTRY( object )){
		ofa_tvbin_refilter( OFA_TVBIN( priv->tview ));
		ofa_reconcil_treeview_default_expand( priv->tview );
		set_reconciliated_balance( self );
	}
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaReconcilPage *self )
{
	static const gchar *thisfn = "ofa_reconcil_page_hub_on_deleted_object";
	ofaReconcilPagePrivate *priv;

	g_debug( "%s: hub=%p, object=%p (%s), self=%p (%s)",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_reconcil_page_get_instance_private( self );

	if( OFO_IS_BAT_LINE( object ) || OFO_IS_CONCIL( object ) || OFO_IS_ENTRY( object )){
		ofa_tvbin_refilter( OFA_TVBIN( priv->tview ));
		ofa_reconcil_treeview_default_expand( priv->tview );
		set_reconciliated_balance( self );
	}
}
