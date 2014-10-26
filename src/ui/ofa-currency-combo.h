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

#ifndef __OFA_CURRENCY_COMBO_H__
#define __OFA_CURRENCY_COMBO_H__

/**
 * SECTION: ofa_currency_combo
 * @short_description: #ofaCurrencyCombo class definition.
 * @include: ui/ofa-currency-combo.h
 *
 * A class to embed a Currencys combobox in a dialog.
 */

#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_CURRENCY_COMBO                ( ofa_currency_combo_get_type())
#define OFA_CURRENCY_COMBO( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_CURRENCY_COMBO, ofaCurrencyCombo ))
#define OFA_CURRENCY_COMBO_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_CURRENCY_COMBO, ofaCurrencyComboClass ))
#define OFA_IS_CURRENCY_COMBO( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_CURRENCY_COMBO ))
#define OFA_IS_CURRENCY_COMBO_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_CURRENCY_COMBO ))
#define OFA_CURRENCY_COMBO_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_CURRENCY_COMBO, ofaCurrencyComboClass ))

typedef struct _ofaCurrencyComboPrivate        ofaCurrencyComboPrivate;

typedef struct {
	/*< public members >*/
	GObject                  parent;

	/*< private members >*/
	ofaCurrencyComboPrivate *priv;
}
	ofaCurrencyCombo;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaCurrencyComboClass;

/**
 * ofaCurrencyComboCb:
 *
 * A callback to be triggered when a new currency is selected.
 *
 * Passed parameters are:
 * - ISO 3A code
 * - user_data provided at init_dialog() time
 */
typedef void ( *ofaCurrencyComboCb )( const gchar *, gpointer );

/**
 * ofsCurrencyComboParms
 *
 * The structure passed to the init_dialog() function.
 *
 * @container: the parent container of the target combo box
 * @dossier: the currently opened ofoDossier
 * @combo_name: the name of the GtkComboBox widget
 * @label_name: [allow-none]: the name of a GtkLabel widget which will
 *  receive the label of the selected currency each time the selection
 *  changes
 * @disp_code: whether the combo box should display the ISO 3A code
 * @disp_label: whether the combo box should display the label
 * @pfnSelected: [allow-none]: a user-provided callback which will be
 *  triggered on each selection change
 * @user_data: user-data passed to the callback
 * @initial_code: the ISO 3A identifier of the initially selected
 *  currency, or NULL
 */
typedef struct {
	GtkContainer     *container;
	ofoDossier       *dossier;
	const gchar      *combo_name;
	const gchar      *label_name;
	gboolean          disp_code;
	gboolean          disp_label;
	ofaCurrencyComboCb  pfnSelected;
	gpointer          user_data;
	const gchar      *initial_code;
}
	ofsCurrencyComboParms;

GType             ofa_currency_combo_get_type     ( void ) G_GNUC_CONST;

ofaCurrencyCombo *ofa_currency_combo_new          ( const ofsCurrencyComboParms *parms );

gint              ofa_currency_combo_get_selection( ofaCurrencyCombo *self, gchar **code, gchar **label );

G_END_DECLS

#endif /* __OFA_CURRENCY_COMBO_H__ */
