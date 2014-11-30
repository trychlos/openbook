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

#ifndef __MY_DATE_COMBO_H__
#define __MY_DATE_COMBO_H__

/**
 * SECTION: my_date_combo
 * @short_description: #myDateCombo class definition.
 * @include: ui/ofa-exercice-combo.h
 *
 * This class manages a combobox which display the known date formats.
 */

#include <gtk/gtk.h>

#include "api/my-date.h"

G_BEGIN_DECLS

#define MY_TYPE_DATE_COMBO                ( my_date_combo_get_type())
#define MY_DATE_COMBO( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_DATE_COMBO, myDateCombo ))
#define MY_DATE_COMBO_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_DATE_COMBO, myDateComboClass ))
#define MY_IS_DATE_COMBO( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_DATE_COMBO ))
#define MY_IS_DATE_COMBO_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_DATE_COMBO ))
#define MY_DATE_COMBO_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_DATE_COMBO, myDateComboClass ))

typedef struct _myDateComboPrivate        myDateComboPrivate;

typedef struct {
	/*< public members >*/
	GObject             parent;

	/*< private members >*/
	myDateComboPrivate *priv;
}
	myDateCombo;

typedef struct {
	/*< public members >*/
	GObjectClass        parent;
}
	myDateComboClass;

GType        my_date_combo_get_type    ( void ) G_GNUC_CONST;

myDateCombo *my_date_combo_new         ( void );

void         my_date_combo_attach_to   ( myDateCombo *combo, GtkContainer *new_parent );

void         my_date_combo_init_view   ( myDateCombo *combo, myDateFormat format );

myDateFormat my_date_combo_get_selected( myDateCombo *combo );

G_END_DECLS

#endif /* __MY_DATE_COMBO_H__ */
