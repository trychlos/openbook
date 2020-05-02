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

#ifndef __OPENBOOK_API_OFO_ACCOUNT_DEF_H__
#define __OPENBOOK_API_OFO_ACCOUNT_DEF_H__

/**
 * SECTION: ofoaccount
 * @include: openbook/ofo-account-def.h
 */

#include <glib.h>

G_BEGIN_DECLS

typedef struct _ofoAccount              ofoAccount;

/**
 * ofeAccountAllowed:
 * When editing/selecting an account, specify which type of account if
 * allowed to be selected
 * Note that this NOT a bitmask, but the set of selectionable properties:
 * @ACCOUNT_ALLOW_DETAIL: all non closed detail accounts.
 * @ACCOUNT_ALLOW_SETTLEABLE: all non closed settleable accounts.
 * @ACCOUNT_ALLOW_RECONCILIABLE: all non closed reconciliable accounts.
 * @ACCOUNT_ALLOW_FORWARDABLE: all non closed forwardable accounts.
 * @ACCOUNT_ALLOW_ALL: all non closed accounts.
 */
typedef enum {
	ACCOUNT_ALLOW_DETAIL = 1,
	ACCOUNT_ALLOW_SETTLEABLE,
	ACCOUNT_ALLOW_RECONCILIABLE,
	ACCOUNT_ALLOW_FORWARDABLE,
	ACCOUNT_ALLOW_ALL,
}
	ofeAccountAllowed;

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_ACCOUNT_DEF_H__ */
