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

#include "my/my-utils.h"

#include "api/ofa-icontext.h"

#define ICONTEXT_LAST_VERSION   1

/* a data structure attached to the instance
 * - action_groups are indexed by their name.
 */
#define ICONTEXT_DATA          "ofa-icontext-data"

typedef struct {
	GMenu     *menu;
	GtkWidget *popup;
	GList     *actionables;
}
	sIContext;

static guint st_initializations         = 0;	/* interface initialization count */

static GType      register_type( void );
static void       interface_base_init( ofaIContextInterface *klass );
static void       interface_base_finalize( ofaIContextInterface *klass );
static gboolean   on_popup_menu( ofaIContext *instance, sIContext *sdata );
static gboolean   on_button_pressed( ofaIContext *instance, GdkEventButton *event, sIContext *sdata );
static void       do_popup_menu( ofaIContext *instance, sIContext *sdata, GdkEvent *event );
static void       enum_action_groups_cb( ofaIActionable *actionable, const gchar *group_name, GActionGroup *action_group, sIContext *sdata );
static void       dump_menu_model( ofaIContext *instance, GMenuModel *model );
static sIContext *get_instance_data( ofaIContext *instance );
static void       connect_to_keyboard_event( ofaIContext *instance, sIContext *sdata );
static void       connect_to_mouse_event( ofaIContext *instance, sIContext *sdata );
static void       on_instance_finalized( sIContext *sdata, GObject *finalized_instance );
static void       free_actionables( GList **actionables );

/**
 * ofa_icontext_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_icontext_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_icontext_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_icontext_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIContextInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIContext", &info, 0 );

	g_type_interface_add_prerequisite( type, GTK_TYPE_WIDGET );

	return( type );
}

static void
interface_base_init( ofaIContextInterface *klass )
{
	static const gchar *thisfn = "ofa_icontext_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIContextInterface *klass )
{
	static const gchar *thisfn = "ofa_icontext_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_icontext_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_icontext_get_interface_last_version( void )
{
	return( ICONTEXT_LAST_VERSION );
}

/**
 * ofa_icontext_get_interface_version:
 * @instance: this #ofaIContext instance.
 *
 * Returns: the version number of this interface implemented by the
 * @instance.
 *
 * Defaults to 1.
 */
