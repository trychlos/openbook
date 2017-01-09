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

#include "my/my-date.h"
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

#include "ofa-recurrent-generate.h"
#include "ofa-recurrent-main.h"
#include "ofa-recurrent-model-page.h"
#include "ofa-recurrent-model-properties.h"
#include "ofa-recurrent-model-treeview.h"
#include "ofa-recurrent-run-page.h"
#include "ofo-recurrent-model.h"

/* priv instance data
 */
typedef struct {

	/* internals
	 */
	ofaHub            *hub;
	gboolean           is_writable;
	gchar             *settings_prefix;

	/* UI
	 */
	ofaRecurrentModelTreeview *tview;

	/* actions
	 */
	GSimpleAction     *new_action;
	GSimpleAction     *update_action;
	GSimpleAction     *duplicate_action;
	GSimpleAction     *delete_action;
	GSimpleAction     *generate_action;
	GSimpleAction     *view_opes_action;
}
	ofaRecurrentModelPagePrivate;

static GtkWidget *page_v_get_top_focusable_widget( const ofaPage *page );
static GtkWidget *action_page_v_setup_view( ofaActionPage *page );
static GtkWidget *setup_treeview( ofaRecurrentModelPage *page );
static void       action_page_v_setup_actions( ofaActionPage *page, ofaButtonsBox *buttons_box );
static void       action_page_v_init_view( ofaActionPage *page );
static void       on_row_selected( ofaRecurrentModelTreeview *view, GList *list, ofaRecurrentModelPage *self );
static void       on_row_activated( ofaRecurrentModelTreeview *view, GList *list, ofaRecurrentModelPage *self );
static void       on_insert_key( ofaRecurrentModelTreeview *view, ofaRecurrentModelPage *self );
static void       on_delete_key( ofaRecurrentModelTreeview *view, GList *list, ofaRecurrentModelPage *self );
static void       action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentModelPage *self );
static void       action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentModelPage *self );
static void       action_on_duplicate_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentModelPage *self );
static void       action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentModelPage *self );
static gboolean   check_for_deletability( ofaRecurrentModelPage *self, ofoRecurrentModel *model );
static void       delete_with_confirm( ofaRecurrentModelPage *self, ofoRecurrentModel *model );
static void       action_on_generate_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentModelPage *self );
static void       action_on_view_opes_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentModelPage *self );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentModelPage, ofa_recurrent_model_page, OFA_TYPE_ACTION_PAGE, 0,
		G_ADD_PRIVATE( ofaRecurrentModelPage ))

static void
recurrent_model_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recurrent_model_page_finalize";
	ofaRecurrentModelPagePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECURRENT_MODEL_PAGE( instance ));

	/* free data members here */
	priv = ofa_recurrent_model_page_get_instance_private( OFA_RECURRENT_MODEL_PAGE( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_model_page_parent_class )->finalize( instance );
}

static void
recurrent_model_page_dispose( GObject *instance )
{
	ofaRecurrentModelPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_RECURRENT_MODEL_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = ofa_recurrent_model_page_get_instance_private( OFA_RECURRENT_MODEL_PAGE( instance ));

		g_object_unref( priv->new_action );
		g_object_unref( priv->update_action );
		g_object_unref( priv->duplicate_action );
		g_object_unref( priv->delete_action );
		g_object_unref( priv->generate_action );
		g_object_unref( priv->view_opes_action );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_model_page_parent_class )->dispose( instance );
}

static void
ofa_recurrent_model_page_init( ofaRecurrentModelPage *self )
{
	static const gchar *thisfn = "ofa_recurrent_model_page_init";
	ofaRecurrentModelPagePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECURRENT_MODEL_PAGE( self ));

	priv = ofa_recurrent_model_page_get_instance_private( self );

	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_recurrent_model_page_class_init( ofaRecurrentModelPageClass *klass )
{
	static const gchar *thisfn = "ofa_recurrent_model_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_model_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_model_page_finalize;

	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = page_v_get_top_focusable_widget;

	OFA_ACTION_PAGE_CLASS( klass )->setup_view = action_page_v_setup_view;
	OFA_ACTION_PAGE_CLASS( klass )->setup_actions = action_page_v_setup_actions;
	OFA_ACTION_PAGE_CLASS( klass )->init_view = action_page_v_init_view;
}

