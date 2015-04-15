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

#ifndef __OFA_LEDGERS_BOOK_BIN_H__
#define __OFA_LEDGERS_BOOK_BIN_H__

/**
 * SECTION: ofa_ledgers_book_bin
 * @short_description: #ofaLedgersBookBin class definition.
 * @include: ui/ofa-ledgers-book-bin.h
 *
 * Display a frame with let the user select the parameters needed to
 * print the entries ledgers between two effect dates.
 */

#include "ui/ofa-idates-filter.h"
#include "ui/ofa-ledger-treeview.h"
#include "ui/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_LEDGERS_BOOK_BIN                ( ofa_ledgers_book_bin_get_type())
#define OFA_LEDGERS_BOOK_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_LEDGERS_BOOK_BIN, ofaLedgersBookBin ))
#define OFA_LEDGERS_BOOK_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_LEDGERS_BOOK_BIN, ofaLedgersBookBinClass ))
#define OFA_IS_LEDGERS_BOOK_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_LEDGERS_BOOK_BIN ))
#define OFA_IS_LEDGERS_BOOK_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_LEDGERS_BOOK_BIN ))
#define OFA_LEDGERS_BOOK_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_LEDGERS_BOOK_BIN, ofaLedgersBookBinClass ))

typedef struct _ofaLedgersBookBinPrivate         ofaLedgersBookBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                    parent;

	/*< private members >*/
	ofaLedgersBookBinPrivate *priv;
}
	ofaLedgersBookBin;

typedef struct {
	/*< public members >*/
	GtkBinClass               parent;
}
	ofaLedgersBookBinClass;

GType              ofa_ledgers_book_bin_get_type  ( void ) G_GNUC_CONST;

ofaLedgersBookBin *ofa_ledgers_book_bin_new       ( ofaMainWindow *main_window );

gboolean           ofa_ledgers_book_bin_is_valid  ( ofaLedgersBookBin *bin,
																		gchar **message );

ofaLedgerTreeview *ofa_ledgers_book_bin_get_treeview
                                                  ( const ofaLedgersBookBin *bin );

gboolean           ofa_ledgers_book_bin_get_all_ledgers
                                                  ( const ofaLedgersBookBin *bin );

gboolean           ofa_ledgers_book_bin_get_new_page_per_ledger
                                                  ( const ofaLedgersBookBin *bin );

ofaIDatesFilter   *ofa_ledgers_book_bin_get_dates_filter
                                                  ( const ofaLedgersBookBin *bin );

G_END_DECLS

#endif /* __OFA_LEDGERS_BOOK_BIN_H__ */