guint
ofa_icontext_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_ICONTEXT );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIContextInterface * ) iface )->get_interface_version ){
		version = (( ofaIContextInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIContext::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_icontext_append_submenu:
 * @instance: this #ofaIContext instance.
 * @actionable: the #ofaIActionable instance, which holds the @menu.
 * @label: the label to be set in the main menu.
 * @menu: the #GMenu to be set as submenu.
 *
 * Append a submenu to the contextual menu.
 *
 * The interface takes its own reference on @menu.
 * Do not take here any more reference to @actionable, because it would
 * prevent the @instance to be finalized if it happened that
 * @instance=@actionable.
 */
void
ofa_icontext_append_submenu( ofaIContext *instance, ofaIActionable *actionable, const gchar *label, GMenu *menu )
{
	static const gchar *thisfn = "ofa_icontext_append_submenu";
	sIContext *sdata;
	GMenuItem *subitem;
	GMenu *submenu;

	g_debug( "%s: instance=%p, actionable=%p, label=%s, menu=%p",
			thisfn, ( void * ) instance, ( void * ) actionable, label, ( void * ) menu );

	if( 0 ){
		dump_menu_model( instance, G_MENU_MODEL( menu ));
	}

	g_return_if_fail( instance && OFA_IS_ICONTEXT( instance ));
	g_return_if_fail( actionable && OFA_IS_IACTIONABLE( actionable ));
	g_return_if_fail( my_strlen( label ));
	g_return_if_fail( menu && G_IS_MENU( menu ));

	sdata = get_instance_data( instance );
	g_return_if_fail( sdata->menu && G_IS_MENU( sdata->menu ));

	subitem = g_menu_item_new_submenu( label, G_MENU_MODEL( menu ));
	submenu = g_menu_new();
	g_menu_append_item( submenu, subitem );
	g_menu_append_section( sdata->menu, NULL, G_MENU_MODEL( submenu ));

	g_object_unref( submenu );
	g_object_unref( subitem );

	sdata->actionables = g_list_append( sdata->actionables, actionable );
}

/**
 * ofa_icontext_get_menu:
 * @instance: this #ofaIContext instance.
 *
 * Returns: the current menu model.
 */
GMenu *
ofa_icontext_get_menu( ofaIContext *instance )
{
	sIContext *sdata;

	g_return_val_if_fail( instance && OFA_IS_ICONTEXT( instance ), NULL );

	sdata = get_instance_data( instance );

	return( sdata->menu );
}

/**
 * ofa_icontext_set_menu:
 * @instance: this #ofaIContext instance.
 * @actionable: the #ofaIActionable instance, which holds the @menu.
 * @menu: the #GMenu to be set as the model for the contextual popup.
 *
 * Setup the model for the contextual menu.
 *
 * The interface takes its own reference on @menu.
 * Do not take here any more reference to @actionable, because it would
 * prevent the @instance to be finalized if it happend that
 * @instance=@actionable.
 */
void
ofa_icontext_set_menu( ofaIContext *instance, ofaIActionable *actionable, GMenu *menu )
{
	static const gchar *thisfn = "ofa_icontext_set_menu";
	sIContext *sdata;

	g_debug( "%s: instance=%p, actionable=%p, menu=%p",
			thisfn, ( void * ) instance, ( void * ) actionable, ( void * ) menu );

	if( 0 ){
		dump_menu_model( instance, G_MENU_MODEL( menu ));
	}

	g_return_if_fail( instance && OFA_IS_ICONTEXT( instance ));
	g_return_if_fail( actionable && OFA_IS_IACTIONABLE( actionable ));
	g_return_if_fail( menu && G_IS_MENU( menu ));

	sdata = get_instance_data( instance );

	g_clear_object( &sdata->menu );
	free_actionables( &sdata->actionables );

	sdata->menu = g_object_ref( menu );
	sdata->actionables = g_list_append( NULL, actionable );
}

/*
 * Opens a context menu.
 * cf. https://developer.gnome.org/gtk3/stable/gtk-migrating-checklist.html#checklist-popup-menu
 */
static gboolean
on_popup_menu( ofaIContext *instance, sIContext *sdata )
{
	static const gchar *thisfn = "ofa_icontext_on_popup_menu";

	g_debug( "%s: instance=%p, sdata=%p", thisfn, ( void * ) instance, ( void * ) sdata );

	do_popup_menu( instance, sdata, gtk_get_current_event());

	return( TRUE );
}

/*
 * Opens a context menu.
 * cf. https://developer.gnome.org/gtk3/stable/gtk-migrating-checklist.html#checklist-popup-menu
 */
static gboolean
on_button_pressed( ofaIContext *instance, GdkEventButton *event, sIContext *sdata )
{
	static const gchar *thisfn = "ofa_icontext_on_button_pressed";
	gboolean stop = FALSE;

	/* Ignore double-clicks and triple-clicks */
	if( gdk_event_triggers_context_menu(( GdkEvent * ) event ) &&
			event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_SECONDARY ){

		g_debug( "%s: instance=%p, event=%p, sdata=%p",
				thisfn, ( void * ) instance, ( void * ) event, ( void * ) sdata );

		do_popup_menu( instance, sdata, ( GdkEvent * ) event );

		stop = TRUE;
	}

	return( stop );
}

static void
do_popup_menu( ofaIContext *instance, sIContext *sdata, GdkEvent *event )
{
	GList *it;
	ofaIActionable *actionable;

	if( !sdata->popup ){
		sdata->popup = gtk_menu_new_from_model( G_MENU_MODEL( sdata->menu ));

		for( it=sdata->actionables ; it ; it=it->next ){
			actionable = OFA_IACTIONABLE( it->data );
			ofa_iactionable_enum_action_groups( actionable, ( ofaIActionableEnumCb ) enum_action_groups_cb, sdata );
		}
		if( 0 ){
			dump_menu_model( instance, G_MENU_MODEL( sdata->menu ));
		}
	}

	gtk_menu_popup_at_pointer( GTK_MENU( sdata->popup ), event );
}

static void
enum_action_groups_cb( ofaIActionable *actionable, const gchar *group_name, GActionGroup *action_group, sIContext *sdata )
{
	gtk_widget_insert_action_group( sdata->popup, group_name, action_group );
}

static void
dump_menu_model( ofaIContext *instance, GMenuModel *model )
{
	static const gchar *thisfn = "ofa_icontext_dump_menu_model";
	gint i, count;
	GMenuAttributeIter *attribute_iter;
	GMenuLinkIter *link_iter;
	const gchar *attribute_name, *link_name;
	GMenuModel *link_model;
	GVariant *attribute_value;

	/* iterate through items */
	count = g_menu_model_get_n_items( model );
	g_debug( "%s: model=%p, items_count=%d", thisfn, ( void * ) model, count );
	for( i=0 ; i<count ; ++i ){

		/* iterate through attributes for this item */
		attribute_iter = g_menu_model_iterate_item_attributes( model, i );
		while( g_menu_attribute_iter_get_next( attribute_iter, &attribute_name, &attribute_value )){
			g_debug( "%s: i=%d, attribute_name=%s, attribute_value=%s", thisfn, i, attribute_name, g_variant_get_string( attribute_value, NULL ));
			g_variant_unref( attribute_value );
		}
		g_object_unref( attribute_iter );

		/* iterates through links for this item */
		link_iter = g_menu_model_iterate_item_links( model, i );
		while( g_menu_link_iter_get_next( link_iter, &link_name, &link_model )){
			g_debug( "%s: i=%d, link_name=%s, link_model=%p", thisfn, i, link_name, ( void * ) link_model );
			dump_menu_model( instance, link_model );
			g_object_unref( link_model );
		}
		g_object_unref( link_iter );
	}
}

static GtkWidget *
get_focused_widget( ofaIContext *instance )
{
	static const gchar *thisfn = "ofa_icontext_get_focused_widget";

	if( OFA_ICONTEXT_GET_INTERFACE( instance )->get_focused_widget ){
		return( OFA_ICONTEXT_GET_INTERFACE( instance )->get_focused_widget( instance ));
	}

	g_info( "%s: ofaIContext's %s implementation does not provide 'get_focused_widget()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));

	return( NULL );
}

/*
 * returns the sIContext data structure attached to the instance
 */
static sIContext *
get_instance_data( ofaIContext *instance )
{
	sIContext *sdata;

	sdata = ( sIContext * ) g_object_get_data( G_OBJECT( instance ), ICONTEXT_DATA );

	if( !sdata ){
		sdata = g_new0( sIContext, 1 );
		sdata->menu = g_menu_new();

		g_object_set_data( G_OBJECT( instance ), ICONTEXT_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );

		connect_to_keyboard_event( instance, sdata );
		connect_to_mouse_event( instance, sdata );
	}

	return( sdata );
}

/*
 * From Gtk+ 3 Reference Manual:
 *   This signal gets emitted whenever a widget should pop up a context
 *   menu. This usually happens through the standard key binding
 *   mechanism; by pressing a certain key while a widget is focused,
 *   the user can cause the widget to pop up a menu.
 *   For example, the GtkEntry widget creates a menu with clipboard
 *   commands.
 *
 * From the Popup Menu Migration Checklist:
 *   For the standard key bindings to work, your widget must be able to
 *   take the keyboard focus. In general, widgets should be fully usable
 *   through the keyboard and not just the mouse. The very first step of
 *   this is to ensure that your widget can receive focus, using
 *   gtk_widget_set_can_focus().
 *
 * As a summary, the 'popup-menu' signal is the one which is triggered
 * to open a contextual menu from the keyboard.
 * More than connecting to signal, we check here that the widget can
 * take the focus.
 */
static void
connect_to_keyboard_event( ofaIContext *instance, sIContext *sdata )
{
	static const gchar *thisfn = "ofa_icontext_connect_to_keyboard_event";
	gboolean can_focus;

	can_focus = gtk_widget_get_can_focus( GTK_WIDGET( instance ));

	if( can_focus ){
		g_debug( "%s: widget=%p can get focus: fine",
				thisfn, ( void * ) instance );
	} else {
		g_debug( "%s: widget=%p cannot get focus",
				thisfn, ( void * ) instance );
	}

#if 0
		g_debug( "%s: widget=%p cannot get focus: modifying this",
				thisfn, ( void * ) instance );
		gtk_widget_set_can_focus( GTK_WIDGET( instance ), TRUE );
		can_focus = gtk_widget_get_can_focus( GTK_WIDGET( instance ));
		if( can_focus ){
			g_debug( "%s: widget=%p can now get focus: fine",
					thisfn, ( void * ) instance );
		} else {
			g_debug( "%s: widget=%p still cannot get focus: "
						"the 'popup-menu' signal will not be received",
					thisfn, ( void * ) instance );
		}
	}
#endif

	/* connect to signal anyway */
	g_signal_connect( instance, "popup-menu", G_CALLBACK( on_popup_menu ), sdata );
}

/*
 * From Gtk+ 3 Reference Manual:
 *   The ::button-press-event signal will be emitted when a button
 *   (typically from a mouse) is pressed.
 *   To receive this signal, the GdkWindow associated to the widget
 *   needs to enable the GDK_BUTTON_PRESS_MASK mask.
 *   This signal will be sent to the grab widget if there is one.
 *
 * As a summary, the 'button-press-event' signal is the one which is
 * triggered to open a contextual menu from the mouse.
 * More than connecting to signal, we check here that the widget has
 * the right event mask, and that it can grab take the focus.
 */
static void
connect_to_mouse_event( ofaIContext *instance, sIContext *sdata )
{
	static const gchar *thisfn = "ofa_icontext_connect_to_mouse_event";
	gint mask;
	GtkWidget *widget;

	widget = get_focused_widget( instance );
	g_return_if_fail( widget && GTK_IS_WIDGET( widget ));

	mask = gtk_widget_get_events( widget );

	if( mask & GDK_BUTTON_PRESS_MASK ){
		g_debug( "%s: GDK_BUTTON_PRESS_MASK is set on widget=%p: fine",
				thisfn, ( void * ) instance );
	} else {
		g_debug( "%s: GDK_BUTTON_PRESS_MASK is cleared on widget=%p: modifying this",
				thisfn, ( void * ) instance );
		gtk_widget_add_events( GTK_WIDGET( instance ), GDK_BUTTON_PRESS_MASK );

		mask = gtk_widget_get_events( widget );
		if( mask & GDK_BUTTON_PRESS_MASK ){
			g_debug( "%s: GDK_BUTTON_PRESS_MASK is now set on widget=%p: fine",
					thisfn, ( void * ) instance );
		} else {
			g_debug( "%s: GDK_BUTTON_PRESS_MASK is still cleared on widget=%p: "
						"the 'button-press-event' signal will not be received",
					thisfn, ( void * ) instance );
		}
	}

	/* connect to signal anyway */
	g_signal_connect( widget, "button-press-event", G_CALLBACK( on_button_pressed ), sdata );

#if 0
	/* grab the focus */
	if( gtk_widget_get_can_focus( widget )){
		gtk_widget_grab_focus( widget );
	}
#endif

}

static void
on_instance_finalized( sIContext *sdata, GObject *instance )
{
	static const gchar *thisfn = "ofa_icontext_on_instance_finalized";

	g_debug( "%s: sdata=%p, instance=%p", thisfn, ( void * ) sdata, ( void * ) instance );

	g_clear_object( &sdata->menu );
	if( sdata->popup ){
		gtk_widget_destroy( sdata->popup );
	}

	g_free( sdata );
}

static void
free_actionables( GList **actionables )
{
	g_list_free( *actionables );

	*actionables = NULL;
}
