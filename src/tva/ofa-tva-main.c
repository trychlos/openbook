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

#include "api/ofa-igetter.h"
#include "api/ofa-ipage-manager.h"
#include "api/ofa-isignaler.h"

#include "tva/ofa-tva-form-page.h"
#include "tva/ofa-tva-main.h"
#include "tva/ofa-tva-record-page.h"

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

static void on_menu_available( ofaISignaler *signaler, const gchar *scope, GActionMap *map, ofaIGetter *getter );
static void menu_add_section( GObject *parent, const sItemDef *sitems, const gchar *placeholder );
static void on_page_manager_available( ofaISignaler *signaler, ofaIGetter *getter, void *empty );
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
		{ "vat-declare",  N_( "VAT _declarations" ),  ofa_tva_record_page_get_type },
		{ "vat-manage",  N_( "VAT _forms management" ),  ofa_tva_form_page_get_type },
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
	static const gchar *thisfn = "tva/ofa_tva_main_signal_connect";
	ofaISignaler *signaler;

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	signaler = ofa_igetter_get_signaler( getter );

	g_signal_connect( signaler,
			"ofa-signaler-page-manager-available", G_CALLBACK( on_page_manager_available ), NULL );

	g_signal_connect( signaler,
			"ofa-signaler-menu-available", G_CALLBACK( on_menu_available ), getter );
}

/*
 * the signal is expected to be sent once for each menu map/model defined
 * by the application; this is a good time for the handler to add our own
 * actions
 */
static void
on_menu_available( ofaISignaler *signaler, const gchar *scope, GActionMap *map, ofaIGetter *getter )
{
	static const gchar *thisfn = "tva/ofa_tva_main_on_menu_available";

	g_debug( "%s: signaler=%p, scope=%s, map=%p, getter=%p",
			thisfn, ( void * ) signaler, scope, ( void * ) map, ( void * ) getter );

	if( GTK_IS_APPLICATION_WINDOW( map )){
		g_action_map_add_action_entries(
				G_ACTION_MAP( map ), st_win_entries, G_N_ELEMENTS( st_win_entries ), map );

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
		}
		g_menu_append_section( G_MENU( menu_model ), NULL, G_MENU_MODEL( section ));
		g_object_unref( section );
	}
}

static void
on_page_manager_available( ofaISignaler *signaler, ofaIGetter *getter, void *empty )
{
	static const gchar *thisfn = "tva/ofa_tva_main_on_page_manager_available";
	ofaIPageManager *manager;
	guint i;

	g_debug( "%s: signaler=%p, getter=%p, empty=%p",
			thisfn, ( void * ) signaler, ( void * ) getter, empty );

	manager = ofa_igetter_get_page_manager( getter );

	for( i=0 ; st_theme_defs[i].action_name ; ++i ){
		ofa_ipage_manager_define( manager, ( *st_theme_defs[i].fntype )(), st_theme_defs[i].theme_label );
	}
}

static void
on_tva_declare( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "tva/ofa_tva_main_on_tva_declare";
	ofaIPageManager *manager;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_IGETTER( user_data ));

	manager = ofa_igetter_get_page_manager( OFA_IGETTER( user_data ));
	ofa_ipage_manager_activate( manager, OFA_TYPE_TVA_RECORD_PAGE );
}

static void
on_tva_manage( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "tva/ofa_tva_main_on_tva_manage";
	ofaIPageManager *manager;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_IGETTER( user_data ));

	manager = ofa_igetter_get_page_manager( OFA_IGETTER( user_data ));
	ofa_ipage_manager_activate( manager, OFA_TYPE_TVA_FORM_PAGE );
}
