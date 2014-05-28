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

#ifndef __OFO_TAUX_DEF_H__
#define __OFO_TAUX_DEF_H__

/**
 * SECTION: ofo_taux
 * @short_description: #ofoTaux class definition.
 * @include: ui/ofo-taux.h
 *
 * This class implements the Taux behavior.
 */

#include "ui/ofo-base-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_TAUX                ( ofo_taux_get_type())
#define OFO_TAUX( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_TAUX, ofoTaux ))
#define OFO_TAUX_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_TAUX, ofoTauxClass ))
#define OFO_IS_TAUX( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_TAUX ))
#define OFO_IS_TAUX_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_TAUX ))
#define OFO_TAUX_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_TAUX, ofoTauxClass ))

typedef struct {
	/*< private >*/
	ofoBaseClass parent;
}
	ofoTauxClass;

typedef struct _ofoTauxPrivate       ofoTauxPrivate;

typedef struct {
	/*< private >*/
	ofoBase         parent;
	ofoTauxPrivate *priv;
}
	ofoTaux;

G_END_DECLS

#endif /* __OFO_TAUX_DEF_H__ */
