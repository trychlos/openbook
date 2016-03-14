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

#ifndef __OFA_PERIODICITY_COMBO_H__
#define __OFA_PERIODICITY_COMBO_H__

/**
 * SECTION: ofa_periodicity_combo
 * @short_description: #ofaPeriodicityCombo class definition.
 * @include: api/ofa-periodicity-combo.h
 *
 * A #GtkComboBox -derived class to manage periodicities.
 *
 * The class defines an "ofa-changed" signal which is triggered when the
 * selected periodicity changes.
 */

#include "api/ofa-hub-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_PERIODICITY_COMBO                ( ofa_periodicity_combo_get_type())
#define OFA_PERIODICITY_COMBO( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PERIODICITY_COMBO, ofaPeriodicityCombo ))
#define OFA_PERIODICITY_COMBO_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PERIODICITY_COMBO, ofaPeriodicityComboClass ))
#define OFA_IS_PERIODICITY_COMBO( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PERIODICITY_COMBO ))
#define OFA_IS_PERIODICITY_COMBO_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PERIODICITY_COMBO ))
#define OFA_PERIODICITY_COMBO_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PERIODICITY_COMBO, ofaPeriodicityComboClass ))

typedef struct _ofaPeriodicityComboPrivate        ofaPeriodicityComboPrivate;

typedef struct {
	/*< public members >*/
	GtkComboBox      parent;
}
	ofaPeriodicityCombo;

typedef struct {
	/*< public members >*/
	GtkComboBoxClass parent;
}
	ofaPeriodicityComboClass;

GType                ofa_periodicity_combo_get_type    ( void ) G_GNUC_CONST;

ofaPeriodicityCombo *ofa_periodicity_combo_new         ( void );

gchar               *ofa_periodicity_combo_get_selected( ofaPeriodicityCombo *combo );

void                 ofa_periodicity_combo_set_selected( ofaPeriodicityCombo *combo,
															const gchar *periodicity );

G_END_DECLS

#endif /* __OFA_PERIODICITY_COMBO_H__ */
