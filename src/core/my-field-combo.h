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

#ifndef __MY_FIELD_COMBO_H__
#define __MY_FIELD_COMBO_H__

/**
 * SECTION: my_field_combo
 * @short_description: #myFieldCombo class definition.
 * @include: core/my-field-combo.h
 *
 * This class manages a combobox which display the available field
 * separators.
 *
 * The instanciated object is automatically unreffed when the parent
 * widget to which it has been attached to is destroyed.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_FIELD_COMBO                ( my_field_combo_get_type())
#define MY_FIELD_COMBO( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_FIELD_COMBO, myFieldCombo ))
#define MY_FIELD_COMBO_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_FIELD_COMBO, myFieldComboClass ))
#define MY_IS_FIELD_COMBO( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_FIELD_COMBO ))
#define MY_IS_FIELD_COMBO_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_FIELD_COMBO ))
#define MY_FIELD_COMBO_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_FIELD_COMBO, myFieldComboClass ))

typedef struct _myFieldComboPrivate        myFieldComboPrivate;

typedef struct {
	/*< public members >*/
	GtkComboBox          parent;

	/*< private members >*/
	myFieldComboPrivate *priv;
}
	myFieldCombo;

typedef struct {
	/*< public members >*/
	GtkComboBoxClass     parent;
}
	myFieldComboClass;

GType         my_field_combo_get_type    ( void ) G_GNUC_CONST;

myFieldCombo *my_field_combo_new         ( void );

gchar        *my_field_combo_get_selected( myFieldCombo *combo );

void          my_field_combo_set_selected( myFieldCombo *combo,
														const gchar *field_sep );

G_END_DECLS

#endif /* __MY_FIELD_COMBO_H__ */
