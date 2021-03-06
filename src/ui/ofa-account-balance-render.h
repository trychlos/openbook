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

#ifndef __OFA_ACCOUNT_BALANCE_RENDER_H__
#define __OFA_ACCOUNT_BALANCE_RENDER_H__

/**
 * SECTION: ofa_account_balance_render
 * @short_description: #ofaAccountBalanceRender class definition.
 * @include: ui/ofa-account-balance-render.h
 *
 * The class which manages the rendering (preview/print) of accounts
 * balances.
 *
 * This render all accounts, even those on which no entry has been
 * imputed.
 *
 * For each account, the solde at the beginning of the period is
 * recomputed, the entries are balances, and the solde at the end
 * of the period is displayed.
 */

#include "api/ofa-render-page.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNT_BALANCE_RENDER                ( ofa_account_balance_render_get_type())
#define OFA_ACCOUNT_BALANCE_RENDER( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNT_BALANCE_RENDER, ofaAccountBalanceRender ))
#define OFA_ACCOUNT_BALANCE_RENDER_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNT_BALANCE_RENDER, ofaAccountBalanceRenderClass ))
#define OFA_IS_ACCOUNT_BALANCE_RENDER( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNT_BALANCE_RENDER ))
#define OFA_IS_ACCOUNT_BALANCE_RENDER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNT_BALANCE_RENDER ))
#define OFA_ACCOUNT_BALANCE_RENDER_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNT_BALANCE_RENDER, ofaAccountBalanceRenderClass ))

typedef struct {
	/*< public members >*/
	ofaRenderPage      parent;
}
	ofaAccountBalanceRender;

typedef struct {
	/*< public members >*/
	ofaRenderPageClass parent;
}
	ofaAccountBalanceRenderClass;

GType ofa_account_balance_render_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_ACCOUNT_BALANCE_RENDER_H__ */
