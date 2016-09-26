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

#include <glib/gi18n.h>

#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itheme-manager.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"

#include "ui/ofa-entry-page.h"
#include "ui/ofa-ledger-properties.h"
#include "ui/ofa-ledger-treeview.h"
#include "ui/ofa-ledger-page.h"

/* private instance data
 */
typedef struct {

	/* internals
	 */
	ofaHub            *hub;
	gboolean           is_writable;
	gint               exe_id;			/* internal identifier of the current exercice */
	gchar             *settings_prefix;

	/* UI
	 */
	ofaLedgerTreeview *tview;

	/* actions
	 */
	GSimpleAction     *new_action;
	GSimpleAction     *update_action;
	GSimpleAction     *delete_action;
	GSimpleAction     *view_entries_action;
}
	ofaLedgerPagePrivate;

static GtkWidget *page_v_get_top_focusable_widget( const ofaPage *page );
static GtkWidget *action_page_v_setup_view( ofaActionPage *page );
static GtkWidget *setup_treeview( ofaLedgerPage *page );
static void       action_page_v_setup_actions( ofaActionPage *page, ofaButtonsBox *buttons_box );
static void       action_page_v_init_view( ofaActionPage *page );
static void       on_row_selected( ofaLedgerTreeview *view, GList *list, ofaLedgerPage *self );
static void       on_row_activated( ofaLedgerTreeview *view, GList *list, ofaLedgerPage *self );
static void       on_insert_key( ofaLedgerTreeview *view, ofaLedgerPage *self );
static void       on_delete_key( ofaLedgerTreeview *view, ofoLedger *ledger, ofaLedgerPage *self );
static void       action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaLedgerPage *self );
static void       action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaLedgerPage *self );
static void       action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaLedgerPage *self );
static gboolean   check_for_deletability( ofaLedgerPage *self, ofoLedger *ledger );
static void       delete_with_confirm( ofaLedgerPage *self, ofoLedger *ledger );
static void       action_on_view_entries_activated( GSimpleAction *action, GVariant *empty, ofaLedgerPage *self );

G_DEFINE_TYPE_EXTENDED( ofaLedgerPage, ofa_ledger_page, OFA_TYPE_ACTION_PAGE, 0,
		G_ADD_PRIVATE( ofaLedgerPage ))

static void
ledger_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledger_page_finalize";
	ofaLedgerPagePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGER_PAGE( instance ));

	/* free data members here */
	priv = ofa_ledger_page_get_instance_private( OFA_LEDGER_PAGE( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_page_parent_class )->finalize( instance );
}

static void
ledger_page_dispose( GObject *instance )
{
	ofaLedgerPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_LEDGER_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = ofa_ledger_page_get_instance_private( OFA_LEDGER_PAGE( instance ));

		g_object_unref( priv->new_action );
		g_object_unref( priv->update_action );
		g_object_unref( priv->delete_action );
		g_object_unref( priv->view_entries_action );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_page_parent_class )->dispose( instance );
}

static void
ofa_ledger_page_init( ofaLedgerPage *self )
{
	static const gchar *thisfn = "ofa_ledger_page_init";
	ofaLedgerPagePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_LEDGER_PAGE( self ));

	priv = ofa_ledger_page_get_instance_private( self );

	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_ledger_page_class_init( ofaLedgerPageClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledger_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledger_page_finalize;

	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = page_v_get_top_focusable_widget;

	OFA_ACTION_PAGE_CLASS( klass )->setup_view = action_page_v_setup_view;
	OFA_ACTION_PAGE_CLASS( klass )->setup_actions = action_page_v_setup_actions;
	OFA_ACTION_PAGE_CLASS( klass )->init_view = action_page_v_init_view;
}

static GtkWidget *
page_v_get_top_focusable_widget( const ofaPage *page )
{
	ofaLedgerPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_LEDGER_PAGE( page ), NULL );

	priv = ofa_ledger_page_get_instance_private( OFA_LEDGER_PAGE( page ));

	return( ofa_tvbin_get_treeview( OFA_TVBIN( priv->tview )));
}

static GtkWidget *
action_page_v_setup_view( ofaActionPage *page )
{
	static const gchar *thisfn = "ofa_ledger_page_v_setup_view";
	ofaLedgerPagePrivate *priv;
	GtkWidget *frame;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_ledger_page_get_instance_private( OFA_LEDGER_PAGE( page ));

	priv->hub = ofa_igetter_get_hub( OFA_IGETTER( page ));
	g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );
	priv->is_writable = ofa_hub_dossier_is_writable( priv->hub );

	frame = setup_treeview( OFA_LEDGER_PAGE( page ));

	return( frame );
}

