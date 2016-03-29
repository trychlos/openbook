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

#ifndef __OPENBOOK_API_OFA_IREGISTER_H__
#define __OPENBOOK_API_OFA_IREGISTER_H__

/**
 * SECTION: iregister
 * @title: ofaIRegister
 * @short_description: The ofaIRegister Interface
 * @include: openbook/ofa-iregister.h
 *
 * The main goal of #ofaIRegister interface is to advertize about the
 * properties of the classes it knows.
 *
 * The application/module should first register the GTypes it manages.
 * The interface may then be called when requesting for a particular
 * property.
 */

#include "api/ofa-hub-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IREGISTER                      ( ofa_iregister_get_type())
#define OFA_IREGISTER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IREGISTER, ofaIRegister ))
#define OFA_IS_IREGISTER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IREGISTER ))
#define OFA_IREGISTER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IREGISTER, ofaIRegisterInterface ))

typedef struct _ofaIRegister                    ofaIRegister;

/**
 * ofaIRegisterInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 * @get_for_type: [should] returns the list of objects for this type.
 *
 * This defines the interface that an #ofaIRegister should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIRegister provider.
	 *
	 * The interface calls this method each time it need to know which
	 * version is implented.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 */
	guint    ( *get_interface_version )( const ofaIRegister *instance );

	/**
	 * get_for_type:
	 * @instance: the #ofaIRegister provider.
	 * @type: the requested GType.
	 *
	 * Return: a list of new reference to objects which have the
	 * requested GType.
	 */
	GList *  ( *get_for_type )         ( ofaIRegister *instance,
											GType type );
}
	ofaIRegisterInterface;

GType    ofa_iregister_get_type                  ( void );

guint    ofa_iregister_get_interface_last_version( void );

guint    ofa_iregister_get_interface_version     ( const ofaIRegister *instance );

GList   *ofa_iregister_get_for_type              ( ofaIRegister *instance,
														GType type );

GList   *ofa_iregister_get_all_for_type          ( ofaHub *hub,
														GType type );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IREGISTER_H__ */
