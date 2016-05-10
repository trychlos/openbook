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

#ifndef __OPENBOOK_API_OFA_ITREE_ADDER_H__
#define __OPENBOOK_API_OFA_ITREE_ADDER_H__

/**
 * SECTION: itree_adder
 * @title: ofaITreeAdder
 * @short_description: The Tree Adder Interface
 * @include: openbook/ofa-itree-adder.h
 *
 * The #ofaITreeAdder interface lets a plugin adds some columns to a
 * list/tree store and to the corresponding treeview.
 */

#include <gtk/gtk.h>

#include "api/ofa-hub-def.h"
#include "api/ofa-istore.h"

G_BEGIN_DECLS

#define OFA_TYPE_ITREE_ADDER                      ( ofa_itree_adder_get_type())
#define OFA_ITREE_ADDER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ITREE_ADDER, ofaITreeAdder ))
#define OFA_IS_ITREE_ADDER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ITREE_ADDER ))
#define OFA_ITREE_ADDER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ITREE_ADDER, ofaITreeAdderInterface ))

typedef struct _ofaITreeAdder                     ofaITreeAdder;

typedef guint ( *TreeAdderTypeCb )( ofaIStore *, GType, void * );

/**
 * ofaITreeAdderInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 * @get_types: [must] defines the implemented GType's.
 * @set_values: [should] set values in a row.
 *
 * This defines the interface that an #ofaITreeAdder should implement.
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
	/**
	 * add_types:
	 * @instance: the #ofaITreeAdder instance.
	 * @store: the target #ofaIStore.
	 * @cb: the callback function to be called for each GType.
	 * @cb_data: the user data for the callback.
	 *
	 * Defines the GType's to be added to the @store.
	 *
	 * Since: version 1.
	 */
	void  ( *add_types )            ( ofaITreeAdder *instance,
											ofaIStore *store,
											TreeAdderTypeCb cb,
											void *cb_data );

	/**
	 * set_values:
	 * @instance: the #ofaITreeAdder instance.
	 * @store: the target #ofaIStore.
	 * @hub: the #ofaHub object of the application.
	 * @iter: the current #GtkTreeIter.
	 * @object: the current #GObject.
	 *
	 * Sets values in the row.
	 *
	 * Since: version 1.
	 */
	void  ( *set_values )           ( ofaITreeAdder *instance,
											ofaIStore *store,
											ofaHub *hub,
											GtkTreeIter *iter,
											void *object );

	/**
	 * add_columns:
	 * @instance: the #ofaITreeAdder instance.
	 * @store: the source #ofaIStore.
	 * @treeview: the target #GtkTreeView.
	 *
	 * Add columns to a view.
	 *
	 * Since: version 1.
	 */
	void  ( *add_columns )          ( ofaITreeAdder *instance,
											ofaIStore *store,
											GtkWidget *treeview );
}
	ofaITreeAdderInterface;

/*
 * Interface-wide
 */
GType            ofa_itree_adder_get_type                  ( void );

guint            ofa_itree_adder_get_interface_last_version( void );

void             ofa_itree_adder_add_types                 ( ofaHub *hub,
																	ofaIStore *store,
																	TreeAdderTypeCb cb,
																	void *cb_data );

void             ofa_itree_adder_set_values                ( ofaHub *hub,
																	ofaIStore *store,
																	GtkTreeIter *iter,
																	void *object );

void             ofa_itree_adder_add_columns               ( ofaHub *hub,
																	ofaIStore *store,
																	GtkWidget *treeview );

/*
 * Implementation-wide
 */
guint            ofa_itree_adder_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ITREE_ADDER_H__ */
