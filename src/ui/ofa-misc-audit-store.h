/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_MISC_AUDIT_STORE_H__
#define __OFA_MISC_AUDIT_STORE_H__

/**
 * SECTION: misc_audit_store
 * @title: ofaMiscAuditStore
 * @short_description: The Audit store class definition
 * @include: ui/ofa-misc-audit-store.h
 *
 * The #ofaMiscAuditStore derived from #ofaListStore, which itself
 * derives from #GtkListStore.
 */

#include "api/ofa-hub-def.h"
#include "api/ofa-list-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_MISC_AUDIT_STORE                ( ofa_misc_audit_store_get_type())
#define OFA_MISC_AUDIT_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MISC_AUDIT_STORE, ofaMiscAuditStore ))
#define OFA_MISC_AUDIT_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MISC_AUDIT_STORE, ofaMiscAuditStoreClass ))
#define OFA_IS_MISC_AUDIT_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MISC_AUDIT_STORE ))
#define OFA_IS_MISC_AUDIT_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MISC_AUDIT_STORE ))
#define OFA_MISC_AUDIT_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MISC_AUDIT_STORE, ofaMiscAuditStoreClass ))

typedef struct {
	/*< public members >*/
	ofaListStore      parent;
}
	ofaMiscAuditStore;

/**
 * ofaMiscAuditStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaListStoreClass parent;
}
	ofaMiscAuditStoreClass;

/**
 * The columns stored in the subjacent #GtkListStore.
 *                                                                      Displayable
 *                                                             Type     in treeview
 *                                                             -------  -----------
 * @AUDIT_COL_DATE         : timestamp                         String       Yes
 * @AUDIT_COL_COMMAND      : command                           String       Yes
 */
enum {
	AUDIT_COL_DATE = 0,
	AUDIT_COL_COMMAND,
	AUDIT_N_COLUMNS
};

GType              ofa_misc_audit_store_get_type       ( void );

ofaMiscAuditStore *ofa_misc_audit_store_new            ( ofaHub *hub );

guint              ofa_misc_audit_store_get_pages_count( ofaMiscAuditStore *store,
																guint page_size );

void               ofa_misc_audit_store_load_lines     ( ofaMiscAuditStore *store,
																guint page_num );

G_END_DECLS

#endif /* __OFA_MISC_AUDIT_STORE_H__ */
