/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_IEXTENDER_SETTER_H__
#define __OPENBOOK_API_OFA_IEXTENDER_SETTER_H__

/**
 * SECTION: iextender_setter
 * @title: ofaIExtenderSetter
 * @short_description: The IExtenderSetter Interface
 * @include: openbook/ofa-iextender-setter.h
 *
 * The #ofaIExtenderSetter interface may be implemented by objects defined by
 * dynamically loaded modules (aka plugins) in order to get some initial
 * pointers at instanciation time.
 */

#include <glib-object.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IEXTENDER_SETTER                      ( ofa_iextender_setter_get_type())
#define OFA_IEXTENDER_SETTER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IEXTENDER_SETTER, ofaIExtenderSetter ))
#define OFA_IS_IEXTENDER_SETTER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IEXTENDER_SETTER ))
#define OFA_IEXTENDER_SETTER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IEXTENDER_SETTER, ofaIExtenderSetterInterface ))

typedef struct _ofaIExtenderSetter                     ofaIExtenderSetter;

/**
 * ofaIExtenderSetterInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 * @set_getter: [should] provides a permanent #ofaIGetter to the object.
 *
 * This defines the interface that an #ofaIExtenderSetter should implement.
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
	/**
	 * get_getter:
	 * @instance: the #ofaIExtenderSetter instance.
	 *
	 * Returns: the previously set #ofaIGetter instance.
	 *
	 * Since: version 1.
	 */
	ofaIGetter * ( *get_getter )          ( ofaIExtenderSetter *instance );

	/**
	 * set_getter:
	 * @instance: the #ofaIExtenderSetter instance.
	 * @getter: a permanent #ofaIGetter of the application.
	 *
	 * Set a @getter.
	 *
	 * This method is called right after instanciation of the object.
	 *
	 * Since: version 1.
	 */
	void         ( *set_getter )          ( ofaIExtenderSetter *instance,
												ofaIGetter *getter );
}
	ofaIExtenderSetterInterface;

/*
 * Interface-wide
 */
GType       ofa_iextender_setter_get_type                  ( void );

guint       ofa_iextender_setter_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint       ofa_iextender_setter_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
ofaIGetter *ofa_iextender_setter_get_getter                ( ofaIExtenderSetter *instance );

void        ofa_iextender_setter_set_getter                ( ofaIExtenderSetter *instance,
																ofaIGetter *getter );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IEXTENDER_SETTER_H__ */
