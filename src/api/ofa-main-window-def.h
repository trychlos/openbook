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

#ifndef __OPENBOOK_API_OFA_MAIN_WINDOW_DEF_H__
#define __OPENBOOK_API_OFA_MAIN_WINDOW_DEF_H__

/**
 * SECTION: main-window
 * @title: ofaMainWindow
 * @short_description: The Main Window class definition
 * @include: core/ofa-main-window.h
 *
 * This class manages the main window.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_MAIN_WINDOW                ( ofa_main_window_get_type())
#define OFA_MAIN_WINDOW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MAIN_WINDOW, ofaMainWindow ))
#define OFA_MAIN_WINDOW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MAIN_WINDOW, ofaMainWindowClass ))
#define OFA_IS_MAIN_WINDOW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MAIN_WINDOW ))
#define OFA_IS_MAIN_WINDOW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MAIN_WINDOW ))
#define OFA_MAIN_WINDOW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MAIN_WINDOW, ofaMainWindowClass ))

typedef struct _ofaMainWindowPrivate        ofaMainWindowPrivate;

typedef struct {
	/*< public members >*/
	GtkApplicationWindow      parent;

	/*< private members >*/
	ofaMainWindowPrivate     *priv;
}
	ofaMainWindow;

typedef struct {
	/*< public members >*/
	GtkApplicationWindowClass parent;
}
	ofaMainWindowClass;

/**
 * OFA_SIGNAL_DOSSIER_OPEN:
 * Action signal to be sent to the main window in order to ask for the
 * opening of a dossier.
 * Args:
 * - a #ofaIDBConnect opened connection on the dossier
 * - whether to remediate the settings.
 */
#define OFA_SIGNAL_DOSSIER_OPEN         "ofa-dossier-open"

/**
 * OFA_SIGNAL_DOSSIER_PROPERTIES:
 *  Action signal to be sent to the main window in order to update the
 *  properties of the currently opened dossier.
 *  Args: none.
 *  Use case: DossierNew: display the properties right after having
 *  opened the new dossier.
 */
#define OFA_SIGNAL_DOSSIER_PROPERTIES   "ofa-dossier-properties"

/**
 * OFA_SIGNAL_DOSSIER_OPENED:
 *  Information signal sent on the main window when a dossier has been.
 *  opened.
 *  Args:
 *  - the #ofoDossier.
 */
#define OFA_SIGNAL_DOSSIER_OPENED       "ofa-dossier-opened"

GType ofa_main_window_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_MAIN_WINDOW_DEF_H__ */
