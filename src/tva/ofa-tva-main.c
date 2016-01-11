/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
 *
 * Open Freelance Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Freelance Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Freelance Accounting; see the file COPYING. If not,
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

#include "tva/ofa-tva-declare-page.h"
#include "tva/ofa-tva-main.h"
#include "tva/ofa-tva-manage-page.h"

/* a structure which defines the menu items
 * menu items are identified by item_name, which must be linked
 * with action_name.
 */
typedef struct {
	const gchar *item_name;
	const gchar *item_label;
}
	sItemDef;

/* a structure which defines the themes
 * theme identifier is returned by the interface implementer
 */
typedef struct {
	const gchar *action_name;
	const gchar *theme_name;
	GType      (*fntype)( void );
	gboolean     with_entries;
	guint        theme_id;
}
	sThemeDef;

static void on_menu_defined( GApplication *application, GActionMap *map, void *empty );
static void menu_add_section( GObject *parent, const sItemDef *sitems, const gchar *placeholder );
static void on_main_window_created( GApplication *application, GtkApplicationWindow *window, void *empty );
static void on_tva_declare( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_tva_manage( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void activate_theme( GtkApplicationWindow *window, const gchar *action_name );

/* all the actions added for the TVA modules
 */
static const GActionEntry st_win_entries[] = {
		{ "tvadeclare",  on_tva_declare,  NULL, NULL, NULL },
		{ "tvamanage",  on_tva_manage,  NULL, NULL, NULL },
};

/* the items respectively added to Operations[2] and References menus
 */
static const sItemDef st_items_ope2[] = {
		{ "tvadeclare", N_( "TVA _declaration..." ) },
		{ 0 }
};

static const sItemDef st_items_ref[] = {
		{ "tvamanage", N_( "TVA _form management" ) },
		{ 0 }
};

static sThemeDef st_theme_defs[] = {
		{ "tvadeclare",  N_( "TVA _declaration" ),  ofa_tva_declare_page_get_type, FALSE, 0 },
		{ "tvamanage",  N_( "TVA _form management" ),  ofa_tva_manage_page_get_type, FALSE, 0 },
		{ 0 }
};

/**
 * ofa_tva_main_signal_connect:
 * @application: the #GApplication provided at extension startup time.
 *
 * Connect to the application signals.
 * This will in particular let us update the application menubar.
 */
void
ofa_tva_main_signal_connect( GApplication *application )
{
	g_signal_connect( application, "menu-defined", G_CALLBACK( on_menu_defined ), NULL );
	g_signal_connect( application, "main-window-created", G_CALLBACK( on_main_window_created ), NULL );
}

/*
 * the signal is expected to be sent once for each menu map/model defined
 * by the application; this is a good time for the handler to add our own
 * actions
 */
static void
on_menu_defined( GApplication *application, GActionMap *map, void *empty )
{
	static const gchar *thisfn = "tva/ofa-module/on_menu_defined";

	g_debug( "%s: application=%p, map=%p, empty=%p",
			thisfn, ( void * ) application, ( void * ) map, ( void * ) empty );

	if( GTK_IS_APPLICATION_WINDOW( map )){
		g_action_map_add_action_entries(
				map, st_win_entries, G_N_ELEMENTS( st_win_entries ), map );

		menu_add_section( G_OBJECT( map ), st_items_ope2, "plugins_win_ope2" );
		menu_add_section( G_OBJECT( map ), st_items_ref, "plugins_win_ref" );
	}
}

static void
menu_add_section( GObject *parent, const sItemDef *sitems, const gchar *placeholder )
{
	static const gchar *thisfn = "tva/ofa-module/menu_add_section";
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
		for( i=0 ; sitems[i].item_name ; ++i ){
			label = g_strdup( sitems[i].item_label );
			action_name = g_strconcat( "win.", sitems[i].item_name, NULL );
			g_menu_insert( section, 0, label, action_name );
			g_free( label );
			g_free( action_name );
			item = g_menu_item_new_section( NULL, G_MENU_MODEL( section ));
			g_menu_item_set_attribute( item, "id", "s", sitems[i].item_name );
			g_menu_append_item( G_MENU( menu_model ), item );
			g_object_unref( item );
		}
		g_object_unref( section );
	}
}

static void
on_main_window_created( GApplication *application, GtkApplicationWindow *window, void *empty )
{
	static const gchar *thisfn = "tva/ofa-module/on_main_window_created";
	guint i;

	g_debug( "%s: application=%p, window=%p, empty=%p",
			thisfn, ( void * ) application, ( void * ) window, empty );

	for( i=0 ; st_theme_defs[i].action_name ; ++i ){
		g_signal_emit_by_name( window, "add-theme",
				st_theme_defs[i].theme_name,
				st_theme_defs[i].fntype,
				st_theme_defs[i].with_entries,
				&st_theme_defs[i].theme_id );
		g_debug( "%s: theme_id=%u", thisfn, st_theme_defs[i].theme_id );
	}
}

static void
on_tva_declare( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "tva/ofa-module/on_tva_declare";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && GTK_IS_APPLICATION_WINDOW( user_data ));

	activate_theme( GTK_APPLICATION_WINDOW( user_data ), "tvadeclare" );
}

static void
on_tva_manage( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "tva/ofa-module/on_tva_manage";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && GTK_IS_APPLICATION_WINDOW( user_data ));

	activate_theme( GTK_APPLICATION_WINDOW( user_data ), "tvamanage" );
}

static void
activate_theme( GtkApplicationWindow *window, const gchar *action_name )
{
	guint theme_id;

	theme_id = ofa_tva_main_get_theme( action_name );
	if( theme_id ){
		g_signal_emit_by_name( window, "activate-theme", theme_id );
	}
}

/**
 * ofa_tva_module_get_theme:
 */
guint
ofa_tva_main_get_theme( const gchar *action_name )
{
	guint i;

	for( i=0 ; st_theme_defs[i].action_name ; ++i ){
		if( !g_utf8_collate( st_theme_defs[i].action_name, action_name )){
			return( st_theme_defs[i].theme_id );
		}
	}

	return( 0 );
}