static GtkWidget *
setup_treeview( ofaLedgerPage *page )
{
	ofaLedgerPagePrivate *priv;

	priv = ofa_ledger_page_get_instance_private( page );

	priv->tview = ofa_ledger_treeview_new();
	ofa_ledger_treeview_set_settings_key( priv->tview, priv->settings_prefix );
	ofa_ledger_treeview_setup_columns( priv->tview );
	ofa_tvbin_set_selection_mode( OFA_TVBIN( priv->tview ), GTK_SELECTION_BROWSE );

	/* ofaTVBin signals */
	g_signal_connect( priv->tview, "ofa-insert", G_CALLBACK( on_insert_key ), page );

	/* ofaLedgerTreeview signals */
	g_signal_connect( priv->tview, "ofa-ledchanged", G_CALLBACK( on_row_selected ), page );
	g_signal_connect( priv->tview, "ofa-ledactivated", G_CALLBACK( on_row_activated ), page );
	g_signal_connect( priv->tview, "ofa-leddelete", G_CALLBACK( on_delete_key ), page );

	return( GTK_WIDGET( priv->tview ));
}

static void
action_page_v_setup_actions( ofaActionPage *page, ofaButtonsBox *buttons_box )
{
	ofaLedgerPagePrivate *priv;

	priv = ofa_ledger_page_get_instance_private( OFA_LEDGER_PAGE( page ));

	/* new action */
	priv->new_action = g_simple_action_new( "new", NULL );
	g_simple_action_set_enabled( priv->new_action, priv->is_writable );
	g_signal_connect( priv->new_action, "activate", G_CALLBACK( action_on_new_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->new_action ),
			OFA_IACTIONABLE_NEW_ITEM );
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->new_action ),
					OFA_IACTIONABLE_NEW_BTN ));

	/* update action */
	priv->update_action = g_simple_action_new( "update", NULL );
	g_signal_connect( priv->update_action, "activate", G_CALLBACK( action_on_update_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->update_action ),
			priv->is_writable ? OFA_IACTIONABLE_PROPERTIES_ITEM_EDIT : OFA_IACTIONABLE_PROPERTIES_ITEM_DISPLAY );
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->update_action ),
					OFA_IACTIONABLE_PROPERTIES_BTN ));

	/* delete action */
	priv->delete_action = g_simple_action_new( "delete", NULL );
	g_signal_connect( priv->delete_action, "activate", G_CALLBACK( action_on_delete_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->delete_action ),
			OFA_IACTIONABLE_DELETE_ITEM );
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->delete_action ),
					OFA_IACTIONABLE_DELETE_BTN ));

	ofa_buttons_box_add_spacer( buttons_box );

	/* view entries */
	priv->view_entries_action = g_simple_action_new( "viewentries", NULL );
	g_signal_connect( priv->view_entries_action, "activate", G_CALLBACK( action_on_view_entries_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->view_entries_action ),
			_( "View entries" ));
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->view_entries_action ),
					_( "_View entries..." )));
}

static void
action_page_v_init_view( ofaActionPage *page )
{
	static const gchar *thisfn = "ofa_ledger_page_v_init_view";
	ofaLedgerPagePrivate *priv;
	GMenu *menu;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_ledger_page_get_instance_private( OFA_LEDGER_PAGE( page ));

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
	ofa_ledger_treeview_set_hub( priv->tview, priv->hub );
}

/*
 * LedgerTreeview callback
 */
static void
on_row_selected( ofaLedgerTreeview *view, GList *list, ofaLedgerPage *self )
{
	ofaLedgerPagePrivate *priv;
	ofoLedger *ledger;
	gboolean is_ledger;

	priv = ofa_ledger_page_get_instance_private( self );

	ledger = list ? ( ofoLedger * ) list->data : NULL;
	is_ledger = ledger && OFO_IS_LEDGER( ledger );

	g_simple_action_set_enabled( priv->update_action, is_ledger );
	g_simple_action_set_enabled( priv->delete_action, check_for_deletability( self, ledger ));
	g_simple_action_set_enabled( priv->view_entries_action, is_ledger && ofo_ledger_has_entries( ledger ));
}

/*
 * LedgerTreeview callback
 */
static void
on_row_activated( ofaLedgerTreeview *view, GList *list, ofaLedgerPage *self )
{
	ofaLedgerPagePrivate *priv;

	priv = ofa_ledger_page_get_instance_private( self );

	g_action_activate( G_ACTION( priv->update_action ), NULL );
}

