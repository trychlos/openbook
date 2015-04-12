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

#ifndef __OFA_RENDER_BOOKS_BIN_H__
#define __OFA_RENDER_BOOKS_BIN_H__

/**
 * SECTION: ofa_render_books_bin
 * @short_description: #ofaRenderBooksBin class definition.
 * @include: ui/ofa-render-books-bin.h
 *
 * Display a frame with let the user select the parameters needed to
 * print the entries books between two effect dates.
 */

#include "ui/ofa-iaccounts-filter.h"
#include "ui/ofa-idates-filter.h"
#include "ui/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_RENDER_BOOKS_BIN                ( ofa_render_books_bin_get_type())
#define OFA_RENDER_BOOKS_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RENDER_BOOKS_BIN, ofaRenderBooksBin ))
#define OFA_RENDER_BOOKS_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RENDER_BOOKS_BIN, ofaRenderBooksBinClass ))
#define OFA_IS_RENDER_BOOKS_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RENDER_BOOKS_BIN ))
#define OFA_IS_RENDER_BOOKS_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RENDER_BOOKS_BIN ))
#define OFA_RENDER_BOOKS_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RENDER_BOOKS_BIN, ofaRenderBooksBinClass ))

typedef struct _ofaRenderBooksBinPrivate         ofaRenderBooksBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                    parent;

	/*< private members >*/
	ofaRenderBooksBinPrivate *priv;
}
	ofaRenderBooksBin;

typedef struct {
	/*< public members >*/
	GtkBinClass               parent;
}
	ofaRenderBooksBinClass;

GType               ofa_render_books_bin_get_type     ( void ) G_GNUC_CONST;

ofaRenderBooksBin  *ofa_render_books_bin_new          ( ofaMainWindow *main_window );

gboolean            ofa_render_books_bin_is_valid     ( ofaRenderBooksBin *bin,
																		gchar **message );

gboolean            ofa_render_books_bin_get_new_page_per_account
                                                      ( const ofaRenderBooksBin *bin );

ofaIAccountsFilter *ofa_render_books_bin_get_accounts_filter
                                                      ( const ofaRenderBooksBin *bin );

ofaIDatesFilter    *ofa_render_books_bin_get_dates_filter
                                                      ( const ofaRenderBooksBin *bin );

G_END_DECLS

#endif /* __OFA_RENDER_BOOKS_BIN_H__ */
