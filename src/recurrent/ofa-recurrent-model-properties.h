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

#ifndef __OFA_RECURRENT_MODEL_PROPERTIES_H__
#define __OFA_RECURRENT_MODEL_PROPERTIES_H__

/**
 * SECTION: ofa_recurrent_model_properties
 * @short_description: #ofaRecurrentModelProperties class definition.
 * @include: recurrent/ofa-recurrent_model-properties.h
 *
 * Create a new/Update the recurrent model properties.
 *
 * The content of the provided ofoRecurrentModel object is not modified until the
 * do_update() function. At this time, all its content is _replaced_
 * with which is found in the dialog box.
 *
 * Development rules:
 * - type:       non-modal dialog
 * - settings:   yes
 * - current:    yes
 */

#include "api/ofa-igetter-def.h"

#include "ofo-recurrent-model.h"

G_BEGIN_DECLS

#define OFA_TYPE_RECURRENT_MODEL_PROPERTIES                ( ofa_recurrent_model_properties_get_type())
#define OFA_RECURRENT_MODEL_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RECURRENT_MODEL_PROPERTIES, ofaRecurrentModelProperties ))
#define OFA_RECURRENT_MODEL_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RECURRENT_MODEL_PROPERTIES, ofaRecurrentModelPropertiesClass ))
#define OFA_IS_RECURRENT_MODEL_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RECURRENT_MODEL_PROPERTIES ))
#define OFA_IS_RECURRENT_MODEL_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RECURRENT_MODEL_PROPERTIES ))
#define OFA_RECURRENT_MODEL_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RECURRENT_MODEL_PROPERTIES, ofaRecurrentModelPropertiesClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaRecurrentModelProperties;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaRecurrentModelPropertiesClass;

GType ofa_recurrent_model_properties_get_type( void ) G_GNUC_CONST;

void  ofa_recurrent_model_properties_run     ( ofaIGetter *getter,
													GtkWindow *parent,
													ofoRecurrentModel *model );

G_END_DECLS

#endif /* __OFA_RECURRENT_MODEL_PROPERTIES_H__ */
