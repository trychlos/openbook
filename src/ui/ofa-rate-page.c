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

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofo-dossier.h"
#include "api/ofo-rate.h"

#include "ui/ofa-rate-page.h"
#include "ui/ofa-rate-properties.h"
#include "ui/ofa-rate-treeview.h"

/* private instance data
 */
typedef struct {

	/* internals
	 */
	ofaIGetter      *getter;
	gboolean         is_writable;
	gchar           *settings_prefix;

	/* UI
	 */
	ofaRateTreeview *tview;

	/* actions
	 */
	GSimpleAction   *new_action;
	GSimpleAction   *update_action;
	GSimpleAction   *delete_action;
}
	ofaRatePagePrivate;

static GtkWidget *page_v_get_top_focusable_widget( const ofaPage *page );
static GtkWidget *action_page_v_setup_view( ofaActionPage *page );
static void       action_page_v_setup_actions( ofaActionPage *page, ofaButtonsBox *buttons_box );
static void       action_page_v_init_view( ofaActionPage *page );
static void       on_row_selected( ofaRateTreeview *tview, ofoRate *rate, ofaRatePage *self );
static void       on_row_activated( ofaRateTreeview *tview, ofoRate *rate, ofaRatePage *self );
static void       on_delete_key( ofaRateTreeview *tview, ofoRate *rate, ofaRatePage *self );
static void       on_insert_key( ofaRateTreeview *tview, ofaRatePage *self );
static void       action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaRatePage *self );
static void       action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaRatePage *self );
static void       action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaRatePage *self );
static gboolean   check_for_deletability( ofaRatePage *self, ofoRate *class );
static void       delete_with_confirm( ofaRatePage *self, ofoRate *class );

G_DEFINE_TYPE_EXTENDED( ofaRatePage, ofa_rate_page, OFA_TYPE_ACTION_PAGE, 0,
		G_ADD_PRIVATE( ofaRatePage ))

static void
rate_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_rate_page_finalize";
	ofaRatePagePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RATE_PAGE( instance ));

	/* free data members here */
	priv = ofa_rate_page_get_instance_private( OFA_RATE_PAGE( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rate_page_parent_class )->finalize( instance );
}

static void
rate_page_dispose( GObject *instance )
{
	ofaRatePagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_RATE_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = ofa_rate_page_get_instance_private( OFA_RATE_PAGE( instance ));

		g_object_unref( priv->new_action );
		g_object_unref( priv->update_action );
		g_object_unref( priv->delete_action );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rate_page_parent_class )->dispose( instance );
}

static void
ofa_rate_page_init( ofaRatePage *self )
{
	static const gchar *thisfn = "ofa_rate_page_init";
	ofaRatePagePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RATE_PAGE( self ));

	priv = ofa_rate_page_get_instance_private( self );

	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_rate_page_class_init( ofaRatePageClass *klass )
{
	static const gchar *thisfn = "ofa_rate_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = rate_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = rate_page_finalize;

	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = page_v_get_top_focusable_widget;

	OFA_ACTION_PAGE_CLASS( klass )->setup_view = action_page_v_setup_view;
	OFA_ACTION_PAGE_CLASS( klass )->setup_actions = action_page_v_setup_actions;
	OFA_ACTION_PAGE_CLASS( klass )->init_view = action_page_v_init_view;
}

static GtkWidget *
page_v_get_top_focusable_widget( const ofaPage *page )
{
	ofaRatePagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_RATE_PAGE( page ), NULL );

	priv = ofa_rate_page_get_instance_private( OFA_RATE_PAGE( page ));

	return( ofa_tvbin_get_tree_view( OFA_TVBIN( priv->tview )));
}

static GtkWidget *
action_page_v_setup_view( ofaActionPage *page )
{
	static const gchar *thisfn = "ofa_rate_page_v_setup_view";
	ofaRatePagePrivate *priv;
	ofaHub *hub;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_rate_page_get_instance_private( OFA_RATE_PAGE( page ));

	priv->getter = ofa_page_get_getter( OFA_PAGE( page ));

	hub = ofa_igetter_get_hub( priv->getter );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	priv->is_writable = ofa_hub_is_writable_dossier( hub );

	priv->tview = ofa_rate_treeview_new( priv->getter, priv->settings_prefix );

	/* ofaTVBin signals */
	g_signal_connect( priv->tview, "ofa-insert", G_CALLBACK( on_insert_key ), page );

	/* ofaRateTreeview signals */
	g_signal_connect( priv->tview, "ofa-ratchanged", G_CALLBACK( on_row_selected ), page );
	g_signal_connect( priv->tview, "ofa-ratactivated", G_CALLBACK( on_row_activated ), page );
	g_signal_connect( priv->tview, "ofa-ratdelete", G_CALLBACK( on_delete_key ), page );

	return( GTK_WIDGET( priv->tview ));
}

