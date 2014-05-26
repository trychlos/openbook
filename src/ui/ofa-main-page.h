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
 * notebook on the right one.
 * All datas of the applications are displayed in pages of this 'main'
 * notebook. These datas are handled by ofaMainPage-derived classes
 * (this class).
 */

#include "ui/ofa-main-page-def.h"
#include "ui/ofo-dossier-def.h"
#include "ui/ofa-main-window.h"

G_BEGIN_DECLS

/**
 * Properties set against this base class at instanciation time
 */
#define MAIN_PAGE_PROP_WINDOW   "main-page-prop-window"
#define MAIN_PAGE_PROP_DOSSIER  "main-page-prop-dossier"
#define MAIN_PAGE_PROP_GRID     "main-page-prop-grid"
#define MAIN_PAGE_PROP_THEME    "main-page-prop-theme"

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
 * Signals defined on this base class
 */
#define OFA_SIGNAL_JOURNAL_UPDATED          "ofa-signal-journal-updated"

GType          ofa_main_page_get_type       ( void );

ofaMainWindow *ofa_main_page_get_main_window( const ofaMainPage *page );
ofoDossier    *ofa_main_page_get_dossier    ( const ofaMainPage *page );
gint           ofa_main_page_get_theme      ( const ofaMainPage *page );
GtkGrid       *ofa_main_page_get_grid       ( const ofaMainPage *page );
GtkWidget     *ofa_main_page_get_treeview   ( const ofaMainPage *page );

void           ofa_main_page_set_dataset    ( ofaMainPage *page, GList *dataset );

GtkWidget     *ofa_main_page_get_update_btn ( const ofaMainPage *page );
GtkWidget     *ofa_main_page_get_delete_btn ( const ofaMainPage *page );

gboolean       ofa_main_page_delete_confirmed( const ofaMainPage *page, const gchar *message );

G_END_DECLS

#endif /* __OFA_MAIN_PAGE_H__ */
