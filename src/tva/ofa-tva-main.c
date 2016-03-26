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

#include "api/ofa-igetter.h"
#include "api/ofa-itheme-manager.h"

#include "tva/ofa-tva-declare-page.h"
#include "tva/ofa-tva-main.h"
#include "tva/ofa-tva-manage-page.h"

/* a structure which defines the menu items
 * menu items are identified by action_name, which must be linked
 * with action_name.
 */
typedef struct {
	const gchar *action_name;
	const gchar *item_label;
}
	sItemDef;

/* a structure which defines the themes
 * theme identifier is returned by the interface implementer
 */
typedef struct {
	const gchar *action_name;
	const gchar *theme_label;
	GType      (*fntype)( void );
}
	sThemeDef;

static void on_menu_available( GApplication *application, GActionMap *map, const gchar *prefix, void *empty );
static void menu_add_section( GObject *parent, const sItemDef *sitems, const gchar *placeholder );
static void on_theme_available( GApplication *application, ofaIThemeManager *manager, void *empty );
static void on_tva_declare( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_tva_manage( GSimpleAction *action, GVariant *parameter, gpointer user_data );

/* all the actions added for the TVA modules
 */
static const GActionEntry st_win_entries[] = {
		{ "vat-declare",  on_tva_declare,  NULL, NULL, NULL },
		{ "vat-manage",  on_tva_manage,  NULL, NULL, NULL },
};

/* the items respectively added to Operations[2] and References menus
 */
static const sItemDef st_items_ope2[] = {
		{ "vat-declare", N_( "VAT _declarations..." ) },
		{ 0 }
};

static const sItemDef st_items_ref[] = {
		{ "vat-manage", N_( "VAT _forms management..." ) },
		{ 0 }
};

/* the themes which also define the tab titles
 */
static sThemeDef st_theme_defs[] = {
		{ "vat-declare",  N_( "VAT _declarations" ),  ofa_tva_declare_page_get_type },
		{ "vat-manage",  N_( "VAT _forms management" ),  ofa_tva_manage_page_get_type },
		{ 0 }
};

/**
 * ofa_tva_main_signal_connect:
 * @getter: the main #ofaIGetter of the application.
 *
 * Connect to the application signals.
 * This will in particular let us update the application menubar.
 */
void
ofa_tva_main_signal_connect( ofaIGetter *getter )
{
	GApplication *application;

	application = ofa_igetter_get_application( getter );

	g_signal_connect( application, "menu-available", G_CALLBACK( on_menu_available ), NULL );
	g_signal_connect( application, "theme-available", G_CALLBACK( on_theme_available ), NULL );
}

/*
 * the signal is expected to be sent once for each menu map/model defined
 * by the application; this is a good time for the handler to add our own
 * actions
 */
static void
on_menu_available( GApplication *application, GActionMap *map, const gchar *prefix, void *empty )
{
	static const gchar *thisfn = "tva/ofa_tva_main_on_menu_available";

	g_debug( "%s: application=%p, map=%p, prefix=%s, empty=%p",
			thisfn, ( void * ) application, ( void * ) map, prefix, ( void * ) empty );

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
	static const gchar *thisfn = "tva/ofa_tva_main_menu_add_section";
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
on_theme_available( GApplication *application, ofaIThemeManager *manager, void *empty )
{
	static const gchar *thisfn = "tva/ofa_tva_main_on_theme_available";
	guint i;

	g_debug( "%s: application=%p, manager=%p, empty=%p",
			thisfn, ( void * ) application, ( void * ) manager, empty );

	for( i=0 ; st_theme_defs[i].action_name ; ++i ){
		ofa_itheme_manager_define( manager, ( *st_theme_defs[i].fntype )(), st_theme_defs[i].theme_label );
	}
}

static void
on_tva_declare( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "tva/ofa_tva_main_on_tva_declare";
	ofaIThemeManager *manager;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_IGETTER( user_data ));

	manager = ofa_igetter_get_theme_manager( OFA_IGETTER( user_data ));
	ofa_itheme_manager_activate( manager, OFA_TYPE_TVA_DECLARE_PAGE );
}

static void
on_tva_manage( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "tva/ofa_tva_main_on_tva_manage";
	ofaIThemeManager *manager;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_IGETTER( user_data ));

	manager = ofa_igetter_get_theme_manager( OFA_IGETTER( user_data ));
	ofa_itheme_manager_activate( manager, OFA_TYPE_TVA_MANAGE_PAGE );
}
