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

#include "api/ofa-buttons-box.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-ipage-manager.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-page.h"
#include "api/ofo-account.h"
#include "api/ofo-entry.h"

#include "ui/ofa-accentry-store.h"
#include "ui/ofa-accentry-treeview.h"
#include "ui/ofa-settlement-page.h"
#include "ui/ofa-unsettled-page.h"

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

	/* actions
	 */
	GSimpleAction       *collapse_action;
	GSimpleAction       *expand_action;
	GSimpleAction       *settle_action;
}
	ofaUnsettledPagePrivate;

static GtkWidget *page_v_get_top_focusable_widget( const ofaPage *page );
static GtkWidget *action_page_v_setup_view( ofaActionPage *page );
static void       action_page_v_setup_actions( ofaActionPage *page, ofaButtonsBox *buttons_box );
static void       action_page_v_init_view( ofaActionPage *page );
static gboolean   tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaUnsettledPage *self );
static gboolean   tview_is_visible_account( ofaUnsettledPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoAccount *account );
static gboolean   tview_is_visible_entry( ofaUnsettledPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoEntry *entry );
static void       action_on_collapse_activated( GSimpleAction *action, GVariant *empty, ofaUnsettledPage *self );
static void       action_on_expand_activated( GSimpleAction *action, GVariant *empty, ofaUnsettledPage *self );
static void       action_on_settle_activated( GSimpleAction *action, GVariant *empty, ofaUnsettledPage *self );
//static void       store_on_changed( ofaAccentryStore *store, ofaUnsettledPage *self );

G_DEFINE_TYPE_EXTENDED( ofaUnsettledPage, ofa_unsettled_page, OFA_TYPE_ACTION_PAGE, 0,
		G_ADD_PRIVATE( ofaUnsettledPage ))

static void
unsettled_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_unsettled_page_finalize";
	ofaUnsettledPagePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_UNSETTLED_PAGE( instance ));

	/* free data members here */
	priv = ofa_unsettled_page_get_instance_private( OFA_UNSETTLED_PAGE( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_unsettled_page_parent_class )->finalize( instance );
}

static void
unsettled_page_dispose( GObject *instance )
{
	ofaUnsettledPagePrivate *priv;
	GList *it;

	g_return_if_fail( instance && OFA_IS_UNSETTLED_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		priv = ofa_unsettled_page_get_instance_private( OFA_UNSETTLED_PAGE( instance ));

		/* disconnect ofaEntryStore signal handlers */
		for( it=priv->store_handlers ; it ; it=it->next ){
			g_signal_handler_disconnect( priv->store, ( gulong ) it->data );
		}
		g_list_free( priv->store_handlers );

		/* unref object members here */
		g_object_unref( priv->settle_action );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_unsettled_page_parent_class )->dispose( instance );
}

static void
ofa_unsettled_page_init( ofaUnsettledPage *self )
{
	static const gchar *thisfn = "ofa_unsettled_page_init";
	ofaUnsettledPagePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_UNSETTLED_PAGE( self ));

	priv = ofa_unsettled_page_get_instance_private( self );

	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_unsettled_page_class_init( ofaUnsettledPageClass *klass )
{
	static const gchar *thisfn = "ofa_unsettled_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = unsettled_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = unsettled_page_finalize;

	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = page_v_get_top_focusable_widget;

	OFA_ACTION_PAGE_CLASS( klass )->setup_view = action_page_v_setup_view;
	OFA_ACTION_PAGE_CLASS( klass )->setup_actions = action_page_v_setup_actions;
	OFA_ACTION_PAGE_CLASS( klass )->init_view = action_page_v_init_view;
}

static GtkWidget *
page_v_get_top_focusable_widget( const ofaPage *page )
{
	ofaUnsettledPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_UNSETTLED_PAGE( page ), NULL );

	priv = ofa_unsettled_page_get_instance_private( OFA_UNSETTLED_PAGE( page ));

	return( ofa_tvbin_get_tree_view( OFA_TVBIN( priv->tview )));
}

static GtkWidget *
action_page_v_setup_view( ofaActionPage *page )
{
	static const gchar *thisfn = "ofa_unsettled_page_v_setup_view";
	ofaUnsettledPagePrivate *priv;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_unsettled_page_get_instance_private( OFA_UNSETTLED_PAGE( page ));

	priv->getter = ofa_page_get_getter( OFA_PAGE( page ));

	priv->tview = ofa_accentry_treeview_new( priv->getter, priv->settings_prefix );
	ofa_accentry_treeview_setup_columns( priv->tview );
	ofa_accentry_treeview_set_filter_func( priv->tview, ( GtkTreeModelFilterVisibleFunc ) tview_is_visible_row, page );

	return( GTK_WIDGET( priv->tview ));
}

