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

#ifndef __MY_API_MY_IBIN_H__
#define __MY_API_MY_IBIN_H__

/**
 * SECTION: ibin
 * @title: myIBin
 * @short_description: An interface to let a composite GtkWidget manages its life.
 * @include: my/my-ibin.h
 *
 * Idea is that all composite widgets may share this same interface.
 *
 * Signals:
 * - my-ibin-changed: emitted on the #myIBin instance when something changes.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_IBIN                      ( my_ibin_get_type())
#define MY_IBIN( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, MY_TYPE_IBIN, myIBin ))
#define MY_IS_IBIN( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, MY_TYPE_IBIN ))
#define MY_IBIN_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), MY_TYPE_IBIN, myIBinInterface ))

typedef struct _myIBin                     myIBin;

/**
 * myIBinInterface:
 * @get_interface_version: [should]: returns the version of this
 *                         interface that the plugin implements.
 * @get_size_group: [may]: returns the #GtkSizeGroup of the column.
 * @is_valid: [may]: returns %TRUE if the instance is valid.
 * @apply: [may]: apply the pending updates.
 *
 * This defines the interface that an #myIBin may/should/must
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
	 * @instance: this #myIBin instance.
	 * @column: the desired column.
	 *
	 * Returns: the #GtkSizeGroup for the desired @column.
	 *
	 * Since: version 1
	 */
	GtkSizeGroup * ( *get_size_group )       ( const myIBin *instance,
													guint column );

	/**
	 * is_valid:
	 * @instance: this #myIBin instance.
	 * @msgerr: [out]: a placeholder for an error message;
	 *  the interface makes sure that this @msgerr pointer is not null,
	 *  and that the implementation can safely store an error message
	 *  in it.
	 *
	 * Returns: %TRUE if the @instance is valid.
	 *
	 * Defaults to %TRUE.
	 *
	 * Since: version 1
	 */
	gboolean       ( *is_valid )             ( const myIBin *instance,
													gchar **msgerr );

	/**
	 * apply:
	 * @instance: this #myIBin instance.
	 *
	 * Apply the updates.
	 *
	 * Since: version 1
	 */
	void           ( *apply )                ( myIBin *instance );
}
	myIBinInterface;

/*
 * Interface-wide
 */
GType         my_ibin_get_type                  ( void );

guint         my_ibin_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint         my_ibin_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
GtkSizeGroup *my_ibin_get_size_group            ( const myIBin *instance,
														guint column );

gboolean      my_ibin_is_valid                  ( const myIBin *instance,
														gchar **msgerr );

void          my_ibin_apply                     ( myIBin *instance );

G_END_DECLS

#endif /* __MY_API_MY_IBIN_H__ */
