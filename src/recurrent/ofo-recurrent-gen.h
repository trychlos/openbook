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

#ifndef __OFO_RECURRENT_GEN_H__
#define __OFO_RECURRENT_GEN_H__

/**
 * SECTION: ofo_recurrent_gen
 * @short_description: #ofoRecurrentGen class definition.
 * @include: recurrent/ofo-recurrent-gen.h
 *
 * This file defines the #ofoRecurrentGen class behavior.
 */

#include "api/ofa-hub-def.h"
#include "api/ofo-base-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_RECURRENT_GEN                ( ofo_recurrent_gen_get_type())
#define OFO_RECURRENT_GEN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_RECURRENT_GEN, ofoRecurrentGen ))
#define OFO_RECURRENT_GEN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_RECURRENT_GEN, ofoRecurrentGenClass ))
#define OFO_IS_RECURRENT_GEN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_RECURRENT_GEN ))
#define OFO_IS_RECURRENT_GEN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_RECURRENT_GEN ))
#define OFO_RECURRENT_GEN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_RECURRENT_GEN, ofoRecurrentGenClass ))

typedef struct {
	/*< public members >*/
	ofoBase      parent;
}
	ofoRecurrentGen;

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoRecurrentGenClass;

#define RECURRENT_ROW_ID                1

GType        ofo_recurrent_gen_get_type         ( void ) G_GNUC_CONST;

const GDate *ofo_recurrent_gen_get_last_run_date( ofaHub *hub );

void         ofo_recurrent_gen_set_last_run_date( ofaHub *hub, const GDate *date );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_RECURRENT_GEN_H__ */
