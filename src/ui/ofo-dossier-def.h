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

#ifndef __OFO_DOSSIER_DEF_H__
#define __OFO_DOSSIER_DEF_H__

/**
 * SECTION: ofo_dossier_def
 * @short_description: #ofoDossier class definition.
 * @include: ui/ofo-dossier-def.h
 *
 * This class implements the Dossier behavior, including the general
 * DB definition.
 *
 * This file just define the class types, and targets referentially
 * the other header files which may need a forward declaration of the
 * #ofoDossier class.
 */

#include "ui/ofo-base.h"

G_BEGIN_DECLS

#define OFO_TYPE_DOSSIER                ( ofo_dossier_get_type())
#define OFO_DOSSIER( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_DOSSIER, ofoDossier ))
#define OFO_DOSSIER_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_DOSSIER, ofoDossierClass ))
#define OFO_IS_DOSSIER( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_DOSSIER ))
#define OFO_IS_DOSSIER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_DOSSIER ))
#define OFO_DOSSIER_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_DOSSIER, ofoDossierClass ))

typedef struct {
	/*< private >*/
	ofoBaseClass parent;
}
	ofoDossierClass;

typedef struct _ofoDossierPrivate       ofoDossierPrivate;

typedef struct {
	/*< private >*/
	ofoBase            parent;
	ofoDossierPrivate *priv;
}
	ofoDossier;

G_END_DECLS

#endif /* __OFO_DOSSIER_DEF_H__ */
