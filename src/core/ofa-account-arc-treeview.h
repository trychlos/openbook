/*
 * Open Firm AccountArcing
 * A double-entry account_arcing application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
 *
 * Open Firm AccountArcing is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm AccountArcing is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm AccountArcing; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifndef __OFA_ACCOUNT_ARC_TREEVIEW_H__
#define __OFA_ACCOUNT_ARC_TREEVIEW_H__

/**
 * SECTION: ofa_account_arc_treeview
 * @short_description: #ofaAccountArcTreeview class definition.
 * @include: core/ofa-account-arc-treeview.h
 *
 * Manage a treeview with a sorted list of archived soldes.
 *
 * The class does not manage the selection.
 */

#include "api/ofa-igetter-def.h"
#include "api/ofa-tvbin.h"
#include "api/ofo-account-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNT_ARC_TREEVIEW                ( ofa_account_arc_treeview_get_type())
#define OFA_ACCOUNT_ARC_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNT_ARC_TREEVIEW, ofaAccountArcTreeview ))
#define OFA_ACCOUNT_ARC_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNT_ARC_TREEVIEW, ofaAccountArcTreeviewClass ))
#define OFA_IS_ACCOUNT_ARC_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNT_ARC_TREEVIEW ))
#define OFA_IS_ACCOUNT_ARC_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNT_ARC_TREEVIEW ))
#define OFA_ACCOUNT_ARC_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNT_ARC_TREEVIEW, ofaAccountArcTreeviewClass ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaAccountArcTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaAccountArcTreeviewClass;

GType                  ofa_account_arc_treeview_get_type( void ) G_GNUC_CONST;

ofaAccountArcTreeview *ofa_account_arc_treeview_new     ( ofaIGetter *getter,
																ofoAccount *account );

G_END_DECLS

#endif /* __OFA_ACCOUNT_ARC_TREEVIEW_H__ */
