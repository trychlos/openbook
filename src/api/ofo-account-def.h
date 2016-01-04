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

#ifndef __OPENBOOK_API_OFO_ACCOUNT_DEF_H__
#define __OPENBOOK_API_OFO_ACCOUNT_DEF_H__

/**
 * SECTION: ofo_account
 * @short_description: #ofoAccount class definition.
 * @include: openbook/ofo-account.h
 *
 * This class implements the Account behavior, including the general
 * DB definition.
 */

#include "api/ofo-base-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_ACCOUNT                ( ofo_account_get_type())
#define OFO_ACCOUNT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_ACCOUNT, ofoAccount ))
#define OFO_ACCOUNT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_ACCOUNT, ofoAccountClass ))
#define OFO_IS_ACCOUNT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_ACCOUNT ))
#define OFO_IS_ACCOUNT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_ACCOUNT ))
#define OFO_ACCOUNT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_ACCOUNT, ofoAccountClass ))

typedef struct _ofoAccountPrivate       ofoAccountPrivate;

typedef struct {
	/*< public members >*/
	ofoBase            parent;

	/*< private members >*/
	ofoAccountPrivate *priv;
}
	ofoAccount;

typedef struct {
	/*< public members >*/
	ofoBaseClass       parent;
}
	ofoAccountClass;

GType ofo_account_get_type ( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_ACCOUNT_DEF_H__ */
