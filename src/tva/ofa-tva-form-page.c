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

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofo-dossier.h"

#include "tva/ofa-tva-form-page.h"
#include "tva/ofa-tva-form-properties.h"
#include "tva/ofa-tva-form-treeview.h"
#include "tva/ofa-tva-main.h"
#include "tva/ofa-tva-record-new.h"
#include "tva/ofa-tva-record-page.h"
#include "tva/ofo-tva-form.h"
#include "tva/ofo-tva-record.h"

/* priv instance data
 */
typedef struct {

	/* internals
	 */
	ofaHub             *hub;
	gboolean            is_writable;
	gchar              *settings_prefix;

	/* UI
	 */
	ofaTVAFormTreeview *tview;

	/* actions
	 */
	GSimpleAction      *new_action;
	GSimpleAction      *update_action;
	GSimpleAction      *delete_action;
	GSimpleAction      *declare_action;
}
	ofaTVAFormPagePrivate;

static GtkWidget *page_v_get_top_focusable_widget( const ofaPage *page );
static GtkWidget *action_page_v_setup_view( ofaActionPage *page );
static GtkWidget *setup_treeview( ofaTVAFormPage *self );
static void       action_page_v_setup_actions( ofaActionPage *page, ofaButtonsBox *buttons_box );
static void       action_page_v_init_view( ofaActionPage *page );
static void       on_row_selected( ofaTVAFormTreeview *view, ofoTVAForm *form, ofaTVAFormPage *self );
static void       on_row_activated( ofaTVAFormTreeview *view, ofoTVAForm *form, ofaTVAFormPage *self );
static void       on_insert_key( ofaTVAFormTreeview *view, ofaTVAFormPage *self );
static void       on_delete_key( ofaTVAFormTreeview *view, ofoTVAForm *form, ofaTVAFormPage *self );
static void       action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaTVAFormPage *self );
static void       action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaTVAFormPage *self );
static void       action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaTVAFormPage *self );
static gboolean   check_for_deletability( ofaTVAFormPage *self, ofoTVAForm *form );
static void       delete_with_confirm( ofaTVAFormPage *self, ofoTVAForm *form );
static void       action_on_declare_activated( GSimpleAction *action, GVariant *empty, ofaTVAFormPage *self );

G_DEFINE_TYPE_EXTENDED( ofaTVAFormPage, ofa_tva_form_page, OFA_TYPE_ACTION_PAGE, 0,
		G_ADD_PRIVATE( ofaTVAFormPage ))

static void
tva_form_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_form_page_finalize";
	ofaTVAFormPagePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_FORM_PAGE( instance ));

	/* free data members here */
	priv = ofa_tva_form_page_get_instance_private( OFA_TVA_FORM_PAGE( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_form_page_parent_class )->finalize( instance );
}

static void
tva_form_page_dispose( GObject *instance )
{
	ofaTVAFormPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_TVA_FORM_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = ofa_tva_form_page_get_instance_private( OFA_TVA_FORM_PAGE( instance ));

		g_object_unref( priv->new_action );
		g_object_unref( priv->update_action );
		g_object_unref( priv->delete_action );
		g_object_unref( priv->declare_action );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_form_page_parent_class )->dispose( instance );
}

static void
ofa_tva_form_page_init( ofaTVAFormPage *self )
{
	static const gchar *thisfn = "ofa_tva_form_page_init";
	ofaTVAFormPagePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_TVA_FORM_PAGE( self ));

	priv = ofa_tva_form_page_get_instance_private( self );

	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_tva_form_page_class_init( ofaTVAFormPageClass *klass )
{
	static const gchar *thisfn = "ofa_tva_form_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_form_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_form_page_finalize;

	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = page_v_get_top_focusable_widget;

	OFA_ACTION_PAGE_CLASS( klass )->setup_view = action_page_v_setup_view;
	OFA_ACTION_PAGE_CLASS( klass )->setup_actions = action_page_v_setup_actions;
	OFA_ACTION_PAGE_CLASS( klass )->init_view = action_page_v_init_view;
}

