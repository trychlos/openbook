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

#ifndef __MY_API_MY_ISIZEGROUP_H__
#define __MY_API_MY_ISIZEGROUP_H__

/**
 * SECTION: isizegroup
 * @title: myISizegroup
 * @short_description: An interface to let a GtkWidget manages its GtkSizeGroup's.
 * @include: my/my-isizegroup.h
 *
 * Idea is that several classes and interfaces may need to manage
 * some #GtkSizeGroup groups, so that they can all cooperate to build
 * a funny display for the user.
 *
 * This particular interface may provide a common point for this.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_ISIZEGROUP                      ( my_isizegroup_get_type())
#define MY_ISIZEGROUP( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, MY_TYPE_ISIZEGROUP, myISizegroup ))
#define MY_IS_ISIZEGROUP( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, MY_TYPE_ISIZEGROUP ))
#define MY_ISIZEGROUP_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), MY_TYPE_ISIZEGROUP, myISizegroupInterface ))

typedef struct _myISizegroup                     myISizegroup;

/**
 * myISizegroupInterface:
 * @get_interface_version: [should]: returns the version of this
 *                         interface that the plugin implements.
 * @get_size_group: [may]: returns the #GtkSizeGroup of the column.
 *
 * This defines the interface that an #myISizegroup may/should/must
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
	guint          ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * get_size_group:
	 * @instance: this #myISizegroup instance.
	 * @column: the desired column.
	 *
	 * Returns: the #GtkSizeGroup for the desired @column.
	 *
	 * Since: version 1
	 */
	GtkSizeGroup * ( *get_size_group )       ( const myISizegroup *instance,
													guint column );
}
	myISizegroupInterface;

/*
 * Interface-wide
 */
GType         my_isizegroup_get_type                  ( void );

guint         my_isizegroup_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint         my_isizegroup_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
GtkSizeGroup *my_isizegroup_get_size_group            ( const myISizegroup *instance,
															guint column );

G_END_DECLS

#endif /* __MY_API_MY_ISIZEGROUP_H__ */
