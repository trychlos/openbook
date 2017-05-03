/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_ACCOUNT_BALANCE_ARGS_H__
#define __OFA_ACCOUNT_BALANCE_ARGS_H__

/**
 * SECTION: ofa_account_balance_args
 * @short_description: #ofaAccountBalanceArgs class definition.
 * @include: ui/ofa-account-balance-args.h
 *
 * Display a frame with let the user select the parameters needed to
 * print the balance of the accounts between two effect dates.
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

#define OFA_TYPE_ACCOUNT_BALANCE_ARGS                ( ofa_account_balance_args_get_type())
#define OFA_ACCOUNT_BALANCE_ARGS( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNT_BALANCE_ARGS, ofaAccountBalanceArgs ))
#define OFA_ACCOUNT_BALANCE_ARGS_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNT_BALANCE_ARGS, ofaAccountBalanceArgsClass ))
#define OFA_IS_ACCOUNT_BALANCE_ARGS( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNT_BALANCE_ARGS ))
#define OFA_IS_ACCOUNT_BALANCE_ARGS_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNT_BALANCE_ARGS ))
#define OFA_ACCOUNT_BALANCE_ARGS_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNT_BALANCE_ARGS, ofaAccountBalanceArgsClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaAccountBalanceArgs;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaAccountBalanceArgsClass;

GType                  ofa_account_balance_args_get_type              ( void ) G_GNUC_CONST;

ofaAccountBalanceArgs *ofa_account_balance_args_new                   ( ofaIGetter *getter,
																			const gchar *settings_prefix );

gboolean               ofa_account_balance_args_is_valid              ( ofaAccountBalanceArgs *bin,
																			gchar **message );

ofaIAccountFilter     *ofa_account_balance_args_get_account_filter    ( ofaAccountBalanceArgs *bin );

ofaIDateFilter        *ofa_account_balance_args_get_date_filter       ( ofaAccountBalanceArgs *bin );

gboolean               ofa_account_balance_args_get_subtotal_per_class( ofaAccountBalanceArgs *bin );

gboolean               ofa_account_balance_args_get_new_page_per_class( ofaAccountBalanceArgs *bin );

G_END_DECLS

#endif /* __OFA_ACCOUNT_BALANCE_ARGS_H__ */
