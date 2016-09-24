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

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-buttons-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofo-dossier.h"

#include "tva/ofa-tva-record-page.h"
#include "tva/ofa-tva-record-properties.h"
#include "tva/ofa-tva-record-store.h"
#include "tva/ofa-tva-record-treeview.h"
#include "tva/ofo-tva-record.h"

/* priv instance data
 */
typedef struct {

	/* internals
	 */
	ofaHub               *hub;
	gboolean              is_writable;
	gchar                *settings_prefix;

	/* UI
	 */
	ofaTVARecordTreeview *tview;

	/* actions
	 */
	GSimpleAction        *new_action;
	GSimpleAction        *update_action;
	GSimpleAction        *delete_action;
}
	ofaTVARecordPagePrivate;

static GtkWidget *v_setup_view( ofaPage *page );
static GtkWidget *setup_treeview( ofaTVARecordPage *self );
static GtkWidget *v_setup_buttons( ofaPage *page );
static void       v_init_view( ofaPage *page );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       on_row_selected( ofaTVARecordTreeview *view, ofoTVARecord *record, ofaTVARecordPage *self );
static void       on_row_activated( ofaTVARecordTreeview *view, ofoTVARecord *record, ofaTVARecordPage *self );
static void       on_delete_key( ofaTVARecordTreeview *view, ofoTVARecord *record, ofaTVARecordPage *self );
static void       action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaTVARecordPage *self );
static void       action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaTVARecordPage *self );
static void       action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaTVARecordPage *self );
static gboolean   check_for_deletability( ofaTVARecordPage *self, ofoTVARecord *record );
static void       delete_with_confirm( ofaTVARecordPage *self, ofoTVARecord *record );

G_DEFINE_TYPE_EXTENDED( ofaTVARecordPage, ofa_tva_record_page, OFA_TYPE_PAGE, 0,
		G_ADD_PRIVATE( ofaTVARecordPage ))

static void
tva_record_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_record_page_finalize";
	ofaTVARecordPagePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_RECORD_PAGE( instance ));

	/* free data members here */
	priv = ofa_tva_record_page_get_instance_private( OFA_TVA_RECORD_PAGE( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_record_page_parent_class )->finalize( instance );
}

static void
tva_record_page_dispose( GObject *instance )
{
	ofaTVARecordPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_TVA_RECORD_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = ofa_tva_record_page_get_instance_private( OFA_TVA_RECORD_PAGE( instance ));

		g_object_unref( priv->new_action );
		g_object_unref( priv->update_action );
		g_object_unref( priv->delete_action );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_record_page_parent_class )->dispose( instance );
}

static void
ofa_tva_record_page_init( ofaTVARecordPage *self )
{
	static const gchar *thisfn = "ofa_tva_record_page_init";
	ofaTVARecordPagePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_TVA_RECORD_PAGE( self ));

	priv = ofa_tva_record_page_get_instance_private( self );

	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_tva_record_page_class_init( ofaTVARecordPageClass *klass )
{
	static const gchar *thisfn = "ofa_tva_record_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_record_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_record_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_tva_record_page_v_setup_view";
	ofaTVARecordPagePrivate *priv;
	GtkWidget *widget;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_tva_record_page_get_instance_private( OFA_TVA_RECORD_PAGE( page ));

	priv->hub = ofa_igetter_get_hub( OFA_IGETTER( page ));
	g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );
	priv->is_writable = ofa_hub_dossier_is_writable( priv->hub );

	widget = setup_treeview( OFA_TVA_RECORD_PAGE( page ));

	return( widget );
}

/*
 * returns the container which displays the TVARecord declaration
 */
static GtkWidget *
setup_treeview( ofaTVARecordPage *self )
{
	ofaTVARecordPagePrivate *priv;

	priv = ofa_tva_record_page_get_instance_private( self );

	priv->tview = ofa_tva_record_treeview_new();
	my_utils_widget_set_margins( GTK_WIDGET( priv->tview ), 2, 2, 2, 0 );
	ofa_tva_record_treeview_set_settings_key( priv->tview, priv->settings_prefix );
	ofa_tva_record_treeview_setup_columns( priv->tview );

	/* Insert key is not handled here
	 * as creating a new declaration requires first selecting a form */

	/* ofaTVAFormTreeview signals */
	g_signal_connect( priv->tview, "ofa-vatchanged", G_CALLBACK( on_row_selected ), self );
	g_signal_connect( priv->tview, "ofa-vatactivated", G_CALLBACK( on_row_activated ), self );
	g_signal_connect( priv->tview, "ofa-vatdelete", G_CALLBACK( on_delete_key ), self );

	return( GTK_WIDGET( priv->tview ));
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	ofaTVARecordPagePrivate *priv;
	ofaButtonsBox *buttons_box;

	priv = ofa_tva_record_page_get_instance_private( OFA_TVA_RECORD_PAGE( page ));

	buttons_box = ofa_buttons_box_new();
	my_utils_widget_set_margins( GTK_WIDGET( buttons_box ), 2, 2, 0, 0 );

	/* new action - always disabled */
	priv->new_action = g_simple_action_new( "new", NULL );
	g_simple_action_set_enabled( priv->new_action, FALSE );
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
	g_simple_action_set_enabled( priv->update_action, FALSE );
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
	g_simple_action_set_enabled( priv->delete_action, FALSE );
	g_signal_connect( priv->delete_action, "activate", G_CALLBACK( action_on_delete_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->delete_action ),
			OFA_IACTIONABLE_DELETE_ITEM );
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( page ), priv->settings_prefix, G_ACTION( priv->delete_action ),
					OFA_IACTIONABLE_DELETE_BTN ));

	return( GTK_WIDGET( buttons_box ));
}