static GtkWidget *
page_v_get_top_focusable_widget( const ofaPage *page )
{
	ofaRecurrentModelPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_RECURRENT_MODEL_PAGE( page ), NULL );

	priv = ofa_recurrent_model_page_get_instance_private( OFA_RECURRENT_MODEL_PAGE( page ));

	return( ofa_tvbin_get_tree_view( OFA_TVBIN( priv->tview )));
}

static GtkWidget *
action_page_v_setup_view( ofaActionPage *page )
{
	static const gchar *thisfn = "ofa_recurrent_model_page_v_setup_view";
	ofaRecurrentModelPagePrivate *priv;
	GtkWidget *widget;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_recurrent_model_page_get_instance_private( OFA_RECURRENT_MODEL_PAGE( page ));

	priv->hub = ofa_igetter_get_hub( OFA_IGETTER( page ));
	g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );
	priv->is_writable = ofa_hub_dossier_is_writable( priv->hub );

	widget = setup_treeview( OFA_RECURRENT_MODEL_PAGE( page ));

	return( widget );
}

static GtkWidget *
setup_treeview( ofaRecurrentModelPage *self )
{
	ofaRecurrentModelPagePrivate *priv;

	priv = ofa_recurrent_model_page_get_instance_private( self );

	priv->tview = ofa_recurrent_model_treeview_new( priv->hub );
	ofa_recurrent_model_treeview_set_settings_key( priv->tview, priv->settings_prefix );
	ofa_recurrent_model_treeview_setup_columns( priv->tview );

	/* ofaTVBin signals */
	g_signal_connect( priv->tview, "ofa-insert", G_CALLBACK( on_insert_key ), self );

	/* ofaRecurrentModelTreeview signals */
	g_signal_connect( priv->tview, "ofa-recchanged", G_CALLBACK( on_row_selected ), self );
	g_signal_connect( priv->tview, "ofa-recactivated", G_CALLBACK( on_row_activated ), self );
	g_signal_connect( priv->tview, "ofa-recdelete", G_CALLBACK( on_delete_key ), self );

	return( GTK_WIDGET( priv->tview ));
}

static void
action_page_v_setup_actions( ofaActionPage *page, ofaButtonsBox *buttons_box )
{
	ofaRecurrentModelPagePrivate *priv;

	priv = ofa_recurrent_model_page_get_instance_private( OFA_RECURRENT_MODEL_PAGE( page ));

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

	/* duplicate action */
	priv->duplicate_action = g_simple_action_new( "duplicate", NULL );
	g_signal_connect( priv->duplicate_action, "activate", G_CALLBACK( action_on_duplicate_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->duplicate_action ),
			_( "Duplicate this" ));
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->duplicate_action ),
					_( "Duplicate" )));

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

	/* generate operations from selected models */
	priv->generate_action = g_simple_action_new( "generate", NULL );
	g_signal_connect( priv->generate_action, "activate", G_CALLBACK( action_on_generate_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->generate_action ),
			_( "Generate from selected..." ));
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->generate_action ),
					_( "_Generate from selected..." )));

	/* view operations - always enabled */
	priv->view_opes_action = g_simple_action_new( "viewopes", NULL );
	g_signal_connect( priv->view_opes_action, "activate", G_CALLBACK( action_on_view_opes_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->view_opes_action ),
			_( "View operations..." ));
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->view_opes_action ),
					_( "_View operations..." )));
	g_simple_action_set_enabled( priv->view_opes_action, TRUE );
}

static void
action_page_v_init_view( ofaActionPage *page )
{
	static const gchar *thisfn = "ofa_recurrent_model_page_v_init_view";
	ofaRecurrentModelPagePrivate *priv;
	GMenu *menu;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_recurrent_model_page_get_instance_private( OFA_RECURRENT_MODEL_PAGE( page ));

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
	ofa_recurrent_model_treeview_setup_store( priv->tview );

	/* as GTK_SELECTION_MULTIPLE is set, we have to explicitely
	 * setup the initial selection if a first row exists */
	ofa_tvbin_select_all( OFA_TVBIN( priv->tview ));
}

