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

#ifndef __OPENBOOK_API_OFA_ICOLLECTIONABLE_H__
#define __OPENBOOK_API_OFA_ICOLLECTIONABLE_H__

/**
 * SECTION: icollectionable
 * @title: ofaICollectionable
 * @short_description: The ICollectionable Interface
 * @include: openbook/ofa-icollectionable.h
 *
 * The #ofaICollectionable interface should be implemented by all the
 * classes which wish the the #ofaICollector implementation maintains
 * a collection of their objects.
 */

#include "api/ofa-hub-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ICOLLECTIONABLE                      ( ofa_icollectionable_get_type())
#define OFA_ICOLLECTIONABLE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ICOLLECTIONABLE, ofaICollectionable ))
#define OFA_IS_ICOLLECTIONABLE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ICOLLECTIONABLE ))
#define OFA_ICOLLECTIONABLE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ICOLLECTIONABLE, ofaICollectionableInterface ))

typedef struct _ofaICollectionable                    ofaICollectionable;

/**
 * ofaICollectionableInterface:
 * @get_interface_version: [should]: get the version number of the
 *                                   interface implementation.
 *
 * This defines the interface that an #ofaICollectionable may/should
 * implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/*** implementation-wide ***/
	/**
	 * get_interface_version:
	 *
	 * Returns: the version number of this interface which is managed
	 * by the implementation.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1
	 */
	guint   ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * load_collection:
	 * @instance: a fake #ofaICollectionable instance.
	 * @hub: the #ofaHub object.
	 *
	 * Returns: the list of objects of the collection, or %NULL.
	 *
	 * Since: version 1.
	 */
	GList * ( *load_collection )      ( const ofaICollectionable *instance,
											ofaHub *hub );
}
	ofaICollectionableInterface;

/*
 * Interface-wide
 */
GType  ofa_icollectionable_get_type                  ( void );

guint  ofa_icollectionable_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint  ofa_icollectionable_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
GList *ofa_icollectionable_load_collection           ( const ofaICollectionable *instance,
															ofaHub *hub );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ICOLLECTIONABLE_H__ */
