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

#ifndef __OFA_MAIN_WINDOW_DEF_H__
#define __OFA_MAIN_WINDOW_DEF_H__

/**
 * SECTION: main-window
 * @title: ofaMainWindow
 * @short_description: The Main Window class definition
 * @include: ui/ofa-main-window.h
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
	GtkApplicationWindow  parent;

	/*< private members >*/
	ofaMainWindowPrivate *priv;
}
	ofaMainWindow;

typedef struct {
	/*< public members >*/
	GtkApplicationWindowClass parent;
}
	ofaMainWindowClass;

/**
 * OFA_SIGNAL_ACTION_DOSSIER_OPEN:
 * Signal to be sent to the main window in order to ask for the opening
 * of a dossier. See also the #ofsDossierOpen struct.
 */
#define OFA_SIGNAL_ACTION_DOSSIER_OPEN      "ofa-signal-dossier-open"

/**
 * ofsDossierOpen:
 * @label: the label of the dossier, as in the settings.
 * @account: an account authorized to the dossier.
 * @password: the password of the account.
 *
 * This structure must be allocated by the emitter, and attached to the
 * above signal. It will be freed in the signal cleanup handler.
 */
typedef struct {
	gchar *label;
	gchar *account;
	gchar *password;
}
	ofsDossierOpen;

/**
 * OFA_SIGNAL_ACTION_DOSSIER_PROPERTIES:
 *  Signal to be sent to the main window in order to update the
 *  properties of the currently opened dossier.
 *  use case: DossierNew: update the properties right after having
 *  opened the new dossier.
 */
#define OFA_SIGNAL_ACTION_DOSSIER_PROPERTIES "ofa-signal-dossier-properties"

GType ofa_main_window_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_MAIN_WINDOW_DEF_H__ */