/*
 * RecurrentModelTreeview callback
 */
static void
on_row_selected( ofaRecurrentModelTreeview *view, GList *list, ofaRecurrentModelPage *self )
{
	ofaRecurrentModelPagePrivate *priv;
	gboolean is_model;
	ofoRecurrentModel *model;
	gint count_enabled;
	GList *it;

	priv = ofa_recurrent_model_page_get_instance_private( self );

	is_model = FALSE;
	model = NULL;

	if( g_list_length( list ) == 1 ){
		model = ( ofoRecurrentModel * ) list->data;
		is_model = model && OFO_IS_RECURRENT_MODEL( model );
	}

	count_enabled = 0;
	for( it=list ; it ; it=it->next ){
		model = ( ofoRecurrentModel * ) it->data;
		if( ofo_recurrent_model_get_is_enabled( model )){
			count_enabled += 1;
		}
	}

	g_simple_action_set_enabled( priv->update_action, is_model );
	g_simple_action_set_enabled( priv->duplicate_action, priv->is_writable && is_model );
	g_simple_action_set_enabled( priv->delete_action, check_for_deletability( self, model ));
	g_simple_action_set_enabled( priv->generate_action, priv->is_writable && count_enabled > 0 );
}

/*
 * RecurrentModelTreeview callback
 * Activation of a single row open the update dialog, else is ignored
 */
static void
on_row_activated( ofaRecurrentModelTreeview *view, GList *list, ofaRecurrentModelPage *self )
{
	ofaRecurrentModelPagePrivate *priv;

	priv = ofa_recurrent_model_page_get_instance_private( self );

	if( g_list_length( list ) == 1 ){
		g_action_activate( G_ACTION( priv->update_action ), NULL );
	}
}

static void
on_insert_key( ofaRecurrentModelTreeview *view, ofaRecurrentModelPage *self )
{
	ofaRecurrentModelPagePrivate *priv;

	priv = ofa_recurrent_model_page_get_instance_private( self );

	if( priv->is_writable ){
		g_action_activate( G_ACTION( priv->new_action ), NULL );
	}
}

static void
on_delete_key( ofaRecurrentModelTreeview *view, GList *list, ofaRecurrentModelPage *self )
{
	ofaRecurrentModelPagePrivate *priv;
	ofoRecurrentModel *model;

	priv = ofa_recurrent_model_page_get_instance_private( self );

	if( g_list_length( list ) == 1 ){
		model = OFO_RECURRENT_MODEL( list->data );
		if( check_for_deletability( self, model )){
			g_action_activate( G_ACTION( priv->delete_action ), NULL );
		}
	}
}

/**
 * ofa_recurrent_model_page_get_selected:
 * @page: this #ofaRecurrentModelPage instance.
 *
 * Returns: the current selection as a #GList of #ofoRecurrentModel
 * objects.
 *
 * The returned list should be #ofa_recurrent_model_treeview_free_selected()
 * by the caller.
 */
GList *
ofa_recurrent_model_page_get_selected( ofaRecurrentModelPage *page )
{
	ofaRecurrentModelPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_RECURRENT_MODEL_PAGE( page ), NULL );
	g_return_val_if_fail( !OFA_PAGE( page )->prot->dispose_has_run, NULL );

	priv = ofa_recurrent_model_page_get_instance_private( page );

	return( ofa_recurrent_model_treeview_get_selected( priv->tview ));
}

/**
 * ofa_recurrent_model_page_unselect:
 * @page: this #ofaRecurrentModelPage instance.
 * @model: [allow-none]: a #ofoRecurrentModel object.
 *
 * Unselect the @model from the treeview.
 */
