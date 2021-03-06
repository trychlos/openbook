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

#include "api/ofa-idbconnect-def.h"
#include "api/ofa-igetter-def.h"
#include "api/ofa-page-def.h"
#include "api/ofo-dossier-def.h"

#include "ui/ofa-application-def.h"
#include "ui/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_MAIN_WINDOW                ( ofa_main_window_get_type())
#define OFA_MAIN_WINDOW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MAIN_WINDOW, ofaMainWindow ))
#define OFA_MAIN_WINDOW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MAIN_WINDOW, ofaMainWindowClass ))
#define OFA_IS_MAIN_WINDOW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MAIN_WINDOW ))
#define OFA_IS_MAIN_WINDOW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MAIN_WINDOW ))
#define OFA_MAIN_WINDOW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MAIN_WINDOW, ofaMainWindowClass ))

#if 0
typedef struct _ofaMainWindow               ofaMainWindow;
#endif

struct _ofaMainWindow {
	/*< public members >*/
	GtkApplicationWindow      parent;
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
										/* have buttons box */
enum {
	THM_ACCOUNTS = 1,					/*       yes        */
	THM_BATFILES,						/*       yes        */
	THM_CLASSES,						/*       yes        */
	THM_CURRENCIES,						/*       yes        */
	THM_ENTRIES,						/*          no      */
	THM_GUIDED_INPUT,					/*          no      */
	THM_LEDGERS,						/*       yes        */
	THM_OPE_TEMPLATES,					/*       yes        */
	THM_RATES,							/*       yes        */
	THM_RECONCIL,						/*          no      */
	THM_RENDER_BALANCES,				/*          no      */
	THM_RENDER_ACCOUNTS_BOOK,			/*          no      */
	THM_RENDER_LEDGERS_BOOK,			/*          no      */
	THM_RENDER_LEDGERS_SUMMARY,			/*          no      */
	THM_RENDER_RECONCIL,				/*          no      */
	THM_SETTLEMENT,						/*          no      */
	THM_LAST_THEME
};

GType          ofa_main_window_get_type             ( void ) G_GNUC_CONST;

ofaMainWindow *ofa_main_window_new                  ( ofaIGetter *getter );

gboolean       ofa_main_window_is_willing_to_quit   ( const ofaMainWindow *window );

void           ofa_main_window_dossier_close_windows( ofaMainWindow *window );

void           ofa_main_window_dossier_backup       ( ofaMainWindow *window );

void           ofa_main_window_dossier_properties   ( ofaMainWindow *window );

void           ofa_main_window_dossier_apply_actions( ofaMainWindow *window );

void           ofa_main_window_warning_no_entry     ( const ofaMainWindow *window );

G_END_DECLS

#endif /* __OFA_MAIN_WINDOW_H__ */
