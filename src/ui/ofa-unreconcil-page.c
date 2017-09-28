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

#include "my/my-utils.h"

#include "api/ofa-buttons-box.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-ipage-manager.h"
#include "api/ofa-istore.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-operation-group.h"
#include "api/ofa-page.h"
#include "api/ofo-account.h"
#include "api/ofo-concil.h"
#include "api/ofo-entry.h"

#include "core/ofa-account-properties.h"
#include "core/ofa-entry-properties.h"
#include "core/ofa-iconcil.h"
#include "core/ofa-reconcil-page.h"

#include "ui/ofa-accentry-store.h"
#include "ui/ofa-accentry-treeview.h"
#include "ui/ofa-unreconcil-page.h"

/* priv instance data
 */
typedef struct {

	/* runtime
	 */
	ofaIGetter          *getter;
	gchar               *settings_prefix;
	GList               *store_handlers;

	/* UI
	 */
	ofaAccentryTreeview *tview;
	ofaAccentryStore    *store;
	GtkWidget           *account_label;
	GtkWidget           *entry_label;

	/* actions
	 */
	GSimpleAction       *collapse_action;
	GSimpleAction       *expand_action;
	GSimpleAction       *reconcil_action;
	GSimpleAction       *vaccount_action;
	GSimpleAction       *ventry_action;
	GSimpleAction       *vope_action;

	/* selection
	 */
	ofoAccount          *sel_account;
	ofoEntry            *sel_entry;
	GList               *sel_opes;
}
	ofaUnreconcilPagePrivate;

static GtkWidget *page_v_get_top_focusable_widget( const ofaPage *page );
static GtkWidget *action_page_v_setup_view( ofaActionPage *page );
static void       action_page_v_setup_actions( ofaActionPage *page, ofaButtonsBox *buttons_box );
static void       action_page_v_init_view( ofaActionPage *page );
static gboolean   tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaUnreconcilPage *self );
static gboolean   tview_is_visible_account( ofaUnreconcilPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoAccount *account );
static gboolean   tview_is_visible_entry( ofaUnreconcilPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoEntry *entry );
static void       tview_cell_data_render( GtkTreeViewColumn *column, GtkCellRenderer *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaAccentryTreeview *self );
static void       tview_on_accchanged( ofaAccentryTreeview *view, ofoBase *object, ofaUnreconcilPage *self );
static void       tview_on_accactivated( ofaAccentryTreeview *view, ofoBase *object, ofaUnreconcilPage *self );
static void       refresh_status_label( ofaUnreconcilPage *self );
static void       refresh_status_label_rec( ofaUnreconcilPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, guint *account_count, guint *entry_count );
static void       store_on_need_refilter( ofaIStore *store, ofaUnreconcilPage *self );
static void       action_on_collapse_activated( GSimpleAction *action, GVariant *empty, ofaUnreconcilPage *self );
static void       action_on_expand_activated( GSimpleAction *action, GVariant *empty, ofaUnreconcilPage *self );
static void       action_on_reconcil_activated( GSimpleAction *action, GVariant *empty, ofaUnreconcilPage *self );
static void       action_do_reconcil( ofaUnreconcilPage *self, ofoBase *object );
static void       action_on_vaccount_activated( GSimpleAction *action, GVariant *empty, ofaUnreconcilPage *self );
static void       action_on_ventry_activated( GSimpleAction *action, GVariant *empty, ofaUnreconcilPage *self );
static void       action_on_vope_activated( GSimpleAction *action, GVariant *empty, ofaUnreconcilPage *self );

G_DEFINE_TYPE_EXTENDED( ofaUnreconcilPage, ofa_unreconcil_page, OFA_TYPE_ACTION_PAGE, 0,
		G_ADD_PRIVATE( ofaUnreconcilPage ))

static void
unreconcil_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_unreconcil_page_finalize";
	ofaUnreconcilPagePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_UNRECONCIL_PAGE( instance ));

	/* free data members here */
	priv = ofa_unreconcil_page_get_instance_private( OFA_UNRECONCIL_PAGE( instance ));

	g_free( priv->settings_prefix );
	g_list_free( priv->sel_opes );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_unreconcil_page_parent_class )->finalize( instance );
}

