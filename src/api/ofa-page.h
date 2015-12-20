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

#ifndef __OPENBOOK_API_OFA_PAGE_H__
#define __OPENBOOK_API_OFA_PAGE_H__

/**
 * SECTION: ofa_page
 * @short_description: #ofaPage class definition.
 * @include: openbook/ofa-page.h
 *
 * The main window is organized with an horizontal paned which
 * includes:
 * - a treeview on the left pane,
 * - a notebook on the right pane.
 * All datas of the application are displayed as pages of this right-
 * pane 'main' notebook. The top child of these pages is always a
 * #GtkGrid -derived classes. Most of them, though this is far from
 * being mandatory, have a similar display:
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
 * Class               View description                          Buttons box
 * ------------------  ----------------------------------------  -----------
 * ofaAccountPage      an empty grid which is handled by the     book-handled
 *                     ofaAccountsBook class
 *
 * ofaBatPage          a tree view on a list store                   Yes
 *
 * ofaClassPage        a tree view on a list store                   Yes
 *
 * ofaCurrencyPage     a tree view on a list store                   Yes
 *
 * ofaGuidedEx         a paned which embeds:                         No
 *                     - on the left, a tree view on a tree
 *                       store where operation templates are
 *                       stored 'under' the journal;
 *                     - on the right, the characteristics of
 *                       the current operation template
 *
 * ofaLedgerPage       a tree view on a list store                   Yes
 *
 * ofaOpeTemplatePage  a top frame, a grid with dynamic fields       Yes
 *
 * ofaRatePage         a tree view on a list store                   Yes
 *
 * ofaReconcilPage     several top frames, with a treeview on        No
 *                     a tree store
 *
 * ofaEntryPage        several top frames with a treeview on a       No
 *                     list store
 */

#include "api/ofa-main-window-def.h"
#include "api/ofa-page-def.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

/**
 * Properties set against this base class at instanciation time
 */
#define PAGE_PROP_MAIN_WINDOW           "page-prop-main-window"
#define PAGE_PROP_THEME                 "page-prop-theme"

const ofaMainWindow *ofa_page_get_main_window           ( const ofaPage *page );

gint                 ofa_page_get_theme                 ( const ofaPage *page );

#define              ofa_page_get_dossier(P)            ofa_main_window_get_dossier( ofa_page_get_main_window( P ))

GtkWidget           *ofa_page_get_top_focusable_widget  ( const ofaPage *page );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_PAGE_H__ */
