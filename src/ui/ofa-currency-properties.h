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

#ifndef __OFA_CURRENCY_PROPERTIES_H__
#define __OFA_CURRENCY_PROPERTIES_H__

/**
 * SECTION: ofa_currency_properties
 * @short_description: #ofaCurrencyProperties class definition.
 * @include: ui/ofa-currency-properties.h
 *
 * Update the currency properties.
 */

#include "api/my-dialog.h"
#include "api/ofo-currency-def.h"

#include "ui/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_CURRENCY_PROPERTIES                ( ofa_currency_properties_get_type())
#define OFA_CURRENCY_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_CURRENCY_PROPERTIES, ofaCurrencyProperties ))
#define OFA_CURRENCY_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_CURRENCY_PROPERTIES, ofaCurrencyPropertiesClass ))
#define OFA_IS_CURRENCY_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_CURRENCY_PROPERTIES ))
#define OFA_IS_CURRENCY_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_CURRENCY_PROPERTIES ))
#define OFA_CURRENCY_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_CURRENCY_PROPERTIES, ofaCurrencyPropertiesClass ))

typedef struct _ofaCurrencyPropertiesPrivate        ofaCurrencyPropertiesPrivate;

typedef struct {
	/*< public members >*/
	myDialog                      parent;

	/*< private members >*/
	ofaCurrencyPropertiesPrivate *priv;
}
	ofaCurrencyProperties;

typedef struct {
	/*< public members >*/
	myDialogClass                 parent;
}
	ofaCurrencyPropertiesClass;

GType    ofa_currency_properties_get_type( void ) G_GNUC_CONST;

gboolean ofa_currency_properties_run     ( ofaMainWindow *parent, ofoCurrency *currency );

G_END_DECLS

#endif /* __OFA_CURRENCY_PROPERTIES_H__ */
