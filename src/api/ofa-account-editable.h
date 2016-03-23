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

#ifndef __OPENBOOK_API_UI_OFA_ACCOUNT_EDITABLE_H__
#define __OPENBOOK_API_UI_OFA_ACCOUNT_EDITABLE_H__

/**
 * SECTION: iaccount_entry
 * @title: ofaIEntryAccount
 * @short_description: The IEntryAccount interface
 * @include: ui/ofa-account-editable.h
 *
 * The #ofaIEntryAccount interface lets the user enter and select
 * accounts in the provided GtkEntry.
 *
 * Just call #ofa_account_editable_init() with each GtkEntry you want
 * set, and the function will take care of setting an icon, triggering
 * #ofaAccountSelect dialog for account selection.
 */

#include <gtk/gtk.h>

#include "api/ofa-main-window-def.h"
#include "api/ofo-account.h"

G_BEGIN_DECLS

typedef char * ( *AccountPreSelectCb )( GtkEditable *editable, ofeAccountAllowed allowed, void *user_data );
typedef char * ( *AccountPostSelectCb )( GtkEditable *editable, ofeAccountAllowed allowed, const gchar *selected, void *user_data );

void  ofa_account_editable_init             ( GtkEditable *editable,
													ofaMainWindow *main_window,
													ofeAccountAllowed allowed );

void  ofa_account_editable_set_preselect_cb ( GtkEditable *editable,
													AccountPreSelectCb cb,
													void *user_data );

void  ofa_account_editable_set_postselect_cb( GtkEditable *editable,
													AccountPostSelectCb cb,
													void *user_data );

G_END_DECLS

#endif /* __OFA_ACCOUNT_EDITABLE_H__ */
