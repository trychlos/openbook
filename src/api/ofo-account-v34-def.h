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

#ifndef __OPENBOOK_API_OFO_ACCOUNT_V34_DEF_H__
#define __OPENBOOK_API_OFO_ACCOUNT_V34_DEF_H__

/**
 * SECTION: ofoaccount
 * @include: openbook/ofo-account-v34-def.h
 */

#include "api/ofo-base-def.h"

G_BEGIN_DECLS

typedef struct _ofoAccountv34              ofoAccountv34;
typedef struct _ofoAccountv34Class         ofoAccountv34Class;

struct _ofoAccountv34 {
	/*< public members >*/
	ofoBase      parent;
};

struct _ofoAccountv34Class {
	/*< public members >*/
	ofoBaseClass parent;
};


G_END_DECLS

#endif /* __OPENBOOK_API_OFO_ACCOUNT_V34_DEF_H__ */
