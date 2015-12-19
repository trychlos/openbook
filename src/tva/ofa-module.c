/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#include "api/ofa-extension.h"

#include "ofa-tva.h"

typedef struct {
	const gchar *id;
	const gchar *label;
}
	sItem;

static void on_tva_declaration( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_tva_definition( GSimpleAction *action, GVariant *parameter, gpointer user_data );

static const GActionEntry st_win_entries[] = {
		{ "tvadecl", on_tva_declaration, NULL, NULL, NULL },
		{ "tvadef",  on_tva_definition,  NULL, NULL, NULL },
};

static const sItem st_items_ope2[] = {
		{ "tvadecl", N_( "TVA declaration" ) },
		{ 0 }
};

static const sItem st_items_ref[] = {
		{ "tvadef", N_( "TVA definitions" ) },
		{ 0 }
};

/* the count of GType types provided by this extension
 * each new GType type must
 * - be registered in ofa_extension_startup()
 * - be addressed in ofa_extension_list_types().
 */
#define OFA_TYPES_COUNT	1

static void on_menu_defined( GApplication *application, GActionMap *map, void *empty );
static void menu_add_section( GObject *parent, const sItem *sitems, const gchar *placeholder );
static void on_main_window_created( GApplication *application, GtkApplicationWindow *window, void *empty );

/*
 * ofa_extension_startup:
 *
 * mandatory starting with API v. 1.
 */
gboolean
ofa_extension_startup( GTypeModule *module, GApplication *application )
{
	static const gchar *thisfn = "tva/ofa_module_ofa_extension_startup";

	g_debug( "%s: module=%p, application=%p", thisfn, ( void * ) module, ( void * ) application );

	ofa_tva_register_type( module );

	g_signal_connect( application, "menu-defined", G_CALLBACK( on_menu_defined ), NULL );
	g_signal_connect( application, "main-window-created", G_CALLBACK( on_main_window_created ), NULL );

	return( TRUE );
}

/*
 * ofa_extension_get_api_version:
 *
 * optional, defaults to 1.
 */
guint
ofa_extension_get_api_version( void )
{
	static const gchar *thisfn = "tva/ofa_module_ofa_extension_get_api_version";
	guint version;

	version = 1;

	g_debug( "%s: version=%d", thisfn, version );

	return( version );
}

/*
 * ofa_extension_get_name:
 *
 * optional, defaults to NULL.
 */
const gchar *
ofa_extension_get_name( void )
{
	return( "TVA operations management" );
}

/*
 * ofa_extension_get_version_number:
 *
 * optional, defaults to NULL.
 */
const gchar *
ofa_extension_get_version_number( void )
{
	return( PACKAGE_VERSION );
}

/*
 * ofa_extension_list_types:
 *
 * mandatory starting with v. 1.
 */
guint
ofa_extension_list_types( const GType **types )
{
	static const gchar *thisfn = "tva/ofa_module_ofa_extension_list_types";
	static GType types_list [1+OFA_TYPES_COUNT];

	g_debug( "%s: types=%p", thisfn, ( void * ) types );

	types_list[0] = OFA_TYPE_TVA;

	types_list[OFA_TYPES_COUNT] = 0;
	*types = types_list;

	return( OFA_TYPES_COUNT );
}

/*
 * ofa_extension_shutdown:
 *
 * mandatory starting with v. 1.
 */
void
ofa_extension_shutdown( void )
{
	static const gchar *thisfn = "tva/ofa_module_ofa_extension_shutdown";

	g_debug( "%s", thisfn );
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
menu_add_section( GObject *parent, const sItem *sitems, const gchar *placeholder )
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
		for( i=0 ; sitems[i].id ; ++i ){
			label = g_strdup( sitems[i].label );
			action_name = g_strconcat( "win.", sitems[i].id, NULL );
			g_menu_insert( section, 0, label, action_name );
			g_free( label );
			g_free( action_name );
			item = g_menu_item_new_section( NULL, G_MENU_MODEL( section ));
			g_menu_item_set_attribute( item, "id", "s", sitems[i].id );
			g_menu_append_item( G_MENU( menu_model ), item );
			g_object_unref( item );
		}
		g_object_unref( section );
	}
}

static void
on_main_window_created( GApplication *application, GtkApplicationWindow *window, void *empty )
{
	g_debug( "tva/ofa-module/on_main_window_created: application=%p, window=%p, empty=%p",
			( void * ) application, ( void * ) window, empty );
}

static void
on_tva_declaration( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "tva/ofa-module/on_tva_declaration";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && GTK_IS_APPLICATION_WINDOW( user_data ));

	//ofa_about_run( priv->main_window );
}

static void
on_tva_definition( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "tva/ofa-module/on_tva_definition";

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && GTK_IS_APPLICATION_WINDOW( user_data ));

	//ofa_about_run( priv->main_window );
}
