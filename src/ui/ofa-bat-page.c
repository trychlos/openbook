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
#include "my/my-double.h"
#include "my/my-utils.h"

#include "api/ofa-buttons-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-settings.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-bat.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-bat-page.h"
#include "ui/ofa-bat-properties.h"
#include "ui/ofa-bat-store.h"
#include "ui/ofa-bat-treeview.h"
#include "ui/ofa-bat-utils.h"

/* private instance data
 */
typedef struct {

	/* runtime data
	 */
	gboolean        is_writable;		/* whether the dossier is current */

	/* actions
	 */
	GSimpleAction  *new_action;
	GSimpleAction  *update_action;
	GSimpleAction  *delete_action;

	/* UI
	 */
	ofaBatTreeview *tview;				/* the main treeview of the page */
	GtkWidget      *update_btn;
	GtkWidget      *delete_btn;
	GtkWidget      *import_btn;
}
	ofaBatPagePrivate;

static const gchar *st_group_name       = "bat";

static GtkWidget *v_setup_view( ofaPage *page );
static GtkWidget *v_setup_buttons( ofaPage *page );
static void       v_init_view( ofaPage *page );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       on_row_selected( ofaBatTreeview *tview, ofoBat *bat, ofaBatPage *self );
static void       on_row_activated( ofaBatTreeview *tview, ofoBat *bat, ofaBatPage *self );
static void       on_delete_key( ofaBatTreeview *tview, ofoBat *bat, ofaBatPage *self );
static void       on_update_action_activated( GSimpleAction *action, GVariant *empty, ofaBatPage *self );
static void       on_update_clicked( GtkButton *button, ofaBatPage *self );
static void       on_delete_clicked( GtkButton *button, ofaBatPage *self );
static void       on_import_clicked( GtkButton *button, ofaBatPage *self );
static gboolean   check_for_deletability( ofaBatPage *self, ofoBat *bat );
static void       delete_with_confirm( ofaBatPage *self, ofoBat *bat );

G_DEFINE_TYPE_EXTENDED( ofaBatPage, ofa_bat_page, OFA_TYPE_PAGE, 0,
		G_ADD_PRIVATE( ofaBatPage ))

static void
bat_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_bat_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BAT_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_page_parent_class )->finalize( instance );
}

static void
bat_page_dispose( GObject *instance )
{
	ofaBatPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_BAT_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = ofa_bat_page_get_instance_private( OFA_BAT_PAGE( instance ));

		g_object_unref( priv->update_action );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_page_parent_class )->dispose( instance );
}

static void
ofa_bat_page_init( ofaBatPage *self )
{
	static const gchar *thisfn = "ofa_bat_page_init";

	g_return_if_fail( OFA_IS_BAT_PAGE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_bat_page_class_init( ofaBatPageClass *klass )
{
	static const gchar *thisfn = "ofa_bat_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_bat_page_v_setup_view";
	ofaBatPagePrivate *priv;
	ofaHub *hub;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_bat_page_get_instance_private( OFA_BAT_PAGE( page ));

	hub = ofa_igetter_get_hub( OFA_IGETTER( page ));
	priv->is_writable = ofa_hub_dossier_is_writable( hub );

	priv->tview = ofa_bat_treeview_new();
	ofa_bat_treeview_set_settings_key( priv->tview, G_OBJECT_TYPE_NAME( page ));
	ofa_bat_treeview_set_hub( priv->tview, hub );

	my_utils_widget_set_margins( GTK_WIDGET( priv->tview ), 2, 2, 2, 2 );

	/* ofaBatTreeview signals */
	g_signal_connect( priv->tview, "ofa-batchanged", G_CALLBACK( on_row_selected ), page );
	g_signal_connect( priv->tview, "ofa-batactivated", G_CALLBACK( on_row_activated ), page );
	g_signal_connect( priv->tview, "ofa-batdelete", G_CALLBACK( on_delete_key ), page );

	return( GTK_WIDGET( priv->tview ));
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	ofaBatPagePrivate *priv;
	ofaButtonsBox *buttons_box;

	priv = ofa_bat_page_get_instance_private( OFA_BAT_PAGE( page ));

	/* setup actions and UI */
	buttons_box = ofa_buttons_box_new();
	my_utils_widget_set_margins( GTK_WIDGET( buttons_box ), 2, 2, 0, 0 );

	/* action 'New' is always here */
	//ofa_iactionable_add_action_to_menu( OFA_IACTIONABLE( page ), "bat", NULL, _( "New" ));
	ofa_buttons_box_add_button_with_mnemonic(
				buttons_box, BUTTON_NEW, NULL, NULL );

	/* update action */
	priv->update_action = g_simple_action_new( "update", NULL );
	g_signal_connect( priv->update_action, "activate", G_CALLBACK( on_update_action_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), st_group_name, G_ACTION( priv->update_action ),
			priv->is_writable ? OFA_IACTIONABLE_PROPERTIES_ITEM_EDIT : OFA_IACTIONABLE_PROPERTIES_ITEM_DISPLAY );
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_set_button(
					OFA_IACTIONABLE( page ), st_group_name, G_ACTION( priv->update_action ),
					OFA_IACTIONABLE_PROPERTIES_BTN ));
	/*
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), G_ACTION( priv->update_action ),
			priv->is_writable ? _( "Edit properties" ) : _( "Display properties" ));
			*/
	/*
	priv->update_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_PROPERTIES, G_CALLBACK( on_update_clicked ), page );
	*/

	priv->delete_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_DELETE, G_CALLBACK( on_delete_clicked ), page );

	ofa_buttons_box_add_spacer( buttons_box );

	priv->import_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, _( "_Import..." ), G_CALLBACK( on_import_clicked ), page );
	gtk_widget_set_sensitive( priv->import_btn, priv->is_writable );

	return( GTK_WIDGET( buttons_box ));
}

