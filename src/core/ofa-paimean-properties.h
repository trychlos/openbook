/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_PAIMEAN_PROPERTIES_H__
#define __OFA_PAIMEAN_PROPERTIES_H__

/**
 * SECTION: ofa_paimean_properties
 * @short_description: #ofaPaimeanProperties class definition.
 * @include: core/ofa-paimean-properties.h
 *
 * Update the #ofoPaimean properties.
 *
 * Development rules:
 * - type:       modal/non-modal depending of the caller.
 * - settings:   yes
 * - current:    yes
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"
#include "api/ofo-paimean.h"

G_BEGIN_DECLS

#define OFA_TYPE_PAIMEAN_PROPERTIES                ( ofa_paimean_properties_get_type())
#define OFA_PAIMEAN_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PAIMEAN_PROPERTIES, ofaPaimeanProperties ))
#define OFA_PAIMEAN_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PAIMEAN_PROPERTIES, ofaPaimeanPropertiesClass ))
#define OFA_IS_PAIMEAN_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PAIMEAN_PROPERTIES ))
#define OFA_IS_PAIMEAN_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PAIMEAN_PROPERTIES ))
#define OFA_PAIMEAN_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PAIMEAN_PROPERTIES, ofaPaimeanPropertiesClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaPaimeanProperties;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaPaimeanPropertiesClass;

GType ofa_paimean_properties_get_type( void ) G_GNUC_CONST;

void  ofa_paimean_properties_run     ( ofaIGetter *getter,
											GtkWindow *parent,
											ofoPaimean *paimean );

G_END_DECLS

#endif /* __OFA_PAIMEAN_PROPERTIES_H__ */
