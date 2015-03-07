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
 * ofaAccountsPage   an empty grid which is handled by the     book-handled
 *                   ofaAccountsBook class
 *
 * ofaBatsPage       a tree view on a list store                   Yes
 *
 * ofaClassesPage    a tree view on a list store                   Yes
 *
 * ofaCurrenciesPage a tree view on a list store                   Yes
 *
 * ofaGuidedEx       a paned which embeds:                         No
 *                   - on the left, a tree view on a tree
 *                     store where operation templates are
 *                     stored 'under' the journal;
 *                   - on the right, the characteristics of
 *                     the current operation template
 *
 * ofaLedgersPage    a tree view on a list store                   Yes
 *
 * ofaOpeTemplatesPage  a top frame, a grid with dynamic fields    Yes
 *
 * ofaRatesPage      a tree view on a list store                   Yes
 *
 * ofaReconciliation several top frames, with a treeview on        No
 *                   a tree store
 *
 * ofaViewEntries    several top frames with a treeview on a       No
 *                   list store
 */

#include "api/ofo-dossier-def.h"

#include "core/ofa-main-window-def.h"

#include "ui/ofa-page-def.h"

G_BEGIN_DECLS

/**
 * Properties set against this base class at instanciation time
 */
#define PAGE_PROP_MAIN_WINDOW           "page-prop-main-window"
#define PAGE_PROP_TOP_GRID              "page-prop-top-grid"
#define PAGE_PROP_THEME                 "page-prop-theme"

/**
 * The color of the footer (if any)
 */
#define PAGE_RGBA_FOOTER                "#0000ff"	/* blue */

ofaMainWindow *ofa_page_get_main_window           ( const ofaPage *page );
GtkGrid       *ofa_page_get_top_grid              ( const ofaPage *page );
gint           ofa_page_get_theme                 ( const ofaPage *page );
ofoDossier    *ofa_page_get_dossier               ( const ofaPage *page );

GtkWidget     *ofa_page_get_top_focusable_widget  ( const ofaPage *page );

void           ofa_page_pre_remove                ( ofaPage *page );

G_END_DECLS

#endif /* __OFA_PAGE_H__ */
