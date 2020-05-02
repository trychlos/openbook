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

#ifndef __OFA_GUIDED_EX_H__
#define __OFA_GUIDED_EX_H__

/**
 * SECTION: ofa_guided_ex
 * @short_description: #ofaGuidedEx class definition.
 * @include: ui/ofa-guided-ex.h
 */

#include "api/ofa-paned-page.h"

G_BEGIN_DECLS

#define OFA_TYPE_GUIDED_EX                ( ofa_guided_ex_get_type())
#define OFA_GUIDED_EX( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_GUIDED_EX, ofaGuidedEx ))
#define OFA_GUIDED_EX_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_GUIDED_EX, ofaGuidedExClass ))
#define OFA_IS_GUIDED_EX( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_GUIDED_EX ))
#define OFA_IS_GUIDED_EX_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_GUIDED_EX ))
#define OFA_GUIDED_EX_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_GUIDED_EX, ofaGuidedExClass ))

typedef struct {
	/*< public members >*/
	ofaPanedPage      parent;
}
	ofaGuidedEx;

typedef struct {
	/*< public members >*/
	ofaPanedPageClass parent;
}
	ofaGuidedExClass;

GType ofa_guided_ex_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_GUIDED_EX_H__ */
