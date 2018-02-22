/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifndef __OPENBOOK_API_OFA_IACTIONER_H__
#define __OPENBOOK_API_OFA_IACTIONER_H__

/**
 * SECTION: iactioner
 * @title: ofaIActioner
 * @short_description: The IActioner Interface
 * @include: api/ofa-iactioner.h
 *
 * The #ofaIActioner interface is used for proxy and synchronize the
 * action messages between several #ofaIActionable classes which should
 * stay synchronized.
 *
 * It works by connecting to each of the synchronized instances, and
 * proxy each action message from an instance to all others after having
 * changed the action group name.
 */

#include "api/ofa-iactionable.h"

G_BEGIN_DECLS

#define OFA_TYPE_IACTIONER                      ( ofa_iactioner_get_type())
#define OFA_IACTIONER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IACTIONER, ofaIActioner ))
#define OFA_IS_IACTIONER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IACTIONER ))
#define OFA_IACTIONER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IACTIONER, ofaIActionerInterface ))

typedef struct _ofaIActioner                      ofaIActioner;

/**
 * ofaIActionerInterface:
 *
 * This defines the interface that an #ofaIActioner should implement.
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
	 * Since: version 1.
	 */
	guint         ( *get_interface_version )( void );

	/*** instance-wide ***/
}
	ofaIActionerInterface;

/*
 * Interface-wide
 */
GType ofa_iactioner_get_type                  ( void );

guint ofa_iactioner_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint ofa_iactioner_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
void  ofa_iactioner_register_actionable       ( ofaIActioner *instance,
													ofaIActionable *actionable );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IACTIONER_H__ */
