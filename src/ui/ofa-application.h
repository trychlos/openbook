/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_APPLICATION                ( ofa_application_get_type())
#define OFA_APPLICATION( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_APPLICATION, ofaApplication ))
#define OFA_APPLICATION_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_APPLICATION, ofaApplicationClass ))
#define OFA_IS_APPLICATION( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_APPLICATION ))
#define OFA_IS_APPLICATION_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_APPLICATION ))
#define OFA_APPLICATION_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_APPLICATION, ofaApplicationClass ))

typedef struct _ofaApplicationPrivate       ofaApplicationPrivate;

typedef struct {
	/*< private >*/
	GtkApplication         parent;
	ofaApplicationPrivate *private;
}
	ofaApplication;

/**
 * ofaApplicationClass:
 */
typedef struct {
	/*< private >*/
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

GType           ofa_application_get_type      ( void );

ofaApplication *ofa_application_new           ( void );

int             ofa_application_run_with_args ( ofaApplication *application, int argc, GStrv argv );

GMenuModel     *ofa_application_get_menu_model( const ofaApplication *application );

G_END_DECLS

#endif /* __OFA_APPLICATION_H__ */
