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

#ifndef __OFA_CLASS_STORE_H__
#define __OFA_CLASS_STORE_H__

/**
 * SECTION: class_store
 * @title: ofaClassStore
 * @short_description: The ClassStore class definition
 * @include: ui/ofa-class-store.h
 *
 * The #ofaClassStore derived from #ofaListStore, which itself
 * derives from #GtkListStore. It is populated with all the #ofoClass
 * items on first call, and stay then alive until the dossier is closed.
 *
 * Once more time: there is only one #ofaClassStore while the dossier
 * is opened. All the views are built on this store, using ad-hoc filter
 * models when needed.
 *
 * The #ofaClassStore takes advantage of the dossier signaling system to
 * maintain itself up to date.
 */

#include "api/ofa-igetter-def.h"
#include "api/ofa-list-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_CLASS_STORE                ( ofa_class_store_get_type())
#define OFA_CLASS_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_CLASS_STORE, ofaClassStore ))
#define OFA_CLASS_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_CLASS_STORE, ofaClassStoreClass ))
#define OFA_IS_CLASS_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_CLASS_STORE ))
#define OFA_IS_CLASS_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_CLASS_STORE ))
#define OFA_CLASS_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_CLASS_STORE, ofaClassStoreClass ))

typedef struct {
	/*< public members >*/
	ofaListStore      parent;
}
	ofaClassStore;

/**
 * ofaClassStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaListStoreClass parent;
}
	ofaClassStoreClass;

/**
 * The columns stored in the subjacent #GtkListStore.
 *                                                                      Displayable
 *                                                             Type     in Class page
 *                                                             -------  -------------
 * @CLASS_COL_CLASS        : class number                      String        Yes
 * @CLASS_COL_CLASS_I      : class number                      Int            No
 * @CLASS_COL_CRE_USER     : creation user                     String        Yes
 * @CLASS_COL_CRE_STAMP    : creation stamp                    String        Yes
 * @CLASS_COL_LABEL        : label                             String        Yes
 * @CLASS_COL_NOTES        : notes                             String        Yes
 * @CLASS_COL_NOTES_PNG    : notes indicator                   GdkPixbuf     Yes
 * @CLASS_COL_UPD_USER     : last update user                  String        Yes
 * @CLASS_COL_UPD_STAMP    : last update stamp                 String        Yes
 * @CLASS_COL_OBJECT       : the #ofoClass object              GObject        No
 */
enum {
	CLASS_COL_CLASS = 0,
	CLASS_COL_CLASS_I,
	CLASS_COL_CRE_USER,
	CLASS_COL_CRE_STAMP,
	CLASS_COL_LABEL,
	CLASS_COL_NOTES,
	CLASS_COL_NOTES_PNG,
	CLASS_COL_UPD_USER,
	CLASS_COL_UPD_STAMP,
	CLASS_COL_OBJECT,
	CLASS_N_COLUMNS
};

GType          ofa_class_store_get_type( void );

ofaClassStore *ofa_class_store_new     ( ofaIGetter *getter );

G_END_DECLS

#endif /* __OFA_CLASS_STORE_H__ */
