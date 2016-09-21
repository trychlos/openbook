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
#include <stdlib.h>
#include <string.h>

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-buttons-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itheme-manager.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "ofa-recurrent-main.h"
#include "ofa-recurrent-manage-page.h"
#include "ofa-recurrent-model-properties.h"
#include "ofa-recurrent-model-treeview.h"
#include "ofa-recurrent-new.h"
#include "ofa-recurrent-run-page.h"
#include "ofo-recurrent-model.h"

/* priv instance data
 */
typedef struct {

	/* internals
	 */
	gboolean           is_writable;
	gchar             *settings_prefix;

	/* UI
	 */
	ofaRecurrentModelTreeview *tview;

	/* actions
	 */
	GSimpleAction     *new_action;
	GSimpleAction     *update_action;
	GSimpleAction     *delete_action;
	GSimpleAction     *generate_action;
	GSimpleAction     *view_waiting_action;
}
	ofaRecurrentManagePagePrivate;

static GtkWidget *v_setup_view( ofaPage *page );
static GtkWidget *setup_treeview( ofaRecurrentManagePage *page );
static void       v_init_view( ofaPage *page );
static GtkWidget *v_setup_buttons( ofaPage *page );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       on_row_selected( ofaRecurrentModelTreeview *view, GList *list, ofaRecurrentManagePage *self );
static void       on_row_activated( ofaRecurrentModelTreeview *view, GList *list, ofaRecurrentManagePage *self );
static void       on_insert_key( ofaRecurrentModelTreeview *view, ofaRecurrentManagePage *self );
static void       on_delete_key( ofaRecurrentModelTreeview *view, ofoRecurrentModel *ledger, ofaRecurrentManagePage *self );
static void       action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentManagePage *self );
static void       action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentManagePage *self );
static void       action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentManagePage *self );
static gboolean   check_for_deletability( ofaRecurrentManagePage *self, ofoRecurrentModel *model );
static void       delete_with_confirm( ofaRecurrentManagePage *self, ofoRecurrentModel *model );
static void       action_on_generate_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentManagePage *self );
static void       action_on_view_waiting_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentManagePage *self );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentManagePage, ofa_recurrent_manage_page, OFA_TYPE_PAGE, 0,
		G_ADD_PRIVATE( ofaRecurrentManagePage ))

static void
recurrent_manage_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recurrent_manage_page_finalize";
	ofaRecurrentManagePagePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECURRENT_MANAGE_PAGE( instance ));

	/* free data members here */
	priv = ofa_recurrent_manage_page_get_instance_private( OFA_RECURRENT_MANAGE_PAGE( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_manage_page_parent_class )->finalize( instance );
}

static void
recurrent_manage_page_dispose( GObject *instance )
{
	ofaRecurrentManagePagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_RECURRENT_MANAGE_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = ofa_recurrent_manage_page_get_instance_private( OFA_RECURRENT_MANAGE_PAGE( instance ));

		g_object_unref( priv->new_action );
		g_object_unref( priv->update_action );
		g_object_unref( priv->delete_action );
		g_object_unref( priv->generate_action );
		g_object_unref( priv->view_waiting_action );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_manage_page_parent_class )->dispose( instance );
}

static void
ofa_recurrent_manage_page_init( ofaRecurrentManagePage *self )
{
	static const gchar *thisfn = "ofa_recurrent_manage_page_init";
	ofaRecurrentManagePagePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECURRENT_MANAGE_PAGE( self ));

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_recurrent_manage_page_class_init( ofaRecurrentManagePageClass *klass )
{
	static const gchar *thisfn = "ofa_recurrent_manage_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_manage_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_manage_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_recurrent_manage_page_v_setup_view";
	ofaRecurrentManagePagePrivate *priv;
	ofaHub *hub;
	GtkWidget *widget;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_recurrent_manage_page_get_instance_private( OFA_RECURRENT_MANAGE_PAGE( page ));

	hub = ofa_igetter_get_hub( OFA_IGETTER( page ));
	priv->is_writable = ofa_hub_dossier_is_writable( hub );

	widget = setup_treeview( OFA_RECURRENT_MANAGE_PAGE( page ));

	return( widget );
}

