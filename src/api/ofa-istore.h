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

#ifndef __OPENBOOK_API_OFA_ISTORE_H__
#define __OPENBOOK_API_OFA_ISTORE_H__

/**
 * SECTION: istore
 * @title: ofaIStore
 * @short_description: The IStore Interface
 * @include: openbook/ofa-istore.h
 *
 * The #ofaIStore interface is implemented by #ofaListStore and
 * #ofaTreeStore. It lets us implement some common behavior between
 * all our stores.
 *
 * Implemented behaviors:
 *
 * - Provide a common method for loading a store dataset when the #ofaHub
 *   object of the application has been already set.
 *
 * - Provide some common methods to let plugins add columns to application
 *   stores and associated treeviews.
 *
 * Signals defined here:
 *
 * - ofa-row-inserted: sent on the store when a new row has been
 *   inserted; this is same as 'row-inserted' except that this later is
 *   trapped by Gtk to insert new line in GtkTreeView..
 */

#include <glib-object.h>

#include "api/ofa-igetter-def.h"
#include "api/ofa-tvbin.h"

G_BEGIN_DECLS

#define OFA_TYPE_ISTORE                      ( ofa_istore_get_type())
#define OFA_ISTORE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ISTORE, ofaIStore ))
#define OFA_IS_ISTORE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ISTORE ))
#define OFA_ISTORE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ISTORE, ofaIStoreInterface ))

typedef struct _ofaIStore                    ofaIStore;

/**
 * ofaIStoreInterface:
 *
 * This defines the interface that an #ofaIStore should implement.
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
	 * load_dataset:
	 * @istore: this #ofaIStore instance.
	 *
	 * Requests the datastore to load its datas from DBMS.
	 *
	 * Since: version 1.
	 */
	void  ( *load_dataset )         ( ofaIStore *istore );
}
	ofaIStoreInterface;

/*
 * Interface-wide
 */
GType    ofa_istore_get_type                  ( void );

guint    ofa_istore_get_interface_last_version( const ofaIStore *istore );

/*
 * Implementation-wide
 */
guint    ofa_istore_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
void     ofa_istore_init                      ( ofaIStore *istore );

/*
 * ----- Load the dataset
 */
void     ofa_istore_load_dataset              ( ofaIStore *istore );

/*
 * ----- Manage ofaITreeAdder interface
 */
void     ofa_istore_set_column_types          ( ofaIStore *istore,
													ofaIGetter *getter,
													guint columns_count,
													GType *columns_type );

void     ofa_istore_set_values                ( ofaIStore *istore,
													GtkTreeIter *iter,
													void *object );

gboolean ofa_istore_sort                      ( ofaIStore *store,
													GtkTreeModel *model,
													GtkTreeIter *a,
													GtkTreeIter *b,
													gint column_id,
													gint *cmp );

void     ofa_istore_add_columns               ( ofaIStore *istore,
													ofaTVBin *bin );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ISTORE_H__ */
