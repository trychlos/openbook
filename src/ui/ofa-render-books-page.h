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

#ifndef __OFA_RENDER_BOOKS_PAGE_H__
#define __OFA_RENDER_BOOKS_PAGE_H__

/**
 * SECTION: ofa_render_books_page
 * @short_description: #ofaRenderBooksPage class definition.
 * @include: ui/ofa-render-books-page.h
 *
 * The class which manages the rendering (preview/print) of books.
 */

#include "ui/ofa-render-page.h"

G_BEGIN_DECLS

#define OFA_TYPE_RENDER_BOOKS_PAGE                ( ofa_render_books_page_get_type())
#define OFA_RENDER_BOOKS_PAGE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RENDER_BOOKS_PAGE, ofaRenderBooksPage ))
#define OFA_RENDER_BOOKS_PAGE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RENDER_BOOKS_PAGE, ofaRenderBooksPageClass ))
#define OFA_IS_RENDER_BOOKS_PAGE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RENDER_BOOKS_PAGE ))
#define OFA_IS_RENDER_BOOKS_PAGE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RENDER_BOOKS_PAGE ))
#define OFA_RENDER_BOOKS_PAGE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RENDER_BOOKS_PAGE, ofaRenderBooksPageClass ))

typedef struct _ofaRenderBooksPagePrivate         ofaRenderBooksPagePrivate;

typedef struct {
	/*< public members >*/
	ofaRenderPage              parent;

	/*< private members >*/
	ofaRenderBooksPagePrivate *priv;
}
	ofaRenderBooksPage;

typedef struct {
	/*< public members >*/
	ofaRenderPageClass         parent;
}
	ofaRenderBooksPageClass;

GType ofa_render_books_page_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_RENDER_BOOKS_PAGE_H__ */
