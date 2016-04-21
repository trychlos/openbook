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

#ifndef __MY_API_MY_IDND_DETACH_H__
#define __MY_API_MY_IDND_DETACH_H__

/**
 * SECTION: idnd_detach
 * @title: myIDndDetach
 * @short_description: The IDndDetach Interface
 * @include: my/my-idnd-detach.h
 *
 * The #myIDndDetach interface lets the user detach pages from a
 * #GtkNotebook. It is implemented by the #myDndBook class.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define MY_TYPE_IDND_DETACH                      ( my_idnd_detach_get_type())
#define MY_IDND_DETACH( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, MY_TYPE_IDND_DETACH, myIDndDetach ))
#define MY_IS_IDND_DETACH( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, MY_TYPE_IDND_DETACH ))
#define MY_IDND_DETACH_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), MY_TYPE_IDND_DETACH, myIDndDetachInterface ))

typedef struct _myIDndDetach                     myIDndDetach;

/**
 * myIDndDetachInterface:
 * @get_interface_version: [should]: get the version number of the
 *                                   interface implementation.
 *
 * This defines the interface that an #myIDndDetach may/should
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
	guint ( *get_interface_version )( void );

	/*** instance-wide ***/
}
	myIDndDetachInterface;

/*
 * Interface-wide
 */
GType my_idnd_detach_get_type                  ( void );

guint my_idnd_detach_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint my_idnd_detach_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
void  my_idnd_detach_set_source_widget         ( myIDndDetach *instance,
														GtkWidget *window,
														GtkWidget *source );

G_END_DECLS

#endif /* __MY_API_MY_IDND_DETACH_H__ */
