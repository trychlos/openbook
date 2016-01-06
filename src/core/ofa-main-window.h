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

#ifndef __OFA_MAIN_WINDOW_H__
#define __OFA_MAIN_WINDOW_H__

/**
 * SECTION: main-window
 * @title: ofaMainWindow
 * @short_description: The Main Window class definition
 * @include: core/ofa-main-window.h
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
 * | | | | |             | | | 'main notebook'                | | | | |
 * | | | | |             | | |                                | | | | |
 * | | | | |             | | | see ofa-page.h for a more      | | | | |
 * | | | | |             | | |  complete description of the   | | | | |
 * | | | | |             | | |  ofaPage class behavior        | | | | |
 * | | | | |             | | |                                | | | | |
 * | | | | +-------------+ | +--------------------------------+ | | | |
 * | | | +-----------------+------------------------------------+ | | |
 * | | +----------------------------------------------------------+ | |
 * | +--------------------------------------------------------------+ |
 * +------------------------------------------------------------------+
 */

#include "api/ofa-idbconnect.h"
#include "api/ofa-main-window-def.h"
#include "api/ofa-page-def.h"
#include "api/ofo-dossier-def.h"

#include "ui/ofa-application-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_MAIN_WINDOW                ( ofa_main_window_get_type())
#define OFA_MAIN_WINDOW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MAIN_WINDOW, ofaMainWindow ))
#define OFA_MAIN_WINDOW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MAIN_WINDOW, ofaMainWindowClass ))
#define OFA_IS_MAIN_WINDOW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MAIN_WINDOW ))
#define OFA_IS_MAIN_WINDOW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MAIN_WINDOW ))
#define OFA_MAIN_WINDOW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MAIN_WINDOW, ofaMainWindowClass ))

#if 0
typedef struct _ofaMainWindow               ofaMainWindow;
typedef struct _ofaMainWindowPrivate        ofaMainWindowPrivate;
#endif

struct _ofaMainWindow {
	/*< public members >*/
	GtkApplicationWindow      parent;

	/*< private members >*/
	ofaMainWindowPrivate     *priv;
};

typedef struct {
	/*< public members >*/
	GtkApplicationWindowClass parent;
}
	ofaMainWindowClass;

/**
 * The theme identifiers of the pages of the main notebook
 * sorted in alphabetical order
 */
enum {
	THM_ACCOUNTS = 1,
	THM_BATFILES,
	THM_CLASSES,
	THM_CURRENCIES,
	THM_ENTRIES,
	THM_GUIDED_INPUT,
	THM_LEDGERS,
	THM_OPE_TEMPLATES,
	THM_RATES,
	THM_RECONCIL,
	THM_RENDER_BALANCES,
	THM_RENDER_ACCOUNTS_BOOK,
	THM_RENDER_LEDGERS_BOOK,
	THM_RENDER_RECONCIL,
	THM_SETTLEMENT,
	THM_LAST_THEME
};

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

GType          ofa_main_window_get_type          ( void ) G_GNUC_CONST;

ofaMainWindow *ofa_main_window_new               ( const ofaApplication *application );

ofaPage       *ofa_main_window_activate_theme    ( const ofaMainWindow *window, gint theme_id );

gboolean       ofa_main_window_is_willing_to_quit( const ofaMainWindow *window );

void           ofa_main_window_open_dossier      ( ofaMainWindow *window,
														ofaIDBConnect *connect,
														gboolean remediate );

ofoDossier    *ofa_main_window_get_dossier       ( const ofaMainWindow *window );

void           ofa_main_window_update_title      ( const ofaMainWindow *window );

void           ofa_main_window_close_dossier     ( ofaMainWindow *window );

void           ofa_main_window_warning_no_entry  ( const ofaMainWindow *window );

G_END_DECLS

#endif /* __OFA_MAIN_WINDOW_H__ */