void
ofa_recurrent_model_page_unselect( ofaRecurrentModelPage *page, ofoRecurrentModel *model )
{
	ofaRecurrentModelPagePrivate *priv;

	g_return_if_fail( page && OFA_IS_RECURRENT_MODEL_PAGE( page ));
	g_return_if_fail( !OFA_PAGE( page )->prot->dispose_has_run );
	g_return_if_fail( !model || OFO_IS_RECURRENT_MODEL( model ));

	priv = ofa_recurrent_model_page_get_instance_private( page );

	ofa_recurrent_model_treeview_unselect( priv->tview, model );
}

/*
 * create a new recurrent model
 * creating a new recurrent record is the rule of 'Declare' button
 */
static void
action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentModelPage *self )
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
action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentModelPage *self )
{
	ofaRecurrentModelPagePrivate *priv;
	GList *selected;
	ofoRecurrentModel *model;
	GtkWindow *toplevel;

	priv = ofa_recurrent_model_page_get_instance_private( self );

	selected = ofa_recurrent_model_treeview_get_selected( priv->tview );

	if( g_list_length( selected ) == 1 ){
		model = ( ofoRecurrentModel * ) selected->data;
		g_return_if_fail( model && OFO_IS_RECURRENT_MODEL( model ));
		toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
		ofa_recurrent_model_properties_run( OFA_IGETTER( self ), toplevel, model );
	}

	ofa_recurrent_model_treeview_free_selected( selected );
}

static void
action_on_duplicate_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentModelPage *self )
{
	ofaRecurrentModelPagePrivate *priv;
	GList *selected;
	ofoRecurrentModel *model;
	ofoRecurrentModel *duplicate;

	priv = ofa_recurrent_model_page_get_instance_private( self );

	selected = ofa_recurrent_model_treeview_get_selected( priv->tview );

	if( g_list_length( selected ) == 1 ){
		model = ( ofoRecurrentModel * ) selected->data;
		g_return_if_fail( model && OFO_IS_RECURRENT_MODEL( model ));
		duplicate = ofo_recurrent_model_new_from_model( model );
		if( !ofo_recurrent_model_insert( duplicate, priv->hub )){
			g_object_unref( duplicate );
		}
	}

	ofa_recurrent_model_treeview_free_selected( selected );
}

/*
 * Delete button is expected to be sensitive when the selection count is 1
 * (and dossier is writable, and record is deletable)
 */
static void
action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentModelPage *self )
{
	ofaRecurrentModelPagePrivate *priv;
	GList *selected;
	ofoRecurrentModel *model;

	priv = ofa_recurrent_model_page_get_instance_private( self );

	selected = ofa_recurrent_model_treeview_get_selected( priv->tview );

	if( g_list_length( selected ) == 1 ){
		model = ( ofoRecurrentModel * ) selected->data;
		g_return_if_fail( model && OFO_IS_RECURRENT_MODEL( model ));
		g_return_if_fail( check_for_deletability( self, model ));
		delete_with_confirm( self, model );
	}

	ofa_recurrent_model_treeview_free_selected( selected );
	gtk_widget_grab_focus( page_v_get_top_focusable_widget( OFA_PAGE( self )));
}

static gboolean
check_for_deletability( ofaRecurrentModelPage *self, ofoRecurrentModel *model )
{
	ofaRecurrentModelPagePrivate *priv;
	gboolean is_model;

	priv = ofa_recurrent_model_page_get_instance_private( self );

	is_model = model && OFO_IS_RECURRENT_MODEL( model );

	return( priv->is_writable && is_model && ofo_recurrent_model_is_deletable( model ));
}

static void
delete_with_confirm( ofaRecurrentModelPage *self, ofoRecurrentModel *model )
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
action_on_generate_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentModelPage *self )
{
	GtkWindow *toplevel;

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_recurrent_generate_run( OFA_IGETTER( self ), toplevel, self );
}

/*
 * opening the Run page
 */
static void
action_on_view_opes_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentModelPage *self )
{
	ofaIPageManager *manager;

	manager = ofa_igetter_get_page_manager( OFA_IGETTER( self ));
	ofa_ipage_manager_activate( manager, OFA_TYPE_RECURRENT_RUN_PAGE );
}
