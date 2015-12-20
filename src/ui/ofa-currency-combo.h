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

#ifndef __OFA_CURRENCY_COMBO_H__
#define __OFA_CURRENCY_COMBO_H__

/**
 * SECTION: ofa_currency_combo
 * @short_description: #ofaCurrencyCombo class definition.
 * @include: ui/ofa-currency-combo.h
 *
 * A #GtkComboBox -derived class to manage currencies.
 *
 * The class defines a "changed" signal which is triggered when the
 * selected currency changes.
 */

#include "api/ofa-main-window-def.h"
#include "api/ofo-dossier-def.h"

#include "ui/ofa-currency-store.h"

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
	GtkComboBox              parent;

	/*< private members >*/
	ofaCurrencyComboPrivate *priv;
}
	ofaCurrencyCombo;

typedef struct {
	/*< public members >*/
	GtkComboBoxClass         parent;
}
	ofaCurrencyComboClass;

GType             ofa_currency_combo_get_type       ( void ) G_GNUC_CONST;

ofaCurrencyCombo *ofa_currency_combo_new            ( void );

void              ofa_currency_combo_set_columns    ( ofaCurrencyCombo *combo,
																ofaCurrencyColumns columns );

void              ofa_currency_combo_set_main_window( ofaCurrencyCombo *combo,
																const ofaMainWindow *main_window );

gchar            *ofa_currency_combo_get_selected   ( ofaCurrencyCombo *combo );

void              ofa_currency_combo_set_selected   ( ofaCurrencyCombo *combo,
																const gchar *code );

G_END_DECLS

#endif /* __OFA_CURRENCY_COMBO_H__ */
