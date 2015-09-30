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
 */

#ifndef __OFA_CHECK_BALANCES_BIN_H__
#define __OFA_CHECK_BALANCES_BIN_H__

/**
 * SECTION: ofa_check_balances_bin
 * @short_description: #ofaCheckBalancesBin class definition.
 * @include: ui/ofa-check-balances-bin.h
 *
 * Check entries, accounts and ledgers balances.
 *
 * Please note that the checks are started when setting the dossier,
 * and run asynchronously. So they are most probably still running
 * when #ofa_check_balances_bin_set_dossier() method returns.
 * Caller should connect to "ofa-done" signal in order to be signaled
 * when the checks are done.
 *
 * Development rules:
 * - type:       part_of
 * - parent:     top
 * - change:     no
 * - validation: no
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_CHECK_BALANCES_BIN                ( ofa_check_balances_bin_get_type())
#define OFA_CHECK_BALANCES_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_CHECK_BALANCES_BIN, ofaCheckBalancesBin ))
#define OFA_CHECK_BALANCES_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_CHECK_BALANCES_BIN, ofaCheckBalancesBinClass ))
#define OFA_IS_CHECK_BALANCES_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_CHECK_BALANCES_BIN ))
#define OFA_IS_CHECK_BALANCES_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_CHECK_BALANCES_BIN ))
#define OFA_CHECK_BALANCES_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_CHECK_BALANCES_BIN, ofaCheckBalancesBinClass ))

typedef struct _ofaCheckBalancesBinPrivate         ofaCheckBalancesBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                      parent;

	/*< private members >*/
	ofaCheckBalancesBinPrivate *priv;
}
	ofaCheckBalancesBin;

typedef struct {
	/*< public members >*/
	GtkBinClass                 parent;
}
	ofaCheckBalancesBinClass;

GType                ofa_check_balances_bin_get_type   ( void ) G_GNUC_CONST;

ofaCheckBalancesBin *ofa_check_balances_bin_new        ( void );

void                 ofa_check_balances_bin_set_dossier( ofaCheckBalancesBin *bin,
																	ofoDossier *dossier );

gboolean             ofa_check_balances_bin_get_status ( const ofaCheckBalancesBin *bin );

G_END_DECLS

#endif /* __OFA_CHECK_BALANCES_BIN_H__ */
