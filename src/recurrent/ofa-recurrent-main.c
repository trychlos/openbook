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

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "my/my-utils.h"

#include "api/ofa-igetter.h"
#include "api/ofa-ipage-manager.h"

#include "recurrent/ofa-rec-period-page.h"
#include "recurrent/ofa-recurrent-main.h"
#include "recurrent/ofa-recurrent-model-page.h"
#include "recurrent/ofa-recurrent-run-page.h"

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
 */
typedef struct {
	const gchar *action_name;
	const gchar *theme_label;
	GType      (*fntype)( void );
}
	sThemeDef;

static void on_menu_available( GApplication *application, GActionMap *map, const gchar *prefix, void *empty );
static void menu_add_section( GObject *parent, const sItemDef *sitems, const gchar *placeholder );
static void on_theme_available( ofaIGetter *getter, ofaIPageManager *manager, void *empty );
static void on_rec_period( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_recurrent_run( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_recurrent_manage( GSimpleAction *action, GVariant *parameter, gpointer user_data );

/* all the actions added for the Recurrent modules
 */
static const GActionEntry st_win_entries[] = {
		{ "rec-period",  on_rec_period,  NULL, NULL, NULL },
		{ "recurrent-run",  on_recurrent_run,  NULL, NULL, NULL },
		{ "recurrent-define",  on_recurrent_manage,  NULL, NULL, NULL },
};

/* the items respectively added to Operations[2] and References menus
 */
static const sItemDef st_items_ope2[] = {
		{ "recurrent-run", N_( "_Recurrent operations validation..." ) },
		{ 0 }
};

static const sItemDef st_items_ref[] = {
		{ "recurrent-define", N_( "_Recurrent models management..." ) },
		{ "rec-period",       N_( "Recurrent _periodicities..." ) },
		{ 0 }
};

/* the themes which also define the tab titles
 */
static sThemeDef st_theme_defs[] = {
		{ "rec-period",       N_( "_Recurrent periodicities" ),         ofa_rec_period_page_get_type },
		{ "recurrent-run",    N_( "_Recurrent operations validation" ), ofa_recurrent_run_page_get_type },
		{ "recurrent-define", N_( "_Recurrent models management" ),     ofa_recurrent_model_page_get_type },
		{ 0 }
};

/**
 * ofa_recurrent_main_signal_connect:
 * @getter: the main #ofaIGetter of the application.
 *
 * Connect to the ofaIGetter signals.
 * This will in particular let us update the application menubar.
 */
void
ofa_recurrent_main_signal_connect( ofaIGetter *getter )
{
	GApplication *application;

	application = ofa_igetter_get_application( getter );

	g_signal_connect( application, "theme-available", G_CALLBACK( on_theme_available ), NULL );
	g_signal_connect( application, "menu-available", G_CALLBACK( on_menu_available ), NULL );
}

/*
 * the signal is expected to be sent once for each menu map/model defined
 * by the application; this is a good time for the handler to add our own
 * actions
 */
static void
on_menu_available( GApplication *application, GActionMap *map, const gchar *prefix, void *empty )
{
	static const gchar *thisfn = "recurrent/ofa_recurrent_main_on_menu_available";

	g_debug( "%s: application=%p, map=%p, prefix=%s, empty=%p",
			thisfn, ( void * ) application, ( void * ) map, prefix, ( void * ) empty );

	if( !my_collate( prefix, "win" )){
		g_action_map_add_action_entries(
				map, st_win_entries, G_N_ELEMENTS( st_win_entries ), map );

		menu_add_section( G_OBJECT( map ), st_items_ope2, "plugins_win_ope2" );
		menu_add_section( G_OBJECT( map ), st_items_ref, "plugins_win_ref" );
	}
}

static void
menu_add_section( GObject *parent, const sItemDef *sitems, const gchar *placeholder )
{
	static const gchar *thisfn = "recurrent/ofa_recurrent_main_menu_add_section";
	GMenuModel *menu_model;
    GMenu *section;
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
			g_menu_insert( section, i, label, action_name );
			g_free( label );
			g_free( action_name );
		}
		g_menu_append_section( G_MENU( menu_model ), NULL, G_MENU_MODEL( section ));
		g_object_unref( section );
	}
}

static void
on_theme_available( ofaIGetter *getter, ofaIPageManager *manager, void *empty )
{
	static const gchar *thisfn = "recurrent/ofa_recurrent_main_on_theme_available";
	guint i;

	g_debug( "%s: getter=%p, manager=%p, empty=%p",
			thisfn, ( void * ) getter, ( void * ) manager, empty );

	for( i=0 ; st_theme_defs[i].action_name ; ++i ){
		ofa_ipage_manager_define( manager, ( *st_theme_defs[i].fntype )(), st_theme_defs[i].theme_label );
	}
}

static void
on_rec_period( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "recurrent/ofa_recurrent_main_on_rec_period";
	ofaIPageManager *manager;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_IGETTER( user_data ));

	manager = ofa_igetter_get_theme_manager( OFA_IGETTER( user_data ));
	ofa_ipage_manager_activate( manager, OFA_TYPE_REC_PERIOD_PAGE );
}

static void
on_recurrent_run( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "recurrent/ofa_recurrent_main_on_recurrent_run";
	ofaIPageManager *manager;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_IGETTER( user_data ));

	manager = ofa_igetter_get_theme_manager( OFA_IGETTER( user_data ));
	ofa_ipage_manager_activate( manager, OFA_TYPE_RECURRENT_RUN_PAGE );
}

static void
on_recurrent_manage( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "recurrent/ofa_recurrent_main_on_recurrent_manage";
	ofaIPageManager *manager;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_IGETTER( user_data ));

	manager = ofa_igetter_get_theme_manager( OFA_IGETTER( user_data ));
	ofa_ipage_manager_activate( manager, OFA_TYPE_RECURRENT_MODEL_PAGE );
}
