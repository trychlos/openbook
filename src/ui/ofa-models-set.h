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

#ifndef __OFA_MODELS_SET_H__
#define __OFA_MODELS_SET_H__

/**
 * SECTION: ofa_models_set
 * @short_description: #ofaModelsSet class definition.
 * @include: ui/ofa-models-set.h
 *
 * Display entering models.
 */

#include "ui/ofa-main-page-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_MODELS_SET                ( ofa_models_set_get_type())
#define OFA_MODELS_SET( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MODELS_SET, ofaModelsSet ))
#define OFA_MODELS_SET_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MODELS_SET, ofaModelsSetClass ))
#define OFA_IS_MODELS_SET( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MODELS_SET ))
#define OFA_IS_MODELS_SET_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MODELS_SET ))
#define OFA_MODELS_SET_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MODELS_SET, ofaModelsSetClass ))

typedef struct _ofaModelsSetPrivate        ofaModelsSetPrivate;

typedef struct {
	/*< private >*/
	ofaMainPage          parent;
	ofaModelsSetPrivate *private;
}
	ofaModelsSet;

typedef struct {
	/*< private >*/
	ofaMainPageClass          parent;
}
	ofaModelsSetClass;

GType ofa_models_set_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_MODELS_SET_H__ */
