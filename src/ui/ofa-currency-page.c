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
#include <string.h>

#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofo-currency.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"

#include "core/ofa-currency-store.h"

#include "ui/ofa-currency-page.h"
#include "ui/ofa-currency-properties.h"
#include "ui/ofa-currency-treeview.h"

/* private instance data
 */
typedef struct {

	/* internals
	 */
	ofaHub              *hub;
	GList               *hub_handlers;
	gboolean             is_writable;
	gchar               *settings_prefix;

	/* UI
	 */
	ofaCurrencyTreeview *tview;			/* the main treeview of the page */

	/* actions
	 */
	GSimpleAction       *new_action;
	GSimpleAction       *update_action;
	GSimpleAction       *delete_action;
}
	ofaCurrencyPagePrivate;

static GtkWidget *page_v_get_top_focusable_widget( const ofaPage *page );
static GtkWidget *action_page_v_setup_view( ofaActionPage *page );
static void       action_page_v_setup_actions( ofaActionPage *page, ofaButtonsBox *buttons_box );
static void       action_page_v_init_view( ofaActionPage *page );
static void       on_row_selected( ofaCurrencyTreeview *tview, ofoCurrency *currency, ofaCurrencyPage *self );
static void       on_row_activated( ofaCurrencyTreeview *tview, ofoCurrency *currency, ofaCurrencyPage *self );
static void       on_delete_key( ofaCurrencyTreeview *tview, ofoCurrency *currency, ofaCurrencyPage *self );
static void       on_insert_key( ofaCurrencyTreeview *tview, ofaCurrencyPage *self );
static void       action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaCurrencyPage *self );
static void       action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaCurrencyPage *self );
static void       action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaCurrencyPage *self );
static gboolean   check_for_deletability( ofaCurrencyPage *self, ofoCurrency *class );
static void       delete_with_confirm( ofaCurrencyPage *self, ofoCurrency *class );
static void       hub_connect_to_signaling_system( ofaCurrencyPage *self );
static void       hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaCurrencyPage *self );

G_DEFINE_TYPE_EXTENDED( ofaCurrencyPage, ofa_currency_page, OFA_TYPE_ACTION_PAGE, 0,
		G_ADD_PRIVATE( ofaCurrencyPage ))

static void
currency_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_currency_page_finalize";
	ofaCurrencyPagePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CURRENCY_PAGE( instance ));

	/* free data members here */
	priv = ofa_currency_page_get_instance_private( OFA_CURRENCY_PAGE( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currency_page_parent_class )->finalize( instance );
}

static void
currency_page_dispose( GObject *instance )
{
	ofaCurrencyPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_CURRENCY_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = ofa_currency_page_get_instance_private( OFA_CURRENCY_PAGE( instance ));

		/* note when deconnecting the handlers that the dossier may
		 * have been already finalized (e.g. when the application
		 * terminates) */
		ofa_hub_disconnect_handlers( priv->hub, &priv->hub_handlers );

		g_object_unref( priv->new_action );
		g_object_unref( priv->update_action );
		g_object_unref( priv->delete_action );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_currency_page_parent_class )->dispose( instance );
}

static void
ofa_currency_page_init( ofaCurrencyPage *self )
{
	static const gchar *thisfn = "ofa_currency_page_init";
	ofaCurrencyPagePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_CURRENCY_PAGE( self ));

	priv = ofa_currency_page_get_instance_private( self );

	priv->hub_handlers = NULL;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_currency_page_class_init( ofaCurrencyPageClass *klass )
{
	static const gchar *thisfn = "ofa_currency_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = currency_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = currency_page_finalize;

	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = page_v_get_top_focusable_widget;

	OFA_ACTION_PAGE_CLASS( klass )->setup_view = action_page_v_setup_view;
	OFA_ACTION_PAGE_CLASS( klass )->setup_actions = action_page_v_setup_actions;
	OFA_ACTION_PAGE_CLASS( klass )->init_view = action_page_v_init_view;
}

