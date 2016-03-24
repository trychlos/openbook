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

#ifndef __OFA_CLASS_PROPERTIES_H__
#define __OFA_CLASS_PROPERTIES_H__

/**
 * SECTION: ofa_class_properties
 * @short_description: #ofaClassProperties class definition.
 * @include: ui/ofa-class-properties.h
 *
 * Update the class properties.
 *
 * Development rules:
 * - type:       non-modal dialog
 * - settings:   no
 * - current:    yes
 */

#include "api/ofa-main-window-def.h"
#include "api/ofo-class-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_CLASS_PROPERTIES                ( ofa_class_properties_get_type())
#define OFA_CLASS_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_CLASS_PROPERTIES, ofaClassProperties ))
#define OFA_CLASS_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_CLASS_PROPERTIES, ofaClassPropertiesClass ))
#define OFA_IS_CLASS_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_CLASS_PROPERTIES ))
#define OFA_IS_CLASS_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_CLASS_PROPERTIES ))
#define OFA_CLASS_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_CLASS_PROPERTIES, ofaClassPropertiesClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaClassProperties;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaClassPropertiesClass;

GType ofa_class_properties_get_type( void ) G_GNUC_CONST;

void  ofa_class_properties_run     ( const ofaMainWindow *parent,
										ofoClass *devise );

G_END_DECLS

#endif /* __OFA_CLASS_PROPERTIES_H__ */
