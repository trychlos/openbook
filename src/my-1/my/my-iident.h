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

#ifndef __MY_API_MY_IIDENT_H__
#define __MY_API_MY_IIDENT_H__

/**
 * SECTION: iident
 * @title: myIIdent
 * @short_description: An identification interface for extensions.
 * @include: my/my-iident.h
 *
 * This interface may be implemented by an object in order to provide
 * some sort of identification to the application.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define MY_TYPE_IIDENT                      ( my_iident_get_type())
#define MY_IIDENT( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, MY_TYPE_IIDENT, myIIdent ))
#define MY_IS_IIDENT( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, MY_TYPE_IIDENT ))
#define MY_IIDENT_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), MY_TYPE_IIDENT, myIIdentInterface ))

typedef struct _myIIdent                    myIIdent;

/**
 * myIIdentInterface:
 * @get_interface_version: [should]: returns the version of this interface that the plugin implements.
 * @get_canon_name: [should]: returns the canonical name of the plugin.
 * @get_display_name: [should]: returns the displayable name of the plugin.
 * @get_version: [should]: returns the internal version of the plugin.
 *
 * This defines the interface that an #myIIdent may/should/must
 * implement.
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
	 * get_canon_name:
	 * @instance: the #myIIdent instance.
	 * @user_data: some data to be passed to the final handler.
	 *
	 * Returns: the canonical name of the loadable module, as a newly
	 * allocated string which should be g_free() by the application.
	 *
	 * Since: version 1.
	 */
	gchar *  ( *get_canon_name )       ( const myIIdent *instance,
											void *user_data );

	/**
	 * get_display_name:
	 * @instance: the #myIIdent instance.
	 * @user_data: some data to be passed to the final handler.
	 *
	 * Returns: the displayable name of the loadable module, as a newly
	 * allocated string which should be g_free() by the application.
	 *
	 * Since: version 1.
	 */
	gchar *  ( *get_display_name )     ( const myIIdent *instance,
											void *user_data );

	/**
	 * get_version:
	 * @instance: the #myIIdent instance.
	 * @user_data: some data to be passed to the final handler.
	 *
	 * Returns: the internal version of the loadable module, as a newly
	 * allocated string which should be g_free() by the application.
	 *
	 * Since: version 1.
	 */
	gchar *  ( *get_version )          ( const myIIdent *instance,
											void *user_data );
}
	myIIdentInterface;

/*
 * Interface-wide
 */
GType  my_iident_get_type                  ( void );

guint  my_iident_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint  my_iident_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
gchar *my_iident_get_canon_name            ( const myIIdent *instance,
												void *user_data );

gchar *my_iident_get_display_name          ( const myIIdent *instance,
												void *user_data );

gchar *my_iident_get_version               ( const myIIdent *instance,
												void *user_data );

G_END_DECLS

#endif /* __MY_API_MY_IIDENT_H__ */