static GtkWidget *
setup_treeview( ofaRecurrentManagePage *self )
{
	ofaRecurrentManagePagePrivate *priv;
	ofaHub *hub;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	hub = ofa_igetter_get_hub( OFA_IGETTER( self ));

	priv->tview = ofa_recurrent_model_treeview_new();
	ofa_recurrent_model_treeview_set_settings_key( priv->tview, priv->settings_prefix );
	ofa_recurrent_model_treeview_setup_columns( priv->tview );
	ofa_recurrent_model_treeview_set_hub( priv->tview, hub );
	my_utils_widget_set_margins( GTK_WIDGET( priv->tview ), 2, 2, 2, 0 );

	/* ofaTVBin signals */
	g_signal_connect( priv->tview, "ofa-insert", G_CALLBACK( on_insert_key ), self );

	/* ofaBatTreeview signals */
	g_signal_connect( priv->tview, "ofa-recchanged", G_CALLBACK( on_row_selected ), self );
	g_signal_connect( priv->tview, "ofa-recactivated", G_CALLBACK( on_row_activated ), self );
	g_signal_connect( priv->tview, "ofa-recdelete", G_CALLBACK( on_delete_key ), self );

	return( GTK_WIDGET( priv->tview ));
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	ofaRecurrentManagePagePrivate *priv;
	ofaButtonsBox *buttons_box;

	priv = ofa_recurrent_manage_page_get_instance_private( OFA_RECURRENT_MANAGE_PAGE( page ));

	buttons_box = ofa_buttons_box_new();
	my_utils_widget_set_margins( GTK_WIDGET( buttons_box ), 2, 2, 0, 0 );

	/* new action */
	priv->new_action = g_simple_action_new( "new", NULL );
	g_simple_action_set_enabled( priv->new_action, priv->is_writable );
	g_signal_connect( priv->new_action, "activate", G_CALLBACK( action_on_new_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->new_action ),
			OFA_IACTIONABLE_NEW_ITEM );
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_set_button(
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
			ofa_iactionable_set_button(
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
			ofa_iactionable_set_button(
					OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->delete_action ),
					OFA_IACTIONABLE_DELETE_BTN ));

	ofa_buttons_box_add_spacer( buttons_box );

	/* generate operations from selected models */
	priv->generate_action = g_simple_action_new( "generate", NULL );
	g_signal_connect( priv->generate_action, "activate", G_CALLBACK( action_on_generate_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->generate_action ),
			_( "Generate from selected..." ));
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_set_button(
					OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->generate_action ),
					_( "_Generate from selected..." )));

	/* view entries */
	priv->view_waiting_action = g_simple_action_new( "viewwaiting", NULL );
	g_signal_connect( priv->view_waiting_action, "activate", G_CALLBACK( action_on_view_waiting_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->view_waiting_action ),
			_( "View waiting operations" ));
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_set_button(
					OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->view_waiting_action ),
					_( "_View waiting operations" )));

	return( GTK_WIDGET( buttons_box ));
}

static void
v_init_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_recurrent_manage_page_v_init_view";
	ofaRecurrentManagePagePrivate *priv;
	GMenu *menu;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_recurrent_manage_page_get_instance_private( OFA_RECURRENT_MANAGE_PAGE( page ));

	menu = ofa_iactionable_get_menu( OFA_IACTIONABLE( page ), priv->settings_prefix );
	ofa_icontext_set_menu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( page ),
			menu );

	menu = ofa_itvcolumnable_get_menu( OFA_ITVCOLUMNABLE( priv->tview ));
	ofa_icontext_append_submenu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( priv->tview ),
			OFA_IACTIONABLE_VISIBLE_COLUMNS_ITEM, menu );
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaRecurrentManagePagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_RECURRENT_MANAGE_PAGE( page ), NULL );

	priv = ofa_recurrent_manage_page_get_instance_private( OFA_RECURRENT_MANAGE_PAGE( page ));

	return( ofa_tvbin_get_treeview( OFA_TVBIN( priv->tview )));
}