static GtkWidget *
page_v_get_top_focusable_widget( const ofaPage *page )
{
	ofaCurrencyPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_CURRENCY_PAGE( page ), NULL );

	priv = ofa_currency_page_get_instance_private( OFA_CURRENCY_PAGE( page ));

	return( ofa_tvbin_get_tree_view( OFA_TVBIN( priv->tview )));
}

static GtkWidget *
action_page_v_setup_view( ofaActionPage *page )
{
	static const gchar *thisfn = "ofa_currency_page_v_setup_view";
	ofaCurrencyPagePrivate *priv;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_currency_page_get_instance_private( OFA_CURRENCY_PAGE( page ));

	priv->hub = ofa_igetter_get_hub( OFA_IGETTER( page ));
	g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );
	priv->is_writable = ofa_hub_dossier_is_writable( priv->hub );

	priv->tview = ofa_currency_treeview_new( priv->hub );
	ofa_currency_treeview_set_settings_key( priv->tview, priv->settings_prefix );
	ofa_currency_treeview_setup_columns( priv->tview );

	/* in case the last consumer of a currency disappears, then update
	 * the actions sensitivities */
	hub_connect_to_signaling_system( OFA_CURRENCY_PAGE( page ));

	/* ofaTVBin signals */
	g_signal_connect( priv->tview, "ofa-insert", G_CALLBACK( on_insert_key ), page );

	/* ofaCurrencyTreeview signals */
	g_signal_connect( priv->tview, "ofa-curchanged", G_CALLBACK( on_row_selected ), page );
	g_signal_connect( priv->tview, "ofa-curactivated", G_CALLBACK( on_row_activated ), page );
	g_signal_connect( priv->tview, "ofa-curdelete", G_CALLBACK( on_delete_key ), page );

	return( GTK_WIDGET( priv->tview ));
}

static void
action_page_v_setup_actions( ofaActionPage *page, ofaButtonsBox *buttons_box )
{
	ofaCurrencyPagePrivate *priv;

	priv = ofa_currency_page_get_instance_private( OFA_CURRENCY_PAGE( page ));

	/* new action */
	priv->new_action = g_simple_action_new( "new", NULL );
	g_signal_connect( priv->new_action, "activate", G_CALLBACK( action_on_new_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->new_action ),
			OFA_IACTIONABLE_NEW_ITEM );
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->new_action ),
					OFA_IACTIONABLE_NEW_BTN ));
	g_simple_action_set_enabled( priv->new_action, priv->is_writable );

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
}

static void
action_page_v_init_view( ofaActionPage *page )
{
	static const gchar *thisfn = "ofa_currency_page_v_init_view";
	ofaCurrencyPagePrivate *priv;
	GMenu *menu;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_currency_page_get_instance_private( OFA_CURRENCY_PAGE( page ));

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
	ofa_currency_treeview_setup_store( priv->tview );
}

/*
 * Signal sent by ofaCurrencyTreeview on selection change
 *
 * Other actions do not depend of the selection:
 * - new: enabled when dossier is writable.
 */
static void
on_row_selected( ofaCurrencyTreeview *tview, ofoCurrency *currency, ofaCurrencyPage *self )
{
	ofaCurrencyPagePrivate *priv;
	gboolean is_currency;

	priv = ofa_currency_page_get_instance_private( self );

	is_currency = currency && OFO_IS_CURRENCY( currency );

	g_simple_action_set_enabled( priv->update_action, is_currency );
	g_simple_action_set_enabled( priv->delete_action, check_for_deletability( self, currency ));
}

/*
 * signal sent by ofaCurrencyTreeview on selection activation
 */
static void
on_row_activated( ofaCurrencyTreeview *tview, ofoCurrency *currency, ofaCurrencyPage *self )
{
	ofaCurrencyPagePrivate *priv;

	priv = ofa_currency_page_get_instance_private( self );

	g_action_activate( G_ACTION( priv->update_action ), NULL );
}

