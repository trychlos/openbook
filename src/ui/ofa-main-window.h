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

#ifndef __OFA_MAIN_WINDOW_H__
#define __OFA_MAIN_WINDOW_H__

/**
 * SECTION: main-window
 * @title: ofaMainWindow
 * @short_description: The Main Window class definition
 * @include: ui/ofa-main-window.h
 *
 * This class manages the main window.
 */

#include "ui/ofa-application.h"
#include "ui/ofo-dossier.h"

G_BEGIN_DECLS

#define OFA_TYPE_MAIN_WINDOW                ( ofa_main_window_get_type())
#define OFA_MAIN_WINDOW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MAIN_WINDOW, ofaMainWindow ))
#define OFA_MAIN_WINDOW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MAIN_WINDOW, ofaMainWindowClass ))
#define OFA_IS_MAIN_WINDOW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MAIN_WINDOW ))
#define OFA_IS_MAIN_WINDOW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MAIN_WINDOW ))
#define OFA_MAIN_WINDOW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MAIN_WINDOW, ofaMainWindowClass ))

typedef struct _ofaMainWindowPrivate        ofaMainWindowPrivate;

typedef struct {
	/*< private >*/
	GtkApplicationWindow  parent;
	ofaMainWindowPrivate *private;
}
	ofaMainWindow;

typedef struct _ofaMainWindowClassPrivate   ofaMainWindowClassPrivate;

typedef struct {
	/*< private >*/
	GtkApplicationWindowClass  parent;
	ofaMainWindowClassPrivate *private;
}
	ofaMainWindowClass;

/**
 *  Signal to be sent to the main window in order to ask for the opening
 * of a dossier. See also ofaOpenDossier struct below.
 */
#define MAIN_SIGNAL_OPEN_DOSSIER                 "main-signal-open-dossier"

/**
 * ofaOpenDossier struct
 *
 * This structure should be allocated by the emitter of the signal.
 * The final, cleanup, handler will take care of freeing the data
 * and the structure itself
 */
typedef struct {
	gchar *dossier;
	gchar *host;
	gint   port;
	gchar *socket;
	gchar *dbname;
	gchar *account;
	gchar *password;
}
	ofaOpenDossier;

/* the theme id data is set against each page of the main notebook
 */
#define OFA_DATA_THEME_ID                        "ofa-data-theme-id"

GType          ofa_main_window_get_type          ( void );

ofaMainWindow *ofa_main_window_new               ( const ofaApplication *application );

gboolean       ofa_main_window_is_willing_to_quit( ofaMainWindow *window );

ofoDossier    *ofa_main_window_get_dossier       ( const ofaMainWindow *window );

GtkNotebook   *ofa_main_window_get_notebook      ( const ofaMainWindow *window );

GtkWidget     *ofa_main_window_get_notebook_page ( const ofaMainWindow *window, gint theme );

G_END_DECLS

#endif /* __OFA_MAIN_WINDOW_H__ */
