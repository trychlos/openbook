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
 */

#ifndef __OPENBOOK_OFO_CONCIL_DEF_H__
#define __OPENBOOK_OFO_CONCIL_DEF_H__

/**
 * SECTION: ofo_concil
 * @short_description: #ofoConcil class definition.
 * @include: openbook/ofo-concil.h
 *
 * This file defines the #ofoConcil public API.
 *
 * The #ofoConcil class maintains the conciliations between BAT
 * lines and entries.
 */

#include "api/ofo-base-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_CONCIL                ( ofo_concil_get_type())
#define OFO_CONCIL( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_CONCIL, ofoConcil ))
#define OFO_CONCIL_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_CONCIL, ofoConcilClass ))
#define OFO_IS_CONCIL( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_CONCIL ))
#define OFO_IS_CONCIL_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_CONCIL ))
#define OFO_CONCIL_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_CONCIL, ofoConcilClass ))

typedef struct _ofoConcilPrivate       ofoConcilPrivate;

typedef struct {
	/*< public members >*/
	ofoBase           parent;

	/*< private members >*/
	ofoConcilPrivate *priv;
}
	ofoConcil;

typedef struct {
	/*< public members >*/
	ofoBaseClass      parent;
}
	ofoConcilClass;

GType ofo_concil_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OPENBOOK_OFO_CONCIL_DEF_H__ */