static void
on_insert_key( ofaLedgerTreeview *view, ofaLedgerPage *self )
{
	ofaLedgerPagePrivate *priv;

	priv = ofa_ledger_page_get_instance_private( self );

	if( priv->is_writable ){
		g_action_activate( G_ACTION( priv->new_action ), NULL );
	}
}

/*
 * only delete if there is only one selected ledger
 */
static void
on_delete_key( ofaLedgerTreeview *view, ofoLedger *ledger, ofaLedgerPage *self )
{
	ofaLedgerPagePrivate *priv;

	priv = ofa_ledger_page_get_instance_private( self );

	if( check_for_deletability( self, ledger )){
		g_action_activate( G_ACTION( priv->delete_action ), NULL );
	}
}

static void
action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaLedgerPage *self )
{
	ofoLedger *ledger;
	GtkWindow *toplevel;

	ledger = ofo_ledger_new();
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_ledger_properties_run( OFA_IGETTER( self ), toplevel, ledger );
}

/*
 * selection mode is BROWSE: we expect to have here one and only one
 * selected object
 */
static void
action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaLedgerPage *self )
{
	ofaLedgerPagePrivate *priv;
	GList *selected;
	ofoLedger *ledger;
	GtkWindow *toplevel;

	priv = ofa_ledger_page_get_instance_private( self );

	selected = ofa_ledger_treeview_get_selected( priv->tview );
	ledger = ( ofoLedger * ) selected->data;
	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_ledger_properties_run( OFA_IGETTER( self ), toplevel, ledger );

	ofa_ledger_treeview_free_selected( selected );
}

/*
 * a ledger can be deleted while no entry has been recorded in it,
 * and after user confirm.
 */
static void
action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaLedgerPage *self )
{
	ofaLedgerPagePrivate *priv;
	ofoLedger *ledger;
	GList *selected;

	priv = ofa_ledger_page_get_instance_private( self );

	selected = ofa_ledger_treeview_get_selected( priv->tview );
	if( g_list_length( selected ) == 1 ){
		ledger = ( ofoLedger * ) selected->data;
		g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));
		g_return_if_fail( ofo_ledger_is_deletable( ledger ));
		delete_with_confirm( self, ledger );
	}
	ofa_ledger_treeview_free_selected( selected );

	gtk_widget_grab_focus( page_v_get_top_focusable_widget( OFA_PAGE( self )));
}

static gboolean
check_for_deletability( ofaLedgerPage *self, ofoLedger *ledger )
{
	ofaLedgerPagePrivate *priv;
	gboolean is_ledger;

	priv = ofa_ledger_page_get_instance_private( self );

	is_ledger = ledger && OFO_IS_LEDGER( ledger );

	return( priv->is_writable && is_ledger && ofo_ledger_is_deletable( ledger ));
}

static void
delete_with_confirm( ofaLedgerPage *self, ofoLedger *ledger )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want to delete the '%s - %s' ledger ?" ),
			ofo_ledger_get_mnemo( ledger ),
			ofo_ledger_get_label( ledger ));

	delete_ok = my_utils_dialog_question( msg, _( "_Delete" ));

	g_free( msg );

	if( delete_ok ){
		ofo_ledger_delete( ledger );
	}
}

static void
action_on_view_entries_activated( GSimpleAction *action, GVariant *empty, ofaLedgerPage *self )
{
	ofaLedgerPagePrivate *priv;
	GList *list;
	ofaPage *page;
	const gchar *mnemo;
	ofoLedger *ledger;
	ofaIThemeManager *manager;

	g_return_if_fail( self && OFA_IS_LEDGER_PAGE( self ));

	priv = ofa_ledger_page_get_instance_private( self );

	list = ofa_ledger_treeview_get_selected( priv->tview );
	g_return_if_fail( list && list->data );

	mnemo = ( const gchar * ) list->data;
	ledger = ofo_ledger_get_by_mnemo( priv->hub, mnemo );
	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));

	ofa_ledger_treeview_free_selected( list );

	manager = ofa_igetter_get_theme_manager( OFA_IGETTER( self ));
	page = ofa_itheme_manager_activate( manager, OFA_TYPE_ENTRY_PAGE );
	g_return_if_fail( page && OFA_IS_ENTRY_PAGE( page ));

	ofa_entry_page_display_entries(
			OFA_ENTRY_PAGE( page ), OFO_TYPE_LEDGER, ofo_ledger_get_mnemo( ledger ), NULL, NULL );
}
