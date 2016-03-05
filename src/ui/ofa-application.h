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

#ifndef __OFA_APPLICATION_H__
#define __OFA_APPLICATION_H__

/**
 * SECTION: ofaapplication
 * @title: ofaApplication
 * @short_description: The ofaApplication application class definition
 * @include: ui/ofa-application.h
 *
 * #ofaApplication is the main class for the main openbook application.
 *
 * As of v 0.1 (svn commit #3356), the application is not supposed to be
 * unique. Running several instances of the program from the command-line
 * juste create several instances of the application, each one believing
 * it is the primary instance of a new application. Each ofaApplication
 * is considered as a primary instance, thus creating its own ofaMainWindow.
 *
 * [Gtk+3.8]
 * The menubar GtkWidget is handled by GtkApplicationWindow, and is able
 * to rebuid itself, which is fine. But it rebuilds from a
 * menubar_section GMenu, which itself is only built at initialization
 * time. So it appears that it is impossible to replace the menubar with
 * the given API.
 *
 * To display debug messages, run the command:
 *   $ G_MESSAGES_DEBUG=OFA _install/bin/openbook
 *
 * Startup dynamic:
 * [main]
 *  |
 *  +-> appli = ofa_application_new();
 *      |
 *      +-> ofa_application_class_init: klass=0xd884b0
 *          ofa_application_init: self=0xd8a120 (ofaApplication)
 *            ofa_plugin_load_modules:
 *            ofa_plugin_class_init:
 *              ofa_plugin_init: self=0xd93880 (ofaPlugin)
 *              ofa_plugin_init: self=0xd93920 (ofaPlugin)
 *              ofa_plugin_init: self=0xd93a60 (ofaPlugin)
 *  |
 *  +-> ret = ofa_application_run_with_args( appli, argc, argv );
 *      |
 *      +-> ofa_application_run_with_args: application=0xd8a120 (ofaApplication), argc=1
 *            ofa_application_init_i18n: application=0xd8a120
 *            ofa_application_init_gtk_args: application=0xd8a120
 *            ofa_application_manage_options: application=0xd8a120
 *
 *          ofa_application_run_with_args: entering g_application_run
 *            ofa_application_startup: application=0xd8a120
 *               (init here the application menu when there is no dossier)
 *
 *            ofa_application_activate: application=0xd8a120
 *              (instanciate a new empty main window, and gtk_window_present it)
 *              ofa_main_window_new: application=0xd8a120
 *              ofa_main_window_class_init: klass=0xed8000
 *              ofa_main_window_init: self=0xfe43d0 (ofaMainWindow)
 *              ...
 *            ofa_application_activate: main window instanciated at 0xfe43d0
 *
 *   The application sends a 'main-window-created' signal on the
 *   GApplication object right after main window creation.
 *
 * Letting the plugins update the menus
 * ------------------------------------
 *
 * The application defines two menus which are to be displayed depending
 * a dossier is opened, or not:
 *
 *   XML file definition  ofa-app-menubar.ui         ofa-dos-menubar.ui
 *   displayed when       no dossier                 a dossier is opened
 *   initialized in       ofa_application_startup()  ofa_main_window_constructed()
 *   placeholders         plugins_app_dossier        plugins_win_ope1
 *                        plugins_app_misc           plugins_win_ope2
 *                                                   plugins_win_ope3
 *                                                   plugins_win_ope4
 *                                                   plugins_win_print
 *                                                   plugins_win_ref
 *
 * The placeholders must be both defined in the XML definition file,
 * and initialized in the ad-hoc function (ofa_application_startup/
 * ofa_main_window_constructed).
 *
 * Right after menus definition, the application sends a 'menu-definition'
 * signal on the GApplication object. This signal may be handled by the
 * plugins in order to update the menus.
 *
 * Each of the two menus defines various placeholders to let the plugins
 * choose where to add their items. The corresponding menu models are
 * set as data against the GtkApplication (resp. the GtkApplicationWindow),
 * and may thus be retrieved by the plugins via g_object_get_data().
 *
 * The #ofaApplication class implements the #ofaIHubber interface.
 * As long as the code have an access to the application, it may have
 * an access to the main #ofaHub that the application maintains (and
 * so the currently opened dossier).
 */

#include <gtk/gtk.h>

#include "core/ofa-file-dir.h"

#include "ofa-application-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_APPLICATION                ( ofa_application_get_type())
#define OFA_APPLICATION( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_APPLICATION, ofaApplication ))
#define OFA_APPLICATION_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_APPLICATION, ofaApplicationClass ))
#define OFA_IS_APPLICATION( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_APPLICATION ))
#define OFA_IS_APPLICATION_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_APPLICATION ))
#define OFA_APPLICATION_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_APPLICATION, ofaApplicationClass ))

#if 0
typedef struct _ofaApplication              ofaApplication;
typedef struct _ofaApplicationPrivate       ofaApplicationPrivate;
#endif

struct _ofaApplication {
	/*< public members >*/
	GtkApplication      parent;
};

/**
 * ofaApplicationClass:
 */
typedef struct {
	/*< public members >*/
	GtkApplicationClass parent;
}
	ofaApplicationClass;

/**
 * Properties defined by the ofaApplication class.
 *
 * @OFA_PROP_OPTIONS:          array of command-line options descriptions.
 * @OFA_PROP_APPLICATION_NAME: application name.
 * @OFA_PROP_DESCRIPTION:      short description.
 * @OFA_PROP_ICON_NAME:        icon name.
 * @OFA_PROP_HUB:              the main #ofaHub object.
 */
#define OFA_PROP_OPTIONS                "ofa-application-prop-options"
#define OFA_PROP_APPLICATION_NAME       "ofa-application-prop-name"
#define OFA_PROP_DESCRIPTION            "ofa-application-prop-description"
#define OFA_PROP_ICON_NAME              "ofa-application-prop-icon-name"
#define OFA_PROP_HUB                    "ofa-application-prop-hub"

/**
 * ofaExitCode:
 *
 * The code returned by the application.
 *
 * @OFA_EXIT_CODE_OK = 0:         the program has successfully run, and returns zero.
 * @OFA_EXIT_CODE_ARGS = 1:       unable to interpret command-line options
 * @OFA_EXIT_CODE_WINDOW = 2:     unable to create the startup window
 * @OFA_EXIT_CODE_PROGRAM = 3:    general program error code.
 */
typedef enum {
	OFA_EXIT_CODE_OK = 0,
	OFA_EXIT_CODE_ARGS,
	OFA_EXIT_CODE_WINDOW,
	OFA_EXIT_CODE_PROGRAM
}
	ofaExitCode;

GType           ofa_application_get_type      ( void ) G_GNUC_CONST;

ofaApplication *ofa_application_new           ( void );

int             ofa_application_run_with_args ( ofaApplication *application,
													int argc,
													GStrv argv );

GMenuModel     *ofa_application_get_menu_model( const ofaApplication *application );

const gchar    *ofa_application_get_copyright ( const ofaApplication *application );

ofaFileDir     *ofa_application_get_file_dir  ( const ofaApplication *application );

G_END_DECLS

#endif /* __OFA_APPLICATION_H__ */