static void
unreconcil_page_dispose( GObject *instance )
{
	ofaUnreconcilPagePrivate *priv;
	GList *it;

	g_return_if_fail( instance && OFA_IS_UNRECONCIL_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		priv = ofa_unreconcil_page_get_instance_private( OFA_UNRECONCIL_PAGE( instance ));

		/* disconnect ofaEntryStore signal handlers */
		for( it=priv->store_handlers ; it ; it=it->next ){
			g_signal_handler_disconnect( priv->store, ( gulong ) it->data );
		}
		g_list_free( priv->store_handlers );

		/* unref object members here */
		g_object_unref( priv->collapse_action );
		g_object_unref( priv->expand_action );
		g_object_unref( priv->reconcil_action );
		g_object_unref( priv->vaccount_action );
		g_object_unref( priv->ventry_action );
		g_object_unref( priv->vope_action );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_unreconcil_page_parent_class )->dispose( instance );
}

static void
ofa_unreconcil_page_init( ofaUnreconcilPage *self )
{
	static const gchar *thisfn = "ofa_unreconcil_page_init";
	ofaUnreconcilPagePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_UNRECONCIL_PAGE( self ));

	priv = ofa_unreconcil_page_get_instance_private( self );

	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_unreconcil_page_class_init( ofaUnreconcilPageClass *klass )
{
	static const gchar *thisfn = "ofa_unreconcil_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = unreconcil_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = unreconcil_page_finalize;

	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = page_v_get_top_focusable_widget;

	OFA_ACTION_PAGE_CLASS( klass )->setup_view = action_page_v_setup_view;
	OFA_ACTION_PAGE_CLASS( klass )->setup_actions = action_page_v_setup_actions;
	OFA_ACTION_PAGE_CLASS( klass )->init_view = action_page_v_init_view;
}

static GtkWidget *
page_v_get_top_focusable_widget( const ofaPage *page )
{
	ofaUnreconcilPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_UNRECONCIL_PAGE( page ), NULL );

	priv = ofa_unreconcil_page_get_instance_private( OFA_UNRECONCIL_PAGE( page ));

	return( ofa_tvbin_get_tree_view( OFA_TVBIN( priv->tview )));
}

static GtkWidget *
action_page_v_setup_view( ofaActionPage *page )
{
	static const gchar *thisfn = "ofa_unreconcil_page_v_setup_view";
	ofaUnreconcilPagePrivate *priv;
	GtkWidget *grid, *subgrid;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_unreconcil_page_get_instance_private( OFA_UNRECONCIL_PAGE( page ));

	priv->getter = ofa_page_get_getter( OFA_PAGE( page ));

	grid = gtk_grid_new();

	priv->tview = ofa_accentry_treeview_new( priv->getter, priv->settings_prefix );
	ofa_accentry_treeview_set_filter_func( priv->tview, ( GtkTreeModelFilterVisibleFunc ) tview_is_visible_row, page );
	ofa_tvbin_set_cell_data_func( OFA_TVBIN( priv->tview ), ( GtkTreeCellDataFunc ) tview_cell_data_render, page );
	gtk_grid_attach( GTK_GRID( grid ), GTK_WIDGET( priv->tview ), 0, 0, 1, 1 );

	g_signal_connect( priv->tview, "ofa-accchanged", G_CALLBACK( tview_on_accchanged ), page );
	g_signal_connect( priv->tview, "ofa-accactivated", G_CALLBACK( tview_on_accactivated ), page );

	subgrid = gtk_grid_new();
	gtk_grid_attach( GTK_GRID( grid ), subgrid, 0, 1, 1, 1 );
	gtk_grid_set_column_homogeneous( GTK_GRID( subgrid ), TRUE );

	priv->account_label = gtk_label_new( " " );
	gtk_label_set_xalign( GTK_LABEL( priv->account_label ), 0 );
	gtk_grid_attach(  GTK_GRID( subgrid ), priv->account_label, 0, 0, 1, 1 );

	priv->entry_label = gtk_label_new( " " );
	gtk_label_set_xalign( GTK_LABEL( priv->entry_label ), 0 );
	gtk_grid_attach(  GTK_GRID( subgrid ), priv->entry_label, 1, 0, 1, 1 );

	return( grid );
}

