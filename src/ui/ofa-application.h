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
 *
 * $Id$
 */

#ifndef __OFA_APPLICATION_H__
#define __OFA_APPLICATION_H__

/**
 * SECTION: ofa_application
 * @title: ofaApplication
 * @short_description: The ofaApplication application class definition
 * @include: ui/ofa-application.h
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
 */

#include "core/ofa-application-def.h"

G_BEGIN_DECLS

/**
 * Properties defined by the ofaApplication class.
 *
 * @OFA_PROP_OPTIONS:          array of command-line options descriptions.
 * @OFA_PROP_APPLICATION_NAME: application name.
 * @OFA_PROP_DESCRIPTION:      short description.
 * @OFA_PROP_ICON_NAME:        icon name.
 */
#define OFA_PROP_OPTIONS			"ofa-application-prop-options"
#define OFA_PROP_APPLICATION_NAME	"ofa-application-prop-name"
#define OFA_PROP_DESCRIPTION		"ofa-application-prop-description"
#define OFA_PROP_ICON_NAME			"ofa-application-prop-icon-name"

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

int             ofa_application_run_with_args ( ofaApplication *application, int argc, GStrv argv );

GMenuModel     *ofa_application_get_menu_model( const ofaApplication *application );

G_END_DECLS

#endif /* __OFA_APPLICATION_H__ */
