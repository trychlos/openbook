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

#include "my/my-utils.h"

#include "api/ofa-buttons-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-settings.h"
#include "api/ofo-class.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-class-properties.h"
#include "ui/ofa-class-page.h"
#include "ui/ofa-class-store.h"
#include "ui/ofa-class-treeview.h"

/* private instance data
 */
typedef struct {

	/* internals
	 */
	gboolean           is_writable;

	/* UI
	 */
	ofaClassTreeview  *tview;

	/* actions
	 */
	GSimpleAction     *new_action;
	GSimpleAction     *update_action;
	GSimpleAction     *delete_action;
}
	ofaClassPagePrivate;

static const gchar *st_action_group_name = "class";

static GtkWidget *v_setup_view( ofaPage *page );
static GtkWidget *v_setup_buttons( ofaPage *page );
static void       v_init_view( ofaPage *page );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       on_row_selected( ofaClassTreeview *treeview, ofoClass *class, ofaClassPage *self );
static void       on_row_activated( ofaClassTreeview *treeview, ofoClass *class, ofaClassPage *self );
static void       on_delete_key( ofaClassTreeview *tview, ofoClass *class, ofaClassPage *self );
static void       on_insert_key( ofaClassTreeview *tview, ofaClassPage *self );
static void       action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaClassPage *self );
static void       action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaClassPage *self );
static void       action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaClassPage *self );
static gboolean   check_for_deletability( ofaClassPage *self, ofoClass *class );
static void       delete_with_confirm( ofaClassPage *self, ofoClass *class );

G_DEFINE_TYPE_EXTENDED( ofaClassPage, ofa_class_page, OFA_TYPE_PAGE, 0,
		G_ADD_PRIVATE( ofaClassPage ))

static void
class_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_class_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CLASS_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_class_page_parent_class )->finalize( instance );
}

static void
class_page_dispose( GObject *instance )
{
	ofaClassPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_CLASS_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = ofa_class_page_get_instance_private( OFA_CLASS_PAGE( instance ));

		g_object_unref( priv->new_action );
		g_object_unref( priv->update_action );
		g_object_unref( priv->delete_action );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_class_page_parent_class )->dispose( instance );
}

static void
ofa_class_page_init( ofaClassPage *self )
{
	static const gchar *thisfn = "ofa_class_page_init";

	g_return_if_fail( self && OFA_IS_CLASS_PAGE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_class_page_class_init( ofaClassPageClass *klass )
{
	static const gchar *thisfn = "ofa_class_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = class_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = class_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_class_page_v_setup_view";
	ofaClassPagePrivate *priv;
	ofaHub *hub;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_class_page_get_instance_private( OFA_CLASS_PAGE( page ));

	hub = ofa_igetter_get_hub( OFA_IGETTER( page ));
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	priv->is_writable = ofa_hub_dossier_is_writable( hub );

	priv->tview = ofa_class_treeview_new();
	ofa_class_treeview_set_settings_key( priv->tview, G_OBJECT_TYPE_NAME( page ));
	ofa_class_treeview_setup_columns( priv->tview );
	ofa_class_treeview_set_hub( priv->tview, hub );
	my_utils_widget_set_margins( GTK_WIDGET( priv->tview ), 2, 2, 2, 0 );

	/* ofaTVBin signals */
	g_signal_connect( priv->tview, "ofa-insert", G_CALLBACK( on_insert_key ), page );

	/* ofaBatTreeview signals */
	g_signal_connect( priv->tview, "ofa-classchanged", G_CALLBACK( on_row_selected ), page );
	g_signal_connect( priv->tview, "ofa-classactivated", G_CALLBACK( on_row_activated ), page );
	g_signal_connect( priv->tview, "ofa-classdelete", G_CALLBACK( on_delete_key ), page );

	return( GTK_WIDGET( priv->tview ));
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	ofaClassPagePrivate *priv;
	ofaButtonsBox *buttons_box;

	priv = ofa_class_page_get_instance_private( OFA_CLASS_PAGE( page ));

	buttons_box = ofa_buttons_box_new();
	my_utils_widget_set_margins( GTK_WIDGET( buttons_box ), 2, 2, 0, 0 );

	/* new action */
	priv->new_action = g_simple_action_new( "new", NULL );
	g_simple_action_set_enabled( priv->new_action, priv->is_writable );
	g_signal_connect( priv->new_action, "activate", G_CALLBACK( action_on_new_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), st_action_group_name, G_ACTION( priv->new_action ),
			OFA_IACTIONABLE_NEW_ITEM );
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_set_button(
					OFA_IACTIONABLE( page ), st_action_group_name, G_ACTION( priv->new_action ),
					OFA_IACTIONABLE_NEW_BTN ));

	/* update action */
	priv->update_action = g_simple_action_new( "update", NULL );
	g_signal_connect( priv->update_action, "activate", G_CALLBACK( action_on_update_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), st_action_group_name, G_ACTION( priv->update_action ),
			priv->is_writable ? OFA_IACTIONABLE_PROPERTIES_ITEM_EDIT : OFA_IACTIONABLE_PROPERTIES_ITEM_DISPLAY );
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_set_button(
					OFA_IACTIONABLE( page ), st_action_group_name, G_ACTION( priv->update_action ),
					OFA_IACTIONABLE_PROPERTIES_BTN ));

	/* delete action */
	priv->delete_action = g_simple_action_new( "delete", NULL );
	g_signal_connect( priv->delete_action, "activate", G_CALLBACK( action_on_delete_activated ), page );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( page ), st_action_group_name, G_ACTION( priv->delete_action ),
			OFA_IACTIONABLE_DELETE_ITEM );
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_set_button(
					OFA_IACTIONABLE( page ), st_action_group_name, G_ACTION( priv->delete_action ),
					OFA_IACTIONABLE_DELETE_BTN ));

	return( GTK_WIDGET( buttons_box ));
}

