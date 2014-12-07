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
 * SECTION: ofo_dossier
 * @short_description: #ofoDossier class definition.
 * @include: api/ofo-dossier-def.h
 *
 * This class implements the Dossier behavior, including the general
 * DB definition.
 *
 * This file just define the class types, and targets referentially
 * the other header files which may need a forward declaration of the
 * #ofoDossier class.
 */

#include "api/ofo-base-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_DOSSIER                ( ofo_dossier_get_type())
#define OFO_DOSSIER( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_DOSSIER, ofoDossier ))
#define OFO_DOSSIER_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_DOSSIER, ofoDossierClass ))
#define OFO_IS_DOSSIER( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_DOSSIER ))
#define OFO_IS_DOSSIER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_DOSSIER ))
#define OFO_DOSSIER_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_DOSSIER, ofoDossierClass ))

typedef struct _ofoDossierPrivate       ofoDossierPrivate;

typedef struct {
	/*< public members >*/
	ofoBase            parent;

	/*< private members >*/
	ofoDossierPrivate *priv;
}
	ofoDossier;

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoDossierClass;

/**
 * Dossier signals:
 * @SIGNAL_DOSSIER_NEW_OBJECT:      sent on the dossier by one the #ofoBase
 *  class when a new object is inserted.
 * @SIGNAL_DOSSIER_UPDATED_OBJECT:  sent on the dossier by one the #ofoBase
 *  class when an existing object is updated.
 * @SIGNAL_DOSSIER_DELETED_OBJECT:  sent on the dossier by one the #ofoBase
 *  class when an existing object is deleted.
 * @SIGNAL_DOSSIER_RELOAD_DATASET:  sent on the dossier by one the #ofoBase
 *  class when the full dataset is reloaded.
 * @SIGNAL_DOSSIER_VALIDATED_ENTRY: sent on the dossier when an entry is
 *  validated.
 */
#define SIGNAL_DOSSIER_NEW_OBJECT       "ofa-signal-dossier-new-object"
#define SIGNAL_DOSSIER_UPDATED_OBJECT   "ofa-signal-dossier-updated-object"
#define SIGNAL_DOSSIER_DELETED_OBJECT   "ofa-signal-dossier-deleted-object"
#define SIGNAL_DOSSIER_RELOAD_DATASET   "ofa-signal-dossier-reload-dataset"
#define SIGNAL_DOSSIER_VALIDATED_ENTRY  "ofa-signal-dossier-validated-entry"

/* default length of exercice in months
 */
#define DOS_DEFAULT_LENGTH              12

GType ofo_dossier_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFO_DOSSIER_DEF_H__ */
