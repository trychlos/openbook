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

#ifndef __OFA_ACCOUNTS_BOOK_BIN_H__
#define __OFA_ACCOUNTS_BOOK_BIN_H__

/**
 * SECTION: ofa_accounts_book_bin
 * @short_description: #ofaAccountsBookBin class definition.
 * @include: ui/ofa-accounts-book-bin.h
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

#include "ui/ofa-iaccounts-filter.h"
#include "ui/ofa-idates-filter.h"
#include "ui/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNTS_BOOK_BIN                ( ofa_accounts_book_bin_get_type())
#define OFA_ACCOUNTS_BOOK_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNTS_BOOK_BIN, ofaAccountsBookBin ))
#define OFA_ACCOUNTS_BOOK_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNTS_BOOK_BIN, ofaAccountsBookBinClass ))
#define OFA_IS_ACCOUNTS_BOOK_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNTS_BOOK_BIN ))
#define OFA_IS_ACCOUNTS_BOOK_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNTS_BOOK_BIN ))
#define OFA_ACCOUNTS_BOOK_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNTS_BOOK_BIN, ofaAccountsBookBinClass ))

typedef struct _ofaAccountsBookBinPrivate         ofaAccountsBookBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                     parent;

	/*< private members >*/
	ofaAccountsBookBinPrivate *priv;
}
	ofaAccountsBookBin;

typedef struct {
	/*< public members >*/
	GtkBinClass                parent;
}
	ofaAccountsBookBinClass;

GType               ofa_accounts_book_bin_get_type( void ) G_GNUC_CONST;

ofaAccountsBookBin *ofa_accounts_book_bin_new     ( ofaMainWindow *main_window );

gboolean            ofa_accounts_book_bin_is_valid( ofaAccountsBookBin *bin,
															gchar **message );

gboolean            ofa_accounts_book_bin_get_new_page_per_account
                                                  ( const ofaAccountsBookBin *bin );

ofaIAccountsFilter *ofa_accounts_book_bin_get_accounts_filter
                                                  ( const ofaAccountsBookBin *bin );

ofaIDatesFilter    *ofa_accounts_book_bin_get_dates_filter
                                                  ( const ofaAccountsBookBin *bin );

G_END_DECLS

#endif /* __OFA_ACCOUNTS_BOOK_BIN_H__ */
