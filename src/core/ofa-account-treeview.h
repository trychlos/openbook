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

#ifndef __OFA_ACCOUNT_TREEVIEW_H__
#define __OFA_ACCOUNT_TREEVIEW_H__

/**
 * SECTION: ofa_account_treeview
 * @short_description: #ofaAccountTreeview class definition.
 * @include: ui/ofa-account-treeview.h
 *
 * Manage a treeview with a filtered list of accounts.
 *
 * The class provides the following signals, which are proxyed from
 * #ofaTVBin base class.
 *    +------------------+-------------+
 *    | Signal           | Account may |
 *    |                  | be %NULL    |
 *    +------------------+-------------+
 *    | ofa-accchanged   |     Yes     |
 *    | ofa-accactivated |      No     |
 *    | ofa-accdelete    |      No     |
 *    +------------------+-------------+
 *
 * Properties:
 * - ofa-account-treeview-class-number: class number attached to this page.
 */

#include "api/ofa-igetter-def.h"
#include "api/ofa-tvbin.h"
#include "api/ofo-account-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNT_TREEVIEW                ( ofa_account_treeview_get_type())
#define OFA_ACCOUNT_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNT_TREEVIEW, ofaAccountTreeview ))
#define OFA_ACCOUNT_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNT_TREEVIEW, ofaAccountTreeviewClass ))
#define OFA_IS_ACCOUNT_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNT_TREEVIEW ))
#define OFA_IS_ACCOUNT_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNT_TREEVIEW ))
#define OFA_ACCOUNT_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNT_TREEVIEW, ofaAccountTreeviewClass ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaAccountTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaAccountTreeviewClass;

GType               ofa_account_treeview_get_type        ( void ) G_GNUC_CONST;

ofaAccountTreeview *ofa_account_treeview_new             ( ofaIGetter *getter,
																	const gchar *settings_prefix,
																	gint class_num );

gint                ofa_account_treeview_get_class       ( ofaAccountTreeview *view );

ofoAccount         *ofa_account_treeview_get_selected    ( ofaAccountTreeview *view );

void                ofa_account_treeview_set_selected    ( ofaAccountTreeview *view,
																	const gchar *account_id );

void                ofa_account_treeview_cell_data_render( ofaAccountTreeview *view,
																	GtkTreeViewColumn *column,
																	GtkCellRenderer *renderer,
																	GtkTreeModel *model,
																	GtkTreeIter *iter );

G_END_DECLS

#endif /* __OFA_ACCOUNT_TREEVIEW_H__ */