static void
action_page_v_setup_actions( ofaActionPage *page, ofaButtonsBox *buttons_box )
{
	ofaUnreconcilPagePrivate *priv;

	priv = ofa_unreconcil_page_get_instance_private( OFA_UNRECONCIL_PAGE( page ));

	/* collapse action */
	priv->collapse_action = g_simple_action_new( "collapse", NULL );
	g_signal_connect( priv->collapse_action, "activate", G_CALLBACK( action_on_collapse_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->collapse_action ),
			_( "Collapse all" ));
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->collapse_action ),
					_( "C_ollapse all" )));

	/* expand action */
	priv->expand_action = g_simple_action_new( "expand", NULL );
	g_signal_connect( priv->expand_action, "activate", G_CALLBACK( action_on_expand_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->expand_action ),
			_( "Expand all" ));
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->expand_action ),
					_( "E_xpand all" )));

	ofa_buttons_box_add_spacer( buttons_box );

	/* reconcil action */
	priv->reconcil_action = g_simple_action_new( "reconcil", NULL );
	g_signal_connect( priv->reconcil_action, "activate", G_CALLBACK( action_on_reconcil_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->reconcil_action ),
			_( "Reconciliate..." ));
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->reconcil_action ),
					_( "_Reconciliate..." )));

	/* view account action */
	priv->vaccount_action = g_simple_action_new( "vaccount", NULL );
	g_signal_connect( priv->vaccount_action, "activate", G_CALLBACK( action_on_vaccount_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->vaccount_action ),
			_( "View the account..." ));

	/* view entry action */
	priv->ventry_action = g_simple_action_new( "ventry", NULL );
	g_signal_connect( priv->ventry_action, "activate", G_CALLBACK( action_on_ventry_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->ventry_action ),
			_( "View the entry..." ));

	/* view operation action */
	priv->vope_action = g_simple_action_new( "vope", NULL );
	g_signal_connect( priv->vope_action, "activate", G_CALLBACK( action_on_vope_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->vope_action ),
			_( "View the operation..." ));
}

static void
action_page_v_init_view( ofaActionPage *page )
{
	static const gchar *thisfn = "ofa_unreconcil_page_v_init_view";
	ofaUnreconcilPagePrivate *priv;
	GMenu *menu;
	gboolean is_empty;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_unreconcil_page_get_instance_private( OFA_UNRECONCIL_PAGE( page ));

	menu = ofa_iactionable_get_menu( OFA_IACTIONABLE( page ), priv->settings_prefix );
	ofa_icontext_set_menu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( page ),
			menu );

	menu = ofa_itvcolumnable_get_menu( OFA_ITVCOLUMNABLE( priv->tview ));
	ofa_icontext_append_submenu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( priv->tview ),
			OFA_IACTIONABLE_VISIBLE_COLUMNS_ITEM, menu );

	/* install the store at the very end of the initialization
	 * (i.e. after treeview creation, signals connection, actions and
	 *  menus definition) */
	priv->store = ofa_accentry_store_new( priv->getter );
	ofa_tvbin_set_store( OFA_TVBIN( priv->tview ), GTK_TREE_MODEL( priv->store ));
	g_signal_connect( priv->store, "ofa-istore-need-refilter", G_CALLBACK( store_on_need_refilter ), page );

	ofa_accentry_treeview_expand_all( priv->tview );
	refresh_status_label( OFA_UNRECONCIL_PAGE( page ));

	is_empty = ofa_accentry_store_is_empty( priv->store );
	g_simple_action_set_enabled( priv->collapse_action, !is_empty );
	g_simple_action_set_enabled( priv->expand_action, !is_empty );
}

static gboolean
tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaUnreconcilPage *self )
{
	gboolean visible;
	GObject *object;

	gtk_tree_model_get( tmodel, iter, ACCENTRY_COL_OBJECT, &object, -1 );
	/* as we insert the row before populating it, it may happen that
	 * the object be not set */
	if( !object ){
		return( FALSE );
	}
	g_return_val_if_fail( OFO_IS_ACCOUNT( object ) || OFO_IS_ENTRY( object ), FALSE );
	g_object_unref( object );

	visible = OFO_IS_ACCOUNT( object ) ?
			tview_is_visible_account( self, tmodel, iter, OFO_ACCOUNT( object )) :
			tview_is_visible_entry( self, tmodel, iter, OFO_ENTRY( object ));

	return( visible );
}

/*
 * account is visible if it is reconciliable
 */
static gboolean
tview_is_visible_account( ofaUnreconcilPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoAccount *account )
{
	gboolean visible;

	visible = ofo_account_is_reconciliable( account );

	return( visible );
}

/*
 * entry is visible if on a reconciliable account, and not reconciliated
 */