/*
 * signal sent by ofaCurrencyTreeview on Delete key
 *
 * Note that the key may be pressed, even if the button
 * is disabled. So have to check all prerequisite conditions.
 * If the current row is not deletable, just silently ignore the key.
 */
static void
on_delete_key( ofaCurrencyTreeview *tview, ofoCurrency *currency, ofaCurrencyPage *self )
{
	if( check_for_deletability( self, currency )){
		delete_with_confirm( self, currency );
	}
}

/*
 * signal sent by ofaTVBin on Insert key
 *
 * Note that the key may be pressend even if dossier is not writable.
 * If this is the case, just silently ignore the key.
 */
static void
on_insert_key( ofaCurrencyTreeview *tview, ofaCurrencyPage *self )
{
	ofaCurrencyPagePrivate *priv;

	priv = ofa_currency_page_get_instance_private( self );

	if( priv->is_writable ){
		g_action_activate( G_ACTION( priv->new_action ), NULL );
	}
}

static void
action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaCurrencyPage *self )
{
	ofoCurrency *currency;
	GtkWindow *toplevel;

	currency = ofo_currency_new();
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_currency_properties_run( OFA_IGETTER( self ), toplevel, currency );
}

static void
action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaCurrencyPage *self )
{
	ofaCurrencyPagePrivate *priv;
	ofoCurrency *currency;
	GtkWindow *toplevel;

	priv = ofa_currency_page_get_instance_private( self );

	currency = ofa_currency_treeview_get_selected( priv->tview );
	if( currency ){
		toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
		ofa_currency_properties_run( OFA_IGETTER( self ), toplevel, currency );
	}
}

static void
action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaCurrencyPage *self )
{
	ofaCurrencyPagePrivate *priv;
	ofoCurrency *currency;

	priv = ofa_currency_page_get_instance_private( self );

	currency = ofa_currency_treeview_get_selected( priv->tview );
	g_return_if_fail( check_for_deletability( self, currency ));

	delete_with_confirm( self, currency );
}

static gboolean
check_for_deletability( ofaCurrencyPage *self, ofoCurrency *currency )
{
	ofaCurrencyPagePrivate *priv;
	gboolean is_currency;

	priv = ofa_currency_page_get_instance_private( self );

	is_currency = currency && OFO_IS_CURRENCY( currency );

	return( is_currency && priv->is_writable && ofo_currency_is_deletable( currency ));
}

static void
delete_with_confirm( ofaCurrencyPage *self, ofoCurrency *currency )
{
	gchar *msg;
	gboolean delete_ok;
	GtkWindow *toplevel;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s - %s' currency ?" ),
			ofo_currency_get_code( currency ),
			ofo_currency_get_label( currency ));

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	delete_ok = my_utils_dialog_question( toplevel, msg, _( "_Delete" ));

	g_free( msg );

	if( delete_ok ){
		ofo_currency_delete( currency );
	}
}

/*
 * connect to the dossier signaling system
 */
static void
hub_connect_to_signaling_system( ofaCurrencyPage *self )
{
	ofaCurrencyPagePrivate *priv;
	gulong handler;

	priv = ofa_currency_page_get_instance_private( self );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_UPDATED, G_CALLBACK( hub_on_updated_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 *
 * When a first object takes a reference on a currency, or when an
 * object releases the last reference on a currency, then the actions
 * sensitivity should be reviewed.
 */
static void
hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaCurrencyPage *self )
{
	static const gchar *thisfn = "ofa_currency_page_hub_on_updated_object";

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn, ( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id, ( void * ) self );

	/* make sure the buttons reflect the new currency of the account */
	if( OFO_IS_ACCOUNT( object )){
		//do_on_updated_account( self, OFO_ACCOUNT( object ));
	}
}