static void
action_page_v_setup_actions( ofaActionPage *page, ofaButtonsBox *buttons_box )
{
	ofaRatePagePrivate *priv;

	priv = ofa_rate_page_get_instance_private( OFA_RATE_PAGE( page ));

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
	static const gchar *thisfn = "ofa_rate_page_v_init_view";
	ofaRatePagePrivate *priv;
	GMenu *menu;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_rate_page_get_instance_private( OFA_RATE_PAGE( page ));

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
	ofa_rate_treeview_setup_store( priv->tview );
}

/*
 * Signal sent by ofaRateTreeview on selection change
 *
 * Other actions do not depend of the selection:
 * - new: enabled when dossier is writable.
 */
static void
on_row_selected( ofaRateTreeview *tview, ofoRate *rate, ofaRatePage *self )
{
	ofaRatePagePrivate *priv;
	gboolean is_rate;

	priv = ofa_rate_page_get_instance_private( self );

	is_rate = rate && OFO_IS_RATE( rate );

	g_simple_action_set_enabled( priv->update_action, is_rate );
	g_simple_action_set_enabled( priv->delete_action, check_for_deletability( self, rate ));
}

/*
 * signal sent by ofaRateTreeview on selection activation
 */
static void
on_row_activated( ofaRateTreeview *tview, ofoRate *rate, ofaRatePage *self )
{
	ofaRatePagePrivate *priv;

	priv = ofa_rate_page_get_instance_private( self );

	g_action_activate( G_ACTION( priv->update_action ), NULL );
}

/*
 * signal sent by ofaTVBin on Insert key
 *
 * Note that the key may be pressend even if dossier is not writable.
 * If this is the case, just silently ignore the key.
 */
static void
on_insert_key( ofaRateTreeview *tview, ofaRatePage *self )
{
	ofaRatePagePrivate *priv;

	priv = ofa_rate_page_get_instance_private( self );

	if( priv->is_writable ){
		g_action_activate( G_ACTION( priv->new_action ), NULL );
	}
}

/*
 * signal sent by ofaRateTreeview on Delete key
 *
 * Note that the key may be pressed, even if the button
 * is disabled. So have to check all prerequisite conditions.
 * If the current row is not deletable, just silently ignore the key.
 */
static void
on_delete_key( ofaRateTreeview *tview, ofoRate *rate, ofaRatePage *self )
{
	if( check_for_deletability( self, rate )){
		delete_with_confirm( self, rate );
	}
}

static void
action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaRatePage *self )
{
	ofaRatePagePrivate *priv;
	ofoRate *rate;
	GtkWindow *toplevel;

	priv = ofa_rate_page_get_instance_private( self );

	rate = ofo_rate_new( priv->getter );
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_rate_properties_run( priv->getter, toplevel, rate );
}

static void
action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaRatePage *self )
{
	ofaRatePagePrivate *priv;
	ofoRate *rate;
	GtkWindow *toplevel;

	priv = ofa_rate_page_get_instance_private( self );

	rate = ofa_rate_treeview_get_selected( priv->tview );
	g_return_if_fail( rate && OFO_IS_RATE( rate ));

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_rate_properties_run( priv->getter, toplevel, rate );
}

static void
action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaRatePage *self )
{
	ofaRatePagePrivate *priv;
	ofoRate *rate;

	priv = ofa_rate_page_get_instance_private( self );

	rate = ofa_rate_treeview_get_selected( priv->tview );
	g_return_if_fail( rate && OFO_IS_RATE( rate ));

	delete_with_confirm( self, rate );
}

static gboolean
check_for_deletability( ofaRatePage *self, ofoRate *rate )
{
	ofaRatePagePrivate *priv;
	gboolean is_rate;

	priv = ofa_rate_page_get_instance_private( self );

	is_rate = rate && OFO_IS_RATE( rate );

	return( is_rate && priv->is_writable && ofo_rate_is_deletable( rate ));
}

static void
delete_with_confirm( ofaRatePage *self, ofoRate *rate )
{
	gchar *msg;
	gboolean delete_ok;
	GtkWindow *toplevel;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s - %s' rate ?" ),
			ofo_rate_get_mnemo( rate ),
			ofo_rate_get_label( rate ));

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	delete_ok = my_utils_dialog_question( toplevel, msg, _( "_Delete" ));

	g_free( msg );

	if( delete_ok ){
		ofo_rate_delete( rate );
	}
}
