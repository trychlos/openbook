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

#ifndef __OFA_ACCOUNT_BOOK_RENDER_H__
#define __OFA_ACCOUNT_BOOK_RENDER_H__

/**
 * SECTION: ofa_account_book_render
 * @short_description: #ofaAccountBookRender class definition.
 * @include: ui/ofa-account-book-render.h
 *
 * The class which manages the rendering (preview/print) of books.
 * Arguments are entered via the #ofaAccountBookArgs composite widget.
 *
 * Accounts Book Summary list the entries for the requested account(s)
 * before the requested effect date(s), and displays the balance of
 * these entries by account, and for the total.
 *
 * All entries (but the deleted ones) are taken into account given the
 * specified effect dates.
 *
 * Have a balance of entries by account: always.
 * Have a general balance of entries by currency: always.
 *
 * Have a new page by account: on option.
 * Have a new page by class: on option.
 * Have a balance of entries by class (and by currency): on option.
 *
 * ofaIRenderable group management:
 * - by account: header+footer
 * - by class: header+footer (if requested to)
 *
 * ofaIRenderable page report management:
 * - top/bottom report: current solde of the account
 * - top/bottom report: current solde of the class (if requested to)
 *
 * ofaIRenderable last summary:
 * - general balance
 */

#include "api/ofa-render-page.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNT_BOOK_RENDER                ( ofa_account_book_render_get_type())
#define OFA_ACCOUNT_BOOK_RENDER( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNT_BOOK_RENDER, ofaAccountBookRender ))
#define OFA_ACCOUNT_BOOK_RENDER_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNT_BOOK_RENDER, ofaAccountBookRenderClass ))
#define OFA_IS_ACCOUNT_BOOK_RENDER( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNT_BOOK_RENDER ))
#define OFA_IS_ACCOUNT_BOOK_RENDER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNT_BOOK_RENDER ))
#define OFA_ACCOUNT_BOOK_RENDER_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNT_BOOK_RENDER, ofaAccountBookRenderClass ))

typedef struct {
	/*< public members >*/
	ofaRenderPage      parent;
}
	ofaAccountBookRender;

typedef struct {
	/*< public members >*/
	ofaRenderPageClass parent;
}
	ofaAccountBookRenderClass;

GType ofa_account_book_render_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_ACCOUNT_BOOK_RENDER_H__ */
