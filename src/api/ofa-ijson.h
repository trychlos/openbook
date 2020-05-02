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

#ifndef __OPENBOOK_API_OFA_IJSON_H__
#define __OPENBOOK_API_OFA_IJSON_H__

/**
 * SECTION: ijson
 * @title: ofaIJson
 * @short_description: The IJson Interface
 * @include: api/ofa-ijson.h
 *
 * The #ofaIJson interface is used to let a widget display a
 * contextual popup menu.
 */

#include "api/ofa-iactionable.h"

G_BEGIN_DECLS

#define OFA_TYPE_IJSON                      ( ofa_ijson_get_type())
#define OFA_IJSON( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IJSON, ofaIJson ))
#define OFA_IS_IJSON( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IJSON ))
#define OFA_IJSON_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IJSON, ofaIJsonInterface ))

typedef struct _ofaIJson                      ofaIJson;

/**
 * ofaIJsonInterface:
 *
 * This defines the interface that an #ofaIJson should implement.
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

	/**
	 * get_title:
	 *
	 * Returns: the identifier name associated to the Json object, as a
	 * newly allocated string which has to be released by the caller.
	 *
	 * Defaults to the class name.
	 *
	 * Since: version 1.
	 */
	gchar *       ( *get_title )            ( void );

	/*** instance-wide ***/
	/**
	 * get_as_string:
	 * @instance: this #ofaIJson instance.
	 *
	 * Returns: the JSON representation of the @instance as a newly
	 * allocated string which should be #g_free() by the caller.
	 *
	 * Defaults to %NULL.
	 *
	 * Since: version 1.
	 */
	gchar *       ( *get_as_string )        ( ofaIJson *instance );
}
	ofaIJsonInterface;

/*
 * Interface-wide
 */
GType   ofa_ijson_get_type                  ( void );

guint   ofa_ijson_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint   ofa_ijson_get_interface_version     ( GType type );

gchar  *ofa_ijson_get_title                 ( GType type );

/*
 * Instance-wide
 */
gchar  *ofa_ijson_get_as_string             ( ofaIJson *json );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IJSON_H__ */
