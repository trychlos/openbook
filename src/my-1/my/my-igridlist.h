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
 * All methods must identify the #myIGridList instance (e.g. the dialog)
 * and the concerned #GtkGrid.
 *
 * The managed grid may have a header row at index 0, and the interface
 * adds an empty row with only a '+' (Add) button at the end of the grid.
 *
 * An empty grid from the user point of view has so exacty two rows:
 * the headers row (if present) + the Add row.
 * A grid which has n detail lines has so n+2 rows.
 * Details lines are numbered from 1 to n, which happens to be the row
 * index in the managed grid.
 *
 * Each row has:
 * - either a Add button or the row number at column 0
 * - the detail widgets to be provided by the implementation
 * - the Up, Down and Remove buttons on last right columns
 *
 * From the implementation point of view, the column numbering so starts
 * at 1.
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
 * @setup_row: [may]: set widgets and values on the row.
*
 * This defines the interface that a #myIGridList may/should/must
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
	guint      ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * set_row:
	 * @instance: the #myIGridList instance.
	 * @grid: the target #GtkGrid
	 * @row: the row index in the @grid.
	 * @user_data: [allow-none]: the data passed to my_igridlist_add_row().
	 *
	 * The implementation may take advantage of this method to add
	 * its own widgets and values to the @grid.
	 *
	 * The interface takes care of add
	 *
	 * Since: version 1.
	 */
	void       ( *setup_row )            ( const myIGridList *instance,
												GtkGrid *grid,
												guint row,
												void *user_data );
}
	myIGridListInterface;

/*
 * Interface-wide
 */
GType      my_igridlist_get_type                  ( void );

guint      my_igridlist_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint      my_igridlist_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
void       my_igridlist_init                      ( const myIGridList *instance,
														GtkGrid *grid,
														gboolean has_header,
														gboolean writable,
														guint columns_count );

guint      my_igridlist_add_row                   ( const myIGridList *instance,
														GtkGrid *grid,
														void *user_data );

void       my_igridlist_set_widget                ( const myIGridList *instance,
														GtkGrid *grid,
														GtkWidget *widget,
														guint column,
														guint row,
														guint width,
														guint height );

guint      my_igridlist_get_details_count         ( const myIGridList *instance,
														GtkGrid *grid );

guint      my_igridlist_get_row_index             ( GtkWidget *widget );

G_END_DECLS

#endif /* __MY_API_MY_IGRIDLIST_H__ */
