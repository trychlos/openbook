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

#ifndef __OFA_MISC_AUDIT_TREEVIEW_H__
#define __OFA_MISC_AUDIT_TREEVIEW_H__

/**
 * SECTION: ofa_misc_audit_treeview
 * @short_description: #ofaMiscAuditTreeview class definition.
 * @include: ui/ofa-misc-audit-treeview.h
 *
 * Manage a treeview which displays the DBMS audit trace.
 */

#include "api/ofa-igetter-def.h"
#include "api/ofa-tvbin.h"

#include "ui/ofa-misc-audit-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_MISC_AUDIT_TREEVIEW                ( ofa_misc_audit_treeview_get_type())
#define OFA_MISC_AUDIT_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MISC_AUDIT_TREEVIEW, ofaMiscAuditTreeview ))
#define OFA_MISC_AUDIT_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MISC_AUDIT_TREEVIEW, ofaMiscAuditTreeviewClass ))
#define OFA_IS_MISC_AUDIT_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MISC_AUDIT_TREEVIEW ))
#define OFA_IS_MISC_AUDIT_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MISC_AUDIT_TREEVIEW ))
#define OFA_MISC_AUDIT_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MISC_AUDIT_TREEVIEW, ofaMiscAuditTreeviewClass ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaMiscAuditTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaMiscAuditTreeviewClass;

GType                 ofa_misc_audit_treeview_get_type     ( void ) G_GNUC_CONST;

ofaMiscAuditTreeview *ofa_misc_audit_treeview_new          ( ofaIGetter *getter,
																	const gchar *settings_prefix );

ofaMiscAuditStore    *ofa_misc_audit_treeview_setup_store  ( ofaMiscAuditTreeview *view );

G_END_DECLS

#endif /* __OFA_MISC_AUDIT_TREEVIEW_H__ */
