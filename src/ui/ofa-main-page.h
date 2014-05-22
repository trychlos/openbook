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

#include "ui/ofa-main-window.h"

G_BEGIN_DECLS

#define OFA_TYPE_MAIN_PAGE                ( ofa_main_page_get_type())
#define OFA_MAIN_PAGE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MAIN_PAGE, ofaMainPage ))
#define OFA_MAIN_PAGE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MAIN_PAGE, ofaMainPageClass ))
#define OFA_IS_MAIN_PAGE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MAIN_PAGE ))
#define OFA_IS_MAIN_PAGE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MAIN_PAGE ))
#define OFA_MAIN_PAGE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MAIN_PAGE, ofaMainPageClass ))

typedef struct _ofaMainPagePrivate        ofaMainPagePrivate;

typedef struct {
	/*< private >*/
	GObject             parent;
	ofaMainPagePrivate *private;
}
	ofaMainPage;

typedef struct _ofaMainPageClassPrivate   ofaMainPageClassPrivate;

typedef struct {
	/*< private >*/
	GObjectClass             parent;
	ofaMainPageClassPrivate *private;
}
	ofaMainPageClass;

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
#define MAIN_PAGE_SIGNAL_JOURNAL_UPDATED    "main-page-signal-journal-updated"

GType          ofa_main_page_get_type       ( void );

ofaMainWindow *ofa_main_page_get_main_window( const ofaMainPage *page );
void           ofa_main_page_set_main_window( ofaMainPage *page, ofaMainWindow *window );

ofoDossier    *ofa_main_page_get_dossier    ( const ofaMainPage *page );
void           ofa_main_page_set_dossier    ( ofaMainPage *page, ofoDossier *dossier );

GList         *ofa_main_page_get_dataset    ( const ofaMainPage *page );
void           ofa_main_page_set_dataset    ( ofaMainPage *page, GList *dataset );

GtkGrid       *ofa_main_page_get_grid       ( const ofaMainPage *page );
void           ofa_main_page_set_grid       ( ofaMainPage *page, GtkGrid *grid );

gint           ofa_main_page_get_theme      ( const ofaMainPage *page );
void           ofa_main_page_set_theme      ( ofaMainPage *page, gint theme );

gboolean       ofa_main_page_delete_confirmed( const ofaMainPage *page, const gchar *message );

G_END_DECLS

#endif /* __OFA_MAIN_PAGE_H__ */
