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

#ifndef __OFA_MODEL_PROPERTIES_H__
#define __OFA_MODEL_PROPERTIES_H__

/**
 * SECTION: ofa_model_properties
 * @short_description: #ofaModelProperties class definition.
 * @include: ui/ofa-model-properties.h
 *
 * Update the model properties.
 */

#include "ui/ofa-main-window.h"
#include "ui/ofo-model.h"

G_BEGIN_DECLS

#define OFA_TYPE_MODEL_PROPERTIES                ( ofa_model_properties_get_type())
#define OFA_MODEL_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MODEL_PROPERTIES, ofaModelProperties ))
#define OFA_MODEL_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MODEL_PROPERTIES, ofaModelPropertiesClass ))
#define OFA_IS_MODEL_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MODEL_PROPERTIES ))
#define OFA_IS_MODEL_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MODEL_PROPERTIES ))
#define OFA_MODEL_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MODEL_PROPERTIES, ofaModelPropertiesClass ))

typedef struct _ofaModelPropertiesPrivate        ofaModelPropertiesPrivate;

typedef struct {
	/*< private >*/
	GObject                    parent;
	ofaModelPropertiesPrivate *private;
}
	ofaModelProperties;

typedef struct _ofaModelPropertiesClassPrivate   ofaModelPropertiesClassPrivate;

typedef struct {
	/*< private >*/
	GObjectClass                    parent;
	ofaModelPropertiesClassPrivate *private;
}
	ofaModelPropertiesClass;

GType    ofa_model_properties_get_type( void );

gboolean ofa_model_properties_run     ( ofaMainWindow *parent, ofoModel *model );

G_END_DECLS

#endif /* __OFA_MODEL_PROPERTIES_H__ */
