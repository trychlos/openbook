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
 *
 * Column identification is dynamically computed by the interface based
 * on the previous allocations, i.e. it takes into account the count of
 * columns defined by the standard store, as well as the count of columns
 * already added by other plugins.
 *
 * As a side effect, new columns in the store must always be defined
 * before being able to add columns to the treeview.
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"
#include "api/ofa-istore.h"
#include "api/ofa-tvbin.h"

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
	guint    ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * add_types:
	 * @instance: the #ofaITreeAdder instance.
	 * @store: the target #ofaIStore.
	 * @column_object: the column number of the stored object.
	 * @cb: the callback function to be called for each GType.
	 * @cb_data: the user data for the callback.
	 *
	 * Defines the GType's to be added to the @store.
	 *
	 * Since: version 1.
	 */
	void     ( *add_types )            ( ofaITreeAdder *instance,
											ofaIStore *store,
											guint column_object,
											TreeAdderTypeCb cb,
											void *cb_data );
	/**
	 * get_column_types:
	 * @instance: the #ofaITreeAdder instance.
	 * @store: the target #ofaIStore.
	 * @orig_cols_count: the current count of columns in the @store.
	 * @add_cols: [out]: the count of added columns.
	 *
	 * Returns: an array which contains the GType's to be added to the
	 * @store.
	 *
	 * Since: version 1.
	 */
	GType *  ( *get_column_types )     ( ofaITreeAdder *instance,
											ofaIStore *store,
											guint orig_cols_count,
											guint *add_cols );

	/**
	 * set_values:
	 * @instance: the #ofaITreeAdder instance.
	 * @store: the target #ofaIStore.
	 * @getter: a #ofaIGetter instance.
	 * @iter: the current #GtkTreeIter.
	 * @object: the current #GObject.
	 *
	 * Sets values in the row.
	 *
	 * Since: version 1.
	 */
	void     ( *set_values )           ( ofaITreeAdder *instance,
											ofaIStore *store,
											ofaIGetter *getter,
											GtkTreeIter *iter,
											void *object );

	/**
	 * sort:
	 * @instance: the #ofaITreeAdder instance.
	 * @store: the target #ofaIStore.
	 * @getter: a #ofaIGetter instance.
	 * @a: a row to be compared.
	 * @b: another row to compare to @a.
	 * @column_id: the index of the column to be compared.
	 * @cmp: [out]: the result of the comparison.
	 *
	 * Returns: %TRUE if the column_id is managed here, %FALSE else.
	 *
	 * Since: version 1.
	 */
	gboolean ( *sort )                 ( ofaITreeAdder *instance,
											ofaIStore *store,
											ofaIGetter *getter,
											GtkTreeModel *model,
											GtkTreeIter *a,
											GtkTreeIter *b,
											gint column_id,
											gint *cmp );

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
	void     ( *add_columns )          ( ofaITreeAdder *instance,
											ofaIStore *store,
											ofaTVBin *bin );
}
	ofaITreeAdderInterface;

/*
 * Interface-wide
 */
GType            ofa_itree_adder_get_type                  ( void );

guint            ofa_itree_adder_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint            ofa_itree_adder_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
GType           *ofa_itree_adder_get_column_types          ( ofaIGetter *getter,
																	ofaIStore *store,
																	guint orig_cols_count,
																	GType *orig_col_types,
																	guint *cols_count );

void             ofa_itree_adder_set_values                ( ofaIGetter *getter,
																	ofaIStore *store,
																	GtkTreeIter *iter,
																	void *object );

gboolean         ofa_itree_adder_sort                      ( ofaIGetter *getter,
																	ofaIStore *store,
																	GtkTreeModel *model,
																	GtkTreeIter *a,
																	GtkTreeIter *b,
																	gint column_id,
																	gint *cmp );

void             ofa_itree_adder_add_columns               ( ofaIGetter *getter,
																	ofaIStore *store,
																	ofaTVBin *bin );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ITREE_ADDER_H__ */
