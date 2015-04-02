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

#ifndef __OPENBOOK_API_MY_WINDOW_H__
#define __OPENBOOK_API_MY_WINDOW_H__

/**
 * SECTION: my_window
 * @short_description: #myWindow class definition.
 * @include: openbook/my-window.h
 *
 * This is a base class for application window toplevels. These may be
 * either GtkDialog-derived or GtkAssistant-derived classes.
 *
 * Goals are:
 * - manages size and position of the identified windows
 * - make available most common data (main window, dossier) to child
 *   classes through protected data members
 * - disconnect signals on finalizing
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_WINDOW                ( my_window_get_type())
#define MY_WINDOW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_WINDOW, myWindow ))
#define MY_WINDOW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_WINDOW, myWindowClass ))
#define MY_IS_WINDOW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_WINDOW ))
#define MY_IS_WINDOW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_WINDOW ))
#define MY_WINDOW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_WINDOW, myWindowClass ))

typedef struct _myWindowProtected     myWindowProtected;
typedef struct _myWindowPrivate       myWindowPrivate;

typedef struct {
	/*< public members >*/
	GObject            parent;

	/*< protected members >*/
	myWindowProtected *prot;

	/*< private members >*/
	myWindowPrivate   *priv;
}
	myWindow;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	myWindowClass;

/**
 * Properties defined by the myWindow class.
 *
 * @MY_PROP_MAIN_WINDOW:      main window of the application
 * @MY_PROP_WINDOW_XML:       path to the xml file which contains the UI description
 * @MY_PROP_WINDOW_NAME:      window toplevel name
 * @MY_PROP_SIZE_POSITION:    whether to manage size and position
 */
#define MY_PROP_MAIN_WINDOW             "my-window-prop-main-window"
#define MY_PROP_WINDOW_XML              "my-window-prop-xml"
#define MY_PROP_WINDOW_NAME             "my-window-prop-name"
#define MY_PROP_SIZE_POSITION           "my-window-prop-size-position"

GType                 my_window_get_type          ( void ) G_GNUC_CONST;

GtkApplicationWindow *my_window_get_main_window   ( const myWindow *window );

const gchar          *my_window_get_name          ( const myWindow *window );

GtkWindow            *my_window_get_toplevel      ( const myWindow *window );

G_END_DECLS

#endif /* __OPENBOOK_API_MY_WINDOW_H__ */
