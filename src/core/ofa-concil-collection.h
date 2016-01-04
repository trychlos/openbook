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

#ifndef __OFA_CONCIL_COLLECTION_H__
#define __OFA_CONCIL_COLLECTION_H__

/**
 * SECTION: ofa_concil_collection
 * @short_description: #ofaConcilCollection class definition.
 * @include: core/ofa-concil-collection.h
 *
 * This file defines the #ofaConcilCollection public API.
 *
 * The #ofaConcilCollection class maintains the current collectionion of
 * the reconciliation groups which have been loaded from the database.
 *
 * The collectionion is incremented when the application requires for
 * some information about a reconciliation group, e.g. when wanting
 * to know is an entry is reconciliated.
 *
 * The collectionion auto maintains itself by connecting to the dossier
 * signaling system.
 */

#include "api/ofa-box.h"
#include "api/ofo-concil-def.h"
#include "api/ofo-dossier-def.h"

#include "core/ofa-collection.h"

G_BEGIN_DECLS

#define OFA_TYPE_CONCIL_COLLECTION                ( ofa_concil_collection_get_type())
#define OFA_CONCIL_COLLECTION( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_CONCIL_COLLECTION, ofaConcilCollection ))
#define OFA_CONCIL_COLLECTION_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_CONCIL_COLLECTION, ofaConcilCollectionClass ))
#define OFA_IS_CONCIL_COLLECTION( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_CONCIL_COLLECTION ))
#define OFA_IS_CONCIL_COLLECTION_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_CONCIL_COLLECTION ))
#define OFA_CONCIL_COLLECTION_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_CONCIL_COLLECTION, ofaConcilCollectionClass ))

typedef struct _ofaConcilCollectionPrivate        ofaConcilCollectionPrivate;

typedef struct {
	/*< public members >*/
	ofaCollection               parent;

	/*< private members >*/
	ofaConcilCollectionPrivate *priv;
}
	ofaConcilCollection;

typedef struct {
	/*< public members >*/
	ofaCollectionClass          parent;
}
	ofaConcilCollectionClass;

GType      ofa_concil_collection_get_type       ( void ) G_GNUC_CONST;

ofoConcil *ofa_concil_collection_get_by_id      ( ofxCounter concil_id,
														ofoDossier *dossier );

ofoConcil *ofa_concil_collection_get_by_other_id( ofaConcilCollection *collection,
														const gchar *type,
														ofxCounter id,
														ofoDossier *dossier );

void       ofa_concil_collection_add            ( ofaConcilCollection *collection,
														ofoConcil *concil );

void       ofa_concil_collection_remove         ( ofaConcilCollection *collection,
														ofoConcil *concil );

G_END_DECLS

#endif /* __OFA_CONCIL_COLLECTION_H__ */
