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

#ifndef __OFA_LEDGER_PAGE_H__
#define __OFA_LEDGER_PAGE_H__

/**
 * SECTION: ofa_ledger_page
 * @short_description: #ofaLedgerPage class definition.
 * @include: ui/ofa-ledgers-set.h
 *
 * Display the list of ledgers.
 */

#include "api/ofa-action-page.h"

G_BEGIN_DECLS

#define OFA_TYPE_LEDGER_PAGE                ( ofa_ledger_page_get_type())
#define OFA_LEDGER_PAGE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_LEDGER_PAGE, ofaLedgerPage ))
#define OFA_LEDGER_PAGE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_LEDGER_PAGE, ofaLedgerPageClass ))
#define OFA_IS_LEDGER_PAGE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_LEDGER_PAGE ))
#define OFA_IS_LEDGER_PAGE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_LEDGER_PAGE ))
#define OFA_LEDGER_PAGE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_LEDGER_PAGE, ofaLedgerPageClass ))

typedef struct {
	/*< public members >*/
	ofaActionPage      parent;
}
	ofaLedgerPage;

typedef struct {
	/*< public members >*/
	ofaActionPageClass parent;
}
	ofaLedgerPageClass;

GType ofa_ledger_page_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_LEDGER_PAGE_H__ */
