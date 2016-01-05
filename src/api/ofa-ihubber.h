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

#ifndef __OPENBOOK_API_OFA_IHUBBER_H__
#define __OPENBOOK_API_OFA_IHUBBER_H__

/**
 * SECTION: ihubber
 * @title: ofaIHubber
 * @short_description: The IHubber Interface
 * @include: openbook/ofa-ihubber.h
 *
 * The #ofaIHubber interface is a simple interface to get the main
 * #ofaHub object, which itself handles the current connection, the
 * currently opened dossier and all its collections.
 *
 * It is defined so that getting this main #ofaHub object does not
 * depend of having a direct access to a particular suit of an
 * application.
 *
 * Most of the time, your application will implement this interface.
 *
 * The interface takes care of emitting :
 * - the "hub-opened" signal when a new #ofaHub object has been
 *   successfully instanciated,
 * - the "hub-closed" signal on #ofaHub object finalization.
 */

#include "api/ofa-hub-def.h"
#include "api/ofa-idbconnect.h"

G_BEGIN_DECLS

#define OFA_TYPE_IHUBBER                      ( ofa_ihubber_get_type())
#define OFA_IHUBBER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IHUBBER, ofaIHubber ))
#define OFA_IS_IHUBBER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IHUBBER ))
#define OFA_IHUBBER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IHUBBER, ofaIHubberInterface ))

typedef struct _ofaIHubber                    ofaIHubber;

/**
 * ofaIHubberInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 * @new_hub: [should] instanciates a new #ofaHub object.
 * @get_hub: [should] returns the current #ofaHub object.
 *
 * This defines the interface that an #ofaIHubber should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIHubber instance.
	 *
	 * The application calls this method each time it needs to know
	 * which version of this interface the plugin implements.
	 *
	 * If this method is not implemented by the plugin,
	 * the application considers that the plugin only implements
	 * the version 1 of the ofaIHubber interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 */
	guint    ( *get_interface_version )( const ofaIHubber *instance );

	/**
	 * new_hub:
	 * @instance: the #ofaIHubber instance.
	 * @connect: a valid #ofaIDBConnect connection on the targeted
	 *  database.
	 *
	 * Instanciates a new #ofaHub object, unreffing the previous one
	 * if it exists. Takes care of updating the user interface
	 * accordingly if apply.
	 *
	 * Returns: the newly instanciated #ofaHub object on success,
	 * or %NULL.
	 *
	 * Since: version 1
	 */
	ofaHub * ( *new_hub )             ( ofaIHubber *instance,
											const ofaIDBConnect *connect );

	/**
	 * get_hub:
	 * @instance: the #ofaIHubber instance.
	 *
	 * Returns: the main #ofaHub object which is supposed to be
	 * maintained by the implementation.
	 *
	 * It is expected that the returned reference is owned by the
	 * implementation. The caller should not released it.
	 *
	 * Since: version 1
	 */
	ofaHub * ( *get_hub )             ( const ofaIHubber *instance );

	/**
	 * clear_hub:
	 * @instance: the #ofaIHubber instance.
	 *
	 * Clears the #ofaHub instance.
	 *
	 * Since: version 1
	 */
	void     ( *clear_hub )           ( ofaIHubber *instance );
}
	ofaIHubberInterface;


/**
 * Signals defined here:
 */
#define SIGNAL_HUBBER_NEW               "hubber-new"
#define SIGNAL_HUBBER_CLOSED            "hubber-closed"

GType   ofa_ihubber_get_type                  ( void );

guint   ofa_ihubber_get_interface_last_version( void );

guint   ofa_ihubber_get_interface_version     ( const ofaIHubber *instance );

ofaHub *ofa_ihubber_new_hub                   ( ofaIHubber *instance,
													const ofaIDBConnect *connect );

ofaHub *ofa_ihubber_get_hub                   ( const ofaIHubber *instance );

void    ofa_ihubber_clear_hub                 ( ofaIHubber *instance );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IHUBBER_H__ */
