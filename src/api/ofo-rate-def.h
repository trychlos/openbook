/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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
 */

#ifndef __OFO_RATE_DEF_H__
#define __OFO_RATE_DEF_H__

/**
 * SECTION: ofo_rate
 * @short_description: #ofoRate class definition.
 * @include: api/ofo-rate.h
 *
 * This file implements the #ofoRate class behavior.
 */

#include "api/ofo-base-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_RATE                ( ofo_rate_get_type())
#define OFO_RATE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_RATE, ofoRate ))
#define OFO_RATE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_RATE, ofoRateClass ))
#define OFO_IS_RATE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_RATE ))
#define OFO_IS_RATE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_RATE ))
#define OFO_RATE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_RATE, ofoRateClass ))

typedef struct _ofoRatePrivate       ofoRatePrivate;

typedef struct {
	/*< public members >*/
	ofoBase         parent;

	/*< private members >*/
	ofoRatePrivate *priv;
}
	ofoRate;

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoRateClass;

GType ofo_rate_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFO_RATE_DEF_H__ */
