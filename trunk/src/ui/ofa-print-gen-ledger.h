/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_PRINT_GEN_LEDGER_H__
#define __OFA_PRINT_GEN_LEDGER_H__

/**
 * SECTION: ofa_print_gen_ledger
 * @short_description: #ofaPrintGenLedger class definition.
 * @include: ui/ofa-print-gen-ledger.h
 *
 * Print the General Ledger summary.
 *
 * This is a convenience class around a GtkPrintOperation.
 */

#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_PRINT_GEN_LEDGER                ( ofa_print_gen_ledger_get_type())
#define OFA_PRINT_GEN_LEDGER( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PRINT_GEN_LEDGER, ofaPrintGenLedger ))
#define OFA_PRINT_GEN_LEDGER_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PRINT_GEN_LEDGER, ofaPrintGenLedgerClass ))
#define OFA_IS_PRINT_GEN_LEDGER( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PRINT_GEN_LEDGER ))
#define OFA_IS_PRINT_GEN_LEDGER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PRINT_GEN_LEDGER ))
#define OFA_PRINT_GEN_LEDGER_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PRINT_GEN_LEDGER, ofaPrintGenLedgerClass ))

typedef struct _ofaPrintGenLedgerPrivate         ofaPrintGenLedgerPrivate;

typedef struct {
	/*< public members >*/
	GObject                  parent;

	/*< private members >*/
	ofaPrintGenLedgerPrivate *priv;
}
	ofaPrintGenLedger;

typedef struct {
	/*< public members >*/
	GObjectClass              parent;
}
	ofaPrintGenLedgerClass;

GType    ofa_print_gen_ledger_get_type( void ) G_GNUC_CONST;

gboolean ofa_print_gen_ledger_run     ( ofaMainWindow *parent );

G_END_DECLS

#endif /* __OFA_PRINT_GEN_LEDGER_H__ */
