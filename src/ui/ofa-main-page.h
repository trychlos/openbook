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

#ifndef __OFA_MAIN_PAGE_H__
#define __OFA_MAIN_PAGE_H__

/**
 * SECTION: ofa_main_page
 * @short_description: #ofaMainPage class definition.
 * @include: ui/ofa-main-page.h
 *
 * The main window is organized with a treeview of the left pane, and a
 * notebook on the right one. The main child of the pages of this said
 * 'main' (because owned by the main window) notebook is always a
 * GtkGrid which is itself created by the main window.
 *
 * All datas of the applications are displayed in pages of this 'main'
 * notebook. These datas are handled by ofaMainPage-derived classes
 * (this class). Each ofaMainPage-derived class begins so with an empty
 * GtkGrid.
 *
 * Most of them, though this is far from being mandatory, have a
 * similar display:
 *
 * +------------------------------------------------------------------+
 * | GtkGrid created by the main window,                              |
 * |  top child of the 'main' notebook's page for this theme          |
 * |+------------------------------------------------+---------------+|
 * || left=0, top=0                                  | left=1        ||
 * ||                                                |               ||
 * ||  the view for this theme                       |  buttons box  ||
 * ||                                                |               ||
 * |+------------------------------------------------+---------------+|
 * +------------------------------------------------------------------+
 *
 * Class        Description                               Buttons box
 * -----------  ----------------------------------------  -----------
 * Account      a notebook with one page per account          Yes
 *               class number, each page having its own
 *               tree view on a list store
 *
 * BAT files    a tree view on a list store                   Yes
 *
 * Classes      a tree view on a list store                   Yes
 *
 * Currencies   a tree view on a list store                   Yes
 *
 * Entries      (none)
 *
 * Entry Model  a notebook with one page per journal          Yes
 *               each page having its own tree view
 *               on a list store
 *
 * GuidedInputEx  a paned which embeds a tree view on a        ?
 *                tree store on the left where entry
 *                models are stored 'under' the journal;
 *                the entry model with its characteristics
 *                and its entry grid on the right
 *
 * Journals     a tree view on a list store                   Yes
 *
 * Rate         a tree view on a list store                   Yes
 */

#include "ui/ofa-main-page-def.h"
#include "api/ofo-dossier-def.h"
#include "ui/ofa-main-window-def.h"

G_BEGIN_DECLS

/**
 * Properties set against this base class at instanciation time
 */
#define MAIN_PAGE_PROP_WINDOW            "main-page-prop-window"
#define MAIN_PAGE_PROP_DOSSIER           "main-page-prop-dossier"
#define MAIN_PAGE_PROP_GRID              "main-page-prop-grid"
#define MAIN_PAGE_PROP_THEME             "main-page-prop-theme"
#define MAIN_PAGE_PROP_HAS_IMPORT        "main-page-prop-import"
#define MAIN_PAGE_PROP_HAS_EXPORT        "main-page-prop-export"

/**
 * Whether an object has been created, updated or deleted
 */
typedef enum {
	MAIN_PAGE_OBJECT_CREATED = 1,
	MAIN_PAGE_OBJECT_UPDATED,
	MAIN_PAGE_OBJECT_DELETED
}
	ofaMainPageUpdateType;

/**
 * The name of the buttons created in the buttons box
 */
#define PAGE_BUTTON_NEW                 "btn-new"
#define PAGE_BUTTON_UPDATE              "btn-update"
#define PAGE_BUTTON_DELETE              "btn-delete"
#define PAGE_BUTTON_IMPORT              "btn-import"
#define PAGE_BUTTON_EXPORT              "btn-export"

ofaMainWindow *ofa_main_page_get_main_window    ( const ofaMainPage *page );
ofoDossier    *ofa_main_page_get_dossier        ( const ofaMainPage *page );
gint           ofa_main_page_get_theme          ( const ofaMainPage *page );
GtkGrid       *ofa_main_page_get_grid           ( const ofaMainPage *page );
GtkTreeView   *ofa_main_page_get_treeview       ( const ofaMainPage *page );

GtkWidget     *ofa_main_page_get_new_btn        ( const ofaMainPage *page );
GtkWidget     *ofa_main_page_get_update_btn     ( const ofaMainPage *page );
GtkWidget     *ofa_main_page_get_delete_btn     ( const ofaMainPage *page );
GtkWidget     *ofa_main_page_get_import_btn     ( const ofaMainPage *page );
GtkWidget     *ofa_main_page_get_export_btn     ( const ofaMainPage *page );

GtkBox        *ofa_main_page_get_buttons_box_new( gboolean has_import, gboolean has_export );

gboolean       ofa_main_page_delete_confirmed   ( const ofaMainPage *page, const gchar *message );

G_END_DECLS

#endif /* __OFA_MAIN_PAGE_H__ */
