/*
 * Open Firm Accounting
 * A double-bat-line accounting application for professional services.
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

#ifndef __OFA_BATLINE_PROPERTIES_H__
#define __OFA_BATLINE_PROPERTIES_H__

/**
 * SECTION: ofa_batline_properties
 * @short_description: #ofaBatlineProperties class definition.
 * @include: core/ofa-batline-properties.h
 *
 * Update/display the #ofoBatLine properties.
 *
 * Development rules:
 * - type:       non-modal dialog
 * - settings:   no
 * - current:    yes
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"
#include "api/ofo-bat-line.h"

G_BEGIN_DECLS

#define OFA_TYPE_BATLINE_PROPERTIES                ( ofa_batline_properties_get_type())
#define OFA_BATLINE_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BATLINE_PROPERTIES, ofaBatlineProperties ))
#define OFA_BATLINE_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BATLINE_PROPERTIES, ofaBatlinePropertiesClass ))
#define OFA_IS_BATLINE_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BATLINE_PROPERTIES ))
#define OFA_IS_BATLINE_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BATLINE_PROPERTIES ))
#define OFA_BATLINE_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BATLINE_PROPERTIES, ofaBatlinePropertiesClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaBatlineProperties;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaBatlinePropertiesClass;

GType ofa_batline_properties_get_type( void ) G_GNUC_CONST;

void  ofa_batline_properties_run     ( ofaIGetter *getter,
											GtkWindow *parent,
											ofoBatLine *batline );

G_END_DECLS

#endif /* __OFA_BATLINE_PROPERTIES_H__ */
