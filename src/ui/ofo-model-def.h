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

#ifndef __OFO_MODEL_DEF_H__
#define __OFO_MODEL_DEF_H__

/**
 * SECTION: ofo_model
 * @short_description: #ofoModel class definition.
 * @include: ui/ofo-model.h
 *
 * This class implements the Model behavior
 */

#include "ui/ofo-base-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_MODEL                ( ofo_model_get_type())
#define OFO_MODEL( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_MODEL, ofoModel ))
#define OFO_MODEL_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_MODEL, ofoModelClass ))
#define OFO_IS_MODEL( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_MODEL ))
#define OFO_IS_MODEL_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_MODEL ))
#define OFO_MODEL_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_MODEL, ofoModelClass ))

typedef struct {
	/*< private >*/
	ofoBaseClass parent;
}
	ofoModelClass;

typedef struct _ofoModelPrivate       ofoModelPrivate;

typedef struct {
	/*< private >*/
	ofoBase          parent;
	ofoModelPrivate *private;
}
	ofoModel;

G_END_DECLS

#endif /* __OFO_MODEL_DEF_H__ */
