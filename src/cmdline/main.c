/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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
 *
 * To display debug messages, run the command:
 *   $ G_MESSAGES_DEBUG=OFA _install/bin/openbook
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*
 * A popup menu with toggle items which are not "app" nor "win"
 */
#include <glib/gi18n.h>
#include <gtk/gtk.h>

typedef struct {
	const gchar *name;
	const gchar *label;
	gboolean     def_value;
}
	sDefines;

static const sDefines st_items[] = {
	{ "dope", N_( "_Operation date" ), TRUE, },
	{ "deff", N_( "_Effect date" ),    FALSE },
	{ NULL }
};

static const gchar *st_prefix = "page";

static void
on_action_activated( GAction *action, GVariant *parameter, void *empty )
{

}

static void
on_button_toggled( GtkToggleButton *button, void *empty )
{
	static gboolean opened = FALSE;

	opened = !opened;
}

int
main( int argc, char *argv[] )
{
	GtkWindow *window;
	GtkWidget *button;
	GMenu *menu;
	GMenuItem *item;
	GSimpleAction *action;
	GSimpleActionGroup *group;
	gchar *action_name;

	gtk_init( &argc, &argv );

	window = GTK_WINDOW( gtk_window_new( GTK_WINDOW_TOPLEVEL ));
	gtk_window_set_title( window, "Openbook [Test] popup menu" );
	gtk_window_set_default_size( window, 600, 400 );
	g_signal_connect_swapped( G_OBJECT( window ), "destroy", G_CALLBACK( gtk_main_quit ), NULL );

	button = gtk_menu_button_new();
	gtk_container_add( GTK_CONTAINER( window ), button );
	g_signal_connect( button, "toggled", G_CALLBACK( on_button_toggled ), NULL );

	/* create the action group
	 * - creating the action group and attaching it to the widget before
	 *   creating the menu or after does not change anything
	 * - have a prefix or not does not change anything
	 * - have a connected callback or not does not change anything */
	group = g_simple_action_group_new();
	for( gint i=0 ; st_items[i].name ; ++i ){
		action_name = g_strdup_printf( "%s.%s", st_prefix, st_items[i].name );
		/*action = g_simple_action_new_stateful( action_name, NULL, g_variant_new_boolean( st_items[i].def_value ));*/
		action = g_simple_action_new_stateful( st_items[i].name, NULL, g_variant_new_boolean( st_items[i].def_value ));
		g_signal_connect( action, "activate", G_CALLBACK( on_action_activated ), NULL );
		g_action_map_add_action( G_ACTION_MAP( group ), G_ACTION( action ));
		g_free( action_name );
		g_object_unref( action );
	}
	gtk_widget_insert_action_group( button, st_prefix, G_ACTION_GROUP( group ));

	/* create the menu */
	gtk_menu_button_set_use_popover( GTK_MENU_BUTTON( button ), FALSE );
	menu = g_menu_new();
	for( gint i=0 ; st_items[i].name ; ++i ){
		action_name = g_strdup_printf( "%s.%s", st_prefix, st_items[i].name );
		/*item = g_menu_item_new( st_items[i].label, st_items[i].name );*/
		item = g_menu_item_new( st_items[i].label, action_name );
		g_menu_append_item( menu, item );
		g_free( action_name );
	}
	gtk_menu_button_set_menu_model( GTK_MENU_BUTTON( button ), G_MENU_MODEL( menu ));

	gtk_widget_show_all( GTK_WIDGET( window ));
	gtk_main();

	return 0;
}