static void
v_init_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_bat_page_v_init_view";
	ofaBatPagePrivate *priv;
	GMenu *menu;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_bat_page_get_instance_private( OFA_BAT_PAGE( page ));

	menu = ofa_iactionable_get_menu( OFA_IACTIONABLE( page ), st_group_name );
	ofa_icontext_set_menu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( page ),
			menu );

	menu = ofa_tvbin_get_menu( OFA_TVBIN( priv->tview ));
	ofa_icontext_append_submenu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( priv->tview ),
			OFA_IACTIONABLE_VISIBLE_COLUMNS_ITEM, menu );
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaBatPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_BAT_PAGE( page ), NULL );

	priv = ofa_bat_page_get_instance_private( OFA_BAT_PAGE( page ));

	return( ofa_tvbin_get_treeview( OFA_TVBIN( priv->tview )));
}

/*
 * signal sent by ofaBatTreeview on selection change
 */
static void
on_row_selected( ofaBatTreeview *tview, ofoBat *bat, ofaBatPage *self )
{
	ofaBatPagePrivate *priv;
	gboolean is_bat;

	priv = ofa_bat_page_get_instance_private( self );

	is_bat = bat && OFO_IS_BAT( bat );

	g_simple_action_set_enabled( priv->update_action, is_bat );
	//gtk_widget_set_sensitive( priv->update_btn, is_bat );
	gtk_widget_set_sensitive( priv->delete_btn, check_for_deletability( self, bat ));
}

/*
 * signal sent by ofaBatTreeview on selection activation
 */
static void
on_row_activated( ofaBatTreeview *tview, ofoBat *bat, ofaBatPage *self )
{
	on_update_clicked( NULL, OFA_BAT_PAGE( self ));
}

/*
 * signal sent by ofaBatTreeview on Delete key
 *
 * Note that the key may be pressed, even if the button
 * is disabled. So have to check all prerequisite conditions.
 * If the current row is not deletable, just silently ignore the key.
 */
static void
on_delete_key( ofaBatTreeview *tview, ofoBat *bat, ofaBatPage *self )
{
	if( check_for_deletability( self, bat )){
		delete_with_confirm( self, bat );
	}
}

/*
 * only notes can be updated if the dossier is writable
 */
static void
on_update_action_activated( GSimpleAction *action, GVariant *empty, ofaBatPage *self )
{
	g_debug( "on_update_action_activated" );
}

static void
on_update_clicked( GtkButton *button, ofaBatPage *self )
{
	ofaBatPagePrivate *priv;
	ofoBat *bat;
	GtkWindow *toplevel;

	priv = ofa_bat_page_get_instance_private( self );

	bat = ofa_bat_treeview_get_selected( priv->tview );
	if( bat ){
		toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
		ofa_bat_properties_run( OFA_IGETTER( self ), toplevel, bat );
	}

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( self )));
}

static void
on_delete_clicked( GtkButton *button, ofaBatPage *self )
{
	ofaBatPagePrivate *priv;
	ofoBat *bat;

	priv = ofa_bat_page_get_instance_private( self );

	bat = ofa_bat_treeview_get_selected( priv->tview );
	g_return_if_fail( check_for_deletability( self, bat ));

	delete_with_confirm( self, bat );

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( self )));
}

/*
 * open GtkFileChooser dialog to let the user select the file to be
 * imported and import it
 */
static void
on_import_clicked( GtkButton *button, ofaBatPage *self )
{
	ofaBatPagePrivate *priv;
	ofxCounter bat_id;
	GtkWindow *toplevel;

	priv = ofa_bat_page_get_instance_private( self );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	bat_id = ofa_bat_utils_import( OFA_IGETTER( self ), toplevel );
	if( bat_id > 0 ){
		ofa_bat_treeview_set_selected( priv->tview, bat_id );
	}

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( self )));
}

static gboolean
check_for_deletability( ofaBatPage *self, ofoBat *bat )
{
	ofaBatPagePrivate *priv;
	gboolean is_bat, deletable;

	priv = ofa_bat_page_get_instance_private( self );

	is_bat = bat && OFO_IS_BAT( bat );

	deletable = priv->is_writable && is_bat && ofo_bat_is_deletable( bat );

	return( deletable );
}

static void
delete_with_confirm( ofaBatPage *self, ofoBat *bat )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup( _( "Are you sure you want delete this BAT file\n"
			"(All the corresponding lines will be deleted too) ?" ));

	delete_ok = my_utils_dialog_question( msg, _( "_Delete" ));

	g_free( msg );

	if( delete_ok ){
		ofo_bat_delete( bat );
	}
}
