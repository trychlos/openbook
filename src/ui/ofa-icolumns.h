/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_ICOLUMNS_H__
#define __OFA_ICOLUMNS_H__

/**
 * SECTION: icolumns
 * @title: ofaIColumns
 * @short_description: The IColumns Interface
 * @include: ui/ofa-icolumns.h
 *
 * The #ofaIColumns interface lets the user choose the displayed columns.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_ICOLUMNS                      ( ofa_icolumns_get_type())
#define OFA_ICOLUMNS( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ICOLUMNS, ofaIColumns ))
#define OFA_IS_ICOLUMNS( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ICOLUMNS ))
#define OFA_ICOLUMNS_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ICOLUMNS, ofaIColumnsInterface ))

typedef struct _ofaIColumns                    ofaIColumns;

/**
 * ofaIColumnsInterface:
 *
 * This defines the interface that an #ofaIColumns should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIColumns instance.
	 *
	 * The interface code calls this method each time it needs to know
	 * which version of this interface the application implements.
	 *
	 * If this method is not implemented, then the
	 * interface code considers that the application only implements
	 * the version 1 of the ofaIColumns interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the application is supporting.
	 *
	 * Defaults to 1.
	 */
	guint              ( *get_interface_version )   ( const ofaIColumns *instance );
}
	ofaIColumnsInterface;

GType        ofa_icolumns_get_type          ( void );

guint        ofa_icolumns_get_interface_last_version
                                            ( const ofaIColumns *instance );

void         ofa_icolumns_add_column        ( ofaIColumns *instance,
													guint id,
													const gchar *label,
													gboolean visible,
													GtkTreeViewColumn *column );

void         ofa_icolumns_init_visible      ( ofaIColumns *instance,
													const gchar *key );

gboolean     ofa_icolumns_get_visible       ( const ofaIColumns *instance,
													guint id );

void         ofa_icolumns_set_visible       ( ofaIColumns *instance,
													guint id,
													gboolean visible );

void         ofa_icolumns_attach_menu_button( ofaIColumns *instance,
													GtkContainer *parent );

G_END_DECLS

#endif /* __OFA_ICOLUMNS_H__ */
