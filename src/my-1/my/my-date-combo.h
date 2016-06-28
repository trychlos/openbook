/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifndef __MY_API_MY_DATE_COMBO_H__
#define __MY_API_MY_DATE_COMBO_H__

/**
 * SECTION: my_date_combo
 * @short_description: #myDateCombo class definition.
 * @include: my/my-date-combo.h
 *
 * This class manages a combo box which display the known date formats.
 *
 * The instanciated object is automatically unreffed when the parent
 * widget to which it has been attached to is destroyed.
 *
 * The 'my-changed' signal is sent on selection change.
 */

#include <gtk/gtk.h>

#include <my/my-date.h>

G_BEGIN_DECLS

#define MY_TYPE_DATE_COMBO                ( my_date_combo_get_type())
#define MY_DATE_COMBO( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_DATE_COMBO, myDateCombo ))
#define MY_DATE_COMBO_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_DATE_COMBO, myDateComboClass ))
#define MY_IS_DATE_COMBO( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_DATE_COMBO ))
#define MY_IS_DATE_COMBO_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_DATE_COMBO ))
#define MY_DATE_COMBO_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_DATE_COMBO, myDateComboClass ))

typedef struct {
	/*< public members >*/
	GtkComboBox      parent;
}
	myDateCombo;

typedef struct {
	/*< public members >*/
	GtkComboBoxClass parent;
}
	myDateComboClass;

GType        my_date_combo_get_type    ( void ) G_GNUC_CONST;

myDateCombo *my_date_combo_new         ( void );

myDateFormat my_date_combo_get_selected( myDateCombo *combo );

void         my_date_combo_set_selected( myDateCombo *combo,
												myDateFormat format );

G_END_DECLS

#endif /* __MY_API_MY_DATE_COMBO_H__ */