static GtkWidget *
page_v_get_top_focusable_widget( const ofaPage *page )
{
	ofaTVAFormPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_TVA_FORM_PAGE( page ), NULL );

	priv = ofa_tva_form_page_get_instance_private( OFA_TVA_FORM_PAGE( page ));

	return( ofa_tvbin_get_tree_view( OFA_TVBIN( priv->tview )));
}

static GtkWidget *
action_page_v_setup_view( ofaActionPage *page )
{
	static const gchar *thisfn = "ofa_tva_form_page_v_setup_view";
	ofaTVAFormPagePrivate *priv;
	GtkWidget *widget;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_tva_form_page_get_instance_private( OFA_TVA_FORM_PAGE( page ));

	priv->hub = ofa_igetter_get_hub( OFA_IGETTER( page ));
	g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );
	priv->is_writable = ofa_hub_dossier_is_writable( priv->hub );

	widget = setup_treeview( OFA_TVA_FORM_PAGE( page ));

	return( widget );
}

/*
 * returns the container which displays the TVA form
 */
static GtkWidget *
setup_treeview( ofaTVAFormPage *self )
{
	ofaTVAFormPagePrivate *priv;

	priv = ofa_tva_form_page_get_instance_private( self );

	priv->tview = ofa_tva_form_treeview_new( priv->hub );
	ofa_tva_form_treeview_set_settings_key( priv->tview, priv->settings_prefix );
	ofa_tva_form_treeview_setup_columns( priv->tview );

	/* ofaTVBin signals */
	g_signal_connect( priv->tview, "ofa-insert", G_CALLBACK( on_insert_key ), self );

	/* ofaTVAFormTreeview signals */
	g_signal_connect( priv->tview, "ofa-vatchanged", G_CALLBACK( on_row_selected ), self );
	g_signal_connect( priv->tview, "ofa-vatactivated", G_CALLBACK( on_row_activated ), self );
	g_signal_connect( priv->tview, "ofa-vatdelete", G_CALLBACK( on_delete_key ), self );

	return( GTK_WIDGET( priv->tview ));
}

static void
action_page_v_setup_actions( ofaActionPage *page, ofaButtonsBox *buttons_box )
{
	ofaTVAFormPagePrivate *priv;

	priv = ofa_tva_form_page_get_instance_private( OFA_TVA_FORM_PAGE( page ));

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
	g_simple_action_set_enabled( priv->update_action, FALSE );

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
	g_simple_action_set_enabled( priv->delete_action, FALSE );

	ofa_buttons_box_add_spacer( buttons_box );

	/* declare VAT from selected form */
	priv->declare_action = g_simple_action_new( "declare", NULL );
	g_signal_connect( priv->declare_action, "activate", G_CALLBACK( action_on_declare_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->declare_action ),
			_( "Declare from selected..." ));
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->declare_action ),
					_( "De_clare from selected..." )));
	g_simple_action_set_enabled( priv->declare_action, FALSE );
}

static void
action_page_v_init_view( ofaActionPage *page )
{
	static const gchar *thisfn = "ofa_tva_form_page_v_init_view";
	ofaTVAFormPagePrivate *priv;
	GMenu *menu;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_tva_form_page_get_instance_private( OFA_TVA_FORM_PAGE( page ));

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
	ofa_tva_form_treeview_setup_store( priv->tview );
}

/*
 * TVAFormTreeview callback
 */
static void
on_row_selected( ofaTVAFormTreeview *view, ofoTVAForm *form, ofaTVAFormPage *self )
{
	ofaTVAFormPagePrivate *priv;
	gboolean is_form;

	priv = ofa_tva_form_page_get_instance_private( self );

	is_form = form && OFO_IS_TVA_FORM( form );

	g_simple_action_set_enabled( priv->update_action, is_form );
	g_simple_action_set_enabled( priv->delete_action, check_for_deletability( self, form ));
	g_simple_action_set_enabled( priv->declare_action, priv->is_writable && is_form );
}

