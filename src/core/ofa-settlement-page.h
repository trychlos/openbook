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

#ifndef __OFA_SETTLEMENT_PAGE_H__
#define __OFA_SETTLEMENT_PAGE_H__

/**
 * SECTION: ofa_settlement_page
 * @short_description: #ofaSettlementPage class definition.
 * @include: core/ofa-settlement-page.h
 *
 * Development rules:
 * - type:       page
 * - settings:   yes
 * - current:    no
 */

#include "api/ofa-paned-page.h"

G_BEGIN_DECLS

#define OFA_TYPE_SETTLEMENT_PAGE                ( ofa_settlement_page_get_type())
#define OFA_SETTLEMENT_PAGE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_SETTLEMENT_PAGE, ofaSettlementPage ))
#define OFA_SETTLEMENT_PAGE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_SETTLEMENT_PAGE, ofaSettlementPageClass ))
#define OFA_IS_SETTLEMENT_PAGE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_SETTLEMENT_PAGE ))
#define OFA_IS_SETTLEMENT_PAGE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_SETTLEMENT_PAGE ))
#define OFA_SETTLEMENT_PAGE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_SETTLEMENT_PAGE, ofaSettlementPageClass ))

typedef struct {
	/*< public members >*/
	ofaPanedPage      parent;
}
	ofaSettlementPage;

typedef struct {
	/*< public members >*/
	ofaPanedPageClass parent;
}
	ofaSettlementPageClass;

GType ofa_settlement_page_get_type   ( void ) G_GNUC_CONST;

void  ofa_settlement_page_set_account( ofaSettlementPage *page,
											const gchar *number );

G_END_DECLS

#endif /* __OFA_SETTLEMENT_PAGE_H__ */
