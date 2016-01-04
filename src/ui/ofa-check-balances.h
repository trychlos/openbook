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

#ifndef __OFA_CHECK_BALANCES_H__
#define __OFA_CHECK_BALANCES_H__

/**
 * SECTION: ofa_check_balances
 * @short_description: #ofaCheckBalances class definition.
 * @include: ui/ofa-check-balances.h
 *
 * Check accounts, ledgers and entries balances.
 */

#include "api/my-dialog.h"
#include "api/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_CHECK_BALANCES                ( ofa_check_balances_get_type())
#define OFA_CHECK_BALANCES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_CHECK_BALANCES, ofaCheckBalances ))
#define OFA_CHECK_BALANCES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_CHECK_BALANCES, ofaCheckBalancesClass ))
#define OFA_IS_CHECK_BALANCES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_CHECK_BALANCES ))
#define OFA_IS_CHECK_BALANCES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_CHECK_BALANCES ))
#define OFA_CHECK_BALANCES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_CHECK_BALANCES, ofaCheckBalancesClass ))

typedef struct _ofaCheckBalancesPrivate        ofaCheckBalancesPrivate;

typedef struct {
	/*< public members >*/
	myDialog                 parent;

	/*< private members >*/
	ofaCheckBalancesPrivate *priv;
}
	ofaCheckBalances;

typedef struct {
	/*< public members >*/
	myDialogClass            parent;
}
	ofaCheckBalancesClass;

GType ofa_check_balances_get_type( void ) G_GNUC_CONST;

void  ofa_check_balances_run     ( ofaMainWindow *main_window );

G_END_DECLS

#endif /* __OFA_CHECK_BALANCES_H__ */
