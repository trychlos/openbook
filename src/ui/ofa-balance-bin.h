/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_BALANCE_BIN_H__
#define __OFA_BALANCE_BIN_H__

/**
 * SECTION: ofa_balance_bin
 * @short_description: #ofaBalanceBin class definition.
 * @include: ui/ofa-balance-bin.h
 *
 * Display a frame with let the user select the parameters needed to
 * print the balance of the entries between two effect dates.
 *
 * Have a checkbox which let the user select 'Accounts balance': the
 * entries are so selected from the beginning of the exercice and
 * really show the balances of the accounts at the specified effect
 * date.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: yes (has 'ofa-changed' signal)
 * - settings:   yes
 * - current:    no
 */

#include "api/ofa-idate-filter.h"
#include "api/ofa-igetter-def.h"

#include "ui/ofa-iaccount-filter.h"

G_BEGIN_DECLS

#define OFA_TYPE_BALANCE_BIN                ( ofa_balance_bin_get_type())
#define OFA_BALANCE_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BALANCE_BIN, ofaBalanceBin ))
#define OFA_BALANCE_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BALANCE_BIN, ofaBalanceBinClass ))
#define OFA_IS_BALANCE_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BALANCE_BIN ))
#define OFA_IS_BALANCE_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BALANCE_BIN ))
#define OFA_BALANCE_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BALANCE_BIN, ofaBalanceBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaBalanceBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaBalanceBinClass;

GType              ofa_balance_bin_get_type              ( void ) G_GNUC_CONST;

ofaBalanceBin     *ofa_balance_bin_new                   ( ofaIGetter *getter );

gboolean           ofa_balance_bin_is_valid              ( ofaBalanceBin *bin,
																gchar **message );

ofaIAccountFilter *ofa_balance_bin_get_account_filter    ( ofaBalanceBin *bin );

gboolean           ofa_balance_bin_get_accounts_balance  ( ofaBalanceBin *bin );

gboolean           ofa_balance_bin_get_subtotal_per_class( ofaBalanceBin *bin );

gboolean           ofa_balance_bin_get_new_page_per_class( ofaBalanceBin *bin );

ofaIDateFilter    *ofa_balance_bin_get_date_filter       ( ofaBalanceBin *bin );

G_END_DECLS

#endif /* __OFA_BALANCE_BIN_H__ */
