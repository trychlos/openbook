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

#ifndef __MY_API_MY_IGRIDLIST_H__
#define __MY_API_MY_IGRIDLIST_H__

/**
 * SECTION: igridlist
 * @title: myIGridList
 * @short_description: An interface to manage the dynamic grids.
 * @include: my/my-igridlist.h
 *
 * This interface lets a #GtkContainer manage a dynamic grid, i.e.
 * a grid with a variable set of rows, which may be added, removed
 * or moved.
 *
 * The managed grid is expected to have a header row at index 0.
 * Each detail row has:
 * - either a Add button or the row number at column 0
 * - the detail widgets to be provided by the implementation
 * - the Up, Down and Remove buttons on right columns
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_IGRIDLIST                      ( my_igridlist_get_type())
#define MY_IGRIDLIST( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, MY_TYPE_IGRIDLIST, myIGridList ))
#define MY_IS_IGRIDLIST( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, MY_TYPE_IGRIDLIST ))
#define MY_IGRIDLIST_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), MY_TYPE_IGRIDLIST, myIGridListInterface ))

typedef struct _myIGridList                    myIGridList;

/**
 * myIGridListInterface:
 * @get_interface_version: [should]: returns the version of this
 *                         interface that the plugin implements.
 * @set_row: [may]: set widgets and values on the row.
*
 * This defines the interface that an #myIGridList may/should/must
 * implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #myIGridList instance.
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
	guint      ( *get_interface_version )( const myIGridList *instance );

	/**
	 * set_row:
	 * @instance: the #myIGridList instance.
	 * @grid: the target #GtkGrid
	 * @row: the row index, counted from zero.
	 *
	 * The implementation may take advantage of this method to add
	 * its own widgets and values to the @grid.
	 *
	 * Since: version 1
	 */
	void       ( *set_row )              ( const myIGridList *instance,
												GtkGrid *grid,
												guint row );
}
	myIGridListInterface;

GType      my_igridlist_get_type                  ( void );

guint      my_igridlist_get_interface_last_version( void );

guint      my_igridlist_get_interface_version     ( const myIGridList *instance );

void       my_igridlist_init                      ( const myIGridList *instance,
														GtkGrid *grid,
														gboolean is_current,
														guint columns_count );

guint      my_igridlist_add_row                   ( const myIGridList *instance,
														GtkGrid *grid );

GtkWidget *my_igridlist_add_button                ( const myIGridList *instance,
														GtkGrid *grid,
														const gchar *stock_id,
														guint column,
														guint row,
														guint right_margin,
														GCallback cb,
														void *user_data );

guint      my_igridlist_get_rows_count            ( const myIGridList *instance,
														GtkGrid *grid );

G_END_DECLS

#endif /* __MY_API_MY_IGRIDLIST_H__ */
