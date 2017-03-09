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
#include "api/ofa-ipage-manager.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofo-dossier.h"

#include "ofa-recurrent-main.h"
#include "ofa-rec-period-page.h"
#include "ofa-rec-period-properties.h"
#include "ofa-rec-period-store.h"
#include "ofa-rec-period-treeview.h"

/* priv instance data
 */
typedef struct {

	/* internals
	 */
	ofaIGetter           *getter;
	gboolean              is_writable;
	gchar                *settings_prefix;

	/* UI
	 */
	ofaRecPeriodTreeview *tview;

	/* actions
	 */
	GSimpleAction        *new_action;
	GSimpleAction        *update_action;
	GSimpleAction        *delete_action;
}
	ofaRecPeriodPagePrivate;

static GtkWidget *page_v_get_top_focusable_widget( const ofaPage *page );
static GtkWidget *action_page_v_setup_view( ofaActionPage *page );
static GtkWidget *setup_treeview( ofaRecPeriodPage *page );
static void       action_page_v_setup_actions( ofaActionPage *page, ofaButtonsBox *buttons_box );
static void       action_page_v_init_view( ofaActionPage *page );
static void       on_row_selected( ofaRecPeriodTreeview *view, ofoRecPeriod *period, ofaRecPeriodPage *self );
static void       on_row_activated( ofaRecPeriodTreeview *view, ofoRecPeriod *period, ofaRecPeriodPage *self );
static void       on_insert_key( ofaRecPeriodTreeview *view, ofaRecPeriodPage *self );
static void       on_delete_key( ofaRecPeriodTreeview *view, ofoRecPeriod *period, ofaRecPeriodPage *self );
static void       action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaRecPeriodPage *self );
static void       action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaRecPeriodPage *self );
static void       action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaRecPeriodPage *self );
static gboolean   check_for_deletability( ofaRecPeriodPage *self, ofoRecPeriod *model );
static void       delete_with_confirm( ofaRecPeriodPage *self, ofoRecPeriod *model );

G_DEFINE_TYPE_EXTENDED( ofaRecPeriodPage, ofa_rec_period_page, OFA_TYPE_ACTION_PAGE, 0,
		G_ADD_PRIVATE( ofaRecPeriodPage ))

static void
rec_period_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_rec_period_page_finalize";
	ofaRecPeriodPagePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_REC_PERIOD_PAGE( instance ));

	/* free data members here */
	priv = ofa_rec_period_page_get_instance_private( OFA_REC_PERIOD_PAGE( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rec_period_page_parent_class )->finalize( instance );
}

static void
rec_period_page_dispose( GObject *instance )
{
	ofaRecPeriodPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_REC_PERIOD_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = ofa_rec_period_page_get_instance_private( OFA_REC_PERIOD_PAGE( instance ));

		g_object_unref( priv->new_action );
		g_object_unref( priv->update_action );
		g_object_unref( priv->delete_action );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rec_period_page_parent_class )->dispose( instance );
}

static void
ofa_rec_period_page_init( ofaRecPeriodPage *self )
{
	static const gchar *thisfn = "ofa_rec_period_page_init";
	ofaRecPeriodPagePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_REC_PERIOD_PAGE( self ));

	priv = ofa_rec_period_page_get_instance_private( self );

	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_rec_period_page_class_init( ofaRecPeriodPageClass *klass )
{
	static const gchar *thisfn = "ofa_rec_period_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = rec_period_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = rec_period_page_finalize;

	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = page_v_get_top_focusable_widget;

	OFA_ACTION_PAGE_CLASS( klass )->setup_view = action_page_v_setup_view;
	OFA_ACTION_PAGE_CLASS( klass )->setup_actions = action_page_v_setup_actions;
	OFA_ACTION_PAGE_CLASS( klass )->init_view = action_page_v_init_view;
}

static GtkWidget *
page_v_get_top_focusable_widget( const ofaPage *page )
{
	ofaRecPeriodPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_REC_PERIOD_PAGE( page ), NULL );

	priv = ofa_rec_period_page_get_instance_private( OFA_REC_PERIOD_PAGE( page ));

	return( ofa_tvbin_get_tree_view( OFA_TVBIN( priv->tview )));
}

static GtkWidget *
action_page_v_setup_view( ofaActionPage *page )
{
	static const gchar *thisfn = "ofa_rec_period_page_v_setup_view";
	ofaRecPeriodPagePrivate *priv;
	GtkWidget *widget;
	ofaHub *hub;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_rec_period_page_get_instance_private( OFA_REC_PERIOD_PAGE( page ));

	priv->getter = ofa_page_get_getter( OFA_PAGE( page ));

	hub = ofa_igetter_get_hub( priv->getter );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	priv->is_writable = ofa_hub_is_writable_dossier( hub );

	widget = setup_treeview( OFA_REC_PERIOD_PAGE( page ));

	return( widget );
}

static GtkWidget *
setup_treeview( ofaRecPeriodPage *self )
{
	ofaRecPeriodPagePrivate *priv;

	priv = ofa_rec_period_page_get_instance_private( self );

	priv->tview = ofa_rec_period_treeview_new( priv->getter, priv->settings_prefix );
	ofa_rec_period_treeview_setup_columns( priv->tview );

	/* ofaTVBin signals */
	g_signal_connect( priv->tview, "ofa-insert", G_CALLBACK( on_insert_key ), self );

	/* ofaRecPeriodTreeview signals */
	g_signal_connect( priv->tview, "ofa-perchanged", G_CALLBACK( on_row_selected ), self );
	g_signal_connect( priv->tview, "ofa-peractivated", G_CALLBACK( on_row_activated ), self );
	g_signal_connect( priv->tview, "ofa-perdelete", G_CALLBACK( on_delete_key ), self );

	return( GTK_WIDGET( priv->tview ));
}

