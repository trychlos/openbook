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

#ifndef __MY_API_MY_IBOOK_DETACH_H__
#define __MY_API_MY_IBOOK_DETACH_H__

/**
 * SECTION: ibook_detach
 * @title: myIBookDetach
 * @short_description: The IBookDetach Interface
 * @include: my/my-ibook_detach.h
 *
 * The #myIBookDetach interface lets the user detach pages from a
 * #GtkNotebook. It is implemented by the #myBookDnd class.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define MY_TYPE_IBOOK_DETACH                      ( my_ibook_detach_get_type())
#define MY_IBOOK_DETACH( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, MY_TYPE_IBOOK_DETACH, myIBookDetach ))
#define MY_IS_IBOOK_DETACH( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, MY_TYPE_IBOOK_DETACH ))
#define MY_IBOOK_DETACH_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), MY_TYPE_IBOOK_DETACH, myIBookDetachInterface ))

typedef struct _myIBookDetach                     myIBookDetach;

/**
 * myIBookDetachInterface:
 * @get_interface_version: [should]: get the version number of the
 *                                   interface implementation.
 *
 * This defines the interface that an #myIBookDetach may/should
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
	myIBookDetachInterface;

/*
 * Interface-wide
 */
GType my_ibook_detach_get_type                  ( void );

guint my_ibook_detach_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint my_ibook_detach_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
void  my_ibook_detach_set_source_widget         ( myIBookDetach *instance,
														GtkWidget *widget );

G_END_DECLS

#endif /* __MY_API_MY_IBOOK_DETACH_H__ */
