/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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
 *      |   ofa_application_init: self=0xd8a120 (ofaApplication)
 *      |
 *      +-> ofa_hub_new
 *            ofa_extender_collection_new
 *              ofa_extender_module_class_init:
 *                ofa_extender_module_init: self=0xd93880 (ofaPlugin)
 *                ofa_extender_module_init: self=0xd93920 (ofaPlugin)
 *                ofa_extender_module_init: self=0xd93a60 (ofaPlugin)
 *  |
 *  +-> ret = ofa_application_run_with_args( appli, argc, argv );
 *      |
 *      +-> ofa_application_run_with_args: application=0xd8a120 (ofaApplication), argc=1
 *            ofa_application_init_i18n: application=0xd8a120
 *            ofa_application_init_gtk_args: application=0xd8a120
 *            ofa_application_manage_options: application=0xd8a120
 *
 *          ofa_application_run_with_args: entering g_application_run
 *
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
 *   The application sends a 'theme-available' signal on the
 *   GApplication object right after theme manager availability.
 *
 * Dynamic User Interface
 * ----------------------
 * Every one is able to add items to the displayed menubars, to add
 * displayed pages or new dialogs, and so on.
 *
 * From the menu point of view:
 *
 * - the 'ofaISignaler::ofa-signaler-menu-available' signal is sent each
 *   time a #GActionMap has successfully loaded a menu from its XML
 *   definition.
 *
 *   The menus XML definitions are tagged, at each submenu, at each
 *   section and every item is itself identifier. All dynamically
 *   loadable modules (aka plugins) should find a place for their
 *   specific needs.
 *
 *   As of v0.66, the application provides two different main menus:
 *   - one on 'app' scope when no dossier is opened
 *   - one on 'win' scope when a dossier is opened.
 *
 * - the 'ofaISignaler::ofa-signaler-page-manager-available' signal is
 *   sent at the end of the primary initialization of the main window;
 *   it means the IPageManager is ready to register new themes.
 */

#include <gtk/gtk.h>

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
 * The code returned by the application.
 *
 * @OFA_EXIT_CODE_OK = 0:         the program has successfully run, and returns zero.
 * @OFA_EXIT_CODE_ARGS = 1:       unable to interpret command-line options
 * @OFA_EXIT_CODE_WINDOW = 2:     unable to create the startup window
 * @OFA_EXIT_CODE_PROGRAM = 3:    general program error code.
 */
enum {
	OFA_EXIT_CODE_OK = 0,
	OFA_EXIT_CODE_ARGS,
	OFA_EXIT_CODE_WINDOW,
	OFA_EXIT_CODE_PROGRAM
};

GType           ofa_application_get_type     ( void ) G_GNUC_CONST;

ofaApplication *ofa_application_new          ( void );

int             ofa_application_run_with_args( ofaApplication *application,
													int argc,
													GStrv argv );

G_END_DECLS

#endif /* __OFA_APPLICATION_H__ */