static void
v_init_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_class_page_v_init_view";
	ofaClassPagePrivate *priv;
	GMenu *menu;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_class_page_get_instance_private( OFA_CLASS_PAGE( page ));

	menu = ofa_iactionable_get_menu( OFA_IACTIONABLE( page ), st_action_group_name );
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
	ofaClassPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_CLASS_PAGE( page ), NULL );

	priv = ofa_class_page_get_instance_private( OFA_CLASS_PAGE( page ));

	return( ofa_tvbin_get_treeview( OFA_TVBIN( priv->tview )));
}

/*
 * Signal sent by ofaClassTreeview on selection change
 *
 * Other actions do not depend of the selection:
 * - new: enabled when dossier is writable.
 */
static void
on_row_selected( ofaClassTreeview *tview, ofoClass *class, ofaClassPage *self )
{
	ofaClassPagePrivate *priv;
	gboolean is_class;

	priv = ofa_class_page_get_instance_private( self );

	is_class = class && OFO_IS_CLASS( class );

	g_simple_action_set_enabled( priv->update_action, is_class );

	g_simple_action_set_enabled( priv->delete_action, check_for_deletability( self, class ));
}

/*
 * signal sent by ofaClassTreeview on selection activation
 */
static void
on_row_activated( ofaClassTreeview *tview, ofoClass *class, ofaClassPage *self )
{
	ofaClassPagePrivate *priv;

	priv = ofa_class_page_get_instance_private( self );

	g_action_activate( G_ACTION( priv->update_action ), NULL );
}

/*
 * signal sent by ofaClassTreeview on Delete key
 *
 * Note that the key may be pressed, even if the button
 * is disabled. So have to check all prerequisite conditions.
 * If the current row is not deletable, just silently ignore the key.
 */
static void
on_delete_key( ofaClassTreeview *tview, ofoClass *class, ofaClassPage *self )
{
	if( check_for_deletability( self, class )){
		delete_with_confirm( self, class );
	}
}

/*
 * signal sent by ofaTVBin on Insert key
 *
 * Note that the key may be pressend even if dossier is not writable.
 * If this is the case, just silently ignore the key.
 */
static void
on_insert_key( ofaClassTreeview *tview, ofaClassPage *self )
{
	ofaClassPagePrivate *priv;

	priv = ofa_class_page_get_instance_private( self );

	if( priv->is_writable ){
		g_action_activate( G_ACTION( priv->new_action ), NULL );
	}
}

static void
action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaClassPage *self )
{
	static const gchar *thisfn = "ofa_class_page_action_on_new_activated";
	ofoClass *class;
	GtkWindow *toplevel;

	g_debug( "%s: action=%p, empty=%p, self=%p",
			thisfn, ( void * ) action, ( void * ) empty, ( void * ) self );

	class = ofo_class_new();
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_class_properties_run( OFA_IGETTER( self ), toplevel, class );
}

static void
action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaClassPage *self )
{
	static const gchar *thisfn = "ofa_class_page_action_on_update_activated";
	ofaClassPagePrivate *priv;
	ofoClass *class;
	GtkWindow *toplevel;

	g_debug( "%s: action=%p, empty=%p, self=%p",
			thisfn, ( void * ) action, ( void * ) empty, ( void * ) self );

	priv = ofa_class_page_get_instance_private( self );

	class = ofa_class_treeview_get_selected( priv->tview );
	if( class ){
		toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
		ofa_class_properties_run( OFA_IGETTER( self ), toplevel, class );
	}
}

static void
action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaClassPage *self )
{
	static const gchar *thisfn = "ofa_class_page_action_on_delete_activated";
	ofaClassPagePrivate *priv;
	ofoClass *class;

	g_debug( "%s: action=%p, empty=%p, self=%p",
			thisfn, ( void * ) action, ( void * ) empty, ( void * ) self );

	priv = ofa_class_page_get_instance_private( self );

	class = ofa_class_treeview_get_selected( priv->tview );
	g_return_if_fail( check_for_deletability( self, class ));

	delete_with_confirm( self, class );
}

static gboolean
check_for_deletability( ofaClassPage *self, ofoClass *class )
{
	ofaClassPagePrivate *priv;
	gboolean is_class;

	priv = ofa_class_page_get_instance_private( self );

	is_class = class && OFO_IS_CLASS( class );

	return( is_class && priv->is_writable && ofo_class_is_deletable( class ));
}

static void
delete_with_confirm( ofaClassPage *self, ofoClass *class )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s' class ?" ),
			ofo_class_get_label( class ));

	delete_ok = my_utils_dialog_question( msg, _( "_Delete" ));

	g_free( msg );

	if( delete_ok ){
		ofo_class_delete( class );
	}
}
