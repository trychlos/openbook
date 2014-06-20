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
 *
 * Structure of the main window:
 * +------------------------------------------------------------------+
 * | ofaMainWindow (derived from GtkApplicationWindow)                |
 * | +--------------------------------------------------------------+ |
 * | | GtkGrid                                                      | |
 * | | +----------------------------------------------------------+ | |
 * | | | GtkMenuBar                                               | | |
 * | | +----------------------------------------------------------+ | |
 * | | | +-----------------+------------------------------------+ | | |
 * | | | | GtkPaned        |                                    | | | |
 * | | | | +-------------+ | +--------------------------------+ | | | |
 * | | | | | GtkTreeView | | | GtkNotebook                    | | | | |
 * | | | | |             | | |                                | | | | |
 * | | | | |             | | | where each page is a GtkGrid   | | | | |
 * | | | | |             | | |  which is handled by an        | | | | |
 * | | | | |             | | |  ofaMainPage                   | | | | |
 * | | | | |             | | |                                | | | | |
 * | | | | |             | | | see ofa-main-page.h for a more | | | | |
 * | | | | |             | | |  complete description of the   | | | | |
 * | | | | |             | | |  ofaMainPage class behavior    | | | | |
 * | | | | |             | | |                                | | | | |
 * | | | | +-------------+ | +--------------------------------+ | | | |
 * | | | +-----------------+------------------------------------+ | | |
 * | | +----------------------------------------------------------+ | |
 * | +--------------------------------------------------------------+ |
 * +------------------------------------------------------------------+
 */

#include "ui/ofa-application.h"
#include "ui/ofa-main-page-def.h"
#include "ui/ofa-main-window-def.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

/**
 * The theme identifiers of the pages of the main notebook
 */
enum {
	THM_ACCOUNTS = 1,
	THM_DEVISES,
	THM_JOURNALS,
	THM_MODELS,
	THM_TAUX,
	THM_CONCIL,
	THM_CLASSES,
	THM_BATFILES,
	THM_GUIDED_INPUT,
	THM_VIEW_ENTRIES
};

/**
 *  Signal to be sent to the main window in order to ask for the opening
 * of a dossier. See also the #ofaOpenDossier struct.
 */
#define OFA_SIGNAL_OPEN_DOSSIER                  "ofa-signal-open-dossier"

ofaMainWindow *ofa_main_window_new               ( const ofaApplication *application );

ofaMainPage   *ofa_main_window_activate_theme    ( ofaMainWindow *window, gint theme_id );

gboolean       ofa_main_window_is_willing_to_quit( ofaMainWindow *window );

ofoDossier    *ofa_main_window_get_dossier       ( const ofaMainWindow *window );

G_END_DECLS

#endif /* __OFA_MAIN_WINDOW_H__ */