/*
 * TVAFormTreeview callback
 */
static void
on_row_activated( ofaTVAFormTreeview *view, ofoTVAForm *form, ofaTVAFormPage *self )
{
	ofaTVAFormPagePrivate *priv;

	g_return_if_fail( form && OFO_IS_TVA_FORM( form ));

	priv = ofa_tva_form_page_get_instance_private( self );

	g_action_activate( G_ACTION( priv->update_action ), NULL );
}

static void
on_insert_key( ofaTVAFormTreeview *view, ofaTVAFormPage *self )
{
	ofaTVAFormPagePrivate *priv;

	priv = ofa_tva_form_page_get_instance_private( self );

	if( priv->is_writable ){
		g_action_activate( G_ACTION( priv->new_action ), NULL );
	}
}

static void
on_delete_key( ofaTVAFormTreeview *view, ofoTVAForm *form, ofaTVAFormPage *self )
{
	ofaTVAFormPagePrivate *priv;

	g_return_if_fail( form && OFO_IS_TVA_FORM( form ));

	priv = ofa_tva_form_page_get_instance_private( self );

	if( check_for_deletability( self, form )){
		g_action_activate( G_ACTION( priv->delete_action ), NULL );
	}
}

/*
 * create a new tva form
 * creating a new tva record is the rule of 'Declare' button
 */
static void
action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaTVAFormPage *self )
{
	ofoTVAForm *form;
	GtkWindow *toplevel;

	form = ofo_tva_form_new();
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_tva_form_properties_run( OFA_IGETTER( self ), toplevel, form );
}

static void
action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaTVAFormPage *self )
{
	ofaTVAFormPagePrivate *priv;
	ofoTVAForm *form;
	GtkWindow *toplevel;

	priv = ofa_tva_form_page_get_instance_private( self );

	form = ofa_tva_form_treeview_get_selected( priv->tview );
	g_return_if_fail( form && OFO_IS_TVA_FORM( form ));

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_tva_form_properties_run( OFA_IGETTER( self ), toplevel, form );
}

static void
action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaTVAFormPage *self )
{
	ofaTVAFormPagePrivate *priv;
	ofoTVAForm *form;

	priv = ofa_tva_form_page_get_instance_private( self );

	form = ofa_tva_form_treeview_get_selected( priv->tview );
	g_return_if_fail( form && OFO_IS_TVA_FORM( form ));

	delete_with_confirm( self, form );

	gtk_widget_grab_focus( page_v_get_top_focusable_widget( OFA_PAGE( self )));
}

static gboolean
check_for_deletability( ofaTVAFormPage *self, ofoTVAForm *form )
{
	ofaTVAFormPagePrivate *priv;
	gboolean is_form;

	priv = ofa_tva_form_page_get_instance_private( self );

	is_form = form && OFO_IS_TVA_FORM( form );

	return( priv->is_writable && is_form && ofo_tva_form_is_deletable( form ));
}

static void
delete_with_confirm( ofaTVAFormPage *self, ofoTVAForm *form )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s' TVA form ?" ),
				ofo_tva_form_get_mnemo( form ));

	delete_ok = my_utils_dialog_question( msg, _( "_Delete" ));

	g_free( msg );

	if( delete_ok ){
		ofo_tva_form_delete( form );
	}
}

/*
 * new declaration from the currently selected form
 */
static void
action_on_declare_activated( GSimpleAction *action, GVariant *empty, ofaTVAFormPage *self )
{
	ofaTVAFormPagePrivate *priv;
	ofoTVAForm *form;
	ofoTVARecord *record;
	GtkWindow *toplevel;

	priv = ofa_tva_form_page_get_instance_private( self );

	form = ofa_tva_form_treeview_get_selected( priv->tview );
	g_return_if_fail( form && OFO_IS_TVA_FORM( form ));

	record = ofo_tva_record_new_from_form( form );
	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_tva_record_new_run( OFA_IGETTER( self ), toplevel, record );
}
