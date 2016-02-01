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

#ifndef __OFA_NOMODAL_PAGE_H__
#define __OFA_NOMODAL_PAGE_H__

/**
 * SECTION: ofa_nomodal_page
 * @short_description: #ofaNomodalPage class definition.
 * @include: ui/ofa-nomodal-page.h
 *
 * Let the #ofaPage be diisplayed as a non-modal window.
 *
 * Development rules:
 * - type:       per-theme non-modal window
 * - settings:   yes
 * - current:    yes
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_NOMODAL_PAGE                ( ofa_nomodal_page_get_type())
#define OFA_NOMODAL_PAGE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_NOMODAL_PAGE, ofaNomodalPage ))
#define OFA_NOMODAL_PAGE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_NOMODAL_PAGE, ofaNomodalPageClass ))
#define OFA_IS_NOMODAL_PAGE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_NOMODAL_PAGE ))
#define OFA_IS_NOMODAL_PAGE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_NOMODAL_PAGE ))
#define OFA_NOMODAL_PAGE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_NOMODAL_PAGE, ofaNomodalPageClass ))

typedef struct _ofaNomodalPagePrivate         ofaNomodalPagePrivate;

typedef struct {
	/*< public members >*/
	GtkWindow      parent;
}
	ofaNomodalPage;

typedef struct {
	/*< public members >*/
	GtkWindowClass parent;
}
	ofaNomodalPageClass;

GType           ofa_nomodal_page_get_type    ( void ) G_GNUC_CONST;

void            ofa_nomodal_page_run         ( const ofaMainWindow *parent,
													const gchar *title,
													GtkWidget *page );

ofaNomodalPage *ofa_nomodal_page_get_by_theme( gint theme );

void            ofa_nomodal_page_close_all   ( void );

G_END_DECLS

#endif /* __OFA_NOMODAL_PAGE_H__ */
