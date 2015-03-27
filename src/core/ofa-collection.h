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

#ifndef __OFA_COLLECTION_H__
#define __OFA_COLLECTION_H__

/**
 * SECTION: ofa_collection
 * @short_description: #ofaCollection class definition.
 * @include: core/ofa-collection.h
 *
 * This file defines the #ofaCollection public API.
 *
 * The #ofaCollection class is a base class for classes which wish
 * maintain a dossier-wide collection of objects. These collection
 * classes share common common point:
 *
 * - they are thought to handle a collection of data both class-wide
 *   while keeping attached to the dossier
 *
 * - they should have any new() method to create the collection,
 *   as it is automatically created by the dossier on first get
 *   request
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_COLLECTION                ( ofa_collection_get_type())
#define OFA_COLLECTION( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_COLLECTION, ofaCollection ))
#define OFA_COLLECTION_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_COLLECTION, ofaCollectionClass ))
#define OFA_IS_COLLECTION( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_COLLECTION ))
#define OFA_IS_COLLECTION_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_COLLECTION ))
#define OFA_COLLECTION_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_COLLECTION, ofaCollectionClass ))

typedef struct _ofaCollectionProtected     ofaCollectionProtected;
typedef struct _ofaCollectionPrivate       ofaCollectionPrivate;

typedef struct {
	/*< public members >*/
	GObject                 parent;

	/*< protected members >*/
	ofaCollectionProtected *prot;

	/*< private members >*/
	ofaCollectionPrivate   *priv;
}
	ofaCollection;

typedef struct {
	/*< public members >*/
	GObjectClass            parent;
}
	ofaCollectionClass;

GType ofa_collection_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_COLLECTION_H__ */
