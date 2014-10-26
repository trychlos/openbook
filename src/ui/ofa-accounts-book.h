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

#ifndef __OFA_ACCOUNTS_BOOK_H__
#define __OFA_ACCOUNTS_BOOK_H__

/**
 * SECTION: ofa_accounts_book
 * @short_description: #ofaAccountsBook class definition.
 * @include: ui/ofa-accounts-book.h
 *
 * This is a convenience class which manages the display of the accounts
 * inside of a notebook, with one class per page.
 *
 * At creation time, this convenience class defines a Alt+1..Alt+9
 * mnemonic for each class at the GtkWindow parent level. These
 * mnemonics are removed when disposing.
 *
 * This convenience class also manages the update buttons (new, update,
 * delete and view entries). So that all the AccountsPage features are
 * also available in AccountSelect dialog.
 */

#include "api/ofo-account-def.h"

#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNTS_BOOK                ( ofa_accounts_book_get_type())
#define OFA_ACCOUNTS_BOOK( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNTS_BOOK, ofaAccountsBook ))
#define OFA_ACCOUNTS_BOOK_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNTS_BOOK, ofaAccountsBookClass ))
#define OFA_IS_ACCOUNTS_BOOK( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNTS_BOOK ))
#define OFA_IS_ACCOUNTS_BOOK_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNTS_BOOK ))
#define OFA_ACCOUNTS_BOOK_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNTS_BOOK, ofaAccountsBookClass ))

typedef struct _ofaAccountsBookPrivate        ofaAccountsBookPrivate;

typedef struct {
	/*< private >*/
	GObject                 parent;
	ofaAccountsBookPrivate *private;
}
	ofaAccountsBook;

typedef struct {
	/*< private >*/
	GObjectClass parent;
}
	ofaAccountsBookClass;

/**
 * ofaAccountsBookCb:
 *
 * A callback triggered when the selected account changes.
 *
 * Passed arguments are:
 * - the selected account
 * - provided user data.
 */
typedef void ( *ofaAccountsBookCb )( ofoAccount *, gpointer );

GType ofa_accounts_book_get_type ( void ) G_GNUC_CONST;

/**
 * ofsAccountsBookParms:
 * @book: an empty notebook which must have been defined by the caller
 * @dossier: the currently opened ofoDossier
 * @pfnSelected: [allow-none]: a user-provided callback which will be
 *  triggered each time the selection changes
 * @pfnActivated: [allow-none]: a user-provided callback which will be
 *  triggered each time a new row is activated (by hitting Enter or
 *  double-clicking the row)
 * @user_data: user data passed to the above callbacks
 */
typedef struct {
	ofaMainWindow    *main_window;
	GtkContainer     *parent;
	gboolean          has_import;
	gboolean          has_export;
	gboolean          has_view_entries;
	ofaAccountsBookCb pfnSelected;
	ofaAccountsBookCb pfnActivated;
	ofaAccountsBookCb pfnViewEntries;
	gpointer          user_data;
}
	ofsAccountsBookParms;

ofaAccountsBook *ofa_accounts_book_new         ( ofsAccountsBookParms *parms );

void             ofa_accounts_book_init_view   ( ofaAccountsBook *self, const gchar *number );

ofoAccount      *ofa_accounts_book_get_selected( ofaAccountsBook *self );

void             ofa_accounts_book_set_selected( ofaAccountsBook *self, const gchar *number );

void             ofa_accounts_book_grab_focus  ( ofaAccountsBook *self );

G_END_DECLS

#endif /* __OFA_ACCOUNTS_BOOK_H__ */
