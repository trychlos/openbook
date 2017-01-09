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

#ifndef __OFA_RECONCIL_TREEVIEW_H__
#define __OFA_RECONCIL_TREEVIEW_H__

/**
 * SECTION: ofa_reconcil_treeview
 * @short_description: #ofaReconcilTreeview class definition.
 * @include: ui/ofa-reconcil-treeview.h
 *
 * Manage a treeview with a filtered list of entries and BAT lines.
 *
 * This class is dedicated to reconciliation operations. It does not
 * allow other edition than those of the specific conciliation datas.
 *
 * The class provides the following signals, which are proxyed from
 * #ofaTVBin base class.
 *    +------------------+------------+
 *    | Signal           | List may   |
 *    |                  | be empty   |
 *    +------------------+------------+
 *    | ofa-entchanged   |     Yes    |
 *    | ofa-entactivated |      No    |
 *    +------------------+------------+
 *
 * As the treeview may allow multiple selection, both signals provide
 * a list of selected objects. It is up to the user of this class to
 * decide whether an action may apply or not on a multiple selection.
 */

#include "api/ofa-hub-def.h"
#include "api/ofa-tvbin.h"
#include "api/ofo-entry.h"

G_BEGIN_DECLS

#define OFA_TYPE_RECONCIL_TREEVIEW                ( ofa_reconcil_treeview_get_type())
#define OFA_RECONCIL_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RECONCIL_TREEVIEW, ofaReconcilTreeview ))
#define OFA_RECONCIL_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RECONCIL_TREEVIEW, ofaReconcilTreeviewClass ))
#define OFA_IS_RECONCIL_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RECONCIL_TREEVIEW ))
#define OFA_IS_RECONCIL_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RECONCIL_TREEVIEW ))
#define OFA_RECONCIL_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RECONCIL_TREEVIEW, ofaReconcilTreeviewClass ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaReconcilTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaReconcilTreeviewClass;

GType                ofa_reconcil_treeview_get_type          ( void ) G_GNUC_CONST;

ofaReconcilTreeview *ofa_reconcil_treeview_new               ( ofaHub *hub );

void                 ofa_reconcil_treeview_set_settings_key  ( ofaReconcilTreeview *view,
																const gchar *key );

void                 ofa_reconcil_treeview_setup_columns     ( ofaReconcilTreeview *view );

GList               *ofa_reconcil_treeview_get_selected      ( ofaReconcilTreeview *view );

#define              ofa_reconcil_treeview_free_selected(L)  g_list_free_full(( L ), ( GDestroyNotify ) g_object_unref )

void                 ofa_reconcil_treeview_set_filter_func   ( ofaReconcilTreeview *view,
																	GtkTreeModelFilterVisibleFunc filter_fn,
																	void *filter_data );

void                 ofa_reconcil_treeview_default_expand    ( ofaReconcilTreeview *view );

void                 ofa_reconcil_treeview_expand_all        ( ofaReconcilTreeview *view );

void                 ofa_reconcil_treeview_collapse_by_iter  ( ofaReconcilTreeview *view,
																	GtkTreeIter *iter );

#if 0
void                 ofa_reconcil_treeview_set_selected      ( ofaReconcilTreeview *view,
																	ofxCounter entry );
#endif

G_END_DECLS

#endif /* __OFA_RECONCIL_TREEVIEW_H__ */
