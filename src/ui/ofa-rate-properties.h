/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_RATE_PROPERTIES_H__
#define __OFA_RATE_PROPERTIES_H__

/**
 * SECTION: ofa_rate_properties
 * @short_description: #ofaRateProperties class definition.
 * @include: ui/ofa-rate-properties.h
 *
 * Update the rate properties.
 *
 * From the ofaRateSet page, create a new rate, or update an existing
 * one. in the two cases, zero, one or more validities can be created,
 * updated, deleted.
 *
 * The content of the provided ofoRate object is not modified until the
 * do_update() function. At this time, all its content is _replaced_
 * with which is found in the dialog box.
 *
 * When creating a new validity, we take care of checking that it
 * doesn't override an already existing validity period.
 *
 * Examples:
 * Existing validity
 * (null)     (null)   impossible to create a new period because there
 *                     is no place
 * (null)   31/12/2013 it is possible to create a new period starting
 *                     with 01/01/2014
 *
 * Whether an error be detected or not at recording time, the dialog
 * terminates on OK, maybe after having displayed an error message box.
 *
 * Development rules:
 * - type:               non-modal dialog
 * - message on success: no
 * - settings:           yes
 * - current:            yes
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"
#include "api/ofo-rate-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_RATE_PROPERTIES                ( ofa_rate_properties_get_type())
#define OFA_RATE_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RATE_PROPERTIES, ofaRateProperties ))
#define OFA_RATE_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RATE_PROPERTIES, ofaRatePropertiesClass ))
#define OFA_IS_RATE_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RATE_PROPERTIES ))
#define OFA_IS_RATE_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RATE_PROPERTIES ))
#define OFA_RATE_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RATE_PROPERTIES, ofaRatePropertiesClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaRateProperties;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaRatePropertiesClass;

GType ofa_rate_properties_get_type( void ) G_GNUC_CONST;

void  ofa_rate_properties_run     ( ofaIGetter *getter,
										GtkWindow *parent,
										ofoRate *rate );

G_END_DECLS

#endif /* __OFA_RATE_PROPERTIES_H__ */
