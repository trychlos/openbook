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

#ifndef __OFA_ACCOUNT_BALANCE_H__
#define __OFA_ACCOUNT_BALANCE_H__

/**
 * SECTION: ofa_account_balance
 * @short_description: #ofaAccountBalance class definition.
 * @include: core/ofa-account-balance.h
 *
 * The #ofaAccountBalance is a convenience object which computes the
 * soldes of the accounts between two effect dates.
 *
 * For each account, the solde at the beginning of the period is
 * recomputed, the entries are balanced, and the solde at the end
 * of the period is displayed.
 *
 * Rationale:
 * We want be able to export the accounts balances from the
 * #ofaAccountBalanceRender page. This means that we need an
 * #ofaIExportable which  has to be instanciated from
 * ofaHub::register_types().
 *
 * Property:
 * - 'ofa-getter': the #ofaIGetter of the application;
 *    must be provided at instanciation time.
 */

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofo-account-def.h"
#include "api/ofo-currency-def.h"
#include "api/ofs-account-balance.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNT_BALANCE                ( ofa_account_balance_get_type())
#define OFA_ACCOUNT_BALANCE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNT_BALANCE, ofaAccountBalance ))
#define OFA_ACCOUNT_BALANCE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNT_BALANCE, ofaAccountBalanceClass ))
#define OFA_IS_ACCOUNT_BALANCE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNT_BALANCE ))
#define OFA_IS_ACCOUNT_BALANCE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNT_BALANCE ))
#define OFA_ACCOUNT_BALANCE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNT_BALANCE, ofaAccountBalanceClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaAccountBalance;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaAccountBalanceClass;

/**
 * ofsAccountBalancePeriod:
 *
 * An extension of #ofsAccountBalance structure to hold the solde of
 * a period.
 *
 * This same extended structure is also used to provide subtotals
 * (e.g. per class - with null account).
 * Subtotals are always per currency.
 */
typedef struct {
	ofsAccountBalance account_balance;
	ofxAmount         begin_solde;
	ofxAmount         end_solde;
}
	ofsAccountBalancePeriod;

GType              ofa_account_balance_get_type  ( void ) G_GNUC_CONST;

ofaAccountBalance *ofa_account_balance_new       ( ofaIGetter *getter );

GList             *ofa_account_balance_compute   ( ofaAccountBalance *balance,
														const gchar *account_from,
														const gchar *account_to,
														const GDate *date_from,
														const GDate *date_to );

GList             *ofa_account_balance_get_totals( ofaAccountBalance *balance );

void               ofa_account_balance_clear     ( ofaAccountBalance *balance );

G_END_DECLS

#endif /* __OFA_ACCOUNT_BALANCE_H__ */
