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

#ifndef __OFA_LEDGER_PROPERTIES_H__
#define __OFA_LEDGER_PROPERTIES_H__

/**
 * SECTION: ofa_ledger_properties
 * @short_description: #ofaLedgerProperties class definition.
 * @include: ui/ofa-ledger-properties.h
 *
 * Display/update the ledger properties.
 *
 * Development rules:
 * - type:       dialog
 * - settings:   yes
 * - current:    yes
 */

#include "api/my-dialog.h"
#include "api/ofo-ledger-def.h"

#include "ui/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_LEDGER_PROPERTIES                ( ofa_ledger_properties_get_type())
#define OFA_LEDGER_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_LEDGER_PROPERTIES, ofaLedgerProperties ))
#define OFA_LEDGER_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_LEDGER_PROPERTIES, ofaLedgerPropertiesClass ))
#define OFA_IS_LEDGER_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_LEDGER_PROPERTIES ))
#define OFA_IS_LEDGER_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_LEDGER_PROPERTIES ))
#define OFA_LEDGER_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_LEDGER_PROPERTIES, ofaLedgerPropertiesClass ))

typedef struct _ofaLedgerPropertiesPrivate        ofaLedgerPropertiesPrivate;

typedef struct {
	/*< public members >*/
	myDialog                    parent;

	/*< private members >*/
	ofaLedgerPropertiesPrivate *priv;
}
	ofaLedgerProperties;

typedef struct {
	/*< public members >*/
	myDialogClass               parent;
}
	ofaLedgerPropertiesClass;

GType    ofa_ledger_properties_get_type( void ) G_GNUC_CONST;

gboolean ofa_ledger_properties_run     ( ofaMainWindow *parent, ofoLedger *ledger );

G_END_DECLS

#endif /* __OFA_LEDGER_PROPERTIES_H__ */
