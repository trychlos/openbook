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

#ifndef __MY_DECIMAL_COMBO_H__
#define __MY_DECIMAL_COMBO_H__

/**
 * SECTION: my_decimal_combo
 * @short_description: #myDecimalCombo class definition.
 * @include: ui/my-decimal-combo.h
 *
 * This class manages a combobox which display the available decimal
 * dot separators.
 *
 * The instanciated object is automatically unreffed when the parent
 * widget to which it has been attached to is destroyed.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_DECIMAL_COMBO                ( my_decimal_combo_get_type())
#define MY_DECIMAL_COMBO( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_DECIMAL_COMBO, myDecimalCombo ))
#define MY_DECIMAL_COMBO_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_DECIMAL_COMBO, myDecimalComboClass ))
#define MY_IS_DECIMAL_COMBO( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_DECIMAL_COMBO ))
#define MY_IS_DECIMAL_COMBO_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_DECIMAL_COMBO ))
#define MY_DECIMAL_COMBO_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_DECIMAL_COMBO, myDecimalComboClass ))

typedef struct _myDecimalComboPrivate        myDecimalComboPrivate;

typedef struct {
	/*< public members >*/
	GObject                parent;

	/*< private members >*/
	myDecimalComboPrivate *priv;
}
	myDecimalCombo;

typedef struct {
	/*< public members >*/
	GObjectClass           parent;
}
	myDecimalComboClass;

GType           my_decimal_combo_get_type    ( void ) G_GNUC_CONST;

myDecimalCombo *my_decimal_combo_new         ( void );

void            my_decimal_combo_attach_to   ( myDecimalCombo *combo,
														GtkContainer *new_parent );

gchar          *my_decimal_combo_get_selected( myDecimalCombo *combo );

void            my_decimal_combo_set_selected( myDecimalCombo *combo,
														const gchar *decimal_sep );

G_END_DECLS

#endif /* __MY_DECIMAL_COMBO_H__ */
