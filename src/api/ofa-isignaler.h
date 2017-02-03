/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_ISIGNALER_H__
#define __OPENBOOK_API_OFA_ISIGNALER_H__

/**
 * SECTION: isignaler
 * @title: ofaISignaler
 * @short_description: The ISignaler Interface
 * @include: openbook/ofa-isignaler.h
 *
 * The #ofaISignaler interface is the instance everyone may connect to
 * in order to be advertized of some application-wide events.
 *
 * The architecture of the application makes sure that this object is
 * available right after the #ofaHub has been initialized.
 *
 * Signals defined here:
 *
 * - 'ofa-signaler-page-manager-available':
 *
 * - 'ofa-signaler-menu-available':
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_ISIGNALER                      ( ofa_isignaler_get_type())
#define OFA_ISIGNALER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ISIGNALER, ofaISignaler ))
#define OFA_IS_ISIGNALER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ISIGNALER ))
#define OFA_ISIGNALER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ISIGNALER, ofaISignalerInterface ))

typedef struct _ofaISignaler                    ofaISignaler;

/**
 * ofaISignalerInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 *
 * This defines the interface that an #ofaISignaler should implement.
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
	guint        ( *get_interface_version )( void );

	/*** instance-wide ***/
}
	ofaISignalerInterface;

/*
 * Interface-wide
 */
GType       ofa_isignaler_get_type                  ( void );

guint       ofa_isignaler_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint       ofa_isignaler_get_interface_version     ( GType type );

/*
 * Instance-wide
 */

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ISIGNALER_H__ */
