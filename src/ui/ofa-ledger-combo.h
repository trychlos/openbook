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
 * A class to embed a Ledgers combobox in a dialog.
 */

#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_LEDGER_COMBO                ( ofa_ledger_combo_get_type())
#define OFA_LEDGER_COMBO( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_LEDGER_COMBO, ofaLedgerCombo ))
#define OFA_LEDGER_COMBO_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_LEDGER_COMBO, ofaLedgerComboClass ))
#define OFA_IS_LEDGER_COMBO( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_LEDGER_COMBO ))
#define OFA_IS_LEDGER_COMBO_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_LEDGER_COMBO ))
#define OFA_LEDGER_COMBO_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_LEDGER_COMBO, ofaLedgerComboClass ))

typedef struct _ofaLedgerComboPrivate        ofaLedgerComboPrivate;

typedef struct {
	/*< private >*/
	GObject                parent;
	ofaLedgerComboPrivate *private;
}
	ofaLedgerCombo;

typedef struct {
	/*< private >*/
	GObjectClass parent;
}
	ofaLedgerComboClass;

/**
 * ofaLedgerComboCb:
 *
 * A callback to be triggered when a new journal is selected.
 *
 * Passed parameters are:
 * - mnemo
 * - user_data provided at init_dialog() time
 */
typedef void ( *ofaLedgerComboCb )            ( const gchar *, gpointer );

GType           ofa_ledger_combo_get_type     ( void ) G_GNUC_CONST;

/**
 * ofaLedgerComboParms
 *
 * The structure passed to the init_dialog() function.
 *
 * @container: the parent container of the target combo box
 * @dossier: the current opened ofoDossier
 * @combo_name: the name of the GtkComboBox widget
 * @label_name: [allow-none]: the name of a GtkLabel widget which will
 *  receive the label of the selected journal each time the selection
 *  changes
 * @disp_mnemo: whether the combo box should display the mnemo
 * @disp_label: whether the combo box should display the label
 * @pfnSelected: [allow-none]: a user-provided callback which will be
 *  triggered on each selection change
 * @user_data: user-data passed to the callback
 * @initial_mnemo: the journal identifier of the initial selection, or
 *  NULL
 */
typedef struct {
	GtkContainer     *container;
	ofoDossier       *dossier;
	const gchar      *combo_name;
	const gchar      *label_name;
	gboolean          disp_mnemo;
	gboolean          disp_label;
	ofaLedgerComboCb pfnSelected;
	gpointer          user_data;
	const gchar      *initial_mnemo;
}
	ofaLedgerComboParms;

ofaLedgerCombo *ofa_ledger_combo_new          ( const ofaLedgerComboParms *parms );

gint            ofa_ledger_combo_get_selection( ofaLedgerCombo *self, gchar **mnemo, gchar **label );

void            ofa_ledger_combo_set_selection( ofaLedgerCombo *self, const gchar *mnemo );

G_END_DECLS

#endif /* __OFA_LEDGER_COMBO_H__ */
