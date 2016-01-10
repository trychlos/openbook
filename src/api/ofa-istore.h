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

#ifndef __OPENBOOK_API_OFA_ISTORE_H__
#define __OPENBOOK_API_OFA_ISTORE_H__

/**
 * SECTION: istore
 * @title: ofaIStore
 * @short_description: The IStore Interface
 * @include: openbook/ofa-istore.h
 *
 * The #ofaIStore interface is implemented by #ofaListStore and
 * #ofaTreeStore. It lets us implement some common behavior between
 * all our stores.
 */

#include "api/ofa-hub-def.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ISTORE                      ( ofa_istore_get_type())
#define OFA_ISTORE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ISTORE, ofaIStore ))
#define OFA_IS_ISTORE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ISTORE ))
#define OFA_ISTORE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ISTORE, ofaIStoreInterface ))

typedef struct _ofaIStore                    ofaIStore;

/**
 * ofaIStoreInterface:
 *
 * This defines the interface that an #ofaIStore should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIStore provider.
	 *
	 * The interface code calls this method each time it needs to know
	 * which version of this interface the application implements.
	 *
	 * If this method is not implemented by the application, then the
	 * interface code considers that the application only implements
	 * the version 1 of the ofaIStore interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the application is supporting.
	 *
	 * Defaults to 1.
	 */
	guint ( *get_interface_version )( const ofaIStore *instance );
}
	ofaIStoreInterface;

GType        ofa_istore_get_type             ( void );

guint        ofa_istore_get_interface_last_version
                                             ( const ofaIStore *istore );

void         ofa_istore_init                 ( ofaIStore *istore,
														ofaHub *hub );

void         ofa_istore_simulate_dataset_load( const ofaIStore *istore );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ISTORE_H__ */
