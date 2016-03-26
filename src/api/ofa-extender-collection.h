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

#ifndef __OPENBOOK_API_OFA_EXTENDER_COLLECTION_H__
#define __OPENBOOK_API_OFA_EXTENDER_COLLECTION_H__

/* @title: ofaExtenderCollection
 * @short_description: The ofaExtenderCollection class definition
 * @include: my/my-extender-collection.h
 *
 * The #ofaExtenderCollection class maintains the list of #ofaExtenderModule
 * loadable modules.
 */

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_EXTENDER_COLLECTION                ( ofa_extender_collection_get_type())
#define OFA_EXTENDER_COLLECTION( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_EXTENDER_COLLECTION, ofaExtenderCollection ))
#define OFA_EXTENDER_COLLECTION_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_EXTENDER_COLLECTION, ofaExtenderCollectionClass ))
#define OFA_IS_EXTENDER_COLLECTION( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_EXTENDER_COLLECTION ))
#define OFA_IS_EXTENDER_COLLECTION_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_EXTENDER_COLLECTION ))
#define OFA_EXTENDER_COLLECTION_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_EXTENDER_COLLECTION, ofaExtenderCollectionClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaExtenderCollection;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaExtenderCollectionClass;

GType                  ofa_extender_collection_get_type    ( void ) G_GNUC_CONST;

ofaExtenderCollection *ofa_extender_collection_new         ( GApplication *application,
																	const gchar *extension_dir );

GList                 *ofa_extender_collection_get_for_type( ofaExtenderCollection *collection,
																	GType type );

void                   ofa_extender_collection_free_types  ( GList *list );

const GList           *ofa_extender_collection_get_modules ( const ofaExtenderCollection *collection );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_EXTENDER_COLLECTION_H__ */
