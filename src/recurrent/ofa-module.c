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

#include <gtk/gtk.h>

#include "api/ofa-extension.h"

#include "ofa-recurrent.h"

/* the count of GType types provided by this extension
 * each new GType type must
 * - be registered in ofa_extension_startup()
 * - be addressed in ofa_extension_list_types().
 */
#define OFA_TYPES_COUNT	1

static void on_main_window_created( GApplication *application, GtkApplicationWindow *window, void *empty );

/*
 * ofa_extension_startup:
 *
 * mandatory starting with API v. 1.
 */
gboolean
ofa_extension_startup( GTypeModule *module, GApplication *application )
{
	static const gchar *thisfn = "recurrent/ofa_module_ofa_extension_startup";

	g_debug( "%s: module=%p, application=%p", thisfn, ( void * ) module, ( void * ) application );

	ofa_recurrent_register_type( module );

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
	static const gchar *thisfn = "recurrent/ofa_module_ofa_extension_get_api_version";
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
	return( "Recurrent operations management v1.2015" );
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
	static const gchar *thisfn = "recurrent/ofa_module_ofa_extension_list_types";
	static GType types_list [1+OFA_TYPES_COUNT];

	g_debug( "%s: types=%p", thisfn, ( void * ) types );

	types_list[0] = OFA_TYPE_RECURRENT;

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
	static const gchar *thisfn = "recurrent/ofa_module_ofa_extension_shutdown";

	g_debug( "%s", thisfn );
}

/*
 * sequence:
(openbook:12644): OFA-DEBUG: my_utils_action_enable: map=0x6d7180, action=0x6d70b0, name=open, enable=True
(openbook:12644): OFA-DEBUG: ofa_application_activate: application=0x6d7180
(openbook:12644): OFA-DEBUG: ofa_main_window_new: application=0x6d7180
(openbook:12644): OFA-DEBUG: ofa_main_window_class_init: klass=0x803600
(openbook:12644): OFA-DEBUG: ofa_main_window_init: self=0x884340 (ofaMainWindow)
(openbook:12644): OFA-DEBUG: ofa_main_instance_constructed: instance=0x884340 (ofaMainWindow)
(openbook:12644): OFA-DEBUG: my_utils_window_restore_position: name=MainWindow, x=14, y=31, width=1350, height=590
(openbook:12644): OFA-DEBUG: recurrent/ofa-module/on_main_window_created: application=0x6d7180, window=0x884340, empty=(nil)
(openbook:12644): OFA-DEBUG: ofa_main_window_extract_accels: model=0x765ac0: found accel at i=0: <Control>n
(openbook:12644): OFA-DEBUG: ofa_main_window_extract_accels: model=0x765ac0: found accel at i=1: <Control>o
(openbook:12644): OFA-DEBUG: ofa_main_window_extract_accels: model=0x839980: found accel at i=0: <Control>q
(openbook:12644): OFA-DEBUG: set_menubar: model=0x764d60 (GMenu), menubar=0x915200, grid=0x724490 (GtkGrid)
(openbook:12644): OFA-DEBUG: ofa_application_activate: main window instanciated at 0x884340
 */
static void
on_main_window_created( GApplication *application, GtkApplicationWindow *window, void *empty )
{
	g_debug( "recurrent/ofa-module/on_main_window_created: application=%p, window=%p, empty=%p",
			( void * ) application, ( void * ) window, empty );
}
