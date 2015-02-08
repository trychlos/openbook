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

#ifndef __OFA_APPLICATION_DEF_H__
#define __OFA_APPLICATION_DEF_H__

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
	/*< public members >*/
	GtkApplication         parent;

	/*< private members >*/
	ofaApplicationPrivate *priv;
}
	ofaApplication;

/**
 * ofaApplicationClass:
 */
typedef struct {
	/*< public members >*/
	GtkApplicationClass parent;
}
	ofaApplicationClass;

G_END_DECLS

#endif /* __OFA_APPLICATION_DEF_H__ */