static void
action_page_v_setup_actions( ofaActionPage *page, ofaButtonsBox *buttons_box )
{
	ofaUnsettledPagePrivate *priv;

	priv = ofa_unsettled_page_get_instance_private( OFA_UNSETTLED_PAGE( page ));

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

	/* settle action */
	priv->settle_action = g_simple_action_new( "settle", NULL );
	g_signal_connect( priv->settle_action, "activate", G_CALLBACK( action_on_settle_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->settle_action ),
			_( "Settle..." ));
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->settle_action ),
					_( "_Settle..." )));
}

static void
action_page_v_init_view( ofaActionPage *page )
{
	static const gchar *thisfn = "ofa_unsettled_page_v_init_view";
	ofaUnsettledPagePrivate *priv;
	GMenu *menu;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_unsettled_page_get_instance_private( OFA_UNSETTLED_PAGE( page ));

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

	ofa_accentry_treeview_expand_all( priv->tview );
}

static gboolean
tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaUnsettledPage *self )
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
 * account is visible if it is settleable
 */
static gboolean
tview_is_visible_account( ofaUnsettledPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoAccount *account )
{
	gboolean visible;

	visible = ofo_account_is_settleable( account );

	return( visible );
}

/*
 * entry is visible if on a settleable account, and not settled
 */
static gboolean
tview_is_visible_entry( ofaUnsettledPage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoEntry *entry )
{
	ofaUnsettledPagePrivate *priv;
	const gchar *acc_number;
	ofoAccount *account;
	ofxCounter stlmt_number;

	priv = ofa_unsettled_page_get_instance_private( self );

	acc_number = ofo_entry_get_account( entry );
	account = ofo_account_get_by_number( priv->getter, acc_number );

	if( account ){
		g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );

		if( ofo_account_is_settleable( account )){
			stlmt_number = ofo_entry_get_settlement_number( entry );
			if( stlmt_number == 0 ){
				return( TRUE );
			}
		}
	}

	return( FALSE );
}

static void
action_on_collapse_activated( GSimpleAction *action, GVariant *empty, ofaUnsettledPage *self )
{
	static const gchar *thisfn = "ofa_uncollapsed_page_action_on_collapse_activated";
	ofaUnsettledPagePrivate *priv;

	g_debug( "%s: action=%p, empty=%p, self=%p",
			thisfn, ( void * ) action, ( void * ) empty, ( void * ) self );

	priv = ofa_unsettled_page_get_instance_private( self );

	ofa_accentry_treeview_collapse_all( priv->tview );
}

static void
action_on_expand_activated( GSimpleAction *action, GVariant *empty, ofaUnsettledPage *self )
{
	static const gchar *thisfn = "ofa_unsettled_page_action_on_expand_activated";
	ofaUnsettledPagePrivate *priv;

	g_debug( "%s: action=%p, empty=%p, self=%p",
			thisfn, ( void * ) action, ( void * ) empty, ( void * ) self );

	priv = ofa_unsettled_page_get_instance_private( self );

	ofa_accentry_treeview_expand_all( priv->tview );
}

static void
action_on_settle_activated( GSimpleAction *action, GVariant *empty, ofaUnsettledPage *self )
{
	static const gchar *thisfn = "ofa_unsettled_page_action_on_settle_activated";
	ofaUnsettledPagePrivate *priv;
	ofaIPageManager *manager;
	ofoBase *object;
	const gchar *account;
	ofaPage *page;

	g_debug( "%s: action=%p, empty=%p, self=%p",
			thisfn, ( void * ) action, ( void * ) empty, ( void * ) self );

	priv = ofa_unsettled_page_get_instance_private( self );

	manager = ofa_igetter_get_page_manager( priv->getter );

	object = ofa_accentry_treeview_get_selected( priv->tview );
	g_return_if_fail( object && ( OFO_IS_ACCOUNT( object ) || OFO_IS_ENTRY( object )));

	account = OFO_IS_ACCOUNT( object ) ?
			ofo_account_get_number( OFO_ACCOUNT( object )) : ofo_entry_get_account( OFO_ENTRY( object ));

	page = ofa_ipage_manager_activate( manager, OFA_TYPE_SETTLEMENT_PAGE );
	ofa_settlement_page_set_account( OFA_SETTLEMENT_PAGE( page ), account );
}
