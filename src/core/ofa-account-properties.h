/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_ACCOUNT_PROPERTIES_H__
#define __OFA_ACCOUNT_PROPERTIES_H__

/**
 * SECTION: ofa_account_properties
 * @short_description: #ofaAccountProperties class definition.
 * @include: core/ofa-account-properties.h
 *
 * Update properties of an account.
 *
 * Development rules:
 * - type:       modal/non-modal dialog depending of the caller
 * - settings:   yes
 * - current:    yes
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"
#include "api/ofo-account-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNT_PROPERTIES                ( ofa_account_properties_get_type())
#define OFA_ACCOUNT_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNT_PROPERTIES, ofaAccountProperties ))
#define OFA_ACCOUNT_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNT_PROPERTIES, ofaAccountPropertiesClass ))
#define OFA_IS_ACCOUNT_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNT_PROPERTIES ))
#define OFA_IS_ACCOUNT_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNT_PROPERTIES ))
#define OFA_ACCOUNT_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNT_PROPERTIES, ofaAccountPropertiesClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaAccountProperties;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaAccountPropertiesClass;

GType ofa_account_properties_get_type( void ) G_GNUC_CONST;

void  ofa_account_properties_run     ( ofaIGetter *getter,
											GtkWindow *parent,
											ofoAccount *account );

G_END_DECLS

#endif /* __OFA_ACCOUNT_PROPERTIES_H__ */