static void
v_init_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_tva_record_page_v_init_view";
	ofaTVARecordPagePrivate *priv;
	GMenu *menu;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_tva_record_page_get_instance_private( OFA_TVA_RECORD_PAGE( page ));

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
	ofa_tva_record_treeview_set_hub( priv->tview, priv->hub );
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaTVARecordPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_TVA_RECORD_PAGE( page ), NULL );

	priv = ofa_tva_record_page_get_instance_private( OFA_TVA_RECORD_PAGE( page ));

	return( ofa_tvbin_get_treeview( OFA_TVBIN( priv->tview )));
}

/*
 * TVARecordTreeview callback
 */
static void
on_row_selected( ofaTVARecordTreeview *view, ofoTVARecord *record, ofaTVARecordPage *self )
{
	ofaTVARecordPagePrivate *priv;
	gboolean is_record;

	priv = ofa_tva_record_page_get_instance_private( self );

	is_record = record && OFO_IS_TVA_RECORD( record );

	g_simple_action_set_enabled( priv->update_action, is_record );
	g_simple_action_set_enabled( priv->delete_action, check_for_deletability( self, record ));
}

/*
 * TVARecordTreeview callback
 */
static void
on_row_activated( ofaTVARecordTreeview *view, ofoTVARecord *form, ofaTVARecordPage *self )
{
	ofaTVARecordPagePrivate *priv;

	g_return_if_fail( form && OFO_IS_TVA_RECORD( form ));

	priv = ofa_tva_record_page_get_instance_private( self );

	g_action_activate( G_ACTION( priv->update_action ), NULL );
}

static void
on_delete_key( ofaTVARecordTreeview *view, ofoTVARecord *form, ofaTVARecordPage *self )
{
	ofaTVARecordPagePrivate *priv;

	g_return_if_fail( form && OFO_IS_TVA_RECORD( form ));

	priv = ofa_tva_record_page_get_instance_private( self );

	if( check_for_deletability( self, form )){
		g_action_activate( G_ACTION( priv->delete_action ), NULL );
	}
}

static void
action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaTVARecordPage *self )
{
}

static void
action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaTVARecordPage *self )
{
	ofaTVARecordPagePrivate *priv;
	ofoTVARecord *record;
	GtkWindow *toplevel;

	priv = ofa_tva_record_page_get_instance_private( self );

	record = ofa_tva_record_treeview_get_selected( priv->tview );
	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_tva_record_properties_run( OFA_IGETTER( self ), toplevel, record );
	/* update is taken into account by dossier signaling system */
}

static void
action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaTVARecordPage *self )
{
	ofaTVARecordPagePrivate *priv;
	ofoTVARecord *record;

	priv = ofa_tva_record_page_get_instance_private( self );

	record = ofa_tva_record_treeview_get_selected( priv->tview );
	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));

	delete_with_confirm( self, record );

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( self )));
}

static gboolean
check_for_deletability( ofaTVARecordPage *self, ofoTVARecord *record )
{
	ofaTVARecordPagePrivate *priv;
	gboolean is_record;

	priv = ofa_tva_record_page_get_instance_private( self );

	is_record = record && OFO_IS_TVA_FORM( record );

	return( priv->is_writable && is_record && ofo_tva_record_is_deletable( record ));
}

static void
delete_with_confirm( ofaTVARecordPage *self, ofoTVARecord *record )
{
	gchar *msg, *send;
	gboolean delete_ok;

	send = my_date_to_str( ofo_tva_record_get_end( record ), ofa_prefs_date_display());
	msg = g_strdup_printf( _( "Are you sure you want delete the %s at %s TVA declaration ?" ),
				ofo_tva_record_get_mnemo( record ), send );

	delete_ok = my_utils_dialog_question( msg, _( "_Delete" ));

	g_free( msg );
	g_free( send );

	if( delete_ok ){
		ofo_tva_record_delete( record );
	}
}
