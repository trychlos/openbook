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

#ifndef __OFA_PAGE_H__
#define __OFA_PAGE_H__

/**
 * SECTION: ofa_page
 * @short_description: #ofaPage class definition.
 * @include: ui/ofa-page.h
 *
 * The main window is organized with a treeview on the left pane, and a
 * notebook on the right one. The main child of the pages of this said
 * 'main' (because owned by the main window) notebook is always a
 * GtkGrid which is itself created by the main window.
 *
 * All datas of the applications are displayed in pages of this 'main'
 * notebook. These datas are handled by ofaPage-derived classes
 * (this class). Each ofaPage-derived class begins so with an empty
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
 * Class             View description                          Buttons box
 * ----------------  ----------------------------------------  -----------
 * ofaAccountsPage   a notebook with one page per account          Yes
 *                   class, each page having its own tree
 *                   view on a list store
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

#include "api/ofo-dossier-def.h"

#include "ui/ofa-page-def.h"
#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

/**
 * Properties set against this base class at instanciation time
 */
#define PAGE_PROP_WINDOW            "main-page-prop-window"
#define PAGE_PROP_DOSSIER           "main-page-prop-dossier"
#define PAGE_PROP_GRID              "main-page-prop-grid"
#define PAGE_PROP_THEME             "main-page-prop-theme"
#define PAGE_PROP_HAS_IMPORT        "main-page-prop-import"
#define PAGE_PROP_HAS_EXPORT        "main-page-prop-export"

/**
 * The name of the buttons created in the buttons box
 */
#define PAGE_BUTTON_NEW                 "btn-new"
#define PAGE_BUTTON_UPDATE              "btn-update"
#define PAGE_BUTTON_DELETE              "btn-delete"
#define PAGE_BUTTON_IMPORT              "btn-import"
#define PAGE_BUTTON_EXPORT              "btn-export"

void           ofa_page_pre_remove              ( ofaPage *page );

ofaMainWindow *ofa_page_get_main_window         ( const ofaPage *page );
ofoDossier    *ofa_page_get_dossier             ( const ofaPage *page );
gint           ofa_page_get_theme               ( const ofaPage *page );
GtkGrid       *ofa_page_get_grid                ( const ofaPage *page );

GtkWidget     *ofa_page_get_new_btn             ( const ofaPage *page );
GtkWidget     *ofa_page_get_update_btn          ( const ofaPage *page );
GtkWidget     *ofa_page_get_delete_btn          ( const ofaPage *page );
GtkWidget     *ofa_page_get_import_btn          ( const ofaPage *page );
GtkWidget     *ofa_page_get_export_btn          ( const ofaPage *page );

GtkWidget     *ofa_page_get_top_focusable_widget( ofaPage *page );

GtkBox        *ofa_page_get_buttons_box_new     ( gboolean has_import, gboolean has_export );

G_END_DECLS

#endif /* __OFA_PAGE_H__ */
