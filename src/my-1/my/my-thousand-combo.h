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

#ifndef __MY_API_MY_THOUSAND_COMBO_H__
#define __MY_API_MY_THOUSAND_COMBO_H__

/**
 * SECTION: my_thousand_combo
 * @short_description: #myThousandCombo class definition.
 * @include: my/my-thousand-combo.h
 *
 * This class manages a combobox which display the available thousand
 * separators.
 *
 * The instanciated object is automatically unreffed when the parent
 * widget to which it has been attached to is destroyed.
 *
 * The 'my-changed' signal is sent on selection change.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_THOUSAND_COMBO                ( my_thousand_combo_get_type())
#define MY_THOUSAND_COMBO( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_THOUSAND_COMBO, myThousandCombo ))
#define MY_THOUSAND_COMBO_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_THOUSAND_COMBO, myThousandComboClass ))
#define MY_IS_THOUSAND_COMBO( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_THOUSAND_COMBO ))
#define MY_IS_THOUSAND_COMBO_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_THOUSAND_COMBO ))
#define MY_THOUSAND_COMBO_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_THOUSAND_COMBO, myThousandComboClass ))

typedef struct {
	/*< public members >*/
	GtkComboBox      parent;
}
	myThousandCombo;

typedef struct {
	/*< public members >*/
	GtkComboBoxClass parent;
}
	myThousandComboClass;

GType            my_thousand_combo_get_type    ( void ) G_GNUC_CONST;

myThousandCombo *my_thousand_combo_new         ( void );

gchar           *my_thousand_combo_get_selected( myThousandCombo *combo );

void             my_thousand_combo_set_selected( myThousandCombo *combo,
													const gchar *thousand_sep );

G_END_DECLS

#endif /* __MY_API_MY_THOUSAND_COMBO_H__ */
