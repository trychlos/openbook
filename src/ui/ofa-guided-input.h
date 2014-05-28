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

#ifndef __OFA_GUIDED_INPUT_H__
#define __OFA_GUIDED_INPUT_H__

/**
 * SECTION: ofa_guided_input
 * @short_description: #ofaGuidedInput class definition.
 * @include: ui/ofa-guided-input.h
 */

#include "ui/ofa-main-window-def.h"
#include "ui/ofo-model-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_GUIDED_INPUT                ( ofa_guided_input_get_type())
#define OFA_GUIDED_INPUT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_GUIDED_INPUT, ofaGuidedInput ))
#define OFA_GUIDED_INPUT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_GUIDED_INPUT, ofaGuidedInputClass ))
#define OFA_IS_GUIDED_INPUT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_GUIDED_INPUT ))
#define OFA_IS_GUIDED_INPUT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_GUIDED_INPUT ))
#define OFA_GUIDED_INPUT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_GUIDED_INPUT, ofaGuidedInputClass ))

typedef struct _ofaGuidedInputPrivate        ofaGuidedInputPrivate;

typedef struct {
	/*< private >*/
	GObject                parent;
	ofaGuidedInputPrivate *private;
}
	ofaGuidedInput;

typedef struct _ofaGuidedInputClassPrivate   ofaGuidedInputClassPrivate;

typedef struct {
	/*< private >*/
	GObjectClass                parent;
	ofaGuidedInputClassPrivate *private;
}
	ofaGuidedInputClass;

GType ofa_guided_input_get_type( void );

void  ofa_guided_input_run     ( ofaMainWindow *parent, const ofoModel *model );

G_END_DECLS

#endif /* __OFA_GUIDED_INPUT_H__ */
