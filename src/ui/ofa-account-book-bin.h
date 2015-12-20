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

#ifndef __OFA_ACCOUNT_BOOK_BIN_H__
#define __OFA_ACCOUNT_BOOK_BIN_H__

/**
 * SECTION: ofa_account_book_bin
 * @short_description: #ofaAccountBookBin class definition.
 * @include: ui/ofa-account-book-bin.h
 *
 * Display a frame with let the user select the parameters needed to
 * print the entries books between two effect dates.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: yes (has 'ofa-changed' signal)
 * - settings:   no
 * - current:    no
 */

#include "api/ofa-main-window-def.h"

#include "ui/ofa-iaccount-filter.h"
#include "ui/ofa-idate-filter.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNT_BOOK_BIN                ( ofa_account_book_bin_get_type())
#define OFA_ACCOUNT_BOOK_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNT_BOOK_BIN, ofaAccountBookBin ))
#define OFA_ACCOUNT_BOOK_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNT_BOOK_BIN, ofaAccountBookBinClass ))
#define OFA_IS_ACCOUNT_BOOK_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNT_BOOK_BIN ))
#define OFA_IS_ACCOUNT_BOOK_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNT_BOOK_BIN ))
#define OFA_ACCOUNT_BOOK_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNT_BOOK_BIN, ofaAccountBookBinClass ))

typedef struct _ofaAccountBookBinPrivate         ofaAccountBookBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                    parent;

	/*< private members >*/
	ofaAccountBookBinPrivate *priv;
}
	ofaAccountBookBin;

typedef struct {
	/*< public members >*/
	GtkBinClass               parent;
}
	ofaAccountBookBinClass;

GType               ofa_account_book_bin_get_type( void ) G_GNUC_CONST;

ofaAccountBookBin  *ofa_account_book_bin_new     ( const ofaMainWindow *main_window );

gboolean            ofa_account_book_bin_is_valid( ofaAccountBookBin *bin,
															gchar **message );

gboolean            ofa_account_book_bin_get_new_page_per_account
                                                  ( const ofaAccountBookBin *bin );

ofaIAccountFilter *ofa_account_book_bin_get_account_filter
                                                  ( const ofaAccountBookBin *bin );

ofaIDateFilter    *ofa_account_book_bin_get_date_filter
                                                  ( const ofaAccountBookBin *bin );

G_END_DECLS

#endif /* __OFA_ACCOUNT_BOOK_BIN_H__ */
