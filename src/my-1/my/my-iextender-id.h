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

#ifndef __MY_API_MY_IEXTENDER_ID_H__
#define __MY_API_MY_IEXTENDER_ID_H__

/**
 * SECTION: iextender_id
 * @title: myIExtenderId
 * @short_description: An identification interface for extensions.
 * @include: my/my-iextender_id.h
 *
 * This interface should be implemented by loadable modules, to provide
 * some sort of identification to the application.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define MY_TYPE_IEXTENDER_ID                      ( my_iextender_id_get_type())
#define MY_IEXTENDER_ID( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, MY_TYPE_IEXTENDER_ID, myIExtenderId ))
#define MY_IS_IEXTENDER_ID( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, MY_TYPE_IEXTENDER_ID ))
#define MY_IEXTENDER_ID_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), MY_TYPE_IEXTENDER_ID, myIExtenderIdInterface ))

typedef struct _myIExtenderId                     myIExtenderId;

/**
 * myIExtenderIdInterface:
 * @get_interface_version: [should]: returns the version of this interface that the plugin implements.
 * @get_name: [should]: returns the canonical name of the plugin.
 * @get_version: [should]: returns the internal version of the plugin.
 *
 * This defines the interface that an #myIExtenderId may/should/must
 * implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #myIExtenderId instance.
	 *
	 * The interface calls this method each time it need to know which
	 * version is implemented by the instance.
	 *
	 * Returns: the version number of this interface that the @instance
	 *  is supporting.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1
	 */
	guint    ( *get_interface_version )( const myIExtenderId *instance );

	/**
	 * get_name:
	 * @instance: the #myIExtenderId instance.
	 *
	 * Returns: the canonical name of the loadable module, as a newly
	 * allocated string which should be g_free() by the application.
	 *
	 * Since: version 1
	 */
	gchar *  ( *get_name )             ( const myIExtenderId *instance );

	/**
	 * get_full_name:
	 * @instance: the #myIExtenderId instance.
	 *
	 * Returns: the full name of the loadable module, as a newly
	 * allocated string which should be g_free() by the application.
	 *
	 * Since: version 1
	 */
	gchar *  ( *get_full_name )        ( const myIExtenderId *instance );

	/**
	 * get_version:
	 * @instance: the #myIExtenderId instance.
	 *
	 * Returns: the internal version of the loadable module, as a newly
	 * allocated string which should be g_free() by the application.
	 *
	 * Since: version 1
	 */
	gchar *  ( *get_version )          ( const myIExtenderId *instance );
}
	myIExtenderIdInterface;

GType  my_iextender_id_get_type                  ( void );

guint  my_iextender_id_get_interface_last_version( void );

guint  my_iextender_id_get_interface_version     ( const myIExtenderId *instance );

gchar *my_iextender_id_get_name                  ( const myIExtenderId *instance );

gchar *my_iextender_id_get_full_name             ( const myIExtenderId *instance );

gchar *my_iextender_id_get_version               ( const myIExtenderId *instance );

G_END_DECLS

#endif /* __MY_API_MY_IEXTENDER_ID_H__ */
