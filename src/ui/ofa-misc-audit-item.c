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

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "my/my-utils.h"

#include "api/ofa-igetter.h"

#include "ui/ofa-misc-audit-item.h"
#include "ui/ofa-misc-audit-ui.h"

/* a structure which defines the menu items
 * menu items are identified by action_name, which must be linked
 * with action_name.
 */
typedef struct {
	const gchar *action_name;
	const gchar *item_label;
}
	sItemDef;

static void       on_menu_available( GApplication *application, GActionMap *map, const gchar *prefix, void *empty );
static void       menu_add_section( GObject *parent, const sItemDef *sitems, const gchar *placeholder );
static void       on_misc_audit_item( GSimpleAction *action, GVariant *parameter, gpointer user_data );

/* all the actions defined here
 */
static const GActionEntry st_app_entries[] = {
		{ "misc-audit",  on_misc_audit_item,  NULL, NULL, NULL },
};

/* the items to be added to Misc menus
 */
static const sItemDef st_items_misc[] = {
		{ "misc-audit", N_( "DBMS _audit trace..." ) },
		{ 0 }
};

/**
 * ofa_misc_audit_item_signal_connect:
 * @getter: the main #ofaIGetter of the application.
 *
 * Connect to the application signals.
 * This will in particular let us update the application menubar.
 */
void
ofa_misc_audit_item_signal_connect( ofaIGetter *getter )
{
	GApplication *application;

	application = ofa_igetter_get_application( getter );

	g_signal_connect( application, "menu-available", G_CALLBACK( on_menu_available ), NULL );
}

/*
 * The signal is expected to be sent once for each menu map/model defined
 * by the application; this is a good time for the handler to add our own
 * actions.
 *
 * The items are added to each GActionMap, whether these are application
 * or main window.
 */
static void
on_menu_available( GApplication *application, GActionMap *map, const gchar *prefix, void *empty )
{
	static const gchar *thisfn = "ofa_misc_audit_item_on_menu_available";

	g_debug( "%s: application=%p, map=%p, prefix=%s, empty=%p",
			thisfn, ( void * ) application, ( void * ) map, prefix, ( void * ) empty );

	if( !my_collate( prefix, "win" )){

		g_action_map_add_action_entries(
				map, st_app_entries, G_N_ELEMENTS( st_app_entries ), map );

		menu_add_section( G_OBJECT( map ), st_items_misc, "plugins_app_misc" );
	}
}

static void
menu_add_section( GObject *parent, const sItemDef *sitems, const gchar *placeholder )
{
	static const gchar *thisfn = "ofa_misc_audit_item_menu_add_section";
	GMenuModel *menu_model;
    GMenu *section;
    GMenuItem *item;
    gchar *label, *action_name;
    gint i;

	menu_model = ( GMenuModel * ) g_object_get_data( parent, placeholder );
	g_debug( "%s: menu_model=%p", thisfn, ( void * ) menu_model );
	/*
	g_debug( "%s: menu_model=%p (%s), ref_count=%d",
			thisfn, ( void * ) menu_model, G_OBJECT_TYPE_NAME( menu_model ),
			G_OBJECT( menu_model )->ref_count );
			*/

	if( menu_model ){
		section = g_menu_new();
		for( i=0 ; sitems[i].action_name ; ++i ){
			label = g_strdup( sitems[i].item_label );
			action_name = g_strconcat( "win.", sitems[i].action_name, NULL );
			g_menu_insert( section, 0, label, action_name );
			g_free( label );
			g_free( action_name );
			item = g_menu_item_new_section( NULL, G_MENU_MODEL( section ));
			g_menu_item_set_attribute( item, "id", "s", sitems[i].action_name );
			g_menu_append_item( G_MENU( menu_model ), item );
			g_object_unref( item );
		}
		g_object_unref( section );
	}
}

static void
on_misc_audit_item( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_misc_audit_item_on_misc_audit_item";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_IGETTER( user_data ));

	ofa_misc_audit_ui_run( OFA_IGETTER( user_data ));
}