static gboolean
tview_is_visible_entry( ofaUnreconcilPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoEntry *entry )
{
	ofaUnreconcilPagePrivate *priv;
	const gchar *acc_number;
	ofoAccount *account;
	ofoConcil *concil;
	ofeEntryStatus status;

	priv = ofa_unreconcil_page_get_instance_private( self );

	status = ofo_entry_get_status( entry );
	if( status == ENT_STATUS_DELETED ){
		return( FALSE );
	}

	acc_number = ofo_entry_get_account( entry );
	account = ofo_account_get_by_number( priv->getter, acc_number );

	if( account ){
		g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );

		if( ofo_account_is_reconciliable( account )){
			concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ));
			if( concil == NULL ){
				return( TRUE );
			}
		}
	}

	return( FALSE );
}

static void
tview_cell_data_render( GtkTreeViewColumn *column, GtkCellRenderer *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaAccentryTreeview *self )
{
	ofoBase *object;

	gtk_tree_model_get( tmodel, iter, ACCENTRY_COL_OBJECT, &object, -1 );
	g_return_if_fail( object && ( OFO_IS_ACCOUNT( object ) || OFO_IS_ENTRY( object )));
	g_object_unref( object );

	g_object_set( G_OBJECT( cell ), "weight-set", FALSE, NULL );

	if( OFO_IS_ACCOUNT( object )){
		g_object_set( G_OBJECT( cell ), "weight", PANGO_WEIGHT_BOLD, NULL );
	}
}

static void
tview_on_accchanged( ofaAccentryTreeview *view, ofoBase *object, ofaUnreconcilPage *self )
{
	ofaUnreconcilPagePrivate *priv;
	gboolean reconcil_enabled, vaccount_enabled, ventry_enabled, vope_enabled;
	ofxCounter openum;

	priv = ofa_unreconcil_page_get_instance_private( self );

	/* reconciliate is always enabled
	 * should be disabled when on an account which does not *show* any child
	 * but do not know how to do this */
	reconcil_enabled = TRUE;

	vaccount_enabled = FALSE;
	ventry_enabled = FALSE;
	vope_enabled = FALSE;
	priv->sel_account = NULL;
	priv->sel_entry = NULL;

	if( OFO_IS_ACCOUNT( object )){
		priv->sel_account = OFO_ACCOUNT( object );
		vaccount_enabled = TRUE;

	} else {
		g_return_if_fail( OFO_IS_ENTRY( object ));
		priv->sel_entry = OFO_ENTRY( object );
		ventry_enabled = TRUE;
		openum = ofo_entry_get_ope_number( priv->sel_entry );
		vope_enabled = ( openum > 0 );
		g_list_free( priv->sel_opes );
		priv->sel_opes = openum > 0 ? g_list_append( NULL, ( gpointer ) openum ) : NULL;
	}

	g_simple_action_set_enabled( priv->reconcil_action, reconcil_enabled );
	g_simple_action_set_enabled( priv->vaccount_action, vaccount_enabled );
	g_simple_action_set_enabled( priv->ventry_action, ventry_enabled );
	g_simple_action_set_enabled( priv->vope_action, vope_enabled );
}

static void
tview_on_accactivated( ofaAccentryTreeview *view, ofoBase *object, ofaUnreconcilPage *self )
{
	action_do_reconcil( self, object );
}

static void
refresh_status_label( ofaUnreconcilPage *self )
{
	ofaUnreconcilPagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	guint account_count, entry_count;
	gchar *str;

	priv = ofa_unreconcil_page_get_instance_private( self );

	account_count = 0;
	entry_count = 0;
	tmodel = ofa_tvbin_get_tree_model( OFA_TVBIN( priv->tview ));

	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		refresh_status_label_rec( self, tmodel, &iter, &account_count, &entry_count );
	}

	str = g_strdup_printf( _( "Reconciliable accounts count: %u" ), account_count );
	gtk_label_set_text( GTK_LABEL( priv->account_label ), str );
	g_free( str );

	str = g_strdup_printf( _( "Unreconciliated entries count: %u" ), entry_count );
	gtk_label_set_text( GTK_LABEL( priv->entry_label ), str );
	g_free( str );
}

static void
refresh_status_label_rec( ofaUnreconcilPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, guint *account_count, guint *entry_count )
{
	GtkTreeIter child_iter;
	ofoBase *object;

	while( TRUE ){
		if( gtk_tree_model_iter_children( tmodel, &child_iter, iter )){
			refresh_status_label_rec( self, tmodel, &child_iter, account_count, entry_count );
		}

		gtk_tree_model_get( tmodel, iter, ACCENTRY_COL_OBJECT, &object, -1 );
		g_return_if_fail( object && ( OFO_IS_ACCOUNT( object ) || OFO_IS_ENTRY( object )));
		g_object_unref( object );

		if( OFO_IS_ACCOUNT( object )){
			*account_count += 1;
		} else {
			*entry_count += 1;
		}

		if( !gtk_tree_model_iter_next( tmodel, iter )){
			break;
		}
	}
}

