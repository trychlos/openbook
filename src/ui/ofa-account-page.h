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

#ifndef __OFA_ACCOUNT_PAGE_H__
#define __OFA_ACCOUNT_PAGE_H__

/**
 * SECTION: ofa_account_page
 * @short_description: #ofaAccountPage class definition.
 * @include: ui/ofa-account-page.h
 *
 * Display the chart of accounts, letting the user edit it.
 */

#include "api/ofa-page-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNT_PAGE                ( ofa_account_page_get_type())
#define OFA_ACCOUNT_PAGE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNT_PAGE, ofaAccountPage ))
#define OFA_ACCOUNT_PAGE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNT_PAGE, ofaAccountPageClass ))
#define OFA_IS_ACCOUNT_PAGE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNT_PAGE ))
#define OFA_IS_ACCOUNT_PAGE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNT_PAGE ))
#define OFA_ACCOUNT_PAGE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNT_PAGE, ofaAccountPageClass ))

typedef struct {
	/*< public members >*/
	ofaPage      parent;
}
	ofaAccountPage;

typedef struct {
	/*< public members >*/
	ofaPageClass parent;
}
	ofaAccountPageClass;

GType ofa_account_page_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_ACCOUNT_PAGE_H__ */
