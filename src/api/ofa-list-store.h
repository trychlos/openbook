/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_LIST_STORE_H__
#define __OPENBOOK_API_OFA_LIST_STORE_H__

/**
 * SECTION: ofa_list_store
 * @title: ofaListStore
 * @short_description: The ofaListStore application class definition
 * @include: api/ofa-list-store.h
 *
 * This is a very thin base class for other stores of the application.
 *
 * As a convenience, commonly used interfaces are mainly implemented in
 * this base class, leaving to the derived class to choose to use it or
 * not.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_LIST_STORE                ( ofa_list_store_get_type())
#define OFA_LIST_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_LIST_STORE, ofaListStore ))
#define OFA_LIST_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_LIST_STORE, ofaListStoreClass ))
#define OFA_IS_LIST_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_LIST_STORE ))
#define OFA_IS_LIST_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_LIST_STORE ))
#define OFA_LIST_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_LIST_STORE, ofaListStoreClass ))

typedef struct {
	/*< public members >*/
	GtkListStore      parent;
}
	ofaListStore;

/**
 * ofaListStoreClass:
 */
typedef struct {
	/*< public members >*/
	GtkListStoreClass parent;

	/*< protected virtual functions >*/
	/**
	 * load_dataset:
	 * @store: this #ofaListStore instance.
	 *
	 * Requests the datastore to load its datas from DBMS.
	 *
	 * This is just a redirection of the #ofaIStore::load_dataset() method.
	 */
	void ( *load_dataset )( ofaListStore *store );
}
	ofaListStoreClass;

GType   ofa_list_store_get_type        ( void ) G_GNUC_CONST;

void    ofa_list_store_loading_simulate( ofaListStore *store );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_LIST_STORE_H__ */