/*
 * RecurrentModelTreeview callback
 */
static void
on_row_selected( ofaRecurrentModelTreeview *view, GList *list, ofaRecurrentManagePage *self )
{
#if 0
	ofaRecurrentManagePagePrivate *priv;
	gboolean is_model;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	is_model = ledger && OFO_IS_LEDGER( ledger );

	g_simple_action_set_enabled( priv->update_action, is_ledger );
	g_simple_action_set_enabled( priv->delete_action, check_for_deletability( self, ledger ));
	g_simple_action_set_enabled( priv->view_entries_action, is_ledger && ofo_ledger_has_entries( ledger ));
#endif
}

/*
 * RecurrentModelTreeview callback
 * Activation of a single row open the update dialog, else is ignored
 */
static void
on_row_activated( ofaRecurrentModelTreeview *view, GList *list, ofaRecurrentManagePage *self )
{
	if( g_list_length( list ) == 1 ){
		//g_action_activate( G_ACTION( priv->update_action ), NULL );
	}
}

static void
on_insert_key( ofaRecurrentModelTreeview *view, ofaRecurrentManagePage *self )
{
	ofaRecurrentManagePagePrivate *priv;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	if( priv->is_writable ){
		//g_action_activate( G_ACTION( priv->new_action ), NULL );
	}
}

/*
 * only delete if there is only one selected ledger
 */
static void
on_delete_key( ofaRecurrentModelTreeview *view, ofoRecurrentModel *ledger, ofaRecurrentManagePage *self )
{
	/*
	ofaRecurrentManagePagePrivate *priv;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	if( check_for_deletability( self, ledger )){
		g_action_activate( G_ACTION( priv->delete_action ), NULL );
	}
	*/
}

#if 0
/*
 * selection is multiple mode (0 to n selected rows)
 */
static void
tview_on_selection_changed( GtkTreeSelection *selection, ofaRecurrentManagePage *self )
{
	ofaRecurrentManagePagePrivate *priv;
	GList *selected;
	guint count;
	ofoRecurrentModel *model;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	selected = tview_get_selected( self );
	count = g_list_length( selected );

	model = ( count == 1 ) ? ( ofoRecurrentModel * ) selected->data : NULL;
	g_return_if_fail( count != 1 || ( model && OFO_IS_RECURRENT_MODEL( model )));

	if( priv->update_btn ){
		gtk_widget_set_sensitive( priv->update_btn,
				count == 1 );
	}
	if( priv->delete_btn ){
		gtk_widget_set_sensitive( priv->delete_btn,
				priv->is_writable && count == 1 && ofo_recurrent_model_is_deletable( model ));
	}
	if( priv->generate_btn ){
		gtk_widget_set_sensitive( priv->generate_btn,
				priv->is_writable && count > 0 );
	}

	g_list_free( selected );
}

/*
 * Returns a GList of selected ofoRecurrentModel objects
 *
 * The returned list should be #g_list_free() by the caller.
 */
static GList *
tview_get_selected( ofaRecurrentManagePage *self )
{
	ofaRecurrentManagePagePrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GList *objects, *paths, *it;
	ofoRecurrentModel *object;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	objects = NULL;

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->tview ));
	paths = gtk_tree_selection_get_selected_rows( select, &tmodel );

	if( paths ){
		for( it=paths ; it ; it=it->next ){
			if( gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) it->data )){
				gtk_tree_model_get( tmodel, &iter, REC_MODEL_COL_OBJECT, &object, -1 );
				g_return_val_if_fail( object && OFO_IS_RECURRENT_MODEL( object ), NULL );
				g_object_unref( object );
				objects = g_list_prepend( objects, object );
			}
		}
		g_list_free_full( paths, ( GDestroyNotify ) gtk_tree_path_free );
	}

	return( objects );
}

/**
 * ofa_recurrent_manage_page_get_selected:
 * @page: this #ofaRecurrentManagePage page.
 *
 * Returns: a GList of selected ofoRecurrentModel objects.
 *
 * The returned list should be #g_list_free() by the caller.
 */
