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

#ifndef __OFA_ICOLLECTOR_H__
#define __OFA_ICOLLECTOR_H__

/**
 * SECTION: icollector
 * @title: ofaICollector
 * @short_description: The ICollector Interface
 * @include: core/ofa-icollector.h
 *
 * The #ofaICollector interface is implemented by the #ofoDossier class,
 * so that it is able to manage the a class-wide though dossier-attached
 * object for its client classes.
 *
 * From the #ofaICollector interface point of view, the dossier just
 * maintains a list of GObjects-derived objects. These objects
 * themselves are managed by the collections.
 *
 * Though this whole interface may have been included directly in the
 * #ofoDossier class code, this interface let us isolate the feature,
 * making thus it more reusable.
 *
 * The #ofaICollector implementation should call:
 * - ofa_icollector_init(), on instance intialization
 * - ofa_icollector_dispose(), on (first) instance dispose.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_ICOLLECTOR                      ( ofa_icollector_get_type())
#define OFA_ICOLLECTOR( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ICOLLECTOR, ofaICollector ))
#define OFA_IS_ICOLLECTOR( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ICOLLECTOR ))
#define OFA_ICOLLECTOR_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ICOLLECTOR, ofaICollectorInterface ))

typedef struct _ofaICollector                    ofaICollector;

/**
 * ofaICollectorInterface:
 *
 * This defines the interface that an #ofaICollector should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaICollector provider.
	 *
	 * The interface calls this method each time it need to know which
	 * version is implented.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 */
	guint    ( *get_interface_version )( const ofaICollector *collector );
}
	ofaICollectorInterface;

GType    ofa_icollector_get_type                  ( void );

guint    ofa_icollector_get_interface_last_version( void );

guint    ofa_icollector_get_interface_version     ( const ofaICollector *collector );

GObject *ofa_icollector_get_object                ( ofaICollector *collector, GType type );

/* an API dedicated to the #ofaICollector implementation
 */
void     ofa_icollector_init                      ( ofaICollector *collector );

void     ofa_icollector_dispose                   ( ofaICollector *collector );

G_END_DECLS

#endif /* __OFA_ICOLLECTOR_H__ */
