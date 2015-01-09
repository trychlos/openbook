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

#ifndef __OFA_LEDGER_COMBO_H__
#define __OFA_LEDGER_COMBO_H__

/**
 * SECTION: ofa_ledger_combo
 * @short_description: #ofaLedgerCombo class definition.
 * @include: ui/ofa-ledger-combo.h
 *
 * A #GtkComboBox -derived class which manages ledgers.
 */

#include <gtk/gtk.h>

#include "api/ofo-dossier-def.h"

#include "core/ofa-main-window-def.h"

#include "ui/ofa-ledger-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_LEDGER_COMBO                ( ofa_ledger_combo_get_type())
#define OFA_LEDGER_COMBO( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_LEDGER_COMBO, ofaLedgerCombo ))
#define OFA_LEDGER_COMBO_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_LEDGER_COMBO, ofaLedgerComboClass ))
#define OFA_IS_LEDGER_COMBO( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_LEDGER_COMBO ))
#define OFA_IS_LEDGER_COMBO_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_LEDGER_COMBO ))
#define OFA_LEDGER_COMBO_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_LEDGER_COMBO, ofaLedgerComboClass ))

typedef struct _ofaLedgerComboPrivate        ofaLedgerComboPrivate;

typedef struct {
	/*< public members >*/
	GtkComboBox            parent;

	/*< private members >*/
	ofaLedgerComboPrivate *priv;
}
	ofaLedgerCombo;

typedef struct {
	/*< public members >*/
	GtkComboBoxClass       parent;
}
	ofaLedgerComboClass;

GType           ofa_ledger_combo_get_type       ( void );

ofaLedgerCombo *ofa_ledger_combo_new            ( void );

void            ofa_ledger_combo_attach_to      ( ofaLedgerCombo *instance,
															GtkContainer *parent );

void            ofa_ledger_combo_set_columns    ( ofaLedgerCombo *instance,
															ofaLedgerColumns columns );

void            ofa_ledger_combo_set_main_window( ofaLedgerCombo *instance,
															const ofaMainWindow *main_window );

gchar          *ofa_ledger_combo_get_selected   ( ofaLedgerCombo *combo );

void            ofa_ledger_combo_set_selected   ( ofaLedgerCombo *combo,
															const gchar *mnemo );

G_END_DECLS

#endif /* __OFA_LEDGER_COMBO_H__ */
