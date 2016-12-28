/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_ISETTER_H__
#define __OPENBOOK_API_OFA_ISETTER_H__

/**
 * SECTION: isetter
 * @title: ofaISetter
 * @short_description: The ISetter Interface
 * @include: openbook/ofa-isetter.h
 *
 * The #ofaISetter interface may be implemented by objects defined by
 * dynamically loaded modules (aka plugins) in order to get some initial
 * pointers at instanciation time.
 */

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ISETTER                      ( ofa_isetter_get_type())
#define OFA_ISETTER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ISETTER, ofaISetter ))
#define OFA_IS_ISETTER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ISETTER ))
#define OFA_ISETTER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ISETTER, ofaISetterInterface ))

typedef struct _ofaISetter                    ofaISetter;

/**
 * ofaISetterInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 * @set_getter: [should] provides a permanent #ofaIGetter to the object.
 *
 * This defines the interface that an #ofaISetter should implement.
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
	guint    ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * set_getter:
	 * @instance: the #ofaISetter instance.
	 * @getter: a permanent #ofaIGetter of the application.
	 *
	 * Set a @getter.
	 *
	 * Since: version 1.
	 */
	void      ( *set_getter )          ( ofaISetter *instance,
											ofaIGetter *getter );
}
	ofaISetterInterface;

/*
 * Interface-wide
 */
GType  ofa_isetter_get_type                  ( void );

guint  ofa_isetter_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint  ofa_isetter_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
void   ofa_isetter_set_getter                ( ofaISetter *instance,
													ofaIGetter *getter );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ISETTER_H__ */