GList *
ofa_recurrent_manage_page_get_selected( ofaRecurrentManagePage *page )
{
	g_return_val_if_fail( page && OFA_IS_RECURRENT_MANAGE_PAGE( page ), NULL );
	g_return_val_if_fail( !OFA_PAGE( page )->prot->dispose_has_run, NULL );

	return( tview_get_selected( page ));
}

static void
store_on_row_inserted_or_removed( ofaRecurrentModelStore *store, ofaRecurrentManagePage *self )
{
	ofaRecurrentManagePagePrivate *priv;
	GList *dataset;
	guint count;
	ofaHub *hub;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	hub = ofa_igetter_get_hub( OFA_IGETTER( self ));
	dataset = ofo_recurrent_model_get_dataset( hub );
	count = g_list_length( dataset );

	gtk_widget_set_sensitive( priv->generate_btn, count > 0 );
}
#endif

/*
 * create a new recurrent model
 * creating a new recurrent record is the rule of 'Declare' button
 */
static void
action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentManagePage *self )
{
	ofoRecurrentModel *model;
	GtkWindow *toplevel;

	model = ofo_recurrent_model_new();
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_recurrent_model_properties_run( OFA_IGETTER( self ), toplevel, model );
}

/*
 * Update action is expected to be used when selection is single
 */
static void
action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentManagePage *self )
{
	ofaRecurrentManagePagePrivate *priv;
	GList *selected;
	ofoRecurrentModel *model;
	GtkWindow *toplevel;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	selected = ofa_recurrent_model_treeview_get_selected( priv->tview );
	if( g_list_length( selected ) == 1 ){
		model = ( ofoRecurrentModel * ) selected->data;
		g_return_if_fail( model && OFO_IS_RECURRENT_MODEL( model ));
		toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
		ofa_recurrent_model_properties_run( OFA_IGETTER( self ), toplevel, model );
	}

	g_list_free( selected );
}

/*
 * Delete button is expected to be sensitive when the selection count is 1
 * (and dossier is writable, and record is deletable)
 */
static void
action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentManagePage *self )
{
	ofaRecurrentManagePagePrivate *priv;
	GList *selected;
	ofoRecurrentModel *model;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	selected = ofa_recurrent_model_treeview_get_selected( priv->tview );
	if( g_list_length( selected ) == 1 ){
		model = ( ofoRecurrentModel * ) selected->data;
		g_return_if_fail( model && OFO_IS_RECURRENT_MODEL( model ));
		g_return_if_fail( check_for_deletability( self, model ));
		delete_with_confirm( self, model );
	}
	ofa_recurrent_model_treeview_free_selected( selected );

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( self )));
}

static gboolean
check_for_deletability( ofaRecurrentManagePage *self, ofoRecurrentModel *model )
{
	ofaRecurrentManagePagePrivate *priv;
	gboolean is_model;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	is_model = model && OFO_IS_RECURRENT_MODEL( model );

	return( priv->is_writable && is_model && ofo_recurrent_model_is_deletable( model ));
}

static void
delete_with_confirm( ofaRecurrentManagePage *self, ofoRecurrentModel *model )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s' recurrent model ?" ),
				ofo_recurrent_model_get_mnemo( model ));

	delete_ok = my_utils_dialog_question( msg, _( "_Delete" ));

	g_free( msg );

	if( delete_ok ){
		ofo_recurrent_model_delete( model );
	}
}

/*
 * generating new operations from current selection
 */
static void
action_on_generate_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentManagePage *self )
{
	GtkWindow *toplevel;

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));

	ofa_recurrent_new_run( OFA_IGETTER( self ), toplevel, self );
}

/*
 * opening the Run page
 */
static void
action_on_view_waiting_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentManagePage *self )
{
	ofaIThemeManager *manager;

	manager = ofa_igetter_get_theme_manager( OFA_IGETTER( self ));
	ofa_itheme_manager_activate( manager, OFA_TYPE_RECURRENT_RUN_PAGE );
}