static void
store_on_need_refilter( ofaIStore *store, ofaUnreconcilPage *self )
{
	ofaUnreconcilPagePrivate *priv;

	priv = ofa_unreconcil_page_get_instance_private( self );

	ofa_tvbin_refilter( OFA_TVBIN( priv->tview ));
	ofa_accentry_treeview_expand_all( priv->tview );
}

static void
action_on_collapse_activated( GSimpleAction *action, GVariant *empty, ofaUnreconcilPage *self )
{
	static const gchar *thisfn = "ofa_uncollapsed_page_action_on_collapse_activated";
	ofaUnreconcilPagePrivate *priv;

	g_debug( "%s: action=%p, empty=%p, self=%p",
			thisfn, ( void * ) action, ( void * ) empty, ( void * ) self );

	priv = ofa_unreconcil_page_get_instance_private( self );

	ofa_accentry_treeview_collapse_all( priv->tview );
}

static void
action_on_expand_activated( GSimpleAction *action, GVariant *empty, ofaUnreconcilPage *self )
{
	static const gchar *thisfn = "ofa_unreconcil_page_action_on_expand_activated";
	ofaUnreconcilPagePrivate *priv;

	g_debug( "%s: action=%p, empty=%p, self=%p",
			thisfn, ( void * ) action, ( void * ) empty, ( void * ) self );

	priv = ofa_unreconcil_page_get_instance_private( self );

	ofa_accentry_treeview_expand_all( priv->tview );
}

static void
action_on_reconcil_activated( GSimpleAction *action, GVariant *empty, ofaUnreconcilPage *self )
{
	static const gchar *thisfn = "ofa_unreconcil_page_action_on_reconcil_activated";
	ofaUnreconcilPagePrivate *priv;
	ofoBase *object;

	g_debug( "%s: action=%p, empty=%p, self=%p",
			thisfn, ( void * ) action, ( void * ) empty, ( void * ) self );

	priv = ofa_unreconcil_page_get_instance_private( self );

	object = ofa_accentry_treeview_get_selected( priv->tview );

	action_do_reconcil( self, object );
}

static void
action_do_reconcil( ofaUnreconcilPage *self, ofoBase *object )
{
	ofaUnreconcilPagePrivate *priv;
	ofaIPageManager *manager;
	const gchar *account;
	ofaPage *page;

	g_return_if_fail( object && ( OFO_IS_ACCOUNT( object ) || OFO_IS_ENTRY( object )));

	priv = ofa_unreconcil_page_get_instance_private( self );

	manager = ofa_igetter_get_page_manager( priv->getter );

	account = OFO_IS_ACCOUNT( object ) ?
			ofo_account_get_number( OFO_ACCOUNT( object )) : ofo_entry_get_account( OFO_ENTRY( object ));

	page = ofa_ipage_manager_activate( manager, OFA_TYPE_RECONCIL_PAGE );
	ofa_reconcil_page_set_account( OFA_RECONCIL_PAGE( page ), account );
}

static void
action_on_vaccount_activated( GSimpleAction *action, GVariant *empty, ofaUnreconcilPage *self )
{
	ofaUnreconcilPagePrivate *priv;
	GtkWindow *toplevel;

	priv = ofa_unreconcil_page_get_instance_private( self );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_account_properties_run( priv->getter, toplevel, priv->sel_account );
}

static void
action_on_ventry_activated( GSimpleAction *action, GVariant *empty, ofaUnreconcilPage *self )
{
	ofaUnreconcilPagePrivate *priv;
	GtkWindow *toplevel;

	priv = ofa_unreconcil_page_get_instance_private( self );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_entry_properties_run( priv->getter, toplevel, priv->sel_entry, FALSE );
}

static void
action_on_vope_activated( GSimpleAction *action, GVariant *empty, ofaUnreconcilPage *self )
{
	ofaUnreconcilPagePrivate *priv;

	priv = ofa_unreconcil_page_get_instance_private( self );

	ofa_operation_group_run( priv->getter, NULL, priv->sel_opes );
}
