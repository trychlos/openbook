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
 *
 * $Id$
 */

#ifndef __OFA_SETTLEMENT_H__
#define __OFA_SETTLEMENT_H__

/**
 * SECTION: ofa_settlement
 * @short_description: #ofaSettlement class definition.
 * @include: ui/ofa-settlement.h
 */

#include "ui/ofa-page-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_SETTLEMENT                ( ofa_settlement_get_type())
#define OFA_SETTLEMENT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_SETTLEMENT, ofaSettlement ))
#define OFA_SETTLEMENT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_SETTLEMENT, ofaSettlementClass ))
#define OFA_IS_SETTLEMENT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_SETTLEMENT ))
#define OFA_IS_SETTLEMENT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_SETTLEMENT ))
#define OFA_SETTLEMENT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_SETTLEMENT, ofaSettlementClass ))

typedef struct _ofaSettlementPrivate       ofaSettlementPrivate;

typedef struct {
	/*< public members >*/
	ofaPage               parent;

	/*< private members >*/
	ofaSettlementPrivate *priv;
}
	ofaSettlement;

typedef struct {
	/*< public members >*/
	ofaPageClass parent;
}
	ofaSettlementClass;

GType ofa_settlement_get_type       ( void ) G_GNUC_CONST;

void  ofa_settlement_display_entries( ofaSettlement *self, GType type, const gchar *id, const GDate *begin, const GDate *end );

G_END_DECLS

#endif /* __OFA_SETTLEMENT_H__ */