static void
action_page_v_setup_actions( ofaActionPage *page, ofaButtonsBox *buttons_box )
{
	ofaRecPeriodPagePrivate *priv;

	priv = ofa_rec_period_page_get_instance_private( OFA_REC_PERIOD_PAGE( page ));

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
	static const gchar *thisfn = "ofa_rec_period_page_v_init_view";
	ofaRecPeriodPagePrivate *priv;
	GMenu *menu;
	ofaRecPeriodStore *store;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_rec_period_page_get_instance_private( OFA_REC_PERIOD_PAGE( page ));

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
	store = ofa_rec_period_store_new( priv->getter );
	ofa_tvbin_set_store( OFA_TVBIN( priv->tview ), GTK_TREE_MODEL( store ));
	g_object_unref( store );
}

/*
 * RecPeriodTreeview callback
 */
static void
on_row_selected( ofaRecPeriodTreeview *view, ofoRecPeriod *period, ofaRecPeriodPage *self )
{
	ofaRecPeriodPagePrivate *priv;
	gboolean is_period;

	priv = ofa_rec_period_page_get_instance_private( self );

	is_period = period && OFO_IS_REC_PERIOD( period );

	g_simple_action_set_enabled( priv->update_action, is_period );
	g_simple_action_set_enabled( priv->delete_action, check_for_deletability( self, period ));
}

/*
 * RecPeriodTreeview callback
 * Activation of a single row open the update dialog, else is ignored
 */
static void
on_row_activated( ofaRecPeriodTreeview *view, ofoRecPeriod *period, ofaRecPeriodPage *self )
{
	ofaRecPeriodPagePrivate *priv;

	priv = ofa_rec_period_page_get_instance_private( self );

	g_action_activate( G_ACTION( priv->update_action ), NULL );
}

static void
on_insert_key( ofaRecPeriodTreeview *view, ofaRecPeriodPage *self )
{
	ofaRecPeriodPagePrivate *priv;

	priv = ofa_rec_period_page_get_instance_private( self );

	if( priv->is_writable ){
		g_action_activate( G_ACTION( priv->new_action ), NULL );
	}
}

static void
on_delete_key( ofaRecPeriodTreeview *view, ofoRecPeriod *period, ofaRecPeriodPage *self )
{
	ofaRecPeriodPagePrivate *priv;

	priv = ofa_rec_period_page_get_instance_private( self );

	if( check_for_deletability( self, period )){
		g_action_activate( G_ACTION( priv->delete_action ), NULL );
	}
}

/*
 * create a new recurrent model
 * creating a new recurrent record is the rule of 'Declare' button
 */
static void
action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaRecPeriodPage *self )
{
	ofaRecPeriodPagePrivate *priv;
	ofoRecPeriod *model;
	GtkWindow *toplevel;

	priv = ofa_rec_period_page_get_instance_private( self );

	model = ofo_rec_period_new( priv->getter );
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_rec_period_properties_run( priv->getter, toplevel, model );
}

/*
 * Update action is expected to be used when selection is single
 */
static void
action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaRecPeriodPage *self )
{
	ofaRecPeriodPagePrivate *priv;
	ofoRecPeriod *model;
	GtkWindow *toplevel;

	priv = ofa_rec_period_page_get_instance_private( self );

	model = ofa_rec_period_treeview_get_selected( priv->tview );
	g_return_if_fail( model && OFO_IS_REC_PERIOD( model ));
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_rec_period_properties_run( priv->getter, toplevel, model );
}

/*
 * Delete button is expected to be sensitive when the selection count is 1
 * (and dossier is writable, and record is deletable)
 */
static void
action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaRecPeriodPage *self )
{
	ofaRecPeriodPagePrivate *priv;
	ofoRecPeriod *period;

	priv = ofa_rec_period_page_get_instance_private( self );

	period = ofa_rec_period_treeview_get_selected( priv->tview );
	g_return_if_fail( period && OFO_IS_REC_PERIOD( period ));
	g_return_if_fail( check_for_deletability( self, period ));
	delete_with_confirm( self, period );

	gtk_widget_grab_focus( page_v_get_top_focusable_widget( OFA_PAGE( self )));
}

static gboolean
check_for_deletability( ofaRecPeriodPage *self, ofoRecPeriod *period )
{
	ofaRecPeriodPagePrivate *priv;
	gboolean is_period;

	priv = ofa_rec_period_page_get_instance_private( self );

	is_period = period && OFO_IS_REC_PERIOD( period );

	return( priv->is_writable && is_period && ofo_rec_period_is_deletable( period ));
}

static void
delete_with_confirm( ofaRecPeriodPage *self, ofoRecPeriod *period )
{
	gchar *msg;
	gboolean delete_ok;
	GtkWindow *toplevel;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s' periodicity ?" ),
				ofo_rec_period_get_label( period ));

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	delete_ok = my_utils_dialog_question( toplevel, msg, _( "_Delete" ));

	g_free( msg );

	if( delete_ok ){
		ofo_rec_period_delete( period );
	}
}
